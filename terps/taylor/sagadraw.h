//
//  sagadraw.h
//  SagaDraw
//
//  Created by Administrator on 2021-12-15.
//

#ifndef sagadraw_h
#define sagadraw_h

#include "glk.h"
#include "taylor.h"

typedef struct {
  uint8_t *imagedata;
  uint8_t xoff;
  uint8_t yoff;
  uint8_t width;
  uint8_t height;
} Image;

uint8_t *DrawSagaPictureFromData(uint8_t *dataptr, int xsize, int ysize,
                                     int xoff, int yoff);

void DrawSagaPictureNumber(int picture_number);
void DrawSagaPictureAtPos(int picture_number, int x, int y);
void DrawSagaPictureFromBuffer(void);
void Flip(uint8_t character[]);

void SagaSetup(void);

void PutPixel(glsi32 x, glsi32 y, int32_t color);
void RectFill(int32_t x, int32_t y, int32_t width, int32_t height,
    int32_t color, int usebuffer);

void SwitchPalettes(int pal1, int pal2);
void DefinePalette(void);
int32_t Remap(int32_t color);

void DrawTaylor(int loc);
void ClearGraphMem(void);

extern palette_type palchosen;

extern int white_colour;
extern int blue_colour;

extern glui32 pixel_size;
extern glui32 x_offset;

#endif /* sagadraw_h */
