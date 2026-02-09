# memscope — Source Code Architecture

This document outlines the responsibility of every file in the project.

**Philosophy:** Separation of concerns. The `proc/` layer gathers data, `core/` captures it, `ui/` displays it, and `util/` provides infrastructure.

---

## 📂 Root Directory

| File | Purpose |
|------|---------|
| `Makefile` | Build recipe. Compiles all `src/**/*.c` into `bin/memscope` |
| `README.md` | Project overview, features, usage |
| `CONTRIBUTING.md` | Contribution guidelines |

---

## 📂 src/ (Source Files)

### `main.c` — Application Driver
The entry point implementing the main loop:
1. Initializes memory arenas (double-buffering)
2. Handles user input (60 FPS polling)
3. Captures system data (1 Hz)
4. Renders UI (60 FPS)
5. Handles graceful shutdown (Ctrl+C)

---

### 📂 src/core/ — The Brain
Business logic for data capture and timing.

| File | Purpose |
|------|---------|
| `sample.c` | **Coordinator** — Calls scanners, fills `sample_t`, sorts by PID |
| `time.c` | **Clock** — Monotonic timing via `clock_gettime(CLOCK_MONOTONIC)` |
| `engine.c` | Background collection engine (thread-based, currently unused) |
| `diff.c` | Sample comparison (detects new/dead/changed processes) |

---

### 📂 src/proc/ — The Collector
Parsers for Linux `/proc` filesystem.

| File | Purpose |
|------|---------|
| `procfs.c` | Low-level file reading with `O_CLOEXEC` |
| `scan.c` | Directory walker — reads `/proc/*/` for numeric PIDs |
| `pid_stat.c` | Parses `/proc/[pid]/stat` — state, CPU times, RSS |
| `pid_status.c` | Parses `/proc/[pid]/status` — VmRSS, UID, memory details |
| `smaps.c` | **3-level memory parser** with fallbacks (smaps_rollup → smaps → status) |
| `meminfo.c` | Parses `/proc/meminfo` — system RAM/swap totals |
| `pid_io.c` | Parses `/proc/[pid]/io` — I/O bytes (requires root) |

---

### 📂 src/ui/ — The Display
ncurses-based terminal interface.

| File | Purpose |
|------|---------|
| `display.c` | **Main renderer** — list view, inspect view, input handling, colors |
| `view.c` | **ViewState** — Freezes UI state during render for stable selection |
| `gauge.c` | Unicode block gauges `[████████░░░░]` |
| `sparkline.c` | Multi-row vertical bar graphs for memory trends |
| `box.c` | Box-drawing utilities with Unicode characters |

#### Key Concepts in UI

**Auto-Baseline Tracking:**
```c
// display.c maintains a hashmap of first-seen RSS per PID
static baseline_entry_t baseline_hash[BASELINE_HASH_SIZE];

// Delta shows change since first observation
double delta = current_rss - get_first_rss(pid, current_rss, start_time);
```

**ViewState Pattern:**
```c
// Freeze visible rows at render time
view_begin_frame(selected_idx, scroll, visible, group_mode);
// ... populate rows ...
view_end_frame();

// Later, when Enter is pressed:
pid_t pid = view_get_selected_pid();  // Returns frozen PID
```

---

### 📂 src/util/ — The Foundation
Generic infrastructure code.

| File | Purpose |
|------|---------|
| `arena.c` | **Linear allocator** — Zero-malloc in hot path, O(1) alloc/reset |
| `log.c` | Timestamped logging to stderr |
| `uid_cache.c` | UID → username caching (avoids repeated `getpwuid()` calls) |
| `proc_map.c` | PID hashmap for persistent entity tracking |

---

## 📂 inc/ (Header Files)

### inc/core/
| Header | Defines |
|--------|---------|
| `sample.h` | `process_snapshot_t`, `sample_t`, `sample_capture()` |
| `time.h` | `time_now_ms()`, `time_sleep_ms()` |
| `diff.h` | `diff_samples()` (comparison logic) |
| `engine.h` | Background collector interface |
| `entity.h` | Persistent process entity tracking |

### inc/proc/
| Header | Defines |
|--------|---------|
| `procfs.h` | `procfs_read_file()`, path constants |
| `pid.h` | PID validation helpers |
| `smaps.h` | `smaps_breakdown_t`, `pid_get_memory_detail()` |

### inc/ui/
| Header | Defines |
|--------|---------|
| `display.h` | `ui_init()`, `ui_draw()`, `ui_poll_input()` |
| `view.h` | `ViewState`, `ViewRow`, `view_get_selected_pid()` |
| `gauge.h` | `gauge_draw()`, `gauge_draw_colored()` |
| `sparkline.h` | `sparkline_draw()` |
| `box.h` | Box drawing utilities |

### inc/util/
| Header | Defines |
|--------|---------|
| `arena.h` | `Arena`, `arena_create()`, `arena_alloc()`, `arena_reset()` |
| `log.h` | `LOG_INFO`, `LOG_ERR` macros |
| `hashmap.h` | Generic hashmap interface |
| `uid_cache.h` | `uid_to_name()` |
| `proc_map.h` | `ProcMap` for PID → entity mapping |

---

## 📂 tests/ — Quality Assurance

| File | Purpose |
|------|---------|
| `leak_test.c` | Memory leak simulator — allocates N MB, grows M MB/sec |
| `test_history.c` | Tests for history tracking |

---

## 📂 tools/ — Utilities

| File | Purpose |
|------|---------|
| `proc_dump.c` | Dumps parsed content of a specific PID |
| `replay.c` | Post-mortem analysis (future feature) |

---

## Data Flow Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                           MAIN LOOP                                  │
│                                                                       │
│  ┌──────────┐      ┌─────────────┐      ┌─────────┐                  │
│  │  INPUT   │─────►│   CAPTURE   │─────►│ RENDER  │                  │
│  │  (60fps) │      │   (1 Hz)    │      │ (60fps) │                  │
│  └──────────┘      └─────────────┘      └─────────┘                  │
│       │                  │                   │                        │
│       ▼                  ▼                   ▼                        │
│  ui_poll_input()   sample_capture()     ui_draw()                    │
│                    ┌─────────────┐      ui_draw_detail()             │
│                    │  /proc/*    │                                    │
│                    │  parsing    │                                    │
│                    └─────────────┘                                    │
└─────────────────────────────────────────────────────────────────────┘

Memory: Arena Double-Buffering
┌──────────┐    ┌──────────┐    ┌──────────────┐
│ arena_a  │◄──►│ arena_b  │    │arena_baseline│
│ (curr)   │swap│ (prev)   │    │  (frozen)    │
└──────────┘    └──────────┘    └──────────────┘
```