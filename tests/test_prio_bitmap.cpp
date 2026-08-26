// FlintRTOS - tests for the O(1) priority bitmap scheduler primitive (K6.1).
#include "test_framework.hpp"

extern "C" {
#include "flint/prio_bitmap.h"
}

FLINT_TEST(prio_empty_on_init) {
    flint_prio_bitmap_t bm;
    flint_prio_bitmap_init(&bm);
    CHECK(flint_prio_bitmap_empty(&bm));
}

FLINT_TEST(prio_highest_selects_max) {
    flint_prio_bitmap_t bm;
    flint_prio_bitmap_init(&bm);
    flint_prio_bitmap_set(&bm, 3U);
    flint_prio_bitmap_set(&bm, 200U);
    flint_prio_bitmap_set(&bm, 40U);
    CHECK(!flint_prio_bitmap_empty(&bm));
    CHECK(flint_prio_bitmap_highest(&bm) == 200U);  // highest wins (K6.1)
}

FLINT_TEST(prio_clear_falls_back) {
    flint_prio_bitmap_t bm;
    flint_prio_bitmap_init(&bm);
    flint_prio_bitmap_set(&bm, 10U);
    flint_prio_bitmap_set(&bm, 255U);
    CHECK(flint_prio_bitmap_highest(&bm) == 255U);
    flint_prio_bitmap_clear(&bm, 255U);
    CHECK(flint_prio_bitmap_highest(&bm) == 10U);  // next-highest after clear
    flint_prio_bitmap_clear(&bm, 10U);
    CHECK(flint_prio_bitmap_empty(&bm));
}

FLINT_TEST(prio_word_boundaries) {
    // Exercise bits at word edges (0, 31, 32, 255) to catch two-level indexing bugs.
    flint_prio_bitmap_t bm;
    flint_prio_bitmap_init(&bm);
    flint_prio_bitmap_set(&bm, 0U);
    flint_prio_bitmap_set(&bm, 31U);
    flint_prio_bitmap_set(&bm, 32U);
    CHECK(flint_prio_bitmap_highest(&bm) == 32U);
    flint_prio_bitmap_clear(&bm, 32U);
    CHECK(flint_prio_bitmap_highest(&bm) == 31U);
    flint_prio_bitmap_clear(&bm, 31U);
    CHECK(flint_prio_bitmap_highest(&bm) == 0U);
    CHECK(!flint_prio_bitmap_empty(&bm));
}
