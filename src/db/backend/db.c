#include "db.h"
option GLOB_OPTS;
#define MINI_STACK 512
db_shard db_shard_create(){
    db_shard shard;
    storage_engine * lsm = create_engine(GLOB_OPTS.META_F_N, GLOB_OPTS.BLOOM_F_N);
    compact_manager * manager= init_cm(lsm->meta, &lsm->cach);
    cache * raw = get_raw_cache(lsm->cach);
    init_io_manager(man,raw->max_pages * 1.1,GLOB_OPTS.NUM_COMPACTOR_UNITS, GLOB_OPTS.num_memtable, GLOB_OPTS.SST_TABLE_SIZE, GLOB_OPTS.MEM_TABLE_SIZE);
    shard.io_manager = man;

    const int max_concurrent_ops = raw->max_pages * 1.5;
    shard.io_manager->io_requests = create_pool(max_concurrent_ops);
    db_FILE * io= malloc(sizeofdb_FILE() * max_concurrent_ops);
    
    init_db_FILE_ctx(max_concurrent_ops, io);

    for (int i = 0 ; i < raw->max_pages; i ++){
        raw->frames[i].buf = request_struct(shard.io_manager->four_kb);
    }
    lsm->cm_ref = &manager->check_meta_cond;
    shard.lsm = lsm;
    shard.manager=  manager;
    return shard;
} 
full_db * db(int num_shards){
    full_db * db = malloc(sizeof(*db));

    db->num_shard = num_shards;
    db->shard = malloc(sizeof(db_shard) * num_shards);
    for (int i = 0; i < db->num_shard; i++){
        db->shard[i] = db_shard_create();
    }
    return db;
}



