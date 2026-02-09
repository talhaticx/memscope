#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "core/engine.h"
#include "core/time.h"
#include "proc/procfs.h"
#include "proc/pid.h"
#include "util/log.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <errno.h>

// --- Configuration ---
#define ENGINE_UPDATE_INTERVAL_MS 1000  // 1 Hz

// --- Global State ---
static ProcMap *g_proc_map = NULL;
static EngineMemInfo g_meminfo = {0};
static uint64_t g_last_update_ms = 0;

// --- Threading ---
static pthread_t g_collector_thread;
static pthread_rwlock_t g_rwlock = PTHREAD_RWLOCK_INITIALIZER;
static volatile bool g_running = false;
static pthread_mutex_t g_wake_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_wake_cond = PTHREAD_COND_INITIALIZER;

// --- Forward Declarations ---
static void *collector_thread_func(void *arg);
static void do_scan(void);
static int is_digits_only(const char *s);

// ============================================================
// Public API
// ============================================================

int engine_start(void) {
    if (g_running) {
        LOG_WARN("Engine already running");
        return 0;
    }
    
    // Initialize procfs subsystem
    if (procfs_init() != 0) {
        LOG_ERR("Failed to initialize procfs");
        return -1;
    }
    
    // Create process map
    g_proc_map = proc_map_create();
    if (!g_proc_map) {
        LOG_ERR("Failed to create process map");
        return -1;
    }
    
    // Start collector thread
    g_running = true;
    if (pthread_create(&g_collector_thread, NULL, collector_thread_func, NULL) != 0) {
        LOG_ERR("Failed to create collector thread: %s", strerror(errno));
        g_running = false;
        proc_map_destroy(g_proc_map);
        g_proc_map = NULL;
        return -1;
    }
    
    LOG_INFO("Engine started");
    return 0;
}

void engine_stop(void) {
    if (!g_running) return;
    
    // Signal thread to stop
    g_running = false;
    
    // Wake up the thread if sleeping
    pthread_mutex_lock(&g_wake_mutex);
    pthread_cond_signal(&g_wake_cond);
    pthread_mutex_unlock(&g_wake_mutex);
    
    // Wait for thread to finish
    pthread_join(g_collector_thread, NULL);
    
    // Cleanup
    pthread_rwlock_wrlock(&g_rwlock);
    if (g_proc_map) {
        proc_map_destroy(g_proc_map);
        g_proc_map = NULL;
    }
    pthread_rwlock_unlock(&g_rwlock);
    
    LOG_INFO("Engine stopped");
}

bool engine_is_running(void) {
    return g_running;
}

ProcMap *engine_read_lock(void) {
    pthread_rwlock_rdlock(&g_rwlock);
    return g_proc_map;
}

void engine_read_unlock(void) {
    pthread_rwlock_unlock(&g_rwlock);
}

ProcessEntity *engine_get_entity(pid_t pid) {
    pthread_rwlock_rdlock(&g_rwlock);
    ProcessEntity *e = proc_map_get(g_proc_map, pid);
    pthread_rwlock_unlock(&g_rwlock);
    return e;
}

size_t engine_get_process_count(void) {
    pthread_rwlock_rdlock(&g_rwlock);
    size_t count = proc_map_count(g_proc_map);
    pthread_rwlock_unlock(&g_rwlock);
    return count;
}

void engine_get_meminfo(EngineMemInfo *out) {
    if (!out) return;
    pthread_rwlock_rdlock(&g_rwlock);
    *out = g_meminfo;
    pthread_rwlock_unlock(&g_rwlock);
}

uint64_t engine_get_last_update(void) {
    return g_last_update_ms;
}

void engine_force_update(void) {
    pthread_rwlock_wrlock(&g_rwlock);
    do_scan();
    pthread_rwlock_unlock(&g_rwlock);
}

// ============================================================
// Collector Thread
// ============================================================

static void *collector_thread_func(void *arg) {
    (void)arg;
    LOG_DEBUG("Collector thread started");
    
    while (g_running) {
        uint64_t start = time_now_ms();
        
        // Take write lock and do the scan
        pthread_rwlock_wrlock(&g_rwlock);
        do_scan();
        pthread_rwlock_unlock(&g_rwlock);
        
        g_last_update_ms = time_now_ms();
        
        // Sleep until next update
        uint64_t elapsed = g_last_update_ms - start;
        uint64_t sleep_ms = (elapsed < ENGINE_UPDATE_INTERVAL_MS) 
                            ? (ENGINE_UPDATE_INTERVAL_MS - elapsed) 
                            : 0;
        
        if (sleep_ms > 0 && g_running) {
            // Interruptible sleep using condition variable
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += (sleep_ms % 1000) * 1000000;
            ts.tv_sec += sleep_ms / 1000;
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000;
            }
            
            pthread_mutex_lock(&g_wake_mutex);
            pthread_cond_timedwait(&g_wake_cond, &g_wake_mutex, &ts);
            pthread_mutex_unlock(&g_wake_mutex);
        }
    }
    
    LOG_DEBUG("Collector thread exiting");
    return NULL;
}

