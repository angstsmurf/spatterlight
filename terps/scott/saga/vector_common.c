/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  vector_common.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Created by Petter Sjölund on 2026-02-01.
 */

#include <stdlib.h>
#include <string.h>

#include "apple2_vector_draw.h"
#include "atari_8bit_vector_draw.h"
#include "line_drawing.h"
#include "scott.h"
#include "vector_common.h"
#include "vector_oplist.h"

/* Timer intervals (ms) for slow drawing. Two US pictures are slower. */
#define US_VECTOR_DELAY 10
#define SLOW_VECTOR_DELAY 30
#define HOWARTH_VECTOR_DELAY 20

VectorStateType VectorState = NO_VECTOR_IMAGE;

void DrawSomeVectorPixels(int from_start) {
    if (CurrentSys == SYS_APPLE2) {
        DrawSomeApple2VectorBytes(from_start);
    } else if (CurrentSys == SYS_ATARI8) {
        DrawSomeAtari8VectorBytes(from_start);
    } else {
        DrawSomeHowarthVectorPixels(from_start);
    }
}

int DrawingVector(void) {
    if (CurrentSys == SYS_APPLE2) {
        return DrawingApple2Vector();
    } else if (CurrentSys == SYS_ATARI8) {
        return DrawingAtari8Vector();
    }  else {
        return DrawingHowarthVector();
    }
}

int TimerDelay(void) {
    if (CurrentSys == SYS_APPLE2 || CurrentSys == SYS_ATARI8) {
        if ((CurrentGame == PIRATE_US && vector_image_shown == 80) ||
            (CurrentGame == SECRET_MISSION_US && vector_image_shown == 88))
            return SLOW_VECTOR_DELAY;
        return US_VECTOR_DELAY;
    }  else {
        return HOWARTH_VECTOR_DELAY;
    }
}

static void StartImageSession(VectorOpList *list) {
    VectorOpListStartSession(list);
    glk_request_timer_events(0);
    VectorState = DRAWING_VECTOR_IMAGE;
}

int VectorBeginImage(VectorOpList *list, const USImage *img, void (*redraw_shown_image)(void)) {
    if (img->usage == IMG_ROOM) {
        /* Room images reset any drawing */
        StartImageSession(list);
        vector_image_shown = img->index;
    } else if (VectorState == SHOWING_VECTOR_IMAGE) {
        /* If this is not a room image and we are already showing an image,
           we can assume that we want to draw a room object on top of
           the current room image */
        redraw_shown_image();
        StartImageSession(list);
    } else if (list->ops == NULL) {
        return 0;
    }
    return 1;
}
