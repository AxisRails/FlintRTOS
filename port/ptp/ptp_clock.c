/*
 * FlintRTOS - PTP disciplined clock (AArch64 generic-timer based).
 */
#include "ptp_clock.h"
#include "ptp_time.h"

static int64_t s_offset_ns = 0;   /* servo-maintained correction */

static inline uint64_t read_cntpct(void)
{
    uint64_t v;
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(v));
    return v;
}
static inline uint64_t read_cntfrq(void)
{
    uint64_t v;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(v));
    return v;
}

int64_t ptp_clock_raw_ns(void)
{
    uint64_t cnt  = read_cntpct();
    uint64_t freq = read_cntfrq();
    if (freq == 0U) { return 0; }
    /* ns = cnt * 1e9 / freq, split to avoid 64-bit overflow at high counts. */
    uint64_t secs  = cnt / freq;
    uint64_t rem   = cnt % freq;
    return (int64_t)((secs * 1000000000ULL) + ((rem * 1000000000ULL) / freq));
}

int64_t ptp_clock_now_ns(void)
{
    return ptp_clock_raw_ns() + s_offset_ns;
}

void ptp_clock_now(TimeInternal *t)
{
    ptp_ns_to_time(ptp_clock_now_ns(), t);
}

void ptp_clock_step(int64_t delta_ns)
{
    s_offset_ns += delta_ns;
}

int64_t ptp_clock_offset_ns(void)
{
    return s_offset_ns;
}
