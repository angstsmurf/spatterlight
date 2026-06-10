//
//  atari_8bit_vector_draw.c
//  scott
//
//  Created by Administrator on 2026-01-13.
//
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "apple2draw.h"
#include "common_file_utils.h"
#include "scott.h"
#include "vector_common.h"

#include "atari_8bit_vector_draw.h"

#define A8_IMAGE_SIZE 0xf00
#define A8_SCREEN_COLUMNS 40
#define A8_SCREEN_WIDTH 160
#define A8_SCREEN_HEIGHT 95

typedef struct {
    uint8_t *imagedata;
    size_t datasize;

    uint16_t screen_offset; // screen memory offset in bytes. pixel_offset / 4.
    // (there are four pixels in every screen memory byte, as a pixel uses two bits)
    int pixel_offset; // screen memory offset in pixels. screen_offset * 4 + pixel_remainder
    uint8_t pixel_remainder; // pixel offset within the current byte (0..3). pixel_offset % 4.

    uint8_t color_a_index; // color A index used for pattern lookup
    uint8_t color_b_index; // color B index used for pattern lookup

    // Arrays for creating color patterns and preserving them through opcode calls
    uint8_t pattern_color[4];
    uint8_t pattern_byte[4];
    uint8_t saved_pattern[4];

    uint16_t pattern_start_pixel; // start offset in pixels for pattern fill

    uint8_t plane90_pattern_even; // pattern/mask byte used for writing to even lines of plane $9000
    uint8_t planeA0_pattern_even; // pattern/mask byte used for writing to even lines of plane  $A000
    uint8_t plane90_pattern_odd; // pattern/mask byte used for writing to odd lines of plane $9000
    uint8_t planeA0_pattern_odd; // pattern/mask byte used for writing to odd lines of plane $A000

    uint8_t last_plane90_byte; // cached expected byte value for fast-write path on plane $9000
    uint8_t last_planeA0_byte; // cached expected byte value for fast-write path on plane $A000

    uint8_t color_idx; // Current draw color
    uint8_t fill_background_color; // Any pixels of this color inside a shape will be replaced with the fill color/pattern by the flood fill routine.
    uint8_t fill_edge_color; // This is the color of "contours" that delimit the shapes that the flood fill routine will fill. The flood fill will never change this color.
    bool erase_enabled; // flag: enable erase mode in flood fill, which means that anything that is not the edge color will be replaced by the fill color. Otherwise only the background color is replaced.
    bool lines_only_mode; // A special mode used to redraw the image lines on top in a second pass.

    uint8_t scan_saved_xpos; // saved x position used while scanning the fill
    uint16_t scan_saved_pixoff; // saved pixel position used while scanning the fill

    uint8_t start_x; // previously stored X position (for line drawing)
    uint8_t start_y; // previously stored Y position (for line drawing)
    uint8_t target_x; // current drawing target X coordinate
    uint8_t target_y; // current drawing target Y coordinate
    uint8_t ypos; // current y coordinate
    uint8_t xpos; // current x coordinate
} a8_draw_ctx;

static uint8_t *a8_screenmem90 = NULL;
static uint8_t *a8_screenmemA0 = NULL;

// This is always accessed at even offsets,
// reading a color pair (0..3).
static const uint8_t color_array[32] = {
    0x03, 0x03,
    0x03, 0x02,
    0x03, 0x01,
    0x03, 0x00,
    0x02, 0x02,
    0x02, 0x01,
    0x02, 0x00,
    0x01, 0x01,
    0x01, 0x00,
    0x00, 0x00,
    0x02, 0x03,
    0x01, 0x03,
    0x00, 0x03,
    0x01, 0x02,
    0x00, 0x02,
    0x00, 0x01
};

typedef struct {
    uint16_t offset;
    uint8_t value90;
    uint8_t valueA0;
    bool fill_bg;
    int delay;
} a8_byte_to_write;

static a8_byte_to_write *bytes_to_write = NULL;
static size_t write_ops_capacity = 100;

static size_t total_write_ops = 0;
static size_t current_write_op = 0;

/* Bytes emitted per slow-draw tick. Pairs with the graphics timer interval —
 * change in step with it. */
#define ATARI8_VECTOR_BYTES_PER_TICK 50


static inline bool is_valid_screen_offset(uint16_t offset) {
    return offset < A8_IMAGE_SIZE;
}

static void ensure_capacity(void) {
    // Ensure bytes_to_write has room for all write ops.
    // This might be more than the total amount of bytes
    // in screenmem, as many ops may write to the same offset
    if (total_write_ops >= write_ops_capacity) {
        write_ops_capacity = MAX(write_ops_capacity * 2, total_write_ops + 1);  // Double the capacity
        a8_byte_to_write *new_ops = MemRealloc(bytes_to_write, write_ops_capacity * sizeof(a8_byte_to_write));
        bytes_to_write = new_ops;
    }
}

static void FreeOps(void)
{
    if (bytes_to_write == NULL)
        return;
    free(bytes_to_write);
    bytes_to_write = NULL;
    write_ops_capacity = 100;
}

