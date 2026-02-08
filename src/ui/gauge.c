#define _POSIX_C_SOURCE 200809L
#include "ui/gauge.h"
#include <ncurses.h>
#include <stdio.h>
#include <string.h>

// Color pairs (must match display.c)
#define COL_BAR_LOW    5  // Green
#define COL_BAR_MED    6  // Yellow
#define COL_BAR_HIGH   7  // Red

void gauge_draw(int y, int x, int width, double percent, gauge_style_t style) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    int inner_width = width - 2; // Excluding brackets
    int fill_len = (int)((percent / 100.0) * inner_width);

    mvaddch(y, x, '[');

    // Choose color based on percentage
    int color = COL_BAR_LOW;
    if (percent > 70) color = COL_BAR_HIGH;
    else if (percent > 50) color = COL_BAR_MED;

    attron(COLOR_PAIR(color));
    for (int i = 0; i < inner_width; i++) {
        if (i < fill_len) {
            addch(style == GAUGE_STYLE_BLOCK ? '#' : '|');
        } else {
            addch('-');
        }
    }
    attroff(COLOR_PAIR(color));

    addch(']');
}

void gauge_draw_colored(int y, int x, int width, double current, double max, int color_pair) {
    if (x < 0 || y < 0 || width < 3) return;
    
    double pct = (max > 0) ? (current / max) : 0;
    if (pct > 1.0) pct = 1.0;
    
    mvaddch(y, x, '[');
    int bar_w = width - 2;
    int fill = (int)(pct * bar_w);
    
    attron(COLOR_PAIR(color_pair));
    for (int i = 0; i < bar_w; i++) {
        addch(i < fill ? '|' : ' ');
    }
    attroff(COLOR_PAIR(color_pair));
    addch(']');
}

void gauge_draw_labeled(int y, int x, const char *label, 
                        double current, double max, const char *unit, int width) {
    // Format: "LABEL [████░░░░] CURRENT/MAX UNIT"
    double percent = (max > 0) ? (current / max) * 100.0 : 0;

    // Draw label
    mvprintw(y, x, "%s ", label);
    int label_len = strlen(label) + 1;

    // Draw gauge
    gauge_draw(y, x + label_len, width, percent, GAUGE_STYLE_BLOCK);

    // Draw values
    mvprintw(y, x + label_len + width + 1, "%.1f/%.0f %s", current, max, unit);
}
