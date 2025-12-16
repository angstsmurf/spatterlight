//
//  pcdraw.h
//
//  Used by ScottFree and Plus,
//  two interpreters for adventures in Scott Adams formats
//
//  Routines to draw IBM PC RLE bitmap graphics.
//  Based on Code by David Lodge.
//
//  Original code:
// https://github.com/tautology0/textadventuregraphics/blob/master/AdventureInternational/pcdraw.c
//

#ifndef pcdraw_h
#define pcdraw_h

int DrawDOSImageFromData(uint8_t *ptr);

#endif /* pcdraw_h */
