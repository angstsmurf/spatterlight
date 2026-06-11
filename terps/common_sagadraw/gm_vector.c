//
//  gm_vector.c
//
//  Shared Apple II Graphics Magician vector primitives. See gm_vector.h.
//
//  This is a verbatim extraction of the cursor/line code that was duplicated,
//  byte-for-byte, in scott/saga/apple2_vector_draw.c and
//  comprehend/graphics_magician.cpp. Both renderers are validated pixel-exact
//  against hardware captures, so this code must not change behaviour: it is a
//  pure move, not a rewrite.
//

#include "gm_vector.h"

// Kept local to this translation unit so the two engines can keep their own
// copies of the same macros without a redefinition clash.
#define COL_BITS           7
#define GM_MAX_SCREEN_ADDR 0x1fff
#define CARRY_THRESHOLD    0x100

#define BRUSH_PART_SIZE        8
#define BRUSH_TOTAL_PARTS      4
#define BRUSH_PART_HEIGHT      8
#define GM_BRUSH_BITMAPS_SIZE  256

#define CALC_APPLE2_ADDRESS(y) \
    ((((y / 8) & 0x07) << 7) + (((y / 8) & 0x18) * 5) + ((y & 7) << 10))

void gm_spread7(uint8_t src, uint8_t offset, uint8_t *mask_a, uint8_t *mask_b) {
    /* Rotate the 7-bit source left `offset` times, collecting bit 6 into the
       carry byte that spills into the next column. */
    uint8_t carry = 0;
    for (uint8_t n = offset; n != 0; n--) {
        carry = (uint8_t)((carry << 1) | ((src & 0x7f) >> 6));
        src = (uint8_t)(src << 1);
    }
    *mask_a = src & 0x7f;   /* bits painted into column col */
    *mask_b = carry;        /* bits carried into column col+1 */
}

void gm_set_color(uint8_t color_index, gm_vector_ctx *c) {
    static const uint8_t COLORTBL[8] = {0x00, 0x2a, 0x55, 0x7f, 0x80, 0xaa, 0xd5, 0xff};
    if (color_index < 8)
        c->HGR_COLOR = COLORTBL[color_index];
}

void gm_set_draw_position(uint16_t xpos, uint8_t ypos, gm_vector_ctx *c) {
    c->HGR_X = xpos;
    c->HGR_Y = ypos;
    c->HBAS = CALC_APPLE2_ADDRESS(ypos);
    c->HGR_HORIZ = xpos / COL_BITS;
    static const uint8_t masktable[COL_BITS] = {0x81, 0x82, 0x84, 0x88, 0x90, 0xa0, 0xc0};
    c->HMASK = masktable[xpos % COL_BITS];
    c->HGR_BITS = c->HGR_COLOR;
    // If odd column, rotate bits
    if ((c->HGR_HORIZ & 1) == 1 && ((c->HGR_COLOR * 2 + 0x40) & 0x80))
        c->HGR_BITS = c->HGR_COLOR ^ 0x7f;
}

static void move_left(gm_vector_ctx *c) {
    if ((c->HMASK & 1) == 0) {
        /* Shift mask right, moves dot left. Move sign bit back where it was. */
        c->HMASK = c->HMASK >> 1 ^ 0xc0;
        return;
    }
    /* moved to next byte, so decrease index */
    c->HGR_HORIZ--;
    if ((int8_t)c->HGR_HORIZ < 0) {
        /* off the left edge, wrap around to the right edge */
        c->HGR_HORIZ = 39;
    }
    c->HMASK = 0xc0;
    if ((c->HGR_BITS * 2 + 0x40) & 0x80) {
        c->HGR_BITS ^= 0x7f;
    }
}

static void move_right(gm_vector_ctx *c) {
    /* shifting byte left moves pixel right */
    c->HMASK = c->HMASK << 1 ^ 0x80;
    if ((int8_t)c->HMASK < 0) {
        return;
    }
    c->HMASK = 0x81;
    c->HGR_HORIZ++;
    if (c->HGR_HORIZ >= 40) {
        /* off the right edge, wrap around to the left edge */
        c->HGR_HORIZ = 0;
    }
    if ((c->HGR_BITS * 2 + 0x40) & 0x80) {
        c->HGR_BITS ^= 0x7f;
    }
}

static void move_up_or_down(bool down, gm_vector_ctx *c) {
    if (down) {
        c->HGR_Y++;
        if (c->HGR_Y > 191) c->HGR_Y = 0;
    } else {
        c->HGR_Y--;
        /* Note: `(int8_t)HGR_Y < 0` would wrongly fire for valid rows 128..191. */
        if (c->HGR_Y > 191) c->HGR_Y = 191;
    }
    c->HBAS = CALC_APPLE2_ADDRESS(c->HGR_Y);
}

static void move_left_or_right(bool left, gm_vector_ctx *c) {
    if (left)
        move_left(c);
    else
        move_right(c);
}

void gm_plot_pixel(gm_vector_ctx *c) {
    uint16_t offset = c->HBAS + c->HGR_HORIZ;
    if (offset > GM_MAX_SCREEN_ADDR)
        return;
    c->write(offset,
             (uint8_t)(((c->screenmem[offset] ^ c->HGR_BITS) & c->HMASK) ^ c->screenmem[offset]),
             c->user);
}

