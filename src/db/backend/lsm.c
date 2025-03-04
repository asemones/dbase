#include "lsm.h"
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
    if (w->fn->len  == 0 ){
        return -1;
    }
    int ret = 0;
    for (int i = 0;  i < lost_tables; i++){
        mem_table * m = e->table[i];
        char ** file = get_last(w->fn);
        if (file  == NULL  || *file == NULL) return -1;
        FILE * target = fopen(*file, "rb");
        byte_buffer* skip_buffer = request_struct(e->write_pool);
        if (target == NULL) return -1;
        skip_buffer->curr_bytes = read_wrapper(skip_buffer->buffy,1, get_file_size(target), target);

        m->bytes = skip_from_stream(m->skip, skip_buffer);
        ret += m->bytes;

    }
    return ret;

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
    if (engine == NULL) return NULL;
    for (int i = 0; i < LOCKED_TABLE_LIST_LENGTH; i++){
        engine->table[i] = create_table();
        if (engine->table[i] == NULL) return NULL; 
    }
    set_debug_defaults(&GLOB_OPTS);
    engine->meta = load_meta_data(file, bloom_file);
    if (engine->meta == NULL) return NULL;
    engine->num_table = 0;
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
    engine->compactor_wait = malloc(sizeof(pthread_cond_t));
    engine->compactor_wait_mtx = malloc(sizeof(pthread_mutex_t));
    engine->cm_ref = NULL;
    pthread_mutex_init(engine->compactor_wait_mtx, NULL);
    pthread_cond_init(engine->compactor_wait, NULL);
    return engine;
}
int write_record(storage_engine* engine ,db_unit key, db_unit value){
    if (key.entry== NULL || value.entry == NULL ) return -1;
    int success = write_WAL(engine->w, key, value);
    if (success  == FAILED_TRANSCATION) {
        fprintf(stdout, "WARNING: transcantion failure \n");
        return FAILED_TRANSCATION;
    }
    if (engine->table[CURRENT_TABLE]->bytes + 4 + key.len + value.len >=  GLOB_OPTS.MEM_TABLE_SIZE){
        lock_table(engine);
        int status = flush_table(engine);
        if (status != 0) return -1;
    }
    
    insert_list(engine->table[CURRENT_TABLE]->skip, key, value);
    engine->table[CURRENT_TABLE]->num_pair++;
    engine->table[CURRENT_TABLE]->bytes += 4 + key.len + value.len ;
    map_bit(key.entry,engine->table[CURRENT_TABLE]->filter);
    
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
        
        cache_entry * c = retrieve_entry_sharded(engine->cach, index, sst->file_name, sst);
        if (c == NULL ){
            engine->error_code = INVALID_DATA;
            return NULL;
        }
        int k_v_array_index = json_b_search(c->ar, keyword);
        if (k_v_array_index== -1) continue;

        return  c->ar->values[k_v_array_index].entry;    
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
        
        cache_entry * c = retrieve_entry_sharded(*snap->cache_ref, index, sst->file_name, sst);
        if (c == NULL ){
            return NULL;
        }
        int k_v_array_index = json_b_search(c->ar, keyword);
        if (k_v_array_index== -1) continue;

        return  c->ar->values[k_v_array_index].entry;    
    }
    return NULL;
}
char* read_record(storage_engine * engine, const char * keyword){
    Node * entry= search_list(engine->table[CURRENT_TABLE]->skip, keyword);
    if (entry== NULL) return disk_read(engine, keyword);
    return entry->value.entry; 
}
char* read_record_snap(storage_engine * engine, const char * keyword, snapshot * s){
    Node * entry= search_list(engine->table[CURRENT_TABLE]->skip, keyword);
    if (entry== NULL) return disk_read_snap(s,keyword);
    return entry->value.entry; 
}
void seralize_table(SkipList * list, byte_buffer * buffer, sst_f_inf * s){
    if (list == NULL) return;
    Node * node = list->header->forward[0];
    size_t sum = 2;
    int num_entries=  0;
    int num_entry_loc = buffer->curr_bytes;
    buffer->curr_bytes+=2;
    db_unit last_entry;
    memcpy(&s->min, node->key.entry, node->key.len);

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
        map_bit(node->key.entry, b.filter);
        node = node->forward[0];
        num_entries++;
    }
    memcpy(&buffer->buffy[num_entry_loc], &num_entries, 2);
    b.len = sum;
    build_index(s, &b,buffer, num_entries, num_entry_loc);
    memcpy(&s->max, last_entry.entry, last_entry.len);
    gettimeofday(&s->time, NULL);
}

