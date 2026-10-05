//
//  graphics.h
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-06-04.
//

#ifndef graphics_h
#define graphics_h

#include "glk.h"

#include "definitions.h"

/* Bytes needed for a short image name such as "R003", with terminator */
#define IMAGE_NAME_SIZE 5

extern winid_t Graphics;

extern int pixel_size;
extern int ImageWidth, ImageHeight;
extern int x_offset, y_offset, right_margin;
extern int upside_down;

extern int x, y;
extern int xlen, ylen;

int DrawCloseup(int item);
void DrawCurrentRoom(void);
int DrawRoomImage(int room);
void DrawItemImage(int item);
int DrawImageWithName(char *filename);
int DrawImageWithTypeAndNumber(char type, int index);
void DrawOverlayImage(ImgType type, int index);
void DrawObjectImages(int room, int location);
char *ShortNameFromType(char type, int index);
int ImageNumberFromName(const char *name);
void SetColor(int32_t index, glui32 color);
// Only IBM PC graphics in "striped" mode use PutPixel()
void PutPixel(glsi32 xpos, glsi32 ypos, int32_t color);
void PutDoublePixel(glsi32 xpos, glsi32 ypos, int32_t color);
void PutPixelWithWidth(glsi32 xpos, glsi32 ypos, int32_t color, int pixelwidth);

#endif /* graphics_h */
