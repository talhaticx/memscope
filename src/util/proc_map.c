#include "util/proc_map.h"
#include "util/log.h"
#include <stdlib.h>
#include <string.h>

// Tombstone marker
#define TOMBSTONE_PID ((pid_t)-1)

// Hash function: fast integer hash (Knuth multiplicative)
static inline size_t hash_pid(pid_t pid, size_t capacity) {
    // Golden ratio hash for good distribution
    uint64_t h = (uint64_t)pid * 2654435769ULL;
    return (size_t)(h % capacity);
}

// Find slot for a PID (for get/insert)
static ProcMapEntry *find_slot(ProcMap *map, pid_t pid, bool for_insert) {
    if (!map || !map->entries) return NULL;
    
    size_t idx = hash_pid(pid, map->capacity);
    size_t start = idx;
    ProcMapEntry *first_tombstone = NULL;
    
    do {
        ProcMapEntry *entry = &map->entries[idx];
        
        // Empty slot
        if (entry->pid == 0) {
            if (for_insert && first_tombstone) {
                return first_tombstone;  // Reuse tombstone
            }
            return for_insert ? entry : NULL;
        }
        
        // Tombstone
        if (entry->pid == TOMBSTONE_PID) {
            if (for_insert && !first_tombstone) {
                first_tombstone = entry;
            }
            // Continue searching for get
        }
        // Match found
        else if (entry->pid == pid) {
            return entry;
        }
        
        // Linear probe
        idx = (idx + 1) % map->capacity;
    } while (idx != start);
    
    // Table full (shouldn't happen with proper load factor)
    return first_tombstone;
}

// Resize the hashtable
static bool resize(ProcMap *map, size_t new_capacity) {
    ProcMapEntry *old_entries = map->entries;
    size_t old_capacity = map->capacity;
    
    // Allocate new array
    map->entries = calloc(new_capacity, sizeof(ProcMapEntry));
    if (!map->entries) {
        map->entries = old_entries;  // Restore on failure
        return false;
    }
    
    map->capacity = new_capacity;
    map->count = 0;
    map->tombstones = 0;
    
    // Rehash all entries
    for (size_t i = 0; i < old_capacity; i++) {
        if (old_entries[i].pid > 0) {  // Skip empty and tombstones
            ProcMapEntry *slot = find_slot(map, old_entries[i].pid, true);
            if (slot) {
                slot->pid = old_entries[i].pid;
                slot->entity = old_entries[i].entity;
                map->count++;
            }
        }
    }
    
    free(old_entries);
    return true;
}

ProcMap *proc_map_create(void) {
    ProcMap *map = malloc(sizeof(ProcMap));
    if (!map) {
        LOG_ERR("Failed to allocate ProcMap");
        return NULL;
    }
    
    map->capacity = PROC_MAP_INITIAL_CAPACITY;
    map->count = 0;
    map->tombstones = 0;
    
    map->entries = calloc(map->capacity, sizeof(ProcMapEntry));
    if (!map->entries) {
        LOG_ERR("Failed to allocate ProcMap entries");
        free(map);
        return NULL;
    }
    
    LOG_DEBUG("ProcMap created with capacity %zu", map->capacity);
    return map;
}

void proc_map_destroy(ProcMap *map) {
    if (!map) return;
    
    // Free all entities
    if (map->entries) {
        for (size_t i = 0; i < map->capacity; i++) {
            if (map->entries[i].pid > 0 && map->entries[i].entity) {
                free(map->entries[i].entity);
            }
        }
        free(map->entries);
    }
    
    free(map);
    LOG_DEBUG("ProcMap destroyed");
}

ProcessEntity *proc_map_get(ProcMap *map, pid_t pid) {
    if (!map || pid <= 0) return NULL;
    
    ProcMapEntry *entry = find_slot(map, pid, false);
    return entry ? entry->entity : NULL;
}

ProcessEntity *proc_map_upsert(ProcMap *map, pid_t pid, bool *created) {
    if (!map || pid <= 0) return NULL;
    if (created) *created = false;
    
    // Check load factor before insert
    double load = (double)(map->count + map->tombstones) / (double)map->capacity;
    if (load >= PROC_MAP_LOAD_FACTOR) {
        if (!resize(map, map->capacity * 2)) {
            LOG_ERR("ProcMap resize failed");
            return NULL;
        }
        LOG_DEBUG("ProcMap resized to %zu", map->capacity);
    }
    
    ProcMapEntry *entry = find_slot(map, pid, true);
    if (!entry) return NULL;
    
    // Existing entry
    if (entry->pid == pid) {
        return entry->entity;
    }
    
    // New entry (empty or tombstone)
    ProcessEntity *entity = calloc(1, sizeof(ProcessEntity));
    if (!entity) {
        LOG_ERR("Failed to allocate ProcessEntity");
        return NULL;
    }
    
    // Initialize
    entity->pid = pid;
    entity_reset(entity);
    
    // Handle tombstone count
    if (entry->pid == TOMBSTONE_PID) {
        map->tombstones--;
    }
    
    entry->pid = pid;
    entry->entity = entity;
    map->count++;
    
    if (created) *created = true;
    return entity;
}

bool proc_map_remove(ProcMap *map, pid_t pid) {
    if (!map || pid <= 0) return false;
    
    ProcMapEntry *entry = find_slot(map, pid, false);
    if (!entry || entry->pid != pid) return false;
    
    // Free entity
    if (entry->entity) {
        free(entry->entity);
        entry->entity = NULL;
    }
    
    // Mark as tombstone
    entry->pid = TOMBSTONE_PID;
    map->count--;
    map->tombstones++;
    
    return true;
}

void proc_map_foreach(ProcMap *map, ProcMapIterator callback, void *userdata) {
    if (!map || !callback) return;
    
    for (size_t i = 0; i < map->capacity; i++) {
        if (map->entries[i].pid > 0 && map->entries[i].entity) {
            if (!callback(map->entries[i].entity, userdata)) {
                break;
            }
        }
    }
}

void proc_map_mark_all_dead(ProcMap *map) {
    if (!map) return;
    
    for (size_t i = 0; i < map->capacity; i++) {
        if (map->entries[i].pid > 0 && map->entries[i].entity) {
            map->entries[i].entity->is_alive = false;
        }
    }
}

size_t proc_map_reap_dead(ProcMap *map) {
    if (!map) return 0;
    
    size_t reaped = 0;
    for (size_t i = 0; i < map->capacity; i++) {
        if (map->entries[i].pid > 0 && map->entries[i].entity) {
            if (!map->entries[i].entity->is_alive) {
                free(map->entries[i].entity);
                map->entries[i].entity = NULL;
                map->entries[i].pid = TOMBSTONE_PID;
                map->count--;
                map->tombstones++;
                reaped++;
            }
        }
    }
    
    return reaped;
}
