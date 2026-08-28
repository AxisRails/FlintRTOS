/*
 * FlintRTOS - PTP time arithmetic on ptpd's TimeInternal (seconds+nanoseconds).
 * Reimplemented from ptpd arith.c (BSD) for the bare-metal port.
 */
#ifndef FLINT_PTP_TIME_H
#define FLINT_PTP_TIME_H

#include "ptp_config.h"
#include <stdint.h>

void   ptp_time_normalize(TimeInternal *r);
void   ptp_time_add(TimeInternal *r, const TimeInternal *x, const TimeInternal *y);
void   ptp_time_sub(TimeInternal *r, const TimeInternal *x, const TimeInternal *y);
void   ptp_time_div2(TimeInternal *r);
int64_t ptp_time_to_ns(const TimeInternal *t);
void   ptp_ns_to_time(int64_t ns, TimeInternal *t);

/* Convert an on-wire Timestamp (already host-order fields) to/from TimeInternal. */
void   ptp_ts_to_internal(const Timestamp *ext, TimeInternal *in);
void   ptp_internal_to_ts(const TimeInternal *in, Timestamp *ext);

#endif /* FLINT_PTP_TIME_H */
