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
    SHAPE_CIRCLE_LARGE = 5,
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
    byte bVar1;
    byte bVar2;
    byte bVar3;
    byte bVar4;

    GET_SCREEN_ADDRESS(ctx);
//    ctx->OPCODE08_H = ctx->ADDRESS_8f52_H + (ctx->ADDRESS_8f52 + ctx->COLUMN > 0xff);
//    ctx->OPCODE08 = ctx->ADDRESS_8f52 + ctx->COLUMN;
    ctx->SCREEN_OFFSET = ctx->HBAS + ctx->PREVIOUS_COLUMN;
//    uint16_t screen_address = (ctx->ADDRESS_8f52_H << 8 | ctx->ADDRESS_8f52) + ctx->COLUMN;
    ctx->OLD_COLOR_VALUE = screenmem[ctx->SCREEN_OFFSET];
//    fprintf(stderr, "GET_COLOR: Read value 0x%x into PREVIOUS_COLOR_8f5e from screen offset 0x%04x\n", screenmem[screen_address], screen_address);
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
//    if (offset > largest_mask_offset)
//        largest_mask_offset = offset;
    uint8_t color_mask = FILL_COLOR_ARRAY_9054[offset];
    ctx->OLD_COLOR_VALUE = (ctx->OLD_COLOR_VALUE ^ ctx->SCRBYTE) & 0x7f;
    screenmem[ctx->SCREEN_OFFSET] = ((ctx->SCRBYTE | 0x80) & color_mask) |
    ctx->OLD_COLOR_VALUE;
//    fprintf(stderr, "Wrote 0x%x to screen offset 0x%04x\n", screenmem[screenaddress], screenaddress);
}

void divide_and_modulo(flood_fill_ctx *ctx) {
    ctx->PREVIOUS_COLUMN = ctx->STARTING_X / 7;
    ctx->NUMBITS = ctx->STARTING_X % 7;
}

//static const uint8_t SHAPE_DATA[32][8] = {
//    {    0,    0,    0,    0,    0,    0,    0,    0 },
//    {    0,    0,    0,    0,    0,    0,    0,    0 },
//    {    0,    0,    0,    0,    0,    0,    0, 0x80 },
//    {    0,    0,    0,    0,    0,    0,    0,    0 },
//    {    0,    0,    0,    0,    0,    0,    0,    1 },
//    {    1,    0,    0,    0,    0,    0,    0,    0 },
//    {    0,    0,    0,    0,    0,    0,    0, 0x80 },
//    { 0x80,    0,    0,    0,    0,    0,    0,    0 },
//    {    0,    0,    0,    0,    0,    0,    1,    3 },
//    {    3,    1,    0,    0,    0,    0,    0,    0 },
//    {    0,    0,    0,    0,    0,    0, 0x80, 0xC0 },
//    { 0xC0, 0x80,    0,    0,    0,    0,    0,    0 },
//    {    0,    0,    0,    0,    0,    3,    7,    7 },
//    {    7,    7,    3,    0,    0,    0,    0,    0 },
//    {    0,    0,    0,    0,    0, 0xC0, 0xE0, 0xE0 },
//    { 0xE0, 0xE0, 0xC0,    0,    0,    0,    0,    0 },
//    {    0,    0,    0,    3, 0x0F, 0x0F, 0x1F, 0x1F },
//    { 0x1F, 0x1F, 0x0F, 0x0F,    3,    0,    0,    0 },
//    {    0,    0,    0, 0xC0, 0xF0, 0xF0, 0xF8, 0xF8 },
//    { 0xF8, 0xF8, 0xF0, 0xF0, 0xC0,    0,    0,    0 },
//    {    0,    3, 0x1F, 0x3F, 0x3F, 0x3F, 0x7F, 0x7F },
//    { 0x7F, 0x7F, 0x3F, 0x3F, 0x3F, 0x1F,    3,    0 },
//    {    0, 0xC0, 0xF8, 0xFC, 0xFC, 0xFC, 0xFE, 0xFE },
//    { 0xFE, 0xFE, 0xFC, 0xFC, 0xFC, 0xF8, 0xC0,    0 },
//    {    0,    0,    0,    0,    1,    8,    2,    0 },
//    { 0x0A,    0,    4,    0,    0,    0,    0,    0 },
//    {    0,    0,    0,    0,    0, 0x20,    0, 0x90 },
//    {    0, 0xA0,    0, 0x80,    0,    0,    0,    0 },
//    {    0,    2,    8, 0x12,    1, 0x24, 0x0B,    3 },
//    { 0x23,    9, 0x22, 0x0A,    4,    1,    0,    0 },
//    {    0, 0x20, 0x80, 0x28,    0, 0xD4, 0xC0, 0xE4 },
//    { 0xE8, 0x90, 0x44, 0xA8,    0, 0x50,    0,    0 }
//};

