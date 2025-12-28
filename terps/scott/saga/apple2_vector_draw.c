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
#include "common_file_utils.h"
#include "vector_common.h"

#define APPLE2_SCREEN_HEIGHT 192
#define APPLE2_SCREEN_COLS 40
#define COL_BITS 7
#define RIGHT_EXPAND_MASK 0x40
#define APPLE2_WHITE 0xff

extern uint8_t *screenmem;

#define CALC_APPLE2_ADDRESS(y) (((y / 8) & 0x07) << 7) + (((y / 8) & 0x18) * 5) + ((y & 7) << 10)

enum Opcode {
    OPCODE_END = 0,
    OPCODE_SET_TEXT_POS = 1,
    OPCODE_SET_PEN_COLOR = 2,
    OPCODE_TEXT_CHAR = 3,
    OPCODE_SET_SHAPE = 4,
    OPCODE_TEXT_OUTLINE = 5,
    OPCODE_SET_FILL_COLOR = 6,
    OPCODE_END2 = 7,
    OPCODE_MOVE_TO = 8,
    OPCODE_DRAW_BOX = 9,
    OPCODE_DRAW_LINE = 10,
    OPCODE_DRAW_CIRCLE = 11,
    OPCODE_DRAW_SHAPE = 12,
    OPCODE_DELAY = 13,
    OPCODE_PAINT = 14,
    OPCODE_RESET = 15
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
    uint8_t LINE;
    uint8_t PATTERN_EVEN;
    uint8_t PATTERN_ODD;
    uint8_t FILL_COLOR;
    uint16_t SCREEN_OFFSET;
    uint16_t SCREEN_ADDRESS;
    uint8_t COLUMN;
    uint8_t STARTING_COLUMN;
    uint8_t OLD_COLOR_VALUE;
    uint8_t PIXELS;
    uint8_t FILLBYTE;
    uint8_t HORIZ_PIXELS;
    uint8_t BITMAP_INDEX;
    Brush BRUSH_INDEX;
} a2_fill_ctx;


/* Single-line color patterns.  There are 30 patterns, 4 bytes each.
 When written to the hi-res screen, the 4-byte alignment is maintained so that
 overlapping areas drawn with the same pattern don't clash.
 */

static const uint8_t pattern_data[120] =  {
    0x00, 0x00, 0x00, 0x00,
    0x55, 0x2A, 0x55, 0x2A,
    0x2A, 0x55, 0x2A, 0x55,
    0x7F, 0x7F, 0x7F, 0x7F,
    0x80, 0x80, 0x80, 0x80,
    0xD5, 0xAA, 0xD5, 0xAA,
    0xAA, 0xD5, 0xAA, 0xD5,
    0xFF, 0xFF, 0xFF, 0xFF,
    0x33, 0x66, 0x4C, 0x19,
    0xB3, 0xE6, 0xCC, 0x99,
    0x4C, 0x19, 0x33, 0x66,
    0xCC, 0x99, 0xB3, 0xE6,
    0x11, 0x22, 0x44, 0x08,
    0x91, 0xA2, 0xC4, 0x88,
    0x44, 0x08, 0x11, 0x22,
    0xC4, 0x88, 0x91, 0xA2,
    0x22, 0x44, 0x08, 0x11,
    0xA2, 0xC4, 0x88, 0x91,
    0x08, 0x11, 0x22, 0x44,
    0x88, 0x91, 0xA2, 0xC4,
    0xC9, 0xA4, 0x92, 0x89,
    0x24, 0x12, 0x49, 0x24,
    0x77, 0x6E, 0x5D, 0x3B,
    0xF7, 0xEE, 0xDD, 0xBB,
    0x5D, 0x3B, 0x77, 0x6E,
    0xDD, 0xBB, 0xF7, 0xEE,
    0x6E, 0x5D, 0x3B, 0x77,
    0xEE, 0xDD, 0xBB, 0xF7,
    0x3B, 0x77, 0x6E, 0x5D,
    0xBB, 0xF7, 0xEE, 0xDD
};

