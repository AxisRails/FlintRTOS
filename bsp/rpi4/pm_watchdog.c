/*
 * FlintRTOS - BCM2711 power-management watchdog. See pm_watchdog.h.
 */
#include "pm_watchdog.h"
#include "rpi4.h"

#define PM_BASE                 (BCM2711_PERIPH_BASE + 0x100000UL)
#define PM_RSTC                 (PM_BASE + 0x1CUL)
#define PM_RSTS                 (PM_BASE + 0x20UL)
#define PM_WDOG                 (PM_BASE + 0x24UL)
#define PM_PASSWORD             (0x5A000000U)
#define PM_WDOG_TIME_SET        (0x000FFFFFU)
#define PM_RSTC_WRCFG_CLR       (0xFFFFFFCFU)
#define PM_RSTC_WRCFG_FULL_RESET (0x00000020U)
#define PM_RSTC_RESET           (0x00000102U)

static uint32_t s_ticks = 0U;

static uint32_t ms_to_ticks(uint32_t ms)
{
    if (ms > PM_WDOG_MAX_MS) { ms = PM_WDOG_MAX_MS; }
    if (ms == 0U)            { ms = 1U; }
    return ((ms * 65536U) / 1000U) & PM_WDOG_TIME_SET;
}

static void arm(uint32_t ticks)
{
    uint32_t rstc;
    mmio_write32(PM_WDOG, PM_PASSWORD | (ticks & PM_WDOG_TIME_SET));
    rstc = mmio_read32(PM_RSTC) & PM_RSTC_WRCFG_CLR;
    mmio_write32(PM_RSTC, PM_PASSWORD | rstc | PM_RSTC_WRCFG_FULL_RESET);
}

void pm_wdog_start(uint32_t timeout_ms)
{
    s_ticks = ms_to_ticks(timeout_ms);
    arm(s_ticks);
}

void pm_wdog_feed(void)
{
    if (s_ticks != 0U)
    {
        mmio_write32(PM_WDOG, PM_PASSWORD | s_ticks);
    }
}

void pm_wdog_stop(void)
{
    mmio_write32(PM_RSTC, PM_PASSWORD | PM_RSTC_RESET);
    s_ticks = 0U;
}

void pm_wdog_reset_now(void)
{
    arm(10U);                                   /* ~150 us */
    for (;;) { __asm__ volatile("wfe"); }
}

uint32_t pm_wdog_reset_status(void)
{
    return mmio_read32(PM_RSTS);
}
