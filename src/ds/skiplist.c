

#include "skiplist.h"
#define NUM_ARENA 50
db_unit dummy = {0, NULL};
Node* allocate_a_node(SkipList * list, int level, db_unit key, db_unit value);
Node* create_skiplist_node(int level, db_unit key, db_unit value) {
    Node* node = (Node*)malloc(sizeof(Node) + level * sizeof(Node*));
    if (node == NULL) return NULL;
    node->key = key;
    node->value = value;
    for (int i = 0; i < level; i++) {
        node->forward[i] = NULL;
    }
    return node;
}

SkipList* create_skiplist(int (*compare)(const void*, const void*)) {
    SkipList* list = (SkipList*)malloc(sizeof(SkipList));
    list->skiplist_alloactor = calloc(sizeof(arena) * NUM_ARENA); /*cheap*/
    if (list == NULL) return NULL;
    list->level = 1;
    list->compare = compare;
    list->header = allocate_a_node(list, MAX_LEVEL, dummy, dummy);
    if (list->header == NULL){
        free(list);
        return NULL;
    }
   
    memset(&list->skiplist_alloactor[0], 0, sizeof(arena) * NUM_ARENA);
    
    return list;
}
void reset_skip_list(SkipList * list){
    for(int i = 0; i < list->arena_num; i++){
        reset_arena(&list->skiplist_alloactor[i]);
    }
    list->header = create_skiplist_node(MAX_LEVEL, dummy, dummy);
}
static inline int calculate_avg_node_size(){
    const int avg_num_level = 6;
    return sizeof(Node) + avg_num_level * sizeof(Node*);
}
/*if average entry size set to 0 use default*/
void init_skiplist_allocator(SkipList * list, int entry_total, int average_entry_size, int start_size){
    const int default_average = 128;
    if (average_entry_size <= 0 ) average_entry_size = default_average;
    int num_nodes = entry_total / average_entry_size;
    int mem_size = num_nodes * calculate_avg_node_size();
    int num_arenas_to_make = mem_size/num_nodes;
    int size_per_arena = mem_size/num_arenas_to_make;
    int rounded_size = size_per_arena + (size_per_arena % calculate_avg_node_size());
    list->curr_arena = 0;
    if (start_size >= mem_size){
        list->arena_num = 1;
        init_arena(&list->skiplist_alloactor[0], start_size);
        return;
    }
    for(int i = 0; i <  num_arenas_to_make; i++){
        init_arena(&list->skiplist_alloactor[i],, size_per_arena);
    }
    list->arena_num = num_arenas_to_make
    
}
Node* allocate_a_node(SkipList * list, int level, db_unit key, db_unit value){
    Node* node = arena_alloc(&list->skiplist_alloactor[list->curr_arena], sizeof(Node) + level * sizeof(Node*));
    if (node == NULL){
        list->curr_arena ++;
        node = arena_alloc(&list->skiplist_alloactor[list->curr_arena], sizeof(Node) + level * sizeof(Node*))
    }
    if (node == NULL) {
        return NULL;
    }
    node->key = key;
    node->value = value;
    for (int i = 0; i < level; i++) {
        node->forward[i] = NULL;
    }
    return node;
}
int random_level() {
    int level = 1;
    while ((rand() & 1) && level < MAX_LEVEL) {
        level++;
    }
    return level;
}
int skip_from_stream(SkipList * s, byte_buffer * stream){
    int ret= 0;
    while (stream->read_pointer < stream->curr_bytes){
        db_unit key, value;
        read_buffer(stream, &key.len, sizeof(key.len));
        read_buffer(stream, key.entry, key.len);
        read_buffer(stream, &value.len, sizeof(key.len) );
        read_buffer(stream, value.entry,value.len);
        insert_list(s, key, value);
        ret = 4+ value.len + key.len;
    }
    return ret;
}

int insert_list(SkipList* list, db_unit key, db_unit value) {
    Node* update[MAX_LEVEL];
    Node* x = list->header;
    for (int i = list->level - 1; i >= 0; i--) {
        while (x->forward[i] && list->compare(x->forward[i]->key.entry, key.entry) < 0) {
            x = x->forward[i];
        }
        update[i] = x;
    }

    int level = random_level();
    if (level > list->level) {
        for (int i = list->level; i < level; i++) {
            update[i] = list->header;
        }
        list->level = level;
    }

    Node* newNode = allocate_a_node(list, level, key, value );
    if (newNode == NULL){
        return STRUCT_NOT_MADE;
    }
    for (int i = 0; i < level; i++) {
        newNode->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = newNode;
    }
    return 0;
}

Node* search_list(SkipList* list, const void* key) {
    Node* x = list->header;
    for (int i = list->level - 1; i >= 0; i--) {
        while (x->forward[i] && list->compare(x->forward[i]->key.entry, key) < 0) {
            x = x->forward[i];
        }
    }
    x = x->forward[0];
    if (x && list->compare(x->key.entry, key) == 0) {
        return x;
    } 
    else {
        return NULL;
    }
}
Node* search_list_prefix(SkipList* list, void* key) {
    Node* x = list->header;
    for (int i = list->level - 1; i >= 0; i--) {
        while (x->forward[i] && list->compare(x->forward[i]->key.entry, key) < 0) {
            x = x->forward[i];
        }
    }
    x = x->forward[0];
    if (x){
        return x;
    }
    else return NULL;

}
/*this should never really be used*/
void delete_element(SkipList* list, void* key) {
    Node* update[MAX_LEVEL];
    Node* x = list->header;
    for (int i = list->level - 1; i >= 0; i--) {
        while (x->forward[i] && list->compare(x->forward[i]->key.entry, key) < 0) {
            x = x->forward[i];
        }
        update[i] = x;
    }

    x = x->forward[0];
    if (x && list->compare(x->key.entry, key) == 0) {
        for (int i = 0; i < list->level; i++) {
            if (update[i]->forward[i] != x) {
                break;
            }
            update[i]->forward[i] = x->forward[i];
        }
        free(x);
        while (list->level > 1 && list->header->forward[list->level - 1] == NULL) {
            list->level--;
        }
    }
}
void freeSkipList(SkipList* list) {
    if (list == NULL || list->header == NULL) return;
    Node* current = list->header->forward[0];
    while (current != NULL) {
        Node* next = current->forward[0];
        free(current);
        current = next;
    }
    free(list->header);
    free(list);
}
int compareInt(const void* a, const void* b) {
    return (*(int*)a - *(int*)b);
}

int compareString(const void* a, const void* b) {
    return strcmp((const char*)a, (const char*)b);
}