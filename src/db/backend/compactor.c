#include "compactor.h"
/*i was very short sighted and created this struct, and frankly i do not want to change my 20+ tests on this
so i have two different "copies", one for the pq and one for the regular system*/
#define NUM_THREADP 1
#define NUM_COMPACT 10
#define MAX_COMPARE 5
#define NUM_META_DATA 3
#define NUM_THREAD 10
#define MAX_SST_LENGTH 100000000
#define VICTIM_INDEX 1
#define MB 1048576
#define KB 1024
#define GB 1073741824
#define PREV 1
#define END_OF_BLOCK 0x04
#define NUM_PAGE_BUFFERS 50
#define NUM_BIG_BUFFERS 5

compact_infos* create_ci(size_t page_size){
    compact_infos *ci = (compact_infos*)wrapper_alloc((sizeof(compact_infos)), NULL,NULL);
    ci->buffer = create_buffer(page_size);
    ci->complete = false;
    return ci;
}

/*sets the cm with the most re*/
void set_cm_sst_files(meta_data * meta, compact_manager * cm){
    for (int i =0; i < MAX_LEVELS; i++){
        if (meta->sst_files[i] == NULL) continue;
        cm->sst_files[i]= thread_safe_list(0, sizeof (sst_f_inf), false);
        for (int j = 0; j < meta->sst_files[i]->len; j++){
            sst_f_inf f;
            sst_f_inf * sst_to_copy=  at(meta->sst_files[i],j);
            int ret =sst_deep_copy(sst_to_copy, &f);
            if (ret !=OK) return;
            insert(cm->sst_files[i], &f);
        }
    }
}
int compare_jobs(const void * job1, const void * job2){
    const compact_job_internal * job_1 = job1;
    const compact_job_internal * job_2 = job2;

    return job_1->id > job_2->id;
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
    //set_cm_sst_files(meta, manager);
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
    manager->job_queue = Frontier(sizeof(compact_job_internal), false, &compare_jobs);
    pthread_mutex_init(&manager->compact_lock, NULL);
    manager->check_meta_cond = false;
    manager->exit = false;
    manager->lvl0_job_running= false;
    pthread_mutex_init(&manager->resource_lock, NULL);
    pthread_cond_init(&manager->resource_sig, NULL);
    return manager;
}

int compare_time_stamp(struct timeval t1, struct timeval t2){
    if (t1.tv_sec < t2.tv_sec) return -1;    
    else if (t1.tv_sec > t2.tv_sec) return 1;

    if (t1.tv_usec < t2.tv_usec) return -1;
    else if (t1.tv_usec > t2.tv_usec) return 1;
    return 0;
}

