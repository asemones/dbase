#include "cache.h"
#define OVER_FLOW_EXTRA 100
#define NUM_COMPRESSION_BUF 20
cache_entry empty;
static cache_entry create_cache_entry(size_t page_size, size_t num_keys, arena *a) {
    cache_entry entry;
    entry.buf = NULL;
    entry.ar  = (k_v_arr*)arena_alloc(a, sizeof(k_v_arr));
    entry.ar->keys   = arena_alloc(a, num_keys * sizeof(db_unit));
    entry.ar->values = arena_alloc(a, num_keys * sizeof(db_unit));
    entry.ar->cap    = num_keys;
    entry.ar->len    = 0;
    entry.ref_count  = 0;
    return entry;
}
void back_cache(struct io_manager * m, cache * c){
    
}
cache create_cache(size_t capacity, size_t page_size) {
    cache c;
    c.capacity     = capacity;
    c.page_size    = page_size;
    c.filled_pages = 0;
    c.map          = Dict();
    c.mem          = calloc_arena(capacity * 2);
    c.max_pages = capacity / (page_size + sizeof(byte_buffer) + sizeof(cache_entry)
                  + OVER_FLOW_EXTRA + (sizeof(db_unit)) * 2);
    c.frames     = (cache_entry*)malloc(sizeof(cache_entry) * c.max_pages);
    c.ref_bits   = (uint8_t*)calloc(c.max_pages, sizeof(uint8_t));
    c.clock_hand = 0;

    for (size_t i = 0; i < c.max_pages; i++) {
        c.frames[i]   = create_cache_entry(page_size, page_size / 16, c.mem);
        c.ref_bits[i] = 0;
    }
    c.compression_buffers = create_pool(NUM_COMPRESSION_BUF);
    for (int i = 0; i < NUM_COMPRESSION_BUF; i++){
        insert_struct(c.compression_buffers, create_buffer(page_size));
    }
    pthread_mutex_init(&c.c_lock, NULL);
    return c;
}

size_t clock_evict(cache *c) {
    while (1) {
        size_t idx = c->clock_hand;
        if (c->ref_bits[idx] == 0 && c->frames[idx].ref_count == 0) {
            if (c->frames[idx].buf->utility_ptr) {
                remove_kv(c->map, c->frames[idx].buf->utility_ptr);
            }
            reset_buffer(c->frames[idx].buf);
            c->frames[idx].ar->len = 0;
            c->clock_hand = (c->clock_hand + 1) % c->max_pages;
            return idx;
        } else {
            c->ref_bits[idx] = 0;
            c->clock_hand = (c->clock_hand + 1) % c->max_pages;
        }
    }
}

static inline size_t get_free_frame(cache *c) {
    if (c->filled_pages < c->max_pages) {
        return c->filled_pages++;
    }
    return clock_evict(c);
}
cache_entry * retrieve_entry(cache *c, block_index *index, const char *file_name, sst_f_inf *sst) {
    void *val = get_v(c->map, index->uuid);
    empty.buf = NULL;
    if (val) {
        size_t idx = (size_t)(uintptr_t)val;
        //pthread_mutex_lock(&c->c_lock);
        c->ref_bits[idx] = 1;
        //pthread_mutex_unlock(&c->c_lock);
        cache_entry ce = c->frames[idx];
        return ce;
    }
    FILE *sst_file = fopen(file_name, "rb");
    if (!sst_file) {
        return empty;
    }
    if (fseek(sst_file, index->offset, SEEK_SET) < 0) {
        fclose(sst_file);
        return empty;
    }
    //pthread_mutex_lock(&c->c_lock);
    size_t idx = get_free_frame(c);
    byte_buffer * compression_buf = request_struct(c->compression_buffers);
    //pthread_mutex_unlock(&c->c_lock);
    cache_entry ce = c->frames[idx];
    byte_buffer *buffer = ce.buf;
    if (sst->compressed_len > 0 ){
        compression_buf->curr_bytes = read_wrapper(compression_buf->buffy, 1, index->len, sst_file);
    }
    else{
        buffer->curr_bytes = read_wrapper(compression_buf->buffy, 1, index->len, sst_file);
    }
    if (GLOB_OPTS.compress_level > -5){
        handle_decompression(sst, compression_buf, buffer);
    }    
    /*read COMPRESSED LENGTH*/

    int uuid_len = (int)strlen(index->uuid) + 1;
    if (uuid_len <= (int)buffer->max_bytes) {
        memcpy(&buffer->buffy[buffer->max_bytes - uuid_len], index->uuid, uuid_len);
        buffer->utility_ptr = &buffer->buffy[buffer->max_bytes - uuid_len];
    } 
    else {
        buffer->utility_ptr = NULL;
    }
    //pthread_mutex_lock(&c->c_lock);
    c->ref_bits[idx] = 1;
    return_struct(c->compression_buffers, compression_buf, &reset_buffer);
    if (buffer->utility_ptr) {
        add_kv(c->map, buffer->utility_ptr, (void*)(uintptr_t)idx);
    }
    /*race condition, c->frames[idx] gets overwritten*/
    if (!verify_data((uint8_t*)buffer->buffy, buffer->curr_bytes, index->checksum)) {
        remove_kv(c->map, buffer->utility_ptr);
        fprintf(stderr, "invalid data\n");
        fprintf(stderr, "block offset %ld filename %s cache size %d\n", index->offset, file_name, c->filled_pages);
        printf("Checking block checksum: %u for len: %zu at offset: %ld\n", 
        index->checksum, buffer->curr_bytes, index->offset);
        uint32_t computed_checksum = crc32((uint8_t*)buffer->buffy, buffer->curr_bytes);
        printf("Computed: %u, Original: %u, Match: %d\n", 
        computed_checksum, index->checksum, computed_checksum == index->checksum);
        fclose(sst_file);
        //pthread_mutex_unlock(&c->c_lock);
        return empty;
    }
    //pthread_mutex_unlock(&c->c_lock);
    load_block_into_into_ds(buffer, ce.ar, &into_array);
    fclose(sst_file);
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
        free_entry(&c->frames[i]);
    }
    free(c->frames);
    free(c->ref_bits);
    free_arena(c->mem);
    free_dict_no_element(c->map);
    pthread_mutex_destroy(&c->c_lock);
}