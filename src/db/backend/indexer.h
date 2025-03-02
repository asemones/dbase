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
#include <zstd.h>
#include "../../util/stringutil.h"
#include "key-value.h"
#include "../../ds/associative_array.h"
#include "../../ds/hashtbl.h"
#include "compression.h"
#define MAX_KEY_SIZE 100
#define MAX_F_N_SIZE 64
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
/*the requirements for a successful compresssion*/
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
    sst_cmpr_inf compr_info;

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

sst_f_inf create_sst_empty(void);
sst_f_inf create_sst_filter(bloom_filter * b);
int sst_deep_copy(sst_f_inf * master, sst_f_inf * copy);

block_index create_ind_stack(size_t est_num_keys);
block_index * block_from_stream(byte_buffer * stream, block_index * index);
int block_to_stream(byte_buffer * targ, block_index * index);
int read_index_block(sst_f_inf * file, byte_buffer * stream);
int all_index_stream(size_t num_table, byte_buffer * stream, list * indexes);
void reuse_block_index(void * b);

int cmp_sst_f_inf(void * one, void * two);
int sst_equals(sst_f_inf * one, sst_f_inf * two);
int find_sst_file_eq_iter(list * sst_files, size_t num_files, const char * file_n);
void generate_unique_sst_filename(char *buffer, size_t buffer_size, int level);
void build_index(sst_f_inf * sst, block_index * index, byte_buffer * b, size_t num_entries, size_t block_offsets);
int json_b_search(k_v_arr * json, const char * target);
int prefix_b_search(k_v_arr *json, const char *target);
void free_sst_inf(void * ss);
void grab_min_b_key(block_index * index, byte_buffer *b, int loc);