#ifndef MEMSCOPE_VIEW_H
#define MEMSCOPE_VIEW_H

#include <sys/types.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * View State Module
 * 
 * CRITICAL: This decouples rendering from data state.
 * 
 * PROBLEM: When user presses Enter, old code re-sorted the list to find
 * the selected PID. Between cursor movement and Enter press, the list
 * could re-sort, causing wrong process selection.
 * 
 * SOLUTION: Snapshot the visible rows at render time. Input handling
 * reads from this frozen snapshot, guaranteed to match what user sees.
 */

#define VIEW_MAX_ROWS 1024

/**
 * A single row in the view.
 * Contains just enough info for selection and display.
 */
typedef struct {
    pid_t pid;               // Process ID
    char comm[16];           // Process name
    double cpu_pct;          // CPU usage %
    uint64_t rss_bytes;      // Memory usage
    double rss_delta_mb;     // Memory change (vs baseline)
    uint8_t tags;            // Leak/Spike/Zombie flags
    bool is_group;           // true if this is a group summary
    int group_count;         // Number of processes in group
} ViewRow;

/**
 * The frozen view state.
 * Populated once per render, then read-only until next render.
 */
typedef struct {
    ViewRow rows[VIEW_MAX_ROWS];  // Visible rows
    size_t count;                  // Number of populated rows
    int selected_idx;              // Current cursor position
    int scroll_offset;             // First visible row index
    int visible_height;            // Number of visible rows
    bool group_mode;               // true if in group view
} ViewState;

/**
 * Get the current view state snapshot.
 * This is updated by ui_draw() at the start of each frame.
 * 
 * DO NOT MODIFY the returned pointer's contents!
 */
const ViewState *view_get_state(void);

/**
 * Get the PID at the selected index.
 * Returns -1 if nothing selected or out of bounds.
 * 
 * This is THE safe way to get the selected PID for Enter key handling.
 */
pid_t view_get_selected_pid(void);

/**
 * Get the selected row data.
 * Returns NULL if nothing selected.
 */
const ViewRow *view_get_selected_row(void);

// --- Internal (called by display.c) ---

/**
 * Begin populating the view state for a new frame.
 * Resets the row count and prepares for adding rows.
 */
void view_begin_frame(int selected_idx, int scroll_offset, int visible_height, bool group_mode);

/**
 * Add a row to the view state.
 * Called during ui_draw() to populate visible_rows.
 */
void view_add_row(const ViewRow *row);

/**
 * Finalize the frame after all rows are added.
 */
void view_end_frame(void);

#endif // MEMSCOPE_VIEW_H
