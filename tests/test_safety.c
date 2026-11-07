/*
 * FlintRTOS - host unit tests for kernel/flint_safety.c and kernel/flint_timing.c.
 *
 * Simulates the reset-surviving RAM area with a plain buffer: "rebooting" is
 * calling flint_safety_boot() again on the same buffer. flint_panic() ends in
 * flint_safety_port_reset(), which the test turns into a longjmp.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <stdint.h>

#include "flint_safety.h"
#include "flint_timing.h"

static int s_fail, s_checks;
#define CHECK(c) do { s_checks++; if (!(c)) { s_fail++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

/* ---- port stubs ----------------------------------------------------------- */
static uint32_t   s_now_ms;
static const char *s_task = "net";
static int        s_feeds, s_resets;
static jmp_buf    s_reset_jmp;
static char       s_out[4096];

uint32_t flint_safety_port_ms(void)            { return s_now_ms; }
const char *flint_safety_port_task_name(void)  { return s_task; }
void flint_safety_port_wdog_start(uint32_t ms) { (void)ms; }
void flint_safety_port_wdog_feed(void)         { s_feeds++; }
void flint_safety_port_reset(void)             { s_resets++; longjmp(s_reset_jmp, 1); }
void flint_safety_port_print(const char *s)    { strncat(s_out, s, sizeof(s_out) - strlen(s_out) - 1U); }

static uint8_t s_noinit[1024];

static void reboot(void) { s_out[0] = '\0'; flint_safety_boot(s_noinit, 0x1000U); }

static void test_cold_and_watchdog(void)
{
    memset(s_noinit, 0xA7, sizeof(s_noinit));           /* power-on garbage */
    reboot();
    CHECK(flint_safety_last_reset()->reason == FLINT_RESET_COLD);
    CHECK(flint_safety_last_reset()->boot_count == 1U);
    CHECK(flint_safety_last_reset()->pm_rsts == 0x1000U);

    /* Run for a while, then reset without any crash being logged. */
    flint_safety_note_switch("hb", 1234U);
    flint_safety_note_switch("net", 5678U);
    reboot();
    CHECK(flint_safety_last_reset()->reason == FLINT_RESET_WATCHDOG);
    CHECK(flint_safety_last_reset()->boot_count == 2U);
    CHECK(flint_safety_last_reset()->have_activity);
    CHECK(strcmp(flint_safety_last_reset()->last_task, "net") == 0);
    CHECK(flint_safety_last_reset()->last_tick == 5678U);
    flint_safety_print_report();
    CHECK(strstr(s_out, "watchdog reset") != NULL);
    CHECK(strstr(s_out, "last task running: 'net' at tick 5678") != NULL);
}

static void test_panic_round_trip(void)
{
    FlintCpuState cpu = { 0x96000045U, 0x80abcU, 0x100000000ULL, 0x345U, 0x80aa0U, 0U, 0x200000U };
    s_now_ms = 42000U;
    s_task   = "net";
    if (setjmp(s_reset_jmp) == 0)
    {
        flint_panic(FLINT_RESET_CPU_FAULT, "data abort", &cpu);
    }
    CHECK(s_resets == 1);
    CHECK(strstr(s_out, "FLINT PANIC: CPU fault in task 'net': data abort") != NULL);

    reboot();
    {
        const FlintResetReport *r = flint_safety_last_reset();
        CHECK(r->reason == FLINT_RESET_CPU_FAULT);
        CHECK(r->boot_count == 3U);
        CHECK(strcmp(r->task, "net") == 0);
        CHECK(strcmp(r->detail, "data abort") == 0);
        CHECK(r->tick == 42000U);
        CHECK(r->cpu.esr == 0x96000045U);
        CHECK(r->cpu.far == 0x100000000ULL);
        CHECK(r->bt_depth == 0U);            /* fp 0 -> no frames walked */
    }
    flint_safety_print_report();
    CHECK(strstr(s_out, "EC=0x0000000000000025") != NULL);

    /* After a clean boot following the crash, the next reset is not a crash. */
    reboot();
    CHECK(flint_safety_last_reset()->reason == FLINT_RESET_WATCHDOG);
}

static void test_named_panic_and_corruption(void)
{
    if (setjmp(s_reset_jmp) == 0)
    {
        flint_panic_task(FLINT_RESET_TASK_HUNG, "hb", "silent 6250 ms > 6000 ms", NULL);
    }
    /* Corrupt one byte of the crash detail: CRC must reject the crash part. */
    {
        uint8_t copy[sizeof(s_noinit)];
        memcpy(copy, s_noinit, sizeof(copy));
        reboot();
        CHECK(flint_safety_last_reset()->reason == FLINT_RESET_TASK_HUNG);
        CHECK(strcmp(flint_safety_last_reset()->task, "hb") == 0);
        CHECK(strcmp(flint_safety_last_reset()->detail, "silent 6250 ms > 6000 ms") == 0);

        memcpy(s_noinit, copy, sizeof(copy));
        {
            /* find the detail text and flip a byte */
            uint8_t *p = (uint8_t *)memmem(s_noinit, sizeof(s_noinit), "silent", 6);
            CHECK(p != NULL);
            if (p != NULL) { p[1] ^= 0x20U; }
        }
        reboot();
        CHECK(flint_safety_last_reset()->reason == FLINT_RESET_WATCHDOG);   /* crash part rejected */
    }
    /* Corrupt the header: everything is untrusted -> cold boot, count restarts. */
    s_noinit[2] ^= 0xFFU;
    reboot();
    CHECK(flint_safety_last_reset()->reason == FLINT_RESET_COLD);
    CHECK(flint_safety_last_reset()->boot_count == 1U);
    /* Reason names */
    CHECK(strcmp(flint_safety_reason_name(FLINT_RESET_OUT_OF_MEMORY), "out of memory") == 0);
    CHECK(strcmp(flint_safety_reason_name((FlintResetReason)99), "?") == 0);
}

