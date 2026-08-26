/*
 * FlintRTOS - List Management (manifest 1). TCB code (MISRA C:2023).
 * Ordered circular doubly-linked list terminating in a sentinel (xListEnd).
 */
#include "list.h"

#include <stddef.h>

void vListInitialise(List_t *pxList)
{
    /* Empty list: index points at the sentinel, which rings back to itself. */
    pxList->pxIndex = (ListItem_t *)&(pxList->xListEnd);

    /* Sentinel sorts last so ordered inserts never fall off the end. */
    pxList->xListEnd.xItemValue = portMAX_DELAY;
    pxList->xListEnd.pxNext     = (ListItem_t *)&(pxList->xListEnd);
    pxList->xListEnd.pxPrevious = (ListItem_t *)&(pxList->xListEnd);

    pxList->uxNumberOfItems = (UBaseType_t)0;
}

void vListInitialiseItem(ListItem_t *pxItem)
{
    /* A fresh item belongs to no list. */
    pxItem->pxContainer = NULL;
    pxItem->pvOwner     = NULL;
    pxItem->pxNext      = NULL;
    pxItem->pxPrevious  = NULL;
    pxItem->xItemValue  = (TickType_t)0;
}

void vListInsertEnd(List_t *pxList, ListItem_t *pxNewListItem)
{
    ListItem_t *const pxIndex = pxList->pxIndex;

    /* Insert immediately before the current index -> FIFO within a priority. */
    pxNewListItem->pxNext          = pxIndex;
    pxNewListItem->pxPrevious      = pxIndex->pxPrevious;
    pxIndex->pxPrevious->pxNext    = pxNewListItem;
    pxIndex->pxPrevious            = pxNewListItem;

    pxNewListItem->pxContainer = pxList;
    pxList->uxNumberOfItems++;
}

void vListInsert(List_t *pxList, ListItem_t *pxNewListItem)
{
    ListItem_t       *pxIterator;
    const TickType_t  xValueOfInsertion = pxNewListItem->xItemValue;

    /* Walk to the correct ascending position. The sentinel's value is
       portMAX_DELAY, guaranteeing termination without a NULL check. */
    if (xValueOfInsertion == portMAX_DELAY)
    {
        pxIterator = pxList->xListEnd.pxPrevious;
    }
    else
    {
        for (pxIterator = (ListItem_t *)&(pxList->xListEnd);
             pxIterator->pxNext->xItemValue <= xValueOfInsertion;
             pxIterator = pxIterator->pxNext)
        {
            /* advance */
        }
    }

    pxNewListItem->pxNext            = pxIterator->pxNext;
    pxNewListItem->pxNext->pxPrevious = pxNewListItem;
    pxNewListItem->pxPrevious        = pxIterator;
    pxIterator->pxNext               = pxNewListItem;

    pxNewListItem->pxContainer = pxList;
    pxList->uxNumberOfItems++;
}

UBaseType_t uxListRemove(ListItem_t *pxItemToRemove)
{
    List_t *const pxList = pxItemToRemove->pxContainer;

    pxItemToRemove->pxNext->pxPrevious = pxItemToRemove->pxPrevious;
    pxItemToRemove->pxPrevious->pxNext = pxItemToRemove->pxNext;

    /* If the walk cursor pointed at this item, step it back. */
    if (pxList->pxIndex == pxItemToRemove)
    {
        pxList->pxIndex = pxItemToRemove->pxPrevious;
    }

    pxItemToRemove->pxContainer = NULL;
    pxList->uxNumberOfItems--;

    return pxList->uxNumberOfItems;
}
