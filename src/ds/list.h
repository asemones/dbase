#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "hashtbl.h"
#include <pthread.h>
#include "byte_buffer.h"

#ifndef LIST_H
#define LIST_H

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
    pthread_mutex_t write_lock;
    pthread_mutex_t read_lock;
}list;
typedef int (*compare)( void* , void*);

list* List(int Size, int dataTypeSize, bool alloc);
list* thread_safe_list(int Size, int dataTypeSize, bool alloc);


void expand(list * my_list);
void insert(list * my_list, void * element);
void* internal_at_unlocked(list * my_list, int index);
void inset_at_unlocked(list * my_list, void * element, int index);
void inset_at(list * my_list, void * element, int index);


void free_list(list * my_list, void (*free_func)(void*));
void remove_at(list * my_list, int index);
void remove_at_unlocked(list * my_list, int index);
void remove_no_shift(list * my_list, int index, void (*free_func)(void*));
void * at(list * my_list, int index);
void at_copy(list * my_list, void * storage, int index);
void swap(void * a, void * b, size_t len);
void shift_list(list * my_list, int start_index);
void * get_last(list * my_list);


int compare_values_by_key(void * one, void * two);
int compare_string_values(void * one, void * two);
void sort_list(list * my_list, bool ascending, compare func);
bool check_in_list(list * my_list, void * element, bool (*func)(void*, void*));
bool compare_size_t(void * one, void * two);
int dump_list_ele(list * my_list, int (*item_write_func)(byte_buffer*, void*),
                  byte_buffer * stream, int num, int start);
int merge_lists(list * merge_into, list * merge_from, compare func);



#endif





