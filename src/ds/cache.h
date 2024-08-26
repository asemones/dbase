#include <stdio.h>
#include <stdlib.h>
#include "byte_buffer.h"
#include "arena.h"
#include "hashtbl.h"
#include "structure_pool.h"
#include "linkl.h"
#include "associative_array.h"


#pragma once

typedef struct cache{
    size_t capacity;
    size_t page_size;
    size_t filled_pages;
    size_t max_pages;
    dict * map;
    ll * queue;
    ll_node * head;
    ll_node * tail;
    arena * mem;

}
cache;
typedef struct cache_entry{
    byte_buffer * buf;
    k_v_arr *ar;
}cache_entry;
/* cache with lru evict policy implemented via double linked list and hashmap
hashmap values contain the address of the ll_node where the data is present
adding and removing nodes is done in a special fornat to remove excess memory allocations and improve memory locaclity

*/
cache * create_cache(size_t capacity, size_t page_size);
void free_cache(cache * c);
ll_node *  add_page(cache * c, FILE * f, char * min_key, size_t size);
ll_node* get_page(cache * c, char * min_key);
ll_node * get_node(cache * c, char * min_key);
void evict(cache * c);
size_t combine_values(size_t level, size_t sst_ind, size_t block_index);

cache_entry* create_cache_entry(size_t page_s, size_t num_keys, arena * a) {
    cache_entry* entry = (cache_entry*)arena_alloc(a, sizeof(*entry));
  
    entry->buf = create_buffer(page_s); 

    entry->ar = arena_alloc(a, sizeof(k_v_arr));
   
    entry->ar->keys = (char**)arena_alloc(a,num_keys * sizeof(char*));
    entry->ar->values = (char**)arena_alloc(a,num_keys * sizeof(char*));
    

    entry->ar->cap = num_keys;
    entry->ar->len = 0;

    return entry;
}
cache * create_cache(size_t capacity, size_t page_size){
    cache * c = (cache*)malloc(sizeof(cache));
    c->capacity = capacity;
    c->page_size = page_size;
    c->filled_pages= 0;
    c->map = Dict();
    c->mem = calloc_arena((capacity) * 2);
    c->queue = create_ll(c->mem);
    c->head = c->queue->head;
    c->tail = c->queue->head;
    c->max_pages = capacity / page_size;

    ll_node * last = c->queue->head;
    c->queue->iter = c->queue->head;
    for (int i = 1; i < c->max_pages; i++){      
         add_to_ll(c->queue, create_cache_entry(page_size, page_size/16, c->mem));
         last = last->next;
    }
    c->queue->iter = c->head;
    c->tail = last;  
    c->tail->data = create_cache_entry(page_size, page_size/8, c->mem);
    return c;
}
size_t combine_values(size_t level, size_t sst_ind, size_t block_index){
    return (level << 16) | (sst_ind << 8) | block_index;
}
void evict(cache * c){
    if (c->filled_pages == 0) return;
    if (c->max_pages == 1){
        reset_buffer((byte_buffer*)c->tail->data);
        remove_kv(c->map, ((byte_buffer*)(c->tail->data))->utility_ptr);
        return;
    }
    ll_node * evicted = c->tail;
    cache_entry * entry = evicted->data;

    reset_buffer(entry->buf);
    entry->ar->len = 0;
    c->tail = evicted->prev;
    c->tail->next = NULL;  
    
    evicted->prev = NULL; 
    evicted->next = c->head;
    c->head->prev = evicted;
    c->head = evicted; 
    remove_kv(c->map, entry->buf->utility_ptr);
    c->filled_pages--;
}

ll_node * add_page(cache * c, FILE * f, char * min_key, size_t size) {
    ll_node * node = get_v(c->map, min_key);
    
    if (c->filled_pages == c->max_pages) {
        evict(c);
    }

    if (node && node->data) {
     
        if (node->prev) node->prev->next = node->next;
        if (node->next) node->next->prev = node->prev;
        if (c->tail == node) c->tail = node->prev;

       
        node->next = c->head;
        node->prev = NULL;
        if (c->head) c->head->prev = node;
        c->head = node;

        return c->head;
    }
    ll_node * page = c->head;
    cache_entry * entry = (cache_entry *)page->data;
    byte_buffer * buffer = entry->buf;

    buffer->curr_bytes = fread(buffer->buffy, 1, size, f);
    buffer->utility_ptr = min_key;
    add_kv(c->map, (void*)min_key, (void*)page);
    c->filled_pages++;

    if (c->head->next == NULL) {
        c->tail = c->head; 
        return c->head;
    }
    c->head = c->head->next;
    c->head->prev = NULL;
    page->next = NULL;
    page->prev = c->tail;
    c->tail->next = page;
    c->tail = page;

    return c->tail;  
}
ll_node* get_page(cache * c,char * min_key){
    return (ll_node*)get_v(c->map, min_key);
}
cache_entry *retrieve_entry(cache * cach, block_index * index, char * file_name){
    ll_node * page = get_page(cach, index->min_key);
        cache_entry * c;
        if (page){
            c = page->data;
        }
        else {
            FILE * sst_file = fopen(file_name,"rb");
            fseek(sst_file, index->offset, SEEK_SET);
            ll_node * fresh_page = add_page(cach, sst_file, index->min_key, index->len);
            c = fresh_page->data;
            deseralize_into_structure(&into_array,c->ar, c->buf);
            fclose(sst_file);
        }
        return c;
}
void free_cache(cache * c){
    free_ll(c->queue, NULL);
    free_arena(c->mem);
    free_dict_no_element(c->map);
    free(c);
}

