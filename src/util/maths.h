#include <math.h>
#include <stdbool.h>
#ifndef MATHS_H
#define MATHS_H


int compare_time_stamp(struct timeval t1, struct timeval t2){
    if (t1.tv_sec < t2.tv_sec) return -1;    
    else if (t1.tv_sec > t2.tv_sec) return 1;

    if (t1.tv_usec < t2.tv_usec) return -1;
    else if (t1.tv_usec > t2.tv_usec) return 1;
    return 0;
}

static inline int max(int a, int b){return a > b ? a : b;}
static inline int min(int a, int b){return a < b ? a : b;}
static inline int ceil_int_div(int dividend, int divsor){
    int floor = dividend/divsor;
    if (dividend % divsor) floor ++;
    return floor;
}
static inline bool is_pwr_2(size_t n){ return n > 0 && (n & (n - 1)) == 0;}
#endif