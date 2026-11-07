/*
 * FlintRTOS - AArch64 EL1 exception vector table (RPi4 port).
 * 16 entries x 128 bytes, 2 KiB aligned (VBAR_EL1 requirement).
 *
 * Tasks run at EL1h (SPSel=1), so their SVC (taskYIELD) and the timer IRQ are
 * taken through the "Current EL with SPx" group and routed to the port
 * handlers in portASM.S. Other vectors trap (spin) so faults are observable.
 */
    .section .text
    .balign 2048
    .global flint_vectors
flint_vectors:

    /* --- Current EL with SP0 (unused: we run SPx) --- */
    .balign 0x80
    b       flint_trap          /* Synchronous */
    .balign 0x80
    b       flint_trap          /* IRQ         */
    .balign 0x80
    b       flint_trap          /* FIQ         */
    .balign 0x80
    b       flint_trap          /* SError      */

    /* --- Current EL with SPx (the group tasks use) --- */
    .balign 0x80
    b       flint_sync_entry    /* Synchronous (SVC == yield) */
    .balign 0x80
    b       flint_irq_entry     /* IRQ (timer tick)           */
    .balign 0x80
    b       flint_trap          /* FIQ         */
    .balign 0x80
    b       flint_trap          /* SError      */

    /* --- Lower EL, AArch64 (future: EL0 user tasks) --- */
    .balign 0x80
    b       flint_trap
    .balign 0x80
    b       flint_irq_entry
    .balign 0x80
    b       flint_trap
    .balign 0x80
    b       flint_trap

    /* --- Lower EL, AArch32 (unused) --- */
    .balign 0x80
    b       flint_trap
    .balign 0x80
    b       flint_trap
    .balign 0x80
    b       flint_trap
    .balign 0x80
    b       flint_trap

    .balign 4
    .global flint_trap
flint_trap:
    /* Unhandled exception. Capture the faulting context's FP/LR/SP before
     * touching anything, move to a dedicated trap stack (the task stack may be
     * the thing that just overflowed), then hand the syndrome to C, which
     * records a crash report and resets the board (flint_panic). */
    mov     x4, x29                   /* frame pointer at the fault */
    mov     x5, x30                   /* link register at the fault */
    mov     x6, sp                    /* stack pointer at the fault */
    ldr     x9, =flint_trap_stack_top
    mov     sp, x9
    mrs     x0, esr_el1
    mrs     x1, elr_el1
    mrs     x2, far_el1
    mrs     x3, spsr_el1
    bl      flint_trap_report         /* does not return */
flint_trap_halt:
    wfe
    b       flint_trap_halt

    .section .bss
    .balign 16
flint_trap_stack:
    .space  8192
    .global flint_trap_stack_top
flint_trap_stack_top:
