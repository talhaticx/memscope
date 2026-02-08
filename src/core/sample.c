#include "core/sample.h"
#include "proc/procfs.h"
#include "core/time.h"
#include "util/log.h"
#include <stdlib.h> // for qsort
#include <string.h>

// Helper: Comparator for qsort
// Returns <0 if a < b, 0 if a == b, >0 if a > b
static int compare_pids(const void *a, const void *b) {
    const process_snapshot_t *pa = (const process_snapshot_t *)a;
    const process_snapshot_t *pb = (const process_snapshot_t *)b;
    
    // Simple integer subtraction is safe for PIDs (they are positive)
    return (pa->pid - pb->pid);
}

int sample_capture(sample_t *sample, Arena *a) {
    // 1. Lazy Initialization
    // We do this here so the user doesn't have to remember to call init() in main.
    static int initialized = 0;
    if (!initialized) {
        if (procfs_init() != 0) {
            LOG_ERR("Failed to initialize procfs subsystem");
            return -1;
        }
        initialized = 1;
    }

    // 2. Set Timestamp (Monotonic)
    sample->timestamp_ms = time_now_ms();

    // 3. Scan System (Fills sample->processes and global meminfo)
    // Note: procfs_scan internally calls procfs_read_meminfo per Step 4.2
    if (procfs_scan(sample, a) != 0) {
        return -1;
    }

    // 4. Sort by PID (CRITICAL)
    // This enables O(N) linear-time diffing later.
    // Without this, the Diff Engine is O(N^2) and will choke on 10k processes.
    qsort(sample->processes, 
          sample->process_count, 
          sizeof(process_snapshot_t), 
          compare_pids);

    return 0;
}

void sample_copy(const sample_t *src, sample_t *dst, Arena *a) {
    if (!src || !dst || !a) return;
    
    *dst = *src; // Copy scalars
    
    if (src->process_count > 0 && src->processes) {
        dst->processes = arena_alloc(a, sizeof(process_snapshot_t) * src->process_count);
        if (dst->processes) {
            // We need string.h for memcpy
            extern void *memcpy(void *dest, const void *src, size_t n);
            memcpy(dst->processes, src->processes, sizeof(process_snapshot_t) * src->process_count);
        } else {
            dst->process_count = 0;
            dst->processes = NULL;
        }
    } else {
        dst->processes = NULL;
        dst->process_count = 0;
    }
}