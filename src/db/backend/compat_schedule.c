#include "compat_schedule.h"


/*shortcut computation. table size * list len since most tables will be the same size, besides maybe the last one*/
bool check_for_compact(list * level, int lvl){
    if (lvl > 0 ){
        return level->len * GLOB_OPTS.SST_TABLE_SIZE > get_level_size(lvl);
    }

    return level->len > GLOB_OPTS.NUM_FILES_COMPACT_ZER0;
}
/*safely marks and extracts an sst while holding the lock*/
sst_f_inf sst_mark_and_grab(list * my_list, sst_f_inf * victim, int index){
    sst_f_inf empty;
    memcpy(empty.file_name, "empty", strlen("empty") + 1);
    sst_f_inf * target = internal_at_unlocked(my_list, index);
    if (target == NULL){
        return empty;
    }
    if (target->in_cm_job == true){
         return empty;
    }
    if (victim == NULL){
        sst_f_inf to_return = *target;
        target->in_cm_job = true;
        return to_return;
    }
    if (sst_equals(victim, target)){
        return empty;
    }
    int score = calculate_overlap(victim->min,victim->max, target->min, target->max);
    if (target->in_cm_job && score == 0){
        memcpy(empty.file_name, "cancel", strlen("cancel") + 1);
        return empty;
    }
    if (score != 0){
        return empty;
    }
    target->in_cm_job = true;
    sst_f_inf to_return = *target;
    return to_return;
}   
int find_compact_friend(list * next_level,list* list_to_merge,sst_f_inf * victim){

    if (next_level == NULL) return -1;
    for (size_t i = 0 ; i < next_level->len; i++){
        
        sst_f_inf sst_ret = sst_mark_and_grab(next_level, victim, i);
        if (strcmp(sst_ret.file_name, "cancel") == 0) return -1;
        if (strcmp(sst_ret.file_name, "empty") !=0) insert(list_to_merge, &sst_ret);
    }
    return 0;
}
future_t merge_wrapper(void * arg){
    compact_tble_info * inf = arg;
    compact_one_table(inf->cm, inf->job, inf->job.target);
    int len =  inf->job.to_merge->len;
    fprintf(stdout, "finished job: lvl %ld to lvl %ld with %d files\n", inf->job.start_level, inf->job.end_level, len);
    insert_struct(inf->pool,  inf);
    cascade_return_none();

}
int check_compacts(compact_manager* cm, int * levels_needed){
    int ret = 0;
    for (int i = 0; i < LEVELS; i++){
        if (!check_for_compact(cm->sst_files[i], i)) continue;
            levels_needed[i] = 1;
            ret =1;
        }
    return ret;

}/*temp stub*/
int compute_priority(int level){
    return level;
}
/*need to convert the compactor off of the really dumb index system to using pointers and searching for dupes in the list 
for removal
rewrite the intergrating tables stuff to use real ssts and not something retarded like indexs */
int create_jobs(compact_manager * cm, int levels []){
    compact_job_internal job;
    for (int i = 0 ; i < LEVELS-1; i++){
        list * sst_fs =  cm->sst_files[i];
        if (levels[i] <= 0 ) continue;
        for(int j = 0; j < sst_fs->len; j++){
            int res= 0;
            if (i == 0 && cm->lvl0_job_running) break;
            sst_f_inf ret = sst_mark_and_grab(sst_fs, NULL, j);
            if (strcmp(ret.file_name, "empty") == 0) continue;
            job.id = compute_priority(i);
            job.start_level = i;
            job.to_merge = List(0, sizeof(sst_f_inf), false);
            job.new_sst_files = List(0, sizeof(sst_f_inf), false);
            job.end_level = i + 1;
            job.search_level = i + 1;
            insert( job.to_merge,&ret);
            if (i == 0) {
                res+=find_compact_friend(cm->sst_files[0],  job.to_merge,  &ret);
            } 
            
            res += find_compact_friend(cm->sst_files[job.search_level],  job.to_merge,  &ret);
            if (res == -1){
                int ind  = find_sst_file_eq_iter(cm->sst_files[job.start_level], sst_fs->len, ret.file_name);
                sst_f_inf * reset_targ = internal_at_unlocked(cm->sst_files[job.start_level], ind);
                reset_targ->in_cm_job = false;
                for (int l = 1; l < job.to_merge->len; l++){
                    sst_f_inf  * sst_to_reset = internal_at_unlocked(job.to_merge, l);
                    ind  = find_sst_file_eq_iter(cm->sst_files[job.search_level],cm->sst_files[job.search_level]->len, sst_to_reset->file_name);
                    reset_targ = internal_at_unlocked(cm->sst_files[job.search_level], ind);
                    if (reset_targ != NULL) reset_targ->in_cm_job = false;
                }
                if (i != 0){
                    free_list(job.to_merge, NULL);
                    free_list(job.new_sst_files, NULL);
                    continue;
                }
                for(int l =  1; l < job.to_merge->len; l++){
                    sst_f_inf  * sst_to_reset = internal_at_unlocked(job.to_merge, l);
                    ind  = find_sst_file_eq_iter(cm->sst_files[0],cm->sst_files[0]->len, sst_to_reset->file_name);
                    reset_targ = internal_at_unlocked(cm->sst_files[0], ind);
                    if (reset_targ != NULL) reset_targ->in_cm_job = false;
                }
                free_list(job.to_merge, NULL);
                free_list(job.new_sst_files, NULL);
                continue;
            }
            job.target = at(job.to_merge, 0);
            enqueue(cm->job_queue, &job);
            if (i == 0 ) cm->lvl0_job_running = true;
           
        } 
    }
    return 0;
}
int start_jobs(struct_pool * pool, compact_manager* cm, uint16_t num){
    for (int i = 0; i < num; i++){
        compact_job_internal job;
        int res = dequeue(cm->job_queue, &job);
        if (res < 0) break;
        compact_tble_info * info = request_struct(pool);
        info->cm = cm;
        info->job = job; 
        fprintf(stdout, "starting job: lvl %ld to lvl %ld with %d files\n", job.start_level, job.end_level, job.to_merge->len);
        cascade_sub_intern_nowait(merge_wrapper, info, NULL);
    }
    return 0;
}
/*task func*/
typedef struct compactor_args{
    cascade_runtime_t * rt;
    compact_manager * cm;
}compactor_args;
future_t run_compactor(compactor_args * args){
    compact_manager * cm = args->cm;
    const int max_concurrent_compactions = GLOB_OPTS.NUM_COMPACTOR_UNITS;
    int  *levels = pad_allocate(sizeof(int) * LEVELS);
    struct_pool * job_args = create_pool(max_concurrent_compactions);
    for(int i = 0; i < max_concurrent_compactions; i++){
        compact_tble_info * inf = pad_allocate(sizeof(compact_tble_info));
        inf->pool = job_args;
        insert_struct(job_args, inf);
    }
    while(1){
        if (cm->exit){
            break;
        }
        if (!check_compacts(cm,levels)) {
            yield_compute;
            continue;
        }
    
        uint32_t max_to_add = max_concurrent_compactions - job_args->size;
        create_jobs(cm, levels);
        start_jobs(job_args, cm, max_to_add);
        intern_wait_for_x(1);
    }
}



