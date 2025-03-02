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
#define PREFIX_LENGTH 5


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
        k[i].len = strlen(key);
        v[i].len = strlen(value);

        write_record(l, k[i],v[i]);
    }

    lock_table(l);
    flush_table(l);

   
    for (int i = 0; i < size; i++) {
        free(k[i].entry);
        free(v[i].entry);
    }
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

        k[i].len = strlen(key);
        v[i].len = strlen(value);
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
void create_a_bigbase(storage_engine * l) {
    const int size = 60000;
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
        k[i].len = strlen(key);
        v[i].len = strlen(value);

        write_record(l, k[i],v[i]);
    }
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

        k[i].len = strlen(key);
        v[i].len = strlen(value);
        write_record(l, k[i],v[i]); 
    }
}
storage_engine * create_messy_db(){
    create_a_babybase();
    storage_engine * l = create_engine("meta.bin", "bloom.bin");
    compact_manager* cm = init_cm(l->meta, l->cach);
    sst_f_inf * victim = at(cm->sst_files[0], 0);
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
        k.len = strlen(k.entry);
        v.len= strlen(v.entry);
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
    remove ("WAL_0.bin");
    remove("WAL_1.bin");
    remove("WAL_M.bin");
}


void generate_random_prefix(char* buffer, size_t prefix_len) {
    const char *allowed = "abcdefghijklmnopqrstuvwxyz0123456789";
    for (size_t i = 0; i < prefix_len; i++) {
        buffer[i] = allowed[rand() % 36];
    }
    buffer[prefix_len] = '\0';
}
void write_random_units(byte_buffer *b, const int iters, const int prefix_size) {
    for (int i = 0; i < iters; i++){
        char buf[256];  

        db_unit key_unit;
        db_unit value_unit;

        generate_random_prefix(buf, prefix_size);
        key_unit.entry = buf;
        key_unit.len = strlen(buf);
        write_db_unit(b, key_unit);

        memset(buf, 0, sizeof(buf));

        generate_random_prefix(buf, prefix_size);
        value_unit.entry = buf;
        value_unit.len = strlen(buf);
        write_db_unit(b, value_unit);
    }
}
void read_entire_sst(byte_buffer * b, sst_f_inf * inf){
    FILE * file=  fopen(inf->file_name, "rb");
    int read=  fread(b->buffy, 1, inf->length,file);
    b->curr_bytes += read;
}
void add_random_records_a_z(storage_engine *l, const int size) {
   
    srand((unsigned int)time(NULL));

    db_unit * k = malloc(sizeof(db_unit)* size);
    db_unit  * v = malloc(sizeof(db_unit)* size);


    for (int i = 0; i < size; i++) {
        char *key = (char*)wrapper_alloc(30, NULL, NULL);
        char *value = (char*)wrapper_alloc(30, NULL, NULL);
        k[i].entry = key;
        v[i].entry = value;

        
        char prefix[PREFIX_LENGTH + 1];
        generate_random_prefix(prefix, PREFIX_LENGTH);

    
        sprintf(key, "%s_key_%d", prefix, i);
        sprintf(value, "first_value%d", i);

        k[i].len = strlen(key);
        v[i].len = strlen(value);

        write_record(l, k[i], v[i]);
    }

    lock_table(l);
    flush_table(l);

    for (int i = 0; i < size; i++) {
        free(k[i].entry);
        free(v[i].entry);
    }

    for (int i = 0; i < size; i++) {
        char *key = (char*)wrapper_alloc(30, NULL, NULL);
        char *value = (char*)wrapper_alloc(30, NULL, NULL);
        k[i].entry = key;
        v[i].entry = value;

    
        char prefix[PREFIX_LENGTH + 1];
        generate_random_prefix(prefix, PREFIX_LENGTH);

        sprintf(key, "%s_key_%d", prefix, i);
        sprintf(value, "second_value%d", i);

        k[i].len = strlen(key);
        v[i].len = strlen(value);

        write_record(l, k[i], v[i]);
    }

    lock_table(l);
    flush_table(l);  
    
    for (int i = 0; i < size; i++) {
         free(k[i].entry);
         free(v[i].entry);
    }
    free(k);
    free(v);
}
void remove_ssts(list ** sst2){
    for(int j = 0; j < LEVELS; j++){
        list * ssts= sst2[j];
        for(int i = 0; i <ssts->len; i++){
            sst_f_inf * sst= at(ssts, i);
            int res = remove(sst->file_name);
            if (res!=0){
                fprintf(stdout, "%s failed at %d level\n",sst->file_name, j);
            }
        }
    }
}
