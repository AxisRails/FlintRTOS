/*
 * FlintRTOS - Queue Management (manifest 1). TCB code (MISRA C:2023).
 * Copy-semantics circular FIFO with blocking send/receive and timeouts, built
 * on the scheduler's event lists (task.h). Foundation for semaphores/mutexes
 * (see semphr.h). A zero item size is permitted for semaphores.
 *
 * Timeout model (simplified): a blocked caller waits up to xTicksToWait, wakes
 * on the event or the timeout, then re-checks once. This is functional but not
 * perfectly fair across repeated partial timeouts (a TimeOut_t refinement is a
 * later step).
 */
#include "queue.h"
#include "task.h"
#include "list.h"
#include "portable.h"

#include <stddef.h>
#include <stdint.h>

extern void *memcpy(void *, const void *, unsigned long);

typedef struct QueueDefinition
{
    uint8_t     *pcHead;
    uint8_t     *pcTail;
    uint8_t     *pcWriteTo;
    uint8_t     *pcReadFrom;
    UBaseType_t  uxLength;
    UBaseType_t  uxItemSize;
    volatile UBaseType_t uxMessagesWaiting;
    List_t       xTasksWaitingToSend;
    List_t       xTasksWaitingToReceive;
} Queue_t;

QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize)
{
    Queue_t *pxQueue;
    size_t   xStorage;

    if (uxQueueLength == 0U)          /* itemSize 0 allowed (semaphores) */
    {
        return NULL;
    }

    xStorage = (size_t)uxQueueLength * (size_t)uxItemSize;
    pxQueue  = (Queue_t *)pvPortMalloc(sizeof(Queue_t) + xStorage);
    if (pxQueue == NULL) { return NULL; }

    pxQueue->pcHead     = (uint8_t *)pxQueue + sizeof(Queue_t);
    pxQueue->pcTail     = pxQueue->pcHead + xStorage;
    pxQueue->uxLength   = uxQueueLength;
    pxQueue->uxItemSize = uxItemSize;
    pxQueue->pcWriteTo  = pxQueue->pcHead;
    pxQueue->pcReadFrom = pxQueue->pcHead +
                          ((uxQueueLength - 1U) * ((uxItemSize > 0U) ? uxItemSize : 1U));
    pxQueue->uxMessagesWaiting = 0U;
    vListInitialise(&(pxQueue->xTasksWaitingToSend));
    vListInitialise(&(pxQueue->xTasksWaitingToReceive));

    return (QueueHandle_t)pxQueue;
}

void vQueueDelete(QueueHandle_t xQueue) { vPortFree(xQueue); }

