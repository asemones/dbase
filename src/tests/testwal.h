#include "./unity/src/unity.h"  
#include <string.h>
#include <stdlib.h>
#include "../db/backend/WAL.h"
#pragma once




void test_wal_init(){
    byte_buffer * b = create_buffer(1024);
    WAL * w = init_WAL(b);
    TEST_ASSERT_NOT_NULL(w);
    TEST_ASSERT_NOT_NULL(w->wal_buffer);
    reset_buffer(b);
    kill_WAL(w,b);
    free_buffer(b);
}