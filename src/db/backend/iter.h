#include "lsm.h"
#include <stdio.h>
#include "sst_builder.h"
#include "indexer.h"
#include <stdlib.h>
#include "../../ds/associative_array.h"
#include "../../ds/frontier.h"
#include "../../ds/hashtbl.h"
#define EODB 0
#pragma once

/* write a proper implementation plan*/
/*The read path involves block_indexs with values, encased by sst tables, which are seperated into
seven levels. The iterator is used for range scans/deletes and allows an abstraction of as if 
every key-value pair can be accessed linearly in memeory. Since every sst table outside of level 0
has a gurantee of no range overlaps, only one sst_iter is needed per level and one for memetable\
level 0. Since blocks also have a gurantee of being ordered, only one block index cursor is required
per level. This enables each sub-iter to return their "next" and have that compared to every
other "next".*/

typedef struct block_iter {
    k_v_arr * arr;
    int curr_key_ind;          
    block_index *index;    
} block_iter;

typedef struct sst_iter {
    block_iter cursor; 
    sst_f_inf *file; //pointer to the sst_f_inf
    int curr_index;      
} sst_iter;

typedef struct level_iter {
    sst_iter cursor;    
    list *level; //simply a pointer to the real sst_list of the level    
    int curr_index;          
} level_iter;

typedef struct mem_table_iter {
    Node *cursor; 
    mem_table *t;           
} mem_table_iter;
/*A single database iterator*/
typedef struct aseDB_iter {
    mem_table_iter mem_table[2]; 
    level_iter l_1_n_level_iters [6];
    list * l_0_sst_iters;
    frontier * pq;
    list * ret;
    cache * c;
} aseDB_iter;






int compare_merge_data(const void * one, const void * two){
    if (one == NULL || two == NULL) return -5;
    const merge_data * m1 = one;
    const merge_data * m2 = two;
    return strcmp(m1->key->entry, m2->key->entry);
}
aseDB_iter * create_aseDB_iter(){
    aseDB_iter * dbi = malloc(sizeof(aseDB_iter));
    if (dbi == NULL ) return NULL;
    dbi->l_0_sst_iters = List(0,sizeof(sst_iter), false);
    dbi->pq = Frontier(sizeof(merge_data),false, &compare_merge_data);
    dbi->ret = List(0,sizeof(merge_data), false);
    if (dbi->l_0_sst_iters == NULL || dbi->l_1_n_level_iters == NULL || dbi->ret == NULL){
        if (dbi->l_0_sst_iters !=NULL){
            free_list(dbi->l_0_sst_iters, NULL);
        }
        if (dbi->l_1_n_level_iters != NULL){
            free_list(dbi->l_1_n_level_iters, NULL);
        }
        if (dbi->pq == NULL){
            free_front(dbi->pq);
        }
        free(dbi);
        return NULL;
    }
    return dbi;
}
void init_block_iter(block_iter *b_cursor, block_index *file) {
    b_cursor->curr_key_ind = 0;
    b_cursor->index = file;
}
void init_sst_iter(sst_iter * cursor, sst_f_inf* level) {
    cursor->curr_index = 0;
    cursor->file = level;
    block_index * first = at(cursor->file->block_indexs, 0);
    if (first == NULL){
        cursor->file = NULL;
    }
    init_block_iter(&cursor->cursor, first);
}
void init_mem_table_iters(mem_table_iter *mem_table_iters, storage_engine *s, size_t num_tables) {
    for (size_t i = 0; i < num_tables; i++) {
        mem_table_iters[i].t = s->table[i];
        mem_table_iters[i].cursor = s->table[i]->skip->header->forward[0];
    }
}
void init_level_iters(level_iter *level_iters, storage_engine *s, size_t num_levels) {
    for (size_t i = 0; i < num_levels; i++) {
        level_iters[i].curr_index = 0;
        level_iters[i].level = s->meta->sst_files[i+1];
        if (level_iters[i].level == NULL){
            level_iters[i].curr_index = -1;
            continue;
        }
        sst_f_inf * f = at(level_iters[i].level, 0);
        if (f == NULL) continue;
        init_sst_iter(&level_iters[i].cursor,f);
    }
}
void init_level_0_sst_iters(list *l_0_sst_iters, storage_engine *s) {
    for (int i = 0; i < s->meta->sst_files[0]->len; i++) {
        sst_iter cursor;
        sst_f_inf* file = at(s->meta->sst_files[0], i);
        if (file == NULL){
            l_0_sst_iters->len --;
            continue;
        }
        init_sst_iter(&cursor,file);
        insert(l_0_sst_iters, &cursor);
    }
}

