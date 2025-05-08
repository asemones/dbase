#ifndef BLOCKED_BLOOM_H
#define BLOCKED_BLOOM_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include "hashtbl.h"
#include <immintrin.h>
#include "../../db/backend/key-value.h"
#define BB_HAS_AVX2 1


typedef struct {
    uint32_t *data;
    uint32_t bucket_mask;
    uint32_t bucket_cnt;
} bb_filter;

#define BB_BUCKET_BITS   512u
#define BB_BUCKET_WORDS  16u
#define BB_K             8u
#define C_LINE 64
static inline uint64_t bb_mix64(uint64_t x){
    x += 0x9E3779B97f4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

static uint32_t bb_ceil_pow2_u32(uint32_t x){
    if (x <= 1) return 1;
    --x; x |= x >> 1; x |= x >> 2; x |= x >> 4; x |= x >> 8; x |= x >> 16;
    return x + 1;
}
static inline seralize_filter(byte_buffer * b, bb_filter f){
    write_int32(b, f.bucket_mask);
    write_int32(b, f.bucket_cnt);
    uint64_t size = C_LINE * f.bucket_cnt;
    write_buffer(b, f.data, size);
}
static inline deseralize_filter(byte_buffer * b, bb_filter f){
    f.bucket_mask = read_int32(b);
    f.bucket_cnt = read_int32(b);
    uint64_t size = C_LINE * f.bucket_cnt;
    read_buffer(b, f.data, size);
}
static int bb_filter_init_pow2(bb_filter *f, uint32_t log2_buckets){
    f->bucket_cnt = 1u << log2_buckets;
    f->bucket_mask = f->bucket_cnt - 1u;
    size_t bytes = (size_t)f->bucket_cnt * 64u;
    return 0;
}

static int bb_filter_init_capacity(bb_filter *f, size_t keys, double bits_per_key){
    double total_bits = (double)keys * bits_per_key;
    uint64_t buckets = (uint64_t)(total_bits / BB_BUCKET_BITS + 0.5);
    if (buckets == 0) buckets = 1;
    buckets = bb_ceil_pow2_u32(buckets);
    return bb_filter_init_pow2(f, (uint32_t)__builtin_ctz(buckets));
}
static inline void bb_calc_bucket_bits(uint64_t h, const bb_filter *f, uint32_t *bucket_out, uint32_t bits[BB_K]){
    *bucket_out = (uint32_t)h & f->bucket_mask;
    uint64_t x = bb_mix64(h);
    uint64_t y = bb_mix64(x);
    for (uint32_t i = 0; i < BB_K; ++i) {
        x = bb_mix64(x + y + i);
        bits[i] = (uint32_t)(x & 0x1FFu);
    }
}

static inline bool bb_bucket_test_scalar(uint32_t *bucket, const uint32_t bits[BB_K]){
    for (uint32_t i = 0; i < BB_K; ++i) {
        uint32_t bit = bits[i];
        uint32_t word = bit >> 5;
        uint32_t mask = 1u << (bit & 31);
        if (!(bucket[word] & mask)) return false;
    }
    return true;
}

static inline void bb_bucket_set_scalar(uint32_t *bucket, const uint32_t bits[BB_K]){   
    for (uint32_t i = 0; i < BB_K; ++i) {
        uint32_t bit = bits[i];
        uint32_t word = bit >> 5;
        uint32_t mask = 1u << (bit & 31);
        bucket[word] |= mask;
    }
}

static inline bool bb_bucket_test_avx2(uint32_t *bucket, const uint32_t bits[BB_K]){
    uint32_t lo[8] = {0}, hi[8] = {0};
    for (uint32_t i = 0; i < BB_K; ++i) {
        uint32_t b = bits[i];
        uint32_t w = b >> 5;
        uint32_t m = 1u << (b & 31);
        if (w < 8) lo[w] |= m;
        else hi[w - 8] |= m;
    }
    __m256i v0 = _mm256_load_si256((const __m256i*)bucket);
    __m256i v1 = _mm256_load_si256((const __m256i*)(bucket + 8));
    __m256i m0 = _mm256_loadu_si256((const __m256i*)lo);
    __m256i m1 = _mm256_loadu_si256((const __m256i*)hi);
    __m256i t0 = _mm256_and_si256(v0, m0);
    __m256i t1 = _mm256_and_si256(v1, m1);
    __m256i c0 = _mm256_cmpeq_epi32(t0, m0);
    __m256i c1 = _mm256_cmpeq_epi32(t1, m1);
    int ok0 = _mm256_movemask_epi8(c0) + 1;
    int ok1 = _mm256_movemask_epi8(c1) + 1;
    return (ok0 & ok1) == 0;
}

static inline void bb_bucket_set_avx2(uint32_t *bucket, const uint32_t bits[BB_K]){
    uint32_t lo[8] = {0}, hi[8] = {0};
    for (uint32_t i = 0; i < BB_K; ++i) {
        uint32_t b = bits[i];
        uint32_t w = b >> 5;
        uint32_t m = 1u << (b & 31);
        if (w < 8) lo[w] |= m;
        else hi[w - 8] |= m;
    }
    __m256i v0 = _mm256_load_si256((const __m256i*)bucket);
    __m256i v1 = _mm256_load_si256((const __m256i*)(bucket + 8));
    __m256i m0 = _mm256_loadu_si256((const __m256i*)lo);
    __m256i m1 = _mm256_loadu_si256((const __m256i*)hi);
    v0 = _mm256_or_si256(v0, m0);
    v1 = _mm256_or_si256(v1, m1);
    _mm256_store_si256((__m256i*)bucket, v0);
    _mm256_store_si256((__m256i*)(bucket + 8), v1);
}


static inline bool bb_filter_may_contain(const bb_filter *f,  db_unit key){
    uint32_t bucket_idx, bits[BB_K];
    uint64_t hash = fnv1a_64(key.entry, key.len);
    bb_calc_bucket_bits(hash, f, &bucket_idx, bits);
    uint32_t *bucket = f->data + (size_t)bucket_idx * BB_BUCKET_WORDS;
#if BB_HAS_AVX2
    return bb_bucket_test_avx2(bucket, bits);
#else
    return bb_bucket_test_scalar(bucket, bits);
#endif
}

static inline void bb_filter_add(bb_filter *f, db_unit key){
    uint32_t bucket_idx, bits[BB_K];
    uint64_t hash = fnv1a_64(key.entry, key.len);
    bb_calc_bucket_bits(hash, f, &bucket_idx, bits);
    uint32_t *bucket = f->data + (size_t)bucket_idx * BB_BUCKET_WORDS;
#if BB_HAS_AVX2
    bb_bucket_set_avx2(bucket, bits);
#else
    bb_bucket_set_scalar(bucket, bits);
#endif
}
#endif