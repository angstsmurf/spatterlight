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
#include "gm_artifact.h"
#include <cstring>
#include <vector>

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

// A WriteOp with this (out-of-page) offset is a DELAY marker rather than a byte
// write: op13 emitted it, and value holds the delay's operand. The reveal pauses
// when it reaches one.
#define DELAY_MARKER 0xffff
// Wall-clock length of one delay unit, in slow-draw ticks. Matched to the
// standard-hi-res renderer's op13 timing (~10 host ticks per unit).
#define DELAY_TICKS_PER_UNIT 10

struct WriteOp { uint16_t offset; uint8_t page; uint8_t value; }; // page: 1=main, 0=aux

static uint8_t s_slowMain[A2_PAGE_SIZE]; // progressively-revealed pages
static uint8_t s_slowAux[A2_PAGE_SIZE];
static std::vector<WriteOp> s_ops;       // ordered byte writes of this image
static size_t s_opsDrawn = 0;            // how many ops are on the slow pages
static bool s_recordOps = false;         // record ops for slow reveal?
static int s_pauseTicks = 0;             // reveal ticks still owed to a DELAY

// Bounding range of screen rows touched since the last blit, so the host can
// repaint just the changed band instead of the whole window each tick.
static int s_dirtyMin = 0x7fffffff, s_dirtyMax = -1;

// offset -> screen row (0..191), or 0xff for the inter-row gaps in the page
// address space. Built once; lets a reveal map a byte write back to its row.
static uint8_t s_rowOfOffset[A2_PAGE_SIZE];
static bool s_rowTableInit = false;

static void mark_row_dirty(uint16_t offset) {
	if (!s_rowTableInit) {
		std::memset(s_rowOfOffset, 0xff, sizeof(s_rowOfOffset));
		for (int row = 0; row < 192; row++) {
			uint16_t base = CALC(row);
			for (int col = 0; col < 40; col++)
				s_rowOfOffset[base + col] = (uint8_t)row;
		}
		s_rowTableInit = true;
	}
	uint8_t r = s_rowOfOffset[offset];
	if (r == 0xff)
		return;
	if (r < s_dirtyMin) s_dirtyMin = r;
	if (r > s_dirtyMax) s_dirtyMax = r;
}

// Every plotted byte funnels through here: write the final page and, when
// recording, append the op for the progressive reveal. `page` is s_main or
// s_aux; the recorded op tags which so the reveal targets the matching slow page.
static inline void dhgr_put(uint8_t *page, uint16_t off, uint8_t value) {
	page[off] = value;
	if (s_recordOps)
		s_ops.push_back({off, (uint8_t)(page == s_main ? 1 : 0), value});
}

// ---- drawing tables (loaded at runtime from T5) -------------------------------
// T5 is the boot disk's headerless double-hi-res interpreter file; it loads at
// $4000, so file_offset = RAM_addr - $4000. The four tables sit at fixed offsets.
static uint8_t s_subidx[256];     // $41B2 colour subindex (patched per-image by op7)
static uint8_t s_subidxDefault[256]; // pristine T5 copy, restored at each image's start
static uint8_t s_subtbl[512];     // $421E sub-table (idx -> two pattern rows)
static uint8_t s_pattern[2048];   // $441E pattern rows (8 bytes each)
static uint8_t s_brush[256];      // $4B2E brush bitmaps (8 brushes x 4 quad x 8)
static bool s_tablesInstalled = false;

