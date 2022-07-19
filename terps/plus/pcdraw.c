/* Routine to draw the PC graphics from Claymorgue

 Code by David Lodge 29/04/2005
 */
#include <stdio.h>
#include <stdlib.h>

#include "glk.h"

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

static void PutPixel(glsi32 x, glsi32 y, int32_t color)
{
    glui32 glk_color = ((pal[color][0] << 16)) | ((pal[color][1] << 8)) | (pal[color][2]);

    glsi32 xpos = x * pixel_size + x_offset;

    if (xpos < x_offset || xpos >= right_margin) {
        return;
    }

    glk_window_fill_rect(Graphics, glk_color, xpos,
                         y * pixel_size + y_offset, pixel_size, pixel_size);
}


static void DrawPixels(int pattern)
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

static void set_color(int32_t index, RGB *colour)
{
    pal[index][0] = (*colour)[0];
    pal[index][1] = (*colour)[1];
    pal[index][2] = (*colour)[2];
}


int DrawImageFromFile(char *filename)
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
    // RGB blue =    { 0,0,255 };
    // RGB red =     { 255,0,0 };
    RGB magenta = { 255,0,255 };
    // RGB green =   { 0,255,0 };
    RGB cyan =    { 0,255,255 };
    // RGB yellow =  { 255,255,0 };
    RGB white =   { 255,255,255 };

    /* set up the palette */
    set_color(0,&black);
    set_color(1,&cyan);
    set_color(2,&magenta);
    set_color(3,&white);
    //   get_palette(pal);

    infile=fopen(filename,"rb");

    if (infile == NULL) {
        fprintf(stderr, "DrawImageFromFile: Could not open file named \"%s\"\n", filename);
        return 0;
    }

    // Get the size of the graphics chuck
    fseek(infile, 0x05, SEEK_SET);
    work=fgetc(infile);
    size=work+(fgetc(infile)*256);

    // Get whether it is lined
    fseek(infile,0x0d,SEEK_SET);
    work=fgetc(infile);
    if (work == 0xff) skipy=0;

    // Get the offset
    fseek(infile,0x0f,SEEK_SET);
    work=fgetc(infile);
    rawoffset=work+fgetc(infile)*256;
    xoff=((rawoffset % 80)*4)-24;
    yoff=rawoffset / 40;
    yoff -= (yoff % 2 == 1);
    x=xoff;
    y=yoff;

    // Get the y length
    fseek(infile,0x11,SEEK_SET);
    work=fgetc(infile);
    ylen=work+(fgetc(infile)*256);
    ylen-=rawoffset;
    ylen/=80;

    // Get the x length
    fseek(infile,0x13,SEEK_SET);
    xlen=fgetc(infile)*4;

    fseek(infile,0x17,SEEK_SET);
    while (!feof(infile) && ftell(infile)<size)
    {
        // First get count
        c=fgetc(infile);

        if ((c & 0x80) == 0x80)
        { // is a counter
            work=fgetc(infile);
            c &= 0x7f;
            for (i=0;i<c+1;i++)
            {
                DrawPixels(work);
            }
        }
        else
        {
            // Don't count on the next j characters
            for (i=0;i<c+1;i++)
            {
                work=fgetc(infile);
                DrawPixels(work);
            }
        }
        if (count>1) break;
    }
    fclose(infile);
    return 1;
}
