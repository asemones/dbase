#include "arena.h"
#include "../db/backend/key-value.h"

#pragma once
typedef struct k_v_arr{
    db_unit* keys;
    db_unit * values;
    size_t cap;
    int len;
}k_v_arr;


k_v_arr* create_k_v_arr (size_t size, arena * a);
void into_array(void * ar, void * key, void * value);

