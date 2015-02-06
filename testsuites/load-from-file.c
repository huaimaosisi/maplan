#include <cu/cu.h>
#include "plan/problem.h"

static void pVar(const plan_var_t *var, int var_size)
{
    int i, j;

    printf("Vars[%d]:\n", var_size);
    for (i = 0; i < var_size; ++i){
        printf("[%d] name: `%s', range: %d, is_private:", i, var[i].name, var[i].range);
        for (j = 0; j < var[i].range; ++j)
            printf(" %d", var[i].is_private[j]);
        printf("\n");
    }
}

static void pInitState(const plan_state_pool_t *state_pool, plan_state_id_t sid)
{
    plan_state_t *state;
    int i, size;

    state = planStateNew(state_pool->num_vars);
    planStatePoolGetState(state_pool, sid, state);
    size = planStateSize(state);
    printf("Initial state:");
    for (i = 0; i < size; ++i){
        printf(" %d:%d", i, (int)planStateGet(state, i));
    }
    printf("\n");
    planStateDel(state);
}

static void pPartState(const plan_part_state_t *p)
{
    plan_var_id_t var;
    plan_val_t val;
    int i;

    PLAN_PART_STATE_FOR_EACH(p, i, var, val){
        printf(" %d:%d", (int)var, (int)val);
    }
}

static void pGoal(const plan_part_state_t *p)
{
    printf("Goal:");
    pPartState(p);
    printf("\n");
}

static void pOp(const plan_op_t *op, int op_size)
{
    int i, j;

    printf("Ops[%d]:\n", op_size);
    for (i = 0; i < op_size; ++i){
        printf("[%d] cost: %d, gid: %d, owner: %d (%lx),"
               " private: %d, recv_agent: %lx, name: `%s'\n",
               i, (int)op[i].cost, op[i].global_id,
               op[i].owner, (unsigned long)op[i].ownerarr,
               op[i].is_private,
               (unsigned long)op[i].recv_agent,
               op[i].name);
        printf("[%d] pre:", i);
        pPartState(op[i].pre);
        printf(", eff:");
        pPartState(op[i].eff);
        printf("\n");

        for (j = 0; j < op[i].cond_eff_size; ++j){
            printf("[%d] cond_eff[%d]:", i, j);
            printf(" pre:");
            pPartState(op[i].cond_eff[j].pre);
            printf(", eff:");
            pPartState(op[i].cond_eff[j].eff);
            printf("\n");
        }
    }
}

static void pPrivateVal(const plan_problem_private_val_t *pv, int pvsize)
{
    int i;

    printf("PrivateVal:");
    for (i = 0; i < pvsize; ++i){
        printf(" %d:%d", pv[i].var, pv[i].val);
    }
    printf("\n");
}

TEST(testLoadFromProto)
{
    plan_problem_t *p;
    int flags;

    flags = PLAN_PROBLEM_USE_CG | PLAN_PROBLEM_PRUNE_DUPLICATES;
    p = planProblemFromProto("../data/ma-benchmarks/rovers/p03.proto", flags);
    printf("---- testLoadFromProto ----\n");
    pVar(p->var, p->var_size);
    pInitState(p->state_pool, p->initial_state);
    pGoal(p->goal);
    pOp(p->op, p->op_size);
    printf("Succ Gen: %d\n", (int)(p->succ_gen != NULL));
    printf("---- testLoadFromProto END ----\n");
    planProblemDel(p);
}

TEST(testLoadFromProtoCondEff)
{
    plan_problem_t *p;

    p = planProblemFromProto("../data/ipc2014/satisficing/CityCar/p3-2-2-0-1.proto",
                             PLAN_PROBLEM_USE_CG);
    printf("---- testLoadFromProtoCondEff ----\n");
    pVar(p->var, p->var_size);
    pInitState(p->state_pool, p->initial_state);
    pGoal(p->goal);
    pOp(p->op, p->op_size);
    printf("Succ Gen: %d\n", (int)(p->succ_gen != NULL));
    printf("---- testLoadFromProtoCondEff END ----\n");
    planProblemDel(p);
}

static void pAgent(int agent_id, const plan_problem_t *p)
{
    plan_op_t *private_op;
    int private_op_size;
    planProblemCreatePrivateProjOps(p->op, p->op_size, p->var, p->var_size,
                                    &private_op, &private_op_size);

    printf("++++ %s ++++\n", p->agent_name);
    printf("Agent ID: %d\n", agent_id);
    pVar(p->var, p->var_size);
    pPrivateVal(p->private_val, p->private_val_size);
    pInitState(p->state_pool, p->initial_state);
    pGoal(p->goal);
    pOp(p->op, p->op_size);
    printf("Succ Gen: %d\n", (int)(p->succ_gen != NULL));
    printf("Proj op:\n");
    pOp(p->proj_op, p->proj_op_size);
    printf("Private Proj op:\n");
    pOp(private_op, private_op_size);
    printf("++++ %s END ++++\n", p->agent_name);

    planProblemDestroyOps(private_op, private_op_size);
}

static void testAgentProto(const char *proto)
{
    plan_problem_agents_t *agents;
    int i;
    int flags;

    flags = PLAN_PROBLEM_USE_CG | PLAN_PROBLEM_PRUNE_DUPLICATES;
    agents = planProblemAgentsFromProto(proto, flags);
    assertNotEquals(agents, NULL);
    if (agents == NULL)
        return;

    printf("---- %s ----\n", proto);
    pVar(agents->glob.var, agents->glob.var_size);
    pInitState(agents->glob.state_pool, agents->glob.initial_state);
    pGoal(agents->glob.goal);
    pOp(agents->glob.op, agents->glob.op_size);
    printf("Succ Gen: %d\n", (int)(agents->glob.succ_gen != NULL));

    for (i = 0; i < agents->agent_size; ++i)
        pAgent(i, agents->agent + i);

    printf("---- %s END ----\n", proto);
    planProblemAgentsDel(agents);
}

TEST(testLoadAgentFromProto)
{
    testAgentProto("../data/ma-benchmarks/rovers/p03.proto");
    testAgentProto("../data/ma-benchmarks/depot/pfile1.proto");
}
