#ifndef TYPESAFE_LIST_H
#define TYPESAFE_LIST_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>

/*
 * DECLARE_LIST_TYPE(NAME, TYPE)
 *
 * NAME: an identifier that will be appended to function names.
 * TYPE: the actual C type to be stored (e.g. int, double, struct foo*, etc.)
 *
 * This macro defines:
 *  - A struct named list_<NAME> that holds a dynamic array of TYPE.
 *  - A “constructor” List_<NAME>(initial_capacity, alloc) that creates a new list.
 *  - A thread-safe constructor thread_safe_list_<NAME>().
 *  - Functions to expand, insert at end, insert at an index,
 *    retrieve an element, remove an element, free the list,
 *    swap two elements, and sort the list.
 *
 * If isAlloc is true then free_list will, for each element,
 * call a provided free function (if non-NULL).
 */

#define DECLARE_LIST_TYPE(NAME, TYPE)                                     \
                                                                          \
typedef struct {                                                          \
    TYPE *arr;                                                            \
    int len;                                                              \
    int cap;                                                              \
    bool isAlloc;   /* if true, free_list will call free_func on each element */ \
    bool thread_safe;                                                     \
    pthread_mutex_t write_lock;                                           \
    pthread_mutex_t read_lock;                                            \
} list_##NAME;                                                            \
                                                                          \
/* Create a new (non-thread-safe) list with a given initial capacity. */ \
static list_##NAME* List_##NAME(int Size, bool alloc) {                   \
    list_##NAME *my_list = (list_##NAME*) malloc(sizeof(list_##NAME));      \
    if (!my_list) return NULL;                                             \
    my_list->len = 0;                                                      \
    my_list->cap = (Size < 16 ? 16 : Size);                                \
    my_list->arr = (TYPE*) malloc(my_list->cap * sizeof(TYPE));            \
    if (!my_list->arr) {                                                   \
        free(my_list);                                                     \
        return NULL;                                                       \
    }                                                                      \
    my_list->isAlloc = alloc;                                              \
    my_list->thread_safe = false;                                          \
    return my_list;                                                        \
}                                                                         \
                                                                          \
/* Create a thread-safe list */                                           \
static list_##NAME* thread_safe_list_##NAME(int Size, bool alloc) {         \
    list_##NAME *my_list = List_##NAME(Size, alloc);                       \
    if (!my_list) return NULL;                                             \
    my_list->thread_safe = true;                                           \
    pthread_mutex_init(&my_list->read_lock, NULL);                         \
    pthread_mutex_init(&my_list->write_lock, NULL);                        \
    return my_list;                                                        \
}                                                                         \
                                                                          \
/* Expand the list's capacity by doubling it */                          \
static void expand_##NAME(list_##NAME *my_list) {                          \
    if (!my_list) return;                                                  \
    int new_cap = my_list->cap * 2;                                          \
    TYPE *temp = (TYPE*) realloc(my_list->arr, new_cap * sizeof(TYPE));      \
    if (!temp) return;                                                     \
    my_list->arr = temp;                                                   \
    my_list->cap = new_cap;                                                \
}                                                                         \
                                                                          \
/* Insert an element at the end of the list */                            \
static void insert_##NAME(list_##NAME *my_list, TYPE element) {            \
    if (!my_list) return;                                                  \
    if (my_list->thread_safe) pthread_mutex_lock(&my_list->write_lock);     \
    if (my_list->len >= my_list->cap) {                                    \
        expand_##NAME(my_list);                                            \
    }                                                                      \
    my_list->arr[my_list->len++] = element;                                \
    if (my_list->thread_safe) pthread_mutex_unlock(&my_list->write_lock);   \
}                                                                         \
                                                                          \
