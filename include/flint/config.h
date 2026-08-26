/*
 * FlintRTOS - compile-time configuration constants.
 * TCB code (MISRA C:2023). See docs/CODING-STANDARD.md.
 *
 * These parameters are fixed per build; a real system sets them from the
 * system description (see FlintRTOS-System-Description-Generator-Design).
 */
#ifndef FLINT_CONFIG_H
#define FLINT_CONFIG_H

/* Machine word size in bits for this build (host port: 32-bit logical word). */
#define FLINT_WORD_BITS (32U)

/*
 * Scheduler: number of priority levels (0 = lowest .. MAX_PRIO-1 = highest).
 * Must be a multiple of FLINT_WORD_BITS and <= FLINT_WORD_BITS*FLINT_WORD_BITS
 * so the two-level bitmap summary fits in one word (K6.1, O(1) selection).
 */
#define FLINT_CFG_PRIO_COUNT (256U)

/* IPC message registers: fast-path words and hard maximum (K4.2). */
#define FLINT_CFG_MR_FAST (4U)
#define FLINT_CFG_MR_MAX  (64U)

/* Maximum capabilities transferable in one IPC message (K4.3). */
#define FLINT_CFG_MSG_MAX_CAPS (4U)

/* Default flat CNode slot count per CSpace (K3.2). Power of two recommended. */
#define FLINT_CFG_CNODE_SLOTS (64U)

#endif /* FLINT_CONFIG_H */
