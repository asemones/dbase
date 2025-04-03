#include "io.h"
#define DEPTH 64
#define ALLIGNMENT 4 * 1024
struct db_FILE;
/*REMEBR: need to allign*/
/*design info:
This is a generalized io_uring interface for disk and eventually network. O_Direct for io, 
ZERO memcpy. Kernel polling in an event loop with callbacks for submitted tasks. priority based scheduling
. extenability for io statistics and rate limiting. single point of IO for majority of IO, excluding metadata reads at the 
start-no need to overcomplicate. 
*/
typedef void (*aio_callback)(void *arg);
int setup_io_uring(struct io_uring *ring, int queue_depth) {
    int ret = io_uring_queue_init(queue_depth, ring, 0);
    if (ret < 0) {
        fprintf(stderr, "io_uring_queue_init failed: %s\n", strerror(-ret));
        return -1;
    }
    return 0;
}
int init_struct_pool_kernel(struct_pool * pool, arena * a,int bufsize, int bufnum){
      for (int i = 0; i< bufnum; i++){
        byte_buffer  * buffer = malloc(sizeof(byte_buffer));

        buffer->max_bytes = bufsize;
        buffer->curr_bytes = 0;
        buffer->buffy = arena_alloc(a, bufsize);
        insert_struct(pool, buffer);
    }
    return 0;
}
int add_fsync_request(struct io_uring *ring, int fd, int seq) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        fprintf(stderr, "Could not get SQE for fsync request\n");
        return -1;
    }
    if (seq) {
        sqe->flags |= IOSQE_IO_LINK;
    }
    io_uring_prep_fsync(sqe, fd, IORING_FSYNC_DATASYNC);
    
    return 0;
}
int register_logical_segments(struct io_manager *manage, const int max_single_buffer) {
    struct_pool * pools [3];
    pools[0] = manage->four_kb;
    pools[1] = manage->sst_table_buffers;
    pools[2] = manage->mem_table_buffers;
    int curr_mem = 0;
    for (int i = 0; i < sizeof(pools)/ sizeof(pools[0]); i++){
        for (int j = 0; j < pools[i]->capacity; j++){
            byte_buffer * buf = pools[i]->pool[j];
            buf->id = curr_mem / max_single_buffer;
            curr_mem += buf->max_bytes;
        }
    }
    return ceil_int_div(curr_mem, max_single_buffer);
}
void init_io_manager(struct io_manager * manage, int num_4kb, int num_sst_tble, int num_memtable, int sst_tbl_s, int mem_tbl_s){
    manage->four_kb = create_pool(num_4kb);
    manage->mem_table_buffers = create_pool(num_memtable);
    manage->sst_table_buffers = create_pool(num_sst_tble);
    
    const int max_single_buffer = 1024 * 1024* 1024;
    size_t total_size = (num_4kb * 4096) + (num_memtable * mem_tbl_s) + (num_sst_tble * sst_tbl_s);
    
    const int physical_buffers = ceil_int_div(total_size, max_single_buffer);

    manage->iovecs = calloc(physical_buffers, sizeof(struct iovec));

    manage->a.capacity = 0;
    manage->a.data = aligned_alloc(ALLIGNMENT, total_size);
    if (!manage->a.data) {
        perror("Failed to allocate aligned memory");
        return;
    }
    manage->a.size = total_size;
    int max_to_alloc = min(max_single_buffer, total_size);
    int chunked = 0;
    for (int i = 0 ; i < physical_buffers; i++){
        int real_size = min(total_size - chunked, max_to_alloc);
        chunked += real_size;
        manage->iovecs[i].iov_base= arena_alloc(&manage->a, real_size);
        manage->iovecs[i].iov_len = real_size;
    }
    manage->a.capacity = 0;

    int res = setup_io_uring(&manage->ring, DEPTH);
    if (res < 0) {
        perror("Failed to setup io_uring");
        return;
    }
    res = io_uring_register_buffers(&manage->ring, manage->iovecs, physical_buffers);

    if (res <  0){
        perror("Failed to allocate segements");
    }
    int logical_buff = num_4kb + num_memtable + num_sst_tble;
    
    manage->total_buffers = logical_buff;

    if (num_4kb > 0){
        init_struct_pool_kernel(manage->four_kb, &manage->a, 4 * 1024, num_4kb);
    }
    if (num_sst_tble > 0){
        init_struct_pool_kernel(manage->sst_table_buffers, &manage->a, sst_tbl_s, num_sst_tble);
    }
    if (num_memtable > 0){
        init_struct_pool_kernel(manage->mem_table_buffers, &manage->a, mem_tbl_s, num_memtable);
    }
    register_logical_segments(manage, max_single_buffer);
    manage->sst_tbl_s = sst_tbl_s;
    manage->m_tbl_s = mem_tbl_s;
    
    const int max_concurrent_ops = 0;
    manage->io_requests = create_pool(max_concurrent_ops);
    for (int i = 0 ; i < manage->total_buffers / 4 ; i++){
        struct db_FILE * re = calloc(sizeof(*re), 1);
        re->callback = NULL;
        insert_struct(re, manage);       
    }

}
int allign_to_page_size(int curr_size) {
    if (curr_size % 4096 == 0) {
        return curr_size;
    }
    int to_add = 4096 - (curr_size % 4096);
    return curr_size + to_add;
}
// Modified function to add read/write requests using the segmented approach
int add_read_write_requests(struct io_uring *ring, struct io_manager *manage, struct db_FILE *requests, int seq) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        fprintf(stderr, "Could not get SQE for request\n");
        return -1;
    }
    
    if (seq) {
        sqe->flags |= IOSQE_IO_LINK;
    }
    
    requests->len = allign_to_page_size(requests->len);
    
    if (requests->op == READ) {
        io_uring_prep_read_fixed(sqe, requests->desc.fd, requests->buf->buffy, 
                              requests->len, requests->offset, requests->buf->id);
    } else if (requests->op == WRITE) {
        io_uring_prep_write_fixed(sqe, requests->desc.fd, requests->buf->buffy, 
                               requests->len, requests->offset, requests->buf->id);
    }
    io_uring_sqe_set_data(sqe, requests);
    if (!io_uring_sq_space_left(ring)){
        int res = io_uring_submit(ring);
        if (res < 0) perror("submit fail");
        
    }
    return 0;
}
void cleanup_io_uring(struct io_uring *ring) {
    io_uring_queue_exit(ring);
}
int add_open_close_requests(struct io_uring *ring, struct db_FILE * requests, int seq) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        fprintf(stderr, "Could not get SQE for request\n" );
        return -1;
    }
    if (seq){
        sqe->flags|= IOSQE_IO_LINK;
    }
    if (requests->op == OPEN){
        io_uring_prep_openat(sqe, AT_FDCWD, requests->desc.fn,requests->flags, requests->perms);
    }
    else if (requests->op == CLOSE){
        io_uring_prep_close(sqe,requests->desc.fd);
    }
    io_uring_sqe_set_data(sqe, requests);
    if (!io_uring_sq_space_left(ring)){
        int res = io_uring_submit(ring);
        if (res < 0) perror("submit fail");

    }
    return 0;
}
int process_completions(struct io_uring *ring) {
    if (!ring) return 0;
    
    struct io_uring_cqe *cqes[DEPTH * 2]; 
    int count = io_uring_peek_batch_cqe(ring, cqes, DEPTH * 2);
    if (count <= 0) return 0;

    for (int i = 0; i < count; i++) {
        struct db_FILE *io = io_uring_cqe_get_data(cqes[i]);
        if (!io) continue;

        io->response_code = cqes[i]->res;
        if (cqes[i]->res < 0) {
            perror(strerror(cqes[i]->res));
        }
        if (io->callback) {
            io->callback(io->callback_arg);
        }
        else{
            std_io_callback(io->callback_arg);
        }
        
    }

    io_uring_cq_advance(ring, count);  // advance by actual batch count
    return count;
}
byte_buffer * select_buffer(struct io_manager * m, int size){
    if (size <= 4 * 1024){
        return request_struct(m->four_kb);
    }
    if (size < m->m_tbl_s){
        return request_struct(m->m_tbl_s);
    }
    if (size < m->sst_tbl_s){
        return request_struct(m->sst_tbl_s);
    }
    return NULL;
}
int chain_open_op_close(struct io_uring *ring, struct io_manager * m, struct db_FILE * req){
    int r_w_flag = 0;
    if (req->op == READ) {
        r_w_flag = O_RDONLY | O_DIRECT;
    }
    else {
        r_w_flag = O_WRONLY | O_CREAT | O_APPEND | O_DIRECT;
    }
    mode_t perms  = S_IRUSR  | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

    int res;
    res = open(req->desc.fn, r_w_flag, perms);
    if (res < 0) return -1;
    req->desc.fd =res;
    if (io_uring_sq_space_left(ring) == 0){
        io_uring_submit(ring);
    }
    res = add_read_write_requests(ring,m, req, 0);
    if(res < 0 ) return -1;
    if (io_uring_sq_space_left(ring) == 0){
        io_uring_submit(ring);
    }
    if (req->op== WRITE){
        res = add_fsync_request(ring, req->desc.fd, 1);
    }
    if(res < 0 ) return -1;
    if (io_uring_sq_space_left(ring) == 0){
        io_uring_submit(ring);
    }
    req->op = CLOSE;
    res = add_open_close_requests(ring,req ,0);
    if (res < 0) return -1;
    return 0;
}
void std_io_callback(void * arg){
    task_t * task = arg;
    task->stat = RUNNING;
    task_q_enqueue(task->active, task);
}
inline int do_write(struct db_FILE * req, off_t offset, size_t len, struct io_manager * manager){
    req->op = WRITE;
    req->len = len;
    add_read_write_requests(&manager->ring,manager, req, 0);
    yield();
    return req->response_code;
}
inline int do_read(struct db_FILE * req, off_t offset, size_t len, struct io_manager * manager){   
    req->len = len;
    req->op = READ;
    add_read_write_requests(&manager->ring,manager, req, 0);
    yield();
    return req->response_code;
}
inline int do_open(char * fn, struct db_FILE * req, struct io_manager * manager){
    
    req->op = OPEN;
    add_open_close_requests(&manager->ring,req, 0);
    yield();
    return req->response_code;
}
inline int do_close(int fd, struct db_FILE * req, struct io_manager * manager){
    
    req->op = CLOSE;
    add_open_close_requests(&manager->ring,req, 0);
    yield();
    return req->response_code;
}
inline int do_fsync(int fd, struct io_manager * manager){
    add_fsync_request(&manager->ring,fd, 0);
    yield();
    return 0;
}


