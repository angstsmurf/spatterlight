// DOS (IBM PCjr / Tandy 16-colour) Graphics Magician picture renderer, in the 320x200 BIOS mode 9
// framebuffer that NOVEL.EXE drives through JR_GRAPH.OVR. The model was derived from disassembly
// of JR_GRAPH.OVR and NOVEL.EXE. The picture data is shared with the CGA renderer, so the vector
// geometry is identical and only the pixel model and tables differ:
//   - 4 bits/pixel, 2 pixels/byte; the leftmost pixel of a byte is its high nibble.
//   - Pen colours go through 8-entry colLow[]/colHigh[] nibble tables indexed by (pen & 7). White
//     (entries 3 and 7) carries 0xff in colHigh, so the original write-modify-write also whitens
//     the adjacent low nibble, a one-pixel bleed reproduced here.
//   - Fills use a 30-entry x 4-byte dither table selected through a 108-entry subindex with one
//     pattern index per row parity, as in the v1 CGA path, but the first index is for odd rows.
//   - Brush bitmaps and the font are mode-independent masks, byte-identical to the CGA renderer.

#include "graphics_magician_pcjr.h"
#include "graphics_magician.h"         // Opcode enum
#include "graphics_magician_vector.h"  // shared line/circle rasterizers
#include "gm_stream.h"                 // shared opcode dispatcher
#include "slow_draw_page.h"            // shared progressive-reveal helper
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>               // gmpcjr_flood_fill's scanline stack

