//  line_drawing.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  This file is based on code from the Level9 interpreter by Simon Baldwin

#include <stdlib.h>
#include <string.h>

#include "line_drawing.h"
#include "ringbuffer.h"
#include "sagadraw.h"
#include "sagagraphics.h"
#include "scott.h"
#include "vector_common.h"

#define OPCODE_MOVE_TO  0xc0
#define OPCODE_FILL     0xc1
#define OPCODE_END      0xff

#define DRAW_HEIGHT_OFFSET 191

#define MYSTERIOUS_WIDTH 255
#define MYSTERIOUS_HEIGHT 97
#define MYSTERIOUS_CLIPHEIGHT 95

line_image *LineImages;

typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t colour;
} pixel_to_draw;

static pixel_to_draw **pixels_to_draw = NULL;
static size_t draw_ops_capacity = 100;

static int total_draw_instructions = 0;
static int current_draw_instruction = 0;
int vector_image_shown = -1;

static uint8_t *picture_bitmap = NULL;

static int line_colour = 15;
static int bg_colour = 0;

/*
 * scott_linegraphics_plot_clip()
 * scott_linegraphics_draw_line()
 *
 * Draw a line from x1,y1 to x2,y2 in colour line_colour.
 * The function uses Bresenham's algorithm.
 * The second function, scott_graphics_plot_clip, is a line drawing helper;
 * it handles clipping.
 */

void FreePixels(void)
{
    for (int i = 0; i < total_draw_instructions; i++)
        if (pixels_to_draw[i] != NULL)
            free(pixels_to_draw[i]);
    free(pixels_to_draw);
    pixels_to_draw = NULL;
    draw_ops_capacity = 100;
}

static void ensure_capacity(void) {
    // Ensure pixels_to_draw has room for all draw instructions.
    // This could theoretically be more than the total amount of
    // pixels in the bitmap, as lines may be overlapping.
    if (total_draw_instructions >= draw_ops_capacity) {
        draw_ops_capacity *= 2;  // Double the capacity
        pixel_to_draw **new_pixels = MemRealloc(pixels_to_draw, draw_ops_capacity * sizeof(pixel_to_draw *));
        pixels_to_draw = new_pixels;
    }
}

static void shrink_capacity(void) {
    // When the image has finished drawing all its instructions,
    // we might have allocated a lot more memory than we need,
    // so free any excess space.
    if (pixels_to_draw != NULL && draw_ops_capacity > total_draw_instructions) {
        if (total_draw_instructions == 0) {
            FreePixels();
            return;
        }
        draw_ops_capacity = total_draw_instructions;
        // Our wrapper of realloc() will exit() on failure,
        // so no need to check the result here.
        pixel_to_draw **new_pixels = MemRealloc(pixels_to_draw, draw_ops_capacity * sizeof(pixel_to_draw *));
        pixels_to_draw = new_pixels;
    }
}

static void
scott_linegraphics_plot_clip(int x, int y, int colour)
{
    /*
     * Clip the plot if the value is outside the context.  Otherwise, plot the
     * pixel as colour.
     */
    if (x >= 0 && x <= MYSTERIOUS_WIDTH && y >= 0 && y < MYSTERIOUS_CLIPHEIGHT) {
        picture_bitmap[y * MYSTERIOUS_WIDTH + x] = colour;
        pixel_to_draw *todraw = MemAlloc(sizeof(pixel_to_draw));
        todraw->x = x;
        todraw->y = y;
        todraw->colour = colour;
        ensure_capacity();
        pixels_to_draw[total_draw_instructions++] = todraw;
    }
}

int DrawingHowarthVector(void)
{
    return (total_draw_instructions > current_draw_instruction);
}

#ifndef SPATTERLIGHT
static int gli_slowdraw = 0;
#endif

void DrawSomeHowarthVectorPixels(int from_start)
{
    VectorState = DRAWING_VECTOR_IMAGE;
    int i = current_draw_instruction;
    if (from_start)
        i = 0;

    // Draw background if this is the first instruction
    if (i == 0)
        RectFill(0, 0, MYSTERIOUS_WIDTH, MYSTERIOUS_CLIPHEIGHT, Remap(bg_colour));

    for (; i < total_draw_instructions && (!gli_slowdraw || i < current_draw_instruction + 50); i++) {
        pixel_to_draw todraw = *pixels_to_draw[i];
        PutPixel(todraw.x, todraw.y, Remap(todraw.colour));
    }
    current_draw_instruction = i;
    if (current_draw_instruction >= total_draw_instructions) {
        glk_request_timer_events(0);
        VectorState = SHOWING_VECTOR_IMAGE;
        FreePixels();
    }
}

