//
//  apple2_vector_draw.c
//  scott

//  Code to draw Apple 2 vector images in certain Scott Adams games.
//
//  Variants of this particular images format was used in many
//  Apple 2 games at the time. It was created by Penguin Software
//  (later Polarware) and marketed under names such as The Graphics
//  Magician and the Picture Painter tool.
//
//  The main purpose of this code is to stay pixel perfect with the
//  original code, so it closely follows the disassembly.
//  Parts are based on ScummVM's Comprehend engine.
//
//  The disassembly at
//  https://6502disassembly.com/a2-graphics-magician/PICDRAWH_1984.html
//  has been very helpful, but does not completely match this code.
//  Presumably it is based on a later version.
//
//  Created by Petter Sjölund on 2025-12-29.
//

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "scott.h"
#include "apple2draw.h"
#include "apple2_vector_draw.h"
#include "ciderpress.h"
#include "common_file_utils.h"
#include "vector_common.h"

#define APPLE2_SCREEN_HEIGHT 192
#define APPLE2_SCREEN_COLS 40
#define COL_BITS 7
#define RIGHT_EXPAND_MASK 0x40
#define APPLE2_WHITE 0xff

extern uint8_t *screenmem;

#define CALC_APPLE2_ADDRESS(y) ((((y / 8) & 0x07) << 7) + (((y / 8) & 0x18) * 5) + ((y & 7) << 10))

// The ROM dispatcher (DRAW_OPCODE @ $8E30) selects on `byte & 0xe0`: a 3-bit
// opcode in the top bits, with the low nibble as the parameter and bit 4
// ignored. So only the even values below ever occur; doImageOp folds bit 4 out
// before switching. (The odd values are not real opcodes.)
enum Opcode {
    OPCODE_END = 0,
    OPCODE_SET_PEN_COLOR = 2,
    OPCODE_SET_SHAPE = 4,
    OPCODE_SET_FILL_COLOR = 6,
    OPCODE_MOVE_TO = 8,
    OPCODE_DRAW_LINE = 10,
    OPCODE_DRAW_SHAPE = 12,
    OPCODE_PAINT = 14
};

typedef enum {
    BRUSH_PIXEL = 0,
    BRUSH_CIRCLE_TINY = 1,
    BRUSH_CIRCLE_SMALLER = 2,
    BRUSH_CIRCLE_SMALL = 3,
    BRUSH_CIRCLE_MED = 4,
    BRUSH_CIRCLE_LARGE = 5,
    BRUSH_SRAY_SMALL = 6,
    BRUSH_SPRAY_LARGE = 7
} Brush;

typedef struct {
    uint16_t HBAS;
    uint16_t HGR_X;
    uint8_t HGR_Y;
    uint8_t HMASK; // mask to apply to screen address when plotting pixel.masktable[xpos % 7]
    uint8_t HGR_BITS; // mask to apply to screen address when drawing color
    uint8_t HGR_HORIZ; // column position
    uint8_t HGR_COLOR; // current pen color
    Brush BRUSH; // current shape type
    uint8_t FILL_COLOR; // current fill
} a2_vector_ctx;

typedef struct {
    uint8_t scanline;
    uint8_t pattern_even;
    uint8_t pattern_odd;
    uint8_t fill_color;
    uint16_t screen_offset;
    uint16_t screen_row_base;
    uint8_t column;
    uint8_t seed_column;
    uint8_t existing_byte;
    uint8_t pixel_offset;
    uint8_t fill_mask;
    uint8_t pattern_base;
    uint8_t bitmap_index;
    Brush brush_type;
} a2_fill_ctx;


/* Single-line color patterns.  There are 30 patterns, 4 bytes each.
 When written to the hi-res screen, the 4-byte alignment is maintained so that
 overlapping areas drawn with the same pattern don't clash.
 */

static uint8_t pattern_data[120];

/* Sub-indexes for 108 patterns. Each entry is two adjacent  bytes, holding
 the sub-indices into the color pattern data table for even and odd rows.
 Values are in the range [0,29].

 For example, color pattern 8 uses sub-index $03 for even rows and $06 for odd.
 In the pattern table, $03 is white0, $06 is orange. The pattern drawn will
 have alternating rows of white and orange.
 */

static uint8_t color_pattern_subindices[216];

/* Brush bitmaps.  Each bitmap has four 8-byte pieces. */

static uint8_t brush_bitmaps[256];

void SetBrushBitmaps(const uint8_t *data) {
    memcpy(brush_bitmaps, data, sizeof(brush_bitmaps));
}

void SetPatternData(const uint8_t *data) {
    memcpy(pattern_data, data, sizeof(pattern_data));
}

void SetColorPatternSubindices(const uint8_t *data) {
    memcpy(color_pattern_subindices, data, sizeof(color_pattern_subindices));
}

typedef struct {
    uint16_t offset;
    uint8_t value;
    bool fill_bg;
} byte_to_write;

static uint8_t *slow_vector_screenmem = NULL;

