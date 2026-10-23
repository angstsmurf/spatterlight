/* graphics_magician_dhgr.cpp -- Apple II DOUBLE hi-res ("<D>" mode) renderer for
 * the V2 Comprehend / Polarware games (Talisman, OO-Topos, Transylvania v2, The
 * Coveted Mirror).
 *
 * Companion to graphics_magician.cpp (the STANDARD hi-res renderer). Both consume
 * the SAME Penguin "Graphics Magician" / Picture Painter vector stream from the
 * RA/OA picture files -- the format is resolution-independent, the double-hi-res
 * interpreter simply doubles the X coordinate and plots into a 560-wide aux/main
 * framebuffer (7 px/byte, column even -> aux, odd -> main, byte index col>>1).
 * The finished pages are converted to RGBA with MAME's composite-DHGR NTSC
 * artifact-colour kernel (gm_render_dhgr_line_colors in common_sagadraw).
 *
 * The line / flood-fill / brush primitives are ported byte-faithfully from the
 * real 6502 double-hi-res interpreter (validated pixel-exact against MAME for
 * Talisman's prison cell, RA image 4 -- see the regressions in test/).
 * The colour, sub-table, pattern and brush tables are loaded at runtime from
 * the boot disk's "T5" file (gmDhgrInstallDrawingTables); NOTHING is baked.
 * Per-image colour overrides arrive as op7 commands (see below).
 *
 * op7 (0x7x) -- reverse-engineered from the interpreter ($0951): NOT a no-op. It
 * is a per-image SET-PALETTE-SUBINDEX command: `aux[$41B2 + operand1] = operand2`,
 * patching the colour subindex table so the following op6 SET_FILL_COLOR resolves
 * to the image's intended pattern. The op6 builder ($0D1A) does:
 *   idx = subindex[colour];  b0,b1 = subtable[2*idx{,+1}];
 *   even[y] = pattern[(b0<<3)+y];  odd[y] = pattern[(b1<<3)+y].
 */

#include "graphics_magician_dhgr.h"
#include "graphics_magician.h"
#include "gm_artifact.h"
#include "gm_hires.h"
#include "gm_stream.h"         // shared opcode dispatcher
#include "slow_draw_page.h"     // shared progressive-reveal helper
#include "gm_span_fill.h"       // shared op14 flood fill
#include <cstdlib>
#include <cstring>

