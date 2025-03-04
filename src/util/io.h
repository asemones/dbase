#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include "alloc_util.h"
#ifndef IO_H
#define IO_H
//typedef void (*aioCallback)(void * fd, void *arg);
//typedef void (*aioCallback)(void * fd, void **arg);
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
//int add(eventLoop* loop, int fd, short filter, aioCallback* callback, void* userdata);
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

/*eventLoop* createLoop(size_t size){
    eventLoop * loop =(eventLoop*)wrapper_alloc((sizeof (*loop)), NULL,NULL);
    loop->events = (struct kevent *)wrapper_alloc((sizeof( struct kevent)), NULL,NULL* size);
    loop->maxEvents = size;
    loop->kq = kqueue();
    loop->numEvent =0;
    loop->running = true;
    pthread_mutex_init(&loop->queueLock,NULL);
    pthread_mutex_init(&loop->addLock,NULL);
    return loop;
}

int add(eventLoop* loop, int fd, short filter, aioCallback* callback, void* userdata){
    if (loop->numEvent >= loop->maxEvents){
        return -1;
    }
    struct kevent *ev = &loop->events[loop->numEvent++];
    pthread_mutex_lock(&loop->addLock);
    EV_SET(ev, fd, filter, EV_ADD | EV_ENABLE, 0, 0, userdata);
    pthread_mutex_unlock(&loop->addLock);
    return kevent(loop->kq, ev, 1, NULL, 0, NULL);
}
void runLoop(eventLoop * loop){
    while(atomic_load(&loop->running)){
         pthread_mutex_lock(&loop->queueLock);
         int n = kevent(loop->kq, NULL, 0, loop->events, loop->maxEvents, NULL);
         pthread_mutex_unlock(&loop->queueLock);
         for (int i = 0;  i < n; i ++){
             struct kevent *ev = &loop->events[i];
             aioCallback cb = (aioCallback*)ev->udata;
             cb(ev->ident, ev->udata);
         }
    }
}
void destroyLoop(eventLoop * loop){
    loop->running  = false;
}
*/
#endif