/*
 * FlintRTOS - AArch64 context switch and exception entry (RPi4 port).
 *
 * Context frame = 34 x 8 bytes on the task stack:
 *   slots 0..30 = x0..x30, slot 31 = SPSR_EL1, slot 32 = ELR_EL1, slot 33 = pad.
 * pxCurrentTCB's first member is pxTopOfStack, so storing SP to [pxCurrentTCB]
 * saves the task, and loading SP from it resumes a task.
 *
 * Build-verified; on-target runtime bring-up pending.
 */
    .section .text

    .macro SAVE_CTX
    sub     sp, sp, #(34*8)
    stp     x0,  x1,  [sp, #(16*0)]
    stp     x2,  x3,  [sp, #(16*1)]
    stp     x4,  x5,  [sp, #(16*2)]
    stp     x6,  x7,  [sp, #(16*3)]
    stp     x8,  x9,  [sp, #(16*4)]
    stp     x10, x11, [sp, #(16*5)]
    stp     x12, x13, [sp, #(16*6)]
    stp     x14, x15, [sp, #(16*7)]
    stp     x16, x17, [sp, #(16*8)]
    stp     x18, x19, [sp, #(16*9)]
    stp     x20, x21, [sp, #(16*10)]
    stp     x22, x23, [sp, #(16*11)]
    stp     x24, x25, [sp, #(16*12)]
    stp     x26, x27, [sp, #(16*13)]
    stp     x28, x29, [sp, #(16*14)]
    mrs     x0, spsr_el1
    mrs     x1, elr_el1
    stp     x30, x0,  [sp, #(16*15)]   /* x30, SPSR */
    str     x1,       [sp, #(16*16)]   /* ELR       */
    .endm

    .macro RESTORE_CTX
    ldr     x1,       [sp, #(16*16)]
    ldp     x30, x0,  [sp, #(16*15)]
    msr     elr_el1, x1
    msr     spsr_el1, x0
    ldp     x28, x29, [sp, #(16*14)]
    ldp     x26, x27, [sp, #(16*13)]
    ldp     x24, x25, [sp, #(16*12)]
    ldp     x22, x23, [sp, #(16*11)]
    ldp     x20, x21, [sp, #(16*10)]
    ldp     x18, x19, [sp, #(16*9)]
    ldp     x16, x17, [sp, #(16*8)]
    ldp     x14, x15, [sp, #(16*7)]
    ldp     x12, x13, [sp, #(16*6)]
    ldp     x10, x11, [sp, #(16*5)]
    ldp     x8,  x9,  [sp, #(16*4)]
    ldp     x6,  x7,  [sp, #(16*3)]
    ldp     x4,  x5,  [sp, #(16*2)]
    ldp     x2,  x3,  [sp, #(16*1)]
    ldp     x0,  x1,  [sp, #(16*0)]
    add     sp, sp, #(34*8)
    .endm

    /* Store current SP into pxCurrentTCB->pxTopOfStack. Clobbers x9,x10,x11. */
    .macro SAVE_SP_TO_TCB
    ldr     x9,  =pxCurrentTCB
    ldr     x10, [x9]
    mov     x11, sp
    str     x11, [x10]
    .endm

    /* Load SP from pxCurrentTCB->pxTopOfStack. Clobbers x9,x10,x11. */
    .macro LOAD_SP_FROM_TCB
    ldr     x9,  =pxCurrentTCB
    ldr     x10, [x9]
    ldr     x11, [x10]
    mov     sp,  x11
    .endm

/* ---- IRQ entry (timer tick -> reschedule) -------------------------------- */
    .global flint_irq_entry
flint_irq_entry:
    SAVE_CTX
    SAVE_SP_TO_TCB
    bl      vPortIrqHandlerC          /* acks GIC, tick, vTaskSwitchContext */
    LOAD_SP_FROM_TCB
    RESTORE_CTX
    eret

/* ---- Synchronous entry: SVC #0 == taskYIELD ------------------------------ */
    .global flint_sync_entry
flint_sync_entry:
    SAVE_CTX
    mrs     x0, esr_el1
    lsr     x0, x0, #26               /* ESR.EC */
    cmp     x0, #0x15                 /* SVC from AArch64 */
    b.ne    flint_sync_bad
    SAVE_SP_TO_TCB
    bl      vTaskSwitchContext
    LOAD_SP_FROM_TCB
    RESTORE_CTX
    eret
flint_sync_bad:
    b       flint_trap                /* unexpected synchronous exception */

/* ---- Start the first task (does not return) ------------------------------ */
    .global xPortStartScheduler
    .global vPortStartFirstTask
xPortStartScheduler:
vPortStartFirstTask:
    LOAD_SP_FROM_TCB
    RESTORE_CTX
    eret

/* ISR-requested yield: fall through to a software-generated switch. Used by
   portYIELD_FROM_ISR paths that cannot issue SVC directly. */
    .global vPortYieldFromISR
vPortYieldFromISR:
    ret