static byte_to_write *bytes_to_write = NULL;
static size_t write_ops_capacity = 100;

static size_t total_write_ops = 0;
static size_t current_write_op = 0;

/* Bytes emitted per slow-draw tick. Pairs with the graphics timer interval —
 * change in step with it. */
#define APPLE2_VECTOR_BYTES_PER_TICK 50

#ifdef SPATTERLIGHT
extern int gli_slowdraw;
#else
static int gli_slowdraw = 0;
#endif


static void set_color(uint8_t color_index, a2_vector_ctx *ctx) {
    const uint8_t COLORTBL[8] = {0x00, 0x2a, 0x55, 0x7f, 0x80, 0xaa, 0xd5, 0xff};
    if (color_index < 8) {
        ctx->HGR_COLOR = COLORTBL[color_index];
    } else {
        fprintf(stderr, "SET_COLOR called with illegal color index %d\n", color_index);
    }
}

static void set_draw_position(uint16_t xpos, uint8_t ypos, a2_vector_ctx *ctx)
{
    ctx->HGR_X = xpos;
    ctx->HGR_Y = ypos;

    ctx->HBAS = CALC_APPLE2_ADDRESS(ypos);
    ctx->HGR_HORIZ = xpos / COL_BITS;
    const uint8_t masktable[COL_BITS] = {0x81,0x82,0x84,0x88,0x90,0xa0,0xc0};
    ctx->HMASK = masktable[xpos % COL_BITS];
    ctx->HGR_BITS = ctx->HGR_COLOR;
    // If odd column, rotate bits
    if ((ctx->HGR_HORIZ & 1) == 1 && ((ctx->HGR_COLOR * 2 + 0x40) & 0x80)) {
        ctx->HGR_BITS = ctx->HGR_COLOR ^ 0x7f;
    }
    return;
}

static inline bool is_valid_screen_offset(uint16_t offset) {
    return offset <= MAX_SCREEN_ADDR;
}

#pragma mark SLOW DRAW

static void ensure_capacity(void) {
    // Ensure bytes_to_write has room for all write ops.
    // This might be more than the total amount of bytes
    // in screenmem, as many ops may write to the same offset
    if (total_write_ops >= write_ops_capacity) {
        write_ops_capacity = MAX(write_ops_capacity * 2, total_write_ops + 1);  // Double the capacity
        byte_to_write *new_ops = MemRealloc(bytes_to_write, write_ops_capacity * sizeof(byte_to_write));
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
        byte_to_write *new_pixels = MemRealloc(bytes_to_write, write_ops_capacity * sizeof(byte_to_write));
        bytes_to_write = new_pixels;
    }
}

static void write_to_screenmem(uint16_t offset, uint8_t value, bool fill) {
    if (!is_valid_screen_offset(offset))
        return;
    if (fill) {
        // For now we assume that we only fill the background as the first instruction
        // and that it always is white (0xff) (and only when drawing room images)
        memset(screenmem, APPLE2_WHITE, A2_SCREEN_MEM_SIZE);
        memset(slow_vector_screenmem, APPLE2_WHITE, A2_SCREEN_MEM_SIZE);
    } else {
        screenmem[offset] = value;
    }
    ensure_capacity();
    byte_to_write *op = &bytes_to_write[total_write_ops++];
    op->offset = offset;
    op->value = value;
    op->fill_bg = fill;
}

#pragma mark FLOOD FILL

/* Read the screen byte at the given column on the current scanline row.
   Sets ctx->screen_offset and ctx->existing_byte.
   Returns false if the offset is out of bounds. */
static bool read_screen_byte(a2_fill_ctx *ctx, uint8_t col) {
    ctx->screen_offset = ctx->screen_row_base + col;
    if (!is_valid_screen_offset(ctx->screen_offset))
        return false;
    ctx->existing_byte = screenmem[ctx->screen_offset];
    return true;
}

/* Select the pattern table base offset for the current scanline,
   choosing even or odd pattern based on scanline parity. */
static void select_pattern_for_scanline(a2_fill_ctx *ctx) {
    uint8_t pattern_index = (ctx->scanline & 1) ? ctx->pattern_odd : ctx->pattern_even;
    ctx->pattern_base = pattern_index * 4;
}

/* Tests the contents of the hi-res screen to see if a specific pixel is white.
 Used by the fill code.

 Pixels appear white when two adjacent bits are set.  This code does not check
 for set bits in adjacent bytes, which makes the code faster but slightly less
 precise.

 The double-bit masks are a little off - the first two entries are identical -
 because we only want to examine a single byte.  The high bit is excluded,
 because we're just testing visible pixels.

 00000011
 00000011
 00000110
 00001100
 00011000
 00110000
 01100000

 */

static bool is_seed_pixel_white(a2_fill_ctx *ctx) {
    ctx->screen_row_base = CALC_APPLE2_ADDRESS(ctx->scanline);
    if (!read_screen_byte(ctx, ctx->seed_column))
        return false;
    uint8_t two_bits[7] = {0x3, 0x3, 0x6, 0xC, 0x18, 0x30, 0x60};
    uint8_t mask = two_bits[ctx->pixel_offset];
    return (mask & ctx->existing_byte) == mask;
}