/* Frame-pointer backtrace: a fake AArch64 frame chain [fp]=next, [fp+8]=ret. */
static void test_backtrace(void)
{
    static uint64_t frames[3][2] __attribute__((aligned(16)));
    FlintCpuState cpu = { 0x96000045U, 0x1000U, 0U, 0U, 0U, 0U, 0U };
    frames[0][0] = (uint64_t)(uintptr_t)frames[1]; frames[0][1] = 0xAAA0U;
    frames[1][0] = (uint64_t)(uintptr_t)frames[2]; frames[1][1] = 0xBBB0U;
    frames[2][0] = 0U;                             frames[2][1] = 0xCCC0U;
    cpu.fp = (uint64_t)(uintptr_t)frames[0];
    if (setjmp(s_reset_jmp) == 0)
    {
        flint_panic(FLINT_RESET_CPU_FAULT, "bt test", &cpu);
    }
    reboot();
    {
        const FlintResetReport *r = flint_safety_last_reset();
        CHECK(r->bt_depth == 3U);
        CHECK(r->backtrace[0] == 0xAAA0U && r->backtrace[1] == 0xBBB0U && r->backtrace[2] == 0xCCC0U);
    }
    flint_safety_print_report();
    CHECK(strstr(s_out, "#2 0x0000000000000ccc0") == NULL);   /* formatting sanity */
    CHECK(strstr(s_out, "#2 0x000000000000ccc0") != NULL);
}

static void test_health(void)
{
    int a, b;
    s_now_ms = 1000U; s_feeds = 0;
    a = flint_health_register("net", 3000U);
    b = flint_health_register("hb",  6000U);
    CHECK((a == 0) && (b == 1));

    s_now_ms = 3500U;
    CHECK(flint_health_check(s_now_ms) == -1);     /* both on time */
    CHECK(s_feeds == 1);
    flint_health_checkin(a);                       /* net at 3500 */

    s_now_ms = 6900U;                              /* net silent 3400 > 3000 */
    CHECK(flint_health_check(s_now_ms) == a);
    CHECK(s_feeds == 1);                           /* not fed while a task is late */
    CHECK(flint_health_silence_ms(a, s_now_ms) == 3400U);
    CHECK(strcmp(flint_health_name(a), "net") == 0);

    flint_health_checkin(a);
    CHECK(flint_health_check(s_now_ms) == -1);     /* hb silent 5900 <= 6000: OK */
    s_now_ms = 7100U;                              /* hb silent 6100 > 6000 */
    CHECK(flint_health_check(s_now_ms) == b);
}

static void test_timing(void)
{
    FlintLatencyStats s;
    flint_timing_irq_stats(&s, true);              /* clear */
    flint_timing_irq_sample(27U, 54000000U);        /* 500 ns  -> bucket 1 */
    flint_timing_irq_sample(54U, 54000000U);        /* 1000 ns -> bucket 2 */
    flint_timing_irq_sample(10U, 54000000U);        /* 185 ns  -> bucket 0 */
    flint_timing_irq_sample(540000U, 54000000U);    /* 10 ms   -> last bucket */
    flint_timing_irq_stats(&s, true);
    CHECK(s.count == 4U);
    CHECK(s.min_ns == 185U);
    CHECK(s.max_ns == 10000000U);
    CHECK(s.hist[0] == 1U && s.hist[1] == 1U && s.hist[2] == 1U && s.hist[FLINT_LAT_BUCKETS - 1U] == 1U);
    CHECK(s.mean_ns == (185U + 500U + 1000U + 10000000U) / 4U);
    flint_timing_irq_stats(&s, false);
    CHECK(s.count == 0U && s.min_ns == 0U);
    CHECK(flint_timing_ticks_to_ns(54000000ULL * 10U, 54000000U) == 0xFFFFFFFFU);  /* saturates */
    {
        FlintTimingAcc acc;
        flint_timing_acc_reset(&acc);
        flint_timing_acc_add(&acc, 900U);
        flint_timing_acc_add(&acc, 1100U);
        CHECK(acc.min_ns == 900U && acc.max_ns == 1100U && flint_timing_acc_mean(&acc) == 1000U);
    }
}

int main(void)
{
    test_cold_and_watchdog();
    test_panic_round_trip();
    test_named_panic_and_corruption();
    test_backtrace();
    test_health();
    test_timing();
    printf("safety/timing: %d checks, %d failed\n", s_checks, s_fail);
    return (s_fail == 0) ? 0 : 1;
}
