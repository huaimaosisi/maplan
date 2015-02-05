#include <strings.h>
#include <string.h>
#include <boruvka/alloc.h>

#include "plan/op.h"

/** Initializes and frees conditional effect */
static void planOpCondEffInit(plan_op_cond_eff_t *ceff, int size);
static void planOpCondEffFree(plan_op_cond_eff_t *ceff);
/** Copies conditional effect from src to dst */
static void planOpCondEffCopy(plan_op_cond_eff_t *dst,
                              const plan_op_cond_eff_t *src);
/** Bit operator: a = a | b */
_bor_inline void bitOr(void *a, const void *b, int size);

void planOpInit(plan_op_t *op, int var_size)
{
    bzero(op, sizeof(*op));
    op->pre = planPartStateNew(var_size);
    op->eff = planPartStateNew(var_size);
    op->cost = PLAN_COST_ZERO;

    op->owner = -1;
}

void planOpFree(plan_op_t *op)
{
    int i;

    if (op->name)
        BOR_FREE(op->name);
    planPartStateDel(op->pre);
    planPartStateDel(op->eff);

    for (i = 0; i < op->cond_eff_size; ++i){
        planOpCondEffFree(op->cond_eff + i);
    }
    if (op->cond_eff)
        BOR_FREE(op->cond_eff);
}

void planOpCopy(plan_op_t *dst, const plan_op_t *src)
{
    int i;

    planPartStateCopy(dst->pre, src->pre);
    planPartStateCopy(dst->eff, src->eff);

    if (src->cond_eff_size > 0){
        dst->cond_eff_size = src->cond_eff_size;
        dst->cond_eff = BOR_ALLOC_ARR(plan_op_cond_eff_t,
                                      dst->cond_eff_size);
        for (i = 0; i < dst->cond_eff_size; ++i){
            planOpCondEffInit(dst->cond_eff + i, dst->eff->size);
            planOpCondEffCopy(dst->cond_eff + i, src->cond_eff + i);
        }
    }

    dst->cost = src->cost;
    dst->name = strdup(src->name);

    dst->global_id  = src->global_id;
    dst->owner      = src->owner;
    dst->ownerarr   = src->ownerarr;
    dst->is_private = src->is_private;
    dst->recv_agent = src->recv_agent;
}

static void unsetNonPrivate(plan_part_state_t *dst,
                            const plan_part_state_t *src,
                            const plan_var_t *var)
{
    int i;
    plan_var_id_t varid;
    plan_val_t val;

    PLAN_PART_STATE_FOR_EACH(src, i, varid, val){
        if (!var[varid].is_private[val])
            planPartStateUnset(dst, varid);
    }
}

void planOpCopyPrivate(plan_op_t *dst, const plan_op_t *src,
                       const plan_var_t *var)
{
    int i;

    planOpCopy(dst, src);
    unsetNonPrivate(dst->pre, src->pre, var);
    unsetNonPrivate(dst->eff, src->eff, var);

    for (i = 0; i < dst->cond_eff_size; ++i){
        unsetNonPrivate(dst->cond_eff[i].pre, src->cond_eff[i].pre, var);
        unsetNonPrivate(dst->cond_eff[i].eff, src->cond_eff[i].eff, var);
    }
}

static void planOpCondEffInit(plan_op_cond_eff_t *ceff, int size)
{
    ceff->pre = planPartStateNew(size);
    ceff->eff = planPartStateNew(size);
}

void planOpSetName(plan_op_t *op, const char *name)
{
    if (op->name)
        BOR_FREE(op->name);
    op->name = strdup(name);
}

int planOpAddCondEff(plan_op_t *op)
{
    ++op->cond_eff_size;
    op->cond_eff = BOR_REALLOC_ARR(op->cond_eff, plan_op_cond_eff_t,
                                   op->cond_eff_size);
    planOpCondEffInit(op->cond_eff + op->cond_eff_size - 1, op->eff->size);
    return op->cond_eff_size - 1;
}

void planOpCondEffSetPre(plan_op_t *op, int cond_eff,
                         plan_var_id_t var, plan_val_t val)
{
    planPartStateSet(op->cond_eff[cond_eff].pre, var, val);
}

void planOpCondEffSetEff(plan_op_t *op, int cond_eff,
                         plan_var_id_t var, plan_val_t val)
{
    planPartStateSet(op->cond_eff[cond_eff].eff, var, val);
}

void planOpDelLastCondEff(plan_op_t *op)
{
    planOpCondEffFree(op->cond_eff + op->cond_eff_size - 1);
    --op->cond_eff_size;
}

