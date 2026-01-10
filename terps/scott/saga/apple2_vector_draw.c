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

extern uint8_t *screenmem;

#define CALC_APPLE2_ADDRESS(y) (((y / 8) & 0x07) << 7) + (((y / 8) & 0x18) * 5) + ((y & 7) << 10)

void HPOSN(uint16_t xpos, uint8_t ypos, pixelpos_ctx *ctx)

{
    ctx->HGR_X = xpos;
    ctx->HGR_Y = ypos;

    ctx->HBAS = CALC_APPLE2_ADDRESS(ypos);
    ctx->HGR_HORIZ = xpos / 7;
    const uint8_t masktable[7] = {0x81,0x82,0x84,0x88,0x90,0xa0,0xc0};
    ctx->HMASK = masktable[xpos % 7];
    ctx->HGR_BITS = ctx->HGR_COLOR;
    // If odd column, rotate bits
    if ((ctx->HGR_HORIZ & 1) == 1 && ((ctx->HGR_COLOR * 2 + 0x40) & 0x80)) {
        ctx->HGR_BITS = ctx->HGR_COLOR ^ 0x7f;
    }
    return;
}

typedef struct {
    uint8_t CURRENT_Y;
    uint8_t STARTING_Y;
    uint16_t STARTING_X;
    uint8_t PATTERN_EVEN;
    uint8_t PATTERN_ODD;
    uint8_t FILL_COLOR;
    uint16_t SCREEN_OFFSET;
    uint16_t HBAS;
    uint8_t HGR_PAGE;
    uint8_t COLUMN;
    uint8_t PREVIOUS_COLUMN;
    uint8_t OLD_COLOR_VALUE;
    uint8_t NUMBITS;
    uint8_t SCRBYTE;
    uint8_t HORIZ_PIXELS;
    uint8_t SHAPE_OFFSET;
    Shape SHAPE;
} flood_fill_ctx;

static uint16_t GET_SCREEN_ADDRESS(flood_fill_ctx *ctx)
{
    ctx->HBAS = CALC_APPLE2_ADDRESS(ctx->CURRENT_Y);
    return ctx->HBAS;
}

uint8_t opcodes[1024];


