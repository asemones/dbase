#include <stdint.h>
#include "hashtbl.h"
#include "../util/io.h"
#include "byte_buffer.h"
#include "../util/alloc_util.h"
#ifndef BLOOMFILTER_H
#define BLOOMFILTER_H

typedef struct bit_array{
    uint64_t * array;
    size_t size;
}bit_array;
typedef struct bloom_filter{
    bit_array* ba;
    size_t num_hash;

}bloom_filter;

uint32_t MurmurHash3_x86_32(void *key, int len, uint32_t seed);
bit_array* create_bit_array(size_t size);
void set_bit(bit_array *ba, size_t index);
void clear_bit(bit_array *ba, size_t index);
bool get_bit(bit_array *ba, size_t index);
void free_bit(bit_array *ba);
bloom_filter* bloom(size_t num_hash, size_t num_word,bool read_file, char * file);
void map_bit(const char * url, bloom_filter *filter);
bool check_bit(const char * url, bloom_filter *filter);
void free_filter( void *filter);
void dump_filter(bloom_filter *filter, char * file);
bloom_filter * from_stream(byte_buffer * buffy_the_buffer);
void copy_filter(bloom_filter * filter, byte_buffer* buffer);
bloom_filter* deep_copy_bloom_filter(const bloom_filter* original);
#endif

