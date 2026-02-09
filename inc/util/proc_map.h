#ifndef MEMSCOPE_PROC_MAP_H
#define MEMSCOPE_PROC_MAP_H

#include "core/entity.h"
#include <stddef.h>
#include <stdbool.h>

/**
 * ProcMap: A hashmap for PID -> ProcessEntity* lookup.
 * 
 * Implementation: Open addressing with linear probing.
 * - Simple, cache-friendly traversal
 * - Load factor 0.7 triggers resize
 * - Tombstones for deletion
 */

// Initial capacity (must be power of 2)
#define PROC_MAP_INITIAL_CAPACITY 1024

// Load factor threshold (70%)
#define PROC_MAP_LOAD_FACTOR 0.7

typedef struct {
    pid_t pid;              // Key (0 = empty, -1 = tombstone)
    ProcessEntity *entity;  // Value
} ProcMapEntry;

typedef struct {
    ProcMapEntry *entries;  // Array of entries
    size_t capacity;        // Current capacity
    size_t count;           // Number of live entries
    size_t tombstones;      // Number of tombstone entries
} ProcMap;

/**
 * Create a new process map.
 * @return New map or NULL on failure.
 */
ProcMap *proc_map_create(void);

/**
 * Destroy a process map and all contained entities.
 * @param map Map to destroy (may be NULL).
 */
void proc_map_destroy(ProcMap *map);

/**
 * Get an entity by PID.
 * @param map The map.
 * @param pid Process ID to look up.
 * @return Entity pointer or NULL if not found.
 */
ProcessEntity *proc_map_get(ProcMap *map, pid_t pid);

/**
 * Insert or update an entity.
 * If PID already exists, returns existing entity (for update).
 * If new, allocates and returns new entity.
 * @param map The map.
 * @param pid Process ID.
 * @param created Output: set to true if new entity was created.
 * @return Entity pointer or NULL on allocation failure.
 */
ProcessEntity *proc_map_upsert(ProcMap *map, pid_t pid, bool *created);

/**
 * Mark an entity as dead (tombstone).
 * The entity is freed and slot marked for reuse.
 * @param map The map.
 * @param pid Process ID to remove.
 * @return true if found and removed, false if not found.
 */
bool proc_map_remove(ProcMap *map, pid_t pid);

/**
 * Get the number of live entries.
 */
static inline size_t proc_map_count(const ProcMap *map) {
    return map ? map->count : 0;
}

/**
 * Iterator callback type.
 * @param entity The entity.
 * @param userdata User-provided context.
 * @return true to continue iterating, false to stop.
 */
typedef bool (*ProcMapIterator)(ProcessEntity *entity, void *userdata);

/**
 * Iterate over all live entities.
 * @param map The map.
 * @param callback Function to call for each entity.
 * @param userdata Context passed to callback.
 */
void proc_map_foreach(ProcMap *map, ProcMapIterator callback, void *userdata);

/**
 * Mark all entities as not-alive (call before scan to detect dead processes).
 */
void proc_map_mark_all_dead(ProcMap *map);

/**
 * Remove all entities marked as dead.
 * @return Number of entities removed.
 */
size_t proc_map_reap_dead(ProcMap *map);

#endif // MEMSCOPE_PROC_MAP_H
