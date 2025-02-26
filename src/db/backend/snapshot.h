#include <stdlib.h>
#include <stdio.h>
#include "../../ds/list.h"
#include <pthread.h>
#include "../../util/error.h"
#include "../../util/stringutil.h"
#include "indexer.h"
#include "../../ds/cache.h"
#define LEVELS 7
#define TIME_STAMP_SIZE 64
#pragma once





/*need to replace current metadata raw list with snapshots. Snapshots = the db, minus meetables for now. Using this, we can effectively track and exchange database snapshots while controlling
freeing of resources. It's the solution for allowing concurrenct compaction and required for mvcc*/
/*problems to solve:
1. How can we ensure that compactations can occur without locking reads/writes
*/
typedef struct snapshot{
    char * timestamp;
    int reference_counter;
    pthread_mutex_t counter_lock;
    list * sst_files[LEVELS];
    cache * cache_ref; /*for reading and writing*/
}snapshot;


/*creates copy of metadata info*/
snapshot * create_snap(void);
int init_snapshot(snapshot * master, snapshot * s, cache * c);
int destroy_snapshot(snapshot * s);
void ref_snapshot(snapshot * s);
void deref_snapshot(snapshot * s);
void deref_with_free(snapshot * s);
