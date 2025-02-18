#include <stdlib.h>
#include <stdio.h>
#include "../../ds/list.h"
#include <pthread.h>
#include "../../util/error.h"
#include "../../util/stringutil.h"
#include "sst_builder.h"
#include "indexer.h"
#include "../../ds/cache.h"
#define LEVELS 7
#pragma once





/*need to replace current metadata raw list with snapshots. Snapshots = the db, minus meetables for now. Using this, we can effectively track and exchange database snapshots while controlling
freeing of resources. It's the solution for allowing concurrenct compaction and required for mvcc*/
/*problems to solve:
1. How can we ensure that compactations can occur without locking reads/writes
*/
typedef struct snapshot{
    char * timestamp;
    int reference_counter;
    pthread_mutex_t counter_lock;
    list * sst_files[LEVELS];
    cache * cache_ref; /*for reading and writing*/
}snapshot;


/*creates copy of metadata info*/
snapshot * create_snap(){
    snapshot * s = malloc(sizeof(snapshot));
    s->reference_counter = 0;
    s->timestamp = malloc(TIME_STAMP_SIZE);
    s->cache_ref = NULL;
    return s;
}
int init_snapshot(snapshot * master, snapshot * s, cache * c){
    pthread_mutex_init(&s->counter_lock, NULL);
    for (int i =0; i < LEVELS; i++){
        s->sst_files[i]= thread_safe_list(0, sizeof (sst_f_inf), false);
        if (master == NULL|| master->sst_files[i]== NULL) continue;
        for (int j = 0; j < master->sst_files[i]->len; j++){
            sst_f_inf f;
            sst_f_inf * sst_to_copy=  at(master->sst_files[i],j);
            int ret =sst_deep_copy(sst_to_copy, &f);
            if (ret !=OK) return ret;
            insert(s->sst_files[i], &f);
        }
    }
    grab_time_char(s->timestamp);
    s->cache_ref = c;
    return OK;
}
int destroy_snapshot(snapshot * s){
    if (s->reference_counter > 0){
        return PROTECTED_RESOURCE;
    }
    pthread_mutex_destroy(&s->counter_lock);
    for (int i = 0; i < MAX_LEVELS; i++ ){
        for (int j= 0;  i < s->sst_files[i]->len; j++){
            sst_f_inf * f = at(s->sst_files[i],j);
            if (f->marked){
                remove(f->file_name);
            }
        }
        free_list(s->sst_files[i], &free_sst_inf);
    }
    free(s->timestamp);
    free(s);
    s->cache_ref =  NULL;
    return OK;
}

void ref_snapshot(snapshot * s){
    pthread_mutex_lock(&s->counter_lock);
    s->reference_counter ++;
    pthread_mutex_unlock(&s->counter_lock);
}
void deref_snapshot(snapshot * s){
    pthread_mutex_lock(&s->counter_lock);
    s->reference_counter --;
    pthread_mutex_unlock(&s->counter_lock);
}
void deref_with_free(snapshot * s){
    pthread_mutex_lock(&s->counter_lock);
    s->reference_counter --;
    pthread_mutex_unlock(&s->counter_lock);
    if (s->reference_counter == 0) destroy_snapshot(s);
}