// Apple II double hi-res ("<D>" mode) renderer for the V2 games (Talisman, OO-Topos, Transylvania
// v2, The Coveted Mirror). It takes the same vector stream as graphics_magician.cpp but doubles X
// and plots into a 560-wide aux/main framebuffer (7 px/byte, column even -> aux, odd -> main, byte
// index col>>1). The pages convert to RGBA with the composite-DHGR kernel in gm_artifact.cpp.
// The primitives were derived from the 6502 double hi-res interpreter. The tables are loaded from
// the boot disk's "T5" file.
//
// op7 (0x7x) is not a no-op here: it is a per-image SET-PALETTE-SUBINDEX,
// s_subidx[operand1] = operand2, so that the following op6 SET_FILL_COLOR resolves to the image's
// intended pattern. op6 builds it as:
//   idx = subindex[colour];  b0,b1 = subtable[2*idx{,+1}];
//   even[y] = pattern[(b0<<3)+y];  odd[y] = pattern[(b1<<3)+y].

#include "graphics_magician_dhgr.h"
#include "graphics_magician.h"
#include "gm_artifact.h"
#include "gm_hires.h"
#include "gm_stream.h"         // shared opcode dispatcher
#include "gm_blit.h"           // shared 560-colour row sampling
#include "slow_draw_page.h"     // shared progressive-reveal helper
#include "gm_span_fill.h"       // shared op14 flood fill
#include "graphics_magician_vector.h"  // shared circle stepper
#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace Glk {
namespace Comprehend {

#define A2_PAGE_SIZE   GM_HIRES_PAGE_SIZE
#define DHGR_HEIGHT    192
#define DHGR_PIC_ROWS  160   // picture region (text rows 0xa0..0xbf excluded)
#define DHGR_COLS      80    // 560 px / 7

static uint8_t s_main[A2_PAGE_SIZE];
static uint8_t s_aux[A2_PAGE_SIZE];

#define CALC(y) gm_row_address(y)

// Slow draw, as in graphics_magician.cpp. The reveal state machine is the shared SlowDrawEngine;
// the policy below supplies the dual main/aux pages, a page-aware run test and the Coveted Mirror
// panel-column exclusion. op13 DELAY operands are recorded as pause markers.
static uint8_t s_slowMain[A2_PAGE_SIZE]; // progressively-revealed pages
static uint8_t s_slowAux[A2_PAGE_SIZE];

// Apply one recorded byte to the matching slow page; defined after the CM panel state it consults.
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

// Drawing tables, loaded at runtime from T5, the boot disk's headerless double hi-res interpreter.
// It loads at $4000, so file_offset = RAM_addr - $4000. The tables sit at fixed offsets.
static uint8_t s_subidx[256];     // $41B2 colour subindex (patched per-image by op7)
static uint8_t s_subidxDefault[256]; // pristine T5 copy, restored at each image's start
static uint8_t s_subtbl[512];     // $421E sub-table (idx -> two pattern rows)
static uint8_t s_pattern[2048];   // $441E pattern rows (8 bytes each)
static uint8_t s_brush[256];      // $4B2E brush bitmaps (8 brushes x 4 quad x 8)
// op3/op5 picture font: 128 chars x 8 rows, sharing the brush table's base, so the 8 brush
// bitmaps double as the control-char glyphs $00..$1f, as in standard hi-res.
static uint8_t s_fontGlyphs[128 * 8];
static bool s_tablesInstalled = false;

bool gmDhgrInstallDrawingTables(const uint8_t *t5, size_t size) {
	// Need everything up to the brush table end ($4B2E + 256 = $4C2E -> off 0xC2E).
	if (size < 0xC2E)
		return false;
	std::memcpy(s_subidx,  t5 + 0x1B2, sizeof(s_subidx));
	std::memcpy(s_subidxDefault, s_subidx, sizeof(s_subidx));
	// Full 512-byte sub-table: a colour's subindex can be up to 255, so the builder reads
	// subtbl[2*idx{,+1}] up to index 511 (e.g. Talisman's lamp body, fill colour 60).
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

// op6 SET_FILL_COLOR.
// The current fill pattern: one byte per column (mod 8), on even and odd scanlines.
static uint8_t s_fillPatternEven[8], s_fillPatternOdd[8];
static uint8_t s_fillColor = 0x4d;             // the colour the pattern was built from
static uint8_t g_brush = 5;

static void set_fill_color(uint8_t color) {
	s_fillColor = color;
	uint8_t idx = s_subidx[color];
	uint8_t b0 = s_subtbl[(2 * idx) & 0x1ff];
	uint8_t b1 = s_subtbl[(2 * idx + 1) & 0x1ff];
	for (int y = 0; y < 8; y++) {
		s_fillPatternEven[y] = s_pattern[((b0 << 3) + y) & 0x7ff];
		s_fillPatternOdd[y]  = s_pattern[((b1 << 3) + y) & 0x7ff];
	}
}

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

// op10 DRAW_LINE from the pen position to (x2d, y2), leaving the pen there. The longer axis is
// cut into one run per step of the shorter axis, with an accumulator deciding which runs get an
// extra pixel. Quirks of the original, kept as they decide which pixels are lit: axis lengths
// count pixels (|d| + 1) and a tie is y-major; a colour pen steps x by a whole 4-pixel group, from
// and to x aligned down to a multiple of 4, and leaves the pen at that aligned x; when the
// accumulator lands exactly on zero, the next run is not lengthened and it restarts at the
// remainder.
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

// op12 brush.
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

// op11 DRAW_CIRCLE, centred on the pen position, which it leaves unchanged. x extends to
// +-2*radius and advances at double resolution (a half step plots the odd x between full steps),
// so the outline is round on the 560-wide raster and gap-free. Fill containment depends on this:
// the Talisman title's lamp-knob circle must close or the following PAINT leaks into the sky.
// The extra full step at the stepper's final (i, r) only repaints pixels, but changing it would
// change the slow-draw reveal sequence.
static void circle_point(int x, int y) {
	if (y < 0 || y > 0x9f || x < 0 || x >= 0x230) return;  // the original's bounds
	plot_pen(x, (uint8_t)y);
}
static void draw_circle(uint8_t radius) {
	if (radius == 0) return;
	const int cx = s_penX, cy = s_penY;
	auto full = [&](int i, int r) {
		gmvCirclePlot8(cx, cy, 2 * i, r, 2 * r, i, circle_point);
	};
	const GmvCircleEnd e = gmvAppleCircleSteps(radius, full,
		[&](int i, int r, bool shrink) {
			// When r shrinks, the side points sit between the old and new 2r.
			gmvCirclePlot8(cx, cy, 2 * i - 1, r, 2 * r + shrink, i - 1, circle_point);
		});
	full(e.i, e.r);
}

// Text cursor (op1 SET_TEXT_POS). x is in 560-wide pixels: op1 doubles the operand like MOVE_TO,
// and op3/op5 advance it by 14 (7 source pixels doubled) per character.
static uint16_t s_textX = 0;
static uint8_t  s_textY = 0;

// op3 TEXT_CHAR plot: XOR the 7-bit glyph bits onto the page byte. Bit 7 is preserved; on the
// white subtitle cloud this paints the letters in the contrasting artifact colour.
static void glyph_xor_col(int row, int col, uint8_t bits) {
	if (!bits) return;
	uint16_t base = CALC(row);
	int a = base + (col >> 1); if (a < 0 || a >= A2_PAGE_SIZE) return;
	uint8_t *pg = (col & 1) ? s_main : s_aux;
	dhgr_put(pg, (uint16_t)a, (uint8_t)(pg[a] ^ bits));
}

// Blit one 7-bit glyph half: shift it right by `bit` (spilling the high bits into `e`, the next
// column), then plot the two resulting bytes into col / col+1. op3 XORs (glyph_xor_col); op5 and
// the brush blitter blend through the fill pattern (brush_blit_col).
static void glyph_blit_half(int row, int col, uint8_t half, int bit, bool normal) {
	if (!half) return;
	uint8_t A = (uint8_t)(half << 1), e = 0;
	for (int s = bit; s > 0; s--) { uint8_t c = (uint8_t)(A >> 7); A = (uint8_t)(A << 1); e = (uint8_t)((e << 1) | c); }
	if (normal) { glyph_xor_col(row, col, (uint8_t)(A >> 1)); glyph_xor_col(row, col + 1, e); }
	else        { brush_blit_col(row, col, (uint8_t)(A >> 1)); brush_blit_col(row, col + 1, e); }
}

// Expand one source byte into two nibble-doubled 7-bit halves via DHGR_DOUBLE_BITS and blit both
// across three columns at (row, col..col+2). The shared core of the op12 brush blitter
// (normal=false) and the op3/op5 glyph blitter. A zero byte is a no-op.
static void dhgr_blit_byte(int row, int col, uint8_t b, int bit, bool normal) {
	if (row < 0 || row > 0xbf) return;
	uint8_t cc0 = DHGR_DOUBLE_BITS[b >> 4], lo = DHGR_DOUBLE_BITS[b & 0xf];
	uint8_t cy = (uint8_t)(lo >> 7); lo = (uint8_t)(lo << 1); cc0 = (uint8_t)((cc0 << 1) | cy); lo >>= 1;
	glyph_blit_half(row, col,     lo,  bit, normal);   // left half
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

// op15 fill-bounds rectangle, in source byte columns (0..39) and rows (0..0x9f). op14 PAINT clips
// to it: otherwise a room whose floor opens toward the right edge (the Coveted Mirror prison
// tower) leaks the fill over the whole picture.
static GmFillBounds g_fill;

// The span-fill engine is shared with the standard hi-res and CGA renderers
// (gm_span_fill.h); this is the double hi-res page it runs on: 80 columns
// alternating aux/main bytes, patterned from an 8-byte-wide dither selected by
// row parity.
struct DhgrFillPage {
	// Drop a single-pixel span (left == right), as the Coveted Mirror <D> span fill does. Otherwise
	// a flood leaks through the 1-cell gaps that doubled boundaries leave in an outline when the
	// fill bounds are full-screen (e.g. the prison-tower floor, RA idx0).
	static const bool kDropSinglePixelSpans = true;

	uint16_t rowAddr;
	bool oddRow;

	void setRow(uint8_t row) { rowAddr = CALC(row); oddRow = (row & 1) != 0; }
	// A malformed op15/2 or an op14 PAINT with an out-of-range row can drive
	// rowAddr + (col >> 1) past A2_PAGE_SIZE. Valid images never exceed 0x1ff7.
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
static GmHiresSpanFill<DhgrFillPage> s_spanFill(s_fillPage);

static void flood_fill(uint16_t x, uint8_t y) {
	// Clip to the op15 rectangle. A source byte column B spans DHGR columns 2B and 2B+1, so the
	// column bounds double (left -> 2*left, right -> 2*right+1); rows do not.
	s_spanFill.fill(x, y, (uint8_t)(2 * g_fill.left), (uint8_t)(2 * g_fill.right + 1),
					g_fill.top, g_fill.bottom);
}

// op15 sub-op 3: fill the op15 bounds with the current fill pattern: rows top..bottom,
// DHGR columns 2*left..2*right+1 (Talisman $0A4E). Bounds default to the whole picture,
// and Talisman's OA 2 and OA 4 use small ones. The clamps only guard malformed bytes.
static void fill_rect() {
	const int bottom = std::min<int>(g_fill.bottom, 0x9f), right = std::min<int>(2 * g_fill.right + 1, 0x4f);
	for (int row = g_fill.top; row <= bottom; row++) {
		uint16_t base = CALC(row);
		const uint8_t *pat = (row & 1) ? s_fillPatternOdd : s_fillPatternEven;
		for (int col = 2 * g_fill.left; col <= right; col++)
			dhgr_put(col & 1 ? s_main : s_aux, (uint16_t)(base + (col >> 1)), pat[col & 7]);
	}
}

// op15 sub-op 0: RLE-compressed bitmap rectangle (The Coveted Mirror), decoded by
// gmDecodeRleBitmap. Each source byte is split through the nibble-doubling table into a main + aux
// byte stored at the same page offset (logical columns 2*col / 2*col+1), the same expansion the
// brush blitter performs.
static void rle_bitmap_dhgr(const uint8_t **pp, const uint8_t *end) {
	const uint8_t *p = *pp;
	// The four bounds bytes become the fill-clip rectangle, as with op15/2, and a later PAINT is
	// clipped to it. The cupboard-door overlay (OE idx0) relies on this: its only op15 is this
	// bitmap (rows 97..135), and the following PAINT must not leak onto the floor. Rooms like
	// RA idx2 re-open the bounds with an explicit op15/2.
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

// The Coveted Mirror's right-hand panel, as in graphics_magician.cpp: captured by byte column and
// re-composited on every in-game picture, with the grains stamped per turn. Each Apple column
// doubles into the matching aux+main byte, so the panel is byte columns 24..39 on both pages.
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

// Panel-column helpers shared by the overlay, band-restore and grain-mirror paths.
// cm_panel_to_pages copies the saved static panel onto both the final and the slow pages (rows
// y0..y1); cm_panel_final_to_slow mirrors the final pages' panel columns (e.g. with freshly
// stamped grains) onto the slow pages only.
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

// Copy the saved panel byte-columns onto both the final and the slow pages, so the panel is
// present even while a room's reveal is still running.
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

// The Coveted Mirror per-turn grain fall. The resting pile is already stamped at the post-drop
// level by gmDhgrDrawCMHourglass(); these paint the freed grain travelling down the neck. The frame
// sequence and geometry are CMGrainFall's (cm_panel.h); the grain x is doubled.
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

// The loop and operand decode are the shared gmRunStream (gm_stream.h). Pen, text cursor, brush and
// fill bounds are the file statics above.
struct DhgrRenderer {
	const uint8_t *base;

	// Source x is doubled onto the 560-wide raster for every operand.
	uint16_t scaleX(uint16_t x) const { return (uint16_t)(x << 1); }
	int penX() const { return s_penX; }
	int penY() const { return s_penY; }

	void begin() {
		// Restore the T5 default colour subindex table first: op7 overrides are per-image, and
		// set_fill_color(0x4d) below reads s_subidx to build the default fill pattern, so it
		// must not see a previous room's patches.
		std::memcpy(s_subidx, s_subidxDefault, sizeof(s_subidx));
		s_penColor = 4; move_pen(140 * 2, 96); // per-image draw init (pen colour 4, centre)
		set_fill_color(0x4d);             // default fill colour
		// Default PAINT clip = full screen, until an op15/1 or op15/2 narrows it.
		g_fill.reset(0x9f);
		// Default shape, reset per image: an item overlay that inherits the room's last op4
		// SET_SHAPE draws its brushes with the wrong shape, so its fills find no seed (e.g. the
		// OO-Topos prison-cell bottle and food).
		g_brush = 5;
	}
	// When not animating, keep the visible pages in step with the final pages so a later
	// slow-drawn overlay (e.g. a moved object) composes onto the right background.
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
	// SET_PALETTE_SUBINDEX. Patching the colour the current fill pattern was built from rebuilds
	// that pattern at once, so a picture can select a colour once and repaint it between PAINTs
	// (Talisman's Lands Beyond rooms do). Patching any other colour only takes effect at its next
	// SET_FILL_COLOR.
	bool op7(const uint8_t *&p, const uint8_t *e) {
		if (!gmHave(p, e, 2))
			return true;
		const uint8_t i = *p++;
		const uint8_t v = *p++;
		s_subidx[i] = v;
		if (i == s_fillColor)
			set_fill_color(i);
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
		default: // param 3: fill the picture area with the current pattern
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
		// 560 native DHGR columns sampled down to the surface width w (1:1 when wider).
		gmSampleRow560(colors560, out + row * w, w, true);
	}
}

void gmDhgrBlitToSurface(uint32_t *out, int w, int h) {
	blit_pages(s_aux, s_main, out, w, h);
}

// Apply one recorded byte to the matching slow page. Returns false (no dirty marking) for CM panel
// columns: a room drawn with a full-width op15/0 RLE bitmap records writes across them, and
// replaying those would wipe the overlaid panel off the slow pages.
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
