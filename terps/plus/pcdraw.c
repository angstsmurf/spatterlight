/* Routine to draw the PC graphics from Claymorgue

 Code by David Lodge 29/04/2005
 */
#include <stdio.h>
#include <stdlib.h>

#include "glk.h"
#include "graphics.h"
#include "common.h"

int x = 0, y = 0, at_last_line = 0;

int xlen=280, ylen=158;
int xoff=0, yoff=0;
int ycount=0;
int skipy=1;

extern winid_t Graphics;

int pixel_size;
int x_offset, y_offset, right_margin;

/* palette handler stuff starts here */

typedef uint8_t RGB[3];

typedef RGB PALETTE[16];

PALETTE pal;

void PutPixel(glsi32 xpos, glsi32 ypos, int32_t color)
{
    glui32 glk_color = ((pal[color][0] << 16)) | ((pal[color][1] << 8)) | (pal[color][2]);

    xpos = xpos * pixel_size;

    if (upside_down)
        xpos = ImageWidth * pixel_size - xpos;

    xpos += x_offset;

    if (xpos < x_offset || xpos >= right_margin) {
        return;
    }

    ypos = ypos * pixel_size;
    if (upside_down)
        ypos = (ImageHeight - 1) * pixel_size - ypos;
    ypos += y_offset;

    glk_window_fill_rect(Graphics, glk_color, xpos,
                         ypos, pixel_size, pixel_size);
}

void PutDoublePixel(glsi32 xpos, glsi32 ypos, int32_t color)
{
    glui32 glk_color = ((pal[color][0] << 16)) | ((pal[color][1] << 8)) | (pal[color][2]);

    xpos = xpos * pixel_size;

    if (upside_down)
        xpos = ImageWidth * pixel_size - xpos;
    xpos += x_offset;

    if (xpos < x_offset || xpos >= right_margin) {
        return;
    }

    ypos = ypos * pixel_size;
    if (upside_down) {
        ypos = ImageHeight * pixel_size - ypos;
        if (CurrentSys == SYS_ST)
            ypos -= 3 * pixel_size;
        else if (CurrentSys == SYS_ATARI8 || CurrentSys == SYS_MSDOS)
            ypos -= pixel_size;
    }
    ypos += y_offset;

    glk_window_fill_rect(Graphics, glk_color, xpos,
                         ypos, pixel_size * 2, pixel_size);
}



static void DrawDOSPixels(int pattern)
{
    int pix1,pix2,pix3,pix4;
    // Now get colors
    pix1=(pattern & 0xc0)>>6;
    pix2=(pattern & 0x30)>>4;
    pix3=(pattern & 0x0c)>>2;
    pix4=(pattern & 0x03);

    if (!skipy) {
        PutDoublePixel(x,y, pix1); x += 2;
        PutDoublePixel(x,y, pix2); x += 2;
        PutDoublePixel(x,y, pix3); x += 2;
        PutDoublePixel(x,y, pix4); x += 2;
    } else {
        PutPixel(x,y, pix1); x++;
        PutPixel(x,y, pix2); x++;
        PutPixel(x,y, pix3); x++;
        PutPixel(x,y, pix4); x++;
    }

    if (x >= xlen + xoff)
    {
        y += 2;
        x = xoff;
        ycount++;
    }

    if (ycount > ylen)
    {
        y = yoff + 1;
        at_last_line++;
        ycount = 0;
    }
}

void SetColor(int32_t index, const RGB *color)
{
    pal[index][0] = (*color)[0];
    pal[index][1] = (*color)[1];
    pal[index][2] = (*color)[2];
}

void SetRGB(int32_t index, int red, int green, int blue) {
    red = red * 35.7;
    green = green * 35.7;
    blue = blue * 35.7;

    pal[index][0] = red;
    pal[index][1] = green;
    pal[index][2] = blue;
}


int DrawDOSImageFromData(uint8_t *ptr, size_t datasize)
{
    x = 0;
    y = 0;
    at_last_line = 0;

    xlen = 0;
    ylen = 0;
    xoff = 0; yoff = 0;
    ycount = 0;
    skipy = 1;

    int work;
    int c;
    int i;
    int rawoffset;
    RGB black =   { 0,0,0 };
    RGB magenta = { 255,0,255 };
    RGB cyan =    { 0,255,255 };
    RGB white =   { 255,255,255 };

    /* set up the palette */
    SetColor(0,&black);
    SetColor(1,&cyan);
    SetColor(2,&magenta);
    SetColor(3,&white);

    if (ptr == NULL) {
        fprintf(stderr, "DrawMSDOSImageFromData: ptr == NULL\n");
        return 0;
    }

    uint8_t *origptr = ptr;

    // Get the size of the graphics chunk
    ptr = origptr + 0x05;
    work = *ptr++;
    int imagesize = work + (*ptr * 256);

    // Get whether it is lined
    ptr = origptr + 0x0d;
    work = *ptr++;
    if (work == 0xff) skipy=0;

    // Get the offset
    ptr = origptr + 0x0f;
    work = *ptr++;
    rawoffset = work + (*ptr * 256);
    xoff = ((rawoffset % 80) * 4) - 24;
    yoff = rawoffset / 40;
    yoff -= (yoff % 2 == 1);
    x = xoff;
    y = yoff;

    // Get the y length
    ptr = origptr + 0x11;
    work = *ptr++;
    ylen = work + (*ptr * 256);
    ylen -= rawoffset;
    ylen /= 80;

    // Get the x length
    ptr = origptr + 0x13;
    xlen = *ptr * 4;

    ptr = origptr + 0x17;
    while (ptr - origptr < imagesize)
    {
        // First get count
        c = *ptr++;

        if ((c & 0x80) == 0x80)
        { // is a counter
            work = *ptr++;
            c &= 0x7f;
            for (i = 0; i <= c; i++)
            {
                DrawDOSPixels(work);
            }
        }
        else
        {
            // Don't count on the next j characters
            for (i = 0; i <= c; i++)
            {
                work = *ptr++;
                DrawDOSPixels(work);
            }
        }
        if (at_last_line > 1) break;
    }
    return 1;
}
