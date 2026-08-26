/*
 * FlintRTOS - PL011 UART0 console for Raspberry Pi 4 (BCM2711).
 * Bare-metal, freestanding. Build-verified; on-target runtime pending.
 */
#include "uart.h"
#include "rpi4.h"

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>

static void delay_cycles(uint32_t n)
{
    while (n-- > 0U)
    {
        __asm__ volatile("nop");
    }
}

void uart_init(void)
{
    /* Disable UART while configuring. */
    mmio_write32(UART0_CR, 0U);

    /* Select ALT0 (PL011 TXD0/RXD0) on GPIO14/15. GPFSEL1 fields 12-14 / 15-17. */
    uint32_t sel = mmio_read32(GPFSEL1);
    sel &= ~((7U << 12) | (7U << 15));   /* clear FSEL14, FSEL15            */
    sel |=  ((4U << 12) | (4U << 15));   /* ALT0 == 0b100                   */
    mmio_write32(GPFSEL1, sel);

    /* Disable pull-up/down on GPIO14/15 (BCM2711 control register). */
    uint32_t pud = mmio_read32(GPIO_PUP_PDN_CNTRL_0);
    pud &= ~((3U << 28) | (3U << 30));   /* 00 == no pull for pins 14,15    */
    mmio_write32(GPIO_PUP_PDN_CNTRL_0, pud);

    /* Clear pending interrupts. */
    mmio_write32(UART0_ICR, 0x7FFU);

    /* Baud 115200 @ 48 MHz reference: divisor = 48e6 / (16*115200) = 26.0417.
       IBRD = 26, FBRD = round(0.0417 * 64) = 3. (QEMU ignores baud.) */
    mmio_write32(UART0_IBRD, 26U);
    mmio_write32(UART0_FBRD, 3U);

    /* 8 bits, FIFO enabled (WLEN=0b11 -> bits 5-6, FEN bit 4). */
    mmio_write32(UART0_LCRH, (3U << 5) | (1U << 4));

    /* Enable UART, TX, RX. */
    mmio_write32(UART0_CR, (1U << 0) | (1U << 8) | (1U << 9));

    delay_cycles(150U);
}

void uart_putc(char c)
{
    while ((mmio_read32(UART0_FR) & UART0_FR_TXFF) != 0U)
    {
        /* wait until TX FIFO has space */
    }
    mmio_write32(UART0_DR, (uint32_t)(uint8_t)c);
}

void uart_puts(const char *s)
{
    while (*s != '\0')
    {
        if (*s == '\n')
        {
            uart_putc('\r');
        }
        uart_putc(*s);
        s++;
    }
}

static void uart_put_uint(uint64_t v, uint32_t base, bool upper)
{
    char buf[24];
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    size_t i = 0U;

    if (v == 0U)
    {
        uart_putc('0');
        return;
    }
    while ((v != 0U) && (i < sizeof(buf)))
    {
        buf[i] = digits[v % base];
        v /= base;
        i++;
    }
    while (i > 0U)
    {
        i--;
        uart_putc(buf[i]);
    }
}

static void uart_put_int(int64_t v)
{
    if (v < 0)
    {
        uart_putc('-');
        uart_put_uint((uint64_t)(-(v + 1)) + 1U, 10U, false);
    }
    else
    {
        uart_put_uint((uint64_t)v, 10U, false);
    }
}

void uart_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    for (const char *p = fmt; *p != '\0'; p++)
    {
        if (*p != '%')
        {
            if (*p == '\n')
            {
                uart_putc('\r');
            }
            uart_putc(*p);
            continue;
        }
        p++;
        switch (*p)
        {
            case 'c': uart_putc((char)va_arg(ap, int)); break;
            case 's': uart_puts(va_arg(ap, const char *)); break;
            case 'd': uart_put_int((int64_t)va_arg(ap, int)); break;
            case 'u': uart_put_uint((uint64_t)va_arg(ap, unsigned int), 10U, false); break;
            case 'x': uart_put_uint((uint64_t)va_arg(ap, unsigned int), 16U, false); break;
            case 'p':
                uart_puts("0x");
                uart_put_uint((uint64_t)(uintptr_t)va_arg(ap, void *), 16U, false);
                break;
            case '%': uart_putc('%'); break;
            case '\0': va_end(ap); return;
            default:  uart_putc('%'); uart_putc(*p); break;
        }
    }
    va_end(ap);
}