void init_aseDB_iter(aseDB_iter *dbi, storage_engine *s) {
    dbi->c = s->cach;
    init_mem_table_iters(dbi->mem_table, s, 2);
    init_level_0_sst_iters(dbi->l_0_sst_iters, s);
    init_level_iters(dbi->l_1_n_level_iters, s, 6);
}
/* returns the next value*/
merge_data next_entry(block_iter *b, cache * c, char * file_name) {
    merge_data final;
    final.key = NULL;
    final.value = NULL;
    if (b == NULL || b->arr == NULL || b->curr_key_ind >= b->arr->len) {
       return final;
    }
    if (b->index->num_keys!= b->arr->len){
        cache_entry * ce = retrieve_entry(c,b->index, file_name);
        b->arr = ce->ar;
    }
    db_unit *ret = &b->arr->values[b->curr_key_ind];
    db_unit *key = &b->arr->keys[b->curr_key_ind];
    final.key = key;
    final.value = ret;
    b->curr_key_ind++;
    return final;
}
merge_data next_key_block(sst_iter *sst, cache *c) {
    merge_data bad_return;
    bad_return.key = NULL;
    bad_return.value = NULL;
    
    if (sst == NULL || c == NULL || sst->file == NULL) {
        return  bad_return;
    }
    if (sst->cursor.curr_key_ind >= sst->cursor.arr->len) {
        sst->curr_index++;
        if (sst->curr_index >= sst->file->block_indexs->len) {
            return bad_return;
        }
        block_index *index = at(sst->file->block_indexs, sst->curr_index);
        if (index == NULL) {
            return bad_return;
        }
        cache_entry *ce = retrieve_entry(c, index, sst->file->file_name);
        if (ce == NULL || ce->ar == NULL) {
            return bad_return;
        }
        sst->cursor.arr = ce->ar;
        sst->cursor.curr_key_ind = 0;
        sst->cursor.index = index;
    }
    return next_entry(&sst->cursor, c, sst->file->file_name);
}
merge_data next_sst_block(level_iter *level, cache *c) {
    merge_data bad_return;
    bad_return.key = NULL;
    bad_return.value = NULL;
    if (level == NULL || c == NULL || level->level == NULL) {
        return bad_return;
    }
    merge_data next = next_key_block(&level->cursor, c);
    if (next.key != NULL) return next;
    level->curr_index++;
    if (level->curr_index >= level->level->len) {
        return bad_return;
    }
    sst_f_inf *f = at(level->level, level->curr_index);
    if (f == NULL) return bad_return;
     
    level->cursor.file = f;
    level->cursor.curr_index = 0;
    block_index * next_block= at(level->cursor.file->block_indexs,0);
    init_block_iter(&level->cursor.cursor,next_block);
    return next_key_block(&level->cursor, c);
}
merge_data get_curr_kv(sst_iter  sst_it){
    block_iter  block_it = sst_it.cursor;
    merge_data ret;
    ret.key = &block_it.arr->keys[block_it.curr_key_ind];
    ret.value = &block_it.arr->values[block_it.curr_key_ind];
    return ret;
}
merge_data next_entry_memtable(mem_table_iter * iter){
    iter->cursor = iter->cursor->forward[0];
    merge_data m;
    m.key = &iter->cursor->key;
    m.value =&iter->cursor->value;
    return m;
}
/*seeks a prefix in every table*/
void seek_sst(sst_iter* sst_it, cache * cache, const char * prefix){
        sst_f_inf * f = sst_it->file;
        size_t b_index = find_block(f, prefix);
        sst_it->curr_index = b_index;
        block_index * entry = at(f->block_indexs, b_index);
        sst_it->cursor.index = entry;
        cache_entry * c = retrieve_entry(cache, sst_it->cursor.index, f->file_name);
        int k_v_array_index = prefix_b_search(c->ar,prefix);
        sst_it->cursor.arr = c->ar;
        sst_it->cursor.curr_key_ind = k_v_array_index;
}
/*gotta make everything work with the new pq */
void seek(aseDB_iter * iter , const char * prefix){
    cache * cach = iter->c;
    for(int i = 0; i < 6; i++){
        size_t f_index =  find_sst_file(iter->l_1_n_level_iters[i].level, iter->l_1_n_level_iters[i].level->len,prefix);
        sst_f_inf * f = at(iter->l_1_n_level_iters[i].level, f_index);
        if (f == NULL)continue;
        iter->l_1_n_level_iters[i].cursor.file = f;
        iter->l_1_n_level_iters[i].curr_index = f_index;
        seek_sst(&iter->l_1_n_level_iters[i].cursor, cach, prefix);
        merge_data m = get_curr_kv(iter->l_1_n_level_iters[i].cursor);
        m.src = lvl_1_7;
        m.index = i;
        enqueue(iter->pq,&m);
        iter->l_1_n_level_iters[i].cursor.cursor.curr_key_ind ++;
    }
     for (int i = 0; i < iter->l_0_sst_iters->len; i++){
        sst_iter *sst_it = at(iter->l_0_sst_iters, i);
        if(sst_it->file== NULL) continue;
        seek_sst(sst_it, cach, prefix);
        merge_data merge = get_curr_kv(*sst_it);
        merge.src = lvl_0;
        merge.index = i;
        enqueue(iter->pq, &merge);
        sst_it->cursor.curr_key_ind ++;
    }
    for(int i =0; i < 2; i++){
        SkipList * mem_t_s = iter->mem_table[i].t->skip;
        if (mem_t_s == NULL){
            iter->mem_table[i].cursor= NULL;
        }
        Node * seeked_next = search_list_prefix(mem_t_s, (void*)prefix);
        if (seeked_next == NULL) continue;
        iter->mem_table[i].cursor = seeked_next;
        merge_data entry;
        entry.key = &seeked_next->key;
        entry.value = &seeked_next->value;
        entry.index = i;
        entry.src = memtable;
        enqueue(iter->pq, &entry);
        iter->mem_table[i].cursor = seeked_next->forward[0];
    }
}
merge_data  same_merge_key(const merge_data next, const merge_data last){
    if (next.src == last.src) return (next.index <  last.index) ? next:last;

    else return (next.src < last.src) ? next : last;
}
/*carefully study this function and find all of the edge cases. 
One that exists is running out of things to check on the current level but not updating the other
levels. Think of a solution*/
merge_data aseDB_iter_next(aseDB_iter * iter){
    merge_data next;
    merge_data next_grab;
     

    merge_data dummy = {EODB,EODB, -1};
    int cmp_res = -1;
    do
    {
        if (iter->pq->queue->len == 0) return dummy;
        dequeue(iter->pq, &next);
        merge_data * last_element = get_last(iter->ret);
        sst_iter *old_sst_src = NULL;
        level_iter *old_src_level;
        mem_table_iter old_src_memtable;

        switch (next.src){
            case (int)lvl_0:
                old_sst_src  = at(iter->l_0_sst_iters,next.index);
                next_grab =  next_key_block(old_sst_src, iter->c);
                next_grab.index = next.index;
                next_grab.src = lvl_0;
                break;
            case (int)lvl_1_7:
                old_src_level = &iter->l_1_n_level_iters[(int)next.index];
                next_grab = next_sst_block(old_src_level, iter->c);
                next_grab.index = next.index;
                next_grab.src = lvl_1_7;
                break;
            case (int)memtable:
                old_src_memtable = iter->mem_table[(int)next.index];
                next_grab = next_entry_memtable(&old_src_memtable);
                next_grab.src = memtable;
                next_grab.index = next.index;
                break;
            default:
                return dummy;

        }
        if (next_grab.key == NULL || (last_element != NULL) && (strcmp(next_grab.key->entry, last_element->key->entry) == 0 && strcmp(next_grab.value->entry, last_element->value->entry)==0 )){
            return next;
        }
        enqueue(iter->pq, &next_grab);
        cmp_res = compare_merge_data(&next, last_element);

        if (cmp_res == 0 && next.key !=NULL){
            merge_data element_to_keep=  same_merge_key(next,*last_element);
            inset_at(iter->ret, &element_to_keep, iter->ret->len-1);

        }

        else if (next.key!= NULL)insert(iter->ret,&next);

    } while (cmp_res == 0 && next.key !=NULL) ;

    merge_data* final= get_last(iter->ret);
    return (final != NULL) ? *final : dummy;
}
int write_db_entry(byte_buffer * b, void * element){
    merge_data * m = element;
    if (m->key == NULL || m->value == NULL){
        return -1;
    }
    int size= 0;
    size+= write_db_unit(b, *m->key);
    size+= write_db_unit(b, *m->value);
    return size;
}
void free_aseDB_iter(aseDB_iter *iter) {
    free_list(iter->l_0_sst_iters, NULL);
    free(iter);
}