/* Sub-indexes for 108 patterns. Each entry is two adjacent  bytes, holding
 the sub-indices into the color pattern data table for even and odd rows.
 Values are in the range [0,29].

 For example, color pattern 8 uses sub-index $03 for even rows and $06 for odd.
 In the pattern table, $03 is white0, $06 is orange. The pattern drawn will
 have alternating rows of white and orange.
 */

static const uint8_t color_pattern_subindices[216] = {
    0x03, 0x07, 0x16, 0x07, 0x1A, 0x1D, 0x1C, 0x17, 0x08, 0x0B, 0x00, 0x1B,
    0x00, 0x04, 0x03, 0x1B, 0x03, 0x06, 0x1A, 0x06, 0x00, 0x06, 0x00, 0x11,
    0x02, 0x06, 0x1C, 0x13, 0x10, 0x13, 0x10, 0x07, 0x02, 0x1B, 0x02, 0x07,
    0x02, 0x17, 0x02, 0x09, 0x1A, 0x04, 0x10, 0x04, 0x02, 0x05, 0x12, 0x17,
    0x1A, 0x07, 0x03, 0x17, 0x16, 0x19, 0x03, 0x05, 0x03, 0x0D, 0x1A, 0x0D,
    0x1A, 0x05, 0x10, 0x05, 0x00, 0x0D, 0x00, 0x17, 0x08, 0x05, 0x16, 0x05,
    0x01, 0x05, 0x16, 0x0B, 0x01, 0x07, 0x01, 0x17, 0x01, 0x09, 0x01, 0x04,
    0x16, 0x04, 0x0C, 0x0F, 0x01, 0x1B, 0x01, 0x11, 0x0C, 0x17, 0x0C, 0x04,
    0x16, 0x13, 0x01, 0x06, 0x16, 0x06, 0x0C, 0x11, 0x07, 0x07, 0x04, 0x04,
    0x07, 0x1B, 0x1B, 0x1D, 0x07, 0x11, 0x06, 0x07, 0x17, 0x06, 0x06, 0x1B,
    0x06, 0x06, 0x04, 0x06, 0x11, 0x13, 0x04, 0x11, 0x17, 0x07, 0x17, 0x0B,
    0x17, 0x19, 0x05, 0x07, 0x17, 0x05, 0x07, 0x0D, 0x05, 0x05, 0x05, 0x0D,
    0x0D, 0x0F, 0x04, 0x0D, 0x17, 0x04, 0x05, 0x1B, 0x05, 0x06, 0x03, 0x03,
    0x16, 0x03, 0x03, 0x0C, 0x00, 0x00, 0x08, 0x1A, 0x02, 0x16, 0x1A, 0x1C,
    0x03, 0x10, 0x02, 0x03, 0x02, 0x1A, 0x02, 0x02, 0x12, 0x1C, 0x00, 0x1A,
    0x12, 0x1A, 0x10, 0x12, 0x00, 0x10, 0x03, 0x1A, 0x16, 0x1A, 0x16, 0x12,
    0x01, 0x02, 0x16, 0x18, 0x01, 0x03, 0x01, 0x1A, 0x01, 0x16, 0x01, 0x01,
    0x01, 0x00, 0x16, 0x00, 0x16, 0x0C, 0x16, 0x0E, 0x0C, 0x0E, 0x00, 0x0C
};

/* Brush bitmaps.  Each bitmap has four 8-byte pieces. */

static const uint8_t brush_bitmaps[256] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x60,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03,
    0x60, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x70, 0x70,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x07, 0x07,
    0x70, 0x70, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x07, 0x07, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x60, 0x78, 0x78, 0x7C, 0x7C,
    0x00, 0x00, 0x00, 0x03, 0x0F, 0x0F, 0x1F, 0x1F,
    0x7C, 0x7C, 0x78, 0x78, 0x60, 0x00, 0x00, 0x00,
    0x1F, 0x1F, 0x0F, 0x0F, 0x03, 0x00, 0x00, 0x00,
    0x00, 0x60, 0x7C, 0x7E, 0x7E, 0x7E, 0x7F, 0x7F,
    0x00, 0x03, 0x1F, 0x3F, 0x3F, 0x3F, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7E, 0x7E, 0x7E, 0x7C, 0x60, 0x00,
    0x7F, 0x7F, 0x3F, 0x3F, 0x3F, 0x1F, 0x03, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x40, 0x08, 0x20, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x09,
    0x28, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x20, 0x08, 0x24, 0x40, 0x12, 0x68, 0x60,
    0x00, 0x04, 0x01, 0x14, 0x00, 0x2B, 0x03, 0x27,
    0x62, 0x48, 0x22, 0x28, 0x00, 0x10, 0x40, 0x00,
    0x17, 0x09, 0x22, 0x15, 0x00, 0x0A, 0x00, 0x00
};

