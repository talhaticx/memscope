#ifndef MEMSCOPE_PID_H
#define MEMSCOPE_PID_H

#include "core/sample.h" // for process_snapshot_t
#include <sys/types.h>

/**
 * Parses /proc/[pid]/stat (Basic info + CPU + Memory)
 * Implemented in src/proc/pid_stat.c
 */
int pid_parse_stat(pid_t pid, process_snapshot_t *out);

/**
 * Parses /proc/[pid]/io (Disk IO)
 * Implemented in src/proc/pid_io.c
 * Note: Requires privileges. Fails gracefully (zeros fields) if unreadable.
 */
void pid_parse_io(pid_t pid, process_snapshot_t *out);

/**
 * Parses /proc/[pid]/status (Detailed Memory)
 * Implemented in src/proc/pid_status.c
 */
void pid_parse_status(pid_t pid, process_snapshot_t *out);

/**
 * Get UID for a process (parses from /proc/[pid]/status)
 */
uid_t pid_parse_uid(pid_t pid);

#endif // MEMSCOPE_PID_H