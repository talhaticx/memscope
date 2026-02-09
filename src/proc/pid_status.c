// Enable POSIX features for snprintf
#define _POSIX_C_SOURCE 200809L

#include "proc/pid.h"
#include "proc/procfs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

    // 2. Grab memory values
    out->swap_bytes = procfs_scan_kb(buf, "VmSwap:");
    out->vss_bytes  = procfs_scan_kb(buf, "VmSize:");
    out->rss_bytes  = procfs_scan_kb(buf, "VmRSS:");

    // 3. Parse UID (first field is real UID)
    // Format: "Uid:	1000	1000	1000	1000"
    // We want the first field (real UID)
    const char *uid_line = strstr(buf, "\nUid:");
    if (uid_line) {
        uid_line += 5;  // Skip "\nUid:"
        while (*uid_line == '\t' || *uid_line == ' ') uid_line++;
        // Note: We're storing in cpu_id as a temporary hack
        // The proper fix is to add uid to process_snapshot_t
        // For now, we'll parse UID separately in engine.c using the entity
    }

    // 4. Parse Name (Name in status is truncated less often than stat)
    procfs_scan_str(buf, "Name:", out->comm, sizeof(out->comm));
}

// New function: Get UID directly for ProcessEntity
#include <string.h>

uid_t pid_parse_uid(pid_t pid) {
    char path[64];
    char buf[4096];
    
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    
    if (procfs_read_file(path, buf, sizeof(buf)) <= 0) {
        return (uid_t)-1;
    }
    
    // Find "Uid:" line
    const char *uid_line = strstr(buf, "\nUid:");
    if (!uid_line) {
        // Try at start of buffer
        if (strncmp(buf, "Uid:", 4) == 0) {
            uid_line = buf - 1;  // Will be incremented
        } else {
            return (uid_t)-1;
        }
    }
    
    uid_line += 5;  // Skip "\nUid:" or "Uid:"
    
    // Skip whitespace
    while (*uid_line == '\t' || *uid_line == ' ') uid_line++;
    
    // Parse first field (real UID)
    return (uid_t)strtoul(uid_line, NULL, 10);
}