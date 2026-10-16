//
//  c64a8draw.c
//
//  Atari 8-bit and Commodore 64 bitmap graphics for ScottFree and Plus,
//  two interpreters for adventures in Scott Adams formats
//
//  Routines to draw Atari 8-bit and C64 RLE graphics
//  Based on Code by David Lodge 29/04/2005
//
//  Original code at https://github.com/tautology0/textadventuregraphics

#include "glk.h"
#include "read_le16.h"
#include "c64a8draw.h"
#include "sagadraw_host.h"

#include "atari8_palette.h"

static void TranslateColorAtari8(uint8_t index, uint8_t value)
{
    SetColor(index, ataricolors[value]);
}

/* C64 colors */
enum c64_color {
    C64_WHITE,
    C64_RED,
    C64_PURPLE,
    C64_GREEN,
    C64_BLUE,
    C64_YELLOW,
    C64_ORANGE,
    C64_LRED,
    C64_GREY,
    C64_LGREEN,
    C64_LBLUE,
    C64_BROWN,
    C64_NUM_COLORS,
    C64_UNUSED = -1 /* leave the palette slot unchanged */
};

static const glui32 c64colors[C64_NUM_COLORS] = {
    [C64_WHITE]  = 0xffffff,
    [C64_RED]    = 0xbf6148,
    [C64_PURPLE] = 0xb159b9,
    [C64_GREEN]  = 0x79d570,
    [C64_BLUE]   = 0x5f48e9,
    [C64_YELLOW] = 0xf7ff6c,
    [C64_ORANGE] = 0xba8620,
    [C64_LRED]   = 0xe79a84,
    [C64_GREY]   = 0xa7a7a7,
    [C64_LGREEN] = 0xc0ffb9,
    [C64_LBLUE]  = 0xa28fff,
    [C64_BROWN]  = 0x837000,
};

/*
 The C64 colour for each colour value a game uses, determined by looking at
 the games running in the VICE C64 emulator.
 I have no idea how the original interpreter calculates
 them. I might have made some mistakes.
 C64_UNUSED values are not used by any game; they leave the palette slot
 holding whatever colour it had before. */
static const int8_t c64colormap[] = {
    /*   0 */ C64_PURPLE,  C64_BLUE,    C64_WHITE,   C64_WHITE,   C64_WHITE,   C64_UNUSED,  C64_UNUSED,  C64_BLUE,
    /*   8 */ C64_WHITE,   C64_WHITE,   C64_WHITE,   C64_WHITE,   C64_WHITE,   C64_WHITE,   C64_WHITE,   C64_WHITE,
    /*  16 */ C64_YELLOW,  C64_ORANGE,  C64_ORANGE,  C64_GREEN,   C64_GREEN,   C64_YELLOW,  C64_YELLOW,  C64_YELLOW,
    /*  24 */ C64_YELLOW,  C64_YELLOW,  C64_YELLOW,  C64_YELLOW,  C64_YELLOW,  C64_YELLOW,  C64_YELLOW,  C64_BROWN,
    /*  32 */ C64_BROWN,   C64_BROWN,   C64_BROWN,   C64_BROWN,   C64_BROWN,   C64_BROWN,   C64_BROWN,   C64_BROWN,
    /*  40 */ C64_BROWN,   C64_YELLOW,  C64_YELLOW,  C64_YELLOW,  C64_YELLOW,  C64_YELLOW,  C64_YELLOW,  C64_ORANGE,
    /*  48 */ C64_ORANGE,  C64_ORANGE,  C64_ORANGE,  C64_ORANGE,  C64_ORANGE,  C64_ORANGE,  C64_ORANGE,  C64_ORANGE,
    /*  56 */ C64_ORANGE,  C64_ORANGE,  C64_ORANGE,  C64_ORANGE,  C64_ORANGE,  C64_ORANGE,  C64_ORANGE,  C64_ORANGE,
    /*  64 */ C64_ORANGE,  C64_ORANGE,  C64_ORANGE,  C64_RED,     C64_RED,     C64_RED,     C64_RED,     C64_RED,
    /*  72 */ C64_PURPLE,  C64_PURPLE,  C64_PURPLE,  C64_PURPLE,  C64_PURPLE,  C64_PURPLE,  C64_PURPLE,  C64_PURPLE,
    /*  80 */ C64_PURPLE,  C64_PURPLE,  C64_PURPLE,  C64_PURPLE,  C64_PURPLE,  C64_PURPLE,  C64_PURPLE,  C64_PURPLE,
    /*  88 */ C64_UNUSED,  C64_LRED,    C64_PURPLE,  C64_PURPLE,  C64_PURPLE,  C64_PURPLE,  C64_PURPLE,  C64_PURPLE,
    /*  96 */ C64_PURPLE,  C64_PURPLE,  C64_PURPLE,  C64_PURPLE,  C64_PURPLE,  C64_PURPLE,  C64_PURPLE,  C64_PURPLE,
    /* 104 */ C64_PURPLE,  C64_PURPLE,  C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_LBLUE,   C64_UNUSED,
    /* 112 */ C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_BLUE,    C64_BLUE,    C64_BLUE,    C64_BLUE,
    /* 120 */ C64_BLUE,    C64_BLUE,    C64_BLUE,    C64_BLUE,    C64_BLUE,    C64_BLUE,    C64_BLUE,    C64_BLUE,
    /* 128 */ C64_BLUE,    C64_BLUE,    C64_BLUE,    C64_BLUE,    C64_BLUE,    C64_BLUE,    C64_BLUE,    C64_BLUE,
    /* 136 */ C64_UNUSED,  C64_BLUE,    C64_WHITE,   C64_WHITE,   C64_WHITE,   C64_WHITE,   C64_WHITE,   C64_UNUSED,
    /* 144 */ C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_BLUE,    C64_BLUE,    C64_BLUE,    C64_BLUE,
    /* 152 */ C64_BLUE,    C64_BLUE,    C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_LBLUE,   C64_UNUSED,  C64_UNUSED,
    /* 160 */ C64_UNUSED,  C64_GREY,    C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_UNUSED,
    /* 168 */ C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_UNUSED,
    /* 176 */ C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_GREEN,   C64_GREEN,   C64_GREEN,   C64_GREEN,   C64_GREEN,
    /* 184 */ C64_GREEN,   C64_GREEN,   C64_GREEN,   C64_GREEN,   C64_GREEN,   C64_GREEN,   C64_GREEN,   C64_GREEN,
    /* 192 */ C64_GREEN,   C64_GREEN,   C64_GREEN,   C64_GREEN,   C64_GREEN,   C64_GREEN,   C64_GREEN,   C64_GREEN,
    /* 200 */ C64_GREEN,   C64_LGREEN,  C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_UNUSED,
    /* 208 */ C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_GREEN,   C64_GREEN,   C64_GREEN,   C64_GREEN,
    /* 216 */ C64_GREEN,   C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_UNUSED,
    /* 224 */ C64_PURPLE,  C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_YELLOW,  C64_YELLOW,
    /* 232 */ C64_YELLOW,  C64_YELLOW,  C64_YELLOW,  C64_YELLOW,  C64_YELLOW,  C64_YELLOW,  C64_YELLOW,  C64_UNUSED,
    /* 240 */ C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_BROWN,   C64_BROWN,   C64_BROWN,   C64_BROWN,
    /* 248 */ C64_BROWN,   C64_UNUSED,  C64_UNUSED,  C64_UNUSED,  C64_YELLOW,  C64_UNUSED,  C64_UNUSED,  C64_WHITE
};

