/* Routine to draw the Atari 8-bit and C64 RLE graphics

 Code by David Lodge 29/04/2005
 */
#include <stdio.h>
#include <stdlib.h>

#include "scott.h"
#include "graphics.h"

extern int x, y, count;
extern int xlen, ylen;
extern int xoff, yoff;
extern int size;

typedef uint8_t RGB[3];

static void DrawA8C64Pixels(int pattern, int pattern2)
{
    int pix1,pix2,pix3,pix4;

    if (x>(xlen - 3)*8)
      return;

    pix1 = (pattern & 0xc0)>>6;
    pix2 = (pattern & 0x30)>>4;
    pix3 = (pattern & 0x0c)>>2;
    pix4 = (pattern & 0x03);

//    fprintf(stderr, "Drawing pixels at %d,%d\n", x, y);

    PutDoublePixel(x,y, pix1); x += 2;
    PutDoublePixel(x,y, pix2); x += 2;
    PutDoublePixel(x,y, pix3); x += 2;
    PutDoublePixel(x,y, pix4); x += 2;
    y++;
    x-=8;

    pix1=(pattern2 & 0xc0)>>6;
    pix2=(pattern2 & 0x30)>>4;
    pix3=(pattern2 & 0x0c)>>2;
    pix4=(pattern2 & 0x03);

    PutDoublePixel(x,y, pix1); x += 2;
    PutDoublePixel(x,y, pix2); x += 2;
    PutDoublePixel(x,y, pix3); x += 2;
    PutDoublePixel(x,y, pix4); x += 2;
    y++;
    x -= 8;

    if (y > ylen)
    {
        x += 8;
        y = yoff;
    }
}

/* C64 colors */
static const RGB black = { 0, 0, 0 };
static const RGB white = { 255, 255, 255 };
static const RGB red = { 191, 97, 72 };
static const RGB purple = { 177, 89, 185 };
static const RGB green = { 121, 213, 112 };
static const RGB blue = { 95, 72, 233 };
static const RGB yellow = { 247, 255, 108 };
static const RGB orange = { 186, 134, 32 };
static const RGB brown = { 131, 112, 0 };
static const RGB lred = { 231, 154, 132 };
static const RGB grey = { 167, 167, 167 };
static const RGB lgreen = { 192, 255, 185 };
static const RGB lblue = { 162, 143, 255 };


/* Atari 8-bit colors */
static const RGB cyan = { 0x9c, 0xe2, 0xc5 };
static const RGB lpurple = { 0xdf, 0xaa, 0xff };
static const RGB apurple = { 0xdb, 0x47, 0xdd };
static const RGB deeppurple = { 0x73, 0x1c, 0x73 };
static const RGB dblue = { 0x30, 0x24, 0xff };
static const RGB agreen = { 0x08, 0x88, 0x17 };
static const RGB dgreen = { 0x4f, 0x74, 0x20 };
static const RGB darkergreen = { 0x08, 0x38, 0x00 };
static const RGB ablue = { 0x36, 0x6e, 0xff };
static const RGB ayellow = { 0xef, 0xf2, 0x58};
static const RGB aorange = { 0xbf, 0x77, 0x30 };
static const RGB abrown = { 0xab, 0x51, 0x1f };
static const RGB dbrown = { 0x73, 0x2c, 0x00 };
static const RGB alred = { 0xc2, 0x52, 0x57 };
static const RGB beige = { 0xff, 0x8f, 0x8f };
static const RGB dred = { 0xa2, 0x3f, 0x40 };
static const RGB agrey = { 0x92, 0x92, 0x92 };
static const RGB lgrey = { 0xb4, 0xb5, 0xb4};
static const RGB algreen = { 0x5f, 0x8f, 0x00 };
static const RGB tan = { 0xaf, 0x99, 0x3a };
static const RGB lilac = { 0x83, 0x58, 0xee };

