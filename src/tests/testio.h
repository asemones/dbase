#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <assert.h>
#include <errno.h>
#include "unity/src/unity.h"
#include "../util/io.h"
#include "../util/alloc_util.h"
#include "../ds/circq.h"
#include "../ds/structure_pool.h"
#include "../ds/byte_buffer.h"
#pragma once

// Forward declaration for cleanup_io_uring function
void cleanup_io_uring(struct io_uring *ring);


// Define a simple event loop for testing async operations
typedef struct {
    struct io_uring *ring;
    int running;
    int processed_events;
    pthread_mutex_t mutex_lock;
} event_loop;

// Define a structure to track completion callbacks
typedef struct {
    int completed;
    int success;
    pthread_mutex_t mutex_lock;
    pthread_cond_t cond;
} completion_tracker;

// Define a structure for benchmark results
typedef struct {
    double io_uring_time;
    double file_time;
    size_t bytes_processed;
    int operations;
} benchmark_result;

// Helper function to initialize a completion tracker
void init_completion_tracker(completion_tracker *tracker) {
    tracker->completed = 0;
    tracker->success = 0;
    pthread_mutex_init(&tracker->mutex_lock, NULL);
    pthread_cond_init(&tracker->cond, NULL);
}

// Helper function to clean up a completion tracker
void cleanup_completion_tracker(completion_tracker *tracker) {
    pthread_mutex_destroy(&tracker->mutex_lock);
    pthread_cond_destroy(&tracker->cond);
}

// Callback function for io_uring operations
void io_completion_callback(void *arg) {
    completion_tracker *tracker = (completion_tracker *)arg;

    tracker->completed = 1;
    struct io_request *req = (struct io_request *)((char*)arg - offsetof(struct io_request, callback_arg));
    if (req->response_code >= 0) {
        tracker->success = 1;
    }
    else{
         printf("%d", req->response_code);
         perror(strerror(req->response_code));
    }

}

// Create and initialize an event loop
event_loop* create_event_loop(struct io_uring *ring) {
    event_loop *loop = (event_loop *)malloc(sizeof(event_loop));
    if (!loop) return NULL;
    
    loop->ring = ring;
    loop->running = 0;
    loop->processed_events = 0;
    pthread_mutex_init(&loop->mutex_lock, NULL);
    
    return loop;
}

// Run the event loop
void run_event_loop(event_loop *loop, int max_events) {
    if (!loop) return;
    
    loop->running = 1;
    loop->processed_events = 0;
    
    while (loop->running && (max_events == 0 || loop->processed_events < max_events)) {
        int processed = process_completions(loop->ring);
        if (processed > 0) {
            pthread_mutex_lock(&loop->mutex_lock);
            loop->processed_events += processed;
            pthread_mutex_unlock(&loop->mutex_lock);
        }
        // Small sleep to avoid busy waiting
        usleep(1000);
    }
}

// Stop the event loop
void stop_event_loop(event_loop *loop) {
    if (!loop) return;
    loop->running = 0;
}

// Clean up the event loop
void destroy_event_loop(event_loop *loop) {
    if (!loop) return;
    pthread_mutex_destroy(&loop->mutex_lock);
    free(loop);
}

// Test basic write functionality with io_uring
void test_io_uring_write(void) {
    // Initialize io_manager
    struct io_manager manager;
    init_io_manager(&manager, 4, 2, 2, 4096, 4096);
    
    // Get a buffer from the pool
    byte_buffer *buffer =request_struct(manager.four_kb);
   
    
    // Prepare test data
    const char *test_data = "Hello, io_uring world!";
    size_t data_len = strlen(test_data) + 1;
    memcpy(buffer->buffy, test_data, data_len);
    buffer->curr_bytes = data_len;
    
    // Prepare io_request
    struct io_request req;
    memset(&req, 0, sizeof(req));
    req.desc.fn = "test_io_uring_write.txt";
    req.op = WRITE;
    req.buf = buffer;
    req.len = data_len;
    req.offset = 0;
    int result;
    // Set up completion tracking
    completion_tracker tracker;
    init_completion_tracker(&tracker);
    req.callback = io_completion_callback;
    req.callback_arg = &tracker;
    
    // Submit the write request
    result = chain_open_op_close(&manager.ring, &req);
    TEST_ASSERT_EQUAL_INT(0, result);
    
    // Submit to the ring
    result = io_uring_submit(&manager.ring);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(1, result);
    
    // Process completions
    int max_wait_ms = 1000;  // 5 seconds timeout
    struct timeval timeout;
    gettimeofday(&timeout, NULL);
    timeout.tv_sec += max_wait_ms / 1000;
    
    pthread_mutex_lock(&tracker.mutex_lock);
    while (!tracker.completed) {
        struct timespec ts;
        ts.tv_sec = timeout.tv_sec;
        ts.tv_nsec = timeout.tv_usec * 1000;
        
        int wait_result = pthread_cond_timedwait(&tracker.cond, &tracker.mutex_lock, &ts);
        if (wait_result == ETIMEDOUT) {
            break;
        }
        process_completions(&manager.ring);
    }
    process_completions(&manager.ring);
    pthread_mutex_unlock(&tracker.mutex_lock);
    
    TEST_ASSERT_EQUAL_INT(1, tracker.completed);

    cleanup_completion_tracker(&tracker);
    return_struct(manager.four_kb, buffer, &reset_buffer);
    cleanup_io_uring(&manager.ring);
    remove("test_io_uring_write.txt");
}