static const uint8_t ARRAY_8f7c[256] = {
    0x03, 0x07,
    0x16, 0x07,
    0x1A, 0x1D,
    0x1C, 0x17,
    0x08, 0x0B,
    0x00, 0x1B,
    0x00, 0x04,
    0x03, 0x1B,
    0x03, 0x06,
    0x1A, 0x06,
    0x00, 0x06,
    0x00, 0x11,
    0x02, 0x06,
    0x1C, 0x13,
    0x10, 0x13,
    0x10, 0x07,
    0x02, 0x1B,
    0x02, 0x07,
    0x02, 0x17,
    0x02, 0x09,
    0x1A, 0x04,
    0x10, 0x04,
    0x02, 0x05,
    0x12, 0x17,
    0x1A, 0x07,
    0x03, 0x17,
    0x16, 0x19,
    0x03, 0x05,
    0x03, 0x0D,
    0x1A, 0x0D,
    0x1A, 0x05,
    0x10, 0x05,
    0x00, 0x0D,
    0x00, 0x17,
    0x08, 0x05,
    0x16, 0x05,
    0x01, 0x05,
    0x16, 0x0B,
    0x01, 0x07,
    0x01, 0x17,
    0x01, 0x09,
    0x01, 0x04,
    0x16, 0x04,
    0x0C, 0x0F,
    0x01, 0x1B,
    0x01, 0x11,
    0x0C, 0x17,
    0x0C, 0x04,
    0x16, 0x13,
    0x01, 0x06,
    0x16, 0x06,
    0x0C, 0x11,
    0x07, 0x07,
    0x04, 0x04,
    0x07, 0x1B,
    0x1B, 0x1D,
    0x07, 0x11,
    0x06, 0x07,
    0x17, 0x06,
    0x06, 0x1B,
    0x06, 0x06,
    0x04, 0x06,
    0x11, 0x13,
    0x04, 0x11, 0x17, 0x07, 0x17, 0x0B, 0x17, 0x19, 0x05, 0x07, 0x17, 0x05, 0x07, 0x0D, 0x05, 0x05, 0x05, 0x0D, 0x0D, 0x0F, 0x04, 0x0D, 0x17, 0x04, 0x05, 0x1B, 0x05, 0x06, 0x03, 0x03, 0x16, 0x03, 0x03, 0x0C, 0x00, 0x00, 0x08, 0x1A, 0x02, 0x16, 0x1A, 0x1C, 0x03, 0x10, 0x02, 0x03, 0x02, 0x1A, 0x02, 0x02, 0x12, 0x1C, 0x00, 0x1A, 0x12, 0x1A, 0x10, 0x12, 0x00, 0x10, 0x03, 0x1A, 0x16, 0x1A, 0x16, 0x12, 0x01, 0x02, 0x16, 0x18, 0x01, 0x03, 0x01, 0x1A, 0x01, 0x16, 0x01, 0x01, 0x01, 0x00, 0x16, 0x00, 0x16, 0x0C, 0x16, 0x0E, 0x0C, 0x0E, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x55, 0x2A, 0x55, 0x2A, 0x2A, 0x55, 0x2A, 0x55, 0x7F, 0x7F, 0x7F, 0x7F, 0x80, 0x80, 0x80, 0x80, 0xD5, 0xAA, 0xD5, 0xAA, 0xAA, 0xD5, 0xAA, 0xD5, 0xFF, 0xFF, 0xFF, 0xFF, 0x33, 0x66
};

//void Surface::drawShape(int16 x, int16 y, Shape shapeType, uint32 fillColor) {
//    uint shapeNum = (uint)shapeType * 4;
//
//    // Outer loop to draw the shape across a 2x2 grid of 8x8 sub-shapes
//    for (int shapeX = 0; shapeX <= 8; shapeX += 8) {
//        for (int shapeY = 0; shapeY <= 8; shapeY += 8, ++shapeNum) {
//            // Inner loop for character
//            for (int charY = 0; charY < 8; ++charY) {
//                int yp = y + shapeY + charY;
//                if (yp < 0 || yp >= this->h)
//                    continue;
//
//                int xp = x + shapeX;
//                uint32 *lineP = (uint32 *)getBasePtr(xp, yp);
//                byte bits = SHAPE_DATA[shapeNum][charY];
//
//                for (int charX = 0; charX < 8; ++lineP, ++charX, ++xp, bits <<= 1) {
//                    if (xp >= 0 && xp < this->w && (bits & 0x80) != 0)
//                        *lineP = fillColor;
//                }
//            }
//
//        }
//    }
//}

