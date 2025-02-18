#include <stdio.h>
#include<stdlib.h>
#include <stdint.h>
#include "../../ds/list.h"
#include "../../ds/bloomfilter.h"
#include "../../util/alloc_util.h"
#include "../../ds/arena.h"
#include "../../ds/checksum.h"
#include "../../util/error.h"
#include <time.h>


#define NUM_HASH_BLOK 10
#define NUM_HASH_SST 10
#define MAX_KEY_SIZE 100
#define TIME_STAMP_SIZE 32
#define MAX_F_N_SIZE 64
#define MAX_LEVELS 7
#define BASE_LEVEL_SIZE 6400000
#define MIN_COMPACT_RATIO 50
#define MEM_SIZE 100000
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
    char max[MAX_KEY_SIZE];
    char min[MAX_KEY_SIZE];
    struct timeval time;
    size_t block_start;
    bloom_filter * filter;
    arena * mem_store;
    bool marked;
    bool in_cm_job;
}sst_f_inf;

typedef struct block_index{
    size_t offset; //a pointer to the starting location of the block index
    bloom_filter * filter;
    char *min_key;
    size_t len;
    size_t num_keys;
    char * uuid;
    uint32_t checksum;
    
} block_index;



block_index * create_block_index(size_t est_num_keys);
void free_block_index(void * index);

sst_f_inf create_sst_empty(){
    sst_f_inf file;
    file.block_indexs = List(0, sizeof(block_index), true);
    file.mem_store = calloc_arena(MEM_SIZE);
    file.length = 0;
    file.block_start = -1;
    file.filter = bloom(NUM_HASH_SST,2000, false, NULL);
    file.marked = false;
    file.in_cm_job = false;
    gettimeofday(&file.time, NULL);
    return file;

}
sst_f_inf create_sst_filter(bloom_filter * b){
    sst_f_inf file;
    file.block_indexs = List(0, sizeof(block_index), true);
    file.mem_store = calloc_arena(MEM_SIZE);
    file.length = 0;
    file.block_start = -1;
    file.filter = b;
    file.marked = false;
    file.in_cm_job = false;
    gettimeofday(&file.time, NULL);
    return file;

}
sst_f_inf *  create_sst_file_info(char * file_name, size_t len, size_t num_word, bloom_filter * b){
    sst_f_inf * file = (sst_f_inf*)wrapper_alloc(sizeof(sst_f_inf), NULL, NULL);
    if (file == NULL) return NULL;
    file->block_indexs = List(0, sizeof(block_index), true);
    file->mem_store = calloc_arena(MEM_SIZE);
    file->length = len;
    file->block_start = -1;
    if( b != NULL) file->filter = b;
    else file->filter= bloom(NUM_HASH_SST,num_word, false, NULL);
    if (file->filter == NULL || file->mem_store == NULL || file->block_indexs == NULL){
        if (file->filter!= NULL){
            free_filter(file->filter);
        }
        if (file->mem_store != NULL){
            free_arena(file->mem_store);
        }
        if (file->block_indexs != NULL){
            free_list(file->block_indexs, NULL);
        }
        free(file);
        return NULL;
    }
    gettimeofday(&file->time, NULL);
    if (file_name == NULL) return file;
    strcpy(file->file_name, file_name);
    file->marked = false;
    file->in_cm_job = false;
    return file;
}
int sst_deep_copy(sst_f_inf * master, sst_f_inf * copy){
    if (master == NULL) return 0;
    memcpy(copy, master, sizeof(sst_f_inf));
    copy->mem_store = calloc_arena(MEM_SIZE);
    if (copy->mem_store == NULL){
        return STRUCT_NOT_MADE;
    }
    copy->block_indexs = List(0, sizeof(block_index),false);
    for (int i = 0; i < master->block_indexs->len; i++){
        block_index * master_b = at(copy->block_indexs, i);
        if (master_b == NULL) continue;
        block_index ind;
        ind.min_key = arena_alloc(copy->mem_store, 40);
        ind.uuid = arena_alloc(copy->mem_store, 32);
        ind.checksum =master_b->checksum;
        ind.len = master_b->len;
        ind.num_keys = master_b->num_keys;
        ind.offset = master_b->offset;
        memcpy(ind.min_key, master_b->min_key, 40);
        memcpy(ind.uuid,master_b->uuid, 32);
        ind.filter = deep_copy_bloom_filter(master->filter);
        if (ind.filter == NULL){
            return STRUCT_NOT_MADE;
        }
        insert(copy->block_indexs, &ind);
    }
    copy->marked = master->marked;
    copy->in_cm_job = master->in_cm_job;
    return 0;
}
block_index * create_block_index(size_t est_num_keys){
    block_index * index = (block_index*)wrapper_alloc((sizeof(block_index)), NULL,NULL);
    if (index== NULL) return NULL;
    index->filter = bloom(NUM_HASH_SST, est_num_keys, false, NULL);
    if (index->filter == NULL){
        free(index);
        return NULL;
    }
    index->min_key = NULL;
    index->len = 0; 
    index->num_keys =0;
    index->uuid = NULL;
    return index;
}
block_index create_ind_stack(size_t  est_num_keys){
    block_index index;
    index.filter = bloom(NUM_HASH_SST, est_num_keys, false, NULL);
    index.min_key = NULL;
    index.len = 0; 
    index.num_keys  =0;
    index.uuid =  NULL;
    return index;
}
block_index * block_from_stream(byte_buffer * stream, block_index * index){
    read_buffer(stream, &index->offset, sizeof(size_t));
    index->filter = from_stream(stream);
    if (index->filter == NULL){
        return NULL;
    } 
    read_buffer(stream,index->min_key, 40);
    read_buffer(stream, index->uuid, 40);
    read_buffer(stream, &index->len, sizeof(size_t));
    read_buffer(stream, &index->num_keys, sizeof(index->num_keys));
    read_buffer(stream, &index->checksum, sizeof(index->checksum));
    return index;
}
int block_to_stream(byte_buffer * targ, block_index * index){
    int result = write_buffer(targ, (char*)&index->offset, sizeof(size_t));
    copy_filter(index->filter, targ);
    result = write_buffer(targ,index->min_key,40);
    result = write_buffer(targ,index->uuid, 40);
    result = write_buffer(targ, (char*)&index->len, sizeof(size_t)); 
    result = write_buffer(targ, &index->num_keys, sizeof(&index->num_keys));
    result = write_buffer(targ, &index->checksum, sizeof(index->checksum));
    return result;
}
int read_index_block(sst_f_inf * file, byte_buffer * stream)
{
    FILE * f = fopen(file->file_name, "rb");
    if (f == NULL) {
        return SST_F_INVALID;
    }
    int ret = fseek (f, 0, SEEK_END);
    if (ret  < 0 ) return FAILED_SEEK;
    size_t eof = ftell(f);
    ret = fseek(f, file->block_start, SEEK_SET);
    if (ret < 0 ) return FAILED_SEEK;
    int read = fread(stream->buffy, 1, eof - file->block_start, f);
    if (read != eof - file->block_start){
        return FAILED_READ;
    }
    stream->curr_bytes = read;
    stream->read_pointer = 0;
    for (int i =0; i < file->block_indexs->len; i++){
        block_index * index = (block_index*)at(file->block_indexs, i);
        index->min_key = (char*)arena_alloc(file->mem_store, 40);
        index->uuid = (char*)arena_alloc(file->mem_store, 40);
        if (index->min_key == NULL || index->uuid == NULL){
            return FAILED_ARENA;
        }
        block_index * ret = block_from_stream(stream, index);
        if (ret == NULL){
            return STRUCT_NOT_MADE;
        }
    }
    file->filter = from_stream(stream);
    if (file->filter == NULL){
        return STRUCT_NOT_MADE;
    }
    fclose(f);
    return 0;
}

