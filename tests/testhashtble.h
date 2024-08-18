#pragma once
#include "./unity/src/unity.h" 
#include <stdlib.h>
#include <string.h>
#include "../ds/hashtbl.h"
#include "../util/alloc_util.h"




void test_fnv1a_64() {
    // Test with empty string
    const char* data_empty = "";
    uint64_t hash_empty = fnv1a_64(data_empty, strlen(data_empty));
    TEST_ASSERT_NOT_EQUAL(0, hash_empty);

    // Test with simple string
    const char* data_test = "test";
    uint64_t hash_test = fnv1a_64(data_test, strlen(data_test));
    TEST_ASSERT_NOT_EQUAL(0, hash_test);

    // Test with longer string
    const char* data_long = "a much longer string for hashing";
    uint64_t hash_long = fnv1a_64(data_long, strlen(data_long));
    TEST_ASSERT_NOT_EQUAL(0, hash_long);

    // Test with special characters
    const char* data_special = "!@#$%^&*()_+-=";
    uint64_t hash_special = fnv1a_64(data_special, strlen(data_special));
    TEST_ASSERT_NOT_EQUAL(0, hash_special);

    // Test with null data (should handle gracefully, assuming fnv1a_64 does not crash on NULL)
    uint64_t hash_null = fnv1a_64(NULL, 0);
    TEST_ASSERT_NOT_EQUAL(0, hash_null);  // Ensure no zero hash even for NULL input
}

void test_dict_creation() {
    dict* myDict = Dict();
    TEST_ASSERT_NOT_NULL(myDict);
    TEST_ASSERT_EQUAL(0, myDict->size);

    // Test if newly created dict is empty
    TEST_ASSERT_NULL(get_v(myDict, (void*)"nonexistent_key"));

    free_dict(myDict);
}

void test_addKV_and_getV() {
    dict* myDict = Dict();
    char* key = strdup("key1");
    char* value = strdup("value1");

    add_kv(myDict, key, value);
    TEST_ASSERT_EQUAL_STRING(value, get_v(myDict, key));

    // Test overwrite value
    char* newValue = strdup("newValue1");
    add_kv(myDict, key, newValue);
    TEST_ASSERT_EQUAL_STRING(newValue, get_v(myDict, key));

    // Test multiple keys
    char* key2 = strdup("key2");
    char* value2 = strdup("value2");
    add_kv(myDict, key2, value2);
    TEST_ASSERT_EQUAL_STRING(value2, get_v(myDict, key2));

    free_dict(myDict);
}

void test_removeKV() {
    dict* myDict = Dict();
    char* key = strdup("key1");
    char* value = strdup("value1");

    add_kv(myDict, key, value);
    remove_kv(myDict, key);
    TEST_ASSERT_NULL(get_v(myDict, "key1"));

    // Test removing non-existent key
    remove_kv(myDict, (void*)"nonexistent_key");
    TEST_ASSERT_NULL(get_v(myDict, (void*)"nonexistent_key"));

    // Test removing key from an empty dict
    dict* emptyDict = Dict();
    remove_kv(emptyDict, (void*)"another_nonexistent_key");
    TEST_ASSERT_NULL(get_v(emptyDict, (void*)"another_nonexistent_key"));

    free_dict(myDict);
    free_dict(emptyDict);
}
void test_freeDict() {
    dict* myDict = Dict();
    char* key1 = strdup("key1");
    char* value1 = strdup("value1");
    char* key2 = strdup("key2");
    char* value2 = strdup("value2");

    add_kv(myDict, key1, value1);
    add_kv(myDict, key2, value2);
    free_dict(myDict);

   
    dict* emptyDict = Dict();
    free_dict(emptyDict);


}
