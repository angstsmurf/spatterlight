/*  This file is based on code from the Level9 interpreter by Simon Baldwin
 */

#include <string.h>
#include <stdlib.h>
#include "scott.h"
#include "sagadraw.h"
#include "line_drawing.h"

struct line_image *LineImages;

uint8_t *picture_bitmap = NULL;

int line_colour = 15;
int bg_colour = 0;

int scott_graphics_width = 255;
int scott_graphics_height = 94;

/*
 * scott_linegraphics_plot_clip()
 * scott_linegraphics_draw_line_if()
 *
 * Draw a line from x1,y1 to x2,y2 in colour1, where the existing pixel
 * colour is colour2.  The function uses Bresenham's algorithm.  The second
 * function, scott_graphics_plot_clip, is a line drawing helper; it handles
 * clipping, and the requirement to plot a point only if it matches colour2.
 */
static void
scott_linegraphics_plot_clip (int x, int y, int colour)
{
    y = 190 - y;

    /*
     * Clip the plot if the value is outside the context.  Otherwise, plot the
     * pixel as colour1 if it is currently colour2.
     */
    if (x >= 0 && x <= scott_graphics_width && y >= 0 && y < scott_graphics_height)
    {
        picture_bitmap[y * 255 + x] = colour;
        PutPixel(x, y, colour);
    }
}

static void
scott_linegraphics_draw_line(int x1, int y1, int x2, int y2,
                             int colour)
{
    int x, y, dx, dy, incx, incy, balance;

    /* Normalize the line into deltas and increments. */
    if (x2 >= x1)
    {
        dx = x2 - x1;
        incx = 1;
    }
    else
    {
        dx = x1 - x2;
        incx = -1;
    }

    if (y2 >= y1)
    {
        dy = y2 - y1;
        incy = 1;
    }
    else
    {
        dy = y1 - y2;
        incy = -1;
    }

    /* Start at x1,y1. */
    x = x1;
    y = y1;

    /* Decide on a direction to progress in. */
    if (dx >= dy)
    {
        dy <<= 1;
        balance = dy - dx;
        dx <<= 1;

        /* Loop until we reach the end point of the line. */
        while (x != x2)
        {
            scott_linegraphics_plot_clip(x, y, colour);
            if (balance >= 0)
            {
                y += incy;
                balance -= dx;
            }
            balance += dy;
            x += incx;
        }
        scott_linegraphics_plot_clip (x, y, colour);
    }
    else
    {
        dx <<= 1;
        balance = dx - dy;
        dy <<= 1;

        /* Loop until we reach the end point of the line. */
        while (y != y2)
        {
            scott_linegraphics_plot_clip (x, y, colour);
            if (balance >= 0)
            {
                x += incx;
                balance -= dy;
            }
            balance += dx;
            y += incy;
        }
        scott_linegraphics_plot_clip (x, y, colour);
    }
}

int scott_linegraphics_fill_segments_length = 0,
scott_linegraphics_fill_segments_allocation = 0;

/*
 * Structure of a Seed Fill segment entry, and a growable stack-based array
 * of segments pending fill.  When length exceeds size, size is increased
 * and the array grown.
 */
typedef struct
{
    int y;   /* Segment y coordinate */
    int xl;  /* Segment x left hand side coordinate */
    int xr;  /* Segment x right hand side coordinate */
    int dy;  /* Segment y delta */
} scott_linegraphics_segment_t;

static scott_linegraphics_segment_t *scott_linegraphics_fill_segments = NULL;

/*
 * scott_linegraphics_push_fill_segment()
 * scott_linegraphics_pop_fill_segment()
 * scott_linegraphics_fill_4way()
 *
 * Area fill algorithm, set a region to colour.  This function is a derivation of Paul Heckbert's Seed Fill,
 * from "Graphics Gems", Academic Press, 1990, which fills 4-connected
 * neighbors.
 *
 * The main modification is to make segment stacks growable, through the
 * helper push and pop functions.
 */
static void
scott_linegraphics_push_fill_segment (int y, int xl, int xr, int dy)
{
    /* Clip points outside the graphics context. */
    if (!(y + dy < 0 || y + dy >= scott_graphics_height))
    {
        int length, allocation;

        length = ++scott_linegraphics_fill_segments_length;
        allocation = scott_linegraphics_fill_segments_allocation;

        /* Grow the segments stack if required, successively doubling. */
        if (length > allocation)
        {
            size_t bytes;

            allocation = allocation == 0 ? 1 : allocation << 1;

            bytes = allocation * sizeof (*scott_linegraphics_fill_segments);
            scott_linegraphics_fill_segments =
            realloc(scott_linegraphics_fill_segments, bytes);
        }

        /* Push top of segments stack. */
        scott_linegraphics_fill_segments[length - 1].y  = y;
        scott_linegraphics_fill_segments[length - 1].xl = xl;
        scott_linegraphics_fill_segments[length - 1].xr = xr;
        scott_linegraphics_fill_segments[length - 1].dy = dy;

        /* Write back local dimensions copies. */
        scott_linegraphics_fill_segments_length = length;
        scott_linegraphics_fill_segments_allocation = allocation;
    }
}

