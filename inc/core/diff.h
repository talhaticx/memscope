#ifndef MEMSCOPE_DIFF_H
#define MEMSCOPE_DIFF_H

#include "core/sample.h"

// --- Tuning Knobs ---

// 1. RSS Threshold: Ignore changes smaller than this.
//    Setting this to 1MB (1024*1024) filters out 90% of noise.
//    For development/testing, 4KB is fine. For production, 1MB is better.
#define DIFF_THRESHOLD_RSS_BYTES (1024 * 1024) 

// 2. CPU Threshold: Ignore usage below this percent.
//    0.5% filters out idle background tasks.
#define DIFF_THRESHOLD_CPU_PCT   (0.5)

// Types of events the engine can detect
typedef enum {
    DIFF_EVENT_PROC_START,
    DIFF_EVENT_PROC_EXIT,
    DIFF_EVENT_METRIC_CHANGE
} diff_event_type_t;

/**
 * callback_t: The function signature for reporting events.
 * @param type      What happened? (START, EXIT, CHANGE)
 * @param proc      The process snapshot (New for START/CHANGE, Old for EXIT)
 * @param rss_delta Change in RSS (bytes). 0 for Start/Exit.
 * @param cpu_pct   CPU usage since last sample.
 */
typedef void (*diff_callback_t)(diff_event_type_t type, 
                                const process_snapshot_t *proc, 
                                int64_t rss_delta, 
                                double cpu_pct);

/**
 * Compares two snapshots.
 * Instead of printing, it calls 'on_event' for every significant change.
 */
void diff_compute(const sample_t *prev, 
                  const sample_t *curr, 
                  diff_callback_t on_event);

#endif // MEMSCOPE_DIFF_H