int all_index_stream(size_t num_table, byte_buffer* stream, list * indexes){
    int ret =0;
    for (int i = 0; i < num_table; i ++){
        block_index * index =(block_index*)at(indexes, i);
        ret = block_to_stream(stream, index);
        if (ret != 0 ){
            return STRUCT_NOT_MADE;
        }
    }
    return 0;
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
    index->num_keys = 0;
}
int cmp_sst_f_inf(void* one, void * two){
    sst_f_inf * info1 = one;
    sst_f_inf * info2 = two;
    
    return strcmp(info1->min, info2->max);
}
int sst_equals(sst_f_inf * one, sst_f_inf * two){
    return one->block_indexs == two->block_indexs;
}

int find_sst_file_eq_iter(list  *sst_files, size_t num_files, const char * file_n) {
   

    for (int i = 0; i < sst_files->len; i++){
        sst_f_inf * sst = internal_at_unlocked(sst_files,i);
        if (strcmp(sst->file_name, file_n) == 0) return i;
    }
    return -1;
}
void generate_unique_sst_filename(char *buffer, size_t buffer_size, int level) {
    static int seeded = 0;
    if (!seeded) {
        struct timeval tv_seed;
        gettimeofday(&tv_seed, NULL);
        srand((unsigned int)(tv_seed.tv_sec ^ tv_seed.tv_usec ^ getpid()));
        seeded = 1;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);

    uint32_t r1 = (uint32_t)rand();
    uint32_t r2 = (uint32_t)rand();
    uint64_t random_val = ((uint64_t)r1 << 32) | r2;

    snprintf(buffer, buffer_size, "sst_%d_%ld_%ld_%016llx",
             level, tv.tv_sec, tv.tv_usec, random_val);
}