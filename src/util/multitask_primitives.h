#include "multitask.h"

typedef struct cascade_mutex_t {
    task_t *owner;
    bool locked;
    task_q *wake_queue;
} cascade_mutex_t;
typedef struct cascade_cond_t{
  task_q *wake_queue;
    
}cascade_cond_t;
void cascade_mutex_init(cascade_mutex_t *m);
void cascade_mutex_lock(cascade_mutex_t *m);
void cascade_mutex_unlock(cascade_mutex_t *m);
void cascade_mutex_destroy(cascade_mutex_t *m);
bool cascade_mutex_checklock(cascade_mutex_t * m);