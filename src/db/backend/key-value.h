#include <stdlib.h>
#include <stdio.h>
#include<string.h>
#include <stdint.h>
#include "../../ds/byte_buffer.h"
#include "option.h"
#include <stdint.h>

#define INLINE_CAP 8
#define V_LOG_TOMBSTONE UINT16_MAX
/**
 * @brief Basic unit for storing database entries
 * @struct db_unit
 * @param len Length of the entry in bytes
 * @param entry Pointer to the actual data
 */

typedef struct db_unit{
    u_int16_t len;
    union {
        void * entry;
    };
}db_unit;
typedef struct db_large_unit{
    uint64_t len;
    union{
        void * entry;
    };
}db_large_unit;
#pragma once
/* for comparison functions*/
/*NEW STORAGE FORMAT: 
[num entries ... key len A, value len A,....... Key A Value A ]
*/
/**
 * @brief Enumeration of data sources in the LSM tree
 * @enum source
 */
enum source {
    memtable,  /**< Data from memory table */
    lvl_0,     /**< Data from level 0 SST files */
    lvl_1_7    /**< Data from levels 1-7 SST files */
};
/**
 * @brief Structure for merging data from different sources
 * @struct merge_data
 * @param key Pointer to the key db_unit
 * @param value Pointer to the value db_unit
 * @param index Index of the entry
 * @param src Source of the data (memtable, level 0, or levels 1-7)
 */
typedef struct merge_data {
    db_unit * key;
    db_unit * value;
    char index;
    enum source src;
}merge_data;
/**
 * @brief Writes a db_unit to a byte buffer
 * @param b Pointer to the byte buffer
 * @param u The db_unit to write
 * @return Number of bytes written or error code
 */
int write_db_unit(byte_buffer * b, db_unit u);

/**
 * @brief Reads a db_unit from a byte buffer
 * @param b Pointer to the byte buffer
 * @param u Pointer to the db_unit to populate
 */
void read_db_unit(byte_buffer * b, db_unit * u);

/**
 * @brief Loads a block from a byte buffer into a data structure
 * @param stream Pointer to the byte buffer containing the block
 * @param ds Pointer to the data structure
 * @param func Function to call for each entry loaded
 * @return Number of entries loaded
 */
size_t load_block_into_into_ds(byte_buffer *stream, void *ds, void (*func)(void *, void *, void *));

/**
 * @brief Compares two db_units byte by byte
 * @param one First db_unit
 * @param two Second db_unit
 * @return Negative if one < two, 0 if equal, positive if one > two
 */
int byte_wise_comp(db_unit one, db_unit two);

/**
 * @brief Compares two db_units as integers
 * @param one First db_unit
 * @param two Second db_unit
 * @return Negative if one < two, 0 if equal, positive if one > two
 */
int comp_int(db_unit one, db_unit two);

/**
 * @brief Compares two db_units as floats
 * @param one First db_unit
 * @param two Second db_unit
 * @return Negative if one < two, 0 if equal, positive if one > two
 */
int comp_float(db_unit one, db_unit two);

/**
 * @brief Master comparison function that selects the appropriate comparison method
 * @param one Pointer to the first db_unit
 * @param two Pointer to the second db_unit
 * @param max Maximum number of bytes to compare
 * @param dt Data type indicator for selecting comparison method
 * @return Negative if one < two, 0 if equal, positive if one > two
 */
int master_comparison(db_unit *one, db_unit *two, int max, int dt);