static void paint_column_pattern(a2_fill_ctx *ctx) {
    uint16_t offset = ((ctx->column & 3) | ctx->pattern_base);
    uint8_t color_mask = pattern_data[offset];
    ctx->existing_byte = (ctx->existing_byte ^ ctx->fill_mask) & 0x7f;
    if (!is_valid_screen_offset(ctx->screen_offset))
        return;
    write_to_screenmem(ctx->screen_offset, ((ctx->fill_mask | 0x80) & color_mask) |
                       ctx->existing_byte, false);
}

/* Convert an X pixel coordinate into a column number and pixel offset. */
static void x_to_column_and_pixel(a2_fill_ctx *ctx, uint16_t x) {
    ctx->seed_column = x / COL_BITS;
    ctx->pixel_offset = x % COL_BITS;
}

static bool scan_up_to_border(a2_fill_ctx *ctx)
{
    /* Move up until we find a pixel that's not white (0xff)
     * or we hit the top.
     */
    do {
        if (ctx->scanline == 0)
            return false; /* Found no non-white pixels */
        ctx->scanline--;
    } while (is_seed_pixel_white(ctx));
    return true;
}

/* Compute the right edge of the fill mask for the current scanline.
   Rotates the existing screen byte to determine
   how many pixels on the right side of the seed column are already filled.
   Sets ctx->fill_mask and returns the right edge pixel count. */
static uint8_t compute_right_edge(a2_fill_ctx *ctx)
{
    /* Simulate rotating left with carry (7 - ctx->pixel_offset) times */
    uint16_t rotate_word = ctx->existing_byte << (7 - ctx->pixel_offset);
    ctx->fill_mask = (rotate_word & 0xff) | (rotate_word >> 9);

    /* If the rotation left some leading set bits, ensure fill_mask is extended appropriately */
    uint8_t loop_byte = (rotate_word & 0x100) != 0;
    int8_t remaining_bits;
    for (remaining_bits = 6 - ctx->pixel_offset;
         remaining_bits > 0 && loop_byte != 0; remaining_bits--) {
        uint8_t carry = loop_byte << 7;
        loop_byte = ctx->fill_mask & 1;
        ctx->fill_mask = (ctx->fill_mask >> 1) | carry;
    }

    /* Align the byte and remember the shift used */
    ctx->fill_mask >>= remaining_bits;
    return remaining_bits;
}

/* Compute the left edge of the fill mask for the current scanline.
   Continues transforming ctx->fill_mask (set by compute_right_edge)
   to determine how many pixels on the left are already filled.
 Sets ctx->fill_mask and returns the left edge pixel count. */
static uint8_t compute_left_edge(a2_fill_ctx *ctx)
{
    /* Simulate rotating right with carry, (pixel_offset + 1) times */
    uint16_t rotate_word = (ctx->fill_mask << 8) >> (ctx->pixel_offset + 1);
    ctx->fill_mask = (rotate_word >> 8) | (rotate_word << 1);

    /* If pixel_offset > 0, ensure fill_mask is extended appropriately */
    uint8_t loop_byte = 1;
    int8_t remaining_bits;
    for (remaining_bits = ctx->pixel_offset;
         remaining_bits > 0 && loop_byte != 0; remaining_bits--) {
        loop_byte = ctx->fill_mask >> 7;
        ctx->fill_mask = (ctx->fill_mask << 1) | 1;
    }

    /* Align the byte and remember the shift used */
    ctx->fill_mask <<= remaining_bits;
    return remaining_bits;
}

/*  Expand right from the seed point.

    For each column to the right, rotate the screen byte right with carry
    to find the boundary between set and unset low bits. The rotation count
    determines the right edge, and the remaining upper bits determine
    whether to keep expanding further right. */
static uint8_t fill_span_right(a2_fill_ctx *ctx, uint8_t right_edge, bool has_more) {
    while (has_more && ctx->column + 1 < APPLE2_SCREEN_COLS) {
        ctx->column++;
        if (!read_screen_byte(ctx, ctx->column)) break;

        /* Rotate right with carry to find the first zero bit from the LSB.
           Carry chain feeds 1-bits back in from the top as long as the LSB
           keeps producing 1s. */
        ctx->fill_mask = ctx->existing_byte;
        bool carry = false;
        bool lsb_set = true;
        int remaining;
        for (remaining = 8; remaining > 0 && lsb_set; remaining--) {
            lsb_set = ctx->fill_mask & 1;
            ctx->fill_mask = (ctx->fill_mask >> 1) | (carry << 7);
            carry = lsb_set;
        }
        ctx->fill_mask >>= (remaining + 1);
        has_more = (ctx->fill_mask & RIGHT_EXPAND_MASK) != 0;
        right_edge = remaining;

        paint_column_pattern(ctx);
    }
    return right_edge;
}

