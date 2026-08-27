/*
 * FlintRTOS - Semaphores & Mutexes (manifest 1): built on Queue Management.
 * A semaphore is a zero-item-size queue whose message count is the token count.
 * TCB code (MISRA C:2023). FreeRTOS-style API.
 *
 * Note: this mutex does not yet implement priority inheritance (a later step);
 * it provides mutual exclusion via a binary token.
 */
#ifndef FLINT_SEMPHR_H
#define FLINT_SEMPHR_H

#include "FlintRTOS.h"
#include "queue.h"

typedef QueueHandle_t SemaphoreHandle_t;

/* Binary semaphore: starts EMPTY (must be given before it can be taken). */
#define xSemaphoreCreateBinary()                 xQueueCreate(1U, 0U)

/* Counting semaphore with a maximum and initial token count. */
#define xSemaphoreCreateCounting(uxMax, uxInit)  xQueueCreateCounting((uxMax), (uxInit))

/* Mutex: starts AVAILABLE. */
#define xSemaphoreCreateMutex()                  xQueueCreateMutex()

#define xSemaphoreTake(xSem, xTicksToWait)       xQueueReceive((xSem), NULL, (xTicksToWait))
#define xSemaphoreGive(xSem)                     xQueueSendToBack((xSem), NULL, 0U)

#define vSemaphoreDelete(xSem)                   vQueueDelete((xSem))
#define uxSemaphoreGetCount(xSem)                uxQueueMessagesWaiting((xSem))

#endif /* FLINT_SEMPHR_H */
