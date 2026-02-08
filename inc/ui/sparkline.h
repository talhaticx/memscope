#ifndef MEMSCOPE_SPARKLINE_H
#define MEMSCOPE_SPARKLINE_H

#include <stddef.h>

/**
 * Sparkline Module
 * Provides functions for drawing mini trend graphs using Unicode blocks.
 * Uses characters: ▁▂▃▄▅▆▇█ (8 levels)
 */

/**
 * Draw a sparkline from an array of values.
 * @param y Row position
 * @param x Column position
 * @param values Array of values to plot
 * @param count Number of values
 * @param width Display width (values will be sampled if count > width)
 */
void sparkline_draw(int y, int x, const double *values, size_t count, int width);

/**
 * Get the sparkline string for embedding in other UI elements.
 * @param values Array of values
 * @param count Number of values
 * @param min_val Minimum value for scaling (frame of reference)
 * @param max_val Maximum value for scaling
 * @param width Number of characters to output
 * @param out Output buffer (caller allocates, needs width + 1 bytes)
 */
void sparkline_to_str(char *out, const double *values, size_t count, double min_val, double max_val, int width);

#endif // MEMSCOPE_SPARKLINE_H
