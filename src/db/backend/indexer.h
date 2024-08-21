#include <stdio.h>
#include<stdlib.h>
#include "../../ds/list.h"
#include "../../ds/bloomfilter.h"
#include "../../util/alloc_util.h"
#include "../../ds/arena.h"


#define NUM_HASH_BLOK 10
#define NUM_HASH_SST 10
#define MIN_KEY_SIZE 100
#define TIME_STAMP_SIZE 20
#define MAX_F_N_SIZE 32
#define MAX_LEVELS 7
#define BASE_LEVEL_SIZE 6400000
#define MIN_COMPACT_RATIO 50
#pragma once
/*
Each SST (Sorted String Table) will have its own Bloom filter and min/max key.
These indexes are created and managed by the indexer.

Index Details:
- The index will be sparse, with a default block size of 16KB, but it can be smaller.
- Indexes should be stored at the bottom of the SST file to maintain organization and efficiency.
- Indexes are created both during SST table building and compaction.

Read Path Hierarchy:
0. Cache
1. Indexer
2. First-level filter
3. SST file index
4. SST block Bloom filter/ checking min/max key
5. Reading the actual data

Index Structure:
- Each SST file will have:
  - A primary Bloom filter.
  - Pointers to block-specific indexes, which include their Bloom filter and the max key
  - Min/max key for the entire SST file.
- The sst_index will be a list of sorted block_indexs that can be binary searched

*/

typedef struct sst_file_info{
    char file_name[MAX_F_N_SIZE];
    //size_t id;
    size_t length;
    list * block_indexs; //type:  block_index
    char max[MIN_KEY_SIZE];
    char min[MIN_KEY_SIZE];
    char timestamp[TIME_STAMP_SIZE];
    size_t block_start;
    bloom_filter * filter;
    arena * mem_store;
}sst_f_inf;

typedef struct block_index{
    size_t offset; //a pointer to the starting location of the block index
    bloom_filter * filter;
    char *min_key;
    size_t len;
} block_index;



block_index * create_block_index(size_t est_num_keys);;
void free_block_index(void * index);


sst_f_inf *  create_sst_file_info(char * file_name, size_t len, size_t num_word, bloom_filter * b){
    sst_f_inf * file = (sst_f_inf*)wrapper_alloc(sizeof(sst_f_inf), NULL, NULL);
    file->block_indexs = List(0, sizeof(block_index), true);
    file->mem_store = calloc_arena(100000);
    file->length = len;
    file->block_start = -1;
    if( b != NULL) file->filter = b;
    else file->filter= bloom(NUM_HASH_SST,num_word, false, NULL);
    grab_time_char(file->timestamp);
    if (file_name == NULL) return file;
    strcpy(file->file_name, file_name);
    return file;
}
block_index * create_block_index(size_t est_num_keys){
    block_index * index = (block_index*)wrapper_alloc((sizeof(block_index)), NULL,NULL);
    index->filter = bloom(NUM_HASH_SST, est_num_keys, false, NULL);
    index->min_key = NULL;
    index->len = 0; 
    return index;
}
block_index * block_from_stream(byte_buffer * stream, block_index * index){
    read_buffer(stream, &index->offset, sizeof(size_t));
    index->filter = from_stream(stream);
    read_buffer(stream,index->min_key, 100);
    read_buffer(stream, &index->len, sizeof(size_t));
    return index;
}
void block_to_stream(byte_buffer * targ, block_index * index){
    write_buffer(targ, (char*)&index->offset, sizeof(size_t));
    copy_filter(index->filter, targ);
    write_buffer(targ,index->min_key,100);
    write_buffer(targ, (char*)&index->len, sizeof(size_t)); 
}
void read_index_block(sst_f_inf * file, byte_buffer * stream)
{
    FILE * f = fopen(file->file_name, "rb");
    if (f == NULL) return;
    fseek (f, 0, SEEK_END);
    size_t eof = ftell(f);
    fseek(f, file->block_start, SEEK_SET);
    int read = fread(stream->buffy, 1, eof - file->block_start, f);
    stream->curr_bytes = read;
    stream->read_pointer = 0;
    for (int i =0; i < file->block_indexs->len; i++){
        block_index * index = (block_index*)get_element(file->block_indexs, i);
        index->min_key = (char*)arena_alloc(file->mem_store, 100);
        block_from_stream(stream, index);
    }
    file->filter = from_stream(stream);
    fclose(f);
}

void all_index_stream(size_t num_table, byte_buffer* stream, list * indexes){
    for (int i = 0; i < num_table; i ++){
        block_index * index =(block_index*)get_element(indexes, i);
        block_to_stream(stream, index);
    }
}
void free_block_index(void * block){
    block_index * index = (block_index*)block;
    if (index == NULL) return;
    free_filter(index->filter);
    free(index->min_key);
    free(index);
}
void reuse_block_index(void * b){
    block_index * index = (block_index*)b;
    if (index == NULL) return;
    free_filter(index->filter);
    index->filter = NULL;
    index->min_key = NULL;
    index->len = 0;
}
