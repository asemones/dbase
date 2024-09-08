#include <stdio.h>
#include "lsm.h"
#include <string.h>
#include <stdio.h>
#include "../../ds/byte_buffer.h"
#include "../../util/stringutil.h"
#include <math.h>
#include "../../util/maths.h"
#include "../../ds/linkl.h"
#include "../../ds/arena.h"
#include "../../ds/frontier.h"
#include "../../util/alloc_util.h"
#include "sst_builder.h"
#include "option.h"
#include "iter.h"
#include "../../ds/threadpool.h"
#ifndef COMPACTOR_H
#define COMPACTOR_H
#define NUM_THREADP 1
#define NUM_COMPACT 10
#define MAX_COMPARE 5
#define MAX_KEY_SIZE 1024
#define NUM_META_DATA 3
#define NUM_THREAD 3
#define MAX_SST_LENGTH 100000000
#define VICTIM_INDEX 1
#define MB 1048576
#define KB 1024
#define GB 1073741824
#define PREV 1
#define END_OF_BLOCK 0x04
#define NUM_PAGE_BUFFERS 50
#define NUM_BIG_BUFFERS 5
#define TEST_MAX_FILE_SIZE 64*KB
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
typedef struct compact_job{
    size_t id;
    enum compact_status status;
    list * new_sst_files; // of type sst_f_inf. A temp holding ground
   // list * bloom_filters; // of type bloom_filter, added from the compactation
    size_t start_level;
    size_t end_level;
    size_t working_files;
    size_t maxiumum_file_size;
    size_t search_level;

} compact_job;
typedef struct compact_manager{
    thread_p * pool [NUM_THREADP]; // just use one pool for now
    struct_pool * compact_pool;
    list ** sst_files;
    struct_pool * arena_pool;
    size_t base_level_size;
    size_t min_compact_ratio;
    struct_pool * page_buffer_pool;
    struct_pool * big_buffer_pool;
    list * bloom_filters;
    cache * c; // of type bloom_filter, FROM META DATA
}compact_manager;

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
void free_cm(compact_manager * manager);
static void integrate_new_tables(compact_manager *cm, compact_job *job, list *indexs) ;
compact_infos* create_ci(size_t page_size){
    compact_infos *ci = (compact_infos*)wrapper_alloc((sizeof(compact_infos)), NULL,NULL);
    ci->buffer = create_buffer(page_size);
    ci->complete = false;
    return ci;
}
compact_job * create_compact_job(){
    compact_job * j = (compact_job*)wrapper_alloc((sizeof(compact_job)), NULL,NULL);
    j->status = COMPACTION_NOT_STARTED;
    j->new_sst_files = List(0, sizeof(sst_f_inf), false);
    return j;
}
void generate_random_filename(char *filename, size_t length) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    size_t charset_size = sizeof(charset) - 1;
    srand(time(NULL));

    for (size_t i = 0; i < length - 21; i++) {
        filename[i] = charset[rand() % charset_size];
    }
    filename[length - 1] = '\0';

    // Add a timestamp to ensure uniqueness
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char timestamp[20];
    strftime(timestamp, 20, "_%Y%m%d%H%M%S", tm_info);
    strcat(filename, timestamp);
}
compact_manager * init_cm(meta_data * meta, cache * c){
    compact_manager * manager = (compact_manager*)wrapper_alloc((sizeof(compact_manager)), NULL,NULL);
    if (manager == NULL) return NULL;
    for (int i = 0;  i < NUM_THREADP ;i++){
        manager->pool[i] = thread_Pool(NUM_THREAD, 0);
    }
    manager->compact_pool = create_pool(NUM_COMPACT*NUM_THREAD*NUM_THREADP);
    for (int i = 0; i < NUM_COMPACT*NUM_THREAD*NUM_THREADP; i++){
        insert_struct(manager->compact_pool, create_ci(GLOB_OPTS.BLOCK_INDEX_SIZE));
    }
    manager->sst_files = meta->sst_files;
    manager->base_level_size = meta->base_level_size;
    manager->min_compact_ratio = meta->min_c_ratio; 
    manager->arena_pool = create_pool(NUM_COMPACT*NUM_THREAD*NUM_THREADP);
    for (int i =0 ; i < NUM_THREAD*NUM_THREADP; i++){
        insert_struct(manager->arena_pool, calloc_arena(1*MB));
    }
    manager->page_buffer_pool = create_pool(100);
    for (int i = 0; i < NUM_PAGE_BUFFERS; i++){
        insert_struct(manager->page_buffer_pool, create_buffer(GLOB_OPTS.BLOCK_INDEX_SIZE));
    }
    manager->big_buffer_pool = create_pool(NUM_BIG_BUFFERS);    
    for (int i = 0; i < NUM_BIG_BUFFERS ; i++)
    {
        insert_struct(manager->big_buffer_pool, create_buffer(MB));
    }   
    manager->c = c;
    if (manager->compact_pool == NULL  || manager->arena_pool == NULL || manager->big_buffer_pool == NULL || manager->page_buffer_pool == NULL ){
        return NULL;
    }
    

    return manager;
}
struct tm parse_timestamp(const char *timestamp) {
    struct tm tm;
    memset(&tm, 0, sizeof(struct tm));
    
