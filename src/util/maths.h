#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#ifndef MATHS_H
#define MATHS_H



static inline int max(int a, int b){return a > b ? a : b;}
static inline int min(int a, int b){return a < b ? a : b;}
static inline int ceil_int_div(int dividend, int divsor){
    int floor = dividend/divsor;
    if (dividend % divsor) floor ++;
    return floor;
}
static inline bool is_pwr_2(size_t n){ return n > 0 && (n & (n - 1)) == 0;}
static inline uint64_t round_up_pow2(uint64_t x) {
    if (x == 0) return 0;
    x--;                            
    x |= x >> 1;  x |= x >> 2;       
    x |= x >> 4;  x |= x >> 8;
    x |= x >> 16; x |= x >> 32;
    return x + 1;                   
}
#endif