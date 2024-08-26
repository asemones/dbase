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

void add_table_to_bloom(bloom_filter * filter, byte_buffer * staging){
    go_nearest_v(staging, '{');
    char temp_store[100];
    while(staging->read_pointer < staging->curr_bytes){
        int stat = get_next_key(staging, temp_store);
        if (stat<=1 ) break;
        map_bit(temp_store, filter);       
    }
    return;
}
void get_max_key(sst_f_inf * sst, byte_buffer * table, size_t table_size){
    table->read_pointer =  back_track(table, ',', table->curr_bytes, table_size);
    get_next_key(table,&sst->max[0]);
    return;
}
void build_index(sst_f_inf * sst, size_t block_offsets, size_t num_tables, struct write_info info){
   
    block_index * index = (block_index*)get_element(sst->block_indexs, num_tables);
    if (index == NULL){
        size_t multipler = info.table_s/(GLOB_OPTS.BLOCK_INDEX_SIZE);
        index = create_block_index(9 *multipler);
        index->offset = block_offsets;
        index->min_key = (char*)arena_alloc(sst->mem_store, 40);
        get_next_key(info.staging,index->min_key);
        index->len = info.staging->curr_bytes+1;
        insert(sst->block_indexs, index);
    }
    else{
        index->offset = block_offsets;
        index->min_key = (char*)arena_alloc(sst->mem_store, 40);
        get_next_key(info.staging,index->min_key);
        index->len= info.staging->curr_bytes+1;
    }
    info.staging->read_pointer = 0;
    add_table_to_bloom(index->filter, info.staging);
     
}   
int build_and_write_all_tables(struct write_info info, sst_f_inf * sst){
    size_t iter= 0;
    size_t num_table = 0;
    size_t block_offsets =0;
    FILE * f = fopen(info.file, "wb");
    size_t tot = 0;
    info.table_store->read_pointer = 0;
    get_next_key(info.table_store,sst->min);
    info.table_store->read_pointer = 0;
    //better locating the offset of the blocks as they are consistently misaligned
    while (iter < info.table_store->curr_bytes){
        block_offsets =tot;
        build_table_from_json_stream(info, num_table);
        go_nearest_v(info.staging, '}');
        tot += info.staging->read_pointer+1;
        info.staging->read_pointer= 0;
        build_index(sst, block_offsets, num_table, info);
        fwrite(info.staging->buffy, 1, info.staging->curr_bytes+1,f);
        block_index * block = (block_index*)get_element(sst->block_indexs, num_table);
        block->len = info.staging->curr_bytes+1;
        info.staging->curr_bytes = 0;
        iter += info.table_s;
        num_table++;
        reset_buffer(info.staging);
    }
    info.table_store->read_pointer = info.table_store->curr_bytes;
    info.table_store->buffy[info.table_store->curr_bytes-1] = '}';
    size_t index = back_track(info.table_store, ',', info.table_store->curr_bytes,info.table_store->curr_bytes);
    info.table_store->read_pointer = index;
    get_next_key(info.table_store,sst->max);
    sst->block_start = tot+10;
    reset_buffer(info.staging);
    for (int i = 0; i < num_table; i++){
        block_index * block = (block_index*)get_element(sst->block_indexs, i);
        block_to_stream(info.staging, block);
    }
    
    grab_time_char(sst->timestamp);
    fseek(f, tot+10, SEEK_SET);
    copy_filter(sst->filter, info.staging);
    fwrite(info.staging->buffy, 1, info.staging->curr_bytes,f);
    fflush(f);
    return num_table;
}
void read_a_table(FILE * f, byte_buffer * staging, size_t table_s){
    fread(staging->buffy, 1, table_s, f);
    staging->curr_bytes = table_s;
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





