#ifndef MULTITASK_H
#define MULTITASK_H

#include <stdbool.h>
#include "../ds/list.h"
#include "aco.h"
#include "../ds/circq.h"
#include <pthread.h>
#include <liburing.h>
#include <stdint.h>
#include "../ds/arena.h"
#include "io.h"
#include "../ds/sync_queue.h"
#include "../ds/rpc_table.h"
#include "../ds/bitmap.h"
#define MAX_RUNTIMES_DEFAULT 512
#define SEED_ID 0

#define yield_compute \
    task_t * task = aco_get_arg();\
    task->stat = COMPUTE_YIELD;\
    aco_yield();

#define await_complete \
    task_t * task = aco_get_arg();\
    task->stat =INTERRUPT_YIELD;\
    aco_yield(); 
#define await_rpc\
    task_t * task = aco_get_arg();\
    task->stat = RPC_YIELD;\
    task->running_rpcs ++;\
    aco_yield();
#define cascade_return_int(val)\
    future_t fut;\
    fut.return_code = val;\
    return fut

#define cascade_return_ptr(val)\
    future_t fut;\
    fut.value = val;\
    return fut

#define await_local_id(table, id)                        \
    do {                                                     \
        while (!rpc_is_complete(table, id)) {     \
                                                             \
            aco_yield();                                     \
        }                                                    \
    } while(0)
enum task_status {
    NOT_START,/*task is in pool and has not begun execution*/
    RUNNING, /*standard running*/
    IO_WAIT, /*task is waiting for an IO request and should not be polled*/
    COMPUTE_YIELD,
    RPC_YIELD,/*task yielded to reduce starvation, safe to re pool in a round robin system*/
    DONE /*finished, recycle task*/
};
enum message_type{
    ADD_TASK,
    FUNCTION_RETURN_VAL,
    FUNCTION_RETURN_VAL_NOWAIT
};
enum task_type {
    MISC_COMPUTE,
    WAL_WRITE,
    DB_READ,
    DB_WRITE,
    COMPACT_MERGE,
    POOL_WAIT,
    TERMINATE_WAIT,
    TERMINATE_NO_WAIT,
    RPC,
    RPC_NO_WAIT,
    EXTERNAL_RPC
};
/* Forward declaration of task_func type */
typedef struct task task_t;
typedef void (* func)(void * arg);
DEFINE_CIRCULAR_QUEUE(task_t *, task_q);
typedef future_t (*task_func)(void * arg);
typedef struct msg_t {
    enum message_type type;
    uint16_t sender_shard_id;
    uint16_t reciver_shard_id;
    task_func on_recieve;
    future_t payload;
    uint64_t size;
    task_t *sender;
    task_t *recive;
} msg_t;
DEFINE_SYNC_CIRCULAR_QUEUE(future_t, ret_q);
DEFINE_SYNC_CIRCULAR_QUEUE(msg_t , msg_q);
typedef struct shard_link_t{
   bitmap * queues_with_items;
   msg_q * inbox;
   msg_q * outbox;
}shard_link_t;
typedef struct db_schedule {
    task_q * active;
    task_q * pool;
    int ongoing_tasks;
    rpc_table_t * table;
    uint16_t linked;
    bitmap  my_active_msgs;
    shard_link_t links[MAX_RUNTIMES_DEFAULT];
    aco_share_stack_t * shared;
    arena * a;
    uint32_t id;
    aco_t* main_co;
} db_schedule;
typedef struct cascade_runtime_t{
    pthread_t thread;
    aco_t* main_co;
    int cpu; /*id*/
    db_schedule scheduler;
}cascade_runtime_t;
typedef struct io_config{
    int max_concurrent_io;
    int big_buf_s;
    int huge_buf_s;
    int num_big_buf;
    int num_huge_buf;
    int small_buff_s;
    int coroutine_stack_size;
}io_config;
/*why do we have p_id and parent? because parent is a memory address that should not be accessed
accross shards and especially accross frameworks. it could potentially be a read from a different numa node*/
typedef struct task {
    uint8_t stat;           
    uint8_t type;           
    uint16_t running_rpcs;  
    aco_t *thread;         
    task_func real_task; 
    cascade_runtime_t *scheduler; 
    void *arg;            
    uint64_t id;           
    uint64_t p_id;         
    task_t *parent;        
    future_t ret;    
} task_t;
typedef struct partition_t{
    char * min;
    char * max;
}partition_t;
typedef struct cascade_framework_t{
    cascade_runtime_t *runtimes[MAX_RUNTIMES_DEFAULT];
    int num_shards;
    partition_t * ranges;
} cascade_framework_t;
static int  l = sizeof(task_t);
static int  p = sizeof(db_schedule);
// Using the cascade_runtime_t definition from above
/* Define circular queue after the struct task is fully defined */
 /*why do we do this? libaco aco requires heap allocated args sadly*/
task_t* create_task(aco_t* main, void * allocator, int id, enum task_type type, int stack_s, void* stack, aco_cofuncp_t func, void* args);
task_t * add_task(uint64_t id, task_func func, enum task_type type, void * arg ,cascade_runtime_t* scheduler, task_t * parent);
void init_scheduler(db_schedule *scheduler, aco_t *main_co, int pool_size,int stack_size, void * io_manager);
void cascade_schedule(cascade_runtime_t* rt, struct io_uring* ring);
cascade_runtime_t * cascade_spawn_runtime_config(task_func  seed, void * arg, io_config config);
cascade_runtime_t * cascade_spawn_runtime_default(task_func  seed, void * arg);
/*add a function call as a user thread not apart of a cascade rt*/
uint64_t cascade_sub_intern_nowait(task_func func, void* arg);
/*add a function call as the same thread apart of the same cacscade, with a wait*/
future_t cascade_sub_intern_wait(task_func func, void* arg);
/*add a series as the same msg_q * inbox[MAX_RUNTIMES_DEFAULT];
    msg_q * outbox [MAX_RUNTIMES_DEFAULT];thread apart of the same cacscade, without a wait */
uint64_t cascade_submit_external(cascade_runtime_t * rt, task_func function, void * args);
void cleanup_scheduler(db_schedule *scheduler);
future_t get_return_val(uint64_t id);
void poll_rpc(uint64_t *rpc_ids, int num);
void poll_rpc_external(cascade_runtime_t * frame, uint64_t *rpc_ids, int num);\
cascade_framework_t * create_framework();
void add_link_to_framework(cascade_framework_t * framework, cascade_runtime_t * new_runtime);
uint64_t find_node_hash(cascade_framework_t * frame, const char * req_key, int size,  int me);
uint64_t find_node_hash_internal(cascade_framework_t * frame, const char * req_key, int size);
uint64_t cascade_rpc_wait(cascade_runtime_t * targ, task_func func, void * arg);
uint64_t cascade_rpc_nowait(cascade_runtime_t * targ, task_func func, void * arg);
#endif 