/*
 * FlintRTOS - umbrella kernel header. Include this from application code.
 *
 * Pulls in the application's FlintRTOSConfig.h (which selects enabled modules),
 * the project return codes, and the portability layer. Individual APIs
 * (task.h, queue.h, ...) are included by the application as needed.
 */
#ifndef FLINTRTOS_H
#define FLINTRTOS_H

#include "FlintRTOSConfig.h"   /* application-provided; on the include path */
#include "projdefs.h"
#include "portable.h"

/* ---- Configuration defaults (a real config sets these explicitly) -------- */
#ifndef configTICK_RATE_HZ
#define configTICK_RATE_HZ        (1000U)
#endif
#ifndef configMAX_PRIORITIES
#define configMAX_PRIORITIES      (32U)
#endif
#ifndef configMINIMAL_STACK_SIZE
#define configMINIMAL_STACK_SIZE  (512U)   /* in StackType_t words */
#endif
#ifndef configUSE_PREEMPTION
#define configUSE_PREEMPTION      (1)
#endif
#ifndef configUSE_16_BIT_TICKS
#define configUSE_16_BIT_TICKS    (0)
#endif

#endif /* FLINTRTOS_H */
