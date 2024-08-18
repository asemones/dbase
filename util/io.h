#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include "alloc_util.h"
#ifndef IO_H
#define IO_H
//typedef void (*aioCallback)(void * fd, void *arg);
//typedef void (*aioCallback)(void * fd, void **arg);

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


int write_file(char * buf, char * file, char * mode, size_t bytes, size_t element);
int read_file( char * buf, char * file, char * mode, size_t bytes, size_t element);

int read_file( char * buf, char * file, char * mode, size_t bytes, size_t element)
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

int write_file(char * buf, char * file, char * mode, size_t bytes, size_t element){
    FILE *File= fopen(file, mode);
    if (File == NULL){
        perror("fopen");
        return -1;
    }
    int written_size = fwrite(buf, bytes, element ,File);
    fclose(File);
    return written_size;
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