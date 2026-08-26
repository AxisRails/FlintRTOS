/*
 * FlintRTOS - Software Timers (manifest 2). TCB code (MISRA C:2023).
 * Skeleton: a command queue (queue.c) feeds the timer service task, which
 * maintains an ordered list (list.c) of active timers and invokes callbacks
 * when they expire. Full body lands once queue blocking is wired to tasks.c.
 * Compiles to nothing unless configUSE_TIMERS == 1.
 */
#include "FlintRTOS.h"

#if (configUSE_TIMERS == 1)

#include "timers.h"
#include "list.h"
#include "queue.h"

/* TODO(next increment): timer service task + command queue + ordered list.
   API is declared in timers.h; storage/logic added when blocking queues land. */

void *pvTimerGetTimerID(TimerHandle_t xTimer)
{
    return (void *)xTimer; /* placeholder */
}

#endif /* configUSE_TIMERS */