//void FloodFillSurface::floodFillRow(int16 x, int16 y, uint32 fillColor) {
//    int x1, x2, i;
//
//    // Left end of scanline
//    for (x1 = x; x1 > 0; x1--)
//        if (!isPixelWhite(x1 - 1, y))
//            break;
//
//    // Right end of scanline
//    for (x2 = x; x2 < this->w; x2++)
//        if (!isPixelWhite(x2 + 1, y))
//            break;
//
//    drawLine(x1, y, x2, y, fillColor);
//
//    //dumpToScreen();
//
//    // Scanline above
//    if (y > 0) {
//        for (i = x1; i <= x2; i++)
//            if (isPixelWhite(i, y - 1))
//                floodFillRow(i, y - 1, fillColor);
//    }
//
//    // Scanline below
//    if (y < (this->h - 1)) {
//        for (i = x1; i <= x2; i++)
//            if (isPixelWhite(i, y + 1))
//                floodFillRow(i, y + 1, fillColor);
//    }
//}

#define CARRY1(a,b) ((uint8_t)(a) + (uint8_t)(b) > 0xff)
#define CONCAT11(a,b) ((uint8_t)a << 8 | (uint8_t)b)

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

    ctx->FILL_PATTERN_EVEN = ARRAY_8f7c[(byte)(ctx->FILL_COLOR * 2)];
    ctx->FILL_PATTERN_ODD = ARRAY_8f7c[(byte)(ctx->FILL_COLOR * 2 + 1)];
//    if (ctx->FILL_COLOR * 2 + 1 > largest_offset)
//        largest_offset = ctx->FILL_COLOR * 2 + 1;

//    fprintf(stderr, "Read FILL_PATTERN value 0x%04x from array at 8f7c + offset 0x%x. (largest: %d)\n", opcode, ctx->FILL_COLOR * 2 + 1, largest_offset);

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
//        fprintf(stderr, "FLOOD_FILL: Read value 0x%x into PREVIOUS_COLOR_8f5e from screen offset 0x%04x\n", screenmem[screen_address], screen_address);
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

typedef uint16_t ushort;

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
    //    , 0xE0, 0xAA, 0x6F, 0xE0, 0xFA, 0x72, 0xE0, 0x23,
    //    0x6D, 0xE0, 0x69, 0x5F, 0xE0, 0x00, 0x74, 0xE0, 0x01, 0x5F, 0xE0,
    //    0x12, 0x75, 0xE0, 0x15, 0x75, 0x60, 0x5B, 0x47, 0xC0, 0x30, 0x00,
    //    0xC0, 0x3D, 0x14, 0xC0, 0x42, 0x1A, 0xC0, 0x45, 0x33, 0xC0, 0x36,
    //    0x3F, 0xC0, 0x1F, 0x40, 0xC0, 0x00, 0x42, 0xC0, 0x29, 0x1C, 0xC0,
    //    0x1E, 0x1F, 0xC0, 0x24, 0x22, 0xC0, 0x01, 0x23, 0xC0, 0x0D, 0x23,
    //    0xC0, 0x00, 0x1C, 0xC0, 0x0C, 0x00, 0xC0, 0x00, 0x04, 0xC0, 0x6C,
    //    0x11, 0xC0, 0x61, 0x24, 0xC0, 0x5B, 0x39, 0xC0, 0x78, 0x42, 0xC0,
    //    0x8C, 0x32, 0xC0, 0x89, 0x29, 0xC0, 0x7C, 0x21, 0xC0, 0x77, 0x4C
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

int lowest_shape_array = __INT_MAX__;
int largest_shape_offset = 0;
int largest_shape_offset2 = 0;

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
//        DAT_9365 = DAT_9365 >> 1 | 0x80;
//        DAT_9364 = (DAT_9364 & 0x7f) | 0x80;
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

void penguin_put_pixel(int x, int y, pixelpos_ctx *ctx) {
    HPOSN(x, y, ctx);
    uint16_t offset = ctx->HBAS + ctx->HGR_HORIZ;
    if (offset > MAX_SCREEN_ADDR)
        return;
    screenmem[offset] = ((screenmem[offset] ^ ctx->HGR_BITS) & ctx->HMASK) ^ screenmem[offset];
}

void penguin_plot_clip(int x, int y, pixelpos_ctx *ctx)
{
//    if (x < 0)
//        x = ImageWidth - x;
//    if (x > ImageWidth)
//        x = x - ImageWidth;
//    if (y < 0)
//        y = ImageHeight - y;
//    if (y > ImageHeight)
//        y = y - ImageHeight;
//    if (x < 0 || y < 0 || x > ImageWidth || y > ImageHeight)
//        return;
    penguin_put_pixel(x, y, ctx);
}

#define SWAP(a, b) { int tmp = a; a = b; b = tmp; }

