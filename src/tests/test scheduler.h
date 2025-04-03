#include "../util/multitask.h"
#include "../util/io.h"
#include "unity/src/unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <assert.h>
#include <errno.h>
#include "../util/aco.h"
#pragma once

void task_func_1(task_t * arg){
    int sum = 0 ;
    for (int i = 0; i < 10; i++){
        for (int j = 0; j < 3000; j++){
            sum -= j * 2;
        }
        fprintf(stdout, "func 1 yield %d", sum);
        aco_yield();
    }
    return;
}
void task_func_2(task_t * arg){
    int sum = 0 ;
    for (int i = 0; i < 10; i++){
        aco_yield();
        for (int j = 0; j < 3000;j++){
            sum += j * 2;
        }
        fprintf(stdout, "func 2 yield %d", sum);
        aco_yield();
    }
    return;
    
}
void task_func_3(task_t * arg){
    int sum = 0 ;
    aco_yield();
    for (int i = 0; i < 4000; i++){
        for (int j = 0; j < 3000; j++){
            sum -= j * 2;
        }
        sum += sum * 2;
        fprintf(stdout, "func 3 yield %d", sum);
        aco_yield();
    }
    return;
}
void  * thread_func(void * arg){
      db_schedule * scheduler = arg;
      aco_t * main = aco_create(NULL, NULL, 0, NULL, NULL);
      init_scheduler(scheduler,main, 400, 4096);
      add_task(task_func_1, MISC_COMPUTE,NULL, scheduler);
      add_task(task_func_2, MISC_COMPUTE,NULL, scheduler);
      add_task(NULL, TERMINATE_WAIT, NULL, scheduler);
      aco_schedule(scheduler, NULL);

      return NULL;
}
void * tasks_no_ret(void * arg){
      db_schedule * scheduler = arg;
      aco_t * main = aco_create(NULL, NULL, 0, NULL, NULL);
      init_scheduler(scheduler,main, 400, 4096);
      add_task(task_func_1, MISC_COMPUTE,NULL, scheduler);
      add_task(task_func_2, MISC_COMPUTE, NULL, scheduler);
      add_task(NULL, TERMINATE_WAIT,NULL, scheduler);
      aco_schedule(scheduler, NULL);

}
typedef struct io_test_arg{
    struct io_manager * io_manager;
    db_schedule * scheduler;
    struct io_request * pinned;
}io_test_arg;
void callback(void * arg){
    task_t * task = arg;
    task->stat = RUNNING;
    task_q_enqueue(task->active, task);
}
void io_task_func(task_t * arg){
    io_test_arg * a=  arg->arg;
    task_t * task = aco_get_arg();
    a->pinned->callback_arg = task;
    a->pinned->callback = callback;
    int old_op = a->pinned->op;
    a->pinned->op = OPEN;
    a->pinned->desc.fn = "test.txt";
    aco_yield();
    add_open_close_requests(&a->io_manager->ring, a->pinned, 0);
    arg->stat = IO_WAIT;
    aco_yield();
    a->pinned->desc.fd = a->pinned->response_code;
    a->pinned->op = old_op;
    arg->stat = IO_WAIT;
    add_read_write_requests(&a->io_manager->ring, a->io_manager, a->pinned,0);
    aco_yield();
    a->pinned->op = CLOSE;
    arg->stat = IO_WAIT;
    add_open_close_requests(&a->io_manager->ring, a->pinned, 0);
    return;
}
/*finish this test func*/
void io_thrd_f(void * arg){
    io_test_arg * a = arg;
    int write_off = 0;
    int read_off = 0;
    const int len = 4096;
    for (int i = 0; i < a->scheduler->pool->capacity - 1; i++){
        io_test_arg * ar=  malloc(sizeof(*ar));
        struct io_request * pinned = request_struct(a->io_manager->io_requests);
        ar->io_manager = a->io_manager;
        ar->scheduler = a->scheduler;
        ar->pinned = pinned;
        pinned->len = len;
        if ( i % 2 == 0){
            pinned->op = WRITE;
            pinned->offset = write_off;
            write_off += len;
            pinned->perms = DEFAULT_PERMS;
            pinned->flags = DEFAULT_WRT_FLAGS;
        }
        else{
            pinned->op = READ;
            pinned->offset = read_off;
            read_off += len;
            pinned->perms = DEFAULT_PERMS;
            pinned->flags = DEFAULT_READ_FLAGS;
        }
        add_task(io_task_func, DB_READ,ar, a->scheduler);
    }
    add_task(NULL, TERMINATE_WAIT,NULL, a->scheduler);
    aco_schedule(a->scheduler, &a->io_manager->ring);
}
void io_prep(struct io_manager * io_manager, int small, int fat){
    const int max_concurrent_ops = 40000;
    init_io_manager(io_manager, small, 0, 0, 0, 0);
    io_manager->io_requests = create_pool(max_concurrent_ops);
    struct io_request * io= malloc(sizeof(struct io_request) * max_concurrent_ops);
    for (int i = 0; i < max_concurrent_ops; i++){
        io[i].buf = request_struct(io_manager->four_kb);
        insert_struct(io_manager->io_requests, &io[i]);
    }

}
void test_tasks_no_io(){
   pthread_t thread;
   db_schedule scheduler;
   pthread_create(&thread, NULL, &thread_func,&scheduler);
   pthread_join(thread, NULL);

}
void test_tasks_io(){
      db_schedule scheduler;
      aco_t * main = aco_create(NULL, NULL, 0, NULL, NULL);
      init_scheduler(&scheduler,main, 40000, 512);
      struct io_manager * io_mana = malloc(sizeof(*io_mana));
      io_prep(io_mana, 40000, 128);
      FILE * open = fopen("test.txt", "wb+");
      
      pthread_t thread;
      io_test_arg a;
      a.io_manager = io_mana;
      a.scheduler= &scheduler;
      pthread_create(&thread, NULL,&io_thrd_f,&a);
      pthread_join(thread, NULL);

}