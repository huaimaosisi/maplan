#include <boruvka/timer.h>
#include <plan/search.h>
#include <plan/heur.h>
#include <plan/search_ehc.h>
#include <plan/search_lazy.h>
#include <opts.h>

static char *def_problem = NULL;
static char *def_search = "ehc";
static char *def_list = "heap";
static char *def_heur = "goalcount";
static char *plan_output_fn = NULL;
static int max_time = 60 * 30; // 30 minutes
static int max_mem = 1024 * 1024; // 1GB
static int progress_freq = 10000;

static int readOpts(int argc, char *argv[])
{
    int help;

    optsAddDesc("help", 'h', OPTS_NONE, &help, NULL,
                "Print this help.");
    optsAddDesc("problem", 'p', OPTS_STR, &def_problem, NULL,
                "Path to the .json problem definition.");
    optsAddDesc("search", 's', OPTS_STR, &def_search, NULL,
                "Define search algorithm [ehc|lazy] (default: ehc)");
    optsAddDesc("list", 'l', OPTS_STR, &def_list, NULL,
                "Define list type [heap|bucket] (default: heap)");
    optsAddDesc("heur", 'H', OPTS_STR, &def_heur, NULL,
                "Define heuristic [goalcount|add|max|ff] (default: goalcount)");
    optsAddDesc("plan-output", 'o', OPTS_STR, &plan_output_fn, NULL,
                "Path where to write resulting plan.");
    optsAddDesc("max-time", 0x0, OPTS_INT, &max_time, NULL,
                "Maximal time the search can spent on finding solution in"
                " seconds. (default: 30 minutes).");
    optsAddDesc("max-mem", 0x0, OPTS_INT, &max_mem, NULL,
                "Maximal memory (peak memory) in kb. (default: 1GB)");

    if (opts(&argc, argv) != 0){
        return -1;
    }

    if (help)
        return -1;

    if (def_problem == NULL){
        fprintf(stderr, "Error: Problem must be defined (-p).\n");
        return -1;
    }

    return 0;
}

static void usage(const char *progname)
{
    fprintf(stderr, "Usage: %s [OPTIONS]\n", progname);
    fprintf(stderr, "  OPTIONS:\n");
    optsPrint(stderr, "    ");
    fprintf(stderr, "\n");
}

static int progress(const plan_search_stat_t *stat)
{
    fprintf(stderr, "[%.3f s, %ld kb] %ld steps, %ld evaluated,"
                    " %ld expanded, %ld generated, dead-end: %d,"
                    " found-solution: %d\n",
            stat->elapsed_time, stat->peak_memory,
            stat->steps, stat->evaluated_states, stat->expanded_states,
            stat->generated_states,
            stat->dead_end, stat->found_solution);

    if (stat->elapsed_time > max_time){
        fprintf(stderr, "Abort: Exceeded max-time.\n");
        printf("Abort: Exceeded max-time.\n");
        return PLAN_SEARCH_ABORT;
    }

    if (stat->peak_memory > max_mem){
        fprintf(stderr, "Abort: Exceeded max-mem.\n");
        printf("Abort: Exceeded max-mem.\n");
        return PLAN_SEARCH_ABORT;
    }

    return PLAN_SEARCH_CONT;
}