    // Assume the timestamp format is "YYYY-MM-DD HH:MM:SS"
    if (strptime(timestamp, "%Y-%m-%d %H:%M:%S", &tm) == NULL) {
        fprintf(stderr, "Failed to parse timestamp: %s\n", timestamp);
        return tm;
    }

    return tm;
}
int compare_time_stamp(const char * time1, const char * time2){
    if (time1 == NULL || time2 == NULL) return 0;
    struct tm tm1 = parse_timestamp(time1);
    struct tm tm2 = parse_timestamp(time2);
    return difftime (mktime(&tm1), mktime(&tm2));
}

static int entry_len(merge_data entry){
    return entry.key->len + sizeof(entry.key->len) + entry.value->len + sizeof(entry.value->len);

}
merge_data discard_same_keys(frontier *pq, sst_iter *its, cache *c, merge_data initial) {
    merge_data best_entry = initial;
    merge_data current = initial;

    while (pq->queue->len > 0) {
        merge_data *next_in_line = peek(pq);
        if (next_in_line == NULL || next_in_line->key == NULL || compare_merge_data(&current, next_in_line) != 0) {
            break;
        }

        dequeue(pq, &current);

        
        merge_data next_entry = next_key_block(&its[(int)current.index], c);
        if (next_entry.key != NULL && strncmp(next_entry.key->entry, TOMB_STONE, next_entry.key->len)!=0) {
            next_entry.index = current.index;
            enqueue(pq, &next_entry);
        }

        
        if (strncmp(current.key->entry, TOMB_STONE, current.key->len) != 0 && compare_time_stamp(its[(int)current.index].file->timestamp, its[(int)best_entry.index].file->timestamp) > 0) {
            best_entry = current;
        }
    }

    return best_entry;
}
void complete_block(sst_f_inf * curr_sst, block_index * current_block, int block_b_count){
    current_block->len =  block_b_count;
    if (current_block->len <3 ) return;
    build_index(curr_sst,current_block, NULL, current_block->num_keys,current_block->offset);
    memset(current_block, 0, sizeof(*current_block));
    *current_block = create_ind_stack(10 *(GLOB_OPTS.BLOCK_INDEX_SIZE/(4*1024)));
}
void reset_block_counters(int *block_b_count, int *sst_b_count, int *num_block_counter){
    *block_b_count = 2;
    *sst_b_count +=2;
    *num_block_counter+=1;
}
int write_blocks_to_file(byte_buffer *dest_buffer, sst_f_inf* curr_sst, int *written_block_pointer, list * entrys, FILE *curr_file) {
    int keys = 0;
    int skipped = 0;
    for (int i = *written_block_pointer; i < curr_sst->block_indexs->len; i++) {
        block_index *ind = at(curr_sst->block_indexs, i);
        int loc = dest_buffer->curr_bytes;
        write_buffer(dest_buffer, (char*)&ind->num_keys, 2);
        keys = dump_list_ele(entrys, &write_db_entry, dest_buffer, ind->num_keys, keys);
        grab_min_b_key(ind, dest_buffer, loc);
        if (is_avx_supported()){
            ind->checksum =  crc32_avx2(buf_ind(dest_buffer, loc), ind->len);
        }
        else{
            ind->checksum = crc32(buf_ind(dest_buffer, loc), ind->len);
        }
        int bytes_to_skip = GLOB_OPTS.BLOCK_INDEX_SIZE - (dest_buffer->curr_bytes - loc);
        skipped += bytes_to_skip;
        dest_buffer->curr_bytes += bytes_to_skip;
        (*written_block_pointer)++;
    }
    fwrite(dest_buffer->buffy, dest_buffer->curr_bytes, 1, curr_file);
    return skipped;
}
void process_sst(byte_buffer *dest_buffer, sst_f_inf *curr_sst, FILE *curr_file, compact_job*job, int sst_b_count) {
    fseek(curr_file, curr_sst->block_start, SEEK_SET);
    reset_buffer(dest_buffer);
    block_index * b_0= at(curr_sst->block_indexs,0);
    memcpy(curr_sst->min, b_0->min_key, 40);
    for (int i = 0; i < curr_sst->block_indexs->len; i++) {
        block_to_stream(dest_buffer, at(curr_sst->block_indexs, i));
    }
    copy_filter(curr_sst->filter, dest_buffer);
    fwrite(dest_buffer->buffy, dest_buffer->curr_bytes, 1, curr_file);
    fclose(curr_file);
    reset_buffer(dest_buffer);
    
    curr_sst->length = sst_b_count;
    grab_time_char(curr_sst->timestamp);
    insert(job->new_sst_files, curr_sst);
    memset(curr_sst, 0, sizeof(*curr_sst));
}
block_index init_block(arena * mem_store, size_t* off_track){
    block_index current_block=  create_ind_stack(10 *(GLOB_OPTS.BLOCK_INDEX_SIZE/(4*1024)));
    current_block.min_key = arena_alloc(mem_store, 40);
    current_block.offset = *off_track;
    *off_track += GLOB_OPTS.BLOCK_INDEX_SIZE;
    return current_block;
}


