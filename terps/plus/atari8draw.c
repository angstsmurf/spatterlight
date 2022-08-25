/* Routine to draw the Atari RLE graphics

 Code by David Lodge 29/04/2005
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "definitions.h"
#include "graphics.h"
#include "glk.h"

extern int x, y, count;
extern int xlen, ylen;
extern int xoff, yoff;
extern int size;

typedef uint8_t RGB[3];

extern winid_t Graphics;

void PutPixel(glsi32 x, glsi32 y, int32_t color);

void SetColour(int32_t index, const RGB *colour);

static const RGB black = { 0, 0, 0 };
static const RGB white = { 255, 255, 255 };

static const RGB cyan = { 0x9c, 0xe2, 0xc5 };
static const RGB lpurple = { 0xdf, 0xaa, 0xff };
static const RGB purple = { 0xdb, 0x47, 0xdd };
static const RGB deeppurple = { 0x73, 0x1c, 0x73 };
static const RGB dblue = { 0x30, 0x24, 0xff };

static const RGB green = { 0x08, 0x88, 0x17 };
static const RGB dgreen = { 0x4f, 0x74, 0x20 };
static const RGB darkergreen = { 0x08, 0x38, 0x00 };
static const RGB blue = { 0x36, 0x6e, 0xff };
static const RGB yellow = { 0xef, 0xf2, 0x58};
static const RGB orange = { 0xbf, 0x77, 0x30 };
static const RGB brown = { 0xab, 0x51, 0x1f };
static const RGB dbrown = { 0x73, 0x2c, 0x00 };
static const RGB lred = { 0xc2, 0x52, 0x57 };
static const RGB beige = { 0xff, 0x8f, 0x8f };
static const RGB dred = { 0xa2, 0x3f, 0x40 };
static const RGB grey = { 0x92, 0x92, 0x92 };
static const RGB lgrey = { 0xb4, 0xb5, 0xb4};
static const RGB lgreen = { 0x5f, 0x8f, 0x00 };
static const RGB tan = { 0xaf, 0x99, 0x3a };
static const RGB lilac = { 0x83, 0x58, 0xee };

static void TranslateAtariColour(int index, uint8_t value) {
    switch(value) {
        case 0:
        case 224:
            SetColour(index,&black);
            break;
        case 5:
        case 7:
            SetColour(index,&grey);
            break;
        case 6:
        case 8:
            SetColour(index,&blue);
            break;
        case 9:
        case 10:
        case 12:
            SetColour(index,&lgrey);
            break;
        case 14:
            SetColour(index,&white);
            break;
        case 17:
        case 32:
        case 36:
            SetColour(index,&dbrown);
            break;
        case 40:
            SetColour(index, &orange);
            break;
        case 54:
            if (CurrentGame == FANTASTIC4)
                SetColour(index, &orange);
            else
                SetColour(index,&dred);
            break;
        case 56:
            SetColour(index,&lred);
            break;
        case 58:
        case 60:
            SetColour(index,&beige);
            break;
        case 69:
        case 71:
            SetColour(index,&deeppurple);
            break;
        case 68:
        case 103:
            SetColour(index,&purple);
            break;
        case 89:
            SetColour(index,&lilac);
            break;
        case 85:
        case 101:
            SetColour(index,&dblue);
            break;
        case 110:
            SetColour(index,&lpurple);
            break;
        case 135:
            SetColour(index,&blue);
            break;
        case 174:
            SetColour(index,&cyan);
            break;
        case 182:
        case 196:
        case 214:
        case 198:
        case 200:
            SetColour(index,&green);
            break;
        case 194:
            SetColour(index,&darkergreen);
            break;
        case 212:
            SetColour(index,&dgreen);
            break;
        case 199:
        case 215:
        case 201:
            SetColour(index,&lgreen);
            break;
        case 230:
            SetColour(index,&tan);
            break;
        case 237:
            SetColour(index,&yellow);
            break;
        case 244:
            SetColour(index,&brown);
            break;
        case 246:
        case 248:
            SetColour(index,&orange);
            break;
        default:
            fprintf(stderr, "Unknown colour %d ", value);
            break;
    }
}

void SetColour(int32_t index, const RGB *colour);

void PrintFirstTenBytes(uint8_t *ptr, size_t offset);

void DrawC64Pixels(int pattern, int pattern2);

int DrawAtari8ImageFromData(uint8_t *ptr, size_t datasize)
{
    int work,work2;
    int c;
    int i;

    uint8_t *origptr = ptr;

    x = 0; y = 0;

    ptr += 2;

    work = *ptr++;
    size = work+(*ptr++ * 256);
    fprintf(stderr, "size: %d (%x)\n",size,size);

    // Get the offsets
    xoff = *ptr++ - 3;
    if (xoff < 0) {
        xoff = 0;
    }
    yoff = *ptr++;
    x = xoff * 8;
    y = yoff;
    fprintf(stderr, "xoff: %d yoff: %d\n",xoff,yoff);

    // Get the x length
    xlen = *ptr++;
    ylen = *ptr++;
    fprintf(stderr, "xlen: %d ylen: %d\n",xlen,ylen);

    glui32 curheight, curwidth;
    glk_window_get_size(Graphics, &curwidth, &curheight);
    fprintf(stderr, "Current graphwin height:%d pixel_size:%d ylen*pixsize:%d\n", curheight, pixel_size, ylen * pixel_size);

    if (CurrentGame == SPIDERMAN && ((ylen == 126 && xlen == 39) || (ylen == 158 && xlen == 38))) {
        ImageHeight = ylen + 2;
        ImageWidth = xlen * 8;
        if (ylen == 158)
            ImageWidth -= 24;
        else
            ImageWidth -= 16;
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

    // Get the palette
    debug_print("Colours: ");

    for (i = 1; i < 5; i++) {
        work=*ptr++;
        TranslateAtariColour(i, work);
    }

    debug_print("%d\n",work);

    while (ptr - origptr < datasize - 2)
    {
        // First get count
        c = *ptr++;

        if ((c & 0x80) == 0x80)
        { // is a counter
            c &= 0x7f;
            work=*ptr++;
            work2=*ptr++;
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
                work = *ptr++;
                work2 = *ptr++;
                DrawC64Pixels(work,work2);
            }
        }
    }
    return 1;
}
