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
void db_start();

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




