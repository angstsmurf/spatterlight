// Small helpers shared by the SAGA renderer harnesses in this directory. Header
// only (static inline), because most harnesses are a single translation unit
// that unity-includes the renderer under test.

#ifndef SAGA_TEST_UTIL_H
#define SAGA_TEST_UTIL_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Read a whole file into a malloc'd buffer; *size (if non-NULL) gets its
// length. NULL if the file cannot be opened or read. An empty file is NULL as
// well unless allow_empty is set, in which case it is a 1-byte buffer with
// *size 0.
static inline uint8_t *read_file_ex(const char *path, size_t *size, int allow_empty)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0 || (n == 0 && !allow_empty)) { fclose(f); return NULL; }
    uint8_t *buf = malloc((size_t)n ? (size_t)n : 1);
    if (buf && n && fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); buf = NULL; }
    fclose(f);
    if (buf && size) *size = (size_t)n;
    return buf;
}

// The common case: an empty file counts as unreadable.
static inline uint8_t *read_file(const char *path, size_t *size)
{
    return read_file_ex(path, size, 0);
}

// ZX Spectrum SCREEN$ (6912 bytes: 6144 of bitmap, whose lines are stored
// with the three bit fields of y shuffled, then 768 attribute bytes): the
// colour index (0..7, +8 when BRIGHT) of pixel (x, y), 0 <= x < 256, 0 <= y < 192.
static inline int zx_scr_colour(const uint8_t *scr, int x, int y)
{
    int addr = ((y & 0xc0) << 5) | ((y & 0x07) << 8) | ((y & 0x38) << 2) | (x >> 3);
    uint8_t attr = scr[6144 + (y >> 3) * 32 + (x >> 3)];
    int set = (scr[addr] >> (7 - (x & 7))) & 1;
    return (set ? (attr & 7) : ((attr >> 3) & 7)) + ((attr & 0x40) ? 8 : 0);
}

#endif
