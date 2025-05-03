#include "indexer.h"
#include "../../ds/sst_sl.h"
#include "../../ds/slab.h"
#include "../../ds/list.h"
#include "../../ds/circq.h"
#include "../../util/maths.h"
typedef struct index_cache;
typedef struct sst_manager{
    list * l_0;
    sst_sl *non_zero_l[6];
    int cached_levels;
    index_cache cache;
}sst_manager;
sst_manager create_manager(uint64_t sst_size, uint64_t partition_size, uint64_t mem_size);
void add_sst(sst_manager * mana ,sst_f_inf sst, int level);
void remove_sst(sst_manager * mana, sst_f_inf * sst, int level);
sst_f_inf * get_sst(sst_manager * mana, const char * targ, int level);
void free_sst_man(sst_manager * man);
int check_sst(sst_manager * mana, sst_f_inf * inf, const char * targ, int level);
uint64_t get_num_l_0(sst_manager * mana);
block_index * get_block(sst_manager * mana, sst_f_inf * inf, const char * target, int level, int32_t part_ind );
