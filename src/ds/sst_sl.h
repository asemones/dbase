#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include "byte_buffer.h"
#include "../util/error.h"
#include "slab.h"
#include "../db/backend/indexer.h"
#pragma once

#define MAX_LEVEL 16
/**
 * @brief Node in a skip list data structure
 * @struct Node
 * @param key The key stored in this node
 * @param value The value associated with the key
 * @param forward Array of pointers to next nodes at different levels
 */
typedef struct sst_node {
    sst_f_inf sst;
    struct  sst_node* forward[];
} sst_node;
/**
 * @brief Skip list data structure for efficient key-value storage and retrieval
 * @struct sst_sl
 * @param level Current maximum level in the skip list
 * @param header Pointer to the header node
 * @param compare Function pointer to the comparison function for keys
 */
typedef struct sst_sl {
    int level;
    int use_default;
    sst_node * header;
    slab_allocator allocator;
} sst_sl;

/**
 * @brief Inserts a key-value pair into the skip list
 * @param list Pointer to the skip list
 * @param key The key to insert
 * @param value The value associated with the key
 * @return 0 on success, error code on failure
 */
int sst_insert_list(sst_sl* list, sst_f_inf sst);

/**
 * @brief Creates a new skip list
 * @param compare Function pointer to the comparison function for keys
 * @return Pointer to the newly created skip list
 */
sst_sl* create_sst_sl(int max_nodes);


/**
 * @brief Loads a skip list from a byte buffer stream
 * @param s Pointer to the skip list to populate
 * @param stream Pointer to the byte buffer containing the skip list data
 * @return Number of entries loaded or error code
 */
sst_sl* create_sst_sl(int max_nodes);
sst_node* sst_search_list(sst_sl* list, const void * key);

sst_node* sst_search_list_prefix(sst_sl* list, void* key);


/**
 * @brief Deletes a key-value pair from the skip list
 * @param list Pointer to the skip list
 * @param key The key to delete
 */
void sst_delete_element(sst_sl* list, void* key);

/**
 * @brief Frees all memory allocated for the skip list
 * @param list Pointer to the skip list to free
 */
void freesst_sl(sst_sl* list);

/**
 * @brief Comparison function for integer keys
 * @param a Pointer to the first integer
 * @param b Pointer to the second integer
 * @return Negative if a < b, 0 if a == b, positive if a > b
 */
int compareInt(const void* a, const void* b);

/**
 * @brief Comparison function for string keys
 * @param a Pointer to the first string
 * @param b Pointer to the second string
 * @return Negative if a < b, 0 if a == b, positive if a > b
 */
static inline int compareString(const void* a, const void* b) {
    return strcmp((const char*)a, (const char*)b);
}
void reset_skip_list(sst_sl* list);