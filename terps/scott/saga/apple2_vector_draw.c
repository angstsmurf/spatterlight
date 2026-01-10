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

typedef uint8_t byte;

typedef struct {
    uint8_t CURRENT_Y;
    uint8_t STARTING_Y;
    uint16_t STARTING_X;
    uint8_t FILL_PATTERN_EVEN;
    uint8_t FILL_PATTERN_ODD;
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
    uint8_t SHAPE;
} flood_fill_ctx;


void compare_screen_memory(void) {
    size_t size;
    uint8_t *screen_dump =
    ReadFileIfExists("/Users/administrator/mame/screen.bin", &size);
    if (screen_dump == NULL || size == 0) {
        fprintf(stderr, "Bad file!\n");
        return;
    }
    int error = 0;
    for (int i = 0; i < SCREEN_MEM_SIZE; i++) {
        if (screen_dump[i] != screenmem[i]) {
            fprintf(stderr, "Mismatch at 0x%x: expected 0x%x, got 0x%x\n", i, screen_dump[i], screenmem[i]);
            error = 1;
        }
    }
    if (error == 0) {
        fprintf(stderr, "Incredible! All screen data matched!\n");
    }
}

void wait_for_key(void) {
    glk_cancel_char_event(Graphics);
    glk_request_char_event(Graphics);
    event_t ev;
    int result = 0;
    do {
        glk_select(&ev);
        if (ev.type == evtype_CharInput) {
            if (ev.val1 == keycode_Return) {
                result = 1;
            } else if (ev.val1 == 'c') {
                compare_screen_memory();
                glk_request_char_event(Graphics);
            } else {
                glk_request_char_event(Graphics);
            }
        }
    } while (result == 0);
}

uint16_t GET_SCREEN_ADDRESS(flood_fill_ctx *ctx)
{
    ctx->HBAS = CALC_APPLE2_ADDRESS(ctx->CURRENT_Y);
    return ctx->HBAS;
}

uint8_t opcodes[1024];


bool GET_COLOR(flood_fill_ctx *ctx)
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

int largest_mask_offset = 0;

const uint8_t FILL_COLOR_ARRAY_9054[128] =  { 0x00, 0x00, 0x00, 0x00, 0x55, 0x2A, 0x55, 0x2A, 0x2A, 0x55, 0x2A, 0x55, 0x7F, 0x7F, 0x7F, 0x7F, 0x80, 0x80, 0x80, 0x80, 0xD5, 0xAA, 0xD5, 0xAA, 0xAA, 0xD5, 0xAA, 0xD5, 0xFF, 0xFF, 0xFF, 0xFF, 0x33, 0x66, 0x4C, 0x19, 0xB3, 0xE6, 0xCC, 0x99, 0x4C, 0x19, 0x33, 0x66, 0xCC, 0x99, 0xB3, 0xE6, 0x11, 0x22, 0x44, 0x08, 0x91, 0xA2, 0xC4, 0x88, 0x44, 0x08, 0x11, 0x22, 0xC4, 0x88, 0x91, 0xA2, 0x22, 0x44, 0x08, 0x11, 0xA2, 0xC4, 0x88, 0x91, 0x08, 0x11, 0x22, 0x44, 0x88, 0x91, 0xA2, 0xC4, 0xC9, 0xA4, 0x92, 0x89, 0x24, 0x12, 0x49, 0x24, 0x77, 0x6E, 0x5D, 0x3B, 0xF7, 0xEE, 0xDD, 0xBB, 0x5D, 0x3B, 0x77, 0x6E, 0xDD, 0xBB, 0xF7, 0xEE, 0x6E, 0x5D, 0x3B, 0x77, 0xEE, 0xDD, 0xBB, 0xF7, 0x3B, 0x77, 0x6E, 0x5D, 0xBB, 0xF7, 0xEE, 0xDD, 0xA9, 0x00, 0x8D, 0x53, 0x8F, 0xAD, 0x56, 0x8F };

void PLOT_FILL_COLOR(flood_fill_ctx *ctx) {
    uint16_t offset = ((ctx->COLUMN & 3) | ctx->HORIZ_PIXELS);
    uint8_t color_mask = FILL_COLOR_ARRAY_9054[offset];
    ctx->OLD_COLOR_VALUE = (ctx->OLD_COLOR_VALUE ^ ctx->SCRBYTE) & 0x7f;
    screenmem[ctx->SCREEN_OFFSET] = ((ctx->SCRBYTE | 0x80) & color_mask) |
    ctx->OLD_COLOR_VALUE;
}

