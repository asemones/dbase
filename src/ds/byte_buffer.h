#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "../util/alloc_util.h"
#include <fcntl.h>
#include <unistd.h>
#include "../util/maths.h"
#ifndef BYTE_BUFFER_H
#define BYTE_BUFFER_H
typedef struct byte_buffer {
    char * buffy;
    void * utility_ptr;
    size_t max_bytes;
    size_t curr_bytes;
    size_t read_pointer;
} byte_buffer;

byte_buffer * create_buffer(size_t size);
byte_buffer * mem_aligned_buffer(size_t size, size_t alligment);
int buffer_resize(byte_buffer * b, size_t min_size);
int write_buffer(byte_buffer * buffer, void * data, size_t size);
int read_buffer(byte_buffer * buffer, void * dest, size_t len);
int dump_buffy(byte_buffer * buffer, FILE * file);
char * buf_ind(byte_buffer * b, int size);
char * go_nearest_v(byte_buffer * buffer, char c);
char * go_nearest_backwards(byte_buffer * buffer, char c);
char * get_curr(byte_buffer * buffer);
char * get_next(byte_buffer * buffer);
int byte_buffer_transfer(byte_buffer * read_me, byte_buffer * write_me, size_t num);
void reset_buffer(void * v_buffer);
void free_buffer(void * v_buffer);

#endif

