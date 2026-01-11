//
//  apple2_vector_draw.c
//  scott
//
//  Created by Administrator on 2025-12-29.
//

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "scott.h"
#include "apple2draw.h"
#include "apple2_vector_draw.h"
#include "common_file_utils.h"

#define APPLE2_SCREEN_HEIGHT 192
#define APPLE2_SCREEN_COLS 40
#define BITS_PER_BYTE 8
#define COL_BITS 7
#define RIGHT_EXPAND_MASK 0x40


extern uint8_t *screenmem;

#define CALC_APPLE2_ADDRESS(y) (((y / 8) & 0x07) << 7) + (((y / 8) & 0x18) * 5) + ((y & 7) << 10)


typedef enum {
    SHAPE_PIXEL = 0,
    SHAPE_BOX = 1,
    SHAPE_CIRCLE_TINY = 2,
    SHAPE_CIRCLE_SMALL = 3,
    SHAPE_CIRCLE_MED = 4,
    STANDARD_SHAPE = 5,
    SHAPE_A = 6,
    SHAPE_SPRAY = 7
} Shape;

typedef struct {
    uint16_t HBAS;
    uint16_t HGR_X;
    uint8_t HGR_Y;
    uint8_t HMASK; // mask to apply to screen address when plotting pixel. masktable[xpos % 7]
    uint8_t HGR_BITS; // mask to apply to screen address when drawing color
    uint8_t HGR_HORIZ; // column position
    uint8_t HGR_COLOR; // current pen color
    Shape SHAPE; // current shape type
    uint8_t FILL_COLOR; // current fill
} pixelpos_ctx;

typedef struct {
    uint8_t LINE;
    uint8_t STARTING_Y;
    uint16_t STARTING_X;
    uint8_t PATTERN_EVEN;
    uint8_t PATTERN_ODD;
    uint8_t FILL_COLOR;
    uint16_t SCREEN_OFFSET;
    uint16_t HBAS;
    uint8_t HGR_PAGE;
    uint8_t COLUMN;
    uint8_t STARTING_COLUMN;
    uint8_t OLD_COLOR_VALUE;
    uint8_t NUMBITS;
    uint8_t FILLBYTE;
    uint8_t HORIZ_PIXELS;
    uint8_t SHAPE_OFFSET;
    Shape SHAPE;
} flood_fill_ctx;

static void set_draw_position(uint16_t xpos, uint8_t ypos, pixelpos_ctx *ctx)
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

static uint16_t get_screen_address(flood_fill_ctx *ctx)
{
    ctx->HBAS = CALC_APPLE2_ADDRESS(ctx->LINE);
    return ctx->HBAS;
}

static inline bool is_valid_screen_offset(uint16_t offset) {
    return offset <= MAX_SCREEN_ADDR;
}

static bool is_pixel_white(flood_fill_ctx *ctx)
{
    uint8_t bVar1;
    uint8_t bVar2;
    uint8_t bVar3;
    uint8_t bVar4;

    get_screen_address(ctx);
    ctx->SCREEN_OFFSET = ctx->HBAS + ctx->STARTING_COLUMN;
    if (!is_valid_screen_offset(ctx->SCREEN_OFFSET))
        return false;
    ctx->OLD_COLOR_VALUE = screenmem[ctx->SCREEN_OFFSET];
    bVar4 = ctx->OLD_COLOR_VALUE & 1;
    bVar2 = ctx->OLD_COLOR_VALUE >> 1;
    bVar3 = ctx->NUMBITS;
    if (ctx->NUMBITS != 0) {
        while (--bVar3 != 0) {
            bVar1 = bVar4 << 7;
            bVar4 = bVar2 & 1;
            bVar2 = bVar2 >> 1 | bVar1;
        }
    }
    if (bVar4 != 0) {
        bVar3 = bVar2 & 1;
        if (bVar3 != 0) {
            return true;
        }
    }
    return false;
}

