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
    //size_t archived_index;
}WAL;
/*finish the WAL. split into multiple files that get archivewd after some time. 
*/
/*the WAL is only valid/matters for entries inside a memtable that haven't been flushed yet. */
/*not writing file names to the fn buffer*/
void seralize_wal(WAL * w, byte_buffer * b){
    write_buffer(b, (char*)&w->len, sizeof(w->len));
   // write_buffer(b, (char*)&w->archived_index, sizeof(w->archived_index));
    write_buffer(b, (char*)&w->fn->len, sizeof(w->fn->len));
    write_buffer(b,(char*)&w->curr_fd_bytes, sizeof(w->curr_fd_bytes));
    write_buffer(b, (char*)&w->fn_buffer->curr_bytes, sizeof(w->fn_buffer->curr_bytes));
    byte_buffer_transfer(w->fn_buffer, b, w->fn_buffer->curr_bytes);
}
void deseralize_wal(WAL * w, byte_buffer * b){
    read_buffer(b, (char*)&w->len, sizeof(w->len));
   // read_buffer(b, (char*)&w->archived_index, sizeof(w->archived_index));
    int fn_list_len = 0;
    read_buffer(b, (char*)&fn_list_len, sizeof(fn_list_len));
    read_buffer(b,(char*)&w->curr_fd_bytes, sizeof(w->curr_fd_bytes));
    size_t temp_curr_bytes=  0;
    read_buffer(b,(char*)&temp_curr_bytes, sizeof(temp_curr_bytes));
    byte_buffer_transfer(b, w->fn_buffer, MAX_WAL_FN_LEN * fn_list_len);
    size_t s = 0;
    for (int i = 0; i < fn_list_len; i ++){
        char * element = buf_ind(w->fn_buffer,s);
        if (element  == NULL) break;
        s+=MAX_WAL_FN_LEN;
        insert(w->fn, &element);
    }

}
char * add_new_wal_file(WAL * w){

    char * file_name = buf_ind(w->fn_buffer, (MAX_WAL_FN_LEN* w->fn->len));
    sprintf(file_name, "WAL_%d.bin", w->fn->len);
    w->fn_buffer->curr_bytes +=MAX_WAL_FN_LEN;
    insert(w->fn, &file_name);
    return file_name;
}
WAL* init_WAL(byte_buffer * b){
    WAL * w = malloc(sizeof(WAL));
    if (w == NULL ) return NULL;
    w->wal_buffer = mem_aligned_buffer(GLOB_OPTS.WAL_BUFFERING_SIZE, FS_B_SIZE);
    if (w->wal_buffer == NULL) return NULL;
    w->fn_buffer = create_buffer(MAX_WAL_FN_LEN * (GLOB_OPTS.MAX_WAL_FILES + 1));
    w->fn = List(0, sizeof(char*), false);
    char * file_name = NULL;
    w->curr_fd_bytes = 0;
    w->len = 0;
    if ((w->wal_meta = fopen(GLOB_OPTS.WAL_M_F_N, "r+")) == NULL){
        w->wal_meta = fopen(GLOB_OPTS.WAL_M_F_N, "w+");
        file_name = add_new_wal_file(w);
    }
    else{
        b->curr_bytes +=  fread(b->buffy,1, get_file_size(w->wal_meta), w->wal_meta);
        deseralize_wal(w,b);
        char ** file_n_ptr = get_last(w->fn);
        if (file_n_ptr!= NULL) file_name = *file_n_ptr;
        /*double pointer this*/
    }
    w->fd = open(file_name, O_CREAT | O_WRONLY| O_APPEND| __O_DIRECT,  S_IRUSR  | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (w->fd == -1){
        perror("open failed");
        exit(EXIT_FAILURE);
    }
    return w;
}
int write_WAL( WAL * w, db_unit  key, db_unit  value){
    int ret= 0;
    if (w->curr_fd_bytes  + key.len + value.len + 4 > GLOB_OPTS.WAL_SIZE){
        int diff = (FS_B_SIZE - (w->wal_buffer->curr_bytes % FS_B_SIZE)) % FS_B_SIZE;
        w->wal_buffer->curr_bytes += diff;
        write(w->fd, w->wal_buffer->buffy, w->wal_buffer->curr_bytes);
        reset_buffer(w->wal_buffer);
        w->curr_fd_bytes = 0;
        close(w->fd);
        char * file_name = add_new_wal_file(w);
        w->fd =  open(file_name, O_CREAT |  O_WRONLY| O_APPEND| __O_DIRECT,  S_IRUSR  | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    }
    if (w->wal_buffer->curr_bytes + key.len + value.len + 4 >= GLOB_OPTS.WAL_BUFFERING_SIZE){
        int size = write(w->fd, w->wal_buffer->buffy, GLOB_OPTS.WAL_BUFFERING_SIZE);
        if (size != GLOB_OPTS.WAL_BUFFERING_SIZE) {
            perror("write failed");
            return FAILED_TRANSCATION;/*proper error handleing in the future, since this signifies the transacation failed*/
        }
        reset_buffer(w->wal_buffer);
    }
    if (w->fn->len >=GLOB_OPTS.MAX_WAL_FILES){
        char * file_to_remove=  at(w->fn, 0);
        remove(file_to_remove);
        remove_at(w->fn, 0);
        memmove(buf_ind(w->fn_buffer, 0), buf_ind(w->fn_buffer, MAX_WAL_FN_LEN)
        , w->fn_buffer->curr_bytes- MAX_WAL_FN_LEN);
        w->fn_buffer->curr_bytes -= MAX_WAL_FN_LEN;
    }
    ret += write_db_unit(w->wal_buffer, key);
    ret += write_db_unit(w->wal_buffer, value);
    w->len++;
    w->curr_fd_bytes += ret;
    return 0;
}
void kill_WAL(WAL * w, byte_buffer * temp){
    int diff = (FS_B_SIZE - (w->wal_buffer->curr_bytes % FS_B_SIZE)) % FS_B_SIZE;
    w->wal_buffer->curr_bytes += diff;
    write(w->fd, w->wal_buffer->buffy, w->wal_buffer->curr_bytes);
    seralize_wal(w, temp);
    fwrite(temp->buffy, 1, temp->curr_bytes, w->wal_meta);
    fflush(w->wal_meta);
    close(w->fd);
    free_buffer(w->wal_buffer);
    free_buffer(w->fn_buffer);
    free_list(w->fn, NULL);
    free(w);

}





