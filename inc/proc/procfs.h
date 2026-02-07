#ifndef MEMSCOPE_PROCFS_H
#define MEMSCOPE_PROCFS_H

#include <stddef.h>
#include <stdint.h>
#include <unistd.h> // for sysconf
#include "core/sample.h" // For sample_t
#include "util/arena.h"  // For Arena

#define PROC_ROOT "/proc"

// Global System Constants (Populated by procfs_init)
extern long sc_clk_tck;   // Clock ticks per second (usually 100)
extern long sc_page_size; // Bytes per page (usually 4096)

/**
 * Initializes the procfs module.
 * Queries the kernel for system constants (page size, clock ticks).
 * Must be called once at startup.
 * @return 0 on success, -1 on failure.
 */
int procfs_init(void);

/**
 * Fast file reader helper.
 * Reads a file from /proc into a provided buffer.
 * Does NOT use malloc. Uses low-level open/read/close.
 * @param path Full path to file (e.g. "/proc/1/stat")
 * @param buf Target buffer
 * @param max_len Size of buffer
 * @return Number of bytes read, or -1 on error.
 */
ssize_t procfs_read_file(const char *path, char *buf, size_t max_len);

/**
 * Scans /proc for all active processes.
 * Populates the sample->processes array using memory from the Arena.
 * * @param sample Pointer to the system snapshot to populate.
 * @param a The memory arena to allocate process structs from.
 * @return Number of processes scanned, or -1 on critical error.
 */
int procfs_scan(sample_t *sample, Arena *a);

/**
 * Scans a buffer for "Key: 123" and returns 123.
 * Used for /proc/[pid]/io.
 * @param buf Target buffer
 * @param key Key to find (e.g., "read_bytes:")
 * @return Unsigned Integer value, or 0 if not found.
 */
uint64_t procfs_scan_u64(const char *buf, const char *key);

/**
 * Scans a buffer for "Key: 123 kB" and returns 123 * 1024.
 * Used for /proc/[pid]/status.
 * @param buf Target buffer
 * @param key Key to find (e.g., "VmRSS:")
 * @return Unsigned Integer value in Bytes, or 0 if not found.
 */
uint64_t procfs_scan_kb(const char *buf, const char *key);

/**
 * Scans a buffer for "Key: value_string" and copies it.
 * Used for /proc/[pid]/status Name field.
 * @param buf Source buffer
 * @param key Key to find (e.g., "Name:")
 * @param out Destination buffer
 * @param max_len Max chars to write to out
 */
void procfs_scan_str(const char *buf, const char *key, char *out, size_t max_len);

#endif // MEMSCOPE_PROCFS_H