static bool GET_COLOR(flood_fill_ctx *ctx)
{
    uint8_t bVar1;
    uint8_t bVar2;
    uint8_t bVar3;
    uint8_t bVar4;

    GET_SCREEN_ADDRESS(ctx);
    ctx->SCREEN_OFFSET = ctx->HBAS + ctx->PREVIOUS_COLUMN;
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

static void PLOT_FILL_COLOR(flood_fill_ctx *ctx) {
    uint16_t offset = ((ctx->COLUMN & 3) | ctx->HORIZ_PIXELS);
    uint8_t color_mask = FILL_COLOR_ARRAY_9054[offset];
    ctx->OLD_COLOR_VALUE = (ctx->OLD_COLOR_VALUE ^ ctx->SCRBYTE) & 0x7f;
    screenmem[ctx->SCREEN_OFFSET] = ((ctx->SCRBYTE | 0x80) & color_mask) |
    ctx->OLD_COLOR_VALUE;
}

static void divide_and_modulo(flood_fill_ctx *ctx) {
    ctx->PREVIOUS_COLUMN = ctx->STARTING_X / 7;
    ctx->NUMBITS = ctx->STARTING_X % 7;
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
 * On each iteration:
 *   new_carry = (value >> 7) & 1;
 *   value = (value << 1) | (carry ? 1 : 0);
 *   carry = new_carry;
 *
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
 * On each iteration:
 *   new_carry = value & 1;
 *   value = (value >> 1) | (carry ? 0x80 : 0);
 *   carry = new_carry;
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

static void FLOOD_FILL(flood_fill_ctx *ctx)
{
    uint8_t loop_count;
    uint8_t temp_count;
    uint8_t screen_val;
    int8_t chosen_pattern_char;
    bool carry_flag;
    bool msb_flag = 0;
    bool carry;
    uint8_t tmp_bit;
    uint8_t seed_pixel;
    uint8_t has_more_pixels;
    uint8_t rightmost_column;
    uint8_t left_shift_count;
    uint8_t right_shift_count;

    ctx->PATTERN_EVEN = ARRAY_8f7c[(uint8_t)(ctx->FILL_COLOR * 2)];
    ctx->PATTERN_ODD = ARRAY_8f7c[(uint8_t)(ctx->FILL_COLOR * 2 + 1)];

    divide_and_modulo(ctx);
    uint8_t work_byte = ctx->NUMBITS;

    /*
     * Find the first seed row: move upward from STARTING_Y until we find a
     * scanline that is inside the region to be filled (GET_COLOR == false),
     * or we reach the top. This mirrors the original upward search.
     */
    do {
        if (ctx->CURRENT_Y == 0)
            goto AT_TOP;
        work_byte = 0;
        ctx->CURRENT_Y--;
    } while (GET_COLOR(ctx));

SCAN_DOWN:
    /*
     * Move downward from the found row until we find the first eligible row
     * (GET_COLOR == true) or reach the bottom. This establishes the seed
     * scanline for the fill.
     */
    work_byte = ctx->CURRENT_Y + 1;
    if (work_byte != 192) {
    AT_TOP:
        ctx->CURRENT_Y = work_byte;
        if (GET_COLOR(ctx)) {
            left_shift_count = 0;
            right_shift_count = 0;

            // Choose odd/even pattern for this row
            chosen_pattern_char = (ctx->CURRENT_Y & 1) == 0 ?
            ctx->PATTERN_EVEN : ctx->PATTERN_ODD;

            // HORIZ_PIXELS encodes which pattern alignment to use for table lookup
            ctx->HORIZ_PIXELS = chosen_pattern_char * 4;

            // Prepare a working byte based on the existing screen byte and NUMBITS
            // bVar5 in original code = 6 - NUMBITS
            screen_val = 6 - ctx->NUMBITS;
            work_byte = ctx->OLD_COLOR_VALUE;

            carry_flag = 0;

            /* Rotate work_byte left `temp_count` times with carry propagation */
            msb_flag = rotate_left_with_carry(&work_byte, &carry_flag, screen_val + 1);

            /* If after rotation there are leading zeroes, shift them out */
            while( true ) {
                if ((int8_t)screen_val <= 0) goto PREP_SCRBYTE;
                if (msb_flag == false) break;
                loop_count = msb_flag << 7;
                msb_flag = (bool)(work_byte & 1);
                work_byte = (work_byte >> 1) | loop_count;
                screen_val--;
            }
            work_byte >>= screen_val;
            left_shift_count = screen_val;

        PREP_SCRBYTE:
            carry = work_byte & 1;
            screen_val = work_byte >> 1;

            /* Set SCRBYTE to the right-aligned 7-bit chunk for the starting X */
            ctx->SCRBYTE = screen_val;
            rotate_right_with_carry(&ctx->SCRBYTE, &carry, ctx->NUMBITS);

            work_byte = 1;
            for (screen_val = ctx->NUMBITS; screen_val > 0; screen_val--) {
                if (work_byte == 0) {
                    loop_count = screen_val;
                    goto SCRBYTE_SHIFTED_LEFT;
                }
                work_byte = ctx->SCRBYTE >> 7;
                ctx->SCRBYTE = ctx->SCRBYTE << 1 | 1;
            }
            goto SCRBYTE_DONE;
        }
    }
    return;

SCRBYTE_SHIFTED_LEFT:
    ctx->SCRBYTE <<= loop_count;
    right_shift_count = screen_val;
SCRBYTE_DONE:
    seed_pixel = ctx->SCRBYTE & 1;
    has_more_pixels = ((ctx->SCRBYTE & 0x40) != 0);

    /* Start painting at the initial column */
    ctx->COLUMN = ctx->PREVIOUS_COLUMN;
    PLOT_FILL_COLOR(ctx);

    /* Expand to the right while there are more pixels set and columns remain */
    work_byte = ctx->COLUMN + 1;
    while (has_more_pixels != 0 && work_byte < 40) {
        bool bit0_flag = false;
        ctx->SCREEN_OFFSET = ctx->HBAS + work_byte;
        ctx->OLD_COLOR_VALUE = screenmem[ctx->SCREEN_OFFSET];
        loop_count = 8;
        screen_val = ctx->OLD_COLOR_VALUE;
        uint8_t rotating_bit = bit0_flag;

        /* Rotate the screen byte right until we find a 0 bit or run out */
        do {
            temp_count = rotating_bit << 7;
            rotating_bit = (screen_val & 1);
            screen_val = screen_val >> 1 | temp_count;
            if (--loop_count == 0) goto AFTER_RIGHT_SCAN;
        } while (rotating_bit);

        temp_count = loop_count;
        screen_val >>= temp_count;
        has_more_pixels = 0;
        left_shift_count = loop_count;

    AFTER_RIGHT_SCAN:
        ctx->SCRBYTE = screen_val >> 1;
        ctx->COLUMN = work_byte;
        PLOT_FILL_COLOR(ctx);
        if (has_more_pixels != 0)
            work_byte = ctx->COLUMN + 1;
    }
    rightmost_column = ctx->COLUMN;

    /* Now expand left from the starting spot */
    ctx->COLUMN = ctx->PREVIOUS_COLUMN;

LEFT_SCAN:
    work_byte = ctx->COLUMN - 1;
    if (seed_pixel != 0 && (int8_t)work_byte >= 0) {
        ctx->SCREEN_OFFSET = ctx->HBAS + work_byte;
        if (ctx->SCREEN_OFFSET > MAX_SCREEN_ADDR)
            return;
        ctx->OLD_COLOR_VALUE = screenmem[ctx->SCREEN_OFFSET];
        temp_count = 7;
        screen_val = ctx->OLD_COLOR_VALUE << 1;
        loop_count = 0;

        do {
            tmp_bit = -((char)screen_val >> 7);
            screen_val = screen_val << 1 | loop_count;
            temp_count--;
            if (tmp_bit == 0) {
                loop_count = temp_count;
                goto LEFT_SCAN_DONE;
            }
            loop_count = tmp_bit;
        } while (temp_count != 0);
        goto LEFT_SCAN_EXIT;
    }

    /* If we couldn't expand left further, compute offsets for next seed */
    work_byte = rightmost_column - ctx->COLUMN - 1;
    if ((int8_t)work_byte < 0) {
        work_byte = 7;
    } else {
        work_byte = work_byte * 7 + 14;
    }

    work_byte = (uint8_t)(work_byte - left_shift_count) - (left_shift_count > work_byte);
    work_byte -= right_shift_count;
    work_byte = work_byte >> 1;

    /* Advance PREVIOUS_COLUMN by (work_byte / 7), set up NUMBITS for next scanline */
    ctx->PREVIOUS_COLUMN = ctx->COLUMN + work_byte / 7;
    work_byte = work_byte % 7;
    
    ctx->NUMBITS = (right_shift_count + work_byte) % 7;
    ctx->PREVIOUS_COLUMN += (right_shift_count + work_byte) / 7;

    goto SCAN_DOWN;

LEFT_SCAN_DONE:
    screen_val <<= loop_count;
    tmp_bit = 0;
    seed_pixel = 0;
    right_shift_count = temp_count;

LEFT_SCAN_EXIT:
    ctx->SCRBYTE = screen_val << 1 | tmp_bit;
    ctx->COLUMN = work_byte;
    PLOT_FILL_COLOR(ctx);
    goto LEFT_SCAN;
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

static void PLOT_SHAPE(flood_fill_ctx *ctx)
{
    for (int row = 0; row < 8; row++) {
        uint16_t address = GET_SCREEN_ADDRESS(ctx) + ctx->PREVIOUS_COLUMN;
        if ((ctx->SHAPE_OFFSET + row) > sizeof(shape_array))
            return;
        uint8_t shape_bits = shape_array[ctx->SHAPE_OFFSET + row];

        uint8_t shape_mask_a, shape_mask_b;
        rotate_shape_bits(shape_bits, ctx->NUMBITS, &shape_mask_a, &shape_mask_b);

        uint8_t shape_mask_c = shape_mask_a;
        uint8_t shape_mask_d = shape_mask_b;

        // Choose fill pattern based on row parity
        uint8_t fill_pattern = (ctx->CURRENT_Y & 1) ? ctx->PATTERN_ODD : ctx->PATTERN_EVEN;
        ctx->HORIZ_PIXELS = fill_pattern << 2;

        // Apply shapes and fill to screen memory
        for (int col_offset = 0; col_offset <= 1; col_offset++) {
            int fill_offset = (col_offset + ctx->PREVIOUS_COLUMN & 3) | ctx->HORIZ_PIXELS;

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
        ctx->CURRENT_Y++;
    }
    ctx->CURRENT_Y--;
}

static void DRAW_SHAPE(flood_fill_ctx *ctx)
{
    ctx->PATTERN_EVEN = ARRAY_8f7c[ctx->FILL_COLOR * 2];
    ctx->PATTERN_ODD = ARRAY_8f7c[ctx->FILL_COLOR * 2 + 1];
    divide_and_modulo(ctx);
    ctx->SHAPE_OFFSET = ctx->SHAPE << 5;
    PLOT_SHAPE(ctx);
    ctx->SHAPE_OFFSET += 8;
    uint8_t previous_x = ctx->PREVIOUS_COLUMN;
    ctx->PREVIOUS_COLUMN++;
    ctx->CURRENT_Y -= 7;
    PLOT_SHAPE(ctx);
    ctx->SHAPE_OFFSET += 8;
    ctx->PREVIOUS_COLUMN = previous_x;
    ctx->CURRENT_Y++;
    PLOT_SHAPE(ctx);
    ctx->SHAPE_OFFSET += 8;
    ctx->PREVIOUS_COLUMN++;
    ctx->CURRENT_Y -= 7;
    PLOT_SHAPE(ctx);
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

static void MOVE_LEFT(pixelpos_ctx *ctx) {
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

static void MOVE_RIGHT(pixelpos_ctx *ctx) {
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

static void MOVE_UP_OR_DOWN(bool down, pixelpos_ctx *ctx) {
    if (down) {
        ctx->HGR_Y++;
        if (ctx->HGR_Y > 191) ctx->HGR_Y = 0;
    } else {
        ctx->HGR_Y--;
        if (ctx->HGR_Y < 0) ctx->HGR_Y = 191;
    }
    ctx->HBAS = CALC_APPLE2_ADDRESS(ctx->HGR_Y);
}

static void MOVE_LEFT_OR_RIGHT(bool left, pixelpos_ctx *ctx) {
    if (left)
        MOVE_LEFT(ctx);
    else
        MOVE_RIGHT(ctx);
}

static void put_a2_vector_pixel(pixelpos_ctx *ctx) {
    uint16_t offset = ctx->HBAS + ctx->HGR_HORIZ;
    if (offset > MAX_SCREEN_ADDR)
        return;
    screenmem[offset] = ((screenmem[offset] ^ ctx->HGR_BITS) & ctx->HMASK) ^ screenmem[offset];
}

static void HGLIN(uint16_t target_x, uint8_t target_y, pixelpos_ctx *ctx)
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
            MOVE_UP_OR_DOWN(move_down, ctx);
            vertical_movement = ((uint8_t)e + (uint8_t)delta_x <= 0xff);
            e += delta_x;
        } else {
            MOVE_LEFT_OR_RIGHT(move_left, ctx);
            vertical_movement = ((uint8_t)e + (uint8_t)(-delta_y - 1) + 1 <= 0xff);
            e -= delta_y - vertical_movement * 0x100;
        }
        put_a2_vector_pixel(ctx);
    }

    ctx->HGR_X = target_x;
}


static void SET_COLOR(uint8_t color_index, pixelpos_ctx *ctx) {
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
            SET_COLOR(param, ctx);
            break;

        case OPCODE_TEXT_CHAR: // 3
        case OPCODE_TEXT_OUTLINE: // 5
                                  // Text outline mode draws a bunch of pixels that sort of looks like the char
                                  // TODO: See if the outline mode is ever used
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
            HPOSN(a, b, ctx);
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
            HGLIN(a, b, ctx);
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
            flood_ctx.CURRENT_Y = b;
            flood_ctx.STARTING_X = a;
            flood_ctx.HGR_PAGE = 0;

            DRAW_SHAPE(&flood_ctx);
            break;

        case OPCODE_DELAY: // 13
                           // The original allowed for rendering to be paused briefly. We don't do
                           // that in ScummVM, and just show the finished rendered image
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
            flood_ctx.CURRENT_Y = b;
            flood_ctx.HGR_PAGE = 0;

            FLOOD_FILL(&flood_ctx);
            break;

        case OPCODE_RESET: // 15
            a = *ptr++;
            fprintf(stderr, "OPCODE_RESET: %x\n", a);
            return true;
    }

    *outptr = ptr;
    return false;
}

size_t writeToFile(const char *name, uint8_t *data, size_t size)
{
    FILE *fptr = fopen(name, "w");

    if (fptr == NULL) {
        Fatal("File open error!");
        return 0;
    }

    size_t result = fwrite(data, 1, size, fptr);

    fclose(fptr);
    return result;
}

static void FILL_SCREEN(uint8_t color, pixelpos_ctx *ctx) {
    ctx->HGR_BITS = color;
    for (int i = 0; i < SCREEN_MEM_SIZE; i++) {
        screenmem[i] = ctx->HGR_BITS;
        if (((ctx->HGR_BITS * 2 + 0x40) & 0x80) != 0) {
            ctx->HGR_BITS ^= 0x7f;
        }
    }
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
    SET_COLOR(4, &ctx);
    HPOSN(140,96, &ctx);
    ctx.FILL_COLOR = 0;
    ctx.SHAPE = STANDARD_SHAPE;

    while (done == 0 && vecdata < img->imagedata + img->datasize) {
        done = doImageOp(&vecdata, &ctx);
    }
    return 1;
}
