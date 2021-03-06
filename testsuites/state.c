#include <cu/cu.h>
#include <boruvka/alloc.h>
#include "plan/state_pool.h"


TEST(testStateBasic)
{
    plan_var_t vars[4];
    plan_state_pool_t *pool;
    plan_state_t *state;
    plan_state_id_t ins[4];

    planVarInit(vars + 0, "a", 6);
    planVarInit(vars + 1, "b", 2);
    planVarInit(vars + 2, "c", 3);
    planVarInit(vars + 3, "d", 7);

    pool = planStatePoolNew(vars, 4);
    state = planStateNew(pool->num_vars);

    planStateZeroize(state);
    planStateSet(state, 0, 1);
    planStateSet(state, 1, 1);
    ins[0] = planStatePoolInsert(pool, state);
    assertEquals(planStatePoolInsert(pool, state), ins[0]);

    planStateZeroize(state);
    planStateSet(state, 2, 2);
    planStateSet(state, 3, 5);
    ins[1] = planStatePoolInsert(pool, state);
    assertEquals(planStatePoolInsert(pool, state), ins[1]);

    planStateZeroize(state);
    planStateSet(state, 0, 4);
    planStateSet(state, 1, 1);
    planStateSet(state, 2, 0);
    planStateSet(state, 3, 1);
    ins[2] = planStatePoolInsert(pool, state);
    assertEquals(planStatePoolInsert(pool, state), ins[2]);

    planStateZeroize(state);
    planStateSet(state, 0, 5);
    planStateSet(state, 1, 0);
    planStateSet(state, 2, 1);
    planStateSet(state, 3, 2);
    ins[3] = planStatePoolInsert(pool, state);
    assertEquals(planStatePoolInsert(pool, state), ins[3]);



    planStateZeroize(state);
    planStateSet(state, 0, 5);
    planStateSet(state, 1, 0);
    planStateSet(state, 2, 1);
    planStateSet(state, 3, 2);
    assertEquals(planStatePoolFind(pool, state), ins[3]);
    planStateSet(state, 3, 0);
    assertEquals(planStatePoolFind(pool, state), PLAN_NO_STATE);

    planStateZeroize(state);
    planStateSet(state, 2, 2);
    planStateSet(state, 3, 5);
    assertEquals(planStatePoolFind(pool, state), ins[1]);

    planStateZeroize(state);
    planStateSet(state, 0, 1);
    planStateSet(state, 1, 1);
    assertEquals(planStatePoolFind(pool, state), ins[0]);

    planStateZeroize(state);
    planStateSet(state, 0, 4);
    planStateSet(state, 1, 1);
    planStateSet(state, 2, 0);
    planStateSet(state, 3, 1);
    assertEquals(planStatePoolFind(pool, state), ins[2]);

    planStatePoolGetState(pool, ins[0], state);
    assertEquals(planStateGet(state, 0), 1);
    assertEquals(planStateGet(state, 1), 1);
    assertEquals(planStateGet(state, 2), 0);
    assertEquals(planStateGet(state, 3), 0);

    planStatePoolGetState(pool, ins[1], state);
    assertEquals(planStateGet(state, 0), 0);
    assertEquals(planStateGet(state, 1), 0);
    assertEquals(planStateGet(state, 2), 2);
    assertEquals(planStateGet(state, 3), 5);

    planStatePoolGetState(pool, ins[2], state);
    assertEquals(planStateGet(state, 0), 4);
    assertEquals(planStateGet(state, 1), 1);
    assertEquals(planStateGet(state, 2), 0);
    assertEquals(planStateGet(state, 3), 1);

    planStatePoolGetState(pool, ins[3], state);
    assertEquals(planStateGet(state, 0), 5);
    assertEquals(planStateGet(state, 1), 0);
    assertEquals(planStateGet(state, 2), 1);
    assertEquals(planStateGet(state, 3), 2);


    planStateDel(state);
    planStatePoolDel(pool);

    planVarFree(vars + 0);
    planVarFree(vars + 1);
    planVarFree(vars + 2);
    planVarFree(vars + 3);
}