namespace Glk {
namespace Comprehend {

// ---- framebuffer --------------------------------------------------------------
#define A2_PAGE_SIZE   GM_HIRES_PAGE_SIZE
#define DHGR_HEIGHT    192
#define DHGR_PIC_ROWS  160   // picture region (text rows 0xa0..0xbf excluded)
#define DHGR_COLS      80    // 560 px / 7

static uint8_t s_main[A2_PAGE_SIZE];
static uint8_t s_aux[A2_PAGE_SIZE];

#define CALC(y) gm_row_address(y)

// ---- Slow ("animated") draw ---------------------------------------------------
//
// Mirrors the standard-hi-res renderer (graphics_magician.cpp): when recording
// is enabled (gmDhgrSetSlowDraw(true)), every byte the interpreter plots goes
// both into the final pages (s_main/s_aux, as before) and onto an ordered op
// list, with op13 DELAY operands recorded as pause markers. The visible pages
// (s_slowMain/s_slowAux) start at the background and the host re-applies the ops
// a chunk at a time on a Glk timer, re-blitting after each chunk. Item overlays
// start from whatever is already on the slow pages (the room just finished), so
// they compose exactly as on the real machine.

// The reveal state machine, op list and dirty band live in the shared
// SlowDrawEngine (slow_draw_page.h); the DHGR policy below supplies the
// renderer-specific behaviour: dual main/aux pages, a page-aware run test, and
// the Coveted-Mirror panel-column exclusion during a reveal.
static uint8_t s_slowMain[A2_PAGE_SIZE]; // progressively-revealed pages
static uint8_t s_slowAux[A2_PAGE_SIZE];

// The reveal maps a byte write back to its row (dirty band) and column (so the CM
// panel columns can be excluded from the reveal -- see dhgr_apply_slow) with
// gm_row_of_offset / gm_col_of_offset; both give -1 in the inter-row gaps of the
// page address space.

// Apply one recorded byte to the matching slow page; defined after the CM-panel
// state it consults (forward-declared here for the policy).
static bool dhgr_apply_slow(const SlowWriteOp &op);

// DHGR page: a main/aux pair (op.page picks which), so the run test also keys on
// page, and panel columns are skipped during the reveal.
struct DhgrSlowPolicy {
	bool apply(const SlowWriteOp &op) { return dhgr_apply_slow(op); }
	int rowOf(const SlowWriteOp &op) const { return gm_row_of_offset(op.offset); }
	bool adjacent(const SlowWriteOp &a, const SlowWriteOp &b) const {
		return (a.page == b.page &&
				(int)b.offset >= (int)a.offset - 1 && (int)b.offset <= (int)a.offset + 1) ||
			   b.value == a.value;
	}
	void sync() {
		std::memcpy(s_slowMain, s_main, A2_PAGE_SIZE);
		std::memcpy(s_slowAux,  s_aux,  A2_PAGE_SIZE);
	}
};
static DhgrSlowPolicy s_slowPolicy;
static SlowDrawEngine<DhgrSlowPolicy> s_slow(s_slowPolicy);

// Every plotted byte funnels through here: write the final page and, when
// recording, append the op for the progressive reveal. `page` is s_main or
// s_aux; the recorded op tags which so the reveal targets the matching slow page.
#ifdef DHGR_WATCH
static int s_dhgrWatchOff = -1, s_dhgrWatchPage = 1, s_dhgrOpTag = -1;
void gmDhgrSetWatch(int off, int page) { s_dhgrWatchOff = off; s_dhgrWatchPage = page; }
#endif
static inline void dhgr_put(uint8_t *page, uint16_t off, uint8_t value) {
#ifdef DHGR_WATCH
	if ((int)off == s_dhgrWatchOff && (page==s_main?1:0)==s_dhgrWatchPage)
		fprintf(stderr, "WATCH op=%d off=%04x page=%s %02x->%02x\n", s_dhgrOpTag,
			off, page==s_main?"main":"aux", page[off]&0x7f, value&0x7f);
#endif
	page[off] = value;
	s_slow.record(off, (uint8_t)(page == s_main ? 1 : 0), value);
}

// ---- drawing tables (loaded at runtime from T5) -------------------------------
// T5 is the boot disk's headerless double-hi-res interpreter file; it loads at
// $4000, so file_offset = RAM_addr - $4000. The four tables sit at fixed offsets.
static uint8_t s_subidx[256];     // $41B2 colour subindex (patched per-image by op7)
static uint8_t s_subidxDefault[256]; // pristine T5 copy, restored at each image's start
static uint8_t s_subtbl[512];     // $421E sub-table (idx -> two pattern rows)
static uint8_t s_pattern[2048];   // $441E pattern rows (8 bytes each)
static uint8_t s_brush[256];      // $4B2E brush bitmaps (8 brushes x 4 quad x 8)
// op3/op5 picture font: 128 chars x 8 rows, sharing the brush table's base
// ($4B2E -> file 0xB2E). The 6502 glyph handler ($0c60) computes the glyph
// pointer as $4B2E + ch*8, so the 8 brush bitmaps (256 bytes) double as the
// control-char glyphs $00..$1f and the printable glyphs follow -- exactly the
// standard-hi-res renderer's layout (graphics_magician.cpp s_fontGlyphs).
static uint8_t s_fontGlyphs[128 * 8];
static bool s_tablesInstalled = false;

bool gmDhgrInstallDrawingTables(const uint8_t *t5, size_t size) {
	// Need everything up to the brush table end ($4B2E + 256 = $4C2E -> off 0xC2E).
	if (size < 0xC2E)
		return false;
	std::memcpy(s_subidx,  t5 + 0x1B2, sizeof(s_subidx));
	std::memcpy(s_subidxDefault, s_subidx, sizeof(s_subidx));
	// Full 512-byte sub-table ($421E..$441E): a colour's subindex can be up to 255,
	// so the builder reads subtbl[2*idx{,+1}] up to index 510/511. Copying only 500
	// left those high entries zero, so high-subindex fills (e.g. Talisman's lamp
	// body, fill colour 60 -> idx 255 -> subtbl[510]=0x0a) resolved to pattern row 0
	// (black) instead of their real grey dither.
	std::memcpy(s_subtbl,  t5 + 0x21E, 512);
	std::memcpy(s_pattern, t5 + 0x41E, 1808);
	std::memcpy(s_brush,   t5 + 0xB2E, sizeof(s_brush));
	// The font occupies 0xB2E..0xF2E (128*8). Copy what fits; leave the tail zero
	// (blank glyphs) rather than failing the critical tables above if T5 ends early.
	std::memset(s_fontGlyphs, 0, sizeof(s_fontGlyphs));
	size_t fontEnd = 0xB2E + sizeof(s_fontGlyphs);
	std::memcpy(s_fontGlyphs, t5 + 0xB2E,
				(fontEnd <= size) ? sizeof(s_fontGlyphs) : (size - 0xB2E));
	s_tablesInstalled = true;
	return true;
}

bool gmDhgrHaveDrawingTables() { return s_tablesInstalled; }

// ---- op6 SET_FILL_COLOR (the $0D1A builder) -----------------------------------
// The current fill pattern: one byte per column (mod 8), on even and odd scanlines.
static uint8_t s_fillPatternEven[8], s_fillPatternOdd[8];
static uint8_t g_brush = 5;

static void set_fill_color(uint8_t color) {
	uint8_t idx = s_subidx[color];
	uint8_t b0 = s_subtbl[(2 * idx) & 0x1ff];
	uint8_t b1 = s_subtbl[(2 * idx + 1) & 0x1ff];
	for (int y = 0; y < 8; y++) {
		s_fillPatternEven[y] = s_pattern[((b0 << 3) + y) & 0x7ff];
		s_fillPatternOdd[y]  = s_pattern[((b1 << 3) + y) & 0x7ff];
	}
}

// ---- line subsystem -----------------------------------------------------------
// Pen position: x doubled to the 560-wide raster, and y.
static uint16_t s_penX;
static uint8_t s_penY;
static uint8_t s_penColor = 7;                 // op2 SET_PEN_COLOR
// The 4-pixel pattern each colour pen draws (see plot_pen).
static const uint8_t PEN_COLOR_PATTERN[8] = {0, 6, 9, 0xf, 0, 3, 0xc, 0};

static void move_pen(uint16_t xd, uint8_t y) { s_penX = xd; s_penY = y; }

// Set or clear one pixel of the 560-wide raster. Each run of 7 pixels is one
// byte, alternating between the aux and main pages.
static void plot_pixel(int x, uint8_t y, bool set) {
	int group = x / 7;
	uint8_t mask = (uint8_t)(1 << (x % 7));
	uint8_t *p = (group & 1) ? s_main : s_aux;
	uint16_t a = CALC(y) + (group >> 1);
	if (a >= A2_PAGE_SIZE) return;
	dhgr_put(p, a, set ? (uint8_t)(p[a] | mask) : (uint8_t)(p[a] & ~mask));
}

// Pens 4 (black) and 7 (white) draw single pixels. Every other pen is a colour,
// drawn as its 4-bit pattern (leftmost pixel = bit 3) across the aligned group
// of four pixels containing x.
static bool pen_is_mono() { return s_penColor == 4 || s_penColor == 7; }
static void plot_pen(int x, uint8_t y) {
	if (pen_is_mono()) {
		plot_pixel(x, y, s_penColor == 7);
		return;
	}
	uint8_t pattern = PEN_COLOR_PATTERN[s_penColor & 7];
	x &= ~3;
	for (int i = 0; i < 4; i++)
		plot_pixel(x + i, y, pattern & (8 >> i));
}

// op10 DRAW_LINE from the pen position to (x2d, y2), leaving the pen there.
//
// A port of the 6502 run-slice line: the longer axis is cut into one run per
// step of the shorter axis, each run major / minor pixels long, with an
// accumulator adding the remainder that decides which runs get an extra pixel.
// Quirks of the original that are kept, as they decide which pixels are lit:
//  - the axis lengths are counted in pixels (|d| + 1), and a tie is y-major;
//  - a colour pen steps x by a whole 4-pixel group, from and to x aligned down
//    to a multiple of 4, and leaves the pen at that aligned end x;
//  - when the accumulator lands exactly on zero, the next run is not lengthened
//    and the accumulator restarts at the remainder.
static void line(uint16_t x2d, uint8_t y2) {
	const int group = pen_is_mono() ? 1 : 4;
	if (group == 4)
		x2d &= ~3;
	int x = s_penX & ~(group - 1);
	int y = s_penY;
	const int xstep = (x2d > x) ? group : -group;
	const int ystep = (y2 < y) ? -1 : 1;

	const int xcount = abs(x2d - x) / group + 1;
	const int ycount = abs(y2 - y) + 1;
	const bool steep = xcount <= ycount;
	const int major = steep ? ycount : xcount;
	const int minor = steep ? xcount : ycount;
	const int q = major / minor, r = major % minor;

	int run = q, acc = r;
	for (int i = 0; i < minor; i++) {
		for (int k = 0; k < run; k++) {
			plot_pen(x, (uint8_t)y);
			if (steep) y += ystep; else x += xstep;
		}
		if (steep) x += xstep; else y += ystep;

		if (acc == 0) {
			acc = r;
			run = q;
		} else {
			run = q + (acc + r >= minor);
			acc = (acc + r) % minor;
		}
	}
	move_pen(x2d, y2);
}

// ---- op12 brush ($105A) -------------------------------------------------------
static const uint8_t DHGR_DOUBLE_BITS[16] = {0x00, 0x03, 0x0c, 0x0f, 0x30, 0x33, 0x3c, 0x3f,
									 0xc0, 0xc3, 0xcc, 0xcf, 0xf0, 0xf3, 0xfc, 0xff};
static void brush_blit_col(int row, int col, uint8_t bits) {
	if (!bits) return;
	uint16_t base = CALC(row);
	const uint8_t *pat = (row & 1) ? s_fillPatternOdd : s_fillPatternEven;
	uint8_t v3 = (uint8_t)((bits | 0x80) & pat[col & 7]);
	uint8_t mask = (uint8_t)(bits ^ 0x7f);
	int a = base + (col >> 1); if (a < 0 || a >= A2_PAGE_SIZE) return;
	uint8_t *pg = (col & 1) ? s_main : s_aux;
	dhgr_put(pg, (uint16_t)a, (uint8_t)((pg[a] & mask) | v3));
}
// Expand one source byte through the DHGR nibble-doubling table and blit it across
// three columns at (row, col0..col0+2). The shared core of the brush and glyph
// blitters; defined with the glyph code below.
static void dhgr_blit_byte(int row, int col, uint8_t b, int bit, bool normal);

static void brush_quadrant(int col0, int row0, int bit, int brushbase) {
	for (int r = 0; r < 8; r++)
		dhgr_blit_byte(row0 + r, col0, s_brush[(brushbase + r) & 0xff], bit, false);
}
static void draw_brush(uint16_t x16, uint8_t y, uint8_t brush) {
	int col = x16 / 7, bit = x16 % 7, bb = brush * 32;
	brush_quadrant(col,     y,     bit, bb + 0);
	brush_quadrant(col + 2, y,     bit, bb + 8);
	brush_quadrant(col,     y + 8, bit, bb + 16);
	brush_quadrant(col + 2, y + 8, bit, bb + 24);
}

// ---- op11 DRAW_CIRCLE ---------------------------------------------------------
//
// Port of the <D> circle (FUN_0aeb + plot8 FUN_0bcb). The centre is the current
// pen position (s_penX, s_penY), which the circle leaves unchanged.
// The x extents run to +-2*radius and x is advanced at DOUBLE resolution (a plot
// between two single x steps) while y advances at single resolution, so the
// outline is round on the 560-wide raster AND gap-free. This matters for fill
// containment: the Talisman title's lamp-knob circle (op DRAW_CIRCLE r=4 @135,90)
// must close, or the following fill=62 PAINT leaks out of the lamp into the sky.
//
// The loop walks i up from 0 while r shrinks from the radius, as a midpoint
// circle does. Each step plots the eight-way symmetric points (+-2i, +-r) and
// (+-2r, +-i), then the same again half a step back, at the odd x in between.
static void circle_point(int x, int y) {
	if (y < 0 || y > 0x9f || x < 0 || x >= 0x230) return;  // FUN_0c35 bounds
	plot_pen(x, (uint8_t)y);
}
// FUN_0bcb: plot (+-ax, +-ay) near the top and bottom, then (+-bx, +-by) near
// the sides, in the original's order.
static void circle_plot8(int cx, int cy, int ax, int ay, int bx, int by) {
	circle_point(cx - ax, cy - ay);
	circle_point(cx + ax, cy - ay);
	circle_point(cx + ax, cy + ay);
	circle_point(cx - ax, cy + ay);
	circle_point(cx + bx, cy - by);
	circle_point(cx - bx, cy - by);
	circle_point(cx - bx, cy + by);
	circle_point(cx + bx, cy + by);
}
static void draw_circle(uint8_t radius) {
	if (radius == 0) return;
	const int cx = s_penX, cy = s_penY;
	int r = radius, d = r >> 1;
	for (int i = 0;; ) {
		circle_plot8(cx, cy, 2 * i, r, 2 * r, i);
		if (r < i)
			break;
		i++;
		// The half step: when r shrinks, the side points sit between the old
		// and new 2r.
		bool shrink = d < i;
		if (shrink) {
			r--;
			d += r - i;
		} else {
			d -= i;
		}
		circle_plot8(cx, cy, 2 * i - 1, r, 2 * r + shrink, i - 1);
	}
}

// ---- op1/op3/op5 text glyphs --------------------------------------------------
//
// Text cursor (op1 SET_TEXT_POS). x is kept in 560-wide DHGR pixels: the 6502
// handler ($09e6 -> $09f9) doubles the operand x exactly like MOVE_TO, then op3/
// op5 advance it by 14 (= 7 source px doubled) per character ($093d ADC #$0e).
static uint16_t s_textX = 0;
static uint8_t  s_textY = 0;

// op3 TEXT_CHAR plot: XOR the 7-bit glyph bits onto the page byte. Because `bits`
// is 7-bit, bit 7 is preserved -- on the white subtitle cloud this paints the
// letters in the contrasting artifact colour, leaving solid text. (FUN_108e
// $1111 branch, taken when the mode flag $0839 == 1.)
static void glyph_xor_col(int row, int col, uint8_t bits) {
	if (!bits) return;
	uint16_t base = CALC(row);
	int a = base + (col >> 1); if (a < 0 || a >= A2_PAGE_SIZE) return;
	uint8_t *pg = (col & 1) ? s_main : s_aux;
	dhgr_put(pg, (uint16_t)a, (uint8_t)(pg[a] ^ bits));
}

// Blit one 7-bit glyph half: shift it right by `bit` (spilling the high bits into
// `e`, the next column), then plot the two resulting bytes into col / col+1. op3
// XORs (glyph_xor_col); op5 -- and the brush blitter -- blend through the fill
// pattern (brush_blit_col). Matches the $0839 flag.
static void glyph_blit_half(int row, int col, uint8_t half, int bit, bool normal) {
	if (!half) return;
	uint8_t A = (uint8_t)(half << 1), e = 0;
	for (int s = bit; s > 0; s--) { uint8_t c = (uint8_t)(A >> 7); A = (uint8_t)(A << 1); e = (uint8_t)((e << 1) | c); }
	if (normal) { glyph_xor_col(row, col, (uint8_t)(A >> 1)); glyph_xor_col(row, col + 1, e); }
	else        { brush_blit_col(row, col, (uint8_t)(A >> 1)); brush_blit_col(row, col + 1, e); }
}

// Expand one source byte into its DHGR nibble-doubled 14-bit pattern -- two 7-bit
// halves (left = cbf, right = cc0 with carry) via DHGR_DOUBLE_BITS (FUN_0c8d) -- and blit
// both across three columns at (row, col..col+2). The shared core of the op12 brush
// blitter (normal=false) and the op3/op5 glyph blitter (FUN_0c60 -> FUN_108e). A
// zero byte expands to two empty halves, i.e. a no-op.
static void dhgr_blit_byte(int row, int col, uint8_t b, int bit, bool normal) {
	if (row < 0 || row > 0xbf) return;
	uint8_t cc0 = DHGR_DOUBLE_BITS[b >> 4], lo = DHGR_DOUBLE_BITS[b & 0xf];
	uint8_t cy = (uint8_t)(lo >> 7); lo = (uint8_t)(lo << 1); cc0 = (uint8_t)((cc0 << 1) | cy); lo >>= 1;
	glyph_blit_half(row, col,     lo,  bit, normal);   // left half (cbf)
	glyph_blit_half(row, col + 1, cc0, bit, normal);   // right half (+carry)
}

// Draw one glyph at DHGR pixel x (0..559) / screen row y: each of the 8 glyph rows
// is expanded and blitted exactly like a brush row. `normal` picks op3's XOR plot
// vs op5's fill-pattern blend.
static void dhgr_draw_glyph(uint16_t x, uint8_t y, uint8_t ch, bool normal) {
	const uint8_t *glyph = &s_fontGlyphs[(ch & 0x7f) * 8];
	int col = x / 7, bit = x % 7;
	for (int r = 0; r < 8; r++)
		dhgr_blit_byte(y + r, col, glyph[r], bit, normal);
}

// ---- op14 flood fill ----------------------------------------------------------

// op15 fill-bounds rectangle, in source byte columns (0..39) and rows (0..0x9f),
// set by op15/1 (full screen) and op15/2 (4 explicit bounds). op14 PAINT clips
// the flood fill to this rectangle, exactly as the standard renderer does
// (ctx->fill_*): without it a room whose floor opens toward the right edge -- the
// Coveted Mirror prison tower -- leaks the fill up and over the whole picture.
static GmFillBounds g_fill;

// The span-fill engine is shared with the standard hi-res renderer
// (gm_span_fill.h); this is the double hi-res page it runs on: 80 columns
// alternating aux/main bytes, patterned from an 8-byte-wide dither selected by
// row parity.
struct DhgrFillPage {
	// Drop a single-pixel span (left == right): the real CM <D> span fill ($0F50)
	// rejects it, and without it a flood leaks through the 1-cell gaps that the
	// doubled DHGR boundaries leave in a drawn outline. This matters whenever the
	// fill bounds are full-screen so only the drawn boundary contains the flood --
	// e.g. the prison-tower floor (RA idx0), whose op15/0 RLE bounds are re-opened
	// to full by a following op15/2, so the floor fill leaks up over the whole
	// picture without this guard. It is merely redundant (not wrong) for scenes
	// like the cupboard door (OE idx0), where the op15/0 RLE bounds persist and
	// already clip the fill; that scene stays byte-exact with the guard in place.
	static const bool kDropSinglePixelSpans = true;

