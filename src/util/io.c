#include "io.h"
#define DEPTH 64
#define ALLIGNMENT 4 * 1024
struct io_request;
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
int init_struct_pool_kernel(struct_pool * pool, arena * a, struct iovec * ios, int bufsize, int bufnum, int id_counter){
      for (int i = 0; i< bufnum; i++, id_counter++){
        byte_buffer  * buffer = malloc(sizeof(byte_buffer));

        buffer->max_bytes = bufsize;
        buffer->curr_bytes = 0;
        buffer->buffy = arena_alloc(a, bufsize);
        insert_struct(pool, buffer);
        ios[id_counter].iov_base= buffer->buffy;
        ios[id_counter].iov_len = bufsize;
        buffer->id = id_counter;
    }
    return id_counter;
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
void init_io_manager(struct io_manager * manage, int num_4kb, int num_sst_tble, int num_memtable, int sst_tbl_s, int mem_tbl_s){
    // Initialize the pools
    manage->four_kb = create_pool(num_4kb);
    manage->mem_table_buffers = create_pool(num_memtable);
    manage->sst_table_buffers = create_pool(num_sst_tble);
    
   
    size_t total_size = (num_4kb * 4096) + (num_memtable * mem_tbl_s) + (num_sst_tble * sst_tbl_s);
    
   
    manage->a.capacity = 0;
    manage->a.data = aligned_alloc(ALLIGNMENT, total_size);
    if (!manage->a.data) {
        perror("Failed to allocate aligned memory");
        return;
    }
    manage->a.size = total_size;

    int total_buffers = num_4kb + num_memtable + num_sst_tble;
    struct iovec *iovecs = calloc(total_buffers, sizeof(struct iovec));
    if (!iovecs) {
        perror("Failed to allocate iovecs");
        free(manage->a.data);
        manage->a.data = NULL;
        return;
    }
    
 
    int id_counter = 0;
    id_counter = init_struct_pool_kernel(manage->four_kb, &manage->a, iovecs, 4 * 1024, num_4kb, id_counter);
    id_counter = init_struct_pool_kernel(manage->sst_table_buffers, &manage->a, iovecs, sst_tbl_s, num_sst_tble, id_counter);
    id_counter = init_struct_pool_kernel(manage->mem_table_buffers, &manage->a, iovecs, mem_tbl_s, num_memtable, id_counter);
    
 
    int res = setup_io_uring(&manage->ring, DEPTH);
    if (res < 0) {
        perror("Failed to setup io_uring");
        free(iovecs);
        return;
    }
    
    
    res = io_uring_register_buffers(&manage->ring, iovecs, total_buffers);
    if (res < 0) {
        perror("Failed to register buffers");
    }
}
int allign_to_page_size(int curr_size) {
    if (curr_size % 4096 == 0) {
        return curr_size;
    }
    int to_add = 4096 - (curr_size % 4096);
    return curr_size + to_add;
}
void cleanup_io_uring(struct io_uring *ring) {
    io_uring_queue_exit(ring);
}
int chain_open_op_close(struct io_uring *ring, struct io_request * req){
    int r_w_flag = 0;
    if (req->op == READ) {
        r_w_flag = O_RDONLY | O_DIRECT;
    }
    else {
        r_w_flag = O_WRONLY | O_CREAT | O_APPEND | O_DIRECT;
    }
    mode_t perms  = S_IRUSR  | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    struct open_close_req op_cl_req;
    op_cl_req.flags = r_w_flag;
    op_cl_req.perm = perms;
    op_cl_req.desc.fn = req->desc.fn;
    op_cl_req.op = OPEN;

    int res;
    res = open(req->desc.fn, r_w_flag, perms);
    if (res < 0) return -1;
    req->desc.fd =res;
    op_cl_req.desc.fd = res;
    if (io_uring_sq_space_left(ring) == 0){
        io_uring_submit(ring);
    }
    res = add_read_write_requests(ring, req, 0);
    if(res < 0 ) return -1;
    op_cl_req.op = CLOSE;
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
    res = add_open_close_requests(ring,op_cl_req ,0);
    if (res < 0) return -1;
    return 0;
}

int add_open_close_requests(struct io_uring *ring, struct open_close_req requests, int seq) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        fprintf(stderr, "Could not get SQE for request\n" );
        return -1;
    }
    if (seq){
        sqe->flags|= IOSQE_IO_LINK;
    }
    if (requests.op == OPEN){
        io_uring_prep_openat(sqe, AT_FDCWD, requests.desc.fn,requests.flags, requests.perm);
    }
    else if (requests.op == CLOSE){
        io_uring_prep_close(sqe,requests.desc.fd);
    }
    return 0;
}
int add_read_write_requests(struct io_uring *ring, struct io_request * requests, int seq){
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        fprintf(stderr, "Could not get SQE for request\n");
        return -1;
    }
    if (seq){
        sqe->flags|= IOSQE_IO_LINK;
    }
    requests->len = allign_to_page_size(requests->len);

    if (requests->op == READ){
        io_uring_prep_read_fixed(sqe, requests->desc.fd, requests->buf->buffy, requests->len, requests->offset, requests->buf->id);
    }
    else if (requests->op == WRITE){
        io_uring_prep_write_fixed(sqe, requests->desc.fd, requests->buf->buffy, requests->len, requests->offset, requests->buf->id);
    }
    io_uring_sqe_set_data(sqe, requests);
    return 0;

}
int process_completions(struct io_uring *ring) {
    struct io_uring_cqe *cqe;
    struct io_request *io;
    int processed = 0;
    unsigned head = 0;
    
    io_uring_for_each_cqe(ring, head, cqe) {
        io = io_uring_cqe_get_data(cqe);
        if (cqe->res < 0){
            perror(strerror(cqe->res));
        }
        processed++;
        if (!io){
            continue;
        }
        io->response_code = cqe->res;
        if (cqe->res < 0){
            perror(strerror(cqe->res));
        }
        if (io->callback) {
            io->callback(io->callback_arg);
        }
    }
    io_uring_cq_advance(ring, processed);
    
    return processed;
}


