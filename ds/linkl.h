#include <stdio.h>
#include <stdlib.h>
#include "arena.h"
#include "../util/alloc_util.h"
#pragma once

typedef struct ll_node{
    void * data;
    struct  ll_node *next;
    struct ll_node * prev;
} ll_node;

typedef struct ll{
    ll_node *head;
    ll_node * iter;
    size_t num_linked;
    arena * a;
}ll;
ll * create_ll(arena * a){
    ll * list = (ll*)wrapper_alloc((sizeof(ll)), NULL,NULL);
    list->head = arena_alloc(a, sizeof(ll_node));
    list->num_linked = 0;
    list->a = a;
    return list;
}

ll * free_ll(ll * list, void (*free_func)(void*)){
    ll_node * current = list->head;
    while (current != NULL){
        ll_node * temp = current;
        current = current->next;
        if (free_func) free_func(temp->data);
        if (list->a == NULL) free(temp);
    }
    list->a = NULL;
    free(list);
    return NULL;
}
void push_ll_void(void * key, void * value,void* v_ll){
    ll * l = (ll*)v_ll;
    kv * temp = arena_alloc(l->a, sizeof(kv));
    temp->key = key;
    temp->value = value;
    l->iter->data = temp;
    l->iter->next = arena_alloc(l->a, sizeof(ll_node));
    l->iter->next->prev = l->iter;
    l->iter = l->iter->next;

}
ll * add_to_ll(ll * list, void * data){
    list->iter->data = data;
    list->iter->next = arena_alloc(list->a, sizeof(ll_node));
    list->iter->next->prev = list->iter;
    list->iter = list->iter->next;
    return list;
}
void  remove_ll (ll_node * node, ll * list){
    if (node ==NULL) return;
    ll_node * temp_prev = node->prev;
    ll_node *temp_next = node->next;

    if (temp_prev != NULL) temp_prev->next = temp_next;
    if (temp_next != NULL) temp_next->prev = temp_prev;
    list->num_linked--;

}