/**
 * @file arena.c
 * @brief Linear (bump) allocator for zero-malloc hot path.
 *
 * The Arena allocator is the performance foundation of memscope.
 * Instead of calling malloc/free for each process snapshot, we:
 *
 *   1. Pre-allocate a large block (e.g., 4MB) at startup
 *   2. Bump a pointer for each allocation — O(1), no fragmentation
 *   3. Reset pointer to start when done — O(1), no individual frees
 *
 * This eliminates all memory allocation overhead in the capture loop,
 * which runs 60+ times per second.
 *
 * Memory Layout:
 *   [Arena struct] --> [buf: uint8_t* ----------------...]
 *                       ^                               ^
 *                       |                               |
 *                     offset=0                        size
 *
 * @see inc/util/arena.h for API
 */

#include "util/arena.h"
#include "util/log.h"
#include <stdlib.h>
#include <string.h> // For memset (optional, but good for debug builds)

// Align all allocations to 8 bytes (64-bit word size)
// This prevents bus errors on some architectures and optimizes CPU access
#define ARENA_ALIGNMENT 8
#define ALIGN_UP(n) (((n) + (ARENA_ALIGNMENT - 1)) & ~(ARENA_ALIGNMENT - 1))

Arena *arena_create(size_t size) {
    // 1. Allocate the metadata struct
    Arena *a = malloc(sizeof(Arena));
    if (!a) {
        LOG_ERR("Failed to allocate Arena struct");
        return NULL;
    }

    // 2. Allocate the actual data buffer
    // usage of calloc here is optional, but malloc is faster
    a->buf = malloc(size);
    if (!a->buf) {
        LOG_ERR("Failed to allocate Arena buffer of size %zu", size);
        free(a); // CRITICAL FIX: Don't leak the struct if buffer fails!
        return NULL;
    }

    // 3. Initialize state
    a->size = size;
    a->offset = 0;

    LOG_DEBUG("Arena created: %zu bytes", size);
    return a;
}

void *arena_alloc(Arena *a, size_t size) {
    if (!a) return NULL;

    // 1. Calculate aligned size
    size_t aligned_size = ALIGN_UP(size);

    // 2. Check for overflow
    if (a->offset + aligned_size > a->size) {
        LOG_ERR("Arena OOM! Requested %zu, Available %zu", 
                aligned_size, a->size - a->offset);
        return NULL;
    }

    // 3. Bump the pointer
    // a->buf is uint8_t*, so pointer arithmetic is safe (byte-wise)
    void *ptr = a->buf + a->offset;
    a->offset += aligned_size;

    return ptr;
}

void arena_reset(Arena *a) {
    if (a) {
        a->offset = 0;
        // Optimization: We do NOT zero out the memory. 
        // We just move the pointer back. The next write overwrites old data.
    }
}

void arena_destroy(Arena *a) {
    if (a) {
        if (a->buf) free(a->buf);
        free(a);
        LOG_DEBUG("Arena destroyed");
    }
}