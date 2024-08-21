#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "../util/alloc_util.h"
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


struct_pool * create_pool(size_t capacity){
    struct_pool * pool = (struct_pool*)wrapper_alloc((sizeof(struct_pool)), NULL,NULL);
    pool->pool = (void**)wrapper_alloc((sizeof(void*) * capacity), NULL,NULL);
    pool->free_list= (bool*)wrapper_alloc((sizeof(bool) * capacity), NULL,NULL);
    pool->size = 0;
    pool->capacity = capacity;
    pthread_mutex_init(&pool->write_lock, NULL);
    for (int i = 0; i < capacity; i++){
        pool->pool[i] = NULL;
        pool->free_list[i] = false;
    }
    return pool;
}
void insert_struct(struct_pool * pool, void * data){
    lock(&pool->write_lock);
    if (pool->size >= pool->capacity){
        unlock(&pool->write_lock);
        return;
    }
    pool->pool[pool->size] = data;
    pool->free_list[pool->size] = true;
    pool->size++;
    unlock(&pool->write_lock);
    return;
}
void * request_struct(struct_pool * pool){
    lock(&pool->write_lock);
    if (pool->size <= 0){
        unlock(&pool->write_lock);
        return NULL;
    }
    void * ret =  NULL;
    for (int i = pool->size - 1; i >= 0; i--){
        if (pool->free_list[i] == false) continue;
        pool->free_list[i] = false;
        ret = pool->pool[i];
        break;
    }
    pool->size--;
    unlock(&pool->write_lock);
    return ret;
}
void return_struct(struct_pool * pool, void * struct_ptr, void reset_func(void*)){
    lock(&pool->write_lock);
    for (int i = pool->size - 1; i >= 0; i--){
        if (pool->pool[i] != struct_ptr) continue;
        pool->free_list[i] = true;
        if (reset_func != NULL) reset_func(struct_ptr); 
        break;
    }   
    pool->size++;
    unlock(&pool->write_lock);
}
struct_pool * free_pool(struct_pool * pool, void free_func(void*)){
    for (int i = 0; i < pool->size; i++) free_func(pool->pool[i]);
    pthread_mutex_destroy(&pool->write_lock);
    free(pool->pool);
    free(pool->free_list);
    free(pool);
    pool = NULL;
    return pool;
}
#endif