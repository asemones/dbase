#include <stdio.h>
#include "btree.h"
#include "../../ds/bloomfilter.h"
#include "dal/metadata.h"
#include "../../ds/list.h"
#include "json.h"
#include "../../ds/hashtbl.h"
#include <time.h>
#include <string.h>
#include <pthread.h>
#include "../../ds/structure_pool.h"
#include "../../util/stringutil.h"
#include "sst_builder.h"
#include "../../util/alloc_util.h"
#include "../../ds/skiplist.h"

#ifndef LSM_H
#define LSM_H

#define MEMTABLE_SIZE 80000
#define TOMB_STONE "-"
#define NUM_HASH 7
#define LOCKED_TABLE_LIST_LENGTH 2
#define CURRENT_TABLE 0
#define PREV_TABLE 1
#define READER_BUFFS 10
#define WRITER_BUFFS 10
#define LVL_0 0
#define MAXIUMUM_KEY 0
#define MINIMUM_KEY 1
#define KB 1024

/*This is the storage engine struct which controls all operations at the disk level. Ideally,
the user should never be concerned with this struct. The storage engine contains memtables, one active and one immutable. It also has
pools for reading and writing data and access the metadata struct. The mem_table uses a skiplist.
The general read and write functions are present here*/
typedef struct mem_table{
    size_t bytes;
    bloom_filter *filter;
    size_t num_pair;
    bool immutable;
    char *range[2];
    SkipList *skip;
}mem_table;

typedef struct storage_engine{
    mem_table * table[LOCKED_TABLE_LIST_LENGTH];
    meta_data * meta;
    size_t num_table;
    struct_pool * read_pool;
    struct_pool * write_pool;
}storage_engine;
static int flush_table(storage_engine * engine);
void lock_table(storage_engine * engine);

static mem_table * create_table(){
    mem_table * table = (mem_table*)wrapper_alloc((sizeof(mem_table)), NULL,NULL);
    table->bytes = 0;
    table->filter = bloom(NUM_HASH,NUM_WORD,false, NULL);
    table->immutable = false;
    table->num_pair = 0;
    table->skip = createSkipList(&compareString);
    for(int i = 0 ; i < 2; i++){
        table->range[i] = NULL;
    }
    return table;
}
static void clear_table(mem_table * table){
    if (table->skip != NULL) freeSkipList(table->skip);
    if (table->num_pair <= 0  && table->filter!=NULL) free_filter(table->filter);
    table->skip = createSkipList(&compareString);
    table->bytes = 0;
    table->filter = bloom(NUM_HASH,NUM_WORD,false, NULL);
    table->num_pair = 0;
    table->immutable = false;
    for(int i = 0 ; i < 2; i++){
        table->range[i] = NULL;
    }
}
storage_engine * create_engine(char * file, char * bloom_file){
    storage_engine * engine = (storage_engine*)wrapper_alloc((sizeof(storage_engine)), NULL,NULL);
    for (int i = 0; i < LOCKED_TABLE_LIST_LENGTH; i++){
        engine->table[i] = create_table();
    }
    engine->meta = load_meta_data(file, bloom_file);
    engine->num_table = 0;
    engine->read_pool = create_pool(READER_BUFFS);
    engine->write_pool = create_pool(WRITER_BUFFS);
    for(int i = 0; i < READER_BUFFS; i++){
        byte_buffer * buffer = create_buffer(MEMTABLE_SIZE*1.5);
        insert_struct(engine->read_pool,buffer);
    }
    for(int i = 0; i < WRITER_BUFFS; i++){
        byte_buffer * buffer = create_buffer(MEMTABLE_SIZE*1.5);
        insert_struct(engine->write_pool,buffer);
    }
    return engine;
}
int write_record(storage_engine* engine ,keyword_entry * entry, size_t recordSize){
    if (entry->keyword == NULL) return -1;
    if (engine->table[CURRENT_TABLE]->bytes + recordSize >=  MEMTABLE_SIZE){
        lock_table(engine);
        int status = flush_table(engine);
        if (status != 0) return -1;
    }
    insert_list(engine->table[CURRENT_TABLE]->skip, entry->keyword, entry->value);
    engine->table[CURRENT_TABLE]->num_pair++;
    engine->table[CURRENT_TABLE]->bytes += recordSize;
    map_bit(entry->keyword,engine->table[CURRENT_TABLE]->filter);
    
    return 0;
}
size_t find_sst_file(list  *sst_files, size_t num_files, const char *key) {
    size_t max_index = num_files;
    size_t min_index = 0;
    size_t middle_index;

    while (min_index < max_index) {
        middle_index = min_index + (max_index - min_index) / 2;
        sst_f_inf *sst = get_element(sst_files, middle_index);
        if (strcmp(key, sst->min) >= 0 && strcmp(key, sst->max) <= 0) {
            return middle_index;
        } 
        else if (strcmp(key, sst->max) > 0) {
            min_index = middle_index + 1;
        } 
        else {
            
            max_index = middle_index;
        }
    }
    return 0;
}
size_t find_block(sst_f_inf *sst, const char *key) {
   for (int i = 0 ; i < sst->block_indexs->len; i++){

       block_index * index = get_element(sst->block_indexs, i);
       block_index * next_index = get_element(sst->block_indexs, i+1);
       if (next_index == NULL) return i;
       else if (strcmp(key, index->min_key) >= 0 && strcmp(key, next_index->min_key) < 0) return i;
   }
    return sst->block_indexs->len - 1;
}
/* this is our read path. This read path */

