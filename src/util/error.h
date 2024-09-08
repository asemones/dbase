#include <stdio.h>
#include <stdlib.h>
#define MAX_ERROR_MESSAGE_LENGTH 4096


#pragma once


typedef enum error_codes{
    OK,   
    INVALID_DATA,
    INVALID_META_DATA,
    INVALID_BLOCK,
    INVALID_WAL_FILE,
    CRITICAL_MALLOC_FAILURE, //requires immediate shutdown or unrecoverable
    NON_CRITICAL_MALLOC_FAILURE, //can be handled
    FAILED_BUFFER_WRITE,
    SST_F_INVALID,
    FAILED_OPEN,
    FAILED_SEEK,
    FAILED_READ,
    FALED_WRITE,
    FAILED_ARENA,
    STRUCT_NOT_MADE,
    FAILED_TRANSCATION,
    UNKNOWN_ERROR
    

}error_codes;




/*error codes ordered by recency in the call stack*/
/*Each "worker" gets an error call stsck*/
const char * get_error_string(error_codes error);


void create_error_message(int* error_codes, int num_codes, char* buf) {
    if (num_codes <= 0 || buf == NULL) {
        return;
    }

    buf[0] = '\0';  
    int remaining_space = MAX_ERROR_MESSAGE_LENGTH - 1; 

    const char* main_error = get_error_string(error_codes[num_codes - 1]);
    int written = snprintf(buf, remaining_space, "Error: %s", main_error);
    
    if (written > 0) {
        remaining_space -= written;
        buf += written;
    }

    if (num_codes <= 1) return;

    written = snprintf(buf, remaining_space, "\nContributing factors:");
    if (written > 0) {
        remaining_space -= written;
        buf += written;
    }

    for (int i = num_codes - 2; i >= 0 && remaining_space > 0; i--) {
        const char* error_str = get_error_string(error_codes[i]);
        written = snprintf(buf, remaining_space, "\n  - %s", error_str);
        if (written > 0) {
            remaining_space -= written;
            buf += written;
        }
    }
}

const char* get_error_string(error_codes error) {
    switch(error) {
        case OK:
            return "Operation successful";
        case INVALID_DATA:
            return "Invalid data";
        case INVALID_META_DATA:
            return "Invalid metadata";
        case INVALID_BLOCK:
            return "Invalid block";
        case INVALID_WAL_FILE:
            return "Invalid WAL file";
        case CRITICAL_MALLOC_FAILURE:
            return "Critical memory allocation failure";
        case NON_CRITICAL_MALLOC_FAILURE:
            return "Non-critical memory allocation failure";
        case FAILED_BUFFER_WRITE:
            return "Failed to write to buffer";
        case SST_F_INVALID:
            return "Invalid SST file";
        case FAILED_OPEN:
            return "Failed to open file";
        case FAILED_SEEK:
            return "Failed to seek in file";
        case FAILED_READ:
            return "Failed to read from file";
        case FALED_WRITE:
            return "Failed to write to file";
        case FAILED_ARENA:
            return "Failed arena operation";
        case STRUCT_NOT_MADE:
            return "Failed to create structure";
        case FAILED_TRANSCATION:
            return "Failed transaction";
        default:
            return "Unknown error";
    }
}

