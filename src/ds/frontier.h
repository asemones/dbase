#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#pragma once

typedef struct frontier{
    list* queue;
    int (*compare)(const void*, const void*); // Comparison function pointer
} frontier;

/**
 * @brief Creates a new frontier priority queue structure.
 *
 * @param dts Size of the elements being added
 * @param is_alloc Boolean indicating if allocation is needed.
 * @param compare Function pointer for comparing two elements.
 * @return Pointer to the newly created frontier.
 */
frontier* Frontier(size_t dts, bool is_alloc, int (*compare)(const void*, const void*));

/**
 * @brief Ensures the heap property is maintained at the specified index.
 *
 * @param front Pointer to the frontier.
 * @param index The index at which to ensure the heap property.
 */
void heapify(frontier *front, int index);

/**
 * @brief Adds a new element to the frontier (enqueue operation).
 *
 * @param front Pointer to the frontier.
 * @param element Pointer to the element to be added to the frontier.
 */
void enqueue(frontier *front, void* element);

/**
 * @brief Removes and returns the element with the highest priority from the frontier (dequeue operation).
 * The returned element is copied to the provided buffer.
 *
 * @param front Pointer to the frontier.
 * @param out_buffer Buffer where the removed element will be copied.
 */
int dequeue(frontier * front, void* out_buffer);

/**
 * @brief Returns the element with the highest priority without removing it from the frontier.
 *
 * @param front Pointer to the frontier.
 * @return Pointer to the element with the highest priority.
 */
void* peek(frontier * front);

/**
 * @brief Frees all resources associated with the frontier.
 *
 * @param front Pointer to the frontier to be freed.
 */
void free_front(frontier * front);

// Implementation of the functions

frontier* Frontier(size_t dts, bool is_alloc, int (*compare)(const void*, const void*)) {
    frontier *front = (frontier*)wrapper_alloc((sizeof(frontier)), NULL, NULL);
    if (front  == NULL ) return NULL;
    front->queue = List(16, dts, true);
    if (front->queue == NULL) return NULL;
    front->queue->isAlloc = is_alloc;
    front->compare = compare;
    return front;
}

void enqueue(frontier *front, void* element) {
    if (front == NULL || element == NULL) {
        return;
    }
    insert(front->queue, element);
    int i = front->queue->len - 1;
    while (i != 0 && front->compare(at(front->queue, (i - 1) / 2), at(front->queue, i)) >= 0) {
        swap(at(front->queue, (i - 1) / 2), at(front->queue, i), front->queue->dtS);
        i = (i - 1) / 2;
    }
}

void heapify(frontier *front, int index) {
    size_t size = front->queue->len;
    int largest = index;
    int left = 2 * index + 1;
    int right = 2 * index + 2;

    if (left < size) {
        void * left_element = at(front->queue,left );
        void * curr_element = at(front->queue, largest);
        if (front->compare(left_element, curr_element) <= 0){
            largest = left;
        }
        
    }
    if (right < size){
        void * right_element = at(front->queue ,right );
        void * curr_element = at(front->queue, largest);
        if (front->compare(right_element, curr_element) <= 0){
            largest = right;
        }
    }
    if (largest != index) {
        swap(at(front->queue, index), at(front->queue, largest), front->queue->dtS);
        heapify(front, largest);
    }
}

int dequeue(frontier* front, void* out_buffer) {
    if (front->queue->len == 0) {
        return -1; // or handle the empty queue case appropriately
    }

    void* root = at(front->queue, 0);

    memcpy(out_buffer, root, front->queue->dtS);

    void* last_element = at(front->queue, front->queue->len - 1);
   
    swap(root, last_element, front->queue->dtS);

    front->queue->len--;
    heapify(front, 0);
    return 0;
}

void* peek(frontier * front) {
    return at(front->queue, 0);
}
void debug_print(frontier * front){
    for (int i =0; i < front->queue->len; i++){
     kv* l =(kv*) at(front->queue,i);
     fprintf(stdout, "%s, %s\n",(char*)l->key, (char*)l->value);
    }
}
void free_front(frontier * front) {
    free_list(front->queue, NULL);
    free(front);
}
