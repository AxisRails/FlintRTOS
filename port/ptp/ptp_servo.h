/*
 * FlintRTOS - PTP clock servo: integer PI controller in the style of
 * linuxptp's pi.c, driving ptp_clock's phase step + frequency (ppb) knobs.
 *
 *   UNLOCKED : first sample -> step the clock onto the master.
 *   STEPPED  : second sample -> estimate the oscillator's frequency error from
 *              how far we drifted since the step, set it, step again -> LOCKED.
 *   LOCKED   : PI on the offset; adjust frequency only. PTP_SERVO_OUTLIERS
 *              consecutive offsets beyond PTP_SERVO_STEP_NS (master restart,
 *              lost lock) re-step and drop back to STEPPED; a single one is
 *              discarded as a timestamp outlier.
 * No floating point: gains are rationals per second of sync interval
 * (KP = 3/10, KI = 1/20 - tuned for software timestamps, see ptp_servo.c).
 */
#ifndef FLINT_PTP_SERVO_H
#define FLINT_PTP_SERVO_H

#include <stdint.h>

#define PTP_SERVO_STEP_NS   (1000000)    /* re-step if |offset| > 1 ms ...  */
#define PTP_SERVO_OUTLIERS  (3U)         /* ... this many samples in a row  */

typedef enum
{
    PTP_SERVO_UNLOCKED = 0,
    PTP_SERVO_STEPPED  = 1,
    PTP_SERVO_LOCKED   = 2
} PtpServoState;

typedef struct
{
    PtpServoState state;
    int64_t  drift_ppb;     /* estimated intrinsic oscillator error (ppb) */
    uint64_t ref_cnt;       /* counter of the sample that caused the step  */
    int64_t  ref_raw;       /* raw offset (t2-t1) right after that step    */
    uint64_t last_cnt;      /* counter of the previous LOCKED sample       */
    uint32_t outliers;      /* consecutive over-threshold samples          */
} PtpServo;

void ptp_servo_init(PtpServo *s);

/*
 * Feed one Sync measurement.
 *   raw_ns   : t2 - t1 (includes path delay), in the current clock timescale
 *   delay_ns : filtered mean path delay, or -1 if not measured yet
 *   rx_cnt   : counter tick of t2 (for interval measurement)
 * Applies the correction to ptp_clock itself. Returns the offset from master
 * used (ns), for reporting.
 */
int64_t ptp_servo_sample(PtpServo *s, int64_t raw_ns, int64_t delay_ns, uint64_t rx_cnt);

const char *ptp_servo_state_name(PtpServoState st);

#endif /* FLINT_PTP_SERVO_H */
