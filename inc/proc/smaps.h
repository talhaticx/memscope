#ifndef MEMSCOPE_SMAPS_H
#define MEMSCOPE_SMAPS_H

#include <stdint.h>
#include <sys/types.h>

/**
 * Smaps Parser Module
 * Parses /proc/[pid]/smaps for detailed memory breakdown.
 * This is EXPENSIVE and should only be called for single-process inspection.
 */

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
 * Parse /proc/[pid]/smaps and extract memory breakdown.
 * @param pid Process ID to inspect
 * @param out Output struct
 * @return 0 on success, -1 on failure
 */
int smaps_parse(pid_t pid, smaps_breakdown_t *out);

#endif // MEMSCOPE_SMAPS_H