/* Expand left from the seed point.
 
   Scans the screen byte at the column to the left, rotating bits left
   to find the first zero bit from the MSB. If found, has_more is cleared
   (stopping leftward expansion). */
static uint8_t fill_span_left(a2_fill_ctx *ctx, uint8_t left_edge, bool has_more) {
    while (has_more && ctx->column > 0) {
        ctx->column--;
        if (!read_screen_byte(ctx, ctx->column)) break;

        /* Rotate left through the byte to find the first zero from the MSB */
        ctx->fill_mask = ctx->existing_byte << 1;
        bool carry = false;
        bool msb_set = true;
        int remaining;
        for (remaining = COL_BITS - 1; remaining >= 0 && msb_set; remaining--) {
            msb_set = ctx->fill_mask & 0x80;
            ctx->fill_mask = (ctx->fill_mask << 1) | carry;
            carry = msb_set;
            if (!msb_set) {
                ctx->fill_mask <<= remaining;
                has_more = false;
                left_edge = remaining;
            }
        }
        ctx->fill_mask = (ctx->fill_mask << 1) | msb_set;
        paint_column_pattern(ctx);
    }
    return left_edge;
}

/* Re-seed at the horizontal midpoint of the span just filled, so the next
   scanline drops from its centre (this is what gives the fill its better
   handling of concave shapes). The span runs from ctx->column (the leftmost
   painted column) to rightmost_column, with left_edge_pixels / right_edge_pixels
   left unfilled at the two ends.

   The arithmetic is deliberately 8-bit to match the original 6502 routine,
   including the explicit borrow when right_edge_pixels exceeds the running
   value. */
static void advance_seed_to_midpoint(a2_fill_ctx *ctx,
                                     uint8_t rightmost_column,
                                     uint8_t right_edge_pixels,
                                     uint8_t left_edge_pixels)
{
    /* Total span width in pixels: the two partial end columns plus the fully
       painted columns between them. */
    int8_t filled_columns = rightmost_column - ctx->column - 1;
    uint8_t span_pixels = (filled_columns < 0) ? COL_BITS
                                               : (filled_columns + 2) * COL_BITS;

    /* Trim the unfilled pixels at each end, then halve: the offset of the
       midpoint from the span's left edge. */
    uint8_t half = (uint8_t)(span_pixels - right_edge_pixels) - (right_edge_pixels > span_pixels);
    half = (uint8_t)(half - left_edge_pixels) / 2;

    /* The left edge itself sits left_edge_pixels into ctx->column, so add that
       back when turning the midpoint offset into a column + pixel position. */
    uint8_t offset = left_edge_pixels + (half % COL_BITS);
    ctx->seed_column = ctx->column + (half / COL_BITS) + (offset / COL_BITS);
    ctx->pixel_offset = offset % COL_BITS;
}

/*

 Fills a region of the hi-res screen with a pattern.

 Algorithm description, from the manual:
  1) Scan directly up from the selected point until a border is found.
  2) Move down one line, filling to the left and right borders.
  3) Average the left and right borders to find the midpoint, and move down one line from there.
  4) Check to see if the point moved down to is a border. If not, go back to step 2.

  The midpoint adjustment provides better handling of concave shapes.
 */

static void apple2_flood_fill(uint16_t x, uint8_t y, uint8_t color)
{
    a2_fill_ctx ctx = {};

    ctx.fill_color = color;
    ctx.scanline = y;

    /* Configure pattern colors */
    ctx.pattern_even = color_pattern_subindices[(uint8_t)(ctx.fill_color * 2)];
    ctx.pattern_odd  = color_pattern_subindices[(uint8_t)(ctx.fill_color * 2 + 1)];

    /* Get col/pixel for this X position */
    x_to_column_and_pixel(&ctx, x);

    if (y == 0) {
        /* 6502 quirk: when the seed is already on row 0, FLOOD_FILL skips the
         * scan-up entirely (`goto FoundTop`) with the A register still holding
         * MODULO_AND_DIVIDE's result — the X remainder, i.e. pixel_offset. So a
         * row-0 seed begins filling at scanline = pixel_offset, leaving rows
         * 0..pixel_offset-1 untouched. (This differs from a seed started below
         * row 0 that scans all the way up to a white row 0, which does fill
         * from row 0.) */
        ctx.scanline = ctx.pixel_offset;
    } else if (scan_up_to_border(&ctx)) {
        /* Move back down to the last white pixel we saw.
         * (Unless we are at row 0 and it is white) */
        ctx.scanline++;
    }

    uint8_t right_edge_pixels = 0;
    uint8_t left_edge_pixels = 0;

    /* Main scan loop: paint horizontal spans, then move down */
    while (ctx.scanline < APPLE2_SCREEN_HEIGHT && is_seed_pixel_white(&ctx)) {

        select_pattern_for_scanline(&ctx);

        /* Work out fill mask and edge pixels for this scanline */
        right_edge_pixels = compute_right_edge(&ctx);
        left_edge_pixels = compute_left_edge(&ctx);

        bool has_more_left = (ctx.fill_mask & 1) != 0;
        bool has_more_right = (ctx.fill_mask & RIGHT_EXPAND_MASK) != 0;

        /* Paint starting column */
        ctx.column = ctx.seed_column;
        paint_column_pattern(&ctx);

        /* Expand right */
        right_edge_pixels = fill_span_right(&ctx, right_edge_pixels, has_more_right);
        uint8_t rightmost_column = ctx.column;

        ctx.column = ctx.seed_column;

        /* Expand left */
        left_edge_pixels = fill_span_left(&ctx, left_edge_pixels, has_more_left);

        /* Move seed position to the midpoint of the filled span */
        advance_seed_to_midpoint(&ctx, rightmost_column, right_edge_pixels, left_edge_pixels);
        ctx.scanline++;
    }
}

