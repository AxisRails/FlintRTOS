/*
 * FlintRTOS - IPC message tag encoding (K4.2).
 * TCB code (MISRA C:2023).
 *
 * A message tag packs, into one word:
 *   label  [16 bits]  application/method selector
 *   length  [7 bits]  number of message-register words in the payload
 *   n_caps  [3 bits]  number of capabilities to transfer
 *   flags   [6 bits]  e.g. E2E-present, extended-buffer-used
 *
 * Layout (bit 0 = LSB):
 *   flags  : bits  0..5
 *   n_caps : bits  6..8
 *   length : bits  9..15
 *   label  : bits 16..31
 */
#ifndef FLINT_MSGTAG_H
#define FLINT_MSGTAG_H

#include "flint/types.h"
#include "flint/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Message-tag flag bits (K4.2). */
#define FLINT_MSGFLAG_E2E     (0x01U) /* payload carries an E2E wrapper (COM3) */
#define FLINT_MSGFLAG_EXTBUF  (0x02U) /* payload spills into the IPC buffer     */

/* Field widths / limits (derived from the layout above). */
#define FLINT_MSGTAG_LABEL_MAX  (0xFFFFU) /* 16 bits */
#define FLINT_MSGTAG_LENGTH_MAX (0x7FU)   /*  7 bits */
#define FLINT_MSGTAG_NCAPS_MAX  (0x07U)   /*  3 bits */
#define FLINT_MSGTAG_FLAGS_MAX  (0x3FU)   /*  6 bits */

/*
 * Pack fields into a tag. Returns FLINT_ERR_RANGE (and leaves *out_tag
 * unchanged) if any field exceeds its width or n_caps/length exceed the
 * configured maxima. Otherwise writes the tag and returns FLINT_OK.
 */
flint_status_t flint_msgtag_pack(uint16_t label,
                                 uint8_t  length,
                                 uint8_t  n_caps,
                                 uint8_t  flags,
                                 flint_msgtag_t *out_tag);

/* Field accessors (total functions; no failure mode). */
uint16_t flint_msgtag_label(flint_msgtag_t tag);
uint8_t  flint_msgtag_length(flint_msgtag_t tag);
uint8_t  flint_msgtag_ncaps(flint_msgtag_t tag);
uint8_t  flint_msgtag_flags(flint_msgtag_t tag);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FLINT_MSGTAG_H */
