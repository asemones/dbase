#include "multitask.h"
#include <stdbool.h>
#include "io.h"
#define SHARED_S_SIZE 512
#define MAX_RUNTIMES_DEFAULT 512
/*our user space task scheduler*/
void aco_execute(void);
// Using the cascade_runtime_t definition from the header file
task_t* create_task(aco_t* main, void * allocator, int id, enum task_type type, int stack_s, void* stack, aco_cofuncp_t func, void* args);
void allocate_new_tasks(cascade_runtime_t * runtime) {
    while(!task_q_is_full(runtime->scheduler.pool)){
        task_t* new_task = create_task(runtime->main_co, runtime->scheduler.a, 0, POOL_WAIT, SHARED_S_SIZE, runtime->scheduler.shared, aco_execute, NULL);
        new_task->scheduler = runtime;
        task_q_enqueue(runtime->scheduler.pool, new_task);
    }
}
void std_io_callback(void * arg){
    task_t * task = arg;
    task->stat = RUNNING;
    task_q_enqueue(task->scheduler->scheduler.active, task);
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
    task->real_task = NULL;
    
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
        task->ret = task->real_task(task->arg);
        task->real_task = NULL;
        task->stat = DONE;
    }
}
task_t * add_task(uint64_t id, task_func func, enum task_type type, void * arg ,cascade_runtime_t * scheduler, task_t * parent) {
    task_t* task;
    
    if (task_q_is_empty(scheduler->scheduler.pool)) {
        allocate_new_tasks(scheduler);
    }
    
    task_q_dequeue(scheduler->scheduler.pool, &task);
    task->real_task = func;
    task->type = type;
    task->arg = arg;
    task->id = id;
    task->parent = parent;
    task_q_enqueue(scheduler->scheduler.active, task);
    scheduler->scheduler.ongoing_tasks++;
    return task;
}
task_t * add_rpc(uint64_t id, task_func func,void * arg ,cascade_runtime_t * runtime, uint64_t p_id, task_t * parent) {
    task_t* task;
    db_schedule* scheduler = &runtime->scheduler;
    if (task_q_is_empty(scheduler->pool)) {
        allocate_new_tasks(runtime);
    }
    
    task_q_dequeue(scheduler->pool, &task);
    task->real_task = func;
    task->type = RPC;
    task->arg = arg;
    task->id = id;
    task->parent = parent;
    task->p_id = p_id;
    task_q_enqueue(scheduler->active, task);
    scheduler->ongoing_tasks++;
    return task;
}
uint64_t cascade_submit_external(cascade_runtime_t * rt, task_func function, void * args){
    uint64_t id = rpc_start(rt->scheduler.table);
    if (aco_unlikely(function == NULL)){
         add_task(id, function, TERMINATE_WAIT, args, rt, NULL);
    }
    add_task(id, function, MISC_COMPUTE, args, rt, NULL);
    return id;
}
future_t cascade_sub_intern_wait(task_func func, void* arg) {
    task_t * task =  aco_get_arg();
    uint64_t id = rpc_start(task->scheduler->scheduler.table);

    add_task(id, func, MISC_COMPUTE, arg, task->scheduler, task);
    await_local_id(task->scheduler->scheduler.table, id);
    future_t fut;
    rpc_get_result(task->scheduler->scheduler.table, id, &fut);
    return fut;
}
uint64_t cascade_sub_intern_nowait(task_func func, void* arg) {
    task_t * task =  aco_get_arg();
    uint64_t id = rpc_start(task->scheduler->scheduler.table);
    add_task(id, func, MISC_COMPUTE, arg, task->scheduler, task);
    return id;
}
uint64_t cascade_rpc_wait(cascade_runtime_t * targ, task_func func, void * arg){
    uint64_t id;
    {
        task_t * ts = aco_get_arg();
        msg_q * outbox = ts->scheduler->scheduler.links[targ->cpu].outbox;
        rpc_table_t * tbl = ts->scheduler->scheduler.table;
        id = rpc_start(tbl);
        if (msg_q_is_empty(outbox)){
            bitmap_set(ts->scheduler->scheduler.links[targ->cpu].queues_with_items, ts->scheduler->cpu);
        }
        msg_q_enqueue(outbox, (msg_t){
        .payload.value = arg,
        .on_recieve = func,
        .sender = ts,
        .recive = NULL,
        .size = id,
    });
    }
    await_rpc;
    return id;
}
uint64_t cascade_rpc_nowait(cascade_runtime_t * targ, task_func func, void * arg){
    uint64_t id;
    {
        task_t * ts = aco_get_arg();
        msg_q * outbox = ts->scheduler->scheduler.links[targ->cpu].outbox;
        rpc_table_t * tbl = ts->scheduler->scheduler.table;
        id = rpc_start(tbl);
        if (msg_q_is_empty(outbox)){
            bitmap_set(ts->scheduler->scheduler.links[targ->cpu].queues_with_items, ts->scheduler->cpu);
        }
        msg_q_enqueue(outbox, (msg_t){
        .payload.value = arg,
        .on_recieve = func,
        .sender = ts,
        .recive = NULL,
        .size = id,
    });
    }
    return id;
}
void poll_rpc(uint64_t *rpc_ids, int num) {
    rpc_table_t* tbl;
    {
          task_t *task = (task_t*) aco_get_arg();
          cascade_runtime_t * scheduler = task->scheduler;
          tbl = scheduler->scheduler.table;
    }
    while (1) {
        uint16_t completed_count = 0;
        for (uint16_t i = 0; i < num; i++) {
            if (rpc_is_complete(tbl, rpc_ids[i])) {
                completed_count++;
            }
        }
        if (completed_count == num) {
            return;
        }
        {
            task_t *task = aco_get_arg();
            task->stat = RPC_YIELD;
        }
        aco_yield();
    }
}
void poll_rpc_external(cascade_runtime_t * frame, uint64_t *rpc_ids, int num) {
    while (1) {
        uint16_t completed_count = 0;
        for (uint16_t i = 0; i < num; i++) {
            if (rpc_is_complete(frame->scheduler.table, rpc_ids[i])) {
                completed_count++;
            }
        }
        if (completed_count == num) {
            return;
        }
    }
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
            task_q_enqueue(scheduler->pool, task);
            scheduler->ongoing_tasks --;
        } else {
            task_q_enqueue(scheduler->active, task);
        }  
        process_completions(ring);
    }
    pthread_exit(NULL);
}
void task_cpy(task_t * dest, task_t * src){
    memcpy(dest, src, sizeof(sizeof(*dest)));
}
void process_msg(cascade_runtime_t * rt, msg_t msg){
    db_schedule * sch = &rt->scheduler;
    switch (msg.type){
        case ADD_TASK: {
            uint64_t id = rpc_start(sch->table);
            task_t * sub = add_rpc(id, msg.on_recieve, msg.payload.value,rt,  msg.size, msg.sender);
            sub->parent = msg.sender;
            break;
        }
        case FUNCTION_RETURN_VAL: {
            task_t * callee = msg.recive;
            callee->running_rpcs --;
            if (callee->running_rpcs <= 0 ){
                task_q_enqueue(sch->active, callee);
                callee->stat = RUNNING;
            }
            rpc_complete(sch->table, msg.size, msg.payload);
            break;
        }
        case FUNCTION_RETURN_VAL_NOWAIT: {
            task_t * callee = msg.recive;
            callee->running_rpcs --;
            rpc_complete(sch->table, msg.size, msg.payload);
            break;
        }
    }
}

