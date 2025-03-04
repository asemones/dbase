#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unity/src/unity.h"
#include "../ds/arena.h"
#include "../util/alloc_util.h"
#include "test_util_funcs.h"
#include "../db/backend/iter.h"
#include "../db/backend/lsm.h"
#pragma once


/* really just want no crashing*/
void test_init_iters(){
    clean_test_files();
    aseDB_iter * it=  create_aseDB_iter();
    create_a_babybase();
    storage_engine * f = create_engine("meta.bin", "bloom.bin");
    init_aseDB_iter(it,f);
    remove_ssts(f->meta->sst_files);
    free_engine(f,"meta.bin", "bloom.bin");
    clean_test_files();
    free_aseDB_iter(it);
}
void test_seek(){
    aseDB_iter * it=  create_aseDB_iter();
    create_a_babybase();
    storage_engine * f = create_engine("meta.bin", "bloom.bin");
   

    init_aseDB_iter(it, f);
    seek(it, "k");
    clean_test_files();
    remove_ssts(f->meta->sst_files);
    free_aseDB_iter(it);
}
void test_next(){
    aseDB_iter * it=  create_aseDB_iter();
    create_a_babybase();
    storage_engine * f = create_engine("meta.bin", "bloom.bin");
  
    init_aseDB_iter(it, f);
    seek(it, "k");
    merge_data m = aseDB_iter_next(it);
    char * next= m.value->entry;
    TEST_ASSERT_EQUAL_STRING("value1",next);
    m = aseDB_iter_next(it);
    next = m.value->entry;
    TEST_ASSERT_EQUAL_STRING("value10",next);
    remove_ssts(f->meta->sst_files);
    clean_test_files();

    free_aseDB_iter(it);
}
void test_run_scan(){
    aseDB_iter * it=  create_aseDB_iter();
    create_a_babybase();
    storage_engine * f = create_engine("meta.bin", "bloom.bin");

    init_aseDB_iter(it, f);
    seek(it, "c");
    for (merge_data next = aseDB_iter_next(it); ((char*)next.key->entry)[0] < 'k'; next = aseDB_iter_next(it)){
        TEST_ASSERT_NOT_NULL(next.value);
     
        if (next.index == -1){
            break;
        }
    }
    remove_ssts(f->meta->sst_files);
    free_aseDB_iter(it);
    free_engine(f, "meta.bin", "bloom.bin");
    clean_test_files();
}
void test_full_table_scan_messy(){
    storage_engine * s = create_messy_db();
    aseDB_iter * it=  create_aseDB_iter();
    init_aseDB_iter(it,s);
    seek(it, "c");
    for(merge_data next = aseDB_iter_next(it); ;next = aseDB_iter_next(it)){
        if (next.key== EODB){
            break;
        }
    }
    remove_ssts(s->meta->sst_files);
    free_aseDB_iter(it);
    free_engine(s, "meta.bin", "bloom.bin");
    clean_test_files();
}























