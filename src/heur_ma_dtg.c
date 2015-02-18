#include <boruvka/alloc.h>
#include "plan/heur.h"
#include "heur_dtg.h"

struct _plan_heur_ma_dtg_t {
    plan_heur_t heur;
    plan_heur_dtg_data_t data;
    plan_heur_dtg_ctx_t ctx;

    plan_op_t *fake_op;
    int fake_op_size;
};
typedef struct _plan_heur_ma_dtg_t plan_heur_ma_dtg_t;
#define HEUR(parent) \
    bor_container_of((parent), plan_heur_ma_dtg_t, heur)

static void hdtgDel(plan_heur_t *heur);
static int hdtgHeur(plan_heur_t *heur, plan_ma_comm_t *comm,
                    const plan_state_t *state, plan_heur_res_t *res);
static int hdtgUpdate(plan_heur_t *heur, plan_ma_comm_t *comm,
                      const plan_ma_msg_t *msg, plan_heur_res_t *res);
static void hdtgRequest(plan_heur_t *heur, plan_ma_comm_t *comm,
                        const plan_ma_msg_t *msg);

static void initFakeOp(plan_heur_ma_dtg_t *hdtg,
                       const plan_problem_t *prob);
static void initDTGData(plan_heur_ma_dtg_t *hdtg,
                        const plan_problem_t *prob);

plan_heur_t *planHeurMADTGNew(const plan_problem_t *agent_def)
{
    plan_heur_ma_dtg_t *hdtg;

    hdtg = BOR_ALLOC(plan_heur_ma_dtg_t);
    _planHeurInit(&hdtg->heur, hdtgDel, NULL);
    _planHeurMAInit(&hdtg->heur, hdtgHeur, hdtgUpdate, hdtgRequest);
    initFakeOp(hdtg, agent_def);
    initDTGData(hdtg, agent_def);

    return &hdtg->heur;
}

static void hdtgDel(plan_heur_t *heur)
{
    plan_heur_ma_dtg_t *hdtg = HEUR(heur);
    int i;

    for (i = 0; i < hdtg->fake_op_size; ++i)
        planOpFree(hdtg->fake_op + i);
    if (hdtg->fake_op)
        BOR_FREE(hdtg->fake_op);
    planHeurDTGDataFree(&hdtg->data);

    _planHeurFree(&hdtg->heur);
    BOR_FREE(hdtg);
}

static int hdtgHeur(plan_heur_t *heur, plan_ma_comm_t *comm,
                    const plan_state_t *state, plan_heur_res_t *res)
{
    // TODO
    return 0;
}

static int hdtgUpdate(plan_heur_t *heur, plan_ma_comm_t *comm,
                      const plan_ma_msg_t *msg, plan_heur_res_t *res)
{
    // TODO
    return 0;
}

static void hdtgRequest(plan_heur_t *heur, plan_ma_comm_t *comm,
                        const plan_ma_msg_t *msg)
{
    // TODO
}

static void dtgAddOp(plan_heur_ma_dtg_t *hdtg, const plan_op_t *op)
{
    const plan_op_t *trans_op = hdtg->fake_op + op->owner;
    plan_var_id_t var;
    plan_val_t val, val2;
    int _;

    PLAN_PART_STATE_FOR_EACH(op->pre, _, var, val){
        val2 = hdtg->data.dtg.dtg[var].val_size - 1;
        if (!planDTGHasTrans(&hdtg->data.dtg, var, val, val2, trans_op)){
            planDTGAddTrans(&hdtg->data.dtg, var, val, val2, trans_op);
        }
    }

    PLAN_PART_STATE_FOR_EACH(op->eff, _, var, val){
        val2 = hdtg->data.dtg.dtg[var].val_size - 1;
        if (!planDTGHasTrans(&hdtg->data.dtg, var, val2, val, trans_op)){
            planDTGAddTrans(&hdtg->data.dtg, var, val2, val, trans_op);
        }
    }
}

static void initFakeOp(plan_heur_ma_dtg_t *hdtg,
                       const plan_problem_t *prob)
{
    int max_agent_id = 0;
    int i;
    char name[128];

    for (i = 0; i < prob->proj_op_size; ++i)
        max_agent_id = BOR_MAX(max_agent_id, prob->proj_op[i].owner);

    hdtg->fake_op_size = max_agent_id + 1;
    hdtg->fake_op = BOR_ALLOC_ARR(plan_op_t, hdtg->fake_op_size);
    for (i = 0; i < hdtg->fake_op_size; ++i){
        planOpInit(hdtg->fake_op + i, 1);
        hdtg->fake_op[i].owner = i;
        sprintf(name, "agent-%d", i);
        hdtg->fake_op[i].name = strdup(name);
    }
}

static void initDTGData(plan_heur_ma_dtg_t *hdtg,
                        const plan_problem_t *prob)
{
    int i;
    const plan_op_t *op;

    planHeurDTGDataInitUnknown(&hdtg->data, prob->var, prob->var_size,
                               prob->op, prob->op_size);
    for (i = 0; i < prob->proj_op_size; ++i){
        op = prob->proj_op + i;
        if (op->owner == prob->agent_id)
            continue;
        dtgAddOp(hdtg, op);
    }
}