static void
scott_linegraphics_pop_fill_segment (int *y, int *xl, int *xr, int *dy)
{
    int length;
    if (scott_linegraphics_fill_segments_length <= 0)
        return;

    length = --scott_linegraphics_fill_segments_length;

    /* Pop top of segments stack. */
    *y  = scott_linegraphics_fill_segments[length].y;
    *xl = scott_linegraphics_fill_segments[length].xl;
    *xr = scott_linegraphics_fill_segments[length].xr;
    *dy = scott_linegraphics_fill_segments[length].dy;
}

static int linegraphics_get_pixel(int x, int y) {
    return picture_bitmap[y * 255 + x];
}

static void
scott_linegraphics_fill_4way(int x, int y, int colour)
{

    y = 190 - y;

//    int previous = linegraphics_get_pixel(x, y);
//
//    if (previous != bg_colour)
//        return;

    /* Clip fill requests to visible graphics region. */
    if (x >= 0 && x < scott_graphics_width && y >= 0 && y < scott_graphics_height)
    {
        int left, x1, x2, dy, x_lo, x_hi;

        /*
         * Set up inclusive window dimension to ease algorithm translation.
         * The original worked with inclusive rectangle limits.
         */
        x_lo = 0;
        x_hi = scott_graphics_width - 1;

        /*
         * The first of these is "needed in some cases", the second is the seed
         * segment, popped first.
         */
        scott_linegraphics_push_fill_segment(y, x, x, 1);
        scott_linegraphics_push_fill_segment(y + 1, x, x, -1);

        while (scott_linegraphics_fill_segments_length > 0)
        {
            /* Pop segment off stack and add delta to y coord. */
            scott_linegraphics_pop_fill_segment(&y, &x1, &x2, &dy);
            y += dy;

            /*
             * Segment of scan line y-dy for x1<=x<=x2 was previously filled,
             * now explore adjacent pixels in scan line y.
             */
            for (x = x1;
                 x >= x_lo && linegraphics_get_pixel(x, y) == bg_colour;
                 x--)
            {
                PutPixel(x, y, colour);
                picture_bitmap[y * 255 + x] = colour;
            }

            if (x >= x1)
                goto skip;

            left = x + 1;
            if (left < x1)
            {
                /* Leak on left? */
                scott_linegraphics_push_fill_segment(y, left, x1 - 1, -dy);
            }

            x = x1 + 1;
            do
            {
                for (;
                     x <= x_hi && linegraphics_get_pixel(x, y) == bg_colour;
                     x++)
                {
                    PutPixel(x, y, colour);
                    picture_bitmap[y * 255 + x] = colour;
                }

                scott_linegraphics_push_fill_segment(y, left, x - 1, dy);

                if (x > x2 + 1)
                {
                    /* Leak on right? */
                    scott_linegraphics_push_fill_segment(y, x2 + 1, x - 1, -dy);
                }
            skip:
                for (x++;
                     x <= x2 && linegraphics_get_pixel(x, y) != bg_colour;
                     x++)
                    ;

                left = x;
            }
            while (x <= x2);
        }
    }
}

extern int pixel_size;
extern int x_offset;

void DrawLinePicture(int image) {
    palchosen = GameInfo->palette;
    DefinePalette();
    picture_bitmap = MemAlloc(255 * 97);
    bg_colour = LineImages[image].bgcolour;
    memset(picture_bitmap, bg_colour, 255 * 97);
    if (bg_colour == 0)
        line_colour = 7;
    else
        line_colour = 0;
    RectFill(0, 0, scott_graphics_width, scott_graphics_height, bg_colour);
    int x = 0, y = 0;
    uint8_t arg1, arg2, arg3;
    uint8_t *p = LineImages[image].data;
    while (p - LineImages[image].data < LineImages[image].size) {
        uint8_t opcode = *(p++);
        switch(opcode) {
            case 0xc0:
                y = *(p++);
                x = *(p++);
                break;
            case 0xc1:
                arg1 = *(p++);
                arg2 = *(p++);
                arg3 = *(p++);
                scott_linegraphics_fill_4way(arg3, arg2, arg1);
                break;
            case 0xff:
                return;
            default:
                arg1 = *(p++);
                scott_linegraphics_draw_line(x, y, arg1, opcode, line_colour);
                x = arg1; y = opcode;
                break;
        }
    }
    free(picture_bitmap);
}
