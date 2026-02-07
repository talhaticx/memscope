// Enable POSIX features for snprintf
#define _POSIX_C_SOURCE 200809L

#include "proc/pid.h"
#include "proc/procfs.h"
#include <stdio.h>

void pid_parse_status(pid_t pid, process_snapshot_t *out) {
    char path[64];
    // /proc/[pid]/status can be large (up to 4KB+ on complex systems)
    // We allocate a static buffer to avoid stack overflow, 
    // or a reasonably sized stack buffer. 4KB is usually safe for stack.
    char buf[4096]; 

    snprintf(path, sizeof(path), "/proc/%d/status", pid);

    // 1. Read the file
    if (procfs_read_file(path, buf, sizeof(buf)) <= 0) {
        out->swap_bytes = 0;
        return;
    }

    // 2. Grab the values
    out->swap_bytes = procfs_scan_kb(buf, "VmSwap:");
    out->vss_bytes  = procfs_scan_kb(buf, "VmSize:");
    out->rss_bytes  = procfs_scan_kb(buf, "VmRSS:");

    // 5. Parse RssAnon (Anonymous pages - Heap/Stack/mmap)
    // * This is the most important metric for memory leaks *
    // "RssAnon" isn't in our struct yet explicitly, but it makes up the bulk of RSS.
    // For now, we rely on RSS, but if you want to track leaks specifically, 
    // add 'rss_anon_bytes' to process_snapshot_t in sample.h.
    // For this version, we stick to the spec which uses total RSS.

    // 6. Name (Name in status is truncated less often than stat, sometimes)
    procfs_scan_str(buf, "Name:", out->comm, sizeof(out->comm));
}