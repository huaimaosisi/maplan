#ifndef __PLAN_MA_COMM_QUEUE_H__
#define __PLAN_MA_COMM_QUEUE_H__

#include <pthread.h>
#include <semaphore.h>
#include <boruvka/fifo.h>

#include "ma_msg.pb-c.h"

/** Forward declarations */
typedef struct _plan_ma_comm_queue_pool_t plan_ma_comm_queue_pool_t;
typedef struct _plan_ma_comm_queue_t plan_ma_comm_queue_t;

struct _plan_ma_comm_queue_node_t {
    bor_fifo_t fifo;      /*!< Queue with messages */
    pthread_mutex_t lock; /*!< Mutex-lock for the queue */
    sem_t full;           /*!< Full semaphore for the queue */
    sem_t empty;          /*!< Empty semaphore for the queue */
};
typedef struct _plan_ma_comm_queue_node_t plan_ma_comm_queue_node_t;

struct _plan_ma_comm_queue_pool_t {
    plan_ma_comm_queue_node_t *node; /*!< Array of communaction nodes */
    int node_size;                   /*!< Number of nodes */
    plan_ma_comm_queue_t *queue;
};

struct _plan_ma_comm_queue_t {
    plan_ma_comm_queue_pool_t pool; /*!< Corresponding pool */
    int node_id;                    /*!< ID of the current node */
    int arbiter;                    /*!< True if this node is arbiter */
};


/**
 * Creates a pool of communication queues for specified number of nodes.
 */
plan_ma_comm_queue_pool_t *planMACommQueuePoolNew(int num_nodes);

/**
 * Destroys a queue pool.
 */
void planMACommQueuePoolDel(plan_ma_comm_queue_pool_t *pool);

/**
 * Returns a queue corresponding to the specified node.
 * The returned queue is still owned by the pool.
 */
plan_ma_comm_queue_t *planMACommQueue(plan_ma_comm_queue_pool_t *pool,
                                      int node_id);

/**
 * Sends the message to all nodes.
 * Returns 0 on success.
 */
int planMACommQueueSendToAll(plan_ma_comm_queue_t *comm,
                             const PlanMultiAgentMsg *msg);

/**
 * Sends the message to an arbiter.
 * Returns 0 on success.
 */
int planMACommQueueSendToArbiter(plan_ma_comm_queue_t *comm,
                                 const PlanMultiAgentMsg *msg);

/**
 * Sends the message to the specified node.
 * Returns 0 on success.
 */
int planMACommQueueSendToNode(plan_ma_comm_queue_t *comm,
                              int node_id,
                              const PlanMultiAgentMsg *msg);

/**
 * Receives a next message in non-blocking mode.
 * It is caller's responsibility to destroy the returned message.
 */
PlanMultiAgentMsg *planMACommQueueRecv(plan_ma_comm_queue_t *comm);

/**
 * Receives a next message in blocking mode.
 * It is caller's responsibility to destroy the returned message.
 */
PlanMultiAgentMsg *planMACommQueueRecvBlock(plan_ma_comm_queue_t *comm);

#endif /* __PLAN_MA_COMM_QUEUE_H__ */
