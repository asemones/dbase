#include <pthread.h>
#include "frontier.h"
#include <unistd.h>
#include <stdio.h>
#include "../util/alloc_util.h"
#include "arena.h"
#ifndef THREADPOOL_H
#define THREADPOOL_H





typedef struct thread{
    pthread_t thread;
    int ID; 
    struct thread_p * thrpool;
    char ** buf;
}thread;
typedef struct work{
    void (*function)(void* arg, void** result, thread* thread);
    void * arg;
    void ** reV;
}work;

typedef struct thrd_pq_value{
    int priority;
    work item;
}thrd_pq_value;

typedef struct thread_p{
    thread** threads;
    volatile int  num_threads;
    int num_killed;
    pthread_mutex_t counter_lock;
    pthread_cond_t queue_cond;  
    pthread_mutex_t out_lock; 
    pthread_mutex_t queue_lock;  
    pthread_cond_t freeze;  
    frontier *work_pq;
    frontier * out_pq;
    bool killSig;

}thread_p;

/**
 * @brief Executes the threads
 *
 * @param p Pointer to the thread to be executed.
 */
void execute(thread *p);

/**
 * @brief Creates a new thread with the given id and adds it to the specified thread pool.
 *
 * @param id The identifier for the new thread.
 * @param pool Pointer to the thread pool to which the new thread will be added.
 * @return Pointer to the newly created thread.
 */
thread* Thread(int id, thread_p* pool, size_t dts);

/**
 * @brief Creates a thread pool with a specified number of threads.
 *
 * @param num_threads The number of threads to create in the thread pool.
 * @return Pointer to the newly created thread pool.
 */
thread_p *thread_Pool(int num_threads, size_t thrd_mem_size);

/**
 * @brief Adds a new work item to the thread pool.
 *
 * @param p Pointer to the thread pool.
 * @param functions Pointer to the function to be executed.
 * @param args Pointer to the arguments to be passed to the function.
 * @param retV Pointer to store the return value of the function.
 * @param priority Pointer to the priority of the work item.
 */
void add_work(thread_p *p,  void (*functions)(void*, void **, thread *p), void* args, void ** retV, int * priority);

/**
 * @brief Retrieves the result of a work item executed by a thread.
 *
 * @param p Pointer to the thread.
 * @param work_item Pointer to store the result of the work item.
 */
void get_result(thread * p , thrd_pq_value work_item);
/**
 * @brief Terminates all threads in the specified thread pool.
 *
 * @param p Pointer to the thread pool.
 */
void kill(thread_p * p);

/**
 * @brief Destroys the thread pool and frees all associated resources.
 *
 * @param p Pointer to the thread pool.
 */
void destroy_pool(thread_p *p);
/**
 * @brief Destroys the thread pool and frees all associated resources.
 *
 * @param temp pointer to the kv element of the queue
 * @param void(*function)(void* arg, void *arg2) a function to free the elemenets (whatever is stored as kv->value)
 * @param arg2 the second arguement of the function to free the work elements
 */
void free_work_elements(kv* temp, void(*function)(void* arg, void *arg2), void *arg2);

int compare_pq_v(const void * pq_v, const void * pq_v2){
    const thrd_pq_value * k1 = pq_v;
    const thrd_pq_value * k2 = pq_v2;
    if (k1 == NULL || k2 == NULL){
        return -1;
    }
    
    return  k1->priority > k2->priority; 
}
thread* Thread(int id, thread_p* pool,size_t dts){
    thread *p = (thread*)wrapper_alloc((sizeof(thread)), NULL,NULL);
    p->ID = id;
    p->thrpool= pool;
    p->buf = wrapper_alloc((sizeof(char*)), NULL,NULL);
   *p->buf = wrapper_alloc((dts), NULL,NULL);
    pthread_create(&(p)->thread, NULL, (void * (*)(void *)) execute, (p));
	pthread_detach((p)->thread);

    return p;
}
thread_p *thread_Pool(int num_threads, size_t thrd_mem_size){
    thread_p* p = (thread_p*)wrapper_alloc((sizeof(thread_p)), NULL,NULL);
    p->threads = (thread**)wrapper_alloc((num_threads* sizeof(thread *)), NULL,NULL);
    p->num_threads =0;
    p->killSig = false;
    p->work_pq = Frontier(sizeof(thrd_pq_value),true, &compare_pq_v);
    p->out_pq = Frontier(sizeof(thrd_pq_value),false, &compare_pq_v);
    pthread_cond_init(&(p->freeze), NULL);
	pthread_mutex_init(&(p->counter_lock), NULL);
    pthread_mutex_init(&(p->out_lock), NULL);
    pthread_cond_init(&(p->queue_cond), NULL);
	pthread_mutex_init(&(p->queue_lock), NULL);
    for (int i = 0 ; i < num_threads; i++){
        p->threads[i] = Thread(i,p, thrd_mem_size);
        p->num_threads ++;
    }
    p->num_killed = 0;
    return p;
}   
void add_work(thread_p *p,  void (*functions)(void*, void ** , thread* p), void* args,void ** retV, int * priority){
    work worked;
    worked.function = functions;
    worked.arg=  args;
    worked.reV = retV;
    pthread_mutex_lock(&(p->queue_lock));
    thrd_pq_value temp;
    temp.priority= *priority;
    temp.item= worked;
    enqueue(p->work_pq,&temp);
    //.free(temp);
    pthread_cond_broadcast(&(p->queue_cond));
    pthread_mutex_unlock(&(p->queue_lock));

}

