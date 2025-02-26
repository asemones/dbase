#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include "byte_buffer.h"
#include "../db/backend/key-value.h"
#include "../util/error.h"

#pragma once

#define MAX_LEVEL 16
typedef struct Node {
    db_unit key;
    db_unit value;
    struct Node* forward[];
} Node;
typedef struct SkipList {
    int level;
    Node* header;
    int (*compare)(const void*, const void*);
} SkipList;

int insert_list(SkipList* list, db_unit key, db_unit value);
Node* create_skiplist_node(int level, db_unit key, db_unit value);
SkipList* create_skiplist(int (*compare)(const void*, const void*));
int random_level(void);
int skip_from_stream(SkipList *s, byte_buffer *stream);
int insert_list(SkipList* list, db_unit key, db_unit value);
Node* search_list(SkipList* list, const void* key);
Node* search_list_prefix(SkipList* list, void* key);
void delete_element(SkipList* list, void* key);
void freeSkipList(SkipList* list);
int compareInt(const void* a, const void* b);
int compareString(const void* a, const void* b);
