#include "buffer_pool_stratgies.h"



int min_exp_from_bytes(uint64_t bucket_size) {
    return 63 - __builtin_clzll(bucket_size);
}
buffer_pool make_vm_mapped(uint64_t mem_size, size_tier_config config){
    buffer_pool pool;
    uint64_t curr = config.start;
    uint64_t iters =  ceil(log(config.stop / config.start) / log(config.multi));
    pool.vm.allocators = malloc(iters * sizeof(slab_allocator));
    for (int i = 0; i < iters; i++){
        pool.vm.allocators[i] = create_allocator(curr, ceil_int_div(curr, mem_size));
        curr *= config.multi;
    }
    pool.vm.config = config;
    pool.vm.config.min_exp = min_exp_from_bytes(config.min_exp);
}
buffer_pool make_buddy(uint64_t memsize,  size_tier_config config){
    buffer_pool pool;
    uint64_t iters =  ceil(log(config.stop / config.start) / log(config.multi));
    pool.pinned.pinned = mmap(NULL, memsize, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    pool.pinned.config = config;
    for (int i =  0; i < iters; i++){

    }
    return pool;
}
static inline int get_bucket_idx(uint64_t size, int min_exp, int step) {
    if (size <= (1ULL << min_exp)) return 0;

    int exp = 64 -__builtin_clzll(size - 1); 
    int rel_exp = exp - min_exp;
    int index = (rel_exp + step - 1) / step;  
    return index;
}
void * buddy_request_block(pinned_buddy_strategy pool, uint64_t size){
    
}
void * vm_request_block(vm_mapped_strategy pool, uint64_t size){
    uint8_t bucket=  get_bucket_idx(size, pool.config.min_exp, pool.config.multi);
    void  * chunk = slalloc(&pool.allocators[bucket], pool.allocators->pagesz);
    return chunk;
}
void vm_return_block(vm_mapped_strategy pool, uint64_t size, void * ptr){
    uint8_t bucket=  get_bucket_idx(size, pool.config.min_exp, pool.config.multi);
    slfree(&pool.allocators[bucket],ptr);
    madvise(ptr, pool.allocators[bucket].pagesz, MADV_DONTNEED);
}
buffer_pool make_b_p(uint8_t strategy,uint64_t mem_size, size_tier_config config){
    buffer_pool pool;
    pool.strat = strategy;
    switch (strategy){
        case FIXED :
            break;
        case VM_MAPPED:
            pool = make_vm_mapped(mem_size, config);
            break;
        case PINNED_BUDDY:
            pool = make_buddy()
            break;
    default:
        break;
    }
    return pool;
}