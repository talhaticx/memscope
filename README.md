# memscope

> **A Linux memory forensics engine with real-time visualization.**

`memscope` is a lightweight, terminal-based C tool for monitoring process memory behavior. Unlike `top` or `htop` which show current state, memscope focuses on **change**—answering "Which process caused that memory spike?"

![Language](https://img.shields.io/badge/language-C11-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)
![Dependencies](https://img.shields.io/badge/dependencies-ncurses-orange.svg)

## ✨ Features

- **Real-time Memory Tracking** — Monitor RSS, PSS, swap usage per process
- **Auto-Baseline Detection** — Delta column shows memory growth since process started
- **Inspect Mode** — Drill into any process with detailed memory breakdown
- **Visual Gauges** — Unicode block gauges and multi-row sparkline graphs
- **Process Grouping** — Collapse Chrome/Firefox tabs into single entries
- **Leak Detection Tags** — Automatic LEAK/SPIKE/ZOMBIE tagging
- **Zero Dependencies** — Pure `/proc` parsing, no eBPF or kernel modules

## 🚀 Quick Start

### Prerequisites
```bash
# Ubuntu/Debian
sudo apt install build-essential libncursesw5-dev

# Arch
sudo pacman -S ncurses

# Fedora
sudo dnf install ncurses-devel
```

### Build & Run
```bash
git clone https://github.com/talhaticx/memscope.git
cd memscope
make
./bin/memscope
```

## ⌨️ Keybindings

| Key | Action |
|-----|--------|
| `↑`/`↓` or `j`/`k` | Navigate process list |
| `Enter` | Inspect selected process |
| `ESC` | Exit inspect mode / Quit |
| `Space` | Reset baseline (delta = 0) |
| `g` | Toggle group mode |
| `m` | Sort by memory (RSS) |
| `c` | Sort by CPU usage |
| `d` | Sort by delta (memory change) |
| `p` | Sort by PID |
| `q` | Quit |

## 📊 Views

### Main Dashboard
```
 MEMSCOPE [SORT: MEM]                    RAM [████████░░░░] 12.4/32G
──────────────────────────────────────────────────────────────────────
 PID    NAME              RSS      CPU%      DELTA   TAGS
──────────────────────────────────────────────────────────────────────
 3587   chrome         483.7 MB    2.1%    +12.30 MB LEAK
 38524  firefox        465.0 MB    5.0%     +0.01 MB
 2918   Hyprland       205.5 MB   13.0%     +0.00 MB
```

### Inspect Mode
```
 INSPECT: chrome (PID 3587)
────────────────────────────────────────────────────────────────
 MEMORY BREAKDOWN                    MEMORY TREND (Last 64 sec)
 Private  [████████░░░░]  256.2 MB    1300│      ▄▆█
 Anon     [██████░░░░░░]  180.1 MB    1200│    ▂▆███
 File     [██░░░░░░░░░░]   47.4 MB    1100│  ▂▆█████
 Swap     [░░░░░░░░░░░░]    0.0 MB     900│█████████
 ────────────────────────
 Total RSS:    483.7 MB   +12.3       Range: 450.0 - 483.7 MB
 Total PSS:    412.1 MB               Current: 483.68 MB
```

## 🏗️ Architecture

```
memscope/
├── src/
│   ├── main.c           # Application entry, main loop
│   ├── core/            # Data capture & timing
│   │   ├── sample.c     # System-wide snapshot capture
│   │   └── time.c       # Monotonic timing
│   ├── proc/            # Linux /proc parsers
│   │   ├── smaps.c      # Memory breakdown (3-level fallback)
│   │   ├── pid_stat.c   # Process stats parsing
│   │   └── scan.c       # /proc directory walking
│   ├── ui/              # ncurses interface
│   │   ├── display.c    # Main rendering logic
│   │   ├── gauge.c      # Unicode gauges
│   │   ├── sparkline.c  # Multi-row graphs
│   │   └── view.c       # ViewState for stable selection
│   └── util/            # Infrastructure
│       ├── arena.c      # Zero-malloc linear allocator
│       └── uid_cache.c  # UID→username caching
├── inc/                 # Header files
├── docs/                # Architecture & API docs
└── tests/               # Unit tests & leak simulator
```

## 🔧 Core Concepts

### Arena Allocator
Zero allocations in the hot path. All per-frame data lives in pre-allocated arenas that get reset each tick.

### Auto-Baseline
Each process automatically tracks its first-seen RSS. Delta column shows growth since first observation—no need to manually set baselines for new processes.

### 3-Level Memory Parsing
1. `/proc/[pid]/smaps_rollup` (fastest, requires kernel 4.14+)
2. `/proc/[pid]/smaps` (detailed, slower)
3. `/proc/[pid]/status` (fallback, VmRSS only)

## 📖 Documentation

- [Architecture Guide](docs/architecture.md) — System design & data flow
- [Developer Guide](docs/DEVELOPER.md) — How to contribute

## 🧪 Testing

```bash
# Build and run leak test simulator
make
./tests/leak_test 500 10   # Start at 500MB, grow 10MB/sec

# In another terminal
./bin/memscope            # Watch delta column grow
```

## 📄 License

MIT License — See [LICENSE](LICENSE) for details.

## 🤝 Contributing

Contributions welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting PRs.

---