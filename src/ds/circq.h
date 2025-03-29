#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h> // For size_t

#define DEFINE_CIRCULAR_QUEUE(T, NAME) \
\
typedef struct { \
    size_t capacity; \
    _Atomic(T)* buffer; \
    _Atomic size_t head; \
    _Atomic size_t tail; \
} NAME; \
\
NAME* NAME##_create(int capacity_arg) { \
    if (capacity_arg < 1) return NULL; \
    size_t capacity = (size_t)capacity_arg; \
    size_t buffer_size = capacity + 1; \
    NAME* queue = (NAME*)malloc(sizeof(NAME)); \
    if (!queue) return NULL; \
    \
    queue->buffer = (_Atomic(T)*)malloc(buffer_size * sizeof(_Atomic(T))); \
    if (!queue->buffer) { \
        free(queue); \
        return NULL; \
    } \
    \
    queue->capacity = buffer_size; \
    atomic_init(&queue->head, 0); \
    atomic_init(&queue->tail, 0); \
    \
    return queue; \
} \
\
bool NAME##_enqueue(NAME* queue, T item) { \
    size_t current_head = atomic_load_explicit(&queue->head, memory_order_relaxed); \
    size_t next_head = (current_head + 1) % queue->capacity; \
    size_t current_tail = atomic_load_explicit(&queue->tail, memory_order_acquire); \
    \
    if (next_head == current_tail) { \
        return false; \
    } \
    \
    atomic_store_explicit(&queue->buffer[current_head], item, memory_order_relaxed); \
    atomic_store_explicit(&queue->head, next_head, memory_order_release); \
    \
    return true; \
} \
\
bool NAME##_is_empty(NAME* queue) { \
    size_t current_head = atomic_load_explicit(&queue->head, memory_order_acquire); \
    size_t current_tail = atomic_load_explicit(&queue->tail, memory_order_acquire); \
    return current_head == current_tail; \
} \
\
bool NAME##_dequeue(NAME* queue, T* item_out) { \
    size_t current_tail = atomic_load_explicit(&queue->tail, memory_order_relaxed); \
    size_t current_head = atomic_load_explicit(&queue->head, memory_order_acquire); \
    \
    if (current_head == current_tail) { \
        return false; \
    } \
    \
    *item_out = atomic_load_explicit(&queue->buffer[current_tail], memory_order_relaxed); \
    size_t next_tail = (current_tail + 1) % queue->capacity; \
    atomic_store_explicit(&queue->tail, next_tail, memory_order_release); \
    \
    return true; \
} \
\
void NAME##_destroy(NAME* queue) { \
    if (queue) { \
        if (queue->buffer) { \
            free(queue->buffer); \
        } \
        free(queue); \
    } \
}
