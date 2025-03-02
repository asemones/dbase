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
int write_db_unit(byte_buffer * b, db_unit u);
void read_db_unit(byte_buffer * b, db_unit * u);
size_t load_block_into_into_ds(byte_buffer *stream, void *ds, void (*func)(void *, void *, void *));
int byte_wise_comp(db_unit one, db_unit two);
int comp_int(db_unit one, db_unit two);
int comp_float(db_unit one, db_unit two);
int master_comparison(db_unit *one, db_unit *two, int max, int dt);
