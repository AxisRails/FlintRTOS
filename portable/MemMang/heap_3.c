/*
 * FlintRTOS - MemMang heap_3 (manifest 2): thread-safe wrapper over the
 * standard library malloc/free. Only usable where a C library heap exists
 * (host builds, or a target with newlib and a configured _sbrk). It simply
 * brackets malloc/free with the scheduler suspension for thread-safety.
 * Selected when configHEAP_ALGORITHM == 3.
 */
#include "FlintRTOS.h"

#if (configHEAP_ALGORITHM == 3)

#include <stdlib.h>

void *pvPortMalloc(size_t xWantedSize)
{
    void *pvReturn;
    portENTER_CRITICAL();
    pvReturn = malloc(xWantedSize);
    portEXIT_CRITICAL();
    return pvReturn;
}

void vPortFree(void *pv)
{
    if (pv != NULL)
    {
        portENTER_CRITICAL();
        free(pv);
        portEXIT_CRITICAL();
    }
}

/* Sizes are managed by the C library; report 0 (unknown). */
size_t xPortGetFreeHeapSize(void)            { return 0U; }
size_t xPortGetMinimumEverFreeHeapSize(void) { return 0U; }
void   vPortInitialiseBlocks(void)           { }

#endif /* configHEAP_ALGORITHM == 3 */