static const uint8_t FILL_COLOR_ARRAY_9054[128] =  { 0x00, 0x00, 0x00, 0x00, 0x55, 0x2A, 0x55, 0x2A, 0x2A, 0x55, 0x2A, 0x55, 0x7F, 0x7F, 0x7F, 0x7F, 0x80, 0x80, 0x80, 0x80, 0xD5, 0xAA, 0xD5, 0xAA, 0xAA, 0xD5, 0xAA, 0xD5, 0xFF, 0xFF, 0xFF, 0xFF, 0x33, 0x66, 0x4C, 0x19, 0xB3, 0xE6, 0xCC, 0x99, 0x4C, 0x19, 0x33, 0x66, 0xCC, 0x99, 0xB3, 0xE6, 0x11, 0x22, 0x44, 0x08, 0x91, 0xA2, 0xC4, 0x88, 0x44, 0x08, 0x11, 0x22, 0xC4, 0x88, 0x91, 0xA2, 0x22, 0x44, 0x08, 0x11, 0xA2, 0xC4, 0x88, 0x91, 0x08, 0x11, 0x22, 0x44, 0x88, 0x91, 0xA2, 0xC4, 0xC9, 0xA4, 0x92, 0x89, 0x24, 0x12, 0x49, 0x24, 0x77, 0x6E, 0x5D, 0x3B, 0xF7, 0xEE, 0xDD, 0xBB, 0x5D, 0x3B, 0x77, 0x6E, 0xDD, 0xBB, 0xF7, 0xEE, 0x6E, 0x5D, 0x3B, 0x77, 0xEE, 0xDD, 0xBB, 0xF7, 0x3B, 0x77, 0x6E, 0x5D, 0xBB, 0xF7, 0xEE, 0xDD, 0xA9, 0x00, 0x8D, 0x53, 0x8F, 0xAD, 0x56, 0x8F };

static void plot_fill_color(flood_fill_ctx *ctx) {
    uint16_t offset = ((ctx->COLUMN & 3) | ctx->HORIZ_PIXELS);
    uint8_t color_mask = FILL_COLOR_ARRAY_9054[offset];
    ctx->OLD_COLOR_VALUE = (ctx->OLD_COLOR_VALUE ^ ctx->FILLBYTE) & 0x7f;
    if (!is_valid_screen_offset(ctx->SCREEN_OFFSET))
        return;
    screenmem[ctx->SCREEN_OFFSET] = ((ctx->FILLBYTE | 0x80) & color_mask) |
    ctx->OLD_COLOR_VALUE;
}

/* work out starting column and remainder bits from STARTING_X */
/* In Apple 2 screen memory, every column uses 7 bits */
static void calculate_xpos(flood_fill_ctx *ctx) {
    ctx->STARTING_COLUMN = ctx->STARTING_X / COL_BITS;
    ctx->NUMBITS = ctx->STARTING_X % COL_BITS;
}

static const uint8_t ARRAY_8f7c[256] = {
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
    0x01, 0x00, 0x16, 0x00, 0x16, 0x0C, 0x16, 0x0E, 0x0C, 0x0E, 0x00, 0x0C,
    0x00, 0x00, 0x00, 0x00, 0x55, 0x2A, 0x55, 0x2A, 0x2A, 0x55, 0x2A, 0x55,
    0x7F, 0x7F, 0x7F, 0x7F, 0x80, 0x80, 0x80, 0x80, 0xD5, 0xAA, 0xD5, 0xAA,
    0xAA, 0xD5, 0xAA, 0xD5, 0xFF, 0xFF, 0xFF, 0xFF, 0x33, 0x66};


/* Helper: rotate value left `times` iterations, propagating carry.
 * `times` may be 0. `carry` is updated in-place.
 */
static inline bool rotate_left_with_carry(uint8_t *value, bool *carry, int times)
{
    uint8_t v = *value;
    bool new_c = v >> 7;
    if (times <= 0) return new_c;
    bool c = *carry;
    for (int i = 0; i < times; i++) {
        new_c = (v >> 7) & 1;
        v = (uint8_t)(v << 1) | (c ? 1u : 0u);
        c = new_c;
    }
    *value = v;
    *carry = c;
    return new_c;
}

/* Helper: rotate value right `times` iterations, propagating carry.
 */
