// FlintRTOS - tests for MemMang heap_4 (manifest 2).
#include "test_framework.hpp"

extern "C" {
#include "FlintRTOS.h"
#include "portable.h"
}

#include <cstdint>

FLINT_TEST(heap_alloc_free_roundtrip) {
    const size_t before = xPortGetFreeHeapSize();
    void* p = pvPortMalloc(128);
    CHECK(p != nullptr);
    CHECK(xPortGetFreeHeapSize() < before);   // shrank
    vPortFree(p);
    CHECK(xPortGetFreeHeapSize() == before);   // fully reclaimed via coalescing
}

FLINT_TEST(heap_alignment) {
    void* a = pvPortMalloc(1);
    void* b = pvPortMalloc(7);
    void* c = pvPortMalloc(33);
    CHECK(a != nullptr);
    CHECK(b != nullptr);
    CHECK(c != nullptr);
    // Returned pointers must honour portBYTE_ALIGNMENT (16).
    CHECK((reinterpret_cast<uintptr_t>(a) & 0xFU) == 0U);
    CHECK((reinterpret_cast<uintptr_t>(b) & 0xFU) == 0U);
    CHECK((reinterpret_cast<uintptr_t>(c) & 0xFU) == 0U);
    vPortFree(a);
    vPortFree(b);
    vPortFree(c);
}

FLINT_TEST(heap_zero_returns_null) {
    CHECK(pvPortMalloc(0) == nullptr);
}

FLINT_TEST(heap_free_null_is_safe) {
    const size_t before = xPortGetFreeHeapSize();
    vPortFree(nullptr);
    CHECK(xPortGetFreeHeapSize() == before);
}

FLINT_TEST(heap_coalesces_adjacent_frees) {
    // Allocate three, free the middle then its neighbours; the pool should
    // return to its original free size (coalescing works in both directions).
    const size_t before = xPortGetFreeHeapSize();
    void* a = pvPortMalloc(256);
    void* b = pvPortMalloc(256);
    void* c = pvPortMalloc(256);
    CHECK(a && b && c);
    vPortFree(b);
    vPortFree(a);
    vPortFree(c);
    CHECK(xPortGetFreeHeapSize() == before);
}

FLINT_TEST(heap_exhaustion_returns_null) {
    // A request larger than the whole pool must fail gracefully.
    void* huge = pvPortMalloc(configTOTAL_HEAP_SIZE * 2U);
    CHECK(huge == nullptr);
}
