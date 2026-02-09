/**
 * Memory Leak Simulator for Memscope Testing
 * 
 * Usage: ./leak_test [initial_mb] [growth_mb_per_sec]
 * Default: Starts at 500MB, grows 10MB/second
 * 
 * Press Ctrl+C to stop
 */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static volatile int running = 1;

void sigint_handler(int sig) {
    (void)sig;
    running = 0;
}

// Touch every page to force RSS allocation (not just VSS)
void touch_memory(char *ptr, size_t size) {
    size_t page_size = 4096;
    for (size_t i = 0; i < size; i += page_size) {
        ptr[i] = (char)(i & 0xFF);
    }
}

int main(int argc, char **argv) {
    signal(SIGINT, sigint_handler);
    
    // Parse args
    size_t initial_mb = 500;    // Start at 500 MB
    size_t growth_mb = 10;      // Grow 10 MB per second
    
    if (argc > 1) initial_mb = (size_t)atoi(argv[1]);
    if (argc > 2) growth_mb = (size_t)atoi(argv[2]);
    
    printf("=== MEMORY LEAK SIMULATOR ===\n");
    printf("Initial: %zu MB\n", initial_mb);
    printf("Growth:  %zu MB/second\n", growth_mb);
    printf("Press Ctrl+C to stop\n\n");
    
    // Initial allocation
    size_t current_size = initial_mb * 1024 * 1024;
    char *memory = malloc(current_size);
    if (!memory) {
        fprintf(stderr, "Failed to allocate initial %zu MB\n", initial_mb);
        return 1;
    }
    
    // Touch to commit to RSS
    printf("Allocating initial %zu MB...\n", initial_mb);
    touch_memory(memory, current_size);
    printf("Done. Starting growth...\n\n");
    
    int seconds = 0;
    while (running) {
        sleep(1);
        seconds++;
        
        // Grow memory
        size_t new_size = current_size + (growth_mb * 1024 * 1024);
        char *new_mem = realloc(memory, new_size);
        
        if (!new_mem) {
            fprintf(stderr, "Realloc failed at %zu MB, stopping growth\n", new_size / (1024*1024));
            break;
        }
        
        memory = new_mem;
        
        // Touch the new pages to commit to RSS
        touch_memory(memory + current_size, growth_mb * 1024 * 1024);
        current_size = new_size;
        
        printf("[%4ds] RSS: %7.1f MB (+%zu MB)\n", 
               seconds, 
               (double)current_size / (1024.0 * 1024.0),
               growth_mb);
        fflush(stdout);
    }
    
    printf("\nCleaning up %zu MB...\n", current_size / (1024*1024));
    free(memory);
    printf("Done.\n");
    
    return 0;
}
