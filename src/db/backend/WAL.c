#include "WAL.h"
#include "../../util/multitask.h"
#include "../../util/io.h"
#include "../../util/error.h"
#include "../../util/maths.h"
#include "../../ds/byte_buffer.h"
#include "../../ds/structure_pool.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h> // For exit()
#include <assert.h>
db_FILE * clone_ctx(db_FILE * ctx){
    db_FILE * ctx_cp = get_ctx();
    size_t size = sizeofdb_FILE();
    task_t * task = aco_get_arg();
    memcpy(ctx_cp, ctx, size);
    ctx_cp->callback_arg = task;
    return ctx_cp;
}
void serialize_wal_metadata(WAL *w, byte_buffer *b) {
    reset_buffer(b);
    write_buffer(b, (char*)&w->total_len, sizeof(w->total_len));
    write_buffer(b, (char*)&w->segments_manager.current_segment_idx, sizeof(w->segments_manager.current_segment_idx));
    write_buffer(b, (char*)&w->segments_manager.segments[w->segments_manager.current_segment_idx].current_size, sizeof(size_t));
}

bool deserialize_wal_metadata(WAL *w, byte_buffer *b) {
    read_buffer(b, (char*)&w->total_len, sizeof(w->total_len));
    read_buffer(b, (char*)&w->segments_manager.current_segment_idx, sizeof(w->segments_manager.current_segment_idx));
    read_buffer(b, (char*)&w->segments_manager.segments[w->segments_manager.current_segment_idx].current_size, sizeof(size_t));

    if (w->segments_manager.current_segment_idx < 0 || w->segments_manager.current_segment_idx >= w->segments_manager.num_segments) {
        fprintf(stderr, "Warning: Invalid current_segment_idx (%d) read from WAL metadata. Resetting.\n", w->segments_manager.current_segment_idx);
        w->segments_manager.current_segment_idx = 0;
        w->segments_manager.segments[0].current_size = 0;
        w->total_len = 0;
        return false;
    }
    return true;
}

int rotate_wal_segment(WAL *w) {
    WAL_segments_manager *mgr = &w->segments_manager;
    int current_idx = mgr->current_segment_idx;
    int next_idx = (current_idx + 1) % mgr->num_segments;

    mgr->segments[current_idx].active = false;
    mgr->segments[next_idx].active = true;
    mgr->segments[next_idx].current_size = 0;

    mgr->current_segment_idx = next_idx;

    return 0;
}

int flush_wal_buffer(WAL *w, db_unit k, db_unit v) {
    if (!w || !w->wal_buffer || w->wal_buffer->curr_bytes == 0) {
        return 0;
    }

    WAL_segments_manager *mgr = &w->segments_manager;
    WAL_segment *current_segment = &mgr->segments[mgr->current_segment_idx];
    byte_buffer *buffer_to_flush = w->wal_buffer;
    size_t flush_size = buffer_to_flush->curr_bytes;
    size_t write_offset = current_segment->current_size;

    db_FILE *file_ctx = clone_ctx(current_segment->model);
    if (!file_ctx) {
        fprintf(stderr, "CRITICAL: No db_FILE available in pool for segment %d. Aborting.\n", mgr->current_segment_idx);
        exit(EXIT_FAILURE);
    }
    w->wal_buffer = select_buffer(GLOB_OPTS.WAL_BUFFERING_SIZE);
    if (!w->wal_buffer) {
        fprintf(stderr, "CRITICAL: Failed to select new WAL buffer during flush. Aborting.\n");
        return_ctx(file_ctx);
        exit(EXIT_FAILURE);
    }
    if (k.entry){
        current_segment->current_size += write_db_unit(w->wal_buffer, k);
        current_segment->current_size += write_db_unit(w->wal_buffer, v);
    }

    file_ctx->offset = write_offset;
    file_ctx->op = WRITE;
    file_ctx->len = flush_size;
    set_context_buffer(file_ctx, buffer_to_flush);

    int submission_result = dbio_write(file_ctx, write_offset, flush_size);


    return_ctx(file_ctx);
    return_buffer(buffer_to_flush);

    if (submission_result < 0) {
        perror("dbio_write submission failed in flush_wal_buffer");
       
        return submission_result;
    }
    return flush_size;
}