char * disk_read(storage_engine * engine, const char * keyword, dict * read_dict, byte_buffer * buffer, byte_buffer * key_values){ 
    
    for (int i= 0; i < MAX_LEVELS; i++){
        if (engine->meta->sst_files[i] == NULL || engine->meta->sst_files[i]->len ==0) continue;
        list * sst_files_for_x = engine->meta->sst_files[i];
        size_t index_sst = find_sst_file(sst_files_for_x, sst_files_for_x->len, keyword);
        if (index_sst == -1) continue;

        sst_f_inf * sst = get_element(sst_files_for_x, index_sst);
       // bloom_filter * filter = get_element(engine->meta->filters, sst->filter_index);
        bloom_filter * filter=  sst->filter;
        if (!check_bit(keyword,filter)) continue;

        FILE * sst_file = fopen(sst->file_name,"rb");
        size_t index_block= find_block(sst, keyword);
        block_index * index = get_element(sst->block_indexs, index_block);

        if (!check_bit(keyword, index->filter)) continue;
        else fseek(sst_file, index->offset, SEEK_SET);
        int read = fread(buffer->buffy, 1, index->len, sst_file);
        buffer->curr_bytes = read;
        deseralize_into_structure(&add_kv_void,read_dict, buffer,key_values);
        fclose(sst_file);
        char * value = (char *)get_v(read_dict, keyword);
        if (value != NULL) return value;      
    }
    return NULL;
}
char* read_record(storage_engine * engine, const char * keyword, dict * read_dict, byte_buffer * buffer, byte_buffer * key_values){
    keyword_entry * entry = search_list(engine->table[CURRENT_TABLE]->skip, keyword);
    if (entry == NULL) return disk_read(engine, keyword,read_dict, buffer, key_values);
    return entry->value;
}
int delete_record(storage_engine * engine, const char * keyword, keyword_entry * alloc_entry , dict * read_dict){
    keyword_entry * entry = search_list(engine->table[CURRENT_TABLE]->skip,(char*)keyword);
    if (entry == NULL && alloc_entry!=NULL){
        memcpy(alloc_entry->keyword, keyword, strlen(keyword));
        memcpy(alloc_entry->value, TOMB_STONE,2);
        delete_element(engine->table[CURRENT_TABLE]->skip,alloc_entry->keyword);
        insert_list(engine->table[CURRENT_TABLE]->skip, alloc_entry->keyword, alloc_entry->value);
        return 1;
    }
    if (entry == NULL) return -1;
    memcpy(entry->value, TOMB_STONE,2);
    return 0;
}
static void seralize_table(SkipList * list, byte_buffer * buffer){
    if (list == NULL) return;
    Node * node = list->header->forward[0];
    while (node != NULL){
        seralize_field(node->key, node->value, buffer ,STRING);
        node = node->forward[0];
    }
}
void lock_table(storage_engine * engine){
   engine->table[CURRENT_TABLE]->immutable = true;
   swap(&engine->table[CURRENT_TABLE], &engine->table[PREV_TABLE],8);
   engine->num_table++;
   clear_table(engine->table[CURRENT_TABLE]);
}
static int flush_table(storage_engine * engine){
    mem_table * table = engine->table[PREV_TABLE];
    byte_buffer * buffer = request_struct(engine->write_pool);
    meta_data * meta = engine->meta;
    meta->sst_files[0]->len++;
    sst_f_inf* sst = get_element(meta->sst_files[0], meta->num_sst_file); 
    if (sst == NULL){
        meta->sst_files[0]->len --;
        sst = create_sst_file_info(NULL, 0, NUM_WORD, table->filter);
        insert(meta->sst_files[0],sst);
        sst = get_element(meta->sst_files[0], meta->sst_files[0]->len-1);
    }
    else{
        sst->mem_store = calloc_arena(4096);
        sst->block_indexs = List(0, sizeof(block_index), true);
        sst->filter = table->filter;
    }


    
    if (table->bytes <= 0) {
        free_buffer(buffer);
        return -1;
    }
    seralize_table(table->skip, buffer);
    sst->length = buffer->curr_bytes;
    
   //sst->id = meta->num_sst_file;

   
    gen_sst_fname(meta->num_sst_file, LVL_0, sst->file_name);
    meta->num_sst_file ++;
    go_nearest_v(buffer, '{');
    get_next(buffer);
    byte_buffer * staging = request_struct(engine->write_pool);
    struct write_info w_info;
    w_info.file = sst->file_name;
    w_info.staging = staging;
    w_info.table_store = buffer;
    w_info.table_s = 4*KB;

    build_and_write_all_tables(w_info, sst);
    

    meta->db_length+= buffer->curr_bytes;
    return_struct(engine->write_pool, buffer,&reset_buffer);

    //insert(meta->filters,table->filter);
    table->immutable= false;
    return_struct(engine->write_pool, buffer,&reset_buffer);
    return 0;

}
static void free_one_table(mem_table * table){
    //destroy_b_tree (table->tree);
    freeSkipList(table->skip);;
    if (table->num_pair <= 0) free_filter(table->filter);
    else free(table->filter);
    free(table);
    table = NULL;
}
void dump_tables(storage_engine * engine){
    for(int i = 0;  i < LOCKED_TABLE_LIST_LENGTH; i++){
        if (engine->table[i]== NULL) continue;
        if (engine->table[i]->bytes > 0 && i==0){
            //this should be changed asap to stop edgecases
            lock_table(engine);
            flush_table(engine);
        }
    }
    //fclose(engine->table->file);  
}
void free_tables(storage_engine * engine){
    for(int i = 0;  i < LOCKED_TABLE_LIST_LENGTH; i++){
        if (engine->table[i]== NULL) continue;
        free_one_table(engine->table[i]);
        engine->table[i] = NULL;
    }
}
void free_engine(storage_engine * engine, char* meta_file,  char * bloom_file){
    dump_tables(engine);
    destroy_meta_data(meta_file, bloom_file,engine->meta);  
    engine->meta = NULL;
    free_tables(engine);
    free_pool(engine->read_pool, &free_buffer);
    free_pool(engine->write_pool, &free_buffer);
    free(engine);
    engine = NULL;
}
#endif


