/*
 * FlintRTOS - PTP clock servo (integer PI).
 */
#include "ptp_servo.h"

#define STEP_THRESHOLD_NS   (1000000)     /* 1 ms: hard-step above this      */
#define INTEGRAL_CLAMP_NS   (100000000)   /* +/- 100 ms integrator windup cap */

void ptp_servo_init(PtpServo *s)
{
    s->integral = 0;
    s->kp_shift = 3;    /* Kp = 1/8   */
    s->ki_shift = 8;    /* Ki = 1/256 */
}

int64_t ptp_servo_run(PtpServo *s, int64_t offset_ns)
{
    int64_t abs_off = (offset_ns < 0) ? -offset_ns : offset_ns;

    if (abs_off > STEP_THRESHOLD_NS)
    {
        /* Far off: step the clock and reset the integrator. */
        s->integral = 0;
        return -offset_ns;
    }

    s->integral += offset_ns;
    if (s->integral >  INTEGRAL_CLAMP_NS) { s->integral =  INTEGRAL_CLAMP_NS; }
    if (s->integral < -INTEGRAL_CLAMP_NS) { s->integral = -INTEGRAL_CLAMP_NS; }

    {
        int64_t p = offset_ns >> s->kp_shift;
        int64_t i = s->integral >> s->ki_shift;
        return -(p + i);
    }
}
