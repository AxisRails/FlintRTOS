/* Host-only stubs so tasks.c links in the native unit-test build. The scheduler
   is not actually run on the host; only queue/semaphore logic is exercised. */
#include "FlintRTOS.h"
StackType_t *pxPortInitialiseStack(StackType_t *top, void (*code)(void *), void *p)
{ (void)code; (void)p; return top; }
BaseType_t xPortStartScheduler(void) { return pdTRUE; }
void vPortSetupTimerInterrupt(void) { }
