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

/* Accessors for the two rendered screen planes ($9000 / $A000 on real
   hardware), each A8_IMAGE_SIZE (0xf00) bytes. Used by the byte-exact
   ground-truth regression harness. */
#define ATARI8_PLANE_SIZE 0xf00
const uint8_t *GetAtari8Screenmem90(void);
const uint8_t *GetAtari8ScreenmemA0(void);

#endif /* atari_8bit_vector_draw_h */