// ============================================================
// Scanning Logic
// ============================================================

static int is_digits_only(const char *s) {
    while (*s) {
        if (*s < '0' || *s > '9') return 0;
        s++;
    }
    return 1;
}

static void do_scan(void) {
    if (!g_proc_map) return;
    
    uint64_t now = time_now_ms();
    
    // Mark all entities as dead (we'll revive the ones we see)
    proc_map_mark_all_dead(g_proc_map);
    
    // Clear "is_new" flag from previous scan
    proc_map_foreach(g_proc_map, (ProcMapIterator)({
        bool fn(ProcessEntity *e, void *ud) {
            (void)ud;
            e->is_new = false;
            return true;
        }
        fn;
    }), NULL);
    
    // Read global memory info
    // We need a temporary sample_t to use the existing function
    sample_t temp_sample = {0};
    if (procfs_read_meminfo(&temp_sample) == 0) {
        g_meminfo.total_ram = temp_sample.system_total_ram;
        g_meminfo.free_ram = temp_sample.system_free_ram;
        g_meminfo.cached_ram = temp_sample.system_cached_ram;
        g_meminfo.total_swap = temp_sample.system_total_swap;
        g_meminfo.free_swap = temp_sample.system_free_swap;
        g_meminfo.timestamp_ms = now;
    }
    
    // Scan /proc for processes
    DIR *dir = opendir(PROC_ROOT);
    if (!dir) {
        LOG_ERR("Failed to open " PROC_ROOT);
        return;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Filter non-directories
        if (entry->d_type != DT_DIR && entry->d_type != DT_UNKNOWN) continue;
        
        // Filter non-PIDs
        if (!isdigit(entry->d_name[0])) continue;
        if (!is_digits_only(entry->d_name)) continue;
        
        pid_t pid = atoi(entry->d_name);
        if (pid <= 0) continue;
        
        // Get or create entity
        bool created = false;
        ProcessEntity *e = proc_map_upsert(g_proc_map, pid, &created);
        if (!e) continue;
        
        // Parse /proc/[pid]/stat
        process_snapshot_t snap = {0};
        if (pid_parse_stat(pid, &snap) != 0) {
            // Process died, mark as dead for reaping
            e->is_alive = false;
            continue;
        }
        
        // Check for PID reuse (different start_time)
        if (!created && e->start_time != snap.start_time) {
            // PID was reused, reset entity
            entity_reset(e);
            created = true;  // Treat as new
        }
        
        // Calculate CPU percentage (delta since last update)
        double cpu_pct = 0.0;
        if (!created && e->last_update_ms > 0) {
            uint64_t dt = now - e->last_update_ms;
            if (dt > 0) {
                uint64_t prev_cpu = e->utime_ms + e->stime_ms;
                uint64_t curr_cpu = snap.utime_ms + snap.stime_ms;
                if (curr_cpu >= prev_cpu) {
                    cpu_pct = (double)(curr_cpu - prev_cpu) * 100.0 / (double)dt;
                }
            }
        }
        
        // Update entity
        e->is_alive = true;
        e->is_new = created;
        e->last_update_ms = now;
        
        e->start_time = snap.start_time;
        e->ppid = snap.ppid;
        e->state = snap.state;
        memcpy(e->comm, snap.comm, sizeof(e->comm));
        
        e->utime_ms = snap.utime_ms;
        e->stime_ms = snap.stime_ms;
        e->cpu_pct = cpu_pct;
        
        e->rss_bytes = snap.rss_bytes;
        e->vss_bytes = snap.vss_bytes;
        e->swap_bytes = snap.swap_bytes;
        
        e->io_read_bytes = snap.io_read_bytes;
        e->io_write_bytes = snap.io_write_bytes;
        
        // Parse optional fields
        pid_parse_status(pid, &snap);
        pid_parse_io(pid, &snap);
        
        // Push to history
        entity_push_history(e);
    }
    
    closedir(dir);
    
    // Reap dead processes
    size_t reaped = proc_map_reap_dead(g_proc_map);
    if (reaped > 0) {
        LOG_DEBUG("Reaped %zu dead processes", reaped);
    }
}
