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
    int n = 0, w = 0;

    while (w < nwin && n < maxout) {
        const uint8_t *first = descriptors + w * 4;
        int sim = ((first[3] >> 5) & 7) + 1;   /* 1..8 simultaneous windows */
        if (w + sim > nwin)
            sim = nwin - w;

        AlkSlot slot[8];
        for (int s = 0; s < sim; s++) {
            const uint8_t *d = descriptors + (w + s) * 4;
            uint16_t addr = (uint16_t)(d[0] | (d[1] << 8));
            int dir = (d[2] >> 6) & 3;
            int width = d[2] & 0x3f;
            int height = (d[3] & 0x1f) * 8;
            slot[s].dir = dir;
            slot[s].addr = addr;
            slot[s].olow = addr & 0xff;
            slot[s].ohigh = (addr >> 8) & 0xff;
            slot[s].done = 0;
            if (dir >= 2) {                      /* vertical: rows of `width` */
                slot[s].vertical = 1;
                slot[s].inner = slot[s].inner_reset = width;
                slot[s].outer = height;
            } else {                             /* horizontal: columns of `height` */
                slot[s].vertical = 0;
                slot[s].inner = slot[s].inner_reset = height;
                slot[s].outer = width;
            }
        }
        w += sim;

        /* Round-robin the simultaneous slots a byte at a time until all done. */
        int active = 1, guard = 0;
        while (active && n < maxout && guard++ < 0x40000) {
            active = 0;
            for (int s = 0; s < sim; s++) {
                if (slot[s].done)
                    continue;
                active = 1;

                alk_emit_attr(slot[s].addr, slot[s].dir, out, &n, maxout);
                if (n < maxout)
                    out[n++] = slot[s].addr;

                if (slot[s].vertical) {
                    slot[s].addr = alk_next_col(slot[s].addr, slot[s].dir);
                    if (--slot[s].inner == 0) {
                        slot[s].inner = slot[s].inner_reset;
                        slot[s].addr = (uint16_t)((slot[s].addr & 0xff00) | slot[s].olow);
                        slot[s].addr = alk_next_line(slot[s].addr, slot[s].dir);
                        slot[s].olow = slot[s].addr & 0xff;
                        if (--slot[s].outer == 0)
                            slot[s].done = 1;
                    }
                } else {
                    slot[s].addr = alk_next_line(slot[s].addr, slot[s].dir);
                    if (--slot[s].inner == 0) {
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

    return n;
}
