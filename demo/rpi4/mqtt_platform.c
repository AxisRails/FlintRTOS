/*
 * FlintRTOS - MQTT platform services on the Raspberry Pi 4 (mqtt_platform.h).
 */
#include "FlintRTOS.h"

#if (configUSE_LWIP == 1) && (configUSE_CORE_MQTT == 1)

#include "task.h"
#include "uart.h"
#include "rpi4.h"
#include "portable.h"
#include "portmacro.h"

#include "mqtt_platform.h"
#include "jsonw.h"
#include "net_demo.h"
#include "flint_safety.h"
#include "flint_timing.h"
#include "bench.h"

#include "lwip/netif.h"
#include "lwip/ip4_addr.h"

#if (configUSE_PTP == 1)
#include "ptp_core.h"
#include "ptp_lwip_raw.h"
#endif

/* ACT (green) LED: GPIO42 on the Pi 4, active high. */
#define LED_GPIO        (42U)
#define GPFSEL4         (GPIO_BASE + 0x10UL)
#define GPSET1          (GPIO_BASE + 0x20UL)
#define GPCLR1          (GPIO_BASE + 0x2CUL)

static int  s_led = -1;   /* -1 = not configured yet */

uint32_t fm_plat_ms(void)
{
    return (uint32_t)xTaskGetTickCount();          /* 1 kHz tick */
}

void fm_plat_pump(bool idle)
{
    net_pump(idle);
}

const char *fm_plat_ip(void)
{
    return ip4addr_ntoa(netif_ip4_addr(net_demo_netif()));
}

void fm_plat_log(const char *line)
{
    uart_printf("%s\n", line);
}

int fm_plat_led(int on)
{
    if (s_led < 0)
    {
        uint32_t v = mmio_read32(GPFSEL4);
        v &= ~(7U << ((LED_GPIO - 40U) * 3U));
        v |=  (1U << ((LED_GPIO - 40U) * 3U));     /* output */
        mmio_write32(GPFSEL4, v);
        s_led = 0;
    }
    s_led = (on < 0) ? ((s_led != 0) ? 0 : 1) : ((on != 0) ? 1 : 0);
    mmio_write32((s_led != 0) ? GPSET1 : GPCLR1, 1U << (LED_GPIO - 32U));
    return s_led;
}

size_t fm_plat_ptp_json(char *buf, size_t cap)
{
#if (configUSE_PTP == 1)
    JsonW     w;
    PtpStatus st;
    char      when[28];
    char      gm[17];

    if (!ptp_raw_running()) { return 0U; }
    ptp_core_get_status(&st);

    jw_init(&w, buf, cap);
    jw_obj_open(&w);
    jw_str(&w, "state", st.state);
    jw_bool(&w, "locked", st.locked);
    if (st.have_master)
    {
        uint64_t id = 0U;
        for (uint32_t i = 0U; i < 8U; i++) { id = (id << 8) | st.gm_id[i]; }
        jw_str(&w, "gm", fm_hex64(id, gm, 16U));
        jw_i64(&w, "offset_ns", st.offset_ns);
        jw_i64(&w, "delay_ns", st.delay_ns);
        jw_i64(&w, "freq_ppb", st.freq_ppb);
        jw_u32(&w, "syncs", st.n_sync);
    }
    if (st.locked)
    {
        ptp_format_utc(ptp_core_utc_now_ns(), when);
        jw_str(&w, "utc", when);
    }
    jw_obj_close(&w);
    return jw_ok(&w) ? jw_len(&w) : 0U;
#else
    (void)buf; (void)cap;
    return 0U;
#endif
}

