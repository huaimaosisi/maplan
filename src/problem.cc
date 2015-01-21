#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <boruvka/alloc.h>

#include "plan/problem.h"
#include "plan/causalgraph.h"
#include "problemdef.pb.h"

/** Forward declaration */
class AgentVarVals;

/** Parses protobuffer from the given file. Returns NULL on failure. */
static PlanProblem *parseProto(const char *fn);
/** Loads problem from protobuffer */
static int loadProblem(plan_problem_t *prob,
                       const PlanProblem *proto,
                       int flags);
/** Load agents part from the protobuffer */
static void loadAgents(plan_problem_agents_t *p,
                       const PlanProblem *proto,
                       int flags);

static void loadProtoProblem(plan_problem_t *prob,
                             const PlanProblem *proto,
                             const plan_var_id_t *var_map,
                             int var_size);
static int hasUnimportantVars(const plan_causal_graph_t *cg);
static void pruneUnimportantVars(plan_problem_t *oldp,
                                 const PlanProblem *proto,
                                 const int *important_var,
                                 plan_var_id_t *var_order);

/** Initializes agent's problem struct from global problem struct */
static void agentInitProblem(plan_problem_t *dst, const plan_problem_t *src);
/** Sets owner of the operators according to the agent names */
static void setOpOwner(plan_op_t *ops, int op_size,
                       const plan_problem_agents_t *agents);
/** Sets up .recv_agent bitarray of operators */
static void setOpRecvAgent(plan_op_t *ops, int op_size,
                           int agent_size, const AgentVarVals &vals);
/** Sets private flag to operators */
static void setOpPrivate(plan_op_t *ops, int op_size,
                         const AgentVarVals &vals);
/** Projects partial state to agent's facts */
static void projectPartState(plan_part_state_t *ps,
                             int agent_id,
                             const AgentVarVals &vals);
/** Project operator to agent's facts */
static bool projectOp(plan_op_t *op, int agent_id, const AgentVarVals &vals);
/** Creates projected operators in problem struct */
static void createProjectedOps(const plan_op_t *ops, int ops_size,
                               int agent_id, plan_problem_t *agent,
                               const AgentVarVals &vals);
/** Creates agent's operators array in dst problem struct */
static void createOps(const plan_op_t *ops, int op_size,
                      int agent_id, plan_problem_t *dst);
/** Sets .private_vals array in problem struct and .is_private member of
 *  var[] structures. */
static void setPrivateVals(plan_problem_t *agent, int agent_id,
                           const AgentVarVals &vals);


plan_problem_t *planProblemFromProto(const char *fn, int flags)
{
    plan_problem_t *p = NULL;
    PlanProblem *proto = NULL;

    proto = parseProto(fn);
    if (proto == NULL)
        return NULL;

    p = BOR_ALLOC(plan_problem_t);
    loadProblem(p, proto, flags);

    delete proto;

    return p;
}

plan_problem_agents_t *planProblemAgentsFromProto(const char *fn, int flags)
{
    plan_problem_agents_t *p = NULL;
    PlanProblem *proto = NULL;

    proto = parseProto(fn);
    if (proto == NULL)
        return NULL;

    p = BOR_ALLOC(plan_problem_agents_t);
    loadProblem(&p->glob, proto, flags);
    loadAgents(p, proto, flags);

    delete proto;

    return p;
}

static PlanProblem *parseProto(const char *fn)
{
    PlanProblem *proto = NULL;
    int fd;

    // Open file with problem definition
    fd = open(fn, O_RDONLY);
    if (fd == -1){
        fprintf(stderr, "Plan Error: Could not open file `%s'\n", fn);
        return NULL;
    }

    // Load protobuf definition from the file
    proto = new PlanProblem;
    proto->ParseFromFileDescriptor(fd);
    close(fd);

    // Check version protobuf version
    if (proto->version() != 3){
        fprintf(stderr, "Plan Error: Unknown version of problem"
                        " definition: %d\n", proto->version());
        delete proto;
        return NULL;
    }

    return proto;
}

