#include "sst_sl.h"

#include "slab.h"
sst_node * slab_alloc_func(slab_allocator * alloc, int lvl){
    sst_node * node= slalloc(alloc, sizeof(sst_node) + sizeof(sst_node*) * lvl);
    return node;
}
sst_node * create_sst_sl_node(int level,sst_f_inf inf, slab_allocator * allocator) {
    sst_node * node = slab_alloc_func(allocator,level);
    if (node == NULL) return NULL;
    node->sst = inf;
    for (int i = 0; i < level; i++) {
        node->forward[i] = NULL;
    }
    return node;
}

sst_sl* create_sst_sl(int max_nodes) {
    sst_sl* list = (sst_sl*)malloc(sizeof(sst_sl));
    if (list == NULL) return NULL;
    list->level = 1;
    const int slab_size = 15;
    list->allocator = create_allocator(sizeof(slab_allocator) *slab_size, max_nodes);
    sst_f_inf inf;
    memset(&inf, 0, sizeof(inf));
    list->header = create_sst_sl_node(MAX_LEVEL, inf, &list->allocator) ;
    if (list->header == NULL){
        free(list);
        return NULL;
    }
    list->use_default = 1;
    return list;
}
int random_level() {
    int level = 1;
    while ((fast_coin()) && level < MAX_LEVEL) {
        level++;
    }
    return level;
}
int sst_insert_list(sst_sl* list, sst_f_inf sst) {
    sst_node* update[MAX_LEVEL];
    sst_node* x = list->header;
    for (int i = list->level - 1; i >= 0; i--) {
        while (x->forward[i] && compare_sst(&sst, &x->forward[i]->sst) < 0) {
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
    sst_node* newsst_node = create_sst_sl_node(level,sst, &list->allocator);
    if (newsst_node == NULL){
        return STRUCT_NOT_MADE;
    }
    for (int i = 0; i < level; i++) {
        newsst_node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = newsst_node;
    }
    return 0;
}

sst_node* sst_search_list(sst_sl* list, const void * key) {
    sst_node* x = list->header;
    for (int i = list->level - 1; i >= 0; i--) {
        while (x->forward[i] && strcmp(key, x->forward[i]->sst.min) < 0) {
            x = x->forward[i];
        }
    }
    x = x->forward[0];
    if (x && strcmp(key, x->sst.min) == 0) {
        return x;
    } 
    else {
        return NULL;
    }
}
sst_node* sst_search_list_prefix(sst_sl* list, void* key) {
    sst_node* x = list->header;
    for (int i = list->level - 1; i >= 0; i--) {
        while (x->forward[i] && strcmp(key, x->forward[i]->sst.min) < 0) {
            x = x->forward[i];
        }
    }
    x = x->forward[0];
    if (x){
        return x;
    }
    else return NULL;

}
sst_node * sst_search_between(sst_sl * list, void * key){
    sst_node* x = list->header;
    for (int i = list->level - 1; i >= 0; i--) {
        while (x->forward[i] && strcmp(key, x->forward[i]->sst.min) < 0) {
            x = x->forward[i];
        }
    }
    x = x->forward[0];
    if (x && strcmp(key, x->sst.max) >= 0){
        return x;
    }
    else return NULL;
}
void sst_delete_element(sst_sl* list, void* key) {
    sst_node* update[MAX_LEVEL];
    sst_node* x = list->header;
    for (int i = list->level - 1; i >= 0; i--) {
        while (x->forward[i] && strcmp(key, x->forward[i]->sst.min) < 0) {
            x = x->forward[i];
        }
        update[i] = x;
    }

    x = x->forward[0];
    if (x && strcmp(key, x->sst.min) == 0) {
        for (int i = 0; i < list->level; i++) {
            if (update[i]->forward[i] != x) {
                break;
            }
            update[i]->forward[i] = x->forward[i];
        }
        while (list->level > 1 && list->header->forward[list->level - 1] == NULL) {
            list->level--;
        }
    }

}
void freesst_sl(sst_sl* list) {
    if (list == NULL || list->header == NULL) return;
    free_slab_allocator(list->allocator);
    free(list);
}