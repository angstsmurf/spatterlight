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

static pixel_to_draw *pixels_to_draw = NULL;
static size_t draw_ops_capacity = 100;

static int total_draw_instructions = 0;
static int current_draw_instruction = 0;
int vector_image_shown = -1;

/* Pixels emitted per slow-draw tick. Pairs with the 20 ms timer interval
 * used by DrawHowarthVectorPicture — change in step with it. */
#define SCOTT_VECTOR_PIXELS_PER_TICK 50

/* TRUE once the background fill has been issued for the current image. Reset
 * in FreePixels() and whenever DrawSomeHowarthVectorPixels is asked to start
 * over. */
static int vector_background_painted = 0;

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
    vector_background_painted = 0;
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
        pixel_to_draw *new_pixels = MemRealloc(pixels_to_draw, draw_ops_capacity * sizeof(pixel_to_draw));
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
        pixel_to_draw *new_pixels = MemRealloc(pixels_to_draw, draw_ops_capacity * sizeof(pixel_to_draw));
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
        ensure_capacity();
        pixel_to_draw *todraw = &pixels_to_draw[total_draw_instructions++];
        todraw->x = x;
        todraw->y = y;
        todraw->colour = colour;
    }
}

int DrawingHowarthVector(void)
{
    return (total_draw_instructions > current_draw_instruction);
}

#ifndef SPATTERLIGHT
static int gli_slowdraw = 1;
#endif

void DrawSomeHowarthVectorPixels(int from_start)
{
    VectorState = DRAWING_VECTOR_IMAGE;
    if (from_start) {
        current_draw_instruction = 0;
        vector_background_painted = 0;
    }
    int i = current_draw_instruction;

    /* Paint the background once per image, before any pixels are plotted. */
    if (!vector_background_painted) {
        RectFill(0, 0, MYSTERIOUS_WIDTH, MYSTERIOUS_CLIPHEIGHT, Remap(bg_colour));
        vector_background_painted = 1;
    }

    /* Emit pixels until we run out or, in slow-draw mode, hit the chunk limit
     * that yields back to the timer loop. */
    int chunk_end = i + SCOTT_VECTOR_PIXELS_PER_TICK;
    for (; i < total_draw_instructions && (!gli_slowdraw || i < chunk_end); i++) {
        const pixel_to_draw *todraw = &pixels_to_draw[i];
        PutPixel(todraw->x, todraw->y, Remap(todraw->colour));
    }
    current_draw_instruction = i;

    /* All instructions consumed: stop the timer, transition to "showing",
     * and release the instruction buffer. */
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
    /*
     * Byte-exact reimplementation of the original Digital Fantasia /
     * Mysterious Adventures ROM line routine (Golden Baton ram:0x6c06..0x6ca3,
     * reverse-engineered from the ZX Spectrum snapshot in Ghidra). It is a
     * centred Bresenham distinct from the generic Level9-derived one this file
     * used to carry:
     *
     *   - error is initialised to major/2 (round-to-nearest), not 2*minor-major;
     *   - the major axis steps every iteration and the loop runs exactly
     *     `major` times, plotting the NEW position each step, so the START
     *     point is NOT plotted (the previous segment's end already covers it)
     *     and the END point IS;
     *   - a zero-length segment (dx == dy == 0) plots nothing;
     *   - ties (dx == dy) take the x-major branch.
     *
     * The original works in raw coordinates (y up) and plots at screen row
     * 191-y; the caller here has already converted y to screen space, and the
     * iteration is invariant under that negation, so the placement matches
     * pixel-for-pixel. Verified byte-identical to the ROM over Golden Baton's
     * room 0 (1740/1740 line pixels land on the real Spectrum bitmap).
     */
    int dx, dy, sx, sy, x, y, acc, i, major, minor, step;

    if (x2 >= x1) { sx = 1;  dx = x2 - x1; } else { sx = -1; dx = x1 - x2; }
    if (y2 >= y1) { sy = 1;  dy = y2 - y1; } else { sy = -1; dy = y1 - y2; }

    x = x1;
    y = y1;

    if (dx >= dy) {
        if (dx == 0)            /* dx == dy == 0: degenerate, plot nothing */
            return;
        major = dx; minor = dy;
        acc = major >> 1;
        for (i = 0; i < major; i++) {
            acc += minor;
            step = 0;
            if (acc >= major) { acc -= major; step = sy; }
            x += sx;
            y += step;
            scott_linegraphics_plot_clip(x, y, colour);
        }
    } else {
        major = dy; minor = dx;
        acc = major >> 1;
        for (i = 0; i < major; i++) {
            acc += minor;
            step = 0;
            if (acc >= major) { acc -= major; step = sx; }
            y += sy;
            x += step;
            scott_linegraphics_plot_clip(x, y, colour);
        }
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
        /* Match the original ROM fill's interior clamp (Golden Baton
         * ram:0x6cee/0x6cf9/0x6d04/0x6d0f): it never grows into the top screen
         * row or the extreme edge columns, so the picture border acts as an
         * implicit wall. The ROM-exact line rasterizer plots no pixel in row 0,
         * so without this clamp a 4-connected flood escapes along the top edge
         * and floods the whole image (the previous Level9 Bresenham only stayed
         * contained because its ~1px-wide misplacement happened to seal row 0). */
        if (x >= 1 && x < MYSTERIOUS_WIDTH - 1 && y >= 1 && y < MYSTERIOUS_CLIPHEIGHT && linegraphics_get_pixel(x, y) == bg_colour) {
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
    pixels_to_draw = MemAlloc(draw_ops_capacity * sizeof(pixel_to_draw));

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
