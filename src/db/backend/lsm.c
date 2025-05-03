#include "lsm.h"
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
    const int est_max_nodes= GLOB_OPTS.MEM_TABLE_SIZE / 4;
    table->skip = create_skiplist(est_max_nodes,&compareString);
    if (table->skip == NULL) {
        free_filter(table->filter);
        free(table);
        return NULL;
    }
    for(int i = 0 ; i < 2; i++){
        table->range[i] = NULL;
    }
    table->ref_count = 0;
    return table;
}
void free_db_resource(db_resource * resource){
    if (resource->src == INVALID) return;
    if (resource->src == CACHE){
       unpin_page(resource->resource);
       resource->resource = NULL;
       resource->value.entry = NULL;
       resource->value.len= 0;
    }
    else if (resource->src == MEMTABLE){
        mem_table * homeless = resource->resource;
        homeless->ref_count  --;
        if (homeless->ref_count < 0){
            clear_table(homeless);
            memtable_queue_t * q = homeless->util;
            memtable_queue_t_enqueue(q, homeless);
        }
    }
}
/*
int restore_state(storage_engine * e ,int lost_tables){
    WAL* w = e->w;
    const int flush_cadence = w->flush_cadence;
    byte_buffer * buffers[NUM_WAL_SEGMENTS];
    for (int i = 0; i < NUM_WAL_SEGMENTS; i++){
        buffers[i]=  create_buffer(w->segments_manager.segment_capacity * 1.1);
        char buf [256];
        get_wal_fn(buf, i);
        FILE * wal_f = fopen(buf, "rb");
        fread(buffers[i], 1,, wal_f);
    }
}
*/
void clear_table(mem_table * table){
  

    reset_skip_list(table->skip);
    table->filter =  bloom(NUM_HASH,NUM_WORD,false, NULL);
    const int est_max_nodes= GLOB_OPTS.MEM_TABLE_SIZE / 4;
    table->skip = create_skiplist(est_max_nodes,&compareString);
    table->bytes = 0;
    table->num_pair = 0;
    table->immutable = false;
    for(int i = 0 ; i < 2; i++){
        table->range[i] = NULL;
    }
     table->ref_count = 0;
}
storage_engine * create_engine(char * file, char * bloom_file){
    init_crc32_table();
    storage_engine * engine = (storage_engine*)wrapper_alloc((sizeof(storage_engine)), NULL,NULL);

    set_debug_defaults(&GLOB_OPTS);

    engine->ready_queue = memtable_queue_t_create(GLOB_OPTS.num_memtable);
    engine->flush_queue = memtable_queue_t_create(GLOB_OPTS.num_memtable);


    engine->active_table = create_table();
    engine->active_table->util = engine->ready_queue;
    engine->num_table = GLOB_OPTS.num_memtable;

    
    for (int i = 1; i < GLOB_OPTS.num_memtable; i++) { // Start from 1 as active_table is the first
        mem_table* spare_table = create_table();
        spare_table->util = engine->ready_queue;
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
    /*restore_state(engine,1); change 1 to a variable when implementing error system. also remeber
        the stray skipbufs inside this function skipbuf*/
        // wtf is a skipbuf
    }
    engine->error_code = OK;
    engine->cm_ref = NULL;
    engine->mana = create_manager(GLOB_OPTS.BLOCK_INDEX_SIZE, GLOB_OPTS.SST_TABLE_SIZE, GLOB_OPTS.index_cache_mem);

    return engine;
}
/*NO TOUCHING*/
int handle_annoying_ass_fucking_edge_case_fuck(storage_engine* engine ,db_unit key, db_unit value){
    mem_table * table = NULL;
    while(1){
        table = engine->active_table;
        table->immutable = true;
        memtable_queue_t_enqueue(engine->flush_queue, table); 

        mem_table* next_table = NULL;
        while (!memtable_queue_t_dequeue(engine->ready_queue, &next_table)) {
            aco_yield();
        }
        engine->active_table = next_table; 
        table = engine->active_table;      

        if (engine->flush_queue->size >= GLOB_OPTS.num_to_flush) {
            flush_all_tables(engine);
        }
        table = engine->active_table;
        if (table->immutable){
            continue;
        }
        if (table->bytes + 4 + key.len + value.len < GLOB_OPTS.MEM_TABLE_SIZE){
            break;
        }
    }
    insert_list(table->skip, key, value);
    table->num_pair++;
    table->bytes += 4 + key.len + value.len;
    map_bit(key.entry, table->filter);
    return 0;
}
int write_record(storage_engine* engine ,db_unit key, db_unit value){
    if (key.entry== NULL || value.entry == NULL ) return -1;
    int success = write_WAL(engine->w, key, value);
    if (success  == FAILED_TRANSCATION) {
        fprintf(stdout, "WARNING: transcantion failure \n");
        return FAILED_TRANSCATION;
    }
   
    mem_table * table = NULL;
    while((table = engine->active_table)== NULL){
        aco_yield();
    }
    // Check if the active table needs to be rotated
    if (table->bytes + 4 + key.len + value.len >= GLOB_OPTS.MEM_TABLE_SIZE) {
        return handle_annoying_ass_fucking_edge_case_fuck(engine, key, value);
    }
    insert_list(table->skip, key, value);
    table->num_pair++;
    table->bytes += 4 + key.len + value.len;
    map_bit(key.entry, table->filter);
    
    return 0;
}
db_resource return_bad_result(){
    db_resource src;
    src.resource = NULL;
    src.value.entry = NULL;
    return src;
}
db_resource get_key_from_block(shard_controller cach, sst_f_inf * sst, block_index * index ,const char * keyword){
    cache_entry c = retrieve_entry_sharded(cach, index, sst->file_name, sst);
    if (c.buf== NULL ){
        return return_bad_result();
    }   
    int k_v_array_index = json_b_search(c.ar, keyword);
    if (k_v_array_index== -1){
        return return_bad_result();
    }
    db_resource src;
    src.resource = index->page;
    src.value= c.ar->values[k_v_array_index]; 
    src.src = CACHE;
    return src;   
}

