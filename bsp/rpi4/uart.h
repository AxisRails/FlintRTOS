/*
 * FlintRTOS - PL011 UART0 console for Raspberry Pi 4.
 */
#ifndef FLINT_BSP_UART_H
#define FLINT_BSP_UART_H

#include <stdint.h>

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);

/* Minimal printf: supports %c %s %d %u %x %p %% and width-less formatting. */
void uart_printf(const char *fmt, ...);

/* Bring-up diagnostics: write to BOTH the PL011 and the mini-UART using the
 * firmware's existing pin routing (no GPIO/clock changes, bounded spins).
 * Safe to call before uart_init() and from any exception level. */
void dbg_putc(char c);
void dbg_puts(const char *s);
void dbg_hex(uint64_t v);

#endif /* FLINT_BSP_UART_H */
