/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  us_companion.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Finds the other disk or file of a US game that comes in two parts.
 */

#ifndef us_companion_h
#define us_companion_h

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *filename;
    uint32_t stringlength;
} pairrec;

char *LookInDatabase(const pairrec list[][2], const char *game_path, size_t pathlen);

/* Swaps the game file with its companion file, when the database turned out
   to be on the companion disk */
void SwapWithCompanionFile(uint8_t **sf, size_t *extent, uint8_t **companion, size_t *companionsize);

#endif /* us_companion_h */
