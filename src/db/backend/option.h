
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
    size_t WAL_CLEAR_SIZE;
    size_t WAL_BUFFERING_SIZE;
    double SST_TABLE_SIZE_SCALAR;
    char *WAL_F_N;
    bool WAL;
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
    opt->WAL_CLEAR_SIZE = 1024 *1024*10;
    opt->WAL_BUFFERING_SIZE =0;
    opt->LRU_CACHE_SIZE = 32 * 1024 * 1024;
    opt->WAL =true;
    opt->WAL_F_N = "WAL.bin";
}
void set_debug_defaults(option * opt){
    opt->SST_TABLE_SIZE = 64 * 1024;
    opt->MEM_TABLE_SIZE = 64 * 1024;
    opt->SST_TABLE_SIZE_SCALAR = 1;
    opt->BLOCK_INDEX_SIZE = 4 * 1024;
    opt->LEVEL_0_SIZE = 400 * 1024;
    opt->LEVEL_SCALAR = 2;
    opt->NUM_COMPACTOR_UNITS = 3;
    opt->READ_WORKERS = 3;
    opt->WRITE_WORKERS = 3;
    opt->LRU_CACHE_SIZE = 1024 * 1024;
    opt->WAL_BUFFERING_SIZE = 32 * 1024;
    opt->WAL_BUFFERING_SIZE = 64 * 1024;
    opt->WAL = true;
    opt->WAL_F_N = "WAL.bin";
}


