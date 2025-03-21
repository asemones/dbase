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
#pragma once

void task_func_1(void * arg){
    task_t * task = arg;
     int sum = 0 ;
    for (int i = 0; i < 4000; i++){
        for (int j = 0; j < 3000; j++){
            sum -= j * 2;
        }
        printf("func 1 yield %d", sum);
        aco_yield();
    }
}
void task_func_2(void * arg){
    task_t * task = arg;
    int sum = 0 ;
    for (int i = 0; i < 4000; i++){
        aco_yield();
        for (int j = 0; j < 3000; j++){
            sum += j * 2;
        }
        printf("func 2 yield %d", sum);
        aco_yield();
    }
    
}
void task_func_3(void * arg){
    task_t * task = arg;
    int sum = 0 ;
    aco_yield();
    for (int i = 0; i < 4000; i++){
        for (int j = 0; j < 3000; j++){
            sum -= j * 2;
        }
        sum += sum * 2;
        printf("func 3 yield %d", sum);
        aco_yield();
    }
}
void thread_func(void * arg){
      db_schedule * scheduler = arg;
      aco_t * main = aco_create(NULL, NULL, 0, NULL, NULL);
      init_scheduler(scheduler,main, 400);

}
void test_tasks_no_io(){
   pthread_t thread;
   db_schedule scheduler;
   pthread_create(&thread, NULL, &thread_func,&scheduler);
   pthread_join(&thread, NULL);

}