void lock_table(storage_engine * engine){
  
   engine->table[CURRENT_TABLE]->immutable = true;
   swap(&engine->table[CURRENT_TABLE], &engine->table[PREV_TABLE],8);
   engine->num_table++;
   
   clear_table(engine->table[CURRENT_TABLE]);

}
int flush_table(storage_engine * engine){
   
    mem_table * table = engine->table[PREV_TABLE];
    
    byte_buffer * buffer = request_struct(engine->write_pool);
    byte_buffer* compressed_buffer = request_struct(engine->write_pool);
    meta_data * meta = engine->meta;
  

    sst_f_inf sst_s = create_sst_filter(table->filter);
    

    sst_f_inf * sst = &sst_s;
    if (table->bytes <= 0) {
        printf("DEBUG: Table bytes <= 0, skipping flush\n");
        return_struct(engine->write_pool, buffer,&reset_buffer);
        return -1;
    }
    seralize_table(table->skip, buffer, sst);
    sst->length = buffer->curr_bytes;
    if (GLOB_OPTS.compress_level > -5){   
        handle_compression(sst,buffer, compressed_buffer, NULL);
        sst->compressed_len = compressed_buffer->curr_bytes;
        return_struct(engine->write_pool, buffer, &reset_buffer);
        buffer = compressed_buffer;
        buffer->curr_bytes +=2;
        sst->block_start = sst->compressed_len + 2;
    }
    else{
        buffer->curr_bytes +=2;
        sst->block_start = sst->length + 2;
        sst->compressed_len = 0;
    }
    meta->db_length+= buffer->curr_bytes;
    all_index_stream(sst->block_indexs->len, buffer, sst->block_indexs);
    copy_filter(sst->filter, buffer);
    generate_unique_sst_filename(sst->file_name, MAX_F_N_SIZE, LVL_0);
    
    sst->use_dict_compression = false;

    if (compressed_buffer == NULL) {
        return_struct(engine->write_pool, buffer, &reset_buffer);
        return -1;
    }
    FILE * sst_f = fopen(sst->file_name, "wb");
    
    write_wrapper(buffer->buffy, sizeof(unsigned char), buffer->curr_bytes, sst_f);
    return_struct(engine->write_pool, buffer, &reset_buffer);
    fflush(sst_f);
    fsync(sst_f->_fileno);
    meta->num_sst_file ++;
    insert(meta->sst_files[0], sst);

    
    table->immutable = false;
    table->bytes = 0;
  
    if (engine->cm_ref!= NULL) *engine->cm_ref = true;
    pthread_cond_signal(engine->compactor_wait);
    return 0;

}
void free_one_table(mem_table * table){
    //destroy_b_tree (table->tree);
    freeSkipList(table->skip);;
    if (table->num_pair <= 0) free_filter(table->filter);
    else free(table->filter);
    free(table);
    table = NULL;
}
void dump_tables(storage_engine * engine){
    printf("DEBUG: Starting dump_tables\n");
    for(int i = 0;  i < LOCKED_TABLE_LIST_LENGTH; i++){
        if (engine->table[i]== NULL) continue;
        printf("DEBUG: Dump table[%d] - addr: %p, bytes: %zu, immutable: %d\n",
               i, (void*)engine->table[i], engine->table[i]->bytes, engine->table[i]->immutable);
        
        if (engine->table[i]->bytes > 0 && i==0){
      
            //this should be changed asap to stop edgecases
            lock_table(engine);
            flush_table(engine);
        }
    }
    printf("DEBUG: Finished dump_tables\n");
    //fclose(engine->table->file);
}
void free_tables(storage_engine * engine){
    for(int i = 0;  i < LOCKED_TABLE_LIST_LENGTH; i++){
        if (engine->table[i]== NULL) continue;
        free_one_table(engine->table[i]);
        engine->table[i] = NULL;
    }
}
void free_engine(storage_engine * engine, char* meta_file,  char * bloom_file){
    dump_tables(engine);
    destroy_meta_data(meta_file, bloom_file,engine->meta);  
    engine->meta = NULL;
    free_tables(engine);
    kill_WAL(engine->w, request_struct(engine->write_pool));
    free_pool(engine->write_pool, &free_buffer);
    free_shard_controller(&engine->cach);
    pthread_mutex_destroy(engine->compactor_wait_mtx);
    pthread_cond_destroy(engine->compactor_wait);
    free(engine->compactor_wait);
    free(engine->compactor_wait_mtx);
    free(engine);
    engine = NULL;
}


