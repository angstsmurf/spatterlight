/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  sagagraphics.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Created by Petter Sjölund on 2022-09-07.
 */

#ifndef sagagraphics_h
#define sagagraphics_h

#include <stddef.h>
#include "glk.h"
#include "scott_defines.h"

struct USImage {
    USImageType usage;
    int index;
    MachineType systype;
    size_t datasize;
    uint8_t *imagedata;
    int cropleft;
    int cropright;
    struct USImage *next;
};

typedef struct USImage USImage;

extern USImage *USImages;

/* The inventory picture is stored as room image 98. */
#define INVENTORY_IMAGE 98

extern int pixel_size;
extern int x_offset, y_offset;
extern int left_margin;
extern glui32 pal[16];

void SetColor(int32_t index, glui32 color);

void PutPixel(glsi32 x, glsi32 y, int32_t color);
void PutDoublePixel(glsi32 x, glsi32 y, int32_t color);
void PutPixelWithWidth(glsi32 xpos, glsi32 ypos, int32_t color, int pixel_width);
/* Bracket a bitmap picture that is drawn in one go, so that its pixels
   reach the window as runs rather than one at a time. */
void StartPixelRuns(void);
void FlushPixelRuns(void);
void RectFill(int32_t x, int32_t y, int32_t width, int32_t height,
              int32_t color);
uint8_t *FindImageFile(const char *shortname, size_t *datasize);

/* Returns a new, empty image that is not in the USImages list. */
USImage *NewImage(void);
/* Adds a picture to the end of the USImages list and returns it. The list
   takes over data, which must have been allocated with MemAlloc or malloc. */
USImage *AddImage(USImageType usage, int index, MachineType systype,
                  uint8_t *data, size_t datasize);
/* Frees the USImages list and the picture data in it. */
void FreeImages(void);

int IsSagaImage(const char *name);
int HasGraphics(void);
void DrawImageOrVector(void);

#endif /* sagagraphics_h */