void planOpCondEffSimplify(plan_op_t *op)
{
    plan_op_cond_eff_t *e1, *e2;
    int i, j, tmpi, cond_eff_size;
    plan_op_cond_eff_t *cond_eff;
    plan_var_id_t var;
    plan_val_t val;

    if (op->cond_eff_size == 0)
        return;

    cond_eff_size = op->cond_eff_size;

    // Try to merge conditional effects with same conditions
    for (i = 0; i < op->cond_eff_size - 1; ++i){
        e1 = op->cond_eff + i;
        if (e1->pre == NULL)
            continue;

        for (j = i + 1; j < op->cond_eff_size; ++j){
            e2 = op->cond_eff + j;
            if (e2->pre == NULL)
                continue;

            if (planPartStateEq(e2->pre, e1->pre)){
                // Extend effects of e1 by effects of e2
                PLAN_PART_STATE_FOR_EACH(e2->eff, tmpi, var, val){
                    planPartStateSet(e1->eff, var, val);
                }

                // and destroy e2
                planOpCondEffFree(e2);
                e2->pre = e2->eff = NULL;
                --cond_eff_size;
            }
        }
    }

    if (cond_eff_size < op->cond_eff_size){
        // If some cond effect was deleted the array must be reassigned
        cond_eff = BOR_ALLOC_ARR(plan_op_cond_eff_t, cond_eff_size);
        for (i = 0, j = 0; i < op->cond_eff_size; ++i){
            if (op->cond_eff[i].pre != NULL){
                cond_eff[j++] = op->cond_eff[i];
            }
        }

        BOR_FREE(op->cond_eff);
        op->cond_eff = cond_eff;
        op->cond_eff_size = cond_eff_size;
    }
}

plan_state_id_t planOpApply(const plan_op_t *op,
                            plan_state_pool_t *state_pool,
                            plan_state_id_t state_id)
{
    if (op->cond_eff_size == 0){
        // Use faster branch for non-conditional effects
        return planStatePoolApplyPartState(state_pool, op->eff, state_id);

    }else{
        int size = planStatePackerBufSize(state_pool->packer);
        char maskbuf[size];
        char valbuf[size];
        int i;

        // Initialize mask and value by non-conditional effects.
        memcpy(maskbuf, op->eff->maskbuf, size);
        memcpy(valbuf, op->eff->valbuf, size);

        // Test conditional effects
        for (i = 0; i < op->cond_eff_size; ++i){
            if (planStatePoolPartStateIsSubset(state_pool,
                                               op->cond_eff[i].pre,
                                               state_id)){
                // If condition associated with the effect holds, extend
                // mask and value buffers accordingly.
                bitOr(maskbuf, op->cond_eff[i].eff->maskbuf, size);
                bitOr(valbuf, op->cond_eff[i].eff->valbuf, size);
            }
        }

        return planStatePoolApplyPartState2(state_pool, maskbuf, valbuf,
                                            state_id);
    }
}

void planOpAddRecvAgent(plan_op_t *op, int agent_id)
{
    uint64_t recv = 1 << agent_id;
    op->recv_agent |= recv;
}

void planOpDelRecvAgent(plan_op_t *op, int agent_id)
{
    uint64_t recv = 1 << agent_id;
    op->recv_agent &= ~recv;
}

void planOpAddOwner(plan_op_t *op, int agent_id)
{
    uint64_t ow = 1 << agent_id;
    op->ownerarr |= ow;
}

int planOpIsOwner(const plan_op_t *op, int agent_id)
{
    uint64_t ow = 1 << agent_id;
    return (op->ownerarr & ow);
}

void planOpSetFirstOwner(plan_op_t *op)
{
    uint64_t ow = op->ownerarr;
    int i;

    for (i = 0; i < 64; ++i){
        if (ow & 0x1){
            op->owner = i;
            break;
        }
        ow >>= 1;
    }
}

void planOpPack(plan_op_t *op, plan_state_packer_t *packer)
{
    int i;

    planPartStatePack(op->pre, packer);
    planPartStatePack(op->eff, packer);

    for (i = 0; i < op->cond_eff_size; ++i){
        planPartStatePack(op->cond_eff[i].pre, packer);
        planPartStatePack(op->cond_eff[i].eff, packer);
    }
}

static void planOpCondEffFree(plan_op_cond_eff_t *ceff)
{
    planPartStateDel(ceff->pre);
    planPartStateDel(ceff->eff);
}

static void planOpCondEffCopy(plan_op_cond_eff_t *dst,
                              const plan_op_cond_eff_t *src)
{
    planPartStateCopy(dst->pre, src->pre);
    planPartStateCopy(dst->eff, src->eff);
}

_bor_inline void bitOr(void *a, const void *b, int size)
{
    uint32_t *a32;
    const uint32_t *b32;
    uint8_t *a8;
    const uint8_t *b8;
    int size32, size8;

    size32 = size / 4;
    a32 = a;
    b32 = b;
    for (; size32 != 0; --size32, ++a32, ++b32){
        *a32 |= *b32;
    }

    size8 = size % 4;
    a8 = (uint8_t *)a32;
    b8 = (uint8_t *)b32;
    for (; size8 != 0; --size8, ++a8, ++b8){
        *a8 |= *b8;
    }
}