static inline bool rotate_right_with_carry(uint8_t *value, bool *carry, int times)
{
    uint8_t v = *value;
    bool new_c = v & 1;
    if (times <= 0) return new_c;

    bool c = *carry;
    for (int i = 0; i < times; i++) {
        bool new_c = v & 1;
        v = (uint8_t)(v >> 1) | (c ? 0x80u : 0u);
        c = new_c;
    }
    *value = v;
    *carry = c;
    return new_c;
}

static bool find_top_seed_row(flood_fill_ctx *ctx)
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
 * Prepare ctx->FILLBYTE for painting at the current STARTING_X/NUMBITS
 * and the current scanline (ctx->LINE). Also compute the left/right
 * shift counts used later by the expansion logic.
 */
static void prepare_fillbyte_for_seed(flood_fill_ctx *ctx,
                                     uint8_t *out_left_shift,
                                     uint8_t *out_right_shift,
                                     bool *out_seed_bit,
                                     uint8_t *out_has_more)
{
    /* Choose pattern alignment for table lookup (keeps existing behavior) */
    int chosen_pattern_char = (ctx->LINE & 1) == 0 ? ctx->PATTERN_EVEN : ctx->PATTERN_ODD;
    ctx->HORIZ_PIXELS = chosen_pattern_char * 4;

    /* Work out rotations based on NUMBITS */
    uint8_t rotate_count = 6 - ctx->NUMBITS;
    uint8_t colorbyte = ctx->OLD_COLOR_VALUE;
    bool carry = false;

    /* Rotate left (with carry) rotate_count + 1 times */
    bool msb_flag = rotate_left_with_carry(&colorbyte, &carry, rotate_count + 1);

    /* If the rotation left some leading set bits, shift them out */
    while (rotate_count > 0 && msb_flag) {
        uint8_t tmp_loop = (uint8_t)(msb_flag << 7);
        msb_flag = (bool)(colorbyte & 1);
        colorbyte = (colorbyte >> 1) | tmp_loop;
        rotate_count--;
    }

    /* Align the byte and remember left shift used */
    if (rotate_count > 0)
        colorbyte >>= rotate_count;
    *out_left_shift = rotate_count;

    /* Build FILLBYTE as a right-aligned 7-bit chunk for PREVIOUS_COLUMN */
    carry = (colorbyte & 1) != 0;
    ctx->FILLBYTE = colorbyte >> 1;
    rotate_right_with_carry(&ctx->FILLBYTE, &carry, ctx->NUMBITS);

    /* If NUMBITS > 0, ensure FILLBYTE is extended appropriately */
    uint8_t remaining_bits = ctx->NUMBITS;
    uint8_t loop_byte = 1;
    while (remaining_bits > 0) {
        if (loop_byte == 0) {
            break;
        }
        loop_byte = ctx->FILLBYTE >> 7;
        ctx->FILLBYTE = (ctx->FILLBYTE << 1) | 1;
        remaining_bits--;
    }
    if (loop_byte == 0) {
        /* we broke out of the loop while there were
           still remaining bits */
        ctx->FILLBYTE <<= remaining_bits;
        *out_right_shift = remaining_bits;
    } else {
        *out_right_shift = 0;
    }

    /* seed bit is LSB; has_more is bit 6 (0x40) */
    *out_seed_bit = ctx->FILLBYTE & 1;
    *out_has_more = ((ctx->FILLBYTE & 0x40) != 0);
}

/*  Expand right from the seed point */
static uint8_t expand_fill_right(flood_fill_ctx *ctx, uint8_t has_more, uint8_t left_shift) {
    while (has_more && (ctx->COLUMN + 1) < APPLE2_SCREEN_COLS) {
        ctx->COLUMN++;
        ctx->SCREEN_OFFSET = ctx->HBAS + ctx->COLUMN;

        if (!is_valid_screen_offset(ctx->SCREEN_OFFSET)) break;
        ctx->OLD_COLOR_VALUE = screenmem[ctx->SCREEN_OFFSET];
        uint8_t colorbyte = ctx->OLD_COLOR_VALUE;
        uint8_t rotations = BITS_PER_BYTE;
        bool carry_bit = false;
        while (rotations > 0) {
            uint8_t carry_out = colorbyte & 1;
            colorbyte = (colorbyte >> 1) | (carry_bit ? 0x80 : 0);
            carry_bit = carry_out;
            rotations--;
            if (!carry_out) break; // found a zero LSB
        }
        colorbyte >>= (rotations + 1);
        ctx->FILLBYTE = colorbyte;
        has_more = ((ctx->FILLBYTE & RIGHT_EXPAND_MASK) != 0);
        left_shift = rotations;

        plot_fill_color(ctx);
    }
    return left_shift;
}

