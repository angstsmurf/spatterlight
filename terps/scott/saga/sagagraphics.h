//
//  graphics.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-09-07.
//

#ifndef graphics_h
#define graphics_h

#include <stdio.h>

#include "glk.h"
#include "scottdefines.h"

typedef struct USImage {
    USImageType usage;
    int index;
    MachineType systype;
    size_t datasize;
    uint8_t *imagedata;
    int cropleft;
    int cropright;
    struct USImage *previous;
    struct USImage *next;
} USImage;

extern struct USImage *USImages;

typedef struct CropList {
    GameIDType game;
    USImageType usage;
    int index;
    int cropleft;
    int cropright;
} CropList;

typedef uint8_t RGB[3];
typedef RGB PALETTE[16];

extern int pixel_size;
extern int x_offset, y_offset;
extern int right_margin, left_margin;
extern PALETTE pal;

void SetColor(int32_t index, const RGB *color);

void PutPixel(glsi32 x, glsi32 y, int32_t color);
void PutDoublePixel(glsi32 xpos, glsi32 ypos, int32_t color);
void RectFill(int32_t x, int32_t y, int32_t width, int32_t height,
              int32_t color);

USImage *new_image(void);
int issagaimg(const char *name);
int has_graphics(void);

#endif /* graphics_h */
