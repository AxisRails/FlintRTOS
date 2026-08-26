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
_start:
    /* Only core 0 (Aff0==0) continues; others wait for an event forever. */
    mrs     x0, mpidr_el1
    and     x0, x0, #0xFF
    cbz     x0, primary_core
hang_secondary:
    wfe
    b       hang_secondary

primary_core:
    /* Dispatch on the current Exception Level. */
    mrs     x0, CurrentEL
    lsr     x0, x0, #2          /* CurrentEL[3:2] -> EL number */
    cmp     x0, #3
    b.eq    from_el3
    cmp     x0, #2
    b.eq    from_el2
    b       at_el1              /* already EL1 */

from_el3:
    /* Configure EL3 to hand off to EL2 (Non-secure, AArch64). */
    mov     x0, #0x0501         /* SCR_EL3: RW | HCE | NS */
    msr     scr_el3, x0
    mov     x0, #0x03C9         /* SPSR_EL3: DAIF masked, return to EL2h */
    msr     spsr_el3, x0
    adr     x0, from_el2
    msr     elr_el3, x0
    eret

from_el2:
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
    /* Install the exception vector table (defined in portASM.S). */
    ldr     x0, =flint_vectors
    msr     vbar_el1, x0

    /* Set up the boot stack. */
    ldr     x0, =__boot_stack_top
    mov     sp, x0

    /* Zero the BSS. */
    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
zero_bss:
    cmp     x0, x1
    b.ge    bss_done
    str     xzr, [x0], #8
    b       zero_bss
bss_done:

    /* Enter C. Should never return; hang if it does. */
    bl      flint_boot
hang_main:
    wfe
    b       hang_main
