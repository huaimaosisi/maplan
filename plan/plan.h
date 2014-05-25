#ifndef __PLAN_PLAN_H__
#define __PLAN_PLAN_H__

#include <plan/var.h>

struct _plan_t {
    plan_var_t *var;
    size_t var_size;
};
typedef struct _plan_t plan_t;


/**
 * Creates a new (empty) instance of a Fast Downward algorithm.
 */
plan_t *planNew(void);

/**
 * Deletes plan instance.
 */
void planDel(plan_t *plan);

/**
 * Loads definitions from specified file.
 */
int planLoadFromJsonFile(plan_t *plan, const char *filename);

#endif /* __PLAN_PLAN_H__ */