static void
scott_linegraphics_draw_line(int x1, int y1, int x2, int y2,
    int colour)
{
    int x, y, dx, dy, incx, incy, balance;

    /* Normalize the line into deltas and increments. */
    if (x2 >= x1) {
        dx = x2 - x1;
        incx = 1;
    } else {
        dx = x1 - x2;
        incx = -1;
    }

    if (y2 >= y1) {
        dy = y2 - y1;
        incy = 1;
    } else {
        dy = y1 - y2;
        incy = -1;
    }

    /* Start at x1,y1. */
    x = x1;
    y = y1;

    /* Decide on a direction to progress in. */
    if (dx >= dy) {
        dy *= 2;
        balance = dy - dx;
        dx *= 2;

        /* Loop until we reach the end point of the line. */
        while (x != x2) {
            scott_linegraphics_plot_clip(x, y, colour);
            if (balance >= 0) {
                y += incy;
                balance -= dx;
            }
            balance += dy;
            x += incx;
        }
        scott_linegraphics_plot_clip(x, y, colour);
    } else {
        dx *= 2;
        balance = dx - dy;
        dy *= 2;

        /* Loop until we reach the end point of the line. */
        while (y != y2) {
            scott_linegraphics_plot_clip(x, y, colour);
            if (balance >= 0) {
                x += incx;
                balance -= dy;
            }
            balance += dx;
            y += incy;
        }
        scott_linegraphics_plot_clip(x, y, colour);
    }
}

static int linegraphics_get_pixel(int x, int y)
{
    return picture_bitmap[y * MYSTERIOUS_WIDTH + x];
}

static void diamond_fill(uint8_t x, uint8_t y, int colour)
{
    uint8_t buffer[2048];
    cbuf_handle_t ringbuf = circular_buf_init(buffer, 2048);
    circular_buf_putXY(ringbuf, x, y);
    while (!circular_buf_empty(ringbuf)) {
        circular_buf_getXY(ringbuf, &x, &y);
        if (x >= 0 && x < MYSTERIOUS_WIDTH && y >= 0 && y < MYSTERIOUS_CLIPHEIGHT && linegraphics_get_pixel(x, y) == bg_colour) {
            scott_linegraphics_plot_clip(x, y, colour);
            circular_buf_putXY(ringbuf, x, y + 1);
            circular_buf_putXY(ringbuf, x, y - 1);
            circular_buf_putXY(ringbuf, x + 1, y);
            circular_buf_putXY(ringbuf, x - 1, y);
        }
    }
    circular_buf_free(ringbuf);
}

static inline int ConvertY(int y) { return DRAW_HEIGHT_OFFSET - y; }

void DrawHowarthVectorPicture(int image)
{
    if (image < 0) {
        return;
    }

    // If this image is already shown:
    if (vector_image_shown == image && pixels_to_draw) {
        if (VectorState == SHOWING_VECTOR_IMAGE) {
            return;
        } else {
            if (gli_slowdraw)
                glk_request_timer_events(20);
            DrawSomeHowarthVectorPixels(1);
            return;
        }
    }

    glk_request_timer_events(0);
    vector_image_shown = image;

    // Free previous pixels if necessary
    if (pixels_to_draw != NULL)
        FreePixels();

    // Start with a small allocation for pixels_to_draw.
    // scott_linegraphics_plot_clip() will grow this as needed.
    draw_ops_capacity = 100;  // Initial capacity 100 instructions.
    pixels_to_draw = MemAlloc(draw_ops_capacity * sizeof(pixel_to_draw *));

    total_draw_instructions = 0;
    current_draw_instruction = 0;

    if (palchosen == NO_PALETTE) {
        palchosen = Game->palette;
        DefinePalette();
    }
    picture_bitmap = MemAlloc(MYSTERIOUS_WIDTH * MYSTERIOUS_HEIGHT);

    bg_colour = LineImages[image].bgcolour;
    memset(picture_bitmap, bg_colour, MYSTERIOUS_WIDTH * MYSTERIOUS_HEIGHT);

    if (bg_colour == 0)
        line_colour = 7;
    else
        line_colour = 0;

    int x = 0, y = 0, y2 = 0;
    uint8_t arg1, arg2, arg3;
    uint8_t *p = LineImages[image].data;
    uint8_t opcode = 0;

    while ((size_t)(p - LineImages[image].data) < LineImages[image].size && opcode != OPCODE_END) {
        if (p > entire_file + file_length) {
            fprintf(stderr, "Out of range! Opcode: %x. Image: %d. LineImages[%d].size: %zu\n", opcode, image, image, LineImages[image].size);
            break;
        }
        opcode = *(p++);
        switch (opcode) {
            case OPCODE_MOVE_TO:
                y = ConvertY(*(p++));
                x = *(p++);
                break;
            case OPCODE_FILL:
                arg1 = *(p++);
                arg2 = *(p++);
                arg3 = *(p++);
                diamond_fill(arg3, ConvertY(arg2), arg1);
                break;
            case OPCODE_END:
                break;
            default: // draw line
                arg1 = *(p++);
                y2 = ConvertY(opcode);
                scott_linegraphics_draw_line(x, y, arg1, y2, line_colour);
                x = arg1;
                y = y2;
                break;
        }
    }

    // Free bitmap memory
    if (picture_bitmap != NULL) {
        free(picture_bitmap);
        picture_bitmap = NULL;
    }

    shrink_capacity();

    // Either draw immediately or use a timer for slow, "animated" drawing
    if (gli_slowdraw)
        glk_request_timer_events(20);
    else
        DrawSomeHowarthVectorPixels(1);
}
