#include "structure_pool.h"
#include <stdint.h>
#include "../util/maths.h"
#include <sys/mman.h>
#include "slab.h"
#include "byte_buffer.h"
#include "buddy_page.h"
enum buffer_pool_strategies{
    FIXED,
    VM_MAPPED,
    PINNED_BUDDY
};
enum buddy_stat{
    FREE,
    ALLOC,
    SPLIT
};

typedef struct size_tier_config{
    uint32_t start;
    uint64_t multi;
    uint64_t stop;
    uint8_t min_exp;
}size_tier_config;
typedef struct fixed_strategy{
    struct_pool * four_kb;
} fixed_strategy;
typedef struct vm_mapped_strategy{
    slab_allocator * allocators;
    size_tier_config config;
}vm_mapped_strategy;
typedef struct pinned_buddy_strategy{
    void * pinned;
    buddy_allocator alloc;
    size_tier_config config;
}pinned_buddy_strategy;
typedef union b_p_strategy{
    pinned_buddy_strategy pinned;
    vm_mapped_strategy vm;
    fixed_strategy trad;
}b_p_strategy;
typedef struct buffer_pool{
    enum buffer_pool_strategies strat;
    union{
        pinned_buddy_strategy pinned;
        vm_mapped_strategy vm;
        fixed_strategy trad;
    };
    struct_pool * empty_buffers;
} buffer_pool;
buffer_pool make_b_p(uint8_t strategy,uint64_t mem_size, size_tier_config config);
void end_b_p(buffer_pool * pool);
byte_buffer *  get_buffer(buffer_pool pool, uint64_t size);
void return_buffer_strat(buffer_pool pool, byte_buffer * b);
