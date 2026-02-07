// tests/test_arena.c
#include "util/arena.h"
#include "util/log.h"
#include <assert.h>

int main(void) {
    LOG_INFO("Testing Arena...");

    // 1. Create 1KB arena
    Arena *a = arena_create(1024);
    assert(a != NULL);

    // 2. Alloc generic int
    int *nums = arena_alloc(a, sizeof(int) * 10);
    assert(nums != NULL);
    nums[0] = 99;

    // 3. Alloc struct (check alignment)
    // Previous alloc was 40 bytes. Aligned to 8 = 40.
    // Offset should be 40.
    char *str = arena_alloc(a, 10);
    assert(str != NULL);
    
    // 4. Reset
    arena_reset(a);
    // Offset is now 0. This overwrites 'nums'
    int *nums2 = arena_alloc(a, sizeof(int));
    assert(nums2 == (int*)a->buf); // Should point to start again

    arena_destroy(a);
    LOG_INFO("Arena Test Passed.");
    return 0;
}