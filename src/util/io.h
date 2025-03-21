#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include "alloc_util.h"
#include <liburing.h>
#include <fcntl.h>
#include "../ds/list.h"
#include "../ds/byte_buffer.h"
#include "../ds/structure_pool.h"
#include "../ds/arena.h"
#include <sys/resource.h>

#ifndef IO_H
#define IO_H

enum operation{
    READ,
    WRITE,
    OPEN,
    CLOSE,
};
typedef void (*aio_callback)(void *arg);
/*since we want no memcpys, EVERY SINGLE buffer MUST be owned by the manager*/

struct io_manager{
    struct_pool * sst_table_buffers;
    struct_pool * mem_table_buffers;
    struct_pool * four_kb;
    struct_pool * io_requests; // we keep these tasks heap allocated to mimimize the stack size
    struct_pool * open_close_req;
    struct io_uring ring;
    arena a;
    int num_in_queue;
    struct timeval val;
};
union descriptor{
    uint64_t fd;
    char * fn;
}; 
struct io_request{
    union descriptor desc; 
    int priority;
    off_t offset;
    size_t len;
    aio_callback callback;
    void * callback_arg;
    enum operation op ;
    byte_buffer* buf;
    int response_code;
}; 
struct open_close_req{
    union descriptor desc; 
    enum operation op;
    char * f_n;
    int flags;
    mode_t perm;
};
//typedef void (*aio_callback)(void * fd, void *arg);
//typedef void (*aio_callback)(void * fd, void **arg);
/*
typedef struct event_loop {
    int kq;
    size_t max_events;
    struct kevent *events;
    int num_event;
    atomic_bool running;
    pthread_mutex_t queue_lock;
    pthread_mutex_t add_lock;
}event_loop;
event_loop* create_loop(size_t size);
//int add(eventLoop* loop, int fd, short filter, aio_callback* callback, void* userdata);
void run_loop(event_loop * loop);
*/
/**
 * @brief Wrapper function for fwrite
 * @param ptr Pointer to the data to be written
 * @param size Size of each element to be written
 * @param n Number of elements to be written
 * @param file Pointer to the FILE object
 * @return Number of elements successfully written
 */
static inline int write_wrapper(void * ptr, size_t size, size_t n, FILE * file ){
    return fwrite(ptr, size, n, file);
}
/**
 * @brief Wrapper function for fread
 * @param ptr Pointer to the buffer where read data will be stored
 * @param size Size of each element to be read
 * @param n Number of elements to be read
 * @param file Pointer to the FILE object
 * @return Number of elements successfully read
 */
static inline int read_wrapper(void  * ptr, size_t size, size_t n, FILE * file ){
    return fread(ptr, size, n, file);
}
/**
 * @brief Writes data to a file
 * @param buf Pointer to the buffer containing data to be written
 * @param file Path to the file
 * @param mode File opening mode
 * @param bytes Size of each element to be written
 * @param element Number of elements to be written
 * @return Number of elements successfully written or -1 on error
 */
static inline int write_file(char * buf, char * file, char * mode, size_t bytes, size_t element);

/**
 * @brief Reads data from a file
 * @param buf Pointer to the buffer where read data will be stored
 * @param file Path to the file
 * @param mode File opening mode
 * @param bytes Size of each element to be read
 * @param element Number of elements to be read (if 0, reads entire file)
 * @return Number of elements successfully read or -1 on error
 */
static inline int read_file( char * buf, char * file, char * mode, size_t bytes, size_t element);

static inline int read_file( char * buf, char * file, char * mode, size_t bytes, size_t element)
{
    FILE *File= fopen(file, mode);
    if (File == NULL){
        perror("fopen");
        return -1;
    }
    fseek (File, 0, SEEK_END);
    if (element == 0){
        element= ftell (File);
    }
    fseek (File, 0, SEEK_SET);
    int read_size = fread(buf, bytes, element ,File);
    fclose(File);
    return read_size;
}

static inline int write_file(char * buf, char * file, char * mode, size_t bytes, size_t element){
    FILE *File= fopen(file, mode);
    if (File == NULL){
        perror("fopen");
        return -1;
    }
    int written_size = fwrite(buf, bytes, element ,File);
    fclose(File);
    return written_size;
}
/**
 * @brief Gets the size of a file
 * @param file Pointer to the FILE object
 * @return Size of the file in bytes
 */
static inline long get_file_size(FILE *file) {
    long size;
    
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    return size;
}
void init_io_manager(struct io_manager * manage, int num_4kb, int num_sst_tble, int num_memtable, int sst_tbl_s, int mem_tbl_s);
int chain_open_op_close(struct io_uring *ring, struct io_request * req);
int add_open_close_requests(struct io_uring *ring, struct open_close_req requests, int seq);
int add_read_write_requests(struct io_uring *ring, struct io_request * requests, int seq);
int process_completions(struct io_uring *ring);

#endif