	uint16_t rowAddr;
	bool oddRow;

	void setRow(uint8_t row) { rowAddr = CALC(row); oddRow = (row & 1) != 0; }
	// A malformed op15/2 (unclamped fill-bounds bytes) or an op14 PAINT with an
	// out-of-range row can drive rowAddr + (col >> 1) past A2_PAGE_SIZE. Valid
	// images never exceed 0x1ff7, so these guards only bite in the corrupt regime;
	// they mirror the explicit bounds checks the other dhgr_put callers already do.
	uint8_t read(int col) const {
		uint16_t off = (uint16_t)(rowAddr + (col >> 1));
		if (off >= A2_PAGE_SIZE) return 0;
		return (col & 1 ? s_main : s_aux)[off];
	}
	void write(int col, uint8_t v) {
		uint16_t off = (uint16_t)(rowAddr + (col >> 1));
		if (off >= A2_PAGE_SIZE) return;
		dhgr_put(col & 1 ? s_main : s_aux, off, v);
	}
	uint8_t pattern(int col) const { return (oddRow ? s_fillPatternOdd : s_fillPatternEven)[col & 7]; }
};

static DhgrFillPage s_fillPage;
static GmSpanFill<DhgrFillPage> s_spanFill(s_fillPage);

static void flood_fill(uint16_t x, uint8_t y) {
	// Clip to the op15 fill-bounds rectangle. The fill runs in DHGR columns
	// (0..0x4f); a source byte column B spans DHGR columns 2B and 2B+1, so the
	// byte-column bounds double (left -> 2*left, right -> 2*right+1). Rows are not
	// doubled. Matches the standard renderer, which clips PAINT to ctx->fill_*.
	s_spanFill.fill(x, y, (uint8_t)(2 * g_fill.left), (uint8_t)(2 * g_fill.right + 1),
					g_fill.top, g_fill.bottom);
}

// op15 sub-op 3: fill the clip rectangle with the current fill pattern.
static void fill_rect() {
	for (int row = 0; row <= 0x9f; row++) {
		uint16_t base = CALC(row);
		const uint8_t *pat = (row & 1) ? s_fillPatternOdd : s_fillPatternEven;
		for (int col = 0; col <= 0x4f; col++)
			dhgr_put(col & 1 ? s_main : s_aux, (uint16_t)(base + (col >> 1)), pat[col & 7]);
	}
}

// op15 sub-op 0: RLE-compressed bitmap rectangle (The Coveted Mirror). Mirrors
// the standard-mode drawer in graphics_magician.cpp exactly -- 4 bounds bytes
// (right/left/bottom/top), then raw page bytes streamed column-major
// (top->bottom within a column, then the next column), with a 0x80 byte escaping
// a run (next two bytes are count and value, written count+1 times); it stops
// the moment the bottom-right cell is written. The one difference is the write:
// the 6502 <D> routine ($0a7b) splits each source byte through the nibble-
// doubling table (DHGR_DOUBLE_BITS) into a main + aux byte and stores both at the same
// page offset (logical DHGR columns 2*col / 2*col+1) -- the same expansion the
// brush blitter above performs. Without this, op15/0 used to fall through doing
// nothing AND consuming no operand bytes, so the bounds+RLE stream was misparsed
// as opcodes and the picture collapsed to the white reset page.
static void rle_bitmap_dhgr(const uint8_t **pp, const uint8_t *end) {
	const uint8_t *p = *pp;
	// The real CM op15/0 reads its four bounds bytes through gm_op15_read_bounds
	// (cm_ram_raw.bin @0x0a17), which is the SAME routine op15/2 uses: it stores
	// them as the fill-clip rectangle (left/right/top/bottom). A later PAINT flood
	// is clipped to this rectangle. The cupboard-door overlay (OE idx0) relies on
	// this -- its only op15 is this RLE bitmap (rows 97..135), and the following
	// fill=62 PAINT must not leak below row 135 onto the floor. (Rooms like RA idx2
	// re-open the bounds with an explicit op15/2 after their RLE, so this is safe.)
	if (!g_fill.read(p, end)) { *pp = end; return; }
	gmDecodeRleBitmap(g_fill, p, end, [](int row, int col, uint8_t b) {
		uint16_t off = (uint16_t)(CALC(row) + col);
		if (row <= 0x9f && col < 40 && off < A2_PAGE_SIZE) {
			uint8_t hi = DHGR_DOUBLE_BITS[b >> 4], lo = DHGR_DOUBLE_BITS[b & 0xf];
			uint8_t mainByte = (uint8_t)((hi << 1) | (lo >> 7));
			uint8_t auxByte = (uint8_t)(lo & 0x7f);
			dhgr_put(s_aux, off, auxByte);
			dhgr_put(s_main, off, mainByte);
		}
	});
	*pp = p;
}

// ---- public API ---------------------------------------------------------------
// ---- The Coveted Mirror persistent right-hand panel ---------------------------
// Double-hi-res analogue of the standard-hi-res panel (graphics_magician.cpp).
// The panel (logo + hourglass frame + static bottom mound) is drawn once into the
// pages by the caller, captured here by byte-column, and re-composited on top of
// every in-game picture; the draining top-bulb grains are stamped per turn. The
// source column indices match the standard page: the DHGR renderer doubles each
// Apple column into the matching aux+main byte (logical cols 2c / 2c+1), so the
// panel occupies the same byte columns 24..39 in both pages as it does standard.
static CMPanelStore<2> s_cmPanel;     // planes: 0 = aux, 1 = main

// The live and slow plane pairs, in CMPanelStore's plane order.
#define DHGR_LIVE_PLANES uint8_t *const live[2] = { s_aux, s_main }
#define DHGR_SLOW_PLANES uint8_t *const slow[2] = { s_slowAux, s_slowMain }

void gmDhgrCaptureCMPanel(int col0, int col1) {
	DHGR_LIVE_PLANES;
	s_cmPanel.capture(live, col0, col1);
}

bool gmDhgrCMPanelValid() { return s_cmPanel.valid(); }

void gmDhgrBeginCMPanelRebuild() {
	DHGR_LIVE_PLANES;
	DHGR_SLOW_PLANES;
	s_cmPanel.beginRebuild(live, slow, s_slow.recording());
	s_slow.setRecording(false);
	std::memset(s_main, 0x00, A2_PAGE_SIZE);   // black panel background
	std::memset(s_aux,  0x00, A2_PAGE_SIZE);
}

void gmDhgrEndCMPanelRebuild() {
	DHGR_LIVE_PLANES;
	DHGR_SLOW_PLANES;
	s_slow.setRecording(s_cmPanel.endRebuild(live, slow));
}

// Panel-column helpers shared by the overlay / band-restore / grain-mirror paths.
// cm_panel_to_pages copies the saved static panel onto BOTH the final and the slow
// pages (rows y0..y1); cm_panel_final_to_slow mirrors the final pages' panel columns
// (e.g. with freshly stamped grains) onto the slow pages only.
static void cm_panel_to_pages(int y0, int y1) {
	DHGR_LIVE_PLANES;
	DHGR_SLOW_PLANES;
	s_cmPanel.toPages(live, slow, y0, y1);
}
static void cm_panel_final_to_slow(int y0, int y1) {
	DHGR_LIVE_PLANES;
	DHGR_SLOW_PLANES;
	s_cmPanel.finalToSlow(live, slow, y0, y1);
}

// Copy the saved panel byte-columns onto both the final and the
// progressively-revealed pages, so the panel is present immediately even while a
// room's slow-draw reveal is still running (mirrors the standard renderer, which
// writes the panel to s_screenmem and s_slowScreen alike).
void gmDhgrOverlayCMPanel() {
	if (!s_cmPanel.valid())
		return;
	cm_panel_to_pages(0, DHGR_HEIGHT - 1);
}

// Stamp the resting pile of `sand` white grains on top of the panel, at the same
// per-grain positions the standard renderer uses (gmCMHourglassGrainPos), the
// Apple x doubled onto the 560-wide pages. Call after gmDhgrOverlayCMPanel().
void gmDhgrDrawCMHourglass(int sand) {
	if (!s_cmPanel.valid())
		return;
	if (sand < 0)
		sand = 0;
	// Arm the per-turn fall the same way the standard renderer does (shared state),
	// so gmCMHourglassConsumeFallArmed() reports a single-grain drop in DHGR too.
	gmCMHourglassArm(sand);
	// White grains use the same SET_FILL_COLOR 0x34 the original paints them with.
	// Stamp straight onto the final pages with op recording suppressed, so the
	// grains are not deferred to the slow-draw reveal.
	bool savedRecord = s_slow.recording();
	s_slow.setRecording(false);
	set_fill_color(0x34);
	cmForEachPileGrain(sand, [](uint16_t x, uint8_t y) {
		draw_brush((uint16_t)(x << 1), y, 0);
	});
	s_slow.setRecording(savedRecord);
	// Mirror the panel columns (now including the freshly stamped grains) onto the
	// slow pages, so they are visible during a reveal too.
	cm_panel_final_to_slow(0, DHGR_HEIGHT - 1);
}

// --- The Coveted Mirror per-turn grain fall (double hi-res) -------------------
// Double-hi-res counterpart of the standard fall (gmCMHourglassFall* in
// graphics_magician.cpp). The resting pile is already stamped at the post-drop
// level by gmDhgrDrawCMHourglass(); these paint the single freed grain travelling
// down the neck. The frame sequence and geometry are CMGrainFall's (cm_panel.h),
// shared with the standard renderer; the grain x is doubled onto the 560-wide
// pages (draw_brush(x<<1,...)), the same way gmDhgrDrawCMHourglass stamps the pile.
static CMGrainFall s_dhgrCmFall;

// Restore the saved static panel (no grains) for rows [y0,y1] onto both the final
// and slow pages, wiping the previous frame's falling grain before the next.
static void cmDhgrOverlayPanelBand(int y0, int y1) {
	if (!s_cmPanel.valid())
		return;
	if (y0 < 0)
		y0 = 0;
	if (y1 > DHGR_HEIGHT - 1)
		y1 = DHGR_HEIGHT - 1;
	cm_panel_to_pages(y0, y1);
}

void gmDhgrCMHourglassFallBegin() { s_dhgrCmFall.begin(); }
bool gmDhgrCMHourglassFallActive() { return s_dhgrCmFall.active(); }

bool gmDhgrCMHourglassFallStep(int *y0, int *y1) {
	return s_dhgrCmFall.step(y0, y1, cmDhgrOverlayPanelBand, [](uint16_t x, uint8_t y) {
		bool savedRecord = s_slow.recording();
		s_slow.setRecording(false);
		set_fill_color(0x34);                 // white, as the pile grains
		draw_brush((uint16_t)(x << 1), y, 0);
		s_slow.setRecording(savedRecord);
		// Mirror the band's panel columns (with the grain) onto the slow pages so a
		// blit reading either page shows it.
		cm_panel_final_to_slow(CMGrainFall::kBandTop, CMGrainFall::kBandBot);
	});
}

void gmDhgrCMHourglassFallAbort() {
	s_dhgrCmFall.abort(cmDhgrOverlayPanelBand);
}

void gmDhgrResetScreen(bool white) {
	uint8_t bg = white ? 0x7f : 0x00;
	std::memset(s_main, bg, A2_PAGE_SIZE);
	std::memset(s_aux,  bg, A2_PAGE_SIZE);
	// A reset starts a fresh page: the background appears instantly on the
	// visible pages too, and any pending reveal is dropped.
	std::memset(s_slowMain, bg, A2_PAGE_SIZE);
	std::memset(s_slowAux,  bg, A2_PAGE_SIZE);
	s_slow.clear();
}

// ---- Opcode interpreter -------------------------------------------------------
// The loop and operand decode are the shared gmRunStream (gm_stream.h); this is
// the double hi-res side of it.  Pen, text cursor, brush and fill bounds are the
// file statics above, so a renderer object carries only the stream base for the
// DHGR_WATCH trace.
struct DhgrRenderer {
	const uint8_t *base;

