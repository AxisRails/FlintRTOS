/*
 * FlintRTOS - portable fixed-width kernel types.
 * TCB code (MISRA C:2023). No bare int/long in interfaces (MISRA Dir 4.6).
 */
#ifndef FLINT_TYPES_H
#define FLINT_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Machine word (logical). Host port uses 32-bit; a 64-bit target redefines. */
typedef uint32_t flint_word_t;

/* Capability pointer: an index into a CSpace (flat CNode) slot array (K3.2). */
typedef uint32_t flint_cptr_t;

/* Scheduling priority: 0 = lowest. Range checked against config. */
typedef uint16_t flint_prio_t;

/* Message tag as packed into a single word (K4.2); see msgtag.h. */
typedef flint_word_t flint_msgtag_t;

/* Immutable badge value carried by badge-bearing capabilities (K3.3). */
typedef flint_word_t flint_badge_t;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FLINT_TYPES_H */
