#include "compression.h"
#include "indexer.h"
struct block_index;
void init_compress_tr_inf(compress_tr_inf * inf, byte_buffer * buffer, sst_cmpr_inf* sst){
    inf->curr_size = 0;
    inf->sizes = List(0, sizeof(size_t), false);
    inf->buffer = buffer;
    inf->sst = sst;
    inf->key_mem = malloc(2048);
    inf->value_mem = malloc(2048);
}
void init_sst_compr_inf(sst_cmpr_inf * sst, void * dict_buffer){
    memset(sst, 0, sizeof(*sst));
    if (dict_buffer != NULL) sst->dict_buffer = dict_buffer;
}
void set_dict_buffer(sst_cmpr_inf * sst, void * dict_buffer){
    sst->dict_buffer = dict_buffer;
}
void into_dict_buffer( compress_tr_inf * inf, db_unit  key, db_unit  value){
    write_buffer(inf->buffer, key.entry,key.len);
    write_buffer(inf->buffer, value.entry, value.len);
    size_t real = key.len + value.len;
    insert(inf->sizes, &real);
}
void prep_dict_compression(compress_tr_inf * inf , list * blocks, byte_buffer * sst_stream){
    uint16_t dummy = 0;
    db_unit key;
    db_unit value;
    key.entry = inf->key_mem;
    int real_c = 0;
    value.entry = inf->value_mem;
    for (int i = 0; i < blocks->len; i++){
        block_index * b = at(blocks, i);
        sst_stream->read_pointer = b->offset;
        read_buffer(sst_stream,&dummy, sizeof(dummy));
        for (int j = 0; j < b->num_keys; j++){
            read_db_unit(sst_stream, &key);
            read_db_unit(sst_stream, &value);
            into_dict_buffer(inf, key, value);
            real_c++;
        }
    }
    fprintf(stdout, "%d", real_c);
}
int sample_for_dict(sst_cmpr_inf * sst, int sst_size, int compression_level, void * key_val_sample, size_t * entry_lens, size_t num_entries){
    if (GLOB_OPTS.dict_size_ratio == 0) return 1;
    int dict_size =  sst_size / GLOB_OPTS.dict_size_ratio;
    if (sst->dict_buffer == NULL){
        sst->dict_buffer = calloc(dict_size * 2, 1);
        if (sst->dict_buffer == NULL) return -1;
    }
    size_t res = ZDICT_trainFromBuffer(sst->dict_buffer, dict_size * 2, key_val_sample, entry_lens, num_entries);
    if (ZSTD_isError(res)){
        fprintf(stdout, "%s\n", ZSTD_getErrorName(res));
        return -1;
    }
    if (sst->compression_dict == NULL){
        sst->compression_dict = ZSTD_createCDict(sst->dict_buffer, dict_size, compression_level);
    }
    if (ZSTD_isError(res)){
        ZSTD_freeCDict(sst->compression_dict);
        sst->compression_dict= NULL;
        return res;
    }
    if (sst->decompress_dict == NULL){
        sst->decompress_dict= ZSTD_createDDict(sst->dict_buffer, dict_size);
    }
    return 0;
}
// DB SPECIFIC
int fill_dicts(compress_tr_inf * inf, int sst_size,  list * blocks, byte_buffer * sst_stream){
    prep_dict_compression( inf , blocks, sst_stream);
    return sample_for_dict(inf->sst,sst_size, GLOB_OPTS.compress_level,inf->buffer, inf->sizes->arr, inf->sizes->len);
}
int compress_dict(sst_cmpr_inf * sst, byte_buffer * sst_stream, byte_buffer * dest ){
    if (sst->compr_context == NULL){
         sst->compr_context = ZSTD_createCCtx();
    }
    size_t bytes = ZSTD_compress_usingCDict(sst->compr_context, dest->buffy, dest->max_bytes, sst_stream->buffy, sst_stream->curr_bytes, sst->compression_dict);
    if (ZSTD_isError(bytes)){
        fprintf(stdout, "%s\n", ZSTD_getErrorName(bytes));
        return -1;
    }
    dest->curr_bytes+= bytes;
    return bytes;
}
int decompress_dict(sst_cmpr_inf * sst,byte_buffer * sst_stream, byte_buffer * dest ){
    if (sst->decompr_context == NULL){
        sst->decompr_context = ZSTD_createDCtx();
    }
    size_t bytes = ZSTD_decompress_usingDDict(sst->decompr_context, dest->buffy, dest->max_bytes, sst_stream->buffy, sst_stream->curr_bytes, sst->decompress_dict);
    if (ZSTD_isError(bytes)) {
        printf("%s", ZSTD_getErrorName(bytes));
        ZSTD_getErrorName(bytes);
        return -1;
    }
    dest->curr_bytes+= bytes;
    return bytes;
}
int regular_decompress(sst_cmpr_inf * sst, byte_buffer * sst_stream, byte_buffer * dest){
    if (sst->decompr_context == NULL){
        sst->decompr_context = ZSTD_createDCtx();
    }
    size_t bytes= ZSTD_decompressDCtx(sst->decompr_context, dest->buffy, dest->max_bytes, sst_stream->buffy, sst_stream->curr_bytes);
    if (ZSTD_isError(bytes)) {
        printf("%s", ZSTD_getErrorName(bytes));
        ZSTD_getErrorName(bytes);
        return -1;
    }
    dest->curr_bytes+= bytes;
    return bytes;
}
int regular_compress(sst_cmpr_inf * sst, byte_buffer * sst_stream, byte_buffer * dest){
    if (sst->compr_context == NULL){
        sst->compr_context = ZSTD_createCCtx();
    }
    size_t bytes= ZSTD_compressCCtx(sst->compr_context, dest->buffy, dest->max_bytes, sst_stream->buffy, sst_stream->curr_bytes, GLOB_OPTS.compress_level);
    if (ZSTD_isError(bytes)) {
        printf("%s", ZSTD_getErrorName(bytes));
        return -1;
    }
    dest->curr_bytes+= bytes;
    return bytes;
}
void free_sst_cmpr_inf(sst_cmpr_inf * sst){
    if (sst->compr_context != NULL) ZSTD_freeCCtx(sst->compr_context);
    if (sst->decompr_context != NULL) ZSTD_freeDCtx(sst->decompr_context);
    if (sst->decompress_dict != NULL) ZSTD_freeDDict(sst->decompress_dict);
    if (sst->compression_dict != NULL) ZSTD_freeCDict(sst->compression_dict);
    if (sst->dict_buffer != NULL) free(sst->dict_buffer);
    memset(sst, 0, sizeof(*sst));
}

