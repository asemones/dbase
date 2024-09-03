#include <math.h>
#include <stdbool.h>
#ifndef MATHS_H
#define MATHS_H



int max(int a, int b){return a > b ? a : b;}
int min(int a, int b){return a < b ? a : b;}
bool is_pwr_2(size_t n){ return n > 0 && (n & (n - 1)) == 0;}
#endif