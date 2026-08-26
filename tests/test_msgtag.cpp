// FlintRTOS - tests for the IPC message-tag codec (K4.2).
#include "test_framework.hpp"

extern "C" {
#include "flint/msgtag.h"
#include "flint/config.h"
}

FLINT_TEST(msgtag_roundtrip) {
    flint_msgtag_t tag = 0U;
    const flint_status_t st =
        flint_msgtag_pack(0xBEEFU, 12U, 3U, FLINT_MSGFLAG_E2E, &tag);
    CHECK(st == FLINT_OK);
    CHECK(flint_msgtag_label(tag) == 0xBEEFU);
    CHECK(flint_msgtag_length(tag) == 12U);
    CHECK(flint_msgtag_ncaps(tag) == 3U);
    CHECK(flint_msgtag_flags(tag) == FLINT_MSGFLAG_E2E);
}

FLINT_TEST(msgtag_fields_are_independent) {
    // Max label must not bleed into other fields.
    flint_msgtag_t tag = 0U;
    CHECK(flint_msgtag_pack(0xFFFFU, 0U, 0U, 0U, &tag) == FLINT_OK);
    CHECK(flint_msgtag_label(tag) == 0xFFFFU);
    CHECK(flint_msgtag_length(tag) == 0U);
    CHECK(flint_msgtag_ncaps(tag) == 0U);
    CHECK(flint_msgtag_flags(tag) == 0U);
}

FLINT_TEST(msgtag_rejects_oversized_length) {
    flint_msgtag_t tag = 0x5A5A5A5AU;  // sentinel; must be untouched on failure
    const flint_status_t st =
        flint_msgtag_pack(0U, (uint8_t)(FLINT_CFG_MR_MAX + 1U), 0U, 0U, &tag);
    CHECK(st == FLINT_ERR_MSG_TOO_LONG);
    CHECK(tag == 0x5A5A5A5AU);  // fail-closed: no side effects (K1.3)
}

FLINT_TEST(msgtag_rejects_too_many_caps) {
    flint_msgtag_t tag = 0U;
    CHECK(flint_msgtag_pack(0U, 0U, (uint8_t)(FLINT_CFG_MSG_MAX_CAPS + 1U), 0U,
                            &tag) == FLINT_ERR_MSG_TOO_LONG);
}

FLINT_TEST(msgtag_rejects_null_out) {
    CHECK(flint_msgtag_pack(0U, 0U, 0U, 0U, nullptr) == FLINT_ERR_RANGE);
}