TEST(testStatePreEff)
{
    plan_var_t vars[4];
    plan_state_pool_t *pool;
    plan_state_t *state;
    plan_state_id_t ids[4];
    plan_state_id_t newid;
    plan_part_state_t *parts[4];

    planVarInit(vars + 0, "a", 3);
    planVarInit(vars + 1, "b", 2);
    planVarInit(vars + 2, "c", 2);
    planVarInit(vars + 3, "d", 4);

    pool = planStatePoolNew(vars, 4);

    state = planStateNew(pool->num_vars);
    planStateSet(state, 0, 1);
    planStateSet(state, 1, 0);
    planStateSet(state, 2, 1);
    planStateSet(state, 3, 3);
    ids[0] = planStatePoolInsert(pool, state);

    planStateSet(state, 0, 0);
    planStateSet(state, 1, 0);
    planStateSet(state, 2, 1);
    planStateSet(state, 3, 3);
    ids[1] = planStatePoolInsert(pool, state);

    planStateSet(state, 0, 2);
    planStateSet(state, 1, 0);
    planStateSet(state, 2, 0);
    planStateSet(state, 3, 1);
    ids[2] = planStatePoolInsert(pool, state);

    planStateSet(state, 0, 0);
    planStateSet(state, 1, 0);
    planStateSet(state, 2, 0);
    planStateSet(state, 3, 3);
    ids[3] = planStatePoolInsert(pool, state);


    parts[0] = planPartStateNew(pool->num_vars);
    planPartStateSet(parts[0], 0, 1);
    planPartStateSet(parts[0], 2, 1);
    planStatePackerPackPartState(pool->packer, parts[0]);

    parts[1] = planPartStateNew(pool->num_vars);
    planPartStateSet(parts[1], 2, 1);
    planPartStateSet(parts[1], 3, 3);
    planStatePackerPackPartState(pool->packer, parts[1]);

    parts[2] = planPartStateNew(pool->num_vars);
    planPartStateSet(parts[2], 0, 2);
    planPartStateSet(parts[2], 2, 0);
    planPartStateSet(parts[2], 3, 1);
    planStatePackerPackPartState(pool->packer, parts[2]);

    parts[3] = planPartStateNew(pool->num_vars);
    planPartStateSet(parts[3], 3, 3);
    planPartStatePack(parts[3], pool->packer);


    assertTrue(planStatePoolPartStateIsSubset(pool, parts[0], ids[0]));
    assertFalse(planStatePoolPartStateIsSubset(pool, parts[0], ids[1]));
    assertFalse(planStatePoolPartStateIsSubset(pool, parts[0], ids[2]));
    assertFalse(planStatePoolPartStateIsSubset(pool, parts[0], ids[3]));

    assertTrue(planStatePoolPartStateIsSubset(pool, parts[1], ids[0]));
    assertTrue(planStatePoolPartStateIsSubset(pool, parts[1], ids[1]));
    assertFalse(planStatePoolPartStateIsSubset(pool, parts[1], ids[2]));
    assertFalse(planStatePoolPartStateIsSubset(pool, parts[1], ids[3]));

    assertFalse(planStatePoolPartStateIsSubset(pool, parts[2], ids[0]));
    assertFalse(planStatePoolPartStateIsSubset(pool, parts[2], ids[1]));
    assertTrue(planStatePoolPartStateIsSubset(pool, parts[2], ids[2]));
    assertFalse(planStatePoolPartStateIsSubset(pool, parts[2], ids[3]));

    assertTrue(planStatePoolPartStateIsSubset(pool, parts[3], ids[0]));
    assertTrue(planStatePoolPartStateIsSubset(pool, parts[3], ids[1]));
    assertFalse(planStatePoolPartStateIsSubset(pool, parts[3], ids[2]));
    assertTrue(planStatePoolPartStateIsSubset(pool, parts[3], ids[3]));

    // Apply parts[0]
    newid = planStatePoolApplyPartState(pool, parts[0], ids[0]);
    assertEquals(newid, ids[0]);
    assertNotEquals(newid, ids[1]);
    assertNotEquals(newid, ids[2]);
    assertNotEquals(newid, ids[3]);

    newid = planStatePoolApplyPartState(pool, parts[0], ids[1]);
    assertEquals(newid, ids[0]);
    assertNotEquals(newid, ids[1]);
    assertNotEquals(newid, ids[2]);
    assertNotEquals(newid, ids[3]);

    newid = planStatePoolApplyPartState(pool, parts[0], ids[2]);
    assertNotEquals(newid, ids[0]);
    assertNotEquals(newid, ids[1]);
    assertNotEquals(newid, ids[2]);
    assertNotEquals(newid, ids[3]);
    planStatePoolGetState(pool, newid, state);
    assertEquals(planStateGet(state, 0), 1);
    assertEquals(planStateGet(state, 1), 0);
    assertEquals(planStateGet(state, 2), 1);
    assertEquals(planStateGet(state, 3), 1);

    newid = planStatePoolApplyPartState(pool, parts[0], ids[3]);
    assertEquals(newid, ids[0]);
    assertNotEquals(newid, ids[1]);
    assertNotEquals(newid, ids[2]);
    assertNotEquals(newid, ids[3]);


    // Apply parts[1]
    newid = planStatePoolApplyPartState(pool, parts[1], ids[0]);
    assertEquals(newid, ids[0]);
    assertNotEquals(newid, ids[1]);
    assertNotEquals(newid, ids[2]);
    assertNotEquals(newid, ids[3]);

    newid = planStatePoolApplyPartState(pool, parts[1], ids[1]);
    assertNotEquals(newid, ids[0]);
    assertEquals(newid, ids[1]);
    assertNotEquals(newid, ids[2]);
    assertNotEquals(newid, ids[3]);

    newid = planStatePoolApplyPartState(pool, parts[1], ids[2]);
    assertNotEquals(newid, ids[0]);
    assertNotEquals(newid, ids[1]);
    assertNotEquals(newid, ids[2]);
    assertNotEquals(newid, ids[3]);
    planStatePoolGetState(pool, newid, state);
    assertEquals(planStateGet(state, 0), 2);
    assertEquals(planStateGet(state, 1), 0);
    assertEquals(planStateGet(state, 2), 1);
    assertEquals(planStateGet(state, 3), 3);

    newid = planStatePoolApplyPartState(pool, parts[1], ids[3]);
    assertNotEquals(newid, ids[0]);
    assertEquals(newid, ids[1]);
    assertNotEquals(newid, ids[2]);
    assertNotEquals(newid, ids[3]);


    // Apply parts[2]
    newid = planStatePoolApplyPartState(pool, parts[2], ids[0]);
    assertNotEquals(newid, ids[0]);
    assertNotEquals(newid, ids[1]);
    assertEquals(newid, ids[2]);
    assertNotEquals(newid, ids[3]);

    newid = planStatePoolApplyPartState(pool, parts[2], ids[1]);
    assertNotEquals(newid, ids[0]);
    assertNotEquals(newid, ids[1]);
    assertEquals(newid, ids[2]);
    assertNotEquals(newid, ids[3]);

    newid = planStatePoolApplyPartState(pool, parts[2], ids[2]);
    assertNotEquals(newid, ids[0]);
    assertNotEquals(newid, ids[1]);
    assertEquals(newid, ids[2]);
    assertNotEquals(newid, ids[3]);

    newid = planStatePoolApplyPartState(pool, parts[2], ids[3]);
    assertNotEquals(newid, ids[0]);
    assertNotEquals(newid, ids[1]);
    assertEquals(newid, ids[2]);
    assertNotEquals(newid, ids[3]);


    // Apply parts[3]
    newid = planStatePoolApplyPartState(pool, parts[3], ids[0]);
    assertEquals(newid, ids[0]);
    assertNotEquals(newid, ids[1]);
    assertNotEquals(newid, ids[2]);
    assertNotEquals(newid, ids[3]);

    newid = planStatePoolApplyPartState(pool, parts[3], ids[1]);
    assertNotEquals(newid, ids[0]);
    assertEquals(newid, ids[1]);
    assertNotEquals(newid, ids[2]);
    assertNotEquals(newid, ids[3]);

    newid = planStatePoolApplyPartState(pool, parts[3], ids[2]);
    assertNotEquals(newid, ids[0]);
    assertNotEquals(newid, ids[1]);
    assertNotEquals(newid, ids[2]);
    assertNotEquals(newid, ids[3]);
    planStatePoolGetState(pool, newid, state);
    assertEquals(planStateGet(state, 0), 2);
    assertEquals(planStateGet(state, 1), 0);
    assertEquals(planStateGet(state, 2), 0);
    assertEquals(planStateGet(state, 3), 3);

    newid = planStatePoolApplyPartState(pool, parts[3], ids[3]);
    assertNotEquals(newid, ids[0]);
    assertNotEquals(newid, ids[1]);
    assertNotEquals(newid, ids[2]);
    assertEquals(newid, ids[3]);



    planStateDel(state);
    planPartStateDel(parts[0]);
    planPartStateDel(parts[1]);
    planPartStateDel(parts[2]);
    planPartStateDel(parts[3]);
    planStatePoolDel(pool);
    planVarFree(vars + 0);
    planVarFree(vars + 1);
    planVarFree(vars + 2);
    planVarFree(vars + 3);
}

