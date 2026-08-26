/*
 * FlintRTOS - AArch64 (ARMv8-A) port macros and types. Raspberry Pi 4.
 * TCB code (MISRA C:2023).
 */
#ifndef FLINT_PORTMACRO_AARCH64_H
#define FLINT_PORTMACRO_AARCH64_H

#include <stdint.h>

/* --- Port-specific scalar types (AArch64, LP64) --------------------------- */
typedef int64_t   BaseType_t;    /* natural signed word   */
typedef uint64_t  UBaseType_t;   /* natural unsigned word */
typedef uint64_t  StackType_t;   /* stack entries are 64-bit */

#if defined(FLINT_CFG_TICK_64BIT) && (FLINT_CFG_TICK_64BIT == 1)
typedef uint64_t  TickType_t;
#define portMAX_DELAY  ((TickType_t)0xFFFFFFFFFFFFFFFFULL)
#else
typedef uint32_t  TickType_t;
#define portMAX_DELAY  ((TickType_t)0xFFFFFFFFUL)
#endif

/* --- Architecture constants ----------------------------------------------- */
#define portSTACK_GROWTH      (-1)          /* full descending stack */
#define portBYTE_ALIGNMENT    (16)          /* AArch64 SP 16-byte aligned */
#define portBYTE_ALIGNMENT_MASK (0x0FU)
#define portTICK_PERIOD_MS    ((TickType_t)1000U / configTICK_RATE_HZ)
#define portPOINTER_SIZE_TYPE uint64_t

/* --- Critical sections: mask/unmask IRQ (DAIF.I) -------------------------- */
static inline void portDISABLE_INTERRUPTS(void)
{
    __asm__ volatile("msr daifset, #2" ::: "memory");
}

static inline void portENABLE_INTERRUPTS(void)
{
    __asm__ volatile("msr daifclr, #2" ::: "memory");
}

/* Nesting-aware critical section (implemented in port.c). */
void vPortEnterCritical(void);
void vPortExitCritical(void);
#define portENTER_CRITICAL()  vPortEnterCritical()
#define portEXIT_CRITICAL()   vPortExitCritical()

/* --- Yield ---------------------------------------------------------------- */
/* Request a context switch via a synchronous supervisor call (handled in
   portASM.S). Used by taskYIELD() and from ISRs that unblock a higher task. */
#define portYIELD()  __asm__ volatile("svc #0" ::: "memory")

void vPortYieldFromISR(void);
#define portYIELD_FROM_ISR(xSwitch)  do { if ((xSwitch) != pdFALSE) { vPortYieldFromISR(); } } while (0)

/* --- Task function prototype ---------------------------------------------- */
#define portTASK_FUNCTION_PROTO(vFn, pvParams)  void vFn(void *pvParams)
#define portTASK_FUNCTION(vFn, pvParams)        void vFn(void *pvParams)

#endif /* FLINT_PORTMACRO_AARCH64_H */