/* Expand left from the seed point */
static bool expand_fill_left(flood_fill_ctx *ctx, uint8_t *right_shift, bool seed_bit) {
    ctx->COLUMN--;
    ctx->SCREEN_OFFSET = ctx->HBAS + ctx->COLUMN;
    if (!is_valid_screen_offset(ctx->SCREEN_OFFSET)) return seed_bit;
    ctx->OLD_COLOR_VALUE = screenmem[ctx->SCREEN_OFFSET];

    /* Rotate left to find first zero */
    uint8_t colorbyte = ctx->OLD_COLOR_VALUE << 1;
    uint8_t remaining = COL_BITS;
    uint8_t run_bits = 0;
    while (remaining > 0) {
        uint8_t top = (uint8_t)(-((int8_t)colorbyte >> 7));
        colorbyte = (colorbyte << 1) | run_bits;
        remaining--;
        if (top == 0) {
            run_bits = 0;
            colorbyte <<= remaining;
            seed_bit = false;
            *right_shift = remaining;
            break;
        }
        run_bits = top;
    }
    /* colorbyte is ready; compute new FILLBYTE */
    ctx->FILLBYTE = colorbyte << 1 | run_bits;
    plot_fill_color(ctx);
    return seed_bit;
}

static void apple2_flood_fill(flood_fill_ctx *ctx)
{
    uint8_t left_shift = 0;
    uint8_t right_shift = 0;
    bool seed_bit = 0;
    uint8_t has_more = 0;

    /* set fill patterns */
    ctx->PATTERN_EVEN = ARRAY_8f7c[(uint8_t)(ctx->FILL_COLOR * 2)];
    ctx->PATTERN_ODD  = ARRAY_8f7c[(uint8_t)(ctx->FILL_COLOR * 2 + 1)];

    /* compute STARTING_COLUMN and NUMBITS from STARTING_X */
    calculate_xpos(ctx);

    /* Find the topmost seed row to start fill from. */
    if (find_top_seed_row(ctx)) {
        /* Start below the first non-white pixel encountered */
        /* If we didn't encounter one, start at zero */
        ctx->LINE++;
    }
    /* Main scan loop: find next seed scanline and paint horizontal spans */
    for (;;) {
        if (ctx->LINE >= APPLE2_SCREEN_HEIGHT || !is_pixel_white(ctx))
            return;

        /* Work out FILLBYTE and shift counts for this scanline */
        prepare_fillbyte_for_seed(ctx, &left_shift, &right_shift, &seed_bit, &has_more);

        /* Paint starting column */
        ctx->COLUMN = ctx->STARTING_COLUMN;
        plot_fill_color(ctx);

        /* Expand to the right while more columns and pixels remain. Keep track of left shift of FILLBYTE */
        left_shift = expand_fill_right(ctx, has_more, left_shift);

        /* remember rightmost painted column for later left-scan offset calculation */
        uint8_t rightmost_column = ctx->COLUMN;

        ctx->COLUMN = ctx->STARTING_COLUMN;

        /* Expand left from the starting spot. We'll loop until left expansion cannot proceed. */
        while (1) {
            /* If the seed bit is set and we can move left, attempt to expand */
            if (seed_bit != 0 && ctx->COLUMN > 0) {
                /* continue expanding left as long as FILLBYTE LSB remains set */
                seed_bit = expand_fill_left(ctx, &right_shift, seed_bit);
                continue;
            }

            /* Can't expand left further. Compute offsets for next seed on the same scanline */
            int8_t gap_columns = rightmost_column - ctx->COLUMN - 1;
            uint8_t work = (gap_columns < 0) ? COL_BITS : (gap_columns * COL_BITS + 2 * COL_BITS);

            /* adjust by left/right shifts computed earlier */
            work = (uint8_t)(work - left_shift) - (left_shift > work);
            work -= right_shift;
            work >>= 1;

            /* Advance STARTING_COLUMN by (work / 7), set up NUMBITS for next scanline */
            ctx->STARTING_COLUMN = ctx->COLUMN + (work / COL_BITS);
            uint8_t remainder = work % COL_BITS;

            ctx->NUMBITS = (right_shift + remainder) % COL_BITS;
            ctx->STARTING_COLUMN += (right_shift + remainder) / COL_BITS;

            /* After finishing this scanline we continue scanning downward for next seed */
            ctx->LINE++;
            break;
        }
        /* main loop continues, scanning for next seed row */
    }
}

