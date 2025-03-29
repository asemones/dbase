
#include "../ds/list.h"
#include "aco.h"
#include "../ds/circq.h"
#include <pthread.h>
#include "io.h"
#include <liburing.h>
DEFINE_CIRCULAR_QUEUE(task_t , task_q);
enum task_status {
    NOT_START,/*task is in pool and has not begun execution*/
    RUNNING, /*standard running*/
    IO_WAIT, /*task is waiting for an IO request and should not be polled*/
    COMPUTE_YIELD,/*task yielded to reduce starvation, safe to re pool in a round robin system*/
    DONE /*finished, recycle task*/
};

enum task_type {
    WAL_WRITE,
    DB_READ,
    DB_WRITE,
    COMPACT_MERGE,
    POOL_WAIT
};

typedef void (*task_func)(task_t *task);
typedef struct task {
    int id;
    int stack_s;
    enum task_status stat;
    enum task_type type;
    void * arg;
    void *ret;
    task_func real_task;
    aco_t * thread;
    int active_io_requests;
} task_t;

typedef struct io_arg{
    task_t task;
    task_q * active;
    pthread_cond_t * item_sig;
}io_arg;
typedef struct db_schedule {
    task_q * active;
    task_q * staging;
    task_q * pool;

    pthread_cond_t new_task_sig;
    pthread_mutex_t new_task_lock;

} db_schedule;
task_t  create_task(aco_t * main, int id, enum task_type type, int stack_s, void * stack, aco_cofuncp_t func, void * args);
void add_task(task_func  func, enum task_type type, db_schedule * scheduler);
void init_scheduler(db_schedule *scheduler, aco_t *main_co, int pool_size);
void cleanup_scheduler(db_schedule *scheduler);