typedef struct {
    uint16_t offset;
    uint8_t value;
    bool fill_bg;
} byte_to_write;

static uint8_t *slow_vector_screenmem = NULL;

static byte_to_write **bytes_to_write = NULL;
static size_t write_ops_capacity = 100;

static size_t total_write_ops = 0;
static size_t current_write_op = 0;

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
        byte_to_write **new_ops = MemRealloc(bytes_to_write, write_ops_capacity * sizeof(byte_to_write *));
        bytes_to_write = new_ops;
    }
}

static void FreeOps(void)
{
    if (bytes_to_write == NULL)
        return;
    for (int i = 0; i < total_write_ops; i++)
        if (bytes_to_write[i] != NULL)
            free(bytes_to_write[i]);
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
        byte_to_write **new_pixels = MemRealloc(bytes_to_write, write_ops_capacity * sizeof(byte_to_write *));
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
    byte_to_write *op = MemAlloc(sizeof(byte_to_write));
    op->offset = offset;
    op->value = value;
    op->fill_bg = fill;
    ensure_capacity();
    bytes_to_write[total_write_ops++] = op;
}

#pragma mark FLOOD FILL

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

static bool is_pixel_white(a2_fill_ctx *ctx) {
    ctx->SCREEN_ADDRESS = CALC_APPLE2_ADDRESS(ctx->LINE);
    ctx->SCREEN_OFFSET = ctx->SCREEN_ADDRESS + ctx->STARTING_COLUMN;
    if (!is_valid_screen_offset(ctx->SCREEN_OFFSET))
        return false;
    uint8_t two_bits[7] = {0x3, 0x3, 0x6, 0xC, 0x18, 0x30, 0x60};
    uint8_t mask = two_bits[ctx->PIXELS];
    ctx->OLD_COLOR_VALUE = screenmem[ctx->SCREEN_OFFSET];
    return (mask & ctx->OLD_COLOR_VALUE) == mask;
}

static void draw_pattern_pixels(a2_fill_ctx *ctx) {
    uint16_t offset = ((ctx->COLUMN & 3) | ctx->HORIZ_PIXELS);
    uint8_t color_mask = pattern_data[offset];
    ctx->OLD_COLOR_VALUE = (ctx->OLD_COLOR_VALUE ^ ctx->FILLBYTE) & 0x7f;
    if (!is_valid_screen_offset(ctx->SCREEN_OFFSET))
        return;
    write_to_screenmem(ctx->SCREEN_OFFSET, ((ctx->FILLBYTE | 0x80) & color_mask) |
                       ctx->OLD_COLOR_VALUE, false);
}

/*
  Divides the 16-bit X coordinate by 7.  Produces an 8-bit quotient and an 8-bit
  remainder. Used to convert an X coordinate into a column number and pixel
  offset.
*/
static uint8_t divide_x_by_7(a2_fill_ctx *ctx, uint16_t x) {
    ctx->STARTING_COLUMN = x / COL_BITS;
    ctx->PIXELS = x % COL_BITS;
    return ctx->STARTING_COLUMN;
}

static bool find_top_seed_row(a2_fill_ctx *ctx)
{
    /* Move up until we find a pixel that's not white (0xff)
     * or we hit the top.
     */
    do {
        if (ctx->LINE == 0)
            return false; /* Found no non-white pixels */
        ctx->LINE--;
    } while (is_pixel_white(ctx));
    return true;
}

/*
 * Prepare ctx->FILLBYTE for painting at the current STARTING_X/PIXELS
 * and the current scanline (ctx->LINE). Also compute the left/right
 * edges used later by the expansion logic.
 */