static const uint8_t shape_array[256] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x60,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x60, 0x40, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x70, 0x70, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x03, 0x07, 0x07, 0x70, 0x70, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x07, 0x07, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60,
    0x78, 0x78, 0x7C, 0x7C, 0x00, 0x00, 0x00, 0x03, 0x0F, 0x0F, 0x1F, 0x1F,
    0x7C, 0x7C, 0x78, 0x78, 0x60, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x0F, 0x0F,
    0x03, 0x00, 0x00, 0x00, 0x00, 0x60, 0x7C, 0x7E, 0x7E, 0x7E, 0x7F, 0x7F,
    0x00, 0x03, 0x1F, 0x3F, 0x3F, 0x3F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7E, 0x7E,
    0x7E, 0x7C, 0x60, 0x00, 0x7F, 0x7F, 0x3F, 0x3F, 0x3F, 0x1F, 0x03, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x40, 0x08, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x04, 0x00, 0x09, 0x28, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x08, 0x24,
    0x40, 0x12, 0x68, 0x60, 0x00, 0x04, 0x01, 0x14, 0x00, 0x2B, 0x03, 0x27,
    0x62, 0x48, 0x22, 0x28, 0x00, 0x10, 0x40, 0x00, 0x17, 0x09, 0x22, 0x15,
    0x00, 0x0A, 0x00, 0x00
};

static const uint8_t shape_array9054[128] = {
    0x00, 0x00, 0x00, 0x00, 0x55, 0x2A, 0x55, 0x2A, 0x2A, 0x55, 0x2A, 0x55,
    0x7F, 0x7F, 0x7F, 0x7F, 0x80, 0x80, 0x80, 0x80, 0xD5, 0xAA, 0xD5, 0xAA,
    0xAA, 0xD5, 0xAA, 0xD5, 0xFF, 0xFF, 0xFF, 0xFF, 0x33, 0x66, 0x4C, 0x19,
    0xB3, 0xE6, 0xCC, 0x99, 0x4C, 0x19, 0x33, 0x66, 0xCC, 0x99, 0xB3, 0xE6,
    0x11, 0x22, 0x44, 0x08, 0x91, 0xA2, 0xC4, 0x88, 0x44, 0x08, 0x11, 0x22,
    0xC4, 0x88, 0x91, 0xA2, 0x22, 0x44, 0x08, 0x11, 0xA2, 0xC4, 0x88, 0x91,
    0x08, 0x11, 0x22, 0x44, 0x88, 0x91, 0xA2, 0xC4, 0xC9, 0xA4, 0x92, 0x89,
    0x24, 0x12, 0x49, 0x24, 0x77, 0x6E, 0x5D, 0x3B, 0xF7, 0xEE, 0xDD, 0xBB,
    0x5D, 0x3B, 0x77, 0x6E, 0xDD, 0xBB, 0xF7, 0xEE, 0x6E, 0x5D, 0x3B, 0x77,
    0xEE, 0xDD, 0xBB, 0xF7, 0x3B, 0x77, 0x6E, 0x5D, 0xBB, 0xF7, 0xEE, 0xDD,
    0xA9, 0x00, 0x8D, 0x53, 0x8F, 0xAD, 0x56, 0x8F
};

