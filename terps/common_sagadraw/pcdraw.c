//
//  pcdraw.c
//
//  Used by ScottFree and Plus,
//  two interpreters for adventures in Scott Adams formats
//
//  Routines to draw IBM PC RLE bitmap graphics.
//  Based on Code by David Lodge.
//
//  Original code:
// https://github.com/tautology0/textadventuregraphics/blob/master/AdventureInternational/pcdraw.c
//

#include "glk.h"
#include "read_le16.h"

static int x = 0, y = 0, at_last_line = 0;

static int xlen = 280, ylen = 158;
static int xoff = 0, yoff = 0;
static int ycount = 0;
static int skipy = 1;

void PutDoublePixel(glsi32 xpos, glsi32 ypos, int32_t color);
void PutPixel(glsi32 xpos, glsi32 ypos, int32_t color);

static void DrawDOSPixels(int pattern)
{
    int pix1, pix2, pix3, pix4;
    // Now get colors
    pix1 = (pattern & 0xc0) >> 6;
    pix2 = (pattern & 0x30) >> 4;
    pix3 = (pattern & 0x0c) >> 2;
    pix4 = (pattern & 0x03);

    if (!skipy) {
        PutDoublePixel(x, y, pix1);
        x += 2;
        PutDoublePixel(x, y, pix2);
        x += 2;
        PutDoublePixel(x, y, pix3);
        x += 2;
        PutDoublePixel(x, y, pix4);
        x += 2;
    } else {
        PutPixel(x, y, pix1);
        x++;
        PutPixel(x, y, pix2);
        x++;
        PutPixel(x, y, pix3);
        x++;
        PutPixel(x, y, pix4);
        x++;
    }

    if (x >= xlen + xoff) {
        y += 2;
        x = xoff;
        ycount++;
    }
    if (ycount > ylen) {
        y = yoff + 1;
        at_last_line++;
        ycount = 0;
    }
}

typedef uint8_t RGB[3];
void SetColor(int32_t index, const RGB *color);

int DrawDOSImageFromData(uint8_t *ptr)
{
    uint8_t *origptr = ptr;

    x = 0;
    y = 0;
    at_last_line = 0;

    xlen = 0;
    ylen = 0;
    xoff = 0;
    yoff = 0;
    ycount = 0;
    skipy = 1;

    int work;
    int repetitions;
    int i;

    // clang-format off

    RGB black =   { 0,0,0 };
    RGB magenta = { 255,0,255 };
    RGB cyan =    { 0,255,255 };
    RGB white =   { 255,255,255 };

    // clang-format on

    /* set up the palette */
    SetColor(0, &black);
    SetColor(1, &cyan);
    SetColor(2, &magenta);
    SetColor(3, &white);

    // Get the size of the graphics chunk
    size_t size = READ_LE_UINT16(origptr + 5);

    // Get whether it is lined
    if (origptr[0x0d] == 0xff)
        skipy = 0;

    // Get the offset
    int rawoffset = READ_LE_UINT16(origptr + 0x0f);
    xoff = ((rawoffset % 80) * 4) - 24;
    yoff = rawoffset / 40;
    yoff -= (yoff & 1);
    x = xoff;
    y = yoff;

    // Get the height
    ylen = (READ_LE_UINT16(origptr + 0x11) - rawoffset) / 80;

    // Get the width
    xlen = origptr[0x13] * 4;

    ptr = origptr + 0x17;
    while (ptr - origptr < size) {
        // First get count
        repetitions = *ptr++;

        if ((repetitions & 0x80) == 0x80) {
            repetitions &= 0x7f;
            // Repeat the next byte (repetitions + 1) times
            work = *ptr++;
            for (i = 0; i < repetitions + 1; i++) {
                DrawDOSPixels(work);
            }
        } else {
            // Draw the next (repetitions + 1) bytes
            // without repeating
            for (i = 0; i < repetitions + 1; i++) {
                work = *ptr++;
                DrawDOSPixels(work);
            }
        }
        if (at_last_line > 1)
            break;
    }
    return 1;
}
