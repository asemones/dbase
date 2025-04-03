#include "multitask.h"
#include <stdbool.h>
#define SHARED_S_SIZE 512
/*our user space task scheduler*/

void aco_execute(void);
task_t* create_task(aco_t* main, void * allocator, int id, enum task_type type, int stack_s, void* stack, aco_cofuncp_t func, void* args);

void allocate_new_tasks(db_schedule * scheduler) {
    int id = scheduler->pool->size + scheduler->active->size;
    while(!task_q_is_full(scheduler->pool)){
        task_t* new_task = create_task(scheduler->main, scheduler->a, id, POOL_WAIT, SHARED_S_SIZE, scheduler->shared, aco_execute, NULL);
        new_task->active = scheduler->active;
        new_task->scheduler = scheduler;
        task_q_enqueue(scheduler->pool, new_task);
        id++;
    }
}
void check_return_code(task_t * task){

}
inline void change_task_priv(task_t * task, enum task_privilege priv){
    task->priv = priv;
}
task_t* create_task(aco_t* main, void * allocator, int id, enum task_type type, int stack_s, void* stack, aco_cofuncp_t func, void* args) {
    task_t * task;
    if (allocator != NULL){
        task = arena_alloc(allocator, sizeof(*task));
    }
    else{
        task  = malloc(sizeof(*task));
    }
    if (!task) {
        // Handle allocation failure
        return NULL;
    }

    task->type = type;
    task->stat = NOT_START;
    task->ret = NULL;
    task->real_task = NULL;
    task->priv = PINNED;
    
    // Now we can pass the heap-allocated task pointer directly as the argument
    if (stack != NULL) {
        task->thread = aco_create(main, stack, stack_s, func, task);
    } else {
        task->thread = aco_create(main, aco_share_stack_new(stack_s), stack_s, func, task);
    }
    
    return task;
}

void aco_execute(void) {
    task_t* task = (task_t*)aco_get_arg(); // Use aco_get_arg() to get the passed task pointer
    
    task->stat = NOT_START;
    aco_yield();
    
    while(1) {
        while(!task->real_task) {
            aco_yield();
        }
        
        task->stat = RUNNING;
        task->real_task(task);
        task->real_task = NULL;
        task->stat = DONE;
        task->type = POOL_WAIT;
        task->real_task = NULL;
    }
}
void add_task(task_func func, enum task_type type, void * arg ,db_schedule* scheduler) {
    task_t* task;
    
    if (task_q_is_empty(scheduler->pool)) {
        allocate_new_tasks(scheduler);
    }
    
    task_q_dequeue(scheduler->pool, &task);
    task->real_task = func;
    task->type = type;
    task->arg = arg;
    task_q_enqueue(scheduler->active, task);
    scheduler->ongoing_tasks ++;
}    
void freeze_scheduler(db_schedule* scheduler, int finish_tasks, struct io_uring  *ring){
    const int TERM_TASK = 1;
    while(scheduler->ongoing_tasks - TERM_TASK> 0){
        task_t* task;
        if (task_q_is_empty(scheduler->active)){
            process_completions(ring);
            continue;
        }
        task_q_dequeue(scheduler->active, &task);
        if (task->real_task && task->stat != IO_WAIT) {
            aco_resume(task->thread);
        }
        /*if the task enters io wait, drop from queue. its state is saved in an io_arg struct
        connected to the db_FILE*/
        if (task->stat == IO_WAIT) {
            continue;
        }
        
        if (task->stat == DONE) {
            check_return_code(task);
            task_q_enqueue(scheduler->pool, task);
            scheduler->ongoing_tasks --;
        } else {
            task_q_enqueue(scheduler->active, task);
        }
        
        process_completions(ring);
    }
}
void aco_schedule(db_schedule* scheduler, struct io_uring* ring) {
    
    while(1) {
        task_t* task;
        
        /*dont aquire a lock for no real reason*/
        if (task_q_is_empty(scheduler->active)) {
            if(io_uring_sq_ready(ring)){
                io_uring_submit(ring);
            }
            process_completions(ring);
            continue;
            /*pthread_mutex_lock(&scheduler->new_task_lock);
            while (task_q_is_empty(scheduler->active)) {
                pthread_cond_wait(&scheduler->new_task_sig, &scheduler->new_task_lock);
            }
            pthread_mutex_unlock(&scheduler->new_task_lock);
            */
        }
        
        task_q_dequeue(scheduler->active, &task);
        if (aco_unlikely(task->type == TERMINATE_NO_WAIT)){
            return;
        }
        else if(aco_unlikely(task->type == TERMINATE_WAIT)){
            freeze_scheduler(scheduler, 1, ring);
            return;
        }
        else if (aco_likely(task->real_task && task->stat != IO_WAIT)) {
            aco_resume(task->thread);
        }
        /*if the task enters io wait, drop from queue. its state is saved in an io_arg struct
        connected to the db_FILE*/
        if (task->stat == IO_WAIT) {
            continue;
        }
        
        if (task->stat == DONE) {
            check_return_code(task);
            task_q_enqueue(scheduler->pool, task);
            scheduler->ongoing_tasks --;
        } 
        else {
            task_q_enqueue(scheduler->active, task);
        }
        
        process_completions(ring);

    }
}

void init_scheduler(db_schedule* scheduler, aco_t* main_co, int pool_size, int stack_size, void * io_manager) {
    const int arena_size = pool_size * 2.5 * sizeof(task_t);
    scheduler->active = task_q_create(pool_size);
    scheduler->pool = task_q_create(pool_size);
    scheduler->shared = aco_share_stack_new(stack_size);
    scheduler->main = main_co;
    scheduler->a = malloc_arena(arena_size);
    scheduler->ongoing_tasks = 0;
    scheduler->io_manager = io_manager;
}
void cleanup_scheduler(db_schedule* scheduler) {
    task_t* task = NULL;
    
    while (!task_q_is_empty(scheduler->active)) {
        task_q_dequeue(scheduler->active, &task);
        aco_destroy(task->thread);
    }
    
    while (!task_q_is_empty(scheduler->pool)) {
        task_q_dequeue(scheduler->pool, &task);
        aco_destroy(task->thread);
    }
    
    task_q_destroy(scheduler->active);
    task_q_destroy(scheduler->pool);
    free_arena(scheduler->a);
}

