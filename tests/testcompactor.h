#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../db/backend/compactor.h"
#include "../db/backend/lsm.h"
#include "../ds/byte_buffer.h"
#include "unity/src/unity.h"
#include "../ds/arena.h"
#include "../util/alloc_util.h"
#pragma once


void clean_test_files(void){
    remove("db.bin");
    remove("meta.bin");
    remove("bloom.bin");
    remove ("sst_0_0");
    remove ("sst_1_0");
    remove ("sst_2_0");
    remove ("sst_3_0");
    remove ("sst_4_0");
    remove ("sst_0_1");
    remove ("sst_1_1");
    remove ("sst_2_1");
    remove ("sst_3_1");
    remove ("sst_4_1");
    remove ("in_prog_sst_3_0");
    remove ("in_prog_sst_4_0");
    remove ("in_prog_sst_5_0");
    remove ("in_prog_sst_6_0");
}
void create_a_babybase(void) {
    char* meta_file = "meta.bin";
    char* bloom_file = "bloom.bin";
    storage_engine *l = create_engine(meta_file, bloom_file);
    const int size = 6000;
    keyword_entry entry[size];
    for (int i = 0; i < size; i++) {
        char *key = (char*)wrapper_alloc(30, NULL, NULL);
        char *value = (char*)wrapper_alloc(30, NULL, NULL);
        entry[i].keyword = key;
        entry[i].value = value;

        if (i % 3 == 0) {
            sprintf(key, "common%d", i / 3); 
            sprintf(value, "first_value%d", i / 3);
        } else {
            sprintf(key, "hello%d", i);
            sprintf(value, "world%d", i);
        }

        write_record(l, &entry[i], strlen(entry[i].keyword) + strlen(entry[i].value));
    }

    lock_table(l);
    flush_table(l);

   
    for (int i = 0; i < size; i++) {
        free(entry[i].keyword);
        free(entry[i].value);
    }

    usleep(1000000);
    for (int i = 0; i < size; i++) {
        char *key = (char*)wrapper_alloc(30, NULL, NULL);
        char *value = (char*)wrapper_alloc(30, NULL, NULL);
        entry[i].keyword = key;
        entry[i].value = value;

        if (i % 3 == 0) {
            sprintf(key, "common%d", i / 3); 
            sprintf(value, "second_value%d", i / 3);  
        } else {
            sprintf(key, "key%d", i);
            sprintf(value, "value%d", i);
        }

        write_record(l, &entry[i], strlen(entry[i].keyword) + strlen(entry[i].value));
    }

    lock_table(l);
    flush_table(l);

    // Free allocated memory for the second table
    for (int i = 0; i < size; i++) {
        free(entry[i].keyword);
        free(entry[i].value);
    }

    free_engine(l, meta_file, bloom_file);
}
void certify_babybase(storage_engine * l) {
    const int size = 6000;
    keyword_entry entry;
    char expected_key[30];
    char expected_value[30];
    dict *read = Dict();
    byte_buffer *b = create_buffer(10000);
    byte_buffer *b2 = create_buffer(10000);
    for (int i = 0; i < size; i++) {
        char key[30] = {0};
        char value[30] = {0};
        entry.keyword = key;
        entry.value = value;

        if (i % 3 == 0) {
            sprintf(expected_key, "common%d", i / 3);
            sprintf(expected_value, "second_value%d", i / 3);  
        } else {
            sprintf(expected_key, "key%d", i);
            sprintf(expected_value, "value%d", i);
        }
        char * v = read_record(l, expected_key, read,b,b2 );
        TEST_ASSERT_EQUAL_STRING(expected_value, v);
        reset_buffer(b);
        reset_buffer(b2);
    }
    free_dict_no_element(read);
    printf("Database certification passed successfully.\n");
}

