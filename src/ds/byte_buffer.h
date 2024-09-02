#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "../util/alloc_util.h"
#ifndef BYTE_BUFFER_H
#define BYTE_BUFFER_H
typedef struct byte_buffer {
    char * buffy;
    void * utility_ptr;
    size_t max_bytes;
    size_t curr_bytes;
    size_t read_pointer;
} byte_buffer;

byte_buffer * create_buffer(size_t size){
    byte_buffer * buffer = (byte_buffer*)wrapper_alloc((sizeof(byte_buffer)), NULL,NULL);
    buffer->buffy = (char*)calloc(size, 1);
    buffer->max_bytes = size;
    buffer->curr_bytes = 0;
    buffer->read_pointer = 0;
    buffer->utility_ptr = NULL;
    return buffer;
}

int write_buffer(byte_buffer * buffer, char * data, size_t size){
    if (buffer->curr_bytes + size >= buffer->max_bytes){
        return -1;
    }
    memcpy(&buffer->buffy[buffer->curr_bytes], data, size);
    buffer->curr_bytes += size;
    return 0;
}
int read_buffer(byte_buffer * buffer, void* dest, size_t len){
    if (buffer->read_pointer + len >= buffer->max_bytes){
        return -1;
    }
    memcpy(dest, &buffer->buffy[buffer->read_pointer], len);
    buffer->read_pointer += len;
    return len;
}
int dump_buffy(byte_buffer * buffer, FILE * file){
    int size = fwrite(buffer->buffy, 1, buffer->curr_bytes, file);
    fflush(file);
    if (size == buffer->curr_bytes) return 0;
    
    return -1;
}
int write_over_buffer(byte_buffer * buffer, char * data, size_t size, size_t *start){
    if (*start+ size >= buffer->max_bytes){
        return -1;
    }
     memcpy(&buffer->buffy[buffer->curr_bytes], data, size);
    *start += size;
     return size;
}
char * go_nearest_v(byte_buffer * buffer, char c) {
    char *result = memchr(&buffer->buffy[buffer->read_pointer], c, buffer->curr_bytes - buffer->read_pointer);
    buffer->read_pointer = (result!= NULL )?  result - buffer->buffy:  buffer->curr_bytes;
    return result;
}

char * go_nearest_backwards(byte_buffer * buffer, char c){
    for (size_t i = buffer->read_pointer; i > 0; i--) {
        if (buffer->buffy[i - 1] == c) {
            buffer->read_pointer = i - 1;
            return &buffer->buffy[i - 1];
        }
    }
    buffer->read_pointer = 0;
    return NULL;
}

char * get_curr(byte_buffer * buffer){
    if (buffer->read_pointer >= buffer->curr_bytes) return NULL;
    char * temp = &buffer->buffy[buffer->read_pointer];
    return temp;
}
char * get_next(byte_buffer * buffer){
    buffer->read_pointer++;
    if (buffer->read_pointer >= buffer->curr_bytes) return NULL;
    char * temp = &buffer->buffy[buffer->read_pointer];
    return temp;
}
int byte_buffer_transfer(byte_buffer * read_me, byte_buffer* write_me, size_t num){
    int l=read_buffer(read_me, &write_me->buffy[write_me->curr_bytes],num);
    write_me->curr_bytes += num;
    return l;
}
void reset_buffer(void * v_buffer){
    byte_buffer * buffer = (byte_buffer*)v_buffer;
    //memset(buffer->buffy, 0, buffer->curr_bytes);
    buffer->curr_bytes = 0;
    buffer->read_pointer = 0;
}
void free_buffer(void* v_buffer){
    byte_buffer * buffer = (byte_buffer*)v_buffer;
    if (v_buffer == NULL) return;
    free(buffer->buffy);
    free(buffer);
    buffer=  NULL;
}
#endif

