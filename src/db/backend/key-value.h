#include <stdlib.h>
#include <stdio.h>
#include<string.h>
#include <stdint.h>
#include "../../ds/byte_buffer.h"
#include "option.h"
#include <stdint.h>

typedef struct db_unit{
    u_int16_t len;
    void * entry;
}db_unit;

#pragma once



/*NEW STORAGE FORMAT: 
[num entries ... key len A, value len A,....... Key A Value A ]
*/

int write_db_unit(byte_buffer * b, db_unit u){

    write_buffer(b, (char*)&u.len, sizeof(u.len));
    write_buffer(b,(char*)u.entry, u.len );
    return sizeof(u.len) + u.len;
}
size_t load_block_into_into_ds(byte_buffer *stream, void *ds, void (*func)(void *, void *, void *)) {
    size_t num_bytes = 0;  
    u_int16_t num_entries;
    db_unit key, value;
    read_buffer(stream, &num_entries, sizeof(u_int16_t));  
    for (int i = 0; i < num_entries; i++) {
        read_buffer(stream, &key.len, sizeof(u_int16_t));
        key.entry = &stream->buffy[stream->read_pointer];
        stream->read_pointer += key.len;
        read_buffer(stream, &value.len, sizeof(u_int16_t));  
        value.entry = &stream->buffy[stream->read_pointer];
        stream->read_pointer += value.len;
        (*func)(ds, &key, &value);  
        num_bytes += sizeof(u_int16_t) * 2 + key.len + value.len;
    }

    return num_bytes;
}



