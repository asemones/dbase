#include "compactor.h"
#include "iter.h"
#include "lsm.h"
#include <stdio.h>
#include <stdlib.h>
#include "option.h"
#include <pthread.h>
#include <time.h>


typedef struct compact_tble_info{
    compact_manager * cm;
    compact_job_internal job;
}compact_tble_info;

bool check_for_compact(list * level, int lvl);
sst_f_inf sst_mark_and_grab(list * my_list, sst_f_inf * victim, int index);
int find_compact_friend(list * next_level, list * list_to_merge, sst_f_inf * victim);
void merge_wrapper(void * info, void ** null_ret, thread * thrd);
int check_compacts(compact_manager * cm, int * levels_needed);
int compute_priority(int level);
int create_jobs(compact_manager * cm, int levels[]);
int start_jobs(compact_manager * cm);
void run_compactor(void * cm_thrd, void ** null_ret, thread * pool);
