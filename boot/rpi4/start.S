/*
 * FlintRTOS - Raspberry Pi 4 (AArch64) reset entry and low-level init.
 *
 * Responsibilities:
 *   - park secondary cores (only core 0 runs the kernel in this bring-up)
 *   - drop from the entry Exception Level to EL1
 *   - enable EL1 access to the physical generic timer
 *   - set the stack, clear BSS, install the vector table, call flint_boot()
 *
 * NOTE: build-verified (assembles/links); on-target runtime verification is
 * pending (no emulator in the build environment). Standard AArch64 patterns.
 */
    .section .text.boot
    .global _start

/*
 * Bring-up probe: emit one raw character on BOTH the PL011 UART and the
 * mini-UART (UART1), reusing whatever pin routing/baud the GPU firmware set up
 * (uart_2ndstage=1). Writing to both means the marker appears whichever UART
 * the firmware left on the GPIO14/15 header pins - which on a Pi 4 without a
 * device tree is the *mini-UART*, not PL011. This runs before uart_init() and
 * without a stack (no bl / no memory), so it works at any exception level and
 * shows exactly how far the kernel gets even if our driver is at fault.
 *   PL011:     DR=0xFE201000  FR=0xFE201018 (TXFF=bit5, 1=full)
 *   mini-UART: IO=0xFE215040  LSR=0xFE215054 (bit5=1 -> TX ready)
 * Every spin is bounded so a dead/absent UART can never hang the boot.
 * Clobbers x9-x13 only (all dead at every call site below).
 */
.macro BOOTMARK ch
    mov     w11, \ch
    /* --- PL011 --- */
    ldr     x9,  =0xFE201000
    ldr     x10, =0xFE201018
    mov     w13, #0x8000
Lp\@:
    ldr     w12, [x10]
    tbz     w12, #5, Lpw\@      /* TXFF clear -> not full -> write */
    subs    w13, w13, #1
    b.ne    Lp\@
Lpw\@:
    str     w11, [x9]
    /* --- mini-UART --- */
    ldr     x9,  =0xFE215040
    ldr     x10, =0xFE215054
    mov     w13, #0x8000
Lm\@:
    ldr     w12, [x10]
    tbnz    w12, #5, Lmw\@      /* TXRDY set -> write */
    subs    w13, w13, #1
    b.ne    Lm\@
Lmw\@:
    str     w11, [x9]
.endm

_start:
    /* Only core 0 (Aff0==0) continues; others wait for an event forever. */
    mrs     x0, mpidr_el1
    and     x0, x0, #0xFF
    cbz     x0, primary_core
hang_secondary:
    wfe
    b       hang_secondary

primary_core:
    BOOTMARK #'R'               /* R: reset entry reached (core 0) */
    /* Dispatch on the current Exception Level. */
    mrs     x0, CurrentEL
    lsr     x0, x0, #2          /* CurrentEL[3:2] -> EL number */
    cmp     x0, #3
    b.eq    from_el3
    cmp     x0, #2
    b.eq    from_el2
    b       at_el1              /* already EL1 */

from_el3:
    BOOTMARK #'3'               /* entered at EL3 */
    /* Configure EL3 to hand off to EL2 (Non-secure, AArch64). */
    mov     x0, #0x0501         /* SCR_EL3: RW | HCE | NS */
    msr     scr_el3, x0
    mov     x0, #0x03C9         /* SPSR_EL3: DAIF masked, return to EL2h */
    msr     spsr_el3, x0
    adr     x0, from_el2
    msr     elr_el3, x0
    eret

from_el2:
    BOOTMARK #'2'               /* entered/arrived at EL2 */
    /* Let EL1 use the physical timer and counter (CNTHCTL_EL2). */
    mrs     x0, cnthctl_el2
    orr     x0, x0, #3          /* EL1PCTEN | EL1PCEN */
    msr     cnthctl_el2, x0
    msr     cntvoff_el2, xzr    /* zero virtual offset */

    /* EL1 executes in AArch64. */
    mov     x0, #(1 << 31)      /* HCR_EL2.RW = 1 */
    msr     hcr_el2, x0

    /* Minimal SCTLR_EL1 (MMU/caches off for bring-up). */
    mov     x0, #0x0800
    movk    x0, #0x30d0, lsl #16
    msr     sctlr_el1, x0

    mov     x0, #0x3C5          /* SPSR_EL2: DAIF masked, return to EL1h */
    msr     spsr_el2, x0
    adr     x0, at_el1
    msr     elr_el2, x0
    eret

at_el1:
    BOOTMARK #'E'               /* E: now running at EL1 */

    /* Install the exception vector table (defined in portASM.S). */
    ldr     x0, =flint_vectors
    msr     vbar_el1, x0
    BOOTMARK #'V'               /* V: VBAR_EL1 installed */

    /* Set up the boot stack. */
    ldr     x0, =__boot_stack_top
    mov     sp, x0
    BOOTMARK #'S'               /* S: boot stack set */

    /* Zero the BSS. */
    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
zero_bss:
    cmp     x0, x1
    b.ge    bss_done
    str     xzr, [x0], #8
    b       zero_bss
bss_done:
    BOOTMARK #'B'               /* B: BSS cleared */

    BOOTMARK #'C'               /* C: entering C (flint_boot) */

    /* Enter C. Should never return; hang if it does. */
    bl      flint_boot
hang_main:
    wfe
    b       hang_main
