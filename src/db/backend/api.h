#include <stdio.h>
#include "lsm.h"
#include "iter.h"


/*all operations do not preform copies of the data and let the user determine whether copying 
is worth it. this is because due to the design of the db, there is no event that occurs 
where a page can be accessed and the user will not have the chance to decide whether to copy
values into memory. For safety purposes, transcations will copy memory by default unless
NULL is provided into the arena for transcation execute function. Not copying is not reccomended
 .*/
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