#include "./unity/src/unity.h" 
#include <stdlib.h>
#include <string.h>
#include "testhashtble.h"
#include "testlist.h"
#include "testfrontier.h"
#include "testthreadp.h"
#include "testbloom.h"
#include "testio.h"
#include "testmetadata.h"
#include "testlsm.h"
#include "testcompactor.h"
#include "test_iter.h"
#include "test_iter.h"
#include "testwal.h"
#include "testcompress.h"
#include "test scheduler.h"



void setUp (void) {} 
void tearDown (void) {} 

int main(void) {
    UNITY_BEGIN();
    fprintf(stderr, "RUNNING LIST TESTS:\n");
    RUN_TEST(test_create_list);
    RUN_TEST(test_insert_elements);
    RUN_TEST(test_insert_at_specific_index);
    RUN_TEST(test_remove_element_at_specific_index);
    RUN_TEST(test_list_expansions);
    RUN_TEST(test_access_elements);
    RUN_TEST(test_free_list_elements);
    RUN_TEST(test_free_list);
    RUN_TEST(test_insert_into_NULL_list);
    RUN_TEST(test_insert_NULL_element);
    RUN_TEST(test_insert_at_invalid_index);
    RUN_TEST(test_remove_from_NULL_list);
    RUN_TEST(test_remove_at_invalid_index);
    RUN_TEST(test_at_from_NULL_list);
    RUN_TEST(test_at_at_invalid_index);
    RUN_TEST(test_sortList_ascending);
    RUN_TEST(test_sortList_descending);
    RUN_TEST(test_sortList_empty);
    RUN_TEST(test_sortList_single_element);
    fprintf(stderr, "RUNNING DICT/HASH TESTS:\n");
    RUN_TEST(test_fnv1a_64);
    RUN_TEST(test_dict_creation);
    RUN_TEST(test_addKV_and_getV);
    RUN_TEST(test_removeKV);
    RUN_TEST(test_freeDict);
    fprintf(stderr, "RUNNING HEAP/PQ TESTS:\n");
    RUN_TEST(test_enqueue);
    RUN_TEST(test_dequeue);
    RUN_TEST(test_peek);
    RUN_TEST(test_freeFront);
    fprintf(stderr, "RUNNING BLOOM FILTER TESTS\n");
    RUN_TEST(test_create_bit_array);
    RUN_TEST(test_set_and_get_bit);
    RUN_TEST(test_clear_bit);
    RUN_TEST(test_multiple_operations);
    RUN_TEST(test_bloom_creation);
    RUN_TEST(test_map_and_check_bit);
    RUN_TEST(test_multiple_urls);
    RUN_TEST(test_null_and_empty_url);
    RUN_TEST(test_dumpFilter);
    fprintf(stderr, "RUNNING MULTI TASK TESTS\n");
    RUN_TEST(test_tasks_no_io);
    RUN_TEST(test_tasks_io);
    fprintf(stderr, "RUNNING IO TESTS\n");
    RUN_TEST(test_writeFile);
    RUN_TEST(test_readFile);
    RUN_TEST(test_io_uring_write);
    RUN_TEST(test_io_uring_read);
    RUN_TEST(test_async_io_with_event_loop);
    RUN_TEST(test_benchmark_io_performance);
    fprintf(stderr,"Running compression tests\n");
    RUN_TEST(test_regular_compress);
    RUN_TEST(test_regular_decompress);
    RUN_TEST(test_dict_compress);
    RUN_TEST(test_dict_decompress);
    fprintf(stderr, "RUNNING METADATA TESTS\n");
    RUN_TEST(test_create_metadata_fresh);
    RUN_TEST(test_use_and_load);
    fprintf(stderr, "RUNNING ENGINE TESTS\n");
    RUN_TEST(test_create_engine);
    RUN_TEST(test_write_engine);
    RUN_TEST(test_write_then_read_engine);
    fprintf(stderr, "RUNNING COMPACTION TESTS\n");
    RUN_TEST(fat_db_passive);
    RUN_TEST(random_values_short);
    RUN_TEST(random_values_long);
    fprintf(stderr,"RUNNING ITER TESTS\n");
    RUN_TEST(test_init_iters);
    RUN_TEST(test_seek);
    RUN_TEST(test_next);
    RUN_TEST(test_run_scan);
    RUN_TEST(test_full_table_scan_messy);
    fprintf(stderr, "RUNNING WAL TESTS\n");
    RUN_TEST(test_wal_init);
    RUN_TEST(test_seralize_wal);
    RUN_TEST(test_deseralize_wal);
    RUN_TEST(test_write_WAL);
    return UNITY_END();
}