namespace Glk {
namespace Comprehend {

// Drawing tables

static uint8_t s_colLow[8];        // pen (0-7) -> colour in low nibble
static uint8_t s_colHigh[8];       // pen (0-7) -> colour in high nibble (white=0xff)
static uint8_t s_maskHigh = 0x0f;  // AND mask when writing the high nibble (even x)
static uint8_t s_maskLow  = 0xf0;  // AND mask when writing the low nibble  (odd x)
static uint8_t s_fillPat[30][4];   // 16-colour 4bpp dither tiles (4 column phases)
static uint8_t s_fillSubindex[512];// op6 byte -> (odd,even) pattern index pair; sel*2+parity max 511
static uint8_t s_brushBitmaps[256];// 8 brushes x 32 bytes (shared with CGA)
static uint8_t s_fontGlyphs[96 * 8];// chars 32-127, 8 bytes each, MSB-first

static bool s_haveDrawingTables = false;

bool gmpcjrHaveDrawingTables() { return s_haveDrawingTables; }

bool gmpcjrInstallDrawingTables(const uint8_t *ovr, size_t ovrSize,
								const uint8_t *exe, size_t exeSize) {
	if (!ovr || !exe)
		return false;

	// Locate the overlay's data block by the bankStride word (0x2000) and the two nibble masks,
	// which immediately precede the pen colour tables. The signature is the same in Transylvania
	// and Crimson Crown; the tables after it are read per game.
	static const uint8_t kSig[] = { 0x00, 0x20, 0x0f, 0xf0 };
	size_t sig = SIZE_MAX;
	for (size_t i = 0; i + sizeof(kSig) <= ovrSize; i++) {
		if (std::memcmp(ovr + i, kSig, sizeof(kSig)) == 0) {
			sig = i;
			break;
		}
	}
	if (sig == SIZE_MAX)
		return false;

	const size_t base = sig + 4;          // colLow; colHigh follows at +8
	const size_t fillOff = base + 0x2b;
	const size_t subOff  = base + 0xa3;
	if (subOff + 108 * 2 > ovrSize)
		return false;

	s_maskHigh = ovr[sig + 2];
	s_maskLow  = ovr[sig + 3];
	std::memcpy(s_colLow,  ovr + base,     8);
	std::memcpy(s_colHigh, ovr + base + 8, 8);
	std::memcpy(s_fillPat, ovr + fillOff, sizeof(s_fillPat));   // 30 x 4 bytes
	std::memset(s_fillSubindex, 0, sizeof(s_fillSubindex));
	std::memcpy(s_fillSubindex, ovr + subOff, 108 * 2);

	// Brush bitmaps live in NOVEL.EXE, byte-identical to the CGA renderer.
	size_t brushBase;
	if (!gmvFindBrushTable(exe, exeSize, &brushBase))
		return false;
	std::memcpy(s_brushBitmaps, exe + brushBase, sizeof(s_brushBitmaps));

	// No embedded picture font (as in v1 CGA): op3/op5 glyphs come from
	// CHARSET.GDA via gmpcjrSetFont().  Leave zeroed until then.
	std::memset(s_fontGlyphs, 0, sizeof(s_fontGlyphs));

	s_haveDrawingTables = true;
	return true;
}

bool gmpcjrSetFont(const uint8_t *charsetGda, size_t size) {
	return gmvLoadCharsetGda(s_fontGlyphs, charsetGda, size);
}

// 16-colour framebuffer: the mode 9 logical page, 2 pixels/byte (high nibble = left pixel), 160
// bytes per row. The real 4-bank VRAM interleave only affects addressing, so a flat buffer holds
// the same pixels. The picture window is at x+20 px and spans the same 280x160 logical area as
// the CGA/Apple renderers.

#define PCJR_BYTES_PER_ROW 160
#define PCJR_ROWS          200
#define PCJR_PIC_W         280   // logical picture width  (x 0..279)
#define PCJR_PIC_H         160   // logical picture height (y 0..159)
#define PCJR_X_OFFSET      20    // picture window offset in pixels

static uint8_t s_screen[PCJR_ROWS * PCJR_BYTES_PER_ROW];

// Slow ("animated") draw: s_screen is the fully drawn page; s_slowScreen is the page the host
// reveals.

static uint8_t s_slowScreen[PCJR_ROWS * PCJR_BYTES_PER_ROW];
static SlowDrawPage s_slow(s_screen, s_slowScreen, sizeof(s_screen),
						   PCJR_BYTES_PER_ROW);

static inline void store_byte(int off, uint8_t value) {
	s_screen[off] = value;
	s_slow.record(off, value);
}

// Read the colour index (0-15) of the pixel at logical picture (x,y).
static inline uint8_t read_pixel(int x, int y) {
	if ((unsigned)x >= PCJR_PIC_W || (unsigned)y >= PCJR_PIC_H) return 0;
	const int gx = x + PCJR_X_OFFSET;
	const uint8_t b = s_screen[y * PCJR_BYTES_PER_ROW + (gx >> 1)];
	return (gx & 1) ? (uint8_t)(b & 0x0f) : (uint8_t)(b >> 4);
}

// Set the nibble for logical picture (x,y) to colour `col` (0-15).
static inline void write_pixel(int x, int y, uint8_t col) {
	if ((unsigned)x >= PCJR_PIC_W || (unsigned)y >= PCJR_PIC_H) return;
	const int gx = x + PCJR_X_OFFSET;
	const int off = y * PCJR_BYTES_PER_ROW + (gx >> 1);
	uint8_t b = s_screen[off];
	if (gx & 1) b = (uint8_t)((b & 0xf0) | (col & 0x0f));
	else        b = (uint8_t)((b & 0x0f) | (col << 4));
	store_byte(off, b);
}

// Pen plot: the pen colour byte (colLow/colHigh) is OR'd into the target nibble after masking.
// For pen white (colHigh==0xff) the OR also whitens the adjacent low nibble, as in the original.
static inline void plot_pen(int x, int y, uint8_t pen) {
	if ((unsigned)x >= PCJR_PIC_W || (unsigned)y >= PCJR_PIC_H) return;
	const int gx = x + PCJR_X_OFFSET;
	const int off = y * PCJR_BYTES_PER_ROW + (gx >> 1);
	const uint8_t p = (uint8_t)(pen & 7);
	uint8_t b = s_screen[off];
	if (gx & 1) b = (uint8_t)((b & s_maskLow)  | s_colLow[p]);
	else        b = (uint8_t)((b & s_maskHigh) | s_colHigh[p]);
	store_byte(off, b);
}

void gmpcjrResetScreen(bool white) {
	memset(s_screen,     white ? 0xff : 0x00, sizeof(s_screen));
	memset(s_slowScreen, white ? 0xff : 0x00, sizeof(s_slowScreen));
	s_slow.clear();
}

// Fill pattern: op6 selector -> subindex pair -> 4bpp dither tile. The first tile of the pair is
// for odd rows and the second for even rows. The tile's byte for a global byte column is
// s_fillPat[idx][byteCol & 3]; the pixel nibble is the high or low half by (x+20) parity.
//
// The flood fill paints a word at a time and doubles one tile byte into both halves, so it only
// uses tile bytes 0 and 2 (`wordFill`). Brushes and glyphs go a byte at a time and use all four.
// The difference shows in the eight tiles whose bytes alternate, e.g. the castle of the
// Transylvania title.
static inline uint8_t fill_pixel(uint8_t sel, int x, int y, bool wordFill = false) {
	const uint8_t idx = s_fillSubindex[(unsigned)sel * 2 + ((y & 1) ^ 1)];
	if (idx >= 30) return 0;
	const int gx = x + PCJR_X_OFFSET;
	const uint8_t pb = s_fillPat[idx][(gx >> 1) & (wordFill ? 2 : 3)];
	return (gx & 1) ? (uint8_t)(pb & 0x0f) : (uint8_t)(pb >> 4);
}

// Flood fill (op14 PAINT): the 4-connected run of white (15) pixels reachable from the seed,
// bounded by any non-white pixel. This is a scanline flood with a visited mask, because the dither
// can re-introduce white, and not the original's span queue. Traversal order could only affect
// dither self-limiting at boundaries, and the pictures checked show no such difference.
static uint8_t s_visited[PCJR_PIC_W * PCJR_PIC_H];

static void gmpcjr_flood_fill(int seedx, int seedy, uint8_t sel) {
	if ((unsigned)seedx >= PCJR_PIC_W || (unsigned)seedy >= PCJR_PIC_H) return;
	if (read_pixel(seedx, seedy) != 15) return;

	memset(s_visited, 0, sizeof(s_visited));
	struct Seed { int x, y; };
	std::vector<Seed> stack;
	stack.push_back({ seedx, seedy });

	while (!stack.empty()) {
		Seed s = stack.back();
		stack.pop_back();
		int x = s.x, y = s.y;
		if ((unsigned)y >= PCJR_PIC_H) continue;

		// Walk left to the start of the white run on this row.
		while (x > 0 && read_pixel(x - 1, y) == 15 && !s_visited[y * PCJR_PIC_W + (x - 1)])
			x--;

		bool spanAbove = false, spanBelow = false;
		for (; x < PCJR_PIC_W; x++) {
			const int vi = y * PCJR_PIC_W + x;
			if (s_visited[vi] || read_pixel(x, y) != 15)
				break;
			s_visited[vi] = 1;
			write_pixel(x, y, fill_pixel(sel, x, y, true));

			if (y > 0) {
				bool up = read_pixel(x, y - 1) == 15 && !s_visited[(y - 1) * PCJR_PIC_W + x];
				if (up && !spanAbove) { stack.push_back({ x, y - 1 }); spanAbove = true; }
				else if (!up) spanAbove = false;
			}
			if (y < PCJR_PIC_H - 1) {
				bool dn = read_pixel(x, y + 1) == 15 && !s_visited[(y + 1) * PCJR_PIC_W + x];
				if (dn && !spanBelow) { stack.push_back({ x, y + 1 }); spanBelow = true; }
				else if (!dn) spanBelow = false;
			}
		}
	}
}

// Line (op10) and box (op9); the geometry is shared with the CGA renderer.
static void draw_line(int x0, int y0, int x1, int y1, uint8_t pen) {
	gmvDrawLine(x0, y0, x1, y1,
				[pen](int x, int y) { plot_pen(x, y, pen); });
}

// Circle (op11); the geometry is shared with the CGA renderer.
static void draw_circle(int cx, int cy, int radius, uint8_t pen) {
	gmvDrawCircle(cx, cy, radius,
				  [pen](int x, int y) { plot_pen(x, y, pen); });
}

// Brush stamp (op12). Each brush is 32 bytes = four 8x8 quadrants (Q0 upper-left, Q1 lower-left,
// Q2 upper-right, Q3 lower-right), bytes MSB-first. A set bit takes the current fill-pattern
// colour at that pixel; clear bits leave the background.
static void blit_quadrant(const uint8_t *bytes8, int px, int py, uint8_t fill_sel) {
	for (int row = 0; row < 8; row++, py++) {
		const uint8_t b = bytes8[row];
		if (!b) continue;
		for (int p = 0; p < 8; p++) {
			if (!((b >> (7 - p)) & 1)) continue;
			const int x = px + p;
			write_pixel(x, py, fill_pixel(fill_sel, x, py));
		}
	}
}

static void draw_brush(int x, int y, uint8_t brush, uint8_t fill_sel) {
	gmvStampBrush(s_brushBitmaps, x, y, brush,
				  [fill_sel](const uint8_t *q, int px, int py) { blit_quadrant(q, px, py, fill_sel); });
}

// Text glyph. op3 (TEXT_CHAR): each set glyph pixel inverts the background nibble, so white
// background gives black letters. op5 (TEXT_OUTLINE): each set pixel takes the fill-pattern
// colour. Glyph bytes are MSB-first; the text cursor advances 8 px/char.
static void draw_glyph(int x, int y, uint8_t ch, bool invert, uint8_t fill_sel) {
	gmvDrawGlyph(s_fontGlyphs, x, y, ch, [invert, fill_sel](int px, int py) {
		if (invert) write_pixel(px, py, (uint8_t)(read_pixel(px, py) ^ 0x0f));
		else        write_pixel(px, py, fill_pixel(fill_sel, px, py));
	});
}

// Opcode interpreter: the PCjr side of the shared gmRunStream (gm_stream.h).
struct PcjrRenderer {
	int16_t  pen_x, pen_y;
	uint8_t  pen;       // pen colour index (0-15; & 7 selects colLow/colHigh)
	uint8_t  brush;     // brush index 0-7
	uint8_t  fill_sel;  // op6 selector byte
	uint16_t text_x;
	uint8_t  text_y;

