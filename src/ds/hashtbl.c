#include "hashtbl.h"

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
    if (table == NULL) return NULL;
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
    if (myKv == NULL) return NULL;
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
    if (k1 == NULL || k2 == NULL || k1->key == NULL || k2->key == NULL){
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
int compare_kv_v(const void * kv1, const void * kv2){
    const kv * k1 = kv1;
    const kv * k2 = kv2;
    if (k1 == NULL || k2 == NULL){
        return -1;
    }
    
    return strcmp(k1->key, k2->key);
}