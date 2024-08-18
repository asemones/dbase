#include "lsm.h"
#include <stdio.h>


typedef struct iter{
    void * curr_key;
    void * curr_value;
    void * next;
}iter;