static void get_fillbyte_and_edges(a2_fill_ctx *ctx,
                                     uint8_t *out_right_edge,
                                     uint8_t *out_left_edge)
{
    /* Choose pattern alignment for table lookup */
    int pattern_index = (ctx->LINE & 1) == 0 ? ctx->PATTERN_EVEN : ctx->PATTERN_ODD;
    ctx->HORIZ_PIXELS = pattern_index * 4;

    /* Work out rotations based on PIXELS */
    int8_t rotate_count = 6 - ctx->PIXELS;

    /* Simulate rotating left with carry (rotate_count + 1) times */
    uint16_t colorword = ctx->OLD_COLOR_VALUE << (rotate_count + 1);
    bool msb_flag = colorword & 0x100;
    ctx->FILLBYTE = (colorword & 0xff) | (colorword >> 9);

    /* If the rotation left some leading set bits, shift them out */
    while (rotate_count > 0 && msb_flag) {
        uint8_t tmp_loop = (uint8_t)(msb_flag << 7);
        msb_flag = (bool)(ctx->FILLBYTE & 1);
        ctx->FILLBYTE = (ctx->FILLBYTE >> 1) | tmp_loop;
        rotate_count--;
    }

    /* Align the byte and remember left shift used */
    ctx->FILLBYTE >>= rotate_count;
    *out_right_edge = rotate_count;

    /* Simulate rotating right with carry, (ctx->PIXELS + 1) times */
    uint16_t rotate_word = (ctx->FILLBYTE << 8) >> (ctx->PIXELS + 1);
    ctx->FILLBYTE = (rotate_word >> 8) | (rotate_word << 1);

    /* If PIXELS > 0, ensure FILLBYTE is extended appropriately */
    uint8_t remaining_bits = ctx->PIXELS;
    uint8_t loop_byte = 1;
    while (remaining_bits > 0) {
        if (loop_byte == 0) {
            break;
        }
        loop_byte = ctx->FILLBYTE >> 7;
        ctx->FILLBYTE = (ctx->FILLBYTE << 1) | 1;
        remaining_bits--;
    }

    *out_left_edge = remaining_bits;

    /* In case we broke out of the loop while there
     were still remaining bits */
    ctx->FILLBYTE <<= remaining_bits;
}

/*  Expand right from the seed point */
static uint8_t expand_fill_right(a2_fill_ctx *ctx, uint8_t has_more, uint8_t right_edge) {
    while (has_more && (ctx->COLUMN + 1) < APPLE2_SCREEN_COLS) {
        ctx->COLUMN++;
        ctx->SCREEN_OFFSET = ctx->SCREEN_ADDRESS + ctx->COLUMN;

        if (!is_valid_screen_offset(ctx->SCREEN_OFFSET)) break;

        ctx->OLD_COLOR_VALUE = screenmem[ctx->SCREEN_OFFSET];
        ctx->FILLBYTE = ctx->OLD_COLOR_VALUE;
        uint8_t rotations = 8;
        bool carry_bit = false;
        while (rotations > 0) {
            uint8_t carry_out = ctx->FILLBYTE & 1;
            ctx->FILLBYTE = (ctx->FILLBYTE  >> 1) | (carry_bit ? 0x80 : 0);
            carry_bit = carry_out;
            rotations--;
            if (!carry_out) break; // found a zero LSB
        }
        ctx->FILLBYTE >>= (rotations + 1);

        has_more = ((ctx->FILLBYTE & RIGHT_EXPAND_MASK) != 0);
        right_edge = rotations;

        draw_pattern_pixels(ctx);
    }
    return right_edge;
}