/* Insert an element at a specific index (shifting later elements right) */\
static void inset_at_##NAME(list_##NAME *my_list, TYPE element, int index) {\
    if (!my_list || index < 0 || index > my_list->len) return;             \
    if (my_list->thread_safe) pthread_mutex_lock(&my_list->write_lock);     \
    if (my_list->len >= my_list->cap) {                                    \
        expand_##NAME(my_list);                                            \
    }                                                                      \
    memmove(&my_list->arr[index+1], &my_list->arr[index],                   \
            (my_list->len - index) * sizeof(TYPE));                        \
    my_list->arr[index] = element;                                         \
    my_list->len++;                                                        \
    if (my_list->thread_safe) pthread_mutex_unlock(&my_list->write_lock);   \
}                                                                         \
                                                                          \
/* Retrieve an element at the given index */                             \
static TYPE at_##NAME(list_##NAME *my_list, int index) {                   \
    if (!my_list || index < 0 || index >= my_list->len) {                  \
        fprintf(stderr, "Index %d out of bounds (len: %d)\n", index, my_list ? my_list->len : 0); \
        exit(EXIT_FAILURE);                                                \
    }                                                                      \
    if (my_list->thread_safe) pthread_mutex_lock(&my_list->read_lock);      \
    TYPE element = my_list->arr[index];                                    \
    if (my_list->thread_safe) pthread_mutex_unlock(&my_list->read_lock);    \
    return element;                                                        \
}                                                                         \
                                                                          \
/* Remove the element at the given index (shifting later elements left) */ \
static void remove_at_##NAME(list_##NAME *my_list, int index) {            \
    if (!my_list || index < 0 || index >= my_list->len) return;            \
    if (my_list->thread_safe) pthread_mutex_lock(&my_list->write_lock);     \
    memmove(&my_list->arr[index], &my_list->arr[index+1],                   \
            (my_list->len - index - 1) * sizeof(TYPE));                    \
    my_list->len--;                                                        \
    if (my_list->thread_safe) pthread_mutex_unlock(&my_list->write_lock);   \
}                                                                         \
                                                                          \
/* Free the list.
   If free_func is provided and isAlloc is true, it is called on each element. */ \
static void free_list_##NAME(list_##NAME *my_list, void (*free_func)(TYPE)) {\
    if (!my_list) return;                                                  \
    if (my_list->isAlloc && free_func) {                                   \
        for (int i = 0; i < my_list->len; i++) {                           \
            free_func(my_list->arr[i]);                                    \
        }                                                                  \
    }                                                                      \
    free(my_list->arr);                                                    \
    if (my_list->thread_safe) {                                            \
        pthread_mutex_destroy(&my_list->read_lock);                        \
        pthread_mutex_destroy(&my_list->write_lock);                       \
    }                                                                      \
    free(my_list);                                                         \
}                                                                         \
                                                                          \
/* Swap two elements */                                                   \
static void swap_##NAME(TYPE *a, TYPE *b) {                                \
    TYPE temp = *a;                                                        \
    *a = *b;                                                               \
    *b = temp;                                                             \
}                                                                         \
                                                                          \
/* Sort the list using insertion sort.
   'compare' should be a function that takes two const TYPE* arguments and returns an int. */ \
static void sort_list_##NAME(list_##NAME *my_list,                           \
                             int (*compare)(const TYPE*, const TYPE*),       \
                             bool ascending) {                              \
    if (!my_list || my_list->len <= 1) return;                             \
    if (my_list->thread_safe) pthread_mutex_lock(&my_list->write_lock);     \
    for (int i = 1; i < my_list->len; i++) {                               \
        int j = i;                                                       \
        while (j > 0 && ((ascending && compare(&my_list->arr[j], &my_list->arr[j-1]) < 0) || \
                         (!ascending && compare(&my_list->arr[j], &my_list->arr[j-1]) > 0))) { \
            swap_##NAME(&my_list->arr[j], &my_list->arr[j-1]);             \
            j--;                                                         \
        }                                                                  \
    }                                                                      \
    if (my_list->thread_safe) pthread_mutex_unlock(&my_list->write_lock);   \
}

#endif // TYPESAFE_LIST_H
