/*
 * FlintRTOS - PTP disciplined clock. Raw time comes from the ARM generic-timer
 * physical counter (CNTPCT, high resolution); a servo-maintained offset makes it
 * track the PTP master. This is a software-timestamp clock; the GENET MAC's
 * IEEE-1588 hardware timestamps replace the software reads when available.
 */
#ifndef FLINT_PTP_CLOCK_H
#define FLINT_PTP_CLOCK_H

#include "ptp_config.h"
#include <stdint.h>

/* Raw monotonic nanoseconds from the hardware counter (undisciplined). */
int64_t ptp_clock_raw_ns(void);

/* Disciplined PTP time = raw + offset. */
void    ptp_clock_now(TimeInternal *t);
int64_t ptp_clock_now_ns(void);

/* Servo hooks: jump by delta_ns, or read/adjust the current offset. */
void    ptp_clock_step(int64_t delta_ns);
int64_t ptp_clock_offset_ns(void);

#endif /* FLINT_PTP_CLOCK_H */
