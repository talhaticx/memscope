#ifndef MEMSCOPE_DISPLAY_H
#define MEMSCOPE_DISPLAY_H

#include "core/sample.h"
#include "proc/smaps.h"
#include <sys/types.h>

// Initialize NCurses (colors, keyboard, no echo)
void ui_init(void);

// Restore terminal state (cursor, echo)
void ui_cleanup(void);

// Draw the main dashboard
// curr: Current sample
// compare: Comparison sample (baseline or previous, can be NULL)
// selected_idx: Row currently highlighted
// has_baseline: 1 if comparing to baseline, 0 if comparing to previous
void ui_draw(const sample_t *curr, const sample_t *compare, int selected_idx, int has_baseline);

// Draw detailed inspection view for a single process
// baseline: Baseline smaps for delta comparison
// history: Array of last N RSS values for sparkline
void ui_draw_detail(pid_t pid, const smaps_breakdown_t *smaps, const smaps_breakdown_t *baseline,
                   const sample_t *curr, const double *history, int history_count);

// Get the PID of the process at the selected row (after sorting)
// Returns -1 if invalid
pid_t ui_get_selected_pid(const sample_t *curr, const sample_t *prev, int selected_idx);

// Toggle between List and Group mode
void ui_toggle_grouping(void);

// Reset all auto-baselines (call when Space is pressed for manual baseline)
void ui_reset_baselines(void);

// Handle user input (Non-blocking)
// Returns: 
//   0 = No input / sort key
//   1 = Quit / Back
//   2 = Up Arrow
//   3 = Down Arrow
//   4 = Enter (Inspect)
//   5 = Space (Baseline)
//   6 = w (Watchlist)
//   7 = g (Group Toggle)
int ui_poll_input(void);

#endif // MEMSCOPE_DISPLAY_H