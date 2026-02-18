//
//  atari_8bit_vector_draw.h
//  scott
//
//  Created by Administrator on 2026-01-13.
//

#ifndef atari_8bit_vector_draw_h
#define atari_8bit_vector_draw_h

#include <stdio.h>
#include "sagagraphics.h"

int DrawingAtari8Vector(void);
uint8_t *DrawAtari8BitVectorImage(USImage *img);
void DrawSomeAtari8VectorBytes(int from_start);
int RunAtari8bitVectorTests(const char *supportpath);

#endif /* atari_8bit_vector_draw_h */
