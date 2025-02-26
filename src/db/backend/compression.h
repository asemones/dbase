#include <zstd.h>
#include <zdict.h>
#include "../../ds/byte_buffer.h"
#include "indexer.h"
#include "key-value.h"
#define MAX_SAMPLE 128
#pragma once


typedef struct compress_tr_inf{
    byte_buffer * buffer;
    list * sizes;
    int curr_size;
    sst_f_inf * sst;
}compress_tr_inf;
void init_compress_tr_inf(compress_tr_inf * inf, byte_buffer * buffer, sst_f_inf * sst);
void into_dict_buffer(compress_tr_inf * inf, db_unit key, db_unit value);
void prep_dict_compression(compress_tr_inf * inf, list * blocks, byte_buffer * sst_stream);
int sample_for_dict_compress(sst_f_inf * sst, int compression_level, void * key_val_sample, size_t * entry_lens, int num_entries);
int sample_for_dict_decompress(sst_f_inf * sst, int compression_level, void * key_val_sample, size_t * entry_lens, int num_entries);
int fill_compression_dict(compress_tr_inf * inf, list * blocks, byte_buffer * sst_stream);
int fill_decompression_dict(compress_tr_inf * inf, list * blocks, byte_buffer * sst_stream);
int compress_dict(sst_f_inf * sst, byte_buffer * sst_stream, byte_buffer * dest);
int decompress_dict(sst_f_inf * sst, byte_buffer * sst_stream, byte_buffer * dest);
int regular_decompress(sst_f_inf * sst, byte_buffer * sst_stream, byte_buffer * dest);
int regular_compress(sst_f_inf * sst, byte_buffer * sst_stream, byte_buffer * dest);