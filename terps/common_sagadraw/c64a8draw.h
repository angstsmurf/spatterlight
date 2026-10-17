//
//  c64a8draw.h
//
//  Used by ScottFree and Plus,
//  two interpreters for adventures in Scott Adams formats
//
//  Routines to draw Atari 8-bit and C64 RLE bitmap graphics
//  Based on Code by David Lodge 29/04/2005
//
//  Original code at https://github.com/tautology0/textadventuregraphics

#ifndef c64a8draw_h
#define c64a8draw_h

#include <stddef.h>
#include <stdint.h>

typedef int (*adjustments_fn)(int, int, int *);

// How the palette bytes of an image translate into colours
enum c64a8_palette {
    C64A8_ATARI8,          // Atari 8-bit
    C64A8_C64,             // C64: Spider-Man, Buckaroo Banzai, Fantastic Four
    C64A8_C64_CLAYMORGUE,  // C64: Sorcerer of Claymorgue Castle
};

int DrawC64A8ImageFromData(uint8_t *ptr, size_t datasize, int voodoo_or_count, adjustments_fn adjustments, enum c64a8_palette palette);
// Draws one bitmap byte as four double-width pixels at (x, *y), two bits
// per pixel with the high pair leftmost, then moves *y down one row.
// Returns x unchanged, so a column of bytes keeps the same left edge.
int DrawPatternAndAdvancePos(int x, int *y, uint8_t pattern);

#endif /* c64a8draw_h */
