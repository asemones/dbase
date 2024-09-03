#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "hashtbl.h"
#include <pthread.h>
#include "byte_buffer.h"

#ifndef LIST_H
#define LIST_H
#define rw_mutex pthread_rwlock_t
#define r_lock pthread_rwlock_rdlock
#define w_lock pthread_rwlock_wrlock
#define unlock pthread_mutex_unlock
/**
* Simple List Structure
* @param arr the base dynamic array
* @param len the number of elements in the array
* @param cap the maxiumum size of the array
* @param dtS the size of element datatype. Get this from sizeof(list_element_type)
*/
typedef struct Lists{
    void* arr;
    int len;
    int cap;
    int dtS;
    bool isAlloc;
    bool thread_safe;
    rw_mutex lock;
}list;
/**
 * @brief Struct for function pointers to use for sorting
 * 
 */
typedef int (*compare)( void* , void*);

/**
 * Creates a new list with an initial size and data type size.
 *
 * @param Size The initial size of the list. If less than 16, the initial size will be set to 16.
 * @param dataTypeSize The size of the data type to be stored in the list.
 * @return A pointer to the newly created list.
 */
list* List(int Size, int dataTypeSize, bool alloc);

/**
 * Expands the capacity of the list by doubling its current capacity.
 *
 * @param my_list A pointer to the list to be expanded.
 */
void expand(list *my_list);

/**
 * Inserts an element at a specific index in the list. Expands the list if necessary.
 *
 * @param my_list A pointer to the list where the element will be inserted.
 * @param element A pointer to the element to be inserted.
 * @param index The index at which to insert the element.
 */
void inset_at(list *my_list, void *element, int index);
/**
 * Inserts an element at the end of the list. Expands the list if necessary.
 *
 * @param my_list A pointer to the list where the element will be inserted.
 * @param element A pointer to the element to be inserted.
 */
void insert(list * my_list, void * element);
/**
 * Frees the memory allocated for the list and its elements.
 *
 * @param my_list A pointer to the list to be freed.
 */
void free_list(list *my_list, void free_func(void*));

/**
 * Removes an element at a specific index in the list.
 *
 * @param my_list A pointer to the list from which the element will be removed.
 * @param index The index of the element to be removed.
 */
void remove_at(list *my_list, int index);

/**
 * Retrieves an element at a specific index from the list.
 *
 * @param my_list A pointer to the list from which the element will be retrieved.
 * @param index The index of the element to be retrieved.
 * @return A pointer to the element at the specified index, or NULL if the index is invalid.
 */
void* at(list *my_list, int index);

/**
 * @brief Compares the keys of two kv structures.
 *
 * @param one Pointer to the first kv structure.
 * @param two Pointer to the second kv structure.
 * @return An integer less than, equal to, or greater than zero if the key of
 *         the first kv is found, respectively, to be less than, to match, or be
 *         greater than the key of the second kv.
 */
int compare_values_by_key(void *one, void *two);

/**
 * @brief Swaps the contents of two memory locations.
 *
 * @param one Pointer to the first memory location.
 * @param two Pointer to the second memory location.
 * @param size The size of the memory blocks to swap.
 */
void swap(void *one, void *two, size_t size);

/**
 * @brief Sorts a list using the insertion sort algorithm.
 *
 * @param my_list Pointer to the list to be sorted.
 * @param ascending A boolean indicating whether to sort in ascending (true) or descending (false) order.
 * @param func A comparison function to determine the order of the elements.
 */
void sort_list(list *my_list, bool ascending, compare func);

list* List(int Size, int dataTypeSize, bool alloc){
    list * my_list  = (list*)wrapper_alloc((sizeof(list)), NULL,NULL);
    my_list->cap = 0;
    my_list->len =0;
    if (Size < 16){
        my_list->arr = wrapper_alloc((16* dataTypeSize), NULL,NULL);
        my_list->cap= 16;
    }
    else {
        my_list->arr = wrapper_alloc((Size* dataTypeSize), NULL,NULL);
        my_list->cap= Size;
    }
    my_list->isAlloc=alloc;
    my_list->dtS = dataTypeSize;
    my_list->thread_safe = false;
    return my_list;
}
list * thread_safe_list(int Size, int dataTypeSize, bool alloc){
    list * my_list  = (list*)wrapper_alloc((sizeof(list)), NULL,NULL);
    my_list->cap = 0;
    my_list->len =0;
    if (Size < 16){
        my_list->arr = wrapper_alloc((16* dataTypeSize), NULL,NULL);
        my_list->cap= 16;
    }
    else {
        my_list->arr = wrapper_alloc((Size* dataTypeSize), NULL,NULL);
        my_list->cap= Size;
    }
    my_list->isAlloc=alloc;
    my_list->dtS = dataTypeSize;
    my_list->thread_safe = true;
    pthread_rwlock_init(&my_list->lock, NULL);
    return my_list;
}
void expand(list * my_list){
    if (my_list == NULL){
        return;
    }
    my_list->cap *=2;
    void * arr  =  realloc(my_list->arr, my_list->dtS * (my_list->cap));
    if (arr == NULL){
        return;
    }
    my_list->arr= arr;
    return;
}
void insert(list * my_list, void * element){
    if (my_list == NULL || element == NULL) {
        return;
    }
    //if (my_list->thread_safe) w_lock(&my_list->lock);
    if (my_list->len >= my_list->cap){
        expand(my_list);
    }
    //void pointers need to be typecasted, use (char*) so we start with value 1
    memmove((char *)my_list->arr + (my_list->len * my_list->dtS), element, my_list->dtS);
    my_list->len++;
    if (my_list->thread_safe) unlock(&my_list->lock);
    return;
}
void inset_at(list * my_list, void * element, int index){
    if (my_list == NULL || element == NULL || index < 0 || index > my_list->cap) {
        return;
    }
    if (my_list->len >= my_list->cap){
        expand(my_list);
    }
    void * temp;
    if ((temp= at(my_list,index)) !=  NULL){
        my_list->len--;
    }
    //if (my_list->thread_safe) w_lock(&my_list->lock);
    memmove((char *)my_list->arr + index * my_list->dtS, element, my_list->dtS);
    my_list->len++;
   // unlock(&my_list->lock);
}
  /*DO NOT TOUCH THIS UNLESS YOU WANT TO SPEND HOURS DEBUGGING DUMB STUFF*/
