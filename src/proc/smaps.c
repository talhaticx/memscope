#define _POSIX_C_SOURCE 200809L
#include "proc/smaps.h"
#include "proc/procfs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ROLLUP_BUF_SIZE 4096

/**
 * Parse /proc/[pid]/smaps_rollup for aggregated memory stats.
 * smaps_rollup is much faster than parsing full smaps as the kernel
 * does the aggregation for us.
 * 
 * Format:
 *   562663f0f000-7ffdf9d2d000 ---p 00000000 00:00 0    [rollup]
 *   Rss:                2168 kB
 *   Pss:                 150 kB
 *   Pss_Anon:            108 kB
 *   Shared_Clean:       2048 kB
 *   Private_Dirty:       108 kB
 *   Anonymous:           108 kB
 *   Swap:                  0 kB
 *   ...
 */
int smaps_parse(pid_t pid, smaps_breakdown_t *out) {
    if (!out) return -1;
    
    memset(out, 0, sizeof(smaps_breakdown_t));
    
    // Try smaps_rollup first (faster, kernel-aggregated)
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/smaps_rollup", pid);
    
    char *buf = malloc(ROLLUP_BUF_SIZE);
    if (!buf) return -1;
    
    ssize_t bytes = procfs_read_file(path, buf, ROLLUP_BUF_SIZE);
    if (bytes < 0) {
        free(buf);
        return -1;
    }
    
    // Parse line by line
    char *line = buf;
    char *end = buf + bytes;
    
    while (line < end) {
        char *eol = strchr(line, '\n');
        if (!eol) break;
        *eol = '\0';
        
        // Parse key: value pairs
        if (strncmp(line, "Rss:", 4) == 0) {
            out->total_rss_kb = procfs_scan_u64(line, "Rss:");
        }
        else if (strncmp(line, "Pss:", 4) == 0) {
            out->total_pss_kb = procfs_scan_u64(line, "Pss:");
        }
        else if (strncmp(line, "Pss_Anon:", 9) == 0) {
            out->anon_kb = procfs_scan_u64(line, "Pss_Anon:");
        }
        else if (strncmp(line, "Pss_File:", 9) == 0) {
            // File-backed memory (shared libs, code)
            out->shared_kb = procfs_scan_u64(line, "Pss_File:");
        }
        else if (strncmp(line, "Anonymous:", 10) == 0) {
            // If Pss_Anon wasn't available, use Anonymous
            if (out->anon_kb == 0) {
                out->anon_kb = procfs_scan_u64(line, "Anonymous:");
            }
        }
        else if (strncmp(line, "Private_Dirty:", 14) == 0) {
            // Private dirty is usually heap + stack
            out->heap_kb = procfs_scan_u64(line, "Private_Dirty:");
        }
        else if (strncmp(line, "Swap:", 5) == 0) {
            out->swap_kb = procfs_scan_u64(line, "Swap:");
        }
        
        line = eol + 1;
    }
    
    // smaps_rollup doesn't separate heap/stack, but we can estimate:
    // - Heap is typically the bulk of Private_Dirty
    // - Stack is typically very small (8-64KB)
    // Since we can't distinguish, we'll show:
    //   heap_kb   = Private_Dirty (already set) 
    //   stack_kb  = 0 (not available from rollup)
    //   anon_kb   = Pss_Anon or Anonymous
    //   shared_kb = Pss_File (file-backed)
    //   swap_kb   = Swap
    
    free(buf);
    return 0;
}
