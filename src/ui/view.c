#include "ui/view.h"
#include <string.h>

/**
 * Global view state.
 * 
 * This is the SINGLE source of truth for what's displayed on screen.
 * ui_draw() populates it, input handling reads from it.
 */
static ViewState g_view_state = {0};

// ============================================================
// Public API (read-only access)
// ============================================================

const ViewState *view_get_state(void) {
    return &g_view_state;
}

pid_t view_get_selected_pid(void) {
    if (g_view_state.count == 0) return -1;
    if (g_view_state.selected_idx < 0) return -1;
    if ((size_t)g_view_state.selected_idx >= g_view_state.count) return -1;
    
    return g_view_state.rows[g_view_state.selected_idx].pid;
}

const ViewRow *view_get_selected_row(void) {
    if (g_view_state.count == 0) return NULL;
    if (g_view_state.selected_idx < 0) return NULL;
    if ((size_t)g_view_state.selected_idx >= g_view_state.count) return NULL;
    
    return &g_view_state.rows[g_view_state.selected_idx];
}

// ============================================================
// Internal API (called by display.c)
// ============================================================

void view_begin_frame(int selected_idx, int scroll_offset, int visible_height, bool group_mode) {
    g_view_state.count = 0;
    g_view_state.selected_idx = selected_idx;
    g_view_state.scroll_offset = scroll_offset;
    g_view_state.visible_height = visible_height;
    g_view_state.group_mode = group_mode;
}

void view_add_row(const ViewRow *row) {
    if (!row) return;
    if (g_view_state.count >= VIEW_MAX_ROWS) return;
    
    g_view_state.rows[g_view_state.count++] = *row;
}

void view_end_frame(void) {
    // Clamp selected index to valid range
    if (g_view_state.count > 0) {
        if (g_view_state.selected_idx < 0) {
            g_view_state.selected_idx = 0;
        }
        if ((size_t)g_view_state.selected_idx >= g_view_state.count) {
            g_view_state.selected_idx = (int)g_view_state.count - 1;
        }
    } else {
        g_view_state.selected_idx = -1;
    }
}
