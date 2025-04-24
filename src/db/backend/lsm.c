#include "lsm.h"
#define MEMTABLE_SIZE 80000
#define TOMB_STONE "-"
#define LOCKED_TABLE_LIST_LENGTH 2
#define PREV_TABLE 1
#define READER_BUFFS 10
#define WRITER_BUFFS 10
#define LVL_0 0
#define MAXIUMUM_KEY 0
#define MINIMUM_KEY 1
#define KB 1024
#define TEST_LRU_CAP 1024*40
#define NUM_WORD 2000
mem_table * create_table(){
    mem_table * table = (mem_table*)wrapper_alloc((sizeof(mem_table)), NULL,NULL);
    if (table == NULL) return NULL;
    table->bytes = 0;
    table->filter = bloom(NUM_HASH,NUM_WORD,false, NULL);
    if (table->filter == NULL){
        free(table);
        return NULL;
    } 
    table->immutable = false;
    table->num_pair = 0;
    table->skip = create_skiplist(&compareString);
    if (table->skip == NULL) {
        free_filter(table->filter);
        free(table);
        return NULL;
    }
    for(int i = 0 ; i < 2; i++){
        table->range[i] = NULL;
    }
    return table;
}

int restore_state(storage_engine * e ,int lost_tables){
    WAL* w = e->w;
}
void clear_table(mem_table * table){
    if (table->skip != NULL) freeSkipList(table->skip);
    if (table->num_pair <= 0  && table->filter!=NULL) free_filter(table->filter);
    table->skip = create_skiplist(&compareString);
    table->bytes = 0;
    table->filter = bloom(NUM_HASH,NUM_WORD,false, NULL);
    table->num_pair = 0;
    table->immutable = false;
    for(int i = 0 ; i < 2; i++){
        table->range[i] = NULL;
    }
}
storage_engine * create_engine(char * file, char * bloom_file){
    init_crc32_table();
    storage_engine * engine = (storage_engine*)wrapper_alloc((sizeof(storage_engine)), NULL,NULL);
    // Assume engine allocation succeeded before this function call
    set_debug_defaults(&GLOB_OPTS);

    // Initialize queues (assuming success)
    engine->ready_queue = memtable_queue_t_create(GLOB_OPTS.num_memtable);
    engine->flush_queue = memtable_queue_t_create(GLOB_OPTS.num_memtable);

    // Initialize active table (assuming success)
    engine->active_table = create_table();
    engine->num_table = GLOB_OPTS.num_memtable; // Total number of tables

    // Create spare tables and add them to the ready queue (assuming success)
    for (int i = 1; i < GLOB_OPTS.num_memtable; i++) { // Start from 1 as active_table is the first
        mem_table* spare_table = create_table();
        memtable_queue_t_enqueue(engine->ready_queue, spare_table);
    }
    engine->meta = load_meta_data(file, bloom_file);
    if (engine->meta == NULL) return NULL;
    engine->write_pool = create_pool(WRITER_BUFFS);
    if (engine->write_pool == NULL) return NULL;
    engine->cach= create_shard_controller(GLOB_OPTS.num_cache,GLOB_OPTS.LRU_CACHE_SIZE, GLOB_OPTS.BLOCK_INDEX_SIZE);
    for(int i = 0; i < WRITER_BUFFS; i++){
        byte_buffer * buffer = create_buffer( GLOB_OPTS.MEM_TABLE_SIZE*1.5);
        if (buffer == NULL) return NULL;
        insert_struct(engine->write_pool,buffer);
    }
    byte_buffer * b=  request_struct(engine->write_pool);
    engine->w = init_WAL(b);
    if (engine->w == NULL) return NULL;
    if (engine->meta->shutdown_status !=0){
        restore_state(engine,1); /*change 1 to a variable when implementing error system. also remeber
        the stray skipbufs inside this function skipbuf*/
    }
    engine->error_code = OK;
    engine->cm_ref = NULL;
    return engine;
}
int write_record(storage_engine* engine ,db_unit key, db_unit value){
    if (key.entry== NULL || value.entry == NULL ) return -1;
    int success = write_WAL(engine->w, key, value);
    if (success  == FAILED_TRANSCATION) {
        fprintf(stdout, "WARNING: transcantion failure \n");
        return FAILED_TRANSCATION;
    }
   
    mem_table * table = engine->active_table;

    // Check if the active table needs to be rotated
    if (table->bytes + 4 + key.len + value.len >= GLOB_OPTS.MEM_TABLE_SIZE) {
        table->immutable = true;
        memtable_queue_t_enqueue(engine->flush_queue, table); // Add full table to flush queue

        // Try to get a new table from the ready queue
        mem_table* next_table = NULL;
        while (!memtable_queue_t_dequeue(engine->ready_queue, &next_table)) {
             aco_yield();
        }
        engine->active_table = next_table; 
        table = engine->active_table;      
        // Check if the flush queue has reached the threshold to trigger flushing
        if (engine->flush_queue->size >= GLOB_OPTS.num_to_flush) {
             flush_all_tables(engine);
        }
    }
    insert_list(table->skip, key, value);
    table->num_pair++;
    table->bytes += 4 + key.len + value.len;
    map_bit(key.entry, table->filter);
    
    return 0;
}
size_t find_sst_file(list  *sst_files, size_t num_files, const char *key) {
    size_t max_index = num_files;
    size_t min_index = 0;
    size_t middle_index;

    while (min_index < max_index) {
        middle_index = min_index + (max_index - min_index) / 2;
        sst_f_inf *sst = at(sst_files, middle_index);
        if (strcmp(key, sst->min) >= 0 && strcmp(key, sst->max) <= 0) {
            return middle_index;
        } 
        else if (strcmp(key, sst->max) > 0) {
            min_index = middle_index + 1;
        } 
        else {
            
            max_index = middle_index;
        }
    }
    return 0;
}

