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

#include <cstdint>  // uint8_t, uint16_t
#include <cstdlib>  // abs
#include <cstring>  // memcmp
#include <cstddef>  // size_t
#include <cstdint>  // SIZE_MAX
#include <utility>  // std::swap

namespace Glk {
namespace Comprehend {

// Reverse the 8 bits of a byte (bit 0 <-> bit 7): converts an LSB-first glyph
// row (charset.gda) to the MSB-first order both renderers' draw_glyph expects.
inline uint8_t gmvMirrorByte(uint8_t b) {
	b = (uint8_t)((b >> 4) | (b << 4));
	b = (uint8_t)(((b & 0xcc) >> 2) | ((b & 0x33) << 2));
	b = (uint8_t)(((b & 0xaa) >> 1) | ((b & 0x55) << 1));
	return b;
}

// Locate the 8 brush bitmaps (8 x 32 bytes) in a NOVEL.EXE / NOVEL1.EXE image.
// Brush 5 (the filled-circle default) is found by its distinctive left column and
// the table starts 5 * 32 bytes before it.  On success *base is the file offset of
// brush 0 and the whole 256-byte table lies inside the image.  Shared by the CGA
// v1 loader and the PCjr loader: both read the same brushes out of the same EXE.
inline bool gmvFindBrushTable(const uint8_t *exe, size_t size, size_t *base) {
	static const uint8_t kBrush5Sig[] = {
		0x00,0x03,0x1f,0x3f,0x3f,0x3f,0x7f,0x7f,
		0x7f,0x7f,0x3f,0x3f,0x3f,0x1f,0x03,0x00,
	};
	size_t b5 = SIZE_MAX;
	for (size_t i = 0; i + sizeof(kBrush5Sig) <= size; i++) {
		if (memcmp(exe + i, kBrush5Sig, sizeof(kBrush5Sig)) == 0) {
			b5 = i;
			break;
		}
	}
	if (b5 == SIZE_MAX || b5 < 5 * 32 || (b5 - 5 * 32) + 256 > size)
		return false;
	*base = b5 - 5 * 32;
	return true;
}

// Load the in-picture op3/op5 font (96 glyphs, chars 32-127, 8 bytes each) from a
// CHARSET.GDA image: version word 0x1100, then the glyphs at offset 4 stored
// LSB-first (bit 0 = leftmost pixel).  Each byte is reversed into the MSB-first
// order both renderers' draw_glyph expects.  Returns false (dst untouched) if the
// image is too short or has the wrong version.
inline bool gmvLoadCharsetGda(uint8_t dst[96 * 8], const uint8_t *gda, size_t size) {
	if (!gda || size < 4 + 96 * 8)
		return false;
	const uint16_t version = (uint16_t)(gda[0] | (gda[1] << 8));
	if (version != 0x1100)
		return false;
	for (int i = 0; i < 96 * 8; i++)
		dst[i] = gmvMirrorByte(gda[4 + i]);
	return true;
}

// Major-axis Bresenham (PicDrawLineBresenham @0x2c00): the longer axis is
// stepped every pixel; the error term (minor - major) decides when the minor
// axis advances.  This exact formulation matters -- the symmetric two-axis
// variant tie-breaks the other way on 45deg-ish lines, shifting a line (and the
// fill it bounds) by one pixel.
//
// A y-major line is drawn by swapping x and y, walking it as x-major, and
// swapping them back when plotting.  Lines with dx == dy stay x-major, as in
// the original.
template <typename Plot>
inline void gmvDrawLine(int x0, int y0, int x1, int y1, Plot plot) {
	const bool steep = abs(y1 - y0) > abs(x1 - x0);
	if (steep) {
		std::swap(x0, y0);
		std::swap(x1, y1);
	}
	auto put = [&](int x, int y) { if (steep) plot(y, x); else plot(x, y); };

	put(x0, y0);
	const int dx = abs(x1 - x0), dy = abs(y1 - y0);
	const int sx = (x0 < x1) ? 1 : -1;
	const int sy = (y0 < y1) ? 1 : -1;
	int x = x0, y = y0;
	int err = dy - dx;
	for (int i = 0; i < dx; i++) {
		x += sx;
		if (err < 0) err += dy;
		else { y += sy; err += dy - dx; }
		put(x, y);
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
