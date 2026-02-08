// MUST BE FIRST: Enables Linux extensions (DT_DIR, d_type)
#define _DEFAULT_SOURCE 

#include "proc/procfs.h"
#include "proc/pid.h"
#include "util/log.h"
#include <dirent.h>     
#include <ctype.h>      
#include <stdlib.h>     
#include <string.h>     

static int is_digits_only(const char *s) {
    while (*s) {
        if (*s < '0' || *s > '9') return 0;
        s++;
    }
    return 1;
}

int procfs_scan(sample_t *sample, Arena *a) {
    DIR *dir = opendir(PROC_ROOT);
    if (!dir) {
        LOG_ERR("Failed to open " PROC_ROOT);
        return -1;
    }

    // sample->timestamp_ms = 0;
    sample->process_count = 0;

    if (procfs_read_meminfo(sample) != 0) {
        // Warning only, we can still scan processes
        LOG_WARN("Could not read system memory stats");
    }
    
    // Capture the starting pointer in the Arena.
    // The cast is necessary because buf is uint8_t* but we need struct pointer
    sample->processes = (process_snapshot_t *)(a->buf + a->offset);

    struct dirent *entry;
    
    while ((entry = readdir(dir)) != NULL) {
        // Filter: Must be a directory.
        // d_type is a Linux optimization. If filesystem returns DT_UNKNOWN, we must assume it might be a dir.
        if (entry->d_type != DT_DIR && entry->d_type != DT_UNKNOWN) continue;
        
        // Fast check: First char must be digit
        if (!isdigit(entry->d_name[0])) continue;

        if (!is_digits_only(entry->d_name)) continue;

        pid_t pid = atoi(entry->d_name);
        if (pid <= 0) continue;

        process_snapshot_t *proc = arena_alloc(a, sizeof(process_snapshot_t));
        if (!proc) {
            LOG_ERR("Arena OOM during scan! (PID %d)", pid);
            break; 
        }

        if (pid_parse_stat(pid, proc) != 0) {
            // Process died mid-scan. Rewind arena to reclaim memory.
            a->offset -= sizeof(process_snapshot_t);
            continue;
        }

        // These are optional
        pid_parse_status(pid, proc);
        pid_parse_io(pid, proc);

        sample->process_count++;
    }

    closedir(dir);
    return 0;
}