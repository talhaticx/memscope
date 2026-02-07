// Enable POSIX features
#define _POSIX_C_SOURCE 200809L

#include "proc/procfs.h"
#include "core/sample.h"
#include "util/log.h"
#include <stdio.h>

int procfs_read_meminfo(sample_t *sample) {
    char buf[2048]; // meminfo is usually ~1.5KB

    // 1. Read the file
    if (procfs_read_file("/proc/meminfo", buf, sizeof(buf)) <= 0) {
        LOG_ERR("Failed to read /proc/meminfo");
        return -1;
    }

    // 2. Parse Metrics (All in kB, converted to Bytes by helper)
    sample->system_total_ram = procfs_scan_kb(buf, "MemTotal:");
    
    // We prefer MemAvailable over MemFree because it accounts for reclaimable cache.
    // Older kernels (<3.14) might not have MemAvailable; fallback to MemFree would be ideal,
    // but for this project we assume modern Linux.
    sample->system_free_ram  = procfs_scan_kb(buf, "MemAvailable:");
    
    // Calculate Cached: Buffers + Cached + SReclaimable (slab reclaimable)
    // SReclaimable is often significant on servers.
    uint64_t buffers = procfs_scan_kb(buf, "Buffers:");
    uint64_t cached  = procfs_scan_kb(buf, "Cached:");
    uint64_t sreclaim = procfs_scan_kb(buf, "SReclaimable:");
    
    sample->system_cached_ram = buffers + cached + sreclaim;

    // Swap Metrics
    sample->system_total_swap = procfs_scan_kb(buf, "SwapTotal:");
    sample->system_free_swap  = procfs_scan_kb(buf, "SwapFree:");

    return 0;
}