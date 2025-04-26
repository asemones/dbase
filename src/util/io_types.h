#ifndef IO_TYPES_H
#define IO_TYPES_H

#include <stddef.h>
#include <sys/types.h>
#include <stdint.h>
#include <float.h>
#include <time.h> // Added for timespec
#include <liburing.h>
#include <sys/uio.h>

struct arena;
struct byte_buffer;
struct struct_pool;

#include "../ds/arena.h"
#include "../ds/byte_buffer.h"
#include "../ds/structure_pool.h"


enum operation{
    READ,
    WRITE,
    OPEN,
    CLOSE,
};

typedef void (*aio_callback)(void *arg);

typedef struct io_batch_tuner{
    uint64_t full_flush_runs;
    uint64_t non_full_runs;
    double batch_timer_len_ns;
    double full_ratio;
    uint64_t last_recorded_ns;
    uint64_t io_req_in_slice;
} io_batch_tuner;

struct io_manager{
    int m_tbl_s;
    int sst_tbl_s;
    struct_pool * sst_table_buffers;
    struct_pool * mem_table_buffers;
    struct_pool * four_kb;
    struct_pool * io_requests;
    struct io_uring ring;
    arena a;
    int num_in_queue;
    int total_buffers;
    int num_segments;
    struct iovec *iovecs;
    int pending_sqe_count;      
    struct timespec first_sqe_timestamp; 
    io_batch_tuner tuner;
};

// Declaration for the thread-local io_manager pointer defined in io.c
extern __thread struct io_manager * man;


union descriptor{
    uint64_t fd;
    char * fn;
};

typedef struct db_FILE {
    union descriptor desc;
    int priority;
    off_t offset;
    mode_t perms;
    size_t len;
    int flags;
    aio_callback callback;
    void *callback_arg;
    enum operation op;
    byte_buffer* buf;
    int response_code;
} db_FILE;
#endif // IO_TYPES_H