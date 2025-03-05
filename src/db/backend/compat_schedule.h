#include "compactor.h"
#include "iter.h"
#include "lsm.h"
#include <stdio.h>
#include <stdlib.h>
#include "option.h"
#include <pthread.h>
#include <time.h>


/**
 * @brief Structure containing information for a compaction table job
 * @struct compact_tble_info
 * @param cm Pointer to the compaction manager
 * @param job Internal compaction job details
 */
typedef struct compact_tble_info{
    compact_manager * cm;
    compact_job_internal job;
}compact_tble_info;

/**
 * @brief Checks if a level needs compaction
 * @param level Pointer to the list of SST files in the level
 * @param lvl Level number
 * @return true if compaction is needed, false otherwise
 */
/**
 * @brief Checks if a level needs compaction.
 * @param level Pointer to the list of SST files in the level.
 * @param lvl Level number.
 * @return true if compaction is needed, false otherwise.
 */
bool check_for_compact(list * level, int lvl);

/**
 * @brief Marks an SST file for compaction and grabs it.
 * @param my_list Pointer to the list of SST files.
 * @param victim Pointer to the SST file to mark.
 * @param index Index of the SST file in the list.
 * @return The marked SST file information.
 */
sst_f_inf sst_mark_and_grab(list * my_list, sst_f_inf * victim, int index);

/**
 * @brief Finds a compatible SST file for compaction in the next level.
 * @param next_level Pointer to the list of SST files in the next level.
 * @param list_to_merge Pointer to the list of SST files to merge.
 * @param victim Pointer to the victim SST file.
 * @return Index of the compatible SST file or error code.
 */
int find_compact_friend(list * next_level, list * list_to_merge, sst_f_inf * victim);

/**
 * @brief Wrapper function for merging SST files.
 * @param info Pointer to the compaction table information.
 * @param null_ret Unused return value pointer.
 * @param thrd Pointer to the thread executing the merge.
 */
void merge_wrapper(void * info, void ** null_ret, thread * thrd);

/**
 * @brief Checks which levels need compaction.
 * @param cm Pointer to the compaction manager.
 * @param levels_needed Array to store which levels need compaction.
 * @return Number of levels that need compaction.
 */
int check_compacts(compact_manager * cm, int * levels_needed);

/**
 * @brief Computes the priority of a compaction job based on level.
 * @param level Level number.
 * @return Priority value.
 */
int compute_priority(int level);

/**
 * @brief Creates compaction jobs for the specified levels.
 * @param cm Pointer to the compaction manager.
 * @param levels Array indicating which levels need compaction.
 * @return Number of jobs created.
 */
int create_jobs(compact_manager * cm, int levels[]);

/**
 * @brief Starts the compaction jobs.
 * @param cm Pointer to the compaction manager.
 * @return 0 on success, error code on failure.
 */
int start_jobs(compact_manager * cm);

/**
 * @brief Main function for the compactor thread.
 * @param cm_thrd Pointer to the compaction manager.
 * @param null_ret Unused return value pointer.
 * @param pool Pointer to the thread executing the compactor.
 */
void run_compactor(void * cm_thrd, void ** null_ret, thread * pool);
