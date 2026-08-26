/*
 * FlintRTOS - Event Groups (manifest 2, extension). Public API.
 * Tasks block/wait on combinations of bitwise flags. configUSE_EVENT_GROUPS==1.
 */
#ifndef FLINT_EVENT_GROUPS_H
#define FLINT_EVENT_GROUPS_H

#include "FlintRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *EventGroupHandle_t;
typedef TickType_t EventBits_t;   /* one bit per event flag */

EventGroupHandle_t xEventGroupCreate(void);
void               vEventGroupDelete(EventGroupHandle_t xEventGroup);
EventBits_t        xEventGroupSetBits(EventGroupHandle_t xEventGroup, EventBits_t uxBitsToSet);
EventBits_t        xEventGroupClearBits(EventGroupHandle_t xEventGroup, EventBits_t uxBitsToClear);
EventBits_t        xEventGroupWaitBits(EventGroupHandle_t xEventGroup,
                                       EventBits_t uxBitsToWaitFor,
                                       BaseType_t xClearOnExit,
                                       BaseType_t xWaitForAllBits,
                                       TickType_t xTicksToWait);

#ifdef __cplusplus
}
#endif

#endif /* FLINT_EVENT_GROUPS_H */
