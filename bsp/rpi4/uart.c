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

/* --- Output serialisation --------------------------------------------------
 * The UART is shared by both tasks and the timer IRQ handler. Without a lock,
 * a tick IRQ (or a preempting task) interleaves its characters into the middle
 * of another message, producing garbled output. Masking IRQs for the duration
 * of one message makes each line atomic. Save/restore of DAIF nests correctly,
 * so it is safe to call from IRQ context (where I is already masked) too. */
static inline uint64_t uart_lock(void)
{
    uint64_t daif;
    __asm__ volatile("mrs %0, daif" : "=r"(daif));
    __asm__ volatile("msr daifset, #2" ::: "memory");  /* mask IRQ (I bit) */
    return daif;
}

static inline void uart_unlock(uint64_t daif)
{
    __asm__ volatile("msr daif, %0" :: "r"(daif) : "memory");
}

/* --- Bring-up debug: raw output to BOTH UARTs -----------------------------
 * Writes a byte to the PL011 *and* the mini-UART using whatever pin routing
 * and baud the GPU firmware already configured (uart_2ndstage=1). It does NOT
 * touch GPFSEL/clocks, so it works before uart_init() and regardless of which
 * UART the firmware left on GPIO14/15. Every spin is bounded so a dead/absent
 * UART can never hang the boot. Use to localise "no output at all" faults. */
#define DBG_SPIN_MAX   (0x40000U)

void dbg_putc(char c)
{
    uint32_t g;

    /* PL011: wait while TX FIFO full (FR bit5), bounded. */
    for (g = 0U; (g < DBG_SPIN_MAX) &&
                 ((mmio_read32(UART0_FR) & UART0_FR_TXFF) != 0U); g++)
    {
    }
    mmio_write32(UART0_DR, (uint32_t)(uint8_t)c);

    /* mini-UART: wait until TX ready (LSR bit5), bounded. Harmless if the
     * AUX block is disabled (LSR reads 0 -> we just fall through the spin). */
    for (g = 0U; (g < DBG_SPIN_MAX) &&
                 ((mmio_read32(AUX_MU_LSR) & AUX_MU_LSR_TXRDY) == 0U); g++)
    {
    }
    mmio_write32(AUX_MU_IO, (uint32_t)(uint8_t)c);
}

void dbg_puts(const char *s)
{
    uint64_t daif = uart_lock();
    while (*s != '\0')
    {
        if (*s == '\n')
        {
            dbg_putc('\r');
        }
        dbg_putc(*s);
        s++;
    }
    uart_unlock(daif);
}

/* Print a value as 0x-prefixed hex on both UARTs (bring-up diagnostics). */
void dbg_hex(uint64_t v)
{
    static const char digits[] = "0123456789abcdef";
    int i;

    dbg_putc('0');
    dbg_putc('x');
    for (i = 60; i >= 0; i -= 4)
    {
        dbg_putc(digits[(v >> (unsigned int)i) & 0xFU]);
    }
}

void uart_init(void)
{
    /* BRING-UP NOTE: we deliberately do NOT re-mux GPIO14/15 here. The GPU
     * firmware (uart_2ndstage=1) has already routed and clocked whichever UART
     * is on the header pins; re-muxing to PL011 ALT0 would steal the pins from
     * a firmware-configured mini-UART (the default when no device tree applies
     * disable-bt) and produce total silence. So we leave the firmware routing
     * intact and simply (re)assert PL011's 115200 8N1 config in case PL011 is
     * the active UART. All console output goes through dbg_putc(), which drives
     * BOTH UARTs, so the console works whichever one owns the pins. */
    dbg_puts("[uart] init: leaving firmware GPIO routing intact\n");
    dbg_puts("[uart]   GPFSEL1="); dbg_hex(mmio_read32(GPFSEL1));
    dbg_puts(" AUX_ENABLES=");     dbg_hex(mmio_read32(AUX_ENABLES));
    dbg_puts(" UART0_CR=");        dbg_hex(mmio_read32(UART0_CR));
    dbg_puts("\n");

    /* Re-assert PL011 115200 8N1 without touching GPIO. Harmless if PL011 is
     * not the pin-routed UART (writes land on an off-header PL011). */
    mmio_write32(UART0_CR, 0U);
    mmio_write32(UART0_ICR, 0x7FFU);
    mmio_write32(UART0_IBRD, 26U);              /* 48 MHz / (16*115200)      */
    mmio_write32(UART0_FBRD, 3U);
    mmio_write32(UART0_LCRH, (3U << 5) | (1U << 4));
    mmio_write32(UART0_CR, (1U << 0) | (1U << 8) | (1U << 9));
    delay_cycles(150U);

    dbg_puts("[uart] init done\n");
}

void uart_putc(char c)
{
    /* During bring-up, drive BOTH UARTs so output appears whichever one the
     * firmware left on the header pins. */
    dbg_putc(c);
}

void uart_puts(const char *s)
{
    uint64_t daif = uart_lock();
    while (*s != '\0')
    {
        if (*s == '\n')
        {
            uart_putc('\r');
        }
        uart_putc(*s);
        s++;
    }
    uart_unlock(daif);
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
    uint64_t daif = uart_lock();
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
            case '\0': va_end(ap); uart_unlock(daif); return;
            default:  uart_putc('%'); uart_putc(*p); break;
        }
    }
    va_end(ap);
    uart_unlock(daif);
}