static const RGB colors[256] = {
    { 0x00, 0x00, 0x00 },
    { 0x1c, 0x1c, 0x1c },
    { 0x39, 0x39, 0x39 },
    { 0x59, 0x59, 0x59 },
    { 0x79, 0x79, 0x79 },
    { 0x92, 0x92, 0x92 },
    { 0xab, 0xab, 0xab },
    { 0xbc, 0xbc, 0xbc },
    { 0xcd, 0xcd, 0xcd },
    { 0xd9, 0xd9, 0xd9 },
    { 0xe6, 0xe6, 0xe6 },
    { 0xec, 0xec, 0xec },
    { 0xf2, 0xf2, 0xf2 },
    { 0xf8, 0xf8, 0xf8 },
    { 0xff, 0xff, 0xff },
    { 0xff, 0xff, 0xff },
    { 0x39, 0x17, 0x01 },
    { 0x5e, 0x23, 0x04 },
    { 0x83, 0x30, 0x08 },
    { 0xa5, 0x47, 0x16 },
    { 0xc8, 0x5f, 0x24 },
    { 0xe3, 0x78, 0x20 },
    { 0xff, 0x91, 0x1d },
    { 0xff, 0xab, 0x1d },
    { 0xff, 0xc5, 0x1d },
    { 0xff, 0xce, 0x34 },
    { 0xff, 0xd8, 0x4c },
    { 0xff, 0xe6, 0x51 },
    { 0xff, 0xf4, 0x56 },
    { 0xff, 0xf9, 0x77 },
    { 0xff, 0xff, 0x98 },
    { 0xff, 0xff, 0x98 },
    { 0x45, 0x19, 0x04 },
    { 0x72, 0x1e, 0x11 },
    { 0x9f, 0x24, 0x1e },
    { 0xb3, 0x3a, 0x20 },
    { 0xc8, 0x51, 0x22 },
    { 0xe3, 0x69, 0x20 },
    { 0xff, 0x81, 0x1e },
    { 0xff, 0x8c, 0x25 },
    { 0xff, 0x98, 0x2c },
    { 0xff, 0xae, 0x38 },
    { 0xff, 0xc5, 0x45 },
    { 0xff, 0xc5, 0x59 },
    { 0xff, 0xc6, 0x6d },
    { 0xff, 0xd5, 0x87 },
    { 0xff, 0xe4, 0xa1 },
    { 0xff, 0xe4, 0xa1 },
    { 0x4a, 0x17, 0x04 },
    { 0x7e, 0x1a, 0x0d },
    { 0xb2, 0x1d, 0x17 },
    { 0xc8, 0x21, 0x19 },
    { 0xdf, 0x25, 0x1c },
    { 0xec, 0x3b, 0x38 },
    //    { 0xfa, 0x52, 0x55 },
    { 0xa2, 0x3f, 0x40 },
    { 0xfc, 0x61, 0x61 },
    { 0xff, 0x70, 0x6e },
    { 0xff, 0x7f, 0x7e },
    { 0xff, 0x8f, 0x8f },
    { 0xff, 0x9d, 0x9e },
    { 0xff, 0xab, 0xad },
    { 0xff, 0xb9, 0xbd },
    { 0xff, 0xc7, 0xce },
    { 0xff, 0xc7, 0xce },
    { 0x05, 0x05, 0x68 },
    { 0x3b, 0x13, 0x6d },
    { 0x71, 0x22, 0x72 },
    { 0x8b, 0x2a, 0x8c },
    { 0xa5, 0x32, 0xa6 },
    { 0xb9, 0x38, 0xba },
    { 0xcd, 0x3e, 0xcf },
    { 0xdb, 0x47, 0xdd },
    { 0xea, 0x51, 0xeb },
    { 0xf4, 0x5f, 0xf5 },
    { 0xfe, 0x6d, 0xff },
    { 0xfe, 0x7a, 0xfd },
    { 0xff, 0x87, 0xfb },
    { 0xff, 0x95, 0xfd },
    { 0xff, 0xa4, 0xff },
    { 0xff, 0xa4, 0xff },
    { 0x28, 0x04, 0x79 },
    { 0x40, 0x09, 0x84 },
    { 0x59, 0x0f, 0x90 },
    { 0x70, 0x24, 0x9d },
    { 0x88, 0x39, 0xaa },
    { 0xa4, 0x41, 0xc3 },
    { 0xc0, 0x4a, 0xdc },
    { 0xd0, 0x54, 0xed },
    { 0xe0, 0x5e, 0xff },
    { 0xe9, 0x6d, 0xff },
    { 0xf2, 0x7c, 0xff },
    { 0xf8, 0x8a, 0xff },
    { 0xff, 0x98, 0xff },
    { 0xfe, 0xa1, 0xff },
    { 0xfe, 0xab, 0xff },
    { 0xfe, 0xab, 0xff },
    { 0x35, 0x08, 0x8a },
    { 0x42, 0x0a, 0xad },
    { 0x50, 0x0c, 0xd0 },
    { 0x64, 0x28, 0xd0 },
    { 0x79, 0x45, 0xd0 },
    { 0x8d, 0x4b, 0xd4 },
    { 0xa2, 0x51, 0xd9 },
    { 0xb0, 0x58, 0xec },
    { 0xbe, 0x60, 0xff },
    { 0xc5, 0x6b, 0xff },
    { 0xcc, 0x77, 0xff },
    { 0xd1, 0x83, 0xff },
    { 0xd7, 0x90, 0xff },
    { 0xdb, 0x9d, 0xff },
    { 0xdf, 0xaa, 0xff },
    { 0xdf, 0xaa, 0xff },
    { 0x05, 0x1e, 0x81 },
    { 0x06, 0x26, 0xa5 },
    { 0x08, 0x2f, 0xca },
    { 0x26, 0x3d, 0xd4 },
    { 0x44, 0x4c, 0xde },
    { 0x4f, 0x5a, 0xee },
    { 0x5a, 0x68, 0xff },
    { 0x65, 0x75, 0xff },
    { 0x71, 0x83, 0xff },
    { 0x80, 0x91, 0xff },
    { 0x90, 0xa0, 0xff },
    { 0x97, 0xa9, 0xff },
    { 0x9f, 0xb2, 0xff },
    { 0xaf, 0xbe, 0xff },
    { 0xc0, 0xcb, 0xff },
    { 0xc0, 0xcb, 0xff },
    { 0x0c, 0x04, 0x8b },
    { 0x22, 0x18, 0xa0 },
    { 0x38, 0x2d, 0xb5 },
    { 0x48, 0x3e, 0xc7 },
    { 0x58, 0x4f, 0xda },
    { 0x61, 0x59, 0xec },
    { 0x6b, 0x64, 0xff },
    //    { 0x7a, 0x74, 0xff },
    { 0x39, 0x61, 0xac },
    { 0x8a, 0x84, 0xff },
    { 0x91, 0x8e, 0xff },
    { 0x99, 0x98, 0xff },
    { 0xa5, 0xa3, 0xff },
    { 0xb1, 0xae, 0xff },
    { 0xb8, 0xb8, 0xff },
    { 0xc0, 0xc2, 0xff },
    { 0xc0, 0xc2, 0xff },
    { 0x1d, 0x29, 0x5a },
    { 0x1d, 0x38, 0x76 },
    { 0x1d, 0x48, 0x92 },
    { 0x1c, 0x5c, 0xac },
    { 0x1c, 0x71, 0xc6 },
    { 0x32, 0x86, 0xcf },
    { 0x48, 0x9b, 0xd9 },
    { 0x4e, 0xa8, 0xec },
    { 0x55, 0xb6, 0xff },
    { 0x70, 0xc7, 0xff },
    { 0x8c, 0xd8, 0xff },
    { 0x93, 0xdb, 0xff },
    { 0x9b, 0xdf, 0xff },
    { 0xaf, 0xe4, 0xff },
    { 0xc3, 0xe9, 0xff },
    { 0xc3, 0xe9, 0xff },
    { 0x2f, 0x43, 0x02 },
    { 0x39, 0x52, 0x02 },
    { 0x44, 0x61, 0x03 },
    { 0x41, 0x7a, 0x12 },
    { 0x3e, 0x94, 0x21 },
    { 0x4a, 0x9f, 0x2e },
    { 0x57, 0xab, 0x3b },
    { 0x5c, 0xbd, 0x55 },
    { 0x61, 0xd0, 0x70 },
    { 0x69, 0xe2, 0x7a },
    { 0x72, 0xf5, 0x84 },
    { 0x7c, 0xfa, 0x8d },
    { 0x87, 0xff, 0x97 },
    { 0x9a, 0xff, 0xa6 },
    //    { 0xad, 0xff, 0xb6 },
    //    { 0xad, 0xff, 0xb6 },
    { 0x9c, 0xe2, 0xc5 },
    { 0x9c, 0xe2, 0xc5 },
    { 0x0a, 0x41, 0x08 },
    { 0x0d, 0x54, 0x0a },
    { 0x10, 0x68, 0x0d },
    { 0x13, 0x7d, 0x0f },
    { 0x16, 0x92, 0x12 },
    { 0x19, 0xa5, 0x14 },
    //    { 0x1c, 0xb9, 0x17 },
    { 0x31, 0x85, 0x00 },
    { 0x1e, 0xc9, 0x19 },
    { 0x21, 0xd9, 0x1b },
    { 0x47, 0xe4, 0x2d },
    { 0x6e, 0xf0, 0x40 },
    { 0x78, 0xf7, 0x4d },
    { 0x83, 0xff, 0x5b },
    { 0x9a, 0xff, 0x7a },
    { 0xb2, 0xff, 0x9a },
    { 0xb2, 0xff, 0x9a },
    { 0x04, 0x41, 0x0b },
    { 0x05, 0x53, 0x0e },
    { 0x06, 0x66, 0x11 },
    { 0x07, 0x77, 0x14 },
    { 0x08, 0x88, 0x17 },
    { 0x09, 0x9b, 0x1a },
    //    { 0x0b, 0xaf, 0x1d },
    { 0x41, 0x81, 0x00 },
    { 0x48, 0xc4, 0x1f },
    //    { 0x86, 0xd9, 0x22 },
    { 0x5a, 0x9d, 0x00 },
    { 0x8f, 0xe9, 0x24 },
    { 0x99, 0xf9, 0x27 },
    { 0xa8, 0xfc, 0x41 },
    { 0xb7, 0xff, 0x5b },
    { 0xc9, 0xff, 0x6e },
    { 0xdc, 0xff, 0x81 },
    { 0xdc, 0xff, 0x81 },
    { 0x02, 0x35, 0x0f },
    { 0x07, 0x3f, 0x15 },
    { 0x0c, 0x4a, 0x1c },
    { 0x2d, 0x5f, 0x1e },
    { 0x4f, 0x74, 0x20 },
    { 0x59, 0x83, 0x24 },
    { 0x64, 0x92, 0x28 },
    { 0x82, 0xa1, 0x2e },
    { 0xa1, 0xb0, 0x34 },
    { 0xa9, 0xc1, 0x3a },
    { 0xb2, 0xd2, 0x41 },
    { 0xc4, 0xd9, 0x45 },
    { 0xd6, 0xe1, 0x49 },
    { 0xe4, 0xf0, 0x4e },
    { 0xf2, 0xff, 0x53 },
    { 0xf2, 0xff, 0x53 },
    { 0x26, 0x30, 0x01 },
    { 0x24, 0x38, 0x03 },
    { 0x23, 0x40, 0x05 },
    { 0x51, 0x54, 0x1b },
    { 0x80, 0x69, 0x31 },
    { 0x97, 0x81, 0x35 },
    { 0xaf, 0x99, 0x3a },
    { 0xc2, 0xa7, 0x3e },
    { 0xd5, 0xb5, 0x43 },
    { 0xdb, 0xc0, 0x3d },
    { 0xe1, 0xcb, 0x38 },
    { 0xe2, 0xd8, 0x36 },
    { 0xe3, 0xe5, 0x34 },
    { 0xef, 0xf2, 0x58 },
    { 0xfb, 0xff, 0x7d },
    { 0xfb, 0xff, 0x7d },
    { 0x40, 0x1a, 0x02 },
    { 0x58, 0x1f, 0x05 },
    { 0x70, 0x24, 0x08 },
    { 0x8d, 0x3a, 0x13 },
    { 0xab, 0x51, 0x1f },
    { 0xb5, 0x64, 0x27 },
    { 0xbf, 0x77, 0x30 },
    { 0xd0, 0x85, 0x3a },
    { 0xe1, 0x93, 0x44 },
    { 0xed, 0xa0, 0x4e },
    { 0xf9, 0xad, 0x58 },
    { 0xfc, 0xb7, 0x5c },
    { 0xff, 0xc1, 0x60 },
    { 0xff, 0xc6, 0x71 },
    { 0xff, 0xcb, 0x83 },
    { 0xff, 0xcb, 0x83 }
};

