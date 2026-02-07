#ifndef MEMSCOPE_PROCFS_H
#define MEMSCOPE_PROCFS_H

#include <stddef.h>
#include <unistd.h> // for sysconf

#define PROC_ROOT "/proc"

// Global System Constants (Populated by procfs_init)
// We use 'extern' so they are accessible anywhere but defined in .c
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

#endif // MEMSCOPE_PROCFS_H