/*
 * FlintRTOS - two-level priority bitmap for O(1) scheduler selection (K6.1).
 * TCB code (MISRA C:2023).
 *
 * Tracks which priority levels have at least one runnable thread. Selection of
 * the highest occupied priority is O(1) and independent of thread count
 * (A11.1 numeric scalability): a one-word summary indexes which level-1 word is
 * non-empty; a find-highest-set within that word gives the priority.
 *
 * Priority 0 is lowest; (FLINT_CFG_PRIO_COUNT - 1) is highest.
 */
#ifndef FLINT_PRIO_BITMAP_H
#define FLINT_PRIO_BITMAP_H

#include "flint/types.h"
#include "flint/config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLINT_PRIO_WORDS (FLINT_CFG_PRIO_COUNT / FLINT_WORD_BITS)

typedef struct flint_prio_bitmap
{
    flint_word_t summary;                  /* bit w set => level1[w] non-zero */
    flint_word_t level1[FLINT_PRIO_WORDS]; /* one bit per priority level      */
} flint_prio_bitmap_t;

/* Initialise to empty. */
void flint_prio_bitmap_init(flint_prio_bitmap_t *bm);

/* Mark / clear a priority level as occupied. prio must be < PRIO_COUNT. */
void flint_prio_bitmap_set(flint_prio_bitmap_t *bm, flint_prio_t prio);
void flint_prio_bitmap_clear(flint_prio_bitmap_t *bm, flint_prio_t prio);

/* True if no priority level is occupied. */
bool flint_prio_bitmap_empty(const flint_prio_bitmap_t *bm);

/*
 * Return the highest occupied priority. Precondition: not empty (caller checks
 * flint_prio_bitmap_empty first). O(1).
 */
flint_prio_t flint_prio_bitmap_highest(const flint_prio_bitmap_t *bm);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FLINT_PRIO_BITMAP_H */
