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
void free_vm_mapped( buffer_pool * pool ){
    size_tier_config config = pool->vm.config;
    uint64_t iters =  ceil(log(config.stop / config.start) / log(config.multi));
    for (int i = 0; i < iters; i++){
        free_slab_allocator(pool->vm.allocators[i]);
    }
}
void free_buddy(pinned_buddy_strategy pool){
    munmap(pool.alloc.base, pool.alloc.arena_end - pool.alloc.base);

}
buffer_pool make_buddy(uint64_t memsize,  size_tier_config config){
    buffer_pool pool;
    uint64_t iters =  ceil(log(config.stop / config.start) / log(config.multi));
    pool.pinned.pinned = mmap(NULL, memsize, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    pool.pinned.config = config;
    uint64_t curr = config.start;
    buddy_init(&pool.pinned.alloc, pool.pinned.pinned, memsize, log2(config.start), log2(config.stop));
    double point_lookup_start = .4f;
    init_hot_caches(&pool.pinned.alloc, &config.start, 1, &point_lookup_start);

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
    return buddy_alloc(&pool.alloc, size);
}
void buddy_return_block(pinned_buddy_strategy pool, void * ptr){
    buddy_free(&pool.alloc,ptr);
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
byte_buffer *  get_buffer(buffer_pool pool, uint64_t size){
    byte_buffer b;
    b.curr_bytes  = 0;
    b.read_pointer = 0;
    b.max_bytes = size;
    if (pool.strat == VM_MAPPED){
        b.buffy = vm_request_block(pool.vm, size);
    }
    else if (pool.strat == PINNED_BUDDY){
        b.buffy = buddy_request_block(pool.pinned, size);
    }
    byte_buffer * actual = request_struct(pool.empty_buffers);
    *actual = b;
    return actual;
}
void return_buffer_strat(buffer_pool pool, byte_buffer * b){
    if (pool.strat == VM_MAPPED){
        vm_return_block(pool.vm, b->max_bytes, b->buffy);
    }
    else if (pool.strat == PINNED_BUDDY){
        buddy_return_block(pool.pinned, b->buffy);
    }
    return_struct(pool.empty_buffers, b, &reset_buffer);
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
            pool = make_buddy(mem_size, config);
            break;
        default:
            break;
    }
    if (pool.strat != FIXED){
        uint64_t res = ceil_int_div(mem_size, config.start);
        pool.empty_buffers = create_pool(res);
        for (int i = 0; i < res; i++){
            insert_struct(pool.empty_buffers,create_empty_buffer());
        }
    }
    return pool;
}
void end_b_p(buffer_pool * pool){
    switch (pool->strat){
        case FIXED :
            break;
        case VM_MAPPED:
            free_buddy(pool->pinned);
            break;
        case PINNED_BUDDY:
            free_vm_mapped(&pool->vm);
            break;
        default:
            break;
    }
}