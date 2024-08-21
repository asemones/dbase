#include "arena.h"

#pragma once
typedef struct k_v_arr{
    char ** keys;
    char ** values;
    size_t cap;
    int len;
}k_v_arr;


k_v_arr* create_k_v_arr (size_t size, arena * a){
    k_v_arr * ar = arena_alloc(a, sizeof(k_v_arr));
    ar->keys = arena_alloc(a, size);
    ar->values = arena_alloc(a, size);
    ar->cap = size;
    ar->len = 0;
    return ar;
}
void into_array(void * key, void * value, void * ar){
    k_v_arr * arr = (k_v_arr*)ar;
    arr->values[arr->len] = (char*)value;
    arr->keys[arr->len] =(char*)key;
    arr->len ++;
}