static void prvCopyIn(Queue_t *pxQueue, const void *pvItem, BaseType_t xToFront)
{
    if (pxQueue->uxItemSize > 0U)
    {
        if (xToFront == pdFALSE)
        {
            (void)memcpy(pxQueue->pcWriteTo, pvItem, (size_t)pxQueue->uxItemSize);
            pxQueue->pcWriteTo += pxQueue->uxItemSize;
            if (pxQueue->pcWriteTo >= pxQueue->pcTail) { pxQueue->pcWriteTo = pxQueue->pcHead; }
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
    }
    pxQueue->uxMessagesWaiting++;
}

static void prvCopyOut(Queue_t *pxQueue, void *pvBuffer)
{
    if (pxQueue->uxItemSize > 0U)
    {
        pxQueue->pcReadFrom += pxQueue->uxItemSize;
        if (pxQueue->pcReadFrom >= pxQueue->pcTail) { pxQueue->pcReadFrom = pxQueue->pcHead; }
        (void)memcpy(pvBuffer, pxQueue->pcReadFrom, (size_t)pxQueue->uxItemSize);
    }
    pxQueue->uxMessagesWaiting--;
}

static BaseType_t prvSend(Queue_t *pxQueue, const void *pvItem,
                          TickType_t xTicksToWait, BaseType_t xToFront)
{
    for (;;)
    {
        BaseType_t xDone = pdFAIL;
        BaseType_t xYield = pdFALSE;

        taskENTER_CRITICAL();
        if (pxQueue->uxMessagesWaiting < pxQueue->uxLength)
        {
            prvCopyIn(pxQueue, pvItem, xToFront);
            xYield = xTaskRemoveFromEventList(&(pxQueue->xTasksWaitingToReceive));
            xDone  = pdPASS;
        }
        taskEXIT_CRITICAL();

        if (xDone == pdPASS)
        {
            if (xYield != pdFALSE) { taskYIELD(); }
            return pdPASS;
        }
        if (xTicksToWait == 0U) { return errQUEUE_FULL; }

        taskENTER_CRITICAL();
        vTaskPlaceOnEventList(&(pxQueue->xTasksWaitingToSend), xTicksToWait);
        taskEXIT_CRITICAL();
        taskYIELD();

        /* Re-check once after waking; if still full, we timed out. */
        taskENTER_CRITICAL();
        if (pxQueue->uxMessagesWaiting < pxQueue->uxLength)
        {
            prvCopyIn(pxQueue, pvItem, xToFront);
            xYield = xTaskRemoveFromEventList(&(pxQueue->xTasksWaitingToReceive));
            taskEXIT_CRITICAL();
            if (xYield != pdFALSE) { taskYIELD(); }
            return pdPASS;
        }
        taskEXIT_CRITICAL();
        return errQUEUE_FULL;
    }
}

BaseType_t xQueueSendToBack(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait)
{
    return prvSend((Queue_t *)xQueue, pvItemToQueue, xTicksToWait, pdFALSE);
}

BaseType_t xQueueSendToFront(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait)
{
    return prvSend((Queue_t *)xQueue, pvItemToQueue, xTicksToWait, pdTRUE);
}

BaseType_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait)
{
    Queue_t *pxQueue = (Queue_t *)xQueue;

    for (;;)
    {
        BaseType_t xDone = pdFAIL;
        BaseType_t xYield = pdFALSE;

        taskENTER_CRITICAL();
        if (pxQueue->uxMessagesWaiting > 0U)
        {
            prvCopyOut(pxQueue, pvBuffer);
            xYield = xTaskRemoveFromEventList(&(pxQueue->xTasksWaitingToSend));
            xDone  = pdPASS;
        }
        taskEXIT_CRITICAL();

        if (xDone == pdPASS)
        {
            if (xYield != pdFALSE) { taskYIELD(); }
            return pdPASS;
        }
        if (xTicksToWait == 0U) { return errQUEUE_EMPTY; }

        taskENTER_CRITICAL();
        vTaskPlaceOnEventList(&(pxQueue->xTasksWaitingToReceive), xTicksToWait);
        taskEXIT_CRITICAL();
        taskYIELD();

        taskENTER_CRITICAL();
        if (pxQueue->uxMessagesWaiting > 0U)
        {
            prvCopyOut(pxQueue, pvBuffer);
            xYield = xTaskRemoveFromEventList(&(pxQueue->xTasksWaitingToSend));
            taskEXIT_CRITICAL();
            if (xYield != pdFALSE) { taskYIELD(); }
            return pdPASS;
        }
        taskEXIT_CRITICAL();
        return errQUEUE_EMPTY;
    }
}

QueueHandle_t xQueueCreateCounting(UBaseType_t uxMaxCount, UBaseType_t uxInitialCount)
{
    QueueHandle_t xQueue = xQueueCreate(uxMaxCount, 0U);
    if (xQueue != NULL)
    {
        ((Queue_t *)xQueue)->uxMessagesWaiting = uxInitialCount;
    }
    return xQueue;
}

QueueHandle_t xQueueCreateMutex(void)
{
    QueueHandle_t xQueue = xQueueCreate(1U, 0U);
    if (xQueue != NULL)
    {
        (void)xQueueSendToBack(xQueue, NULL, 0U);   /* mutex starts available */
    }
    return xQueue;
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
