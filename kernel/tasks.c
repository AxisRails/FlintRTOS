/*
 * FlintRTOS - Task Management (manifest 1). TCB code (MISRA C:2023).
 *
 * Fixed-priority preemptive scheduler with per-priority ready lists, a delayed
 * list (timed blocking), a suspended list (indefinite blocking), and event-list
 * blocking used by queues/semaphores (vTaskPlaceOnEventList /
 * xTaskRemoveFromEventList). Context switching is delegated to the port.
 *
 * Build-verified for AArch64; on-target runtime bring-up pending.
 */
#include "task.h"
#include "list.h"

#include <stddef.h>
#include <stdint.h>

/* Bring-up scheduler trace: bounded so it cannot flood the console. Enabled
 * only for the firmware build (Makefile passes -DFLINT_SCHED_TRACE=1); the host
 * unit-test build leaves it off so tasks.c needs no UART. */
#ifndef FLINT_SCHED_TRACE
#define FLINT_SCHED_TRACE 0
#endif
#define FLINT_TRACE_MAX   40U
#if (FLINT_SCHED_TRACE == 1)
#include "uart.h"
#endif

/* pxTopOfStack MUST be first: the port asm saves/restores SP via *pxCurrentTCB. */
struct tskTaskControlBlock
{
    volatile StackType_t *pxTopOfStack;
    ListItem_t            xStateListItem;   /* ready / delayed / suspended list */
    ListItem_t            xEventListItem;   /* a queue's waiting list           */
    UBaseType_t           uxPriority;
    StackType_t          *pxStack;
    char                  pcTaskName[configMAX_TASK_NAME_LEN];
};

TCB_t *volatile pxCurrentTCB = NULL;

static List_t     xReadyTasksLists[configMAX_PRIORITIES];
static List_t     xDelayedTaskList;         /* tasks blocked with a timeout      */
static List_t     xSuspendedTaskList;       /* tasks blocked indefinitely        */
static volatile UBaseType_t uxTopReadyPriority = 0U;
static volatile TickType_t  xTickCount        = 0U;
static volatile UBaseType_t uxCurrentNumberOfTasks = 0U;
static volatile BaseType_t  xSchedulerRunning = pdFALSE;

static void prvIdleTask(void *pvParameters);
static void prvInitialiseTaskLists(void);

static void prvAddTaskToReadyList(TCB_t *pxTCB)
{
    if (pxTCB->uxPriority > uxTopReadyPriority)
    {
        uxTopReadyPriority = pxTCB->uxPriority;
    }
    listSET_LIST_ITEM_VALUE(&(pxTCB->xStateListItem), (TickType_t)pxTCB->uxPriority);
    listSET_LIST_ITEM_OWNER(&(pxTCB->xStateListItem), pxTCB);
    vListInsertEnd(&(xReadyTasksLists[pxTCB->uxPriority]), &(pxTCB->xStateListItem));
}

BaseType_t xTaskCreate(TaskFunction_t pxTaskCode,
                       const char *pcName,
                       uint32_t usStackDepth,
                       void *pvParameters,
                       UBaseType_t uxPriority,
                       TaskHandle_t *pxCreatedTask)
{
    TCB_t       *pxNewTCB;
    StackType_t *pxStack;
    UBaseType_t  ux;

    if (uxPriority >= (UBaseType_t)configMAX_PRIORITIES)
    {
        uxPriority = (UBaseType_t)configMAX_PRIORITIES - 1U;
    }

    {
        static BaseType_t xListsInitialised = pdFALSE;
        if (xListsInitialised == pdFALSE)
        {
            prvInitialiseTaskLists();
            xListsInitialised = pdTRUE;
        }
    }

    pxStack = (StackType_t *)pvPortMalloc((size_t)usStackDepth * sizeof(StackType_t));
    if (pxStack == NULL)
    {
        return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }
    pxNewTCB = (TCB_t *)pvPortMalloc(sizeof(TCB_t));
    if (pxNewTCB == NULL)
    {
        vPortFree(pxStack);
        return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }

    pxNewTCB->pxStack    = pxStack;
    pxNewTCB->uxPriority = uxPriority;
    for (ux = 0U; ux < (UBaseType_t)configMAX_TASK_NAME_LEN; ux++)
    {
        char c = (pcName != NULL) ? pcName[ux] : '\0';
        pxNewTCB->pcTaskName[ux] = c;
        if (c == '\0') { break; }
    }
    pxNewTCB->pcTaskName[configMAX_TASK_NAME_LEN - 1] = '\0';

    vListInitialiseItem(&(pxNewTCB->xStateListItem));
    vListInitialiseItem(&(pxNewTCB->xEventListItem));
    listSET_LIST_ITEM_OWNER(&(pxNewTCB->xStateListItem), pxNewTCB);
    listSET_LIST_ITEM_OWNER(&(pxNewTCB->xEventListItem), pxNewTCB);
    /* Event lists order by priority: highest priority == lowest item value. */
    listSET_LIST_ITEM_VALUE(&(pxNewTCB->xEventListItem),
                            (TickType_t)((UBaseType_t)configMAX_PRIORITIES - uxPriority));

    {
        StackType_t *pxTopOfStack = &(pxStack[usStackDepth - 1U]);
        pxTopOfStack = (StackType_t *)(((uintptr_t)pxTopOfStack) &
                                       ~((uintptr_t)portBYTE_ALIGNMENT_MASK));
        pxNewTCB->pxTopOfStack =
            pxPortInitialiseStack(pxTopOfStack, (void (*)(void *))pxTaskCode, pvParameters);
    }

    taskENTER_CRITICAL();
    {
        uxCurrentNumberOfTasks++;
        if (pxCurrentTCB == NULL) { pxCurrentTCB = pxNewTCB; }
        prvAddTaskToReadyList(pxNewTCB);
    }
    taskEXIT_CRITICAL();

    if (pxCreatedTask != NULL) { *pxCreatedTask = (TaskHandle_t)pxNewTCB; }
    return pdPASS;
}

