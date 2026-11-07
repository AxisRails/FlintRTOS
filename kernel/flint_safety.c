/*
 * FlintRTOS - safety services: crash record, panic path, task health monitor.
 * TCB code (MISRA C:2023). See flint_safety.h.
 */
#include "flint_safety.h"

#ifndef FLINT_SAFETY_RAM_LO
#define FLINT_SAFETY_RAM_LO   (0x00080000ULL)   /* image base           */
#endif
#ifndef FLINT_SAFETY_RAM_HI
#define FLINT_SAFETY_RAM_HI   (0x3B400000ULL)   /* end of ARM RAM (948M) */
#endif

#define REC_MAGIC     (0x464C4E54U)             /* 'FLNT' */
#define REC_VERSION   (1U)
#define STATE_RUNNING (0x52554E21U)             /* 'RUN!' */
#define STATE_CRASHED (0x44454144U)             /* 'DEAD' */

/* Layout of the reset-surviving area. Three independently checked parts, so
   the per-switch activity update never has to recompute a large CRC. */
typedef struct
{
    /* header */
    uint32_t magic;
    uint32_t version;
    uint32_t boot_count;
    uint32_t state;
    uint32_t hdr_crc;
    uint32_t pad0;
    /* last activity (written on every context switch) */
    char     act_task[FLINT_SAFETY_NAME_LEN];
    uint32_t act_tick;
    uint32_t act_check;                         /* ~act_tick */
    /* crash */
    uint32_t reason;
    uint32_t tick;
    char     task[FLINT_SAFETY_NAME_LEN];
    char     detail[FLINT_SAFETY_DETAIL_LEN];
    FlintCpuState cpu;
    uint64_t backtrace[FLINT_SAFETY_BT_DEPTH];
    uint32_t bt_depth;
    uint32_t crash_crc;
} SafetyRecord;

#ifdef FLINT_SAFETY_HOST_TEST
#define IRQ_SAVE_DISABLE(st)  do { (st) = 0U; } while (0)
#define IRQ_RESTORE(st)       do { (void)(st); } while (0)
#define IRQ_ALL_OFF()         do { } while (0)
#else
#define IRQ_SAVE_DISABLE(st)  __asm__ volatile("mrs %0, daif\n\tmsr daifset, #2" : "=r"(st) :: "memory")
#define IRQ_RESTORE(st)       __asm__ volatile("msr daif, %0" :: "r"(st) : "memory")
#define IRQ_ALL_OFF()         __asm__ volatile("msr daifset, #3" ::: "memory")
#endif

static volatile SafetyRecord *s_rec = NULL;
static FlintResetReport      s_report;
static volatile uint32_t     s_in_panic = 0U;

