#include <pthread.h>
#include "frontier.h"
#include <unistd.h>
#include <stdio.h>
#include "../util/alloc_util.h"
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

typedef struct thread_p{
    thread** threads;
    volatile int  num_threads;
    volatile int  num_alive;
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
thread_p *thread_Pool(int num_threads, size_t dts);

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
void get_result(thread*p ,kv *work_item);

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
thread_p *thread_Pool(int num_threads, size_t dts){
    thread_p* p = (thread_p*)wrapper_alloc((sizeof(thread_p)), NULL,NULL);
    p->threads = (thread**)wrapper_alloc((num_threads* sizeof(thread *)), NULL,NULL);
    p->num_threads =0;
    p->num_alive = 0;
    p->killSig = false;
    p->work_pq = Frontier(sizeof(kv*),true);
    p->out_pq = Frontier(sizeof(kv*),true );
    pthread_cond_init(&(p->freeze), NULL);
	pthread_mutex_init(&(p->counter_lock), NULL);
    pthread_mutex_init(&(p->out_lock), NULL);
    pthread_cond_init(&(p->queue_cond), NULL);
	pthread_mutex_init(&(p->queue_lock), NULL);
    for (int i = 0 ; i < num_threads; i++){
        p->threads[i] = Thread(i,p, dts);
        p->num_threads ++;
    }
    return p;
}   
void add_work(thread_p *p,  void (*functions)(void*, void ** , thread* p), void* args,void ** retV, int * priority){
    work* worked= (work*)wrapper_alloc((sizeof(work)), NULL,NULL);
    worked->function = functions;
    worked->arg=  args;
    worked->reV = retV;
    pthread_mutex_lock(&(p->queue_lock));
    kv * temp = (kv*)wrapper_alloc((sizeof(kv)), NULL,NULL);
    temp->key = priority;
    temp->value= worked;
    enqueue(p->work_pq,temp);
    free(temp);
    pthread_cond_broadcast(&(p->queue_cond));
    pthread_mutex_unlock(&(p->queue_lock));

}

void execute(thread *p) {
    while (1) {
        if (p->thrpool->killSig) {
            fprintf(stderr, "Thread %d is killed at first check\n", p->ID);
            fprintf(stderr, "num killed %d\n", p->thrpool->num_killed);
            p->thrpool->num_killed++;
            break;
        }

        pthread_mutex_lock(&(p->thrpool->queue_lock));
        while (p->thrpool->killSig == false && p->thrpool->work_pq->queue->len == 0) {
            pthread_cond_wait(&(p->thrpool->queue_cond), &(p->thrpool->queue_lock));
        }

        if (p == NULL || p->thrpool->killSig) {
            pthread_mutex_unlock(&(p->thrpool->queue_lock));
            p->thrpool->num_killed++;
            fprintf(stderr, "Thread %d is killed at second check\n", p->ID);
            fprintf(stderr, "num killed %d\n", p->thrpool->num_killed);
            break;
        }

        kv *work_item = dequeue(p->thrpool->work_pq);
        pthread_mutex_unlock(&(p->thrpool->queue_lock));

        p->thrpool->num_alive++;
        get_result(p, work_item);

        pthread_mutex_lock(&(p->thrpool->counter_lock));
        p->thrpool->num_alive--;

        if (p->thrpool->killSig) {
            pthread_mutex_unlock(&(p->thrpool->counter_lock));
            p->thrpool->num_killed++;
            fprintf(stderr, "Thread %d is killed at final check\n", p->ID);
            fprintf(stderr, "num killed %d\n", p->thrpool->num_killed);
            break;
        }

        pthread_cond_signal(&(p->thrpool->freeze));
        pthread_cond_signal(&(p->thrpool->queue_cond));
        pthread_mutex_unlock(&(p->thrpool->counter_lock));
    }

    return;
}

void get_result(thread * p ,kv *work_item){
    ((work*)work_item->value)->function(((work*)work_item->value)->arg,((work*)work_item->value)->reV, p);
     pthread_mutex_lock(&(p->thrpool->out_lock));
     enqueue(p->thrpool->out_pq, work_item);
     free(work_item);
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
    while (p->num_alive > 0) {
        pthread_cond_wait(&(p->freeze), &(p->counter_lock));
    }
    sleep(1);
    pthread_mutex_unlock(&(p->counter_lock));
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
    free(temp);
    return;
}



#endif