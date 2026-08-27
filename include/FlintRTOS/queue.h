/*
 * FlintRTOS - Queue Management (manifest 1): public API.
 * FIFO message queues; the structural foundation for semaphores and mutexes.
 * TCB code (MISRA C:2023). FreeRTOS-style API.
 *
 * v0.2 scope: create + non-blocking send/receive + counts (host-testable).
 * Blocking with timeouts (task event lists) integrates with tasks.c next.
 */
#ifndef FLINT_QUEUE_H
#define FLINT_QUEUE_H

#include "FlintRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *QueueHandle_t;

QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize);
void          vQueueDelete(QueueHandle_t xQueue);
QueueHandle_t xQueueCreateCounting(UBaseType_t uxMaxCount, UBaseType_t uxInitialCount);
QueueHandle_t xQueueCreateMutex(void);

/* Non-blocking send/receive (xTicksToWait honoured once blocking lands). */
BaseType_t    xQueueSendToBack(QueueHandle_t xQueue, const void *pvItemToQueue,
                               TickType_t xTicksToWait);
BaseType_t    xQueueSendToFront(QueueHandle_t xQueue, const void *pvItemToQueue,
                                TickType_t xTicksToWait);
BaseType_t    xQueueReceive(QueueHandle_t xQueue, void *pvBuffer,
                            TickType_t xTicksToWait);

UBaseType_t   uxQueueMessagesWaiting(QueueHandle_t xQueue);
UBaseType_t   uxQueueSpacesAvailable(QueueHandle_t xQueue);

#define xQueueSend(q, item, wait)  xQueueSendToBack((q), (item), (wait))

#ifdef __cplusplus
}
#endif

#endif /* FLINT_QUEUE_H */
