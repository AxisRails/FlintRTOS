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
    wfe
    b       flint_trap
