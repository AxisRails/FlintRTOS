/*
 * FlintRTOS - PTP disciplined clock (AArch64 generic-timer based).
 * See ptp_clock.h for the model.
 */
#include "ptp_clock.h"
#include "ptp_time.h"

#define NS_PER_S   (1000000000LL)

#ifdef PTP_HOST_TEST
/* Host unit test: the test harness owns a simulated counter. */
extern uint64_t g_sim_cnt;
extern uint64_t g_sim_freq;
uint64_t ptp_clock_read_cnt(void) { return g_sim_cnt; }
uint64_t ptp_clock_cnt_freq(void) { return g_sim_freq; }
#else
uint64_t ptp_clock_read_cnt(void)
{
    uint64_t v;
    __asm__ volatile("isb\n\tmrs %0, cntpct_el0" : "=r"(v) :: "memory");
    return v;
}
uint64_t ptp_clock_cnt_freq(void)
{
    uint64_t v;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(v));
    return v;
}
#endif

static uint64_t s_base_cnt = 0U;   /* anchor: counter value ...          */
static int64_t  s_base_ns  = 0;    /* ... and the PTP time it maps to    */
static int32_t  s_ppb      = 0;    /* frequency correction               */
static uint32_t s_epoch    = 0U;   /* step counter                       */

/* Signed tick delta -> ns without 64-bit overflow for any realistic span. */
static int64_t ticks_to_ns(int64_t dt)
{
    const int64_t f = (int64_t)ptp_clock_cnt_freq();
    if (f <= 0) { return 0; }
    {
        int64_t secs = dt / f;
        int64_t rem  = dt % f;
        return (secs * NS_PER_S) + ((rem * NS_PER_S) / f);
    }
}

/* d * ppb / 1e9, split so it cannot overflow. */
static int64_t scale_ppb(int64_t d, int32_t ppb)
{
    int64_t whole = d / NS_PER_S;
    int64_t frac  = d % NS_PER_S;
    return (whole * (int64_t)ppb) + ((frac * (int64_t)ppb) / NS_PER_S);
}

int64_t ptp_clock_from_cnt(uint64_t cnt)
{
    int64_t d = ticks_to_ns((int64_t)(cnt - s_base_cnt));
    return s_base_ns + d + scale_ppb(d, s_ppb);
}

int64_t ptp_clock_now_ns(void)
{
    return ptp_clock_from_cnt(ptp_clock_read_cnt());
}

void ptp_clock_now(TimeInternal *t)
{
    ptp_ns_to_time(ptp_clock_now_ns(), t);
}

/* Re-anchor the model at the current counter value. */
static void rebase(void)
{
    uint64_t now = ptp_clock_read_cnt();
    s_base_ns  = ptp_clock_from_cnt(now);
    s_base_cnt = now;
}

void ptp_clock_step(int64_t delta_ns)
{
    rebase();
    s_base_ns += delta_ns;
    s_epoch++;
}

void ptp_clock_adjfreq(int32_t ppb)
{
    if (ppb >  PTP_CLOCK_MAX_PPB) { ppb =  PTP_CLOCK_MAX_PPB; }
    if (ppb < -PTP_CLOCK_MAX_PPB) { ppb = -PTP_CLOCK_MAX_PPB; }
    rebase();
    s_ppb = ppb;
}

int32_t ptp_clock_get_freq(void)
{
    return s_ppb;
}

uint32_t ptp_clock_epoch(void)
{
    return s_epoch;
}

int64_t ptp_clock_raw_ns(void)
{
    return ticks_to_ns((int64_t)ptp_clock_read_cnt());
}
