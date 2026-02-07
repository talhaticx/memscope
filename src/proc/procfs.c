// MUST BE FIRST: Enables O_CLOEXEC and other POSIX features
#define _POSIX_C_SOURCE 200809L

#include "proc/procfs.h"
#include "util/log.h"
#include <fcntl.h>      // for open, O_RDONLY
#include <unistd.h>     // for read, close, sysconf
#include <errno.h>

// Global variables definition
long sc_clk_tck = 0;
long sc_page_size = 0;

int procfs_init(void) {
    // 1. Get Clock Ticks per Second
    sc_clk_tck = sysconf(_SC_CLK_TCK);
    if (sc_clk_tck <= 0) {
        LOG_ERR("Failed to get _SC_CLK_TCK");
        return -1;
    }

    // 2. Get Page Size
    sc_page_size = sysconf(_SC_PAGESIZE);
    if (sc_page_size <= 0) {
        LOG_ERR("Failed to get _SC_PAGESIZE");
        return -1;
    }

    LOG_INFO("System Constants: CLK_TCK=%ld, PAGE_SIZE=%ld bytes", 
             sc_clk_tck, sc_page_size);
    
    return 0;
}

ssize_t procfs_read_file(const char *path, char *buf, size_t max_len) {
    // O_CLOEXEC prevents file descriptor leaks to child processes (security best practice)
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    
    if (fd < 0) {
        // It is normal for open() to fail if a process dies before we read it.
        // We do not log error here to avoid spamming stderr.
        return -1;
    }

    // Read up to max_len - 1 to ensure we have space for null terminator
    ssize_t bytes_read = read(fd, buf, max_len - 1);
    
    close(fd);

    if (bytes_read >= 0) {
        buf[bytes_read] = '\0'; // Null-terminate safely
    }

    return bytes_read;
}