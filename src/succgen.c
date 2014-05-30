#include <boruvka/alloc.h>
#include "plan/succgen.h"

/**
 * Base building structure for a tree node of successor generator.
 */
struct _plan_succ_gen_tree_t {
    unsigned var;                       /*!< Decision variable */
    plan_operator_t **ops;              /*!< List of immediate operators
                                             that are returned once this
                                             node is reached */
    size_t ops_size;
    struct _plan_succ_gen_tree_t **val; /*!< Subtrees indexed by the value
                                             of the decision variable */
    size_t val_size;
    struct _plan_succ_gen_tree_t *def;  /*!< Default subtree containing
                                             operators without precondition
                                             on the decision variable */
};
typedef struct _plan_succ_gen_tree_t plan_succ_gen_tree_t;

/** Creates a new tree node (and recursively all subtrees) */
static plan_succ_gen_tree_t *treeNew(unsigned start_var,
                                     plan_operator_t **ops,
                                     size_t len);
/** Recursively deletes a tree */
static void treeDel(plan_succ_gen_tree_t *tree);

/** Finds applicable operators to the given state */
static size_t treeFind(const plan_succ_gen_tree_t *tree,
                       const plan_state_t *state,
                       plan_operator_t **op, size_t op_size);

/** Set immediate operators to tree node */
static void treeBuildSetOps(plan_succ_gen_tree_t *tree,
                            plan_operator_t **ops, size_t len);
/** Build default subtree */
static size_t treeBuildDef(plan_succ_gen_tree_t *tree,
                           unsigned var,
                           plan_operator_t **ops, size_t len);
/** Prepare array of .val[] subtrees */
static void treeBuildPrepareVal(plan_succ_gen_tree_t *tree,
                                unsigned val);
/** Build val subtree */
static size_t treeBuildVal(plan_succ_gen_tree_t *tree,
                           unsigned var,
                           plan_operator_t **ops, size_t len);

/** Comparator for qsort which sorts operators by its variables and its
 *  values. */
static int opsSortCmp(const void *a, const void *b);




plan_succ_gen_t *planSuccGenNew(plan_operator_t *op, size_t opsize)
{
    plan_succ_gen_t *sg;
    plan_operator_t **sorted_ops;
    size_t i;

    // prepare array for sorting operators
    sorted_ops = BOR_ALLOC_ARR(plan_operator_t *, opsize);
    for (i = 0; i < opsize; ++i)
        sorted_ops[i] = op + i;

    // Sort operators by values of preconditions.
    qsort(sorted_ops, opsize, sizeof(plan_operator_t *), opsSortCmp);

    sg = BOR_ALLOC(plan_succ_gen_t);
    sg->root = treeNew(0, sorted_ops, opsize);
    sg->num_operators = opsize;

    BOR_FREE(sorted_ops);
    return sg;
}

void planSuccGenDel(plan_succ_gen_t *sg)
{
    if (sg->root)
        treeDel(sg->root);

    BOR_FREE(sg);
}

size_t planSuccGenFind(const plan_succ_gen_t *sg,
                       const plan_state_t *state,
                       plan_operator_t **op, size_t op_size)
{
    return treeFind(sg->root, state, op, op_size);
}






static int opsSortCmp(const void *a, const void *b)
{
    const plan_operator_t *opa = *(const plan_operator_t **)a;
    const plan_operator_t *opb = *(const plan_operator_t **)b;
    size_t i, size = planPartStateSize(opa->pre);
    int aset, bset;
    unsigned aval, bval;

    for (i = 0; i < size; ++i){
        aset = planPartStateIsSet(opa->pre, i);
        bset = planPartStateIsSet(opb->pre, i);

        if (aset && bset){
            aval = planPartStateGet(opa->pre, i);
            bval = planPartStateGet(opb->pre, i);

            if (aval < bval){
                return -1;
            }else if (aval > bval){
                return 1;
            }
        }else if (!aset && bset){
            return -1;
        }else if (aset && !bset){
            return 1;
        }
    }

    return 0;
}

static void treeBuildSetOps(plan_succ_gen_tree_t *tree,
                            plan_operator_t **ops, size_t len)
{
    size_t i;

    if (tree->ops)
        BOR_FREE(tree->ops);

    tree->ops_size = len;
    tree->ops = BOR_ALLOC_ARR(plan_operator_t *, tree->ops_size);

    for (i = 0; i < len; ++i)
        tree->ops[i] = ops[i];
}

