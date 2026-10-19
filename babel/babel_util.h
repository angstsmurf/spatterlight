/* babel_util.h  helpers shared by the format modules written for Spatterlight
 *
 * The upstream modules keep their own copies so that they stay close to the
 * Treaty of Babel distribution; new modules should use these instead.
 *
 * This file depends on treaty.h (for int32).
 */

#ifndef BABEL_UTIL_H
#define BABEL_UTIL_H

#include <ctype.h>
#include <stdint.h>
#include <string.h>

/* The 16-bit additive checksum the length + checksum registries use */
static inline uint16_t babel_sum16(const unsigned char *p, int32 n)
{
    uint16_t c = 0;
    for (int32 i = 0; i < n; i++)
        c += p[i];
    return c;
}

/* Returns the offset of the first copy of needle in hay, or -1 */
static inline int32 babel_memfind(const void *hay, int32 n, const void *needle, int32 m)
{
    const unsigned char *h = hay;
    if (m <= 0 || m > n)
        return -1;
    for (int32 i = 0; i <= n - m; i++)
        if (h[i] == *(const unsigned char *)needle && memcmp(h + i, needle, (size_t)m) == 0)
            return i;
    return -1;
}

/* Copies a registry IFID (a 32-digit MD5 or a 36-character GUID) to out,
   in capitals, which is how the library stores IFIDs. The caller must have
   asserted room for 37 bytes. */
static inline void babel_copy_ifid(char *out, const char *ifid)
{
    while (*ifid)
        *out++ = (char)toupper((unsigned char)*ifid++);
    *out = 0;
}

#endif
