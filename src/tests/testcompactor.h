#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../db/backend/compactor.h"
#include "../db/backend/lsm.h"
#include "../ds/byte_buffer.h"
#include "unity/src/unity.h"
#include "../ds/arena.h"
#include "../util/alloc_util.h"
#include "test_util_funcs.h"
#include"../db/backend/key-value.h"
#include "../db/backend/compat_schedule.h"
#pragma once


compact_manager * cm;
storage_engine * engine;
thread_p * thrd;

void prep_test(){
    engine = create_engine(GLOB_OPTS.META_F_N, GLOB_OPTS.BLOOM_F_N);
    cm = init_cm(engine->meta, engine->cach);
    engine->cm_ref = &cm->check_meta_cond;
    cm->wait = engine->compactor_wait;
    cm->wait_mtx = engine->compactor_wait_mtx;

    thread_p * thrd = thread_Pool(3, 0);
    int priority=  1;
    add_work(thrd, &run_compactor, cm, NULL, &priority);
    
}
void kill_test(){
    sleep(10);
    shutdown_cm(cm);
    destroy_pool(thrd);
    remove_ssts(cm->sst_files);
    sleep(10);
    free_cm(cm);
    free_engine(engine, GLOB_OPTS.META_F_N, GLOB_OPTS.BLOOM_F_N);
    remove(GLOB_OPTS.META_F_N);
    remove(GLOB_OPTS.BLOOM_F_N);
    remove ("WAL_0.bin");
    remove("WAL_1.bin");
    remove("WAL_M.bin");
}
void fat_db_passive(){
    prep_test();
    create_a_bigbase(engine);
    kill_test();
}
void random_values_short(){
    const int size_per_add =5000;
    prep_test();
    create_a_bigbase(engine);
    add_random_records_a_z(engine,size_per_add);
    kill_test();
}
void random_values_long(){
    const int size_per_add =50000;
    prep_test();
    create_a_bigbase(engine);
    add_random_records_a_z(engine,size_per_add);
    kill_test();
}
