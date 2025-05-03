#include "sst_manager.h"

typedef struct partiton_cache_element{
    uint64_t off;
    uint64_t len;
    bloom_filter filter;
    block_index * block_indexs;
    uint16_t num_indexs;
    uint16_t ref_count;
}partiton_cache_element;

typedef struct index_cache {
    size_t capacity;
    size_t page_size;
    size_t filled_pages;
    partiton_cache_element *frames;
    uint8_t *ref_bits;
    size_t  clock_hand;
    slab_allocator memome;
    uint64_t max_pages;
} index_cache;

index_cache create_ind_cache(uint64_t sst_size, uint64_t partition_size, uint64_t mem_size){
    index_cache cache;
    const float overflow_mult = 1.25;
    uint64_t num_slabs = ceil_int_div(mem_size, partition_size);
    cache.capacity = mem_size;
    cache.filled_pages = 0;
    cache.clock_hand = 0;
    cache.frames = malloc(mem_size * sizeof(partiton_cache_element));
    cache.ref_bits = malloc(mem_size * sizeof(uint8_t));
    cache.memome = create_allocator(partition_size, num_slabs * overflow_mult);
    cache.max_pages = num_slabs;
    return cache;
} 
size_t ind_clock_evict(index_cache *c) {
    while (1) {
        size_t idx = c->clock_hand;
        partiton_cache_element  f = c->frames[idx];

        if (c->ref_bits[idx] == 0 && f.ref_count == 0) {
            slfree(&c->memome, f.block_indexs);
            slfree(&c->memome, f.filter.ba->array);
            slfree(&c->memome, f.filter.ba);
            f.num_indexs = 0;
            c->clock_hand = (c->clock_hand + 1) % c->capacity;
            return idx;
        }

        c->ref_bits[idx] = 0;
        c->clock_hand = (c->clock_hand + 1) % c->capacity;
    }
}
static size_t get_free_frame(index_cache *c) {
    if (c->filled_pages < c->max_pages) {
        return c->filled_pages++;
    }
    return clock_evict(c);
} 
partiton_cache_element * get_part(index_cache *c, sst_partition_ind * ind, const char * fn){
    if (ind->pg){
        return ind->pg;
    }
    db_FILE * targ = dbio_open(fn, 'r');
    byte_buffer * buffer = select_buffer(ind->len);
    set_context_buffer(targ, buffer);

    uint64_t read=  dbio_read(targ, ind->off, ind->len);
    if (read <= ind->len){
        dbio_close(targ);
        return NULL;
    }

    uint64_t free = get_free_frame(c);
    partiton_cache_element * ele=  &c->frames[free];

    ele->filter = part_from_stream(targ, &c->memome);
    ele->block_indexs = slalloc(&c->memome, ind->num_blocks * block_ind_size());
    for (int i = 0; i < ind->num_blocks; i++){
        block_from_stream(targ, &ele->block_indexs[i]);
    }
    ind->blocks = ele->block_indexs;
    ind->pg = ele;
    ele->ref_count ++;
    dbio_close(targ);
    return ele;
}
void free_ind_cache(index_cache * c){
    free(c->ref_bits);
    free_slab_allocator(c->memome);
}
void pin_part(sst_partition_ind * ind){
    partiton_cache_element * ele = ind->pg;
    ele->ref_count ++;
}
void unpin_part(sst_partition_ind * ind){
    partiton_cache_element * ele = ind->pg;
    ele->ref_count --;
    if (ele->ref_count <= 0) {
        ind->pg = NULL;
    }
}
sst_manager create_manager(uint64_t sst_size, uint64_t partition_size, uint64_t mem_size){
    sst_manager mana;
    mana.l_0 = List(32, sizeof(sst_f_inf), false);
    for (int i =0; i < 6; i++){
        mana.non_zero_l[i] = create_sst_sl(1024);
    }
    mana.cached_levels = 1;
    mana.cache = create_ind_cache(sst_size, partition_size, mem_size);
    return mana;
}
void free_manager(sst_manager * manager){
    free_ind_cache(&manager->cache);
    free_list(manager->l_0, NULL);
    for (int i = 0; i < 6; i++){
        freesst_sl(manager->non_zero_l[i]);
    }
}
void add_sst(sst_manager * mana ,sst_f_inf sst, int level){
    if (level <= 0){
        insert(mana->l_0, &sst);
    }
    else{
        sst_insert_list(mana->non_zero_l[level],sst );
    }
}
void remove_sst(sst_manager * mana, sst_f_inf * sst, int level){
    if (level <= 0){
        sst_f_inf * l = mana->l_0->arr;
        int found = false;
        int index = 0;
        for (;index < mana->l_0->len; index){
            if (&l[index] == sst){
                found = true;
                break;
            }
        }
        if (found == false ) return;
        remove_at(mana->l_0, index);
    }
    else{
        sst_delete_element(mana->non_zero_l[level], sst->min);
    }
}
size_t find_sst_file(list  *sst_files,  const char *key) {
    size_t max_index = sst_files->len;
    size_t min_index = 0;
    size_t middle_index;
    sst_f_inf * l = sst_files->arr;
    while (min_index < max_index) {
        middle_index = min_index + (max_index - min_index) / 2;
        sst_f_inf *sst = &l[middle_index];
        if (strcmp(key, sst->min) >= 0 && strcmp(key, sst->max) <= 0) {
            return middle_index;
        } 
        else if (strcmp(key, sst->max) > 0) {
            min_index = middle_index + 1;
        } 
        else {
            
            max_index = middle_index;
        }
    }
    return 0;
}
sst_f_inf * get_sst(sst_manager * mana, const char * targ, int level){
    if (level <= 0){
        size_t ind = find_sst_file(mana->l_0, targ);
        if (ind == 0) return NULL;
        else return at(mana->l_0, ind);
    }
    else{
        sst_node * node = sst_search_list_prefix(mana->non_zero_l[level], targ);
        if (node == NULL) return NULL;
        return &node->sst;
    }
}
void free_sst_man(sst_manager * man){
    free_list(man->non_zero_l,  &free_sst_inf);
    for (int i = 0; i < 6; i++){
        freesst_sl(man->non_zero_l[i]);
    }
}
size_t find_block(list * block_indexs, const char *key) {
    int left = 0;
    int right = block_indexs->len - 1;
    int mid = 0;
    while (left <= right) {
        mid = left + (right - left) / 2;
        block_index *index = at(block_indexs, mid);
        block_index *next_index = at(block_indexs, mid + 1);

        if (next_index == NULL || (strcmp(key, index->min_key) >= 0 && strcmp(key, next_index->min_key) < 0)) {
            return mid;
        }

        if (strcmp(key, index->min_key) < 0) {
            right = mid - 1;
        } 
        else {
            left = mid + 1;
        }
    }

    return mid;
}
size_t find_parition(sst_f_inf *sst, const char *key){
    int left = 0;
    int right = sst->block_indexs->len - 1;
    int mid = 0;
    while (left <= right) {
        mid = left + (right - left) / 2;
        sst_partition_ind *index = at(sst->sst_partitions, mid);
        sst_partition_ind *next_index = at(sst->sst_partitions, mid + 1);

        if (next_index == NULL || (strcmp(key, index->min_fence) >= 0 && strcmp(key, next_index->min_fence) < 0)) {
            return mid;
        }

        if (strcmp(key, index->min_fence) < 0) {
            right = mid - 1;
        } 
        else {
            left = mid + 1;
        }
    }

    return mid;
}
block_index * get_block(sst_manager * mana, sst_f_inf * inf, const char * target, int level, int32_t part_ind ){
    if (level <= mana->cached_levels){
        size_t ind = find_block(inf->block_indexs ,target);
        if (ind <=0 ) return NULL;
        return at(mana->l_0, ind);
    }
   
    size_t ind  = find_parition(inf, target);
    if (ind <=0 ) return NULL;
    sst_partition_ind * part = at(inf->sst_partitions, ind);
    if (!part->pg){
        part=  get_part(&mana->cache, part, inf->file_name);
    }
    ind = find_block(part->blocks, target);
    if (ind <=0) return NULL;
    return at(part->blocks, ind);
}
int check_sst(sst_manager * mana, sst_f_inf * inf, const char * targ, int level){
    if (level <= mana->cached_levels){
        bool might = check_bit(targ, inf->filter);
        return might;
    }
    size_t ind =  find_parition(inf, targ);
    sst_partition_ind * part = at(inf->sst_partitions, ind);
    bool might = check_bit(targ, part->filter);
    if (!might ) return might;
    return ind;
}
uint64_t get_num_l_0(sst_manager * mana){
    return mana->l_0->len;
}