static void shrink_capacity(void) {
    // When the image has finished writing all its ops,
    // we might have allocated a lot more memory than we need,
    // so we free any excess space.
    if (bytes_to_write != NULL && write_ops_capacity > total_write_ops) {
        if (total_write_ops == 0) {
            FreeOps();
            return;
        }
        write_ops_capacity = total_write_ops;
        // Our wrapper of realloc() will exit() on failure,
        // so no need to check the result here.
        a8_byte_to_write *new_pixels = MemRealloc(bytes_to_write, write_ops_capacity * sizeof(a8_byte_to_write));
        bytes_to_write = new_pixels;
    }
}

static void write_to_screenmem(uint16_t offset, uint8_t value90, uint8_t valueA0, bool fill, int delay) {
    if (!is_valid_screen_offset(offset) && delay == 0) {
        fprintf(stderr, "write_to_screenmem: 0x%04x is an invalid screen address\n", offset);
        return;
    }

    if (fill) {
        memset(a8_screenmem90, value90, A8_IMAGE_SIZE);
        memset(a8_screenmemA0, valueA0, A8_IMAGE_SIZE);
    } else if (delay == 0) {
        a8_screenmem90[offset] = value90;
        a8_screenmemA0[offset] = valueA0;
    }

    ensure_capacity();
    a8_byte_to_write *op = &bytes_to_write[total_write_ops++];
    op->offset = offset;
    op->value90 = value90;
    op->valueA0 = valueA0;
    op->fill_bg = fill;
    op->delay = delay;
}

static void move_left(a8_draw_ctx *ctx) {
    ctx->xpos--;
    ctx->pixel_offset--;
    ctx->pixel_remainder--;
    if ((int8_t)ctx->pixel_remainder < 0) {
        ctx->pixel_remainder = 3;
        ctx->screen_offset--;
    }
}

static void move_right(a8_draw_ctx *ctx) {
    ctx->xpos++;
    ctx->pixel_offset++;
    ctx->pixel_remainder++;
    if (ctx->pixel_remainder > 3) {
        ctx->pixel_remainder = 0;
        ctx->screen_offset++;
    }
}

static void move_left_or_right(bool right_and_down, a8_draw_ctx *ctx) {
    if (right_and_down) {
        move_right(ctx);
    } else {
        move_left(ctx);
    }
}

static void move_up(a8_draw_ctx *ctx) {
    ctx->ypos--;
    ctx->screen_offset -= A8_SCREEN_COLUMNS;
}

static void move_down(a8_draw_ctx *ctx) {
    ctx->ypos++;
    ctx->screen_offset += A8_SCREEN_COLUMNS;
}

static void move_up_or_down(bool right_and_down, a8_draw_ctx *ctx) {
    if (right_and_down) {
        move_down(ctx);
    } else {
        move_up(ctx);
    }
}

// Determine x and y coordinates from screen memory offset in pixels.
// (remember: there are four pixels in every screen byte, as a pixel uses two bits)
static void coordinates_from_pixel_offset(a8_draw_ctx *ctx) {
    ctx->ypos = ctx->pixel_offset / A8_SCREEN_WIDTH;
    ctx->xpos = ctx->pixel_offset % A8_SCREEN_WIDTH;
}

// Determine screen memory byte offset and pixel remainder from pixel offset
static inline void byte_offset_from_pixel_offset(a8_draw_ctx *ctx) {
    ctx->pixel_remainder = ctx->pixel_offset % 4;
    ctx->screen_offset = ctx->pixel_offset / 4;
}

// Determine screen memory offsets from x and y coordinates
static void offsets_from_coordinates(a8_draw_ctx *ctx) {
    ctx->pixel_offset = ctx->ypos * A8_SCREEN_WIDTH + ctx->xpos;
    byte_offset_from_pixel_offset(ctx);
}

static inline void plot_pixel(a8_draw_ctx *ctx) {
    if (ctx->screen_offset >= A8_IMAGE_SIZE) return;

    uint8_t pixmask[4] = {0xc0, 0x30, 0x0c, 0x03};
    uint8_t mask = pixmask[ctx->pixel_remainder & 3];

    write_to_screenmem(ctx->screen_offset, (a8_screenmem90[ctx->screen_offset] & ~mask) | (ctx->plane90_pattern_even & mask), (a8_screenmemA0[ctx->screen_offset] & ~mask) | (ctx->planeA0_pattern_even & mask), false, 0);
}

