#include "compactor.h"
#include "iter.h"
#include "lsm.h"
#include <stdio.h>
#include <stdlib.h>
#include "option.h"
#include <pthread.h>
#include "../../util/multitask.h"
#include "../../util/io.h"
/**
 * @brief Initializes and starts the database
 * Initializes all database components including:
 * - Storage engine
 * - Compaction manager
 * - Memory pools
 * - Thread pools
 * - WAL (if enabled)
 * Must be called before any database operations
 */
typedef struct db_shard{
    storage_engine  *lsm;
    compact_manager  * manager;
    db_schedule  scheduler;
    struct io_manager * io_manager;
    pthread_t thread;
}db_shard;
typedef struct full_db{
    db_shard * shard;
    int num_shard;
} full_db;
db_shard  db_shard_create();
void db_shard_start_run(void * shard);
full_db * db(int num_shards);
void db_run(full_db * db);
void db_stop(full_db * db);


/**
 * @brief Shuts down the database and cleans up resources
 * Performs graceful shutdown including:
 * - Flushing all in-memory data to disk
 * - Completing pending compaction operations
 * - Freeing all allocated resources
 * - Closing all open files
 * @param lsm Pointer to the storage engine instance to shutdown
 * @param manager Pointer to the compaction manager to shutdown
 * @note Must be called before program exit to ensure data integrity
 */
void db_end(storage_engine * lsm, compact_manager * manager);




