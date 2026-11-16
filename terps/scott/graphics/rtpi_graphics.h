/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  rtpi_graphics.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Created by Petter Sjölund on 2026-02-12.
 */

#ifndef rtpi_graphics_h
#define rtpi_graphics_h

#include <stddef.h>
#include <stdint.h>

#include "sagagraphics.h"
#include "scott_defines.h"

int DrawRTPIImage(USImage *image);
int DrawFuzzyRTPIImage(USImage *image);
void DrawRTPIFromMem(void);
void ClearVDP(void);
void SetupRTPIColors(void);

/* The 52-entry tile font, which the loader reads from the CPU ROM (c.bin) */
#define NUM_RTPI_TILES 52
extern uint8_t rtpi_tile_font[NUM_RTPI_TILES][8];

#endif /* rtpi_graphics_h */
