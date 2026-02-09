/**
 * @file smaps.c
 * @brief 3-level memory breakdown parser with automatic fallback.
 *
 * Provides detailed memory breakdown per process using three data sources:
 *
 *   Level 1: /proc/[pid]/smaps_rollup (kernel 4.14+)
 *            - Fastest, kernel pre-aggregates all mappings
 *            - Best choice when available
 *
 *   Level 2: /proc/[pid]/smaps
 *            - Detailed per-mapping breakdown
 *            - Slower (can be 100KB+ for large processes)
 *            - Fallback when rollup unavailable
 *
 *   Level 3: /proc/[pid]/status
 *            - Basic VmRSS only
 *            - Always available, but minimal detail
 *            - Last resort fallback
 *
 * @see inc/proc/smaps.h for API and smaps_breakdown_t struct
 */

#define _POSIX_C_SOURCE 200809L
#include "proc/smaps.h"
#include "proc/procfs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define ROLLUP_BUF_SIZE 4096
#define SMAPS_BUF_SIZE  (256 * 1024)  // Full smaps can be large
#define STATUS_BUF_SIZE 2048

// ============================================================
// Level 1: smaps_rollup (fast, kernel 4.14+)
// ============================================================

static int try_smaps_rollup(pid_t pid, smaps_breakdown_t *out) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/smaps_rollup", pid);
    
    char *buf = malloc(ROLLUP_BUF_SIZE);
    if (!buf) return -1;
    
    ssize_t bytes = procfs_read_file(path, buf, ROLLUP_BUF_SIZE);
    if (bytes < 0) {
        free(buf);
        return -1;
    }
    
    // Parse key: value pairs
    char *line = buf;
    char *end = buf + bytes;
    
    while (line < end) {
        char *eol = strchr(line, '\n');
        if (!eol) break;
        *eol = '\0';
        
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
            out->shared_kb = procfs_scan_u64(line, "Pss_File:");
        }
        else if (strncmp(line, "Anonymous:", 10) == 0) {
            if (out->anon_kb == 0) {
                out->anon_kb = procfs_scan_u64(line, "Anonymous:");
            }
        }
        else if (strncmp(line, "Private_Dirty:", 14) == 0) {
            out->heap_kb = procfs_scan_u64(line, "Private_Dirty:");
        }
        else if (strncmp(line, "Swap:", 5) == 0) {
            out->swap_kb = procfs_scan_u64(line, "Swap:");
        }
        
        line = eol + 1;
    }
    
    free(buf);
    return 0;
}

// ============================================================
// Level 2: Full smaps parse (slower, more detail)
// ============================================================

static int try_smaps_full(pid_t pid, smaps_breakdown_t *out) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/smaps", pid);
    
    char *buf = malloc(SMAPS_BUF_SIZE);
    if (!buf) return -1;
    
    ssize_t bytes = procfs_read_file(path, buf, SMAPS_BUF_SIZE);
    if (bytes < 0) {
        free(buf);
        return -1;
    }
    
    // Parse each memory region
    char *line = buf;
    char *end = buf + bytes;
    
    char current_region[64] = {0};
    uint64_t region_rss = 0;
    uint64_t region_pss = 0;
    uint64_t region_swap = 0;
    
    while (line < end) {
        char *eol = strchr(line, '\n');
        if (!eol) break;
        *eol = '\0';
        
        // Check if this is a header line (memory region)
        // Format: 7f1234000000-7f1234001000 r-xp ... /path/to/lib.so
        if (line[0] != ' ' && strchr(line, '-') && strchr(line, ' ')) {
            // Find region name (last token after path)
            char *name = strrchr(line, '/');
            if (name) {
                name++;  // Skip '/'
            } else {
                // Check for [heap], [stack], etc.
                char *bracket = strchr(line, '[');
                if (bracket) {
                    name = bracket;
                }
            }
            
            if (name) {
                strncpy(current_region, name, sizeof(current_region) - 1);
                current_region[sizeof(current_region) - 1] = '\0';
                // Remove trailing bracket if present
                char *end_bracket = strchr(current_region, ']');
                if (end_bracket) {
                    *(end_bracket + 1) = '\0';
                }
            } else {
                current_region[0] = '\0';
            }
        }
        // Parse metrics
        else if (strncmp(line, "Rss:", 4) == 0) {
            region_rss = procfs_scan_u64(line, "Rss:");
            out->total_rss_kb += region_rss;
            
            // Categorize by region name
            if (strstr(current_region, "[heap]")) {
                out->heap_kb += region_rss;
            } else if (strstr(current_region, "[stack]")) {
                out->stack_kb += region_rss;
            } else if (strstr(current_region, ".so")) {
                out->shared_kb += region_rss;
            }
        }
        else if (strncmp(line, "Pss:", 4) == 0) {
            region_pss = procfs_scan_u64(line, "Pss:");
            out->total_pss_kb += region_pss;
        }
        else if (strncmp(line, "Swap:", 5) == 0) {
            region_swap = procfs_scan_u64(line, "Swap:");
            out->swap_kb += region_swap;
        }
        else if (strncmp(line, "Anonymous:", 10) == 0) {
            out->anon_kb += procfs_scan_u64(line, "Anonymous:");
        }
        
        line = eol + 1;
    }
    
    free(buf);
    return 0;
}

// ============================================================
// Level 3: Fallback to /proc/[pid]/status (VmRSS only)
// ============================================================

static int try_status_vmrss(pid_t pid, smaps_breakdown_t *out) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    
    char buf[STATUS_BUF_SIZE];
    ssize_t bytes = procfs_read_file(path, buf, sizeof(buf));
    if (bytes < 0) {
        return -1;
    }
    
    // Parse VmRSS, VmSwap from status
    out->total_rss_kb = procfs_scan_u64(buf, "VmRSS:");
    out->swap_kb = procfs_scan_u64(buf, "VmSwap:");
    
    // PSS not available from status, estimate as RSS
    out->total_pss_kb = out->total_rss_kb;
    
    // No detailed breakdown available
    out->heap_kb = 0;
    out->stack_kb = 0;
    out->anon_kb = 0;
    out->shared_kb = 0;
    
    return 0;
}

// ============================================================
// Public API
// ============================================================

int pid_get_memory_detail(pid_t pid, smaps_breakdown_t *out, int *access_level) {
    if (!out) return -1;
    
    memset(out, 0, sizeof(smaps_breakdown_t));
    
    int level = SMAPS_ACCESS_BASIC;
    
    // Try Level 1: smaps_rollup (fastest)
    if (try_smaps_rollup(pid, out) == 0) {
        level = SMAPS_ACCESS_FULL;
    }
    // Try Level 2: Full smaps
    else if (try_smaps_full(pid, out) == 0) {
        level = SMAPS_ACCESS_SMAPS;
    }
    // Fallback Level 3: VmRSS from status
    else if (try_status_vmrss(pid, out) == 0) {
        level = SMAPS_ACCESS_BASIC;
    }
    else {
        // Complete failure
        if (access_level) *access_level = 0;
        return -1;
    }
    
    if (access_level) *access_level = level;
    return 0;
}

// Legacy compatibility function
int smaps_parse(pid_t pid, smaps_breakdown_t *out) {
    int level;
    return pid_get_memory_detail(pid, out, &level);
}
