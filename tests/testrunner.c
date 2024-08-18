#include "./unity/src/unity.h" 
#include <stdlib.h>
#include <string.h>
#include "testhashtble.h"
#include "testlist.h"
#include "testurl.h"
#include "testrequest.h"
#include "testresponse.h"
#include "testexternals.h"
#include "testhtmlparse.h"
#include "testfrontier.h"
#include "testthreadp.h"
#include "testextractors.h"
#include "testcrawler.h"
#include "testbloom.h"
#include "testio.h"
#include "testbtree.h"
#include "testjson.h"
#include "testmetadata.h"
#include "testlsm.h"
#include "testcompactor.h"
#include "testlru.h"




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
    RUN_TEST(test_get_element_from_NULL_list);
    RUN_TEST(test_get_element_at_invalid_index);
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
    /*
    fprintf(stderr, "RUNNING URL.H TESTS:\n");
    RUN_TEST(test_fromURL_NullURL);
    RUN_TEST(test_fromURL_InvalidProtocol);
    RUN_TEST(test_fromURL_HTTPURL);
    RUN_TEST(test_fromURL_HTTPSURL);
    RUN_TEST(test_freeUrlInfo_Null);
    RUN_TEST(test_freeUrlInfo_Valid);
    RUN_TEST(test_buildSubDirectories);
    RUN_TEST(test_updateURL);
    RUN_TEST(test_attemptRepair_null_url);
    RUN_TEST(test_attemptRepair_empty_url);
    RUN_TEST(test_attemptRepair_add_protocol);
    RUN_TEST(test_attemptRepair_keep_existing_protocol);
    RUN_TEST(test_attemptRepair_strip_hash);
    RUN_TEST(test_attemptRepair_keep_hash);
    RUN_TEST(test_attemptRepair_complex_url);
    RUN_TEST(test_attemptRepair_edge_case);
    fprintf(stderr, "RUNNING REQUEST.H TESTS:\n");
    RUN_TEST(test_fromParams_valid_input);
    RUN_TEST(test_buildHeaderStr);
    RUN_TEST(test_buildReqLine);
    RUN_TEST(test_updateRequest);
    fprintf(stderr, "RUNNING RESPONSE.H TESTS:\n");
    RUN_TEST(test_extractResponseCode_valid_code);
    RUN_TEST(test_extractResponseCode_invalid_code);
    RUN_TEST(test_parseHTTPRes_valid_input);
    RUN_TEST(test_parseHTTPRes_invalid_input);
    fprintf(stderr, "RUNNING EXTERNAL FUNCTION TESTS: SEND_REQUEST:\n");
    RUN_TEST(test_SEND_REQUEST_with_null_headers);
    RUN_TEST(test_SEND_REQUEST_with_valid_headers);
    RUN_TEST(test_SEND_REQUEST_with_POST);
    fprintf(stderr, "RUNNING HTMLPARSING.H TESTS:\n");
    RUN_TEST(test_cleanBody_success);
    RUN_TEST(test_cleanBody_failure);
    RUN_TEST(test_getNodeType);
    RUN_TEST(test_findAllText);
    RUN_TEST(test_getAllAttributes);
    RUN_TEST(test_screenAttributes);
    RUN_TEST(test_findTag);
    RUN_TEST(test_yoinkUrl);
    RUN_TEST(test_cleanText);
    fprintf(stderr, "RUNNING THREADPOOL TESTS:\n");
    RUN_TEST(test_threadPool_initialization);
    RUN_TEST(test_addWork_to_threadPool);
    RUN_TEST(test_execute_work_in_threadPool);
    RUN_TEST(test_destroy_threadPool);
    fprintf(stderr, "RUNNING EXTRACTOR TESTS\n");
    RUN_TEST(test_Extractor_creates_extractor);
    RUN_TEST(test_processPageText);
    RUN_TEST(test_extract);
    RUN_TEST(test_FreeExtractor);
    fprintf(stderr, "RUNNING CRAWLER TESTS\n");
    RUN_TEST(test_initCrawl_empty);
    RUN_TEST(test_initCrawl_regular);
    //RUN_TEST(test_updateCrawler);
    RUN_TEST(test_bfs);
    */
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
    fprintf(stderr, "RUNNING IO TESTS\n");
    RUN_TEST(test_writeFile);
    RUN_TEST(test_readFile);
    fprintf(stderr, "RUNNING BTREE TESTS\n");
    RUN_TEST(test_createBTree);
    RUN_TEST(test_insert_and_search_single_entry);
    RUN_TEST(test_insert_multiple_entries);
    RUN_TEST(test_search_nonexistent_entry);
    RUN_TEST(test_insert_duplicate_entry);
    RUN_TEST(test_insert_causing_split);
    fprintf(stderr, "RUNNING JSON TESTS\n");
    RUN_TEST(test_serialize_dict);
    RUN_TEST(test_deserialize_dict);
    RUN_TEST(test_write_read_json);
    RUN_TEST(test_numeric_read_write);
    fprintf(stderr, "RUNNING METADATA TESTS\n");
    RUN_TEST(test_create_metadata_fresh);
    RUN_TEST(test_use_and_load);
    fprintf(stderr, "RUNNING ENGINE TESTS\n");
    RUN_TEST(test_create_engine);
    RUN_TEST(test_write_engine);
    RUN_TEST(test_write_then_read_engine);
    fprintf(stderr, "RUNNING COMPACTION TESTS\n");
    RUN_TEST(test_merge_tables);
    RUN_TEST(test_merge_one_table);
    RUN_TEST(test_merge_0_to_empty_one);
    RUN_TEST(test_merge_0_to_filled_one);
    RUN_TEST(test_attempt_to_break_compactor);
    fprintf(stderr, "RUNNING LRU TESTS\n");
    RUN_TEST(test_cache_init);
    RUN_TEST(test_cache_add_and_get);
    RUN_TEST(test_cache_evict);

    return UNITY_END();
}