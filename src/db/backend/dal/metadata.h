#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../../../util/io.h"
#include "../../../ds/hashtbl.h"
#include "../json.h"
#include "../../../ds/list.h"
#include "../../../ds/bloomfilter.h"
#include "../../../ds/byte_buffer.h"
#include "../../../util/alloc_util.h"
#include "../sst_builder.h"
#include"../indexer.h"
#ifndef METADATA_H
#define METADATA_H
#define BUFFER_SIZE 563840
#define DICT_BUFFER_SIZE 563840
#define NUM_HASH 7
#define NUM_WORD 2000
#define MIN_KEY_SIZE 10
#define TIME_STAMP_SIZE 20
#define MAX_LEVELS 7
#define BASE_LEVEL_SIZE 6400000
#define MIN_COMPACT_RATIO 50
#define INDEX_MEMORY_COUNT 1024*500

/*This is the meta_data struct. The meta_data struct is the equvialent of the cataologue in most dbms. 
It containes all the information about the database including all indexes and the bloom filters for each level,
although stored in one fat list. */
typedef struct meta_data{
    size_t num_sst_file;
    size_t file_ptr;
    size_t db_length;
    size_t base_level_size;
    size_t min_c_ratio; //minimum compact ratio
    list * sst_files[MAX_LEVELS];
    int levels_with_data[MAX_LEVELS];
    int shutdown_status;
}meta_data;
/* something doesnt read or write properly after the first sst list is written*/
static void read_sst_list(list * sst_files, byte_buffer * tempBuffer, size_t num_sst){
   for (int i = 0; i < sst_files->len; i++){
        size_t block_ind_len = 0;
        if (i +1 ==  sst_files->cap) expand(sst_files);
        sst_f_inf * sst = (sst_f_inf*)at(sst_files,i);
        read_buffer(tempBuffer, sst->file_name, strlen(&tempBuffer->buffy[tempBuffer->read_pointer])+1);
        read_buffer(tempBuffer, &sst->length, sizeof(size_t));
        read_buffer(tempBuffer, sst->max, MIN_KEY_SIZE);
        read_buffer(tempBuffer, sst->min, MIN_KEY_SIZE);
        read_buffer(tempBuffer, sst->timestamp, TIME_STAMP_SIZE);
        read_buffer(tempBuffer, &sst->block_start, sizeof(size_t));
        read_buffer(tempBuffer, &block_ind_len, sizeof(size_t));
        sst->block_indexs = thread_safe_list(block_ind_len,sizeof(block_index),false);
        sst->block_indexs->len = block_ind_len;
        sst->mem_store= calloc_arena(GLOB_OPTS.BLOCK_INDEX_SIZE);
        
    }
    byte_buffer * small_buff = create_buffer(30 * 1024);
    for (int i = 0 ;i < num_sst; i++){
        sst_f_inf * sst = (sst_f_inf*)at(sst_files,i);
        read_index_block(sst, small_buff);

    }
    free_buffer(small_buff);
}

