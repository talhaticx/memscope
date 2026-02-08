#ifndef MEMSCOPE_GAUGE_H
#define MEMSCOPE_GAUGE_H

#include <stdint.h>

/**
 * Gauge Module
 * Provides functions for drawing progress bars and gauges.
 */

typedef enum {
    GAUGE_STYLE_BLOCK,    // [████████░░░░]
    GAUGE_STYLE_BAR       // [||||||||    ]
} gauge_style_t;

/**
 * Draw a horizontal gauge bar.
 * @param y Row position
 * @param x Column position
 * @param width Total width including brackets
 * @param percent Value 0.0 to 100.0
 * @param style Visual style
 */
void gauge_draw(int y, int x, int width, double percent, gauge_style_t style);

/**
 * Draw a labeled gauge with text (Legacy)
 */
void gauge_draw_labeled(int y, int x, const char *label, 
                        double current, double max, const char *unit, int width);

/**
 * Draw a simple gauge bar with a specific color.
 * @param y Row position
 * @param x Column position
 * @param width Total width
 * @param current Current value
 * @param max Maximum value
 * @param color_pair NCurses color pair to use for the bar
 */
void gauge_draw_colored(int y, int x, int width, double current, double max, int color_pair);

#endif // MEMSCOPE_GAUGE_H
