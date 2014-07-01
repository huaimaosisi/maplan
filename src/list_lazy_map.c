#include <boruvka/alloc.h>
#include <boruvka/splaytree_int.h>
#include <boruvka/fifo.h>
#include "plan/list_lazy.h"

/** A structure containing a stored value */
struct _node_t {
    plan_state_id_t parent_state_id;
    plan_operator_t *op;
};
typedef struct _node_t node_t;

/** A node holding a key and all the values. */
struct _keynode_t {
    bor_fifo_t fifo;               /*!< Structure containing all values */
    bor_splaytree_int_node_t tree; /*!< Connector to the tree */
};
typedef struct _keynode_t keynode_t;

/** A main structure */
struct _plan_list_lazy_map_t {
    plan_list_lazy_t list;    /*!< Parent class */
    bor_splaytree_int_t tree; /*!< Instance of a tree */
    keynode_t *pre_keynode;   /*!< Preinitialized key-node */
};
typedef struct _plan_list_lazy_map_t plan_list_lazy_map_t;

static void planListLazyMapDel(void *);
static void planListLazyMapPush(void *,
                                plan_cost_t cost,
                                plan_state_id_t parent_state_id,
                                plan_operator_t *op);
static int planListLazyMapPop(void *,
                              plan_state_id_t *parent_state_id,
                              plan_operator_t **op);
static void planListLazyMapClear(void *);


plan_list_lazy_t *planListLazyMapNew(void)
{
    plan_list_lazy_map_t *l;

    l = BOR_ALLOC(plan_list_lazy_map_t);
    borSplayTreeIntInit(&l->tree);

    l->pre_keynode = BOR_ALLOC(keynode_t);
    borFifoInit(&l->pre_keynode->fifo, sizeof(node_t));

    planListLazyInit(&l->list,
                     planListLazyMapDel,
                     planListLazyMapPush,
                     planListLazyMapPop,
                     planListLazyMapClear);

    return &l->list;
}

static void planListLazyMapDel(void *_l)
{
    plan_list_lazy_map_t *l = _l;
    planListLazyMapClear(l);
    if (l->pre_keynode){
        borFifoFree(&l->pre_keynode->fifo);
        BOR_FREE(l->pre_keynode);
    }
    borSplayTreeIntFree(&l->tree);
    BOR_FREE(l);
}

static void planListLazyMapPush(void *_l,
                                plan_cost_t cost,
                                plan_state_id_t parent_state_id,
                                plan_operator_t *op)
{
    plan_list_lazy_map_t *l = _l;
    node_t n;
    keynode_t *keynode;
    bor_splaytree_int_node_t *kn;

    // Try tu push the preinitialized keynode with the cost as a key
    kn = borSplayTreeIntInsert(&l->tree, cost, &l->pre_keynode->tree);

    if (kn == NULL){
        // The insertion was successful, so remember the inserted key-node
        // and preinitialize a next one.
        keynode = l->pre_keynode;
        l->pre_keynode = BOR_ALLOC(keynode_t);
        borFifoInit(&l->pre_keynode->fifo, sizeof(node_t));

    }else if (kn != NULL){
        // Key already in tree
        keynode = bor_container_of(kn, keynode_t, tree);
    }

    // Set up the actual values and insert it into key-node.
    n.parent_state_id = parent_state_id;
    n.op = op;
    borFifoPush(&keynode->fifo, &n);
}

static int planListLazyMapPop(void *_l,
                              plan_state_id_t *parent_state_id,
                              plan_operator_t **op)
{
    plan_list_lazy_map_t *l = _l;
    node_t *n;
    keynode_t *keynode;
    bor_splaytree_int_node_t *kn;

    if (borSplayTreeIntEmpty(&l->tree))
        return -1;

    // Get the minimal key-node
    kn = borSplayTreeIntMin(&l->tree);
    keynode = bor_container_of(kn, keynode_t, tree);

    // We know for sure that this key-node must contain some values because
    // an empty key-nodes are removed.
    // Pop the values from the key-node.
    n = borFifoFront(&keynode->fifo);
    *parent_state_id = n->parent_state_id;
    *op              = n->op;
    borFifoPop(&keynode->fifo);

    // If the key-node is empty, remove it from the tree
    if (borFifoEmpty(&keynode->fifo)){
        borSplayTreeIntRemove(&l->tree, &keynode->tree);
        borFifoFree(&keynode->fifo);
        BOR_FREE(keynode);
    }

    return 0;
}

static void planListLazyMapClear(void *_l)
{
    plan_list_lazy_map_t *l = _l;
    bor_splaytree_int_node_t *kn, *tmp;
    keynode_t *keynode;

    BOR_SPLAYTREE_INT_FOR_EACH_SAFE(&l->tree, kn, tmp){
        borSplayTreeIntRemove(&l->tree, kn);
        keynode = bor_container_of(kn, keynode_t, tree);
        borFifoFree(&keynode->fifo);
        BOR_FREE(keynode);
    }
}