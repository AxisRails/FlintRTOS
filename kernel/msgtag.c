/*
 * FlintRTOS - IPC message tag encoding (K4.2). TCB code (MISRA C:2023).
 */
#include "flint/msgtag.h"
#include "flint/config.h"

#include <stddef.h>
#include <stdint.h>

/* Shift positions for each field (see msgtag.h layout). */
enum msgtag_shift
{
    FLAGS_SHIFT  = 0,
    NCAPS_SHIFT  = 6,
    LENGTH_SHIFT = 9,
    LABEL_SHIFT  = 16
};

flint_status_t flint_msgtag_pack(uint16_t label,
                                 uint8_t  length,
                                 uint8_t  n_caps,
                                 uint8_t  flags,
                                 flint_msgtag_t *out_tag)
{
    flint_status_t st;

    if (out_tag == NULL)
    {
        st = FLINT_ERR_RANGE;
    }
    else if ((length > (uint8_t)FLINT_MSGTAG_LENGTH_MAX) ||
             ((uint32_t)length > FLINT_CFG_MR_MAX))
    {
        st = FLINT_ERR_MSG_TOO_LONG;
    }
    else if ((n_caps > (uint8_t)FLINT_MSGTAG_NCAPS_MAX) ||
             ((uint32_t)n_caps > FLINT_CFG_MSG_MAX_CAPS))
    {
        st = FLINT_ERR_MSG_TOO_LONG;
    }
    else if (flags > (uint8_t)FLINT_MSGTAG_FLAGS_MAX)
    {
        st = FLINT_ERR_RANGE;
    }
    else
    {
        flint_word_t tag = 0U;
        tag |= ((flint_word_t)flags  & (flint_word_t)FLINT_MSGTAG_FLAGS_MAX)  << FLAGS_SHIFT;
        tag |= ((flint_word_t)n_caps & (flint_word_t)FLINT_MSGTAG_NCAPS_MAX)  << NCAPS_SHIFT;
        tag |= ((flint_word_t)length & (flint_word_t)FLINT_MSGTAG_LENGTH_MAX) << LENGTH_SHIFT;
        tag |= ((flint_word_t)label  & (flint_word_t)FLINT_MSGTAG_LABEL_MAX)  << LABEL_SHIFT;
        *out_tag = tag;
        st = FLINT_OK;
    }

    return st;
}

uint16_t flint_msgtag_label(flint_msgtag_t tag)
{
    return (uint16_t)((tag >> LABEL_SHIFT) & (flint_word_t)FLINT_MSGTAG_LABEL_MAX);
}

uint8_t flint_msgtag_length(flint_msgtag_t tag)
{
    return (uint8_t)((tag >> LENGTH_SHIFT) & (flint_word_t)FLINT_MSGTAG_LENGTH_MAX);
}

uint8_t flint_msgtag_ncaps(flint_msgtag_t tag)
{
    return (uint8_t)((tag >> NCAPS_SHIFT) & (flint_word_t)FLINT_MSGTAG_NCAPS_MAX);
}

uint8_t flint_msgtag_flags(flint_msgtag_t tag)
{
    return (uint8_t)((tag >> FLAGS_SHIFT) & (flint_word_t)FLINT_MSGTAG_FLAGS_MAX);
}