static void draw_line(a8_draw_ctx *ctx) {
    int dx = abs(ctx->target_x - ctx->start_x);
    int dy = abs(ctx->target_y - ctx->start_y);

    bool going_left = ctx->target_x < ctx->start_x;
    bool going_up = ctx->target_y < ctx->start_y;
    bool x_major = dy < dx;
    bool positive_direction = (going_left == going_up);

    /* Choose which endpoint to start from so the iteration direction
       matches the primary axis.  For x-major the secondary step is
       always move_down, so start from the endpoint with smaller y.
       For y-major the secondary step is always move_right, so start
       from the endpoint with smaller x. */
    if (x_major ? !going_up : !going_left) {
        ctx->xpos = ctx->start_x;
        ctx->ypos = ctx->start_y;
    } else {
        ctx->xpos = ctx->target_x;
        ctx->ypos = ctx->target_y;
    }

    offsets_from_coordinates(ctx);

    /* For y-major, swap axes so the loop body is identical to x-major.
       "major" is the primary iteration axis, "minor" is the secondary. */
    int major = x_major ? dx : dy;
    int minor = x_major ? dy : dx;

    /* 8-bit overflow tracking (original Atari code used single-byte
       arithmetic, so values >= 128 cause twice_* to wrap) */
    uint8_t twice_major = major * 2;
    uint8_t twice_minor = minor * 2;
    int minor_overflows = (int8_t)minor < 0;
    int major_overflows = (int8_t)major < 0;

    int16_t error = (minor_overflows << 8) + twice_minor - major;
    for (int i = 0; i < major; i++) {
        plot_pixel(ctx);
        if (error < 0) {
            error += ((uint8_t)minor_overflows << 8) + twice_minor;
        } else {
            if (x_major)
                move_down(ctx);
            else
                move_right(ctx);
            uint8_t err_lo = twice_minor - twice_major;
            uint8_t err_hi = minor_overflows - major_overflows - (twice_major > twice_minor ? 1 : 0);
            error += (int16_t)(err_hi << 8 | err_lo);
        }
        if (x_major)
            move_left_or_right(positive_direction, ctx);
        else
            move_up_or_down(positive_direction, ctx);
    }
    plot_pixel(ctx);
}

/*
 Pattern system overview:
 
 The Scott Adams Atari 8-bit vector draw code uses two screen memory planes ($9000 and $A000).
 Each byte holds four 2-bit pixels. The orignal code alternated between these on every screen update, giving the impression of a total of 16 colors (the color mixing being a side effect of the NTSC CRT hardware limitations, in combination with the perception limitations of the player.)
 We simulate this with a lookup table of 16 colors (color_matrix), indexed by combining data from the two images (A0 bits as high pair, 90 bits as low pair).

 On top of all this, additional colors are simulated by dithering.

 Dithering patterns are defined as four bytes (pattern_byte[0..3]):
   - Bytes 0/1 are written to even scanlines (A0 and 90 planes respectively).
   - Bytes 2/3 are written to odd scanlines (A0 and 90 planes respectively).
 This allows patterns to alternate row-by-row for dithering effects.
 
 The four pattern_color values (derived from color_array via color_a_index
 and color_b_index) serve as the building blocks:
   - pattern_color[0] and [1] come from color_a_index (a pair of 2-bit colors)
   - pattern_color[2] and [3] come from color_b_index (a second pair)
 
 Five pattern types (0-4) combine these colors in different ways to produce
 solid fills, horizontal stripes, checkerboards, and mixed dithers.
*/

/* Each row defines four pattern bytes as pairs of pattern_color indices.
   Solid fills use identical indices; alternating fills use different ones.
   Pattern 0: Horizontal stripes (solid color_a on even and odd scanlines)
   Pattern 1: Checkerboard (alternating color_a/color_b, swapped on odd lines)
   Pattern 2: Mixed solid stripes (color_a on even, color_b on odd scanlines)
   Pattern 3: Uniform vertical stripes (alternating color_b/color_a, all lines)
   Pattern 4: Solid-to-striped dither (solid even, alternating odd scanlines) */
static const uint8_t pattern_defs[5][4][2] = {
    {{0,0}, {1,1}, {0,0}, {1,1}},
    {{0,2}, {1,3}, {2,0}, {3,1}},
    {{0,0}, {1,1}, {2,2}, {3,3}},
    {{2,0}, {3,1}, {2,0}, {3,1}},
    {{0,0}, {1,1}, {2,0}, {3,1}},
};

static void make_pattern(uint8_t pattern, a8_draw_ctx *ctx) {
    for (int i = 0; i < 4; i++) {
        uint8_t a = ctx->pattern_color[pattern_defs[pattern][i][0]];
        uint8_t b = ctx->pattern_color[pattern_defs[pattern][i][1]];
        /* Repeat two 2-bit color values in a single byte (abab)
           representing four screen pixels. */
        ctx->pattern_byte[i] = a << 6 | b << 4 | a << 2 | b;
    }
}

/* Build the complete fill pattern from an opcode's parameters.
   Steps:
   1. Look up the four base colors from color_a_index and color_b_index.
   2. Compute the seed pixel offset from the x/y arguments.
   3. Generate the four pattern bytes using the selected pattern type (0-4).
   4. Copy the pattern bytes into the plane-specific drawing registers. */
