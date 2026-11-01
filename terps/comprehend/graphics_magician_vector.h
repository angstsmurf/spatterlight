// Shared Graphics Magician pen geometry: line and circle for the CGA and PCjr renderers, which
// differ only in their per-pixel writer, and the 6502 circle stepper for the Apple II hi-res and
// DHGR renderers. The fills diverge (2-bpp dither vs 4-bpp nibble writes) and stay in their files.
// The geometry is templated on a Plot functor `void plot(int x, int y)`. The plot order must not
// change, as it is the slow-draw reveal sequence.

#ifndef COMPREHEND_GRAPHICS_MAGICIAN_VECTOR_H
#define COMPREHEND_GRAPHICS_MAGICIAN_VECTOR_H

#include <cstdint>  // uint8_t, uint16_t, SIZE_MAX
#include <cstdlib>  // abs
#include <cstring>  // memcmp
#include <cstddef>  // size_t
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

// Brush stamp (op12).  Each brush is 32 bytes = four 8x8 quadrants, laid out in the
// DOS data as Q0 upper-left (+0), Q1 lower-left (+8), Q2 upper-right (+16), Q3
// lower-right (+24), and drawn left column first, top half then bottom half.  The
// renderer's `blit(bytes8, px, py)` paints one quadrant.  (The Apple double hi-res
// renderer lays its quadrants out differently and does not use this.)
template <typename Blit>
inline void gmvStampBrush(const uint8_t *bitmaps, int x, int y, uint8_t brush, Blit blit) {
	if (brush >= 8) return;
	const uint8_t *base = &bitmaps[brush * 32];
	blit(base,      x,     y);       // Q0 upper-left
	blit(base + 8,  x,     y + 8);   // Q1 lower-left
	blit(base + 16, x + 8, y);       // Q2 upper-right
	blit(base + 24, x + 8, y + 8);   // Q3 lower-right
}

// Text glyph (op3/op5).  `font` holds chars 32-127, 8 MSB-first bytes each (bit 7 =
// leftmost pixel); characters outside that range draw nothing.  pixel(px, py) is
// called for every set glyph pixel in row-major order and does the renderer's own
// clipping and plot (XOR for op3, fill pattern for op5).
template <typename Pixel>
inline void gmvDrawGlyph(const uint8_t *font, int x, int y, uint8_t ch, Pixel pixel) {
	if (ch < 32 || ch >= 128) return;
	const uint8_t *glyph = &font[(ch - 32) * 8];
	for (int row = 0; row < 8; row++) {
		const uint8_t b = glyph[row];
		if (!b) continue;
		for (int p = 0; p < 8; p++)
			if ((b >> (7 - p)) & 1)
				pixel(x + p, y + row);
	}
}

// Major-axis Bresenham: the longer axis is stepped every pixel; the error term (minor - major)
// decides when the minor axis advances. This exact formulation matters: the symmetric two-axis
// variant tie-breaks the other way on near-45-degree lines, shifting a line and the fill it
// bounds by one pixel. A y-major line is walked with x and y swapped; dx == dy stays x-major.
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

// Midpoint circle of the DOS renderers, not the Apple variant: the error term starts at -r, the
// radius shrinks as the arc closes, and the "error went non-negative" test checks bit 7 of the
// 16-bit error word rather than its sign. That quirk of the original rounds the diagonal steps
// one pixel wider than the textbook circle.
template <typename Plot>
inline void gmvDrawCircle(int cx, int cy, int radius, Plot plot) {
	uint16_t d = (uint16_t)(0 - radius);
	int i = 0;
	int r = radius;
	for (;;) {
		plot(cx - i, cy - r);
		plot(cx + i, cy - r);
		plot(cx + i, cy + r);
		plot(cx - i, cy + r);
		plot(cx + r, cy - i);
		plot(cx - r, cy - i);
		plot(cx - r, cy + i);
		plot(cx + r, cy + i);
		d = (uint16_t)(d + 2 * i + 1);
		i++;
		if (!(d & 0x80)) {
			d = (uint16_t)(d + 2 - 2 * r);
			r--;
		}
		if (r < i)
			break;
	}
}

// The eight-way symmetric plot of the Apple II circle loops: (+-ax, +-ay) near the top and bottom,
// then (+-bx, +-by) near the sides, in the original's order.
template <class Plot>
inline void gmvCirclePlot8(int cx, int cy, int ax, int ay, int bx, int by, Plot plot) {
	plot(cx - ax, cy - ay);
	plot(cx + ax, cy - ay);
	plot(cx + ax, cy + ay);
	plot(cx - ax, cy + ay);
	plot(cx + bx, cy - by);
	plot(cx - bx, cy - by);
	plot(cx - bx, cy + by);
	plot(cx + bx, cy + by);
}

// Midpoint circle stepper of the Apple II renderers. i walks up from 0 while r shrinks from the
// radius, the decision variable starting at radius / 2, calling `step(i, r)` at each point. The
// DHGR loop also plots at the odd x between steps, so after each advance `half(i, r, shrink)` gets
// the new (i, r) and whether r just shrank. The walk ends once r < i, after that half step, and
// returns that (i, r). The hi-res loop stops there; the DHGR loop tests one full step later, so
// it plots one more step at the returned (i, r) itself.
struct GmvCircleEnd { int i, r; };
template <class Step, class Half>
inline GmvCircleEnd gmvAppleCircleSteps(uint8_t radius, Step step, Half half) {
	int r = radius, d = r >> 1;
	if (radius == 0)
		return { 0, r };
	for (int i = 0;;) {
		step(i, r);
		i++;
		const bool shrink = d < i;
		if (shrink) {
			r--;
			d += r - i;
		} else {
			d -= i;
		}
		half(i, r, shrink);
		if (r < i)
			return { i, r };
	}
}
template <class Step>
inline GmvCircleEnd gmvAppleCircleSteps(uint8_t radius, Step step) {
	return gmvAppleCircleSteps(radius, step, [](int, int, bool) {});
}

} // namespace Comprehend
} // namespace Glk

#endif
