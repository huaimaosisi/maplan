#include <strings.h>
#include <stdio.h>
#include <boruvka/alloc.h>
#include "plan/prioqueue.h"

void planBucketQueueInit(plan_bucket_queue_t *q)
{
    q->bucket_size = PLAN_BUCKET_QUEUE_SIZE;
    q->bucket = BOR_ALLOC_ARR(plan_prioqueue_bucket_t, q->bucket_size);
    bzero(q->bucket, sizeof(plan_prioqueue_bucket_t) * q->bucket_size);
    q->lowest_key = q->bucket_size;
    q->size = 0;
}

void planBucketQueueFree(plan_bucket_queue_t *q)
{
    int i;

    for (i = 0; i < q->bucket_size; ++i){
        if (q->bucket[i].value)
            BOR_FREE(q->bucket[i].value);
    }
    BOR_FREE(q->bucket);
}

void planBucketQueuePush(plan_bucket_queue_t *q, int key, int value)
{
    plan_prioqueue_bucket_t *bucket;

    if (key >= PLAN_BUCKET_QUEUE_SIZE){
        fprintf(stderr, "Error: planBucketQueue: key %d is over a size of"
                        " the bucket queue, which is %d.",
                        key, PLAN_BUCKET_QUEUE_SIZE);
        exit(-1);
    }

    bucket = q->bucket + key;
    if (bucket->value == NULL){
        // TODO: remove constant
        bucket->alloc = 64;
        bucket->value = BOR_ALLOC_ARR(int, bucket->alloc);

    }else if (bucket->size + 1 > bucket->alloc){
        // TODO: constant
        bucket->alloc *= 2;
        bucket->value = BOR_REALLOC_ARR(bucket->value,
                                        int, bucket->alloc);
    }
    bucket->value[bucket->size++] = value;
    ++q->size;

    if (key < q->lowest_key)
        q->lowest_key = key;
}

int planBucketQueuePop(plan_bucket_queue_t *q, int *key)
{
    plan_prioqueue_bucket_t *bucket;
    int val;

    bucket = q->bucket + q->lowest_key;
    while (bucket->size == 0){
        ++q->lowest_key;
        bucket += 1;
    }

    val = bucket->value[--bucket->size];
    *key = q->lowest_key;
    --q->size;
    return val;
}
