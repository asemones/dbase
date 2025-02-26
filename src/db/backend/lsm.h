
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
typedef struct mem_table{
    size_t bytes;
    bloom_filter *filter;
    size_t num_pair;
    bool immutable;
    char *range[2];
    SkipList *skip;
}mem_table;

typedef struct storage_engine{
    mem_table * table[LOCKED_TABLE_LIST_LENGTH];
    meta_data * meta;
    size_t num_table;
    struct_pool * write_pool;
    cache * cach;
    WAL * w;
    pthread_cond_t * compactor_wait;
    pthread_mutex_t * compactor_wait_mtx;
    bool * cm_ref;
    int error_code;
}storage_engine;
void lock_table(storage_engine * engine);

mem_table * create_table(void);
int restore_state(storage_engine * e, int lost_tables);
void clear_table(mem_table * table);
storage_engine * create_engine(char * file, char * bloom_file);
int write_record(storage_engine * engine, db_unit key, db_unit value);
size_t find_sst_file(list * sst_files, size_t num_files, const char * key);
size_t find_block(sst_f_inf * sst, const char * key);
char * disk_read(storage_engine * engine, const char * keyword);
char * disk_read_snap(snapshot * snap, const char * keyword);
char* read_record(storage_engine * engine, const char * keyword);
char* read_record_snap(storage_engine * engine, const char * keyword, snapshot * s);
void seralize_table(SkipList * list, byte_buffer * buffer, sst_f_inf * s);
void lock_table(storage_engine * engine);
int flush_table(storage_engine * engine);
void free_one_table(mem_table * table);
void dump_tables(storage_engine * engine);
void free_tables(storage_engine * engine);
void free_engine(storage_engine * engine, char * meta_file, char * bloom_file);

#endif


