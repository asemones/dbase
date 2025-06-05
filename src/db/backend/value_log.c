#include "value_log.h"

void * get_value(value_log v, db_unit key, db_unit inline_v){
    uint64_t * size = inline_v.entry;
    if (*size < v.medium_thresh){
        
    }
    else{

    }
}
void * v_log_large_read(db_unit inline_v, bool async, byte_buffer * storage){
    uint32_t file_no = *inline_v.entry;
    uint64_t real_size = *(inline_v.entry + sizeof(file_no));
    char fn [16] = BLOB_FN;
    strcat(fn, file_no);
    db_FILE * file = dbio_open(fn,'r');
    set_context_buffer(file, storage);
    int32_t err=  dbio_read(file,0, real_size);
    dbio_close(file);
    if (err < 0) return NULL;
    storage->curr_bytes += err;
    return storage->buffy; 
}
/*is this tacky? yes it is*/
void * format_inline_v(db_large_unit * value, uint64_t counter){
    uint64_t * mem = value;
    mem[0] = (uint32_t)counter;
    mem[1] = value->len;
    return mem;
}
db_unit v_log_large_write(value_log v, db_large_unit * value){
    char fn [16] = BLOB_FN;
    strcat(fn, v.large.blob_counter);
    uint64_t frozen_counter = v.large.blob_counter;
    v.large.blob_counter++;
    db_FILE * file = dbio_open(fn,'r');
    byte_buffer b;
    b.curr_bytes = value->len;
    b.buffy= value.entry;
    set_context_buffer(file, b.buffy);
    uint64_t old_len = value->len;
    db_unit inline_v;
    inline_v.len = 16;
    inline_v.entry = format_inline_v(value,  v.large.blob_counter);
    int32_t err=  dbio_unfixed_write(file,0, old_len);
    dbio_close(file);
    if (err < 0 ){
        inline_v.entry = NULL;
        inline_v.len = 0;
    }
    return inline_v;
}

void write_value_large();
void write_value_med();
void v_log_write(value_log v, db_unit key, db_large_unit value){
    if ( value.len < v.medium_thresh){
        write_value_med();
    }
    else{
        write_value_large();
    }
}