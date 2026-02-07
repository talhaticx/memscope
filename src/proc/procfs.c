// MUST BE FIRST: Enables O_CLOEXEC and other POSIX features
#define _POSIX_C_SOURCE 200809L

#include "proc/procfs.h"
#include "util/log.h"
#include <fcntl.h>      // for open, O_RDONLY
#include <unistd.h>     // for read, close, sysconf
#include <errno.h>
#include <string.h> // for strstr
#include <stdint.h>

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

// --- Private Helper: Fast String to Int ---
static uint64_t fast_atoi(const char *str) {
    uint64_t val = 0;
    while (*str >= '0' && *str <= '9') {
        val = (val * 10) + (*str++ - '0');
    }
    return val;
}

// --- Public Helper: Find Key and Parse Number ---
uint64_t procfs_scan_u64(const char *buf, const char *key) {
    // 1. Find the key in the buffer (e.g. "rchar:")
    const char *line = strstr(buf, key);
    if (!line) return 0; // Key not found

    // 2. Skip the key length
    line += strlen(key);

    // 3. Skip whitespace (spaces/tabs)
    while (*line == ' ' || *line == '\t') line++;

    // 4. Parse the number
    return fast_atoi(line);
}

// --- Public Helper: Find Key, Parse Number, Convert kB to Bytes ---
uint64_t procfs_scan_kb(const char *buf, const char *key) {
    // Reuse the logic above, then scale
    uint64_t val_kb = procfs_scan_u64(buf, key);
    return val_kb * 1024;
}
// --- Public Helper: Find Key and Parse String ---
void procfs_scan_str(const char *buf, const char *key, char *out, size_t max_len) {
    const char *ptr = strstr(buf, key);
    if (!ptr) return;

    ptr += strlen(key);
    while (*ptr == ' ' || *ptr == '\t') ptr++;

    /* Copy until whitespace or newline */
    size_t i = 0;
    while (ptr[i] && ptr[i] != '\n' && i < max_len - 1) {
        out[i] = ptr[i];
        i++;
    }
    out[i] = '\0';
}