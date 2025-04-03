/**
 * io_uring buffer strategy comparison
 * 
 * This program tests two different approaches for using registered buffers
 * with io_uring, both staying within the 1024 buffer registration limit:
 * 
 * 1. Segmented approach: Register a few large buffers and manually manage
 *    segments within them (e.g., 16 large buffers of 1MB each)
 * 
 * 2. Dedicated approach: Register many small dedicated buffers
 *    (e.g., 1024 buffers of 16KB each)
 * 
 * The test performs identical I/O workloads with both approaches and
 * measures throughput, latency, and CPU usage.
 * 
 * Compile: gcc -o buffer_comparison buffer_comparison.c -luring
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <liburing.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <time.h>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <limits.h>
#include <float.h>

// Configuration
#define TEST_FILE               "test_data.bin"
#define TEST_FILE_SIZE          (512 * 1024* 1024)  // 512MB
#define QUEUE_DEPTH             512
#define BATCH_SIZE              512
#define IO_SIZE                 (4 * 1024)          // 16KB per I/O operation
#define TOTAL_OPS               100000               // Total operations to perform
#define WARMUP_OPS              10000                // Warmup operations

// Segmented strategy config
#define SEG_NUM_BUFFERS         1
#define SEG_BUFFER_SIZE         (1 * 1024 * 1024* 4)    // 1MB per buffer
#define SEG_SEGMENTS_PER_BUFFER (SEG_BUFFER_SIZE / IO_SIZE)
#define SEG_TOTAL_SEGMENTS      (SEG_NUM_BUFFERS * SEG_SEGMENTS_PER_BUFFER)

// Dedicated strategy config
#define DED_NUM_BUFFERS         1024
#define DED_BUFFER_SIZE         IO_SIZE              // 16KB per buffer

// I/O pattern
enum io_pattern {
    PATTERN_SEQUENTIAL,
    PATTERN_RANDOM
};

// Strategy
enum strategy {
    STRATEGY_SEGMENTED,
    STRATEGY_DEDICATED
};

// I/O data - passed to completion queue for tracking
struct io_data {
    int buffer_id;          // Buffer ID
    int segment_id;         // Segment within buffer (for segmented strategy)
    size_t offset;          // Offset within buffer
    off_t file_offset;      // Offset within file
    size_t len;             // Length of I/O
    enum strategy strategy; // Which strategy is being used
    struct timespec start;  // Operation start time
    struct timespec end;    // Operation end time
    int completed;          // 1 if completed
    int error;              // Error code if any
};

// Performance statistics
struct perf_stats {
    int total_completed;
    int total_errors;
    double min_latency_us;
    double max_latency_us;
    double avg_latency_us;
    double p95_latency_us;
    double p99_latency_us;
    double throughput_mbps;
    double cpu_usage;
};

// Global data structures
struct global_data {
    // Segmented strategy data
    void **seg_buffers;
    struct iovec *seg_iovecs;
    
    // Dedicated strategy data
    void **ded_buffers;
    struct iovec *ded_iovecs;
    
    // Test data
    struct io_data *io_data_array;
    int *random_offsets;
    
    // Stats
    double *latencies;
};

// Utility functions
double timespec_diff_us(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000000.0 + 
           (end.tv_nsec - start.tv_nsec) / 1000.0;
}

double get_cpu_usage() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_utime.tv_sec + usage.ru_stime.tv_sec + 
           (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1000000.0;
}

int compare_doubles(const void *a, const void *b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    return (da > db) - (da < db);
}

// Create test file with random data
int create_test_file() {
    printf("Creating test file (%d MB)...\n", TEST_FILE_SIZE / (1024 * 1024));
    
    int fd = open(TEST_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Failed to create test file");
        return -1;
    }
    
    // Pre-allocate the file if possible
    #ifdef FALLOC_FL_KEEP_SIZE
    if (fallocate(fd, 0, 0, TEST_FILE_SIZE) < 0) {
        // Fallback to traditional method if fallocate isn't supported
        if (ftruncate(fd, TEST_FILE_SIZE) < 0) {
            perror("Failed to allocate test file");
            close(fd);
            return -1;
        }
    }
    #else
    if (ftruncate(fd, TEST_FILE_SIZE) < 0) {
        perror("Failed to allocate test file");
        close(fd);
        return -1;
    }
    #endif
    
    // Write the file in chunks to avoid excessive memory usage
    const size_t chunk_size = 4 * 1024 * 1024;  // 4MB chunks
    char *buffer = aligned_alloc(4096, chunk_size);
    if (!buffer) {
        perror("Failed to allocate write buffer");
        close(fd);
        return -1;
    }
    
    srand(time(NULL));
    size_t remaining = TEST_FILE_SIZE;
    
    while (remaining > 0) {
        size_t to_write = (remaining < chunk_size) ? remaining : chunk_size;
        
        // Fill buffer with random data
        for (size_t i = 0; i < to_write; i += sizeof(int)) {
            *((int*)(buffer + i)) = rand();
        }
        
        ssize_t written = write(fd, buffer, to_write);
        if (written < 0) {
            perror("Write failed");
            free(buffer);
            close(fd);
            return -1;
        }
        
        remaining -= written;
    }
    
    free(buffer);
    close(fd);
    printf("Test file created successfully.\n");
    return 0;
}

// Initialize the global data structures
int init_global_data(struct global_data *data) {
    // Allocate segmented strategy buffers
    data->seg_buffers = calloc(SEG_NUM_BUFFERS, sizeof(void*));
    data->seg_iovecs = calloc(SEG_NUM_BUFFERS, sizeof(struct iovec));
    if (!data->seg_buffers || !data->seg_iovecs) {
        perror("Failed to allocate segmented buffer arrays");
        return -1;
    }
    
    printf("Allocating segmented strategy buffers: %d buffers x %d MB = %d MB\n", 
           SEG_NUM_BUFFERS, SEG_BUFFER_SIZE / (1024 * 1024), 
           (SEG_NUM_BUFFERS * SEG_BUFFER_SIZE) / (1024 * 1024));
    
    for (int i = 0; i < SEG_NUM_BUFFERS; i++) {
        data->seg_buffers[i] = aligned_alloc(4096, SEG_BUFFER_SIZE);
        if (!data->seg_buffers[i]) {
            perror("Failed to allocate segmented buffer");
            return -1;
        }
        data->seg_iovecs[i].iov_base = data->seg_buffers[i];
        data->seg_iovecs[i].iov_len = SEG_BUFFER_SIZE;
    }
    
    // Allocate dedicated strategy buffers
    data->ded_buffers = calloc(DED_NUM_BUFFERS, sizeof(void*));
    data->ded_iovecs = calloc(DED_NUM_BUFFERS, sizeof(struct iovec));
    if (!data->ded_buffers || !data->ded_iovecs) {
        perror("Failed to allocate dedicated buffer arrays");
        return -1;
    }
    
    printf("Allocating dedicated strategy buffers: %d buffers x %d KB = %d MB\n", 
           DED_NUM_BUFFERS, DED_BUFFER_SIZE / 1024, 
           (DED_NUM_BUFFERS * DED_BUFFER_SIZE) / (1024 * 1024));
    
    for (int i = 0; i < DED_NUM_BUFFERS; i++) {
        data->ded_buffers[i] = aligned_alloc(4096, DED_BUFFER_SIZE);
        if (!data->ded_buffers[i]) {
            perror("Failed to allocate dedicated buffer");
            return -1;
        }
        data->ded_iovecs[i].iov_base = data->ded_buffers[i];
        data->ded_iovecs[i].iov_len = DED_BUFFER_SIZE;
    }
    
    // Allocate IO data array
    data->io_data_array = calloc(QUEUE_DEPTH * 2, sizeof(struct io_data));
    if (!data->io_data_array) {
        perror("Failed to allocate IO data array");
        return -1;
    }
    
    // Generate random offsets for random I/O pattern
    data->random_offsets = calloc(TOTAL_OPS, sizeof(int));
    if (!data->random_offsets) {
        perror("Failed to allocate random offsets");
        return -1;
    }
    
    const int max_offset = TEST_FILE_SIZE / IO_SIZE - 1;
    srand(time(NULL));
    for (int i = 0; i < TOTAL_OPS; i++) {
        data->random_offsets[i] = rand() % max_offset;
    }
    
    // Allocate latency array for percentile calculations
    data->latencies = calloc(TOTAL_OPS, sizeof(double));
    if (!data->latencies) {
        perror("Failed to allocate latency array");
        return -1;
    }
    
    return 0;
}

// Clean up global data
void cleanup_global_data(struct global_data *data) {
    if (data->latencies) free(data->latencies);
    if (data->random_offsets) free(data->random_offsets);
    if (data->io_data_array) free(data->io_data_array);
    
    if (data->ded_iovecs) free(data->ded_iovecs);
    if (data->ded_buffers) {
        for (int i = 0; i < DED_NUM_BUFFERS; i++) {
            if (data->ded_buffers[i]) free(data->ded_buffers[i]);
        }
        free(data->ded_buffers);
    }
    
    if (data->seg_iovecs) free(data->seg_iovecs);
    if (data->seg_buffers) {
        for (int i = 0; i < SEG_NUM_BUFFERS; i++) {
            if (data->seg_buffers[i]) free(data->seg_buffers[i]);
        }
        free(data->seg_buffers);
    }
}

// Run benchmark with a specific strategy and I/O pattern
struct perf_stats run_benchmark(struct global_data *data, struct io_uring *ring, 
                               enum strategy strategy, enum io_pattern pattern, 
                               int is_write, int warmup) {
    struct perf_stats stats;
    memset(&stats, 0, sizeof(stats));
    stats.min_latency_us = DBL_MAX;
    
    int fd = open(TEST_FILE, is_write ? O_WRONLY | __O_DIRECT : O_RDONLY | __O_DIRECT);
    if (fd < 0) {
        perror("Failed to open test file");
        return stats;
    }
    
    // Start tracking CPU usage
    double cpu_start = get_cpu_usage();
    
    struct timespec test_start, test_end;
    clock_gettime(CLOCK_MONOTONIC, &test_start);
    
    int ops_remaining = warmup ? WARMUP_OPS : TOTAL_OPS;
    int ops_submitted = 0;
    int ops_completed = 0;
    int op_idx = 0;
    int latency_idx = 0;
    
    while (ops_completed < ops_remaining) {
        // Submit operations until queue is full or we've submitted all ops
        while (ops_submitted < ops_remaining && 
               io_uring_sq_space_left(ring) > 0 && 
               ops_submitted - ops_completed < QUEUE_DEPTH) {
            
            struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
            if (!sqe) break;
            
            // Calculate file offset based on pattern
            off_t file_offset;
            if (pattern == PATTERN_SEQUENTIAL) {
                file_offset = (op_idx % (TEST_FILE_SIZE / IO_SIZE)) * IO_SIZE;
            } else {
                file_offset = data->random_offsets[op_idx % TOTAL_OPS] * IO_SIZE;
            }
            
            // Get a free slot in our io_data array
            struct io_data *io_data = &data->io_data_array[op_idx % (QUEUE_DEPTH * 2)];
            memset(io_data, 0, sizeof(struct io_data));
            
            io_data->file_offset = file_offset;
            io_data->len = IO_SIZE;
            io_data->strategy = strategy;
            
            // Set up operation based on strategy
            if (strategy == STRATEGY_SEGMENTED) {
                // For segmented, calculate which buffer and segment to use
                io_data->buffer_id = op_idx % SEG_NUM_BUFFERS;
                io_data->segment_id = (op_idx / SEG_NUM_BUFFERS) % SEG_SEGMENTS_PER_BUFFER;
                io_data->offset = io_data->segment_id * IO_SIZE;
                
                // Prepare actual buffer pointer
                void *buf_ptr = (char*)data->seg_buffers[io_data->buffer_id] + io_data->offset;
                
                if (is_write) {
                    // Fill buffer with a pattern for verification if needed
                    *(int*)buf_ptr = op_idx;
                    io_uring_prep_write_fixed(sqe, fd, buf_ptr, IO_SIZE, 
                                          file_offset, io_data->buffer_id);
                } else {
                    io_uring_prep_read_fixed(sqe, fd, buf_ptr, IO_SIZE, 
                                         file_offset, io_data->buffer_id);
                }
            } else { // STRATEGY_DEDICATED
                // For dedicated, each operation gets its own buffer
                io_data->buffer_id = op_idx % DED_NUM_BUFFERS;
                io_data->offset = 0;
                
                if (is_write) {
                    // Fill buffer with a pattern for verification if needed
                    *(int*)data->ded_buffers[io_data->buffer_id] = op_idx;
                    io_uring_prep_write_fixed(sqe, fd, data->ded_buffers[io_data->buffer_id], 
                                          IO_SIZE, file_offset, io_data->buffer_id);
                } else {
                    io_uring_prep_read_fixed(sqe, fd, data->ded_buffers[io_data->buffer_id], 
                                         IO_SIZE, file_offset, io_data->buffer_id);
                }
            }
            
            // Record start time
            clock_gettime(CLOCK_MONOTONIC, &io_data->start);
            io_uring_sqe_set_data(sqe, io_data);
            
            ops_submitted++;
            op_idx++;
        }
        
        // Submit a batch of operations
        int submitted = io_uring_submit(ring);
        if (submitted < 0) {
            fprintf(stderr, "io_uring_submit error: %s\n", strerror(-submitted));
            break;
        }
        
        // Reap completions
        struct io_uring_cqe *cqes[BATCH_SIZE];
        int completed = io_uring_peek_batch_cqe(ring, cqes, BATCH_SIZE);
        
        for (int i = 0; i < completed; i++) {
            struct io_data *io_data = io_uring_cqe_get_data(cqes[i]);
            
            // Record completion time
            clock_gettime(CLOCK_MONOTONIC, &io_data->end);
            io_data->completed = 1;
            
            if (cqes[i]->res < 0) {
                io_data->error = -cqes[i]->res;
                stats.total_errors++;
            } else {
                double latency = timespec_diff_us(io_data->start, io_data->end);
                
                // Only track stats for non-warmup runs
                if (!warmup) {
                    if (latency < stats.min_latency_us) stats.min_latency_us = latency;
                    if (latency > stats.max_latency_us) stats.max_latency_us = latency;
                    stats.avg_latency_us += latency;
                    
                    // Save for percentile calculations
                    data->latencies[latency_idx++] = latency;
                }
            }
            
            io_uring_cqe_seen(ring, cqes[i]);
            ops_completed++;
        }
    }
    
    clock_gettime(CLOCK_MONOTONIC, &test_end);
    double cpu_end = get_cpu_usage();
    
    close(fd);
    
    // Calculate final stats
    if (!warmup) {
        stats.total_completed = ops_completed - stats.total_errors;
        
        double test_duration_sec = timespec_diff_us(test_start, test_end) / 1000000.0;
        stats.throughput_mbps = ((double)stats.total_completed * IO_SIZE) / 
                               (test_duration_sec * 1024 * 1024);
        
        stats.avg_latency_us /= stats.total_completed;
        stats.cpu_usage = cpu_end - cpu_start;
        
        // Calculate percentiles
        qsort(data->latencies, latency_idx, sizeof(double), compare_doubles);
        stats.p95_latency_us = data->latencies[(int)(latency_idx * 0.95)];
        stats.p99_latency_us = data->latencies[(int)(latency_idx * 0.99)];
    }
    
    return stats;
}

// Print the benchmark results in a formatted way
void print_results(const char *title, struct perf_stats stats) {
    printf("\n=== %s ===\n", title);
    printf("Completed operations: %d\n", stats.total_completed);
    printf("Error operations:     %d\n", stats.total_errors);
    printf("Throughput:           %.2f MB/s\n", stats.throughput_mbps);
    printf("Latency:\n");
    printf("  Minimum:            %.2f µs\n", stats.min_latency_us);
    printf("  Average:            %.2f µs\n", stats.avg_latency_us);
    printf("  95th percentile:    %.2f µs\n", stats.p95_latency_us);
    printf("  99th percentile:    %.2f µs\n", stats.p99_latency_us);
    printf("  Maximum:            %.2f µs\n", stats.max_latency_us);
    printf("CPU usage:            %.2f seconds\n", stats.cpu_usage);
    printf("CPU usage per op:     %.2f µs\n", (stats.cpu_usage * 1000000) / stats.total_completed);
}

int main() {
    struct global_data data = {0};
    struct io_uring ring;
    int ret;
    
    // Check if test file exists
    if (access(TEST_FILE, F_OK) != 0) {
        if (create_test_file() < 0) {
            return 1;
        }
    }
    
    // Initialize global data
    if (init_global_data(&data) < 0) {
        cleanup_global_data(&data);
        return 1;
    }
    
    // Initialize io_uring
    struct io_uring_params params = {0};
    ret = io_uring_queue_init_params(QUEUE_DEPTH, &ring, &params);
    if (ret < 0) {
        fprintf(stderr, "io_uring_queue_init_params: %s\n", strerror(-ret));
        cleanup_global_data(&data);
        return 1;
    }
    
    printf("\nRunning buffer strategy performance comparison:\n");
    printf("- Queue depth: %d\n", QUEUE_DEPTH);
    printf("- I/O size: %d KB\n", IO_SIZE / 1024);
    printf("- Total operations: %d\n", TOTAL_OPS);
    
    // Test segmented strategy with registered buffers
    printf("\nRegistering segmented strategy buffers (%d buffers)...\n", SEG_NUM_BUFFERS);
    ret = io_uring_register_buffers(&ring, data.seg_iovecs, SEG_NUM_BUFFERS);
    if (ret < 0) {
        fprintf(stderr, "Failed to register segmented buffers: %s\n", strerror(-ret));
        io_uring_queue_exit(&ring);
        cleanup_global_data(&data);
        return 1;
    }
    
    printf("Warming up segmented strategy...\n");
    run_benchmark(&data, &ring, STRATEGY_SEGMENTED, PATTERN_RANDOM, 0, 1);
    
    printf("Running segmented strategy - Sequential Read...\n");
    struct perf_stats seg_seq_read = run_benchmark(
        &data, &ring, STRATEGY_SEGMENTED, PATTERN_SEQUENTIAL, 0, 0);
    
    printf("Running segmented strategy - Random Read...\n");
    struct perf_stats seg_rand_read = run_benchmark(
        &data, &ring, STRATEGY_SEGMENTED, PATTERN_RANDOM, 0, 0);
    
    printf("Running segmented strategy - Sequential Write...\n");
    struct perf_stats seg_seq_write = run_benchmark(
        &data, &ring, STRATEGY_SEGMENTED, PATTERN_SEQUENTIAL, 1, 0);
    
    printf("Running segmented strategy - Random Write...\n");
    struct perf_stats seg_rand_write = run_benchmark(
        &data, &ring, STRATEGY_SEGMENTED, PATTERN_RANDOM, 1, 0);
    
    // Unregister segmented buffers
    io_uring_unregister_buffers(&ring);
    
    // Test dedicated strategy with registered buffers
    printf("\nRegistering dedicated strategy buffers (%d buffers)...\n", DED_NUM_BUFFERS);
    ret = io_uring_register_buffers(&ring, data.ded_iovecs, DED_NUM_BUFFERS);
    if (ret < 0) {
        fprintf(stderr, "Failed to register dedicated buffers: %s\n", strerror(-ret));
        io_uring_queue_exit(&ring);
        cleanup_global_data(&data);
        return 1;
    }
    
    printf("Warming up dedicated strategy...\n");
    run_benchmark(&data, &ring, STRATEGY_DEDICATED, PATTERN_RANDOM, 0, 1);
    
    printf("Running dedicated strategy - Sequential Read...\n");
    struct perf_stats ded_seq_read = run_benchmark(
        &data, &ring, STRATEGY_DEDICATED, PATTERN_SEQUENTIAL, 0, 0);
    
    printf("Running dedicated strategy - Random Read...\n");
    struct perf_stats ded_rand_read = run_benchmark(
        &data, &ring, STRATEGY_DEDICATED, PATTERN_RANDOM, 0, 0);
    
    printf("Running dedicated strategy - Sequential Write...\n");
    struct perf_stats ded_seq_write = run_benchmark(
        &data, &ring, STRATEGY_DEDICATED, PATTERN_SEQUENTIAL, 1, 0);
    
    printf("Running dedicated strategy - Random Write...\n");
    struct perf_stats ded_rand_write = run_benchmark(
        &data, &ring, STRATEGY_DEDICATED, PATTERN_RANDOM, 1, 0);
    
    // Unregister dedicated buffers
    io_uring_unregister_buffers(&ring);
    
    // Print final results
    printf("\n========== PERFORMANCE COMPARISON RESULTS ==========\n");
    
    print_results("SEGMENTED STRATEGY - SEQUENTIAL READ", seg_seq_read);
    print_results("DEDICATED STRATEGY - SEQUENTIAL READ", ded_seq_read);
    
    print_results("SEGMENTED STRATEGY - RANDOM READ", seg_rand_read);
    print_results("DEDICATED STRATEGY - RANDOM READ", ded_rand_read);
    
    print_results("SEGMENTED STRATEGY - SEQUENTIAL WRITE", seg_seq_write);
    print_results("DEDICATED STRATEGY - SEQUENTIAL WRITE", ded_seq_write);
    
    print_results("SEGMENTED STRATEGY - RANDOM WRITE", seg_rand_write);
    print_results("DEDICATED STRATEGY - RANDOM WRITE", ded_rand_write);
    
    // Print summary
    printf("\n========== SUMMARY ==========\n");
    printf("Throughput ratio (Dedicated/Segmented):\n");
    printf("  Sequential Read:  %.2fx\n", ded_seq_read.throughput_mbps / seg_seq_read.throughput_mbps);
    printf("  Random Read:      %.2fx\n", ded_rand_read.throughput_mbps / seg_rand_read.throughput_mbps);
    printf("  Sequential Write: %.2fx\n", ded_seq_write.throughput_mbps / seg_seq_write.throughput_mbps);
    printf("  Random Write:     %.2fx\n", ded_rand_write.throughput_mbps / seg_rand_write.throughput_mbps);
    
    printf("\nLatency ratio (Dedicated/Segmented):\n");
    printf("  Sequential Read:  %.2fx\n", ded_seq_read.avg_latency_us / seg_seq_read.avg_latency_us);
    printf("  Random Read:      %.2fx\n", ded_rand_read.avg_latency_us / seg_rand_read.avg_latency_us);
    printf("  Sequential Write: %.2fx\n", ded_seq_write.avg_latency_us / seg_seq_write.avg_latency_us);
    printf("  Random Write:     %.2fx\n", ded_rand_write.avg_latency_us / seg_rand_write.avg_latency_us);
    
    printf("\nCPU usage ratio (Dedicated/Segmented):\n");
    printf("  Sequential Read:  %.2fx\n", ded_seq_read.cpu_usage / seg_seq_read.cpu_usage);
    printf("  Random Read:      %.2fx\n", ded_rand_read.cpu_usage / seg_rand_read.cpu_usage);
    printf("  Sequential Write: %.2fx\n", ded_seq_write.cpu_usage / seg_seq_write.cpu_usage);
    printf("  Random Write:     %.2fx\n", ded_rand_write.cpu_usage / seg_rand_write.cpu_usage);
    
    // Cleanup
    io_uring_queue_exit(&ring);
    cleanup_global_data(&data);
    
    return 0;
}