#include "txn.h"

int init_txn(txn * empty,snapshot * snap, int num){
    empty->id = num;
    empty->snapshot =snap;
    empty->state  = TXN_INACTIVE;
    empty->results = List(0, sizeof(union result), false);
    empty->ops = List(0, sizeof(txn_op), false);
    return num ++;
}
void add_op(txn * to_add, db_unit *key, db_unit* value, txn_op_type op_t){
    txn_op op;
    op.key = key;
    op.value = value;
    op.type = op_t;
    insert(to_add->ops, &op);
}
char * scan_l_0_tx(sst_f_inf * ssts, const char * key, int max, struct timeval tx_timer){
    struct timeval newest;
    bool found = false;
    char * newest_key = NULL;
    for (int i = 0 ; i < max; i++){
        bloom_filter * filter = ssts[i].filter;
        if (!check_bit(key, filter)) continue;
        size_t index_block= find_block(sst, keyword);
        block_index * index = at(sst->block_indexs, index_block);
        
        cache_entry c = retrieve_entry_sharded(engine->cach, index, sst->file_name, sst);
        if (c.buf== NULL ){
            engine->error_code = INVALID_DATA;
            return NULL;
        }
        int k_v_array_index = json_b_search(c.ar, key);
        if (k_v_array_index== -1) continue;
        if (found == false){
            newest_key = c.ar->values[k_v_array_index].entry;  
            newest = sst[i].time;  
        }
        /*fix/double check timer comps*/
        else if (compare_time_stamp(tx_timer, sst[i].time) > 0){
            continue;
        }
        else if (compare_time_stamp(newest, ssts[i].time) > 0){
            newest_key = c.ar->values[k_v_array_index].entry;    
            newest = sst[i].time;  
        }

    }
    return newest_key;
}
char * disk_read_snap(snapshot * snap, const char * keyword, struct timeval tx_timer){ 
    char * l_0_test;
    if ((l_0_test = scan_l_0_tx(snap->sst_files[0]->arr,keyword, snap->sst_files[0]->len, tx_timer))){
        return l_0_test;
    }
    for (int i= 1; i < MAX_LEVELS; i++){
        if (snap->sst_files[i] == NULL |snap->sst_files[i]->len ==0) continue;

        list * sst_files_for_x = snap->sst_files[i];
        size_t index_sst = find_sst_file(sst_files_for_x, sst_files_for_x->len, keyword);
        if (index_sst == -1) continue;
        
        sst_f_inf * sst = at(sst_files_for_x, index_sst);
        if (compare_time_stamp(sst.time > tx_timer)) continue;
       
        bloom_filter * filter=  sst->filter;
        if (!check_bit(keyword,filter)) continue;
       
        size_t index_block= find_block(sst, keyword);
        block_index * index = at(sst->block_indexs, index_block);
        
        cache_entry c = retrieve_entry_sharded(*snap->cache_ref, index, sst->file_name, sst);
        if (c.buf== NULL ){
            return NULL;
        }
        int k_v_array_index = json_b_search(c.ar, keyword);
        if (k_v_array_index== -1) continue;

        return  c.ar->values[k_v_array_index].entry;    
    }
    return NULL;
}
char* read_record_snap_txn(storage_engine * engine, const char * keyword, snapshot * s, txn * tx){
    Node * entry = NULL;
    for (int i = 0; i < engine->tables->len; i++){
        mem_table * table = at(engine->tables, i);
        entry= search_list(table->skip, keyword);
        if (entry == NULL) continue;
        if (entry->id > tx->id) continue;
    }
    if (entry== NULL) return disk_read_snap(s,keyword, tx->snapshot->time);
    return entry->value.entry; 
}
void execute_no_distr(storage_engine * e, txn * tx){
    txn_op * ops = tx->ops->arr;
    tx->state = TXN_ACTIVE;
    for (int i = 0; i < tx->ops->len; i++){
        union result result;
        switch (ops[i].type){
            case READ:
                result.resp_v = read_record_snap_txn();
                break;
            case WRITE:
                result.error_code = write_record(e, ops[i].key, ops[i].value);
                break;
            default:
                break;
        }
    }
}
void commit_instance(txn * tx){
    execute_no_distr(tx);
}
