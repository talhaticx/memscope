#define _POSIX_C_SOURCE 200809L
#include "core/diff.h"
#include <stdlib.h> // for llabs

void diff_compute(const sample_t *prev, 
                  const sample_t *curr, 
                  diff_callback_t on_event) {
    
    size_t i = 0;
    size_t j = 0;

    // Time delta for CPU calc
    uint64_t dt_ms = curr->timestamp_ms - prev->timestamp_ms;
    if (dt_ms == 0) dt_ms = 1; 

    while (i < prev->process_count || j < curr->process_count) {
        // ... [Cases 1, 2, 4, 5 (Start/Exit) remain the same] ...
        
        // Case 1: Process Started
        if (i >= prev->process_count) {
            on_event(DIFF_EVENT_PROC_START, &curr->processes[j], 0, 0.0);
            j++; continue;
        }
        // Case 2: Process Exited
        if (j >= curr->process_count) {
            on_event(DIFF_EVENT_PROC_EXIT, &prev->processes[i], 0, 0.0);
            i++; continue;
        }

        const process_snapshot_t *p1 = &prev->processes[i];
        const process_snapshot_t *p2 = &curr->processes[j];

        // Case 3: Match
        if (p1->pid == p2->pid) {
            if (p1->start_time != p2->start_time) {
                // PID Reuse
                on_event(DIFF_EVENT_PROC_EXIT, p1, 0, 0.0);
                on_event(DIFF_EVENT_PROC_START, p2, 0, 0.0);
            } else {
                // Same Process: Calculate Deltas
                int64_t rss_delta = (int64_t)p2->rss_bytes - (int64_t)p1->rss_bytes;
                
                uint64_t cpu_delta_ms = (p2->utime_ms + p2->stime_ms) - 
                                        (p1->utime_ms + p1->stime_ms);
                
                double cpu_pct = (double)cpu_delta_ms * 100.0 / (double)dt_ms;

                // --- THE FILTER ---
                // We only report if:
                // 1. RSS change is BIG (positive or negative)
                // 2. OR CPU usage is SIGNIFICANT
                int significant_rss = (llabs(rss_delta) >= DIFF_THRESHOLD_RSS_BYTES);
                int significant_cpu = (cpu_pct >= DIFF_THRESHOLD_CPU_PCT);

                if (significant_rss || significant_cpu) {
                    on_event(DIFF_EVENT_METRIC_CHANGE, p2, rss_delta, cpu_pct);
                }
            }
            i++;
            j++;
        }
        else if (p1->pid < p2->pid) {
            on_event(DIFF_EVENT_PROC_EXIT, p1, 0, 0.0);
            i++;
        }
        else {
            on_event(DIFF_EVENT_PROC_START, p2, 0, 0.0);
            j++;
        }
    }
}