#pragma mark SHAPE DRAWING

/* One 8-row brush quadrant: for each row, spread the brush byte across two adjacent
   hi-res bytes by a 7-bit rotate (collecting bit 6 into the right byte), then
   blend it into the screen using the current fill colour pattern. */
static void draw_bitmap(a2_fill_ctx *ctx)
{
    for (int row = 0; row < 8; row++) {
        uint16_t address = CALC_APPLE2_ADDRESS(ctx->scanline) + ctx->seed_column;
        int idx = ctx->bitmap_index + row;
        if (idx < (int)sizeof(brush_bitmaps) && is_valid_screen_offset(address)) {
            uint8_t b = brush_bitmaps[idx];
            if (b != 0) {
                /* 7-bit spread by the pixel offset within the column. */
                uint8_t carry = 0;
                for (uint8_t n = ctx->pixel_offset; n != 0; n--) {
                    carry = (uint8_t)((carry << 1) | ((b & 0x7f) >> 6));
                    b = (uint8_t)(b << 1);
                }
                uint8_t mask_a = b & 0x7f;   /* bits painted into column `col` */
                uint8_t mask_b = carry;      /* bits carried into column `col+1` */

                uint8_t pat_base = ((ctx->scanline & 1) ? ctx->pattern_odd
                                                       : ctx->pattern_even) * 4;
                uint8_t pat0 = pattern_data[(ctx->seed_column & 3) | pat_base];
                uint8_t pat1 = pattern_data[((ctx->seed_column + 1) & 3) | pat_base];

                if (mask_a != 0)
                    write_to_screenmem(address,
                        ((mask_a ^ 0x7f) & screenmem[address]) | ((mask_a | 0x80) & pat0), false);
                if (mask_b != 0 && is_valid_screen_offset(address + 1))
                    write_to_screenmem(address + 1,
                        ((mask_b ^ 0x7f) & screenmem[address + 1]) | ((mask_b | 0x80) & pat1), false);
            }
        }
        ctx->scanline++;
    }
}

/* Draws a brush as a series of four bitmaps.

   The four parts of the brush appear in the order top-left, top-right,
   bottom-left, bottom-right.  The current X,Y position determines the
   top-left corner, not the center.
 */

#define BRUSH_PART_SIZE 8
#define BRUSH_TOTAL_PARTS 4
#define BRUSH__PART_HEIGHT 8

static void draw_brush_quadrant(a2_fill_ctx *ctx, int column_offset, int line_offset) {
    ctx->seed_column += column_offset;
    ctx->scanline += line_offset;

    draw_bitmap(ctx);

    /* Restore original column and line after drawing */
    ctx->seed_column -= column_offset;
    /* The draw_bitmap() call added 8 lines */
    ctx->scanline -= line_offset + BRUSH__PART_HEIGHT;
}

static void draw_brush(uint16_t x, uint8_t y, Brush brush, uint8_t color) {
    a2_fill_ctx ctx = {};

    /* Initialize the context */
    ctx.scanline = y;
    ctx.brush_type = brush;
    ctx.fill_color = color;
    ctx.pattern_even = color_pattern_subindices[ctx.fill_color * 2];
    ctx.pattern_odd = color_pattern_subindices[ctx.fill_color * 2 + 1];

    /* Split X coordinate into byte/pixel */
    x_to_column_and_pixel(&ctx, x);
    ctx.bitmap_index = ctx.brush_type * BRUSH_PART_SIZE * BRUSH_TOTAL_PARTS;

    if (ctx.bitmap_index + BRUSH_PART_SIZE * BRUSH_TOTAL_PARTS > sizeof(brush_bitmaps)) {
        fprintf(stderr, "Error: Brush bitmap index out of bounds!\n");
        return;
    }

    /* Draw each quadrant of the brush */
    draw_brush_quadrant(&ctx, 0, 0);                   // Top-left
    ctx.bitmap_index += BRUSH_PART_SIZE;
    draw_brush_quadrant(&ctx, 1, 0);                   // Top-right
    ctx.bitmap_index += BRUSH_PART_SIZE;
    draw_brush_quadrant(&ctx, 0, BRUSH__PART_HEIGHT);  // Bottom-left
    ctx.bitmap_index += BRUSH_PART_SIZE;
    draw_brush_quadrant(&ctx, 1, BRUSH__PART_HEIGHT);  // Bottom-right
}

