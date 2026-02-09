/**
 * @file gauge.c
 * @brief Unicode block gauges for visual memory/CPU representation.
 *
 * Renders horizontal gauges using Unicode block elements:
 *   [████████░░░░░░░░] 66%
 *
 * Features:
 *   - Sub-character precision using 8-level fractional blocks (▏▎▍▌▋▊▉█)
 *   - Three-tier coloring: green (low), yellow (medium), red (high)
 *   - Automatic clamping for out-of-range values
 *
 * @see inc/ui/gauge.h for API
 */

#define _POSIX_C_SOURCE 200809L
#include "ui/gauge.h"
#include <ncurses.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>

// Color pairs (must match display.c)
#define COL_BAR_LOW    5  // Green
#define COL_BAR_MED    6  // Yellow
#define COL_BAR_HIGH   7  // Red

// Unicode block elements for high-resolution gauges
// From full (█) to 1/8 block (▏) - 8 levels of precision
static const char *BLOCK_CHARS[] = {
    " ",     // 0/8 (empty)
    "▏",    // 1/8
    "▎",    // 2/8
    "▍",    // 3/8
    "▌",    // 4/8 (half)
    "▋",    // 5/8
    "▊",    // 6/8
    "▉",    // 7/8
    "█"     // 8/8 (full)
};

void gauge_draw(int y, int x, int width, double percent, gauge_style_t style) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    int inner_width = width - 2; // Excluding brackets
    
    // Calculate fill with sub-character precision
    double fill_exact = (percent / 100.0) * inner_width;
    int fill_full = (int)fill_exact;  // Number of full blocks
    double remainder = fill_exact - fill_full;  // Fractional part (0.0-1.0)
    int partial_idx = (int)(remainder * 8);  // Which partial block (0-7)

    mvaddch(y, x, '[');

    // Choose color based on percentage
    int color = COL_BAR_LOW;
    if (percent > 70) color = COL_BAR_HIGH;
    else if (percent > 50) color = COL_BAR_MED;

    attron(COLOR_PAIR(color));

    if (style == GAUGE_STYLE_BLOCK) {
        // Unicode high-resolution bar
        for (int i = 0; i < inner_width; i++) {
            if (i < fill_full) {
                addstr(BLOCK_CHARS[8]);  // Full block █
            } else if (i == fill_full && partial_idx > 0) {
                addstr(BLOCK_CHARS[partial_idx]);  // Partial block
            } else {
                addch(' ');  // Empty
            }
        }
    } else {
        // ASCII fallback (GAUGE_STYLE_LINE)
        for (int i = 0; i < inner_width; i++) {
            if (i < fill_full) {
                addch('|');
            } else {
                addch('-');
            }
        }
    }
    
    attroff(COLOR_PAIR(color));
    addch(']');
}

void gauge_draw_colored(int y, int x, int width, double current, double max, int color_pair) {
    if (x < 0 || y < 0 || width < 3) return;
    
    double pct = (max > 0) ? (current / max) : 0;
    if (pct > 1.0) pct = 1.0;
    if (pct < 0) pct = 0;
    
    mvaddch(y, x, '[');
    int bar_w = width - 2;
    
    // High-resolution fill
    double fill_exact = pct * bar_w;
    int fill_full = (int)fill_exact;
    double remainder = fill_exact - fill_full;
    int partial_idx = (int)(remainder * 8);
    
    attron(COLOR_PAIR(color_pair));
    for (int i = 0; i < bar_w; i++) {
        if (i < fill_full) {
            addstr(BLOCK_CHARS[8]);  // Full block █
        } else if (i == fill_full && partial_idx > 0) {
            addstr(BLOCK_CHARS[partial_idx]);  // Partial block
        } else {
            addch(' ');
        }
    }
    attroff(COLOR_PAIR(color_pair));
    addch(']');
}

void gauge_draw_labeled(int y, int x, const char *label, 
                        double current, double max, const char *unit, int width) {
    // Format: "LABEL [████    ] CURRENT/MAX UNIT"
    double percent = (max > 0) ? (current / max) * 100.0 : 0;

    // Draw label
    mvprintw(y, x, "%s ", label);
    int label_len = strlen(label) + 1;

    // Draw gauge
    gauge_draw(y, x + label_len, width, percent, GAUGE_STYLE_BLOCK);

    // Draw values
    mvprintw(y, x + label_len + width + 1, "%.1f/%.0f %s", current, max, unit);
}

// New: Vertical gauge using block levels ▁▂▃▄▅▆▇█
static const char *VBLOCK_CHARS[] = {
    " ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"
};

void gauge_draw_vertical_bar(int y, int x, double percent, int color_pair) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    
    int level = (int)((percent / 100.0) * 8);
    if (level > 8) level = 8;
    
    attron(COLOR_PAIR(color_pair));
    mvaddstr(y, x, VBLOCK_CHARS[level]);
    attroff(COLOR_PAIR(color_pair));
}
