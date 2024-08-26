#include <stdio.h>
#include <stdlib.h>
#include "option.h"
#include "../../ds/byte_buffer.h"




typedef struct WAL {
    FILE * f;
    byte_buffer * holding;
    size_t len;
}WAL;


void init_WAL(WAL * w){
    if (!(w->f = fopen(GLOB_OPTS.WAL_F_N, "rb+"))){
        w->f = fopen(GLOB_OPTS.WAL_F_N, "wb+");
        w->len  =0;
        fseek(w->f, 8, SEEK_SET);

    }
    else{
        w->len = fread(&w->len, sizeof(size_t), 1, w->f);
        fseek(w->f, 8 + w->len, SEEK_SET);
    }
    w->holding = create_buffer(GLOB_OPTS.WAL_BUFFERING_SIZE*1.1);
    
}
int write_to_WAL(char * record){

}