void divide_and_modulo(flood_fill_ctx *ctx) {
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

int largest_offset = 0;

void FLOOD_FILL(flood_fill_ctx *ctx)
{
    uint8_t bVar1;
    uint8_t count;
    uint8_t bVar3;
    int8_t cVar4;
    uint8_t bVar5;
    bool bVar6;
    bool bVar7 = 0;
    uint8_t bVar8;
    uint8_t carry;
    uint8_t current_pixel;
    uint8_t DAT_8f5c;
    uint8_t DAT_9360;
    uint8_t DAT_9361;
    uint8_t DAT_9362;

    ctx->FILL_PATTERN_EVEN = ARRAY_8f7c[(uint8_t)(ctx->FILL_COLOR * 2)];
    ctx->FILL_PATTERN_ODD = ARRAY_8f7c[(uint8_t)(ctx->FILL_COLOR * 2 + 1)];

    divide_and_modulo(ctx);
    bVar1 = ctx->NUMBITS;
    do {
        // Move upwards, line by line, until we find a pixel
        // that is not white, or reach the top.
        if (ctx->CURRENT_Y == 0)
            goto LAB_91d6;
        bVar1 = 0;
        ctx->CURRENT_Y--;
    } while (GET_COLOR(ctx));
look_down:
    // Move downwards, line by line, until we find a pixel
    // that is not white, or reach the bottom.
    bVar1 = ctx->CURRENT_Y + 1;
    if (bVar1 != 192) {
    LAB_91d6:
        ctx->CURRENT_Y = bVar1;
        if (GET_COLOR(ctx)) {
            DAT_9361 = 0;
            DAT_9362 = 0;
            cVar4 = ctx->FILL_PATTERN_ODD;
            if ((ctx->CURRENT_Y & 1) == 0) {
                cVar4 = (char)ctx->FILL_PATTERN_EVEN;
            }
            ctx->HORIZ_PIXELS = cVar4 * 4;
            bVar5 = 6 - ctx->NUMBITS;
            bVar1 = ctx->OLD_COLOR_VALUE;
            bVar6 = (((ctx->NUMBITS & (bVar5 | 0xf9)) | (bVar5 & 0xf9)) & 0x80) != 0;
            for (int i = 0; i <= bVar5; i++) {
                bVar7 = (bool)(bVar1 >> 7); // Store bit 7 of bVar1
                // Shift bVar6 into bit 0
                bVar1 = bVar1 << 1 | bVar6;
                bVar6 = bVar7;
            }
            while( true ) {
                if ((int8_t)bVar5 <= 0) goto LAB_9221;
                if (bVar7 == false) break;
                count = bVar7 << 7;
                bVar7 = (bool)(bVar1 & 1);
                bVar1 = (bVar1 >> 1) | count;
                bVar5--;
            }
            bVar1 >>= bVar5;
            DAT_9361 = bVar5;
        LAB_9221:
            carry = bVar1 & 1;
            bVar5 = bVar1 >> 1;

            ctx->SCRBYTE = bVar5;
            for (int i = 0; i < ctx->NUMBITS; i++) {
                bVar1 = ctx->SCRBYTE;
                uint8_t next_carry = ctx->SCRBYTE & 1;
                ctx->SCRBYTE = ctx->SCRBYTE / 2 + (carry * 0x80);
                carry = next_carry;
            }
            bVar1 &= 1;
            for (bVar5 = ctx->NUMBITS; bVar5 > 0; bVar5--) {
                count = bVar5;
                if (bVar1 == 0) goto LAB_923d;
                count = ctx->SCRBYTE >> 7;
                ctx->SCRBYTE = ctx->SCRBYTE * 2 + 1;
                bVar1 = count;
            }
            goto LAB_9242;
        }
    }
    return;
LAB_923d:
    ctx->SCRBYTE <<= count;
    DAT_9362 = bVar5;
LAB_9242:
    current_pixel = ctx->SCRBYTE & 1;
    DAT_8f5c = ((ctx->SCRBYTE & 0x40) != 0);
    ctx->COLUMN = ctx->PREVIOUS_COLUMN;
    PLOT_FILL_COLOR(ctx);
    bVar1 = ctx->COLUMN + 1; // Proceed to the right
    while (DAT_8f5c != 0 && bVar1 < 40) {
        bVar6 = false;
        ctx->SCREEN_OFFSET = ctx->HBAS + bVar1;
        ctx->OLD_COLOR_VALUE = screenmem[ctx->SCREEN_OFFSET];
        count = 8;
        bVar5 = ctx->OLD_COLOR_VALUE;
        uint8_t bit0 = bVar6;
        do {
            bVar3 = bit0 << 7;
            bit0 = (bVar5 & 1);
            bVar5 = bVar5 >> 1 | bVar3;
            if (--count == 0) goto LAB_92a4;
            bVar3 = count;
        } while (bit0);

        bVar5 >>= bVar3;
        DAT_8f5c = 0;
        DAT_9361 = count;
    LAB_92a4:
        ctx->SCRBYTE = bVar5 >> 1;
        ctx->COLUMN = bVar1;
        PLOT_FILL_COLOR(ctx);
        if (DAT_8f5c != 0)
            bVar1 = ctx->COLUMN + 1;
    }
    DAT_9360 = ctx->COLUMN;
    ctx->COLUMN = ctx->PREVIOUS_COLUMN;
look_left:
    // Proceed to the left
    bVar1 = ctx->COLUMN - 1;
    if (current_pixel != 0 && (int8_t)bVar1 >= 0) {
        ctx->SCREEN_OFFSET = ctx->HBAS + bVar1;
        ctx->OLD_COLOR_VALUE = screenmem[ctx->SCREEN_OFFSET ];
        bVar3 = 7;
        bVar5 = ctx->OLD_COLOR_VALUE << 1;
        count = 0;
        do {
            bVar8 = -((char)bVar5 >> 7);
            bVar5 = bVar5 << 1 | count;
            bVar3--;
            bVar6 = bVar3 == 0;
            count = bVar3;
            if (bVar8 == 0) goto LAB_92f5;
            count = bVar8;
        } while (bVar3 != 0);
        goto LAB_9305;
    }
    bVar1 = DAT_9360 - ctx->COLUMN - 1;
    if ((int8_t)bVar1 < 0) {
        bVar1 = 7;
    } else {
        bVar1 = bVar1 * 7 + 14;
    }

    bVar1 = (uint8_t)(bVar1 - DAT_9361) - (DAT_9361 > bVar1);
    bVar1 -= DAT_9362;
    bVar1 = bVar1 >> 1;

    // Increase HORIZ_8f57 by bVar1 / 7
    ctx->PREVIOUS_COLUMN = ctx->COLUMN + bVar1 / 7;
    bVar1 = bVar1 % 7;
    
    ctx->NUMBITS = (DAT_9362 + bVar1) % 7;
    ctx->PREVIOUS_COLUMN += (DAT_9362 + bVar1) / 7;

    goto look_down;
LAB_92f5:
    while (!bVar6) {
        bVar5 <<= 1;
        bVar8 = 0;
        bVar6 = (uint8_t)(count - 1) == 0;
        count--;
    }
    current_pixel = 0;
    DAT_9362 = bVar3;
LAB_9305:
    ctx->SCRBYTE = bVar5 << 1 | bVar8;
    ctx->COLUMN = bVar1;
    PLOT_FILL_COLOR(ctx);
    goto look_left;
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

void PLOT_SHAPE(flood_fill_ctx *ctx)
{
    uint8_t bVar3;
    uint8_t bVar4;
    uint8_t bVar5;
    uint8_t DAT_9364;
    uint8_t DAT_9365;
    uint8_t DAT_9366;
    uint8_t DAT_9367;
    uint8_t DAT_94fe;
    uint8_t DAT_94ff;

    for (int index = 0; index < 8; index++) {
        uint16_t address = GET_SCREEN_ADDRESS(ctx) + ctx->PREVIOUS_COLUMN;
        uint8_t shape_byte = shape_array[ctx->SHAPE_OFFSET + index];
        DAT_9365 = 0xfe;
        DAT_9364 = 0;
        DAT_9366 = 0;
        bVar3 = shape_byte << 1;
        bVar4 = 0;
        for (int i = 0; i < ctx->NUMBITS; i++) {
            bVar5 = bVar3 >> 7;
            bVar3 = bVar3 << 1 | bVar4;
            bVar4 = DAT_9366 >> 7;
            DAT_9366 = DAT_9366 << 1 | bVar5;
            bVar5 = DAT_9365 >> 7;
            DAT_9365 = DAT_9365 << 1 | bVar4;
            bVar4 = DAT_9364 >> 7;
            DAT_9364 = DAT_9364 << 1 | bVar5;
        }
        DAT_9367 = bVar3 >> 1 | 0x80;
        DAT_9366 = (DAT_9366 & 0x7f) | 0x80;
        uint8_t fill_pattern = ctx->FILL_PATTERN_ODD;
        if ((ctx->CURRENT_Y & 1) == 0) {
            fill_pattern = ctx->FILL_PATTERN_EVEN;
        }
        ctx->HORIZ_PIXELS = fill_pattern << 2;
        DAT_94fe = DAT_9367;
        DAT_94ff = DAT_9366;
        for (int i = 0; i <= 1; i++) {
            int offset = (i + ctx->PREVIOUS_COLUMN & 3) | ctx->HORIZ_PIXELS;
            if (i == 0) {
                DAT_9367 = shape_array9054[offset] & DAT_9367;
                if (DAT_94fe != 0x80) {
                    screenmem[address] =
                    ((DAT_94fe | screenmem[address]) ^ DAT_94fe) | DAT_9367;
                }
            } else {
                DAT_9366 = shape_array9054[offset] & DAT_9366;
            }
        }
        if (DAT_94ff != 0x80) {
            screenmem[address + 1] =
            ((DAT_94ff | screenmem[address + 1]) ^ DAT_94ff) | DAT_9366;
        }
        ctx->CURRENT_Y++;
    }
    ctx->CURRENT_Y--;
}

void DRAW_SHAPE(flood_fill_ctx *ctx)
{
    ctx->FILL_PATTERN_EVEN = ARRAY_8f7c[ctx->FILL_COLOR * 2];
    ctx->FILL_PATTERN_ODD = ARRAY_8f7c[ctx->FILL_COLOR * 2 + 1];
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

void MOVE_LEFT(pixelpos_ctx *ctx) {
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

void MOVE_RIGHT(pixelpos_ctx *ctx) {
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


void MOVE_UP_OR_DOWN(bool down, pixelpos_ctx *ctx) {
    if (down) {
        ctx->HGR_Y++;
        if (ctx->HGR_Y > 191) ctx->HGR_Y = 0;
    } else {
        ctx->HGR_Y--;
        if (ctx->HGR_Y < 0) ctx->HGR_Y = 191;
    }
    ctx->HBAS = CALC_APPLE2_ADDRESS(ctx->HGR_Y);
}

void MOVE_LEFT_OR_RIGHT(bool left, pixelpos_ctx *ctx) {
    if (left)
        MOVE_LEFT(ctx);
    else
        MOVE_RIGHT(ctx);
}

void put_a2_vector_pixel(pixelpos_ctx *ctx) {
    uint16_t offset = ctx->HBAS + ctx->HGR_HORIZ;
    if (offset > MAX_SCREEN_ADDR)
        return;
    screenmem[offset] = ((screenmem[offset] ^ ctx->HGR_BITS) & ctx->HMASK) ^ screenmem[offset];
}

void HGLIN(uint16_t target_x, uint8_t target_y, pixelpos_ctx *ctx)
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


void SET_COLOR(uint8_t color_index, pixelpos_ctx *ctx) {
    const uint8_t COLORTBL[8] = {0x00, 0x2a, 0x55, 0x7f, 0x80, 0xaa, 0xd5, 0xff};
    if (color_index < 8) {
        ctx->HGR_COLOR = COLORTBL[color_index];
    } else {
        fprintf(stderr, "SET_COLOR called with illegal color index %d\n", color_index);
    }
}

bool doImageOp(uint8_t **outptr, pixelpos_ctx *ctx) {

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

void FILL_SCREEN(uint8_t color, pixelpos_ctx *ctx) {
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
