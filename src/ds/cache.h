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

#define OVER_FLOW_EXTRA 100

#pragma once

typedef struct cache_entry {
    byte_buffer *buf;
    k_v_arr     *ar;
    int          ref_count;
} cache_entry;

typedef struct cache {
    size_t         capacity;
    size_t         page_size;
    size_t         filled_pages;
    size_t         max_pages;
    dict          *map;
    arena         *mem;
    pthread_mutex_t c_lock;
    cache_entry  **frames;
    uint8_t       *ref_bits;
    size_t         clock_hand;
} cache;

size_t combine_values(size_t level, size_t sst_ind, size_t block_index) {
    return (level << 16) | (sst_ind << 8) | block_index;
}

static cache_entry* create_cache_entry(size_t page_size, size_t num_keys, arena *a) {
    cache_entry *entry = (cache_entry*)arena_alloc(a, sizeof(*entry));
    entry->buf = create_buffer(page_size + OVER_FLOW_EXTRA);
    entry->ar  = (k_v_arr*)arena_alloc(a, sizeof(k_v_arr));
    entry->ar->keys   = arena_alloc(a, num_keys * sizeof(db_unit));
    entry->ar->values = arena_alloc(a, num_keys * sizeof(db_unit));
    entry->ar->cap    = num_keys;
    entry->ar->len    = 0;
    entry->ref_count  = 0;
    return entry;
}

cache* create_cache(size_t capacity, size_t page_size) {
    cache *c = (cache*)malloc(sizeof(cache));
    c->capacity     = capacity;
    c->page_size    = page_size;
    c->filled_pages = 0;
    c->map          = Dict();
    c->mem          = calloc_arena(capacity * 2);
    c->max_pages = capacity / (page_size + sizeof(byte_buffer) + sizeof(cache_entry)
                  + OVER_FLOW_EXTRA + (sizeof(db_unit)) * 2);
    c->frames     = (cache_entry**)malloc(sizeof(cache_entry*) * c->max_pages);
    c->ref_bits   = (uint8_t*)calloc(c->max_pages, sizeof(uint8_t));
    c->clock_hand = 0;

    for (size_t i = 0; i < c->max_pages; i++) {
        c->frames[i]   = create_cache_entry(page_size, page_size / 16, c->mem);
        c->ref_bits[i] = 0;
    }
    pthread_mutex_init(&c->c_lock, NULL);
    return c;
}

size_t clock_evict(cache *c) {
    while (1) {
        size_t idx = c->clock_hand;
        if (c->ref_bits[idx] == 0 && c->frames[idx]->ref_count == 0) {
            if (c->frames[idx]->buf->utility_ptr) {
                remove_kv(c->map, c->frames[idx]->buf->utility_ptr);
            }
            reset_buffer(c->frames[idx]->buf);
            c->frames[idx]->ar->len = 0;
            c->clock_hand = (c->clock_hand + 1) % c->max_pages;
            return idx;
        } else {
            c->ref_bits[idx] = 0;
            c->clock_hand = (c->clock_hand + 1) % c->max_pages;
        }
    }
}

static size_t get_free_frame(cache *c) {
    if (c->filled_pages < c->max_pages) {
        return c->filled_pages++;
    }
    return clock_evict(c);
}
cache_entry* get_page(cache *c, const char *uuid) {
    pthread_mutex_lock(&c->c_lock);
    void *val = get_v(c->map, uuid);
    if (!val) {
        pthread_mutex_unlock(&c->c_lock);
        return NULL;
    }
    size_t idx = (size_t)(uintptr_t)val;
    c->ref_bits[idx] = 1;
    cache_entry *entry = c->frames[idx];
    pthread_mutex_unlock(&c->c_lock);
    return entry;
}

cache_entry* retrieve_entry(cache *c, block_index *index, const char *file_name) {
    void *val = get_v(c->map, index->uuid);
    if (val) {
        size_t idx = (size_t)(uintptr_t)val;
        pthread_mutex_lock(&c->c_lock);
        c->ref_bits[idx] = 1;
        pthread_mutex_unlock(&c->c_lock);
        cache_entry *ce = c->frames[idx];
        return ce;
    }
    FILE *sst_file = fopen(file_name, "rb");
    if (!sst_file) {
        return NULL;
    }
    if (fseek(sst_file, index->offset, SEEK_SET) < 0) {
        fclose(sst_file);
        return NULL;
    }
    pthread_mutex_lock(&c->c_lock);
    size_t idx = get_free_frame(c);
    pthread_mutex_unlock(&c->c_lock);
    cache_entry *ce = c->frames[idx];
    byte_buffer *buffer = ce->buf;
    buffer->curr_bytes = fread(buffer->buffy, 1, index->len, sst_file);

    int uuid_len = (int)strlen(index->uuid) + 1;
    if (uuid_len <= (int)buffer->max_bytes) {
        memcpy(&buffer->buffy[buffer->max_bytes - uuid_len], index->uuid, uuid_len);
        buffer->utility_ptr = &buffer->buffy[buffer->max_bytes - uuid_len];
    } 
    else {
        buffer->utility_ptr = NULL;
    }
    pthread_mutex_lock(&c->c_lock);
    c->ref_bits[idx] = 1;
    if (buffer->utility_ptr) {
        add_kv(c->map, buffer->utility_ptr, (void*)(uintptr_t)idx);
    }
    pthread_mutex_unlock(&c->c_lock);
    if (!verify_data((uint8_t*)buffer->buffy, buffer->curr_bytes, index->checksum)) {
        pthread_mutex_lock(&c->c_lock);
        remove_kv(c->map, buffer->utility_ptr);
        fprintf(stderr, "invalid data\n");
        fclose(sst_file);
        pthread_mutex_unlock(&c->c_lock);
        return NULL;
    }
    load_block_into_into_ds(buffer, ce->ar, &into_array);
    fclose(sst_file);
    pthread_mutex_unlock(&c->c_lock);
    return ce;
}

void pin_page(cache_entry *c) {
    __sync_fetch_and_add(&c->ref_count, 1);
}

void unpin_page(cache_entry *c) {
    __sync_fetch_and_sub(&c->ref_count, 1);
}

void free_entry(void *entry) {
    cache_entry *c = (cache_entry*)entry;
    free_buffer(c->buf);
}

void free_cache(cache *c) {
    if (!c) return;
    for (size_t i = 0; i < c->max_pages; i++) {
        free_entry(c->frames[i]);
    }
    free(c->frames);
    free(c->ref_bits);
    free_arena(c->mem);
    free_dict_no_element(c->map);
    pthread_mutex_destroy(&c->c_lock);
    free(c);
}
