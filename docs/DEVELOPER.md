# Memscope Developer Guide

This guide explains the core concepts and architecture for contributors.

## Table of Contents
1. [Core Concepts](#core-concepts)
2. [Data Flow](#data-flow)
3. [Module Overview](#module-overview)
4. [Key Data Structures](#key-data-structures)
5. [Adding New Features](#adding-new-features)
6. [Code Style](#code-style)

---

## Core Concepts

### Arena Allocator (Zero-Malloc Hot Path)

The biggest performance win in memscope is **never calling malloc/free in the main loop**.

```c
// Instead of:
process_t *p = malloc(sizeof(process_t));  // ❌ Slow, fragments memory
// ...
free(p);

// We use:
process_t *p = arena_alloc(arena, sizeof(process_t));  // ✅ O(1), just bump pointer
// At end of frame:
arena_reset(arena);  // ✅ O(1), just reset pointer to start
```

**How it works:**
1. At startup, allocate one big block (e.g., 64MB)
2. `arena_alloc()` just bumps a pointer forward
3. `arena_reset()` sets pointer back to start
4. All allocations are freed at once — no individual frees

**Files:** `inc/util/arena.h`, `src/util/arena.c`

### Double Buffering

We keep two arenas and two samples:
- `curr` / `arena_curr` — Current frame data
- `prev` / `arena_prev` — Previous frame for comparison

Each tick, we swap them:
```c
sample_t *temp = prev; prev = curr; curr = temp;
Arena *ta = arena_prev; arena_prev = arena_curr; arena_curr = ta;
arena_reset(arena_curr);  // Reset the "new current"
sample_capture(curr, arena_curr);
```

This lets us compare current vs previous without copying data.

### /proc Filesystem Parsing

All data comes from Linux's `/proc` pseudo-filesystem:

| Path | Data |
|------|------|
| `/proc/[pid]/stat` | Basic stats: state, CPU time, RSS |
| `/proc/[pid]/status` | Detailed: VmRSS, VmSwap, UID |
| `/proc/[pid]/smaps_rollup` | Memory breakdown (fast, kernel 4.14+) |
| `/proc/[pid]/smaps` | Detailed memory maps (slower fallback) |
| `/proc/[pid]/io` | I/O stats (needs root) |
| `/proc/meminfo` | System-wide memory stats |

**Files:** `src/proc/*.c`

### Process Lifecycle Tracking (PID Reuse)

Linux recycles PIDs. PID 1234 might be Firefox now, Chrome later.

We detect this using `start_time` from `/proc/[pid]/stat`:
```c
// In baseline hashmap
if (entry->pid == pid && entry->start_time == start_time) {
    // Same process
} else {
    // PID was reused — this is a new process
    entry->first_rss = current_rss;  // Reset baseline
}
```

**Files:** `src/ui/display.c` (baseline_hash)

### ViewState (Stable UI Selection)

**The Problem:** User presses ↓ to select row 5, then presses Enter. Meanwhile, the list re-sorts and row 5 is now a different process!

**The Solution:** Freeze the UI state during rendering:
```c
view_begin_frame(selected_idx, scroll, visible, group_mode);
// ... populate ViewState during render ...
view_end_frame();

// Later, when Enter is pressed:
pid_t pid = view_get_selected_pid();  // Returns the frozen PID
```

**Files:** `inc/ui/view.h`, `src/ui/view.c`

---

## Data Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                         MAIN LOOP                                │
│  ┌─────────┐    ┌──────────┐    ┌─────────┐    ┌─────────────┐  │
│  │  Input  │───►│  Capture │───►│  Diff   │───►│   Render    │  │
│  │ (60fps) │    │  (1Hz)   │    │         │    │   (60fps)   │  │
│  └─────────┘    └──────────┘    └─────────┘    └─────────────┘  │
└─────────────────────────────────────────────────────────────────┘
         │              │              │               │
         ▼              ▼              ▼               ▼
    ui_poll_input  sample_capture   Compare       ui_draw()
                   ┌──────────┐    curr vs prev   ui_draw_detail()
                   │ /proc/*  │
                   │ parsing  │
                   └──────────┘
```

**Timing:**
- Input polling: 60 FPS (smooth navigation)
- Data capture: 1 Hz (once per second)
- UI render: 60 FPS (responsive display)

---

## Module Overview

### `core/` — Data Capture & Timing

| File | Purpose |
|------|---------|
| `sample.c` | Orchestrates full system capture |
| `time.c` | Monotonic timing (avoids clock drift) |
| `engine.c` | Background data collection (unused currently) |
| `diff.c` | Sample comparison (unused currently) |

### `proc/` — Linux /proc Parsers

| File | Purpose |
|------|---------|
| `procfs.c` | Low-level file reading with O_CLOEXEC |
| `scan.c` | Walks /proc to find numeric PID directories |
| `pid_stat.c` | Parses `/proc/[pid]/stat` |
| `pid_status.c` | Parses `/proc/[pid]/status` (VmRSS, UID) |
| `smaps.c` | 3-level memory parsing with fallbacks |
| `meminfo.c` | System-wide `/proc/meminfo` |
| `pid_io.c` | I/O stats (requires root) |

### `ui/` — ncurses Interface

| File | Purpose |
|------|---------|
| `display.c` | Main rendering: list view, inspect view, colors |
| `view.c` | ViewState snapshot for stable process selection |
| `gauge.c` | Unicode block gauges `[████████░░░░]` |
| `sparkline.c` | Multi-row vertical bar graphs |
| `box.c` | Box-drawing utilities |

### `util/` — Infrastructure

| File | Purpose |
|------|---------|
| `arena.c` | Linear bump allocator |
| `log.c` | Logging with timestamps |
| `uid_cache.c` | UID → username caching |
| `proc_map.c` | PID hashmap for persistent entity tracking |

---

## Key Data Structures

### `process_snapshot_t` (inc/core/sample.h)
```c
typedef struct {
    pid_t pid;
    char comm[16];          // Process name
    char state;             // R/S/D/Z/T
    uint64_t rss_bytes;     // Resident memory
    uint64_t utime_ms;      // User CPU time
    uint64_t stime_ms;      // Kernel CPU time
    uint64_t start_time;    // For PID reuse detection
    // ... more fields
} process_snapshot_t;
```

### `sample_t` (inc/core/sample.h)
```c
typedef struct {
    uint64_t timestamp_ms;
    uint64_t system_total_ram;
    uint64_t system_free_ram;
    size_t process_count;
    process_snapshot_t *processes;  // Array in arena
} sample_t;
```

### `smaps_breakdown_t` (inc/proc/smaps.h)
```c
typedef struct {
    uint64_t total_rss_kb;
    uint64_t total_pss_kb;
    uint64_t heap_kb;       // Private memory
    uint64_t anon_kb;       // Anonymous pages
    uint64_t shared_kb;     // File-backed
    uint64_t swap_kb;       // Swapped out
} smaps_breakdown_t;
```

### `ViewState` (inc/ui/view.h)
```c
typedef struct {
    int selected_idx;
    int scroll_offset;
    size_t count;
    ViewRow rows[VIEW_MAX_ROWS];  // Frozen snapshot
} ViewState;
```

---

## Adding New Features

### Adding a New Metric

1. **Add field to `process_snapshot_t`** in `inc/core/sample.h`
2. **Parse it** in `src/proc/pid_stat.c` or `pid_status.c`
3. **Display it** in `src/ui/display.c` in `ui_draw()`
4. **Optionally add to ViewRow** if needed for sorting

### Adding a New View

1. Create new function in `display.c`: `void ui_draw_myview(...)`
2. Add input handling in `ui_poll_input()` for the key
3. Add state flag in `main.c` (like `inspect_mode`)
4. Call your function in the render section of main loop

### Adding a New Sort Mode

1. Add enum value in `display.c`: `SORT_MY_FIELD_DESC`
2. Add comparison function: `static int cmp_myfield(...)`
3. Add case in the sort switch
4. Add keybind in `ui_poll_input()`

---

## Code Style

- **Indentation:** 4 spaces (no tabs)
- **Braces:** K&R style
- **Naming:** `snake_case` for functions and variables
- **Types:** `_t` suffix for typedefs
- **Headers:** Include guards with `MEMSCOPE_` prefix
- **Comments:** `//` for single line, `/* */` for blocks
- **Max line length:** 100 chars (soft limit)

```c
// Good
void my_function(int arg) {
    if (condition) {
        do_something();
    }
}

// Bad
void my_function(int arg)
{
  if(condition){
    do_something();
  }
}
```
