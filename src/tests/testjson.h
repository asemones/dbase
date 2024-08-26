#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unity/src/unity.h"
#include "../db/backend/json.h"
#include "../ds/hashtbl.h"
#include "../ds/byte_buffer.h"
#include "../util/alloc_util.h"
#pragma once
void test_serialize_dict(void) {
    
    dict *d = Dict();
    const char *key = "key";
    const char *value = "value";
    add_kv(d, (void*)key, (void*)value);
    byte_buffer * buffer = create_buffer(1024);

    seralize_dict(d, buffer, STRING);


    TEST_ASSERT_EQUAL_STRING("{\"key\":\"value\"}", buffer->buffy);
    free_dict_no_element(d);
    free_buffer(buffer);
}

void test_deserialize_dict(void) {
    
    char json[] = "{\"key\":\"value\",\"no\":\"value2\"}";
    byte_buffer * buffer = create_buffer(1024);
    dict *d = Dict();
    write_buffer(buffer, json, strlen(json));

    deseralize_into_structure(&add_kv_void, d,buffer);
    TEST_ASSERT_EQUAL_STRING("value", get_v(d, (void*)"key"));
    free_dict_no_element(d);
}
void test_write_read_json(void){
    dict *d = Dict();
        

    const char *key = "key";
    const char *value = "value";
    const char *key2 = "key2";
    const char *value2 = "value2";
    add_kv(d, (void*)key, (void*)value);
    add_kv(d, (void*)key2, (void*)value2);
    byte_buffer * buffer = create_buffer(1024);

    seralize_dict(d, buffer,STRING);
    write_file(buffer->buffy, "test.bin", "wb", 1, buffer->curr_bytes);

    byte_buffer* buffer2 = create_buffer(1024);
    int l= read_file(buffer2->buffy,"test.bin","rb", 1, buffer->curr_bytes);
    fprintf(stderr, "read %d bytes\n", l);
    deseralize_into_structure(&add_kv_void, d,buffer2);
    TEST_ASSERT_EQUAL_STRING("value", get_v(d, (void*)"key"));
    TEST_ASSERT_EQUAL_STRING("value2", get_v(d, (void*)"key2"));
    free_buffer(buffer);
    free_dict_no_element(d);
}
void test_numeric_read_write(void){
    dict *d = Dict();
    const char *key = "key";
    size_t value = 123;
    add_kv(d, (void*)key, (void*)&value);
    byte_buffer* buffer = create_buffer(1024);
    byte_buffer* buffer2 = create_buffer(1024);
    seralize_dict(d, buffer, NUMBER);
    write_file(buffer->buffy, "test.bin", "wb", 1, buffer->curr_bytes);
    read_file(buffer2->buffy,"test.bin","rb", 1,buffer->curr_bytes);
    deseralize_into_structure(&add_kv_void, d,buffer2);
    TEST_ASSERT_EQUAL_INT(value, *((size_t*)get_v(d, (void*)"key")));
    free_buffer(buffer);
    free_buffer(buffer2);
    free_dict_no_element(d);


}