static void TranslateAtariColorRGB(int index, uint8_t value) {
    SetColor(index,&colors[value]);
}

static void TranslateAtariColor(int index, uint8_t value) {
    switch(value) {
        case 0:
        case 1:
        case 224:
            SetColor(index,&black);
            break;
        case 5:
        case 7:
            SetColor(index,&agrey);
            break;
        case 6:
        case 8:
            SetColor(index,&ablue);
            break;
        case 9:
        case 10:
        case 12:
            SetColor(index,&lgrey);
            break;
        case 14:
        case 255:
            SetColor(index,&white);
            break;
        case 17:
        case 32:
        case 36:
            SetColor(index,&dbrown);
            break;
        case 40:
            SetColor(index, &aorange);
            break;
        case 54:
            SetColor(index,&dred);
            break;
        case 56:
            SetColor(index,&alred);
            break;
        case 58:
        case 60:
            SetColor(index,&beige);
            break;
        case 69:
        case 71:
            SetColor(index,&deeppurple);
            break;
        case 68:
        case 103:
            SetColor(index,&apurple);
            break;
        case 84:
        case 89:
            SetColor(index,&lilac);
            break;
        case 85:
        case 101:
            SetColor(index,&dblue);
            break;
        case 110:
        case 128:
            SetColor(index,&lpurple);
            break;
        case 135:
        case 149:
            SetColor(index,&ablue);
            break;
        case 170:
        case 174:
            SetColor(index,&cyan);
            break;
        case 182:
        case 187:
        case 196:
        case 214:
        case 198:
        case 200:
            SetColor(index,&agreen);
            break;
        case 194:
            SetColor(index,&darkergreen);
            break;
        case 199:
        case 215:
        case 201:
            SetColor(index,&algreen);
            break;
        case 212:
            SetColor(index,&dgreen);
            break;
        case 229:
        case 230:
            SetColor(index,&tan);
            break;
        case 238:
        case 237:
            SetColor(index,&ayellow);
            break;
        case 244:
            SetColor(index,&abrown);
            break;
        case 246:
        case 248:
            SetColor(index,&aorange);
            break;
        default:
            fprintf(stderr, "Unknown color %d ", value);
            break;
    }
}

