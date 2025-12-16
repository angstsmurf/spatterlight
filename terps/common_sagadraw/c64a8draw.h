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

typedef void(*translate_color_fn)(uint8_t, uint8_t);
typedef int(*adjustments_fn)(int, int, int *);

int DrawC64A8ImageFromData(uint8_t *ptr, size_t datasize, int voodoo_or_count, adjustments_fn adjustments, translate_color_fn translate);

#endif /* c64a8draw_h */
