#define _POSIX_C_SOURCE 200809L
#include "ui/box.h"
#include <ncurses.h>
#include <string.h>

// Unicode Box Drawing Characters (UTF-8)
// ┌ ─ ┐
// │   │
// ├ ─ ┤
// └ ─ ┘

void box_draw(int y, int x, int height, int width, const char *title) {
    // Top border
    mvaddstr(y, x, "┌");
    for (int i = 1; i < width - 1; i++) {
        addstr("─");
    }
    addstr("┐");

    // Title (if provided)
    if (title && strlen(title) > 0) {
        int title_len = strlen(title);
        int title_x = x + 2;
        if (title_len < width - 4) {
            mvprintw(y, title_x, "[ %s ]", title);
        }
    }

    // Sides
    for (int row = 1; row < height - 1; row++) {
        mvaddstr(y + row, x, "│");
        mvaddstr(y + row, x + width - 1, "│");
    }

    // Bottom border
    mvaddstr(y + height - 1, x, "└");
    for (int i = 1; i < width - 1; i++) {
        addstr("─");
    }
    addstr("┘");
}

void box_hline(int y, int x, int width) {
    mvaddstr(y, x, "├");
    for (int i = 1; i < width - 1; i++) {
        addstr("─");
    }
    addstr("┤");
}
