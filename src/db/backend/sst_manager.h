#include "indexer.h"
#include "../../ds/sst_sl.h"
#include "../../ds/slab.h"
#include "../../ds/list.h"
#include "../../ds/circq.h"
#include "../../util/maths.h"
#ifndef SST_MANAGER_H
#define  SST_MANAGER_H

typedef struct partiton_cache_element{
    uint64_t off;
    uint64_t en;
    bb_filter filter;
    block_index * block_indexs;
    uint16_t num_indexs;
    uint16_t ref_count;
}partiton_cache_element;
typedef struct sst_allocator{
    slab_allocator cached;
    slab_allocator non_cached;
    slab_allocator partition_filter_allocator;
}sst_allocator;
typedef struct index_cache {
    size_t capacity;
    size_t page_size;
    size_t filled_pages;
    partiton_cache_element *frames;
    uint8_t *ref_bits;
    size_t  clock_hand;
    uint64_t max_pages;
} index_cache;
typedef struct sst_manager{
    list * l_0;
    sst_sl *non_zero_l[6];
    int cached_levels;
    index_cache cache;
    sst_allocator sst_memory;
    uint64_t size_per_block;
    
}sst_manager;
sst_manager create_manager(uint64_t sst_size, uint64_t partition_size, uint64_t mem_size);
void add_sst(sst_manager * mana ,sst_f_inf sst, int level);
void remove_sst(sst_manager * mana, sst_f_inf * sst, int level);
sst_f_inf * get_sst(sst_manager * mana, const char * targ, int level);
void free_sst_man(sst_manager * man);
int check_sst(sst_manager * mana, sst_f_inf * inf, const char * targ, int level);
uint64_t get_num_l_0(sst_manager * mana);
block_index * get_block(sst_manager * mana, sst_f_inf * inf, const char * target, int level, int32_t part_ind );
void allocate_sst(sst_manager * mana, sst_f_inf * base, uint64_t num_keys);
sst_f_inf * get_sst(sst_manager * mana, const char * targ, int level);

#endif