#include <stdio.h>
#include <string.h>
#include "hashtbl.h"
#include "list.h"
#include "../util/alloc_util.h"
#ifndef FRONTIER_H
#define FRONTIER_H

typedef struct frontiers{
   list * queue;
}frontier;

/**
 * @brief Creates a new frontier prority queue structure.
 *
 * @param dts Size of the elements being added
 * @param is_alloc Boolean indicating if allocation is needed.
 * @return Pointer to the newly created frontier.
 */
frontier* Frontier(size_t dts, bool is_alloc);

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
void enqueue(frontier *front, kv* element);

/**
 * @brief Removes and returns the element with the highest priority from the frontier (dequeue operation).
 *
 * @param front Pointer to the frontier.
 * @return Pointer to the element with the highest priority.
 */
kv* dequeue(frontier * front);

/**
 * @brief Returns the element with the highest priority without removing it from the frontier.
 *
 * @param front Pointer to the frontier.
 * @return Pointer to the element with the highest priority.
 */
kv* peek(frontier * front);

/**
 * @brief Frees all resources associated with the frontier.
 *
 * @param front Pointer to the frontier to be freed.
 */
void free_front(frontier * front);


//key-value pairs with the key being priority
frontier * Frontier(size_t dts, bool is_alloc){
    frontier *front = (frontier*)wrapper_alloc((sizeof(frontier)), NULL,NULL);
    front->queue = List(16,sizeof(kv), true);
    front->queue->isAlloc = is_alloc;
    return front;
}
void enqueue(frontier *front, kv* element) {
    if (front == NULL || element == NULL || element->key == NULL) {
        return;
    }
    insert(front->queue, element);
    int i = front->queue->len - 1;
    while (i != 0 && strcmp((char*)((kv*)get_element(front->queue, (i - 1) / 2))->key, (char*)((kv*)get_element(front->queue, i))->key) >= 0) {
        swap((kv*)get_element(front->queue, (i - 1) / 2), (kv*)get_element(front->queue, i), front->queue->dtS);
        i = (i - 1) / 2;
    }
}

void heapify(frontier *front, int index) {
    size_t size = front->queue->len;
    int largest = index;
    int left = 2 * index + 1;
    int right = 2 * index + 2;

    if (left < size) {
        kv *left_element = (kv *)get_element(front->queue, left);
        kv *current_element = (kv *)get_element(front->queue, largest);
        if (strcmp((char *)left_element->key, (char *)current_element->key) <= 0) {
            largest = left;
        }
    }

    if (right < size) {
        kv *right_element = (kv *)get_element(front->queue, right);
        kv *current_element = (kv *)get_element(front->queue, largest);
        if (strcmp((char *)right_element->key, (char *)current_element->key) <= 0) {
            largest = right;
        }
    }

    if (largest != index) {
        kv *index_element = (kv *)get_element(front->queue, index);
        kv *largest_element = (kv *)get_element(front->queue, largest);
        swap(index_element, largest_element, front->queue->dtS);

        heapify(front, largest);
    }
}

kv* dequeue(frontier * front){
    if (front->queue->len == 0) {
        return NULL;
    }

    kv* root = (kv*)wrapper_alloc((sizeof(kv)), NULL,NULL);
    *root = *(kv*)get_element(front->queue, 0);

    kv* first_element = (kv*)get_element(front->queue, 0);
    kv* last_element = (kv*)get_element(front->queue, front->queue->len - 1);

    swap(first_element, last_element, front->queue->dtS);
    front->queue->len--;
    heapify(front, 0);

    return root; 

}
kv * peek(frontier * front){
    return  (kv*)get_element(front->queue, 0);
}
void free_front(frontier * front){
    free_list(front->queue, NULL);
    free(front);
}
#endif


