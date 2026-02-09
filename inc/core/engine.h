#ifndef MEMSCOPE_ENGINE_H
#define MEMSCOPE_ENGINE_H

#include "core/entity.h"
#include "util/proc_map.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * Engine: The background data collector.
 * 
 * Runs a background thread that scans /proc at 1Hz, updating the
 * persistent ProcessEntity hashmap. The UI thread can safely read
 * snapshots via engine_get_*() functions while the collector runs.
 * 
 * Threading Model:
 * - Collector thread: Takes WRITE lock during update
 * - UI thread: Takes READ lock during render
 * - Multiple readers can proceed simultaneously
 */

/**
 * Start the engine.
 * Spawns the background collector thread.
 * @return 0 on success, -1 on failure.
 */
int engine_start(void);

/**
 * Stop the engine.
 * Signals the collector thread to stop and waits for it to finish.
 */
void engine_stop(void);

/**
 * Check if the engine is running.
 */
bool engine_is_running(void);

/**
 * Get the process map for reading.
 * 
 * IMPORTANT: You must call engine_read_unlock() when done!
 * 
 * @return Pointer to the process map (read-locked).
 */
ProcMap *engine_read_lock(void);

/**
 * Release the read lock.
 */
void engine_read_unlock(void);

/**
 * Get a single entity by PID (takes and releases lock automatically).
 * 
 * WARNING: The returned pointer is only valid while no updates occur.
 * For safe access, use engine_read_lock() instead.
 * 
 * @param pid Process ID.
 * @return Entity pointer or NULL if not found.
 */
ProcessEntity *engine_get_entity(pid_t pid);

/**
 * Get total process count.
 */
size_t engine_get_process_count(void);

/**
 * Get global system memory stats.
 */
typedef struct {
    uint64_t total_ram;
    uint64_t free_ram;
    uint64_t cached_ram;
    uint64_t total_swap;
    uint64_t free_swap;
    uint64_t timestamp_ms;
} EngineMemInfo;

void engine_get_meminfo(EngineMemInfo *out);

/**
 * Get the last update timestamp.
 */
uint64_t engine_get_last_update(void);

/**
 * Force an immediate update (for testing).
 * Blocks until update completes.
 */
void engine_force_update(void);

#endif // MEMSCOPE_ENGINE_H
