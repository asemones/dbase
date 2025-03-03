#include "lsm.h"
#include <stdio.h>
#include "indexer.h"
#include <stdlib.h>
#include "../../ds/associative_array.h"
#include "../../ds/frontier.h"
#include "../../ds/hashtbl.h"
#pragma once
#define EODB 0

/* write a proper implementation plan*/
/*The read path involves block_indexs with values, encased by sst tables, which are seperated into
seven levels. The iterator is used for range scans/deletes and allows an abstraction of as if 
every key-value pair can be accessed linearly in memeory. Since every sst table outside of level 0
has a gurantee of no range overlaps, only one sst_iter is needed per level and one for memetable\
level 0. Since blocks also have a gurantee of being ordered, only one block index cursor is required
per level. This enables each sub-iter to return their "next" and have that compared to every
other "next".*/

typedef struct block_iter {
    k_v_arr * arr;
    int curr_key_ind;          
    block_index *index;    
} block_iter;

typedef struct sst_iter {
    block_iter cursor; 
    sst_f_inf *file; //pointer to the sst_f_inf
    int curr_index;      
} sst_iter;

typedef struct level_iter {
    sst_iter cursor;    
    list *level; //simply a pointer to the real sst_list of the level    
    int curr_index;          
} level_iter;

typedef struct mem_table_iter {
    Node *cursor; 
    mem_table *t;           
} mem_table_iter;
/*A single database iterator*/
typedef struct aseDB_iter {
    mem_table_iter mem_table[2]; 
    level_iter l_1_n_level_iters [6];
    list * l_0_sst_iters;
    frontier * pq;
    list * ret;
    shard_controller * c;
} aseDB_iter;
int compare_merge_data(const void * one, const void * two);
aseDB_iter * create_aseDB_iter(void);
void init_block_iter(block_iter *b_cursor, block_index *file);
void init_sst_iter(sst_iter * cursor, sst_f_inf* level);
void init_mem_table_iters(mem_table_iter *mem_table_iters, storage_engine *s, size_t num_tables);
void init_level_iters(level_iter *level_iters, storage_engine *s, size_t num_levels);
void init_level_0_sst_iters(list *l_0_sst_iters, storage_engine *s);
void init_aseDB_iter(aseDB_iter *dbi, storage_engine *s);
merge_data next_entry(block_iter *b,   shard_controller * c, char * file_name);
merge_data next_key_block(sst_iter *sst, shard_controller  *c);
merge_data next_sst_block(level_iter *level,   shard_controller *c);
merge_data get_curr_kv(sst_iter sst_it);
merge_data next_entry_memtable(mem_table_iter * iter);
void seek_sst(sst_iter* sst_it,   shard_controller * c, const char * prefix);
void seek(aseDB_iter * iter, const char * prefix);
merge_data same_merge_key(const merge_data next, const merge_data last);
merge_data aseDB_iter_next(aseDB_iter * iter);
int write_db_entry(byte_buffer * b, void * element);
void free_aseDB_iter(aseDB_iter *iter);



