#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char **data;
    int size;
    int capacity;
} min_heap;

min_heap* create_min_heap(int capacity);
void add(min_heap *heap, const char *value);
char* extract_min(min_heap *heap);
void min_heapify(min_heap *heap, int index);
void c_swap(char **x, char **y);
void print_heap(min_heap *heap);
void free_heap(min_heap *heap);
int compare_strings(const char *a, const char *b);

min_heap* create_min_heap(int capacity) {
    min_heap *heap = (min_heap *)wrapper_alloc((sizeof(min_heap)), NULL,NULL);
    heap->capacity = capacity;
    heap->size = 0;
    heap->data = (char **)wrapper_alloc((capacity * sizeof(char *)), NULL,NULL);
    return heap;
}
void c_swap(char **x, char **y) {
    char *temp = *x;
    *x = *y;
    *y = temp;
}
void add(min_heap *heap, const char *value) {
    if (heap->size == heap->capacity) {
        printf("Heap is full\n");
        return;
    }
    heap->data[heap->size] = value;
    int index = heap->size++;
    
    while (index != 0 && compare_strings(heap->data[index], heap->data[(index - 1) / 2]) < 0) {
        c_swap(&heap->data[index], &heap->data[(index - 1) / 2]);
        index = (index - 1) / 2;
    }
}

char* extract_min(min_heap *heap) {
    if (heap->size <= 0) return NULL;
    if (heap->size == 1) {
        return heap->data[--heap->size];
    }

    char *root = heap->data[0];
    heap->data[0] = heap->data[--heap->size];
    min_heapify(heap, 0);

    return root;
}
void min_heapify(min_heap *heap, int index) {
    int smallest = index;
    int left = 2 * index + 1;
    int right = 2 * index + 2;
    
    if (left < heap->size && compare_strings(heap->data[left], heap->data[smallest]) < 0) {
        smallest = left;
    }
    
    if (right < heap->size && compare_strings(heap->data[right], heap->data[smallest]) < 0) {
        smallest = right;
    }
    
    if (smallest != index) {
        c_swap(&heap->data[index], &heap->data[smallest]);
        min_heapify(heap, smallest);
    }
}

// Function to print the heap
void print_heap(min_heap *heap) {
    for (int i = 0; i < heap->size; i++) {
        printf("%s ", heap->data[i]);
    }
    printf("\n");
}

void free_heap(min_heap *heap) {
    for (int i = 0; i < heap->size; i++) {
        free(heap->data[i]);
    }
    free(heap->data);
    free(heap);
}
int compare_strings(const char *a, const char *b) {
    return strncmp(a, b,3);
}

