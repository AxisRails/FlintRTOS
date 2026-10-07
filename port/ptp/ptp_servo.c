/*
 * FlintRTOS - PTP clock servo (integer PI). See ptp_servo.h.
 *
 * Sign convention: offset = slave - master. A positive offset means we are
 * ahead, so the clock is stepped by -offset and slowed (negative ppb).
 * drift_ppb is how fast the raw oscillator runs relative to the master; the
 * frequency applied to ptp_clock is -(drift + proportional + integral).
 */
#include "ptp_servo.h"
#include "ptp_clock.h"

#define NS_PER_S      (1000000000LL)
/* Gains tuned for SOFTWARE timestamps (tests/ptp_sim.c, 8-20 us jitter):
   linuxptp's hardware defaults (0.7/0.3) pass timestamp noise straight into
   the frequency; 0.3/0.05 halves the worst-case offset. */
#define KP_NUM        (3)
#define KP_DEN        (10)
#define KI_NUM        (1)
#define KI_DEN        (20)

static int64_t abs64(int64_t v) { return (v < 0) ? -v : v; }

static int64_t clamp_ppb(int64_t v)
{
    if (v >  PTP_CLOCK_MAX_PPB) { return  PTP_CLOCK_MAX_PPB; }
    if (v < -PTP_CLOCK_MAX_PPB) { return -PTP_CLOCK_MAX_PPB; }
    return v;
}

/* Counter span -> ns (intervals here are seconds, so this cannot overflow). */
static int64_t span_ns(uint64_t from, uint64_t to)
{
    const uint64_t f = ptp_clock_cnt_freq();
    uint64_t dt = to - from;
    if (f == 0U) { return 0; }
    return (int64_t)(((dt / f) * (uint64_t)NS_PER_S) + (((dt % f) * (uint64_t)NS_PER_S) / f));
}

void ptp_servo_init(PtpServo *s)
{
    s->state     = PTP_SERVO_UNLOCKED;
    s->drift_ppb = 0;
    s->ref_cnt   = 0U;
    s->ref_raw   = 0;
    s->last_cnt  = 0U;
    s->outliers  = 0U;
}

const char *ptp_servo_state_name(PtpServoState st)
{
    switch (st)
    {
        case PTP_SERVO_UNLOCKED: return "UNLOCKED";
        case PTP_SERVO_STEPPED:  return "STEPPED";
        case PTP_SERVO_LOCKED:   return "LOCKED";
        default:                 return "?";
    }
}

/* Step so that the offset (raw - delay) becomes zero; remember the reference. */
static void do_step(PtpServo *s, int64_t raw_ns, int64_t d, uint64_t rx_cnt)
{
    int64_t offset = raw_ns - d;
    ptp_clock_step(-offset);
    s->ref_cnt = rx_cnt;
    s->ref_raw = d;            /* raw offset immediately after the step */
}

int64_t ptp_servo_sample(PtpServo *s, int64_t raw_ns, int64_t delay_ns, uint64_t rx_cnt)
{
    const int64_t d      = (delay_ns >= 0) ? delay_ns : 0;
    const int64_t offset = raw_ns - d;

    switch (s->state)
    {
        case PTP_SERVO_UNLOCKED:
            do_step(s, raw_ns, d, rx_cnt);
            s->state = PTP_SERVO_STEPPED;
            break;

        case PTP_SERVO_STEPPED:
        {
            int64_t T = span_ns(s->ref_cnt, rx_cnt);
            if (T < (NS_PER_S / 8))
            {
                break;                         /* too close to measure a slope */
            }
            /*
             * Since the step, the raw offset moved from ref_raw to raw_ns with
             * frequency f_applied in effect. Observed slope (ppb) =
             * intrinsic drift + f_applied, so drift = slope - f_applied.
             */
            {
                int64_t slope = ((raw_ns - s->ref_raw) * NS_PER_S) / T;
                s->drift_ppb  = clamp_ppb(slope - (int64_t)ptp_clock_get_freq());
            }
            ptp_clock_adjfreq((int32_t)clamp_ppb(-s->drift_ppb));
            if (delay_ns >= 0)
            {
                /* Frequency known and path delay measured: final step, lock. */
                do_step(s, raw_ns, d, rx_cnt);
                s->state    = PTP_SERVO_LOCKED;
                s->last_cnt = rx_cnt;
            }
            else
            {
                /* No delay yet. Do NOT step (that would invalidate the pending
                   Delay_Req exchange); just re-reference and keep refining. */
                s->ref_cnt = rx_cnt;
                s->ref_raw = raw_ns;
            }
            break;
        }

        case PTP_SERVO_LOCKED:
        default:
        {
            if (abs64(offset) > PTP_SERVO_STEP_NS)
            {
                /* One outlier (a late software timestamp) is ignored; several
                   in a row mean the master really jumped: re-acquire. */
                s->outliers++;
                if (s->outliers >= PTP_SERVO_OUTLIERS)
                {
                    s->outliers = 0U;
                    do_step(s, raw_ns, d, rx_cnt);
                    s->state = PTP_SERVO_STEPPED;
                }
                break;
            }
            s->outliers = 0U;
            {
                /* Gains are per second: scale by the actual sync interval. */
                int64_t T = span_ns(s->last_cnt, rx_cnt);
                int64_t p, i;
                s->last_cnt = rx_cnt;
                if ((T <= 0) || (T > (8 * NS_PER_S))) { T = NS_PER_S; }
                p = (offset * KP_NUM * NS_PER_S) / (KP_DEN * T);
                i = (offset * KI_NUM * NS_PER_S) / (KI_DEN * T);
                s->drift_ppb = clamp_ppb(s->drift_ppb + i);
                ptp_clock_adjfreq((int32_t)clamp_ppb(-(s->drift_ppb + p)));
            }
            break;
        }
    }
    return offset;
}