static void fresh_meta_load(meta_data * meta){
    meta->num_sst_file = 0;
    meta->file_ptr = 0;
    meta->db_length = 0;
    meta->shutdown_status = -1;
   //meta->basic_index = Dict();
    meta->base_level_size = BASE_LEVEL_SIZE;
    meta->min_c_ratio = MIN_COMPACT_RATIO;
    /*make lists for all*/
    for (int i = 0; i < MAX_LEVELS; i++){
        meta->sst_files[i] = thread_safe_list(0,sizeof(sst_f_inf),false); 
    }
    for (size_t i = 0; i < MAX_LEVELS; i++)
    {
        meta->levels_with_data[i] = -1;
    }
    
   
   //meta->dict_buffer = (char*)calloc(1,BUFFER_SIZE);
}
meta_data * load_meta_data(char * file, char * bloom_file){
    meta_data * meta = (meta_data*)wrapper_alloc((sizeof(meta_data)), NULL,NULL);
    byte_buffer * tempBuffer = create_buffer(DICT_BUFFER_SIZE);

    int bytes;
    if ((bytes = read_file(tempBuffer->buffy,file, "rb",1,0)) <= 0) {
        fresh_meta_load(meta);
        free_buffer(tempBuffer);
        return meta;
    }
    int size=  sizeof(size_t);
    read_buffer(tempBuffer, &meta->shutdown_status, sizeof(meta->shutdown_status));
    read_buffer(tempBuffer, &meta->num_sst_file, size);
    read_buffer(tempBuffer, &meta->file_ptr, size);
    read_buffer(tempBuffer, &meta->db_length, size);
    read_buffer(tempBuffer, &meta->base_level_size, size);
    read_buffer(tempBuffer, &meta->min_c_ratio, size);
    read_buffer(tempBuffer, meta->levels_with_data, sizeof(int)*MAX_LEVELS);
    int lengths[MAX_LEVELS];
    read_buffer(tempBuffer, lengths, sizeof(int)*MAX_LEVELS);
    for (int i = 0; i < MAX_LEVELS; i++){
         if (lengths[i] <=0){
            meta->sst_files[i] = thread_safe_list(0,sizeof(sst_f_inf),false);
            continue;
        }
        meta->sst_files[i] = thread_safe_list(lengths[i],sizeof(sst_f_inf),false);
        meta->sst_files[i]->len = lengths[i];
        read_sst_list(meta->sst_files[i], tempBuffer, meta->num_sst_file);
    }
    size_t readSize= 0;
    read_buffer(tempBuffer, &readSize, size);
    free_buffer(tempBuffer);
    tempBuffer = NULL;

    return meta;
}
static void dump_sst_list(list * sst_files, FILE * f){
    for (int i = 0; i < sst_files->len; i++){
        sst_f_inf * sst = at(sst_files,i);
        fwrite(sst->file_name, 1,strlen(sst->file_name)+1,f);
        fwrite(&sst->length, 1,sizeof(size_t),f);
        fwrite(sst->max, 1, MIN_KEY_SIZE, f);
        fwrite(sst->min, 1, MIN_KEY_SIZE, f);
        fwrite(sst->timestamp, 1, TIME_STAMP_SIZE, f);
        if (sst->block_indexs == NULL) continue;
        fwrite(&sst->block_start, 1, sizeof(size_t), f);
        size_t temp = sst->block_indexs->len;
        fwrite(&temp, 1, sizeof(size_t), f);
    }
}
static void write_md_d(char * file, meta_data * meta, byte_buffer * temp_buffer){
       FILE * f = fopen(file, "wb");
       meta->shutdown_status = 0;
       fwrite(&meta->shutdown_status, sizeof(meta->shutdown_status), 1, f);
       fwrite(&meta->num_sst_file, sizeof(size_t),1, f);
       fwrite(&meta->file_ptr, sizeof(size_t),1, f);
       fwrite(&meta->db_length, sizeof(size_t),1, f);
       fwrite(&meta->base_level_size, sizeof(size_t),1, f);
       fwrite(&meta->min_c_ratio, sizeof(size_t),1, f);
       int j = 0;
       /* first loop is to get the info of which sst lists are actually written*/
       for (int i = 0; i < MAX_LEVELS; i++){
            if (meta->sst_files[i] == NULL || meta->sst_files[i]->len <=0) continue;
            meta->levels_with_data[j] = i;
            j++;
       }
       for (; j < MAX_LEVELS; j++)
       {
            meta->levels_with_data[j] = -1;
       }
       
       fwrite(meta->levels_with_data, sizeof(int), MAX_LEVELS, f);
       /*the actual writing occurs here*/
        for (int i = 0; i < MAX_LEVELS; i++){
            fwrite(&meta->sst_files[i]->len, sizeof(int), 1, f);
        }
       
       for (int i = 0; i < MAX_LEVELS; i++){
            if (meta->sst_files[i] == NULL || meta->sst_files[i]->len <=0) continue;
            dump_sst_list(meta->sst_files[i], f);
       }
       fwrite(&temp_buffer->curr_bytes, sizeof(size_t),1, f);
       fclose(f);
}
static void free_md(meta_data * meta){
    
    for (int i = 0 ; i < MAX_LEVELS; i++){
        if (meta->sst_files[i] == NULL) continue;
        free_list(meta->sst_files[i], &free_sst_inf);
        
    }
    free(meta);
    meta = NULL;
}
void destroy_meta_data(char * file,char * bloom_file, meta_data * meta){
    byte_buffer* temp_buffer = create_buffer(DICT_BUFFER_SIZE);
    if (file!= NULL){
       write_md_d(file, meta, temp_buffer);
    }
    
    free_md(meta);
    free_buffer(temp_buffer);
}


#endif