static void create_patterns(uint8_t pattern, uint8_t op_arg_a, uint8_t op_arg_b, a8_draw_ctx *ctx) {

    /* Look up the four 2-bit base colors from color_array.
     color_a_index selects one pair (pattern_color[0..1]),
     color_b_index selects a second pair (pattern_color[2..3]).
     These are the raw color values used by make_pattern(). */
    ctx->pattern_color[0] = color_array[ctx->color_a_index << 1];
    ctx->pattern_color[1] = color_array[ctx->color_a_index << 1 | 1];
    ctx->pattern_color[2] = color_array[ctx->color_b_index << 1];
    ctx->pattern_color[3] = color_array[ctx->color_b_index << 1 | 1];

    /* Convert the opcode's x/y arguments into a pixel offset for the flood fill
     seed point. The formula op_arg_b * 160 + op_arg_a computes the linear pixel
     position (160 pixels per scanline). */
    ctx->pattern_start_pixel = (op_arg_b << 7) + op_arg_b * 32 + op_arg_a;

    if (pattern < 5)
        make_pattern(pattern, ctx);

    /* Copy the four pattern bytes into the plane-specific even/odd registers
     that the drawing routines use directly.
     Bytes 0/1 → even scanlines (A0/90 planes), bytes 2/3 → odd scanlines. */
    ctx->planeA0_pattern_even = ctx->pattern_byte[0];
    ctx->plane90_pattern_even = ctx->pattern_byte[1];
    ctx->planeA0_pattern_odd  = ctx->pattern_byte[2];
    ctx->plane90_pattern_odd  = ctx->pattern_byte[3];
}

// Extract the 2-bit pixel in `slot` (0..3) from a screen byte; slot 0 is the
// most significant pair, slot 3 the least.
inline static uint8_t color_at_byte_slot(uint8_t value, uint8_t slot) {
    return (value >> (6 - 2 * slot)) & 3;
}

static uint8_t get_pixel_color(a8_draw_ctx *ctx) {
    uint8_t color_90 = color_at_byte_slot(a8_screenmem90[ctx->screen_offset], ctx->pixel_remainder);
    uint8_t color_a0 = color_at_byte_slot(a8_screenmemA0[ctx->screen_offset], ctx->pixel_remainder);
    return color_a0 << 2 | color_90;
}

static bool at_edge(a8_draw_ctx *ctx) {
    uint8_t color = get_pixel_color(ctx);
    if (ctx->erase_enabled)
        return color == ctx->fill_edge_color;
    return color != ctx->fill_background_color;
}

// Scan straight up (one full row of pixels per step) until the border above the
// region is reached, or the top of the screen is passed (pixel_offset is signed
// so it goes negative there). Leaves pixel_offset on the topmost fillable row.
static void scan_up(a8_draw_ctx *ctx) {
    for (;;) {
        ctx->pixel_offset -= A8_SCREEN_WIDTH;
        if (ctx->pixel_offset < 0) return;
        byte_offset_from_pixel_offset(ctx);
        if (at_edge(ctx))
            return;
    }
}

// From an edge pixel, scan right looking for a gap to continue the fill through.
// Returns false (and stops on it) as soon as a fillable pixel is found; returns
// true if the row stays blocked all the way to the screen edge or back to the
// column the segment above started at (scan_saved_xpos), meaning there is nothing
// more to fill below and the sweep should stop.
static bool scan_right(a8_draw_ctx *ctx) {
    for (;;) {
        move_right(ctx);
        if (ctx->xpos == A8_SCREEN_WIDTH || ctx->xpos == ctx->scan_saved_xpos)
            return true;
        if (!at_edge(ctx))
            return false;
    }
}

static void swap_even_and_odd_patterns(a8_draw_ctx *ctx) {
    uint8_t temp = ctx->plane90_pattern_odd;
    ctx->plane90_pattern_odd = ctx->plane90_pattern_even;
    ctx->plane90_pattern_even = temp;

    temp = ctx->planeA0_pattern_odd;
    ctx->planeA0_pattern_odd = ctx->planeA0_pattern_even;
    ctx->planeA0_pattern_even = temp;
}

static uint8_t build_plane_byte(uint8_t target_val) {
    uint8_t color_nibble = (target_val & 3) << 2 | (target_val & 3);
    return color_nibble << 4 | color_nibble;
}

/* Fill the region around the seed pixel with the current dither pattern,
   following the Graphics Magician scheme: scan straight up to the top border,
   then sweep downward one scanline at a time, filling the connected segment
   beneath the seed and dropping from its start column to the next line.

   A pixel is "fillable" until at_edge() says otherwise: normally that means
   anything still the background colour, or in erase mode anything that is not
   the contour (edge) colour. The screen is two bit-planes ($9000/$A000) of four
   2-bit pixels per byte, so positions are tracked redundantly as a pixel offset,
   a byte offset + remainder, and x/y, kept in sync by the move_* helpers. */
