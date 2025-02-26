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
typedef struct pair{
    void * key;
    void * value;
}kv;
typedef struct dict{
    void * keys;
    void * values;
    size_t size;
    size_t capacity;
}dict;

uint64_t fnv1a_64(const void *data, size_t len);
dict* Dict(void);
kv KV(void* k, void* v);
kv* dynamic_kv(void* k, void* v);
void add_kv_void(void * key, void * value, void * structure);
void add_kv(dict* table, void * key, void * value);
void* get_v(dict * table, const void * key);
int compare_kv(kv * k1, kv * k2);
void remove_kv(dict * table, void * key);
void free_dict_no_element(dict * table);
void free_dict(dict * table);
void free_kv(kv * k);
int compare_kv_v(const void * kv1, const void * kv2);

#endif
