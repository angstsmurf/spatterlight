//
//  vector_common.c
//  scott
//
//  Created by Administrator on 2026-02-01.
//

#include "apple2_vector_draw.h"
#include "atari_8bit_vector_draw.h"
#include "line_drawing.h"
#include "scott.h"
#include "vector_common.h"

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
            return 30;
        return 10;
    }  else {
        return 20;
    }
}