void check_return_val(rpc_table_t * tbl, task_t * task, db_schedule * scheduler){
    switch (task->type){
        case RPC: {
            msg_t msg;
            msg.payload = task->ret;
            msg.size = task->p_id;
            msg.recive = task->parent;
            msg.reciver_shard_id = rpc_get_shard_id(task->p_id);
            msg.type = FUNCTION_RETURN_VAL;
            if (msg_q_is_empty(scheduler->links[msg.reciver_shard_id].outbox)) {
                bitmap_set(scheduler->links[msg.reciver_shard_id].queues_with_items, scheduler->id);
            }
            msg_q_enqueue(scheduler->links[msg.reciver_shard_id].outbox, msg);
            break;
        }
        case RPC_NO_WAIT: {
            msg_t msg;
            msg.payload = task->ret;
            msg.size = task->p_id;
            msg.recive = task->parent;
            msg.reciver_shard_id = rpc_get_shard_id(task->p_id);
            msg.type = FUNCTION_RETURN_VAL_NOWAIT;
            if (msg_q_is_empty(scheduler->links[msg.reciver_shard_id].outbox)) {
                bitmap_set(scheduler->links[msg.reciver_shard_id].queues_with_items, scheduler->id);
            }
            msg_q_enqueue(scheduler->links[msg.reciver_shard_id].outbox, msg);
            break;
        }
        case EXTERNAL_RPC:
            rpc_complete(tbl, task->id, task->ret);
            break;
        default:
            rpc_complete(tbl, task->id, task->ret);
            task->type = POOL_WAIT;
            if (task->parent){
                task->parent->running_rpcs --;
                if (task->parent->running_rpcs <=0){
                    task->parent->stat = RUNNING;
                    task_q_enqueue(scheduler->active, task);
                }   
            }       
    }
}
uint16_t process_a_queue(cascade_runtime_t *rt, uint32_t targ_id, uint32_t batch_size){
    uint16_t processed;
    msg_t msg;
    bitmap * shard_map = &rt->scheduler.my_active_msgs;
    msg_q * inbox = rt->scheduler.links[targ_id].inbox;
    for (processed =0; processed < batch_size; processed++){
        if (!msg_q_dequeue(inbox, &msg)){
            bitmap_clear(shard_map, targ_id);
            return processed;
        }
        process_msg(rt, msg);
    }
    if (msg_q_is_empty(inbox)){
        bitmap_clear(shard_map, targ_id);
    }
    return batch_size;
}
uint32_t check_queues(cascade_runtime_t *rt, uint32_t batch_size, uint32_t max_to_process){
    uint64_t msgs[MAX_RUNTIMES_DEFAULT];
    uint32_t num = bitmap_get_set_bits_512(&rt->scheduler.my_active_msgs, msgs, MAX_RUNTIMES_DEFAULT);
    if (num == 0) return 0; 
    uint32_t msgs_processed = 0;
    for (uint32_t i = 0; i < num && msgs_processed < max_to_process; i++) {
        uint32_t targ_id = (uint32_t)msgs[i];
        msgs_processed += process_a_queue(rt, targ_id, batch_size);
    }
    return msgs_processed;
}

