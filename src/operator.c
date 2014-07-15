#include <boruvka/alloc.h>

#include "plan/operator.h"

void planOperatorCondEffInit(plan_operator_cond_eff_t *ceff,
                             plan_state_pool_t *state_pool)
{
    ceff->pre = planPartStateNew(state_pool);
    ceff->eff = planPartStateNew(state_pool);
}

void planOperatorCondEffFree(plan_operator_cond_eff_t *ceff)
{
    planPartStateDel(ceff->pre);
    planPartStateDel(ceff->eff);
}

void planOperatorCondEffCopy(plan_state_pool_t *state_pool,
                             plan_operator_cond_eff_t *dst,
                             const plan_operator_cond_eff_t *src)
{
    int i;
    plan_var_id_t var;
    plan_val_t val;

    PLAN_PART_STATE_FOR_EACH(src->pre, i, var, val){
        planPartStateSet(state_pool, dst->pre, var, val);
    }

    PLAN_PART_STATE_FOR_EACH(src->eff, i, var, val){
        planPartStateSet(state_pool, dst->eff, var, val);
    }
}

void planOperatorInit(plan_operator_t *op, plan_state_pool_t *state_pool)
{
    op->state_pool = state_pool;
    op->pre = planPartStateNew(state_pool);
    op->eff = planPartStateNew(state_pool);
    op->cond_eff = NULL;
    op->cond_eff_size = 0;
    op->name = NULL;
    op->cost = PLAN_COST_ZERO;
    op->is_private = 0;
}

void planOperatorFree(plan_operator_t *op)
{
    int i;

    if (op->name)
        BOR_FREE(op->name);
    planPartStateDel(op->pre);
    planPartStateDel(op->eff);

    for (i = 0; i < op->cond_eff_size; ++i){
        planOperatorCondEffFree(op->cond_eff + i);
    }
    if (op->cond_eff)
        BOR_FREE(op->cond_eff);
}

void planOperatorCopy(plan_operator_t *dst, const plan_operator_t *src)
{
    int i;
    plan_var_id_t var;
    plan_val_t val;

    PLAN_PART_STATE_FOR_EACH(src->pre, i, var, val){
        planPartStateSet(dst->state_pool, dst->pre, var, val);
    }

    PLAN_PART_STATE_FOR_EACH(src->eff, i, var, val){
        planPartStateSet(dst->state_pool, dst->eff, var, val);
    }

    if (src->cond_eff_size > 0){
        dst->cond_eff_size = src->cond_eff_size;
        dst->cond_eff = BOR_ALLOC_ARR(plan_operator_cond_eff_t,
                                      dst->cond_eff_size);
        for (i = 0; i < dst->cond_eff_size; ++i){
            planOperatorCondEffInit(dst->cond_eff + i, dst->state_pool);
            planOperatorCondEffCopy(dst->state_pool,
                                    dst->cond_eff + i,
                                    src->cond_eff + i);
        }
    }

    dst->cost = src->cost;
    dst->name = strdup(src->name);
    dst->is_private = src->is_private;
}


void planOperatorSetPrecondition(plan_operator_t *op,
                                 plan_var_id_t var,
                                 plan_val_t val)
{
    planPartStateSet(op->state_pool, op->pre, var, val);
}

void planOperatorSetEffect(plan_operator_t *op,
                           plan_var_id_t var,
                           plan_val_t val)
{
    planPartStateSet(op->state_pool, op->eff, var, val);
}

void planOperatorSetName(plan_operator_t *op, const char *name)
{
    const char *c;
    int i;

    if (op->name)
        BOR_FREE(op->name);

    op->name = BOR_ALLOC_ARR(char, strlen(name) + 1);
    for (i = 0, c = name; c && *c; ++c, ++i){
        op->name[i] = *c;
    }
    op->name[i] = 0;
}

void planOperatorSetCost(plan_operator_t *op, plan_cost_t cost)
{
    op->cost = cost;
}

