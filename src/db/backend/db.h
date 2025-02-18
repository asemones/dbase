#include "compactor.h"
#include "iter.h"
#include "lsm.h"
#include <stdio.h>
#include <stdlib.h>
#include "option.h"
#include <pthread.h>

option GLOB_OPTS;


void db_start(){
    storage_engine * lsm = create_engine(GLOB_OPTS.META_F_N, GLOB_OPTS.BLOOM_F_N);
    compact_manager * manager= init_cm(lsm->meta, lsm->cach);
    lsm->cm_ref = &manager->check_meta_cond;
    manager->wait = lsm->compactor_wait;
    manager->wait_mtx = lsm->compactor_wait_mtx;
}   

void db_end(storage_engine * lsm, compact_manager * manager){
    
    free_cm(manager);
    free_engine(lsm, GLOB_OPTS.META_F_N, GLOB_OPTS.BLOOM_F_N);
}





