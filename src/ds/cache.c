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
/*implement a 256 vector verision or inline asm. we really dont want eviction as a bottleneck*/
size_t clock_evict(cache *c) {
    while (1) {
        size_t idx = c->clock_hand;
        cache_entry *  f = &c->frames[idx];

        if (c->ref_bits[idx] == 0 && f->ref_count == 0) {
            if (f->buf->utility_ptr) {
                remove_kv(c->map, f->buf->utility_ptr);
            }
            reset_buffer(f->buf);
            f->ar->len = 0;

            c->clock_hand = (c->clock_hand + 1) % c->max_pages;
            return idx;
        }

        c->ref_bits[idx] = 0;
        c->clock_hand = (c->clock_hand + 1) % c->max_pages;
    }
}
static size_t get_free_frame(cache *c) {
    if (c->filled_pages < c->max_pages) {
        return c->filled_pages++;
    }
    return clock_evict(c);
}
void handle_checksum_error(cache * c, db_FILE * sst_file, byte_buffer * buffer){
        remove_kv(c->map, buffer->utility_ptr);
        fprintf(stderr, "invalid data\n");
        dbio_close(sst_file);
}
cache_entry retrieve_entry_no_prefetch(cache *c, block_index *index, const char *file_name, sst_f_inf *sst) {
    void *val = get_v(c->map, index->uuid);
    empty.buf = NULL;
    if (val) {
        size_t idx = (size_t)(uintptr_t)val;
        c->ref_bits[idx] = 1;
        cache_entry ce = c->frames[idx];
        return ce;
    }
    db_FILE * sst_file =  dbio_open(file_name, 'r');

    if (!sst_file) {
        return empty;
    }
    size_t idx = get_free_frame(c);
    /*yes, this can include negative indexing! however, this is fine because
    it will never go out of bounds AND index points in contingous memory*/
    block_index * read_loc = get_pg_header(index);

    size_t relative_index = index->offset - read_loc->offset;
    /*len becomes aligned*/
    pin_page(&c->frames[idx]);

    byte_buffer *buffer = request_struct_idx(man->four_kb, idx);
    set_context_buffer(sst_file, buffer);

    size_t bytes_read = dbio_read(sst_file, relative_index, index->len);
    if (bytes_read == 0) return empty;
    byte_buffer * decompressed = NULL;
    int uuid_len = (int)strlen(index->uuid) + 1;

    if (use_compression(sst)){
        decompressed = request_struct(c->compression_buffers);
        buffer->curr_bytes = index->len + relative_index;
        b_seek(buffer, relative_index);
        handle_decompression(sst, buffer, decompressed);
        c->frames[idx].buf = decompressed;
        memcpy(&decompressed->buffy[decompressed->max_bytes - uuid_len], index->uuid, uuid_len);
        decompressed->utility_ptr = &decompressed->buffy[decompressed->max_bytes - uuid_len];
    }
    else{
        memcpy(&buffer->buffy[buffer->max_bytes - uuid_len], index->uuid, uuid_len);
        buffer->utility_ptr = &buffer->buffy[buffer->max_bytes - uuid_len];
    }
     
    c->ref_bits[idx] = 1;

    add_kv(c->map, buffer->utility_ptr, (void*)(uintptr_t)idx);
    
    if (!verify_data((uint8_t*)buffer->buffy, buffer->curr_bytes, index->checksum)) {
        handle_checksum_error(c, sst_file, buffer);
        c->frames[idx].ref_count --;
        if (decompressed) return_struct(c->compression_buffers, decompressed, &reset_buffer);
        return_struct_idx(man->four_kb, idx);
        return empty;
    }
    if (use_compression(sst)){
        load_block_into_into_ds(decompressed, c->frames[idx].ar, &into_array);
        c->frames[idx].buf = decompressed;
        c->frames[idx].idx= -1;
        c->frames[idx].loc = c->compression_buffers;
        return_struct_idx(man->four_kb, idx);
    }
    else{
        load_block_into_into_ds(buffer, c->frames[idx].ar, &into_array);
        c->frames[idx].buf = buffer;
        c->frames[idx].idx = idx;
    }
    dbio_close(sst_file);
    return c->frames[idx];
}

void pin_page(cache_entry *c) {
    __sync_fetch_and_add(&c->ref_count, 1);
}

void unpin_page(cache_entry *c) {
    __sync_fetch_and_sub(&c->ref_count, 1);
    if (c->ref_count <= 0 && c->idx > 0 ){
        return_struct_idx(man->four_kb, c->idx);
    }
    else{
        return_struct(c->loc, c->buf, &reset_buffer);
    }
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