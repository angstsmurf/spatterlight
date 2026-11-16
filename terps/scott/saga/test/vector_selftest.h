/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  vector_selftest.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Helpers for the tests of the Apple II and Atari 8-bit vector renderers.
 */

#ifndef vector_selftest_h
#define vector_selftest_h

#include <stddef.h>
#include <stdint.h>

/* Render the test image <name> and compare it with the reference in
   supportpath. Return 1 on a match. */
int TestApple2ImageWithName(const char *name, const char *supportpath);
int TestAtari8ImageWithName(const char *name, const char *supportpath);
/* Draws the room so_hexagonal with the objects so_rod and so_tint on top and
   compares the result with the tint references. Returns 1 on a match. */
int TestAtari8TintOverlay(const char *supportpath);

uint8_t *ReadTestDataFromFile(const char *filename, const char *supportpath, size_t *size);

struct USImage;
/* Reads the test image <prefix><name>.dat from supportpath. Returns NULL on
   failure. Free the result with FreeTestImage(). */
struct USImage *LoadTestImage(const char *prefix, const char *name, const char *supportpath);
void FreeTestImage(struct USImage *image);
/* Compares actual with the contents of the test file <prefix><name><suffix>
   in supportpath. Returns 1 if they match. */
int CompareWithTestFile(const char *prefix, const char *name, const char *suffix, const char *supportpath, const uint8_t *actual, size_t actuallen);

#endif /* vector_selftest_h */
