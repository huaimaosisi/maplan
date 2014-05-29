#include <cu/cu.h>
#include "plan/state.h"


TEST(testStateBasic)
{
    plan_var_t vars[4];
    plan_state_pool_t *pool;
    plan_state_t *state;
    plan_state_id_t ins[4];

    planVarInit(vars + 0);
    planVarInit(vars + 1);
    planVarInit(vars + 2);
    planVarInit(vars + 3);

    vars[0].range = 6;
    vars[1].range = 2;
    vars[2].range = 3;
    vars[3].range = 7;


    pool = planStatePoolNew(vars, 4);
    state = planStateNew(pool);

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


    planStateDel(pool, state);
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

    planVarInit(vars + 0);
    planVarInit(vars + 1);
    planVarInit(vars + 2);
    planVarInit(vars + 3);

    vars[0].range = 3;
    vars[1].range = 2;
    vars[2].range = 2;
    vars[3].range = 4;

    pool = planStatePoolNew(vars, 4);

    state = planStateNew(pool);
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


    parts[0] = planPartStateNew(pool);
    planPartStateSet(pool, parts[0], 0, 1);
    planPartStateSet(pool, parts[0], 2, 1);

    parts[1] = planPartStateNew(pool);
    planPartStateSet(pool, parts[1], 2, 1);
    planPartStateSet(pool, parts[1], 3, 3);

    parts[2] = planPartStateNew(pool);
    planPartStateSet(pool, parts[2], 0, 2);
    planPartStateSet(pool, parts[2], 2, 0);
    planPartStateSet(pool, parts[2], 3, 1);

    parts[3] = planPartStateNew(pool);
    planPartStateSet(pool, parts[3], 3, 3);


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



    planStateDel(pool, state);
    planPartStateDel(pool, parts[0]);
    planPartStateDel(pool, parts[1]);
    planPartStateDel(pool, parts[2]);
    planPartStateDel(pool, parts[3]);
    planStatePoolDel(pool);
}
