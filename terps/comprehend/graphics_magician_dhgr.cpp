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
 * Talisman's prison cell, RA image 4 -- see talisman-dhgr-wip/TODO_dither_phase.md
 * and the regression in test/). The colour, sub-table, pattern and brush tables
 * are loaded at runtime from the boot disk's "T5" file (gmDhgrInstallDrawingTables);
 * NOTHING is baked. Per-image colour overrides arrive as op7 commands (see below).
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
#include "slow_draw_page.h"     // shared progressive-reveal helper
#include <cstring>

namespace Glk {
namespace Comprehend {

// ---- framebuffer --------------------------------------------------------------
#define A2_PAGE_SIZE   0x2000
#define DHGR_HEIGHT    192
#define DHGR_PIC_ROWS  160   // picture region (text rows 0xa0..0xbf excluded)
#define DHGR_COLS      80    // 560 px / 7

static uint8_t s_main[A2_PAGE_SIZE];
static uint8_t s_aux[A2_PAGE_SIZE];

#define CALC(y) ((((y / 8) & 0x07) << 7) + (((y / 8) & 0x18) * 5) + ((y & 7) << 10))

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

// offset -> screen row (0..191), or 0xff for the inter-row gaps in the page
// address space; offset -> byte column (0..39), or 0xff in the gaps. Built once;
// lets a reveal map a byte write back to its row (dirty band) and column (so the
// CM panel columns can be excluded from the reveal -- see dhgr_apply_slow).
static uint8_t s_rowOfOffset[A2_PAGE_SIZE];
static uint8_t s_colOfOffset[A2_PAGE_SIZE];
static bool s_rowTableInit = false;

static void init_offset_tables() {
	if (s_rowTableInit)
		return;
	std::memset(s_rowOfOffset, 0xff, sizeof(s_rowOfOffset));
	std::memset(s_colOfOffset, 0xff, sizeof(s_colOfOffset));
	for (int row = 0; row < 192; row++) {
		uint16_t base = CALC(row);
		for (int col = 0; col < 40; col++) {
			s_rowOfOffset[base + col] = (uint8_t)row;
			s_colOfOffset[base + col] = (uint8_t)col;
		}
	}
	s_rowTableInit = true;
}

static int dhgr_row_of(uint16_t offset) {
	init_offset_tables();
	uint8_t r = s_rowOfOffset[offset];
	return (r == 0xff) ? -1 : (int)r;        // -1 = inter-row gap, skip
}

// Apply one recorded byte to the matching slow page; defined after the CM-panel
// state it consults (forward-declared here for the policy).
static bool dhgr_apply_slow(const SlowWriteOp &op);

// DHGR page: a main/aux pair (op.page picks which), so the run test also keys on
// page, and panel columns are skipped during the reveal.
struct DhgrSlowPolicy {
	bool apply(const SlowWriteOp &op) { return dhgr_apply_slow(op); }
	int rowOf(const SlowWriteOp &op) const { return dhgr_row_of(op.offset); }
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
static uint8_t curEven[8], curOdd[8];
static uint8_t g_brush = 5;

static void set_fill_color(uint8_t color) {
	uint8_t idx = s_subidx[color];
	uint8_t b0 = s_subtbl[(2 * idx) & 0x1ff];
	uint8_t b1 = s_subtbl[(2 * idx + 1) & 0x1ff];
	for (int y = 0; y < 8; y++) {
		curEven[y] = s_pattern[((b0 << 3) + y) & 0x7ff];
		curOdd[y]  = s_pattern[((b1 << 3) + y) & 0x7ff];
	}
}

// ---- line subsystem (verbatim from the validated dhgr_line_port) --------------
static uint8_t Z0b, Z0c, ZE0, ZE1, ZE2;
static uint16_t rowbase;
static uint8_t Z1c, Z1d, Z1b, Z1e, Z1446, Z18, Z19, Z1a;
static uint8_t Z1490, Z148e, Z148f, Z1491, Z1492, Z1493, Z1494, Z1495, Z1496, Z1497, Z1445;
static uint8_t Z1447 = 7;
static const uint8_t setmask[8] = {1, 2, 4, 8, 16, 32, 64, 0xa6};
static const uint8_t clrmask[8] = {0xfe, 0xfd, 0xfb, 0xf7, 0xef, 0xdf, 0xbf, 0x84};
static const uint8_t ystep[8] = {1, 0xff, 1, 0xff, 1, 0xff, 1, 0xff};
static const uint8_t xstep[8] = {0xff, 0xff, 1, 1, 0xfc, 0xfc, 4, 4};
static const uint8_t color4bit[8] = {0, 6, 9, 0xf, 0, 3, 0xc, 0};
static const uint8_t off13bc[4] = {0, 7, 14, 21};
static int hung = 0, usecolor = 0;

static void rowaddr() { rowbase = CALC(ZE2); }
static void posn(uint16_t xd, uint8_t y) {
	ZE0 = xd & 0xff; ZE1 = xd >> 8; ZE2 = y; rowaddr();
	Z0b = ZE0; Z0c = ZE1;
	for (int c = 8; c; c--) { uint8_t cy = Z0b >> 7; Z0b = Z0b << 1; Z0c = (Z0c << 1) | cy; if (Z0c > 6) { Z0c -= 7; Z0b++; } }
}
static void plot() {
	uint8_t col = Z0b >> 1; uint8_t *p = (Z0b & 1) ? s_main : s_aux; uint16_t a = rowbase + col;
	if (a >= A2_PAGE_SIZE) return;
	dhgr_put(p, a, Z1446 ? (uint8_t)(p[a] | setmask[Z0c]) : (uint8_t)(p[a] & clrmask[Z0c]));
}
static void plotColor() {
	uint8_t sb = Z0b, sc = Z0c;
	Z0b &= 0xfc;
	uint8_t X = sb & 3;
	uint8_t Yv = (uint8_t)((off13bc[X] + sc) & 0xfc);
	uint8_t adv = Yv >> 3;
	Z0b = adv | Z0b;
	Z0c = (uint8_t)(Yv - off13bc[adv & 3]);
	uint8_t a = color4bit[Z1447 & 7] << 4;
	for (int i = 0; i < 4; i++) {
		Z1446 = (a & 0x80) ? 1 : 0; a = (uint8_t)(a << 1);
		plot();
		Z0c++; if (Z0c >= 7) { Z0c = 0; Z0b++; }
	}
	Z0b = sb; Z0c = sc;
}
static void doplot() { if (usecolor) plotColor(); else plot(); }
static void stepA() { ZE2 += ystep[Z1e]; rowaddr(); }
static void stepC() {
	uint8_t v = (uint8_t)(Z0c + xstep[Z1e]);
	if (v & 0x80) { Z0b--; Z0c = (uint8_t)((v & 7) - 1); return; }
	Z0c = v;
	if (Z0c < 7) return;
	if (Z0c == 7) Z0c = 0xff; else Z0c = Z0c & 7;
	Z0c = (uint8_t)(Z0c + 1); Z0b++;
}
static void linesetup() {
	Z1493 = 0; Z1494 = 0;
	for (int g = 0; g < 100000; g++) {
		int diff = (int)Z148e - (int)Z1490;
		if (diff < 0) { if (Z148f == 0) break; Z148f--; }
		Z148e = (uint8_t)diff; Z1493++; if (Z1493 == 0) Z1494++;
	}
	Z1496 = Z148e; Z1495 = Z148e; Z1491 = Z1493; Z1492 = Z1494; Z1497 = (uint8_t)(Z1490 - Z1496);
}
static void stepB() {
	Z1493 = Z1491; Z1494 = Z1492;
	if (Z1495 == 0) { Z1495 = Z1496; return; }
	int s = (int)Z1496 + (int)Z1495;
	if (s > 0xff || (uint8_t)s >= Z1490) { Z1493++; if (Z1493 == 0) Z1494++; }
	int v = (int)Z1495 - (int)Z1497; if (v < 0) v += Z1490; Z1495 = (uint8_t)v;
}
static void line(uint16_t x2d, uint8_t y2) {
	uint8_t p1 = x2d & 0xff, p2 = x2d >> 8, p3 = y2;
	Z1e = 0;
	int dx = (int)(((int)ZE1 << 8) | ZE0) - (int)(((int)p2 << 8) | p1);
	Z1c = dx & 0xff; Z1d = (dx >> 8) & 0xff;
	if (dx < 0) { Z1d = ~Z1d; Z1c = (uint8_t)(~Z1c + 1); if (Z1c == 0) Z1d++; Z1e |= 2; }
	int dy = (int)p3 - (int)ZE2;
	Z1b = dy & 0xff;
	if (dy < 0) { Z1b = (uint8_t)(~Z1b + 1); Z1e |= 1; }
	Z1b++; Z1c++; if (Z1c == 0) Z1d++;
	Z1446 = Z1447 & 1; Z18 = p1; Z19 = p2; Z1a = p3;
	usecolor = 0;
	if (Z1447 != 4 && Z1447 != 7) {
		usecolor = 1; Z1446 = 0xff;
		ZE0 &= 0xfc; Z18 = p1 & 0xfc;
		int dx2 = (int)(((int)ZE1 << 8) | ZE0) - (int)(((int)p2 << 8) | Z18);
		Z1c = dx2 & 0xff; Z1d = (dx2 >> 8) & 0xff;
		if (dx2 < 0) { Z1d = ~Z1d; Z1c = (uint8_t)(~Z1c + 1); if (Z1c == 0) Z1d++; }
		uint16_t w = ((Z1d << 8) | Z1c) >> 2; Z1c = w & 0xff; Z1d = w >> 8;
		Z1c++; if (Z1c == 0) Z1d++;
		Z1e += 4;
	}
	if (Z1d == 0 && Z1c <= Z1b) {
		Z148f = 0; Z1490 = Z1c; Z148e = Z1b; linesetup(); int og = 0;
		do { uint8_t c3 = 0; int ig = 0; do { doplot(); stepA(); c3++; if (++ig > 4000) { hung = 1; return; } } while (c3 != Z1493);
			stepB(); stepC(); Z1c--; if (Z1c == 0xff) Z1d--; if (++og > 8000) { hung = 1; return; } } while (Z1d != 0 || Z1c != 0);
	} else {
		Z148e = Z1c; Z148f = Z1d; Z1490 = Z1b; linesetup(); int og = 0;
		do { uint8_t c3 = 0; Z1445 = 0; int ig = 0; do { doplot(); stepC(); c3++; if (c3 == 0) Z1445++; if (++ig > 40000) { hung = 1; return; } } while (c3 != Z1493 || Z1445 != Z1494);
			stepB(); stepA(); Z1b--; if (++og > 8000) { hung = 1; return; } } while (Z1b != 0);
	}
	posn(((Z19 << 8) | Z18), Z1a);
}

// ---- op12 brush ($105A) -------------------------------------------------------
static const uint8_t DHGR_DBL[16] = {0x00, 0x03, 0x0c, 0x0f, 0x30, 0x33, 0x3c, 0x3f,
                                     0xc0, 0xc3, 0xcc, 0xcf, 0xf0, 0xf3, 0xfc, 0xff};
static void brush_blit_col(int row, int col, uint8_t bits) {
	if (!bits) return;
	uint16_t base = CALC(row);
	const uint8_t *pat = (row & 1) ? curOdd : curEven;
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
// Faithful port of the <D> circle (FUN_0aeb + plot8 FUN_0bcb + steps FUN_0b95/
// FUN_0ba5). The centre is the current pen position (ZE0/ZE1 doubled x, ZE2 y).
// The x extents run to +-2*radius and x is advanced at DOUBLE resolution (a plot
// between two single x steps) while y advances at single resolution, so the
// outline is round on the 560-wide raster AND gap-free. This matters for fill
// containment: the Talisman title's lamp-knob circle (op DRAW_CIRCLE r=4 @135,90)
// must close, or the following fill=62 PAINT leaks out of the lamp into the sky.
static int cir_xRi, cir_xLi, cir_xRo, cir_xLo; // inner/outer x right/left (doubled)
static int cir_yBi, cir_yTi, cir_yBo, cir_yTo; // inner/outer y bottom/top
static int cir_d;                              // half-step parity counter ($000d)

static void circle_point(int x, int y) {
	if (y < 0 || y > 0x9f || x < 0 || x >= 0x230) return;  // FUN_0c35 bounds
	posn((uint16_t)x, (uint8_t)y);
	doplot();
}
static void circle_plot8() {
	cir_d++;                                   // FUN_0bcb increments $000d first
	circle_point(cir_xLi, cir_yTo);
	circle_point(cir_xRi, cir_yTo);
	circle_point(cir_xRi, cir_yBo);
	circle_point(cir_xLi, cir_yBo);
	circle_point(cir_xRo, cir_yTi);
	circle_point(cir_xLo, cir_yTi);
	circle_point(cir_xLo, cir_yBi);
	circle_point(cir_xRo, cir_yBi);
}
static void circle_stepBA() {                  // FUN_0ba5: inner x only, maybe inner y
	cir_xRi++; cir_xLi--;
	if (!(cir_d & 1)) { cir_yBi++; cir_yTi--; }
}
static void circle_stepB() {                   // FUN_0b95: outer x, then FUN_0ba5
	cir_xRo--; cir_xLo++;
	circle_stepBA();
}
static void draw_circle(uint8_t radius) {
	if (radius == 0) return;
	int C = (ZE1 << 8) | ZE0;                  // center x/y = pen position. The real
	int cy = ZE2;                              // circle saves/restores zero-page
	                                           // $05..$17 around itself; here neither
	                                           // C nor cy is mutated, so they double
	                                           // as the saved position to restore.
	usecolor = (Z1447 != 4 && Z1447 != 7) ? 1 : 0;
	Z1446 = Z1447 & 1;
	int r = radius, dvar = r >> 1, i = 0;
	cir_d = 0;
	cir_xRi = cir_xLi = C;
	cir_xRo = C + 2 * r; cir_xLo = C - 2 * r;
	cir_yTi = cy; cir_yBi = cy;
	cir_yBo = cy + r; cir_yTo = cy - r;
	// Loop provably terminates: r only decreases (borrow case) and i only increases
	// until r < i. radius is a uint8_t, so this runs well under 256 iterations.
	for (;;) {
		circle_plot8();
		if (r < i) break;
		i++;
		int tmp = dvar - i;
		if (dvar >= i) {                       // no borrow: inner extents only
			dvar = tmp;
			circle_stepBA(); circle_plot8(); circle_stepBA();
		} else {                               // borrow: step radius + outer extents in
			r--;
			dvar = tmp + r;
			cir_yBo--; cir_yTo++;
			circle_stepB(); circle_plot8(); circle_stepB();
		}
	}
	posn((uint16_t)C, (uint8_t)cy);            // restore pen position
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
// halves (left = cbf, right = cc0 with carry) via DHGR_DBL (FUN_0c8d) -- and blit
// both across three columns at (row, col..col+2). The shared core of the op12 brush
// blitter (normal=false) and the op3/op5 glyph blitter (FUN_0c60 -> FUN_108e). A
// zero byte expands to two empty halves, i.e. a no-op.
static void dhgr_blit_byte(int row, int col, uint8_t b, int bit, bool normal) {
	if (row < 0 || row > 0xbf) return;
	uint8_t cc0 = DHGR_DBL[b >> 4], lo = DHGR_DBL[b & 0xf];
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
#define COL_BITS 7
#define QUEUE_SIZE 32
#define QUEUE_MASK 0x1f
#define DIR_UP 0
#define DIR_DOWN 1
#define DIR_DISABLED 0xff
struct Span { uint8_t row; int left, right; uint8_t dir; };
static Span g_queue[QUEUE_SIZE];
static uint8_t g_head, g_tail;
static uint8_t g_row;
static int g_pos, g_edge;
static uint8_t g_bit, g_edge_bit;
static uint16_t g_row_addr;
static uint8_t g_parity;
static uint8_t g_clip_left, g_clip_right, g_clip_top, g_clip_bottom;

// op15 fill-bounds rectangle, in source byte columns (0..39) and rows (0..0x9f),
// set by op15/1 (full screen) and op15/2 (4 explicit bounds). op14 PAINT clips
// the flood fill to this rectangle, exactly as the standard renderer does
// (ctx->fill_*): without it a room whose floor opens toward the right edge -- the
// Coveted Mirror prison tower -- leaks the fill up and over the whole picture.
static uint8_t g_fillLeft, g_fillRight, g_fillTop, g_fillBottom;

static const uint8_t gmBIT[7]   = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40};
static const uint8_t gmLMASK[7] = {0x80, 0x81, 0x83, 0x87, 0x8f, 0x9f, 0xbf};
static const uint8_t gmRMASK[7] = {0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80};

static inline int col_of(int pos) { return pos / COL_BITS; }
static inline int bit_of(int pos) { return pos % COL_BITS; }
static inline int to_pos(int col, int bit) { return col * COL_BITS + bit; }
// A malformed op15/2 (unclamped fill-bounds bytes) or an op14 PAINT with an
// out-of-range row can drive g_row_addr + (col >> 1) past A2_PAGE_SIZE. Valid
// images never exceed 0x1ff7, so these guards only bite in the corrupt regime;
// they mirror the explicit bounds checks the other dhgr_put callers already do.
static inline uint8_t fb_read(int col) {
	uint16_t off = (uint16_t)(g_row_addr + (col >> 1));
	if (off >= A2_PAGE_SIZE) return 0;
	return (col & 1 ? s_main : s_aux)[off];
}
static inline void fb_write(int col, uint8_t v) {
	uint16_t off = (uint16_t)(g_row_addr + (col >> 1));
	if (off >= A2_PAGE_SIZE) return;
	dhgr_put(col & 1 ? s_main : s_aux, off, v);
}

static void set_row_address() { g_row_addr = CALC(g_row); g_parity = (uint8_t)((g_row & 1) << 3); }
static bool pixel_is_set() { return (gmBIT[bit_of(g_pos)] & fb_read(col_of(g_pos))) != 0; }
static uint8_t right_edge_mask(uint8_t p) {
	if ((p & 0x7f) == 0x7f) return 0xff;
	g_edge_bit = 7; uint8_t bit;
	do { g_edge_bit--; bit = (uint8_t)(p & 0x7f); p = (uint8_t)(p << 1); } while (bit >> 6);
	return gmRMASK[g_edge_bit];
}
static uint8_t left_edge_mask(uint8_t p) {
	p &= 0x7f;
	if (p == 0x7f) return 0xff;
	g_bit = 0xff; uint8_t bit;
	do { g_bit++; bit = (uint8_t)(p & 1); p = (uint8_t)(p >> 1); } while (bit);
	return gmLMASK[g_bit];
}
static void blend_byte(uint8_t mask, int col) {
	uint8_t existing = fb_read(col);
	uint8_t kept = (uint8_t)((mask ^ existing) & 0x7f);
	uint8_t pattern = (g_parity ? curOdd : curEven)[col & 7];
	fb_write(col, (uint8_t)((pattern & mask) | kept));
}
static void paint_span() {
	int col = col_of(g_pos);
	g_bit = bit_of(g_pos);
	g_edge_bit = 0xff;
	uint8_t existing = fb_read(col);
	uint8_t right_mask = right_edge_mask((uint8_t)(existing | gmRMASK[g_bit]));
	uint8_t left_input = (uint8_t)(gmLMASK[g_bit] | existing);
	g_bit = 7;
	uint8_t mask = (uint8_t)(left_edge_mask(left_input) & right_mask);
	uint8_t left_bit;
	for (;;) {
		blend_byte(mask, col);
		if (g_edge_bit != 0xff) { left_bit = g_edge_bit; break; }
		if (--col < g_clip_left) { col++; left_bit = 0xff; break; }
		mask = right_edge_mask(fb_read(col));
	}
	uint8_t edge_bit = left_bit + 1;
	if (edge_bit == 7) { edge_bit = 0; col++; }
	g_edge = to_pos(col, edge_bit);
	col = col_of(g_pos);
	for (;;) {
		if (g_bit == 7 && col != g_clip_right) {
			col++;
			blend_byte(left_edge_mask(fb_read(col)), col);
			continue;
		}
		g_bit--;
		if (g_bit & 0x80) { g_bit = 6; col--; }
		g_pos = to_pos(col, g_bit);
		return;
	}
}
static void enqueue_span(uint8_t row, int left, int right, uint8_t dir) {
	// Drop a single-pixel span (left == right): the real CM <D> span fill ($0F50)
	// rejects it, and without it a flood leaks through the 1-cell gaps that the
	// doubled DHGR boundaries leave in a drawn outline. This matters whenever the
	// fill bounds are full-screen so only the drawn boundary contains the flood --
	// e.g. the prison-tower floor (RA idx0), whose op15/0 RLE bounds are re-opened
	// to full by a following op15/2, so the floor fill leaks up over the whole
	// picture without this guard. It is merely redundant (not wrong) for scenes
	// like the cupboard door (OE idx0), where the op15/0 RLE bounds persist and
	// already clip the fill; that scene stays byte-exact with the guard in place.
	if (left == right) return;
	uint8_t clip_row = (dir == DIR_UP) ? g_clip_top : g_clip_bottom;
	if (row == clip_row) return;
	Span &s = g_queue[g_tail];
	s.row = row; s.left = left; s.right = right; s.dir = dir;
	g_tail = (g_tail + 1) & QUEUE_MASK;
}
enum WalkAction { WALK_ADVANCE, WALK_DONE };
static WalkAction paint_run(const Span &parent) {
	paint_span();
	enqueue_span(g_row, g_edge, g_pos, parent.dir);
	int run_right = g_pos;
	if (g_edge < parent.left - 1)
		enqueue_span(g_row, g_edge, parent.left - 2, parent.dir ^ 1);
	if (parent.right + 1 < run_right) {
		enqueue_span(g_row, parent.right + 2, run_right, parent.dir ^ 1);
		return WALK_DONE;
	}
	if (parent.right + 1 == run_right) return WALK_DONE;
	g_pos = run_right;
	return WALK_ADVANCE;
}
static WalkAction handle_run(const Span &parent) {
	for (uint8_t i = (g_head + 1) & QUEUE_MASK; i != g_tail; i = (i + 1) & QUEUE_MASK) {
		Span &other = g_queue[i];
		if (other.row != g_row) continue;
		if (other.dir == parent.dir) continue;
		if (g_pos < other.left || g_pos > other.right) continue;
		if (g_pos != other.left && other.left < parent.left - 1)
			enqueue_span(g_row, other.left, parent.left - 2, other.dir);
		if (other.right <= parent.right) {
			other.dir = DIR_DISABLED;
			g_pos = other.right;
			return WALK_ADVANCE;
		}
		other.left = parent.right + 1;
		return WALK_DONE;
	}
	return paint_run(parent);
}
static void process_queue_head() {
	Span parent = g_queue[g_head];
	if (parent.dir == DIR_DISABLED) return;
	g_row = (parent.dir == DIR_UP) ? parent.row - 1 : parent.row + 1;
	g_pos = parent.left;
	set_row_address();
	for (;;) {
		if (pixel_is_set() && handle_run(parent) == WALK_DONE) return;
		if (g_pos == parent.right) return;
		g_pos++;
	}
}
static void run_fill() {
	set_row_address();
	if (!pixel_is_set()) return;
	paint_span();
	g_head = g_tail = 0;
	enqueue_span(g_row, g_edge, g_pos, DIR_UP);
	enqueue_span(g_row, g_edge, g_pos, DIR_DOWN);
	long guard = 0;
	while (g_head != g_tail) {
		if (++guard > 2000000) return;
		process_queue_head();
		g_head = (g_head + 1) & QUEUE_MASK;
	}
}
static void flood_fill(uint16_t x, uint8_t y) {
	// Clip to the op15 fill-bounds rectangle. The fill runs in DHGR columns
	// (0..0x4f); a source byte column B spans DHGR columns 2B and 2B+1, so the
	// byte-column bounds double (left -> 2*left, right -> 2*right+1). Rows are not
	// doubled. Matches the standard renderer, which clips PAINT to ctx->fill_*.
	g_clip_left   = (uint8_t)(2 * g_fillLeft);
	g_clip_right  = (uint8_t)(2 * g_fillRight + 1);
	g_clip_top    = g_fillTop;
	g_clip_bottom = g_fillBottom;
	g_row = y; g_pos = x;
	run_fill();
}

// op15 sub-op 3: fill the clip rectangle with the current fill pattern.
static void fill_rect() {
	for (int row = 0; row <= 0x9f; row++) {
		uint16_t base = CALC(row);
		const uint8_t *pat = (row & 1) ? curOdd : curEven;
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
// doubling table (DHGR_DBL) into a main + aux byte and stores both at the same
// page offset (logical DHGR columns 2*col / 2*col+1) -- the same expansion the
// brush blitter above performs. Without this, op15/0 used to fall through doing
// nothing AND consuming no operand bytes, so the bounds+RLE stream was misparsed
// as opcodes and the picture collapsed to the white reset page.
static void rle_bitmap_dhgr(const uint8_t **pp, const uint8_t *end) {
	const uint8_t *p = *pp;
	if (p + 4 > end) { *pp = end; return; }
	uint8_t right = *p++, left = *p++, bottom = *p++, top = *p++;
	// The real CM op15/0 reads these four bytes through gm_op15_read_bounds
	// (cm_ram_raw.bin @0x0a17), which is the SAME routine op15/2 uses: it stores
	// them as the fill-clip rectangle (left/right/top/bottom). A later PAINT flood
	// is clipped to this rectangle. The cupboard-door overlay (OE idx0) relies on
	// this -- its only op15 is this RLE bitmap (rows 97..135), and the following
	// fill=62 PAINT must not leak below row 135 onto the floor. (Rooms like RA idx2
	// re-open the bounds with an explicit op15/2 after their RLE, so this is safe.)
	g_fillRight = right; g_fillLeft = left; g_fillBottom = bottom; g_fillTop = top;
	int col = left, row = top;
	bool done = false;
	while (!done && p < end) {
		uint8_t b = *p++;
		int reps = 1;
		if (b == 0x80) {
			if (p + 2 > end) break;
			reps = *p++ + 1;
			b = *p++;
		}
		while (reps--) {
			uint16_t off = (uint16_t)(CALC(row) + col);
			if (row <= 0x9f && col < 40 && off < A2_PAGE_SIZE) {
				uint8_t hi = DHGR_DBL[b >> 4], lo = DHGR_DBL[b & 0xf];
				uint8_t mainByte = (uint8_t)((hi << 1) | (lo >> 7));
				uint8_t auxByte = (uint8_t)(lo & 0x7f);
				dhgr_put(s_aux, off, auxByte);
				dhgr_put(s_main, off, mainByte);
			}
			if (row != bottom) {
				row++;
			} else if (col == right) {
				done = true;
				break;
			} else {
				col++;
				row = top;
			}
		}
	}
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
static uint8_t s_cmPanelAux[A2_PAGE_SIZE];
static uint8_t s_cmPanelMain[A2_PAGE_SIZE];
static bool s_cmPanelValid = false;
static int s_cmPanelCol0 = 24, s_cmPanelCol1 = 39;

void gmDhgrCaptureCMPanel(int col0, int col1) {
	std::memcpy(s_cmPanelAux,  s_aux,  A2_PAGE_SIZE);
	std::memcpy(s_cmPanelMain, s_main, A2_PAGE_SIZE);
	s_cmPanelCol0 = col0;
	s_cmPanelCol1 = col1;
	s_cmPanelValid = true;
}

bool gmDhgrCMPanelValid() { return s_cmPanelValid; }

// Scratch state for gmDhgrBeginCMPanelRebuild()/gmDhgrEndCMPanelRebuild().
static uint8_t s_cmRebuildSaveAux[A2_PAGE_SIZE], s_cmRebuildSaveMain[A2_PAGE_SIZE];
static uint8_t s_cmRebuildSaveSlowAux[A2_PAGE_SIZE], s_cmRebuildSaveSlowMain[A2_PAGE_SIZE];
static bool s_cmRebuildRecording = false;

void gmDhgrBeginCMPanelRebuild() {
	std::memcpy(s_cmRebuildSaveAux,  s_aux,  A2_PAGE_SIZE);
	std::memcpy(s_cmRebuildSaveMain, s_main, A2_PAGE_SIZE);
	std::memcpy(s_cmRebuildSaveSlowAux,  s_slowAux,  A2_PAGE_SIZE);
	std::memcpy(s_cmRebuildSaveSlowMain, s_slowMain, A2_PAGE_SIZE);
	s_cmRebuildRecording = s_slow.recording();
	s_slow.setRecording(false);
	std::memset(s_main, 0x00, A2_PAGE_SIZE);   // black panel background
	std::memset(s_aux,  0x00, A2_PAGE_SIZE);
}

void gmDhgrEndCMPanelRebuild() {
	gmDhgrCaptureCMPanel(24, 39);
	std::memcpy(s_aux,  s_cmRebuildSaveAux,  A2_PAGE_SIZE);
	std::memcpy(s_main, s_cmRebuildSaveMain, A2_PAGE_SIZE);
	std::memcpy(s_slowAux,  s_cmRebuildSaveSlowAux,  A2_PAGE_SIZE);
	std::memcpy(s_slowMain, s_cmRebuildSaveSlowMain, A2_PAGE_SIZE);
	s_slow.setRecording(s_cmRebuildRecording);
}

// Panel-column helpers shared by the overlay / band-restore / grain-mirror paths.
// cm_panel_to_pages copies the saved static panel onto BOTH the final and the slow
// pages (rows y0..y1); cm_panel_final_to_slow mirrors the final pages' panel columns
// (e.g. with freshly stamped grains) onto the slow pages only.
static void cm_panel_to_pages(int y0, int y1) {
	for (int row = y0; row <= y1; row++) {
		uint16_t base = CALC(row);
		for (int col = s_cmPanelCol0; col <= s_cmPanelCol1; col++) {
			uint16_t a = (uint16_t)(base + col);
			if (a >= A2_PAGE_SIZE) continue;
			s_aux[a]  = s_slowAux[a]  = s_cmPanelAux[a];
			s_main[a] = s_slowMain[a] = s_cmPanelMain[a];
		}
	}
}
static void cm_panel_final_to_slow(int y0, int y1) {
	for (int row = y0; row <= y1; row++) {
		uint16_t base = CALC(row);
		for (int col = s_cmPanelCol0; col <= s_cmPanelCol1; col++) {
			uint16_t a = (uint16_t)(base + col);
			if (a >= A2_PAGE_SIZE) continue;
			s_slowAux[a]  = s_aux[a];
			s_slowMain[a] = s_main[a];
		}
	}
}

// Copy the saved panel byte-columns onto both the final and the
// progressively-revealed pages, so the panel is present immediately even while a
// room's slow-draw reveal is still running (mirrors the standard renderer, which
// writes the panel to s_screenmem and s_slowScreen alike).
void gmDhgrOverlayCMPanel() {
	if (!s_cmPanelValid)
		return;
	cm_panel_to_pages(0, DHGR_HEIGHT - 1);
}

// Stamp the resting pile of `sand` white grains on top of the panel, at the same
// per-grain positions the standard renderer uses (gmCMHourglassGrainPos), the
// Apple x doubled onto the 560-wide pages. Call after gmDhgrOverlayCMPanel().
void gmDhgrDrawCMHourglass(int sand) {
	if (!s_cmPanelValid)
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
	for (int idx = 0; idx < sand && idx < 256; idx++) {
		uint16_t x;
		uint8_t y;
		if (!gmCMHourglassGrainPos((uint8_t)idx, &x, &y))
			continue;
		draw_brush((uint16_t)(x << 1), y, 0);
	}
	s_slow.setRecording(savedRecord);
	// Mirror the panel columns (now including the freshly stamped grains) onto the
	// slow pages, so they are visible during a reveal too.
	cm_panel_final_to_slow(0, DHGR_HEIGHT - 1);
}

// --- The Coveted Mirror per-turn grain fall (double hi-res) -------------------
// Double-hi-res counterpart of the standard fall (gmCMHourglassFall* in
// graphics_magician.cpp). The resting pile is already stamped at the post-drop
// level by gmDhgrDrawCMHourglass(); these paint the single freed grain travelling
// down the neck, confined to the band [kDhgrBandTop,kDhgrBandBot] so the pile
// above is never touched. Geometry matches the standard renderer exactly; the
// grain x is doubled onto the 560-wide pages (draw_brush(x<<1,...)), the same way
// gmDhgrDrawCMHourglass stamps the pile.
static int s_dhgrCmFallFrame = -1;     // active fall frame (0..kDhgrFallFrames); -1 idle

static const int kDhgrNeckTop   = 95;  // first plot y -> screen row ~102 (below waist)
static const int kDhgrNeckStep  = 4;   // plot-y advance per frame
static const int kDhgrFallFrames = 6;  // grain positions; one extra clear frame follows
static const int kDhgrNeckX     = 245; // Apple x of the stream centre (doubled to 490)
static const int kDhgrBandTop   = 100; // erase band: waist down to the mound top,
static const int kDhgrBandBot   = 125; // clear of the resting pile (ends at row ~99)

// Restore the saved static panel (no grains) for rows [y0,y1] onto both the final
// and slow pages, wiping the previous frame's falling grain before the next.
static void cmDhgrOverlayPanelBand(int y0, int y1) {
	if (!s_cmPanelValid)
		return;
	if (y0 < 0)
		y0 = 0;
	if (y1 > DHGR_HEIGHT - 1)
		y1 = DHGR_HEIGHT - 1;
	cm_panel_to_pages(y0, y1);
}

void gmDhgrCMHourglassFallBegin() { s_dhgrCmFallFrame = 0; }
bool gmDhgrCMHourglassFallActive() { return s_dhgrCmFallFrame >= 0; }

// Render the current fall frame and advance. Sets *y0/*y1 to the dirty row band.
// Returns true while more frames remain; the final frame paints no grain (just
// restores the clean neck) so nothing lingers.
bool gmDhgrCMHourglassFallStep(int *y0, int *y1) {
	if (s_dhgrCmFallFrame < 0) {
		*y0 = *y1 = 0;
		return false;
	}
	cmDhgrOverlayPanelBand(kDhgrBandTop, kDhgrBandBot);
	if (s_dhgrCmFallFrame < kDhgrFallFrames) {
		bool savedRecord = s_slow.recording();
		s_slow.setRecording(false);
		set_fill_color(0x34);                 // white, as the pile grains
		int gy = kDhgrNeckTop + s_dhgrCmFallFrame * kDhgrNeckStep;
		if (gy > kDhgrBandBot)
			gy = kDhgrBandBot;
		draw_brush((uint16_t)(kDhgrNeckX << 1), (uint8_t)gy, 0);
		s_slow.setRecording(savedRecord);
		// Mirror the band's panel columns (with the grain) onto the slow pages so a
		// blit reading either page shows it.
		cm_panel_final_to_slow(kDhgrBandTop, kDhgrBandBot);
	}
	*y0 = kDhgrBandTop;
	*y1 = kDhgrBandBot;

	s_dhgrCmFallFrame++;
	bool more = s_dhgrCmFallFrame <= kDhgrFallFrames;
	if (!more)
		s_dhgrCmFallFrame = -1;
	return more;
}

void gmDhgrCMHourglassFallAbort() {
	if (s_dhgrCmFallFrame < 0)
		return;
	cmDhgrOverlayPanelBand(kDhgrBandTop, kDhgrBandBot);
	s_dhgrCmFallFrame = -1;
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

void gmDhgrDrawImage(const uint8_t *data, size_t size) {
	hung = 0;
	// When not animating, keep the visible pages in step with the final pages so a
	// later slow-drawn overlay (e.g. a moved object) composes onto the right
	// background instead of a stale/blank page -- exactly what the standard hi-res
	// renderer does at the end of gmDrawImage. A scope guard so it runs on every
	// exit path, including the op0 END and `hung` early returns below.
	struct SyncOnExit {
		~SyncOnExit() { s_slow.syncIfNotRecording(); }
	} syncOnExit;
	// Restore the T5 default colour subindex FIRST, so the per-image init below
	// (and the image's own op7 SET_PALETTE_SUBINDEX commands) start from a clean
	// baseline. op7 overrides are per-image: the same streams render self-coloured
	// under std-hires, where op7 is a no-op. Order matters -- set_fill_color(0x4d)
	// reads s_subidx to build the default fill pattern used by fill_rect (the
	// full-screen background many rooms paint), so it must see the defaults, not a
	// previous room's patches (which turned e.g. the hill pink after the bazaar).
	std::memcpy(s_subidx, s_subidxDefault, sizeof(s_subidx));
	Z1447 = 4; posn(140 * 2, 96);     // per-image draw init (pen colour 4, centre)
	set_fill_color(0x4d);             // default fill colour
	// Default PAINT clip = full screen, until an op15/1 or op15/2 narrows it
	// (matches the standard renderer's ctx->fill_* defaults).
	g_fillLeft = 0; g_fillTop = 0; g_fillRight = 39; g_fillBottom = 0x9f;
	g_brush = 5;                      // default shape; reset per image (matches the
	                                  // std-hires init ctx.BRUSH=5). Without this an
	                                  // item overlay inherits the room's last op4
	                                  // SET_SHAPE, drawing its brushes with the wrong
	                                  // shape so the fills find no seed (e.g. the
	                                  // OO-Topos prison-cell bottle/food went the
	                                  // colour of the floor/table beneath them).
	const uint8_t *p = data, *e = data + size;
	int guard = 0;
	while (p < e && guard++ < 100000) {
#ifdef DHGR_WATCH
		s_dhgrOpTag = (int)(p - data);
#endif
		uint8_t op = *p++; uint8_t par = op & 0xf; op >>= 4; uint16_t x;
		switch (op) {
		case 0: return;                                   // END
		case 1: { x = *p++ + (par & 1 ? 256 : 0);         // SET_TEXT_POS
			s_textX = (uint16_t)(x << 1); s_textY = *p++; } break;
		case 2: Z1447 = par; break;                       // SET_PEN_COLOR
		case 3: dhgr_draw_glyph(s_textX, s_textY, *p++, true);  s_textX += 14; break;  // TEXT_CHAR
		case 4: g_brush = par; break;                     // SET_SHAPE
		case 5: dhgr_draw_glyph(s_textX, s_textY, *p++, false); s_textX += 14; break;  // TEXT_OUTLINE
		case 6: set_fill_color(*p++); break;              // SET_FILL_COLOR
		case 7: { uint8_t i = *p++; uint8_t v = *p++; s_subidx[i] = v; } break; // SET_PALETTE_SUBINDEX
		case 8: { x = *p++ + (par & 1 ? 256 : 0); uint8_t y = *p++; posn(x << 1, y); } break;  // MOVE_TO
		case 9: { x = *p++ + (par & 1 ? 256 : 0); uint8_t y = *p++; uint16_t a = x << 1, b = y; // DRAW_BOX
			uint16_t x0 = (ZE1 << 8) | ZE0; uint8_t y0 = ZE2;
			line(x0, (uint8_t)b); line(a, (uint8_t)b); line(a, y0); line(x0, y0);
			posn(a, (uint8_t)b); } break;
		case 10: { x = *p++ + (par & 1 ? 256 : 0); uint8_t y = *p++; line(x << 1, y); } break;  // DRAW_LINE
		case 11: draw_circle(*p++); break;                // DRAW_CIRCLE
		case 12: { x = *p++ + (par & 1 ? 256 : 0); uint8_t y = *p++; draw_brush(x << 1, y, g_brush); } break; // DRAW_SHAPE
		case 13: { uint8_t units = *p++;                  // DELAY
			s_slow.recordDelay(units); } break;
		case 14: { x = *p++ + (par & 1 ? 256 : 0); uint8_t y = *p++; flood_fill(x << 1, y); } break; // PAINT
		case 15: // RESET (sub-op in low nibble)
			if (par == 0) rle_bitmap_dhgr(&p, e);   // RLE bitmap (Coveted Mirror)
			else if (par == 1) {                    // bounds = full screen
				g_fillLeft = 0; g_fillTop = 0; g_fillRight = 39; g_fillBottom = 0x9f;
			} else if (par == 2) {                  // read 4 bounds bytes
				g_fillRight = *p++; g_fillLeft = *p++; g_fillBottom = *p++; g_fillTop = *p++;
			} else if (par == 3) fill_rect();       // fill background with pattern
			break;
		}
		if (hung) return;
	}
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
	if (s_cmPanelValid) {
		init_offset_tables();
		uint8_t c = s_colOfOffset[op.offset];
		if (c >= s_cmPanelCol0 && c <= s_cmPanelCol1)
			return false;
	}
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
