/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  vector_common.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Created by Petter Sjölund on 2026-02-01.
 */

#ifndef vector_common_h
#define vector_common_h

#include <stddef.h>
#include <stdint.h>

void DrawSomeVectorPixels(int from_start);
int DrawingVector(void);
int TimerDelay(void);

typedef enum {
    NO_VECTOR_IMAGE,
    DRAWING_VECTOR_IMAGE,
    SHOWING_VECTOR_IMAGE
} VectorStateType;

extern VectorStateType VectorState;
extern int vector_image_shown;

#endif /* vector_common_h */
