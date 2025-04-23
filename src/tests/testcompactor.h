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


db_shard shard; // Use db_shard instead of separate engine/cm/thrd

void prep_test(){
    // Create the shard, which includes the engine, compactor, and its runtime
    shard = db_shard_create();
    // Compactor runs automatically within the shard's runtime, no need for separate thread pool
}
void end_test(){
    sleep(10);
    // db_end handles compactor shutdown and resource cleanup
    remove_ssts(shard.lsm->meta->sst_files); // Get sst_files from the shard's metadata
    sleep(10); // Keep sleep if needed for operations to settle before file removal
    db_end(&shard);
    remove(GLOB_OPTS.META_F_N);
    remove(GLOB_OPTS.BLOOM_F_N);
    remove ("WAL_0.bin");
    remove("WAL_1.bin");
    remove("WAL_M.bin");
}
void fat_db_passive(){
    prep_test();
    create_a_bigbase(&shard); // Pass shard pointer
    end_test();
}
void random_values_short(){
    const int size_per_add =5000;
    prep_test();
    create_a_bigbase(&shard); // Pass shard pointer
    add_random_records_a_z(&shard,size_per_add); // Pass shard pointer
    end_test();
}
void random_values_long(){
    const int size_per_add =50000;
    prep_test();
    create_a_bigbase(&shard); // Pass shard pointer
    add_random_records_a_z(&shard,size_per_add); // Pass shard pointer
    end_test();
}