/*min key copying screwed up*/
int merge_tables(compact_infos** compact, byte_buffer *dest_buffer, compact_job * job, arena *a, cache * c) {
    sst_iter its[20];
    frontier *pq = Frontier(sizeof(merge_data), 0, &compare_merge_data);
    if (pq == NULL) return STRUCT_NOT_MADE;
    list * entrys = List(0, sizeof(merge_data), false);
    if (entrys== NULL) {
        free_front(pq);
        return STRUCT_NOT_MADE;
    }
    for (int i = 0; i < job->working_files; i++) {
        init_sst_iter(&its[i], compact[i]->sst_file);
        cache_entry *ce = retrieve_entry(c, its[i].cursor.index, compact[i]->sst_file->file_name);
        if (ce == NULL){
            return INVALID_DATA;
        }
        its[i].cursor.arr = ce->ar;
        merge_data first_entry = next_key_block(&its[i], c);
        first_entry.index = i;
        enqueue(pq, &first_entry);
    }
    // block counter determines when to write a block. num_block_counter counts the number of blocks, writing them
    // all to file when its equal to the global setting. sst_b_count tracks ssts
    int block_b_count =2;
    int num_block_counter = 0;
    int sst_b_count = 2;
    int written_block_pointer = 0;
   
   
   
    size_t sst_offset_tracker = 0;
    sst_f_inf  curr_sst= create_sst_empty();
    curr_sst.block_start = job->maxiumum_file_size;
    sprintf(curr_sst.file_name, "in_prog_sst_%zu_%zu", job->id + job->new_sst_files->len, job->end_level);

    FILE * curr_file = fopen(curr_sst.file_name, "wb");
    if (curr_file == NULL) return FAILED_OPEN;

    block_index current_block = init_block(curr_sst.mem_store, &sst_offset_tracker);
    
    db_unit last_key;
  
   /*i strongly reccomend not touching this loop, edge case galore thats easy to fuck up*/
    while (pq->queue->len > 0) {
        merge_data current;
        dequeue(pq, &current);

      
        merge_data next_entry = next_key_block(&its[(int)current.index], c);
        if (next_entry.key != NULL) {
            next_entry.index = current.index;
            enqueue(pq, &next_entry);
        }
        merge_data best_entry = discard_same_keys(pq, its, c, current);
        
        if (strcmp (best_entry.key->entry,"key34")==0 ){
            fprintf(stdout, "no\n");
        }
        
        if (block_b_count + entry_len(best_entry) > GLOB_OPTS.BLOCK_INDEX_SIZE){
            complete_block(&curr_sst, &current_block, block_b_count);
            reset_block_counters(&block_b_count, &sst_b_count, &num_block_counter);
            current_block = init_block(curr_sst.mem_store, &sst_offset_tracker); 
            merge_data * this_block_max = get_last(entrys);
            memcpy(curr_sst.max, this_block_max->key->entry, this_block_max->key->len); 
        }
        if (num_block_counter >= GLOB_OPTS.COMPACTOR_WRITE_SIZE  && num_block_counter > 0){
            sst_b_count+= write_blocks_to_file(dest_buffer, &curr_sst, &written_block_pointer, entrys, curr_file);
            reset_buffer(dest_buffer);
            merge_data * last=  get_last(entrys);
            last_key = *last->key;
            entrys->len = 0; // to stop edgecase of max being null
            num_block_counter = 0;
        }
        if (sst_b_count +  entry_len(best_entry)>= GLOB_OPTS.SST_TABLE_SIZE || (block_b_count == 0 && sst_offset_tracker == GLOB_OPTS.SST_TABLE_SIZE)){
            
            complete_block(&curr_sst, &current_block, block_b_count);
            reset_block_counters(&block_b_count, &sst_b_count, &num_block_counter);
            
            write_blocks_to_file(dest_buffer, &curr_sst, &written_block_pointer, entrys, curr_file);
           
            merge_data *  max = get_last(entrys);
            if (max == NULL){
                memcpy(curr_sst.max, last_key.entry, last_key.len);
            }
            else{
                memcpy(curr_sst.max, max->key->entry, max->key->len);
            }
            process_sst(dest_buffer, &curr_sst, curr_file, job, sst_b_count);
            curr_sst = create_sst_empty();
            curr_sst.block_start= job->maxiumum_file_size;
           
            block_b_count = 2;
          
            entrys->len = 0;
            

            num_block_counter = 0;
            sst_b_count = 2;
            sst_offset_tracker = 0;

            sprintf(curr_sst.file_name, "in_prog_sst_%zu_%zu", job->id + job->new_sst_files->len, job->end_level);
            curr_file = fopen(curr_sst.file_name, "wb");
            if (curr_file == NULL) return FAILED_OPEN;
            written_block_pointer =0;
            current_block = init_block(curr_sst.mem_store, &sst_offset_tracker);
        }
        if (best_entry.key!=NULL & best_entry.key->entry != NULL) {
            insert(entrys, &best_entry);
            current_block.num_keys ++;
            map_bit(best_entry.key->entry, current_block.filter);
            map_bit(best_entry.key->entry, curr_sst.filter);
            int best_entry_bytes  = entry_len(best_entry);
            block_b_count +=  best_entry_bytes;
            sst_b_count += best_entry_bytes;
        }
    }
    if (block_b_count > 0){
        complete_block(&curr_sst, &current_block, block_b_count);
        reset_block_counters(&block_b_count, &sst_b_count, &num_block_counter);
    }
  
    if (entrys->len > 0) {
            write_blocks_to_file(dest_buffer, &curr_sst, &written_block_pointer, entrys, curr_file);
            merge_data * max = get_last(entrys);
            memcpy(curr_sst.max, max->key->entry, max->key->len);
            process_sst(dest_buffer, &curr_sst, curr_file, job, sst_b_count);

    }
  

    free_front(pq);
    free_list(entrys, NULL);
    return 0; //INVALID_DATA is 0 so default is 1 
}