_Static_assert(sizeof(c64colormap) == 256, "one entry per colour value");

static void TranslateColorC64(uint8_t index, uint8_t value)
{
    int8_t color = c64colormap[value];
    if (color != C64_UNUSED)
        SetColor(index, c64colors[color]);
}

int DrawPatternAndAdvancePos(int x, int *y, uint8_t pattern) {
    uint8_t mask = 0xc0;

    for (int i = 6; i >= 0; i -= 2) {
        int color = (pattern & mask) >> i;
        PutDoublePixel(x, *y, color);
        mask >>= 2;
        x += 2;
    }
    (*y)++;
    return x - 8;
}

int DrawC64A8ImageFromData(uint8_t *ptr, size_t datasize, int voodoo_or_count, adjustments_fn adjustments, int is_c64)
{
    uint8_t pattern_top = 0, pattern_bottom = 0;
    int repetitions;
    int i;

    uint8_t *origptr = ptr;

    int xpos = 0;
    int ypos = 0;

    // Two bytes, the size, the edges and the palette
    if (datasize < 12)
        return 0;

    ptr += 2;

    // For Atari 8-bit, this vaule is already datasize
    // but on C64, datasize is the actual file size of the image
    // file on disk, so we shrink it here.
    size_t size = READ_LE_UINT16_AND_ADVANCE(&ptr);
    if (size < datasize - 2) {
        datasize = size + 2;
    }

    // Get the offset
    int left = *ptr++ - 3;
    int top = *ptr++;

    // Get the x position of the right edge
    int right = *ptr++;
    // Get the y position of the bottom edge
    int bottom = *ptr++;
    if (voodoo_or_count)
        bottom -= 2;

    // Custom graphics window adjustments
    // depending on whether we are being called
    // by Plus or by ScottFree.
    if (adjustments != NULL)
        right = adjustments(right, bottom, &left);

    xpos = left * 8;
    ypos = top;

    SetColor(0, 0); // Color 0 is always black

    // Get the palette
    for (i = 1; i < 5; i++) {
        uint8_t color = *ptr++;
        if (is_c64) {
            TranslateColorC64(i, color);
        } else {
            TranslateColorAtari8(i, color);
        }
    }

    while ((size_t)(ptr - origptr) < datasize - 2) {
        // First get repetitions
        repetitions = *ptr++;
        
        // If bit 7 is set or this is an early game
        // we write the next two bytes (repetitions) times
        int repeat_next_two_bytes = 0;
        if (((repetitions & 0x80) == 0x80) || voodoo_or_count) {
            repeat_next_two_bytes = 1;
            if (voodoo_or_count) {
                repetitions -= 1;
            } else {
                repetitions &= 0x7f;
            }
            pattern_top = *ptr++;
            pattern_bottom = *ptr++;
        }
        for (i = 0; i < repetitions + 1; i++) {
            // If we are not repeating bytes,
            // we read two new bytes on every iteration
            if (!repeat_next_two_bytes) {
                if ((size_t)(ptr - origptr) >= datasize - 1)
                    return 1;
                pattern_top = *ptr++;
                pattern_bottom = *ptr++;
            }
            if (xpos <= (right - 3) * 8) {
                // We print a "pattern" of 8 x 2 pixels,
                // or technically 4 x 2 double-width, non-square pixels
                xpos = DrawPatternAndAdvancePos(xpos, &ypos, pattern_top);
                xpos = DrawPatternAndAdvancePos(xpos, &ypos, pattern_bottom);
                if (ypos > bottom) {
                    xpos += 8;
                    ypos = top;
                }
            }
        }
    }
    return 1;
}
