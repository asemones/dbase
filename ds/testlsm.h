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
void test_write_engine(void){
    storage_engine *l = create_engine("meta.bin", "bloom.bin");
    keyword_entry entry = {"hello", "world"};

    write_record(l, &entry,strlen(entry.keyword)+ strlen(entry.value));
    char * found = read_record(l, "hello", NULL,NULL,NULL);
    TEST_ASSERT_EQUAL_STRING("world", found);
    free_engine(l, "meta.bin", "bloom.bin");
    remove("meta.bin");
    remove("bloom.bin");
    remove("db.bin");
    remove ("sst_0_0.bin");
}
void test_write_then_read_engine(void){
    storage_engine *l = create_engine("meta.bin", "bloom.bin");
    const int size = 3000;
    keyword_entry entry [size];
    for (int i = 0; i < size; i++){
        char * key = (char*)wrapper_alloc((16), NULL,NULL);
        char * value = (char*)wrapper_alloc((16), NULL,NULL);
        entry[i].keyword = key;
        entry[i].value = value;

        sprintf(key, "hello%d", i);
        sprintf(value, "world%d", i);

        write_record(l, &entry[i],strlen(entry[i].keyword)+ strlen(entry[i].value));
        //free(key);
        //free(value);
    }
    for(int i = 0; i < size; i++){
        char * found = read_record(l, entry[i].keyword, NULL,NULL,NULL );
        TEST_ASSERT_EQUAL_STRING(entry[i].value, found);
    }
    //pretty_print_b_tree(l->table->tree);
    free_engine(l, "meta.bin", "bloom.bin");
    storage_engine *l2 = create_engine("meta.bin", "bloom.bin");
    keyword_entry entry2 [size];
    
    for (int i = 0; i < size; i++){
        char * key = (char*)wrapper_alloc((16), NULL,NULL);
        char * value = (char*)wrapper_alloc((16), NULL,NULL);
        entry2[i].keyword = key;
        entry2[i].value = value;

        sprintf(key, "hello%d", i);
        sprintf(value, "world%d", i);
    }
    dict * read_dict = Dict();
    byte_buffer * buffer = request_struct(l2->read_pool);
    byte_buffer * key_values = request_struct(l2->read_pool);
    for(int i = 0; i < size; i++){
        if (strcmp(entry2[i].keyword, "hello99")==0){
            int pl = 0;
        }
        char * found = read_record(l2, entry2[i].keyword, read_dict, buffer, key_values);
        TEST_ASSERT_EQUAL_STRING(entry2[i].value, found);
        reset_buffer(buffer);
        reset_buffer(key_values);
    }
    return_struct(l2->read_pool, buffer, &reset_buffer);
    return_struct(l2->read_pool, key_values, &reset_buffer);
    keyword_entry entry3 = {"hellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohellohello", "worldworldworldworldworldworldworldworldworldworldworldworldworldworldworldworldworld"};
    write_record(l2,&entry3,strlen(entry3.keyword)+ strlen(entry3.value));
    free_engine(l2, "meta.bin", "bloom.bin");
    remove("meta.bin");
    remove("bloom.bin");
    remove("db.bin");
    remove ("sst_0_0.bin");
    remove ("sst_1_0.bin");
    for (int i = 0; i < size; i++){
        free(entry[i].keyword);
        free(entry[i].value);
        free(entry2[i].keyword);
        free(entry2[i].value);
    }
    free_dict_no_element(read_dict);
}