int planOperatorAddCondEff(plan_operator_t *op)
{
    ++op->cond_eff_size;
    op->cond_eff = BOR_REALLOC_ARR(op->cond_eff, plan_operator_cond_eff_t,
                                   op->cond_eff_size);
    planOperatorCondEffInit(op->cond_eff + op->cond_eff_size - 1,
                            op->state_pool);
    return op->cond_eff_size - 1;
}

void planOperatorCondEffSetPre(plan_operator_t *op, int cond_eff,
                               plan_var_id_t var, plan_val_t val)
{
    planPartStateSet(op->state_pool, op->cond_eff[cond_eff].pre, var, val);
}

void planOperatorCondEffSetEff(plan_operator_t *op, int cond_eff,
                               plan_var_id_t var, plan_val_t val)
{
    planPartStateSet(op->state_pool, op->cond_eff[cond_eff].eff, var, val);
}

static int condEffSimplifyCmp(const void *a, const void *b)
{
    const plan_operator_cond_eff_t *e1 = a;
    const plan_operator_cond_eff_t *e2 = b;
    return e2->pre->vals_size - e1->pre->vals_size;
}

void planOperatorCondEffSimplify(plan_operator_t *op)
{
    plan_operator_cond_eff_t *e1, *e2;
    int i, j, tmpi, cond_eff_size;
    plan_operator_cond_eff_t *cond_eff;
    plan_var_id_t var;
    plan_val_t val;

    if (op->cond_eff_size == 0)
        return;

    // Sort conditional effects so the first are the effects with more
    // preconditions (which means also more restrictive preconditions)
    qsort(op->cond_eff, op->cond_eff_size,
          sizeof(plan_operator_cond_eff_t),
          condEffSimplifyCmp);

    cond_eff_size = op->cond_eff_size;

    // Try to merge all conditional effects
    for (i = 0; i < op->cond_eff_size - 1; ++i){
        e1 = op->cond_eff + i;
        if (e1->pre == NULL)
            continue;

        for (j = i + 1; j < op->cond_eff_size; ++j){
            e2 = op->cond_eff + j;
            if (e2->pre == NULL)
                continue;

            if (planPartStateIsSubset(e2->pre, e1->pre, op->state_pool)){
                // Extend effects of e1 by effects of e2
                PLAN_PART_STATE_FOR_EACH(e2->eff, tmpi, var, val){
                    planPartStateSet(op->state_pool, e1->eff, var, val);
                }

                // and destroy e2
                planOperatorCondEffFree(e2);
                e2->pre = e2->eff = NULL;
                --cond_eff_size;
            }
        }
    }

    if (cond_eff_size < op->cond_eff_size){
        // If some cond effect was deleted the array must be reassigned
        cond_eff = BOR_ALLOC_ARR(plan_operator_cond_eff_t, cond_eff_size);
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

/** Bit operator: a = a | b */
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

plan_state_id_t planOperatorApply(const plan_operator_t *op,
                                  plan_state_id_t state_id)
{
    if (op->cond_eff_size == 0){
        // Use faster branch for non-conditional effects
        return planStatePoolApplyPartState(op->state_pool,
                                           op->eff->maskbuf,
                                           op->eff->valbuf,
                                           state_id);

    }else{
        int size = planStatePackerBufSize(op->state_pool->packer);
        char *maskbuf[size];
        char *valbuf[size];
        int i;

        // Initialize mask and value by non-conditional effects.
        memcpy(maskbuf, op->eff->maskbuf, size);
        memcpy(valbuf, op->eff->valbuf, size);

        // Test conditional effects
        for (i = 0; i < op->cond_eff_size; ++i){
            if (planStatePoolPartStateIsSubset(op->state_pool,
                                               op->cond_eff[i].pre,
                                               state_id)){
                // If condition associated with the effect holds, extend
                // mask and value buffers accordingly.
                bitOr(maskbuf, op->cond_eff[i].eff->maskbuf, size);
                bitOr(valbuf, op->cond_eff[i].eff->valbuf, size);
            }
        }

        return planStatePoolApplyPartState(op->state_pool, maskbuf, valbuf,
                                           state_id);
    }
}