/* ---- helpers -------------------------------------------------------------- */
static uint32_t crc32_bytes(const volatile uint8_t *p, size_t n)
{
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0U; i < n; i++)
    {
        crc ^= p[i];
        for (uint32_t b = 0U; b < 8U; b++)
        {
            uint32_t mask = (uint32_t)(-(int32_t)(crc & 1U));
            crc = (crc >> 1) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

static uint32_t hdr_crc(const volatile SafetyRecord *r)
{
    return crc32_bytes((const volatile uint8_t *)r, offsetof(SafetyRecord, hdr_crc));
}

static uint32_t crash_crc(const volatile SafetyRecord *r)
{
    return crc32_bytes((const volatile uint8_t *)&r->reason,
                       offsetof(SafetyRecord, crash_crc) - offsetof(SafetyRecord, reason));
}

static void copy_name(volatile char *dst, const char *src, size_t cap)
{
    size_t i = 0U;
    if (src != NULL)
    {
        while ((i < (cap - 1U)) && (src[i] != '\0')) { dst[i] = src[i]; i++; }
    }
    while (i < cap) { dst[i] = '\0'; i++; }
}

static void copy_from_rec(char *dst, const volatile char *src, size_t cap)
{
    for (size_t i = 0U; i < cap; i++) { dst[i] = src[i]; }
    dst[cap - 1U] = '\0';
}

/* Small formatting helpers: the panic path must not depend on printf. */
typedef struct { char buf[160]; size_t n; } Line;

static const char k_digits[] = "0123456789abcdef";

size_t flint_safety_append(char *dst, size_t cap, size_t pos, const char *s)
{
    size_t n = pos;
    if ((dst == NULL) || (cap == 0U)) { return pos; }
    if (s != NULL)
    {
        size_t i = 0U;
        while ((s[i] != '\0') && (n < (cap - 1U)))
        {
            dst[n] = s[i];
            n++;
            i++;
        }
    }
    if (n < cap) { dst[n] = '\0'; }
    return n;
}

size_t flint_safety_append_u32(char *dst, size_t cap, size_t pos, uint32_t v)
{
    char     t[11];
    uint32_t n = 10U;
    uint32_t x = v;
    t[10] = '\0';
    do
    {
        n--;
        t[n] = k_digits[x % 10U];
        x /= 10U;
    } while ((x != 0U) && (n > 0U));
    return flint_safety_append(dst, cap, pos, &t[n]);
}

static void l_str(Line *l, const char *s)
{
    l->n = flint_safety_append(l->buf, sizeof(l->buf), l->n, s);
}
static void l_hex(Line *l, uint64_t v)
{
    char t[19];
    t[0] = '0';
    t[1] = 'x';
    for (uint32_t i = 0U; i < 16U; i++)
    {
        t[17U - i] = k_digits[(v >> (i * 4U)) & 0xFU];
    }
    t[18] = '\0';
    l_str(l, t);
}
static void l_dec(Line *l, uint32_t v)
{
    l->n = flint_safety_append_u32(l->buf, sizeof(l->buf), l->n, v);
}
static void l_emit(Line *l)
{
    l_str(l, "\n");
    flint_safety_port_print(l->buf);
    l->n = 0U;
    l->buf[0] = '\0';
}

static bool fp_plausible(uint64_t fp)
{
    return (fp >= FLINT_SAFETY_RAM_LO) && (fp < FLINT_SAFETY_RAM_HI) && ((fp & 0xFU) == 0U);
}

/* AArch64 frame record: [fp] = caller's fp, [fp+8] = return address. */
static uint32_t walk_frames(uint64_t fp, uint64_t *out, uint32_t max)
{
    uint32_t n = 0U;
    while ((n < max) && fp_plausible(fp))
    {
        /* MISRA 11.4 deviation: a frame address is an integer from the CPU. */
        const uint64_t *frame = (const uint64_t *)(uintptr_t)fp;
        uint64_t next = frame[0];
        uint64_t ret  = frame[1];
        if (ret == 0U) { break; }
        out[n] = ret;
        n++;
        if (next <= fp) { break; }              /* stacks grow down: callers are higher */
        fp = next;
    }
    return n;
}

/* ---- boot ----------------------------------------------------------------- */
const char *flint_safety_reason_name(FlintResetReason r)
{
    static const char *const k_names[FLINT_RESET_REASON_COUNT] =
    {
        "cold boot", "watchdog reset", "CPU fault", "stack overflow",
        "task hung", "out of memory", "assertion failed", "reboot requested"
    };
    return ((uint32_t)r < (uint32_t)FLINT_RESET_REASON_COUNT) ? k_names[r] : "?";
}

void flint_safety_boot(void *noinit_area, uint32_t pm_rsts)
{
    volatile SafetyRecord *r = (volatile SafetyRecord *)noinit_area;
    uint32_t boots = 0U;

    s_rec = r;
    s_in_panic = 0U;
    for (size_t i = 0U; i < sizeof(s_report); i++) { ((uint8_t *)&s_report)[i] = 0U; }
    s_report.pm_rsts = pm_rsts;
    s_report.reason  = FLINT_RESET_COLD;

    if ((r->magic == REC_MAGIC) && (r->version == REC_VERSION) && (r->hdr_crc == hdr_crc(r)))
    {
        boots = r->boot_count;
        if (r->act_check == ~r->act_tick)
        {
            s_report.have_activity = true;
            s_report.last_tick     = r->act_tick;
            copy_from_rec(s_report.last_task, r->act_task, FLINT_SAFETY_NAME_LEN);
        }
        if ((r->state == STATE_CRASHED) && (r->crash_crc == crash_crc(r)) &&
            (r->reason < (uint32_t)FLINT_RESET_REASON_COUNT))
        {
            s_report.reason   = (FlintResetReason)r->reason;
            s_report.tick     = r->tick;
            s_report.cpu      = r->cpu;
            s_report.bt_depth = (r->bt_depth <= FLINT_SAFETY_BT_DEPTH) ? r->bt_depth : 0U;
            for (uint32_t i = 0U; i < s_report.bt_depth; i++) { s_report.backtrace[i] = r->backtrace[i]; }
            copy_from_rec(s_report.task,   r->task,   FLINT_SAFETY_NAME_LEN);
            copy_from_rec(s_report.detail, r->detail, FLINT_SAFETY_DETAIL_LEN);
        }
        else
        {
            /* It was running and nobody logged a crash: only the hardware
               watchdog (or a brown-out that kept RAM) resets like that. */
            s_report.reason = FLINT_RESET_WATCHDOG;
        }
    }

    /* Re-arm the record for this run. */
    r->magic      = REC_MAGIC;
    r->version    = REC_VERSION;
    r->boot_count = boots + 1U;
    r->state      = STATE_RUNNING;
    r->pad0       = 0U;
    r->hdr_crc    = hdr_crc(r);
    copy_name(r->act_task, "boot", FLINT_SAFETY_NAME_LEN);
    r->act_tick   = 0U;
    r->act_check  = ~0U;
    s_report.boot_count = boots + 1U;
}

const FlintResetReport *flint_safety_last_reset(void)
{
    return &s_report;
}

void flint_safety_print_report(void)
{
    const FlintResetReport *p = &s_report;
    Line l = { { 0 }, 0U };

    l_str(&l, "[safety] boot #"); l_dec(&l, p->boot_count);
    l_str(&l, ", previous reset: "); l_str(&l, flint_safety_reason_name(p->reason));
    l_str(&l, "  (PM_RSTS="); l_hex(&l, p->pm_rsts); l_str(&l, ")");
    l_emit(&l);

    if (p->have_activity && (p->reason != FLINT_RESET_COLD))
    {
        l_str(&l, "[safety]   last task running: '"); l_str(&l, p->last_task);
        l_str(&l, "' at tick "); l_dec(&l, p->last_tick);
        l_emit(&l);
    }
    if (p->reason >= FLINT_RESET_CPU_FAULT)
    {
        l_str(&l, "[safety]   crash in task '"); l_str(&l, p->task);
        l_str(&l, "' at tick "); l_dec(&l, p->tick);
        l_str(&l, ": "); l_str(&l, p->detail);
        l_emit(&l);
        if (p->reason == FLINT_RESET_CPU_FAULT)
        {
            l_str(&l, "[safety]   ESR="); l_hex(&l, p->cpu.esr);
            l_str(&l, " EC="); l_hex(&l, (p->cpu.esr >> 26) & 0x3FU);
            l_str(&l, " ELR="); l_hex(&l, p->cpu.elr);
            l_emit(&l);
            l_str(&l, "[safety]   FAR="); l_hex(&l, p->cpu.far);
            l_str(&l, " LR="); l_hex(&l, p->cpu.lr);
            l_str(&l, " SP="); l_hex(&l, p->cpu.sp);
            l_emit(&l);
        }
        for (uint32_t i = 0U; i < p->bt_depth; i++)
        {
            l_str(&l, "[safety]   #"); l_dec(&l, i); l_str(&l, " "); l_hex(&l, p->backtrace[i]);
            l_emit(&l);
        }
        if (p->bt_depth > 0U)
        {
            l_str(&l, "[safety]   (decode: llvm-symbolizer --obj=build/flint.elf <addr>)");
            l_emit(&l);
        }
    }
}

/* ---- runtime -------------------------------------------------------------- */
void flint_safety_note_switch(const char *task_name, uint32_t tick)
{
    volatile SafetyRecord *r = s_rec;
    if (r != NULL)
    {
        copy_name(r->act_task, task_name, FLINT_SAFETY_NAME_LEN);
        r->act_tick  = tick;
        r->act_check = ~tick;
    }
}

void flint_panic(FlintResetReason reason, const char *detail, const FlintCpuState *cpu)
{
    flint_panic_task(reason, flint_safety_port_task_name(), detail, cpu);
}

void flint_panic_task(FlintResetReason reason, const char *task, const char *detail,
                      const FlintCpuState *cpu)
{
    volatile SafetyRecord *r = s_rec;
    Line l = { { 0 }, 0U };

    IRQ_ALL_OFF();                                       /* IRQ + FIQ off */
    s_in_panic++;
    if (s_in_panic > 1U)
    {
        flint_safety_port_reset();                       /* fault while panicking */
    }

    if (r != NULL)
    {
        uint64_t fp;
        r->reason = (uint32_t)reason;
        r->tick   = flint_safety_port_ms();
        copy_name(r->task, task, FLINT_SAFETY_NAME_LEN);
        copy_name(r->detail, detail, FLINT_SAFETY_DETAIL_LEN);
        if (cpu != NULL)
        {
            r->cpu = *cpu;
            fp = cpu->fp;
        }
        else
        {
            FlintCpuState z = { 0U, 0U, 0U, 0U, 0U, 0U, 0U };
            r->cpu = z;
            fp = (uint64_t)(uintptr_t)__builtin_frame_address(0);
        }
        {
            uint64_t bt[FLINT_SAFETY_BT_DEPTH];
            uint32_t n = walk_frames(fp, bt, FLINT_SAFETY_BT_DEPTH);
            for (uint32_t i = 0U; i < FLINT_SAFETY_BT_DEPTH; i++) { r->backtrace[i] = (i < n) ? bt[i] : 0U; }
            r->bt_depth = n;
        }
        r->crash_crc = crash_crc(r);
        r->state     = STATE_CRASHED;
        r->hdr_crc   = hdr_crc(r);
    }

    l_str(&l, "\n*** FLINT PANIC: "); l_str(&l, flint_safety_reason_name(reason));
    l_str(&l, " in task '"); l_str(&l, task);
    l_str(&l, "': "); l_str(&l, detail); l_emit(&l);
    if (cpu != NULL)
    {
        l_str(&l, "  ESR="); l_hex(&l, cpu->esr); l_str(&l, " ELR="); l_hex(&l, cpu->elr);
        l_str(&l, " FAR="); l_hex(&l, cpu->far); l_emit(&l);
    }
#if defined(configSAFETY_RESET_ON_FAULT) && (configSAFETY_RESET_ON_FAULT == 0)
    l_str(&l, "  crash recorded; halted (configSAFETY_RESET_ON_FAULT=0)"); l_emit(&l);
    for (;;) { __asm__ volatile("wfe"); }
#else
    l_str(&l, "  crash recorded; resetting via watchdog..."); l_emit(&l);
    flint_safety_port_reset();
#endif
}

/* ---- health monitor ------------------------------------------------------- */
typedef struct
{
    const char       *name;
    uint32_t          deadline_ms;
    volatile uint32_t last_ms;
} HealthSlot;

static HealthSlot s_health[FLINT_HEALTH_MAX];
static volatile uint32_t s_health_n = 0U;

int flint_health_register(const char *name, uint32_t deadline_ms)
{
    int h = -1;
    uint64_t daif;
    IRQ_SAVE_DISABLE(daif);
    if (s_health_n < FLINT_HEALTH_MAX)
    {
        HealthSlot *s = &s_health[s_health_n];
        s->name        = name;
        s->deadline_ms = deadline_ms;
        s->last_ms     = flint_safety_port_ms();
        h = (int)s_health_n;
        s_health_n++;
    }
    IRQ_RESTORE(daif);
    return h;
}

void flint_health_checkin(int handle)
{
    if ((handle >= 0) && ((uint32_t)handle < s_health_n))
    {
        s_health[handle].last_ms = flint_safety_port_ms();
    }
}

uint32_t flint_health_silence_ms(int handle, uint32_t now_ms)
{
    return ((handle >= 0) && ((uint32_t)handle < s_health_n)) ?
           (uint32_t)(now_ms - s_health[handle].last_ms) : 0U;
}

uint32_t flint_health_deadline_ms(int handle)
{
    return ((handle >= 0) && ((uint32_t)handle < s_health_n)) ? s_health[handle].deadline_ms : 0U;
}

int flint_health_check(uint32_t now_ms)
{
    for (uint32_t i = 0U; i < s_health_n; i++)
    {
        if ((uint32_t)(now_ms - s_health[i].last_ms) > s_health[i].deadline_ms)
        {
            return (int)i;
        }
    }
    flint_safety_port_wdog_feed();
    return -1;
}

const char *flint_health_name(int handle)
{
    return ((handle >= 0) && ((uint32_t)handle < s_health_n)) ? s_health[handle].name : "?";
}
