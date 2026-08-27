/*
 * FlintRTOS - portability layer include shim.
 * The build system's include path selects the correct portmacro.h
 * (portable/LLVM_AArch64 for RPi4 firmware, portable/Host for unit tests).
 */
#ifndef FLINT_PORTABLE_H
#define FLINT_PORTABLE_H

#include <stddef.h>
#include "portmacro.h"

/* ---- Kernel heap API (manifest 2: MemMang/heap_N.c) ---------------------- */
void  *pvPortMalloc(size_t xWantedSize);
void   vPortFree(void *pv);
size_t xPortGetFreeHeapSize(void);
size_t xPortGetMinimumEverFreeHeapSize(void);
void   vPortInitialiseBlocks(void);

/* ---- Context/scheduler port API (implemented in port.c / portASM.S) ------ */
struct tskTaskControlBlock; /* fwd */
StackType_t *pxPortInitialiseStack(StackType_t *pxTopOfStack,
                                   void (*pxCode)(void *), void *pvParameters);
BaseType_t xPortStartScheduler(void);
void       vPortTaskExit(void);
void       vPortSetupTimerInterrupt(void);

#endif /* FLINT_PORTABLE_H */