void penguin_draw_line(int x1, int y1, int x2, int y2,
                       pixelpos_ctx *ctx)
{
    bool swapped = false;
    int deltaX = -1, deltaY = -1;
    int xDiff = x1 - x2, yDiff = y1 - y2;

    // Draw pixel at starting point
    penguin_plot_clip(x1, y1, ctx);

    // Figure out the deltas movement for creating the line
    if (xDiff < 0) {
        deltaX = 1;
        xDiff = -xDiff;
    }
    if (yDiff < 0) {
        deltaY = 1;
        yDiff = -yDiff;
    }
    if (xDiff < yDiff) {
        swapped = true;
        SWAP(xDiff, yDiff);
        SWAP(deltaX, deltaY);
        SWAP(x1, y1);
    }

    int temp1 = yDiff;
    int temp2 = yDiff - xDiff;
    int temp3 = temp2;

    // Iterate to draw the remaining pixels of the line
    for (int ctr = xDiff; ctr > 0; --ctr) {
        x1 += deltaX;

        if (temp3 >= 0) {
            y1 += deltaY;
            temp3 += temp2;
        } else {
            temp3 += temp1;
        }

        int xp = x1, yp = y1;
        if (swapped)
            SWAP(xp, yp);

        penguin_plot_clip(xp, yp, ctx);
    }
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

enum SpecialOpcode {
    RESETOP_0 = 0,
    RESETOP_RESET = 1,
    RESETOP_OO_TOPOS_UNKNOWN = 3
};


//static uint8_t shape = 0;
//static uint8_t fillColor = 0;
//static int gx = 0, gy = 0;
//static uint8_t hgr_horiz, hgr_bits, hgr_color, hgr_page;
//static uint8_t hmask; // hi-res graphics on-the-fly bit mask
//static uint16_t hbas; // base address for y position

//HGR_SHAPE       .eq     $1a    {addr/2}   ;(2b)
//hgr_bits        .eq     $1c               ;hi-res color mask
//HGR_COUNT       .eq     $1d               ;hi-res high-order uint8_t of step for line
//MON_CH          .eq     $24               ;cursor horizontal displacement
//HBASL           .eq     $26               ;base address for hi-res drawing (low)
//HBASH           .eq     $27               ;base address for hi-res drawing (high)
//MON_H2          .eq     $2c               ;right end of horizontal line drawn by HLINE
//MON_V2          .eq     $2d               ;bottom of vertical line drawn by VLINE

//HGR_HORIZ       .eq     $e5               ;uint8_t index from GBASH,L
//HGR_PAGE        .eq     $e6               ;hi-res page to draw on ($20 or $40)
//HGR_SCALE       .eq     $e7               ;hi-res graphics scale factor
//HGR_SHAPE_PTR   .eq     $e8    {addr/2}   ;hi-res shape table pointer (2b)
//HGR_COLLISIONS  .eq     $ea               ;collision counter

uint16_t move_down(uint16_t screen_address) {
//    fprintf(stderr, "moving down\n");
    uint8_t hbash = screen_address >> 8;
    uint8_t hbasl = screen_address & 0xff;
    // try it first, by FGH=FGH+1
    // HBASH = PPPFGHCD
    hbash += 4; // HBASH = PPPFGHCD
    // is FGH field now zero? Then we are finished.
    if ((hbash & 0x1c) == 0) {
        // if not, look at "E" bit
        uint8_t e = hbasl & 0x80;
        hbasl <<= 1;
        if (e == 0) {
            // e is zero; make it 1 and leave
            e = hbash >= 0x1f;
            hbash -= 0x20;
        } else {
            // carry = 1, so adds 0xe1
            hbash += 0xe1;
            e = 0;
            // is "CD" not zero?
            // tests bit 2 for carry out of "CD"
            //  if no carry, we are finished
            if ((hbash & 0x4) != 0) {
//                * increment "AB" then
//                * 0000 --> 0101
//                * 0101 --> 1010
//                * 1010 --> wrap around to line 0
                bool temp = hbasl >= 0xaf;
                hbasl += 0x50;
                if ((hbasl ^ 0xf0) <= 0xe) {
                    hbasl &= 0xf;
                }
                if (temp) {
                    hbasl += 0x10;
                }
                hbash = 0;
            }
        }
        // shift in E, to get EABAB000 form
        hbasl = hbasl >> 1 | e << 7;
    }
    return hbash << 8 | hbasl;
}

uint16_t move_up(uint16_t screen_address) {
    uint8_t hbash = screen_address >> 8;
    uint8_t hbasl = screen_address & 0xff;
    // look at bits 000FGH00 in HBASH
    if ((hbash & 0x1c) == 0) {
        uint8_t carry;
        int e = hbasl & 0x80;
        hbasl <<= 1;
        // what is "E" (bit 7 of HBASL)?
        if (e != 0) {
            // E=1, then EFGH=EFGH-1
            carry = hbash >= 0xe0;
            hbash -= 0xe0;
        // else look at 000000CD in HBASH
        } else if ((hbash & 0x3) == 0) {
            // Y-pos is AB000000 form
            carry = hbasl >= 0x4f;
//            *    0000+1011=1011 and carry clear
//            * or 0101+1011=0000 and carry set
//            * or 1010+1011=0101 and carry set
            hbasl += 0xb0 + (hbash >= 0xdc);
            if (carry == 0) {
                carry = hbasl >= 0xf;
                hbasl -= 0x10;
            }
            // enough to make HBASH=PPP11111 later
            hbash += 0x23;
            if (carry == 0) {
                carry = hbash >= 0xe0;
                // change 1011 to 1010 (wrap-around)
                hbash += 0x1f;
            }
        } else {
            // CD <> 0, so CDEFGH=CDEFGH-1
            hbash += 0x1f;
            carry = 1;
        }
        hbash += (hbasl & 1);
        // shift in E, to get EABAB000 form
        hbasl = hbasl >> 1 | carry << 7;
    }
    // finish HBASH mods and return new address
    return (hbash - 4) << 8 | hbasl;
}

//void ROTATE_COLORS(pixelpos_ctx *ctx){
//    ctx->HMASK = uVar3;
//    ctx->HGR_HORIZ = param_2;
//    if (((uint8_t)(ctx->HGR_BITS * 2) + 64) > 255) {
//        ctx->HGR_BITS ^= 0x7f;
//    }
//}

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

//void HFIND(void)
//
//{
//    byte x_coord;
//    byte y_coord;
//
//    HGR_Y = HBASH >> 2 & 7 | ((HBASH & 3) << 1 | HBASL >> 7 | HBASL) << 3;
//    y_coord = (HGR_HORIZ * 3 - ((char)HGR_HORIZ >> 7)) * 2 - 1;
//    x_coord = HMASK & 0x7f;
//    do {
//        y_coord = y_coord + 1;
//        x_coord = x_coord >> 1;
//    } while (x_coord != 0);
//    HGR_X._1_1_ = 0;
//    if (CARRY1(y_coord,HGR_HORIZ)) {
//        HGR_X._1_1_ = 1;
//    }
//    HGR_X._0_1_ = y_coord + HGR_HORIZ;
//    return;
//}

void HFIND(int *outx, int *outy, uint16_t HBAS, uint8_t column)
{
    uint8_t xh, xl;
    uint8_t y;
    uint8_t x_coord;
    uint8_t y_coord;

    int8_t HBASH = HBAS >> 8;
    int8_t HBASL = HBAS & 0xff;

//    HBASL = EABAB000
//    HBASH = PPPFGHCD

//    uint8_t a,b,c,d,e,f,g,h;
//
//    a = (HBASL & 0x40) != 0 && (HBASL & 0x10) != 0;
//    b = (HBASL & 0x20) != 0 && (HBASL & 0x8) != 0;
//    c = (HBASH & 0x2) != 0;
//    d = (HBASH & 0x1) != 0;
//    e = (HBASL & 0x80) != 0;
//    f = (HBASH & 0x10) != 0;
//    g = (HBASH & 0x8) != 0;
//    h = (HBASH & 0x4) != 0;
//
//
//    fprintf(stderr, "ABCDEFGH\n");
//    fprintf(stderr, "%d%d%d%d%d%d%d%d\n", a, b, c, d, e, f, g, h);
//
//    y = a * 0x80 + b * 0x40 + c * 0x20 + d * 0x10 + e * 0x8 + f * 0x4 + g * 0x2 + h;
//    fprintf(stderr, "0x%x (%d)\n", y,y);


    y = ((HBASH >> 2) & 7) | (((HBASH & 3) << 1) | (HBASL >> 7) | HBASL) << 3;
    y_coord = (column * 3 - ((char)column >> 7)) * 2 - 1;
    x_coord = 0x81 & 0x7f;
    do {
        y_coord++;
        x_coord >>= 1;
    } while (x_coord != 0);
    xh = 0;
    if (y_coord + column > 255) {
        xh = 1;
    }
    xl = y_coord + column;
    *outx = xh << 8 | xl;
    *outy = y;
}



//uint16_t calc_address(uint8_t y) {
//    uint8_t lobyte = ((y & 0xc0) >> 2 | (y & 0xc0)) >> 1 | (y & 8) << 4;
//    uint8_t hibyte = ((y & 7) << 1 | (uint8_t)(y << 2) >> 7) << 1 | (uint8_t)(y << 3) >> 7;
//    return hibyte << 8 | lobyte + 0x2000;
//}

void test_move_up_and_down(void) {
    fprintf(stderr, "move_up() test starting\n");
    // there are 192 lines and 40 columns
    // We go from line 0 to 191 and column 0 to 39, calculate the expected address and compare it to the
    // results of the move_up() function.
    for (uint8_t line = 0; line < 192; line++) {
        for (int column = 0; column < 40; column++) {
            uint16_t base_address = CALC_APPLE2_ADDRESS(line) + column;
            int one_line_up = line - 1;
            if (line == 0)
                one_line_up = 191;
            uint16_t expected_address = CALC_APPLE2_ADDRESS(one_line_up) + column;
            uint16_t result = move_up(base_address);
            if (result != expected_address) {
                fprintf(stderr, "move_up: line %d column %d: expected address 0x%04x, got 0x%04x. Diff: %x\n", line, column, expected_address, result, expected_address - result);
            }

            int one_line_down = line + 1;
            if (line == 191)
                one_line_down = 0;
            expected_address = CALC_APPLE2_ADDRESS(one_line_down) + column;
            result = move_down(base_address);
            if (result != expected_address) {
                fprintf(stderr, "move_down: line %d column %d (0x%04x)): expected address 0x%04x, got 0x%04x. Diff:%x\n", line, column, base_address, expected_address, result, abs(expected_address - result));
            } else {
//                fprintf(stderr, "move_down: line %d column %d: got expected result 0x%04x\n", line, column, expected_address);
            }

//            int x,y;
//            HFIND(&x, &y, base_address, column);
////            if (x != column * 7 || y != line)
////            fprintf(stderr, "HFIND thinks 0x%x is x:%d y:%d, but it is x:%d y:%d, \n", base_address, x, y, column * 7, line);
//            if (y == line) {
//                //                fprintf(stderr, "HFIND correctly calculated that 0x%x is x:%d y:%d\n", base_address, x, y);
//            } else {
//                fprintf(stderr, "HFIND thinks 0x%x is x:%d y:%d, but it is x:%d y:%d\n", base_address, x, y, column * 7, line);
//            }
//            else
//                fprintf(stderr, "line %d column %d: got expected result 0x%04x\n", line, column, expected_address);
        }
    }
}


// if negative move down, otherwise move up.
//uint16_t MOVE_UP_OR_DOWN(bool down, uint16_t screen_address) {
//    if (down)
//        return move_down(screen_address);
//    else
//        return move_up(screen_address);
//}

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

void HGLIN(uint16_t target_x, uint8_t target_y, pixelpos_ctx *ctx)
{
    bool move_down = target_y > ctx->HGR_Y;
    bool move_left = target_x < ctx->HGR_X;
    uint16_t delta_x = abs(target_x - ctx->HGR_X);
    uint8_t delta_y = abs(target_y - ctx->HGR_Y);
    uint16_t e = delta_x;

    int iterations = delta_x + delta_y;

    do {
        bool horizontal_movement = ((uint8_t)e + (uint8_t)(-delta_y - 1) + 1 > 0xff);
        e = e - ((!horizontal_movement) << 8) - delta_y;

        while (true) {
            uint16_t offset = ctx->HBAS + ctx->HGR_HORIZ;
            screenmem[offset] = ((screenmem[offset] ^ ctx->HGR_BITS) & ctx->HMASK) ^ screenmem[offset];
//           fprintf(stderr, "HGLIN: Wrote 0x%x to offset 0x%x\n", screenmem[offset], offset);
            if (iterations-- == 0) {
                ctx->HGR_X = target_x;
                ctx->HGR_Y = target_y;
                return;
            }
            if (horizontal_movement)
                break;
            MOVE_UP_OR_DOWN(move_down, ctx);
            horizontal_movement = ((uint8_t)e + (uint8_t)delta_x > 0xff);
            e += delta_x;
        }
        MOVE_LEFT_OR_RIGHT(move_left, ctx);
    } while( true );
}

void put_a2_vector_pixel(pixelpos_ctx *ctx) {
    uint16_t offset = ctx->HBAS + ctx->HGR_HORIZ;
    if (offset > MAX_SCREEN_ADDR)
        return;
    screenmem[offset] = ((screenmem[offset] ^ ctx->HGR_BITS) & ctx->HMASK) ^ screenmem[offset];
}

void HGLIN2(uint16_t target_x, uint8_t target_y, pixelpos_ctx *ctx)
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

bool wait_after_line = false;

bool doImageOp(uint8_t **outptr, pixelpos_ctx *ctx) {

    uint8_t *ptr = *outptr;
    uint8_t opcode;
    uint16_t a, b;

    opcode = *ptr++;
    //    debugCN(kDebugGraphics, "  %.4x [%.2x]: ", (int)ctx->_file.pos() - 1, opcode);

    uint8_t param = opcode & 0xf;
    opcode >>= 4;
    flood_fill_ctx flood_ctx = {};

    switch (opcode) {
        case OPCODE_END: // 0
        case OPCODE_END2: // 7
                          // End of the rendering
                          //            debugC(kDebugGraphics, "End of image");
//            fprintf(stderr, "End of image\n");
            *outptr = ptr;
            return true;

        case OPCODE_SET_TEXT_POS: // 1
            a = *ptr++ + (param & 1 ? 256 : 0);
            b = *ptr++;
            //            debugC(kDebugGraphics, "set_text_pos(%d, %d)", a, b);
            fprintf(stderr, "set_text_pos(%d, %d)\n", a, b);

            //            ctx->_textX = a;
            //            ctx->_textY = b;
            break;

        case OPCODE_SET_PEN_COLOR: // 2
                                   //            debugC(kDebugGraphics, "set_pen_color(%.2x)", opcode);
//            fprintf(stderr, "set_pen_color(%.2x)\n", param);

            //            if (!(ctx->_drawFlags & IMAGEF_NO_FILL))
            //                ctx->_penColor = ctx->_drawSurface->getPenColor(param);
            SET_COLOR(param, ctx);
            break;

        case OPCODE_TEXT_CHAR: // 3
        case OPCODE_TEXT_OUTLINE: // 5
                                  // Text outline mode draws a bunch of pixels that sort of looks like the char
                                  // TODO: See if the outline mode is ever used
            if (opcode == OPCODE_TEXT_OUTLINE)
                fprintf(stderr, "TODO: Implement drawing text outlines\n");

            //                warning("TODO: Implement drawing text outlines");

            a = *ptr++;
            if (a < 0x20 || a >= 0x7f) {
                //                warning("Invalid character - %c", a);
                fprintf(stderr, "Invalid character - %c\n", a);
                a = '?';
            }

            //            debugC(kDebugGraphics, "draw_char(%c)", a);
            fprintf(stderr, "draw_char(%c)\n", a);

            //            ctx->_font->drawChar(ctx->_drawSurface, a, ctx->_textX, ctx->_textY, ctx->getFillColor());
            //            ctx->_textX += ctx->_font->getCharWidth(a);
            break;

        case OPCODE_SET_SHAPE: // 4
                               //            debugC(kDebugGraphics, "set_shape_type(%.2x)", param);
//            fprintf(stderr, "set_shape_type(%.2x)\n", param);
                ctx->SHAPE = (Shape)param;
            break;

        case OPCODE_SET_FILL_COLOR: // 6
            a = *ptr++;
            //            debugC(kDebugGraphics, "set_fill_color(%.2x)", a);
//            fprintf(stderr, "set_fill_color(%.2x)\n", a);
            ctx->FILL_COLOR = a; //pal[a];
            break;

        case OPCODE_MOVE_TO: // 8
            a = *ptr++ + (param & 1 ? 256 : 0);
            b = *ptr++;
//            fprintf(stderr,  "move_to(%d, %d)\n", a, b);

            //            debugC(kDebugGraphics, "move_to(%d, %d)", a, b);
            HPOSN(a, b, ctx);
            break;

        case OPCODE_DRAW_BOX: // 9
            a = *ptr++ + (param & 1 ? 256 : 0);
            b = *ptr++;
            fprintf(stderr,  "draw_box (%d, %d) - (%d, %d)\n",
                    ctx->HGR_X, ctx->HGR_Y, a, b);


            //            debugC(kDebugGraphics, "draw_box (%d, %d) - (%d, %d)",
            //                   ctx->_x, ctx->_y, a, b);

            //            ctx->_drawSurface->drawBox(ctx->_x, ctx->_y, a, b, ctx->_penColor);
            break;
#pragma mark draw line
        case OPCODE_DRAW_LINE: // 10
            a = *ptr++ + (param & 1 ? 256 : 0);
            b = *ptr++;

//            fprintf(stderr,  "draw_line (%d, %d) - (%d, %d)\n",
//                    ctx->HGR_X, ctx->HGR_Y, a, b);
//            SetColor(1, 0xaaaaaa);
//            penguin_draw_line(ctx->HGR_X, ctx->HGR_Y, a, b, ctx);

            //            debugC(kDebugGraphics, "draw_line (%d, %d) - (%d, %d)",
            //                   ctx->_x, ctx->_y, a, b);
            //            ctx->_drawSurface->drawLine(ctx->_x, ctx->_y, a, b, ctx->_penColor);
            HGLIN2(a, b, ctx);
//            HGLIN_ALT2(a, b, ctx);
//            DrawApple2ImageFromVideoMem();

//            if (a == 0xBB && b == 0x65)
//                wait_after_line = true;


//            if (wait_after_line) {
//                DrawApple2ImageFromVideoMem();
//                wait_for_key();
//            }

//            ctx->HGR_X = a;
//            ctx->HGR_Y = b;
            break;

        case OPCODE_DRAW_CIRCLE: // 11
            a = *ptr++;
            fprintf(stderr,  "draw_circle (%d, %d) diameter=%d\n",
                    ctx->HGR_X, ctx->HGR_Y, a);
            //            debugC(kDebugGraphics, "draw_circle (%d, %d) diameter=%d",
            //                   ctx->_x, ctx->_y, a);

            //            ctx->_drawSurface->drawCircle(ctx->_x, ctx->_y, a, ctx->_penColor);
            break;

        case OPCODE_DRAW_SHAPE: // 12
            a = *ptr++ + (param & 1 ? 256 : 0);
            b = *ptr++;
//            fprintf(stderr,   "draw_shape(%d, %d), style=%.2x, fill=%.2x\n",
//                    a, b, ctx->SHAPE, ctx->FILL_COLOR);
            flood_ctx.FILL_COLOR = ctx->FILL_COLOR;
            flood_ctx.SHAPE = ctx->SHAPE;
            flood_ctx.CURRENT_Y = b;
            flood_ctx.STARTING_X = a;
            flood_ctx.HGR_PAGE = 0;

            DRAW_SHAPE(&flood_ctx);

            //            debugC(kDebugGraphics, "draw_shape(%d, %d), style=%.2x, fill=%.2x",
            //                   a, b, ctx->_shape, ctx->_fillColor);

            //            if (!(ctx->_drawFlags & IMAGEF_NO_FILL))
            //                ctx->_drawSurface->drawShape(a, b, ctx->_shape, ctx->_fillColor);
            break;

        case OPCODE_DELAY: // 13
                           // The original allowed for rendering to be paused briefly. We don't do
                           // that in ScummVM, and just show the finished rendered image
            fprintf(stderr,   "OPCODE_DELAY: 0x%x\n", *ptr++);
            //            ptr++;
            break;
#pragma mark flood fill
        case OPCODE_PAINT: // 14
            a = *ptr++ + (param & 1 ? 256 : 0);
            b = *ptr++;
            if (opcode & 0x1)
                a += 255;

//            fprintf(stderr, "paint(%d, %d)\n", a, b);

            flood_ctx.FILL_COLOR = ctx->FILL_COLOR;
            flood_ctx.STARTING_X = a;
            flood_ctx.CURRENT_Y = b;
            flood_ctx.HGR_PAGE = 0;
//            if (a == 117 && b == 76) {
//                DrawApple2ImageFromVideoMem();
//                wait_for_key();
//            }
            FLOOD_FILL(&flood_ctx);
//                DrawApple2ImageFromVideoMem();
//                wait_for_key();
//                wait_after_line = true;
            //            debugC(kDebugGraphics, "paint(%d, %d)", a, b);
            //            ctx->lineFixes();
            //            if (!(ctx->_drawFlags & IMAGEF_NO_FILL))
            //                ctx->_drawSurface->floodFill(a, b, ctx->_fillColor);
            break;

            //#if 0
            // FIXME: The reset case was causing room outside cell to be drawn all white
        case OPCODE_RESET: // 15
            a = *ptr++;
            fprintf(stderr, "OPCODE_RESET: %x\n", a);
            return true;

            //            doResetOp(ctx, a);
//            break;
            //#endif
    }

    //ctx->_drawSurface->dumpToScreen();

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
//    test_move_up_and_down();

//    do {
//        while (!img->imagedata || img->datasize < 2 ) {
////               || strcmp(img->filename, "R0199") != 0) {
//            if (img->next) {
//                img = img->next;
//            } else {
//                return 0;
//            }
//        }
//        const char *path = "/Users/administrator/Desktop/A2VectorFiles/";
//        size_t pathlength = strlen(path);
//        size_t namelength = strlen(img->filename);
//        size_t total = pathlength + namelength + 5;
//        char *filename = (char *)MemAlloc(total);
//
//        snprintf(filename, total, "%s%s.dat", path, img->filename);
//        writeToFile(filename, img->imagedata, img->datasize);

//        uint16_t version = READ_LE_UINT16(img->imagedata);
//        fprintf(stderr, "Image format version: 0x%04x\n", version);
//        fprintf(stderr, "Next word in file: 0x%04x\n", READ_LE_UINT16(img->imagedata + 2));

        uint8_t *vecdata = img->imagedata + 4;
        int done = 0;
    tryagain:
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
        ctx.SHAPE = SHAPE_CIRCLE_LARGE;

//        fprintf(stderr, "Drawing Apple 2 vector image \"%s\" (index %d)\n", img->filename, img->index);
//        int has_drawn = 0;
        while (done == 0 && vecdata < img->imagedata + img->datasize) {
            done = doImageOp(&vecdata, &ctx);
//            if (done == 0)
//                has_drawn = 1;
        }
//        if (has_drawn) {
//            DrawApple2ImageFromVideoMem();
//            fprintf(stderr, "At file offset 0x%lx. Image size: 0x%zx\n", vecdata - img->imagedata, img->datasize);
//            wait_for_key();
//        }

//        if (img->datasize - (vecdata - img->imagedata) > 0xff) {
//            fprintf(stderr, "Trying to draw remains of file.\n");
//            vecdata += 2;
//            done = 0;
//            goto tryagain;
//        }

//
//        img = img->next;
//    } while (img && img->filename);
    return 1;
}
