#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#define RPC_TABLE_SIZE 1024
#define RPC_HASH_MASK (RPC_TABLE_SIZE - 1)

#define SHARD_ID_BITS 16
#define REAL_ID_BITS 48

#define SHARD_ID_MASK (((uint64_t)1 << SHARD_ID_BITS) - 1)
#define REAL_ID_MASK (((uint64_t)1 << REAL_ID_BITS) - 1)
#define SHARD_ID_SHIFT REAL_ID_BITS

#define MAX_ID_COUNTER REAL_ID_MASK

typedef enum {
    ENTRY_EMPTY,
    ENTRY_PENDING,
    ENTRY_COMPLETED,
    ENTRY_TOMBSTONE,
    ENTRY_READ
} entry_state_t;

typedef union future_t {
    int return_code;
    void* value;
} future_t;

typedef struct rpc_table {
    uint64_t shard_id;
    uint64_t id_counter;
    uint64_t keys[RPC_TABLE_SIZE];
    future_t results[RPC_TABLE_SIZE];
    uint8_t states[RPC_TABLE_SIZE];
} rpc_table_t;

static inline uint64_t rpc_encode_id(uint64_t shard_id, uint64_t real_id) {
    return ((shard_id & SHARD_ID_MASK) << SHARD_ID_SHIFT) | (real_id & REAL_ID_MASK);
}

static inline uint64_t rpc_get_shard_id(uint64_t encoded_id) {
    return (encoded_id >> SHARD_ID_SHIFT) & SHARD_ID_MASK;
}

static inline uint64_t rpc_get_real_id(uint64_t encoded_id) {
    return encoded_id & REAL_ID_MASK;
}

static inline void rpc_table_init(rpc_table_t* table, uint64_t shard_id) {
    if (shard_id > SHARD_ID_MASK) {
    }
    
    table->shard_id = shard_id & SHARD_ID_MASK;
    table->id_counter = 1;
    memset(table->keys, 0, sizeof(table->keys));
    memset(table->results, 0, sizeof(table->results));
    memset(table->states, ENTRY_EMPTY, sizeof(table->states));
}

static inline uint32_t rpc_hash(uint64_t id) {
    return (uint32_t)((id ^ (id >> 32)) & RPC_HASH_MASK);
}

static inline uint64_t rpc_start(rpc_table_t* table) {
    for (;;) {
        if (table->id_counter >= MAX_ID_COUNTER) {
            table->id_counter = 1;
        }
        
        uint64_t real_id = table->id_counter++;
        if (real_id == 0) continue;

        uint64_t encoded_id = rpc_encode_id(table->shard_id, real_id);
        
        uint32_t slot = rpc_hash(encoded_id);
        uint32_t orig_slot = slot;

        while (table->states[slot] != ENTRY_EMPTY &&
               table->states[slot] != ENTRY_TOMBSTONE) {
            slot = (slot + 1) & RPC_HASH_MASK;
            if (slot == orig_slot) return 0;
        }

        table->keys[slot] = encoded_id;
        table->states[slot] = ENTRY_PENDING;
        return encoded_id;
    }
}

static inline bool rpc_complete(rpc_table_t* table, uint64_t encoded_id, future_t result) {
    uint32_t slot = rpc_hash(encoded_id);
    uint32_t orig_slot = slot;

    while (true) {
        if (table->states[slot] == ENTRY_EMPTY) return false;

        if (table->keys[slot] == encoded_id &&
            table->states[slot] == ENTRY_PENDING) {
            table->results[slot] = result;
            table->states[slot] = ENTRY_COMPLETED;
            return true;
        }

        slot = (slot + 1) & RPC_HASH_MASK;
        if (slot == orig_slot) break;
    }

    return false;
}

static inline bool rpc_get_result(rpc_table_t* table, uint64_t encoded_id, future_t* out_result) {
    uint32_t slot = rpc_hash(encoded_id);
    uint32_t orig_slot = slot;

    while (true) {
        if (table->states[slot] == ENTRY_EMPTY) return false;

        if (table->keys[slot] == encoded_id) {
            if (table->states[slot] == ENTRY_COMPLETED) {
                if (out_result) *out_result = table->results[slot];
                return true;
            } 
            else {
                return false;
            }
        }

        slot = (slot + 1) & RPC_HASH_MASK;
        if (slot == orig_slot) break;
    }

    return false;
}

static inline void rpc_free(rpc_table_t* table, uint64_t encoded_id) {
    uint32_t slot = rpc_hash(encoded_id);
    uint32_t orig_slot = slot;

    while (true) {
        if (table->states[slot] == ENTRY_EMPTY) return;

        if (table->keys[slot] == encoded_id &&
            table->states[slot] != ENTRY_TOMBSTONE) {
            table->states[slot] = ENTRY_TOMBSTONE;
            return;
        }

        slot = (slot + 1) & RPC_HASH_MASK;
        if (slot == orig_slot) return;
    }
}

static inline bool rpc_is_pending(rpc_table_t* table, uint64_t encoded_id) {
    uint32_t slot = rpc_hash(encoded_id);
    uint32_t orig_slot = slot;

    while (true) {
        if (table->states[slot] == ENTRY_EMPTY) return false;

        if (table->keys[slot] == encoded_id)
            return table->states[slot] == ENTRY_PENDING;

        slot = (slot + 1) & RPC_HASH_MASK;
        if (slot == orig_slot) break;
    }

    return false;
}

static inline bool rpc_is_complete(rpc_table_t* table, uint64_t encoded_id) {
    uint32_t slot = rpc_hash(encoded_id);
    uint32_t orig_slot = slot;

    while (true) {
        if (table->states[slot] == ENTRY_EMPTY) return false;

        if (table->keys[slot] == encoded_id)
            return table->states[slot] == ENTRY_COMPLETED;

        slot = (slot + 1) & RPC_HASH_MASK;
        if (slot == orig_slot) break;
    }

    return false;
}

static inline bool rpc_id_belongs_to_table(rpc_table_t* table, uint64_t encoded_id) {
    return rpc_get_shard_id(encoded_id) == table->shard_id;
}

static inline uint64_t rpc_get_max_id_counter(void) {
    return MAX_ID_COUNTER;
}

static inline bool rpc_is_valid_real_id(uint64_t real_id) {
    return real_id > 0 && real_id <= MAX_ID_COUNTER;
}