#include <zstd.h>
#include <zdict.h>
#include "../../ds/byte_buffer.h"
#include "key-value.h"
#include "../../ds/list.h"
#include <stdint.h>
#include <stdbool.h>
#define MAX_SAMPLE 128
#pragma once
typedef struct block_index block_index;
typedef struct sst_cmpr_inf{
    ZSTD_DDict* decompress_dict;
    ZSTD_CDict* compression_dict;
    ZSTD_CCtx * compr_context;
    ZSTD_DCtx * decompr_context;
    void * dict_buffer;
} sst_cmpr_inf;
typedef struct compress_tr_inf{
    byte_buffer * buffer;
    list * sizes;
    int curr_size;
    sst_cmpr_inf * sst;
    char * key_mem;
    char * value_mem;
}compress_tr_inf;
void init_compress_tr_inf(compress_tr_inf * inf, byte_buffer * buffer, sst_cmpr_inf * sst);
void into_dict_buffer(compress_tr_inf * inf, db_unit key, db_unit value);
int sample_for_dict(sst_cmpr_inf * sst, int sst_size, int compression_level, void * key_val_sample, size_t * entry_lens, size_t num_entries);
void prep_dict_compression(compress_tr_inf * inf, list * blocks, byte_buffer * sst_stream);
int fill_dicts(compress_tr_inf * inf, int sst_size,  list * blocks, byte_buffer * sst_stream);
int compress_dict(sst_cmpr_inf * sst, byte_buffer * sst_stream, byte_buffer * dest);
int decompress_dict(sst_cmpr_inf  * sst, byte_buffer * sst_stream, byte_buffer * dest);
int regular_decompress(sst_cmpr_inf * sst, byte_buffer * sst_stream, byte_buffer * dest);
int regular_compress(sst_cmpr_inf * sst, byte_buffer * sst_stream, byte_buffer * dest);
void init_sst_compr_inf(sst_cmpr_inf * sst, void * dict_buffer);
void set_dict_buffer(sst_cmpr_inf * sst, void * dict_buffer);
void free_sst_cmpr_inf(sst_cmpr_inf * sst);