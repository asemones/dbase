#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../db/backend/btree.h"
#include "unity/src/unity.h"
#include "../util/alloc_util.h"
#pragma once


void test_createBTree(void) {
    b_tree *tree = create_b_tree();
    TEST_ASSERT_NOT_NULL(tree);
    TEST_ASSERT_NOT_NULL(tree->root);
    TEST_ASSERT_EQUAL_INT(0, tree->numDocs);
    TEST_ASSERT_EQUAL_INT(0, tree->root->num_entries);
    TEST_ASSERT_TRUE(tree->root->isLeaf);
    destroy_b_tree(tree);
}

void test_insert_and_search_single_entry(void) {
    b_tree *tree = create_b_tree();
    keyword_entry entry = {"test", "l"};
    int result = insert_node(tree, &entry);
    TEST_ASSERT_EQUAL_INT(0, result);
    
    keyword_entry *found = search("test", tree);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_STRING("test", found->keyword);
    //TEST_ASSERT_EQUAL_INT(1, found->docCount);
    //TEST_ASSERT_EQUAL_INT(1, found->docIDs[0]);
    destroy_b_tree(tree);
}

void test_insert_multiple_entries(void) {
    b_tree *tree = create_b_tree();
    keyword_entry entries[] = {
        {"apple", "l",},
        {"banana", "m",},
        {"cherry", "n"},
        {"date", "o",},
        {"elderberry", "p"}
    };
    
    for (int i = 0; i < 5; i++) {
        int result = insert_node(tree, &entries[i]);
        TEST_ASSERT_EQUAL_INT(0, result);
    }
    
    for (int i = 0; i < 5; i++) {
        keyword_entry *found = search(entries[i].keyword, tree);
        TEST_ASSERT_NOT_NULL(found);
        TEST_ASSERT_EQUAL_STRING(entries[i].keyword, found->keyword);
        //TEST_ASSERT_EQUAL_INT(entries[i].docCount, found->docCount);
        //TEST_ASSERT_EQUAL_INT(entries[i].docIDs[0], found->docIDs[0]);
    }
    pretty_print_b_tree(tree);
    destroy_b_tree(tree);
}

void test_search_nonexistent_entry(void) {
    b_tree *tree = create_b_tree();
    keyword_entry *found = search("nonexistent", tree);
    TEST_ASSERT_NULL(found);
    destroy_b_tree(tree);
}

void test_insert_duplicate_entry(void) {
    b_tree *tree = create_b_tree();
    keyword_entry entry1 = {"duplicate", "m"};
    keyword_entry entry2 = {"duplicate", "m"};
    
    int result1 = insert_node(tree, &entry1);
    TEST_ASSERT_EQUAL_INT(0, result1);
    
    int result2 = insert_node(tree, &entry2);
    TEST_ASSERT_EQUAL_INT(0, result2);  
    
    keyword_entry *found = search("duplicate", tree);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_STRING("duplicate", found->keyword);
    destroy_b_tree(tree);
   
}

void test_insert_causing_split(void) {
    b_tree *tree = create_b_tree();
    char buffer[100000];
    char * ptrs[1000];
    char *temp = buffer;
    size_t remaining = sizeof(buffer);
    keyword_entry keywords[1000];

    for (int i = 0; i < MAXKEYS +900; i++) {
        ptrs[i] = temp;
        int written = snprintf(temp, remaining, "key%d", i);
        keyword_entry temp2 = {ptrs[i],"m"};
        keywords[i] = temp2;
        int result = insert_node(tree, &keywords[i]);
        TEST_ASSERT_EQUAL_INT(0, result);

        temp += written + 1;  
        remaining -= written + 1;
    }
    //pretty_print_b_tree(tree);
    TEST_ASSERT_FALSE(tree->root->isLeaf);
    TEST_ASSERT_TRUE(tree->root->num_entries > 0 && tree->root->num_entries < MAXKEYS);

    
    for (int i = 0; i < MAXKEYS + 900; i++) {
        char temp [30];
        snprintf(temp, sizeof(temp), "key%d", i);
        keyword_entry *found = search(buffer, tree);
        TEST_ASSERT_NOT_NULL(found);
        TEST_ASSERT_EQUAL_STRING(buffer, found->keyword);
    }
    //pretty_print_b_tree(tree);
    destroy_b_tree(tree);
}