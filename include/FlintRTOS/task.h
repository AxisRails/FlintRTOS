/*
 * FlintRTOS - Task Management (manifest 1): public API.
 * Task states, priorities, creation, delay, and the scheduler entry points.
 * TCB code (MISRA C:2023). FreeRTOS-style API for familiarity.
 */
#ifndef FLINT_TASK_H
#define FLINT_TASK_H

#include "FlintRTOS.h"
#include "list.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);

/* Create a dynamically-allocated task. Returns pdPASS / errCOULD_NOT_ALLOCATE... */
BaseType_t xTaskCreate(TaskFunction_t pxTaskCode,
                       const char *pcName,
                       uint32_t usStackDepth,      /* in StackType_t words */
                       void *pvParameters,
                       UBaseType_t uxPriority,
                       TaskHandle_t *pxCreatedTask);

void        vTaskStartScheduler(void);
void        vTaskDelay(TickType_t xTicksToDelay);
void        vTaskSwitchContext(void);          /* called from the port ISR */
void        xTaskIncrementTick(void);          /* called from the tick ISR */

/* Event-list blocking - used by queues/semaphores (queue.c). */
void        vTaskPlaceOnEventList(List_t *pxEventList, TickType_t xTicksToWait);
BaseType_t  xTaskRemoveFromEventList(List_t *pxEventList);
UBaseType_t uxTaskGetNumberOfTasks(void);
TickType_t  xTaskGetTickCount(void);
TaskHandle_t xTaskGetCurrentTaskHandle(void);

/* Application hook invoked when a task stack overflow is detected (enabled by
 * configCHECK_FOR_STACK_OVERFLOW). The application must provide this; it should
 * report and halt. */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName);

#define taskYIELD()                 portYIELD()
#define taskENTER_CRITICAL()        portENTER_CRITICAL()
#define taskEXIT_CRITICAL()         portEXIT_CRITICAL()
#define taskDISABLE_INTERRUPTS()    portDISABLE_INTERRUPTS()
#define taskENABLE_INTERRUPTS()     portENABLE_INTERRUPTS()

/* The scheduler's per-task control block (opaque to applications). */
struct tskTaskControlBlock;
typedef struct tskTaskControlBlock TCB_t;

#ifdef __cplusplus
}
#endif

#endif /* FLINT_TASK_H */
