/*
 * FlintRTOS - RPi4 demo: preemptive multitasking + mutex + stack-overflow guard.
 *
 * Tasks A and B (equal priority 2) increment a shared counter under a mutex and
 * print at different rates. Task C (priority 3, higher) wakes periodically and
 * preempts A/B to read the counter - demonstrating priority preemption and the
 * queue-backed mutex on real hardware. A stack-overflow hook reports and halts.
 */
#include "FlintRTOS.h"
#include "task.h"
#include "semphr.h"
#include "uart.h"

#if defined(FLINT_LWIP_OS)
extern void vNetworkTaskOS(void *pvParameters);
#elif (configUSE_LWIP == 1)
extern void vNetworkTask(void *pvParameters);
#endif

/* Shared state guarded by a mutex (exercises the blocking primitives on HW). */
static SemaphoreHandle_t  xSharedMutex = NULL;
static volatile uint32_t  ulSharedCounter = 0U;

static void prvBumpShared(uint32_t ulAmount, const char *pcWho)
{
    if (xSemaphoreTake(xSharedMutex, 100U) == pdTRUE)
    {
        ulSharedCounter += ulAmount;
        (void)xSemaphoreGive(xSharedMutex);
    }
    else
    {
        uart_printf("[%s] MUTEX TIMEOUT - blocking primitive stuck!\n", pcWho);
    }
}

static void vTaskA(void *pvParameters)
{
    (void)pvParameters;
    uint32_t n = 0U;
    for (;;)
    {
        prvBumpShared(1U, "A");
        uart_printf("[A] tick %u  (t=%u, shared=%u)\n",
                    n, (unsigned int)xTaskGetTickCount(), (unsigned int)ulSharedCounter);
        n++;
        vTaskDelay(500U);
    }
}

static void vTaskB(void *pvParameters)
{
    (void)pvParameters;
    uint32_t n = 0U;
    for (;;)
    {
        prvBumpShared(100U, "B");
        uart_printf("    [B] tock %u\n", n);
        n++;
        vTaskDelay(1000U);
    }
}

/* Higher priority than A/B: when this wakes it must preempt them immediately. */
static void vTaskC(void *pvParameters)
{
    (void)pvParameters;
    uint32_t n = 0U;
    for (;;)
    {
        vTaskDelay(2000U);
        if (xSemaphoreTake(xSharedMutex, 100U) == pdTRUE)
        {
            uart_printf(">>> [C] preempt @t=%u: shared counter = %u (cycle %u)\n",
                        (unsigned int)xTaskGetTickCount(),
                        (unsigned int)ulSharedCounter, n);
            (void)xSemaphoreGive(xSharedMutex);
        }
        n++;
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    uart_printf("\n*** STACK OVERFLOW in task '%s' - halting ***\n",
                (pcTaskName != NULL) ? pcTaskName : "?");
    taskDISABLE_INTERRUPTS();
    for (;;)
    {
        __asm__ volatile("wfe");
    }
}

int main(void)
{
    BaseType_t rc;

    xSharedMutex = xSemaphoreCreateMutex();
    if (xSharedMutex == NULL)
    {
        uart_printf("[main] FATAL: could not create mutex\n");
        for (;;) { __asm__ volatile("wfe"); }
    }
    uart_printf("[main] mutex created; creating tasks...\n");

    rc = xTaskCreate(vTaskA, "A", (uint32_t)configMINIMAL_STACK_SIZE, NULL, 2U, NULL);
    uart_printf("[main]   xTaskCreate(A) -> %d\n", (int)rc);
    rc = xTaskCreate(vTaskB, "B", (uint32_t)configMINIMAL_STACK_SIZE, NULL, 2U, NULL);
    uart_printf("[main]   xTaskCreate(B) -> %d\n", (int)rc);
    rc = xTaskCreate(vTaskC, "C", (uint32_t)configMINIMAL_STACK_SIZE, NULL, 3U, NULL);
    uart_printf("[main]   xTaskCreate(C, prio 3) -> %d\n", (int)rc);

#if defined(FLINT_LWIP_OS)
    (void)xTaskCreate(vNetworkTaskOS, "net", (uint32_t)(configMINIMAL_STACK_SIZE * 8U), NULL, 4U, NULL);
#elif (configUSE_LWIP == 1)
    (void)xTaskCreate(vNetworkTask, "net", (uint32_t)(configMINIMAL_STACK_SIZE * 4U), NULL, 4U, NULL);
#endif

    uart_printf("[main] starting scheduler with %u task(s)...\n",
                (unsigned int)uxTaskGetNumberOfTasks());

    vTaskStartScheduler();   /* does not return */

    uart_printf("[main] ERROR: vTaskStartScheduler() returned!\n");
    for (;;)
    {
        __asm__ volatile("wfe");
    }
}