static void flood_fill(a8_draw_ctx *ctx) {
    ctx->pixel_offset = ctx->pattern_start_pixel;
    byte_offset_from_pixel_offset(ctx);
    ctx->fill_background_color = get_pixel_color(ctx);
    ctx->last_plane90_byte = build_plane_byte(ctx->fill_background_color);
    ctx->last_planeA0_byte = build_plane_byte(ctx->fill_background_color >> 2);

    /* Find top of unfilled region */
    scan_up(ctx);

    /* Move down one line to last non-filled pixel */
    ctx->pixel_offset += A8_SCREEN_WIDTH;
    byte_offset_from_pixel_offset(ctx);
    coordinates_from_pixel_offset(ctx);

    /* Determine whether even/odd patterns need swapping based on
       the parity of the starting line. On even lines the "first" pattern
       is even; on odd lines it is odd. We compare the first against
       the second and swap if the signed difference is negative. */
    uint8_t first_90, second_90, first_A0, second_A0;
    if ((ctx->ypos & 1) == 0) {
        first_90 = ctx->plane90_pattern_even;  second_90 = ctx->plane90_pattern_odd;
        first_A0 = ctx->planeA0_pattern_even;  second_A0 = ctx->planeA0_pattern_odd;
    } else {
        first_90 = ctx->plane90_pattern_odd;   second_90 = ctx->plane90_pattern_even;
        first_A0 = ctx->planeA0_pattern_odd;   second_A0 = ctx->planeA0_pattern_even;
    }
    bool need_swap = (first_90 == second_90)
        ? (int8_t)(second_A0 - first_A0) < 0
        : (int8_t)(second_90 - first_90) < 0;
    if (need_swap) {
        swap_even_and_odd_patterns(ctx);
    }

    /* Descend one scanline at a time, filling the connected segment beneath the
       seed. This was originally a chain of mutually tail-recursive functions
       (scan_left_and_recurse -> flood_fill_scanline ->
       scanline_find_edge_and_recurse -> ...) capped at a fixed recursion depth
       with a restart loop, purely to bound C-stack growth; expressed directly it
       is a plain loop, with no depth limit. */
    for (;;) {
        /* Walk left to the segment's left border, then step back onto its first
           fillable pixel and remember where the segment starts. */
        for (;;) {
            move_left(ctx);
            if ((int8_t)ctx->xpos == -1)
                break;
            if (at_edge(ctx))
                break;
        }
        move_right(ctx);
        ctx->scan_saved_pixoff = ctx->pixel_offset;

        /* Fill the segment left -> right: a fast whole-byte (4-pixel) write where
           the column is aligned and still background, otherwise pixel by pixel. */
        do {
            if (ctx->screen_offset >= A8_IMAGE_SIZE) {
                fprintf(stderr, "flood_fill: Address out of bounds!\n");
                return;
            }
            if (ctx->pixel_remainder != 0 || ctx->erase_enabled ||
                a8_screenmem90[ctx->screen_offset] != ctx->last_plane90_byte ||
                a8_screenmemA0[ctx->screen_offset] != ctx->last_planeA0_byte) {
                if (at_edge(ctx))
                    break;
                plot_pixel(ctx);
                move_right(ctx);
            } else {
                write_to_screenmem(ctx->screen_offset, ctx->plane90_pattern_even, ctx->planeA0_pattern_even, false, 0);
                ctx->xpos += 4;
                ctx->pixel_offset += 4;
                ctx->screen_offset++;
            }
        } while (ctx->xpos != A8_SCREEN_WIDTH);

        /* Drop to the next line at the segment's start column, alternating the
           dither patterns. Stop at the bottom of the screen, or when the line
           below is blocked from the start column rightward. */
        swap_even_and_odd_patterns(ctx);
        ctx->scan_saved_xpos = ctx->xpos;
        ctx->pixel_offset = ctx->scan_saved_pixoff + A8_SCREEN_WIDTH;
        byte_offset_from_pixel_offset(ctx);
        if (ctx->ypos == A8_SCREEN_HEIGHT)
            return;
        coordinates_from_pixel_offset(ctx);
        if (at_edge(ctx) && scan_right(ctx) == true)
            return;
    }
}

static const uint32_t RGBpalette[16] = {
    /* black*/     0x000000,
    /* blue */     0x2f869f,
    /* red04 */    0xcc0043,
    /* magenta */  0x703d49,
    /* green11 */  0x319c4b,
    /* blue10 */   0x279bc2,
    /* yellow15 */ 0x9dc229,
    /* white14 */  0x8da2e6,
    /* gold09 */   0xa24922,
    /* violet06 */ 0x693fa4,
    /* orange5 */  0x971f16,
    /* brmagnta */ 0x8c41e2,
    /* brgreen */  0x75d512,
    /* yellow07 */ 0x696023,
    /* gold12 */   0xd5d062,
    /* gold13 */   0x8c4a26,
};

typedef enum {
    BLACK,
    BLUE,
    RED04,
    MAGENTA,
    GREEN11,
    BLUE10,
    YELLOW15,
    WHITE14,
    GOLD09,
    VIOLET06,
    ORANGE5,
    BRMAGENTA,
    BRGREEN,
    YELLOW07,
    GOLD12,
    GOLD13
} RGBColor;

