#include <stdio.h>
#include<stdlib.h>
#include "../../ds/byte_buffer.h"
#include "json.h"
#include "../../util/stringutil.h"
#include "../../util/maths.h"
#include "../../util/alloc_util.h"
#include "indexer.h"
#include "option.h"

#pragma once    
struct write_info{
    byte_buffer * table_store;
    byte_buffer * staging;
    size_t table_s;
    list * block_index;
    char * file;
};
void build_table_from_json_stream(struct write_info write, size_t num_table){
    write_buffer(write.staging, "{",1);
    size_t write_size = write.table_s;
    byte_buffer_transfer(write.table_store, write.staging, write_size);

    size_t last_entry = back_track(write.staging, ',', write.staging->curr_bytes, write.table_s);
    size_t difference = write.staging->curr_bytes - last_entry;
    memset(&write.staging->buffy[last_entry], 0, write.staging->curr_bytes - last_entry);
    write.staging->curr_bytes = last_entry;
    write.staging->buffy[write.staging->curr_bytes] = '}';
    write.table_store->read_pointer -=difference-1;
    return;
}
void grab_min_b_key(block_index * index, byte_buffer *b, int loc){
    u_int16_t key_len;
    memcpy(&key_len, &b->buffy[loc + 2], sizeof(u_int16_t));
    memcpy(index->min_key, &b->buffy[loc+4], key_len);
}
void build_index(sst_f_inf * sst, block_index * index, byte_buffer * b, size_t num_entries, size_t block_offsets){

    index->offset = block_offsets;
    index->uuid = arena_alloc(sst->mem_store,40);
    grab_uuid(index->uuid);
    u_int16_t key_len;
    if (b){
        index->min_key = (char*)arena_alloc(sst->mem_store, 40);
        memcpy(&key_len, &b->buffy[block_offsets + 2], sizeof(u_int16_t));
        memcpy(index->min_key, &b->buffy[block_offsets +4], key_len);
    } 
    index->num_keys = num_entries;
    insert(sst->block_indexs, index);  
}   
void free_sst_inf(void * ss){
    sst_f_inf * sst = (sst_f_inf*)ss;
    free_list(sst->block_indexs, &reuse_block_index);
    free_arena(sst->mem_store);
    sst->block_indexs = NULL;
    sst->mem_store = NULL;
    free_bit(sst->filter->ba);
    sst->filter = NULL;

    //free(sst);
}





