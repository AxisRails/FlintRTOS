/*
 * FlintRTOS - capabilities and the flat CNode (K3, OM5).
 * TCB code (MISRA C:2023).
 *
 * A capability is an unforgeable reference to a kernel object plus rights and
 * (for badge-bearing types) an immutable badge. Capabilities live only in
 * CSpace slots (kernel memory); user code names a slot by a cptr index (K3.1).
 *
 * v0.1 scope: cap representation, flat CNode with O(1) lookup, and the
 * copy / mint / move / delete operations with rights intersection. Revoke and
 * the derivation tree are a later increment (K3.4) - a single parent index is
 * reserved in the slot now so revoke can be added without layout change.
 */
#ifndef FLINT_CAP_H
#define FLINT_CAP_H

#include "flint/types.h"
#include "flint/status.h"
#include "flint/config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Object types a capability may name (K2). CAP_NULL marks an empty slot. */
typedef enum flint_cap_type
{
    FLINT_CAP_NULL         = 0,
    FLINT_CAP_UNTYPED      = 1,
    FLINT_CAP_DOMAIN       = 2,
    FLINT_CAP_THREAD       = 3,
    FLINT_CAP_CNODE        = 4,
    FLINT_CAP_ENDPOINT     = 5,
    FLINT_CAP_NOTIFICATION = 6,
    FLINT_CAP_REPLY        = 7,
    FLINT_CAP_SCHEDCONTEXT = 8,
    FLINT_CAP_IRQHANDLER   = 9,
    FLINT_CAP_MEMREGION    = 10
} flint_cap_type_t;

/* Rights bits (K3.3). Interpretation is per object type. */
#define FLINT_RIGHT_READ   (0x1U)
#define FLINT_RIGHT_WRITE  (0x2U)
#define FLINT_RIGHT_GRANT  (0x4U)
#define FLINT_RIGHT_EXEC   (0x8U)
#define FLINT_RIGHT_ALL    (0xFU)

typedef uint8_t flint_rights_t;

/* Sentinel: this cap has no derivation parent (root of a derivation chain). */
#define FLINT_CAP_NO_PARENT (0xFFFFFFFFU)

typedef struct flint_cap
{
    flint_cap_type_t type;    /* object type; CAP_NULL => empty slot            */
    flint_rights_t   rights;  /* rights bitset (K3.3)                           */
    flint_badge_t    badge;   /* immutable badge (badge-bearing types), else 0  */
    flint_word_t     object;  /* opaque object reference (id/index)             */
    flint_cptr_t     parent;  /* derivation parent slot, or FLINT_CAP_NO_PARENT */
} flint_cap_t;

/* A flat capability space: a fixed array of slots addressed by cptr (K3.2). */
typedef struct flint_cnode
{
    flint_cap_t slots[FLINT_CFG_CNODE_SLOTS];
} flint_cnode_t;

/* Initialise all slots to empty (CAP_NULL). */
void flint_cnode_init(flint_cnode_t *cn);

/*
 * Resolve a cptr to its slot. O(1), bounds-checked (K3.2, K7.4).
 * Returns FLINT_ERR_CAP_INVALID if cptr is out of range. A pointer to the slot
 * (which may be an empty CAP_NULL slot) is written to *out.
 */
flint_status_t flint_cnode_lookup(flint_cnode_t *cn, flint_cptr_t cptr,
                                  flint_cap_t **out);

/*
 * Install a capability directly into an empty slot. Used by the boot object-
 * graph builder (K9.2). Fails FLINT_ERR_EXISTS if the slot is occupied.
 */
flint_status_t flint_cnode_install(flint_cnode_t *cn, flint_cptr_t slot,
                                   const flint_cap_t *cap);

/*
 * Copy the cap at (src_cn, src) into (dst_cn, dst), intersecting its rights
 * with rights_mask (rights are never added - K3.4). The copy is a derivation
 * child of the source. Fails if src is empty, dst is occupied, or a cptr is
 * out of range. Badge is preserved from the source.
 */
flint_status_t flint_cnode_copy(flint_cnode_t *dst_cn, flint_cptr_t dst,
                                flint_cnode_t *src_cn, flint_cptr_t src,
                                flint_rights_t rights_mask);

/*
 * Like copy, but stamps a new badge. Only permitted on badge-bearing types
 * (Endpoint, Notification) and only when the source badge is 0 (a badge is set
 * once and never changed - K3.3). Otherwise FLINT_ERR_STATE / FLINT_ERR_CAP_TYPE.
 */
flint_status_t flint_cnode_mint(flint_cnode_t *dst_cn, flint_cptr_t dst,
                                flint_cnode_t *src_cn, flint_cptr_t src,
                                flint_rights_t rights_mask, flint_badge_t badge);

/* Move a cap to another slot; the source becomes empty. Atomic (K3.4). */
flint_status_t flint_cnode_move(flint_cnode_t *dst_cn, flint_cptr_t dst,
                                flint_cnode_t *src_cn, flint_cptr_t src);

/* Delete one cap, emptying its slot. (Object teardown is a later increment.) */
flint_status_t flint_cnode_delete(flint_cnode_t *cn, flint_cptr_t slot);

/* True if the type carries a badge (Endpoint / Notification). */
bool flint_cap_type_is_badged(flint_cap_type_t type);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FLINT_CAP_H */
