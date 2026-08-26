// FlintRTOS - tests for capabilities and the flat CNode (K3, OM5).
#include "test_framework.hpp"

extern "C" {
#include "flint/cap.h"
}

// Helper: build a capability value for installation.
static flint_cap_t make_cap(flint_cap_type_t type, flint_rights_t rights,
                            flint_word_t object, flint_badge_t badge) {
    flint_cap_t c;
    c.type = type;
    c.rights = rights;
    c.badge = badge;
    c.object = object;
    c.parent = FLINT_CAP_NO_PARENT;
    return c;
}

FLINT_TEST(cap_install_and_lookup) {
    flint_cnode_t cn;
    flint_cnode_init(&cn);
    const flint_cap_t ep = make_cap(FLINT_CAP_ENDPOINT, FLINT_RIGHT_ALL, 42U, 0U);
    CHECK(flint_cnode_install(&cn, 5U, &ep) == FLINT_OK);

    flint_cap_t* got = nullptr;
    CHECK(flint_cnode_lookup(&cn, 5U, &got) == FLINT_OK);
    CHECK(got->type == FLINT_CAP_ENDPOINT);
    CHECK(got->object == 42U);
}

FLINT_TEST(cap_install_rejects_occupied) {
    flint_cnode_t cn;
    flint_cnode_init(&cn);
    const flint_cap_t ep = make_cap(FLINT_CAP_ENDPOINT, FLINT_RIGHT_ALL, 1U, 0U);
    CHECK(flint_cnode_install(&cn, 0U, &ep) == FLINT_OK);
    CHECK(flint_cnode_install(&cn, 0U, &ep) == FLINT_ERR_EXISTS);
}

FLINT_TEST(cap_lookup_out_of_range) {
    flint_cnode_t cn;
    flint_cnode_init(&cn);
    flint_cap_t* got = nullptr;
    CHECK(flint_cnode_lookup(&cn, FLINT_CFG_CNODE_SLOTS, &got) ==
          FLINT_ERR_CAP_INVALID);
}

FLINT_TEST(cap_copy_intersects_rights_never_adds) {
    flint_cnode_t cn;
    flint_cnode_init(&cn);
    // Source has READ|WRITE only.
    const flint_cap_t src =
        make_cap(FLINT_CAP_ENDPOINT, FLINT_RIGHT_READ | FLINT_RIGHT_WRITE, 7U, 0U);
    CHECK(flint_cnode_install(&cn, 1U, &src) == FLINT_OK);

    // Copy asking for ALL rights: result must NOT exceed the source's rights.
    CHECK(flint_cnode_copy(&cn, 2U, &cn, 1U, FLINT_RIGHT_ALL) == FLINT_OK);
    flint_cap_t* d = nullptr;
    CHECK(flint_cnode_lookup(&cn, 2U, &d) == FLINT_OK);
    CHECK(d->rights == (FLINT_RIGHT_READ | FLINT_RIGHT_WRITE));  // not GRANT/EXEC
    CHECK(d->parent == 1U);                                      // derivation edge

    // Copy asking for only WRITE: intersection is WRITE.
    CHECK(flint_cnode_copy(&cn, 3U, &cn, 1U, FLINT_RIGHT_WRITE) == FLINT_OK);
    flint_cap_t* d2 = nullptr;
    CHECK(flint_cnode_lookup(&cn, 3U, &d2) == FLINT_OK);
    CHECK(d2->rights == FLINT_RIGHT_WRITE);
}

FLINT_TEST(cap_copy_rejects_empty_source_and_occupied_dest) {
    flint_cnode_t cn;
    flint_cnode_init(&cn);
    CHECK(flint_cnode_copy(&cn, 2U, &cn, 1U, FLINT_RIGHT_ALL) == FLINT_ERR_EMPTY);

    const flint_cap_t a = make_cap(FLINT_CAP_ENDPOINT, FLINT_RIGHT_ALL, 1U, 0U);
    const flint_cap_t b = make_cap(FLINT_CAP_ENDPOINT, FLINT_RIGHT_ALL, 2U, 0U);
    CHECK(flint_cnode_install(&cn, 1U, &a) == FLINT_OK);
    CHECK(flint_cnode_install(&cn, 2U, &b) == FLINT_OK);
    CHECK(flint_cnode_copy(&cn, 2U, &cn, 1U, FLINT_RIGHT_ALL) == FLINT_ERR_EXISTS);
}

FLINT_TEST(cap_mint_stamps_badge_once) {
    flint_cnode_t cn;
    flint_cnode_init(&cn);
    const flint_cap_t ep = make_cap(FLINT_CAP_ENDPOINT, FLINT_RIGHT_ALL, 9U, 0U);
    CHECK(flint_cnode_install(&cn, 0U, &ep) == FLINT_OK);

    CHECK(flint_cnode_mint(&cn, 1U, &cn, 0U, FLINT_RIGHT_WRITE, 0x1234U) == FLINT_OK);
    flint_cap_t* m = nullptr;
    CHECK(flint_cnode_lookup(&cn, 1U, &m) == FLINT_OK);
    CHECK(m->badge == 0x1234U);
    CHECK(m->rights == FLINT_RIGHT_WRITE);

    // Re-minting an already-badged cap must fail (badge set once, K3.3).
    CHECK(flint_cnode_mint(&cn, 2U, &cn, 1U, FLINT_RIGHT_WRITE, 0x5678U) ==
          FLINT_ERR_STATE);
}

FLINT_TEST(cap_mint_rejects_unbadged_type) {
    flint_cnode_t cn;
    flint_cnode_init(&cn);
    // A Domain cap is not badge-bearing.
    const flint_cap_t dom = make_cap(FLINT_CAP_DOMAIN, FLINT_RIGHT_ALL, 3U, 0U);
    CHECK(flint_cnode_install(&cn, 0U, &dom) == FLINT_OK);
    CHECK(flint_cnode_mint(&cn, 1U, &cn, 0U, FLINT_RIGHT_ALL, 0x1U) ==
          FLINT_ERR_CAP_TYPE);
}

FLINT_TEST(cap_move_empties_source) {
    flint_cnode_t cn;
    flint_cnode_init(&cn);
    const flint_cap_t ep = make_cap(FLINT_CAP_ENDPOINT, FLINT_RIGHT_ALL, 11U, 0U);
    CHECK(flint_cnode_install(&cn, 0U, &ep) == FLINT_OK);
    CHECK(flint_cnode_move(&cn, 1U, &cn, 0U) == FLINT_OK);

    flint_cap_t* s = nullptr;
    flint_cap_t* d = nullptr;
    CHECK(flint_cnode_lookup(&cn, 0U, &s) == FLINT_OK);
    CHECK(flint_cnode_lookup(&cn, 1U, &d) == FLINT_OK);
    CHECK(s->type == FLINT_CAP_NULL);       // source emptied
    CHECK(d->type == FLINT_CAP_ENDPOINT);   // moved to dest
    CHECK(d->object == 11U);
}

FLINT_TEST(cap_delete_clears_slot) {
    flint_cnode_t cn;
    flint_cnode_init(&cn);
    const flint_cap_t ep = make_cap(FLINT_CAP_ENDPOINT, FLINT_RIGHT_ALL, 1U, 0U);
    CHECK(flint_cnode_install(&cn, 0U, &ep) == FLINT_OK);
    CHECK(flint_cnode_delete(&cn, 0U) == FLINT_OK);
    CHECK(flint_cnode_delete(&cn, 0U) == FLINT_ERR_EMPTY);  // already empty
}
