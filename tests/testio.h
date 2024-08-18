#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unity/src/unity.h"
#include "../util/io.h"
#include "../util/alloc_util.h"
#pragma once

void test_writeFile(void) {
    char *data = "Hello, world!";
    size_t data_len = strlen(data) + 1; 
    char *file_name = "test_output.txt";
    
    int result = write_file(data, file_name, "w", 1, data_len);
    TEST_ASSERT_EQUAL_INT(data_len, result);

    // Verify file content
    FILE *file = fopen(file_name, "r");
    TEST_ASSERT_NOT_NULL(file);
    char read_buf[50];
    fread(read_buf, 1, data_len, file);
    fclose(file);
    TEST_ASSERT_EQUAL_STRING(data, read_buf);
}

void test_readFile(void) {
    char *file_name = "test_output.txt";
    char read_buf[50];
    size_t buf_size = sizeof(read_buf);

    int result = read_file(read_buf, file_name, "r", 1, buf_size);
    TEST_ASSERT_GREATER_THAN(0, result);

    char *expected_data = "Hello, world!";
    TEST_ASSERT_EQUAL_STRING(expected_data, read_buf);
}