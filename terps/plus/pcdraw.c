/* Routine to draw the PC graphics from Claymorgue

 Code by David Lodge 29/04/2005
 */
#include <stdio.h>
#include <stdlib.h>

#include "glk.h"
#include "common.h"

int x=0,y=0,count=0;

int xlen=280, ylen=158;
int xoff=0, yoff=0;
int size;
int ycount=0;
int skipy=1;
FILE *infile,*outfile;

extern winid_t Graphics;

int pixel_size;
int x_offset, y_offset, right_margin;

/* palette handler stuff starts here */

typedef uint8_t RGB[3];

typedef RGB PALETTE[16];

PALETTE pal;

void PutPixel(glsi32 x, glsi32 y, int32_t color)
{
    glui32 glk_color = ((pal[color][0] << 16)) | ((pal[color][1] << 8)) | (pal[color][2]);

    glsi32 xpos = x * pixel_size;

    if (upside_down)
        xpos = 280 * pixel_size - xpos;
    xpos += x_offset;

    if (xpos < x_offset || xpos >= right_margin) {
        return;
    }

    int ypos = y * pixel_size;
    if (upside_down)
        ypos = 157 * pixel_size - ypos;
    ypos += y_offset;

    glk_window_fill_rect(Graphics, glk_color, xpos,
                         ypos, pixel_size, pixel_size);
}


static void DrawDOSPixels(int pattern)
{
    int pix1,pix2,pix3,pix4;
    // Now get colours
    pix1=(pattern & 0xc0)>>6;
    pix2=(pattern & 0x30)>>4;
    pix3=(pattern & 0x0c)>>2;
    pix4=(pattern & 0x03);

    PutPixel(x,y, pix1);
    x++;
    if (!skipy) { PutPixel(x,y, pix1); x++; }
    PutPixel(x,y, pix2);
    x++;
    if (!skipy) { PutPixel(x,y, pix2); x++; }
    PutPixel(x,y, pix3);
    x++;
    if (!skipy) { PutPixel(x,y, pix3); x++; }
    PutPixel(x,y, pix4);
    x++;
    if (!skipy) { PutPixel(x,y, pix4); x++; }
    if (x>=xlen+xoff)
    {
        y+=2;
        x=xoff;
        ycount++;
    }
    if (ycount>ylen)
    {
        y=yoff+1;
        count++;
        ycount=0;
    }
}

void SetColour(int32_t index, const RGB *colour)
{
    pal[index][0] = (*colour)[0];
    pal[index][1] = (*colour)[1];
    pal[index][2] = (*colour)[2];
}

void SetRGB(int32_t index, int red, int green, int blue) {
    red = (red * 35.7);
    green = (green * 35.7);
    blue = (blue * 35.7);

    pal[index][0] = red;
    pal[index][1] = green;
    pal[index][2] = blue;
}


int DrawDOSImageFromData(uint8_t *ptr, size_t datasize)
{
    x=0;
    y=0;
    count=0;

    xlen=280;
    ylen=158;
    xoff=0; yoff=0;
    ycount=0;
    skipy=1;

    int work;
    int c;
    int i;
    int rawoffset;
    RGB black =   { 0,0,0 };
    RGB magenta = { 255,0,255 };
    RGB cyan =    { 0,255,255 };
    RGB white =   { 255,255,255 };

    /* set up the palette */
    SetColour(0,&black);
    SetColour(1,&cyan);
    SetColour(2,&magenta);
    SetColour(3,&white);

    if (ptr == NULL) {
        fprintf(stderr, "DrawMSDOSImageFromData: ptr == NULL\n");
        return 0;
    }

    uint8_t *origptr = ptr;

    // Get the size of the graphics chuck
    ptr = origptr + 0x05;
    work = *ptr++;
    size = work + (*ptr * 256);

    // Get whether it is lined
    ptr = origptr + 0x0d;
    work = *ptr++;
    if (work == 0xff) skipy=0;

    // Get the offset
    ptr = origptr + 0x0f;
    work = *ptr++;
    rawoffset = work + (*ptr * 256);
    xoff=((rawoffset % 80)*4)-24;
    yoff=rawoffset / 40;
    yoff -= (yoff % 2 == 1);
    x=xoff;
    y=yoff;

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
    while (ptr - origptr < size)
    {
        // First get count
        c = *ptr++;

        if ((c & 0x80) == 0x80)
        { // is a counter
            work = *ptr++;
            c &= 0x7f;
            for (i=0;i<c+1;i++)
            {
                DrawDOSPixels(work);
            }
        }
        else
        {
            // Don't count on the next j characters
            for (i=0;i<c+1;i++)
            {
                work = *ptr++;
                DrawDOSPixels(work);
            }
        }
        if (count>1) break;
    }
    return 1;
}
