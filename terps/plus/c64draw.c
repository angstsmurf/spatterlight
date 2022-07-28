/* Routine to draw the C64 RLE graphics

 Code by David Lodge 29/04/2005
 */
#include <stdio.h>
#include <stdlib.h>

#include "definitions.h"
#include "glk.h"

extern int x, y, count;
extern int xlen, ylen;
extern int xoff, yoff;
extern int size;

typedef uint8_t RGB[3];

typedef RGB PALETTE[16];

extern PALETTE pal;

RGB wibble;
int countflag;

extern winid_t Graphics;

extern int pixel_size;
extern int x_offset, y_offset, right_margin;

static void PutPixel(glsi32 x, glsi32 y, int32_t color)
{
    glui32 glk_color = ((pal[color][0] << 16)) | ((pal[color][1] << 8)) | (pal[color][2]);

//    fprintf(stderr, "PutPixel: %d, %d, color %x\n", x, y, glk_color);

    glsi32 xpos = x * pixel_size + x_offset;

    if (xpos < x_offset || xpos >= right_margin) {
        return;
    }

    glk_window_fill_rect(Graphics, glk_color, xpos,
                         y * pixel_size + y_offset, pixel_size, pixel_size);
}

void drawpixels(int pattern, int pattern2)
{
    int pix1,pix2,pix3,pix4;

//    fprintf(stderr, "Plotting at %d %d: %x %x\n",x,y,pattern,pattern2);
    // Now get colours
    if (x>(xlen - 3)*8)
      return;
    pix1=(pattern & 0xc0)>>6;
    pix2=(pattern & 0x30)>>4;
    pix3=(pattern & 0x0c)>>2;
    pix4=(pattern & 0x03);

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
    x-=8;

    if (y>ylen)
    {
        x+=8;
        y=yoff;
    }
}

static void set_color(int32_t index, RGB *colour)
{
    pal[index][0] = (*colour)[0];
    pal[index][1] = (*colour)[1];
    pal[index][2] = (*colour)[2];
}

int DrawC64ImageFromData(uint8_t *ptr, size_t datasize)
{
    int work,work2;
    int c;
    int i,j;
    RGB black = { 0, 0, 0 };
    RGB white = { 255, 255, 255 };
    RGB red = { 191, 97, 72 };
    RGB cyan = { 153, 230, 249 };
    RGB purple = { 177, 89, 185 };
    RGB green = { 121, 213, 112 };
    RGB blue = { 95, 72, 233 };
    RGB yellow = { 247, 255, 108 };
    RGB orange = { 186, 134, 32 };
    RGB brown = { 131, 112, 0 };
    RGB lred = { 231, 154, 132 };
    RGB dgrey = { 69, 69, 69 };
    RGB grey = { 167, 167, 167 };
    RGB lgreen = { 192, 255, 185 };
    RGB lblue = { 162, 143, 255 };
    RGB lgrey = { 200, 200, 200 };

    uint8_t *origptr = ptr;

    x=0;y=0;

    ptr += 2;
    
    work = *ptr++;
    size = work+(*ptr++ * 256);

    // Get the offset
    xoff = *ptr++ - 3;
    if (xoff < 0) xoff = 8;
    yoff = *ptr++;
    x = xoff * 8;
    y = yoff;

    // Get the x length
    xlen = *ptr++;
    ylen = *ptr++;

    set_color(0,&black);

    // Get the palette
    // This is just ad-hoc from observation, I have no idea
    // how this is actually supposed to work
    debug_print("Colours: ");
    for (i=1;i<5;i++)
    {
        work=*ptr++;
        switch(work) {
            case 0:
                set_color(i,&purple);
                break;
            case 1:
                set_color(i,&blue);
                break;
            case 3:
                set_color(i,&white);
                break;
            case 7:
            case 8:
            case 9:
                set_color(i,&blue);
                break;
            case 10:
            case 12:
            case 14:
            case 15:
                set_color(i,&white);
                break;
            case 17:
                set_color(i,&green);
                break;
            case 36:
            case 40:
                set_color(i,&brown);
                break;
            case 46:
                set_color(i,&yellow);
                break;
            case 52:
            case 54:
            case 56:
            case 60:
                set_color(i,&orange);
                break;
            case 68:
            case 69:
            case 71:
                set_color(i,&red);
                break;
            case 77:
            case 85:
            case 87:
                set_color(i,&purple);
                break;
            case 89:
                set_color(i,&lred);
                break;
            case 101:
            case 103:
                set_color(i,&purple);
                break;
            case 110:
                set_color(i,&lblue);
                break;
            case 135:
            case 137:
                set_color(i,&blue);
                break;
            case 157:
                set_color(i,&lblue);
                break;
            case 161:
                set_color(i,&grey);
                break;
            case 179:
            case 182:
            case 194:
            case 196:
            case 198:
            case 199:
            case 200:
                set_color(i,&green);
                break;
            case 201:
                set_color(i,&lgreen);
                break;
            case 212:
            case 214:
            case 215:
                set_color(i,&green);
                break;
            case 224:
                set_color(i,&purple);
                break;
            case 230:
            case 237:
                set_color(i,&yellow);
                break;
            case 244:
            case 246:
            case 248:
                set_color(i,&brown);
                break;
            case 252:
                set_color(i,&yellow);
                break;
            default:
                fprintf(stderr, "Unknown colour %d ", work);
                break;
        }
        debug_print("%d ", work);
    }
    debug_print("\n");

    j=0;
    while (ptr - origptr < datasize - 3)
    {
        // First get count
        c=*ptr++;

        if (((c & 0x80) == 0x80) || countflag)
        { // is a counter
            if (!countflag) c &= 0x7f;
            if (countflag) c-=1;
            work=*ptr++;
            work2=*ptr++;
            for (i=0;i<c+1;i++)
            {
                drawpixels(work,work2);
            }
        }
        else
        {
            // Don't count on the next j characters

            for (i=0;i<c+1 && ptr - origptr < datasize - 1;i++)
            {
                work=*ptr++;
                work2=*ptr++;
                drawpixels(work,work2);
            }
        }
    }
    return 1;
}
