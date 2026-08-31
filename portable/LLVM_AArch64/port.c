/*
 * FlintRTOS - AArch64 port (RPi4): stack init, critical sections, generic
 * timer tick, GIC-400 setup. TCB code (MISRA C:2023).
 * Build-verified; on-target runtime bring-up pending.
 */
#include "FlintRTOS.h"
#include "task.h"
#include "rpi4.h"
#include "uart.h"

#include <stdint.h>

/* Context frame: 34 x 8-byte slots. Layout must match portASM.S:
   slots 0..30 = x0..x30, slot 31 = SPSR_EL1, slot 32 = ELR_EL1, slot 33 = pad. */
#define CTX_SLOTS   (34U)
#define CTX_SPSR    (31U)
#define CTX_ELR     (32U)

/* SPSR_EL1 for a fresh task: EL1h (M=0x5) with IRQ ENABLED (I=0) but FIQ,
 * SError and Debug MASKED (F=A=D=1). A task must run with IRQ enabled so the
 * timer can preempt it; masking F/A/D avoids spurious async exceptions during
 * bring-up. Bits: D(9)|A(8)|F(6)=0x340, plus M=EL1h(0x5) => 0x345. */
#define INITIAL_SPSR  ((StackType_t)0x00000345ULL)

extern void vPortTaskExit(void);

StackType_t *pxPortInitialiseStack(StackType_t *pxTopOfStack,
                                   void (*pxCode)(void *), void *pvParameters)
{
    StackType_t *sp = pxTopOfStack - CTX_SLOTS;
    UBaseType_t  i;

    for (i = 0U; i < CTX_SLOTS; i++)
    {
        sp[i] = 0U;
    }

    sp[0]        = (StackType_t)(uintptr_t)pvParameters;   /* x0 */
    sp[30]       = (StackType_t)(uintptr_t)vPortTaskExit;  /* x30 / LR */
    sp[CTX_SPSR] = INITIAL_SPSR;
    sp[CTX_ELR]  = (StackType_t)(uintptr_t)pxCode;         /* return-to == entry */

    return sp;
}

/* --- Unhandled-exception reporter (called from flint_trap in vectors.S) -----
 * Dumps the fault syndrome so an otherwise-silent hang becomes diagnosable.
 * EC (ESR_EL1[31:26]) decodes the cause: 0x25=data abort, 0x21=instr abort,
 * 0x22=PC alignment, 0x26=SP alignment, 0x00=unknown. Then halts. */
void flint_trap_report(uint64_t esr, uint64_t elr, uint64_t far, uint64_t spsr)
{
    uart_printf("\n*** FLINT TRAP: unhandled exception ***\n");
    uart_printf("  ESR_EL1 =%p  EC=%p\n", (void *)esr, (void *)((esr >> 26) & 0x3FU));
    uart_printf("  ELR_EL1 =%p  (faulting PC)\n", (void *)elr);
    uart_printf("  FAR_EL1 =%p  (faulting addr)\n", (void *)far);
    uart_printf("  SPSR_EL1=%p\n", (void *)spsr);
    uart_printf("  system halted.\n");
}

/* --- Nesting-aware critical sections -------------------------------------- */
static volatile UBaseType_t uxCriticalNesting = 0U;

void vPortEnterCritical(void)
{
    portDISABLE_INTERRUPTS();
    uxCriticalNesting++;
}

void vPortExitCritical(void)
{
    if (uxCriticalNesting > 0U)
    {
        uxCriticalNesting--;
        if (uxCriticalNesting == 0U)
        {
            portENABLE_INTERRUPTS();
        }
    }
}

/* --- ARM generic timer (physical, EL1) ------------------------------------ */
static uint64_t ullTimerReloadTicks = 0U;

static inline uint64_t read_cntfrq(void)
{
    uint64_t v;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(v));
    return v;
}

static inline void arm_timer(uint64_t ticks)
{
    __asm__ volatile("msr cntp_tval_el0, %0" :: "r"(ticks));
    __asm__ volatile("msr cntp_ctl_el0, %0" :: "r"((uint64_t)1)); /* enable */
}

void vPortSetupTimerInterrupt(void)
{
    const uint64_t freq = read_cntfrq();
    ullTimerReloadTicks = freq / (uint64_t)configTICK_RATE_HZ;

    /* CRITICAL: keep IRQs masked from here until the first task's eret. The
     * tasks were created with interrupts enabled (taskEXIT_CRITICAL), so if we
     * armed the timer now with IRQs unmasked, an early tick would fire while
     * still in vTaskStartScheduler - before any task runs - and SAVE_SP_TO_TCB
     * would overwrite pxCurrentTCB's stack pointer with the boot context,
     * corrupting the first task. xPortStartScheduler's eret restores the task
     * SPSR (I=0), re-enabling IRQs atomically as the first task begins. */
    portDISABLE_INTERRUPTS();

    uart_printf("[port] timer setup: cntfrq=%u Hz, reload=%u ticks/tick\n",
                (unsigned int)freq, (unsigned int)ullTimerReloadTicks);

    /* --- GIC-400 (GICv2) minimal init --- */
    uart_printf("[port] GIC init (GICD=%p GICC=%p), enabling PPI %u\n",
                (void *)GICD_BASE, (void *)GICC_BASE, (unsigned int)TIMER_IRQ_INTID);
    /* Distributor: enable. */
    mmio_write32(GICD_BASE + 0x000U, 1U);            /* GICD_CTLR = Enable    */
    /* Enable the timer PPI (INTID 30) in GICD_ISENABLER0. */
    mmio_write32(GICD_BASE + 0x100U, (1U << TIMER_IRQ_INTID));
    /* Priority for INTID30 (GICD_IPRIORITYR): mid priority. */
    mmio_write32(GICD_BASE + 0x400U + (TIMER_IRQ_INTID & ~3U),
                 (uint32_t)(0xA0U << ((TIMER_IRQ_INTID % 4U) * 8U)));

    /* CPU interface: priority mask + enable. */
    mmio_write32(GICC_BASE + 0x004U, 0xF0U);         /* GICC_PMR              */
    mmio_write32(GICC_BASE + 0x000U, 1U);            /* GICC_CTLR = Enable    */

    arm_timer(ullTimerReloadTicks);
    uart_printf("[port] generic timer armed; waiting for first tick IRQ...\n");
}

/* Called from the asm IRQ entry (portASM.S) after context is saved.
   Acks the GIC, services the tick, and requests a reschedule. */
void vPortIrqHandlerC(void)
{
    static uint32_t ulFirstIrq = 0U;
    uint32_t iar = mmio_read32(GICC_BASE + 0x00CU);  /* GICC_IAR */
    uint32_t intid = iar & 0x3FFU;

    if (ulFirstIrq == 0U)
    {
        ulFirstIrq = 1U;
        uart_printf("[port] first IRQ taken: INTID=%u (timer=%u) - IRQ path is live\n",
                    (unsigned int)intid, (unsigned int)TIMER_IRQ_INTID);
    }

    if (intid == TIMER_IRQ_INTID)
    {
        arm_timer(ullTimerReloadTicks);              /* reload for next tick  */
        xTaskIncrementTick();
        vTaskSwitchContext();                        /* pick next task        */
    }

    mmio_write32(GICC_BASE + 0x010U, iar);           /* GICC_EOIR             */
}
