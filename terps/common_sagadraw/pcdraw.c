//
//  pcdraw.c
//
//  Used by ScottFree and Plus,
//  two interpreters for adventures in Scott Adams formats
//
//  Routines to draw IBM PC RLE bitmap graphics.
//  Based on Code by David Lodge.
//
//  Original code:
// https://github.com/tautology0/textadventuregraphics/blob/master/AdventureInternational/pcdraw.c
//

#include <stdint.h>
#include <stddef.h>

#include "glk.h"
#include "read_le16.h"

void PutPixelWithWidth(glsi32 xpos, glsi32 ypos, int32_t color, int pixelwidth);
void SetColor(int32_t index, glui32);

typedef struct {
    int x;
    int y;
    int right;
    int bottom;
    int left;
    int top;
    int ycount;
    int pixelwidth;
    int at_last_line;
} DrawState;

/* Advance x by dx and handle end-of-line/last-line logic. */
static inline void StepX(DrawState *st, int dx)
{
    st->x += dx;
    if (st->x >= st->right + st->left) {
        st->y += 2;
        st->x = st->left;
        st->ycount++;
    }
    if (st->ycount > st->bottom) {
        st->y = st->top + 1;
        st->at_last_line++;
        st->ycount = 0;
    }
}

/* Draw 4 two-bit pixels packed in "pattern". */
/* Skip every other pixel when pixelwidth is 1 */
static inline void DrawPattern(DrawState *st, uint8_t pattern)
{
    uint8_t mask = 0xc0;

    for (int i = 6; i >= 0; i -= 2) {
        int color = (pattern & mask) >> i;
        PutPixelWithWidth(st->x, st->y, color, st->pixelwidth);
        StepX(st, st->pixelwidth);
        mask >>= 2;
    }
}

int DrawDOSImageFromData(uint8_t *ptr)
{
    if (!ptr) return 0;

    DrawState st = {0};

    /* set up the palette */
    const glui32 palette[4] = {
        0x000000, /* black */
        0x00ffff, /* cyan */
        0xff00ff, /* magenta */
        0xffffff  /* white */
    };

    for (int i = 0; i < 4; ++i) {
        SetColor(i, palette[i]);
    }

    /* Get the size of the graphics chunk */
    size_t size = READ_LE_UINT16(ptr + 5);

    /* Get whether it is striped */
    st.pixelwidth = 1 + (ptr[0x0d] == 0xff);

    /* Get the offset */
    int rawoffset = READ_LE_UINT16(ptr + 0x0f);
    st.left = ((rawoffset % 80) * 4) - 24;
    st.top = rawoffset / 40;
    st.top -= (st.top & 1);
    st.x = st.left;
    st.y = st.top;

    /* Get the height and width */
    st.bottom = (READ_LE_UINT16(ptr + 0x11) - rawoffset) / 80;
    st.right = ptr[0x13] * 4;

    uint8_t *endptr = ptr + size;
    ptr += 0x17;

    while (ptr < endptr && st.at_last_line <= 1) {
        uint8_t count = *ptr++;
        int repeat = (count & 0x80) != 0;
        int reps = (count & 0x7f) + 1;

        if (repeat) {
            if (ptr >= endptr) break;
            uint8_t value = *ptr++;
            for (int i = 0; i < reps && st.at_last_line <= 1; ++i)
                DrawPattern(&st, value);
        } else {
            for (int i = 0; i < reps && ptr < endptr && st.at_last_line <= 1; ++i) {
                uint8_t value = *ptr++;
                DrawPattern(&st, value);
            }
        }
    }

    return 1;
}
