/*
 * FlintRTOS - PTP time arithmetic. Reimplemented from ptpd arith.c (BSD).
 */
#include "ptp_time.h"

#define NS_PER_S  (1000000000)

void ptp_time_normalize(TimeInternal *r)
{
    r->seconds     += r->nanoseconds / NS_PER_S;
    r->nanoseconds  = r->nanoseconds % NS_PER_S;

    if ((r->seconds > 0) && (r->nanoseconds < 0))
    {
        r->seconds     -= 1;
        r->nanoseconds += NS_PER_S;
    }
    else if ((r->seconds < 0) && (r->nanoseconds > 0))
    {
        r->seconds     += 1;
        r->nanoseconds -= NS_PER_S;
    }
    else
    {
        /* already normalized */
    }
}

void ptp_time_add(TimeInternal *r, const TimeInternal *x, const TimeInternal *y)
{
    r->seconds     = x->seconds + y->seconds;
    r->nanoseconds = x->nanoseconds + y->nanoseconds;
    ptp_time_normalize(r);
}

void ptp_time_sub(TimeInternal *r, const TimeInternal *x, const TimeInternal *y)
{
    r->seconds     = x->seconds - y->seconds;
    r->nanoseconds = x->nanoseconds - y->nanoseconds;
    ptp_time_normalize(r);
}

void ptp_time_div2(TimeInternal *r)
{
    int64_t ns = ptp_time_to_ns(r) / 2;
    ptp_ns_to_time(ns, r);
}

int64_t ptp_time_to_ns(const TimeInternal *t)
{
    return ((int64_t)t->seconds * NS_PER_S) + (int64_t)t->nanoseconds;
}

void ptp_ns_to_time(int64_t ns, TimeInternal *t)
{
    t->seconds     = (Integer32)(ns / NS_PER_S);
    t->nanoseconds = (Integer32)(ns % NS_PER_S);
    ptp_time_normalize(t);
}

void ptp_ts_to_internal(const Timestamp *ext, TimeInternal *in)
{
    uint64_t secs = ((uint64_t)ext->secondsField.msb << 32) | ext->secondsField.lsb;
    in->seconds     = (Integer32)secs;
    in->nanoseconds = (Integer32)ext->nanosecondsField;
}

void ptp_internal_to_ts(const TimeInternal *in, Timestamp *ext)
{
    ext->secondsField.lsb = (uint32_t)in->seconds;
    ext->secondsField.msb = 0U;
    ext->nanosecondsField = (UInteger32)in->nanoseconds;
}
