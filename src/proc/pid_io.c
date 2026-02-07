// Enable POSIX features for snprintf
#define _POSIX_C_SOURCE 200809L

#include "proc/pid.h"
#include "proc/procfs.h"
#include <stdio.h>

void pid_parse_io(pid_t pid, process_snapshot_t *out) {
    char path[64];
    char buf[1024]; // /proc/[pid]/io is small, 1KB is plenty
    
    // 1. Construct path
    // Safe snprintf to avoid buffer overflow
    snprintf(path, sizeof(path), "/proc/%d/io", pid);

    // 2. Read file
    // We expect this to fail often (Permission Denied). 
    // If it fails, we just zero the fields and return.
    if (procfs_read_file(path, buf, sizeof(buf)) <= 0) {
        out->io_read_bytes = 0;
        out->io_write_bytes = 0;
        return;
    }

    // 3. Parse "rchar" (Bytes read from storage layer)
    // Note: 'rchar' includes cached reads. 'read_bytes' is actual disk IO.
    // For forensics, 'read_bytes' (physical) is usually more interesting than 'rchar' (logical),
    // but 'rchar' shows app behavior better. Let's capture 'read_bytes' (physical) as per spec.
    //
    // Format is:
    // rchar: 12345
    // wchar: 12345
    // syscr: 123
    // syscw: 123
    // read_bytes: 0
    // write_bytes: 0
    
    // Note: We use "read_bytes:" (physical disk IO) not "rchar:" (logical read)
    // per forensic requirements.
    out->io_read_bytes  = procfs_scan_u64(buf, "read_bytes:");
    out->io_write_bytes = procfs_scan_u64(buf, "write_bytes:");
}