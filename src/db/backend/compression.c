#include "compression.h"


void init_compress_tr_inf(compress_tr_inf * inf, byte_buffer * buffer, sst_f_inf * sst){
    inf->curr_size = 0;
    inf->sizes = List(0, sizeof(size_t), false);
    inf->buffer = buffer;
    inf->sst = sst;
}
void into_dict_buffer( compress_tr_inf * inf, db_unit  key, db_unit  value){
    write_buffer(inf->buffer, key.entry,key.len);
    write_buffer(inf->buffer, value.entry, value.len);
    insert(inf->sizes, &key.len);
    insert(inf->sizes, &value.len);

}
void prep_dict_compression(compress_tr_inf * inf , list * blocks, byte_buffer * sst_stream){
    int dummy = 0;
    for (int i = 0; i < blocks->len; i++){
        block_index * b = at(blocks, i);
        read_buffer(sst_stream,&dummy, sizeof(dummy));
        for (int j = 0; j < b->len; j++){
            db_unit key;
            db_unit value;
            key  = read_db_unit(inf->buffer);
            value = read_db_unit(inf->buffer);
            into_dict_buffer(inf, key, value);
        }
    }
}
int sample_for_dict_compress(sst_f_inf * sst, int compression_level, void * key_val_sample, size_t * entry_lens, int num_entries){
    if (GLOB_OPTS.dict_size_ratio == 0) return 1;
    int dict_size = sst->block_start / GLOB_OPTS.dict_size_ratio;
    if (sst->compression_dict == NULL){
        sst->compression_dict = ZSTD_createCDict(sst->dict_buffer, dict_size, compression_level);
    }
    size_t res = ZDICT_trainFromBuffer(sst->dict_buffer, dict_size, key_val_sample, entry_lens, num_entries);
    if (ZSTD_isError(res)){
        ZSTD_freeCDict(sst->compression_dict);
        sst->compression_dict= NULL;
        return res;
    }
    return 0;
}
int sample_for_dict_decompress(sst_f_inf * sst, int compression_level, void * key_val_sample, size_t * entry_lens, int num_entries){
    if (GLOB_OPTS.dict_size_ratio == 0) return 1;
    int dict_size = sst->block_start / GLOB_OPTS.dict_size_ratio;
    if (sst->decompress_dict== NULL){
        sst->decompress_dict= ZSTD_createDDict(sst->dict_buffer, dict_size);
    }
    size_t res = ZDICT_trainFromBuffer(sst->dict_buffer, dict_size, key_val_sample, entry_lens, num_entries);
    if (ZSTD_isError(res)){
        ZSTD_freeDDict(sst->decompress_dict);
        sst->decompress_dict= NULL;
        return res;
    }
    return 0;
}
// DB SPECIFIC
int fill_compression_dict( compress_tr_inf * inf, list * blocks, byte_buffer * sst_stream){
    prep_dict_compression( inf , blocks, sst_stream);
    return sample_for_dict_compress(inf->sst, GLOB_OPTS.compress_level,inf->buffer, inf->sizes->arr, inf->sizes->len);
}
int fill_decompression_dict( compress_tr_inf * inf, list * blocks, byte_buffer * sst_stream){
    prep_dict_compression( inf , blocks, sst_stream);
    return sample_for_dict_decompress(inf->sst, GLOB_OPTS.compress_level,inf->buffer, inf->sizes->arr, inf->sizes->len);
}
int compress_dict(sst_f_inf * sst,byte_buffer * sst_stream, byte_buffer * dest ){
    size_t bytes = ZSTD_compress_usingCDict(sst->compr_context, dest->buffy, dest->max_bytes, sst_stream->buffy, sst_stream->curr_bytes, sst->compression_dict);
    if (ZSTD_isError(bytes)) return -1;
    dest->buffy+= bytes;
    return bytes;
}
int decompress_dict(sst_f_inf * sst,byte_buffer * sst_stream, byte_buffer * dest ){
    size_t bytes = ZSTD_decompress_usingDDict(sst->decompr_context, dest->buffy, dest->max_bytes, sst_stream->buffy, sst_stream->curr_bytes, sst->decompress_dict);
    if (ZSTD_isError(bytes)) return -1;
    dest->buffy+= bytes;
    return bytes;
}
int regular_decompress(sst_f_inf * sst, byte_buffer * sst_stream, byte_buffer * dest){
    size_t bytes= ZSTD_decompressDCtx(sst->decompr_context, dest->buffy, dest->max_bytes, sst_stream->buffy, sst_stream->curr_bytes);
     if (ZSTD_isError(bytes)) {
        printf("%s", ZSTD_getErrorName(bytes));
        ZSTD_getErrorName(bytes);
        return -1;
    }
    dest->curr_bytes+= bytes;
    return bytes;
}
int regular_compress(sst_f_inf * sst, byte_buffer * sst_stream, byte_buffer * dest){
    size_t bytes= ZSTD_compressCCtx(sst->compr_context, dest->buffy, dest->max_bytes, sst_stream->buffy, sst_stream->curr_bytes, GLOB_OPTS.compress_level);
    if (ZSTD_isError(bytes)) {
        printf("%s", ZSTD_getErrorName(bytes));
        return -1;
    }
    dest->curr_bytes+= bytes;
    return bytes;
}