static int loadProblem(plan_problem_t *p,
                       const PlanProblem *proto,
                       int flags)
{
    plan_causal_graph_t *cg;
    plan_var_id_t *var_order;
    int size;


    // Load problem from the protobuffer
    loadProtoProblem(p, proto, NULL, -1);

    // Fix problem with causal graph
    if (flags & PLAN_PROBLEM_USE_CG){
        cg = planCausalGraphNew(p->var_size, p->op, p->op_size, p->goal);

        if (hasUnimportantVars(cg)){
            size = sizeof(plan_var_id_t) * (cg->var_order_size + 1);
            var_order = (plan_var_id_t *)alloca(size);
            memcpy(var_order, cg->var_order, size);
            pruneUnimportantVars(p, proto, cg->important_var, var_order);
        }else{
            var_order = cg->var_order;
        }

        p->succ_gen = planSuccGenNew(p->op, p->op_size, var_order);
        planCausalGraphDel(cg);
    }else{
        p->succ_gen = planSuccGenNew(p->op, p->op_size, NULL);
    }

    return 0;
}



static bool sortCmpPrivateVals(const plan_problem_private_val_t &v1,
                               const plan_problem_private_val_t &v2)
{
    if (v1.var < v2.var)
        return true;
    if (v1.var == v2.var && v1.val < v2.val)
        return true;
    return false;
}

struct AgentVarVal {
    std::vector<bool> use; /*!< true for each agent if the value is used
                                in precondition or effect of its operator */
    std::vector<bool> pre; /*!< true for each agent if the value is used in
                                precondition of its operator */
    bool pub;  /*!< True if the value is public (used by more than one
                    agent) */

    AgentVarVal() : pub(false) {}

    void resize(int size)
    {
        use.resize(size, false);
        pre.resize(size, false);
    }
};

class AgentVarVals {
    int agent_size;
    std::vector<std::vector<AgentVarVal> > val;
    std::vector<std::vector<plan_problem_private_val_t> > private_vals;

  public:
    AgentVarVals(const plan_problem_t *prob, int agent_size)
        : agent_size(agent_size)
    {
        val.resize(prob->var_size);
        for (int i = 0; i < prob->var_size; ++i){
            val[i].resize(prob->var[i].range);
            for (int j = 0; j < (int)prob->var[i].range; ++j){
                val[i][j].resize(agent_size);
            }
        }

        private_vals.resize(agent_size);
    }

    /**
     * Set variable's value as used by the agent in precondition.
     */
    void setPreUse(int var, int value, int agent_id)
    {
        val[var][value].pre[agent_id] = true;
        val[var][value].use[agent_id] = true;
    }

    void setPreUse(const plan_part_state_t *ps, int agent_id)
    {
        plan_var_id_t var;
        plan_val_t val;
        int i;

        PLAN_PART_STATE_FOR_EACH(ps, i, var, val){
            setPreUse(var, val, agent_id);
        }
    }

    /**
     * Set variable's value as used by the agent in an effect.
     */
    void setEffUse(int var, int value, int agent_id)
    {
        val[var][value].use[agent_id] = true;
    }

    void setEffUse(const plan_part_state_t *ps, int agent_id)
    {
        plan_var_id_t var;
        plan_val_t val;
        int i;

        PLAN_PART_STATE_FOR_EACH(ps, i, var, val){
            setEffUse(var, val, agent_id);
        }
    }

    /**
     * Sets variables' values as public if they are used by more than one
     * agent and sets them as private otherwise.
     */
    void determinePublicVals()
    {
        plan_problem_private_val_t private_val;

        for (size_t i = 0; i < val.size(); ++i){
            for (size_t j = 0; j < val[i].size(); ++j){
                int sum = 0;
                int owner = -1;
                for (size_t k = 0; k < val[i][j].use.size() && sum < 2; ++k){
                    if (val[i][j].use[k]){
                        ++sum;
                        owner = k;
                    }
                }
                if (sum > 1){
                    val[i][j].pub = true;
                }else if (sum == 1){
                    private_val.var = i;
                    private_val.val = j;
                    private_vals[owner].push_back(private_val);
                }
            }
        }

        for (size_t i = 0; i < private_vals.size(); ++i){
            std::sort(private_vals[i].begin(), private_vals[i].end(),
                      sortCmpPrivateVals);
        }
    }

