# Contributing to Memscope

Thank you for your interest in contributing! This guide will help you get started.

## Quick Start

```bash
# Fork and clone
git clone https://github.com/YOUR_USERNAME/memscope.git
cd memscope

# Build
make

# Run tests
./tests/leak_test 100 5 &
./bin/memscope

# Make changes, then submit PR
```

## Code Style

### Formatting
- **Indentation:** 4 spaces (no tabs)
- **Braces:** K&R style (opening brace on same line)
- **Line length:** 100 characters (soft limit)
- **Naming:** `snake_case` for functions/variables, `UPPER_CASE` for macros

### Example
```c
// Good
void process_data(const sample_t *sample) {
    if (sample == NULL) {
        return;
    }
    
    for (size_t i = 0; i < sample->process_count; i++) {
        do_something(&sample->processes[i]);
    }
}
```

### Headers
- Include guards: `#ifndef MEMSCOPE_FILENAME_H`
- Order: project headers first, then system headers
- Document all public functions with `/** */` Doxygen comments

## Commit Messages

```
type: short description

Longer explanation if needed.

Fixes #123
```

**Types:** `feat`, `fix`, `docs`, `refactor`, `test`, `chore`

**Examples:**
- `feat: add swap usage warning in inspect mode`
- `fix: prevent gauge overflow for large values`
- `docs: update README with keybindings`

## Pull Request Process

1. **Fork** the repository
2. **Create a branch**: `git checkout -b feat/my-feature`
3. **Make changes** and test
4. **Commit** with clear messages
5. **Push** and open a PR

### PR Checklist
- [ ] Code compiles without warnings (`make clean && make`)
- [ ] Tested on my local Linux machine
- [ ] Updated relevant documentation
- [ ] Added comments for complex logic

## Project Structure

```
src/
├── main.c           # Entry point, main loop
├── core/            # Data capture & timing
├── proc/            # /proc filesystem parsers
├── ui/              # ncurses rendering
└── util/            # Arena allocator, logging
```

See [docs/DEVELOPER.md](docs/DEVELOPER.md) for detailed architecture.

## Adding Features

### New Metric
1. Add field to `process_snapshot_t` in `inc/core/sample.h`
2. Parse it in `src/proc/pid_stat.c` or similar
3. Display in `src/ui/display.c`

### New View
1. Create `ui_draw_myview()` in `display.c`
2. Add keybind in `ui_poll_input()`
3. Add mode toggle in `main.c`

## Testing

```bash
# Memory leak simulator
./tests/leak_test 500 10   # Start 500MB, grow 10MB/sec

# Watch for delta growth in memscope
./bin/memscope
```

## Getting Help

- Open an issue for bugs or feature requests
- Check existing issues before creating new ones

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
