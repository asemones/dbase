#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "../util/alloc_util.h"
#include <stdbool.h>
#ifndef STRUCTURE_POOL_H
#define STRUCTURE_POOL_H
#define mutex pthread_mutex_t
#define lock pthread_mutex_lock
#define unlock pthread_mutex_unlock

/**
 * @brief Pool of reusable structures for efficient memory management
 * @struct struct_pool
 * @param pool Array of pointers to structures in the pool
 * @param free_list Array of flags indicating which structures are free
 * @param size Current number of structures in the pool
 * @param capacity Maximum capacity of the pool
 * @param write_lock Mutex for thread-safe operations
 */
typedef struct struct_pool{
    void ** pool;
    bool * free_list;
    size_t size;
    size_t capacity;
    mutex write_lock;
}struct_pool;

/**
 * @brief Creates a new structure pool
 * @param capacity Maximum capacity of the pool
 * @return Pointer to the newly created structure pool
 */
struct_pool * create_pool(size_t capacity);

/**
 * @brief Inserts a structure into the pool
 * @param pool Pointer to the structure pool
 * @param data Pointer to the structure to insert
 */
void insert_struct(struct_pool * pool, void * data);

/**
 * @brief Requests a structure from the pool
 * @param pool Pointer to the structure pool
 * @return Pointer to a structure from the pool, or NULL if none available
 */
void * request_struct(struct_pool * pool);

/**
 * @brief Returns a structure to the pool
 * @param pool Pointer to the structure pool
 * @param struct_ptr Pointer to the structure to return
 * @param reset_func Function to reset the structure before returning it to the pool
 */
void return_struct(struct_pool * pool, void * struct_ptr, void reset_func(void *));

/**
 * @brief Frees a structure pool and all its structures
 * @param pool Pointer to the structure pool to free
 * @param free_func Function to free each structure in the pool
 * @return NULL
 */
struct_pool * free_pool(struct_pool * pool, void free_func(void *));


#endif