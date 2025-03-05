#include <stdio.h>
#include <stdlib.h>
#include "option.h"
#include "../../ds/byte_buffer.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "../../util/io.h"
#include "key-value.h"
#include "../../util/error.h"
#include "../../ds/list.h"



#define FS_B_SIZE 512
#define MAX_WAL_FN_LEN 20
#pragma once

/**
 * @brief Write-Ahead Log structure for durability and crash recovery
 * @struct WAL
 * @param fd File descriptor for the current WAL file
 * @param wal_meta File pointer to the WAL metadata file
 * @param fn List of WAL filenames
 * @param fn_buffer Buffer for filename operations
 * @param wal_buffer Buffer for WAL operations
 * @param len Total length of the WAL
 * @param curr_fd_bytes Current number of bytes written to the current file
 */
typedef struct WAL {
    int fd;
    FILE * wal_meta;
    list * fn;
    byte_buffer * fn_buffer;
    byte_buffer * wal_buffer;
    size_t len;
    size_t curr_fd_bytes;
}WAL;

/**
 * @brief Serializes a WAL structure to a byte buffer
 * @param w Pointer to the WAL structure
 * @param b Pointer to the byte buffer to serialize to
 */
void seralize_wal(WAL *w, byte_buffer *b);

/**
 * @brief Deserializes a WAL structure from a byte buffer
 * @param w Pointer to the WAL structure to populate
 * @param b Pointer to the byte buffer to deserialize from
 */
void deseralize_wal(WAL *w, byte_buffer *b);

/**
 * @brief Adds a new WAL file to the WAL structure
 * @param w Pointer to the WAL structure
 * @return Pointer to the new WAL filename
 */
char * add_new_wal_file(WAL *w);

/**
 * @brief Initializes a new WAL structure
 * @param b Pointer to a byte buffer for initialization
 * @return Pointer to the newly initialized WAL structure
 */
WAL* init_WAL(byte_buffer *b);

/**
 * @brief Writes a key-value pair to the WAL
 * @param w Pointer to the WAL structure
 * @param key The key to write
 * @param value The value to write
 * @return 0 on success, error code on failure
 */
int write_WAL(WAL *w, db_unit key, db_unit value);

/**
 * @brief Closes and cleans up a WAL structure
 * @param w Pointer to the WAL structure to clean up
 * @param temp Temporary byte buffer for cleanup operations
 */
void kill_WAL(WAL *w, byte_buffer *temp);
