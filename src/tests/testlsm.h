#include "unity/src/unity.h"
#include "../db/backend/lsm.h"
#include <string.h>
#include "../util/alloc_util.h"


void test_create_engine(void) {
    storage_engine *l = create_engine("meta.bin", "bloom.bin");
    TEST_ASSERT_NOT_NULL(l);
    TEST_ASSERT_NOT_NULL(l->meta);
    TEST_ASSERT_NOT_NULL(l->table[0]);
    free_engine(l, "meta.bin", "bloom.bin");
    remove("meta.bin");
    remove("bloom.bin");
    remove("db.bin");
}

void test_write_engine(void) {
    storage_engine *l = create_engine("meta.bin", "bloom.bin");
    db_unit key = {.entry = "hello", .len = 5};
    db_unit value = {.entry = "world", .len = 5};
    write_record(l, key, value);
    char * found = read_record(l, key.entry);
    TEST_ASSERT_EQUAL_STRING("world", found);
    free_engine(l, "meta.bin", "bloom.bin");
    remove("meta.bin");
    remove("bloom.bin");
    remove("db.bin");
    remove("sst_0_0.bin");
}

void test_write_then_read_engine(void) {
    storage_engine *l = create_engine("meta.bin", "bloom.bin");
    const int size = 3000;
    db_unit keys[size];
    db_unit values[size];

    for (int i = 0; i < size; i++) {
        char *key = (char*)wrapper_alloc(16, NULL, NULL);
        char *value = (char*)wrapper_alloc(16, NULL, NULL);
        sprintf(key, "hello%d", i);
        sprintf(value, "world%d", i);
        keys[i].entry = key;
        keys[i].len = strlen(key) + 1;
        values[i].entry = value;
        values[i].len = strlen(value) + 1;
        write_record(l, keys[i], values[i]);
    }

    for (int i = 0; i < size; i++) {
         char * found = read_record(l, keys[i].entry);
        TEST_ASSERT_EQUAL_STRING(values[i].entry, found);
        
    }
    

    free_engine(l, "meta.bin", "bloom.bin");

    storage_engine *l2 = create_engine("meta.bin", "bloom.bin");

    for (int i = 0; i < size; i++) {
        char * found = read_record(l2, keys[i].entry);
        TEST_ASSERT_EQUAL_STRING(values[i].entry, found);
    }

    db_unit long_key = {
        .entry = "hellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohello",
        .len = 85 + 1
    };
    db_unit long_value = {
        .entry = "worldworldworldworldworldworldworldworldworldworldworldworldworldworldworldworldworld",
        .len = 85 + 1
    };
    write_record(l2, long_key, long_value);

    free_engine(l2, "meta.bin", "bloom.bin");
    remove("meta.bin");
    remove("bloom.bin");
    remove("db.bin");
    remove("sst_0_0.bin");
    remove("sst_1_0.bin");

    for (int i = 0; i < size; i++) {
        free((void*)keys[i].entry);
        free((void*)values[i].entry);
    }
}