#include <stdio.h>
#include <stdlib.h>
#include "byte_buffer.h"
#include "arena.h"
#include "hashtbl.h"
#include "structure_pool.h"
#include "linkl.h"



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
/* cache with lru evict policy implemented via double linked list and hashmap
hashmap values contain the address of the ll_node where the data is present
adding and removing nodes is done in a special fornat to remove excess memory allocations and improve memory locaclity

*/
cache * create_cache(size_t capacity, size_t page_size);
void free_cache(cache * c);
int add_page(cache * c, FILE * f, char * min_key);
void* get_page(cache * c, char * min_key);
ll_node * get_node(cache * c, char * min_key);
void evict(cache * c);
size_t combine_values(size_t level, size_t sst_ind, size_t block_index);


cache * create_cache(size_t capacity, size_t page_size){
    cache * c = (cache*)malloc(sizeof(cache));
    c->capacity = capacity;
    c->page_size = page_size;
    c->filled_pages= 0;
    c->map = Dict();
    c->mem = calloc_arena(capacity/page_size * 32);
    c->queue = create_ll(c->mem);
    c->head = c->queue->head;
    c->tail = c->queue->head;
    c->max_pages = capacity / page_size;

    ll_node * last = c->queue->head;
    c->queue->iter = c->queue->head;
    for (int i = 0; i < c->max_pages; i++){        
         add_to_ll(c->queue, create_buffer(page_size));
         last = last->next;
    }
    c->queue->iter = c->head;
    c->tail = last;  
    c->tail->data =  create_buffer(page_size);
    return c;
}
size_t combine_values(size_t level, size_t sst_ind, size_t block_index){
    return (level << 16) | (sst_ind << 8) | block_index;
}
void evict(cache * c){
    if (c->filled_pages == 0) return;
    ll_node * evicted = c->tail; 
    reset_buffer((byte_buffer*)evicted->data);
    c->tail = evicted->prev;
    ll_node * temp = c->head->prev;
    c->head->prev = evicted;
    evicted->next = c->head;
    evicted->prev= temp;
    temp->next = evicted;
    c->filled_pages--;
}

int add_page(cache * c, FILE * f, char * min_key){
    byte_buffer * page = (byte_buffer*)c->head->data;
    ll_node * node = (ll_node*)get_node(c, min_key);
    /*swap nodes to stop alloc*/
    if (c->filled_pages == c->max_pages)
    {
        evict(c);
    }
    if (node){
        remove_ll(node, c->queue);
        ll_node * temp = c->head->next;
        temp->prev->next = node; 
        c->head->next = node;
        node->prev = temp->prev;
        temp->prev = node;
        node->next = temp;
        c->head = node;
        return 1;
    }
    page->curr_bytes = fread(page->buffy, 1, c->page_size, f);
    add_kv(c->map, (void*)min_key,(void*)c->head);
    c->filled_pages++;
    c->head = c->head->prev;

    return 0;
}
ll_node * get_node(cache * c, char * min_key){
    return (ll_node*)get_v(c->map, (void*)min_key);
}
void* get_page(cache * c,char * min_key){
    ll_node * node= (ll_node*)get_node(c->map, (void*)min_key);
    if (node == NULL) return NULL;
    byte_buffer * page = (byte_buffer*)node->data;

    if (page->curr_bytes == 0)  return page;
    else return NULL;
}
void free_cache(cache * c){
    free_ll(c->queue, &free_buffer);
    free_arena(c->mem);
    free_dict(c->map);
    free(c);
}

