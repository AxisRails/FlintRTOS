/*
 * FlintRTOS - safety services, Raspberry Pi 4 port: platform hooks for
 * kernel/flint_safety.c, the health-monitor task, and the hooks the kernel and
 * libraries call on fatal errors (stack overflow, out of memory).
 */
#include "FlintRTOS.h"
#include "task.h"
#include "uart.h"
#include "flint_safety.h"
#include "pm_watchdog.h"

#ifndef configSAFETY_WDOG_MS
#define configSAFETY_WDOG_MS        (4000U)
#endif
#ifndef configSAFETY_MONITOR_MS
#define configSAFETY_MONITOR_MS     (250U)
#endif

/* ---- platform hooks ------------------------------------------------------- */
uint32_t flint_safety_port_ms(void)
{
    return (uint32_t)xTaskGetTickCount();          /* 1 kHz tick */
}

const char *flint_safety_port_task_name(void)
{
    return pcTaskGetName(NULL);
}

void flint_safety_port_wdog_start(uint32_t ms) { pm_wdog_start(ms); }
void flint_safety_port_wdog_feed(void)         { pm_wdog_feed(); }
void flint_safety_port_reset(void)             { pm_wdog_reset_now(); }
void flint_safety_port_print(const char *s)    { uart_puts(s); }

/* ---- fatal-error hooks ---------------------------------------------------- */
void vApplicationMallocFailedHook(void);
void flint_assert_failed(const char *msg, const char *file, int line);

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    flint_panic_task(FLINT_RESET_STACK_OVERFLOW, pcTaskGetName(xTask),
                     (pcTaskName != NULL) ? "stack guard words overwritten" : "stack overflow",
                     NULL);
}

void vApplicationMallocFailedHook(void)
{
    flint_panic(FLINT_RESET_OUT_OF_MEMORY, "pvPortMalloc returned NULL", NULL);
}

/* lwIP LWIP_PLATFORM_ASSERT (port/lwip/include/arch/cc.h). */
void flint_assert_failed(const char *msg, const char *file, int line)
{
    char        d[FLINT_SAFETY_DETAIL_LEN];
    size_t      n    = 0U;
    const char *base = file;

    if (file != NULL)
    {
        for (size_t i = 0U; file[i] != '\0'; i++)
        {
            if ((file[i] == '/') || (file[i] == '\\')) { base = &file[i + 1U]; }
        }
    }
    n = flint_safety_append(d, sizeof(d), n, base);              /* "file:line msg" */
    n = flint_safety_append(d, sizeof(d), n, ":");
    n = flint_safety_append_u32(d, sizeof(d), n, (line > 0) ? (uint32_t)line : 0U);
    n = flint_safety_append(d, sizeof(d), n, " ");
    (void)flint_safety_append(d, sizeof(d), n, msg);
    flint_panic(FLINT_RESET_ASSERT, d, NULL);
}

/* ---- health monitor task -------------------------------------------------- */
static void prvHealthTask(void *pvParameters)
{
    (void)pvParameters;
    flint_safety_port_wdog_start(configSAFETY_WDOG_MS);
    uart_printf("[safety] health monitor running: HW watchdog %u ms, check every %u ms\n",
                (unsigned int)configSAFETY_WDOG_MS, (unsigned int)configSAFETY_MONITOR_MS);
    for (;;)
    {
        uint32_t now  = flint_safety_port_ms();
        int      late = flint_health_check(now);    /* feeds the watchdog if all OK */
        if (late >= 0)
        {
            char        d[FLINT_SAFETY_DETAIL_LEN];
            size_t      n    = 0U;
            const char *name = flint_health_name(late);
            n = flint_safety_append(d, sizeof(d), n, "silent ");             /* "silent 3204 ms > 3000 ms" */
            n = flint_safety_append_u32(d, sizeof(d), n, flint_health_silence_ms(late, now));
            n = flint_safety_append(d, sizeof(d), n, " ms > ");
            n = flint_safety_append_u32(d, sizeof(d), n, flint_health_deadline_ms(late));
            (void)flint_safety_append(d, sizeof(d), n, " ms");
            flint_panic_task(FLINT_RESET_TASK_HUNG, name, d, NULL);
        }
        vTaskDelay((TickType_t)configSAFETY_MONITOR_MS);
    }
}

void flint_health_start(void)
{
    (void)xTaskCreate(prvHealthTask, "health", (uint32_t)configMINIMAL_STACK_SIZE,
                      NULL, (UBaseType_t)(configMAX_PRIORITIES - 1U), NULL);
}