	// Source x is doubled onto the 560-wide raster for every operand.
	uint16_t scaleX(uint16_t x) const { return (uint16_t)(x << 1); }
	int penX() const { return s_penX; }
	int penY() const { return s_penY; }

	void begin() {
		// Restore the T5 default colour subindex FIRST, so the per-image init below
		// (and the image's own op7 SET_PALETTE_SUBINDEX commands) start from a clean
		// baseline. op7 overrides are per-image: the same streams render self-coloured
		// under std-hires, where op7 is a no-op. Order matters -- set_fill_color(0x4d)
		// reads s_subidx to build the default fill pattern used by fill_rect (the
		// full-screen background many rooms paint), so it must see the defaults, not a
		// previous room's patches (which turned e.g. the hill pink after the bazaar).
		std::memcpy(s_subidx, s_subidxDefault, sizeof(s_subidx));
		s_penColor = 4; move_pen(140 * 2, 96); // per-image draw init (pen colour 4, centre)
		set_fill_color(0x4d);             // default fill colour
		// Default PAINT clip = full screen, until an op15/1 or op15/2 narrows it
		// (matches the standard renderer's ctx->fill_* defaults).
		g_fill.reset(0x9f);
		g_brush = 5;                      // default shape; reset per image (matches the
										  // std-hires init ctx.BRUSH=5). Without this an
										  // item overlay inherits the room's last op4
										  // SET_SHAPE, drawing its brushes with the wrong
										  // shape so the fills find no seed (e.g. the
										  // OO-Topos prison-cell bottle/food went the
										  // colour of the floor/table beneath them).
	}
	// When not animating, keep the visible pages in step with the final pages so a
	// later slow-drawn overlay (e.g. a moved object) composes onto the right
	// background instead of a stale/blank page -- exactly what the standard hi-res
	// renderer does at the end of gmDrawImage.
	void end() { s_slow.syncIfNotRecording(); }

