#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include "byte_buffer.h"
#include "arena.h"
#include "hashtbl.h"
#include "structure_pool.h"
#include "associative_array.h"
#include "../db/backend/indexer.h"
#include "../db/backend/key-value.h"
#include "structure_pool.h"

#pragma once

typedef struct cache_entry {
    byte_buffer *buf;
    k_v_arr *ar;
    int ref_count;
} cache_entry;

typedef struct cache {
    size_t capacity;
    size_t page_size;
    size_t filled_pages;
    size_t max_pages;
    dict   *map;
    arena  *mem;
    pthread_mutex_t c_lock;
    cache_entry **frames;
    uint8_t *ref_bits;
    size_t  clock_hand;
} cache;

cache create_cache(size_t capacity, size_t page_size);
cache_entry* get_page(cache *c, const char *uuid);
cache_entry* retrieve_entry(cache *c, block_index *index, const char *file_name);
void pin_page(cache_entry *c);
void unpin_page(cache_entry *c);
void free_entry(void *entry);
void free_cache(cache *c);
