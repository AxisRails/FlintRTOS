/*
 * FlintRTOS - MemMang heap_4 (manifest 2): first-fit allocator with coalescing
 * of adjacent free blocks. The general-purpose default. TCB code (MISRA C:2023).
 *
 * Original implementation of the standard design. Manages one static byte pool
 * of configTOTAL_HEAP_SIZE. Not used inside the certified microkernel TCB
 * (which forbids dynamic allocation) - this serves application/service code.
 */
#include "FlintRTOS.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef configTOTAL_HEAP_SIZE
#error "configTOTAL_HEAP_SIZE must be defined in FlintRTOSConfig.h"
#endif

#define heapBYTE_ALIGNMENT       portBYTE_ALIGNMENT
#define heapALIGN_MASK           ((size_t)(portBYTE_ALIGNMENT - 1))

/* Top bit of xBlockSize flags an allocated block. */
#define heapALLOC_BIT            (((size_t)1) << ((sizeof(size_t) * 8U) - 1U))

typedef struct A_BLOCK_LINK
{
    struct A_BLOCK_LINK *pxNextFreeBlock;
    size_t               xBlockSize;
} BlockLink_t;

static uint8_t     ucHeap[configTOTAL_HEAP_SIZE];
static BlockLink_t xStart;
static BlockLink_t *pxEnd = NULL;

static size_t xFreeBytesRemaining      = 0U;
static size_t xMinimumEverFreeBytes    = 0U;
static const size_t xHeapStructSize =
    (sizeof(BlockLink_t) + heapALIGN_MASK) & ~heapALIGN_MASK;

static bool prvBlockIsAllocated(const BlockLink_t *b)
{
    return (b->xBlockSize & heapALLOC_BIT) != 0U;
}

/* Insert a free block, coalescing with physically-adjacent free neighbours. */
static void prvInsertBlockIntoFreeList(BlockLink_t *pxBlockToInsert)
{
    BlockLink_t *pxIterator;
    uint8_t     *puc;

    for (pxIterator = &xStart;
         pxIterator->pxNextFreeBlock < pxBlockToInsert;
         pxIterator = pxIterator->pxNextFreeBlock)
    {
        /* find insertion point (ascending address) */
    }

    /* Coalesce with the preceding block if contiguous. */
    puc = (uint8_t *)pxIterator;
    if ((puc + pxIterator->xBlockSize) == (uint8_t *)pxBlockToInsert)
    {
        pxIterator->xBlockSize += pxBlockToInsert->xBlockSize;
        pxBlockToInsert = pxIterator;
    }

    /* Coalesce with the following block if contiguous. */
    puc = (uint8_t *)pxBlockToInsert;
    if ((puc + pxBlockToInsert->xBlockSize) == (uint8_t *)pxIterator->pxNextFreeBlock)
    {
        if (pxIterator->pxNextFreeBlock != pxEnd)
        {
            pxBlockToInsert->xBlockSize += pxIterator->pxNextFreeBlock->xBlockSize;
            pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock->pxNextFreeBlock;
        }
        else
        {
            pxBlockToInsert->pxNextFreeBlock = pxEnd;
        }
    }
    else
    {
        pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;
    }

    if (pxIterator != pxBlockToInsert)
    {
        pxIterator->pxNextFreeBlock = pxBlockToInsert;
    }
}

void vPortInitialiseBlocks(void)
{
    BlockLink_t *pxFirstFreeBlock;
    uint8_t     *pucAlignedHeap;
    uintptr_t    uxAddress = (uintptr_t)ucHeap;
    size_t       xTotalHeapSize = configTOTAL_HEAP_SIZE;

    /* Align the start of the usable heap. */
    if ((uxAddress & heapALIGN_MASK) != 0U)
    {
        uxAddress += heapALIGN_MASK;
        uxAddress &= ~(uintptr_t)heapALIGN_MASK;
        xTotalHeapSize -= (size_t)(uxAddress - (uintptr_t)ucHeap);
    }
    pucAlignedHeap = (uint8_t *)uxAddress;

    /* xStart holds the head of the free list. */
    xStart.pxNextFreeBlock = (BlockLink_t *)pucAlignedHeap;
    xStart.xBlockSize      = 0U;

    /* pxEnd marks the end, placed at the top of the pool. */
    uxAddress = ((uintptr_t)pucAlignedHeap) + xTotalHeapSize;
    uxAddress -= xHeapStructSize;
    uxAddress &= ~(uintptr_t)heapALIGN_MASK;
    pxEnd = (BlockLink_t *)uxAddress;
    pxEnd->xBlockSize      = 0U;
    pxEnd->pxNextFreeBlock = NULL;

    /* One big free block spanning the pool. */
    pxFirstFreeBlock = (BlockLink_t *)pucAlignedHeap;
    pxFirstFreeBlock->xBlockSize = (size_t)(uxAddress - (uintptr_t)pxFirstFreeBlock);
    pxFirstFreeBlock->pxNextFreeBlock = pxEnd;

    xFreeBytesRemaining   = pxFirstFreeBlock->xBlockSize;
    xMinimumEverFreeBytes = xFreeBytesRemaining;
}

