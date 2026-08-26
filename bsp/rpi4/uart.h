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

#endif /* FLINT_BSP_UART_H */
