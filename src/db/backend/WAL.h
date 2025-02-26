#include <stdio.h>
#include <stdlib.h>
#include "option.h"
#include "../../ds/byte_buffer.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "../../util/io.h"
#include "key-value.h"
#include "../../util/error.h"
#include "../../ds/list.h"



#define FS_B_SIZE 512
#define MAX_WAL_FN_LEN 20
#pragma once

typedef struct WAL {
    int fd;
    FILE * wal_meta;
    list * fn;
    byte_buffer * fn_buffer;
    byte_buffer * wal_buffer;
    size_t len;
    size_t curr_fd_bytes;
}WAL;

void seralize_wal(WAL *w, byte_buffer *b);
void deseralize_wal(WAL *w, byte_buffer *b);
char * add_new_wal_file(WAL *w);
WAL* init_WAL(byte_buffer *b);
int write_WAL(WAL *w, db_unit key, db_unit value);
void kill_WAL(WAL *w, byte_buffer *temp);
