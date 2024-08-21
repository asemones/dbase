#include <stdio.h>
#include "lsm.h"




/*change read_record and write_record to work properly*/
char * get(storage_engine * engine, char * key){
    byte_buffer* buf = (byte_buffer*)request_struct(engine->read_pool);
    byte_buffer * key_v_store = (byte_buffer*)request_struct(engine->read_pool);
    return read_record(engine, key, buf, key_v_store);
}
int set(storage_engine * engine, char * key, char * value){
    keyword_entry entry = {key, value};
    return write_record(engine, &entry, strlen(key) + strlen(value));
}
int del(storage_engine * engine, char * key){
    return delete_record(engine, key);
}
char ** scan(storage_engine * engine, char * start, char * end){
    //implement scan_records
    //return scan_records(engine, start, end);
}