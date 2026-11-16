/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  game_specific.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Created by Petter Sjölund on 2022-01-27.
 */

#include "aiukpalette.h"
#include "game_specific.h"
#include "scott.h"

/* DefinePalette() gives every palette but these three the C64 colours,
   in which white is 1 and blue is 6. The others are in Spectrum order:
   bright white is 15 and bright blue is 9. */
static int HasC64Colours(void)
{
    return palchosen != ZX && palchosen != ZXOPT && palchosen != VGA;
}

int WhiteColour(void)
{
    return HasC64Colours() ? 1 : 15;
}

int BlueColour(void)
{
    return HasC64Colours() ? 6 : 9;
}
