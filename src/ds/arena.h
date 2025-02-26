#include <stdlib.h>
#pragma once
#include <stdlib.h>
#include <string.h>

typedef struct arena {
    size_t size;
    size_t capacity;
    void *data;
    void *next;
} 
arena;
arena *malloc_arena(size_t size);
arena *calloc_arena(size_t size);
void reset_arena(void *va);
void *arena_alloc(arena *a, size_t size);
void free_arena(void *a);
