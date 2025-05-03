#include "structure_pool.h"
#include "../util/alloc_util.h" // Assuming wrapper_alloc is defined here
#include <stdio.h> // For potential error messages

// Helper function to free allocated resources in create_pool on failure
static void cleanup_create_failure(struct_pool * pool) {
    if (!pool) return;
    if (pool->queue_buffer) free(pool->queue_buffer);
    if (pool->all_ptrs) free(pool->all_ptrs);
    pthread_mutex_destroy(&pool->write_lock); // Destroy mutex if initialized
    free(pool);
}

struct_pool * create_pool(size_t capacity) {
 
    struct_pool * pool = (struct_pool*)wrapper_alloc(sizeof(struct_pool), NULL, NULL);
    if (!pool) {
        perror("Failed to allocate struct_pool");
        return NULL;
    }

    pool->queue_buffer = NULL;
    pool->all_ptrs = NULL;
    pool->capacity = capacity;
    pool->size = 0;
    pool->head = 0;
    pool->tail = 0;
    pool->total_managed = 0;

    // Initialize mutex first
    if (pthread_mutex_init(&pool->write_lock, NULL) != 0) {
        perror("Mutex initialization failed");
        free(pool); // Free the pool struct itself
        return NULL;
    }

    pool->queue_buffer = (void**)wrapper_alloc(sizeof(void*) * capacity, NULL, NULL);
    if (!pool->queue_buffer) {
        perror("Failed to allocate queue_buffer");
        cleanup_create_failure(pool);
        return NULL;
    }

    pool->all_ptrs = (void**)wrapper_alloc(sizeof(void*) * capacity, NULL, NULL);
    if (!pool->all_ptrs) {
        perror("Failed to allocate all_ptrs");
        cleanup_create_failure(pool);
        return NULL;
    }
    return pool;
}

void insert_struct(struct_pool * pool, void * data) {
    if (!pool || !data) return; // Basic validation

    lock(&pool->write_lock);

    // Check if the pool has capacity to manage another struct
    if (pool->total_managed >= pool->capacity) {
        // Optionally log: fprintf(stderr, "Warning: Pool at full management capacity (%zu), cannot insert new struct.\n", pool->capacity);
        unlock(&pool->write_lock);
        return;
    }

    // Check if the queue itself is full (shouldn't happen if total_managed < capacity, but good practice)
    if (pool->size >= pool->capacity) {
         // Optionally log: fprintf(stderr, "Warning: Pool queue is full (%zu), cannot insert struct now (should be returned first).\n", pool->size);
         unlock(&pool->write_lock);
         return;
    }


    // Add to management list
    pool->all_ptrs[pool->total_managed++] = data;

    // Add to available queue
    pool->queue_buffer[pool->tail] = data;
    pool->tail = (pool->tail + 1) % pool->capacity;
    pool->size++;

    unlock(&pool->write_lock);
}

void * request_struct(struct_pool * pool) {
    if (!pool) return NULL;



    if (pool->size == 0) {
        // Pool is empty
       
        return NULL;
    }

    // Dequeue pointer
    void* ptr = pool->queue_buffer[pool->head];
    pool->queue_buffer[pool->head] = NULL; // Optional: Clear the slot
    pool->head = (pool->head + 1) % pool->capacity;
    pool->size--;

    
    return ptr;
}

void return_struct(struct_pool * pool, void * struct_ptr, void reset_func(void*)) {
    if (!pool || !struct_ptr) return;



    // Check if the queue is already full (indicates an issue, like returning more items than capacity)
    if (pool->size >= pool->capacity) {
        fprintf(stderr, "Error: Attempted to return struct to a full pool queue. Pool size: %zu, Capacity: %zu\n", pool->size, pool->capacity);
        // This might indicate a double-return or logic error elsewhere
        return; // Or handle error differently
    }

    // Reset the structure if a reset function is provided
    if (reset_func != NULL) {
        reset_func(struct_ptr);
    }

    // Enqueue pointer
    pool->queue_buffer[pool->tail] = struct_ptr;
    pool->tail = (pool->tail + 1) % pool->capacity;
    pool->size++;


}

struct_pool * free_pool(struct_pool * pool, void free_func(void*)) {
    if (!pool) return NULL;

    // No need to lock if we assume no other threads access during freeing

    // Free all managed structures using the provided function
    if (free_func != NULL) {
        for (size_t i = 0; i < pool->total_managed; i++) {
             if (pool->all_ptrs[i] != NULL) { // Check if pointer is valid before freeing
                 free_func(pool->all_ptrs[i]);
             }
        }
    }

    // Free the internal buffers
    if (pool->queue_buffer) free(pool->queue_buffer);
    if (pool->all_ptrs) free(pool->all_ptrs);

    // Destroy the mutex
    pthread_mutex_destroy(&pool->write_lock);

    // Free the pool structure itself
    free(pool);

    return NULL; // Return NULL as per convention
}

// Compatibility functions request_struct_idx and return_struct_idx removed.