/*
 The values below are determined by looking at the games
 running in the VICE C64 emulator.
 I have no idea how the original interpreter calculates
 them. I might have made some mistakes. */

static void TranslateC64Color(int index, uint8_t value) {
    switch(value) {
        case 0:
            SetColor(index,&purple);
            break;
        case 1:
            SetColor(index,&blue);
            break;
        case 3:
            SetColor(index,&white);
            break;
        case 7:
        case 8:
            SetColor(index,&blue);
            break;
        case 9:
        case 10:
        case 12:
        case 14:
        case 15:
            SetColor(index,&white);
            break;
        case 17:
            SetColor(index,&green);
            break;
        case 36:
        case 40:
            SetColor(index,&brown);
            break;
        case 46:
            SetColor(index,&yellow);
            break;
        case 52:
        case 54:
        case 56:
        case 60:
            SetColor(index,&orange);
            break;
        case 68:
        case 69:
        case 71:
            SetColor(index,&red);
            break;
        case 77:
        case 85:
        case 87:
            SetColor(index,&purple);
            break;
        case 89:
            SetColor(index,&lred);
            break;
        case 101:
        case 103:
            SetColor(index,&purple);
            break;
        case 110:
            SetColor(index,&lblue);
            break;
        case 135:
        case 137:
            SetColor(index,&blue);
            break;
        case 157:
            SetColor(index,&lblue);
            break;
        case 161:
            SetColor(index,&grey);
            break;
        case 179:
        case 182:
        case 194:
        case 196:
        case 198:
        case 199:
        case 200:
            SetColor(index,&green);
            break;
        case 201:
            SetColor(index,&lgreen);
            break;
        case 212:
        case 214:
        case 215:
            SetColor(index,&green);
            break;
        case 224:
            SetColor(index,&purple);
            break;
        case 230:
        case 237:
            SetColor(index,&yellow);
            break;
        case 244:
        case 246:
            SetColor(index,&brown);
            break;
        case 248:
            SetColor(index,&brown);
            break;
        case 252:
            SetColor(index,&yellow);
            break;
        default:
            fprintf(stderr, "Unknown color %d ", value);
            break;
    }
}