static const RGBColor color_matrix[4][4] = {
    {BLACK,    YELLOW15, BLUE,     BLUE},
    {RED04,    ORANGE5,  VIOLET06, YELLOW07},
    {YELLOW07, GOLD09,   BLUE10,   GREEN11},
    {GOLD12,   GOLD13,   WHITE14,  YELLOW15}
};

static void DrawByteAt(glsi32 *x, glsi32 *y, int index) {
    uint8_t mask = 0xc0;

    for (int i = 6; i >= 0; i -= 2) {
        int idx90 = (a8_screenmem90[index] & mask) >> i;
        int idxA0 = (a8_screenmemA0[index] & mask) >> i;
        RGBColor rgbidx = color_matrix[idx90][idxA0];
        glui32 color = RGBpalette[rgbidx];
        SetColor(1, color);
        PutDoublePixel(*x, *y, 1);
        mask >>= 2;
        (*x) += 2;
    }
}

static void DrawAtari8bitImageFromScreenmem(void) {
    glsi32 x = 0;
    glsi32 y = 0;
    for (int i = 0; i < A8_IMAGE_SIZE; i++) {
        DrawByteAt(&x, &y, i);
        if (x >= 320) {
            x = 0;
            y++;
        }
    }
}

typedef enum  {
    OPCODE_END = 0,
    OPCODE_SET_BACKGROUND_OR_MODE = 1,
    OPCODE_DELAY = 2,
    OPCODE_SET_COLOR = 3,
    OPCODE_MOVE_TO = 4,
    OPCODE_DRAW_LINE = 5,
    OPCODE_ERASE_FILL = 6,
    OPCODE_FLOOD_FILL = 7,
} Opcode;

static void handle_draw_line(a8_draw_ctx *ctx) {
    /* Set up two pattern bytes derived from color_array,
     then set plane90/planeA0 pattern even bytes and draw the line. */
    uint8_t color_idx = ctx->color_idx * 2;

    /* Build two bytes by looking up color_array[opcode_arg] and then
     duplicating nibbles. */
    uint8_t nibble0 = color_array[color_idx] & 3;
    uint8_t nibble1 = color_array[color_idx + 1] & 3;
    uint8_t pattern_nibble_combined0 = nibble0 << 2 | nibble0;
    uint8_t pattern_nibble_combined1 = nibble1 << 2 | nibble1;

    ctx->plane90_pattern_even = pattern_nibble_combined1 << 4 | pattern_nibble_combined1;
    ctx->planeA0_pattern_even = pattern_nibble_combined0 << 4 | pattern_nibble_combined0;

    if (ctx->lines_only_mode == true) {
        /* Use saved colors in lines-only mode */
        /* Only draw line if patterns 0 and 1 match current */
        if (ctx->saved_pattern[0] == ctx->planeA0_pattern_even &&
            ctx->saved_pattern[1] == ctx->plane90_pattern_even) {
            ctx->planeA0_pattern_even = ctx->saved_pattern[2];
            ctx->plane90_pattern_even = ctx->saved_pattern[3];
            draw_line(ctx);
        }
    } else {
        draw_line(ctx);
    }
    ctx->start_x = ctx->target_x;
    ctx->start_y = ctx->target_y;
}

static bool handle_fill_or_set_background(a8_draw_ctx *ctx,
                                          Opcode opcode,
                                          uint8_t arg_0,
                                          uint8_t arg_a, uint8_t arg_b) {
    ctx->erase_enabled = false;

    switch (opcode) {
        case OPCODE_ERASE_FILL:
            if (ctx->lines_only_mode) return false;
            ctx->erase_enabled = true;
            ctx->fill_edge_color = color_array[ctx->color_idx << 1] << 2 |
            color_array[(ctx->color_idx << 1) + 1];
            // fallthrough
        case OPCODE_FLOOD_FILL:
            if (ctx->lines_only_mode) return false;
            create_patterns(arg_0, arg_a, arg_b, ctx);
            flood_fill(ctx);
            break;
        case OPCODE_SET_BACKGROUND_OR_MODE:
            /* If opcode_arg == 0 do an immediate background fill with pattern 0
             (unless in lines-only mode) */
            if (arg_0 == 0) {
                if (ctx->lines_only_mode) return false;
                create_patterns(0, arg_a, arg_b, ctx);
                write_to_screenmem(0, ctx->pattern_byte[1], ctx->pattern_byte[0], true, 0);
                /* The "teleport animation" in Pirate Adventure, which mostly just does a
                   number of full-screen color changes in the Atari 8-bit version,
                   needs an extra delay in order to be noticeable */
                if (Game != NULL && CurrentGame == PIRATE_US && vector_image_shown == 90)
                    write_to_screenmem(0, 0, 0, 0, 10);
            } else {
                /* If arg_0 == 1 we store the current pattern and then redraw
                 the entire image in lines-only mode on top of the current one. */
                /* (This is a common trick for drawing room objects such as the dragon
                 on top of background images: draw a colored contour, fill it in erase
                 mode, then draw a black contour on top in lines-only mode.) */

                /* If lines-only mode is aready on,
                   we switch it off instead. */
                ctx->lines_only_mode = !ctx->lines_only_mode;
                if (ctx->lines_only_mode) {
                    create_patterns(2, arg_a, arg_b, ctx);
                    for (int i = 0; i < 4; i++)
                        ctx->saved_pattern[i] = ctx->pattern_byte[i];
                }
                return ctx->lines_only_mode;
            }
            break;
        default:
            fprintf(stderr, "Unhandled opcode: %d\n", opcode);
            break;
    }

    return false;
}

