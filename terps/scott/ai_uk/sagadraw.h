//
//  sagadraw.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2021-12-15.
//

#ifndef sagadraw_h
#define sagadraw_h

#include "glk.h"
#include "scottdefines.h"

void DrawSagaPictureNumber(int picture_number, int draw_to_buffer);
void DrawSagaPictureAtPos(int picture_number, int x, int y, int draw_to_buffer);

void SagaSetup(size_t imgoffset);

void RectFill(int32_t x, int32_t y, int32_t width, int32_t height,
    int32_t color);

void SwitchPalettes(int pal1, int pal2);
void DefinePalette(void);
int32_t Remap(int32_t color);

extern palette_type palchosen;

// Used by Robin of Sherwood waterfall animation
// and Secret Mission screen animation
extern int white_colour;
extern int blue_colour;
// Used by Seas of Blood
extern glui32 dice_colour;

#endif /* sagadraw_h */
