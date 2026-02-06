#include "util/log.h"
#include <stdarg.h>
#include <time.h>
#include <string.h>

// ANSI Color Codes
#define COLOR_RESET  "\x1b[0m"
#define COLOR_DEBUG  "\x1b[36m" // Cyan
#define COLOR_INFO   "\x1b[32m" // Green
#define COLOR_WARN   "\x1b[33m" // Yellow
#define COLOR_ERROR  "\x1b[31m" // Red

static const char *level_strings[] = {
    "DEBUG", "INFO", "WARN", "ERROR"
};

static const char *level_colors[] = {
    COLOR_DEBUG, COLOR_INFO, COLOR_WARN, COLOR_ERROR
};

void log_log(log_level_t level, const char *file, int line, const char *fmt, ...) {
    // 1. Get current time
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    char time_buf[16];
    // Format: HH:MM:SS
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", lt);

    // 2. Prepare varargs
    va_list args;
    va_start(args, fmt);

    // 3. Print to stderr
    // Format: [HH:MM:SS] [INFO] (main.c:45) Message...
    fprintf(stderr, "%s[%s] [%-5s] " COLOR_RESET "\x1b[90m(%s:%d)\x1b[0m ",
            level_colors[level],
            time_buf, 
            level_strings[level],
            file, 
            line);
    
    // Print the actual message
    vfprintf(stderr, fmt, args);
    
    // Reset color and newline
    fprintf(stderr, "\n"); // removed COLOR_RESET here as it's cleaner to handle per line
    
    va_end(args);
}