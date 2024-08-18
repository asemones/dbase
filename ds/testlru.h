#include "unity/src/unity.h"
#include "../ds/cache.h"

#pragma once


void test_cache_init(){
    cache * c = create_cache(1024*1024, 2048);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_EQUAL_INT(c->filled_pages, 0);
    TEST_ASSERT_EQUAL_INT((1024 * 1024),c->capacity);
    ll_node * head=  c->queue->head;
    for (int i = 0; i < c->max_pages; i++){
        TEST_ASSERT_NOT_NULL(head);
        byte_buffer * buf = (byte_buffer *) head->data;
        TEST_ASSERT_EQUAL_INT(buf->curr_bytes, 0);
        head = head->next;

    }
    free_cache(c);
}
void test_cache_add_and_get(){
    cache * c = create_cache(1024*1024, 2048);
    FILE * dummy = fopen("dummy.bin", "wb+");
    for (int i = 0;  i < 1024; i++){
        fwrite("ab", 2, 1, dummy);
    }
    rewind(dummy);
    add_page(c, dummy, "no");
    byte_buffer* test= (byte_buffer*)get_page(c, "no");
    TEST_ASSERT_NOT_NULL(test);
    TEST_ASSERT_EQUAL_INT(2048, test->curr_bytes);
    remove("dummy.bin");
    free_cache(c);
}
void test_cache_evict(){
    cache * c = create_cache(1024*1024, 2048);
    FILE * dummy = fopen("dummy.bin", "wb+");
    for (int i = 0;  i < 1024; i++){
        fwrite("ab", 2, 1, dummy);
    }
    rewind(dummy);
    add_page(c, dummy,"lm");
    remove("dummy.bin");
    evict(c);
    TEST_ASSERT_NULL(get_page(c,"lm"));
    free_cache(c);
}
