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
    list * bloom_filters; // of type bloom_filter, FROM META DATA
}compact_manager;
typedef struct comp_data{
    db_unit key;
    db_unit value;
    char index;
}comp_data;
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
compact_manager * init_cm(meta_data * meta){
    compact_manager * manager = (compact_manager*)wrapper_alloc((sizeof(compact_manager)), NULL,NULL);
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
    

    return manager;
}
int cmp_comp_data(comp_data one, comp_data two, int max, int dt){
    return master_comparison(&one.key, &two.key, max, dt);
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
static size_t write_entry(char * next_key, char * next_value, byte_buffer * dest_buffer){
    const size_t num_of_single_characers = 6;
    const size_t entry_len = strlen(next_key) + strlen(next_value) + num_of_single_characers;
    write_buffer(dest_buffer, "\"", 1);
    write_buffer(dest_buffer, next_key, strlen(next_key));
    write_buffer(dest_buffer, "\"", 1);
    write_buffer(dest_buffer, ":", 1);
    write_buffer(dest_buffer, "\"", 1);
    write_buffer(dest_buffer,next_value, strlen(next_value));
    write_buffer(dest_buffer, "\"", 1);
    write_buffer(dest_buffer, ",", 1);
    return entry_len;
}
static int handle_same_key(kv *prev, kv *current, compact_infos * preve, compact_infos * curr, byte_buffer *dest_buffer) {
    string key = stack_str(current->key);
    string value = stack_str(current->value);
    string prev_value = stack_str(prev->value);

    size_t bytes_changed = 0;

   
    size_t old_read_ptr = dest_buffer->read_pointer;

    
    dest_buffer->read_pointer = dest_buffer->curr_bytes - 1;
    dest_buffer->read_pointer = back_track(dest_buffer, ',', dest_buffer->curr_bytes - 2, dest_buffer->max_bytes);
    char temp[100];
    get_next_key(dest_buffer, temp);
    dest_buffer->read_pointer = old_read_ptr;

 
    if (strcmp(temp, key.data) == 0) {
        size_t bytes_to_remove = strlen(key.data) + strlen(prev_value.data) + 6;  
        dest_buffer->curr_bytes -= bytes_to_remove;
        bytes_changed -= bytes_to_remove;  
    }

    int recent = compare_time_stamp(preve->time_stamp, curr->time_stamp);

    if (recent > 0 && string_cmp(prev_value, stack_str(TOMB_STONE)) != 0) {
        bytes_changed += write_entry(key.data, prev_value.data, dest_buffer);
    } 
    else if (recent <=0 && string_cmp(value, stack_str(TOMB_STONE)) != 0) {
        bytes_changed += write_entry(key.data, value.data, dest_buffer);
    }

    return bytes_changed;
}


size_t load_into_ll(ll* l, void(*func)(void*, void*, void*), byte_buffer * buffer){
    l->iter = l->head;
    size_t ret_check = 0;
    load_block_into_into_ds(buffer, l,&push_ll_void);
    return 0;
}
size_t refill_table(compact_infos * completed_table, ll * list, size_t next_block_index, byte_buffer * use){
    FILE * file = fopen(completed_table->sst_file->file_name,"rb");
    block_index * index = get_element(completed_table->sst_file->block_indexs, next_block_index);
    if (index == NULL) {
        completed_table->complete = true;
        return 0;
    }
    fseek(file,index->offset, SEEK_SET);
    
    size_t read = fread(use->buffy, 1, index->len, file);
    use->curr_bytes = read;
    if (read == 0) {
        completed_table->complete = true;
        return 0;
    }
    fclose(file);
    load_into_ll(list, &push_ll_void,use);
    return use->curr_bytes;
}
static int entry_len(comp_data one){
    return one.key.len + 2 + one.value.len + 2;

}
/*this fucking sucks, build the indexes properly i know its some dumb cunt shit. Strongly consider rewriting the 
god awful merge function*/
size_t write_block_to_file(FILE *file, sst_f_inf *new_sst, byte_buffer *dest_buffer, size_t offset,size_t num_entries) {
    block_index * b = get_last(new_sst->block_indexs);
            size_t bytes_to_skip = GLOB_OPTS.BLOCK_INDEX_SIZE - sum;
            memcpy(&dest_buffer->buffy[offset],&num_entries,2);
            b.len = sum;
            build_index(s, &b, buffer, num_entries, num_entry_loc);
            buffer->curr_bytes +=bytes_to_skip;
            num_entry_loc = buffer->curr_bytes;
            buffer->curr_bytes +=2; //for the next num_entry_loc;
            num_entries = 0;
            sum = 2;
    return ret;
}

sst_f_inf *create_new_sst_file(compact_job *job, size_t *num_output_table, size_t *curr_file_index, byte_buffer *dest_buffer, char files[][128], FILE **file) {
    char file_name[128];
    sprintf(file_name, "in_prog_sst_%zu_%zu", job->id + *num_output_table, job->end_level);
    memcpy(files[*curr_file_index], file_name, strlen(file_name) + 1);
    (*curr_file_index)++;
    *file = fopen(file_name, "wb");

    sst_f_inf *new_sst = create_sst_file_info(file_name, 0, NUM_WORD, NULL);
    new_sst->block_start = job->maxiumum_file_size;
    insert(job->new_sst_files, new_sst);

    reset_buffer(dest_buffer);

    return new_sst;
}



void merge_tables(compact_infos** compact, byte_buffer *dest_buffer, compact_job * job, arena *a) {
    byte_buffer *storage[10];
    byte_buffer * use_me[10];
    ll *table_lists[10];
    frontier *pq = Frontier(sizeof(comp_data), 0, &master_comparison);
    size_t prev_index = 0;
    size_t curr_index = 0;
    size_t block_indexs[10] = {0};
    size_t num_sst = job->working_files;
    size_t big_byte_counter =0;
    char files [10][128];
    size_t curr_file_index = 0;
    comp_data prev;
    char file_name[128]; /*1111*/
    size_t num_output_table = 0;
    sprintf(file_name, "in_prog_sst_%zu_%zu", job->id,job->end_level);
    FILE * file = fopen(file_name, "wb");
    sst_f_inf *  new_sst = create_sst_file_info(file_name, 0, NUM_WORD, NULL);
    insert(job->new_sst_files, new_sst);
    size_t num_blocks = 0;
    size_t dt = 0;
    size_t offset = 0;

    for (int i = 0; i < num_sst; i++) {
        table_lists[i] = create_ll(a);
        table_lists[i]->iter = table_lists[i]->head;
        storage[i] = create_buffer(compact[i]->buffer->max_bytes);
        use_me[i] = compact[i]->buffer;
    }

    for (int i = 0; i < num_sst; i++) {
        ll_node *head = table_lists[i]->head;
        refill_table(compact[i], table_lists[i], block_indexs[i], use_me[i]);
        if (head != NULL) {
            enqueue(pq, head->data);
            table_lists[i]->head = table_lists[i]->head->next;
           
         
        }
        memcpy(compact[i]->time_stamp, compact[i]->sst_file->timestamp, strlen(compact[i]->sst_file->timestamp));
        block_indexs[i]++;
    }
    size_t num_bytes = 0;
    
    while (pq->queue->len > 0) {
        comp_data smallest;
        
        dequeue(pq, &smallest);
        //debug_print(pq);
        //final value can get wiped
        if (strcmp(smallest.value.entry, "second_value1111") == 0){
            fprintf(stdout, "no\n");
        }
        if (strncmp(smallest.value.entry, TOMB_STONE,1) == 0) {
            continue;
        }
        int bytes = -1;
        if (table_lists[smallest.index]->head== NULL || (table_lists[smallest.index]->head->data == NULL 
        && !compact[smallest.index]->complete)){       
                int i = smallest.index;
                use_me[i] = (compact[i]->buffer == use_me[i])? storage[i]:compact[i]->buffer;
                reset_buffer(use_me[i]);
                bytes = refill_table ( compact[i], table_lists[i], block_indexs[i], use_me[i]);
                block_indexs[i]++;

        }
        if (table_lists[smallest.index]->head->data != NULL){
            comp_data * data =  table_lists[smallest.index]->head->data;
            data->index = smallest.index;
            enqueue(pq, table_lists[data->index]->head->data);
        }
        
        if (master_comparison(&smallest.key,&prev.key, 0,dt ) == 0) {
            int l = handle_same_key(&prev, &smallest,compact[prev_index], compact[curr_index],dest_buffer);
            num_bytes += l;
            big_byte_counter += l;
            prev = smallest;
            prev_index = curr_index;
            continue;
        }
        if (entry_len(smallest)+ big_byte_counter >= job->maxiumum_file_size) {
            write_block_to_file(file, new_sst, dest_buffer,offset, &num_blocks, GLOB_OPTS.BLOCK_INDEX_SIZE);
            offset = 0;
            sst_f_inf * temp = get_element(job->new_sst_files, num_output_table);
            memcpy(temp->max, prev.key.entry,prev.key.len);
            reset_buffer(dest_buffer);
            for (int i = 0; i < num_blocks; i++){
                block_index * block = (block_index*)get_element(new_sst->block_indexs, i);
                block_to_stream(dest_buffer, block);
            }
            copy_filter(new_sst->filter, dest_buffer);
            fseek(file, job->maxiumum_file_size+10, SEEK_SET);
            fwrite(dest_buffer->buffy, 1, dest_buffer->curr_bytes+1, file);
        
            temp->block_start= job->maxiumum_file_size+10;
            temp->length = big_byte_counter;
            fclose(file);
            num_output_table++;
            new_sst = create_new_sst_file(job, &num_output_table, &curr_file_index, dest_buffer, files, &file);
            big_byte_counter = 0;
            num_blocks =0;

        }
        if (num_bytes + entry_len(smallest) >=  GLOB_OPTS.BLOCK_INDEX_SIZE){
            write_block_to_file(file, new_sst, dest_buffer, offset, &num_blocks, GLOB_OPTS.BLOCK_INDEX_SIZE);
            offset = dest_buffer->read_pointer + num_blocks;
            num_bytes = 0; 
            big_byte_counter +=1;
        }
        if (big_byte_counter == 0){
            sst_f_inf * temp =get_element(job->new_sst_files, num_output_table);
            memcpy(temp->min, smallest.key.entry, sizeof(smallest.key.entry));
        }
        int new_bytes = write_db_unit(dest_buffer, smallest.key) + write_db_unit(dest_buffer, smallest.value);
        prev = smallest;
        map_bit(smallest.key.entry,new_sst->filter);
        num_bytes+= new_bytes;
        big_byte_counter += new_bytes;
        prev = smallest;
       //free(smallest);
        prev_index = curr_index;
    }
    if (big_byte_counter !=0){
        write_block_to_file(file, new_sst, dest_buffer, offset, &num_blocks, GLOB_OPTS.BLOCK_INDEX_SIZE);
        sst_f_inf * new_sst = get_element(job->new_sst_files, num_output_table);
        new_sst->block_start= job->maxiumum_file_size+10;
        new_sst->length = big_byte_counter;
        memcpy(new_sst->max, prev.key, sizeof(prev.key));
        reset_buffer(dest_buffer);
        inset_at(job->new_sst_files, new_sst, num_output_table);
        fseek(file, job->maxiumum_file_size+10, SEEK_SET);
        for (int i = 0; i < num_blocks; i++){
            block_index * block = (block_index*)get_element(new_sst->block_indexs, i);
            block_to_stream(dest_buffer, block);
        }
        copy_filter(new_sst->filter, dest_buffer);
        fwrite(dest_buffer->buffy, 1, dest_buffer->curr_bytes+1, file);
        fclose(file);
    }
    for (int i = 0; i < num_sst; i++) {
        free_ll(table_lists[i], NULL);
        free_buffer(storage[i]);
    }
    free_front(pq);
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
        sst_f_inf * sst = get_element(next_level, i);
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
    sst_f_inf * victim = get_element(cm->sst_files[start_level],index);
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
        size_t index=   *(size_t*)get_element(indexs,i);
        compact[i]->sst_file = get_element(cm->sst_files[level_to_check],index);
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
    merge_tables (compact, dest_buffer, job,a);
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
    sst_f_inf *old_first = get_element(old_level, index);
    sst_f_inf *new_first = get_element(job->new_sst_files, 0);

    remove_and_rename(old_first, new_first, (const char )48 + job->end_level);
    insert(next_level, new_first);
}
static void remove_changed_ssts(list *old_level, compact_job *job, list * indexes) {
    for (int i = old_level->len+1; i > 0; i--) {
        size_t real_index = i - 1;
        if (!check_in_list(indexes, &real_index, &compare_size_t)) continue;
        sst_f_inf *old = get_element(old_level, real_index);
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
    handle_first_element(cm,*(size_t*)(get_element(indexs, 0)),next_level,curr_level, job);
    list *level_to_change = next_level;
    
    for (size_t i = 1; i < indexs->len; i++)
    {
        sst_f_inf *old = get_element(search_level, *(size_t*)get_element(indexs, i));
        sst_f_inf *new = get_element(job->new_sst_files, i);
        if (new == NULL){
            //remove(old->file_name);
            break;
        }

        remove_and_rename(old, new, (const char )48 + job->end_level);
        insert(level_to_change, new);

    }
    for (size_t i= 0; i < search_level->len; i++){
        if (check_in_list(indexs, &i, &compare_size_t)) continue;
        insert(level_to_change, get_element(search_level, i));
    }
    if (job->start_level != job->search_level) {
       remove_at(curr_level, *(size_t*)(get_element(indexs, 0)));
    }
    //unlock_everything();
    for (int i = indexs->len; i < job->new_sst_files->len; i++) {
        sst_f_inf *new = get_element(job->new_sst_files, i);

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