/*
 * FlintRTOS - two-level priority bitmap (K6.1). TCB code (MISRA C:2023).
 *
 * find-highest-set is a branch-only binary search (no compiler intrinsics, so
 * the code is portable and statically analysable) executing a fixed number of
 * steps => O(1) worst case, satisfying the determinism requirement (K10).
 */
#include "flint/prio_bitmap.h"

#include <stdint.h>

/* This implementation is specialised for a 32-bit logical word. */
_Static_assert(FLINT_WORD_BITS == 32U, "prio_bitmap assumes 32-bit word");
_Static_assert(FLINT_PRIO_WORDS <= FLINT_WORD_BITS,
               "summary word must index all level-1 words");

/* Highest set bit index (0..31) of a non-zero 32-bit word. Precondition: w != 0. */
static uint8_t word_highest_bit(flint_word_t w)
{
    uint8_t pos = 0U;

    if ((w & 0xFFFF0000U) != 0U) { pos = (uint8_t)(pos + 16U); w >>= 16U; }
    if ((w & 0x0000FF00U) != 0U) { pos = (uint8_t)(pos +  8U); w >>=  8U; }
    if ((w & 0x000000F0U) != 0U) { pos = (uint8_t)(pos +  4U); w >>=  4U; }
    if ((w & 0x0000000CU) != 0U) { pos = (uint8_t)(pos +  2U); w >>=  2U; }
    if ((w & 0x00000002U) != 0U) { pos = (uint8_t)(pos +  1U); }

    return pos;
}

void flint_prio_bitmap_init(flint_prio_bitmap_t *bm)
{
    uint32_t i;
    bm->summary = 0U;
    for (i = 0U; i < (uint32_t)FLINT_PRIO_WORDS; i++)
    {
        bm->level1[i] = 0U;
    }
}

void flint_prio_bitmap_set(flint_prio_bitmap_t *bm, flint_prio_t prio)
{
    const uint32_t word = (uint32_t)prio / FLINT_WORD_BITS;
    const uint32_t bit  = (uint32_t)prio % FLINT_WORD_BITS;

    bm->level1[word] |= ((flint_word_t)1U << bit);
    bm->summary      |= ((flint_word_t)1U << word);
}

void flint_prio_bitmap_clear(flint_prio_bitmap_t *bm, flint_prio_t prio)
{
    const uint32_t word = (uint32_t)prio / FLINT_WORD_BITS;
    const uint32_t bit  = (uint32_t)prio % FLINT_WORD_BITS;

    bm->level1[word] &= ~((flint_word_t)1U << bit);
    if (bm->level1[word] == 0U)
    {
        bm->summary &= ~((flint_word_t)1U << word);
    }
}

bool flint_prio_bitmap_empty(const flint_prio_bitmap_t *bm)
{
    return (bm->summary == 0U);
}

flint_prio_t flint_prio_bitmap_highest(const flint_prio_bitmap_t *bm)
{
    const uint8_t top_word = word_highest_bit(bm->summary);
    const uint8_t top_bit  = word_highest_bit(bm->level1[top_word]);

    return (flint_prio_t)(((uint32_t)top_word * FLINT_WORD_BITS) + (uint32_t)top_bit);
}
