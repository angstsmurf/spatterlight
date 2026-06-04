//
//  alkatraz_screen.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  See alkatraz_screen.h. This mirrors the loader's SCADDS/STYLER routines:
//  DECLI moves one pixel row (up/down), DECSI moves one character column
//  (left/right), and COLOUR emits the attribute address for a cell whenever
//  the draw crosses a character-row boundary.
//

#include "alkatraz_screen.h"

/* DECLI: one pixel row up (dir 1/2) or down (dir 0/3), with the Spectrum
   display file's character-row carry. */
static uint16_t alk_next_line(uint16_t addr, int dir)
{
    if (dir == 1 || dir == 2) {                 /* up */
        addr = (uint16_t)(addr - 0x100);
        if (((addr ^ 0xffff) & 0x700) != 0)
            return addr;
        addr = (uint16_t)(addr - 0x20);
        if (((addr ^ 0xffff) & 0xe0) != 0)
            return (uint16_t)(addr + 0x800);
        return (uint16_t)(addr + 0x100);
    } else {                                    /* down */
        addr = (uint16_t)(addr + 0x100);
        if ((addr & 0x700) != 0)
            return addr;
        addr = (uint16_t)(addr + 0x20);
        if ((addr & 0xe0) != 0)
            return (uint16_t)(addr - 0x800);
        return (uint16_t)(addr - 0x100);
    }
}

/* DECSI: one character column right (dir 0/3) or left (dir 1/2). */
static uint16_t alk_next_col(uint16_t addr, int dir)
{
    int d = (dir == 0 || dir == 3) ? 1 : -1;
    return (uint16_t)((addr & 0xff00) | ((addr + d) & 0xff));
}

/* COLOUR: emit the attribute address for `addr` if it sits on a character-row
   boundary. The attribute address keeps the low byte (the column) and maps the
   high byte to 0x58 + third. */
static void alk_emit_attr(uint16_t addr, int dir, uint16_t *out, int *n, int maxout)
{
    int h = (addr >> 8) & 0xff;
    int boundary = ((dir == 1 || dir == 2) ? ~h : h) & 7;
    if (boundary == 0 && *n < maxout)
        out[(*n)++] = (uint16_t)(((0x58 + ((h >> 3) & 3)) << 8) | (addr & 0xff));
}

/* One strip being drawn. `inner` counts cells along the strip's fast axis
   (resetting to `inner_reset` at the end of each line/column); `outer` counts
   how many lines/columns remain. (olow, ohigh) remembers the strip's origin
   along the slow axis so each new line/column starts from the right place. */
typedef struct {
    int dir;
    int vertical;       /* 1 = strip runs across rows; 0 = down columns */
    int inner, inner_reset, outer;
    uint16_t addr;
    uint8_t olow, ohigh;
    int done;
} AlkSlot;

int AlkatrazScreenOrder(const uint8_t *descriptors, int nwin,
    uint16_t *out, int maxout)
{
    int n = 0;     /* number of addresses emitted so far */
    int w = 0;     /* index of the next descriptor to consume */

    /* Walk the descriptors in groups. The first descriptor of each group says
       how many strips (windows) in that group are drawn at the same time. */
    while (w < nwin && n < maxout) {
        const uint8_t *first = descriptors + w * 4;
        int sim = ((first[3] >> 5) & 7) + 1;   /* 1..8 simultaneous windows */
        if (w + sim > nwin)                    /* clamp a malformed last group */
            sim = nwin - w;

        /* Load the group's descriptors into draw-state slots. Each descriptor
           is 4 bytes: [0..1] start address (LE), [2] bits 6-7 direction +
           bits 0-5 width (chars), [3] bits 5-7 simultaneous-count + bits 0-4
           height (chars, x8 = pixels). Directions: 0=right,1=left,2=up,3=down. */
        AlkSlot slot[8];
        for (int s = 0; s < sim; s++) {
            const uint8_t *d = descriptors + (w + s) * 4;
            uint16_t addr = (uint16_t)(d[0] | (d[1] << 8));
            int dir = (d[2] >> 6) & 3;
            int width = d[2] & 0x3f;
            int height = (d[3] & 0x1f) * 8;
            slot[s].dir = dir;
            slot[s].addr = addr;
            slot[s].olow = addr & 0xff;          /* origin low byte (column)  */
            slot[s].ohigh = (addr >> 8) & 0xff;  /* origin high byte (row)    */
            slot[s].done = 0;
            if (dir >= 2) {
                /* Vertical window: drawn row by row. Each row is `width`
                   characters wide; there are `height` pixel rows. */
                slot[s].vertical = 1;
                slot[s].inner = slot[s].inner_reset = width;
                slot[s].outer = height;
            } else {
                /* Horizontal window: drawn column by column. Each column is
                   `height` pixels tall; there are `width` columns. */
                slot[s].vertical = 0;
                slot[s].inner = slot[s].inner_reset = height;
                slot[s].outer = width;
            }
        }
        w += sim;

        /* Emit the group's cells. Within a group the simultaneous strips are
           interleaved one cell each per pass (this is what makes the loader's
           wipe look "woven"), so round-robin the slots until every strip is
           finished. `guard` just bounds a runaway on malformed data. */
        int active = 1, guard = 0;
        while (active && n < maxout && guard++ < 0x40000) {
            active = 0;
            for (int s = 0; s < sim; s++) {
                if (slot[s].done)
                    continue;
                active = 1;

                /* Emit this strip's current cell: first its attribute address
                   if we just crossed onto a new character row, then the byte. */
                alk_emit_attr(slot[s].addr, slot[s].dir, out, &n, maxout);
                if (n < maxout)
                    out[n++] = slot[s].addr;

                /* Advance to the strip's next cell. */
                if (slot[s].vertical) {
                    /* Step along the row (a character column at a time). */
                    slot[s].addr = alk_next_col(slot[s].addr, slot[s].dir);
                    if (--slot[s].inner == 0) {
                        /* Row finished: back to the row's start column, drop
                           down/up one pixel row, and count the row off. */
                        slot[s].inner = slot[s].inner_reset;
                        slot[s].addr = (uint16_t)((slot[s].addr & 0xff00) | slot[s].olow);
                        slot[s].addr = alk_next_line(slot[s].addr, slot[s].dir);
                        slot[s].olow = slot[s].addr & 0xff;
                        if (--slot[s].outer == 0)
                            slot[s].done = 1;
                    }
                } else {
                    /* Step down the column (a pixel row at a time). */
                    slot[s].addr = alk_next_line(slot[s].addr, slot[s].dir);
                    if (--slot[s].inner == 0) {
                        /* Column finished: back to the column's start row,
                           move one character column across, count it off. */
                        slot[s].inner = slot[s].inner_reset;
                        slot[s].addr = (uint16_t)((slot[s].ohigh << 8) | slot[s].olow);
                        slot[s].addr = alk_next_col(slot[s].addr, slot[s].dir);
                        slot[s].olow = slot[s].addr & 0xff;
                        slot[s].ohigh = (slot[s].addr >> 8) & 0xff;
                        if (--slot[s].outer == 0)
                            slot[s].done = 1;
                    }
                }
            }
        }
    }

    return n;     /* total addresses written to out[] */
}
