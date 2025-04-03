#include <math.h>
#include <stdbool.h>
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
#endif