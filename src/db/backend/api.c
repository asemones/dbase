#include "api.h"

/*STANDARD API*/
char * get(storage_engine * engine, char * key){
    snapshot *temp_snap = engine->meta->current_tx_copy;
    ref_snapshot(engine->meta->current_tx_copy);
    char * ret = read_record_snap(engine, key, temp_snap);
    deref_with_free(temp_snap);
    return ret;
}
int put(storage_engine * engine, db_unit key, db_unit value){
    return write_record(engine, key, value)
}
int del(storage_engine * engine, db_unit key){
    return write_record(engine, key,TOMB_STONE);
}
list * forward_scan(storage_engine * engine, char * start, char * end, aseDB_iter * provided){
    snapshot *temp_snap = engine->meta->current_tx_copy;
    ref_snapshot(engine->meta->current_tx_copy);
    if (provided == NULL){
        provided = create_aseDB_iter();
        init_aseDB_iter(provided, engine);
    }
    list * iter_return = scan_records(engine, start, end);
    deref_with_free(temp_snap);
    return iter_return;
}
void init_txn(txn * empty,storage_engine * engine, int num){
    empty->id = num;
    empty->snapshot = engine->meta->current_tx_copy;
    empty->state  = TXN_INACTIVE;
    empty->results = List(0, sizeof(union result), false);
    empty->ops = List(0, sizeof(txn_op), false);
}
void add_op(txn * to_add, db_unit *key, db_unit* value, txn_op_type op_t){
    txn_op op;
    op.key = key;
    op.value = value;
    op.type = op_t;
    insert(to_add->ops, &op);
}
void execute_no_distr(txn * tx, storage_engine * e){
    txn_op * ops = tx->ops->arr;
    tx->state = TXN_ACTIVE;
    for (int i = 0; i < tx->ops->len; i++){
        switch (ops[i].type){
            case READ:
                read_record_snap(e,ops[i].key->entry, tx->snapshot);
                break;
            case WRITE:
                write_record
                break;
            default:
                break;
        }
    }
}
void commit_instance(txn * tx){
    execute_no_distr(tx);

}