void cascade_schedule(cascade_runtime_t* rt, struct io_uring* ring) {
    db_schedule * scheduler = &rt->scheduler;
    
    while(1) {
        task_t* task;
        
        /*dont aquire a lock for no real reason*/
        
        if (task_q_is_empty(scheduler->active)) {
            if(io_uring_sq_ready(ring)){
                io_uring_submit(ring);
            }
            process_completions(ring);
            check_queues(rt, 4, 40);
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
        /*we can drop this, since the rpc core will place in the inbox*/
        else if (aco_unlikely(task->stat == RPC_YIELD)){
            continue;
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
            check_return_val(scheduler->table, task, scheduler);
            task_q_enqueue(scheduler->pool, task);
            scheduler->ongoing_tasks --;
        } 
        else {
            task_q_enqueue(scheduler->active, task);
        }    
        process_completions(ring);
        check_queues(rt, 4, 40);
    }
}
void init_scheduler(db_schedule* scheduler, aco_t* main_co, int pool_size, int stack_size, void * io_manager) {
    const int arena_size = pool_size * 2.5 * sizeof(task_t);
    scheduler->active = task_q_create(pool_size);
    scheduler->pool = task_q_create(pool_size);
    scheduler->shared = aco_share_stack_new(stack_size);
    scheduler->a = malloc_arena(arena_size);
    scheduler->ongoing_tasks = 0;
  
    scheduler->table = calloc(sizeof(*scheduler->table), 1);
    scheduler->table->shard_id = scheduler->id;
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
io_config create_io_config (){
    io_config config;
    config.big_buf_s = 1024 * 64;
    config.max_concurrent_io = 5000;
    config.huge_buf_s = 1024 * 1024;
    config.num_big_buf = 16;
    config.num_huge_buf = 4;
    config.small_buff_s = 4096;
    config.coroutine_stack_size = 512;
    return config;
}
typedef struct runtime_spawn_args{
    task_func  seed;
    void * arg;
    io_config config;
    cascade_runtime_t * runtime;
    pthread_cond_t * sig;
    bool done;
} runtime_spawn_args;
void spawn_runtime_internal(void * arg){
      runtime_spawn_args * args = arg;
      io_config config=  args->config;
      struct io_manager *man = malloc(sizeof(*man));
      io_prep_in(man, config.small_buff_s, config.max_concurrent_io, config.big_buf_s, config.huge_buf_s, config.num_huge_buf, config.num_big_buf);
      aco_t * main = aco_create(NULL, NULL, 0, NULL, NULL);
      args->runtime->main_co = main;
      init_scheduler(&args->runtime->scheduler,main, config.max_concurrent_io, config.coroutine_stack_size, man);
      args->runtime->scheduler.my_active_msgs = bitmap_create(MAX_RUNTIMES_DEFAULT);
      add_task(0, args->seed, MISC_COMPUTE, args->arg, args->runtime, NULL);
      args->done = true;
      pthread_cond_signal(args->sig);
      cascade_schedule(args->runtime, &man->ring);
      
}
cascade_runtime_t * cascade_spawn_runtime_config(task_func  seed, void * arg, io_config config){
    cascade_runtime_t * runtime = calloc(sizeof(*runtime),1);
    runtime_spawn_args * args = calloc(sizeof(*args), 1);
    args->seed = seed;
    args->config = config;
    args->arg = arg;
    args->runtime =runtime;
    args->done =  false;
    pthread_cond_t var;
    pthread_mutex_t mutx;
    pthread_mutex_init(&mutx, NULL);
    pthread_cond_init(&var, NULL);
    args->sig = &var;
    pthread_create(&runtime->thread, NULL, (void *(*)(void *))spawn_runtime_internal, args);
    pthread_mutex_lock(&mutx);
    while(!args->done){
        pthread_cond_wait(&var,&mutx);
    }
    pthread_mutex_unlock(&mutx);
    pthread_mutex_destroy(&mutx);
    pthread_cond_destroy(&var);
    free(args);
    return runtime;
}
cascade_runtime_t * cascade_spawn_runtime_self(task_func  seed, void * arg, io_config config){
    cascade_runtime_t * runtime = calloc(sizeof(*runtime),1);
    runtime_spawn_args * args = calloc(sizeof(*args), 1);
    args->seed = seed;
    args->config = config;
    args->arg = arg;
    args->runtime =runtime;
    spawn_runtime_internal(arg);
    return runtime;
}
cascade_runtime_t * cascade_spawn_runtime_default(task_func  seed, void * arg){
    return cascade_spawn_runtime_config(seed, arg, create_io_config());
}
void link_two_runtimes(cascade_runtime_t * one , cascade_runtime_t * two){
    /*consider using allocators from runtime one and runtime two*/
    msg_q * one_writes_to_me = msg_q_create(MAX_RUNTIMES_DEFAULT);
    msg_q * two_writes_to_me = msg_q_create(MAX_RUNTIMES_DEFAULT);
    int one_id = one->cpu;
    int two_id = two->cpu;
    one->scheduler.links[two_id].outbox = one_writes_to_me;
    one->scheduler.links[two_id].inbox  = two_writes_to_me;
    one->scheduler.links[two_id].queues_with_items = &two->scheduler.my_active_msgs;

    two->scheduler.links[one_id].outbox = two_writes_to_me;
    two->scheduler.links[one_id].inbox = one_writes_to_me;
    two->scheduler.links[one_id].queues_with_items = &one->scheduler.my_active_msgs;
    

}
void add_link_to_framework(cascade_framework_t * framework, cascade_runtime_t * new_runtime){
    new_runtime->cpu = framework->num_shards;
    new_runtime->scheduler.id = new_runtime->cpu;
    new_runtime->scheduler.table->shard_id =  new_runtime->cpu;
    ///pin cpu here?
    for (int i = 0; i < framework->num_shards; i++) {
        cascade_runtime_t *existing = framework->runtimes[i];
        link_two_runtimes(existing, new_runtime);
    }
    framework->runtimes[framework->num_shards] = new_runtime;
    framework->num_shards ++;
}
cascade_framework_t * create_framework(){
    cascade_framework_t * framework = calloc(sizeof(*framework), 1);
    return framework;
}
void kill_framework(cascade_framework_t * frame){
    u_int64_t ids[512];
    for (int i = 0; i < frame->num_shards; i++){
        ids[i] = cascade_submit_external(frame->runtimes[i], NULL, NULL);
    }
}
future_t get_return_val(uint64_t id){
    future_t fut;
    task_t * t = aco_get_arg();
    rpc_get_result(t->scheduler->scheduler.table, id, &fut);
    return fut;
}
future_t get_return_val_rt(cascade_runtime_t * rt, uint64_t id){
    future_t fut;
    rpc_get_result(rt->scheduler.table, id, &fut);
    return fut;
}
uint64_t find_node_hash(cascade_framework_t * frame, const char * req_key, int size,  int me){
    uint64_t hash = fnv1a_64(req_key, size) % frame->num_shards;
    if (me!= hash || frame->num_shards <= 1 ){
        return hash;
    }
    return (hash + 1) % frame->num_shards;
}
uint64_t find_node_hash_internal(cascade_framework_t * frame, const char * req_key, int size){
    task_t * task = aco_get_arg();
    int me = task->scheduler->cpu;
    uint64_t hash = fnv1a_64(req_key, size) % frame->num_shards;
    if (me!= hash|| frame->num_shards <= 1 ){
        return hash;
    }
    return (hash + 1) % frame->num_shards;
}
uint64_t cascade_frame_sub_ext_rndm(cascade_framework_t * frame, task_func func, void * arg, int arg_size){
    uint64_t node= find_node_hash(frame, arg, arg_size, -1);
    cascade_submit_external(frame->runtimes[node], func, arg);
}
void cascade_frame_ext_poll(cascade_framework_t * frame, uint64_t * id, uint64_t num, future_t * store){
    for (int i = 0; i < num; i++){
        uint32_t node= rpc_get_shard_id(id[0]);
        poll_rpc_external(frame->runtimes[node],&id[i],1 );
        get_return_val_rt(frame->runtimes[node], id[i]);
    }
}