/* this is our read path. This read path */
db_resource scan_l_0(sst_manager * mana, shard_controller cach, const char * key){
    uint32_t num = get_num_l_0(&mana);
    db_resource ret;
    ret.resource = NULL;
    for (int i  = 0; i < num; i++){
        sst_f_inf * sst=   get_sst(mana, key, 0);
        int check = check_sst(mana,sst, key,0);
        if (!check) continue;
        block_index * ind = get_block(mana, sst, key, 0, 0);
        if (ind == NULL) continue;
        ret = get_key_from_block(cach, sst, ind, key);
        if (ret.resource != NULL) return ret;
    }
    return ret;
}
db_resource disk_read(storage_engine * engine, const char * keyword){ 
    {
        db_resource l_0_test=  scan_l_0(&engine->mana, engine->cach,keyword);
        if (l_0_test.resource != NULL){
            return l_0_test;
        }
    }
    for (int i= 1; i < MAX_LEVELS; i++){   
        sst_f_inf * sst = get_sst(&engine->mana, keyword, i);
        if (sst == NULL ) return;
        int32_t part_ind = check_sst(&engine->mana, sst, keyword, i);
        if (part_ind<= 0) continue;
        block_index * ind = get_block(&engine->mana, sst, keyword, i, part_ind);
        if (ind == NULL) continue;
        db_resource src = get_key_from_block(engine->cach, sst, ind, keyword);
        if (src.resource != NULL) return src;
    }
    return return_bad_result();
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
int check_memtables(storage_engine * engine, const char * keyword, db_resource * resource){
    
    for (int i =engine->flush_queue->size -1 ; i >= 0; i--){
        mem_table * tbl;
        memtable_queue_t_peek_slot_x(engine->flush_queue, &tbl, i);
        Node * node = search_list(tbl->skip, keyword);
        if (!node) continue;
        if (strcmp(node->value.entry, TOMB_STONE) == 0) return -1;
        resource->value= node->value;
        resource->resource = tbl;
        resource->src = MEMTABLE;
        return true;
    }
    return false;

}
static inline db_resource read_logic(storage_engine * engine, const char * keyword){
    // 1. Check the active table
    mem_table * active_table = engine->active_table;
    Node * entry = search_list(active_table->skip, keyword);
    db_resource src;
    if (entry != NULL) {
        // Found in active table
        // Handle tombstone check if necessary (assuming TOMB_STONE means deleted)
        if (strcmp(entry->value.entry, TOMB_STONE) == 0) {
            src.value.entry = NULL;
            src.src = INVALID;
            return src;
        }
        src.value = entry->value;
        src.resource = active_table;
        src.src = MEMTABLE;
        return src;
    }
    int res=  check_memtables(engine, keyword,&src);
    if (res == -1 ){
        src.value.entry = NULL;
        return src;
    }
    else if (res){

        return src;
    }   
    src= disk_read(engine, keyword);
    return src;
}
/*naive read*/
char* read_record(storage_engine * engine, const char * keyword){
    db_resource record = read_logic(engine, keyword);
    return record.value.entry;
}
/*copy into a usr supplied out*/
int memcpy_read(storage_engine * engine, const char * keyword, db_unit * out){
    db_resource record = read_logic(engine, keyword);
    if (record.value.entry == NULL || record.src == INVALID){
        return -1;
    }
    out->len = record.value.len;
    memcpy(out->entry, record.value.entry, out->len);
    free_db_resource(&record);
    return 0;

}
/*raw slice*/
db_resource read_and_pin(storage_engine * engine, const char * keyword){
    db_resource src = read_logic(engine, keyword);
    if (src.src == MEMTABLE){
        mem_table * tbl = src.resource;
        tbl->ref_count ++;
    }
    return src;
}
// Reads directly from the snapshot's disk view, does not check live memtables.
char* read_record_snap(storage_engine * engine, const char * keyword, snapshot * s){
   // Snapshots represent disk state at a point in time.
   // We should not consult the live/active memtables for a snapshot read.
   return disk_read_snap(s, keyword);
}
/*l_0 tables will never have partitions, because thats extra work for no real gain. hence the reliance on the old block_indexs 
infrastructure*/
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
             if (table_to_flush->ref_count <= 0){
                 clear_table(table_to_flush);
                 memtable_queue_t_enqueue(engine->ready_queue, table_to_flush);
             }
             //ELSE: WE CAN DROP YOU FROM THE QUEUE. HERES WHY:
             //A PINNED MEMTABLE IS ALREADY WRITTEN TO DISK, SO NON PINNED OWNERS DONT CARE
             //THE OWNERS HAVE ACCESS TO THIS RESOURCE AND CAN PREFORM RESOURCE FREES ON IT


        }
        table_to_flush = NULL;
    }
    return 0;
}
int flush_table(mem_table *table, storage_engine * engine){

    meta_data * meta = engine->meta;
    
    sst_f_inf *sst = pad_allocate(sizeof(*sst));
    *sst =  create_sst_filter(table->filter);
    table->filter = NULL;
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
    add_sst(&engine->mana, *sst, 0);

    return 0;

}
void free_one_table(void* table){
    mem_table * m_table= table;
   
    freeSkipList(m_table->skip);
    if (m_table->num_pair <= 0) free_filter(m_table->filter);
    else free(m_table->filter);
    m_table->filter = NULL;
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
    free_sst_man(&engine->mana);
    free(engine);
    engine = NULL;
}


