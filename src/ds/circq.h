#ifndef CIRCQ_H
#define CIRCQ_H

#include <stdio.h>
#include <stdlib.h>
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
/* Function to create a new circular queue with given capacity */ \
static inline NAME* NAME##_create(int capacity) { \
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
 \
static inline bool NAME##_is_full(NAME* queue) { \
    return queue->size == queue->capacity; \
} \
\
/* Function to check if the queue is empty */ \
static inline bool NAME##_is_empty(NAME* queue) { \
    return queue->size == 0; \
} \
\
 \
static inline bool NAME##_enqueue(NAME* queue, T item) { \
    if (NAME##_is_full(queue)) { \
        return false;  /* Queue overflow */ \
    } \
    \
    /* If queue is initially empty */ \
    if (queue->front == -1) { \
        queue->front = 0; \
    } \
    \
    atomic_store_explicit(&queue->buffer[current_head], item, memory_order_relaxed); \
    atomic_store_explicit(&queue->head, next_head, memory_order_release); \
    \
    return true; \
} \
\
 \
static inline bool NAME##_dequeue(NAME* queue, T* item) { \
    if (NAME##_is_empty(queue)) { \
        return false;  /* Queue underflow */ \
    } \
    \
    *item = queue->array[queue->front]; \
    \
     \
    if (queue->front == queue->rear) { \
        queue->front = -1; \
        queue->rear = -1; \
    } \
    else { \
        \
        queue->front = (queue->front + 1) % queue->capacity; \
    } \
    \
    queue->size--; \
    return true; \
} \
\
 \
static inline bool NAME##_peek(NAME* queue, T* item) { \
    if (NAME##_is_empty(queue)) { \
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
 \
static inline void NAME##_destroy(NAME* queue) { \
    if (queue) { \
        if (queue->buffer) { \
            free(queue->buffer); \
        } \
        free(queue); \
    } \
} \
\

#endif /* CIRCQ_H */
