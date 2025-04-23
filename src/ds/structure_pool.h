#include <stdlib.h>
#include <pthread.h> // For mutex
#include <stdbool.h> // For bool type if needed elsewhere

#ifndef STRUCTURE_POOL_H
#define STRUCTURE_POOL_H

// Define mutex types and operations for clarity/portability if needed elsewhere
#define mutex pthread_mutex_t
#define lock pthread_mutex_lock
#define unlock pthread_mutex_unlock

/**
 * @brief Pool of reusable structures implemented as a simple queue.
 * @struct struct_pool
 * @param queue_buffer Circular buffer holding pointers to *available* structs.
 * @param all_ptrs Array holding *all* struct pointers ever added (for freeing).
 * @param capacity Maximum number of structs the pool can manage.
 * @param size Current number of *available* structs in the queue.
 * @param head Queue head index (for dequeue).
 * @param tail Queue tail index (for enqueue).
 * @param total_managed Count of structs currently managed (index for all_ptrs).
 * @param write_lock Mutex for thread-safe operations.
 */
typedef struct struct_pool {
    void** queue_buffer;    // Circular buffer for available struct pointers
    void** all_ptrs;        // Holds all pointers ever added (for freeing)
    size_t capacity;        // Max number of structs pool can manage
    size_t size;            // Current number of *available* structs in queue
    size_t head;            // Queue head index (dequeue)
    size_t tail;            // Queue tail index (enqueue)
    size_t total_managed;   // Count of structs currently tracked in all_ptrs
    mutex write_lock;       // Mutex for thread safety
} struct_pool;

/**
 * @brief Creates a new structure pool.
 * @param capacity Maximum capacity of the pool.
 * @return Pointer to the newly created structure pool, or NULL on failure.
 */
struct_pool * create_pool(size_t capacity);

/**
 * @brief Inserts a structure into the pool's management and makes it available.
 *        Does nothing if the pool is at full capacity (`total_managed == capacity`).
 * @param pool Pointer to the structure pool.
 * @param data Pointer to the structure to insert.
 */
void insert_struct(struct_pool * pool, void * data);

/**
 * @brief Requests a structure from the pool (dequeues an available pointer).
 * @param pool Pointer to the structure pool.
 * @return Pointer to an available structure from the pool, or NULL if the pool is empty.
 */
void * request_struct(struct_pool * pool);

/**
 * @brief Returns a structure pointer to the pool (enqueues it).
 * @param pool Pointer to the structure pool.
 * @param struct_ptr Pointer to the structure to return.
 * @param reset_func Optional function to reset the structure before returning it to the pool. Can be NULL.
 */
void return_struct(struct_pool * pool, void * struct_ptr, void reset_func(void *));

/**
 * @brief Frees the structure pool and all managed structures.
 * @param pool Pointer to the structure pool to free.
 * @param free_func Function to free each individual structure managed by the pool. Can be NULL if structures don't need freeing or are freed elsewhere.
 * @return Always returns NULL.
 */
struct_pool * free_pool(struct_pool * pool, void free_func(void *));

#endif // STRUCTURE_POOL_H