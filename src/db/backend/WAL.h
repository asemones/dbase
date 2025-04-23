#ifndef WAL_H
#define WAL_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h> // For bool type
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "option.h"
#include "../../ds/byte_buffer.h"
#include "../../util/io.h" // For db_FILE
#include "../../util/io_types.h" // For db_FILE definition
#include "key-value.h"
#include "../../util/error.h"
#include "../../ds/structure_pool.h" // Include for struct_pool

// Define constants (Consider moving these to option.h or a config)
#define NUM_WAL_SEGMENTS 4 // Example: Number of WAL segments
#define WAL_SEGMENT_SIZE (GLOB_OPTS.WAL_SIZE) // Use existing option for size per segment
#define MAX_WAL_SEGMENT_FN_LEN 32 // Max length for segment filenames like "WAL_SEG_0.bin"

/**
 * @brief Represents a single segment of the WAL.
 */
typedef struct WAL_segment {
    char filename[MAX_WAL_SEGMENT_FN_LEN]; 
    size_t current_size;                 
    bool active;                          
    db_FILE * model;         
    // Removed mutex
} WAL_segment;

/**
 * @brief Manages the set of WAL segments.
 */
typedef struct WAL_segments_manager {
    WAL_segment segments[NUM_WAL_SEGMENTS]; // Array of WAL segments
    int current_segment_idx;                // Index of the currently active segment (0 to NUM_WAL_SEGMENTS-1)
    size_t segment_capacity;                // Max size for each segment (WAL_SEGMENT_SIZE)
    int num_segments;                       // Number of segments (NUM_WAL_SEGMENTS)
} WAL_segments_manager;

/**
 * @brief Write-Ahead Log structure using rotating segments and async I/O.
 * @struct WAL
 * @param segments_manager Manages the individual WAL file segments and their db_FILE pools.
 * @param meta_ctx File context for the WAL metadata file (assumed synchronous or handled separately).
 * @param wal_buffer Current write buffer for accumulating WAL entries.
 * @param total_len Total number of records written across all segments (logical length).
 */
typedef struct WAL {
    WAL_segments_manager segments_manager;
    struct db_FILE *meta_ctx;
    byte_buffer *wal_buffer;
    size_t total_len; // Logical total number of entries/operations written
    // Removed flush_lock
} WAL;

// --- Public API (Signatures unchanged from original) ---

/**
 * @brief Initializes a new WAL structure using rotating segments.
 * Reads metadata if available, otherwise creates new segments and pools.
 * @param b Pointer to a byte buffer, primarily used internally for metadata read/write.
 * @return Pointer to the newly initialized WAL structure, or NULL on failure.
 */
WAL* init_WAL(byte_buffer *b); // Signature kept as original

/**
 * @brief Writes a key-value pair to the WAL. Handles segment rotation and buffer flushing asynchronously.
 * @param w Pointer to the WAL structure.
 * @param key The key to write.
 * @param value The value to write.
 * @return 0 on success, error code on failure.
 */
int write_WAL(WAL *w, db_unit key, db_unit value); // Signature kept as original

/**
 * @brief Closes WAL segments, writes final metadata, and cleans up resources. Ensures pending async ops complete.
 * @param w Pointer to the WAL structure to clean up.
 * @param temp Temporary byte buffer, primarily used internally for metadata write.
 */
void kill_WAL(WAL *w);// Signature kept as original


// --- Internal functions (Should be static in .c file ideally) ---

/**
 * @brief Serializes the essential WAL state to a byte buffer for metadata persistence.
 * @param w Pointer to the WAL structure.
 * @param b Pointer to the byte buffer to serialize to.
 */
void serialize_wal_metadata(WAL *w, byte_buffer *b);

/**
 * @brief Deserializes the WAL state from a metadata byte buffer.
 * @param w Pointer to the WAL structure to populate.
 * @param b Pointer to the byte buffer containing metadata.
 * @return true if deserialization was successful and state was loaded, false otherwise.
 */
bool deserialize_wal_metadata(WAL *w, byte_buffer *b);

/**
 * @brief Switches to the next WAL segment logically. Does not open/close files.
 * @param w Pointer to the WAL structure.
 * @return 0 on success, error code on failure.
 */
int rotate_wal_segment(WAL *w);

/**
 * @brief Flushes the internal write buffer to the current segment file asynchronously using dbio.
 * @param w Pointer to the WAL structure.
 * @return Number of bytes submitted for writing, or negative on error submitting.
 */
int flush_wal_buffer(WAL *w, db_unit k, db_unit v);


#endif // WAL_H