void execute(thread *p) {
    while (1) {
        if (p->thrpool->killSig) {
            pthread_mutex_lock(&(p->thrpool->counter_lock));
            pthread_cond_signal(&(p->thrpool->freeze));
            p->thrpool->num_killed++;
            pthread_mutex_unlock(&(p->thrpool->counter_lock));
            pthread_exit(NULL);
            break;
        }

        pthread_mutex_lock(&(p->thrpool->queue_lock));
        while (p->thrpool->killSig == false && p->thrpool->work_pq->queue->len == 0) {
            pthread_cond_wait(&(p->thrpool->queue_cond), &(p->thrpool->queue_lock));
        }

        if (p == NULL || p->thrpool->killSig) {
            pthread_mutex_unlock(&(p->thrpool->queue_lock));
            pthread_mutex_lock(&p->thrpool->counter_lock);
            p->thrpool->num_killed++;
            pthread_cond_signal(&(p->thrpool->freeze));
            pthread_mutex_unlock(&p->thrpool->counter_lock);
            pthread_exit(NULL);
            break;
        }

        thrd_pq_value work_item;
        dequeue(p->thrpool->work_pq, &work_item);
        pthread_mutex_unlock(&(p->thrpool->queue_lock));

        get_result(p, work_item);

        pthread_mutex_lock(&(p->thrpool->counter_lock));


        pthread_cond_signal(&(p->thrpool->queue_cond));
        pthread_mutex_unlock(&(p->thrpool->counter_lock));
    }

    return;
}

void get_result(thread * p , thrd_pq_value work_item){
     work_item.item.function(work_item.item.arg, work_item.item.reV, p);
     pthread_mutex_lock(&(p->thrpool->out_lock));
     enqueue(p->thrpool->out_pq, &work_item);
     //free(work_item);
     pthread_mutex_unlock(&((p->thrpool->out_lock)));
    return;
}
void kill(thread_p * p){
    p->killSig = true;
    return;
}
void destroy_pool(thread_p *p){
    if (p== NULL){
        return;
    }

    kill(p);
    pthread_mutex_lock(&(p->counter_lock));
    pthread_cond_broadcast(&(p->queue_cond)); 

    while (p->num_killed < p->num_threads) {
        pthread_cond_wait(&p->freeze, &p->counter_lock);
    }
    pthread_mutex_unlock(&p->counter_lock);

    for (int i = 0 ;  i < p->num_threads; i++){
        free(*(p->threads[i]->buf));
        free(p->threads[i]->buf);
        free(p->threads[i]);
    }
    pthread_mutex_destroy(&(p->queue_lock));
    pthread_mutex_destroy(&(p->counter_lock));
    pthread_cond_destroy(&(p->queue_cond));
    pthread_cond_destroy(&(p->freeze));
    free_front(p->work_pq);
    free_front(p->out_pq);
    free(p->threads);
    p->threads = NULL;
    free(p);
    p = NULL;

}
void free_work_elements(kv* temp, void(*function)(void* arg, void *arg2), void *arg2){
    if (temp== NULL){
        return;
    }
    if (temp->key!= NULL){
        free(temp->key);
    }
    work * temp2 = (work*)temp->value;
    if (temp2 != NULL){
        if (temp2->arg !=NULL){
            free(temp2->arg);
        }
        if (*temp2->reV != NULL){
            function(*temp2->reV , arg2);
        }
        free(temp2->reV);
    }
    free (temp2);
    return;
}



#endif