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
    pthread_mutex_lock(&my_list->write_lock);
    sst_f_inf * target = internal_at_unlocked(my_list, index);
    if (target == NULL){
        pthread_mutex_unlock(&my_list->write_lock);
        return empty;
    }
    if (target->in_cm_job == true){
         pthread_mutex_unlock(&my_list->write_lock);
         return empty;
    }
    if (victim == NULL){
        sst_f_inf to_return = *target;
        target->in_cm_job = true;
        pthread_mutex_unlock(&my_list->write_lock);
        return to_return;
    }
    if (sst_equals(victim, target)){
        pthread_mutex_unlock(&my_list->write_lock);
        return empty;
    }
    int score = calculate_overlap(victim->min,victim->max, target->min, target->max);
    if (target->in_cm_job && score == 0){
        pthread_mutex_unlock(&my_list->write_lock);
        memcpy(empty.file_name, "cancel", strlen("cancel") + 1);
        return empty;
    }
    if (score != 0){
        pthread_mutex_unlock(&my_list->write_lock);
        return empty;
    }
    target->in_cm_job = true;
    sst_f_inf to_return = *target;
    pthread_mutex_unlock(&my_list->write_lock);
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
void merge_wrapper(void * info, void ** null_ret, thread * thrd){
    compact_tble_info * inf = info;
    int len =  inf->job.to_merge->len;
    compact_one_table(inf->cm, inf->job, inf->job.target);
    fprintf(stdout, "finished job: lvl %ld to lvl %ld with %d files\n", inf->job.start_level, inf->job.end_level, len);
    free(inf);
    return;
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
            job.index = j;
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
                pthread_mutex_lock(&(cm->sst_files[job.start_level]->write_lock));
                int ind  = find_sst_file_eq_iter(cm->sst_files[job.start_level], sst_fs->len, ret.file_name);
                sst_f_inf * reset_targ = internal_at_unlocked(cm->sst_files[job.start_level], ind);
                reset_targ->in_cm_job = false;
                pthread_mutex_unlock(&(cm->sst_files[job.start_level]->write_lock));
                pthread_mutex_lock(&(cm->sst_files[job.search_level]->write_lock));
                for (int l = 1; l < job.to_merge->len; l++){
                    sst_f_inf  * sst_to_reset = internal_at_unlocked(job.to_merge, l);
                    ind  = find_sst_file_eq_iter(cm->sst_files[job.search_level],cm->sst_files[job.search_level]->len, sst_to_reset->file_name);
                    reset_targ = internal_at_unlocked(cm->sst_files[job.search_level], ind);
                    if (reset_targ != NULL) reset_targ->in_cm_job = false;
                }
                pthread_mutex_unlock(&(cm->sst_files[job.search_level]->write_lock));
                if (i != 0){
                    free_list(job.to_merge, NULL);
                    free_list(job.new_sst_files, NULL);
                    continue;
                }
                pthread_mutex_lock(&(cm->sst_files[0]->write_lock));
                for(int l =  1; l < job.to_merge->len; l++){
                    sst_f_inf  * sst_to_reset = internal_at_unlocked(job.to_merge, l);
                    ind  = find_sst_file_eq_iter(cm->sst_files[0],cm->sst_files[0]->len, sst_to_reset->file_name);
                    reset_targ = internal_at_unlocked(cm->sst_files[0], ind);
                    if (reset_targ != NULL) reset_targ->in_cm_job = false;
                }
                pthread_mutex_unlock(&(cm->sst_files[0]->write_lock));
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
int start_jobs(compact_manager * cm){
    thread_p * pool = cm->pool[0];
    for (int i = 0; i < 10; i++){
        compact_job_internal job;
        int res = dequeue(cm->job_queue, &job);
        if (res < 0) break;
        compact_tble_info * info  = malloc(sizeof(compact_tble_info));
        info->cm = cm;
        info->job = job; 
        fprintf(stdout, "starting job: lvl %ld to lvl %ld with %d files\n", job.start_level, job.end_level, job.to_merge->len);
        add_work(pool,&merge_wrapper,info, NULL, &i);
    }
    return 0;
}
void run_compactor(void * cm_thrd, void ** null_ret, thread * pool){
    compact_manager * cm = cm_thrd;
    pthread_mutex_lock(cm->wait_mtx);
    int levels[LEVELS];
    struct timespec timer;
    timer.tv_sec = 1;
    timer.tv_nsec = 0;
    while(!cm->exit){
        while(!cm->check_meta_cond){
            int res = pthread_cond_timedwait(cm->wait, cm->wait_mtx, &timer);
            if (res == ETIMEDOUT) break;
        }
        pthread_mutex_unlock(cm->wait_mtx);
        cm->check_meta_cond = false;
        if (cm->exit){
            break;
        }
        if (!check_compacts(cm,levels)) {
            continue;
        }
        create_jobs(cm, levels);
        start_jobs(cm);
        
    }
}



