#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "../util/alloc_util.h"
#include "stdbool.h"
#ifndef STRUCTURE_POOL_H
#define STRUCTURE_POOL_H
#define mutex pthread_mutex_t
#define lock pthread_mutex_lock
#define unlock pthread_mutex_unlock

typedef struct struct_pool{
    void ** pool;
    bool * free_list;
    size_t size;
    size_t capacity;
    mutex write_lock;
}struct_pool;

struct_pool * create_pool(size_t capacity);
void insert_struct(struct_pool * pool, void * data);
void * request_struct(struct_pool * pool);
void return_struct(struct_pool * pool, void * struct_ptr, void reset_func(void *));
struct_pool * free_pool(struct_pool * pool, void free_func(void *));


#endif