TEST(testPartStateUnset)
{
    plan_var_t vars[4];
    plan_state_pool_t *pool;
    plan_part_state_t *ps;
    plan_state_t *state;

    planVarInit(vars + 0, "a", 3);
    planVarInit(vars + 1, "b", 2);
    planVarInit(vars + 2, "c", 2);
    planVarInit(vars + 3, "d", 4);

    pool = planStatePoolNew(vars, 4);
    ps = planPartStateNew(pool->num_vars);

    planPartStateSet(ps, 1, 1);
    planPartStateSet(ps, 2, 0);
    planPartStateSet(ps, 3, 3);
    planPartStatePack(ps, pool->packer);

    assertFalse(planPartStateIsSet(ps, 0));
    assertTrue(planPartStateIsSet(ps, 1));
    assertTrue(planPartStateIsSet(ps, 2));
    assertTrue(planPartStateIsSet(ps, 3));
    assertEquals(planPartStateGet(ps, 0), PLAN_VAL_UNDEFINED);
    assertEquals(planPartStateGet(ps, 1), 1);
    assertEquals(planPartStateGet(ps, 2), 0);
    assertEquals(planPartStateGet(ps, 3), 3);

    state = planStateNew(pool->num_vars);
    planStateSet(state, 0, 1);
    planStateSet(state, 1, 1);
    planStateSet(state, 2, 0);
    planStateSet(state, 3, 3);
    planStatePoolInsert(pool, state); // ID = 0

    planStateSet(state, 0, 1);
    planStateSet(state, 1, 1);
    planStateSet(state, 2, 1);
    planStateSet(state, 3, 1);
    planStatePoolInsert(pool, state); // ID = 1

    planStateSet(state, 0, 1);
    planStateSet(state, 1, 1);
    planStateSet(state, 2, 0);
    planStateSet(state, 3, 2);
    planStatePoolInsert(pool, state); // ID = 2

    assertTrue(planStatePoolPartStateIsSubset(pool, ps, 0));
    assertFalse(planStatePoolPartStateIsSubset(pool, ps, 1));
    assertFalse(planStatePoolPartStateIsSubset(pool, ps, 2));


    planPartStateUnset(ps, 3);
    assertFalse(planPartStateIsSet(ps, 0));
    assertTrue(planPartStateIsSet(ps, 1));
    assertTrue(planPartStateIsSet(ps, 2));
    assertFalse(planPartStateIsSet(ps, 3));
    assertEquals(planPartStateGet(ps, 0), PLAN_VAL_UNDEFINED);
    assertEquals(planPartStateGet(ps, 1), 1);
    assertEquals(planPartStateGet(ps, 2), 0);
    assertEquals(planPartStateGet(ps, 3), PLAN_VAL_UNDEFINED);
    assertEquals(ps->vals_size, 2);
    assertEquals(ps->vals[0].var, 1);
    assertEquals(ps->vals[0].val, 1);
    assertEquals(ps->vals[1].var, 2);
    assertEquals(ps->vals[1].val, 0);

    planPartStatePack(ps, pool->packer);
    assertTrue(planStatePoolPartStateIsSubset(pool, ps, 0));
    assertFalse(planStatePoolPartStateIsSubset(pool, ps, 1));
    assertTrue(planStatePoolPartStateIsSubset(pool, ps, 2));


    planPartStateUnset(ps, 2);
    assertFalse(planPartStateIsSet(ps, 0));
    assertTrue(planPartStateIsSet(ps, 1));
    assertFalse(planPartStateIsSet(ps, 2));
    assertFalse(planPartStateIsSet(ps, 3));
    assertEquals(planPartStateGet(ps, 0), PLAN_VAL_UNDEFINED);
    assertEquals(planPartStateGet(ps, 1), 1);
    assertEquals(planPartStateGet(ps, 2), PLAN_VAL_UNDEFINED);
    assertEquals(planPartStateGet(ps, 3), PLAN_VAL_UNDEFINED);
    assertEquals(ps->vals_size, 1);
    assertEquals(ps->vals[0].var, 1);
    assertEquals(ps->vals[0].val, 1);

    planPartStatePack(ps, pool->packer);
    assertTrue(planStatePoolPartStateIsSubset(pool, ps, 0));
    assertTrue(planStatePoolPartStateIsSubset(pool, ps, 1));
    assertTrue(planStatePoolPartStateIsSubset(pool, ps, 2));

    planStateDel(state);
    planPartStateDel(ps);
    planStatePoolDel(pool);
    planVarFree(vars + 0);
    planVarFree(vars + 1);
    planVarFree(vars + 2);
    planVarFree(vars + 3);
}