WAL* init_WAL(byte_buffer *b) {
    WAL *w = malloc(sizeof(WAL));
    if (!w) {
        perror("CRITICAL: Failed to allocate WAL struct");
        exit(EXIT_FAILURE);
    }
    memset(w, 0, sizeof(WAL));

    WAL_segments_manager *mgr = &w->segments_manager;
    mgr->num_segments = NUM_WAL_SEGMENTS;
    mgr->segment_capacity = WAL_SEGMENT_SIZE;
    mgr->current_segment_idx = 0;

    for (int i = 0; i < mgr->num_segments; ++i) {
        WAL_segment *seg = &mgr->segments[i];
        sprintf(seg->filename, "WAL_SEG_%d.bin", i);
        seg->current_size = 0;
        seg->active = (i == 0);

        
        db_FILE *file_ctx_for_clone = dbio_open(seg->filename, 'a');
        if (!file_ctx_for_clone || file_ctx_for_clone->desc.fd < 0) {
            dbio_close(file_ctx_for_clone); // Close potentially invalid context
            file_ctx_for_clone = dbio_open(seg->filename, 'w'); // Try creating/truncating
        }

        // Ensure the open succeeded
        if (!file_ctx_for_clone || file_ctx_for_clone->desc.fd < 0) {
            perror("CRITICAL: Failed to open/create initial WAL segment file");
            exit(EXIT_FAILURE);
        }
        seg->model = file_ctx_for_clone;
    }

    w->wal_buffer = select_buffer(GLOB_OPTS.WAL_BUFFERING_SIZE);
    if (!w->wal_buffer) {
        perror("CRITICAL: Failed to allocate initial WAL buffer");
        exit(EXIT_FAILURE);
    }

    bool loaded_metadata = false;
    w->meta_ctx = dbio_open(GLOB_OPTS.WAL_M_F_N, 'r');

    if (w->meta_ctx && w->meta_ctx->desc.fd >= 0) {
        byte_buffer *meta_read_buf = b ? b : select_buffer(4096);
        if (meta_read_buf) {
            reset_buffer(meta_read_buf);
            set_context_buffer(w->meta_ctx, meta_read_buf);
            int bytes_read = dbio_read(w->meta_ctx, 0, meta_read_buf->max_bytes);

            if (bytes_read > 0) {
                meta_read_buf->curr_bytes = bytes_read;
                if (deserialize_wal_metadata(w, meta_read_buf)) {
                    loaded_metadata = true;
                    for(int i=0; i<mgr->num_segments; ++i) {
                        mgr->segments[i].active = (i == mgr->current_segment_idx);
                    }
                }
            } else if (bytes_read < 0) {
                perror("Failed to read WAL metadata file, continuing...");
            }
            if (!b) return_buffer(meta_read_buf);
        } else {
            perror("Failed to allocate buffer for reading WAL metadata, continuing...");
        }
        dbio_close(w->meta_ctx);
    }

    w->meta_ctx = dbio_open(GLOB_OPTS.WAL_M_F_N, 'w');
    if (!w->meta_ctx || w->meta_ctx->desc.fd < 0) {
        perror("CRITICAL: Failed to open/create WAL metadata file for writing");
        exit(EXIT_FAILURE);
    }

    if (!loaded_metadata) {
        mgr->segments[0].current_size = 0;
        w->total_len = 0;
        struct db_FILE *trunc_ctx = dbio_open(mgr->segments[0].filename, 'w');
        if(trunc_ctx) dbio_close(trunc_ctx); else perror("Warning: Failed to truncate initial WAL segment");
    }

    // printf("WAL initialized. Active segment index: %d\n", mgr->current_segment_idx); // Reduced verbosity
    return w;
}