int calculate_overlap(char *min1, char *max1, char *min2, char *max2) {  
    if (min1 == NULL || max1 == NULL || min2 == NULL || max2 == NULL) 
        return -1;

    if (strcmp(min1, max2) >= 0) 
        return -1;
    if (strcmp(min2, max1) >= 0) 
        return -1;
    return 0;
}

size_t find_compact_friend(size_t curr_level, size_t start_level, char * min, char * max, list * next_level,list* indexes, size_t victim_index){
    if (next_level == NULL) return -1;
    for (size_t i = 0 ; i < next_level->len; i++){
        if (curr_level == start_level && i == victim_index) continue;
        sst_f_inf * sst = at(next_level, i);
        if (sst == NULL) continue;
        int score = calculate_overlap(min,max,sst->min,sst->max);
        if (score == 0) insert(indexes, (void*)&i); 
    }
    return 0;
}
bool check_compact_condition(compact_manager * cm, size_t level_size ){
    bool io_status = false;
    if (io_status && level_size < (cm->base_level_size * cm->min_compact_ratio)) return 0;
    if (level_size < cm->base_level_size) return 0;
    return 1;
}
static void reset_ci(void * ci){
    compact_infos * c = (compact_infos*)ci;
    reset_buffer(c->buffer);
    c->complete = false;
    memset(c->time_stamp, 0, 20);
    c->sst_file = NULL;
}
/*need a better computation for bounds*/
static void compact_one_table(compact_manager * cm, size_t start_level,  size_t search_level, size_t targ_level, size_t index){
    list *start_lvl = cm->sst_files[start_level];
    list * target_lvl = cm->sst_files[targ_level];
    sst_f_inf * victim = at(cm->sst_files[start_level],index);
    if (victim == NULL) return;
    list * indexs = List(0, sizeof(size_t), false);
    insert(indexs, &index);
   // if (!check_compact_condition(cm, cm->sst_files[level]->len)) return;
    int level_to_check = search_level;
    find_compact_friend(level_to_check,start_level, victim->min, victim->max, cm->sst_files[level_to_check], indexs, index);

    if (indexs->len < 2) {
        free_list(indexs, NULL);
        char file_name [64];
        memcpy(file_name, victim->file_name, strlen(victim->file_name)+1);
        file_name[strlen(file_name)-1] = (const char )48 + targ_level;
        rename(victim->file_name,file_name);
        victim->file_name[strlen(victim->file_name)-1] = (const char )48 + targ_level;
        insert(target_lvl, victim);
        remove_at(start_lvl,index); 

        /*case where the file is just shifting downwards*/
        return;
    }
    compact_infos * compact [10];
    /*figure out whether to deal with sst_f or block_indexs. maybe jsut use the block indexes? easier to chunk operaitosn*/
    compact[0] = request_struct(cm->compact_pool);
    compact[0]->sst_file = victim;
    compact[0]->complete = false;
    for (int i = 1 ; i <indexs->len; i++ ){
        compact[i] = request_struct(cm->compact_pool);
        size_t index=   *(size_t*)at(indexs,i);
        compact[i]->sst_file = at(cm->sst_files[level_to_check],index);
        compact[i]->complete = false; 

    }
    arena * a = request_struct(cm->arena_pool);
    reset_arena(a);
    byte_buffer * dest_buffer = request_struct(cm->big_buffer_pool);
    compact_job * job = create_compact_job();
    job->status = COMPACTION_IN_PROGRESS;
    job->start_level = start_level;
    job->end_level = targ_level;
    job->working_files = indexs->len;
    job->maxiumum_file_size = GLOB_OPTS.SST_TABLE_SIZE; // CHANGE THIS AFTER TESTING
    job->id = index *targ_level  - index ;
    job->search_level = search_level;
    int result = merge_tables (compact, dest_buffer, job,a, cm->c);
    if (result == INVALID_DATA){
        return;
    }
    job->status = COMPACTION_SUCCESS;
    integrate_new_tables(cm, job, indexs);
    free_list(indexs, NULL);
    for (int i = 0; i < job->working_files; i++)
    {
        return_struct(cm->compact_pool, compact[i], &reset_ci);
    }
    return_struct(cm->arena_pool, a, &reset_arena);
    return_struct(cm->big_buffer_pool, dest_buffer, &reset_buffer);
    return;
}