// Test basic read functionality with io_uring
void test_io_uring_read(void) {
    const char *test_data = "Hello, io_uring read test!";
    size_t data_len = strlen(test_data) + 1;
    char *file_name = "test_io_uring_read.txt";
    
    FILE *file = fopen(file_name, "w");
    TEST_ASSERT_NOT_NULL(file);
    size_t written = fwrite(test_data, 1, data_len, file);
    fflush(file);
    TEST_ASSERT_EQUAL_INT(data_len, written);
    fclose(file);
    
 
    struct io_manager manager;
    init_io_manager(&manager, 4, 2, 2, 4096, 4096);
    
   
    byte_buffer *buffer = request_struct(manager.four_kb);
   
   
    struct io_request req;
    memset(&req, 0, sizeof(req));
    req.desc.fn = file_name;
    req.op = READ;
    req.buf = buffer;
    req.len = data_len;
    req.offset = 0;
    int result;
    
    completion_tracker tracker;
    init_completion_tracker(&tracker);
    req.callback = io_completion_callback;
    req.callback_arg = &tracker;
    

    result = chain_open_op_close(&manager.ring, &req);
    TEST_ASSERT_EQUAL_INT(0, result);
    
    
    result = io_uring_submit(&manager.ring);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(1, result);

    int max_wait_ms = 1000;  
    struct timeval timeout;
    gettimeofday(&timeout, NULL);
    timeout.tv_sec += max_wait_ms / 1000;
    
    pthread_mutex_lock(&tracker.mutex_lock);
    while (!tracker.completed) {
        struct timespec ts;
        ts.tv_sec = timeout.tv_sec;
        ts.tv_nsec = timeout.tv_usec * 1000;
        
        int wait_result = pthread_cond_timedwait(&tracker.cond, &tracker.mutex_lock, &ts);
        if (wait_result == ETIMEDOUT) {
            break;
        }
        process_completions(&manager.ring);
    }
    pthread_mutex_unlock(&tracker.mutex_lock);    

    TEST_ASSERT_EQUAL_STRING(test_data, buffer->buffy);
    
    // Clean up
    cleanup_completion_tracker(&tracker);
    return_struct(manager.four_kb, buffer, &reset_buffer);
    cleanup_io_uring(&manager.ring);
    remove(file_name);
}


