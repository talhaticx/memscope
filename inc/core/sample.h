#ifndef MEMSCOPE_SAMPLE_H
#define MEMSCOPE_SAMPLE_H

#include <stdint.h>
#include <sys/types.h> // for pid_t

// Linux kernel limits process names (comm) to 16 chars including null terminator
#define MAX_COMM_LEN 16

/**
 * Process State Codes (from /proc/[pid]/stat)
 */
typedef enum {
    PROC_STATE_RUNNING     = 'R',
    PROC_STATE_SLEEPING    = 'S',
    PROC_STATE_DISK_SLEEP  = 'D',
    PROC_STATE_ZOMBIE      = 'Z',
    PROC_STATE_STOPPED     = 'T',
    PROC_STATE_TRACING     = 't',
    PROC_STATE_DEAD        = 'X', // Should rarely see this
    PROC_STATE_UNKNOWN     = '?'
} proc_state_t;

/**
 * The Atomic Unit of Observation.
 * Represents one process at one specific moment in time.
 * * DESIGN NOTE: 
 * Fields are ordered by size (uint64 -> int -> char) to ensure 
 * tight packing and alignment, minimizing memory waste in the Arena.
 */
typedef struct {
    // --- Identity & Lifecycle ---
    uint64_t start_time;      // Jiffies since boot (Critical for distinguishing PID reuse)
    
    // --- Memory Metrics (The Core Mission) ---
    uint64_t vss_bytes;       // Virtual Memory Size (Total address space)
    uint64_t rss_bytes;       // Resident Set Size (Physical RAM used)
    uint64_t pss_bytes;       // Proportional Set Size (RSS / sharing count) - *Expensive to calculate*
    uint64_t swap_bytes;      // Bytes on disk (The "System is dying" metric)
    
    // --- CPU Metrics ---
    uint64_t utime_ms;        // User CPU time (milliseconds)
    uint64_t stime_ms;        // Kernel CPU time (milliseconds)
    
    // --- IO Metrics (Forensics) ---
    uint64_t io_read_bytes;   // Bytes read from storage
    uint64_t io_write_bytes;  // Bytes written to storage

    // --- Identification ---
    pid_t pid;                // Process ID
    pid_t ppid;               // Parent Process ID
    int32_t cpu_id;           // Last CPU core executed on
    
    // --- Metadata ---
    char state;               // Single char state code (R, S, Z...)
    char comm[MAX_COMM_LEN];  // Short process name (e.g. "chrome", "bash")
    
} process_snapshot_t;

/**
 * A System-Wide Snapshot.
 * Contains the list of all processes and global system state at one instant.
 */
typedef struct {
    uint64_t timestamp_ms;    // Monotonic time of capture
    
    // --- Global Memory Context ---
    uint64_t system_total_ram;// From /proc/meminfo
    uint64_t system_free_ram; // From /proc/meminfo
    uint64_t system_cached_ram;
    
    // --- The Process List ---
    size_t process_count;     // Number of valid processes in the array
    size_t capacity;          // Max capacity (for safety checks)
    process_snapshot_t *processes; // Pointer into the Arena (contiguous array)
    
} sample_t;

#endif // MEMSCOPE_SAMPLE_H