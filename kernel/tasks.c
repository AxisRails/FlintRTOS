/*
 * FlintRTOS - Task Management (manifest 1). TCB code (MISRA C:2023).
 *
 * Fixed-priority preemptive scheduler with per-priority ready lists and a
 * single delayed list for vTaskDelay. Context switching is delegated to the
 * architecture port (pxPortInitialiseStack / xPortStartScheduler / vPortYield).
 *
 * Build-verified for AArch64; on-target runtime bring-up pending (verify the
 * port context switch on hardware/QEMU first).
 */
#include "task.h"
#include "list.h"

#include <stddef.h>
#include <stdint.h>

/* pxTopOfStack MUST be the first member: the port's asm saves/restores SP
   through *pxCurrentTCB (see portASM.S). */
struct tskTaskControlBlock
{
    volatile StackType_t *pxTopOfStack;
    ListItem_t            xStateListItem;   /* links into a ready/delayed list */
    UBaseType_t           uxPriority;
    StackType_t          *pxStack;          /* base of the allocated stack     */
    char                  pcTaskName[configMAX_TASK_NAME_LEN];
};

/* Exposed to the port asm. */
volatile TCB_t *volatile pxCurrentTCB = NULL;

/* Scheduler state. */
static List_t     xReadyTasksLists[configMAX_PRIORITIES];
static List_t     xDelayedTaskList;
static volatile UBaseType_t uxTopReadyPriority = 0U;
static volatile TickType_t  xTickCount        = 0U;
static volatile UBaseType_t uxCurrentNumberOfTasks = 0U;
static volatile BaseType_t  xSchedulerRunning = pdFALSE;

extern void vPortStartFirstTask(void); /* portASM.S */

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

    /* Initialise the scheduler lists on first use. */
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

    pxNewTCB->pxStack     = pxStack;
    pxNewTCB->uxPriority  = uxPriority;
    for (ux = 0U; ux < (UBaseType_t)configMAX_TASK_NAME_LEN; ux++)
    {
        char c = (pcName != NULL) ? pcName[ux] : '\0';
        pxNewTCB->pcTaskName[ux] = c;
        if (c == '\0')
        {
            break;
        }
    }
    pxNewTCB->pcTaskName[configMAX_TASK_NAME_LEN - 1] = '\0';

    vListInitialiseItem(&(pxNewTCB->xStateListItem));

    /* Top of a full-descending stack, 16-byte aligned. */
    {
        StackType_t *pxTopOfStack =
            &(pxStack[usStackDepth - 1U]);
        pxTopOfStack = (StackType_t *)(((uintptr_t)pxTopOfStack) &
                                       ~((uintptr_t)portBYTE_ALIGNMENT_MASK));
        pxNewTCB->pxTopOfStack =
            pxPortInitialiseStack(pxTopOfStack, (void (*)(void *))pxTaskCode, pvParameters);
    }

    taskENTER_CRITICAL();
    {
        uxCurrentNumberOfTasks++;
        if (pxCurrentTCB == NULL)
        {
            pxCurrentTCB = pxNewTCB;   /* first task becomes current */
        }
        prvAddTaskToReadyList(pxNewTCB);
    }
    taskEXIT_CRITICAL();

    if (pxCreatedTask != NULL)
    {
        *pxCreatedTask = (TaskHandle_t)pxNewTCB;
    }
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
}

void vTaskStartScheduler(void)
{
    TaskHandle_t xIdle;
    (void)xTaskCreate(prvIdleTask, "IDLE", (uint32_t)configMINIMAL_STACK_SIZE,
                      NULL, 0U, &xIdle);

    xSchedulerRunning = pdTRUE;
    xTickCount = 0U;

    vPortSetupTimerInterrupt();
    (void)xPortStartScheduler();   /* starts first task; does not return */
}

/* Select the highest-priority ready task as the current task. */
void vTaskSwitchContext(void)
{
    UBaseType_t uxPriority = uxTopReadyPriority;

    while (listLIST_IS_EMPTY(&(xReadyTasksLists[uxPriority])) != pdFALSE)
    {
        if (uxPriority == 0U)
        {
            break;   /* the idle task is always at priority 0 */
        }
        uxPriority--;
    }
    uxTopReadyPriority = uxPriority;

    {
        void *pxOwner = NULL;
        listGET_OWNER_OF_NEXT_ENTRY(pxOwner, &(xReadyTasksLists[uxPriority]));
        pxCurrentTCB = (TCB_t *)pxOwner;
    }
}

void xTaskIncrementTick(void)
{
    xTickCount++;

    /* Wake any delayed tasks whose deadline has arrived. */
    while (listLIST_IS_EMPTY(&xDelayedTaskList) == pdFALSE)
    {
        ListItem_t *pxItem   = listGET_HEAD_ENTRY(&xDelayedTaskList);
        TickType_t  xItemVal = listGET_LIST_ITEM_VALUE(pxItem);
        TCB_t      *pxTCB;

        if (xItemVal > xTickCount)
        {
            break;
        }
        pxTCB = (TCB_t *)listGET_LIST_ITEM_OWNER(pxItem);
        (void)uxListRemove(pxItem);
        prvAddTaskToReadyList(pxTCB);
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

        taskYIELD();
    }
}

UBaseType_t uxTaskGetNumberOfTasks(void) { return uxCurrentNumberOfTasks; }
TickType_t  xTaskGetTickCount(void)      { return xTickCount; }
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return (TaskHandle_t)pxCurrentTCB; }

/* Idle task: runs when nothing else is ready. */
static void prvIdleTask(void *pvParameters)
{
    (void)pvParameters;
    for (;;)
    {
        __asm__ volatile("wfi");   /* low-power wait for the next interrupt */
    }
}

/* Reached if a task function returns. Contain it rather than crash. */
void vPortTaskExit(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;)
    {
        __asm__ volatile("wfe");
    }
}

/* Called once before the scheduler starts (from vTaskStartScheduler path). */
void vTaskInitialise(void)
{
    prvInitialiseTaskLists();
}
