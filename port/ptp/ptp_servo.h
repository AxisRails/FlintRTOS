/*
 * FlintRTOS - PTP clock servo (integer PI). No floating point (the firmware is
 * built -mgeneral-regs-only), so the controller uses fixed-point shifts.
 */
#ifndef FLINT_PTP_SERVO_H
#define FLINT_PTP_SERVO_H

#include <stdint.h>

typedef struct
{
    int64_t integral;    /* accumulated offset (ns), bounded            */
    int     kp_shift;    /* proportional gain = 1 >> kp_shift           */
    int     ki_shift;    /* integral gain    = 1 >> ki_shift            */
} PtpServo;

void    ptp_servo_init(PtpServo *s);

/*
 * Feed the measured offsetFromMaster (ns). Returns the correction to APPLY to
 * the local clock (ns): negative of the control effort. A large offset yields a
 * full step (and resets the integrator); small offsets are slewed via PI.
 */
int64_t ptp_servo_run(PtpServo *s, int64_t offset_ns);

#endif /* FLINT_PTP_SERVO_H */
