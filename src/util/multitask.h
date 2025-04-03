#ifndef MULTITASK_H
#define MULTITASK_H

#include <stdbool.h>
#include "../ds/list.h"
#include "aco.h"
#include "../ds/circq.h"
#include <pthread.h>
#include <liburing.h>
#include <stdint.h>

enum task_status {
    NOT_START,/*task is in pool and has not begun execution*/
    RUNNING, /*standard running*/
    IO_WAIT, /*task is waiting for an IO request and should not be polled*/
    COMPUTE_YIELD,/*task yielded to reduce starvation, safe to re pool in a round robin system*/
    DONE /*finished, recycle task*/
};

enum task_type {
    MISC_COMPUTE,
    WAL_WRITE,
    DB_READ,
    DB_WRITE,
    COMPACT_MERGE,
    POOL_WAIT,
    TERMINATE_WAIT,
    TERMINATE_NO_WAIT
};
enum task_privilege{
    STEAL,
    ATOMIC_SYNC,
    MUTEX_SYNC,
    PINNED
};

/* Forward declaration of task_func type */
typedef struct task task_t;
typedef void (*task_func)(task_t *task);
DEFINE_CIRCULAR_QUEUE(task_t *, task_q);
typedef struct task {
    db_schedule* scheduler;
    enum task_status stat;
    enum task_type type;
    enum task_privilege priv;
    void * arg;
    void *ret;
    task_func real_task;
    aco_t * thread;
    task_q * active;
} task_t;
/* Define circular queue after the struct task is fully defined */
 /*why do we do this? libaco aco requires heap allocated args sadly*/

typedef struct db_schedule {
    task_q * active;
    task_q * staging;
    task_q * pool;
    aco_share_stack_t * shared;
    aco_t * main;
    void * io_manager;
    arena * a;
    int ongoing_tasks;
} db_schedule;
task_t* create_task(aco_t* main, void * allocator, int id, enum task_type type, int stack_s, void* stack, aco_cofuncp_t func, void* args);
void add_task(task_func func, enum task_type type, void * arg ,db_schedule* scheduler);
void init_scheduler(db_schedule *scheduler, aco_t *main_co, int pool_size,int stack_size, void * io_manager);
void aco_schedule(db_schedule * scheduler, struct io_uring * ring );

void cleanup_scheduler(db_schedule *scheduler);

#endif 
