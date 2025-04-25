

#include "skiplist.h"

db_unit dummy = {0, NULL};
Node* create_skiplist_node(int level, db_unit key, db_unit value, arena * allocator) {
    Node* node = arena_alloc_expand(allocator, sizeof(Node) + level * sizeof(Node*), NULL);
    if (node == NULL) return NULL;
    node->key = key;
    node->value = value;
    for (int i = 0; i < level; i++) {
        node->forward[i] = NULL;
    }
    return node;
}

SkipList* create_skiplist(int max_nodes,int (*compare)(const void*, const void*)) {
    SkipList* list = (SkipList*)malloc(sizeof(SkipList));
    if (list == NULL) return NULL;
    list->level = 1;
    list->compare = compare;
    list->arena = malloc_arena(max_nodes * sizeof(Node) + MAX_LEVEL * sizeof(Node *));
    list->header = create_skiplist_node(MAX_LEVEL, dummy, dummy, list->arena);
    if (list->header == NULL){
        free(list);
        return NULL;
    }
    return list;
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

    Node* newNode = create_skiplist_node(level, key, value, list->arena);
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
void reset_skip_list(SkipList* list){
    reset_expanded_arena(list->arena);
    list->header = create_skiplist_node(MAX_LEVEL, dummy, dummy, list->arena);
}
void freeSkipList(SkipList* list) {
    if (list == NULL || list->header == NULL) return;
    free_arena(list->arena);
    free(list);
}
int compareInt(const void* a, const void* b) {
    return (*(int*)a - *(int*)b);
}