void test_async_io_with_event_loop(void) {

    struct io_manager manager;
    init_io_manager(&manager, 8, 2, 2, 4096, 4096);
    
  
    event_loop *loop = create_event_loop(&manager.ring);
    TEST_ASSERT_NOT_NULL(loop);
    
    
    const int num_ops = 5;
    const char *filenames[num_ops];
    completion_tracker trackers[num_ops];
    struct io_request reqs[num_ops];
    byte_buffer *buffers[num_ops];
    
    
    for (int i = 0; i < num_ops; i++) {
        char *filename = malloc(30);
        sprintf(filename, "test_async_%d.txt", i);
        filenames[i] = filename;
        
        init_completion_tracker(&trackers[i]);
        
      
        buffers[i] = request_struct(manager.four_kb);
        
        
        char data[50];
        sprintf(data, "Async test data %d", i);
        size_t data_len = strlen(data) + 1;
        memcpy(buffers[i]->buffy, data, data_len);
        buffers[i]->curr_bytes = data_len;
        
    
        memset(&reqs[i], 0, sizeof(struct io_request));
        reqs[i].desc.fn = filename;
        reqs[i].op = WRITE;
        reqs[i].buf = buffers[i];
        reqs[i].len = data_len;
        reqs[i].offset = 0;
        reqs[i].callback = io_completion_callback;
        reqs[i].callback_arg = &trackers[i];
    }
    
    
    pthread_t event_thread;
    pthread_create(&event_thread, NULL, (void *(*)(void *))run_event_loop, loop);
    
    
    for (int i = 0; i < num_ops; i++) {
        int result = chain_open_op_close(&manager.ring, &reqs[i]);
        TEST_ASSERT_EQUAL_INT(0, result);
    }
    int res;
    res= io_uring_submit(&manager.ring);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(1, res);
 
    for (int i = 0; i < num_ops; i++) {
        struct timeval timeout;
        gettimeofday(&timeout, NULL);
        timeout.tv_sec += 1;  
        
        pthread_mutex_lock(&trackers[i].mutex_lock);
        while (!trackers[i].completed) {
            struct timespec ts;
            ts.tv_sec = timeout.tv_sec;
            ts.tv_nsec = timeout.tv_usec * 1000;
            
            int wait_result = pthread_cond_timedwait(&trackers[i].cond, &trackers[i].mutex_lock, &ts);
            if (wait_result == ETIMEDOUT) {
                break;
            }
        }
        pthread_mutex_unlock(&trackers[i].mutex_lock);
        
        TEST_ASSERT_EQUAL_INT(1, trackers[i].completed);
        TEST_ASSERT_EQUAL_INT(1, trackers[i].success);
    }
    stop_event_loop(loop);
    pthread_join(event_thread, NULL);

    for (int i = 0; i < num_ops; i++) {
        char expected_data[50];
        sprintf(expected_data, "Async test data %d", i);
        
        char read_buf[50];
        FILE *file = fopen(filenames[i], "r");
        TEST_ASSERT_NOT_NULL(file);
        size_t read_size = fread(read_buf, 1, sizeof(read_buf), file);
        fclose(file);
        
        TEST_ASSERT_GREATER_THAN(0, read_size);
        read_buf[read_size - 1] = '\0';  
        TEST_ASSERT_EQUAL_STRING(expected_data, read_buf);
    }
    for (int i = 0; i < num_ops; i++) {
        cleanup_completion_tracker(&trackers[i]);
        return_struct(manager.four_kb, buffers[i], &reset_buffer);
        remove(filenames[i]);
        free((void*)filenames[i]);
    }
    
    destroy_event_loop(loop);
    cleanup_io_uring(&manager.ring);
}


uint64_t get_time_usec() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

benchmark_result benchmark_write(size_t file_size, size_t block_size, int num_files) {
    benchmark_result result;
    result.bytes_processed = file_size * num_files;
    result.operations = num_files;
    
    char *test_data = malloc(file_size);
    for (size_t i = 0; i < block_size; i++) {
        test_data[i] = 'A' + (i % 26);
    }

    char ** fns=  malloc(sizeof(char *) * num_files);
    for (int i = 0; i < num_files; i++){
        fns[i] = malloc(50);
        sprintf(fns[i], "bench_file_%d.txt", i);
    }
    uint64_t start_time = get_time_usec();
    for (int i = 0; i < num_files; i++) {
        FILE *file = fopen(fns[i], "w");
        fwrite(test_data, 1,file_size, file);
        fsync(file->_fileno);
        fclose(file);
    }
    
    uint64_t end_time = get_time_usec();
    result.file_time = (double)(end_time - start_time) / 1000000.0;  
  
    for (int i = 0; i < num_files; i++) {
        char filename[50];
        sprintf(filename, "bench_file_%d.txt", i);
        remove(filename);
    }
    
    
    struct io_manager manager;
    init_io_manager(&manager, num_files + 4,num_files + 1 ,1, file_size, 4096);
    
 
    completion_tracker *trackers = malloc(num_files * sizeof(completion_tracker));
    byte_buffer **buffers = malloc(num_files * sizeof(byte_buffer*));
    struct io_request *reqs = malloc(num_files * sizeof(struct io_request));
    
    for (int i = 0; i < num_files; i++) {
        init_completion_tracker(&trackers[i]);
        buffers[i] = request_struct(manager.sst_table_buffers);
        
        memcpy(buffers[i]->buffy, test_data, file_size);
        buffers[i]->curr_bytes = file_size;
        
        char *filename = malloc(50);
        sprintf(filename, "bench_uring_%d.txt", i);
        
        memset(&reqs[i], 0, sizeof(struct io_request));
        reqs[i].desc.fn = filename;
        reqs[i].op = WRITE;
        reqs[i].buf = buffers[i];
        reqs[i].len = file_size;
        reqs[i].offset = 0;
        reqs[i].callback = io_completion_callback;
        reqs[i].callback_arg = &trackers[i];
    }
    
    // Start timing
    start_time = get_time_usec();
    // Track total operations (both open and close count as operations)
    int total_ops = num_files * 3; // Assuming each file has open + close
    int submitted = 0;
    int completed = 0;
    int total_submitted =0;

    for (int i = 0; i < num_files; i++) {
        int l = chain_open_op_close(&manager.ring, &reqs[i]);
        if (l < 0) exit(EXIT_FAILURE);
        submitted += 3; 

            if (submitted >= 55 || i == num_files - 1) {
                int ret = io_uring_submit(&manager.ring);
                total_submitted +=ret; 
                if (ret < 0) {
                    fprintf(stderr, "io_uring_submit error: %s\n", strerror(-ret));
                    break;
                }
                submitted = 0;
            }
        int processed = process_completions(&manager.ring);
        if (processed > 0) {
            completed += processed;
        }
    }
    while (completed < total_ops) {
        int processed = process_completions(&manager.ring);
        if (processed > 0) {
            completed += processed;
        }
    }
    
    end_time = get_time_usec();
    result.io_uring_time = (double)(end_time - start_time) / 1000000.0; 
    
    // Clean up
    for (int i = 0; i < num_files; i++) {
        cleanup_completion_tracker(&trackers[i]);
        return_struct(manager.four_kb, buffers[i], &reset_buffer);
        char filename[50];
        sprintf(filename, "bench_uring_%d.txt", i);
        remove(filename);
    }
    
    free(trackers);
    free(buffers);
    free(reqs);
    free(test_data);
    cleanup_io_uring(&manager.ring);
    
    return result;
}

