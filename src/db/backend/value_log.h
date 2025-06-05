#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#define V_L_FN "v_log"
#include "option.h"
#include "../../ds/byte_buffer.h"
#include "../../util/io_types.h"
#include "../../util/io.h"
#include "key-value.h"
#include "../../util/error.h"
#include "../../ds/structure_pool.h" 
#include "../../util/stringutil.h"
#define BLOB_FN "blob"
/*value log info
user adjustable setting for key-value threshold. the point of this log is to improve lookups with keys while
reducing write amppliifciation. To reduce write amp at a cost of space amp, we may preform ORDERED value log merges
only at the bottom layer(s) of the tree, and keep these ordered v logs segemented 


size tiers:
medium: ??
large: write out to own file, just delete file on delete
consider some sort of sst to blob file map and keep the blobs ordered
*/
typedef struct medium_strategy{
    uint64_t file_size;
    uint64_t seg_size;
} medium_strategy;
typedef struct large_strategy{
    uint64_t blob_counter;
}large_strategy;
typedef struct value_log{
    uint64_t large_thresh;
    uint64_t medium_thresh;
    medium_strategy med;
    large_strategy large;
}value_log;

void init_v_log(value_log * log);
void * get_value(value_log v, db_unit key, db_unit inline_v);
void clean();
void shutdown_v_log(value_log * log);


