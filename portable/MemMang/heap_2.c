/*
 * FlintRTOS - MemMang heap_2 (manifest 2): best-fit, no coalescing.
 * Legacy allocator; heap_4 is preferred for most systems (it coalesces).
 * Kept for API completeness. Selected when configHEAP_ALGORITHM == 2.
 *
 * Skeleton: the free-list machinery mirrors heap_4 minus the coalescing step.
 * To be completed if a project selects this algorithm; heap_4 is the default.
 */
#include "FlintRTOS.h"

#if (configHEAP_ALGORITHM == 2)
#error "heap_2 is a skeleton in this revision - use configHEAP_ALGORITHM 1, 3, or 4"
#endif