#pragma mark LINE DRAWING

static void move_left(a2_vector_ctx *ctx) {
    if ((ctx->HMASK & 1) == 0) {
        /* Shift mask right, moves dot left. */
        /* Move sign bit back where it was. */
        ctx->HMASK = ctx->HMASK >> 1 ^ 0xc0;
        return;
    }
    /* moved to next byte, so decrease index */
    ctx->HGR_HORIZ--;
    if ((int8_t)ctx->HGR_HORIZ < 0) {
        /* we went off the left edge,
         so wrap around to the right edge */
        ctx->HGR_HORIZ = 39;
    }
    /* new HMASK, rightmost bit on screen */
    ctx->HMASK = 0xc0;
    if ((ctx->HGR_BITS * 2 + 0x40) & 0x80) {
        /* rotate low-order 7 bits
           of HGR_BITS one bit position */
        ctx->HGR_BITS ^= 0x7f;
    }
}

static void move_right(a2_vector_ctx *ctx) {
    /* shifting byte left moves pixel right */
    ctx->HMASK = ctx->HMASK << 1 ^ 0x80;
    if ((int8_t)ctx->HMASK < 0) {
        return;
    }
    /* new mask value */
    ctx->HMASK = 0x81;
    ctx->HGR_HORIZ++;
    /* move to the next byte to the right */
    if (ctx->HGR_HORIZ >= 40) {
        /* we went off the right edge,
         so wrap around to the left edge */
        ctx->HGR_HORIZ = 0;
    }

    if ((ctx->HGR_BITS * 2 + 0x40) & 0x80) {
        /* rotate low-order 7 bits
         of HGR_BITS one bit position */
        ctx->HGR_BITS ^= 0x7f;
    }
}

static void move_up_or_down(bool down, a2_vector_ctx *ctx) {
    if (down) {
        ctx->HGR_Y++;
        // Wrap around if bottom reached
        if (ctx->HGR_Y > 191) ctx->HGR_Y = 0;
    } else {
        ctx->HGR_Y--;
        // Wrap around if top reached
        if (ctx->HGR_Y > 191) ctx->HGR_Y = 191;
    }
    ctx->HBAS = CALC_APPLE2_ADDRESS(ctx->HGR_Y);
}

static void move_left_or_right(bool left, a2_vector_ctx *ctx) {
    if (left)
        move_left(ctx);
    else
        move_right(ctx);
}

bool stepping = false;

static void plot_a2_vector_pixel(a2_vector_ctx *ctx) {
    uint16_t offset = ctx->HBAS + ctx->HGR_HORIZ;
    if (offset > MAX_SCREEN_ADDR)
        return;
    write_to_screenmem(offset, ((screenmem[offset] ^ ctx->HGR_BITS) & ctx->HMASK) ^ screenmem[offset], false);
}

#define CARRY_THRESHOLD 0x100

static void draw_apple2_line(const uint16_t target_x, const uint8_t target_y, a2_vector_ctx *ctx)
{
    /* Determine movement direction */
    const bool move_down = target_y > ctx->HGR_Y;
    const bool move_left = target_x < ctx->HGR_X;

    /* Calculate deltas */
    const uint16_t delta_x = (target_x > ctx->HGR_X) ? (target_x - ctx->HGR_X) : (ctx->HGR_X - target_x);
    const uint8_t delta_y = (target_y > ctx->HGR_Y) ? (target_y - ctx->HGR_Y) : (ctx->HGR_Y - target_y);

    /* Plot the initial pixel */
    plot_a2_vector_pixel(ctx);

    const int total_steps = delta_x + delta_y; // Total iterations of the loop
    if (total_steps == 0) return; // Single pixel, no line to draw

    /* The original 6502 assembly code sets HGR_DY to -delta_y - 1,
       but a carry of 1 is added every time it is used in calculations.
       Note that this is not the same as (uint8_t)(-delta_y), as the
       result may overflow 8 bits. */
    const int minus_delta_y = (uint8_t)(~delta_y) + 1;

    bool carry = (uint8_t)delta_x + minus_delta_y < CARRY_THRESHOLD;
    /* The moving_vertically bool represents the value of the carry
       flag at the end of each iteration in the original 6502 code. */
    bool moving_vertically = (delta_x >> 8) < carry;

    uint16_t error_term = delta_x + minus_delta_y - CARRY_THRESHOLD;

    for (int i = 0; i < total_steps; i++) {
        if (moving_vertically) {
            move_up_or_down(move_down, ctx);
            /* We switch to horizontal movement if the most significant byte of
               error_term + delta_x overflows 8 bits */
            moving_vertically = error_term + delta_x < CARRY_THRESHOLD << 8;
            error_term += delta_x;
        } else {
            move_left_or_right(move_left, ctx);
            /* We switch to vertical movement if the least significant byte
               of error_term - delta_y does not overflow 8 bits.
               (In the original, carry set means move horizontally.) */
            uint16_t nocarry = ((uint8_t)error_term + minus_delta_y < CARRY_THRESHOLD) << 8;
            moving_vertically = error_term < nocarry;
            uint8_t e_low = (uint8_t)error_term + minus_delta_y;
            error_term = ((error_term - nocarry) & 0xff00) | e_low;
        }
        plot_a2_vector_pixel(ctx);
    }
    /* HGR_Y is already set (in move_up_or_down()), but not HGR_X. */
    ctx->HGR_X = target_x;
}

