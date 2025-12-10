//
//  sagadraw.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2021-12-15.
//

#ifndef sagadraw_h
#define sagadraw_h

#include "glk.h"
#include "taylor.h"

void DrawSagaPictureNumber(int picture_number);
void DrawIrmakPictureFromBuffer(void);
void DrawSagaPictureAtPos(int picture_number, int x, int y, int draw_to_buffer);

void SagaSetup(void);

void PutPixel(glsi32 x, glsi32 y, int32_t color);
void RectFill(int32_t x, int32_t y, int32_t width, int32_t height,
    int32_t color);

void DefinePalette(void);

void ClearGraphMem(void);
int32_t Remap(int32_t color);

void PatchAndDrawQP3Cannon(void);

extern palette_type palchosen;

extern glui32 pixel_size;
extern glui32 x_offset;

#endif /* sagadraw_h */
