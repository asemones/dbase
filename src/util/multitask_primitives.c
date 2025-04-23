#include "multitask_primitives.h"
#include "multitask.h" // For task_t, task_q, get_current_task, yield, wake_task, etc.
#include <stdbool.h>
#include <stddef.h> // For NULL
#include <assert.h>

// Initialize a cascade mutex
void cascade_mutex_init(cascade_mutex_t *m) {
    if (!m) return;
    m->owner = NULL;
    m->locked = false;
    const int cap = 500;
    m->wake_queue = task_q_create(cap);
   
    assert(m->wake_queue != NULL);
}

// Lock a cascade mutex
void cascade_mutex_lock(cascade_mutex_t *m) {
    if (!m) return;
    task_t *current_task = aco_get_arg();
    assert(current_task != NULL); 

 
    if (m->locked && m->owner == current_task) {
        return;
    }

    while (m->locked) {
        task_q_push(m->wake_queue, current_task);
        aco_yield();
    }
    m->locked = true;
    m->owner = current_task;
}
bool cascade_mutex_checklock(cascade_mutex_t * m){
    return (m->locked);
}
void cascade_mutex_unlock(cascade_mutex_t *m) {
    if (!m) return;
    task_t *current_task = get_current_task();
    assert(current_task != NULL);

    assert(m->locked && m->owner == current_task);

    if (!task_q_is_empty(m->wake_queue)) {
        task_t *next_task = task_q_pop(m->wake_queue);
        assert(next_task != NULL); 
        m->owner = next_task;
       
        wake_task(next_task);
    } 
    else {
        m->locked = false;
        m->owner = NULL;
    }
}


void cascade_mutex_destroy(cascade_mutex_t *m) {
    if (!m) return;
   
    assert(!m->locked);
    assert(task_q_is_empty(m->wake_queue));

    task_q_destroy(m->wake_queue);
    m->wake_queue = NULL;
    m->owner = NULL;
}