static size_t treeBuildDef(plan_succ_gen_tree_t *tree,
                           unsigned var,
                           plan_operator_t **ops, size_t len)
{
    size_t size;

    for (size = 1;
         size < len && !planPartStateIsSet(ops[size]->pre, var);
         ++size);

    tree->var = var;
    tree->def = treeNew(var + 1, ops, size);

    return size;
}

static void treeBuildPrepareVal(plan_succ_gen_tree_t *tree,
                                unsigned val)
{
    size_t i;

    tree->val_size = val + 1;
    tree->val = BOR_ALLOC_ARR(plan_succ_gen_tree_t *, tree->val_size);

    for (i = 0; i < tree->val_size; ++i)
        tree->val[i] = NULL;
}

static size_t treeBuildVal(plan_succ_gen_tree_t *tree,
                           unsigned var,
                           plan_operator_t **ops, size_t len)
{
    size_t size;
    unsigned val;

    val = planPartStateGet(ops[0]->pre, var);

    for (size = 1;
         size < len && planPartStateGet(ops[size]->pre, var) == val;
         ++size);

    tree->var = var;
    tree->val[val] = treeNew(var + 1, ops, size);

    return size;
}

static plan_succ_gen_tree_t *treeNew(unsigned start_var,
                                     plan_operator_t **ops,
                                     size_t len)
{
    plan_succ_gen_tree_t *tree;
    unsigned var, num_vars;
    size_t start;

    tree = BOR_ALLOC(plan_succ_gen_tree_t);
    tree->var = -1;
    tree->ops = NULL;
    tree->ops_size = 0;
    tree->val = NULL;
    tree->val_size = 0;
    tree->def = NULL;

    // Find first variable that is set for at least one operator.
    // The operators are sorted so that it is enough to check the last
    // operator in the array.
    num_vars = planPartStateSize(ops[0]->pre);
    for (var = start_var; var < num_vars; ++var){
        if (planPartStateIsSet(ops[len - 1]->pre, var))
            break;
    }

    if (var == num_vars){
        // If there isn't any operator with set value anymore insert all
        // operators as immediate ops and exit.
        treeBuildSetOps(tree, ops, len);
        return tree;
    }

    // Now we now that array of operators contain at least one operator
    // with set value of current variable.

    // Prepare val array -- we now that the last operator in array has
    // largest value.
    treeBuildPrepareVal(tree, planPartStateGet(ops[len - 1]->pre, var));

    // Initialize index of the first element with current value
    start = 0;

    // First check unset values from the beggining of the array
    if (!planPartStateIsSet(ops[0]->pre, var)){
        start = treeBuildDef(tree, var, ops, len);
    }

    // Then build subtree for each value
    while (start < len){
        start += treeBuildVal(tree, var, ops + start, len - start);
    }

    return tree;
}

static void treeDel(plan_succ_gen_tree_t *tree)
{
    size_t i;

    if (tree->ops)
        BOR_FREE(tree->ops);

    if (tree->val){
        for (i = 0; i < tree->val_size; ++i)
            if (tree->val[i])
                treeDel(tree->val[i]);
        BOR_FREE(tree->val);
    }

    if (tree->def)
        treeDel(tree->def);

    BOR_FREE(tree);
}

static size_t treeFind(const plan_succ_gen_tree_t *tree,
                       const plan_state_t *state,
                       plan_operator_t **op, size_t op_size)
{
    size_t i, found = 0, size;
    unsigned val;

    // insert all immediate operators
    if (tree->ops_size > 0){
        for (i = 0; i < tree->ops_size; ++i){
            if (op_size > i)
                op[i] = tree->ops[i];
        }
        found = tree->ops_size;
    }

    // check whether this node should check on any variable value
    if (tree->var != (unsigned)-1){
        // get corresponding value from state
        val = planStateGet(state, tree->var);

        // use default tree if present
        if (tree->def){
            size = BOR_MAX(0, (int)op_size - (int)found);
            found += treeFind(tree->def, state, op + found, size);
        }

        // and use tree corresponding to the value if present
        if (val < tree->val_size && tree->val[val]){
            size = BOR_MAX(0, (int)op_size - (int)found);
            found += treeFind(tree->val[val], state, op + found, size);
        }
    }

    return found;
}