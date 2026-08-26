/*
 * FlintRTOS - Raspberry Pi 4 (BCM2711) memory-mapped peripheral addresses.
 * Low-peripheral mode base = 0xFE000000 (matches QEMU raspi4b).
 */
#ifndef FLINT_BSP_RPI4_H
#define FLINT_BSP_RPI4_H

#include <stdint.h>

#define BCM2711_PERIPH_BASE   (0xFE000000UL)

/* GPIO */
#define GPIO_BASE             (BCM2711_PERIPH_BASE + 0x200000UL)
#define GPFSEL1               (GPIO_BASE + 0x04UL)
#define GPPUD                 (GPIO_BASE + 0x94UL)   /* legacy pull (BCM2835) */
#define GPPUDCLK0             (GPIO_BASE + 0x98UL)
#define GPIO_PUP_PDN_CNTRL_0  (GPIO_BASE + 0xE4UL)   /* BCM2711 pull control */

/* PL011 UART0 */
#define UART0_BASE            (BCM2711_PERIPH_BASE + 0x201000UL)
#define UART0_DR              (UART0_BASE + 0x00UL)
#define UART0_FR              (UART0_BASE + 0x18UL)
#define UART0_IBRD            (UART0_BASE + 0x24UL)
#define UART0_FBRD            (UART0_BASE + 0x28UL)
#define UART0_LCRH            (UART0_BASE + 0x2CUL)
#define UART0_CR              (UART0_BASE + 0x30UL)
#define UART0_IMSC            (UART0_BASE + 0x38UL)
#define UART0_ICR             (UART0_BASE + 0x44UL)
#define UART0_FR_TXFF         (1UL << 5)
#define UART0_FR_RXFE         (1UL << 4)

/* GIC-400 (GICv2) */
#define GICD_BASE             (0xFF841000UL)  /* Distributor    */
#define GICC_BASE             (0xFF842000UL)  /* CPU interface  */

/* ARM generic timer: physical non-secure EL1 timer PPI -> GIC INTID 30. */
#define TIMER_IRQ_INTID       (30U)

static inline void mmio_write32(uintptr_t addr, uint32_t value)
{
    *(volatile uint32_t *)addr = value;
}

static inline uint32_t mmio_read32(uintptr_t addr)
{
    return *(volatile uint32_t *)addr;
}

#endif /* FLINT_BSP_RPI4_H */
