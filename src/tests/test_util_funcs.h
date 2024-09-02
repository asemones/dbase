#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../db/backend/compactor.h"
#include "../db/backend/lsm.h"
#include "../ds/byte_buffer.h"
#include "unity/src/unity.h"
#include "../ds/arena.h"
#include "../util/alloc_util.h"
#include "../db/backend/key-value.h"
#pragma once


void create_a_babybase(void) {
    char* meta_file = "meta.bin";
    char* bloom_file = "bloom.bin";
    storage_engine *l = create_engine(meta_file, bloom_file);
    const int size = 6000;
    db_unit k[size];
    db_unit v[size];
    for (int i = 0; i < size; i++) {
        char *key = (char*)wrapper_alloc(30, NULL, NULL);
        char *value = (char*)wrapper_alloc(30, NULL, NULL);
        k[i].entry = key;
        v[i].entry=  value;

        if (i % 3 == 0) {
            sprintf(key, "common%d", i / 3); 
            sprintf(value, "first_value%d", i / 3);
        } else {
            sprintf(key, "hello%d", i);
            sprintf(value, "world%d", i);
        }
        k[i].len = strlen(key)+1;
        v[i].len = strlen(value)+1;

        write_record(l, k[i],v[i]);
    }

    lock_table(l);
    flush_table(l);

   
    for (int i = 0; i < size; i++) {
        free(k[i].entry);
        free(v[i].entry);
    }

    usleep(1000000);
    for (int i = 0; i < size; i++) {
        char *key = (char*)wrapper_alloc(30, NULL, NULL);
        char *value = (char*)wrapper_alloc(30, NULL, NULL);
        k[i].entry = key;
        v[i].entry=  value;
        if (i % 3 == 0) {
            sprintf(key, "common%d", i / 3); 
            sprintf(value, "second_value%d", i / 3);  
        } else {
            sprintf(key, "key%d", i);
            sprintf(value, "value%d", i);
        }

        k[i].len = strlen(key)+1;
        v[i].len = strlen(value)+1;
        write_record(l, k[i],v[i]); 
    }
    lock_table(l);
    flush_table(l);

    // Free allocated memory for the second table
    for (int i = 0; i < size; i++) {
         free(k[i].entry);
         free(v[i].entry);
    }

    free_engine(l, meta_file, bloom_file);
}
storage_engine * create_messy_db(){
    create_a_babybase();
    storage_engine * l = create_engine("meta.bin", "bloom.bin");
    compact_manager* cm = init_cm(l->meta, l->cach);
    compact_one_table(cm, 0,0,1,0);
    compact_one_table(cm, 1,1,6,1);
    for (int i = 6000; i < 9000; i++) {
        char *key = (char*)wrapper_alloc(60, NULL, NULL);
        char *value = (char*)wrapper_alloc(60, NULL, NULL);
        db_unit k;
        db_unit v;
        k.entry = key;
        v.entry= value;
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
        k.len = strlen(k.entry) + 1;
        v.len= strlen(v.entry)+1;
        write_record(l, k,v);
        
    }
    return l;
}
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
    remove ("sst_4_7");
    remove("sst_5_7");
    remove ("in_prog_sst_3_0");
    remove ("in_prog_sst_4_0");
    remove ("in_prog_sst_5_0");
    remove ("in_prog_sst_6_0");
}