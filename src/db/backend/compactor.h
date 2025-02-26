#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../../ds/byte_buffer.h"
#include "../../util/stringutil.h"
#include <math.h>
#include "../../util/maths.h"
#include "../../ds/arena.h"
#include "../../ds/frontier.h"
#include "../../util/alloc_util.h"
#include "option.h"
#include "iter.h"
#include "../../ds/threadpool.h"
#include <time.h>
#include <unistd.h>
#ifndef COMPACTOR_H
#define COMPACTOR_H
#define NUM_THREADP 1
typedef struct compact_infos{
    char time_stamp[20];
    byte_buffer * buffer;
    sst_f_inf * sst_file;
    bool complete;
}compact_infos;
enum compact_status{
    COMPACTION_SUCCESS,
    COMPACTION_FAILED,
    COMPACTION_IN_PROGRESS,
    COMPACTION_NOT_STARTED,
};
/*i was very short sighted and created this struct, and frankly i do not want to change my 20+ tests on this
so i have two different "copies", one for the pq and one for the regular system*/
//compaction algorithm:
/*
leveled compaction with the goal of cutting down on high latency spikes: We can assume that b bytes of io bandwith are assigned to the dbms
by the user. This assumption allows us to create a scheduler that dynamically allocates io bandwith. From this, we can
take advantage of io bandwith when it is avaliable, avoiding latency spikes from compaction triggers lining up. Instead, 
we can use this scheduler to preform single-file compactations when there is "free" bandwith.
We can split all io operations into three priority levels, each one managed by thread(s). The first level
is for any memtable flushes and WAL. The second level is for the "shallow" level compactations (l_0, l_1)
and the third level for deeper level compactions. To prevent l_0 being filled with a bunch of tiny ssts, second level
compactions will always occur even if memtable flush rate is high. In such events, the bandwith will be assigned in a way
to maximize overall throughput to moving ssts from memory to l_1.

General compactation process:
1. Check if bandwith is avaliable for compaction SKIP FOR BASE
2. If bandwith is avaliable, check if a compactation is beneficial. This can be set as a "miniumum compactation size"
   . This is different from the maxiumum size of a sst level. If too small, return. SKIP FOR BASE
3. Then check the size of the next level. If it has room, continue. If not, return and call compact on next level down  
4  If a compactation is needed, find the sst files to merge by targeting files with overlapping key-ranges. 
   . This can be done by checking the metadata of each sst file. 
5. Then preform the compactation in the following manner:
    a. Load the sst files into memory
    b. Merge the sst files
    c. Write sst to a new file
    d. Update the metadata and bloom filter
    e. Delete the old sst files
6. Repeat 1-5 until level is empty
*/
/*More in depth algorithm:
Every x seconds run a basic compactation check on the compact thread. 
If needed utilize a thread from the threadpool to complete the job
*/

/*
*/
typedef struct compact_job_internal{
    size_t id;
    size_t index;
    size_t start_level;
    size_t end_level;
    size_t working_files;
    size_t search_level;
    sst_f_inf * target;
    list * to_merge;
    list * new_sst_files;
} compact_job_internal;

typedef struct compact_manager{
    thread_p * pool [NUM_THREADP]; // just use one pool for now
    struct_pool * compact_pool;
    list ** sst_files;
    struct_pool * arena_pool;
    size_t base_level_size;
    pthread_mutex_t * wait_mtx;
    pthread_cond_t * wait;
    pthread_mutex_t compact_lock;
    pthread_mutex_t  resource_lock;
    pthread_cond_t resource_sig;
    size_t min_compact_ratio;
    struct_pool * page_buffer_pool;
    struct_pool * big_buffer_pool;
    frontier * job_queue;
    bool check_meta_cond;
    bool lvl0_job_running;
    bool exit;
    cache * c; // of type bloom_filter, FROM META DATA
}compact_manager;
void free_cm(compact_manager * manager);

compact_infos* create_ci(size_t page_size);
void set_cm_sst_files(meta_data * meta, compact_manager * cm);
int compare_jobs(const void * job1, const void * job2);
compact_manager * init_cm(meta_data * meta, cache * c);
int compare_time_stamp(struct timeval t1, struct timeval t2);
int entry_len(merge_data entry);
merge_data discard_same_keys(frontier *pq, sst_iter *its, cache *c, merge_data initial);
void complete_block(sst_f_inf * curr_sst, block_index * current_block, int block_b_count);
void reset_block_counters(int *block_b_count, int *sst_b_count, int *num_block_counter);
int write_blocks_to_file(byte_buffer *dest_buffer, sst_f_inf* curr_sst, int *written_block_pointer, list * entrys, FILE *curr_file);
void process_sst(byte_buffer *dest_buffer, sst_f_inf *curr_sst, FILE *curr_file, compact_job_internal*job, int sst_b_count);
block_index init_block(arena * mem_store, size_t* off_track);
int merge_tables(byte_buffer *dest_buffer, compact_job_internal * job, arena *a, cache * c);
int calculate_overlap(char *min1, char *max1, char *min2, char *max2);
int get_level_size(int level);
void reset_ci(void * ci);
int remove_same_sst(sst_f_inf * target, list * target_lvl, int level);
int remove_sst_from_list(sst_f_inf * target, list * target_lvl, int level);
void compact_one_table(compact_manager * cm, compact_job_internal job, sst_f_inf * victim);
void integrate_new_tables(compact_manager *cm, compact_job_internal *job, list *old_ssts); 
void free_ci(void * ci);
void shutdown_cm(compact_manager * cm);
void free_cm(compact_manager * manager);

#endif