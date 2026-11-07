/*
 * FlintRTOS - timing measurements. TCB code (MISRA C:2023). See flint_timing.h.
 */
#include "flint_timing.h"

const uint32_t g_flint_lat_bucket_ns[FLINT_LAT_BUCKETS - 1U] =
{
    500U, 1000U, 2000U, 5000U, 10000U, 50000U, 100000U, 1000000U, 5000000U
};

typedef struct
{
    volatile uint32_t count;
    volatile uint32_t min_ns;
    volatile uint32_t max_ns;
    volatile uint64_t sum_ns;
    volatile uint32_t hist[FLINT_LAT_BUCKETS];
} IrqAcc;

static IrqAcc s_irq = { 0U, 0xFFFFFFFFU, 0U, 0U, { 0U } };

uint32_t flint_timing_ticks_to_ns(uint64_t ticks, uint64_t cnt_freq)
{
    uint64_t ns;
    if (cnt_freq == 0U) { return 0U; }
    if (ticks > (cnt_freq * 4U)) { return 0xFFFFFFFFU; }     /* > 4 s: saturate */
    ns = (ticks * 1000000000ULL) / cnt_freq;
    return (ns > 0xFFFFFFFFULL) ? 0xFFFFFFFFU : (uint32_t)ns;
}

void flint_timing_irq_sample(uint64_t latency_ticks, uint64_t cnt_freq)
{
    uint32_t ns = flint_timing_ticks_to_ns(latency_ticks, cnt_freq);
    uint32_t b  = 0U;

    while ((b < (FLINT_LAT_BUCKETS - 1U)) && (ns >= g_flint_lat_bucket_ns[b])) { b++; }
    s_irq.hist[b]++;
    s_irq.count++;
    s_irq.sum_ns += ns;
    if (ns < s_irq.min_ns) { s_irq.min_ns = ns; }
    if (ns > s_irq.max_ns) { s_irq.max_ns = ns; }
}

#ifdef FLINT_TIMING_HOST_TEST
#define IRQ_SAVE_DISABLE(st)  do { (st) = 0U; } while (0)
#define IRQ_RESTORE(st)       do { (void)(st); } while (0)
#else
#define IRQ_SAVE_DISABLE(st)  __asm__ volatile("mrs %0, daif\n\tmsr daifset, #2" : "=r"(st) :: "memory")
#define IRQ_RESTORE(st)       __asm__ volatile("msr daif, %0" :: "r"(st) : "memory")
#endif

void flint_timing_irq_stats(FlintLatencyStats *out, bool reset)
{
    uint64_t st;
    IRQ_SAVE_DISABLE(st);
    out->count   = s_irq.count;
    out->min_ns  = (s_irq.count > 0U) ? s_irq.min_ns : 0U;
    out->max_ns  = s_irq.max_ns;
    out->mean_ns = (s_irq.count > 0U) ? (uint32_t)(s_irq.sum_ns / s_irq.count) : 0U;
    for (uint32_t i = 0U; i < FLINT_LAT_BUCKETS; i++)
    {
        out->hist[i] = s_irq.hist[i];
        if (reset) { s_irq.hist[i] = 0U; }
    }
    if (reset)
    {
        s_irq.count = 0U; s_irq.sum_ns = 0U;
        s_irq.min_ns = 0xFFFFFFFFU; s_irq.max_ns = 0U;
    }
    IRQ_RESTORE(st);
}

void flint_timing_acc_reset(FlintTimingAcc *a)
{
    a->count = 0U; a->sum_ns = 0U; a->min_ns = 0xFFFFFFFFU; a->max_ns = 0U;
}

void flint_timing_acc_add(FlintTimingAcc *a, uint32_t ns)
{
    a->count++;
    a->sum_ns += ns;
    if (ns < a->min_ns) { a->min_ns = ns; }
    if (ns > a->max_ns) { a->max_ns = ns; }
}

uint32_t flint_timing_acc_mean(const FlintTimingAcc *a)
{
    return (a->count > 0U) ? (uint32_t)(a->sum_ns / a->count) : 0U;
}