	uint16_t scaleX(uint16_t x) const { return x; }
	int penX() const { return pen_x; }
	int penY() const { return pen_y; }

	void beforeOp(const uint8_t *, const uint8_t *, uint8_t, uint8_t) {}
	void afterOp(uint8_t, uint8_t) {}

	void begin() {
		// The op list is dropped by gmpcjrResetScreen() when a new page starts, not here: item
		// overlays append to the room's pending reveal, so the whole scene reveals in paint order.
		pen      = 0;
		pen_x    = 140;   // per-image init: pen (140,6), brush 5
		pen_y    = 6;
		brush    = 5;
		fill_sel = 0;
		text_x   = 140; text_y = 96;
	}
	void end() { s_slow.syncIfNotRecording(); }

	void setTextPos(uint16_t x, uint8_t y) { text_x = x; text_y = y; }
	// op3 inverts (black on white); op5 takes the fill pattern.
	void drawText(uint8_t ch, bool normal) { draw_glyph(text_x, text_y, ch, normal, fill_sel); }
	void advanceText() { text_x += 8; }
	void setPen(uint8_t param) { pen = (uint8_t)(param & 0xf); }
	void setShape(uint8_t param) { brush = param; }
	void setFill(uint8_t b) { fill_sel = b; }
	bool op7(const uint8_t *&p, const uint8_t *e) { return gmSkipOp7(p, e); }   // 2-operand no-op
	void moveTo(uint16_t x, uint8_t y) { pen_x = (int16_t)x; pen_y = (int16_t)y; }
	void lineTo(uint16_t x, uint8_t y) {
		draw_line(pen_x, pen_y, (int)x, (int)y, pen);
		pen_x = (int16_t)x; pen_y = (int16_t)y;
	}
	void circle(uint8_t radius) { draw_circle(pen_x, pen_y, radius, pen); }
	// Brush stamp one px left of the stream coordinate, like CGA.
	void drawBrush(uint16_t x, uint8_t y) { draw_brush((int)x - 1, (int)y, brush, fill_sel); }
	void delay(uint8_t units) { s_slow.recordDelay(units); }
	void paint(uint16_t x, uint8_t y) { gmpcjr_flood_fill((int)x, (int)y, fill_sel); }
	// As in v1 CGA, op15 is flood-fill clip setup, not a paint: param 1 resets the clip to the
	// window and >=2 reads 4 bytes. It is inert for the shipped corpus, so just consume the bytes
	// to keep the stream aligned.
	bool opReset(uint8_t param, const uint8_t *&p, const uint8_t *e) {
		if (param >= 2) {
			GmFillBounds clip;
			if (!clip.read(p, e))
				return true;
		}
		return false;
	}
};

void gmpcjrDrawImage(const uint8_t *data, size_t size) {
	PcjrRenderer r;
	gmRunStream(r, data, size);
}

// 16-colour to RGBA: the standard PCjr/CGA RGBI palette (the game programs no custom palette).
// Output is the 280x160 picture window, scaled to the requested surface size.
static const uint32_t kPcjrPalette[16] = {
	0x000000ffu, // 0  black
	0x0000aaffu, // 1  blue
	0x00aa00ffu, // 2  green
	0x00aaaaffu, // 3  cyan
	0xaa0000ffu, // 4  red
	0xaa00aaffu, // 5  magenta
	0xaa5500ffu, // 6  brown
	0xaaaaaaffu, // 7  light grey
	0x555555ffu, // 8  dark grey
	0x5555ffffu, // 9  light blue
	0x55ff55ffu, // 10 light green
	0x55ffffffu, // 11 light cyan
	0xff5555ffu, // 12 light red
	0xff55ffffu, // 13 light magenta
	0xffff55ffu, // 14 yellow
	0xffffffffu, // 15 white
};

static void blit_page(const uint8_t *page, uint32_t *out, int w, int h) {
	for (int y = 0; y < h; y++) {
		const int sy = (y * PCJR_PIC_H) / h;
		const uint8_t *row = &page[sy * PCJR_BYTES_PER_ROW];
		for (int x = 0; x < w; x++) {
			const int sx = (x * PCJR_PIC_W) / w;       // logical picture x
			const int gx = sx + PCJR_X_OFFSET;
			const uint8_t byteval = row[gx >> 1];
			const uint8_t col = (gx & 1) ? (byteval & 0x0f) : (byteval >> 4);
			out[y * w + x] = kPcjrPalette[col];
		}
	}
}

void gmpcjrBlitToSurface(uint32_t *out, int w, int h) {
	blit_page(s_screen, out, w, h);
}

// Slow-draw control, delegated to the shared helper.

void gmpcjrSetSlowDraw(bool on) { s_slow.setRecording(on); }
bool gmpcjrSlowDrawActive() { return s_slow.active(); }
bool gmpcjrAdvanceSlowDraw(int budget) { return s_slow.advance(budget); }
void gmpcjrFinishSlowDraw() { s_slow.finish(); }
bool gmpcjrSlowConsumeDirty(int *y0, int *y1) { return s_slow.consumeDirty(y0, y1); }

void gmpcjrBlitSlowToSurface(uint32_t *out, int w, int h) {
	blit_page(s_slowScreen, out, w, h);
}

} // namespace Comprehend
} // namespace Glk