#pragma mark Main draw routine

static bool doImageOp(uint8_t **outptr, a2_vector_ctx *ctx) {
    uint8_t *ptr = *outptr;
    uint8_t opcode;
    uint16_t a, b;

    opcode = *ptr++;

    uint8_t param = opcode & 0xf;
    /* The ROM dispatcher (DRAW_OPCODE @ $8E30) selects the opcode with
       `byte & 0xe0` — a 3-bit selector in which bit 4 (0x10) is ignored. So an
       odd top-nibble aliases to the even opcode below it (e.g. 0x5x is
       SET_SHAPE, not a distinct "text outline" op that swallows an argument
       byte). Fold bit 4 out to match, which keeps the opcode stream in sync. */
    opcode = (opcode & 0xe0) >> 4;

//    fprintf(stderr, "doImageOp: opcode %d. param %d\n", opcode, param);

    // After folding bit 4 out above, `opcode` is always even, so only these
    // eight cases are reachable — exactly the ROM dispatcher's branches.
    switch (opcode) {
        case OPCODE_END: // 0
            *outptr = ptr;
            return true;

        case OPCODE_SET_PEN_COLOR: // 2
            set_color(param, ctx);
            break;

        case OPCODE_SET_SHAPE: // 4
            ctx->BRUSH = (Brush)param;
            break;

        case OPCODE_SET_FILL_COLOR: // 6
            a = *ptr++;
            ctx->FILL_COLOR = a;
            break;

        case OPCODE_MOVE_TO: // 8
            a = *ptr++ + (param & 1 ? 256 : 0);
            b = *ptr++;
            set_draw_position(a, b, ctx);
            break;

        case OPCODE_DRAW_LINE: // 10
            a = *ptr++ + (param & 1 ? 256 : 0);
            b = *ptr++;

            draw_apple2_line(a, b, ctx);
            break;

        case OPCODE_DRAW_SHAPE: // 12
            a = *ptr++ + (param & 1 ? 256 : 0);
            b = *ptr++;
            draw_brush(a, b, ctx->BRUSH, ctx->FILL_COLOR);
            break;

        case OPCODE_PAINT: // 14
            a = *ptr++ + (param & 1 ? 256 : 0);
            b = *ptr++;
            apple2_flood_fill(a, b, ctx->FILL_COLOR);
            break;
    }

    *outptr = ptr;
    return false;
}

static void white_background(void) {
    // Draw a white background
    // with a black line to the left
    // to match the artifact renderer
    SetColor(0, 0);
    SetColor(1, 0xffffff);
    RectFill(0, 0, 2, ImageHeight, 0);
    RectFill(2, 0, ImageWidth - 2, ImageHeight, 1);
}

static int write_pixel(const byte_to_write *towrite, int current_op_index) {
    slow_vector_screenmem[towrite->offset] = towrite->value;
    DrawSingleApple2ImageByte(slow_vector_screenmem, towrite->offset);
    if (current_op_index + 1 < total_write_ops) {
        const byte_to_write *next_byte = &bytes_to_write[current_op_index + 1];
        // Extend the chunk only across physically adjacent bytes, whose Apple II
        // artifact colours depend on each other — stopping between them would
        // flash a transient wrong colour at the seam. (A previous version also
        // extended across any two ops sharing a value, which made large
        // same-pattern fills snap in within a single tick instead of revealing
        // gradually like the rest of the picture.)
        if (next_byte->offset >= towrite->offset - 1 && next_byte->offset <= towrite->offset + 1) {
            return 1;
        }
    }
    return 0;
}

