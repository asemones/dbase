#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <time.h>
#include <stdio.h>
#include "structure_pool.h"
#include "../util/maths.h"
#define HARD_MAX_ORDER  30        
#define ORDER_COUNT     (HARD_MAX_ORDER + 1)
struct freelink { 
    uint32_t next;
    uint32_t prev; 
};
struct fl_head { 
    uint32_t next;
    uint32_t prev; 
};
typedef struct hot_cache{
    uint64_t order;
    struct_pool queue;
}hot_cache;
typedef struct buddy_allocator {
    uintptr_t base;          
    uint64_t available_mask; 
    hot_cache caches[4];
    uint64_t hot_cache_mask;
    struct fl_head  head[ORDER_COUNT];
    uint64_t arena_end;     
    uint8_t min_order;      
    uint8_t max_order;      
} buddy_allocator;
void buddy_init(buddy_allocator *ba, void *heap, size_t heap_size, uint8_t min_order, uint8_t max_order);
void init_hot_caches(buddy_allocator *ba, uint64_t * orders, uint8_t num, double * workload_size_per);
void *buddy_alloc(buddy_allocator *ba, size_t size);
void buddy_free(buddy_allocator *ba, void *ptr);
