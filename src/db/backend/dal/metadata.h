#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../../../util/io.h"
#include "../../../ds/hashtbl.h"
#include "../../../ds/list.h"
#include "../../../ds/bloomfilter.h"
#include "../../../ds/byte_buffer.h"
#include "../../../util/alloc_util.h"
#include"../indexer.h"
#include "../../../util/error.h"
#include "../snapshot.h"
#ifndef METADATA_H
#define METADATA_H
#define MAX_LEVELS 7



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
    snapshot * current_tx_copy;
    int shutdown_status;
    int err_code;
}meta_data;
int read_sst_list(list * sst_files, byte_buffer * tempBuffer, size_t num_sst);
int save_snap(meta_data * meta);
void fresh_meta_load(meta_data * meta);
meta_data * load_meta_data(char * file, char * bloom_file);
void dump_sst_list(list * sst_files, FILE * f);
void write_md_d(char * file, meta_data * meta, byte_buffer * temp_buffer);
void free_md(meta_data * meta);
void destroy_meta_data(char * file, char * bloom_file, meta_data * meta);

#endif