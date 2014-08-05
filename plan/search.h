#ifndef __PLAN_SEARCH_H__
#define __PLAN_SEARCH_H__

#include <boruvka/timer.h>

#include <plan/problem.h>
#include <plan/statespace.h>
#include <plan/heur.h>
#include <plan/list_lazy.h>
#include <plan/path.h>
#include <plan/ma_comm_queue.h>

/** Forward declaration */
typedef struct _plan_search_t plan_search_t;

/**
 * Search Algorithms
 * ==================
 */

/**
 * Status that search can (and should) continue. Mostly for internal use.
 */
#define PLAN_SEARCH_CONT       0

/**
 * Status signaling that a solution was found.
 */
#define PLAN_SEARCH_FOUND      1

/**
 * No solution was found.
 */
#define PLAN_SEARCH_NOT_FOUND -1

/**
 * The search process was aborted from outside, e.g., from progress
 * callback.
 */
#define PLAN_SEARCH_ABORT     -2


/**
 * Preferred operators are not used.
 */
#define PLAN_SEARCH_PREFERRED_NONE 0

/**
 * Preferred operators are preferenced over the other ones. This depends on
 * a particular search algorithm.
 */
#define PLAN_SEARCH_PREFERRED_PREF 1

/**
 * Only the preferred operators are used.
 */
#define PLAN_SEARCH_PREFERRED_ONLY 2


/**
 * Struct for statistics from search.
 */
struct _plan_search_stat_t {
    float elapsed_time;
    long steps;
    long evaluated_states;
    long expanded_states;
    long generated_states;
    long peak_memory;
    int found;
    int not_found;
};
typedef struct _plan_search_stat_t plan_search_stat_t;

/**
 * Initializes stat struct.
 */
void planSearchStatInit(plan_search_stat_t *stat);


/**
 * Structure with record of changes made during one step of search
 * algorithm (see planSearchStep()).
 */
struct _plan_search_step_change_t {
    plan_state_space_node_t **closed_node; /*!< Closed nodes */
    int closed_node_size;                  /*!< Number of closed nodes */
    int closed_node_alloc;                 /*!< Allocated space */
};
typedef struct _plan_search_step_change_t plan_search_step_change_t;

/**
 * Initializes plan_search_step_change_t structure.
 */
void planSearchStepChangeInit(plan_search_step_change_t *step_change);

/**
 * Frees all resources allocated within plan_search_step_change_t
 * structure.
 */
void planSearchStepChangeFree(plan_search_step_change_t *step_change);

/**
 * Reset record of the structure.
 */
void planSearchStepChangeReset(plan_search_step_change_t *step_change);

/**
 * Add record about one closed node.
 */
void planSearchStepChangeAddClosedNode(plan_search_step_change_t *step_change,
                                       plan_state_space_node_t *node);



/**
 * Callback for progress monitoring.
 * The function should return PLAN_SEARCH_CONT if the process should
 * continue after this callback, or PLAN_SEARCH_ABORT if the process
 * should be stopped.
 */
typedef int (*plan_search_progress_fn)(const plan_search_stat_t *stat,
                                       void *userdata);


/**
 * Callback called each time a node is closed.
 */
typedef void (*plan_search_run_node_closed)(plan_state_space_node_t *node,
                                            void *data);

/**
 * Common parameters for all search algorithms.
 */
struct _plan_search_params_t {
    plan_heur_t *heur; /*!< Heuristic function that ought to be used */
    int heur_del;      /*!< True if .heur should be deleted in
                            planSearchDel() */

    plan_search_progress_fn progress_fn; /*!< Callback for monitoring */
    long progress_freq;                  /*!< Frequence of calling
                                              .progress_fn as number of steps. */
    void *progress_data;

    plan_problem_t *prob; /*!< Problem definition */
};
typedef struct _plan_search_params_t plan_search_params_t;


struct _plan_search_ma_params_t {
    plan_ma_comm_queue_t *comm;
};
typedef struct _plan_search_ma_params_t plan_search_ma_params_t;

/**
 * Enforced Hill Climbing Search Algorithm
 * ----------------------------------------
 */
struct _plan_search_ehc_params_t {
    plan_search_params_t search; /*!< Common parameters */
    int use_preferred_ops; /*!< One of PLAN_SEARCH_PREFERRED_* constants */
};
typedef struct _plan_search_ehc_params_t plan_search_ehc_params_t;

/**
 * Initializes parameters of EHC algorithm.
 */
void planSearchEHCParamsInit(plan_search_ehc_params_t *p);

/**
 * Creates a new instance of the Enforced Hill Climbing search algorithm.
 */
plan_search_t *planSearchEHCNew(const plan_search_ehc_params_t *params);