static void rotate_shape_bits(uint8_t shape_bits, uint8_t num_bits, uint8_t *out_a, uint8_t *out_b)
{
    uint8_t mask1 = 0, mask2 = 0xfe, mask3 = 0;
    uint8_t bits_to_draw = shape_bits << 1;
    uint8_t carry = 0;
    for (int bit = 0; bit < num_bits; bit++) {
        uint8_t out_bit = bits_to_draw >> 7;
        bits_to_draw = (bits_to_draw << 1) | carry;
        carry = mask1 >> 7;
        mask1 = (mask1 << 1) | out_bit;
        out_bit = mask2 >> 7;
        mask2 = (mask2 << 1) | carry;
        carry = mask3 >> 7;
        mask3 = (mask3 << 1) | out_bit;
    }
    *out_a = (bits_to_draw >> 1) | 0x80;
    *out_b = (mask1 & 0x7f) | 0x80;
}

static void plot_shape(flood_fill_ctx *ctx)
{
    for (int row = 0; row < 8; row++) {
        uint16_t address = get_screen_address(ctx) + ctx->STARTING_COLUMN;
        if (!is_valid_screen_offset(address))
            continue;
        if ((ctx->SHAPE_OFFSET + row) > sizeof(shape_array))
            return;
        uint8_t shape_bits = shape_array[ctx->SHAPE_OFFSET + row];

        uint8_t shape_mask_a, shape_mask_b;
        rotate_shape_bits(shape_bits, ctx->NUMBITS, &shape_mask_a, &shape_mask_b);

        uint8_t shape_mask_c = shape_mask_a;
        uint8_t shape_mask_d = shape_mask_b;

        // Choose fill pattern based on row parity
        uint8_t fill_pattern = (ctx->LINE & 1) ? ctx->PATTERN_ODD : ctx->PATTERN_EVEN;
        ctx->HORIZ_PIXELS = fill_pattern << 2;

        // Apply shapes and fill to screen memory
        for (int col_offset = 0; col_offset <= 1; col_offset++) {
            int fill_offset = (col_offset + ctx->STARTING_COLUMN & 3) | ctx->HORIZ_PIXELS;

            uint8_t fill_mask = shape_array9054[fill_offset];

            if (col_offset == 0) {
                shape_mask_a &= fill_mask;
                if (shape_mask_c != 0x80) {
                    screenmem[address] =
                    ((shape_mask_c | screenmem[address]) ^ shape_mask_c) | shape_mask_a;
                }
            } else {
                shape_mask_b &= fill_mask;
            }
        }
        if (shape_mask_d != 0x80) {
            screenmem[address + 1] =
            ((shape_mask_d | screenmem[address + 1]) ^ shape_mask_d) | shape_mask_b;
        }
        ctx->LINE++;
    }
    ctx->LINE--;
}

static void draw_shape(flood_fill_ctx *ctx)
{
    ctx->PATTERN_EVEN = ARRAY_8f7c[ctx->FILL_COLOR * 2];
    ctx->PATTERN_ODD = ARRAY_8f7c[ctx->FILL_COLOR * 2 + 1];
    calculate_xpos(ctx);
    ctx->SHAPE_OFFSET = ctx->SHAPE << 5;
    plot_shape(ctx);
    ctx->SHAPE_OFFSET += 8;
    uint8_t previous_x = ctx->STARTING_COLUMN;
    ctx->STARTING_COLUMN++;
    ctx->LINE -= 7;
    plot_shape(ctx);
    ctx->SHAPE_OFFSET += 8;
    ctx->STARTING_COLUMN = previous_x;
    ctx->LINE++;
    plot_shape(ctx);
    ctx->SHAPE_OFFSET += 8;
    ctx->STARTING_COLUMN++;
    ctx->LINE -= 7;
    plot_shape(ctx);
}

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

static void move_left(pixelpos_ctx *ctx) {
    if ((ctx->HMASK & 1) == 0) {
        ctx->HMASK = ctx->HMASK >> 1 ^ 0xc0;
        return;
    }
    ctx->HGR_HORIZ--;
    if ((int8_t)ctx->HGR_HORIZ < 0) {
        ctx->HGR_HORIZ = 0x27;
    }
    ctx->HMASK = 0xc0;
    if ((ctx->HGR_BITS * 2 + 0x40) & 0x80) {
        ctx->HGR_BITS ^= 0x7f;
    }
}

