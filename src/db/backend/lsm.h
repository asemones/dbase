
#include <stdio.h>
#include "../../ds/bloomfilter.h"
#include "dal/metadata.h"
#include "../../ds/list.h"
#include "../../ds/hashtbl.h"
#include <time.h>
#include <string.h>
#include <pthread.h>
#include "../../ds/structure_pool.h"
#include "../../util/stringutil.h"
#include "../../util/alloc_util.h"
#include "../../ds/skiplist.h"
#include "../../ds/cache.h"
#include "../../ds/associative_array.h"
#include "option.h"
#include "key-value.h"
#include "WAL.h"
#include "indexer.h"
#include  "../../ds/cache_shrd_mnger.h"
#ifndef LSM_H
#define LSM_H

#define MEMTABLE_SIZE 80000
#define TOMB_STONE "-"
#define NUM_HASH 7
#define LOCKED_TABLE_LIST_LENGTH 2
#define CURRENT_TABLE 0
#define PREV_TABLE 1
#define READER_BUFFS 10
#define WRITER_BUFFS 10
#define LVL_0 0
#define MAXIUMUM_KEY 0
#define MINIMUM_KEY 1
#define KB 1024
#define TEST_LRU_CAP 1024*40


/*This is the storage engine struct which controls all operations at the disk level. Ideally,
the user should never be concerned with this struct. The storage engine contains memtables, one active and one immutable. It also has
pools for reading and writing data and access the metadata struct. The mem_table uses a skiplist.
The general read and write functions are present here*/

/*simplifed MVCC: Since we are using a lsm tree, we can implement mvcc in a clever way by a combintation of using sst timestamps and a strat
to ensure that duplicate instances never exist inside one sst table. To do this, we can ensure that A: only one copy of a record in a memtable is flushed. When the memtable is locked, only the most recent verison of duplicates will be written to disk. Any other copies
will remain in memory. During compaction, we can preform a copy-on-write approach in regard to sst table intergration. Once no more reader threads
are acting on the old copies of the files, we can swap out the sst files.*/
/**
 * @brief Structure representing a memory table in the LSM tree
 * @struct mem_table
 * @param bytes Size of the memory table in bytes
 * @param filter Bloom filter for the memory table
 * @param num_pair Number of key-value pairs in the memory table
 * @param immutable Flag indicating if the memory table is immutable
 * @param range Array containing the minimum and maximum keys
 * @param skip Skip list storing the key-value pairs
 */
typedef struct mem_table{
    size_t bytes;
    bloom_filter *filter;
    size_t num_pair;
    bool immutable;
    char *range[2];
    SkipList *skip;
}mem_table;

/**
 * @brief Structure representing the storage engine.
 * @struct storage_engine
 * @param table Array of memory tables (active and immutable).
 * @param meta Pointer to the metadata structure.
 * @param num_table Number of memory tables.
 * @param write_pool Pool for write operations.
 * @param cach Shard controller for cache management.
 * @param w Pointer to the Write-Ahead Log (WAL).
 * @param compactor_wait Condition variable for compaction waiting.
 * @param compactor_wait_mtx Mutex for compaction waiting.
 * @param cm_ref Flag indicating if compaction is in progress.
 * @param error_code Error code for the storage engine.
 */
typedef struct storage_engine{
    mem_table * table[LOCKED_TABLE_LIST_LENGTH];
    meta_data * meta;
    size_t num_table;
    struct_pool * write_pool;
    shard_controller cach;
    WAL * w;
    pthread_cond_t * compactor_wait;
    pthread_mutex_t * compactor_wait_mtx;
    bool * cm_ref;
    int error_code;
}storage_engine;

/**
 * @brief Locks the current memory table for writing.
 * @param engine A pointer to the storage engine.
 */
void lock_table(storage_engine * engine);

/**
 * @brief Creates a new memory table.
 * @return A pointer to the newly created memory table.
 */
mem_table * create_table(void);

