/*
 * FlintRTOS - MemMang heap_1 (manifest 2): allocate-only, never frees.
 * The safest, most deterministic allocator: no fragmentation possible.
 * Suitable for systems that create all objects at start-up. MISRA C:2023.
 * Selected when FlintRTOSConfig.h sets configHEAP_ALGORITHM == 1.
 */
#include "FlintRTOS.h"
#include <stddef.h>
#include <stdint.h>

#if (configHEAP_ALGORITHM == 1)

#define heapALIGN_MASK ((size_t)(portBYTE_ALIGNMENT - 1))

static uint8_t ucHeap[configTOTAL_HEAP_SIZE];
static size_t  xNextFreeByte = 0U;

void *pvPortMalloc(size_t xWantedSize)
{
    void *pvReturn = NULL;

    if ((xWantedSize & heapALIGN_MASK) != 0U)
    {
        xWantedSize += (portBYTE_ALIGNMENT - (xWantedSize & heapALIGN_MASK));
    }

    if ((xNextFreeByte + xWantedSize) <= configTOTAL_HEAP_SIZE)
    {
        pvReturn = &(ucHeap[xNextFreeByte]);
        xNextFreeByte += xWantedSize;
    }
    return pvReturn;
}

void vPortFree(void *pv)
{
    (void)pv; /* heap_1 never frees */
}

size_t xPortGetFreeHeapSize(void)             { return configTOTAL_HEAP_SIZE - xNextFreeByte; }
size_t xPortGetMinimumEverFreeHeapSize(void)  { return configTOTAL_HEAP_SIZE - xNextFreeByte; }
void   vPortInitialiseBlocks(void)            { xNextFreeByte = 0U; }

#endif /* configHEAP_ALGORITHM == 1 */