/* Expand left from the seed point */
static bool expand_fill_left(a2_fill_ctx *ctx, uint8_t *left_edge, bool seed_bit) {
    ctx->COLUMN--;
    ctx->SCREEN_OFFSET = ctx->SCREEN_ADDRESS + ctx->COLUMN;
    if (!is_valid_screen_offset(ctx->SCREEN_OFFSET)) return seed_bit;
    ctx->OLD_COLOR_VALUE = screenmem[ctx->SCREEN_OFFSET];

    /* Rotate left to find first zero */
    ctx->FILLBYTE = ctx->OLD_COLOR_VALUE << 1;
    uint8_t run_bits = 0;
    for (int remaining = COL_BITS - 1; remaining >= 0; remaining--) {
        uint8_t top = (uint8_t)(-((int8_t)ctx->FILLBYTE >> 7));
        ctx->FILLBYTE = (ctx->FILLBYTE << 1) | run_bits;
        if (top == 0) {
            run_bits = 0;
            ctx->FILLBYTE <<= remaining;
            seed_bit = false;
            *left_edge = remaining;
            break;
        }
        run_bits = top;
    }
    /* Finish the bit manipulation. Store the result in FILLBYTE */
    ctx->FILLBYTE = ctx->FILLBYTE << 1 | run_bits;
    draw_pattern_pixels(ctx);
    return seed_bit;
}

