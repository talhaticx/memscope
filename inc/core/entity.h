#ifndef MEMSCOPE_ENTITY_H
#define MEMSCOPE_ENTITY_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

// History buffer size: 60 samples = 60 seconds at 1Hz
#define ENTITY_HISTORY_SIZE 60

// Linux kernel limits process names to 16 chars
#define ENTITY_COMM_LEN 16

/**
 * ProcessEntity: A persistent representation of a process.
 * 
 * Unlike process_snapshot_t which is transient per-frame, ProcessEntity
 * persists across frames, tracking history and state changes.
 * 
 * This enables:
 * - Sparklines/graphs showing 60-second trends
 * - Stable PID lookup for UI selection
 * - Detection of new/dead processes for visual highlights
 */
typedef struct {
    // --- Identity (immutable after creation) ---
    pid_t pid;
    uint64_t start_time;           // Jiffies at process start (detects PID reuse)
    
    // --- Current Metrics (updated each tick) ---
    uint64_t rss_bytes;            // Resident Set Size
    uint64_t vss_bytes;            // Virtual Memory Size
    uint64_t pss_bytes;            // Proportional Set Size
    uint64_t swap_bytes;           // Swap usage
    double cpu_pct;                // CPU usage percentage
    
    uint64_t utime_ms;             // User CPU time (for delta calc)
    uint64_t stime_ms;             // System CPU time (for delta calc)
    
    uint64_t io_read_bytes;        // I/O read
    uint64_t io_write_bytes;       // I/O write
    
    // --- Metadata ---
    pid_t ppid;                    // Parent PID
    uid_t uid;                     // User ID (for username display)
    char state;                    // Process state (R, S, D, Z, T...)
    char comm[ENTITY_COMM_LEN];    // Process name
    
    // --- History Ring Buffers ---
    // Index 0 = oldest, history_idx = next write position
    double cpu_history[ENTITY_HISTORY_SIZE];
    uint64_t rss_history[ENTITY_HISTORY_SIZE];
    int history_idx;               // Next write position (0 to ENTITY_HISTORY_SIZE-1)
    int history_count;             // Number of valid entries (0 to ENTITY_HISTORY_SIZE)
    
    // --- State Flags ---
    bool is_alive;                 // true if seen in last scan
    bool is_new;                   // true if created this tick (for highlight)
    bool is_dead;                  // true if not seen in last scan (pending removal)
    
    // --- Access Level ---
    int access_level;              // 1=full smaps, 2=smaps, 3=status only
    
    // --- Internal ---
    uint64_t last_update_ms;       // Timestamp of last update
    
} ProcessEntity;

/**
 * Push a new data point to the entity's history buffers.
 * Automatically handles ring buffer wrap-around.
 */
static inline void entity_push_history(ProcessEntity *e) {
    if (!e) return;
    
    // Write to current position
    e->cpu_history[e->history_idx] = e->cpu_pct;
    e->rss_history[e->history_idx] = e->rss_bytes;
    
    // Advance index (wrap around)
    e->history_idx = (e->history_idx + 1) % ENTITY_HISTORY_SIZE;
    
    // Increment count up to max
    if (e->history_count < ENTITY_HISTORY_SIZE) {
        e->history_count++;
    }
}

/**
 * Get a history value at a given age (0 = most recent, history_count-1 = oldest).
 * Returns 0 if out of bounds.
 */
static inline double entity_get_cpu_history(const ProcessEntity *e, int age) {
    if (!e || age < 0 || age >= e->history_count) return 0.0;
    
    // Calculate actual index (history_idx - 1 = most recent)
    int idx = (e->history_idx - 1 - age + ENTITY_HISTORY_SIZE) % ENTITY_HISTORY_SIZE;
    return e->cpu_history[idx];
}

static inline uint64_t entity_get_rss_history(const ProcessEntity *e, int age) {
    if (!e || age < 0 || age >= e->history_count) return 0;
    
    int idx = (e->history_idx - 1 - age + ENTITY_HISTORY_SIZE) % ENTITY_HISTORY_SIZE;
    return e->rss_history[idx];
}

/**
 * Reset entity state for reuse (when PID is recycled).
 */
static inline void entity_reset(ProcessEntity *e) {
    if (!e) return;
    
    e->history_idx = 0;
    e->history_count = 0;
    e->is_alive = true;
    e->is_new = true;
    e->is_dead = false;
    e->access_level = 3;
    
    // Zero history buffers
    for (int i = 0; i < ENTITY_HISTORY_SIZE; i++) {
        e->cpu_history[i] = 0.0;
        e->rss_history[i] = 0;
    }
}

#endif // MEMSCOPE_ENTITY_H
