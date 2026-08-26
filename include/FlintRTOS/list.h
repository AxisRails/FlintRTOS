/*
 * FlintRTOS - List Management (manifest 1): ordered doubly-linked list.
 * The structural backbone of the scheduler's ready and delayed lists.
 * TCB code (MISRA C:2023). Original implementation of the standard design.
 */
#ifndef FLINT_LIST_H
#define FLINT_LIST_H

#include "FlintRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

struct xLIST;

/* A full list item: lives inside a TCB/queue and links into one list. */
typedef struct xLIST_ITEM
{
    TickType_t          xItemValue;   /* ordering key (priority/wake time)   */
    struct xLIST_ITEM  *pxNext;
    struct xLIST_ITEM  *pxPrevious;
    void               *pvOwner;      /* object that owns this item (a TCB)  */
    struct xLIST       *pxContainer;  /* the list currently holding the item */
} ListItem_t;

/* A compact end-marker item. Its first three members are layout-compatible
   with ListItem_t so it can terminate the ring. */
typedef struct xMINI_LIST_ITEM
{
    TickType_t         xItemValue;
    struct xLIST_ITEM *pxNext;
    struct xLIST_ITEM *pxPrevious;
} MiniListItem_t;

typedef struct xLIST
{
    volatile UBaseType_t uxNumberOfItems;
    ListItem_t          *pxIndex;     /* walk cursor for round-robin         */
    MiniListItem_t       xListEnd;    /* sentinel terminating the ring       */
} List_t;

/* ---- Accessor macros ------------------------------------------------------*/
#define listSET_LIST_ITEM_OWNER(pxItem, pxOwner)  ((pxItem)->pvOwner = (void *)(pxOwner))
#define listGET_LIST_ITEM_OWNER(pxItem)           ((pxItem)->pvOwner)
#define listSET_LIST_ITEM_VALUE(pxItem, xValue)   ((pxItem)->xItemValue = (xValue))
#define listGET_LIST_ITEM_VALUE(pxItem)           ((pxItem)->xItemValue)
#define listGET_ITEM_VALUE_OF_HEAD_ENTRY(pxList)  (((pxList)->xListEnd.pxNext)->xItemValue)
#define listGET_HEAD_ENTRY(pxList)                ((pxList)->xListEnd.pxNext)
#define listGET_NEXT(pxItem)                      ((pxItem)->pxNext)
#define listGET_END_MARKER(pxList)                ((ListItem_t *)(&((pxList)->xListEnd)))
#define listLIST_IS_EMPTY(pxList)                 (((pxList)->uxNumberOfItems == (UBaseType_t)0) ? pdTRUE : pdFALSE)
#define listCURRENT_LIST_LENGTH(pxList)           ((pxList)->uxNumberOfItems)
#define listGET_LIST_ITEM_CONTAINER(pxItem)       ((pxItem)->pxContainer)
#define listIS_CONTAINED_WITHIN(pxList, pxItem)   (((pxItem)->pxContainer == (pxList)) ? pdTRUE : pdFALSE)

/* Round-robin: advance pxIndex and return the owner of the next item. */
#define listGET_OWNER_OF_NEXT_ENTRY(pxOwner, pxList)                       \
    do {                                                                   \
        List_t *const pxL = (pxList);                                      \
        (pxL)->pxIndex = (pxL)->pxIndex->pxNext;                           \
        if ((void *)(pxL)->pxIndex == (void *)&((pxL)->xListEnd)) {        \
            (pxL)->pxIndex = (pxL)->pxIndex->pxNext;                       \
        }                                                                  \
        (pxOwner) = (pxL)->pxIndex->pvOwner;                               \
    } while (0)

/* ---- Functions ------------------------------------------------------------*/
void        vListInitialise(List_t *pxList);
void        vListInitialiseItem(ListItem_t *pxItem);
void        vListInsert(List_t *pxList, ListItem_t *pxNewListItem);       /* ordered   */
void        vListInsertEnd(List_t *pxList, ListItem_t *pxNewListItem);    /* FIFO      */
UBaseType_t uxListRemove(ListItem_t *pxItemToRemove);

#ifdef __cplusplus
}
#endif

#endif /* FLINT_LIST_H */
