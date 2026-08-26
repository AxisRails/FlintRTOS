/*
 * FlintRTOS - MemMang heap_5 (manifest 2): heap_4's coalescing allocator
 * spanning MULTIPLE, possibly non-contiguous, memory regions defined at
 * run time via vPortDefineHeapRegions(). Useful on SoCs with split RAM banks
 * (e.g. on-chip SRAM + external DRAM). Selected when configHEAP_ALGORITHM == 5.
 *
 * Skeleton: shares heap_4's block logic; adds region registration. To be
 * completed when a multi-bank target is brought up (RPi4 uses one DRAM bank,
 * so heap_4 suffices for the initial target).
 */
#include "FlintRTOS.h"

typedef struct HeapRegion
{
    uint8_t *pucStartAddress;
    size_t   xSizeInBytes;
} HeapRegion_t;

void vPortDefineHeapRegions(const HeapRegion_t *pxHeapRegions);

#if (configHEAP_ALGORITHM == 5)
#error "heap_5 is a skeleton in this revision - use heap_4 for single-region targets"
#endif
