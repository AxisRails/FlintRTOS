/*
 * FlintRTOS - Raspberry Pi 4 C entry (called from start.S at EL1).
 * Performs early board init, then hands off to the application main().
 */
#include "uart.h"

#include <stdint.h>

extern int main(void);

/* Read the current exception level (CurrentEL[3:2]) for the boot log. */
static uint64_t current_el(void)
{
    uint64_t v;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(v));
    return (v >> 2) & 0x3U;
}

void flint_boot(void)
{
    /* Raw dual-UART output BEFORE any UART re-init, so this appears even if
     * uart_init() would mis-handle the port. If you can read this line, C code
     * is running and the console works. */
    dbg_puts("\n[boot] flint_boot() entered (pre-uart_init)\n");
    dbg_puts("[boot]   CurrentEL=EL"); dbg_hex(current_el()); dbg_puts("\n");

    uart_init();

    /* From here uart_puts()/uart_printf() also drive both UARTs. */
    uart_puts("\n");
    uart_puts("========================================\n");
    uart_puts("  FlintRTOS  -  Raspberry Pi 4 (AArch64)\n");
    uart_puts("  lean . secure . safe\n");
    uart_puts("========================================\n");
    uart_puts("[boot] board init complete; calling main()\n");

    (void)main();

    uart_puts("[boot] main() returned; halting.\n");
    for (;;)
    {
        __asm__ volatile("wfe");
    }
}
