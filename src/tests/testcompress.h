#include "../db/backend/compression.h"
#define TEST_DBUF_SIZE 1024 * 3000
#define TEST_BB_SIZE 1024 * 2000

byte_buffer * sst_stream;
byte_buffer * dest;
sst_f_inf inf;
/*finish these tests*/
void start(){
    inf = create_sst_empty();
    sst_stream = create_buffer(TEST_BB_SIZE);
    dest = create_buffer(TEST_BB_SIZE);

}
void end(){
    remove ("WAL_0.bin");
    remove("WAL_1.bin");
    remove("WAL_M.bin");
    remove("meta.bin");
    remove ("bloom.bin");
    free_sst_inf(&inf);
    free_buffer(sst_stream);
    free_buffer(dest);
}
void write_random_data(byte_buffer * buffer, const int iters){
   for (int i = 0; i < iters; i++){
        write_buffer(buffer,"helloworld", strlen("hellowrld"));
   }
}
void test_regular_compress(){
    start(); 
    write_random_data(sst_stream, 200);
    regular_compress(&inf.compr_info, sst_stream, dest, sst_stream->curr_bytes);
    TEST_ASSERT_NOT_EQUAL_INT(dest->curr_bytes, 0);
    end();
    
}
void test_regular_decompress(){
    start(); 
    write_random_data(sst_stream, 200);
    int bytes = sst_stream->curr_bytes;
    regular_compress(&inf.compr_info, sst_stream, dest, sst_stream->curr_bytes);
    sst_stream->curr_bytes = 0;
    regular_decompress(&inf.compr_info, dest, sst_stream, dest->curr_bytes);
    TEST_ASSERT_EQUAL_INT(sst_stream->curr_bytes, bytes);
    end();
}
void test_dict_compress(){
    start();
    char * meta = "meta.bin";
    char * bloom = "bloom.bin";
    storage_engine * db = create_engine(meta, bloom);
    byte_buffer * extra = create_buffer(GLOB_OPTS.SST_TABLE_SIZE * 1.1);    /*dict compression is only good for similar entries, hence this instead of random value calls*/
    create_a_bigbase(db);
    sst_f_inf * first_sst = at(db->meta->sst_files[0], 0);
    
    // Debug: Print SST file info
    fprintf(stdout, "SST file: %s, length: %zu\n", first_sst->file_name, first_sst->length);
    
    read_entire_sst(extra, sst_stream,first_sst);
    fprintf(stdout, "Read %zu bytes from SST file\n", sst_stream->curr_bytes);
  
    
    compress_tr_inf compr_inf;
    init_compress_tr_inf(&compr_inf, dest, &first_sst->compr_info);
    sst_stream->read_pointer = 0;
    
    prep_dict_compression(&compr_inf, first_sst->block_indexs, sst_stream);
    fprintf(stdout, "After prep_dict_compression, buffer size: %zu\n", compr_inf.buffer->curr_bytes);
    
    // Use a smaller ratio for larger dictionary
    GLOB_OPTS.dict_size_ratio = 20; // Changed from 100 to 20 for a 5x larger dictionary
    
    size_t len = compr_inf.sizes->len;
    size_t sum = 0;
    for (int i = 0; i < compr_inf.sizes->len; i++){
        size_t * l = at(compr_inf.sizes, i);
        sum += *l;
    }
    fprintf(stdout, "Total entry size: %zu, Number of entries: %zu\n", sum, len);
    
    // Calculate and print the dictionary size that will be used
    size_t dict_size = compr_inf.buffer->curr_bytes / GLOB_OPTS.dict_size_ratio;
    fprintf(stdout, "Dictionary size will be: %zu bytes\n", dict_size);
    
    int res = sample_for_dict(&first_sst->compr_info, compr_inf.buffer->curr_bytes, 1, compr_inf.buffer->buffy, (size_t*)compr_inf.sizes->arr, len);
    fprintf(stdout, "sample_for_dict result: %d\n", res);
    
    TEST_ASSERT_EQUAL_INT(0, res);
    
    reset_buffer(dest);
    sst_stream->read_pointer = 0;
    res = compress_dict(&first_sst->compr_info, sst_stream, dest, sst_stream->curr_bytes);
    fprintf(stdout, "compress_dict result: %d\n", res);
    
    TEST_ASSERT_NOT_EQUAL_INT(-1, res);
    
    remove_ssts(db->meta->sst_files);
    free_engine(db, meta, bloom);
    free_buffer(extra);
    end();
}
/*consider the storage format of the compression related things*/
void test_dict_decompress(){
    start();
    char * meta = "meta.bin";
    char * bloom = "bloom.bin";
    byte_buffer * extra = create_buffer(GLOB_OPTS.SST_TABLE_SIZE * 1.1);   
    storage_engine * db = create_engine(meta, bloom);
    /*dict compression is only good for similar entries, hence this instead of random value calls*/
    create_a_bigbase(db);
    sst_f_inf * first_sst = at(db->meta->sst_files[0], 0);
    read_entire_sst(extra, sst_stream,first_sst);
    compress_tr_inf  compr_info;
    init_compress_tr_inf(&compr_info, dest, &first_sst->compr_info);
    sst_stream->read_pointer = 0;
    prep_dict_compression(&compr_info, first_sst->block_indexs, sst_stream);
    GLOB_OPTS.dict_size_ratio = 100;
    int res=  sample_for_dict(&first_sst->compr_info,compr_info.buffer->curr_bytes,1, compr_info.buffer->buffy, compr_info.sizes->arr, compr_info.sizes->len);
    TEST_ASSERT_EQUAL_INT(0, res);
    reset_buffer(dest);
    int bytes = sst_stream->curr_bytes;
    sst_stream->read_pointer = 0;
    res = compress_dict(&first_sst->compr_info, sst_stream, dest, sst_stream->curr_bytes);
    TEST_ASSERT_NOT_EQUAL_INT(-1, res);
    reset_buffer(sst_stream);
    decompress_dict(&first_sst->compr_info,  dest, sst_stream, dest->curr_bytes);
    TEST_ASSERT_EQUAL_INT(sst_stream->curr_bytes, bytes);
    remove_ssts(db->meta->sst_files);
    free_engine(db, meta, bloom);
    free_buffer(extra);
    end();
}