static bool process_opcode(a8_draw_ctx *ctx, Opcode opcode, uint8_t arg0, uint8_t arg_a, uint8_t arg_b) {
    bool start_over = false;
    switch (opcode) {
        case OPCODE_MOVE_TO:
            ctx->start_x = arg_a;
            ctx->start_y = arg_b;
            break;
        case OPCODE_SET_COLOR:
            ctx->color_idx = arg0;
            ctx->color_a_index = arg_a;
            ctx->color_b_index = arg_b;
            break;
        case OPCODE_DELAY:
            if (ctx->lines_only_mode == false) {
                debug_print("Delay %d\n", arg0);
                write_to_screenmem(0, 0, 0, 0, arg0 * 20);
            }
            break;
        case OPCODE_DRAW_LINE:
            ctx->target_x = arg_a;
            ctx->target_y = arg_b;
            handle_draw_line(ctx);
            break;
        default:
            start_over = handle_fill_or_set_background(ctx, opcode, arg0, arg_a, arg_b);
            break;
    }
    return start_over;
}

static void DrawSingleByteAt(uint8_t byte90,  uint8_t bytea0, glsi32 x, glsi32 y) {
    uint8_t mask = 0xc0;

    for (int i = 6; i >= 0; i -= 2) {
        int idx90 = (byte90 & mask) >> i;
        int idxA0 = (bytea0 & mask) >> i;
        RGBColor rgbidx = color_matrix[idx90][idxA0];
        glui32 color = RGBpalette[rgbidx];
        SetColor(1, color);
        PutDoublePixel(x, y, 1);
        mask >>= 2;
        x += 2;
    }
}

void DrawSingleAtari8ImageByte(const a8_byte_to_write *towrite) {
    if (towrite->fill_bg == true) {
        int idx90 = (towrite->value90 & 0x3);
        int idxA0 = (towrite->valueA0 & 0x3);
        RGBColor rgbidx = color_matrix[idx90][idxA0];
        glui32 color = RGBpalette[rgbidx];
        SetColor(1, color);
        RectFill(0, 0, ImageWidth, ImageHeight, 1);
    } else {
        glsi32 y = towrite->offset / A8_SCREEN_COLUMNS;
        glsi32 x = (towrite->offset % A8_SCREEN_COLUMNS) * 8;
        DrawSingleByteAt(towrite->value90, towrite->valueA0, x, y);
    }
}

int delay_active = 0;

void DrawSomeAtari8VectorBytes(int from_start)
{
    if (!gli_slowdraw) {
        DrawAtari8bitImageFromScreenmem();
        current_write_op = total_write_ops;
    } else {
        VectorState = DRAWING_VECTOR_IMAGE;
        if (from_start) {
            current_write_op = 0;
            delay_active = 0;
            shrink_capacity();
        }

        if (delay_active > 0) {
            delay_active--;
            return;
        }

        size_t i = current_write_op;
        size_t chunk_end = i + ATARI8_VECTOR_BYTES_PER_TICK;

        for (; i < total_write_ops && i < chunk_end; i++) {
            const a8_byte_to_write *towrite = &bytes_to_write[i];
            if (towrite->delay > 0) {
                delay_active = towrite->delay;
                debug_print("DrawSomeAtari8VectorBytes: initiating a delay of %d\n", delay_active);
                current_write_op = i + 1;
                return;
            }
            DrawSingleAtari8ImageByte(towrite);
        }

        current_write_op = i;
    }

    if (current_write_op >= total_write_ops) {
        // Finished
        glk_request_timer_events(0);
        VectorState = SHOWING_VECTOR_IMAGE;
        FreeOps();
    }
}

int DrawingAtari8Vector(void) {
    return  (total_write_ops > current_write_op);
}

static void init_a8_vector_draw_session(USImage *img) {
    // Init a new image session. Cancel any in-progress drawing and free any ops.
    FreeOps();
    // Start with a small allocation for bytes_to_write.
    // write_to_screenmem() will grow this as needed.
    write_ops_capacity = 100;
    bytes_to_write = MemAlloc(write_ops_capacity * sizeof(a8_byte_to_write));
    total_write_ops = 0;
    current_write_op = 0;
    glk_request_timer_events(0);
    VectorState = DRAWING_VECTOR_IMAGE;
}