int DrawAtariC64Image(USImage *image)
{
    fprintf(stderr, "DrawAtariC64Image: usage:%d index:%d datasize:%zu\n", image->usage, image->index, image->datasize);

    uint8_t *ptr = image->imagedata;
    size_t datasize = image->datasize;

    int work,work2;
    int c;
    int i;
    int countflag = (CurrentGame == COUNT_US || CurrentGame == VOODOO_CASTLE_US);
    fprintf(stderr, "countflag == %d\n", countflag);

    uint8_t *origptr = ptr;

    x = 0; y = 0;

    ptr += 2;

    work = *ptr++;
    size = work + *ptr++ * 256;
    fprintf(stderr, "size :%d\n", size);

    // Get the offset
    xoff = *ptr++ - 3;
    fprintf(stderr, "xoff :%d\n", xoff);
//    if (xoff < 0) xoff = 0;
    yoff = *ptr++;
    fprintf(stderr, "yoff :%d\n", yoff);
    x = xoff * 8;
    fprintf(stderr, "initial x :%d\n", x);
    y = yoff;

    // Get the x length
    xlen = *ptr++;
    fprintf(stderr, "xlen :%d\n", xlen);
    ylen = *ptr++;
    if (countflag) ylen -= 2;
    fprintf(stderr, "ylen :%d\n", ylen);

    if (image->systype == SYS_ATARI8) {
        glui32 curheight, curwidth;
        glk_window_get_size(Graphics, &curwidth, &curheight);
        if ((ylen == 126 && xlen == 39) || (ylen == 158 && xlen == 38)) {
//            if (xlen == 38)
                xlen--;
            ImageHeight = ylen + 2;
            ImageWidth = xlen * 8;
            if (ylen == 158)
                ImageWidth -= 24;
            else
                ImageWidth -= 17;
        }

        int optimal_height = ImageHeight * pixel_size;
        if (curheight != optimal_height) {
            x_offset = (curwidth - (ImageWidth * pixel_size)) / 2;
            right_margin = (ImageWidth * pixel_size) + x_offset;
            winid_t parent = glk_window_get_parent(Graphics);
            if (parent)
                glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
                                           optimal_height, NULL);
        }
    }

    SetColor(0,&black);

    // Get the palette
//    debug_print("Colors: ");
    for (i = 1; i < 5; i++)
    {
        work = *ptr++;
        if (image->systype == SYS_C64)
            TranslateC64Color(i, work);
        else
            TranslateAtariColorRGB(i, work);
//            TranslateAtariColor(i, work);
//        debug_print("%d ", work);
    }
//    debug_print("\n");

    while (ptr - origptr < datasize - 2)
    {
        // First get count
        c = *ptr++;

        if (((c & 0x80) == 0x80) || countflag)
        { // is a counter
            if (!countflag) c &= 0x7f;
            if (countflag) c -= 1;
            work = *ptr++;
            work2 = *ptr++;
            for (i = 0; i < c + 1; i++)
            {
                DrawA8C64Pixels(work,work2);
            }
        }
        else
        {
            // Don't count on the next c characters
            for (i = 0; i < c + 1 && ptr - origptr < datasize - 1; i++)
            {
                work=*ptr++;
                work2=*ptr++;
                DrawA8C64Pixels(work,work2);
            }
        }
    }
    return 1;
}
