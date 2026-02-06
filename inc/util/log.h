#ifndef MEMSCOPE_LOG_H
#define MEMSCOPE_LOG_H

#include <stdio.h>

// Log Levels
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} log_level_t;

/**
 * Core logging function.
 * @param level The severity of the message.
 * @param file Source file name (injected by macro).
 * @param line Line number (injected by macro).
 * @param fmt Printf-style format string.
 */
void log_log(log_level_t level, const char *file, int line, const char *fmt, ...);

// Convenience Macros - USE THESE IN YOUR CODE
// Usage: LOG_INFO("Process %d started", pid);
#define LOG_DEBUG(...) log_log(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  log_log(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  log_log(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERR(...)   log_log(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)

#endif // MEMSCOPE_LOG_H