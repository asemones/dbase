#include <stdio.h>
#include "lsm.h"
#include "iter.h"


typedef enum  txn_op_type{
    NO_OP,
    READ,
    PUT,
    DELETE, 
    WRITE_BATCH,
    DELETE_BATCH
} txn_op_type;
union result
{
    int error_code;
    db_unit resp_v;
};

typedef struct txn_op {
    txn_op_type type;
    db_unit *key;       
    db_unit *value;
} txn_op;
typedef enum {
    TXN_INACTIVE,
    TXN_ACTIVE,
    TXN_COMMITTED,
    TXN_ROLLED_BACK,
    TXN_ABORTED
} txn_state;

typedef struct txn{
    list * ops;
    list * results;
    int id;
    snapshot * snapshot;
    txn_state state;
}txn;
/*change read_record and write_record to work properly*/

/**
 * @brief Retrieves the value associated with a key from the storage engine
 * @param engine Pointer to the storage engine instance
 * @param key The key to look up
 * @return Pointer to the value if found, NULL otherwise
 */
char * get(storage_engine * engine, char * key);

/**
 * @brief Deletes a key-value pair from the storage engine
 * @param engine Pointer to the storage engine instance
 * @param key The key to delete
 * @return 0 on success, non-zero on failure
 */
int del(storage_engine * engine, char * key);

/**
 * @brief Scans the storage engine for keys in a range
 * @param engine Pointer to the storage engine instance
 * @param start The starting key of the range (inclusive)
 * @param end The ending key of the range (exclusive)
 * @return Array of values for keys in the range, NULL-terminated
 */
char ** scan(storage_engine * engine, char * start, char * end);