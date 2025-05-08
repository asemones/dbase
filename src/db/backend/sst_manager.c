#include "sst_manager.h"
index_cache create_ind_cache(uint64_t sst_size, uint64_t partition_size, uint64_t mem_size){
    index_cache cache;
    const float overflow_mult = 1.25;
    uint64_t num_slabs = ceil_int_div(mem_size, partition_size);
    cache.capacity = mem_size;
    cache.filled_pages = 0;
    cache.clock_hand = 0;
    cache.frames = malloc(mem_size * sizeof(partiton_cache_element));
    cache.ref_bits = malloc(mem_size * sizeof(uint8_t));
    cache.max_pages = num_slabs;
    return cache;
}
size_t ind_clock_evict(index_cache *c) {
    while (1) {
        size_t idx = c->clock_hand;
        partiton_cache_element  f = c->frames[idx];

        if (c->ref_bits[idx] == 0 && f.ref_count == 0) {
            f.num_indexs = 0;
            c->clock_hand = (c->clock_hand + 1) % c->capacity;
            return idx;
        }

        c->ref_bits[idx] = 0;
        c->clock_hand = (c->clock_hand + 1) % c->capacity;
    }
}
block_index * allocate_blocks(uint64_t num, slab_allocator * block_allocator, uint64_t block_size){
    block_index * alloc = NULL;
    uint64_t tot = num * block_size;
    uint64_t fence_size = 40;
    alloc = slalloc(block_allocator, tot);
    for (int i  =0; i < num; i++){
        alloc[i].min_key = slalloc(block_allocator, fence_size);
    }
    return alloc;
}
bloom_filter * sst_make_bloom(uint64_t num_keys, uint64_t num_hash){
   uint8_t bits_per_word = 64;
   uint64_t required = num_keys * GLOB_OPTS.bits_per_key;
   uint64_t words = ceil_int_div(required, bits_per_word);
   return bloom(num_hash,words, false, NULL);
}
bb_filter sst_make_part_bloom(uint64_t num_keys){
    bb_filter f;
    bb_filter_init_capacity(&f,num_keys, GLOB_OPTS.bits_per_key);
    return f;
}
void allocate_sst(sst_manager * mana, sst_f_inf * base, uint64_t num_keys){
    *base = create_sst_filter(NULL);
    base->block_indexs->arr= allocate_blocks(ceil_int_div(base->length, GLOB_OPTS.BLOCK_INDEX_SIZE), &mana->sst_memory, mana->size_per_block);
    base->block_indexs->cap = ceil_int_div(base->length, GLOB_OPTS.BLOCK_INDEX_SIZE);
    base->filter = sst_make_bloom(num_keys, 2);
    base->file_name = slalloc(&mana->sst_memory.cached, MAX_F_N_SIZE); 
    base->max = slalloc(&mana->sst_memory.cached, MAX_KEY_SIZE);
    base->min = slalloc(&mana->sst_memory.cached, MAX_KEY_SIZE);
} 
sst_partition_ind * allocate_part(uint64_t num_keys, uint64_t num, sst_manager * mana){
    uint64_t keys_per_bloom = ceil_int_div(num_keys, num);
    sst_partition_ind * inds = slalloc(&mana->sst_memory.non_cached, sizeof(sst_partition_ind) * num);
    for (int i  =0; i < num; i++){
        inds->min_fence = slalloc(&mana->sst_memory.non_cached, 40);
        inds->filter = sst_make_part_bloom(num_keys);
        inds->pg = NULL;
        inds->num_blocks = 0;
        inds->blocks = NULL;
    }
    return inds;
}
void allocate_non_l0(sst_manager * mana, sst_f_inf * base, uint64_t num_keys, int level){
    *base = create_sst_filter(NULL);
    if (level <= mana->cached_levels ){
        return allocate_sst(mana, base,num_keys);
    }
    base->sst_partitions->arr = allocate_part(num_keys, 0, mana);
}
static size_t get_free_frame(index_cache *c) {
    if (c->filled_pages < c->max_pages) {
        return c->filled_pages++;
    }
    return ind_clock_evict(c);
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

    //ele->filter = part_from_stream(buffer, &c->memome);
    //ele->block_indexs = slalloc(&c->memome, ind->num_blocks * block_ind_size());
    for (int i = 0; i < ind->num_blocks; i++){
        block_from_stream(buffer, &ele->block_indexs[i]);
    }
    ind->blocks = ele->block_indexs;
    ind->pg = ele;
    ele->ref_count ++;
    dbio_close(targ);
    return ele;
}
void free_ind_cache(index_cache * c){
    free(c->ref_bits);
    //free_slab_allocator(c->memome);
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
uint64_t calculate_cached_size(){
    uint64_t blocks_per_sst =  ceil_int_div(GLOB_OPTS.SST_TABLE_SIZE, GLOB_OPTS.BLOCK_INDEX_SIZE);
    uint64_t size_from_blocks  =  blocks_per_sst * block_ind_size();
    uint64_t list_size=  sizeof(list);
    uint64_t base_size = MAX_KEY_SIZE * 2 + MAX_F_N_SIZE + sizeof(sst_f_inf);
    return list_size + base_size + size_from_blocks;
}
uint64_t  calculate_non_cached_size(){
    uint64_t blocks_per_part = ceil_int_div(GLOB_OPTS.partition_size, block_ind_size());
    uint64_t blocks_per_sst = ceil_int_div(GLOB_OPTS.SST_TABLE_SIZE, GLOB_OPTS.BLOCK_INDEX_SIZE);
    uint64_t parts_per_sst = ceil_int_div( blocks_per_sst, blocks_per_part);
    uint64_t list_size=  sizeof(list);
    uint64_t base_size = MAX_KEY_SIZE * 2 + MAX_F_N_SIZE + sizeof(sst_f_inf);
    uint64_t part_size = sizeof(sst_partition_ind) + 40;
    return (parts_per_sst * part_size) + base_size + list_size;
}
sst_allocator create_sst_all(){
    sst_allocator allocator;
    uint64_t cached_size = calculate_cached_size();
    uint64_t non_cached_size = calculate_non_cached_size();
    allocator.cached = create_allocator(cached_size, 1024);
    allocator.non_cached = create_allocator(non_cached_size, 1024);
    return allocator;
}
sst_manager create_manager(uint64_t sst_size, uint64_t partition_size, uint64_t mem_size){
    sst_manager mana;
    mana.l_0 = List(32, sizeof(sst_f_inf), false);
    for (int i =0; i < 6; i++){
        mana.non_zero_l[i] = create_sst_sl(1024);
    }
    mana.cached_levels = 1;
    mana.cache = create_ind_cache(sst_size, partition_size, mem_size);
    mana.size_per_block = block_ind_size();
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
        for (;index < mana->l_0->len; index ++ ){
            if (&l[index] == sst){
                found = true;
                break;
            }
        }
        if (found == false ) return;
        remove_at(mana->l_0, index);
        free_sst_inf(sst);
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
    free_list(man->l_0,  &free_sst_inf);
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
        partiton_cache_element * element=  get_part(&mana->cache, part, inf->file_name);
        part->blocks = element->block_indexs;
        part->filter = element->filter;
    }
    ind = find_block(part->blocks, target);
    if (ind <=0) return NULL;
    return &part->blocks[ind];
}
int check_sst(sst_manager * mana, sst_f_inf * inf, const char * targ, int level){
    if (level <= mana->cached_levels){
        bool might = check_bit(targ, inf->filter);
        return might;
    }
    size_t ind =  find_parition(inf, targ);
    sst_partition_ind * part = at(inf->sst_partitions, ind);
    bool might = bb_filter_may_contain(&part->filter, targ);
    if (!might ) return might;
    return ind;
}
uint64_t get_num_l_0(sst_manager * mana){
    return mana->l_0->len;
}