void compare_a2_screen_memory(const char *filename) {
    fprintf(stderr, "compare_a2_screen_memory: Comparison with file %s\n", filename);
    size_t size;
    uint8_t *screen_dump =
    ReadFileIfExists(filename, &size);
    if (screen_dump == NULL || size == 0) {
        fprintf(stderr, "Bad file!\n");
        return;
    }
    for (int i = 0; i < A2_SCREEN_MEM_SIZE; i++) {
        if (screen_dump[i] != screenmem[i]) {
            fprintf(stderr, "Mismatch at 0x%x: expected 0x%x, got 0x%x\n", i, screen_dump[i], screenmem[i]);
        }
    }
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

void wait_for_key_a2(void) {
    glk_cancel_char_event(Graphics);
    glk_request_char_event(Graphics);
    event_t ev;
    int result = 0;
    do {
        glk_select(&ev);
        if (ev.type == evtype_CharInput) {
            if (ev.val1 == keycode_Return) {
                result = 1;
            } else  if (ev.val1 == 'c') {
                compare_a2_screen_memory("/Users/administrator/mame/border.bin");
            }
            glk_request_char_event(Graphics);
        }
    } while (result == 0);
}

static void apple2_flood_fill(uint16_t x, uint8_t y, uint8_t color)
{
    uint8_t right_edge_pixels = 0;
    uint8_t left_edge_pixels = 0;
    bool seed_bit = 0;
    uint8_t has_more = 0;

    a2_fill_ctx ctx = {};

    ctx.FILL_COLOR = color;
    ctx.LINE = y;

    /* configure pattern colors */
    ctx.PATTERN_EVEN = color_pattern_subindices[(uint8_t)(ctx.FILL_COLOR * 2)];
    ctx.PATTERN_ODD  = color_pattern_subindices[(uint8_t)(ctx.FILL_COLOR * 2 + 1)];

    /* get col/pixel for this X position (in ctx.STARTING_X). */
    ctx.STARTING_COLUMN = divide_x_by_7(&ctx, x);

    /* Find the topmost seed row to start fill from. */
    if (find_top_seed_row(&ctx)) {
        /* Move back down to the last white pixel we saw. */
        /* Unless we are at row 0 and it is white */
        ctx.LINE++;
    }
    /* Main scan loop: find next seed scanline and paint horizontal spans */
    for (;;) {
        /* Bail if we've reached the bottom, or if the initial pixel is not white */
        if (ctx.LINE >= APPLE2_SCREEN_HEIGHT || !is_pixel_white(&ctx))
            return;

        /* Work out FILLBYTE and edge pixels for this scanline */
        get_fillbyte_and_edges(&ctx, &right_edge_pixels, &left_edge_pixels);

        seed_bit = ctx.FILLBYTE & 1;
        has_more = ((ctx.FILLBYTE & RIGHT_EXPAND_MASK) != 0);

        /* Paint starting column */
        ctx.COLUMN = ctx.STARTING_COLUMN;
        draw_pattern_pixels(&ctx);

        /* Expand to the right while more columns and pixels remain. Keep track of right edge pixels */
        right_edge_pixels = expand_fill_right(&ctx, has_more, right_edge_pixels);

        /* remember rightmost painted column for later left-scan offset calculation */
        uint8_t rightmost_column = ctx.COLUMN;

        ctx.COLUMN = ctx.STARTING_COLUMN;

        /* Expand left from the starting spot. We'll loop until left expansion cannot proceed. */
        while (1) {
            /* If the seed bit is set and we can move left, attempt to expand */
            if (seed_bit != 0 && ctx.COLUMN > 0) {
                /* continue expanding left as long as FILLBYTE LSB remains set */
                seed_bit = expand_fill_left(&ctx, &left_edge_pixels, seed_bit);
                /* break out of two loops by abusing the for (;;) loop */
                continue;
            }

            /* Line is filled.  Compute a new center X coord, by adding the left/right column numbers, multiplying by 7 to get a pixel column, then dividing by two. */
            int8_t filled_columns = rightmost_column - ctx.COLUMN - 1;
            uint8_t midpoint = (filled_columns < 0) ? COL_BITS : (filled_columns * COL_BITS + 2 * COL_BITS);

            /* Subtract the edge pixels and divide by two */
            midpoint = (uint8_t)(midpoint - right_edge_pixels) - (right_edge_pixels > midpoint);
            midpoint = (uint8_t)(midpoint - left_edge_pixels) / 2;

            /* Advance by (midpoint / 7) columns */
            ctx.STARTING_COLUMN = ctx.COLUMN + (midpoint / COL_BITS);
            uint8_t remainder = midpoint % COL_BITS;

            /* Adjust by any remainder pixels */
            ctx.PIXELS = (left_edge_pixels + remainder) % COL_BITS;
            ctx.STARTING_COLUMN += (left_edge_pixels + remainder) / COL_BITS;

            /* After finishing this line, we continue scanning downward */
            ctx.LINE++;
            break;
        }
        /* main loop continues */
    }
}

#pragma mark SHAPE DRAWING

static void rotate_brush_bits(uint8_t shape_bits, uint8_t num_bits, uint8_t *out_a, uint8_t *out_b)
{
    /* shift once to get high bit out of the way */
    uint16_t bits_to_draw = (uint8_t)(shape_bits << 1);

    bits_to_draw <<= num_bits;
    *out_a = bits_to_draw >> 1 | 0x80;
    *out_b = bits_to_draw >> 8 | 0x80;
}

static void draw_bitmap(a2_fill_ctx *ctx)
{
    for (int row = 0; row < 8; row++) {
        /* calculate row base address */
        uint16_t address = CALC_APPLE2_ADDRESS(ctx->LINE) + ctx->STARTING_COLUMN;
        /* add byte offset to address. Now address points to
         the first byte we want to change */
        if (!is_valid_screen_offset(address))
            continue;
        if ((ctx->BITMAP_INDEX + row) > sizeof(brush_bitmaps))
            return;
        /* get a byte from the bitmap */
        uint8_t brush_bitmap = brush_bitmaps[ctx->BITMAP_INDEX + row];

        uint8_t mask_a, mask_b;
        rotate_brush_bits(brush_bitmap, ctx->PIXELS, &mask_a, &mask_b);

        // Choose fill pattern based on row parity
        uint8_t fill_pattern = (ctx->LINE & 1) ? ctx->PATTERN_ODD : ctx->PATTERN_EVEN;
        ctx->HORIZ_PIXELS = fill_pattern << 2;

        uint8_t mask_c = mask_a;
        uint8_t mask_d = mask_b;

        /* Iterate twice to write the low and then the high pattern byte */
        for (int col_offset = 0; col_offset <= 1; col_offset++) {
            // reduce the byte offset to 0-3 */
            int pattern_offset = ctx->HORIZ_PIXELS + ((ctx->STARTING_COLUMN + col_offset) & 3);
            if (pattern_offset >= 120)
                return;

            uint8_t pattern = pattern_data[pattern_offset];

            if (col_offset == 0) {
                /* mask off whatever isn't in the color pattern */
                mask_a &= pattern;
                if (mask_c != 0x80) {
                    /* write the low pattern byte */
                    write_to_screenmem(address, ((mask_c | screenmem[address]) ^ mask_c) | mask_a, false);
                }
            } else {
                /* mask off whatever isn't in the color pattern */
                mask_b &= pattern;
            }
        }

        if (mask_d != 0x80) {
            /* write the high byte */
            write_to_screenmem(address + 1, ((mask_d | screenmem[address + 1]) ^ mask_d) | mask_b, false);
        }
        /* move down a line and loop */
        ctx->LINE++;
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
    ctx->STARTING_COLUMN += column_offset;
    ctx->LINE += line_offset;

    draw_bitmap(ctx);

    /* Restore original column and line after drawing */
    ctx->STARTING_COLUMN -= column_offset;
    /* The draw_bitmap() call added 8 lines */
    ctx->LINE -= line_offset + BRUSH__PART_HEIGHT;
}

static void draw_brush(uint16_t x, uint8_t y, Brush brush, uint8_t color) {
    a2_fill_ctx ctx = {};

    /* Initialize the context */
    ctx.LINE = y;
    ctx.BRUSH_INDEX = brush;
    ctx.FILL_COLOR = color;
    ctx.PATTERN_EVEN = color_pattern_subindices[ctx.FILL_COLOR * 2];
    ctx.PATTERN_ODD = color_pattern_subindices[ctx.FILL_COLOR * 2 + 1];

    /* Split X coordinate into byte/pixel */
    ctx.STARTING_COLUMN = divide_x_by_7(&ctx, x);
    ctx.BITMAP_INDEX = ctx.BRUSH_INDEX * BRUSH_PART_SIZE * BRUSH_TOTAL_PARTS;

    if (ctx.BITMAP_INDEX + BRUSH_PART_SIZE * BRUSH_TOTAL_PARTS > sizeof(brush_bitmaps)) {
        fprintf(stderr, "Error: Brush bitmap index out of bounds!\n");
        return;
    }

    /* Draw each quadrant of the brush */
    draw_brush_quadrant(&ctx, 0, 0);                   // Top-left
    ctx.BITMAP_INDEX += BRUSH_PART_SIZE;
    draw_brush_quadrant(&ctx, 1, 0);                   // Top-right
    ctx.BITMAP_INDEX += BRUSH_PART_SIZE;
    draw_brush_quadrant(&ctx, 0, BRUSH__PART_HEIGHT);  // Bottom-left
    ctx.BITMAP_INDEX += BRUSH_PART_SIZE;
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
        ctx->HGR_HORIZ = 0x27;
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
    if (ctx->HGR_HORIZ > 0x27) {
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
        if (ctx->HGR_Y > 191) ctx->HGR_Y = 0;
    } else {
        ctx->HGR_Y--;
        if (ctx->HGR_Y < 0) ctx->HGR_Y = 191;
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
    const int16_t delta_x = (target_x > ctx->HGR_X) ? (target_x - ctx->HGR_X) : (ctx->HGR_X - target_x);
    const int8_t delta_y = (target_y > ctx->HGR_Y) ? (target_y - ctx->HGR_Y) : (ctx->HGR_Y - target_y);

    /* Plot the initial pixel */
    plot_a2_vector_pixel(ctx);

    const int total_steps = (uint16_t)delta_x + (uint8_t)delta_y; // Total iterations of the loop
    if (total_steps == 0) return; // Single pixel, no line to draw

    /* The original sets the global HGR_DY to -delta_y - 1
       (but a carry of 1 is added every time it is used in calculations) */
    const int minus_delta_y = (uint8_t)(-delta_y - 1) + 1;

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
            /* We switch to vertical movement if the least siginificant byte
             of error_term - delta_y does not overflow 8 bits
             (In the original, carry set means move horizontally.) */
            uint16_t nocarry = ((uint8_t)error_term + minus_delta_y < CARRY_THRESHOLD) << 8;
            moving_vertically = error_term < nocarry;
            uint8_t e_low = (uint8_t)error_term + minus_delta_y;
            error_term = ((error_term - nocarry) & 0xff00) | e_low;
        }
        plot_a2_vector_pixel(ctx);
    }
    /* HGR_Y is already set (in move_up_or_down()), but not HGR_X */
    ctx->HGR_X = target_x;
}

#pragma mark Main draw routine

static bool doImageOp(uint8_t **outptr, a2_vector_ctx *ctx) {
    uint8_t *ptr = *outptr;
    uint8_t opcode;
    uint16_t a, b;

    opcode = *ptr++;

    uint8_t param = opcode & 0xf;
    opcode >>= 4;

//    fprintf(stderr, "doImageOp: opcode %d. param %d\n", opcode, param);

    switch (opcode) {
        case OPCODE_END: // 0
        case OPCODE_END2: // 7
            *outptr = ptr;
            return true;

        case OPCODE_SET_TEXT_POS: // 1, unused
            a = *ptr++ + (param & 1 ? 256 : 0);
            b = *ptr++;
            fprintf(stderr, "set_text_pos(%d, %d)\n", a, b);
            break;

        case OPCODE_SET_PEN_COLOR: // 2
            set_color(param, ctx);
            break;

        case OPCODE_TEXT_OUTLINE: // 5, unused
            fprintf(stderr, "OPCODE_TEXT_OUTLINE\n");
        case OPCODE_TEXT_CHAR: // 3, unused
            a = *ptr++;
            if (a < 0x20 || a >= 0x7f) {
                fprintf(stderr, "Invalid character - %c\n", a);
                a = '?';
            }
            fprintf(stderr, "draw_char(%c)\n", a);
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

        case OPCODE_DRAW_BOX: // 9, unused
            a = *ptr++ + (param & 1 ? 256 : 0);
            b = *ptr++;
            fprintf(stderr,  "draw_box (%d, %d) - (%d, %d)\n",
                    ctx->HGR_X, ctx->HGR_Y, a, b);
            break;

        case OPCODE_DRAW_LINE: // 10
            a = *ptr++ + (param & 1 ? 256 : 0);
            b = *ptr++;
//            fprintf(stderr,  "drawing line 0x%x 0x%x (%d, %d) - (%d, %d) (%d, %d)\n", a, b,
//                    ctx->HGR_X, ctx->HGR_Y, a, b, a - ctx->HGR_X, b - ctx->HGR_Y);

            draw_apple2_line(a, b, ctx);
            break;

        case OPCODE_DRAW_CIRCLE: // 11, unused
            a = *ptr++;
            fprintf(stderr,  "draw_circle (%d, %d) diameter=%d\n",
                    ctx->HGR_X, ctx->HGR_Y, a);
            break;

        case OPCODE_DRAW_SHAPE: // 12
            a = *ptr++ + (param & 1 ? 256 : 0);
            b = *ptr++;
            draw_brush(a, b, ctx->BRUSH, ctx->FILL_COLOR);
            break;

        case OPCODE_DELAY: // 13, unused
            fprintf(stderr,   "OPCODE_DELAY: 0x%x\n", *ptr++);
            break;

        case OPCODE_PAINT: // 14
            a = *ptr++ + (param & 1 ? 256 : 0);
            b = *ptr++;
            if (opcode & 0x1)
                a += 255;
            apple2_flood_fill(a, b, ctx->FILL_COLOR);
            break;

        case OPCODE_RESET: // 15, unused
            a = *ptr++;
            fprintf(stderr, "OPCODE_RESET: %x\n", a);
            return true;
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

static int write_pixel(byte_to_write towrite, int current_op_index) {
    slow_vector_screenmem[towrite.offset] = towrite.value;
    DrawSingleApple2ImageByte(slow_vector_screenmem, towrite.offset);
    if (current_op_index + 1 < total_write_ops) {
        byte_to_write next_byte = *bytes_to_write[current_op_index + 1];
        if ((next_byte.offset >= towrite.offset - 1 && next_byte.offset <= towrite.offset + 1) || next_byte.value == towrite.value) {
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
        int i = current_write_op;
        if (from_start) {
            i = 0;
            shrink_capacity();
        }

        int keep_going = 0;

        for (; i < total_write_ops && (!gli_slowdraw || i < current_write_op + 50 || keep_going); i++) {
            byte_to_write towrite = *bytes_to_write[i];
            if (towrite.fill_bg) {
                white_background();
            } else {
                keep_going = write_pixel(towrite, i);
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
    bytes_to_write = MemAlloc(write_ops_capacity * sizeof(byte_to_write *));
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