void *pvPortMalloc(size_t xWantedSize)
{
    BlockLink_t *pxBlock;
    BlockLink_t *pxPreviousBlock;
    BlockLink_t *pxNewBlockLink;
    void        *pvReturn = NULL;

    if (pxEnd == NULL)
    {
        vPortInitialiseBlocks();
    }

    if ((xWantedSize == 0U) || ((xWantedSize & heapALLOC_BIT) != 0U))
    {
        return NULL;
    }

    /* Account for the header and round up to alignment. */
    xWantedSize += xHeapStructSize;
    if ((xWantedSize & heapALIGN_MASK) != 0U)
    {
        xWantedSize += (heapBYTE_ALIGNMENT - (xWantedSize & heapALIGN_MASK));
    }

    if (xWantedSize > xFreeBytesRemaining)
    {
        return NULL;
    }

    /* First fit. */
    pxPreviousBlock = &xStart;
    pxBlock = xStart.pxNextFreeBlock;
    while ((pxBlock->xBlockSize < xWantedSize) && (pxBlock->pxNextFreeBlock != NULL))
    {
        pxPreviousBlock = pxBlock;
        pxBlock = pxBlock->pxNextFreeBlock;
    }

    if (pxBlock != pxEnd)
    {
        /* Return the memory just past the header. */
        pvReturn = (void *)(((uint8_t *)pxPreviousBlock->pxNextFreeBlock) + xHeapStructSize);
        pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

        /* Split if the remainder is big enough to be a usable block. */
        if ((pxBlock->xBlockSize - xWantedSize) > (xHeapStructSize * 2U))
        {
            pxNewBlockLink = (BlockLink_t *)(((uint8_t *)pxBlock) + xWantedSize);
            pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xWantedSize;
            pxBlock->xBlockSize = xWantedSize;
            prvInsertBlockIntoFreeList(pxNewBlockLink);
        }

        xFreeBytesRemaining -= pxBlock->xBlockSize;
        if (xFreeBytesRemaining < xMinimumEverFreeBytes)
        {
            xMinimumEverFreeBytes = xFreeBytesRemaining;
        }

        pxBlock->xBlockSize |= heapALLOC_BIT;      /* mark allocated */
        pxBlock->pxNextFreeBlock = NULL;
    }

    return pvReturn;
}

void vPortFree(void *pv)
{
    uint8_t     *puc = (uint8_t *)pv;
    BlockLink_t *pxLink;

    if (pv == NULL)
    {
        return;
    }

    puc -= xHeapStructSize;
    pxLink = (BlockLink_t *)puc;

    if (!prvBlockIsAllocated(pxLink))
    {
        return;   /* double free / corruption - ignore */
    }

    pxLink->xBlockSize &= ~heapALLOC_BIT;          /* clear allocated flag */
    pxLink->pxNextFreeBlock = NULL;

    xFreeBytesRemaining += pxLink->xBlockSize;
    prvInsertBlockIntoFreeList(pxLink);
}

size_t xPortGetFreeHeapSize(void)
{
    if (pxEnd == NULL)
    {
        vPortInitialiseBlocks();
    }
    return xFreeBytesRemaining;
}

size_t xPortGetMinimumEverFreeHeapSize(void)
{
    if (pxEnd == NULL)
    {
        vPortInitialiseBlocks();
    }
    return xMinimumEverFreeBytes;
}
