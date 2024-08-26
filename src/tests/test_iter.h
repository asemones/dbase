#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unity/src/unity.h"
#include "../ds/arena.h"
#include "../util/alloc_util.h"
#include "test_util_funcs.h"
#include "../db/backend/iter.h"
#pragma once


/* really just want no crashing*/
void test_init_iters(){
    clean_test_files();
    aseDB_iter * it=  create_aseDB_iter();
    create_a_babybase();
    storage_engine * f = create_engine("meta.bin", "bloom.bin");
    init_aseDB_iter(it,f);
    free_engine(f,"meta.bin", "bloom.bin");
    clean_test_files();

    free_aseDB_iter(it);
}
void test_seek(){
    aseDB_iter * it=  create_aseDB_iter();
    create_a_babybase();
    storage_engine * f = create_engine("meta.bin", "bloom.bin");
    compact_manager * cm = init_cm(f->meta);
    compact_one_table(cm, 0,0,1,0);
    init_aseDB_iter(it, f);
    seek(it, "k");
    clean_test_files();

    free_aseDB_iter(it);
}
void test_next(){
    aseDB_iter * it=  create_aseDB_iter();
    create_a_babybase();
    storage_engine * f = create_engine("meta.bin", "bloom.bin");
    compact_manager * cm = init_cm(f->meta);
    compact_one_table(cm, 0,0,1,0);
    init_aseDB_iter(it, f);
    seek(it, "k");
    merge_data m = aseDB_iter_next(it);
    char * next= m.value;
    TEST_ASSERT_EQUAL_STRING("value1",next);
    m = aseDB_iter_next(it);
    next = m.value;
    TEST_ASSERT_EQUAL_STRING("value10",next);
    clean_test_files();

    free_aseDB_iter(it);
}
void test_run_scan(){
    aseDB_iter * it=  create_aseDB_iter();
    create_a_babybase();
    storage_engine * f = create_engine("meta.bin", "bloom.bin");
    compact_manager * cm = init_cm(f->meta);
    compact_one_table(cm, 0,0,1,0);
    compact_one_table(cm, 1,1,2,0);
    compact_one_table(cm,1,2,6,0);
    init_aseDB_iter(it, f);
    seek(it, "c");
    for (merge_data next = aseDB_iter_next(it); ((char*)next.key)[0] < 'k'; next = aseDB_iter_next(it)){
        TEST_ASSERT_NOT_NULL(next.value);
        fprintf(stdout,"%s\n", next.value);
        if (strcmp(next.key, "EOTBL")== 0){
            break;
        }
    }
    clean_test_files();

    free_aseDB_iter(it);
    free_engine(f, "meta.bin", "bloom.bin");
}
void test_full_table_scan_messy(){
    storage_engine * s = create_messy_db();
    aseDB_iter * it=  create_aseDB_iter();
    init_aseDB_iter(it,s);
    seek(it, "c");
    for(merge_data next = aseDB_iter_next(it);;next = aseDB_iter_next(it)){
        TEST_ASSERT_NOT_NULL(next.value);
        fprintf(stdout,"%s\n", next.value);
        if (strcmp(next.key, "EOTBL")== 0){
            break;
        }
    }
    clean_test_files();

    free_aseDB_iter(it);
    free_engine(s, "meta.bin", "bloom.bin");
}























