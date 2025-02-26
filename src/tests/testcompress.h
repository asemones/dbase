#include "../db/backend/compression.h"
#define TEST_DBUF_SIZE 1024 * 300
#define TEST_BB_SIZE 1024 * 200

byte_buffer * sst_stream;
byte_buffer * dest;
sst_f_inf inf;
/*finish these tests*/
void start(){
    inf = create_sst_empty();
    sst_stream = create_buffer(TEST_BB_SIZE);
    dest = create_buffer(TEST_BB_SIZE);
    inf.dict_buffer = malloc(TEST_DBUF_SIZE);

}
void end(){
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
    regular_compress(&inf, sst_stream, dest);
    TEST_ASSERT_NOT_EQUAL_INT(dest->curr_bytes, 0);
    end();
    
}
void test_regular_decompress(){
    start(); 
    write_random_data(sst_stream, 200);
    int bytes = sst_stream->curr_bytes;
    regular_compress(&inf, sst_stream, dest);
    sst_stream->curr_bytes = 0;
    regular_decompress(&inf, dest, sst_stream);
    TEST_ASSERT_EQUAL_INT(sst_stream->curr_bytes, bytes);
    end();
}
void test_dict_compress(){
    start();
    char * meta = "meta.bin";
    char * bloom = "bloom.bin";
    storage_engine * db = create_engine(meta, bloom);
    /*dict compression is only good for similar entries, hence this instead of random value calls*/
    create_a_bigbase(db);
    sst_f_inf * first_sst = at(db->meta->sst_files[0], 0);
    read_entire_sst(sst_stream, first_sst);
    compress_tr_inf  inf;
    inf.buffer = dest;
    inf.sst = first_sst;
    inf.sizes = List(0, sizeof(size_t), false);
    prep_dict_compression(&inf, first_sst->block_indexs, sst_stream);
    int res = compress_dict(inf.sst, sst_stream, dest);
    TEST_ASSERT_NOT_EQUAL_INT(-1, res);
    end();
}
void test_dict_decompress(){
    start();
    end();
}