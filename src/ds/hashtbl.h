#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
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

/**
 * Calculate the 64-bit FNV-1a hash of the given data.
 * 
 * @param data Pointer to the data to be hashed.
 * @param len  Length of the data in bytes.
 * @return     64-bit hash value of the input data.
 */

/**
 * @brief Computes the 64-bit FNV-1a hash of the given data.
 *
 * @param data Pointer to the data to be hashed.
 * @param len Length of the data in bytes.
 * @return 64-bit hash value of the data.
 */
uint64_t fnv1a_64(const void *data, size_t len);

/**
 * @brief Creates a new dictionary.
 *
 * @return Pointer to the newly created dictionary.
 */
dict* Dict();

/**
 * @brief Adds a key-value pair to the dictionary.
 *
 * @param my_dict Pointer to the dictionary.
 * @param _key Pointer to the key.
 * @param _value Pointer to the value.
 */
void add_kv(dict *my_dict, void * _key, void* _value);

/**
 * @brief Retrieves the value associated with the given key in the dictionary.
 *
 * @param my_dict Pointer to the dictionary.
 * @param _key Pointer to the key.
 * @return Pointer to the value associated with the key, or NULL if the key is not found.
 */
void* get_v(dict *my_dict, const void * _key);

/**
 * @brief Removes a key-value pair from the dictionary.
 *
 * @param my_dict Pointer to the dictionary.
 * @param _key Pointer to the key.
 */
void remove_kv(dict * my_dict, void * _key);

/**
 * @brief Frees the memory allocated for the dictionary.
 *
 * @param my_dict Pointer to the dictionary to be freed.
 */
void free_dict(dict* my_dict);

uint64_t fnv1a_64(const void *data, size_t len) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint64_t hash = FNV_OFFSET_64;
    
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint64_t)bytes[i];
        hash *= FNV_PRIME_64;
    }
    
    return hash;
}
dict* Dict(){
    dict *table = (dict*)wrapper_alloc((sizeof(dict)), NULL,NULL);
    table->capacity = KEY_AMOUNT;
    table->size = 0;
    table->keys = calloc(KEY_AMOUNT, sizeof(void*));
    table->values = calloc(KEY_AMOUNT, sizeof(void*));
    return table;
}
kv KV(void* k, void* v){
    kv myKv;
    myKv.value = v;
    myKv.key = k;
    return myKv;
}
kv* dynamic_kv(void* k, void* v){
    kv * myKv = (kv *)wrapper_alloc((sizeof(kv)), NULL,NULL);
    myKv->value = v;
    myKv->key = k;
    return myKv;
}
void add_kv_void(void * key, void * value, void * structure){
    dict * table = (dict*)structure; 
    add_kv(table, key, value);
}
void add_kv(dict*table, void *key, void *value) {
    if (key == NULL || value == NULL) {
        return;
    }
    int len = strlen((char*)key) + 1;
    uint64_t index = fnv1a_64(key, len) % table->capacity;
    int i = 1;
    
    while (((void**)table->keys)[index] != NULL && ((void**)table->keys)[index] != key) {
        index = (index + (i*i)) % table->capacity;
        i++;
    }

    if (((void**)table->keys)[index] == key) {
        ((void**)table->values)[index] = value;
        return;
    }
    
    ((void**)table->keys)[index] = key;
    ((void**)table->values)[index] = value;
    table->size++;
}

void* get_v(dict *table, const void *key) {
    if (key == NULL || table == NULL) {
        return NULL;
    }
    int len = strlen((char*)key) + 1;
    uint64_t index = fnv1a_64(key, len) % table->capacity;
    int i = 0;
    
    while (1) {
        if (((void**)table->keys)[index] == NULL) {
            return NULL;
        }
        if (strcmp((char*)((void**)table->keys)[index], (char*)key) == 0) {
            return ((void**)table->values)[index];
        }
        index = (index + (i*i)) % table->capacity;
        i++;
    }
}
int compare_kv(kv *k1, kv *k2){
    if (k1 == NULL || k2 == NULL){
        return -1;
    }
    if (strcmp((char*)k1->key, (char*)k2->key) == 0 && strcmp((char*)k1->value, (char*)k2->value) == 0){
        return 0;
    }
    return -1;
}

void remove_kv(dict *table, void *key) {
    if (key == NULL) {
        return;
    }
    int len = strlen((char*)key) + 1;
    uint64_t index = fnv1a_64(key, len) % table->capacity;
    int i = 0;
    
    while (1) {
        if (((void**)table->keys)[index] == NULL) {
            return;
        }
        if (strcmp((char*)((void**)table->keys)[index], (char*)key) == 0) {
            ((void**)table->keys)[index] = NULL;
            ((void**)table->values)[index] = NULL;
            table->size--;
            break;
        }
        index = (index + (i*i)) % table->capacity;
        i++;
    }
}
void free_dict_no_element(dict *table) {
    if (table == NULL) {
        return;
    }
    free(table->keys);
    free(table->values);
    free(table);
}
void free_dict(dict *table) {
    if (table == NULL) {
        return;
    }
    for (size_t i = 0; i < table->capacity; i++) {
        if (((void**)table->keys)[i]!= NULL){
            free(((void**)table->keys)[i]);
            free(((void**)table->values)[i]);
        }
    }
    free(table->keys);
    free(table->values);
    free(table);
}
void free_kv(kv *k){
    if (k==NULL){
        return;
    }
    if (k->value!= NULL){
         free(k->value);
    }
    if (k->key!= NULL){
         free(k->key);
    }
    free(k);
}
#endif