// Test to run the benchmark and report results
void test_benchmark_io_performance(void) {
    printf("\n--- IO Performance Benchmark ---\n");
    
    // Small files benchmark
    benchmark_result small = benchmark_write(4096, 4096, 1000);
    printf("Small files (1000 x 4KB):\n");
    printf("  io_uring: %.6f seconds\n", small.io_uring_time);
    printf("  FILE:     %.6f seconds\n", small.file_time);
    printf("  Speedup:  %.2fx\n", small.file_time / small.io_uring_time);
    
    // Medium files benchmark
    benchmark_result medium = benchmark_write(64 * 1024, 4096, 50);
    printf("Medium files (50 x 64KB):\n");
    printf("  io_uring: %.6f seconds\n", medium.io_uring_time);
    printf("  FILE:     %.6f seconds\n", medium.file_time);
    printf("  Speedup:  %.2fx\n", medium.file_time / medium.io_uring_time);
    
    // Large file benchmark
    benchmark_result large = benchmark_write(1 * 1024 * 1024, 4096, 10);
    printf("Large files (10 x 1MB):\n");
    printf("  io_uring: %.6f seconds\n", large.io_uring_time);
    printf("  FILE:     %.6f seconds\n", large.file_time);
    printf("  Speedup:  %.2fx\n", large.file_time / large.io_uring_time);
    
    printf("-------------------------------\n");
    
    // Make some assertions to validate the benchmark
    TEST_ASSERT_TRUE(small.io_uring_time > 0);
    TEST_ASSERT_TRUE(small.file_time > 0);
    TEST_ASSERT_TRUE(medium.io_uring_time > 0);
    TEST_ASSERT_TRUE(medium.file_time > 0);
    TEST_ASSERT_TRUE(large.io_uring_time > 0);
    TEST_ASSERT_TRUE(large.file_time > 0);
}

// Test the original file read/write functions
void test_writeFile(void) {
    char *data = "Hello, world!";
    size_t data_len = strlen(data) + 1; 
    char *file_name = "test_output.txt";
    
    int result = write_file(data, file_name, "w", 1, data_len);
    TEST_ASSERT_EQUAL_INT(data_len, result);

    // Verify file content
    FILE *file = fopen(file_name, "r");
    TEST_ASSERT_NOT_NULL(file);
    char read_buf[50];
    fread(read_buf, 1, data_len, file);
    fclose(file);
    TEST_ASSERT_EQUAL_STRING(data, read_buf);
    
    // Clean up
    remove(file_name);
}

void test_readFile(void) {
    // First create a file to read
    char *data = "Hello, world!";
    size_t data_len = strlen(data) + 1;
    char *file_name = "test_output.txt";
    
    FILE *file = fopen(file_name, "w");
    TEST_ASSERT_NOT_NULL(file);
    fwrite(data, 1, data_len, file);
    fclose(file);
    
    // Test reading the file
    char read_buf[50];
    int result = read_file(read_buf, file_name, "r", 1, data_len);
    TEST_ASSERT_EQUAL_INT(data_len, result);
    TEST_ASSERT_EQUAL_STRING(data, read_buf);
    
    // Clean up
    remove(file_name);
}