/*
 * FlintRTOS - Event Groups (manifest 2). TCB code (MISRA C:2023).
 * Skeleton: an EventBits_t word plus a list of tasks waiting on bit patterns.
 * Set/clear/wait bodies land with the task event-list wiring. Compiles to
 * nothing unless configUSE_EVENT_GROUPS == 1.
 */
#include "FlintRTOS.h"

#if (configUSE_EVENT_GROUPS == 1)

#include "event_groups.h"
#include "list.h"

/* TODO(next increment): EventGroup_t { EventBits_t uxEventBits; List_t xTasksWaiting; }
   with blocking integrated into tasks.c. API declared in event_groups.h. */

#endif /* configUSE_EVENT_GROUPS */