void free_list(list * my_list, void (*free_func)(void*)){
   if (my_list == NULL){
        return;
   }
   //if(my_list->thread_safe) w_lock(&my_list->lock);
   for (int i = 0; i < my_list->cap ;i++ ){
        char **temp = (char**)at(my_list, i);
        if (temp == NULL || my_list->isAlloc==false ) continue;
        if (free_func == NULL)free(*temp);
        else (*free_func)(temp);
        temp = NULL;
   }
   //if(my_list->thread_safe) unlock(&my_list->lock);
   if (my_list->arr == NULL) return;
   free(my_list->arr);
   my_list->arr = NULL;
   if (my_list->thread_safe) pthread_rwlock_destroy(&my_list->lock);
   free(my_list);
}
void remove_at(list *my_list, int index) {
    if (my_list == NULL || index < 0 || index >= my_list->len) {
        return; 
    }
    for (int i = index; i < my_list->len - 1; i++) {
        memmove((char *)my_list->arr + i * my_list->dtS, (char *)my_list->arr + (i + 1) * my_list->dtS, my_list->dtS);
    }
    my_list->len--;
}
void remove_no_shift(list *my_list, int index, void (*free_func)(void*)){
    void * element = at(my_list, index);
    if (element == NULL) return;
    if (free_func != NULL) (*free_func)(element);
    else if (my_list->isAlloc) free(element);
    element = NULL;
}
void * at(list *my_list, int index){
    if (my_list == NULL || index < 0 || index >= my_list->len) {
        return NULL;
    }

    //if (my_list->thread_safe) r_lock(&my_list->lock);
    void * ret = (char*)my_list->arr + (index * my_list->dtS);
    //if (my_list->thread_safe) unlock(&my_list->lock);
    return ret;
}
void swap(void * a, void * b, size_t len)
{
    char temp[len];
    
    memcpy(temp, a, len);
    
    memcpy(a, b, len);
    
    memcpy(b, temp, len);
}
int compare_values_by_key(void * one,  void * two){
    return strcmp(((kv *)one)->key, ((kv *)two)->key);
}
int compare_string_values(void* one, void*two){
    return strcmp((char*)one,(char*)two);
}
/*TODO: MERGE SORT WHEN THE NEED ARISES (LISTS > 50 ELEMENTS)*/   
void sort_list(list *my_list, bool ascending, compare func){
    //insertion is faster for smaller lists
    if (my_list == NULL || my_list->len <= 1) {
        return;
    }
    if (my_list->len < 50){
        if (ascending){
            for (int i = 1; i < my_list->len; i++){
                int j = i;
                while( j > 0 && func(at(my_list, j), at(my_list,j-1)) < 0){
                    swap(at(my_list, j), at(my_list,j-1), my_list->dtS);
                    j--;
                }
            }
        }
        else{
            for (int i = 1; i < my_list->len; i++){
                int j = i;
                while( j > 0 && func(at(my_list, j), at(my_list,j-1)) > 0){
                    swap(at(my_list, j), at(my_list,j-1), my_list->dtS);
                    j--;
                }
            }
        }
    }
    return;
}
bool check_in_list(list * my_list, void * element, bool(*func)(void*, void*)){
    if (my_list == NULL || element == NULL) return false;
    for (int i = 0; i < my_list->len; i++){
        if (func(at(my_list, i), element) == 0) return true;
    }
    return false;
}
bool compare_size_t (void * one, void * two){
    return *(size_t*)one == *(size_t*)two;
}
void shift_list(list * my_list){
    for (int i = 0; i < my_list->len - 1; i++) {
        memmove((char *)my_list->arr + i * my_list->dtS, (char *)my_list->arr + (i + 1) * my_list->dtS, my_list->dtS);
    }
}
void * get_last(list * my_list){
    return at(my_list, my_list->len -1);
}
int dump_list_ele(list * my_list, int(*item_write_func)( byte_buffer*, void*), byte_buffer * stream, int num, int start){
    int loop_len = min(start+num, my_list->len);
    for (int i = start; i < loop_len; i++){
        (*item_write_func)(stream, at(my_list, i));
    }
    
    return loop_len;
}



#endif





