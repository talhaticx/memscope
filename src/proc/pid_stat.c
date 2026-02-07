// Enable POSIX features for snprintf
#define _POSIX_C_SOURCE 200809L

#include "proc/pid.h"
#include "proc/procfs.h"
#include "util/log.h"
#include <stdio.h>
#include <stdlib.h> // for strtoull
#include <string.h> // for strrchr

// Field indices in /proc/[pid]/stat (1-based)
#define FIELD_STATE      3
#define FIELD_PPID       4
#define FIELD_UTIME      14
#define FIELD_STIME      15
#define FIELD_STARTTIME  22
#define FIELD_RSS        24

int pid_parse_stat(pid_t pid, process_snapshot_t *out) {
    char path[64];
    // A single line in /proc/[pid]/stat can be long, 2KB is safe
    char buf[2048]; 

    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    // 1. Read the file
    if (procfs_read_file(path, buf, sizeof(buf)) <= 0) {
        return -1; // Failed to read (process likely dead)
    }

    // 2. Parse PID and Comm (Process Name)
    // Format: 123 (process name) S ...
    // Strategy: Find the *last* closing parenthesis to handle names with parens.
    char *open_paren = strchr(buf, '(');
    char *close_paren = strrchr(buf, ')');

    if (!open_paren || !close_paren || close_paren < open_paren) {
        // Malformed line
        return -1;
    }

    // Copy the name
    size_t len = close_paren - open_paren - 1;
    if (len >= MAX_COMM_LEN) {
        len = MAX_COMM_LEN - 1;
    }
    memcpy(out->comm, open_paren + 1, len);
    out->comm[len] = '\0';

    out->pid = pid;

    // 3. Parse the numeric fields after the name
    // 'curr' points to the space after ')'
    char *curr = close_paren + 2; 
    
    // We are now at Field 3 (State)
    out->state = *curr;

    // We need to loop through spaces to find specific field indices.
    // We start at Field 3.
    int field_idx = 3;
    
    while (*curr != '\0') {
        // Find next space
        char *next_space = strchr(curr, ' ');
        if (!next_space) break;

        // Move to start of next field
        curr = next_space + 1;
        field_idx++;

        // Parse relevant fields
        // Note: strtoull automatically stops at the next space
        switch (field_idx) {
            case FIELD_PPID:
                out->ppid = (pid_t)strtoull(curr, NULL, 10);
                break;
            case FIELD_UTIME:
                out->utime_ms = strtoull(curr, NULL, 10);
                break;
            case FIELD_STIME:
                out->stime_ms = strtoull(curr, NULL, 10);
                break;
            case FIELD_STARTTIME:
                out->start_time = strtoull(curr, NULL, 10);
                break;
            case FIELD_RSS:
                out->rss_bytes = strtoull(curr, NULL, 10);
                break;
        }

        // Optimization: Stop if we passed the last field we care about
        if (field_idx > FIELD_RSS) break;
    }

    // 4. Convert Units
    // Kernel gives CPU time in Jiffies -> Convert to MS
    if (sc_clk_tck > 0) {
        out->utime_ms = (out->utime_ms * 1000) / sc_clk_tck;
        out->stime_ms = (out->stime_ms * 1000) / sc_clk_tck;
    }

    // Kernel gives RSS in Pages -> Convert to Bytes
    if (sc_page_size > 0) {
        out->rss_bytes *= sc_page_size;
    }

    return 0; // Success
}