/**
 * Lazy Best First Search Algorithm
 * ---------------------------------
 */
struct _plan_search_lazy_params_t {
    plan_search_params_t search; /*!< Common parameters */

    int use_preferred_ops;  /*!< One of PLAN_SEARCH_PREFERRED_* constants */
    plan_list_lazy_t *list; /*!< Lazy list that will be used. */
    int list_del;           /*!< True if .list should be deleted in
                                 planSearchDel() */
};
typedef struct _plan_search_lazy_params_t plan_search_lazy_params_t;

/**
 * Initializes parameters of Lazy algorithm.
 */
void planSearchLazyParamsInit(plan_search_lazy_params_t *p);

/**
 * Creates a new instance of the Lazy Best First Search algorithm.
 */
plan_search_t *planSearchLazyNew(const plan_search_lazy_params_t *params);


/**
 * A* Search Algorithm
 * --------------------
 */
struct _plan_search_astar_params_t {
    plan_search_params_t search; /*!< Common parameters */

    int pathmax; /*!< Use pathmax correction */
};
typedef struct _plan_search_astar_params_t plan_search_astar_params_t;

/**
 * Initializes parameters of A* algorithm.
 */
void planSearchAStarParamsInit(plan_search_astar_params_t *p);

/**
 * Creates a new instance of the A* search algorithm.
 */
plan_search_t *planSearchAStarNew(const plan_search_astar_params_t *params);



/**
 * Common Functions
 * -----------------
 */

/**
 * Deletes search object.
 */
void planSearchDel(plan_search_t *search);

/**
 * Searches for the path from the initial state to the goal as defined via
 * parameters.
 * Returns PLAN_SEARCH_FOUND if the solution was found and in this case the
 * path is returned via path argument.
 * If the plan was not found, PLAN_SEARCH_NOT_FOUND is returned.
 * If the search progress was aborted by the "progess" callback,
 * PLAN_SEARCH_ABORT is returned.
 */
int planSearchRun(plan_search_t *search, plan_path_t *path);

/**
 * Runs search in multi-agent mode.
 */
int planSearchMARun(plan_search_t *search,
                    plan_search_ma_params_t *ma_params,
                    plan_path_t *path);

/**
 * Internals
 * ----------
 */

/**
 * Initializes common parameters.
 * This should be called from all *ParamsInit() functions of particular
 * algorithms.
 */
void planSearchParamsInit(plan_search_params_t *params);

/**
 * Updates .peak_memory value of stat structure.
 */
void planSearchStatUpdatePeakMemory(plan_search_stat_t *stat);

/**
 * Increments number of evaluated states by one.
 */
_bor_inline void planSearchStatIncEvaluatedStates(plan_search_stat_t *stat);

/**
 * Increments number of expanded states by one.
 */
_bor_inline void planSearchStatIncExpandedStates(plan_search_stat_t *stat);

/**
 * Increments number of generated states by one.
 */
_bor_inline void planSearchStatIncGeneratedStates(plan_search_stat_t *stat);

/**
 * Set "found" flag which means that solution was found.
 */
_bor_inline void planSearchStatSetFoundSolution(plan_search_stat_t *stat);

/**
 * Sets "not_found" flag meaning no solution was found.
 */
_bor_inline void planSearchStatSetNotFound(plan_search_stat_t *stat);


/**
 * Algorithm's method that frees all resources.
 */
typedef void (*plan_search_del_fn)(plan_search_t *);

/**
 * Initialize algorithm -- first step of algorithm.
 */
typedef int (*plan_search_init_fn)(plan_search_t *);

/**
 * Perform one step of algorithm.
 */
typedef int (*plan_search_step_fn)(plan_search_t *,
                                   plan_search_step_change_t *change);

/**
 * Inject the given state into open-list and performs another needed
 * operations with the state.
 * Returns 0 on success.
 */
typedef int (*plan_search_inject_state_fn)(plan_search_t *search,
                                           plan_state_id_t state_id,
                                           plan_cost_t cost,
                                           plan_cost_t heuristic);

struct _plan_search_applicable_ops_t {
    plan_operator_t **op;  /*!< Array of applicable operators. This array
                                must be big enough to hold all operators. */
    int op_size;           /*!< Size of .op[] */
    int op_found;          /*!< Number of found applicable operators */
    int op_preferred;      /*!< Number of preferred operators (that are
                                stored at the beggining of .op[] array */
    plan_state_id_t state; /*!< State in which these operators are
                                applicable */
};
typedef struct _plan_search_applicable_ops_t plan_search_applicable_ops_t;

/**
 * Common base struct for all search algorithms.
 */
struct _plan_search_t {
    plan_heur_t *heur;      /*!< Heuristic function */
    int heur_del;           /*!< True if .heur should be deleted */

