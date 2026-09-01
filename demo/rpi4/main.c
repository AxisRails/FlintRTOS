/*
 * FlintRTOS - RPi4 demo entry.
 *
 * Two demos, selected at build time:
 *   - Networking build (make / make LWIP_OS=1): a heartbeat task plus the lwIP
 *     network task (net_demo.c / net_demo_os.c), which brings up the GENET MAC.
 *   - Minimal build (make LWIP=0): the deterministic priority-inversion /
 *     inheritance demo (L/M/H + mutex).
 */
#include "FlintRTOS.h"
#include "task.h"
#include "semphr.h"
#include "uart.h"

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    uart_printf("\n*** STACK OVERFLOW in task '%s' - halting ***\n",
                (pcTaskName != NULL) ? pcTaskName : "?");
    taskDISABLE_INTERRUPTS();
    for (;;) { __asm__ volatile("wfe"); }
}

#if defined(FLINT_LWIP_OS) || (configUSE_LWIP == 1)
/* ================= Networking build ===================================== */
#if defined(FLINT_LWIP_OS)
extern void vNetworkTaskOS(void *pvParameters);
#else
extern void vNetworkTask(void *pvParameters);
#endif

/* Heartbeat: proves the scheduler keeps running independently of networking. */
static void vHeartbeat(void *pvParameters)
{
    (void)pvParameters;
    uint32_t n = 0U;
    for (;;)
    {
        uart_printf("[hb] alive (t=%u, beat %u)\n",
                    (unsigned int)xTaskGetTickCount(), n);
        n++;
        vTaskDelay(3000U);
    }
}

int main(void)
{
    uart_printf("[main] FlintRTOS networking demo (lwIP)\n");

    (void)xTaskCreate(vHeartbeat, "hb", (uint32_t)configMINIMAL_STACK_SIZE, NULL, 1U, NULL);
#if defined(FLINT_LWIP_OS)
    (void)xTaskCreate(vNetworkTaskOS, "net", (uint32_t)(configMINIMAL_STACK_SIZE * 8U), NULL, 3U, NULL);
#else
    (void)xTaskCreate(vNetworkTask, "net", (uint32_t)(configMINIMAL_STACK_SIZE * 4U), NULL, 3U, NULL);
#endif

    uart_printf("[main] starting scheduler with %u task(s)...\n",
                (unsigned int)uxTaskGetNumberOfTasks());
    vTaskStartScheduler();
    for (;;) { __asm__ volatile("wfe"); }
}

#else
/* ================= Minimal build: priority-inversion demo =============== */

static SemaphoreHandle_t xMutex     = NULL;
static SemaphoreHandle_t xMutexHeld = NULL;

#define HOLD_MS   60U
#define LOW_GAP   120U
#define SPIN_MS   150U
#define MED_GAP   50U
#define PHASE1_CYCLES 5U

static void busy_wait_ms(uint32_t ms)
{
    uint64_t f, start, now;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(f));
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(start));
    const uint64_t target = start + ((f / 1000U) * (uint64_t)ms);
    do {
        __asm__ volatile("mrs %0, cntpct_el0" : "=r"(now));
    } while (now < target);
}

static void vTaskLow(void *pvParameters)
{
    (void)pvParameters;
    for (;;)
    {
        (void)xSemaphoreTake(xMutex, portMAX_DELAY);
        (void)xSemaphoreGive(xMutexHeld);
        busy_wait_ms(HOLD_MS);
        (void)xSemaphoreGive(xMutex);
        vTaskDelay(LOW_GAP);
    }
}

static void vTaskMed(void *pvParameters)
{
    (void)pvParameters;
    for (;;)
    {
        busy_wait_ms(SPIN_MS);
        vTaskDelay(MED_GAP);
    }
}

static void vTaskHigh(void *pvParameters)
{
    (void)pvParameters;
    uint32_t cycle = 0U;
    for (;;)
    {
        (void)xSemaphoreTake(xMutexHeld, portMAX_DELAY);
        TickType_t t0 = xTaskGetTickCount();
        (void)xSemaphoreTake(xMutex, portMAX_DELAY);
        TickType_t wait = xTaskGetTickCount() - t0;
        (void)xSemaphoreGive(xMutex);

        uart_printf(">>> [H] cycle %u: blocked %u ms on mutex held by low-prio L  [inheritance %s]\n",
                    cycle, (unsigned int)wait,
                    (xTaskGetMutexInheritance() != pdFALSE) ? "ON" : "OFF");
        cycle++;
        if (cycle == PHASE1_CYCLES)
        {
            uart_printf("\n===== enabling PRIORITY INHERITANCE - watch H's wait collapse =====\n\n");
            vTaskSetMutexInheritance(pdTRUE);
        }
    }
}

int main(void)
{
    xMutex     = xSemaphoreCreateMutex();
    xMutexHeld = xSemaphoreCreateBinary();
    if ((xMutex == NULL) || (xMutexHeld == NULL))
    {
        uart_printf("[main] FATAL: could not create semaphores\n");
        for (;;) { __asm__ volatile("wfe"); }
    }

    vTaskSetMutexInheritance(pdFALSE);
    uart_printf("[main] deterministic priority-inversion demo: L(1) M(2) H(3)\n");
    uart_printf("[main] PHASE 1: inheritance OFF - H must wait behind M (inversion)\n");

    (void)xTaskCreate(vTaskLow,  "L", (uint32_t)configMINIMAL_STACK_SIZE, NULL, 1U, NULL);
    (void)xTaskCreate(vTaskMed,  "M", (uint32_t)configMINIMAL_STACK_SIZE, NULL, 2U, NULL);
    (void)xTaskCreate(vTaskHigh, "H", (uint32_t)configMINIMAL_STACK_SIZE, NULL, 3U, NULL);

    uart_printf("[main] starting scheduler with %u task(s)...\n",
                (unsigned int)uxTaskGetNumberOfTasks());
    vTaskStartScheduler();
    for (;;) { __asm__ volatile("wfe"); }
}

#endif /* networking vs minimal */