int main(int argc, char *argv[])
{
    plan_problem_t *prob;
    plan_list_lazy_t *list;
    plan_heur_t *heur;
    plan_search_t *search;
    plan_search_ehc_params_t ehc_params;
    plan_search_lazy_params_t lazy_params;
    plan_search_params_t *params;
    plan_path_t path;
    bor_timer_t timer;
    int res;
    FILE *fout;

    if (readOpts(argc, argv) != 0){
        usage(argv[0]);
        return -1;
    }

    borTimerStart(&timer);

    printf("Problem: %s\n", def_problem);
    printf("Search: %s\n", def_search);
    printf("List: %s\n", def_list);
    printf("Heur: %s\n", def_heur);
    printf("Max Time: %d s\n", max_time);
    printf("Max Mem: %d kb\n", max_mem);
    printf("\n");

    prob = planProblemFromJson(def_problem);
    if (prob == NULL){
        return -1;
    }

    printf("Num variables: %d\n", prob->var_size);
    printf("Num operators: %d\n", prob->op_size);
    printf("Bytes per state: %d\n",
           planStatePackerBufSize(prob->state_pool->packer));
    printf("Size of state id: %d\n", (int)sizeof(plan_state_id_t));
    borTimerStop(&timer);
    printf("Loading Time: %f s\n", borTimerElapsedInSF(&timer));
    printf("\n");

    if (strcmp(def_list, "heap") == 0){
        list = planListLazyHeapNew();
    }else if (strcmp(def_list, "bucket") == 0){
        list = planListLazyBucketNew();
    }else{
        fprintf(stderr, "Error: Invalid list type\n");
        return -1;
    }

    if (strcmp(def_heur, "goalcount") == 0){
        heur = planHeurGoalCountNew(prob->goal);
    }else if (strcmp(def_heur, "add") == 0){
        heur = planHeurRelaxAddNew(prob);
    }else if (strcmp(def_heur, "max") == 0){
        heur = planHeurRelaxMaxNew(prob);
    }else if (strcmp(def_heur, "ff") == 0){
        heur = planHeurRelaxFFNew(prob);
    }else{
        fprintf(stderr, "Error: Invalid heuristic type\n");
        return -1;
    }

    if (strcmp(def_search, "ehc") == 0){
        planSearchEHCParamsInit(&ehc_params);
        ehc_params.heur = heur;
        params = &ehc_params.search;
    }else if (strcmp(def_search, "lazy") == 0){
        planSearchLazyParamsInit(&lazy_params);
        lazy_params.heur = heur;
        lazy_params.list = list;
        params = &lazy_params.search;
    }else{
        fprintf(stderr, "Error: Unkown search algorithm.\n");
        return -1;
    }

    params->progress_fn = progress;
    params->progress_freq = progress_freq;
    params->prob = prob;

    if (strcmp(def_search, "ehc") == 0){
        search = planSearchEHCNew(&ehc_params);
    }else if (strcmp(def_search, "lazy") == 0){
        search = planSearchLazyNew(&lazy_params);
    }

    borTimerStop(&timer);
    printf("Init Time: %f s\n", borTimerElapsedInSF(&timer));

    planPathInit(&path);
    res = planSearchRun(search, &path);
    if (res == PLAN_SEARCH_FOUND){
        printf("Solution found.\n");

        if (plan_output_fn != NULL){
            fout = fopen(plan_output_fn, "w");
            if (fout != NULL){
                planPathPrint(&path, fout);
                fclose(fout);
                printf("Plan written to `%s'\n", plan_output_fn);
            }else{
                fprintf(stderr, "Error: Could not plan write to `%s'\n",
                        plan_output_fn);
            }
        }

        printf("Path Cost: %d\n", (int)planPathCost(&path));

    }else if (res == PLAN_SEARCH_DEAD_END){
        printf("Solution NOT found.");

    }else if (res == PLAN_SEARCH_ABORT){
        printf("Search Aborted.");
    }

    printf("\n");
    printf("Search Time: %f\n", search->stat.elapsed_time);
    printf("Steps: %ld\n", search->stat.steps);
    printf("Evaluated States: %ld\n", search->stat.evaluated_states);
    printf("Expanded States: %ld\n", search->stat.expanded_states);
    printf("Generated States: %ld\n", search->stat.generated_states);
    printf("Peak Memory: %ld kb\n", search->stat.peak_memory);

    planPathFree(&path);

    planSearchDel(search);
    planHeurDel(heur);
    planListLazyDel(list);
    planProblemDel(prob);

    borTimerStop(&timer);
    printf("Overall Time: %f s\n", borTimerElapsedInSF(&timer));

    optsClear();

    return 0;
}