    plan_search_del_fn del_fn;
    plan_search_init_fn init_fn;
    plan_search_step_fn step_fn;
    plan_search_inject_state_fn inject_state_fn;

    plan_search_params_t params;
    plan_search_stat_t stat;

    plan_state_pool_t *state_pool;   /*!< State pool from params.prob */
    plan_state_space_t *state_space;
    plan_state_t *state;             /*!< Preallocated state */
    plan_state_id_t goal_state;      /*!< The found state satisfying the goal */

    plan_search_applicable_ops_t applicable_ops;

    int ma;                        /*!< True if running in multi-agent mode */
    plan_ma_comm_queue_t *ma_comm; /*!< Communication queue for MA search */
    int ma_pub_state_reg;          /*!< ID of the registry that associates
                                        received public state with state-id. */
    int ma_terminated;             /*!< True if already terminated */
    plan_path_t *ma_path;          /*!< Output path for multi-agent mode */
};



/**
 * Initializas the base search struct.
 */
void _planSearchInit(plan_search_t *search,
                     const plan_search_params_t *params,
                     plan_search_del_fn del_fn,
                     plan_search_init_fn init_fn,
                     plan_search_step_fn step_fn,
                     plan_search_inject_state_fn inject_state_fn);

/**
 * Frees allocated resources.
 */
void _planSearchFree(plan_search_t *search);

/**
 * Finds applicable operators in the specified state and store the results
 * in searchc->applicable_ops.
 */
void _planSearchFindApplicableOps(plan_search_t *search,
                                  plan_state_id_t state_id);

/**
 * Returns PLAN_SEARCH_CONT if the heuristic value was computed.
 * Any other status should lead to immediate exit from the search algorithm
 * with the same status.
 * If preferred_ops is non-NULL, the function will find preferred
 * operators and set up the given struct accordingly.
 */
int _planSearchHeuristic(plan_search_t *search,
                         plan_state_id_t state_id,
                         plan_cost_t *heur_val,
                         plan_search_applicable_ops_t *preferred_ops);

/**
 * Adds state's successors to the lazy list with the specified cost.
 */
void _planSearchAddLazySuccessors(plan_search_t *search,
                                  plan_state_id_t state_id,
                                  plan_operator_t **op, int op_size,
                                  plan_cost_t cost,
                                  plan_list_lazy_t *list);

/**
 * Generalization for lazy search algorithms.
 * Injects given state into open-list if the node wasn't discovered yet.
 */
int _planSearchLazyInjectState(plan_search_t *search,
                               plan_list_lazy_t *list,
                               plan_state_id_t state_id,
                               plan_cost_t cost, plan_cost_t heur_val);

/**
 * Let the common structure know that a dead end was reached.
 */
void _planSearchReachedDeadEnd(plan_search_t *search);

/**
 * Creates a new state by application of the operator on the parent_state.
 * Returns 0 if the corresponding node is in NEW state, -1 otherwise.
 * The resulting state and node is returned via output arguments.\
 */
int _planSearchNewState(plan_search_t *search,
                        plan_operator_t *operator,
                        plan_state_id_t parent_state,
                        plan_state_id_t *new_state_id,
                        plan_state_space_node_t **new_node);

/**
 * Open and close the state in one step.
 */
plan_state_space_node_t *_planSearchNodeOpenClose(plan_search_t *search,
                                                  plan_state_id_t state,
                                                  plan_state_id_t parent_state,
                                                  plan_operator_t *parent_op,
                                                  plan_cost_t cost,
                                                  plan_cost_t heur);

/**
 * Updates part of statistics.
 */
void _planUpdateStat(plan_search_stat_t *stat,
                     long steps, bor_timer_t *timer);

/**
 * Returns true if the given state is the goal state.
 * Also the goal state is recorded in stats and the goal state is
 * remembered.
 */
int _planSearchCheckGoal(plan_search_t *search, plan_state_id_t state_id);

/**** INLINES ****/
_bor_inline void planSearchStatIncEvaluatedStates(plan_search_stat_t *stat)
{
    ++stat->evaluated_states;
}

_bor_inline void planSearchStatIncExpandedStates(plan_search_stat_t *stat)
{
    ++stat->expanded_states;
}

_bor_inline void planSearchStatIncGeneratedStates(plan_search_stat_t *stat)
{
    ++stat->generated_states;
}

_bor_inline void planSearchStatSetFoundSolution(plan_search_stat_t *stat)
{
    stat->found = 1;
}

_bor_inline void planSearchStatSetNotFound(plan_search_stat_t *stat)
{
    stat->not_found = 1;
}

#endif /* __PLAN_SEARCH_H__ */