int write_WAL(WAL *w, db_unit key, db_unit value) {
    if (!w) return FAILED_TRANSCATION;

    int data_size = key.len + value.len + (sizeof(uint32_t) * 2);
    int ret = 0;

    WAL_segments_manager *mgr = &w->segments_manager;
    WAL_segment *current_segment = &mgr->segments[mgr->current_segment_idx]; 

 
    bool rotation_needed = (current_segment->current_size + data_size > mgr->segment_capacity);
    bool values_written  = false;
    if (rotation_needed) {
    
        ret = rotate_wal_segment(w);
        if (ret != 0) return FAILED_TRANSCATION;
        if (w->wal_buffer && w->wal_buffer->curr_bytes > 0) {
            ret = flush_wal_buffer(w, key, value);
            if (ret < 0) {
                fprintf(stderr, "Error flushing WAL buffer after rotation\n");
                return FAILED_TRANSCATION;
            }
        }
        current_segment = &mgr->segments[mgr->current_segment_idx];
        values_written = true;
    }

    bool buffer_flush_needed = (w->wal_buffer && (w->wal_buffer->curr_bytes + data_size > w->wal_buffer->max_bytes));
    if (buffer_flush_needed) {
         ret = flush_wal_buffer(w, key, value);
         if (ret < 0) {
             fprintf(stderr, "Error flushing WAL buffer before adding new data\n");
             return FAILED_TRANSCATION;
          }
          values_written = true;
    }

    if (!w->wal_buffer) {
         fprintf(stderr, "CRITICAL: WAL buffer is NULL before writing\n");
         exit(EXIT_FAILURE);
    }
    if (values_written){
        return 0;
    }
    int written_to_buffer = 0;
    ret = write_db_unit(w->wal_buffer, key);
    if (ret < 0) {
        fprintf(stderr, "Error writing key to WAL buffer\n");
        return FAILED_TRANSCATION;
    }
    written_to_buffer += ret;

    ret = write_db_unit(w->wal_buffer, value);
    if (ret < 0) {
        fprintf(stderr, "Error writing value to WAL buffer\n");
        return FAILED_TRANSCATION;
    }
    written_to_buffer += ret;

    w->total_len++;
    current_segment->current_size += written_to_buffer;

    return 0;
}

void kill_WAL(WAL *w) {
    if (!w) return;
    db_unit empty;
    empty.entry = NULL;
    if (flush_wal_buffer(w, empty, empty) < 0) {
        fprintf(stderr, "Warning: Error during final WAL buffer flush in kill_WAL.\n");
    }
    // Assuming flush completes or errors out before proceeding
    w->meta_ctx->callback_arg = aco_get_arg();
    byte_buffer *meta_write_buf = select_buffer(4096);
    if (meta_write_buf) {
        serialize_wal_metadata(w, meta_write_buf);
        if (meta_write_buf->curr_bytes > 0) {
            set_context_buffer(w->meta_ctx, meta_write_buf);
            if (dbio_write(w->meta_ctx, 0, meta_write_buf->curr_bytes) < 0) {
                 perror("Warning: Failed to write final WAL metadata");
            } else {
                 dbio_fsync(w->meta_ctx);
            }
        }
    } 
    else {
        perror("Warning: Failed to allocate buffer for final WAL metadata write");
    }
    for (int  i =0; i < NUM_WAL_SEGMENTS; i++){
        w->segments_manager.segments->model->callback_arg = aco_get_arg();
        dbio_close(w->segments_manager.segments->model);
    }
    if (w->meta_ctx) {
        dbio_close(w->meta_ctx);
        w->meta_ctx = NULL;
    }

    if (w->wal_buffer) {
        return_buffer(w->wal_buffer);
        w->wal_buffer = NULL;
    }

    free(w);
    // printf("WAL killed.\n"); // Reduced verbosity
}
