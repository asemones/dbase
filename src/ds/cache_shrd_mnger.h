#include "cache.h"
#ifndef CACHE_SHRD_MNGER_C
#define CACHE_SHRD_MNGER_C

typedef struct shard_controller{
    cache * caches;
    int num_caches;
}shard_controller;
shard_controller create_shard_controller(int num_caches, int shrd_cap,  int pg_size);
cache_entry * retrieve_entry_sharded(shard_controller controller, block_index * ind,  const char * f_n);
void free_shard_controller(shard_controller *controller);
#endif