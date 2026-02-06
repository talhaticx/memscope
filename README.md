# memscope

> **A Linux memory behavior forensic engine.**

`memscope` is a lightweight, zero-dependency C tool that captures, diffs, and replays process-level memory state over time. Unlike `top` or `htop` which show the *current* state, `memscope` focuses on **change**—answering "Which process caused that memory spike 5 minutes ago?"

![Language](https://img.shields.io/badge/language-C11-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)

## ⚡ Core Philosophy

* **Observability, not Monitoring:** Built to answer "what happened?" not just "what is happening?".
* **Zero Allocations in Hot Path:** Uses a custom linear Arena allocator. No `malloc`/`free` during the capture loop.
* **Pure `/proc`:** No `ptrace`, no kernel modules, no eBPF. Just raw, high-performance file parsing.
* **Machine Friendly:** Outputs structured diffs designed for piping into analysis tools.

## 🚀 Quick Start

### Prerequisites
* Linux (Kernel 4.x+)
* GCC or Clang
* Make

### Build & Run
```bash
# Clone the repository
git clone https://github.com/talhaticx/memscope.git
cd memscope

# Build the binary
make

# Run directly
make run