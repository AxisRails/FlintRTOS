/*
 * FlintRTOS - RPi4 demo: two preemptive tasks over the AArch64 port.
 * Task A and Task B print at different rates, driven by the 1 ms generic-timer
 * tick and vTaskDelay(). Demonstrates creation, scheduling, and context switch.
 */
#include "FlintRTOS.h"
#include "task.h"
#include "uart.h"

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
    uart_printf("[main] creating tasks...\n");
    (void)xTaskCreate(vTaskA, "A", (uint32_t)configMINIMAL_STACK_SIZE, NULL, 2U, NULL);
    (void)xTaskCreate(vTaskB, "B", (uint32_t)configMINIMAL_STACK_SIZE, NULL, 2U, NULL);

    uart_printf("[main] starting scheduler with %u task(s)...\n",
                (unsigned int)uxTaskGetNumberOfTasks());

    vTaskStartScheduler();   /* does not return */

    for (;;)
    {
        __asm__ volatile("wfe");
    }
}
