#include "multitask.h"
/*our user space task scheduler*/


task_t  create_task(aco_t * main, int id, enum task_type type, int stack_s, void * stack, aco_cofuncp_t func, void * args){
    task_t task;
    task.id= id;
    task.type = type;
    task.stat = NOT_START;
    task.ret  = NULL;
    task.active_io_requests = 0;
    if(stack != NULL){
        task.thread = aco_create(main, stack, stack_s, func, args);
    }
    else if (stack == NULL){
        task.thread = aco_create(main, aco_share_stack_new(stack_s), stack_s, func, args );
    }
    task.real_task = NULL;
    return task;
}
void aco_execute(void){
    aco_t * thread = aco_get_co();
    task_t * task = thread->arg;
    task->stat = NOT_START;
    aco_yield();
    while(1){
        while(!task->real_task) {
            aco_yield();
        }
        task->stat = RUNNING;
        task->real_task(task);
        task->stat = DONE;
        task->type = POOL_WAIT;
        task->real_task = NULL;
    }
}
void add_task(task_func  func, enum task_type type, db_schedule * scheduler){
    task_t task;
    if (task_q_is_empty(scheduler->pool)){
        allocate_new_tasks();
    }
    task_q_dequeue(scheduler->pool, &task);
    task.real_task = func;
    task.type = type;
    task_q_enqueue(scheduler->pool,task);
    pthread_cond_signal(&scheduler->new_task_sig);
}
void on_io_complete(void * arg){
    io_arg * parent_task = arg;
    parent_task->task.stat= RUNNING;
    parent_task->task.active_io_requests --;
    if (parent_task->task.active_io_requests <= 0){
        task_q_enqueue(parent_task->active, parent_task->task);
        pthread_cond_signal(parent_task->item_sig);
    }
}
void aco_schedule(db_schedule * scheduler, struct io_uring * ring ){
    while(1){
        task_t  task;
        /*dont aquire a lock for no real reason*/
        if (task_q_is_empty(scheduler->active)){
            pthread_mutex_lock(&scheduler->new_task_lock);
            while (task_q_is_empty(scheduler->active)){
                pthread_cond_wait(&scheduler->new_task_sig, &scheduler->new_task_lock);
            }
            pthread_mutex_unlock(&scheduler->new_task_lock);
        }
        task_q_dequeue(scheduler->active, &task);
        if (task.real_task && task.stat != IO_WAIT){
            aco_resume(task.real_task);
        }
        /*if the task enters io wait, drop from queue. its state is saved in an io_arg struct
        connected to the io_request*/
        if (task.stat == IO_WAIT){
            continue;
        }
        if (task.real_task && task.stat == DONE){
            check_return_code();
            task_q_enqueue(scheduler->pool, &task);
        }
        else {
           task_q_enqueue(scheduler->active, task);
        } 
        process_completions(ring);
    }
}

void init_scheduler(db_schedule *scheduler, aco_t *main_co, int pool_size) {
    //FINISH
    scheduler->active = task_q_create(pool_size);
    scheduler->pool = task_q_create(pool_size);
    
    pthread_mutex_init(&scheduler->new_task_lock, NULL);
    pthread_cond_init(&scheduler->new_task_sig, NULL);
}
void init_scheduler_tasks(task_q * q, aco_t * main, int stack_s, bool shared){
    aco_share_stack_t * stack;
    if (shared){
        stack  = aco_share_stack_new(stack_s);
    }
    for(int i =0; i < q->capacity; i++){
        if (!shared) stack = NULL;
        q->array[i]= create_task(main, i, POOL_WAIT, stack_s, stack,aco_execute,NULL);
    }   
}
void cleanup_scheduler(db_schedule *scheduler) {
    task_t *task;
    while (!task_q_is_empty(scheduler->active)) {
        task_q_dequeue(scheduler->active, &task);
        aco_destroy(task->thread);
        free(task);
    }
    
    while (!task_q_is_empty(scheduler->pool)) {
        task_q_dequeue(scheduler->pool, &task);
        aco_destroy(task->thread);
        free(task);
    }
    
    task_q_destroy(scheduler->active);
    task_q_destroy(scheduler->pool);
    
    pthread_mutex_destroy(&scheduler->new_task_lock);
    pthread_cond_destroy(&scheduler->new_task_sig);
}