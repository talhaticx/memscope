# memscope — Source Code Architecture

This document outlines the responsibility of every file in the project.
**Philosophy:** Separation of concerns. The `proc/` layer gathers data, `core/` analyzes it, and `util/` provides the memory infrastructure.

---

## 📂 Root Directory
The entry point and build configuration.

* **`Makefile`**
    * **Task:** The build recipe.
    * **Details:** Must compile all `*.c` files in `src/**/*.c`. Links them into the final `bin/memscope` executable. Must include flags `-Wall -Wextra -Werror -O2` to ensure code quality.
* **`main.c`**
    * **Task:** The application driver.
    * **Details:**
        1.  Initializes the Memory Arenas (Double buffering).
        2.  Parses command line arguments (interval, PID filtering).
        3.  Runs the Main Loop: `Capture -> Diff -> Print -> Sleep`.
        4.  Handles signals (Ctrl+C) for graceful shutdown.

---

## 📂 inc/ (Header Files)
The "Contracts". These define the data structures and function prototypes.

### `inc/core/` (Business Logic)
* **`sample.h`**
    * **Task:** Defines the Data Model. **(The most important file)**.
    * **Details:** Defines `struct process_snapshot_t` and `struct sample_t`. This is the layout of the data inside the Arena.
* **`diff.h`**
    * **Task:** Defines the Comparison Logic.
    * **Details:** Prototypes for `diff_samples(old, new)`. Defines structures for reporting changes (e.g., `struct process_diff_t`).
* **`time.h`**
    * **Task:** Time standardization.
    * **Details:** Prototypes for getting high-precision monotonic time (nanoseconds).

### `inc/proc/` (Data Source)
* **`procfs.h`**
    * **Task:** The Linux API abstraction.
    * **Details:** Defines the paths (`/proc`, `/proc/meminfo`). Prototypes for the parser functions.
* **`pid.h`**
    * **Task:** Process-specific helpers.
    * **Details:** Helpers for PID validation, checking if a PID is alive, etc.

### `inc/util/` (Infrastructure)
* **`arena.h`**
    * **Task:** The Memory Allocator Interface.
    * **Details:** Prototypes for `arena_create`, `arena_alloc`, `arena_reset`. This is the engine of the project.
* **`log.h`**
    * **Task:** Logging macros.
    * **Details:** Macros `LOG_INFO`, `LOG_ERR` that print to stderr with timestamps.
* **`hashmap.h`**
    * **Task:** Fast lookup interface (Optional).
    * **Details:** If implemented, provides `map_put` and `map_get` for O(1) PID lookups.

---

## 📂 src/core/ ( The Brain)
These files implement the logic that makes sense of the raw data.

* **`sample.c`**
    * **Task:** The Coordinator.
    * **Details:**
        * Implements `sample_capture(Arena *a)`.
        * Calls `scan.c` to find PIDs.
        * Calls `pid_stat.c` etc. to fill the structs.
        * Sorts the array of processes by PID (for efficient diffing).
* **`diff.c`**
    * **Task:** The Analyst.
    * **Details:**
        * Compares two sorted `sample_t` arrays.
        * Detects: New processes, Dead processes, Memory Growth, CPU spikes.
        * Returns a report or prints directly to stdout.
* **`time.c`**
    * **Task:** The Clock.
    * **Details:** Wrappers around `clock_gettime(CLOCK_MONOTONIC)` to ensure all timestamps are comparable.

---

## 📂 src/proc/ (The Collector)
These files do the dirty work of parsing text files from Linux.

* **`scan.c`**
    * **Task:** The Directory Walker.
    * **Details:** Opens `/proc`, calls `readdir`. Filters out non-numeric entries. Returns a list of PIDs to process.
* **`stat.c`** (or `meminfo.c`)
    * **Task:** System-wide metrics.
    * **Details:** Parses `/proc/meminfo` (Total RAM, Free RAM) and `/proc/stat` (System CPU).
* **`pid_stat.c`**
    * **Task:** Basic Process Metrics.
    * **Details:** Parses `/proc/[pid]/stat`.
    * **Key Fields:** State, Parent PID, User CPU, System CPU, Priority, Threads.
* **`pid_status.c`**
    * **Task:** Detailed Memory Metrics.
    * **Details:** Parses `/proc/[pid]/status`.
    * **Key Fields:** `VmRSS`, `RssAnon` (Heap/Stack), `RssFile` (Libraries), `VmSwap`. *Crucial for memory forensics.*
* **`pid_io.c`**
    * **Task:** Disk IO Metrics.
    * **Details:** Parses `/proc/[pid]/io`.
    * **Note:** Requires root usually. Must fail gracefully if permission denied.

---

## 📂 src/util/ (The Foundation)
Generic C code that could exist in any project.

* **`arena.c`**
    * **Task:** The Linear Allocator Implementation.
    * **Details:** Manages the big `malloc` block. Advances a pointer on `alloc`. Resets pointer on `reset`. **Zero overhead.**
* **`log.c`**
    * **Task:** Logger implementation.
    * **Details:** Formats strings with `vfprintf` and adds colors/timestamps.
* **`hashmap.c`**
    * **Task:** Hashmap implementation.
    * **Details:** Integer-keyed hashmap (PID -> Pointer).

---

## 📂 tests/ (Quality Assurance)
Unit tests to ensure the parser doesn't segfault on weird inputs.

* **`test_pid_parse.c`**
    * **Task:** Validate the parser logic.
    * **Details:** Feeds a dummy string (like a fake `/proc/pid/stat` line) into the parser functions and asserts that the struct fields match the string.
* **`test_diff.c`**
    * **Task:** Validate the logic engine.
    * **Details:** Manually constructs two `sample_t` structs (one "before", one "after") and asserts that `diff_samples` correctly identifies the change.

---

## 📂 tools/ (Utilities)
Standalone mini-programs for debugging or specific features.

* **`proc_dump.c`**
    * **Task:** Debugging tool.
    * **Details:** A small `main()` that just dumps the parsed content of a specific PID to stdout. Useful to verify parsing without running the full engine.
* **`replay.c`**
    * **Task:** Post-mortem analysis (Future Feature).
    * **Details:** Reads a binary file (saved history) and "plays it back" through the diff engine, simulating a live run.