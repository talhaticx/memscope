/**
 * Test: Ring Buffer History Tracking
 * 
 * Verifies that ProcessEntity history buffers correctly store
 * and retrieve data points with ring buffer wrap-around.
 */

#include "core/entity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST_PASS(name) printf("  [PASS] %s\n", name)
#define TEST_FAIL(name, msg) do { printf("  [FAIL] %s: %s\n", name, msg); exit(1); } while(0)

void test_basic_push() {
    ProcessEntity e = {0};
    entity_reset(&e);
    
    // Push 5 values
    e.cpu_pct = 10.0; e.rss_bytes = 1000; entity_push_history(&e);
    e.cpu_pct = 20.0; e.rss_bytes = 2000; entity_push_history(&e);
    e.cpu_pct = 30.0; e.rss_bytes = 3000; entity_push_history(&e);
    e.cpu_pct = 40.0; e.rss_bytes = 4000; entity_push_history(&e);
    e.cpu_pct = 50.0; e.rss_bytes = 5000; entity_push_history(&e);
    
    // Verify count
    if (e.history_count != 5) {
        TEST_FAIL("basic_push", "history_count should be 5");
    }
    
    // Verify values (age 0 = most recent)
    if (entity_get_cpu_history(&e, 0) != 50.0) {
        TEST_FAIL("basic_push", "most recent CPU should be 50.0");
    }
    if (entity_get_cpu_history(&e, 4) != 10.0) {
        TEST_FAIL("basic_push", "oldest CPU should be 10.0");
    }
    
    if (entity_get_rss_history(&e, 0) != 5000) {
        TEST_FAIL("basic_push", "most recent RSS should be 5000");
    }
    if (entity_get_rss_history(&e, 4) != 1000) {
        TEST_FAIL("basic_push", "oldest RSS should be 1000");
    }
    
    TEST_PASS("basic_push: 5 values stored and retrieved correctly");
}

void test_ring_buffer_overflow() {
    ProcessEntity e = {0};
    entity_reset(&e);
    
    // Push 65 values (overflow 60-element buffer)
    for (int i = 1; i <= 65; i++) {
        e.cpu_pct = (double)i;
        e.rss_bytes = (uint64_t)(i * 100);
        entity_push_history(&e);
    }
    
    // Count should cap at 60
    if (e.history_count != ENTITY_HISTORY_SIZE) {
        TEST_FAIL("ring_overflow", "history_count should cap at ENTITY_HISTORY_SIZE");
    }
    
    // Most recent should be 65
    if (entity_get_cpu_history(&e, 0) != 65.0) {
        TEST_FAIL("ring_overflow", "most recent should be 65.0");
    }
    
    // Oldest should be 6 (values 1-5 were pushed out)
    if (entity_get_cpu_history(&e, 59) != 6.0) {
        TEST_FAIL("ring_overflow", "oldest should be 6.0 (1-5 overwritten)");
    }
    
    TEST_PASS("ring_overflow: wrap-around works correctly");
}

void test_out_of_bounds() {
    ProcessEntity e = {0};
    entity_reset(&e);
    
    // Push 3 values
    e.cpu_pct = 1.0; entity_push_history(&e);
    e.cpu_pct = 2.0; entity_push_history(&e);
    e.cpu_pct = 3.0; entity_push_history(&e);
    
    // Out of bounds should return 0
    if (entity_get_cpu_history(&e, 3) != 0.0) {
        TEST_FAIL("out_of_bounds", "age >= count should return 0");
    }
    if (entity_get_cpu_history(&e, -1) != 0.0) {
        TEST_FAIL("out_of_bounds", "negative age should return 0");
    }
    
    TEST_PASS("out_of_bounds: invalid indices return 0");
}

void test_reset() {
    ProcessEntity e = {0};
    e.pid = 123;
    e.cpu_pct = 99.0;
    entity_push_history(&e);
    entity_push_history(&e);
    
    entity_reset(&e);
    
    if (e.history_count != 0 || e.history_idx != 0) {
        TEST_FAIL("reset", "reset should clear history");
    }
    if (!e.is_new || !e.is_alive || e.is_dead) {
        TEST_FAIL("reset", "reset should set proper state flags");
    }
    
    TEST_PASS("reset: clears history and sets flags");
}

int main(void) {
    printf("\n========================================\n");
    printf("  Test: History Ring Buffer\n");
    printf("========================================\n\n");
    
    test_basic_push();
    test_ring_buffer_overflow();
    test_out_of_bounds();
    test_reset();
    
    printf("\n========================================\n");
    printf("  All tests passed!\n");
    printf("========================================\n\n");
    
    return 0;
}
