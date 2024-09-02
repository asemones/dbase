#include <stdlib.h>
#include <stdio.h>
#include<string.h>
#include <stdint.h>
#include "../../ds/byte_buffer.h"
#include "option.h"
#include <stdint.h>
#define ERR -1000
#define INT 0
#define FLOAT 1 
#define STR 2



typedef struct db_unit{
    u_int16_t len;
    void * entry;
}db_unit;

#pragma once
/* for comparison functions*/
/*NEW STORAGE FORMAT: 
[num entries ... key len A, value len A,....... Key A Value A ]
*/
enum source {
    memtable,
    lvl_0,
    lvl_1_7
};
typedef struct merge_data {
    db_unit * key;
    db_unit * value;
    char index;
    enum source src;
}merge_data;
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
int byte_wise_comp(db_unit one, db_unit two) {
    int len = max(one.len, two.len);
    char *typecasted_one = one.entry;
    char *typecasted_two = two.entry;
    for (int i = 0; i < len; i++) {
        if (typecasted_one[i] < typecasted_two[i]) return -1;
        else if (typecasted_one[i] > typecasted_two[i]) return 1;
    }
    if (one.len == two.len) return 0;
    return (one.len > two.len) ? 1 : -1;
}

int comp_int(db_unit one, db_unit two) {
    int a = *(int*)one.entry;
    int b = *(int*)two.entry;
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

int comp_float(db_unit one, db_unit two) {
    float a = *(float*)one.entry;
    float b = *(float*)two.entry;
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

int master_comparison(db_unit *one, db_unit *two, int max, int dt) {
    if (one == NULL || two == NULL || two->entry == NULL || one->entry == NULL){
        return ERR;
    }
    switch (dt) {
        case INT:
            return comp_int(*one,* two);
        case FLOAT:
            return comp_float(*one, *two);
        case STR:
            return (max <= 0) ? strcmp((char*)one->entry, (char*)two->entry)
            : strncmp((char*)one->entry, (char*)two->entry, max);
        default:
            return byte_wise_comp(*one, *two);
    }
}