size_t find_block(sst_f_inf *sst, const char *key) {
    int left = 0;
    int right = sst->block_indexs->len - 1;
    int mid = 0;
    while (left <= right) {
        mid = left + (right - left) / 2;
        block_index *index = at(sst->block_indexs, mid);
        block_index *next_index = at(sst->block_indexs, mid + 1);

        if (next_index == NULL || (strcmp(key, index->min_key) >= 0 && strcmp(key, next_index->min_key) < 0)) {
            return mid;
        }

        if (strcmp(key, index->min_key) < 0) {
            right = mid - 1;
        } 
        else {
            left = mid + 1;
        }
    }

    return mid;
}
/* this is our read path. This read path */

char * disk_read(storage_engine * engine, const char * keyword){ 
    
    for (int i= 0; i < MAX_LEVELS; i++){
        if (engine->meta->sst_files[i] == NULL || engine->meta->sst_files[i]->len ==0) continue;

        list * sst_files_for_x = engine->meta->sst_files[i];
        size_t index_sst = find_sst_file(sst_files_for_x, sst_files_for_x->len, keyword);
        if (index_sst == -1) continue;
        
        sst_f_inf * sst = at(sst_files_for_x, index_sst);
       
        bloom_filter * filter=  sst->filter;
        if (!check_bit(keyword,filter)) continue;
       
        size_t index_block= find_block(sst, keyword);
        block_index * index = at(sst->block_indexs, index_block);
        
        cache_entry c = retrieve_entry_sharded(engine->cach, index, sst->file_name, sst);
        if (c.buf== NULL ){
            engine->error_code = INVALID_DATA;
            return NULL;
        }
        int k_v_array_index = json_b_search(c.ar, keyword);
        if (k_v_array_index== -1) continue;

        return  c.ar->values[k_v_array_index].entry;    
    }
    return NULL;
}
char * disk_read_snap(snapshot * snap, const char * keyword){ 
    
    for (int i= 0; i < MAX_LEVELS; i++){
        if (snap->sst_files[i] == NULL |snap->sst_files[i]->len ==0) continue;

        list * sst_files_for_x = snap->sst_files[i];
        size_t index_sst = find_sst_file(sst_files_for_x, sst_files_for_x->len, keyword);
        if (index_sst == -1) continue;
        
        sst_f_inf * sst = at(sst_files_for_x, index_sst);
       
        bloom_filter * filter=  sst->filter;
        if (!check_bit(keyword,filter)) continue;
       
        size_t index_block= find_block(sst, keyword);
        block_index * index = at(sst->block_indexs, index_block);
        
        cache_entry c = retrieve_entry_sharded(*snap->cache_ref, index, sst->file_name, sst);
        if (c.buf== NULL ){
            return NULL;
        }
        int k_v_array_index = json_b_search(c.ar, keyword);
        if (k_v_array_index== -1) continue;

        return  c.ar->values[k_v_array_index].entry;    
    }
    return NULL;
}
char* read_record(storage_engine * engine, const char * keyword){
    // 1. Check the active table
    mem_table * active_table = engine->active_table;
    Node * entry = search_list(active_table->skip, keyword);
    if (entry != NULL) {
        // Found in active table
        // Handle tombstone check if necessary (assuming TOMB_STONE means deleted)
        if (strcmp(entry->value.entry, TOMB_STONE) == 0) {
             return NULL; // Treat tombstone as not found
        }
        return entry->value.entry;
    }

    // 2. Check immutable tables waiting in the flush queue (newest first)
    // Create a temporary snapshot of the flush queue pointers
    int flush_queue_size = engine->flush_queue->size;
    if (flush_queue_size > 0) {
        mem_table* temp_tables[flush_queue_size];
        int count = 0;
        mem_table* temp_table;

        // Dequeue all into temp array
        while(memtable_queue_t_dequeue(engine->flush_queue, &temp_table)) {
            temp_tables[count++] = temp_table;
        }

        // Search temp array (reverse order - newest immutable first) and re-enqueue
        for (int i = count - 1; i >= 0; i--) {
            mem_table* table_to_check = temp_tables[i];
            entry = search_list(table_to_check->skip, keyword);
             // Re-enqueue the table immediately after checking (or before?) - let's do it after search loop
            if (entry != NULL) {
                 // Re-enqueue all tables before returning
                 for(int j=0; j<count; ++j){
                     memtable_queue_t_enqueue(engine->flush_queue, temp_tables[j]);
                 }
                 // Found in an immutable table
                 if (strcmp(entry->value.entry, TOMB_STONE) == 0) {
                     return NULL; // Treat tombstone as not found
                 }
                 return entry->value.entry;
            }
        }
         // Re-enqueue all tables if not found during search
         for(int j=0; j<count; ++j){
             memtable_queue_t_enqueue(engine->flush_queue, temp_tables[j]);
         }
    }


    // 3. If not found in memory, check disk
    return disk_read(engine, keyword);
}
// Reads directly from the snapshot's disk view, does not check live memtables.
char* read_record_snap(storage_engine * engine, const char * keyword, snapshot * s){
   // Snapshots represent disk state at a point in time.
   // We should not consult the live/active memtables for a snapshot read.
   return disk_read_snap(s, keyword);
}
void seralize_table(SkipList * list, byte_buffer * buffer, sst_f_inf * s){
    if (list == NULL) return;
    Node * node = list->header->forward[0];
    size_t sum = 2;
    int num_entries=  0;
    int num_entry_loc = buffer->curr_bytes;
    buffer->curr_bytes+=2;
    db_unit last_entry;
    memcpy(s->min, node->key.entry, node->key.len);

    int est_num_keys = 10 *(GLOB_OPTS.BLOCK_INDEX_SIZE/(4*1024));
    block_index  b = create_ind_stack(est_num_keys);
    while (node != NULL){
        last_entry = node->key;
        if (sum  + 4 + node->key.len + node->value.len > GLOB_OPTS.BLOCK_INDEX_SIZE){
            size_t bytes_to_skip = GLOB_OPTS.BLOCK_INDEX_SIZE - sum;
            memcpy(&buffer->buffy[num_entry_loc],&num_entries,2);
            b.len = sum;
            build_index(s, &b, buffer, num_entries, num_entry_loc);
            buffer->curr_bytes +=bytes_to_skip;
            num_entry_loc = buffer->curr_bytes;
            buffer->curr_bytes +=2; //for the next num_entry_loc;
            num_entries = 0;
            sum = 2;
            b=  create_ind_stack(est_num_keys);
          
        }
        sum += write_db_unit(buffer, node->key);
        sum += write_db_unit(buffer, node->value);
        node = node->forward[0];
        num_entries++;
    }
    memcpy(&buffer->buffy[num_entry_loc], &num_entries, 2);
    b.len = sum;
    build_index(s, &b,buffer, num_entries, num_entry_loc);
    memcpy(s->max, last_entry.entry, last_entry.len);
    gettimeofday(&s->time, NULL);
}

