#ifndef MEMSCOPE_UID_CACHE_H
#define MEMSCOPE_UID_CACHE_H

#include <sys/types.h>

/**
 * UID Cache Module
 * 
 * Caches UID -> username mappings to avoid repeated getpwuid() calls.
 * getpwuid() is expensive and may block on NIS/LDAP lookups.
 */

#define USERNAME_MAX 32

/**
 * Get username for a UID with caching.
 * 
 * @param uid User ID
 * @param out Output buffer (at least USERNAME_MAX bytes)
 * @param out_size Size of output buffer
 * @return 0 on success, -1 on failure (out will contain numeric UID)
 */
int uid_get_name(uid_t uid, char *out, size_t out_size);

/**
 * Clear the UID cache.
 * Call this periodically if long-running to pick up user changes.
 */
void uid_cache_clear(void);

#endif // MEMSCOPE_UID_CACHE_H
