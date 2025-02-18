
#include <stdio.h>
#include <stdlib.h>

#pragma  once

typedef struct option{
    size_t SST_TABLE_SIZE;
    size_t MEM_TABLE_SIZE;
    size_t BLOCK_INDEX_SIZE;
    size_t LEVEL_0_SIZE;
    double LEVEL_SCALAR;
    size_t NUM_COMPACTOR_UNITS;
    size_t READ_WORKERS;
    size_t WRITE_WORKERS;
    size_t LRU_CACHE_SIZE;
    size_t WAL_BUFFERING_SIZE;
    int COMPACTOR_WRITE_SIZE;//the number of blocks to write at once
    double SST_TABLE_SIZE_SCALAR;
    bool WAL;
    size_t WAL_SIZE;
    char * WAL_M_F_N;
    char * META_F_N;
    char * BLOOM_F_N;
    size_t MAX_WAL_FILES;
    size_t NUM_FILES_COMPACT_ZER0;
}option;

extern option GLOB_OPTS;
void set_defaults(option * opt){
    opt->SST_TABLE_SIZE = 64 * 1024 * 1024;
    opt->MEM_TABLE_SIZE = 64 * 1024 * 1024;
    opt->SST_TABLE_SIZE_SCALAR = 1.5;
    opt->BLOCK_INDEX_SIZE = 8 * 1024;
    opt->LEVEL_0_SIZE = 256 * 1024 * 1024;
    opt->LEVEL_SCALAR =2;
    opt->NUM_COMPACTOR_UNITS = 3;
    opt->READ_WORKERS = 3;
    opt->WRITE_WORKERS = 3;
    opt->WAL_BUFFERING_SIZE =4096;
    opt->LRU_CACHE_SIZE = 32 * 1024 * 1024;
    opt->WAL =true;
    opt->WAL_M_F_N = "WAL_M.bin";
    opt->COMPACTOR_WRITE_SIZE =10;
    opt->MAX_WAL_FILES = 5;
    opt->WAL_SIZE = opt->MEM_TABLE_SIZE;
    opt->META_F_N = "meta.bin";
    opt->BLOOM_F_N = "bloom.bin";
    opt->NUM_FILES_COMPACT_ZER0 = 5;
}
void set_debug_defaults(option * opt){
    opt->SST_TABLE_SIZE =64* 1024;
    opt->MEM_TABLE_SIZE = 128 * 1024;
    opt->SST_TABLE_SIZE_SCALAR = 1;
    opt->BLOCK_INDEX_SIZE = 8 * 1024;
    opt->LEVEL_0_SIZE = 400 * 1024;
    opt->LEVEL_SCALAR = 2;
    opt->NUM_COMPACTOR_UNITS = 3;
    opt->READ_WORKERS = 3;
    opt->WRITE_WORKERS = 3;
    opt->LRU_CACHE_SIZE =1024 * 1024;
    opt->WAL_BUFFERING_SIZE = 4096;
    opt->WAL = true;
    opt->WAL_M_F_N = "WAL_M.bin";
    opt->META_F_N = "meta.bin";
    opt->BLOOM_F_N = "bloom.bin";
    opt->COMPACTOR_WRITE_SIZE = 2;
    opt->MAX_WAL_FILES = 2;
    opt->WAL_SIZE = opt->MEM_TABLE_SIZE;
    opt->NUM_FILES_COMPACT_ZER0 = 2;
}