bool gmDhgrInstallDrawingTables(const uint8_t *t5, size_t size) {
	// Need everything up to the brush table end ($4B2E + 256 = $4C2E -> off 0xC2E).
	if (size < 0xC2E)
		return false;
	std::memcpy(s_subidx,  t5 + 0x1B2, sizeof(s_subidx));
	std::memcpy(s_subidxDefault, s_subidx, sizeof(s_subidx));
	std::memcpy(s_subtbl,  t5 + 0x21E, 500);
	std::memcpy(s_pattern, t5 + 0x41E, 1808);
	std::memcpy(s_brush,   t5 + 0xB2E, sizeof(s_brush));
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
static void brush_quadrant(int col0, int row0, int bit, int brushbase) {
	for (int r = 0; r < 8; r++) {
		int row = row0 + r; if (row < 0 || row > 0xbf) continue;
		uint8_t bb = s_brush[(brushbase + r) & 0xff];
		uint8_t cc0 = DHGR_DBL[bb >> 4], lo = DHGR_DBL[bb & 0xf];
		uint8_t cy = (uint8_t)(lo >> 7); lo = (uint8_t)(lo << 1); cc0 = (uint8_t)((cc0 << 1) | cy); lo >>= 1;
		uint8_t cbf = lo;
		if (cbf) { uint8_t A = (uint8_t)(cbf << 1), e = 0;
			for (int y = bit; y > 0; y--) { uint8_t c = (uint8_t)(A >> 7); A = (uint8_t)(A << 1); e = (uint8_t)((e << 1) | c); }
			brush_blit_col(row, col0, (uint8_t)(A >> 1)); brush_blit_col(row, col0 + 1, e); }
		if (cc0) { uint8_t A = (uint8_t)(cc0 << 1), e = 0;
			for (int y = bit; y > 0; y--) { uint8_t c = (uint8_t)(A >> 7); A = (uint8_t)(A << 1); e = (uint8_t)((e << 1) | c); }
			brush_blit_col(row, col0 + 1, (uint8_t)(A >> 1)); brush_blit_col(row, col0 + 2, e); }
	}
}
static void draw_brush(uint16_t x16, uint8_t y, uint8_t brush) {
	int col = x16 / 7, bit = x16 % 7, bb = brush * 32;
	brush_quadrant(col,     y,     bit, bb + 0);
	brush_quadrant(col + 2, y,     bit, bb + 8);
	brush_quadrant(col,     y + 8, bit, bb + 16);
	brush_quadrant(col + 2, y + 8, bit, bb + 24);
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

static const uint8_t gmBIT[7]   = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40};
static const uint8_t gmLMASK[7] = {0x80, 0x81, 0x83, 0x87, 0x8f, 0x9f, 0xbf};
static const uint8_t gmRMASK[7] = {0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80};

static inline int col_of(int pos) { return pos / COL_BITS; }
static inline int bit_of(int pos) { return pos % COL_BITS; }
static inline int to_pos(int col, int bit) { return col * COL_BITS + bit; }
static inline uint8_t fb_read(int col) { return (col & 1 ? s_main : s_aux)[g_row_addr + (col >> 1)]; }
static inline void fb_write(int col, uint8_t v) { dhgr_put(col & 1 ? s_main : s_aux, (uint16_t)(g_row_addr + (col >> 1)), v); }

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
	// Real $0F50 drops a single-pixel span (left == right); without it 1-px spans
	// leak the flood through 1-cell gaps in a boundary.
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
	g_clip_left = 0; g_clip_right = 0x4f; g_clip_top = 0; g_clip_bottom = 0x9f;
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

// ---- public API ---------------------------------------------------------------
void gmDhgrResetScreen(bool white) {
	uint8_t bg = white ? 0x7f : 0x00;
	std::memset(s_main, bg, A2_PAGE_SIZE);
	std::memset(s_aux,  bg, A2_PAGE_SIZE);
	// A reset starts a fresh page: the background appears instantly on the
	// visible pages too, and any pending reveal is dropped.
	std::memset(s_slowMain, bg, A2_PAGE_SIZE);
	std::memset(s_slowAux,  bg, A2_PAGE_SIZE);
	s_ops.clear();
	s_opsDrawn = 0;
	s_pauseTicks = 0;
}

void gmDhgrDrawImage(const uint8_t *data, size_t size) {
	hung = 0;
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
	const uint8_t *p = data, *e = data + size;
	int guard = 0;
	while (p < e && guard++ < 100000) {
		uint8_t op = *p++; uint8_t par = op & 0xf; op >>= 4; uint16_t x;
		switch (op) {
		case 0: return;                                   // END
		case 1: p += 2; break;                            // SET_TEXT_POS
		case 2: Z1447 = par; break;                       // SET_PEN_COLOR
		case 3: p += 1; break;                            // TEXT_CHAR
		case 4: g_brush = par; break;                     // SET_SHAPE
		case 5: p += 1; break;                            // TEXT_OUTLINE
		case 6: set_fill_color(*p++); break;              // SET_FILL_COLOR
		case 7: { uint8_t i = *p++; uint8_t v = *p++; s_subidx[i] = v; } break; // SET_PALETTE_SUBINDEX
		case 8: { x = *p++ + (par & 1 ? 256 : 0); uint8_t y = *p++; posn(x << 1, y); } break;  // MOVE_TO
		case 9: { x = *p++ + (par & 1 ? 256 : 0); uint8_t y = *p++; uint16_t a = x << 1, b = y; // DRAW_BOX
			uint16_t x0 = (ZE1 << 8) | ZE0; uint8_t y0 = ZE2;
			line(x0, (uint8_t)b); line(a, (uint8_t)b); line(a, y0); line(x0, y0);
			posn(a, (uint8_t)b); } break;
		case 10: { x = *p++ + (par & 1 ? 256 : 0); uint8_t y = *p++; line(x << 1, y); } break;  // DRAW_LINE
		case 11: p += 1; break;                           // DRAW_CIRCLE
		case 12: { x = *p++ + (par & 1 ? 256 : 0); uint8_t y = *p++; draw_brush(x << 1, y, g_brush); } break; // DRAW_SHAPE
		case 13: { uint8_t units = *p++;                  // DELAY
			if (s_recordOps) s_ops.push_back({DELAY_MARKER, 0, units}); } break;
		case 14: { x = *p++ + (par & 1 ? 256 : 0); uint8_t y = *p++; flood_fill(x << 1, y); } break; // PAINT
		case 15: if (par == 3) fill_rect(); else if (par == 2) p += 4; break; // RESET
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

void gmDhgrSetSlowDraw(bool on) { s_recordOps = on; }

// True while there is reveal work left: recorded ops still to paint, or a DELAY
// pause still owed.
bool gmDhgrSlowDrawActive() {
	return s_recordOps && (s_opsDrawn < s_ops.size() || s_pauseTicks > 0);
}

// Bytes whose offset is adjacent to, or whose value matches, the previous one
// belong to the same visual run; revealing them together avoids seams.
static bool ops_adjacent(const WriteOp &a, const WriteOp &b) {
	return (a.page == b.page &&
	        (int)b.offset >= (int)a.offset - 1 && (int)b.offset <= (int)a.offset + 1) ||
	       b.value == a.value;
}

static inline void apply_op(const WriteOp &op) {
	(op.page ? s_slowMain : s_slowAux)[op.offset] = op.value;
	mark_row_dirty(op.offset);
}

// Apply up to `budget` recorded ops onto the visible pages, extending the chunk
// across adjacent runs. A DELAY marker halts this tick's reveal and owes a run
// of pause ticks (during which nothing is revealed). Returns true while any
// reveal work -- ops or an outstanding pause -- remains.
bool gmDhgrAdvanceSlowDraw(int budget) {
	if (s_pauseTicks > 0) {
		s_pauseTicks--;
		return s_opsDrawn < s_ops.size() || s_pauseTicks > 0;
	}
	size_t i = s_opsDrawn;
	size_t chunk_end = i + (budget > 0 ? (size_t)budget : 1);
	bool keep_going = false;
	for (; i < s_ops.size() && (i < chunk_end || keep_going); i++) {
		if (s_ops[i].offset == DELAY_MARKER) {
			s_pauseTicks += s_ops[i].value * DELAY_TICKS_PER_UNIT;
			s_opsDrawn = i + 1;             // consume the marker
			return s_opsDrawn < s_ops.size() || s_pauseTicks > 0;
		}
		apply_op(s_ops[i]);
		keep_going = (i + 1 < s_ops.size()) && s_ops[i + 1].offset != DELAY_MARKER &&
		             ops_adjacent(s_ops[i], s_ops[i + 1]);
	}
	s_opsDrawn = i;
	return s_opsDrawn < s_ops.size();
}

// Reveal the rest of the image at once (e.g. on a window resize, or when the
// reveal is cancelled). Leaves the visible pages identical to the final pages.
void gmDhgrFinishSlowDraw() {
	for (; s_opsDrawn < s_ops.size(); s_opsDrawn++) {
		if (s_ops[s_opsDrawn].offset == DELAY_MARKER)
			continue;                      // pauses collapse when finishing at once
		apply_op(s_ops[s_opsDrawn]);
	}
	s_pauseTicks = 0;
}

// Hand back the row band touched since the last call (inclusive), and reset it.
// Returns false if nothing changed.
bool gmDhgrSlowConsumeDirty(int *y0, int *y1) {
	if (s_dirtyMax < 0)
		return false;
	*y0 = s_dirtyMin;
	*y1 = s_dirtyMax;
	s_dirtyMin = 0x7fffffff;
	s_dirtyMax = -1;
	return true;
}

void gmDhgrBlitSlowToSurface(uint32_t *out, int w, int h) {
	blit_pages(s_slowAux, s_slowMain, out, w, h);
}

const uint8_t *gmDhgrMainPtr() { return s_main; }
const uint8_t *gmDhgrAuxPtr() { return s_aux; }

} // namespace Comprehend
} // namespace Glk