    void setUse(const plan_op_t *ops, int op_size,
                const plan_part_state_t *goal)
    {
        int i, agent_id;
        const plan_op_t *op;
        uint64_t ownerarr;

        // Set goals as public for all agents
        for (i = 0; i < agent_size; ++i)
            setPreUse(goal, i);

        // Process operators from all agents and all its operators
        for (i = 0; i < op_size; ++i){
            op = ops + i;

            ownerarr = op->ownerarr;
            for (agent_id = 0; agent_id < agent_size; ++agent_id){
                if (ownerarr & 0x1){
                    setPreUse(op->pre, agent_id);
                    setEffUse(op->eff, agent_id);
                }

                ownerarr >>= 1;
            }
        }

        determinePublicVals();
    }

    /**
     * Returns true if the value is used by the specified agent.
     */
    bool used(int var, int value, int agent_id) const
    {
        return val[var][value].use[agent_id];
    }

    /**
     * Returns true if the value is used as precondition by the specified
     * agent.
     */
    bool usedAsPre(int var, int value, int agent_id) const
    {
        return val[var][value].pre[agent_id];
    }

    /**
     * Returns true if the value is public.
     */
    bool isPublic(int var, int value) const
    {
        return val[var][value].pub;
    }

    bool isPublic(const plan_part_state_t *ps) const
    {
        plan_var_id_t var;
        plan_val_t val;
        int i;

        PLAN_PART_STATE_FOR_EACH(ps, i, var, val){
            if (isPublic(var, val))
                return true;
        }

        return false;
    }

    const std::vector<plan_problem_private_val_t> &privateVals(int agent_id) const
    {
        return private_vals[agent_id];
    }
};


static void loadAgents(plan_problem_agents_t *p,
                       const PlanProblem *proto,
                       int flags)
{
    int i;
    plan_problem_t *agent;

    p->agent_size = proto->agent_name_size();
    if (p->agent_size == 0){
        fprintf(stderr, "Problem Error: No agents defined!\n");
        p->agent = NULL;
        return;

    }else if (p->agent_size > 64){
        fprintf(stderr, "Problem Error: More than 64 agents defined!\n");
        p->agent = NULL;
        return;
    }

    AgentVarVals var_vals(&p->glob, p->agent_size);

    p->agent = BOR_ALLOC_ARR(plan_problem_t, p->agent_size);
    for (i = 0; i < p->agent_size; ++i){
        agent = p->agent + i;
        agentInitProblem(agent, &p->glob);
        agent->agent_name = strdup(proto->agent_name(i).c_str());
    }

    // Set owners of the operators
    setOpOwner(p->glob.op, p->glob.op_size, p);

    // Determine which variable values are used by which agents' operators
    var_vals.setUse(p->glob.op, p->glob.op_size, p->glob.goal);

    // Set up receiving agents of the operators
    setOpRecvAgent(p->glob.op, p->glob.op_size, p->agent_size, var_vals);

    // Mark private operators
    setOpPrivate(p->glob.op, p->glob.op_size, var_vals);


    for (i = 0; i < p->agent_size; ++i){
        // Create operators that belong to the specified agent
        createOps(p->glob.op, p->glob.op_size, i, p->agent + i);

        // Create projected operators
        createProjectedOps(p->glob.op, p->glob.op_size,
                           i, p->agent + i, var_vals);

        // Create successor generator from operators that are owned by the
        // agent
        p->agent[i].succ_gen = planSuccGenNew(p->agent[i].op,
                                              p->agent[i].op_size, NULL);

        setPrivateVals(p->agent + i, i, var_vals);
    }
}

static void loadVar(plan_problem_t *p, const PlanProblem *proto,
                    const plan_var_id_t *var_map, int var_size)
{
    int len;

    // Determine number of variables
    len = proto->var_size();

    // Allocate space for variables
    p->var_size = len;
    if (var_size != -1)
        p->var_size = var_size;
    p->var = BOR_ALLOC_ARR(plan_var_t, p->var_size);

    // Load all variables
    for (int i = 0; i < len; ++i){
        if (var_map && var_map[i] == PLAN_VAR_ID_UNDEFINED)
            continue;

        const PlanProblemVar &proto_var = proto->var(i);
        plan_var_t *var;
        if (var_map){
            var = p->var + var_map[i];
        }else{
            var = p->var + i;
        }

        planVarInit(var, proto_var.name().c_str(), proto_var.range());
    }
}

