/*
 * FlintRTOS - Queue Management (manifest 1). TCB code (MISRA C:2023).
 * Copy-semantics circular FIFO. Foundation for semaphores/mutexes.
 *
 * Blocking (xTicksToWait > 0 suspending the caller on an event list) is wired
 * to tasks.c in the next increment; this version implements the data movement
 * and counts, returning immediately when full/empty.
 */
#include "queue.h"
#include "portable.h"

#include <stddef.h>
#include <stdint.h>
/* memcpy provided by kernel/flint_libc.c on target; libc on host */
extern void *memcpy(void *, const void *, unsigned long);

typedef struct QueueDefinition
{
    uint8_t     *pcHead;         /* start of the storage area              */
    uint8_t     *pcTail;         /* one past the end of the storage area   */
    uint8_t     *pcWriteTo;      /* next free slot                         */
    uint8_t     *pcReadFrom;     /* last-read slot                         */
    UBaseType_t  uxLength;       /* number of items the queue can hold     */
    UBaseType_t  uxItemSize;     /* size of each item in bytes             */
    volatile UBaseType_t uxMessagesWaiting;
} Queue_t;

QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize)
{
    Queue_t *pxQueue;
    size_t   xStorage;

    if ((uxQueueLength == 0U) || (uxItemSize == 0U))
    {
        return NULL;
    }

    xStorage = (size_t)uxQueueLength * (size_t)uxItemSize;
    pxQueue  = (Queue_t *)pvPortMalloc(sizeof(Queue_t) + xStorage);
    if (pxQueue == NULL)
    {
        return NULL;
    }

    pxQueue->pcHead    = (uint8_t *)pxQueue + sizeof(Queue_t);
    pxQueue->pcTail    = pxQueue->pcHead + xStorage;
    pxQueue->uxLength  = uxQueueLength;
    pxQueue->uxItemSize = uxItemSize;
    pxQueue->pcWriteTo = pxQueue->pcHead;
    pxQueue->pcReadFrom = pxQueue->pcHead + ((uxQueueLength - 1U) * uxItemSize);
    pxQueue->uxMessagesWaiting = 0U;

    return (QueueHandle_t)pxQueue;
}

void vQueueDelete(QueueHandle_t xQueue)
{
    vPortFree(xQueue);
}

static void prvCopyIn(Queue_t *pxQueue, const void *pvItem, BaseType_t xToFront)
{
    if (xToFront == pdFALSE)
    {
        (void)memcpy(pxQueue->pcWriteTo, pvItem, (size_t)pxQueue->uxItemSize);
        pxQueue->pcWriteTo += pxQueue->uxItemSize;
        if (pxQueue->pcWriteTo >= pxQueue->pcTail)
        {
            pxQueue->pcWriteTo = pxQueue->pcHead;
        }
    }
    else
    {
        (void)memcpy(pxQueue->pcReadFrom, pvItem, (size_t)pxQueue->uxItemSize);
        if (pxQueue->pcReadFrom == pxQueue->pcHead)
        {
            pxQueue->pcReadFrom = pxQueue->pcTail - pxQueue->uxItemSize;
        }
        else
        {
            pxQueue->pcReadFrom -= pxQueue->uxItemSize;
        }
    }
    pxQueue->uxMessagesWaiting++;
}

BaseType_t xQueueSendToBack(QueueHandle_t xQueue, const void *pvItemToQueue,
                            TickType_t xTicksToWait)
{
    Queue_t   *pxQueue = (Queue_t *)xQueue;
    BaseType_t xResult = pdFAIL;

    (void)xTicksToWait; /* blocking not yet wired */

    portENTER_CRITICAL();
    {
        if (pxQueue->uxMessagesWaiting < pxQueue->uxLength)
        {
            prvCopyIn(pxQueue, pvItemToQueue, pdFALSE);
            xResult = pdPASS;
        }
    }
    portEXIT_CRITICAL();

    return xResult;
}

BaseType_t xQueueSendToFront(QueueHandle_t xQueue, const void *pvItemToQueue,
                             TickType_t xTicksToWait)
{
    Queue_t   *pxQueue = (Queue_t *)xQueue;
    BaseType_t xResult = pdFAIL;

    (void)xTicksToWait;

    portENTER_CRITICAL();
    {
        if (pxQueue->uxMessagesWaiting < pxQueue->uxLength)
        {
            prvCopyIn(pxQueue, pvItemToQueue, pdTRUE);
            xResult = pdPASS;
        }
    }
    portEXIT_CRITICAL();

    return xResult;
}

BaseType_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait)
{
    Queue_t   *pxQueue = (Queue_t *)xQueue;
    BaseType_t xResult = pdFAIL;

    (void)xTicksToWait;

    portENTER_CRITICAL();
    {
        if (pxQueue->uxMessagesWaiting > 0U)
        {
            pxQueue->pcReadFrom += pxQueue->uxItemSize;
            if (pxQueue->pcReadFrom >= pxQueue->pcTail)
            {
                pxQueue->pcReadFrom = pxQueue->pcHead;
            }
            (void)memcpy(pvBuffer, pxQueue->pcReadFrom, (size_t)pxQueue->uxItemSize);
            pxQueue->uxMessagesWaiting--;
            xResult = pdPASS;
        }
    }
    portEXIT_CRITICAL();

    return xResult;
}

UBaseType_t uxQueueMessagesWaiting(QueueHandle_t xQueue)
{
    return ((Queue_t *)xQueue)->uxMessagesWaiting;
}

UBaseType_t uxQueueSpacesAvailable(QueueHandle_t xQueue)
{
    Queue_t *pxQueue = (Queue_t *)xQueue;
    return pxQueue->uxLength - pxQueue->uxMessagesWaiting;
}
