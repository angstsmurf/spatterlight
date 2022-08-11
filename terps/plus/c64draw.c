/* Routine to draw the C64 RLE graphics

 Code by David Lodge 29/04/2005
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>


#include "definitions.h"
#include "glk.h"

extern int x, y, count;
extern int xlen, ylen;
extern int xoff, yoff;
extern int size;

typedef uint8_t RGB[3];

typedef RGB PALETTE[16];

extern PALETTE pal;

extern winid_t Graphics;

void PutPixel(glsi32 x, glsi32 y, int32_t color);

void DrawC64Pixels(int pattern, int pattern2)
{
    int pix1,pix2,pix3,pix4;

//    fprintf(stderr, "Plotting at %d %d: %x %x\n",x,y,pattern,pattern2);
    if (x>(xlen - 3)*8)
      return;

    pix1 = (pattern & 0xc0)>>6;
    pix2 = (pattern & 0x30)>>4;
    pix3 = (pattern & 0x0c)>>2;
    pix4 = (pattern & 0x03);

    PutPixel(x,y, pix1); x++;
    PutPixel(x,y, pix1); x++;
    PutPixel(x,y, pix2); x++;
    PutPixel(x,y, pix2); x++;
    PutPixel(x,y, pix3); x++;
    PutPixel(x,y, pix3); x++;
    PutPixel(x,y, pix4); x++;
    PutPixel(x,y, pix4); x++;
    y++;
    x-=8;

    pix1=(pattern2 & 0xc0)>>6;
    pix2=(pattern2 & 0x30)>>4;
    pix3=(pattern2 & 0x0c)>>2;
    pix4=(pattern2 & 0x03);

    PutPixel(x,y, pix1); x++;
    PutPixel(x,y, pix1); x++;
    PutPixel(x,y, pix2); x++;
    PutPixel(x,y, pix2); x++;
    PutPixel(x,y, pix3); x++;
    PutPixel(x,y, pix3); x++;
    PutPixel(x,y, pix4); x++;
    PutPixel(x,y, pix4); x++;
    y++;
    x -= 8;

    if (y > ylen)
    {
        x+=8;
        y=yoff;
    }
}

void SetColour(int32_t index, const RGB *colour);

static const RGB black = { 0, 0, 0 };
static const RGB white = { 255, 255, 255 };
static const RGB red = { 191, 97, 72 };
//    static const RGB cyan = { 153, 230, 249 };
static const RGB purple = { 177, 89, 185 };
static const RGB green = { 121, 213, 112 };
static const RGB blue = { 95, 72, 233 };
static const RGB yellow = { 247, 255, 108 };
static const RGB orange = { 186, 134, 32 };
static const RGB brown = { 131, 112, 0 };
static const RGB lred = { 231, 154, 132 };
//    static const RGB dgrey = { 69, 69, 69 };
static const RGB grey = { 167, 167, 167 };
static const RGB lgreen = { 192, 255, 185 };
static const RGB lblue = { 162, 143, 255 };
//    static const RGB lgrey = { 200, 200, 200 };

/*
 The values below are determined by looking at the games
 running in the VICE C64 emulator.
 I have no idea how the original interpreter calculates
 them. I might have made some mistakes. */

static void TranslateC64Colour(int index, uint8_t value) {
    switch(value) {
        case 0:
            SetColour(index,&purple);
            break;
        case 1:
            SetColour(index,&blue);
            break;
        case 3:
            SetColour(index,&white);
            break;
        case 7:
        case 8:
            SetColour(index,&blue);
            break;
        case 9:
        case 10:
        case 12:
        case 14:
        case 15:
            SetColour(index,&white);
            break;
        case 17:
            SetColour(index,&green);
            break;
        case 36:
        case 40:
            SetColour(index,&brown);
            break;
        case 46:
            SetColour(index,&yellow);
            break;
        case 52:
        case 54:
        case 56:
        case 60:
            SetColour(index,&orange);
            break;
        case 68:
        case 69:
        case 71:
            SetColour(index,&red);
            break;
        case 77:
        case 85:
        case 87:
            SetColour(index,&purple);
            break;
        case 89:
            SetColour(index,&lred);
            break;
        case 101:
        case 103:
            SetColour(index,&purple);
            break;
        case 110:
            SetColour(index,&lblue);
            break;
        case 135:
        case 137:
            SetColour(index,&blue);
            break;
        case 157:
            SetColour(index,&lblue);
            break;
        case 161:
            SetColour(index,&grey);
            break;
        case 179:
        case 182:
        case 194:
        case 196:
        case 198:
        case 199:
        case 200:
            SetColour(index,&green);
            break;
        case 201:
            SetColour(index,&lgreen);
            break;
        case 212:
        case 214:
        case 215:
            SetColour(index,&green);
            break;
        case 224:
            SetColour(index,&purple);
            break;
        case 230:
        case 237:
            SetColour(index,&yellow);
            break;
        case 244:
        case 246:
        case 248:
            SetColour(index,&brown);
            break;
        case 252:
            SetColour(index,&yellow);
            break;
        default:
            fprintf(stderr, "Unknown colour %d ", value);
            break;
    }
}

int DrawC64ImageFromData(uint8_t *ptr, size_t datasize)
{
    int work,work2;
    int c;
    int i;

    uint8_t *origptr = ptr;

    x = 0; y = 0;

    ptr += 2;
    
    work = *ptr++;
    size = work + *ptr++ * 256;

    // Get the offset
    xoff = *ptr++ - 3;
    if (xoff < 0) xoff = 8;
    yoff = *ptr++;
    x = xoff * 8;
    y = yoff;

    // Get the x length
    xlen = *ptr++;
    ylen = *ptr++;

    SetColour(0,&black);

    // Get the palette
    debug_print("Colours: ");
    for (i = 1; i < 5; i++)
    {
        work = *ptr++;
        TranslateC64Colour(i, work);
        debug_print("%d ", work);
    }
    debug_print("\n");

    while (ptr - origptr < datasize - 3)
    {
        // First get count
        c = *ptr++;

        if ((c & 0x80) == 0x80)
        { // is a counter
            c &= 0x7f;
            work = *ptr++;
            work2 = *ptr++;
            for (i = 0; i < c + 1; i++)
            {
                DrawC64Pixels(work,work2);
            }
        }
        else
        {
            // Don't count on the next c characters

            for (i = 0; i < c + 1 && ptr - origptr < datasize - 1; i++)
            {
                work=*ptr++;
                work2=*ptr++;
                DrawC64Pixels(work,work2);
            }
        }
    }
    return 1;
}