uint8_t *DrawAtari8BitVectorImage(USImage *img) {

    if (!img || img->imagedata == NULL || img->datasize < 2)
        return NULL;

    a8_draw_ctx ctx = {};

    ctx.imagedata = img->imagedata;
    ctx.datasize = img->datasize;

    ctx.color_idx = 10; // initial color index. (2,1)

    if (a8_screenmem90 == NULL)
        a8_screenmem90 = MemAlloc(A8_IMAGE_SIZE);
    if (a8_screenmemA0 == NULL)
        a8_screenmemA0 = MemAlloc(A8_IMAGE_SIZE);

    // We reset any drawing if the image is not supposed
    // to be drawn on top of another image (i.e. it is a room image)
    if (img->usage == IMG_ROOM) {
        init_a8_vector_draw_session(img);
        write_to_screenmem(0, 0, 0, true, 0);
        vector_image_shown = img->index;
    } else if (VectorState == SHOWING_VECTOR_IMAGE) {
        // If this is not a room image and we are already showing an image,
        // we can assume that we want to draw a room object on top of
        // the current room image
        DrawAtari8bitImageFromScreenmem();
        init_a8_vector_draw_session(img);
    }

    uint16_t offset = 0;

    while (offset + 2 < ctx.datasize) {
        Opcode opcode = ctx.imagedata[offset] >> 5;
        if (opcode == OPCODE_END) break;

        uint8_t arg_0 = ctx.imagedata[offset] & 0x1f;
        uint8_t arg_a = ctx.imagedata[offset + 1];
        uint8_t arg_b = ctx.imagedata[offset + 2];

        bool start_over = process_opcode(&ctx, opcode, arg_0, arg_a, arg_b);

        if (start_over) {
            offset = 0;
        } else {
            offset += 3;
        }
    }
    return ctx.imagedata + offset;
}

static int Atari8bitVectorCompare(uint8_t *data, size_t datasize, int a0) {
    fprintf(stderr, "Atari8bitVectorCompare\n");
    uint8_t *target;
    if (a0) {
        target = a8_screenmemA0;
        fprintf(stderr, "Comparing data to screen buffer at A0\n");
    } else {
        target = a8_screenmem90;
        fprintf(stderr, "Comparing data to screen buffer at 90\n");
    }
    for (int i = 0; i < datasize; i++) {
        if (target[i] != data[i]) {
            fprintf(stderr, "Mismatch at offset 0x%04x: expected 0x%02x, got 0x%02x\n", i, data[i], target[i]);
            return 0;
        }
    }
    fprintf(stderr, "Incredible! All data matched!\n");
    return 1;
}

int ReadAndDrawImageWithName(const char *name, const char *supportpath, USImageType usage) {
    USImage *image = NewImage();

    size_t pathlength = strlen(name) + 11;
    char *finalname = MemAlloc(pathlength);

    snprintf(finalname, pathlength, "atari8%s.dat", name);

    size_t size;

    image->imagedata = ReadTestDataFromFile(finalname, supportpath, &size);
    if (!image->imagedata) {
        fprintf(stderr, "Failed to read image data\n");
        free(finalname);
        return 0;
    }
    image->datasize = size;
    image->usage = usage;
    DrawAtari8BitVectorImage(image);
    free(image->imagedata);
    free(image);
    free(finalname);
    return 1;
}

int TestAtari8ImageWithName(const char *name, const char *supportpath) {
    if (!ReadAndDrawImageWithName(name, supportpath, IMG_ROOM))
        return 0;

    size_t pathlength = strlen(name) + 16;
    char *finalname = MemAlloc(pathlength);
    snprintf(finalname, pathlength, "atari8%s90.result", name);

    size_t size;
    uint8_t *arg = ReadTestDataFromFile(finalname, supportpath, &size);
    int result = Atari8bitVectorCompare(arg, size, 0);
    free(arg);
    if (result == 0) {
        free(finalname);
        return 0;
    }
    snprintf(finalname, pathlength, "atari8%sA0.result", name);
    arg = ReadTestDataFromFile(finalname, supportpath, &size);
    result = Atari8bitVectorCompare(arg, size, 1);
    free(arg);
    free(finalname);
    return result;
}

int RunAtari8bitVectorTests(const char *supportpath) {

    gli_slowdraw = 0;

    if (!TestAtari8ImageWithName("mi_boom", supportpath))
        return 0;
    if (!TestAtari8ImageWithName("so_console", supportpath))
        return 0;
    if (!TestAtari8ImageWithName("so_painting", supportpath))
        return 0;

    if (!ReadAndDrawImageWithName("so_hexagonal", supportpath, IMG_ROOM))
        return 0;
    if (!ReadAndDrawImageWithName("so_rod", supportpath, IMG_INV_AND_ROOM_OBJ))
        return 0;
    if (!ReadAndDrawImageWithName("so_tint", supportpath, IMG_INV_AND_ROOM_OBJ))
        return 0;

    size_t size;
    uint8_t *arg = ReadTestDataFromFile("tint90.result", supportpath, &size);
    if (!Atari8bitVectorCompare(arg, size, 0)) {
        return 0;
    }
    free(arg);
    arg = ReadTestDataFromFile("tinta0.result", supportpath, &size);

    if (!Atari8bitVectorCompare(arg, size, 1)) {
        return 0;
    }
    
    return 1;
}
