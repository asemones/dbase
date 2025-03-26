#include "api.h"

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
