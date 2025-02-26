#include "./unity/src/unity.h"  
#include <string.h>
#include <stdlib.h>
#include "../db/backend/WAL.h"
#pragma once




void test_wal_init(){
    byte_buffer * b = create_buffer(1024);
    WAL * w = init_WAL(b);
    TEST_ASSERT_NOT_NULL(w);
    TEST_ASSERT_NOT_NULL(w->wal_buffer);
    reset_buffer(b);
    kill_WAL(w,b);
    free_buffer(b);
}

WAL* test_wal;
byte_buffer* test_buffer;

void init_test(void) {

    test_wal = malloc(sizeof(WAL));
    test_wal->wal_buffer = mem_aligned_buffer(GLOB_OPTS.WAL_BUFFERING_SIZE, FS_B_SIZE);
    test_wal->fn_buffer = create_buffer(MAX_WAL_FN_LEN * (GLOB_OPTS.MAX_WAL_FILES + 1));
    test_wal->fn = List(0, sizeof(char*), false);
    test_wal->len = 10;
    test_wal->curr_fd_bytes = 1024;
    
    test_buffer = create_buffer(1024);
}

void clean(void) {
    
    free_buffer(test_wal->wal_buffer);
    free_buffer(test_wal->fn_buffer);
    free_list(test_wal->fn, NULL);
    free(test_wal);
    free_buffer(test_buffer);
    remove("WAL_M.bin");
}

void test_seralize_wal(void) {
    init_test();
    char test_filename[] = "test_wal.bin";
    insert(test_wal->fn, test_filename);
    write_buffer(test_wal->fn_buffer, test_filename, strlen(test_filename) + 1);

   
    seralize_wal(test_wal, test_buffer);
    char* file_name_len = buf_ind(test_buffer, 8);
    TEST_ASSERT_EQUAL_INT(1, *file_name_len);
    char * file_name = buf_ind(test_buffer, 28);
    TEST_ASSERT_EQUAL_STRING(test_filename, file_name);
    
    clean();
}

void test_deseralize_wal(void) {
    init_test();

    size_t len = 20;
    int fn_list_len = 2;
    size_t curr_fd_bytes = 2048;
 
    size_t fn_buffer_bytes = MAX_WAL_FN_LEN * fn_list_len;

    char *test_file_name = "test_wal1.bin";
    char *test_f_n_2     = "test_wal2.bin";

    write_buffer(test_buffer, (char*)&len, sizeof(len));
    write_buffer(test_buffer, (char*)&fn_list_len, sizeof(fn_list_len));
    write_buffer(test_buffer, (char*)&curr_fd_bytes, sizeof(curr_fd_bytes));
    write_buffer(test_buffer, (char*)&fn_buffer_bytes, sizeof(fn_buffer_bytes));

    write_buffer(test_buffer, test_file_name, strlen(test_file_name) + 1);
    test_buffer->curr_bytes += MAX_WAL_FN_LEN - (strlen(test_file_name) + 1);


    write_buffer(test_buffer, test_f_n_2, strlen(test_f_n_2) + 1);
    test_buffer->curr_bytes += MAX_WAL_FN_LEN - (strlen(test_f_n_2) + 1);

    test_wal->len = 0;
    test_wal->curr_fd_bytes = 0;
    reset_buffer(test_wal->fn_buffer);
    free_list(test_wal->fn, NULL);
    test_wal->fn = List(0, sizeof(char*), false);



    deseralize_wal(test_wal, test_buffer);


    TEST_ASSERT_EQUAL_size_t(20, test_wal->len);
    TEST_ASSERT_EQUAL_size_t(2048, test_wal->curr_fd_bytes);
    TEST_ASSERT_EQUAL_size_t(fn_buffer_bytes, test_wal->fn_buffer->curr_bytes);
    TEST_ASSERT_EQUAL_INT(2, test_wal->fn->len);
    TEST_ASSERT_EQUAL_STRING("test_wal1.bin", *(char**)at(test_wal->fn, 0));
    TEST_ASSERT_EQUAL_STRING("test_wal2.bin", *(char**)at(test_wal->fn, 1));
    clean();
}

void test_write_WAL(void) {
    init_test();
    db_unit test_key = {.entry = "hello", .len = strlen("hello")+1};
    db_unit test_value = {.entry = "key1", .len=  strlen("key1")+1};


    int bytes_written = write_WAL(test_wal, test_key, test_value);

   
    TEST_ASSERT_EQUAL_INT(0,bytes_written);
    TEST_ASSERT_EQUAL_size_t(11, test_wal->len); 
    clean();
}