int entry_len(merge_data entry){
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

        
        if (strncmp(current.key->entry, TOMB_STONE, current.key->len) != 0 && compare_time_stamp(its[(int)current.index].file->time, its[(int)best_entry.index].file->time) > 0) {
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
            ind->checksum =  crc32_avx2((uint8_t*)buf_ind(dest_buffer, loc), ind->len);
        }
        else{
            ind->checksum = crc32((uint8_t*)buf_ind(dest_buffer, loc), ind->len);;
        }
        int bytes_to_skip = GLOB_OPTS.BLOCK_INDEX_SIZE - (dest_buffer->curr_bytes - loc);
        skipped += bytes_to_skip;
        dest_buffer->curr_bytes += bytes_to_skip;
        (*written_block_pointer)++;
    }
    write_wrapper(dest_buffer->buffy, dest_buffer->curr_bytes, 1, curr_file);
    fflush(curr_file);
    fsync(curr_file->_fileno);
    return skipped;
}
void process_sst(byte_buffer *dest_buffer, sst_f_inf *curr_sst, FILE *curr_file, compact_job_internal*job, int sst_b_count) {
    fseek(curr_file, curr_sst->block_start, SEEK_SET);
    reset_buffer(dest_buffer);
    block_index * b_0= at(curr_sst->block_indexs,0);
    memcpy(curr_sst->min, b_0->min_key, 40);
    for (int i = 0; i < curr_sst->block_indexs->len; i++) {
        block_to_stream(dest_buffer, at(curr_sst->block_indexs, i));
    }
    copy_filter(curr_sst->filter, dest_buffer);

    write_wrapper(dest_buffer->buffy, dest_buffer->curr_bytes, 1, curr_file);
    fflush(curr_file);
    fsync(curr_file->_fileno);
    fclose(curr_file);
    reset_buffer(dest_buffer);
    //curr_sst->in_cm_job = true;
    curr_sst->length = sst_b_count;
    gettimeofday(&curr_sst->time, NULL);
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
int merge_tables(byte_buffer *dest_buffer, compact_job_internal * job, arena *a, cache * c) {
    sst_iter  * its = malloc(sizeof(sst_iter)* job->to_merge->len);
    frontier *pq = Frontier(sizeof(merge_data), 0, &compare_merge_data);
    if (pq == NULL) return STRUCT_NOT_MADE;
    list * entrys = List(0, sizeof(merge_data), false);
    if (entrys== NULL) {
        free_front(pq);
        return STRUCT_NOT_MADE;
    }
    for (int i = 0; i < job->to_merge->len; i++) {
        sst_f_inf * sst = at(job->to_merge,i);
        init_sst_iter(&its[i], sst);
        cache_entry *ce = retrieve_entry(c, its[i].cursor.index, sst->file_name);
        if (ce == NULL){
            free(its);
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
    generate_unique_sst_filename(curr_sst.file_name, MAX_F_N_SIZE, job->end_level);
    curr_sst.block_start = GLOB_OPTS.SST_TABLE_SIZE;

    FILE * curr_file = fopen(curr_sst.file_name, "wb");
    if (curr_file == NULL) return FAILED_OPEN;

    block_index current_block = init_block(curr_sst.mem_store, &sst_offset_tracker);
    sst_offset_tracker = 0;
    
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
        if (sst_offset_tracker + entry_len(best_entry) >= GLOB_OPTS.SST_TABLE_SIZE || (block_b_count == 0 && sst_offset_tracker == GLOB_OPTS.SST_TABLE_SIZE)){
            
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
            curr_sst.block_start=  GLOB_OPTS.SST_TABLE_SIZE;
           
            block_b_count = 2;
          
            entrys->len = 0;
            

            num_block_counter = 0;
            sst_b_count = 2;
            sst_offset_tracker = 0;

            generate_unique_sst_filename(curr_sst.file_name, MAX_F_N_SIZE, job->end_level);
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
    free(its);
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

int get_level_size(int level){
    return (GLOB_OPTS.LEVEL_0_SIZE * GLOB_OPTS.SST_TABLE_SIZE_SCALAR) * level;
}
void reset_ci(void * ci){
    compact_infos * c = (compact_infos*)ci;
    reset_buffer(c->buffer);
    c->complete = false;
    memset(c->time_stamp, 0, 20);
    c->sst_file = NULL;
}
/*removes the sst when target is actually the same pointer*/
int remove_same_sst(sst_f_inf * target, list * target_lvl, int level){
    pthread_mutex_lock(&target_lvl->write_lock);
    char * key_to_check = target->file_name;
    int ind;
    /*level 0 has no gurantee of ordering*/
    
    ind = find_sst_file_eq_iter(target_lvl, target_lvl->len, key_to_check);
    

    
    if (ind < 0) {
        pthread_mutex_unlock(&target_lvl->write_lock);
        return ind;
    }
    sst_f_inf * candiate = internal_at_unlocked(target_lvl, ind);
    if (!sst_equals(candiate, target)){
        pthread_mutex_unlock(&target_lvl->write_lock);
        return -1;
    } /*we compare the block index lists because that is a pointer. since we are dealing with structs, we cannot just compare the structs, as they may differ */
    fprintf(stdout, "removed %s in a trival move\n", candiate->file_name);
    memset(candiate, 0, sizeof(*candiate));
    remove_at_unlocked(target_lvl,  ind);
    pthread_mutex_unlock(&target_lvl->write_lock);
    return 0;
}

int remove_sst_from_list(sst_f_inf * target, list * target_lvl, int level){
    pthread_mutex_lock(&target_lvl->write_lock);
    char * key_to_check = target->file_name;
    if (key_to_check == NULL) return -1;
    int ind;
    /*level 0 has no gurantee of ordering*/
    
  
    ind = find_sst_file_eq_iter(target_lvl, target_lvl->len, key_to_check);
  
    if (ind < 0) {
        pthread_mutex_unlock(&target_lvl->write_lock);
        return ind;
    }
    sst_f_inf * candiate = internal_at_unlocked(target_lvl, ind);
    if (!sst_equals(candiate, target)){
        pthread_mutex_unlock(&target_lvl->write_lock);
        return -1;
    } /*we compare the block index lists because that is a pointer. since we are dealing with structs, we cannot just compare the structs, as they may differ */
    if (!candiate->marked) remove(candiate->file_name);
    fprintf(stdout, "removed %s\n", candiate->file_name);
    free_sst_inf(candiate);
    remove_at_unlocked(target_lvl,  ind);
    pthread_mutex_unlock(&target_lvl->write_lock);
    return 0;
}

void compact_one_table(compact_manager * cm, compact_job_internal job,  sst_f_inf * victim){
    if (victim == NULL) return;
   
    list * ssts_to_merge = job.to_merge;
    
    /*trival move*/
    if (ssts_to_merge->len <= 1 && job.start_level != 0) {
        char buf[64];
        int res  = remove_same_sst(victim, cm->sst_files[job.start_level], job.start_level);
        if (res >= 0){
            res = 1;
        }
        else {
            fprintf(stdout,"failed to remove %s\n", victim->file_name);
            res = -1;
        }
        fprintf(stdout, "%d removed from 1\n", res);
        generate_unique_sst_filename(buf, 64, job.end_level);
        rename(victim->file_name, buf);
        memset(victim->file_name, 0, strlen(victim->file_name) + 1);
        memcpy(victim->file_name, buf, strlen(buf)+ 1);
        merge_lists(cm->sst_files[job.end_level], job.to_merge, &cmp_sst_f_inf);
        return;

    }
   
    pthread_mutex_lock(&cm->resource_lock);
    while (cm->arena_pool->size <=0 || cm->big_buffer_pool->size <=0){
        pthread_cond_wait(&cm->resource_sig, &cm->resource_lock);
    }
    arena * a = request_struct(cm->arena_pool);
    reset_arena(a);
    byte_buffer * dest_buffer = request_struct(cm->big_buffer_pool);
    pthread_mutex_unlock(&cm->resource_lock);
    int result = merge_tables (dest_buffer, &job,a, cm->c);
    if (result == INVALID_DATA){
        return_struct(cm->arena_pool, a, &reset_arena);
        return_struct(cm->big_buffer_pool, dest_buffer, &reset_buffer);
        pthread_cond_signal(&cm->resource_sig);
        if(job.start_level == 0){
            cm->lvl0_job_running = false;
        }
      
        return;
    }
    integrate_new_tables(cm, &job, ssts_to_merge);
    free_list(ssts_to_merge, NULL);

    pthread_mutex_lock(&cm->resource_lock);
    return_struct(cm->arena_pool, a, &reset_arena);
    return_struct(cm->big_buffer_pool, dest_buffer, &reset_buffer);
    pthread_cond_signal(&cm->resource_sig);
    pthread_mutex_unlock(&cm->resource_lock);
    if(job.start_level == 0){
        cm->lvl0_job_running = false;
    }
    return;
}
void integrate_new_tables(compact_manager *cm, compact_job_internal *job, list *old_ssts) {
    pthread_mutex_lock(&cm->compact_lock);

    list *start_level = cm->sst_files[job->start_level];
    list *search_level = cm->sst_files[job->search_level];
    list * end_level= cm->sst_files[job->end_level];

    const int victim_index= 0;
    /*need a better system for random file names*/
    /*remove all ssts from the search level data structure. edgecase where start != search IS covered at the first call*/
    int yoareuseless=  0;
    if ((remove_sst_from_list(at(old_ssts,victim_index), start_level, job->start_level)) >= 0)yoareuseless++;
    for (int i = 0; i < old_ssts->len ;i++){
         int res = remove_sst_from_list(at(old_ssts, i), search_level, job->search_level);
         if (res >= 0 ) yoareuseless ++;
    }
    /*case where multiple level 0 ssts were selected.*/
    for (int i = 0; i < old_ssts->len ; i++){
        int res = remove_sst_from_list(at(old_ssts, i), cm->sst_files[0],0);
        if (res>= 0 ) yoareuseless ++;

    }
    //job->new_sst_files are in order, so we can just do a 2 way merge with the end level*/
    fprintf(stdout, "%d removed from %d\n", yoareuseless, old_ssts->len);
    merge_lists(end_level, job->new_sst_files ,&cmp_sst_f_inf);

    free_list(job->new_sst_files, NULL);
    
    pthread_mutex_unlock(&cm->compact_lock);
    return;
}
void free_ci(void * ci){
    compact_infos * c = (compact_infos*)ci;
    if (c == NULL) return;
    if (c->buffer != NULL) free_buffer(c->buffer);
    free(c);
}
void shutdown_cm(compact_manager * cm){
    cm->exit = true;
    cm->check_meta_cond = true;
    pthread_cond_signal(cm->wait);
}
void free_cm(compact_manager * manager){
    for (int i = 0; i < NUM_THREADP; i++){
        destroy_pool(manager->pool[i]);
    }
    free_pool(manager->compact_pool, &free_ci);
    free_pool(manager->arena_pool, &free_arena);
    free_pool(manager->page_buffer_pool, &free_buffer);
    free_pool(manager->big_buffer_pool, &free_buffer);
    free_front(manager->job_queue);
    pthread_mutex_destroy(&manager->compact_lock);
    pthread_mutex_destroy(&manager->resource_lock);
    pthread_cond_destroy(&manager->resource_sig);
    free(manager);
    manager = NULL;
}