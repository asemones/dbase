#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "../util/alloc_util.h"
#ifndef HASHTBLE_H
#define HASHTBLE_H


// HASH TABLE 
// HASH FUNC: fn1va
// COLLISION MECHANISM: quadratic probing
#define KEY_AMOUNT 10607
#define FNV_PRIME_64 1099511628211ULL
#define FNV_OFFSET_64 14695981039346656037ULL
/**
 * Calculate the 64-bit FNV-1a hash of the given data.
 * 
 * @param data Pointer to the data to be hashed.
 * @param len  Length of the data in bytes.
 * @return     64-bit hash value of the input data.
 */
/**
 * @brief Key-value pair structure
 * @struct pair
 * @param key Pointer to the key
 * @param value Pointer to the value associated with the key
 */
typedef struct pair{
    void * key;
    void * value;
}kv;
/**
 * @brief Dictionary (hash table) structure
 * @struct dict
 * @param keys Array of keys
 * @param values Array of values
 * @param size Current number of key-value pairs in the dictionary
 * @param capacity Maximum capacity of the dictionary
 */
typedef struct dict{
    void * keys;
    void * values;
    size_t size;
    size_t capacity;
}dict;

/**
 * @brief Calculate the 64-bit FNV-1a hash of the given data
 * @param data Pointer to the data to be hashed
 * @param len Length of the data in bytes
 * @return 64-bit hash value of the input data
 */
uint64_t fnv1a_64(const void *data, size_t len);

/**
 * @brief Creates a new dictionary (hash table)
 * @return Pointer to the newly created dictionary
 */
dict* Dict(void);

/**
 * @brief Creates a key-value pair on the stack
 * @param k Pointer to the key
 * @param v Pointer to the value
 * @return Key-value pair structure
 */
kv KV(void* k, void* v);

/**
 * @brief Creates a dynamically allocated key-value pair
 * @param k Pointer to the key
 * @param v Pointer to the value
 * @return Pointer to the newly created key-value pair
 */
kv* dynamic_kv(void* k, void* v);

/**
 * @brief Adds a key-value pair to a structure (generic function)
 * @param key Pointer to the key
 * @param value Pointer to the value
 * @param structure Pointer to the structure to add the key-value pair to
 */
void add_kv_void(void * key, void * value, void * structure);
/**
 * @brief Adds a key-value pair to a dictionary
 * @param table Pointer to the dictionary
 * @param key Pointer to the key
 * @param value Pointer to the value
 */
void add_kv(dict* table, void * key, void * value);

/**
 * @brief Retrieves a value from a dictionary by key
 * @param table Pointer to the dictionary
 * @param key Pointer to the key to look up
 * @return Pointer to the value if found, NULL otherwise
 */
void* get_v(dict * table, const void * key);

/**
 * @brief Compares two key-value pairs
 * @param k1 Pointer to the first key-value pair
 * @param k2 Pointer to the second key-value pair
 * @return 0 if equal, non-zero otherwise
 */
int compare_kv(kv * k1, kv * k2);

/**
 * @brief Removes a key-value pair from a dictionary
 * @param table Pointer to the dictionary
 * @param key Pointer to the key to remove
 */
void remove_kv(dict * table, void * key);

/**
 * @brief Frees a dictionary without freeing its elements
 * @param table Pointer to the dictionary to free
 */
void free_dict_no_element(dict * table);

/**
 * @brief Frees a dictionary and all its elements
 * @param table Pointer to the dictionary to free
 */
void free_dict(dict * table);

/**
 * @brief Frees a key-value pair
 * @param k Pointer to the key-value pair to free
 */
void free_kv(kv * k);

/**
 * @brief Compares two key-value pairs by value
 * @param kv1 Pointer to the first key-value pair
 * @param kv2 Pointer to the second key-value pair
 * @return Comparison result
 */
int compare_kv_v(const void * kv1, const void * kv2);

#endif
