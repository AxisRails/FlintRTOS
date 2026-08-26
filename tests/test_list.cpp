// FlintRTOS - tests for the List Management module (manifest 1).
#include "test_framework.hpp"

extern "C" {
#include "list.h"
}

// Owner tag helper (stored as void* in the item).
static void set_owner(ListItem_t* it, unsigned long tag) {
    listSET_LIST_ITEM_OWNER(it, reinterpret_cast<void*>(tag));
}
static unsigned long owner(const ListItem_t* it) {
    return reinterpret_cast<unsigned long>(listGET_LIST_ITEM_OWNER(it));
}

FLINT_TEST(list_init_is_empty) {
    List_t l;
    vListInitialise(&l);
    CHECK(listCURRENT_LIST_LENGTH(&l) == 0U);
    CHECK(listLIST_IS_EMPTY(&l) == pdTRUE);
}

FLINT_TEST(list_insert_end_is_fifo) {
    List_t l;
    vListInitialise(&l);
    ListItem_t a, b, c;
    vListInitialiseItem(&a);
    vListInitialiseItem(&b);
    vListInitialiseItem(&c);
    set_owner(&a, 1);
    set_owner(&b, 2);
    set_owner(&c, 3);
    vListInsertEnd(&l, &a);
    vListInsertEnd(&l, &b);
    vListInsertEnd(&l, &c);
    CHECK(listCURRENT_LIST_LENGTH(&l) == 3U);

    // Head entry is the first inserted (FIFO).
    CHECK(owner(listGET_HEAD_ENTRY(&l)) == 1U);
    // Round-robin walk should visit 1,2,3 in order.
    void* o = nullptr;
    listGET_OWNER_OF_NEXT_ENTRY(o, &l);
    CHECK(reinterpret_cast<unsigned long>(o) == 1U);
    listGET_OWNER_OF_NEXT_ENTRY(o, &l);
    CHECK(reinterpret_cast<unsigned long>(o) == 2U);
    listGET_OWNER_OF_NEXT_ENTRY(o, &l);
    CHECK(reinterpret_cast<unsigned long>(o) == 3U);
    listGET_OWNER_OF_NEXT_ENTRY(o, &l);  // wraps back
    CHECK(reinterpret_cast<unsigned long>(o) == 1U);
}

FLINT_TEST(list_insert_is_ordered_by_value) {
    List_t l;
    vListInitialise(&l);
    ListItem_t a, b, c;
    vListInitialiseItem(&a);
    vListInitialiseItem(&b);
    vListInitialiseItem(&c);
    listSET_LIST_ITEM_VALUE(&a, 50U); set_owner(&a, 50);
    listSET_LIST_ITEM_VALUE(&b, 10U); set_owner(&b, 10);
    listSET_LIST_ITEM_VALUE(&c, 30U); set_owner(&c, 30);
    vListInsert(&l, &a);   // 50
    vListInsert(&l, &b);   // 10
    vListInsert(&l, &c);   // 30
    // Ascending order at head: 10, 30, 50
    CHECK(listGET_ITEM_VALUE_OF_HEAD_ENTRY(&l) == 10U);
    ListItem_t* h = listGET_HEAD_ENTRY(&l);
    CHECK(owner(h) == 10U);
    CHECK(owner(listGET_NEXT(h)) == 30U);
    CHECK(owner(listGET_NEXT(listGET_NEXT(h))) == 50U);
}

FLINT_TEST(list_remove_updates_length_and_container) {
    List_t l;
    vListInitialise(&l);
    ListItem_t a, b;
    vListInitialiseItem(&a);
    vListInitialiseItem(&b);
    set_owner(&a, 1);
    set_owner(&b, 2);
    vListInsertEnd(&l, &a);
    vListInsertEnd(&l, &b);
    CHECK(listIS_CONTAINED_WITHIN(&l, &a) == pdTRUE);

    UBaseType_t remaining = uxListRemove(&a);
    CHECK(remaining == 1U);
    CHECK(listGET_LIST_ITEM_CONTAINER(&a) == nullptr);
    CHECK(owner(listGET_HEAD_ENTRY(&l)) == 2U);

    remaining = uxListRemove(&b);
    CHECK(remaining == 0U);
    CHECK(listLIST_IS_EMPTY(&l) == pdTRUE);
}

FLINT_TEST(list_ordered_insert_stable_for_equal_values) {
    // Equal item values keep insertion order (<= in the walk).
    List_t l;
    vListInitialise(&l);
    ListItem_t a, b, c;
    vListInitialiseItem(&a);
    vListInitialiseItem(&b);
    vListInitialiseItem(&c);
    listSET_LIST_ITEM_VALUE(&a, 20U); set_owner(&a, 1);
    listSET_LIST_ITEM_VALUE(&b, 20U); set_owner(&b, 2);
    listSET_LIST_ITEM_VALUE(&c, 20U); set_owner(&c, 3);
    vListInsert(&l, &a);
    vListInsert(&l, &b);
    vListInsert(&l, &c);
    ListItem_t* h = listGET_HEAD_ENTRY(&l);
    CHECK(owner(h) == 1U);
    CHECK(owner(listGET_NEXT(h)) == 2U);
    CHECK(owner(listGET_NEXT(listGET_NEXT(h))) == 3U);
}