static void move_right(pixelpos_ctx *ctx) {
    ctx->HMASK = ctx->HMASK << 1 ^ 0x80;
    if ((int8_t)ctx->HMASK < 0) {
        return;
    }
    ctx->HMASK = 0x81;
    ctx->HGR_HORIZ++;
    if (ctx->HGR_HORIZ > 0x27) {
        ctx->HGR_HORIZ = 0;
    }
    if ((ctx->HGR_BITS * 2 + 0x40) & 0x80) {
        ctx->HGR_BITS ^= 0x7f;
    }
}

static void move_up_or_down(bool down, pixelpos_ctx *ctx) {
    if (down) {
        ctx->HGR_Y++;
        if (ctx->HGR_Y > 191) ctx->HGR_Y = 0;
    } else {
        ctx->HGR_Y--;
        if (ctx->HGR_Y < 0) ctx->HGR_Y = 191;
    }
    ctx->HBAS = CALC_APPLE2_ADDRESS(ctx->HGR_Y);
}

static void move_left_or_right(bool left, pixelpos_ctx *ctx) {
    if (left)
        move_left(ctx);
    else
        move_right(ctx);
}

static void put_a2_vector_pixel(pixelpos_ctx *ctx) {
    uint16_t offset = ctx->HBAS + ctx->HGR_HORIZ;
    if (offset > MAX_SCREEN_ADDR)
        return;
    screenmem[offset] = ((screenmem[offset] ^ ctx->HGR_BITS) & ctx->HMASK) ^ screenmem[offset];
}

static void draw_apple2_line(uint16_t target_x, uint8_t target_y, pixelpos_ctx *ctx)
{
    bool move_down = target_y > ctx->HGR_Y;
    bool move_left = target_x < ctx->HGR_X;

    uint16_t delta_x = abs(target_x - ctx->HGR_X);
    uint8_t delta_y = abs(target_y - ctx->HGR_Y);

    // Draw pixel at initial position
    put_a2_vector_pixel(ctx);

    int iterations = delta_x + delta_y;
    // If we are only drawing one pixel, then we are done.
    if (iterations == 0)
        return;

    bool vertical_movement = ((uint8_t)delta_x + (uint8_t)(-delta_y - 1) + 1 <= 0xff);
    uint16_t e = delta_x - delta_y - vertical_movement * 0x100;

    for (int i = 0; i < iterations; i++) {
        if (vertical_movement) {
            move_up_or_down(move_down, ctx);
            vertical_movement = ((uint8_t)e + (uint8_t)delta_x <= 0xff);
            e += delta_x;
        } else {
            move_left_or_right(move_left, ctx);
            vertical_movement = ((uint8_t)e + (uint8_t)(-delta_y - 1) + 1 <= 0xff);
            e -= delta_y - vertical_movement * 0x100;
        }
        put_a2_vector_pixel(ctx);
    }

    ctx->HGR_X = target_x;
}


static void set_color(uint8_t color_index, pixelpos_ctx *ctx) {
    const uint8_t COLORTBL[8] = {0x00, 0x2a, 0x55, 0x7f, 0x80, 0xaa, 0xd5, 0xff};
    if (color_index < 8) {
        ctx->HGR_COLOR = COLORTBL[color_index];
    } else {
        fprintf(stderr, "SET_COLOR called with illegal color index %d\n", color_index);
    }
}

