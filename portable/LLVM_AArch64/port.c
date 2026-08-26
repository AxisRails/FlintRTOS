/*
 * FlintRTOS - AArch64 port (RPi4): stack init, critical sections, generic
 * timer tick, GIC-400 setup. TCB code (MISRA C:2023).
 * Build-verified; on-target runtime bring-up pending.
 */
#include "FlintRTOS.h"
#include "task.h"
#include "rpi4.h"

#include <stdint.h>

/* Context frame: 34 x 8-byte slots. Layout must match portASM.S:
   slots 0..30 = x0..x30, slot 31 = SPSR_EL1, slot 32 = ELR_EL1, slot 33 = pad. */
#define CTX_SLOTS   (34U)
#define CTX_SPSR    (31U)
#define CTX_ELR     (32U)

/* SPSR_EL1 for a fresh task: EL1h (M=0x5), DAIF cleared (IRQ enabled). */
#define INITIAL_SPSR  ((StackType_t)0x00000005ULL)

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

    /* --- GIC-400 (GICv2) minimal init --- */
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
}

/* Called from the asm IRQ entry (portASM.S) after context is saved.
   Acks the GIC, services the tick, and requests a reschedule. */
void vPortIrqHandlerC(void)
{
    uint32_t iar = mmio_read32(GICC_BASE + 0x00CU);  /* GICC_IAR */
    uint32_t intid = iar & 0x3FFU;

    if (intid == TIMER_IRQ_INTID)
    {
        arm_timer(ullTimerReloadTicks);              /* reload for next tick  */
        xTaskIncrementTick();
        vTaskSwitchContext();                        /* pick next task        */
    }

    mmio_write32(GICC_BASE + 0x010U, iar);           /* GICC_EOIR             */
}