	void beforeOp(const uint8_t *p, const uint8_t *, uint8_t, uint8_t) {
#ifdef DHGR_WATCH
		s_dhgrOpTag = (int)(p - 1 - base);
#else
		(void)p;
#endif
	}
	void afterOp(uint8_t, uint8_t) {}

	void setTextPos(uint16_t x, uint8_t y) { s_textX = x; s_textY = y; }
	void drawText(uint8_t ch, bool normal) { dhgr_draw_glyph(s_textX, s_textY, ch, normal); }
	void advanceText() { s_textX += 14; }
	void setPen(uint8_t param) { s_penColor = param; }
	void setShape(uint8_t param) { g_brush = param; }
	void setFill(uint8_t b) { set_fill_color(b); }
	// SET_PALETTE_SUBINDEX
	bool op7(const uint8_t *&p, const uint8_t *e) {
		if (!gmHave(p, e, 2))
			return true;
		const uint8_t i = *p++;
		const uint8_t v = *p++;
		s_subidx[i] = v;
		return false;
	}
	void moveTo(uint16_t x, uint8_t y) { move_pen(x, y); }
	void lineTo(uint16_t x, uint8_t y) { line(x, y); }
	void circle(uint8_t radius) { draw_circle(radius); }
	void drawBrush(uint16_t x, uint8_t y) { draw_brush(x, y, g_brush); }
	void delay(uint8_t units) { s_slow.recordDelay(units); }
	void paint(uint16_t x, uint8_t y) { flood_fill(x, y); }

