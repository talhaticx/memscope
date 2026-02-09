/**
 * @file main.c
 * @brief Memscope application entry point and main event loop.
 *
 * This file implements the core application loop which:
 *   1. Initializes memory arenas (double-buffering for zero-malloc hot path)
 *   2. Handles user input (navigation, inspect mode, baseline reset)
 *   3. Captures system/process data at 1 Hz
 *   4. Renders the UI at 60 FPS
 *
 * Architecture:
 *   - Two arenas (arena_a, arena_b) swap each capture tick
 *   - A third arena (arena_baseline) stores the user-set baseline
 *   - ViewState (in display.c) freezes UI state during render for stable selection
 *
 * Main Loop Structure:
 *   while (running) {
 *       A. Handle input (60 FPS polling)
 *       B. Capture data (1 Hz)
 *       C. Render UI (60 FPS)
 *   }
 */

// Enable Linux/BSD default features (includes usleep, DT_DIR, etc.)
#define _DEFAULT_SOURCE

#include "core/sample.h"
#include "core/time.h"
#include "proc/smaps.h"
#include "ui/display.h"
#include "util/arena.h"
#include "util/log.h"
#include <signal.h>
#include <string.h>
#include <ctype.h>

// --- Configuration ---
#define UI_TICK_MS      16      // ~60 FPS
#define DATA_TICK_MS    1000    // 1 Hz data capture

static volatile int running = 1;

void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

int main(void) {
    signal(SIGINT, handle_sigint);

    // ==============================================
    // 1. SETUP DATA - Double Buffering + Baseline
    // ==============================================
    size_t arena_size = 4 * 1024 * 1024;
    Arena *arena_a = arena_create(arena_size);
    Arena *arena_b = arena_create(arena_size);
    Arena *arena_baseline = arena_create(arena_size);  // Dedicated baseline arena
    
    sample_t sample_a = {0};
    sample_t sample_b = {0};
    sample_t sample_baseline = {0};

    Arena *arena_curr = arena_a;
    Arena *arena_prev = arena_b;
    sample_t *curr = &sample_a;
    sample_t *prev = &sample_b;

    int has_prev = 0;
    int has_baseline = 0;
    sample_t *baseline = &sample_baseline;

    // ==============================================
    // 2. SETUP UI STATE
    // ==============================================
    int selected_row = 0;
    int inspect_mode = 0;           // 0 = list, 1 = detail view
    smaps_breakdown_t inspect_smaps = {0};
    pid_t inspect_pid = 0;
    
    // History for Sparkline
    #define HISTORY_MAX 64
    double history[HISTORY_MAX] = {0};
    int history_count = 0;
    
    // Baseline for inspect mode
    smaps_breakdown_t inspect_baseline = {0};
    int has_inspect_baseline = 0;
    
    ui_init();

    // ==============================================
    // 3. TIMERS
    // ==============================================
    uint64_t last_data_tick = 0;
    uint64_t last_ui_tick = 0;
    uint64_t now;

    // Initial capture
    arena_reset(arena_curr);
    sample_capture(curr, arena_curr);

    while (running) {
        now = time_now_ms();

        // -----------------------------------------
        // A. HANDLE INPUT
        // -----------------------------------------
        int action = ui_poll_input();
        switch (action) {
            case 1: // Quit
                if (inspect_mode) {
                    inspect_mode = 0; // Exit detail view first
                } else {
                    running = 0; // Exit app
                }
                break;
            case 2: // Up
                if (!inspect_mode) selected_row--;
                if (selected_row < 0) selected_row = 0;
                break;
            case 3: // Down
                if (!inspect_mode) selected_row++;
                break;
            case 4: // Enter (Inspect)
                if (!inspect_mode) {
                    pid_t pid = ui_get_selected_pid(curr, has_prev ? prev : NULL, selected_row);
                    if (pid > 0) {
                        inspect_mode = 1;
                        inspect_pid = pid;
                        smaps_parse(inspect_pid, &inspect_smaps);
                        // Set baseline to current smaps on entry
                        inspect_baseline = inspect_smaps;
                        has_inspect_baseline = 1;
                        // Reset history for new process
                        memset(history, 0, sizeof(history));
                        history_count = 0;
                    }
                }
                break;
            case 5: // Space (Baseline)
                if (inspect_mode) {
                    // In inspect mode, Space resets baseline
                    inspect_baseline = inspect_smaps;
                } else {
                    // In list mode, reset all auto-baselines
                    ui_reset_baselines();
                    has_baseline = 1;
                    arena_reset(arena_baseline);
                    sample_copy(curr, baseline, arena_baseline);
                }
                break;
            case 7: // 'g' (Group Toggle)
                ui_toggle_grouping();
                selected_row = 0;
                break;
        }

        // Clamp selection
        if (curr->process_count > 0 && selected_row >= (int)curr->process_count) {
            selected_row = (int)curr->process_count - 1;
        }

        // -----------------------------------------
        // B. DATA CAPTURE (1 Hz)
        // -----------------------------------------
        if (now - last_data_tick >= DATA_TICK_MS) {
            last_data_tick = now;

            if (inspect_mode && inspect_pid > 0) {
                smaps_parse(inspect_pid, &inspect_smaps);
                
                // Update RSS history for sparkline
                if (history_count < HISTORY_MAX) {
                    history[history_count++] = inspect_smaps.total_rss_kb / 1024.0;
                } else {
                    memmove(history, history + 1, sizeof(double) * (HISTORY_MAX - 1));
                    history[HISTORY_MAX - 1] = inspect_smaps.total_rss_kb / 1024.0;
                }
            }
            
            // ALWAYS capture samples (even in inspect mode) to keep data fresh
            sample_t *temp = prev; prev = curr; curr = temp;
            Arena *ta = arena_prev; arena_prev = arena_curr; arena_curr = ta;

            arena_reset(arena_curr);
            if (sample_capture(curr, arena_curr) == 0) {
                has_prev = 1;
            }
        }

        // -----------------------------------------
        // C. UI RENDER (60 FPS)
        // -----------------------------------------
        if (now - last_ui_tick >= UI_TICK_MS) {
            last_ui_tick = now;
            
            if (inspect_mode) {
                ui_draw_detail(inspect_pid, &inspect_smaps, &inspect_baseline, curr, history, history_count);
            } else {
                // Pass baseline for delta comparison if available
                sample_t *compare = has_baseline ? baseline : (has_prev ? prev : NULL);
                ui_draw(curr, compare, selected_row, has_baseline);
            }
        }

        time_sleep_ms(1);
    }

    // ==============================================
    // 4. CLEANUP
    // ==============================================
    ui_cleanup();
    arena_destroy(arena_a);
    arena_destroy(arena_b);
    arena_destroy(arena_baseline);
    return 0;
}