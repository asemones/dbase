#define GNU_SOURCE
#ifndef SLAB_H
#define SLAB_H
#include <sys/mman.h>
#include <stdint.h>
#include <stddef.h>
#include <stdalign.h>
#include <stdio.h>
typedef struct chunk {
    struct chunk *next;
} chunk;

typedef struct slab {
    struct slab *next;      
    chunk*local_free; 
    int in_pool;    
    alignas(64) char mem[]; 
} slab;


typedef struct slab_allocator {
    slab *free_list;       
    slab *curr;         
    size_t slab_size;       
    size_t stride;          
    size_t curr_off;       
    char *base; 
} slab_allocator;

slab_allocator create_allocator(size_t slab_size, size_t slab_count) {
    size_t header = offsetof(slab, mem);
    size_t stride = (header + slab_size + 63) & ~(size_t)63;
    size_t total  = stride * slab_count;
    if (total < (1 << 20)) total = (1 << 20);

    char *base = mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS , -1, 0);

    slab *free_list = NULL;
    for (size_t i = 0; i < slab_count; ++i) {
        slab *s = (slab *)(base + i * stride);
        s->next        = free_list;
        s->local_free  = NULL;
        s->in_pool     = 1;
        free_list      = s;
    }
    slab_allocator A = {
        .free_list       = free_list,
        .curr            = NULL,
        .slab_size       = slab_size,
        .stride          = stride,
        .curr_off        = 0,
        .base            = base,
    };
    return A;
}
static inline void *slalloc(slab_allocator *A, size_t size) {
    if (A->curr && A->curr->local_free) {
        chunk *c = A->curr->local_free;
        A->curr->local_free = c->next;
        return (void*)c;
    }
    if (!A->curr || A->curr_off + size > A->slab_size) {
        slab *s = A->free_list;
        if (!s) return NULL;
        A->free_list = s->next;
        s->in_pool    = 0;
        A->curr       = s;
        A->curr_off   = 0;
    }
    void *p = &A->curr->mem[A->curr_off];
    A->curr_off += size;
    return p;
}
static inline void slfree(slab_allocator *A, void *ptr) {
    uintptr_t offset = (uintptr_t)ptr - (uintptr_t)A->base;
    slab *s = (slab*)(A->base + (offset / A->stride) * A->stride);
    chunk *c = (chunk*)ptr;
    c->next = s->local_free;
    s->local_free = c;
    if (!s->in_pool) {
        s->next      = A->free_list;
        A->free_list = s;
        s->in_pool   = 1;
    }
}
static inline void free_slab_allocator(slab_allocator A){
    munmap(A.base, A.slab_size);
}
#endif
