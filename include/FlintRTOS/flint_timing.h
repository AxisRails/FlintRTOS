/*
 * FlintRTOS - timing measurements (TCB, MISRA C:2023).
 *
 * Interrupt latency: the tick timer is programmed with an absolute compare
 * value (CNTP_CVAL), so the moment the interrupt *should* have been taken is
 * known exactly. The tick handler records (CNTPCT at handler entry - CVAL):
 * the full hardware + exception entry + context save + masked-section delay.
 * Kept as count/min/max/mean plus a histogram, in counter ticks (18.5 ns at
 * 54 MHz) converted to ns for reporting.
 *
 * Context switch: flint_timing_record_switch() collects samples from the
 * benchmark in demo/rpi4/bench.c (semaphore wake-up and task yield).
 */
#ifndef FLINT_TIMING_H
#define FLINT_TIMING_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Histogram bucket upper bounds in ns; the last bucket is open-ended. */
#define FLINT_LAT_BUCKETS  (10U)
extern const uint32_t g_flint_lat_bucket_ns[FLINT_LAT_BUCKETS - 1U];

typedef struct
{
    uint32_t count;
    uint32_t min_ns;
    uint32_t max_ns;
    uint32_t mean_ns;
    uint32_t hist[FLINT_LAT_BUCKETS];
} FlintLatencyStats;

/* ISR side (no locking needed: single writer). */
void flint_timing_irq_sample(uint64_t latency_ticks, uint64_t cnt_freq);

/* Snapshot (safe from tasks). reset=true starts a new window. */
void flint_timing_irq_stats(FlintLatencyStats *out, bool reset);

/* Generic accumulator used by the context-switch benchmark. */
typedef struct
{
    uint32_t count;
    uint64_t sum_ns;
    uint32_t min_ns;
    uint32_t max_ns;
} FlintTimingAcc;

void     flint_timing_acc_reset(FlintTimingAcc *a);
void     flint_timing_acc_add(FlintTimingAcc *a, uint32_t ns);
uint32_t flint_timing_acc_mean(const FlintTimingAcc *a);

/* Counter helpers. */
uint32_t flint_timing_ticks_to_ns(uint64_t ticks, uint64_t cnt_freq);

#ifdef __cplusplus
}
#endif

#endif /* FLINT_TIMING_H */
