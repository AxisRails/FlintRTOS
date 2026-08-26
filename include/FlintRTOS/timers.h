/*
 * FlintRTOS - Software Timers (manifest 2, extension). Public API.
 * Periodic/one-shot callbacks serviced by the timer service task, without
 * consuming a hardware timer per timer. Enabled by configUSE_TIMERS == 1.
 */
#ifndef FLINT_TIMERS_H
#define FLINT_TIMERS_H

#include "FlintRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *TimerHandle_t;
typedef void (*TimerCallbackFunction_t)(TimerHandle_t xTimer);

TimerHandle_t xTimerCreate(const char *pcName, TickType_t xPeriodInTicks,
                           BaseType_t xAutoReload, void *pvTimerID,
                           TimerCallbackFunction_t pxCallback);
BaseType_t    xTimerStart(TimerHandle_t xTimer, TickType_t xTicksToWait);
BaseType_t    xTimerStop(TimerHandle_t xTimer, TickType_t xTicksToWait);
BaseType_t    xTimerChangePeriod(TimerHandle_t xTimer, TickType_t xNewPeriod,
                                 TickType_t xTicksToWait);
void         *pvTimerGetTimerID(TimerHandle_t xTimer);

#ifdef __cplusplus
}
#endif

#endif /* FLINT_TIMERS_H */
