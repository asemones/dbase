#include "structure_pool.h"
struct_pool * create_pool(size_t capacity){
    struct_pool * pool = (struct_pool*)wrapper_alloc((sizeof(struct_pool)), NULL,NULL);
    if (pool == NULL) return NULL;
    pool->pool = (void**)wrapper_alloc((sizeof(void*) * capacity), NULL,NULL);
    pool->free_list= (bool*)wrapper_alloc((sizeof(bool) * capacity), NULL,NULL);
    pool->size = 0;
    pool->capacity = capacity;
    pthread_mutex_init(&pool->write_lock, NULL);
    for (int i = 0; i < capacity; i++){
        pool->pool[i] = NULL;
        pool->free_list[i] = false;
    }
    if (pool->pool == NULL || pool->free_list == NULL){
        if (pool->pool != NULL){
            free(pool->pool);
        }
        else if (pool->free_list != NULL){
            free(pool->free_list);
        }
        free(pool);
        return NULL;
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
    for (int i = pool->capacity - 1; i >= 0; i--){
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
    for (size_t i = 0; i < pool->capacity; i++) {
        if (pool->pool[i] == struct_ptr) {
            pool->free_list[i] = true;
            if (reset_func != NULL)
                reset_func(struct_ptr);
            break;
        }
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