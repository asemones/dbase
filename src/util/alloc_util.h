#include <stdlib.h>
#include <stdio.h>
#pragma once




void * wrapper_alloc (size_t size, void * alloc_struct, void*(*alloc_func)(void*, size_t)){
    if (alloc_struct != NULL && alloc_func !=NULL){
        return alloc_func(alloc_struct,size);
    } 
    else return malloc(size);
}
void * wrapper_realloc (void * ptr, size_t size, void * alloc_struct, void*(*alloc_func)(void*, size_t)){
    if (alloc_struct != NULL && alloc_func !=NULL){
        return alloc_func(alloc_struct,size);
    } 
    void * tempoary = realloc(ptr, size);
    if (tempoary == NULL){
        free(ptr);
        return NULL;
    }
    return tempoary;
}