size_t fm_plat_stats_json(char *buf, size_t cap)
{
    JsonW          w;
    TaskSnapshot_t ts[12];
    UBaseType_t    n = uxTaskGetSnapshot(ts, 12U);

    jw_init(&w, buf, cap);
    jw_obj_open(&w);
    jw_u32(&w, "uptime_s", fm_plat_ms() / 1000U);
    jw_u32(&w, "heap_free", (uint32_t)xPortGetFreeHeapSize());
    jw_u32(&w, "heap_min",  (uint32_t)xPortGetMinimumEverFreeHeapSize());
    jw_u32(&w, "genet_irqs", ulPortIrqCount(GENET_IRQ0_INTID));
    jw_arr_open(&w, "tasks");
    for (UBaseType_t i = 0U; i < n; i++)
    {
        jw_obj_open(&w);
        jw_str(&w, "name", ts[i].pcName);
        jw_u32(&w, "prio", (uint32_t)ts[i].uxPriority);
        jw_u32(&w, "stack", ts[i].ulStackDepth * (uint32_t)sizeof(StackType_t));
        jw_u32(&w, "stack_free", ts[i].ulStackFree * (uint32_t)sizeof(StackType_t));
        jw_obj_close(&w);
    }
    jw_arr_close(&w);

    /* Tick-interrupt latency since the previous stats message. */
    {
        FlintLatencyStats ls;
        flint_timing_irq_stats(&ls, true);
        jw_obj_open_key(&w, "irq_latency_ns");
        jw_u32(&w, "samples", ls.count);
        jw_u32(&w, "min", ls.min_ns);
        jw_u32(&w, "mean", ls.mean_ns);
        jw_u32(&w, "max", ls.max_ns);
        jw_arr_open(&w, "hist");                 /* buckets: see "hist_edges_ns" */
        for (uint32_t i = 0U; i < FLINT_LAT_BUCKETS; i++) { jw_u32(&w, NULL, ls.hist[i]); }
        jw_arr_close(&w);
        jw_obj_close(&w);
    }
    {
        const BenchResult *b = bench_result();
        if (b->valid)
        {
            jw_obj_open_key(&w, "ctx_switch_ns");
            jw_u32(&w, "sem_wake_min",  b->sem_wake.min_ns);
            jw_u32(&w, "sem_wake_mean", flint_timing_acc_mean(&b->sem_wake));
            jw_u32(&w, "sem_wake_max",  b->sem_wake.max_ns);
            jw_u32(&w, "yield_min",  b->yield.min_ns);
            jw_u32(&w, "yield_mean", flint_timing_acc_mean(&b->yield));
            jw_u32(&w, "yield_max",  b->yield.max_ns);
            jw_obj_close(&w);
        }
    }
    jw_obj_close(&w);
    return jw_ok(&w) ? jw_len(&w) : 0U;
}

/* ---- safety reporting ------------------------------------------------------ */
const char *fm_plat_reset_reason(void)
{
    return flint_safety_reason_name(flint_safety_last_reset()->reason);
}

uint32_t fm_plat_boot_count(void)
{
    return flint_safety_last_reset()->boot_count;
}

size_t fm_plat_crash_json(char *buf, size_t cap)
{
    const FlintResetReport *r = flint_safety_last_reset();
    JsonW w;
    char  hx[17];
    char  hx2[19];

    jw_init(&w, buf, cap);
    jw_obj_open(&w);
    jw_u32(&w, "boot", r->boot_count);
    jw_str(&w, "reason", flint_safety_reason_name(r->reason));
    fm_strlcpy(hx2, "0x", sizeof(hx2));
    fm_strlcat(hx2, fm_hex64(r->pm_rsts, hx, 8U), sizeof(hx2));
    jw_str(&w, "pm_rsts", hx2);
    if (r->have_activity && (r->reason != FLINT_RESET_COLD))
    {
        jw_str(&w, "last_task", r->last_task);
        jw_u32(&w, "last_tick", r->last_tick);
    }
    if (r->reason >= FLINT_RESET_CPU_FAULT)
    {
        jw_str(&w, "task", r->task);
        jw_u32(&w, "tick", r->tick);
        jw_str(&w, "detail", r->detail);
        if (r->reason == FLINT_RESET_CPU_FAULT)
        {
            const uint64_t v[4] = { r->cpu.esr, r->cpu.elr, r->cpu.far, r->cpu.lr };
            const char *const k[4] = { "esr", "elr", "far", "lr" };
            for (uint32_t i = 0U; i < 4U; i++)
            {
                fm_strlcpy(hx2, "0x", sizeof(hx2));
                fm_strlcat(hx2, fm_hex64(v[i], hx, 16U), sizeof(hx2));
                jw_str(&w, k[i], hx2);
            }
        }
        jw_arr_open(&w, "backtrace");
        for (uint32_t i = 0U; i < r->bt_depth; i++)
        {
            fm_strlcpy(hx2, "0x", sizeof(hx2));
            fm_strlcat(hx2, fm_hex64(r->backtrace[i], hx, 16U), sizeof(hx2));
            jw_str(&w, NULL, hx2);
        }
        jw_arr_close(&w);
    }
    jw_obj_close(&w);
    return jw_ok(&w) ? jw_len(&w) : 0U;
}

