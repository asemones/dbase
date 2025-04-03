#include <stdio.h>
#include "key-value.h"
#include "snapshot.h"
#include "lsm.h"
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