/*
 * FlintRTOS - BCM2711 power-management watchdog (Raspberry Pi 4).
 *
 * The PM block contains a down-counter in 1/65536 s units (max 0xFFFFF,
 * ~16 s). Once armed, it resets the whole SoC when it reaches zero unless it
 * is reloaded. Register protocol and constants as used by the Linux
 * bcm2835_wdt driver (password-protected writes).
 */
#ifndef FLINT_BSP_PM_WATCHDOG_H
#define FLINT_BSP_PM_WATCHDOG_H

#include <stdint.h>

#define PM_WDOG_MAX_MS   (15000U)

/* Arm (or re-arm) with a timeout in milliseconds (clamped to PM_WDOG_MAX_MS). */
void     pm_wdog_start(uint32_t timeout_ms);

/* Reload the counter with the last armed timeout. */
void     pm_wdog_feed(void);

/* Disarm. */
void     pm_wdog_stop(void);

/* Reset the board now (~150 us). Does not return. */
void     pm_wdog_reset_now(void) __attribute__((noreturn));

/* Raw PM_RSTS value latched at boot (reset-status bits), for diagnostics. */
uint32_t pm_wdog_reset_status(void);

#endif /* FLINT_BSP_PM_WATCHDOG_H */