/**
 * @brief Restores the state of the storage engine.
 * @param e A pointer to the storage engine.
 * @param lost_tables The number of tables lost.
 * @return An integer indicating success (0) or failure (non-zero).
 */
int restore_state(storage_engine * e, int lost_tables);

/**
 * @brief Clears the contents of a memory table.
 * @param table A pointer to the memory table to clear.
 */
void clear_table(mem_table * table);

/**
 * @brief Creates a new storage engine.
 * @param file The path to the data file.
 * @param bloom_file The path to the bloom filter file.
 * @return A pointer to the newly created storage engine.
 */
storage_engine * create_engine(char * file, char * bloom_file);

/**
 * @brief Writes a key-value pair to the storage engine.
 * @param engine A pointer to the storage engine.
 * @param key The key to write.
 * @param value The value to write.
 * @return An integer indicating success (0) or failure (non-zero).
 */
int write_record(storage_engine * engine, db_unit key, db_unit value);

/**
 * @brief Finds the SST file containing a given key.
 * @param sst_files A list of SST files.
 * @param num_files The number of SST files in the list.
 * @param key The key to search for.
 * @return The index of the SST file containing the key, or the index where it should be inserted.
 */
size_t find_sst_file(list * sst_files, size_t num_files, const char * key);

/**
 * @brief Finds the block containing a given key within an SST file.
 * @param sst A pointer to the SST file information.
 * @param key The key to search for.
 * @return The index of the block containing the key.
 */
size_t find_block(sst_f_inf * sst, const char * key);

/**
 * @brief Reads a value from disk based on a given key.
 * @param engine A pointer to the storage engine.
 * @param keyword The key to search for.
 * @return A pointer to the value read from disk, or NULL if not found.
 */
char * disk_read(storage_engine * engine, const char * keyword);

/**
 * @brief Reads a value from disk based on a given key and snapshot.
 * @param snap A pointer to the snapshot.
 * @param keyword The key to search for.
 * @return A pointer to the value read from disk, or NULL if not found.
 */
char * disk_read_snap(snapshot * snap, const char * keyword);

/**
 * @brief Reads a value from the storage engine based on a given key.
 * @param engine A pointer to the storage engine.
 * @param keyword The key to search for.
 * @return A pointer to the value read from the storage engine, or NULL if not found.
 */
char* read_record(storage_engine * engine, const char * keyword);

/**
 * @brief Reads a value from the storage engine based on a given key and snapshot.
 * @param engine A pointer to the storage engine.
 * @param keyword The key to search for.
 * @param s A pointer to the snapshot.
 * @return A pointer to the value read from the storage engine, or NULL if not found.
 */
char* read_record_snap(storage_engine * engine, const char * keyword, snapshot * s);

/**
 * @brief Serializes a SkipList to a byte buffer.
 * @param list A pointer to the SkipList.
 * @param buffer A pointer to the byte buffer.
 * @param s A pointer to the SST file information.
 */
void seralize_table(SkipList * list, byte_buffer * buffer, sst_f_inf * s);

/**
 * @brief Flushes the current memory table to disk.
 * @param engine A pointer to the storage engine.
 * @return An integer indicating success (0) or failure (non-zero).
 */
int flush_table(storage_engine * engine);

/**
 * @brief Frees the memory allocated for a single memory table.
 * @param table A pointer to the memory table to free.
 */
void free_one_table(mem_table * table);

/**
 * @brief Dumps the contents of the memory tables to disk.
 * @param engine A pointer to the storage engine.
 */
void dump_tables(storage_engine * engine);

/**
 * @brief Frees the memory allocated for all memory tables.
 * @param engine A pointer to the storage engine.
 */
void free_tables(storage_engine * engine);

/**
 * @brief Frees the memory allocated for the storage engine.
 * @param engine A pointer to the storage engine.
 * @param meta_file The path to the metadata file.
 * @param bloom_file The path to the bloom filter file.
 */
void free_engine(storage_engine * engine, char * meta_file, char * bloom_file);

#endif