	bool opReset(uint8_t param, const uint8_t *&p, const uint8_t *e) {
		switch (param) {
		case 0: // RLE bitmap (Coveted Mirror)
			rle_bitmap_dhgr(&p, e);
			break;
		case 1: // bounds = full screen
			g_fill.reset(0x9f);
			break;
		case 2: // read 4 bounds bytes
			if (!g_fill.read(p, e))
				return true;
			break;
		default: // param 3: fill the rectangle with the current pattern
			fill_rect();
			break;
		}
		return false;
	}
};

void gmDhgrDrawImage(const uint8_t *data, size_t size) {
	DhgrRenderer r;
	r.base = data;
	gmRunStream(r, data, size);
}

static void blit_pages(const uint8_t *aux, const uint8_t *main, uint32_t *out, int w, int h) {
	for (int row = 0; row < h; row++) {
		unsigned base = CALC(row);
		uint16_t words[40];
		uint8_t colors560[560];
		gm_compute_dhgr_row_words(aux + base, main + base, words);
		gm_render_dhgr_line_colors(words, colors560);
		// 560 native DHGR columns sampled down to the surface width w.
		for (int xx = 0; xx < w; xx++) {
			int sx = (w >= 560) ? xx : (xx * 560) / w;
			if (sx > 559) sx = 559;
			uint32_t rgb = gm_apple2_palette[colors560[sx] & 0x0f];
			out[row * w + xx] = (rgb << 8) | 0xff;
		}
	}
}

void gmDhgrBlitToSurface(uint32_t *out, int w, int h) {
	blit_pages(s_aux, s_main, out, w, h);
}

// ---- Slow-draw control (host-driven) ------------------------------------------
// The machinery lives in the shared SlowDrawEngine (slow_draw_page.h); the DHGR
// policy's apply hook is dhgr_apply_slow below.

// Apply one recorded byte to the matching slow page, skipping the CM panel.
// Returns false (suppressing dirty marking) when the byte is skipped.
//
// The Coveted Mirror's panel occupies columns [col0,col1] and is composited on
// top of every finished picture (gmDhgrOverlayCMPanel writes it to the slow pages
// directly). A room drawn with a full-width op15/0 RLE bitmap, however, records
// writes across those columns too; replaying them during the reveal would wipe
// the panel off the slow pages. Skip panel-column ops so the overlaid panel
// survives the reveal (it is already correct on the final pages).
static bool dhgr_apply_slow(const SlowWriteOp &op) {
	if (s_cmPanel.skipsReveal(op.offset))
		return false;
	(op.page ? s_slowMain : s_slowAux)[op.offset] = op.value;
	return true;
}

void gmDhgrSetSlowDraw(bool on) { s_slow.setRecording(on); }
bool gmDhgrSlowDrawActive() { return s_slow.active(); }
bool gmDhgrAdvanceSlowDraw(int budget) { return s_slow.advance(budget); }
void gmDhgrFinishSlowDraw() { s_slow.finish(); }
bool gmDhgrSlowConsumeDirty(int *y0, int *y1) { return s_slow.consumeDirty(y0, y1); }

void gmDhgrBlitSlowToSurface(uint32_t *out, int w, int h) {
	blit_pages(s_slowAux, s_slowMain, out, w, h);
}

const uint8_t *gmDhgrMainPtr() { return s_main; }
const uint8_t *gmDhgrAuxPtr() { return s_aux; }

} // namespace Comprehend
} // namespace Glk