void test_merge_tables(void){
    create_a_babybase();
    storage_engine * l = create_engine("meta.bin", "bloom.bin");
    //set_buffer(l->table, NULL);
    compact_infos * compact[2];
   
    for (int i = 0; i < 2; i++){
       compact[i]= (compact_infos*)wrapper_alloc((sizeof(compact_infos)), NULL,NULL);
       compact[i]->sst_file = get_element(l->meta->sst_files[0], i);
       compact[i]->complete = false;
       compact[i]->buffer = create_buffer(100000);

    }
    //load_compaction(compact, 2);
    byte_buffer * dest_buffer = create_buffer(1000000);
    arena * a = calloc_arena(1000000);
    compact_job job;
    job.start_level = 0;
    job.end_level = 0;
    job.status = 0;
    job.maxiumum_file_size = 64 *KB;
    job.new_sst_files = thread_safe_list(0,sizeof(sst_f_inf),false);
    job.id = 3;
    job.working_files= 2;
    merge_tables(compact,dest_buffer ,&job, a);
    TEST_ASSERT_NOT_NULL(dest_buffer->buffy);

    

    free(compact[0]);
    free(compact[1]);
    free_buffer(dest_buffer);
    free_engine(l,"meta.bin","bloom.bin");
    clean_test_files();
}
void test_merge_one_table(void){
    create_a_babybase();
    storage_engine * l = create_engine("meta.bin", "bloom.bin");
    compact_manager* cm = init_cm(l->meta);
    compact_one_table(cm, 0,0,0,0);
    certify_babybase(l);
    free_engine(l,"meta.bin","bloom.bin");
    clean_test_files();
    
}
void test_merge_0_to_empty_one(void){
    create_a_babybase();
    storage_engine * l = create_engine("meta.bin", "bloom.bin");
    compact_manager* cm = init_cm(l->meta);
    compact_one_table(cm, 0,0,1,0);
    certify_babybase(l);
    free_engine(l,"meta.bin","bloom.bin");
    clean_test_files();
}
void test_merge_0_to_filled_one(void){
    create_a_babybase();
    storage_engine * l = create_engine("meta.bin", "bloom.bin");
    compact_manager* cm = init_cm(l->meta);
    compact_one_table(cm, 0,0,1,0);
    certify_babybase(l);
    for (int i = 6000; i < 9000; i++) {
        char *key = (char*)wrapper_alloc(60, NULL, NULL);
        char *value = (char*)wrapper_alloc(60, NULL, NULL);
        keyword_entry entry;
        entry.keyword = key;
        entry.value = value;
        if (i % 3 == 0) {
            sprintf(key, "common%d", i / 3); 
            sprintf(value, "third_value%d", i / 3);
        }
        else if (i % 2 == 0) {
            sprintf(key, "i want to break your database%d", i / 2); 
            sprintf(value, "break break break%d", i / 2);
        }
        else {
            sprintf(key, "key%d", i);
            sprintf(value, "value%d", i);
        }
        write_record(l, &entry, strlen(entry.keyword) + strlen(entry.value));
        
    }
    lock_table(l);
    flush_table(l);
    compact_one_table(cm, 0,1,1,0);
    free_engine(l,"meta.bin","bloom.bin");
    remove("meta.bin");
    clean_test_files();
}
void reset_engine(storage_engine ** l, compact_manager ** cm){
    free_engine(*l,"meta.bin","bloom.bin");
    free_cm(*cm);
    *l = create_engine("meta.bin", "bloom.bin");
    *cm = init_cm((*l)->meta);
}
void test_attempt_to_break_compactor(){
    create_a_babybase();
    storage_engine * l = create_engine("meta.bin", "bloom.bin");
    compact_manager* cm = init_cm(l->meta);
    compact_one_table(cm, 0,0,1,0);
    certify_babybase(l);
    for (int i = 6000; i < 9000; i++) {
        char *key = (char*)wrapper_alloc(60, NULL, NULL);
        char *value = (char*)wrapper_alloc(60, NULL, NULL);
        keyword_entry entry;
        entry.keyword = key;
        entry.value = value;
        if (i % 3 == 0) {
            sprintf(key, "common%d", i / 3); 
            sprintf(value, "third_value%d", i / 3);
        }
        else if (i % 2 == 0) {
            sprintf(key, "i want to break your database%d", i / 2); 
            sprintf(value, "break break break%d", i / 2);
        }
        else {
            sprintf(key, "key%d", i);
            sprintf(value, "value%d", i);
        }
        write_record(l, &entry, strlen(entry.keyword) + strlen(entry.value));
        
    }
    lock_table(l);
    flush_table(l);
    compact_one_table(cm, 0,1,1,0);
    reset_engine(&l, &cm);
    compact_one_table(cm, 0,1,1,0);
    reset_engine(&l, &cm);
    compact_one_table(cm, 1,2,2,0);
    reset_engine(&l, &cm);
    compact_one_table(cm, 2,3,3,0);
    reset_engine(&l, &cm);
    compact_one_table(cm, 3,4,4,0);
    TEST_ASSERT_EQUAL_STRING("break break break3001", read_record(l, "i want to break your database3001", Dict(), create_buffer(10000), create_buffer(10000)));
    compact_one_table(cm, 4,5,7,0);
    compact_one_table(cm, 1,2,7,0);
    TEST_ASSERT_EQUAL_STRING("second_value0", read_record(l, "common0", Dict(), create_buffer(10000), create_buffer(10000)));
    free_engine(l,"meta.bin","bloom.bin");
}