static void remove_and_rename(sst_f_inf *old, sst_f_inf *new, const char level) {
    remove(old->file_name);
    old->file_name[strlen(old->file_name) -1] = level;
    rename(new->file_name, old->file_name);
    memcpy(new->file_name, old->file_name, strlen(old->file_name) + 1);
    //free_sst_inf(old);
}
static void handle_first_element(compact_manager *cm, size_t index,  list * next_level, list * old_level, compact_job* job){
    sst_f_inf *old_first = at(old_level, index);
    sst_f_inf *new_first = at(job->new_sst_files, 0);

    remove_and_rename(old_first, new_first, (const char )48 + job->end_level);
    insert(next_level, new_first);
}
static void remove_changed_ssts(list *old_level, compact_job *job, list * indexes) {
    for (int i = old_level->len+1; i > 0; i--) {
        size_t real_index = i - 1;
        if (!check_in_list(indexes, &real_index, &compare_size_t)) continue;
        sst_f_inf *old = at(old_level, real_index);
        if (old!= NULL && old->mem_store != NULL) free_sst_inf(old);
        old = NULL;
        remove_at(old_level, real_index);
    }
}
static void integrate_new_tables(compact_manager *cm, compact_job *job, list *indexs) {
    list *curr_level = cm->sst_files[job->start_level];
    list * search_level = cm->sst_files[job->search_level];
    list *next_level = List(0, sizeof(sst_f_inf), false);

    //lock_everything();
    handle_first_element(cm,*(size_t*)(at(indexs, 0)),next_level,curr_level, job);
    list *level_to_change = next_level;
    
    for (size_t i = 1; i < indexs->len; i++)
    {
        sst_f_inf *old = at(search_level, *(size_t*)at(indexs, i));
        sst_f_inf *new = at(job->new_sst_files, i);
        if (new == NULL){
            //remove(old->file_name);
            break;
        }

        remove_and_rename(old, new, (const char )48 + job->end_level);
        insert(level_to_change, new);

    }
    for (size_t i= 0; i < search_level->len; i++){
        if (check_in_list(indexs, &i, &compare_size_t)) continue;
        insert(level_to_change, at(search_level, i));
    }
    if (job->start_level != job->search_level) {
       remove_at(curr_level, *(size_t*)(at(indexs, 0)));
    }
    //unlock_everything();
    for (int i = indexs->len; i < job->new_sst_files->len; i++) {
        sst_f_inf *new = at(job->new_sst_files, i);

        char buf[128];
        gen_sst_fname(level_to_change->len, job->end_level, buf);
        rename(new->file_name, buf);
        memcpy(new->file_name, buf, strlen(buf) + 1);

      
        insert(level_to_change, new);   
    }
    remove_changed_ssts(search_level, job, indexs);
    free_list(job->new_sst_files, NULL);

    cm->sst_files[job->end_level] = level_to_change;
    

    return;
}

void free_ci(void * ci){
    compact_infos * c = (compact_infos*)ci;
    if (c == NULL) return;
    if (c->buffer != NULL) free_buffer(c->buffer);
    free(c);
}
void free_cm(compact_manager * manager){
    for (int i = 0; i < NUM_THREADP; i++){
        destroy_pool(manager->pool[i]);
    }
    free_pool(manager->compact_pool, &free_ci);
    free_pool(manager->arena_pool, &free_arena);
    free_pool(manager->page_buffer_pool, &free_buffer);
    free_pool(manager->big_buffer_pool, &free_buffer);
    manager->bloom_filters = NULL;
    free(manager);
}
#endif