static void _testPackerPubPart(int varsize)
{
    plan_var_t vars[varsize];
    plan_state_packer_t *packer;
    PLAN_STATE_STACK(state1, varsize);
    PLAN_STATE_STACK(state2, varsize);
    char *buf1, *buf2, *pubbuf, *privbuf;
    int i, pubsize = varsize / 2;
    int val, j;
    int last_word;

    for (i = 0; i < varsize - 1; ++i)
        planVarInit(vars + i, "a", (rand() % 1024) + 1);
    planVarInitMAPrivacy(vars + varsize - 1);
    for (i = pubsize; i < varsize - 1; ++i)
        planVarSetPrivate(vars + i);
    packer = planStatePackerNew(vars, varsize);
    /*
    fprintf(stderr, "Size: %d %d\n",
            planStatePackerBufSize(packer),
            planStatePackerBufSizePubPart(packer));
    */
    last_word  = planStatePackerBufSize(packer);
    last_word /= sizeof(plan_packer_word_t);
    last_word -= 1;

    buf1 = BOR_ALLOC_ARR(char, planStatePackerBufSize(packer));
    buf2 = BOR_ALLOC_ARR(char, planStatePackerBufSize(packer));
    pubbuf = BOR_ALLOC_ARR(char, planStatePackerBufSizePubPart(packer));
    privbuf = BOR_ALLOC_ARR(char, planStatePackerBufSizePrivatePart(packer));

    for (j = 0; j < 1000; ++j){
        for (i = 0; i < pubsize; ++i){
            planStateSet(&state1, i, rand() % vars[i].range);
            planStateSet(&state2, i, rand() % vars[i].range);
        }
        for (i = pubsize; i < varsize; ++i){
            val = rand() % vars[i].range;
            planStateSet(&state1, i, val);
            planStateSet(&state2, i, val);
        }

        planStatePackerPack(packer, &state1, buf1);
        planStatePackerPack(packer, &state2, buf2);

        // assert that ma-privacy variable is stored "as-is"
        assertEquals(planStateGet(&state1, varsize - 1),
                     ((plan_packer_word_t *)buf1)[last_word]);
        assertEquals(planStateGet(&state2, varsize - 1),
                     ((plan_packer_word_t *)buf2)[last_word]);

        planStatePackerExtractPubPart(packer, buf2, pubbuf);
        planStatePackerSetPubPart(packer, pubbuf, buf1);
        planStatePackerUnpack(packer, buf1, &state1);
        for (i = 0; i < varsize; ++i){
            assertEquals(planStateGet(&state1, i), planStateGet(&state2, i));
        }

        // Test *SetMAPrivacyVar() function
        planStatePackerSetMAPrivacyVar(packer,
                                       planStateGet(&state2, varsize - 1),
                                       buf1);
        planStatePackerUnpack(packer, buf1, &state1);
        assertEquals(planStateGet(&state2, varsize - 1),
                     planStateGet(&state1, varsize - 1));

        int val = rand();
        planStatePackerSetMAPrivacyVar(packer, val, buf1);
        planStatePackerSetMAPrivacyVar(packer, val, buf2);
        planStatePackerUnpack(packer, buf1, &state1);
        planStatePackerUnpack(packer, buf2, &state2);
        assertEquals(planStateGet(&state2, varsize - 1),
                     planStateGet(&state1, varsize - 1));
        assertEquals(planStateGet(&state2, varsize - 1), val);
    }

    for (j = 0; j < 1000; ++j){
        for (i = 0; i < pubsize; ++i){
            val = rand() % vars[i].range;
            planStateSet(&state1, i, val);
            planStateSet(&state2, i, val);
        }
        for (i = pubsize; i < varsize; ++i){
            planStateSet(&state1, i, rand() % vars[i].range);
            planStateSet(&state2, i, rand() % vars[i].range);
        }
        planStateSet(&state1, varsize - 1, 0);
        planStateSet(&state2, varsize - 1, 0);

        planStatePackerPack(packer, &state1, buf1);
        planStatePackerPack(packer, &state2, buf2);

        planStatePackerExtractPrivatePart(packer, buf2, privbuf);
        planStatePackerSetPrivatePart(packer, privbuf, buf1);
        planStatePackerUnpack(packer, buf1, &state1);
        for (i = 0; i < varsize; ++i){
            assertEquals(planStateGet(&state1, i), planStateGet(&state2, i));
        }
    }

    for (j = 0; j < 1000; ++j){
        for (i = 0; i < varsize; ++i){
            planStateSet(&state1, i, rand() % vars[i].range);
            planStateSet(&state2, i, rand() % vars[i].range);
        }
        planStateSet(&state1, varsize - 1, 0);
        planStateSet(&state2, varsize - 1, 0);

        planStatePackerPack(packer, &state1, buf1);
        planStatePackerPack(packer, &state2, buf2);

        planStatePackerExtractPrivatePart(packer, buf2, privbuf);
        planStatePackerSetPrivatePart(packer, privbuf, buf1);
        planStatePackerExtractPubPart(packer, buf2, pubbuf);
        planStatePackerSetPubPart(packer, pubbuf, buf1);
        planStatePackerUnpack(packer, buf1, &state1);
        for (i = 0; i < varsize; ++i){
            assertEquals(planStateGet(&state1, i), planStateGet(&state2, i));
        }
    }

    BOR_FREE(buf1);
    BOR_FREE(buf2);
    BOR_FREE(pubbuf);
    BOR_FREE(privbuf);
    planStatePackerDel(packer);
    for (i = 0; i < varsize; ++i)
        planVarFree(vars + i);
}

TEST(testPackerPubPart)
{
    _testPackerPubPart(7);
    _testPackerPubPart(20);
    _testPackerPubPart(100);
}
