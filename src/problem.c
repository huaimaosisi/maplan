#include <string.h>

#include <boruvka/alloc.h>

#include "plan/problem.h"


void planProblemInit(plan_problem_t *p)
{
    bzero(p, sizeof(*p));
}

void planProblemDel(plan_problem_t *plan)
{
    planProblemFree(plan);
    BOR_FREE(plan);
}

static void freeOps(plan_op_t *ops, int ops_size)
{
    int i;

    for (i = 0; i < ops_size; ++i){
        planOpFree(ops + i);
    }
    BOR_FREE(ops);
}

void planProblemFree(plan_problem_t *plan)
{
    int i;

    if (plan->succ_gen)
        planSuccGenDel(plan->succ_gen);

    if (plan->goal){
        planPartStateDel(plan->goal);
    }

    if (plan->var != NULL){
        for (i = 0; i < plan->var_size; ++i){
            planVarFree(plan->var + i);
        }
        BOR_FREE(plan->var);
    }

    if (plan->state_pool)
        planStatePoolDel(plan->state_pool);

    if (plan->op)
        freeOps(plan->op, plan->op_size);

    if (plan->agent_name)
        BOR_FREE(plan->agent_name);

    if (plan->proj_op)
        freeOps(plan->proj_op, plan->proj_op_size);

    bzero(plan, sizeof(*plan));
}


void planProblemAgentsDel(plan_problem_agents_t *agents)
{
    int i;

    planProblemFree(&agents->glob);

    for (i = 0; i < agents->agent_size; ++i)
        planProblemFree(agents->agent + i);

    if (agents->agent)
        BOR_FREE(agents->agent);
    BOR_FREE(agents);
}
