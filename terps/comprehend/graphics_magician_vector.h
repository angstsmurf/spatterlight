/* graphics_magician_vector.h -- shared Graphics Magician pen-geometry
 * rasterizers (line + circle) for the CGA and PCjr renderers.
 *
 * draw_line and draw_circle are byte-for-byte identical between
 * graphics_magician_cga.cpp and graphics_magician_pcjr.cpp: both are faithful
 * ports of the same native primitives -- PicDrawLineBresenham (PC_GRAPH.OVR
 * 0x2c00) and PicDrawCircleBresenham (0x2b10) -- and the PCjr JR_GRAPH.OVR
 * copies are identical, so the PCjr renderer was hand-cloned from the CGA one.
 * They differ ONLY in the per-pixel writer (write_2bpp vs plot_pen), so we
 * factor the geometry here and let each renderer pass its own plot callback.
 *
 * NOT shared: the fill, brush and glyph primitives genuinely diverge between the
 * two renderers (2-bpp dither phase-walk + 3-byte spill vs straight 4-bpp nibble
 * writes), so they stay in their respective files.
 *
 * Templated on a Plot functor `void plot(int x, int y)` (the pen colour is
 * captured by the caller's lambda) so it inlines with no per-pixel indirection,
 * matching the header-only style of slow_draw_page.h.  The plot order is
 * preserved exactly so the slow-draw reveal sequence is unchanged.
 */

#ifndef COMPREHEND_GRAPHICS_MAGICIAN_VECTOR_H
#define COMPREHEND_GRAPHICS_MAGICIAN_VECTOR_H

#include <cstdint>  // uint16_t
#include <cstdlib>  // abs

namespace Glk {
namespace Comprehend {

// Major-axis Bresenham (PicDrawLineBresenham @0x2c00): the longer axis is
// stepped every pixel; the error term (minor - major) decides when the minor
// axis advances.  This exact formulation matters -- the symmetric two-axis
// variant tie-breaks the other way on 45deg-ish lines, shifting a line (and the
// fill it bounds) by one pixel.
template <typename Plot>
inline void gmvDrawLine(int x0, int y0, int x1, int y1, Plot plot) {
    plot(x0, y0);
    const int dx = abs(x1 - x0), dy = abs(y1 - y0);
    const int sx = (x0 < x1) ? 1 : -1;
    const int sy = (y0 < y1) ? 1 : -1;
    int x = x0, y = y0;
    if (dx >= dy) {              // x is the major axis
        int err = dy - dx;
        for (int i = 0; i < dx; i++) {
            x += sx;
            if (err < 0) err += dy;
            else { y += sy; err += dy - dx; }
            plot(x, y);
        }
    } else {                     // y is the major axis
        int err = dx - dy;
        for (int i = 0; i < dy; i++) {
            y += sy;
            if (err < 0) err += dx;
            else { x += sx; err += dx - dy; }
            plot(x, y);
        }
    }
}

// Midpoint circle (PicDrawCircleBresenham @0x2b10).  NOT the Apple renderer's
// variant: the error term starts at -r, the radius itself shrinks as the arc
// closes, and -- a genuine quirk -- the "error went non-negative" test is
// TEST [d],0x80, i.e. bit 7 of the 16-bit error word, not its sign.  This rounds
// the diagonal steps one pixel wider than the textbook midpoint circle.
template <typename Plot>
inline void gmvDrawCircle(int cx, int cy, int radius, Plot plot) {
    uint16_t d = (uint16_t)(0 - radius);                 // [0xb1f2] = -r
    int i = 0;                                           // [0xb1f0]
    int r = radius;                                      // [0x9d39], mutated
    for (;;) {
        plot(cx - i, cy - r);
        plot(cx + i, cy - r);
        plot(cx + i, cy + r);
        plot(cx - i, cy + r);
        plot(cx + r, cy - i);
        plot(cx - r, cy - i);
        plot(cx - r, cy + i);
        plot(cx + r, cy + i);
        d = (uint16_t)(d + 2 * i + 1);                   // 2bae
        i++;
        if (!(d & 0x80)) {                               // 2bbd: TEST d,0x80
            d = (uint16_t)(d + 2 - 2 * r);
            r--;
        }
        if (r < i)                                       // 2be0: JL -> done
            break;
    }
}

} // namespace Comprehend
} // namespace Glk

#endif
