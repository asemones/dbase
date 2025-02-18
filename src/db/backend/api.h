#include <stdio.h>
#include "lsm.h"
#include "iter.h"




/*change read_record and write_record to work properly*/
char * get(storage_engine * engine, char * key){
    snapshot *temp_snap = engine->meta->current_tx_copy;
    ref_snapshot(engine->meta->current_tx_copy);
    char * ret = read_record_snap(engine, key, temp_snap);
    deref_with_free(temp_snap);
    return ret;
}
int set(storage_engine * engine, char * key, char * value){
    keyword_entry entry = {key, value};
    return write_record(engine, &entry, strlen(key) + strlen(value));
}
int del(storage_engine * engine, char * key){
    return set(engine, key, TOMB_STONE);
}
char ** scan(storage_engine * engine, char * start, char * end){
    snapshot *temp_snap = engine->meta->current_tx_copy;
    ref_snapshot(engine->meta->current_tx_copy);
    aseDB_iter * iter = create_aseDB_iter();
    init_aseDB_iter(&iter, engine);
    //implement scan_records
    //return scan_records(engine, start, end);
    
    deref_with_free(temp_snap);
}