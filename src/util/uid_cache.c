#define _POSIX_C_SOURCE 200809L
#include "util/uid_cache.h"
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Simple cache: linear search is fine for typical # of users (< 100)
#define CACHE_MAX 128

typedef struct {
    uid_t uid;
    char name[USERNAME_MAX];
} UidCacheEntry;

static UidCacheEntry cache[CACHE_MAX];
static size_t cache_count = 0;

int uid_get_name(uid_t uid, char *out, size_t out_size) {
    if (!out || out_size == 0) return -1;
    
    // Check cache first
    for (size_t i = 0; i < cache_count; i++) {
        if (cache[i].uid == uid) {
            strncpy(out, cache[i].name, out_size - 1);
            out[out_size - 1] = '\0';
            return 0;
        }
    }
    
    // Not in cache, lookup
    struct passwd *pw = getpwuid(uid);
    
    if (pw && pw->pw_name) {
        // Add to cache
        if (cache_count < CACHE_MAX) {
            cache[cache_count].uid = uid;
            strncpy(cache[cache_count].name, pw->pw_name, USERNAME_MAX - 1);
            cache[cache_count].name[USERNAME_MAX - 1] = '\0';
            cache_count++;
        }
        
        strncpy(out, pw->pw_name, out_size - 1);
        out[out_size - 1] = '\0';
        return 0;
    }
    
    // Fallback: use numeric UID
    snprintf(out, out_size, "%u", (unsigned)uid);
    
    // Cache the numeric fallback too
    if (cache_count < CACHE_MAX) {
        cache[cache_count].uid = uid;
        snprintf(cache[cache_count].name, USERNAME_MAX, "%u", (unsigned)uid);
        cache_count++;
    }
    
    return -1;
}

void uid_cache_clear(void) {
    cache_count = 0;
}
