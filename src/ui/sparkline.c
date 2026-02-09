#define _POSIX_C_SOURCE 200809L
#include "ui/sparkline.h"
#include <ncurses.h>
#include <string.h>
#include <float.h>

// Unicode sparkline characters (8 levels from low to high)
// These render as proper vertical bars for beautiful graphs
static const char *SPARK_CHARS[] = {"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
#define SPARK_LEVELS 8

void sparkline_draw(int y, int x, const double *values, size_t count, int width) {
    if (!values || count == 0 || width <= 0) return;

    // Find min/max for scaling
    double min_val = DBL_MAX;
    double max_val = -DBL_MAX;
    for (size_t i = 0; i < count; i++) {
        if (values[i] < min_val) min_val = values[i];
        if (values[i] > max_val) max_val = values[i];
    }

    double range = max_val - min_val;
    if (range < 0.001) range = 1.0; // Avoid division by zero

    move(y, x);
    
    // Sample from the end of the array (most recent values)
    size_t start_idx = 0;
    if (count > (size_t)width) {
        start_idx = count - width;
    }
    
    for (int i = 0; i < width && (start_idx + i) < count; i++) {
        size_t idx = start_idx + i;
        
        double normalized = (values[idx] - min_val) / range; // 0.0 to 1.0
        int level = (int)(normalized * (SPARK_LEVELS - 1));
        if (level < 0) level = 0;
        if (level >= SPARK_LEVELS) level = SPARK_LEVELS - 1;

        // Use addstr for multi-byte Unicode characters
        addstr(SPARK_CHARS[level]);
    }
}

void sparkline_to_str(char *out, const double *values, size_t count, double min_val, double max_val, int width) {
    if (!values || count == 0 || width <= 0 || !out) {
        if (out) out[0] = '\0';
        return;
    }

    double range = max_val - min_val;
    if (range < 0.001) range = 1.0;

    // We want the LAST 'width' items
    size_t start_idx = 0;
    if (count > (size_t)width) {
        start_idx = count - width;
    }

    int out_pos = 0;
    for (int i = 0; i < width && (start_idx + i) < count; i++) {
        size_t idx = start_idx + i;
        
        double val = values[idx];
        if (val < min_val) val = min_val;
        if (val > max_val) val = max_val;

        double normalized = (val - min_val) / range;
        int level = (int)(normalized * (SPARK_LEVELS - 1));
        if (level < 0) level = 0;
        if (level >= SPARK_LEVELS) level = SPARK_LEVELS - 1;

        // Copy multi-byte Unicode character
        const char *ch = SPARK_CHARS[level];
        while (*ch) {
            out[out_pos++] = *ch++;
        }
    }
    out[out_pos] = '\0';
}
