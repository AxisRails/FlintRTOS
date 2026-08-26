/*
 * FlintRTOS - Raspberry Pi 4 C entry (called from start.S at EL1).
 * Performs early board init, then hands off to the application main().
 */
#include "uart.h"

extern int main(void);

void flint_boot(void)
{
    uart_init();
    uart_puts("\n");
    uart_puts("========================================\n");
    uart_puts("  FlintRTOS  -  Raspberry Pi 4 (AArch64)\n");
    uart_puts("  lean . secure . safe\n");
    uart_puts("========================================\n");

    (void)main();

    uart_puts("[boot] main() returned; halting.\n");
    for (;;)
    {
        __asm__ volatile("wfe");
    }
}
