#ifndef MEMSCOPE_TIME_H
#define MEMSCOPE_TIME_H

#include <stdint.h>

/**
 * Returns the current monotonic time in milliseconds.
 * * Origin: Undefined (usually boot time).
 * Use case: Relative time measurements (intervals, rates).
 * Immune to: NTP updates, user changing system clock.
 */
uint64_t time_now_ms(void);

/**
 * Calculates the difference between two timestamps safely.
 * Handles potential (though rare) integer overflow wrapping if runs for 500+ years.
 */
uint64_t time_diff_ms(uint64_t start, uint64_t end);

/**
 * Sleep for a specified number of milliseconds.
 * More precise than standard sleep().
 */
void time_sleep_ms(uint32_t ms);

#endif // MEMSCOPE_TIME_H