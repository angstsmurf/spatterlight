/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  3 of the License, or (at your option) any later version.
 */

/*
 *  apple2_vector_draw.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Created by Petter Sjölund on 2025-12-29.
 */

#ifndef apple2_vector_draw_h
#define apple2_vector_draw_h

#include <stdio.h>
#include "sagagraphics.h"

int DrawApple2VectorImage(USImage *img);
void DrawSomeApple2VectorBytes(int from_start);
int DrawingApple2Vector(void);
void SetBrushBitmaps(const uint8_t *data);
void SetPatternData(const uint8_t *data);
void SetColorPatternSubindices(const uint8_t *data);

#endif /* apple2_vector_draw_h */