void gm_draw_line(uint16_t target_x, uint8_t target_y, gm_vector_ctx *c) {
    const bool move_down = target_y > c->HGR_Y;
    const bool mleft = target_x < c->HGR_X;

    const uint16_t delta_x = (target_x > c->HGR_X) ? (target_x - c->HGR_X) : (c->HGR_X - target_x);
    const uint8_t delta_y = (target_y > c->HGR_Y) ? (target_y - c->HGR_Y) : (c->HGR_Y - target_y);

    gm_plot_pixel(c);

    const int total_steps = delta_x + delta_y;
    if (total_steps == 0) return;

    /* The 6502 sets HGR_DY to -delta_y - 1, with a carry of 1 added each use.
       Not the same as (uint8_t)(-delta_y): the result may overflow 8 bits. */
    const int minus_delta_y = (uint8_t)(~delta_y) + 1;

    bool carry = (uint8_t)delta_x + minus_delta_y < CARRY_THRESHOLD;
    /* moving_vertically tracks the carry flag at the end of each 6502 iteration. */
    bool moving_vertically = (delta_x >> 8) < carry;

    uint16_t error_term = delta_x + minus_delta_y - CARRY_THRESHOLD;

    for (int i = 0; i < total_steps; i++) {
        if (moving_vertically) {
            move_up_or_down(move_down, c);
            moving_vertically = error_term + delta_x < CARRY_THRESHOLD << 8;
            error_term += delta_x;
        } else {
            move_left_or_right(mleft, c);
            uint16_t nocarry = ((uint8_t)error_term + minus_delta_y < CARRY_THRESHOLD) << 8;
            moving_vertically = error_term < nocarry;
            uint8_t e_low = (uint8_t)error_term + minus_delta_y;
            error_term = ((error_term - nocarry) & 0xff00) | e_low;
        }
        gm_plot_pixel(c);
    }
    /* HGR_Y is already set (in move_up_or_down()), but not HGR_X. */
    c->HGR_X = target_x;
}

// ---- Brush / shape drawing ----------------------------------------------------

// Scratch state for one brush draw (not persistent across ops).
typedef struct {
    uint8_t scanline;
    uint8_t seed_column;
    uint8_t pixel_offset;
    uint8_t bitmap_index;
    uint8_t pattern_even;
    uint8_t pattern_odd;
} gm_brush_scratch;

// One 8-row brush quadrant: for each row, spread the brush byte across two
// adjacent hi-res bytes by a 7-bit rotate (collecting bit 6 into the right byte),
// then blend it into the screen using the current fill colour pattern.
static void brush_bitmap(gm_vector_ctx *c, gm_brush_scratch *s) {
    for (int row = 0; row < 8; row++) {
        uint16_t address = CALC_APPLE2_ADDRESS(s->scanline) + s->seed_column;
        int idx = s->bitmap_index + row;
        if (idx < GM_BRUSH_BITMAPS_SIZE && address <= GM_MAX_SCREEN_ADDR) {
            uint8_t b = c->brush_bitmaps[idx];
            if (b != 0) {
                /* 7-bit spread by the pixel offset within the column. */
                uint8_t mask_a, mask_b;
                gm_spread7(b, s->pixel_offset, &mask_a, &mask_b);

                uint8_t pat_base = ((s->scanline & 1) ? s->pattern_odd
                                                      : s->pattern_even) * 4;
                uint8_t pat0 = c->pattern_data[(s->seed_column & 3) | pat_base];
                uint8_t pat1 = c->pattern_data[((s->seed_column + 1) & 3) | pat_base];

                if (mask_a != 0)
                    c->write(address,
                        (uint8_t)(((mask_a ^ 0x7f) & c->screenmem[address]) | ((mask_a | 0x80) & pat0)),
                        c->user);
                if (mask_b != 0 && (uint16_t)(address + 1) <= GM_MAX_SCREEN_ADDR)
                    c->write((uint16_t)(address + 1),
                        (uint8_t)(((mask_b ^ 0x7f) & c->screenmem[address + 1]) | ((mask_b | 0x80) & pat1)),
                        c->user);
            }
        }
        s->scanline++;
    }
}

static void brush_quadrant(gm_vector_ctx *c, gm_brush_scratch *s,
                           int column_offset, int line_offset) {
    s->seed_column += column_offset;
    s->scanline += line_offset;
    brush_bitmap(c, s);
    /* Restore original column; brush_bitmap() advanced scanline by 8 rows. */
    s->seed_column -= column_offset;
    s->scanline -= line_offset + BRUSH_PART_HEIGHT;
}

// Draw a brush as four quadrants: top-left, top-right, bottom-left, bottom-right.
// The current X,Y position determines the top-left corner, not the center.
void gm_draw_brush(uint16_t x, uint8_t y, uint8_t brush,
                   uint8_t pat_even, uint8_t pat_odd, gm_vector_ctx *c) {
    gm_brush_scratch s = {0};
    s.scanline = y;
    s.pattern_even = pat_even;
    s.pattern_odd = pat_odd;
    s.seed_column = x / COL_BITS;
    s.pixel_offset = x % COL_BITS;
    s.bitmap_index = brush * BRUSH_PART_SIZE * BRUSH_TOTAL_PARTS;

    if (s.bitmap_index + BRUSH_PART_SIZE * BRUSH_TOTAL_PARTS > GM_BRUSH_BITMAPS_SIZE)
        return;

    brush_quadrant(c, &s, 0, 0);                   // Top-left
    s.bitmap_index += BRUSH_PART_SIZE;
    brush_quadrant(c, &s, 1, 0);                   // Top-right
    s.bitmap_index += BRUSH_PART_SIZE;
    brush_quadrant(c, &s, 0, BRUSH_PART_HEIGHT);   // Bottom-left
    s.bitmap_index += BRUSH_PART_SIZE;
    brush_quadrant(c, &s, 1, BRUSH_PART_HEIGHT);   // Bottom-right
}
