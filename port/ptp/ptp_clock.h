/*
 * FlintRTOS - PTP disciplined clock.
 *
 * The time base is the ARM generic-timer physical counter (CNTPCT, 54 MHz on
 * the Pi 4 = 18.5 ns resolution). The PTP clock is a piecewise-linear model
 * on top of it:
 *
 *     ptp_ns(cnt) = base_ns + d + d * ppb / 1e9,   d = ticks_to_ns(cnt - base_cnt)
 *
 * The servo steers it with a phase step (ptp_clock_step) and a frequency
 * adjustment in parts-per-billion (ptp_clock_adjfreq). Both re-anchor the
 * model at "now", so earlier time stays continuous. Integer-only (the firmware
 * is built -mgeneral-regs-only). Not thread-safe: call from one task.
 */
#ifndef FLINT_PTP_CLOCK_H
#define FLINT_PTP_CLOCK_H

#include "ptp_config.h"
#include <stdint.h>

#define PTP_CLOCK_MAX_PPB   (500000)   /* +/- 500 ppm slew limit */

/* Raw hardware counter (ticks) and its frequency (Hz). */
uint64_t ptp_clock_read_cnt(void);
uint64_t ptp_clock_cnt_freq(void);

/* Disciplined PTP time (ns since the PTP epoch) of a counter sample. */
int64_t ptp_clock_from_cnt(uint64_t cnt);

/* Disciplined PTP time now. */
int64_t ptp_clock_now_ns(void);
void    ptp_clock_now(TimeInternal *t);

/* Jump the clock by delta_ns (phase step). Bumps the step epoch. */
void    ptp_clock_step(int64_t delta_ns);

/* Set the frequency correction (ppb, clamped to +/- PTP_CLOCK_MAX_PPB). */
void    ptp_clock_adjfreq(int32_t ppb);
int32_t ptp_clock_get_freq(void);

/* Incremented on every step: lets the protocol discard exchanges that
   straddle a step (their timestamps come from different timescales). */
uint32_t ptp_clock_epoch(void);

/* Legacy accessor (raw monotonic ns since boot, undisciplined). */
int64_t ptp_clock_raw_ns(void);

#endif /* FLINT_PTP_CLOCK_H */
