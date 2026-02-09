#ifndef MEMSCOPE_SMAPS_H
#define MEMSCOPE_SMAPS_H

#include <stdint.h>
#include <sys/types.h>

/**
 * Smaps Parser Module
 * 
 * Provides 3-level memory parsing with automatic fallback:
 *   Level 1: /proc/[pid]/smaps_rollup (fast, kernel-aggregated)
 *   Level 2: /proc/[pid]/smaps (detailed, manual aggregation)
 *   Level 3: /proc/[pid]/status (basic VmRSS, always available)
 */

// Access level constants
#define SMAPS_ACCESS_FULL    1  // smaps_rollup available
#define SMAPS_ACCESS_SMAPS   2  // Full smaps available
#define SMAPS_ACCESS_BASIC   3  // Only VmRSS from status

typedef struct {
    uint64_t heap_kb;       // [heap] segment
    uint64_t stack_kb;      // [stack] segment
    uint64_t anon_kb;       // Anonymous mappings (no file)
    uint64_t shared_kb;     // Shared libraries
    uint64_t code_kb;       // Executable code
    uint64_t data_kb;       // Data segments
    uint64_t total_pss_kb;  // Total PSS
    uint64_t total_rss_kb;  // Total RSS
    uint64_t swap_kb;       // Swapped out
} smaps_breakdown_t;

/**
 * Get detailed memory breakdown with automatic fallback.
 * 
 * Tries methods in order of preference:
 * 1. smaps_rollup (fastest, kernel 4.14+)
 * 2. smaps (full parse, slower but more detail)
 * 3. status VmRSS (always available, basic info only)
 * 
 * @param pid Process ID to inspect
 * @param out Output struct
 * @param access_level Output: which level was used (1, 2, or 3)
 * @return 0 on success, -1 on complete failure
 */
int pid_get_memory_detail(pid_t pid, smaps_breakdown_t *out, int *access_level);

/**
 * Legacy function - calls pid_get_memory_detail internally.
 * Kept for backward compatibility.
 */
int smaps_parse(pid_t pid, smaps_breakdown_t *out);

#endif // MEMSCOPE_SMAPS_H