/* ---- platform commands ----------------------------------------------------- */
typedef enum { ACT_NONE = 0, ACT_REBOOT, ACT_ABORT, ACT_HANG, ACT_IRQOFF, ACT_STACK, ACT_OOM } PendingAction;
static PendingAction s_action = ACT_NONE;

static bool streq(const char *a, const char *b)
{
    while ((*a != '\0') && (*a == *b)) { a++; b++; }
    return (*a == *b);
}

static void reply2(char *buf, size_t cap, const char *cmd, const char *k, const char *v)
{
    JsonW w;
    jw_init(&w, buf, cap);
    jw_obj_open(&w);
    jw_str(&w, "cmd", cmd);
    jw_str(&w, k, v);
    jw_obj_close(&w);
}

bool fm_plat_command(const char *cmd, char *reply, size_t cap)
{
    if (streq(cmd, "bench"))
    {
        bench_request();
        reply2(reply, cap, cmd, "status", "started; results in the next stats message");
        return true;
    }
    if (streq(cmd, "reboot"))
    {
        s_action = ACT_REBOOT;
        reply2(reply, cap, cmd, "status", "rebooting");
        return true;
    }
#if (configFAULT_INJECTION == 1)
    {
        static const struct { const char *name; PendingAction act; const char *what; } k_faults[] =
        {
            { "fault abort",  ACT_ABORT,  "data abort -> CPU fault record + reset" },
            { "fault hang",   ACT_HANG,   "net task stops checking in -> health monitor reset" },
            { "fault irqoff", ACT_IRQOFF, "interrupts off forever -> hardware watchdog reset" },
            { "fault stack",  ACT_STACK,  "stack guard overwritten -> overflow record + reset" },
            { "fault oom",    ACT_OOM,    "heap exhausted -> out-of-memory record + reset" },
        };
        for (uint32_t i = 0U; i < (sizeof(k_faults) / sizeof(k_faults[0])); i++)
        {
            if (streq(cmd, k_faults[i].name))
            {
                s_action = k_faults[i].act;
                reply2(reply, cap, cmd, "injecting", k_faults[i].what);
                return true;
            }
        }
    }
#endif
    return false;
}

void fm_plat_after_reply(void)
{
    PendingAction a = s_action;
    s_action = ACT_NONE;
    switch (a)
    {
        case ACT_REBOOT:
            flint_panic(FLINT_RESET_REQUESTED, "reboot command (MQTT)", NULL);
            break;
#if (configFAULT_INJECTION == 1)
        case ACT_ABORT:
            uart_printf("[fault] reading unmapped address 0x100000000\n");
            (void)*(volatile uint32_t *)(uintptr_t)0x100000000ULL;
            break;
        case ACT_HANG:
            uart_printf("[fault] net task spinning with interrupts on\n");
            for (;;) { __asm__ volatile("nop"); }
        case ACT_IRQOFF:
            uart_printf("[fault] interrupts masked, spinning\n");
            __asm__ volatile("msr daifset, #3" ::: "memory");
            for (;;) { __asm__ volatile("nop"); }
        case ACT_STACK:
            uart_printf("[fault] corrupting the stack guard\n");
            vTaskInjectStackOverflow();
            break;
        case ACT_OOM:
            uart_printf("[fault] allocating until the heap is exhausted\n");
            for (;;) { (void)pvPortMalloc(64U * 1024U); }
#endif
        case ACT_NONE:
        default:
            break;
    }
}

#endif
