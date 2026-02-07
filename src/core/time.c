// Feature test macro required for CLOCK_MONOTONIC in strict C environments
#define _POSIX_C_SOURCE 199309L 

#include "core/time.h"
#include <time.h>
#include <errno.h>

// 1 second = 1000 milliseconds
// 1 millisecond = 1,000,000 nanoseconds
#define NS_PER_MS 1000000ULL
#define MS_PER_SEC 1000ULL

uint64_t time_now_ms(void) {
    struct timespec ts;
    
    // CLOCK_MONOTONIC: Represents time since some unspecified starting point (boot).
    // It cannot jump backwards.
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        // Fallback or error handling if needed. 
        // In reality, this syscall never fails on modern Linux unless pointers are bad.
        return 0;
    }

    // Convert everything to milliseconds
    uint64_t ms = (uint64_t)ts.tv_sec * MS_PER_SEC;
    ms += (uint64_t)ts.tv_nsec / NS_PER_MS;

    return ms;
}

uint64_t time_diff_ms(uint64_t start, uint64_t end) {
    if (end < start) {
        // Should not happen with CLOCK_MONOTONIC, but safety first.
        return 0;
    }
    return end - start;
}

void time_sleep_ms(uint32_t ms) {
    struct timespec req, rem;
    
    req.tv_sec = ms / MS_PER_SEC;
    req.tv_nsec = (ms % MS_PER_SEC) * NS_PER_MS;

    // nanosleep is better than usleep because it doesn't interact with signals 
    // in the same legacy way and is higher precision.
    while (nanosleep(&req, &rem) == -1) {
        if (errno == EINTR) {
            // Interrupted by signal (e.g. user resizing window), resume sleeping
            req = rem;
        } else {
            break;
        }
    }
}