void DrawSomeApple2VectorBytes(int from_start)
{
    if (!gli_slowdraw) {
        DrawApple2ImageFromVideoMem();
        current_write_op = total_write_ops;
    } else {
        VectorState = DRAWING_VECTOR_IMAGE;
        if (from_start) {
            current_write_op = 0;
            shrink_capacity();
        }
        size_t i = current_write_op;

        /* keep_going extends the chunk past the normal cap when the next op
         * writes an adjacent byte — this avoids visible seams in runs of
         * neighbouring writes. */
        int keep_going = 0;
        size_t chunk_end = i + APPLE2_VECTOR_BYTES_PER_TICK;

        for (; i < total_write_ops && (i < chunk_end || keep_going); i++) {
            const byte_to_write *towrite = &bytes_to_write[i];
            if (towrite->fill_bg) {
                white_background();
            } else {
                keep_going = write_pixel(towrite, (int)i);
            }
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

int DrawingApple2Vector(void) {
    return  (total_write_ops > current_write_op);
}

static void init_a2_vector_draw_session(USImage *img) {
    // Init a new image session. Cancel any in-progress drawing and free any ops.
    FreeOps();
    // Start with a small allocation for bytes_to_write.
    // write_to_screenmem() will grow this as needed.
    write_ops_capacity = 100;
    bytes_to_write = MemAlloc(write_ops_capacity * sizeof(byte_to_write));
    total_write_ops = 0;
    current_write_op = 0;
    glk_request_timer_events(0);
    VectorState = DRAWING_VECTOR_IMAGE;
}

int DrawApple2VectorImage(USImage *img) {
    if (!img || !img->imagedata || img->datasize < 2)
        return 0;

    a2_vector_ctx ctx = {};

    uint8_t *vecdata = img->imagedata + 4;
    int done = 0;
    if (screenmem == NULL) {
        screenmem = MemCalloc(A2_SCREEN_MEM_SIZE);
    }
    if (slow_vector_screenmem == NULL) {
        slow_vector_screenmem = MemAlloc(A2_SCREEN_MEM_SIZE);
    }

    // We reset any drawing if the image is not supposed
    // to be drawn on top of another image (i.e. it is a room image)
    if (img->usage == IMG_ROOM) {
        init_a2_vector_draw_session(img);
        write_to_screenmem(0, 0, true);
        vector_image_shown = img->index;
    } else if (VectorState == SHOWING_VECTOR_IMAGE) {
        // If this is not a room image and we are already showing an image,
        // we can assume that we want to draw a room object on top of
        // the current room image
        DrawApple2ImageFromVideoMem();
        init_a2_vector_draw_session(img);
    }

    set_color(4, &ctx);
    set_draw_position(140,96, &ctx);
    ctx.FILL_COLOR = 0;
    ctx.BRUSH = BRUSH_CIRCLE_LARGE;

    while (done == 0 && vecdata < img->imagedata + img->datasize) {
        done = doImageOp(&vecdata, &ctx);
    }
    return 1;
}

int CompareA2ScreenMemory(const char *filename, const char *supportpath) {
    size_t pathlength = strlen(supportpath) + strlen(filename) + 1;
    char *path = MemAlloc(pathlength);
    snprintf(path, pathlength, "%s%s", supportpath, filename);
    fprintf(stderr, "CompareA2ScreenMemory: Comparison with file %s\n", path);
    size_t size;
    uint8_t *screen_dump = ReadFileIfExists(path, &size);
    if (screen_dump == NULL || size == 0) {
        fprintf(stderr, "Bad file!\n");
        return 0;
    }
    for (int i = 0; i < A2_SCREEN_MEM_SIZE; i++) {
        if (screen_dump[i] != screenmem[i]) {
            fprintf(stderr, "Mismatch at 0x%x: expected 0x%x, got 0x%x\n", i, screen_dump[i], screenmem[i]);
            free(screen_dump);
            return 0;
        }
    }
    free(screen_dump);
    return 1;
}

int TestApple2ImageWithName(const char *name, const char *supportpath) {
    USImage *image = NewImage();

    size_t pathlength = strlen(name) + 11;
    char *finalname = MemAlloc(pathlength);

    snprintf(finalname, pathlength, "apple2%s.dat", name);

    size_t size;

    image->imagedata = ReadTestDataFromFile(finalname, supportpath, &size);
    if (!image->imagedata) {
        fprintf(stderr, "Failed to read image data\n");
        free(finalname);
        return 0;
    }
    image->datasize = size;
    image->usage = IMG_ROOM;
    DrawApple2VectorImage(image);
    free(image->imagedata);
    free(image);
    free(finalname);

    pathlength = strlen(name) + 14;
    finalname = MemAlloc(pathlength);
    snprintf(finalname, pathlength, "apple2%s.result", name);
    int result = CompareA2ScreenMemory(finalname, supportpath);
    free(finalname);
    return result;
}

int RunApple2VectorTests(const char *supportpath) {

    gli_slowdraw = 0;

    size_t disksize;
    uint8_t *diskdata = ReadTestDataFromFile("MI.dsk", supportpath, &disksize);
    if (diskdata) {
        InitDskImage(diskdata, disksize);
        LoadDrawingDataFromDisk(diskdata, disksize);
        FreeDiskImage();
        free(diskdata);
    }

    if (!TestApple2ImageWithName("mi_boom", supportpath)) {
        return 0;
    }
    if (!TestApple2ImageWithName("so_console", supportpath)) {
        return 0;
    }
    if (!TestApple2ImageWithName("so_painting", supportpath)) {
        return 0;
    }
    return 1;
}