static void prvInitialiseTaskLists(void)
{
    UBaseType_t ux;
    for (ux = 0U; ux < (UBaseType_t)configMAX_PRIORITIES; ux++)
    {
        vListInitialise(&(xReadyTasksLists[ux]));
    }
    vListInitialise(&xDelayedTaskList);
    vListInitialise(&xSuspendedTaskList);
}

void vTaskStartScheduler(void)
{
    TaskHandle_t xIdle;
    (void)xTaskCreate(prvIdleTask, "IDLE", (uint32_t)configMINIMAL_STACK_SIZE,
                      NULL, 0U, &xIdle);
    xSchedulerRunning = pdTRUE;
    xTickCount = 0U;
#if (FLINT_SCHED_TRACE == 1)
    uart_printf("[sched] first task = '%s' (pxCurrentTCB=%p), %u tasks ready\n",
                (pxCurrentTCB != NULL) ? pxCurrentTCB->pcTaskName : "<null>",
                (void *)pxCurrentTCB, (unsigned int)uxCurrentNumberOfTasks);
    uart_printf("[sched] uxTopReadyPriority=%u\n", (unsigned int)uxTopReadyPriority);
#endif
    vPortSetupTimerInterrupt();
    (void)xPortStartScheduler();   /* starts first task; does not return */
}

void vTaskSwitchContext(void)
{
    UBaseType_t uxPriority = uxTopReadyPriority;
    while (listLIST_IS_EMPTY(&(xReadyTasksLists[uxPriority])) != pdFALSE)
    {
        if (uxPriority == 0U) { break; }
        uxPriority--;
    }
    uxTopReadyPriority = uxPriority;
    {
        void *pxOwner = NULL;
        listGET_OWNER_OF_NEXT_ENTRY(pxOwner, &(xReadyTasksLists[uxPriority]));
        pxCurrentTCB = (TCB_t *)pxOwner;
    }
#if (FLINT_SCHED_TRACE == 1)
    {
        /* Small cap: the switch runs every tick, and its UART print is slower
         * than the tick, so tracing many would starve the tasks. A handful
         * proves round-robin selection; then it goes quiet. */
        static UBaseType_t uxSwTrace = 0U;
        if (uxSwTrace < 6U)
        {
            uxSwTrace++;
            uart_printf("[switch #%u] prio=%u -> '%s' (ready len=%u)\n",
                        (unsigned int)uxSwTrace, (unsigned int)uxPriority,
                        (pxCurrentTCB != NULL) ? pxCurrentTCB->pcTaskName : "<null>",
                        (unsigned int)listCURRENT_LIST_LENGTH(&(xReadyTasksLists[uxPriority])));
        }
    }
#endif
}

