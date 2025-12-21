//
//  c64a8draw.c
//
//  Atari 8-bit and Commodore 64 bitmap graphics for ScottFree and Plus,
//  two interpreters for adventures in Scott Adams formats
//
//  Routines to draw Atari 8-bit and C64 RLE graphics
//  Based on Code by David Lodge 29/04/2005
//
//  Original code at https://github.com/tautology0/textadventuregraphics

#include "glk.h"
#include "read_le16.h"

#include "c64a8draw.h"

void PutDoublePixel(glsi32 xpos, glsi32 ypos, int32_t color);
void SetColor(int32_t index, const uint8_t color[3]);

static int DrawPatternAndAdvancePos(int x, int *y,  uint8_t pattern) {
    uint8_t mask = 0xc0;

    for (int i = 6; i >= 0; i -= 2) {
        int color = (pattern & mask) >> i;
        PutDoublePixel(x, *y, color);
        mask >>= 2;
        x += 2;
    }
    (*y)++;
    return x - 8;
}

int DrawC64A8ImageFromData(uint8_t *ptr, size_t datasize, int voodoo_or_count, adjustments_fn adjustments, translate_color_fn translate)
{
    uint8_t work = 0, work2 = 0;
    int repetitions;
    int i;

    uint8_t *origptr = ptr;

    int xpos = 0;
    int ypos = 0;

    ptr += 2;

    // For Atari 8-bit, this vaule is already datasize
    // but on C64, datasize is the actual file size of the image
    // file on disk, so we shrink it here.
    size_t size = READ_LE_UINT16_AND_ADVANCE(&ptr);
    if (size < datasize - 2) {
        datasize = size + 2;
    }
    // Get the offset
    int x_origin = *ptr++ - 3;
    int y_origin = *ptr++;

    // Get the x length
    int width = *ptr++;

    int height = *ptr++;
    if (voodoo_or_count)
        height -= 2;

    // Custom graphics window adjustments
    // depending on whether we are being called
    // by Plus or by ScottFree.
    width = adjustments(width, height, &x_origin);

    xpos = x_origin * 8;
    ypos = y_origin;

    const uint8_t black[3] = { 0, 0, 0 };

    SetColor(0, black);

    // Get the palette
    for (i = 1; i < 5; i++) {
        work = *ptr++;
        // One of four color translation functions
        // depending on whether we are being called
        // by Plus or by ScottFree and whether the format
        // is Commodore 64 or Atari 8-bit.
        translate(i, work);
    }

    while (ptr - origptr < datasize - 2) {
        // First get repetitions
        repetitions = *ptr++;
        
        // If bit 7 is set or this is an early game
        // we write the next two bytes (repetitions) times
        int repeat_next_two_bytes = 0;
        if (((repetitions & 0x80) == 0x80) || voodoo_or_count) {
            repeat_next_two_bytes = 1;
            if (voodoo_or_count) {
                repetitions -= 1;
            } else {
                repetitions &= 0x7f;
            }
            work = *ptr++;
            work2 = *ptr++;
        }
        for (i = 0; i < repetitions + 1; i++) {
            // If we are not repeating bytes,
            // we read two new bytes on every iteration
            if (!repeat_next_two_bytes) {
                if (ptr - origptr >= datasize - 1)
                    return 1;
                work = *ptr++;
                work2 = *ptr++;
            }
            if (xpos <= (width - 3) * 8) {
                // We print a "pattern" of 8 pixels in a row,
                // or technically 4 double-width, non-square pixels
                xpos = DrawPatternAndAdvancePos(xpos, &ypos, work);
                xpos = DrawPatternAndAdvancePos(xpos, &ypos, work2);
                if (ypos > height) {
                    xpos += 8;
                    ypos = y_origin;
                }
            }
        }
    }
    return 1;
}