static bool doImageOp(uint8_t **outptr, pixelpos_ctx *ctx) {

    uint8_t *ptr = *outptr;
    uint8_t opcode;
    uint16_t a, b;

    opcode = *ptr++;

    uint8_t param = opcode & 0xf;
    opcode >>= 4;
    flood_fill_ctx flood_ctx = {};

    switch (opcode) {
        case OPCODE_END: // 0
        case OPCODE_END2: // 7
            *outptr = ptr;
            return true;

        case OPCODE_SET_TEXT_POS: // 1
            a = *ptr++ + (param & 1 ? 256 : 0);
            b = *ptr++;
            fprintf(stderr, "set_text_pos(%d, %d)\n", a, b);
            break;

        case OPCODE_SET_PEN_COLOR: // 2
            set_color(param, ctx);
            break;

        case OPCODE_TEXT_CHAR: // 3
        case OPCODE_TEXT_OUTLINE: // 5
            if (opcode == OPCODE_TEXT_OUTLINE)
                fprintf(stderr, "TODO: Implement drawing text outlines\n");
            a = *ptr++;
            if (a < 0x20 || a >= 0x7f) {
                fprintf(stderr, "Invalid character - %c\n", a);
                a = '?';
            }

            fprintf(stderr, "draw_char(%c)\n", a);
            break;

        case OPCODE_SET_SHAPE: // 4
                ctx->SHAPE = (Shape)param;
            break;

        case OPCODE_SET_FILL_COLOR: // 6
            a = *ptr++;
            ctx->FILL_COLOR = a; //pal[a];
            break;

        case OPCODE_MOVE_TO: // 8
            a = *ptr++ + (param & 1 ? 256 : 0);
            b = *ptr++;
            set_draw_position(a, b, ctx);
            break;

        case OPCODE_DRAW_BOX: // 9
            a = *ptr++ + (param & 1 ? 256 : 0);
            b = *ptr++;
            fprintf(stderr,  "draw_box (%d, %d) - (%d, %d)\n",
                    ctx->HGR_X, ctx->HGR_Y, a, b);
            break;
#pragma mark draw line
        case OPCODE_DRAW_LINE: // 10
            a = *ptr++ + (param & 1 ? 256 : 0);
            b = *ptr++;
            draw_apple2_line(a, b, ctx);
            break;

        case OPCODE_DRAW_CIRCLE: // 11
            a = *ptr++;
            fprintf(stderr,  "draw_circle (%d, %d) diameter=%d\n",
                    ctx->HGR_X, ctx->HGR_Y, a);
            break;

        case OPCODE_DRAW_SHAPE: // 12
            a = *ptr++ + (param & 1 ? 256 : 0);
            b = *ptr++;
            flood_ctx.FILL_COLOR = ctx->FILL_COLOR;
            flood_ctx.SHAPE = ctx->SHAPE;
            flood_ctx.LINE = b;
            flood_ctx.STARTING_X = a;
            flood_ctx.HGR_PAGE = 0;

            draw_shape(&flood_ctx);
            break;

        case OPCODE_DELAY: // 13
                           // The original allowed for rendering to be paused briefly.
            fprintf(stderr,   "OPCODE_DELAY: 0x%x\n", *ptr++);
            break;
#pragma mark flood fill
        case OPCODE_PAINT: // 14
            a = *ptr++ + (param & 1 ? 256 : 0);
            b = *ptr++;
            if (opcode & 0x1)
                a += 255;

            flood_ctx.FILL_COLOR = ctx->FILL_COLOR;
            flood_ctx.STARTING_X = a;
            flood_ctx.LINE = b;
            flood_ctx.HGR_PAGE = 0;

            apple2_flood_fill(&flood_ctx);
            break;

        case OPCODE_RESET: // 15
            a = *ptr++;
            fprintf(stderr, "OPCODE_RESET: %x\n", a);
            return true;
    }

    *outptr = ptr;
    return false;
}

int DrawApple2VectorImage(USImage *img) {
    if (!img || !img->imagedata || img->datasize < 2)
        return 0;
    pixelpos_ctx ctx = {};

    uint8_t *vecdata = img->imagedata + 4;
    int done = 0;
    glk_window_clear(Graphics);
    if (screenmem == NULL) {
        screenmem = MemCalloc(SCREEN_MEM_SIZE);
    }
    if (img->usage == IMG_ROOM) {
        memset(screenmem,0xff,SCREEN_MEM_SIZE);
    }
    set_color(4, &ctx);
    set_draw_position(140,96, &ctx);
    ctx.FILL_COLOR = 0;
    ctx.SHAPE = STANDARD_SHAPE;

    while (done == 0 && vecdata < img->imagedata + img->datasize) {
        done = doImageOp(&vecdata, &ctx);
    }
    return 1;
}
