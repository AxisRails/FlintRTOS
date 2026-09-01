/*
 * FlintRTOS - Raspberry Pi 4 C entry (called from start.S at EL1).
 * Performs early board init, then hands off to the application main().
 */
#include "uart.h"

#include <stdint.h>

extern int main(void);

/* ---- MMU: flat (identity) map so RAM is Normal memory ---------------------
 * With the MMU OFF, AArch64 treats all memory as Device-nGnRnE, which faults on
 * unaligned access. lwIP (and any non-trivial C) does unaligned accesses, so we
 * must enable the MMU. We identity-map the low 3 GB as Normal Non-Cacheable
 * (unaligned OK, and DMA-coherent with caches bypassed) and the top 1 GB - the
 * BCM2711 peripheral/GIC region - as Device-nGnRnE.
 *
 * 4 KB granule, T0SZ=32 (4 GB VA) -> the top lookup level is L1 with 1 GB block
 * descriptors, so a single table with four entries covers everything. */

/* L1 block descriptor: bit0=1 (block at L1), AF=bit10, AttrIndx=bits[4:2]. */
#define DESC_BLOCK_NORMAL_NC(base) ((uint64_t)(base) | (1ULL << 10) | (1ULL << 2) | 1ULL)
#define DESC_BLOCK_DEVICE(base)    ((uint64_t)(base) | (1ULL << 10) | (0ULL << 2) | 1ULL)

static uint64_t g_l1_table[512] __attribute__((aligned(4096)));

static void mmu_init(void)
{
    g_l1_table[0] = DESC_BLOCK_NORMAL_NC(0x00000000ULL);  /* RAM              */
    g_l1_table[1] = DESC_BLOCK_NORMAL_NC(0x40000000ULL);  /* RAM              */
    g_l1_table[2] = DESC_BLOCK_NORMAL_NC(0x80000000ULL);  /* RAM              */
    g_l1_table[3] = DESC_BLOCK_DEVICE(0xC0000000ULL);     /* peripherals+GIC  */

    /* MAIR: Attr0 = Device-nGnRnE (0x00), Attr1 = Normal Inner/Outer NC (0x44). */
    const uint64_t mair = (0x00ULL << 0) | (0x44ULL << 8);
    __asm__ volatile("msr mair_el1, %0" :: "r"(mair));

    /* TCR: T0SZ=32, 4KB granule (TG0=0), IPS=40-bit (2), disable TTBR1 (EPD1). */
    const uint64_t tcr = (32ULL) | (2ULL << 32) | (1ULL << 23);
    __asm__ volatile("msr tcr_el1, %0" :: "r"(tcr));

    __asm__ volatile("msr ttbr0_el1, %0" :: "r"((uint64_t)(uintptr_t)g_l1_table));

    __asm__ volatile("dsb ish");
    __asm__ volatile("tlbi vmalle1");
    __asm__ volatile("dsb ish");
    __asm__ volatile("isb");

    uint64_t sctlr;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    sctlr |= (1ULL << 0);    /* M: enable MMU (A stays 0 -> unaligned allowed) */
    sctlr |= (1ULL << 12);   /* I: instruction cache                          */
    __asm__ volatile("msr sctlr_el1, %0" :: "r"(sctlr));
    __asm__ volatile("isb");
}

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

    /* Enable the MMU (RAM as Normal memory) so unaligned accesses - which lwIP
     * and most C code perform - do not fault. Must happen before main(). */
    dbg_puts("[boot] enabling MMU (identity map, RAM=Normal-NC)...\n");
    mmu_init();
    dbg_puts("[boot] MMU on\n");

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