static void loadInitState(plan_problem_t *p, const PlanProblem *proto,
                          const plan_var_id_t *var_map)
{
    plan_state_t *state;
    const PlanProblemState &proto_state = proto->init_state();

    state = planStateNew(p->state_pool);
    for (int i = 0; i < proto_state.val_size(); ++i){
        if (var_map[i] == PLAN_VAR_ID_UNDEFINED)
            continue;
        planStateSet(state, var_map[i], proto_state.val(i));
    }
    p->initial_state = planStatePoolInsert(p->state_pool, state);
    planStateDel(state);
}

static void loadGoal(plan_problem_t *p, const PlanProblem *proto,
                     const plan_var_id_t *var_map)
{
    const PlanProblemPartState &proto_goal = proto->goal();

    p->goal = planPartStateNew(p->state_pool);
    for (int i = 0; i < proto_goal.val_size(); ++i){
        const PlanProblemVarVal &v = proto_goal.val(i);
        if (var_map[v.var()] == PLAN_VAR_ID_UNDEFINED)
            continue;

        planPartStateSet(p->goal, var_map[v.var()], v.val());
    }
}

static void loadOperator(plan_problem_t *p, const PlanProblem *proto,
                         const plan_var_id_t *var_map)
{
    int i, len, ins;

    len = proto->operator__size();

    // Allocate array for operators
    p->op_size = len;
    p->op = BOR_ALLOC_ARR(plan_op_t, p->op_size);

    for (i = 0, ins = 0; i < len; ++i){
        int num_effects = 0;
        const PlanProblemOperator &proto_op = proto->operator_(i);
        plan_op_t *op = p->op + ins;

        planOpInit(op, p->state_pool);
        op->name = strdup(proto_op.name().c_str());
        op->cost = proto_op.cost();
        op->global_id = ins;

        const PlanProblemPartState &proto_pre = proto_op.pre();
        for (int j = 0; j < proto_pre.val_size(); ++j){
            const PlanProblemVarVal &v = proto_pre.val(j);
            if (var_map[v.var()] == PLAN_VAR_ID_UNDEFINED)
                continue;

            planOpSetPre(op, var_map[v.var()], v.val());
        }

        const PlanProblemPartState &proto_eff = proto_op.eff();
        for (int j = 0; j < proto_eff.val_size(); ++j){
            const PlanProblemVarVal &v = proto_eff.val(j);
            if (var_map[v.var()] == PLAN_VAR_ID_UNDEFINED)
                continue;

            planOpSetEff(op, var_map[v.var()], v.val());
            ++num_effects;
        }

        for (int j = 0; j < proto_op.cond_eff_size(); ++j){
            int num_cond_eff = 0;
            const PlanProblemCondEff &proto_cond_eff = proto_op.cond_eff(j);
            int cond_eff_id = planOpAddCondEff(op);

            const PlanProblemPartState &proto_pre = proto_cond_eff.pre();
            for (int k = 0; k < proto_pre.val_size(); ++k){
                const PlanProblemVarVal &v = proto_pre.val(k);
                if (var_map[v.var()] == PLAN_VAR_ID_UNDEFINED)
                    continue;

                planOpCondEffSetPre(op, cond_eff_id, var_map[v.var()], v.val());
            }

            const PlanProblemPartState &proto_eff = proto_cond_eff.eff();
            for (int k = 0; k < proto_eff.val_size(); ++k){
                const PlanProblemVarVal &v = proto_eff.val(k);
                if (var_map[v.var()] == PLAN_VAR_ID_UNDEFINED)
                    continue;

                planOpCondEffSetEff(op, cond_eff_id, var_map[v.var()], v.val());
                ++num_effects;
                ++num_cond_eff;
            }

            if (num_cond_eff == 0){
                planOpDelLastCondEff(op);
            }
        }

        if (num_effects > 0){
            planOpCondEffSimplify(op);
            ++ins;
        }else{
            planOpFree(op);
        }
    }

    if (ins != p->op_size){
        // Reset number of operators
        p->op_size = ins;
        // and give back some memory
        p->op = BOR_REALLOC_ARR(p->op, plan_op_t, p->op_size);
    }
}