int flush_all_tables(storage_engine * engine){
    mem_table * table_to_flush = NULL;
    while (memtable_queue_t_dequeue(engine->flush_queue, &table_to_flush)) {
        if (table_to_flush != NULL) {
             if (table_to_flush->bytes > 0) {
                 flush_table(table_to_flush, engine);
             }
             clear_table(table_to_flush);
             memtable_queue_t_enqueue(engine->ready_queue, table_to_flush);
        }
        table_to_flush = NULL;
    }
    return 0;
}
int flush_table(mem_table *table, storage_engine * engine){

    meta_data * meta = engine->meta;
    
    sst_f_inf *sst ;
    /*get a heap allocated sst*/
    {
        if (meta->sst_files[0]->len + 1 > meta->sst_files[0]->cap){
            expand(meta->sst_files[0]);
        } 
        sst_f_inf * sst_l = meta->sst_files[0]->arr;
        sst = &sst_l[meta->sst_files[0]->len];
        meta->sst_files[0]->len++;
    }
    *sst =  create_sst_filter(table->filter);
    byte_buffer * buffer = select_buffer(GLOB_OPTS.MEM_TABLE_SIZE);
    if (table->bytes <= 0) {
        printf("DEBUG: Table bytes <= 0, skipping flush\n");
        return_struct(engine->write_pool, buffer,&reset_buffer);
        return -1;
    }
    seralize_table(table->skip, buffer, sst);
    sst->length = buffer->curr_bytes;
    
    buffer->curr_bytes +=2;
    sst->block_start = sst->length + 2;
    sst->compressed_len = 0;

    meta->db_length+= buffer->curr_bytes;
    all_index_stream(sst->block_indexs->len, buffer, sst->block_indexs);
    copy_filter(sst->filter, buffer);
    generate_unique_sst_filename(sst->file_name, MAX_F_N_SIZE, LVL_0);
    
    sst->use_dict_compression = false;

    db_FILE * sst_f = dbio_open(sst->file_name, 'w');
    set_context_buffer(sst_f, buffer);
    
    size_t bytes = dbio_write(sst_f, 0, buffer->curr_bytes);
    if (bytes < sst->length){
        return -1;
    }
    dbio_fsync(sst_f);

    meta->num_sst_file ++;
    /*now a "used table"*/
    table->bytes = 0;
  
    if (engine->cm_ref!= NULL) *engine->cm_ref = true;

    dbio_close(sst_f);
    return_buffer(buffer);

    return 0;

}
void free_one_table(void* table){
    mem_table * m_table= table;
   
    freeSkipList(m_table->skip);;
    if (m_table->num_pair <= 0) free_filter(m_table->filter);
    else free(m_table->filter);
    table = NULL;
}
void dump_tables(storage_engine * engine){
    if (engine->active_table != NULL && engine->active_table->bytes > 0) {
        flush_table(engine->active_table, engine);
    }
    flush_all_tables(engine);
    printf("DEBUG: Finished dump_tables\n");
 
}
void free_engine(storage_engine * engine, char* meta_file,  char * bloom_file){
    dump_tables(engine);
    destroy_meta_data(meta_file, bloom_file,engine->meta);  
    engine->meta = NULL;
    // Free the active table
    if (engine->active_table) {
        free_one_table(engine->active_table);
        free(engine->active_table);
        engine->active_table = NULL;
    }

    // Free tables remaining in queues (ready queue might have tables if dump failed/skipped)
    mem_table* temp_table = NULL;
    if (engine->ready_queue) {
        while (memtable_queue_t_dequeue(engine->ready_queue, &temp_table)) {
            if (temp_table) {
                free_one_table(temp_table);
                free(temp_table);
            }
        }
        memtable_queue_t_destroy(engine->ready_queue);
        engine->ready_queue = NULL;
    }

    // Flush queue should be empty after dump_tables, but clean up defensively
    if (engine->flush_queue) {
         while (memtable_queue_t_dequeue(engine->flush_queue, &temp_table)) {
             if (temp_table) {
                free_one_table(temp_table);
                free(temp_table);
            }
        }
        memtable_queue_t_destroy(engine->flush_queue);
        engine->flush_queue = NULL;
    }
    kill_WAL(engine->w);
    free_pool(engine->write_pool, &free_buffer);
    free_shard_controller(&engine->cach);
    free(engine);
    engine = NULL;
}


