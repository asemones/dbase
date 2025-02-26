#include <stdio.h>
#include "lsm.h"
#include "iter.h"




/*change read_record and write_record to work properly*/
char * get(storage_engine * engine, char * key);
int del(storage_engine * engine, char * key);
char ** scan(storage_engine * engine, char * start, char * end);