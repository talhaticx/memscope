#ifndef MEMSCOPE_ARENA_H
#define MEMSCOPE_ARENA_H

#include <stddef.h> // for size_t
#include <stdint.h> // for uint8_t

typedef struct {
    uint8_t *buf;   // The massive block of memory
    size_t size;    // Total capacity (e.g., 4MB)
    size_t offset;  // Current usage pointer (starts at 0)
} Arena;

/**
 * Allocates the backing memory block.
 * @param size Total bytes to allocate (e.g., 1024 * 1024 * 4 for 4MB).
 * @return A new Arena pointer, or NULL if system malloc fails.
 */
Arena *arena_create(size_t size);

/**
 * The hot-path allocator.
 * @param a The arena to allocate from.
 * @param size Bytes needed.
 * @return Pointer to the start of the allocated block, or NULL if out of memory.
 */
void *arena_alloc(Arena *a, size_t size);

/**
 * Instantly "frees" all memory in the arena.
 * @param a The arena to reset.
 */
void arena_reset(Arena *a);

/**
 * Teardown function (only called at program exit).
 * Frees the backing buffer and the Arena struct itself.
 */
void arena_destroy(Arena *a);

#endif // MEMSCOPE_ARENA_H