static void loadProtoProblem(plan_problem_t *p,
                             const PlanProblem *proto,
                             const plan_var_id_t *var_map,
                             int var_size)
{
    bzero(p, sizeof(*p));

    loadVar(p, proto, var_map, var_size);
    p->state_pool = planStatePoolNew(p->var, p->var_size);

    if (var_map == NULL){
        plan_var_id_t *_var_map;
        _var_map = (plan_var_id_t *)alloca(sizeof(plan_var_id_t) * p->var_size);
        for (int i = 0; i < p->var_size; ++i)
            _var_map[i] = i;
        var_map = _var_map;
    }

    loadInitState(p, proto, var_map);
    loadGoal(p, proto, var_map);
    loadOperator(p, proto, var_map);
}

static int hasUnimportantVars(const plan_causal_graph_t *cg)
{
    int i;
    for (i = 0; i < cg->var_size && cg->important_var[i]; ++i);
    return i != cg->var_size;
}

static void pruneUnimportantVars(plan_problem_t *p,
                                 const PlanProblem *proto,
                                 const int *important_var,
                                 plan_var_id_t *var_order)
{
    int i, id, var_size;
    plan_var_id_t *var_map;

    // Create mapping from old var ID to the new ID
    var_map = (plan_var_id_t *)alloca(sizeof(plan_var_id_t) * p->var_size);
    var_size = 0;
    for (i = 0, id = 0; i < p->var_size; ++i){
        if (important_var[i]){
            var_map[i] = id++;
            ++var_size;
        }else{
            var_map[i] = PLAN_VAR_ID_UNDEFINED;
        }
    }

    // fix var-order array
    for (; *var_order != PLAN_VAR_ID_UNDEFINED; ++var_order){
        *var_order = var_map[*var_order];
    }

    planProblemFree(p);
    loadProtoProblem(p, proto, var_map, var_size);
}

static void agentInitProblem(plan_problem_t *dst, const plan_problem_t *src)
{
    int i;
    plan_state_t *state;

    planProblemInit(dst);

    dst->var_size = src->var_size;
    dst->var = BOR_ALLOC_ARR(plan_var_t, src->var_size);
    for (i = 0; i < src->var_size; ++i){
        planVarCopy(dst->var + i, src->var + i);
    }

    if (!src->state_pool)
        return;

    dst->state_pool = planStatePoolNew(dst->var, dst->var_size);

    state = planStateNew(src->state_pool);
    planStatePoolGetState(src->state_pool, src->initial_state, state);
    dst->initial_state = planStatePoolInsert(dst->state_pool, state);
    planStateDel(state);

    dst->goal = planPartStateNew(dst->state_pool);
    planPartStateCopy(dst->goal, src->goal);

    dst->op_size = 0;
    dst->op = NULL;
    dst->succ_gen = NULL;
    dst->agent_name = NULL;
    dst->proj_op = NULL;
    dst->proj_op_size = 0;
    dst->private_val = NULL;
    dst->private_val_size = 0;
}

static void setOpOwner(plan_op_t *ops, int op_size,
                       const plan_problem_agents_t *agents)
{
    int i, j;
    plan_op_t *op;
    std::vector<std::string> name_token;

    // Create name token for each agent
    name_token.resize(agents->agent_size);
    for (i = 0; i < agents->agent_size; ++i){
        name_token[i] = " ";
        name_token[i] += agents->agent[i].agent_name;
    }

    for (i = 0; i < op_size; ++i){
        op = ops + i;

        for (j = 0; j < agents->agent_size; ++j){
            if (strstr(op->name, name_token[j].c_str()) != NULL){
                planOpAddOwner(op, j);
            }
        }

        if (op->ownerarr == 0){
            // if the operator wasn't inserted anywhere, insert it to all
            // agents
            for (j = 0; j < agents->agent_size; ++j){
                planOpAddOwner(op, j);
            }
        }
    }
}

