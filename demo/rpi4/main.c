/*
 * FlintRTOS - RPi4 demo: two preemptive tasks over the AArch64 port.
 * Task A and Task B print at different rates, driven by the 1 ms generic-timer
 * tick and vTaskDelay(). Demonstrates creation, scheduling, and context switch.
 */
#include "FlintRTOS.h"
#include "task.h"
#include "uart.h"

#if defined(FLINT_LWIP_OS)
extern void vNetworkTaskOS(void *pvParameters);
#elif (configUSE_LWIP == 1)
extern void vNetworkTask(void *pvParameters);
#endif

static void vTaskA(void *pvParameters)
{
    (void)pvParameters;
    uint32_t n = 0U;
    for (;;)
    {
        uart_printf("[A] tick %u  (t=%u)\n", n, (unsigned int)xTaskGetTickCount());
        n++;
        vTaskDelay(500U);   /* 500 ms */
    }
}

static void vTaskB(void *pvParameters)
{
    (void)pvParameters;
    uint32_t n = 0U;
    for (;;)
    {
        uart_printf("    [B] tock %u\n", n);
        n++;
        vTaskDelay(1000U);  /* 1 s */
    }
}

int main(void)
{
    BaseType_t rc;

    uart_printf("[main] creating task A...\n");
    rc = xTaskCreate(vTaskA, "A", (uint32_t)configMINIMAL_STACK_SIZE, NULL, 2U, NULL);
    uart_printf("[main]   xTaskCreate(A) -> %d\n", (int)rc);

    uart_printf("[main] creating task B...\n");
    rc = xTaskCreate(vTaskB, "B", (uint32_t)configMINIMAL_STACK_SIZE, NULL, 2U, NULL);
    uart_printf("[main]   xTaskCreate(B) -> %d\n", (int)rc);

#if defined(FLINT_LWIP_OS)
    (void)xTaskCreate(vNetworkTaskOS, "net", (uint32_t)(configMINIMAL_STACK_SIZE * 8U), NULL, 3U, NULL);
#elif (configUSE_LWIP == 1)
    (void)xTaskCreate(vNetworkTask, "net", (uint32_t)(configMINIMAL_STACK_SIZE * 4U), NULL, 3U, NULL);
#endif

    uart_printf("[main] starting scheduler with %u task(s)...\n",
                (unsigned int)uxTaskGetNumberOfTasks());
    uart_printf("[main] calling vTaskStartScheduler() (enables timer IRQ + first switch)\n");

    vTaskStartScheduler();   /* does not return */

    uart_printf("[main] ERROR: vTaskStartScheduler() returned!\n");

    for (;;)
    {
        __asm__ volatile("wfe");
    }
}