/* Block the current task on an event list (used by queues/semaphores). */
void vTaskPlaceOnEventList(List_t *pxEventList, TickType_t xTicksToWait)
{
    TCB_t *pxTCB = (TCB_t *)pxCurrentTCB;

    /* Remove from the ready list. */
    (void)uxListRemove(&(pxTCB->xStateListItem));

    /* Ordered insert into the event list (by priority). */
    vListInsert(pxEventList, &(pxTCB->xEventListItem));

    /* Track timeout: delayed list if finite, suspended list if indefinite. */
    if (xTicksToWait == portMAX_DELAY)
    {
        vListInsertEnd(&xSuspendedTaskList, &(pxTCB->xStateListItem));
    }
    else
    {
        listSET_LIST_ITEM_VALUE(&(pxTCB->xStateListItem), xTickCount + xTicksToWait);
        vListInsert(&xDelayedTaskList, &(pxTCB->xStateListItem));
    }
}

/* Wake the highest-priority task waiting on an event list. Returns pdTRUE if
   the woken task has higher priority than the current one (caller should yield). */
BaseType_t xTaskRemoveFromEventList(List_t *pxEventList)
{
    TCB_t     *pxTCB;
    BaseType_t xShouldYield = pdFALSE;

    if (listLIST_IS_EMPTY(pxEventList) != pdFALSE)
    {
        return pdFALSE;
    }

    pxTCB = (TCB_t *)listGET_LIST_ITEM_OWNER(listGET_HEAD_ENTRY(pxEventList));
    (void)uxListRemove(&(pxTCB->xEventListItem));     /* off the event list   */
    (void)uxListRemove(&(pxTCB->xStateListItem));     /* off delayed/suspended */
    prvAddTaskToReadyList(pxTCB);                     /* make it runnable      */

    if (pxTCB->uxPriority > ((TCB_t *)pxCurrentTCB)->uxPriority)
    {
        xShouldYield = pdTRUE;
    }
    return xShouldYield;
}

void xTaskIncrementTick(void)
{
    xTickCount++;

    while (listLIST_IS_EMPTY(&xDelayedTaskList) == pdFALSE)
    {
        ListItem_t *pxItem   = listGET_HEAD_ENTRY(&xDelayedTaskList);
        TickType_t  xItemVal = listGET_LIST_ITEM_VALUE(pxItem);
        TCB_t      *pxTCB;

        if (xItemVal > xTickCount) { break; }

        pxTCB = (TCB_t *)listGET_LIST_ITEM_OWNER(pxItem);
        (void)uxListRemove(&(pxTCB->xStateListItem));
        /* If it was also waiting on an event, remove it from that list. */
        if (listGET_LIST_ITEM_CONTAINER(&(pxTCB->xEventListItem)) != NULL)
        {
            (void)uxListRemove(&(pxTCB->xEventListItem));
        }
        prvAddTaskToReadyList(pxTCB);
#if (FLINT_SCHED_TRACE == 1)
        {
            static UBaseType_t uxWakeTrace = 0U;
            if (uxWakeTrace < FLINT_TRACE_MAX)
            {
                uxWakeTrace++;
                uart_printf("[wake #%u] '%s' ready at tick=%u\n",
                            (unsigned int)uxWakeTrace, pxTCB->pcTaskName,
                            (unsigned int)xTickCount);
            }
        }
#endif
    }
}

void vTaskDelay(TickType_t xTicksToDelay)
{
    if (xTicksToDelay > 0U)
    {
        TCB_t *pxTCB = (TCB_t *)pxCurrentTCB;
        taskENTER_CRITICAL();
        {
            (void)uxListRemove(&(pxTCB->xStateListItem));
            listSET_LIST_ITEM_VALUE(&(pxTCB->xStateListItem), xTickCount + xTicksToDelay);
            vListInsert(&xDelayedTaskList, &(pxTCB->xStateListItem));
        }
        taskEXIT_CRITICAL();
#if (FLINT_SCHED_TRACE == 1)
        {
            static UBaseType_t uxDelayTrace = 0U;
            if (uxDelayTrace < FLINT_TRACE_MAX)
            {
                uxDelayTrace++;
                uart_printf("[delay #%u] '%s' sleeps %u ticks (wake@%u, now=%u); yielding\n",
                            (unsigned int)uxDelayTrace, pxTCB->pcTaskName,
                            (unsigned int)xTicksToDelay,
                            (unsigned int)(xTickCount + xTicksToDelay),
                            (unsigned int)xTickCount);
            }
        }
#endif
        taskYIELD();
    }
}

UBaseType_t uxTaskGetNumberOfTasks(void)     { return uxCurrentNumberOfTasks; }
TickType_t  xTaskGetTickCount(void)          { return xTickCount; }
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return (TaskHandle_t)pxCurrentTCB; }

static void prvIdleTask(void *pvParameters)
{
    (void)pvParameters;
    for (;;) { portWAIT_FOR_INTERRUPT(); }
}

void vPortTaskExit(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;) { portWAIT_FOR_INTERRUPT(); }
}