static void setOpRecvAgent(plan_op_t *ops, int op_size,
                           int agent_size, const AgentVarVals &vals)
{
    plan_op_t *op;
    plan_var_id_t var;
    plan_val_t val;
    int i;

    for (int opi = 0; opi < op_size; ++opi){
        op = ops + opi;

        for (int peer = 0; peer < agent_size; ++peer){
            PLAN_PART_STATE_FOR_EACH(op->eff, i, var, val){
                if (vals.isPublic(var, val) && vals.usedAsPre(var, val, peer)){
                    planOpAddRecvAgent(op, peer);
                    break;
                }
            }
        }
    }
}

static void setOpPrivate(plan_op_t *ops, int op_size,
                         const AgentVarVals &vals)
{
    plan_op_t *op;

    for (int opi = 0; opi < op_size; ++opi){
        op = ops + opi;
        if (!vals.isPublic(op->eff) && !vals.isPublic(op->pre)){
            op->is_private = 1;
        }
    }
}

static void projectPartState(plan_part_state_t *ps,
                             int agent_id,
                             const AgentVarVals &vals)
{
    plan_var_id_t var;
    plan_val_t val;
    int i;
    std::vector<int> unset;

    PLAN_PART_STATE_FOR_EACH(ps, i, var, val){
        if (!vals.isPublic(var, val) && !vals.used(var, val, agent_id))
            unset.push_back(var);
    }

    for (size_t i = 0; i < unset.size(); ++i)
        planPartStateUnset(ps, unset[i]);
}

static bool projectOp(plan_op_t *op, int agent_id, const AgentVarVals &vals)
{
    projectPartState(op->pre, agent_id, vals);
    projectPartState(op->eff, agent_id, vals);

    if (op->eff->vals_size > 0 || op->pre->vals_size > 0)
        return true;
    return false;
}

static void createProjectedOps(const plan_op_t *ops, int ops_size,
                               int agent_id, plan_problem_t *agent,
                               const AgentVarVals &vals)
{
    plan_op_t *proj_op;

    // Allocate enough memory
    agent->proj_op = BOR_ALLOC_ARR(plan_op_t, ops_size);
    agent->proj_op_size = 0;

    for (int opi = 0; opi < ops_size; ++opi){
        if (ops[opi].ownerarr == 0)
            continue;

        proj_op = agent->proj_op + agent->proj_op_size;

        planOpInit(proj_op, agent->state_pool);
        planOpCopy(proj_op, ops + opi);

        if (projectOp(proj_op, agent_id, vals)){
            if (planOpIsOwner(proj_op, agent_id)){
                proj_op->owner = agent_id;
            }else{
                planOpSetFirstOwner(proj_op);
            }
            ++agent->proj_op_size;
        }else{
            planOpFree(proj_op);
        }
    }

    // Given back unused memory
    agent->proj_op = BOR_REALLOC_ARR(agent->proj_op, plan_op_t,
                                     agent->proj_op_size);
}

static void createOps(const plan_op_t *ops, int op_size,
                      int agent_id, plan_problem_t *dst)
{
    int i;
    const plan_op_t *op;

    dst->op = BOR_ALLOC_ARR(plan_op_t, op_size);
    dst->op_size = 0;

    for (i = 0; i < op_size; ++i){
        op = ops + i;
        if (planOpIsOwner(op, agent_id)){
            planOpInit(dst->op + dst->op_size, dst->state_pool);
            planOpCopy(dst->op + dst->op_size, op);
            dst->op[dst->op_size].owner = agent_id;
            planOpDelRecvAgent(dst->op + dst->op_size, agent_id);
            ++dst->op_size;
        }
    }

    dst->op = BOR_REALLOC_ARR(dst->op, plan_op_t, dst->op_size);
}

static void setPrivateVals(plan_problem_t *agent, int agent_id,
                           const AgentVarVals &vals)
{
    const std::vector<plan_problem_private_val_t> &pv
                = vals.privateVals(agent_id);

    agent->private_val_size = pv.size();
    agent->private_val = BOR_ALLOC_ARR(plan_problem_private_val_t,
                                       agent->private_val_size);
    for (size_t i = 0; i < pv.size(); ++i){
        agent->private_val[i] = pv[i];
        agent->var[pv[i].var].is_private[pv[i].val] = 1;
    }
}