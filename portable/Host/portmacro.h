/*
 * FlintRTOS - Host (native x86-64) port macros for UNIT TESTING ONLY.
 * Lets the portable modules (list.c, queue.c, heap_*.c) compile and be tested
 * natively. Not a runtime port - context switching is not provided here.
 */
#ifndef FLINT_PORTMACRO_HOST_H
#define FLINT_PORTMACRO_HOST_H

#include <stdint.h>

typedef intptr_t  BaseType_t;
typedef uintptr_t UBaseType_t;
typedef uintptr_t StackType_t;
typedef uint32_t  TickType_t;

#define portMAX_DELAY         ((TickType_t)0xFFFFFFFFUL)
#define portSTACK_GROWTH      (-1)
#define portBYTE_ALIGNMENT    (16)
#define portBYTE_ALIGNMENT_MASK (0x0FU)
#define portTICK_PERIOD_MS    ((TickType_t)1000U / configTICK_RATE_HZ)
#define portPOINTER_SIZE_TYPE uintptr_t

/* No-op synchronisation for single-threaded host tests. */
#define portDISABLE_INTERRUPTS()  ((void)0)
#define portENABLE_INTERRUPTS()   ((void)0)
#define portENTER_CRITICAL()      ((void)0)
#define portEXIT_CRITICAL()       ((void)0)
#define portYIELD()               ((void)0)
#define portWAIT_FOR_INTERRUPT()  ((void)0)
#define portYIELD_FROM_ISR(x)     ((void)(x))

#define portTASK_FUNCTION_PROTO(vFn, pvParams)  void vFn(void *pvParams)
#define portTASK_FUNCTION(vFn, pvParams)        void vFn(void *pvParams)

#endif /* FLINT_PORTMACRO_HOST_H */
