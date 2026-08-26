/*
 * FlintRTOS - capabilities and the flat CNode (K3, OM5). TCB code (MISRA C:2023).
 */
#include "flint/cap.h"

#include <stdbool.h>
#include <stdint.h>

static void cap_clear(flint_cap_t *cap)
{
    cap->type   = FLINT_CAP_NULL;
    cap->rights = 0U;
    cap->badge  = 0U;
    cap->object = 0U;
    cap->parent = FLINT_CAP_NO_PARENT;
}

void flint_cnode_init(flint_cnode_t *cn)
{
    uint32_t i;
    for (i = 0U; i < (uint32_t)FLINT_CFG_CNODE_SLOTS; i++)
    {
        cap_clear(&cn->slots[i]);
    }
}

bool flint_cap_type_is_badged(flint_cap_type_t type)
{
    return (type == FLINT_CAP_ENDPOINT) || (type == FLINT_CAP_NOTIFICATION);
}

flint_status_t flint_cnode_lookup(flint_cnode_t *cn, flint_cptr_t cptr,
                                  flint_cap_t **out)
{
    flint_status_t st;

    if (cptr >= (flint_cptr_t)FLINT_CFG_CNODE_SLOTS)
    {
        st = FLINT_ERR_CAP_INVALID;
    }
    else
    {
        *out = &cn->slots[cptr];
        st = FLINT_OK;
    }

    return st;
}

flint_status_t flint_cnode_install(flint_cnode_t *cn, flint_cptr_t slot,
                                   const flint_cap_t *cap)
{
    flint_cap_t   *dst = NULL;
    flint_status_t st  = flint_cnode_lookup(cn, slot, &dst);

    if (st == FLINT_OK)
    {
        if (dst->type != FLINT_CAP_NULL)
        {
            st = FLINT_ERR_EXISTS;
        }
        else
        {
            *dst = *cap;
        }
    }

    return st;
}

/* Shared front-half of copy/mint: validate slots and fetch pointers. */
static flint_status_t derive_check(flint_cnode_t *dst_cn, flint_cptr_t dst,
                                   flint_cnode_t *src_cn, flint_cptr_t src,
                                   flint_cap_t **out_dst, flint_cap_t **out_src)
{
    flint_status_t st = flint_cnode_lookup(src_cn, src, out_src);

    if (st == FLINT_OK)
    {
        st = flint_cnode_lookup(dst_cn, dst, out_dst);
    }
    if (st == FLINT_OK)
    {
        if ((*out_src)->type == FLINT_CAP_NULL)
        {
            st = FLINT_ERR_EMPTY;
        }
        else if ((*out_dst)->type != FLINT_CAP_NULL)
        {
            st = FLINT_ERR_EXISTS;
        }
        else
        {
            /* ok */
        }
    }

    return st;
}

flint_status_t flint_cnode_copy(flint_cnode_t *dst_cn, flint_cptr_t dst,
                                flint_cnode_t *src_cn, flint_cptr_t src,
                                flint_rights_t rights_mask)
{
    flint_cap_t   *ps  = NULL;
    flint_cap_t   *pd  = NULL;
    flint_status_t st  = derive_check(dst_cn, dst, src_cn, src, &pd, &ps);

    if (st == FLINT_OK)
    {
        pd->type   = ps->type;
        pd->object = ps->object;
        pd->badge  = ps->badge;
        /* Rights are intersected, never added (K3.4). */
        pd->rights = (flint_rights_t)(ps->rights & rights_mask & (flint_rights_t)FLINT_RIGHT_ALL);
        pd->parent = src;
    }

    return st;
}

flint_status_t flint_cnode_mint(flint_cnode_t *dst_cn, flint_cptr_t dst,
                                flint_cnode_t *src_cn, flint_cptr_t src,
                                flint_rights_t rights_mask, flint_badge_t badge)
{
    flint_cap_t   *ps  = NULL;
    flint_cap_t   *pd  = NULL;
    flint_status_t st  = derive_check(dst_cn, dst, src_cn, src, &pd, &ps);

    if (st == FLINT_OK)
    {
        if (!flint_cap_type_is_badged(ps->type))
        {
            st = FLINT_ERR_CAP_TYPE;
        }
        else if (ps->badge != 0U)
        {
            /* A badge is set once and never re-badged (K3.3). */
            st = FLINT_ERR_STATE;
        }
        else
        {
            pd->type   = ps->type;
            pd->object = ps->object;
            pd->badge  = badge;
            pd->rights = (flint_rights_t)(ps->rights & rights_mask & (flint_rights_t)FLINT_RIGHT_ALL);
            pd->parent = src;
        }
    }

    return st;
}

flint_status_t flint_cnode_move(flint_cnode_t *dst_cn, flint_cptr_t dst,
                                flint_cnode_t *src_cn, flint_cptr_t src)
{
    flint_cap_t   *ps = NULL;
    flint_cap_t   *pd = NULL;
    flint_status_t st = derive_check(dst_cn, dst, src_cn, src, &pd, &ps);

    if (st == FLINT_OK)
    {
        *pd = *ps;
        cap_clear(ps);
    }

    return st;
}

flint_status_t flint_cnode_delete(flint_cnode_t *cn, flint_cptr_t slot)
{
    flint_cap_t   *p  = NULL;
    flint_status_t st = flint_cnode_lookup(cn, slot, &p);

    if (st == FLINT_OK)
    {
        if (p->type == FLINT_CAP_NULL)
        {
            st = FLINT_ERR_EMPTY;
        }
        else
        {
            cap_clear(p);
        }
    }

    return st;
}
