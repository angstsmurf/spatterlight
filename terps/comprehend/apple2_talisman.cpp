/* apple2_talisman.cpp -- pixel-faithful renderer for the Apple II version of
 * Talisman (Polarware, 1987), STANDARD hi-res mode.
 *
 * Talisman's room/item pictures are drawn with Penguin Software's "Graphics
 * Magician" / "Picture Painter" vector format -- the SAME engine the Apple II
 * Scott Adams games use.  The drawing primitives render into a real Apple hi-res
 * framebuffer (s_screenmem); a second pass converts that to RGB using the Apple
 * NTSC artifact-colour algorithm (from terps/common_sagadraw/apple2draw.c).
 *
 * Each primitive here was transliterated from, and verified byte-for-byte
 * against, Talisman's own 6502 routines (Ghidra on a MAME RAM dump, plus running
 * the real ROM routines on identical inputs and diffing the resulting hi-res
 * page): line = Applesoft HLINE ($F53A); box = four HLINEs ($095B); circle =
 * midpoint circle ($0AD3); brush = $0FFB/$102E; flood fill = the queue-based
 * span fill ($0CCA); background fill = op15 sub-op3 ($0A2D).  Output matches the
 * hardware to ~99.8% (the residue is sub-pixel dither on a few fill/diagonal
 * edge bytes).
 *
 * This is the STANDARD hi-res renderer (the game's <S> boot option); the
 * canonical double hi-res renderer (<D>) is a separate, much larger porting
 * effort and is not used here.
 *
 * Talisman uses a LATER variant of the format that repurposes two opcodes the
 * 1984 Graphics Magician left unused (verified against MAME + the disassembly
 * at https://6502disassembly.com/a2-graphics-magician/PICDRAWH_1984.html):
 *   - op7 (0x7x): a 2-operand NO-OP (1984: end-of-image).
 *   - op15 (0xf, "RESET"): sub-op in the low nibble -- param1 sets the fill
 *     rectangle to full screen, param2 reads 4 bytes of bounds, param3 fills
 *     the rectangle with the current colour pattern (the per-image background).
 * The colour/pattern tables (kPatternData/kColorPatternSubindices/kBrushBitmaps)
 * were extracted from a live MAME RAM dump ($120A/$1132/$1282) and match the
 * Graphics Magician tables byte-for-byte.
 */

#include "apple2_talisman.h"
#include <cstring>

namespace Glk {
namespace Comprehend {

// ---- Drawing tables (extracted from Talisman's Apple II RAM: $1132/$120A/$1282)

/* 30 colour patterns, 4 bytes each (hi-res byte patterns). */
static const uint8_t kPatternData[120] = {
    0x00, 0x00, 0x00, 0x00, 0x55, 0x2a, 0x55, 0x2a, 0x2a, 0x55, 0x2a, 0x55,
    0x7f, 0x7f, 0x7f, 0x7f, 0x80, 0x80, 0x80, 0x80, 0xd5, 0xaa, 0xd5, 0xaa,
    0xaa, 0xd5, 0xaa, 0xd5, 0xff, 0xff, 0xff, 0xff, 0x33, 0x66, 0x4c, 0x19,
    0xb3, 0xe6, 0xcc, 0x99, 0x4c, 0x19, 0x33, 0x66, 0xcc, 0x99, 0xb3, 0xe6,
    0x11, 0x22, 0x44, 0x08, 0x91, 0xa2, 0xc4, 0x88, 0x44, 0x08, 0x11, 0x22,
    0xc4, 0x88, 0x91, 0xa2, 0x22, 0x44, 0x08, 0x11, 0xa2, 0xc4, 0x88, 0x91,
    0x08, 0x11, 0x22, 0x44, 0x88, 0x91, 0xa2, 0xc4, 0xc9, 0xa4, 0x92, 0x89,
    0x24, 0x12, 0x49, 0x24, 0x77, 0x6e, 0x5d, 0x3b, 0xf7, 0xee, 0xdd, 0xbb,
    0x5d, 0x3b, 0x77, 0x6e, 0xdd, 0xbb, 0xf7, 0xee, 0x6e, 0x5d, 0x3b, 0x77,
    0xee, 0xdd, 0xbb, 0xf7, 0x3b, 0x77, 0x6e, 0x5d, 0xbb, 0xf7, 0xee, 0xdd,
};

/* 108 fill colours, each two bytes = even-row and odd-row sub-index [0,29]. */
static const uint8_t kColorPatternSubindices[216] = {
    0x03, 0x07, 0x16, 0x07, 0x1a, 0x1d, 0x1c, 0x17, 0x08, 0x0b, 0x00, 0x1b,
    0x00, 0x04, 0x03, 0x1b, 0x03, 0x06, 0x1a, 0x06, 0x00, 0x06, 0x00, 0x11,
    0x02, 0x06, 0x1c, 0x13, 0x10, 0x13, 0x10, 0x07, 0x02, 0x1b, 0x02, 0x07,
    0x02, 0x17, 0x02, 0x09, 0x1a, 0x04, 0x10, 0x04, 0x02, 0x05, 0x12, 0x17,
    0x1a, 0x07, 0x03, 0x17, 0x16, 0x19, 0x03, 0x05, 0x03, 0x0d, 0x1a, 0x0d,
    0x1a, 0x05, 0x10, 0x05, 0x00, 0x0d, 0x00, 0x17, 0x08, 0x05, 0x16, 0x05,
    0x01, 0x05, 0x16, 0x0b, 0x01, 0x07, 0x01, 0x17, 0x01, 0x09, 0x01, 0x04,
    0x16, 0x04, 0x0c, 0x0f, 0x01, 0x1b, 0x01, 0x11, 0x0c, 0x17, 0x0c, 0x04,
    0x16, 0x13, 0x01, 0x06, 0x16, 0x06, 0x0c, 0x11, 0x07, 0x07, 0x04, 0x04,
    0x07, 0x1b, 0x1b, 0x1d, 0x07, 0x11, 0x06, 0x07, 0x17, 0x06, 0x06, 0x1b,
    0x06, 0x06, 0x04, 0x06, 0x11, 0x13, 0x04, 0x11, 0x17, 0x07, 0x17, 0x0b,
    0x17, 0x19, 0x05, 0x07, 0x17, 0x05, 0x07, 0x0d, 0x05, 0x05, 0x05, 0x0d,
    0x0d, 0x0f, 0x04, 0x0d, 0x17, 0x04, 0x05, 0x1b, 0x05, 0x06, 0x03, 0x03,
    0x16, 0x03, 0x03, 0x0c, 0x00, 0x00, 0x08, 0x1a, 0x02, 0x16, 0x1a, 0x1c,
    0x03, 0x10, 0x02, 0x03, 0x02, 0x1a, 0x02, 0x02, 0x12, 0x1c, 0x00, 0x1a,
    0x12, 0x1a, 0x10, 0x12, 0x00, 0x10, 0x03, 0x1a, 0x16, 0x1a, 0x16, 0x12,
    0x01, 0x02, 0x16, 0x18, 0x01, 0x03, 0x01, 0x1a, 0x01, 0x16, 0x01, 0x01,
    0x01, 0x00, 0x16, 0x00, 0x16, 0x0c, 0x16, 0x0e, 0x0c, 0x0e, 0x00, 0x0c,
};

/* 8 brushes, each 4 quadrants x 8 bytes. */
static const uint8_t kBrushBitmaps[256] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x60,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x60, 0x40, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x70, 0x70, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x03, 0x07, 0x07, 0x70, 0x70, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x07, 0x07, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60,
    0x78, 0x78, 0x7c, 0x7c, 0x00, 0x00, 0x00, 0x03, 0x0f, 0x0f, 0x1f, 0x1f,
    0x7c, 0x7c, 0x78, 0x78, 0x60, 0x00, 0x00, 0x00, 0x1f, 0x1f, 0x0f, 0x0f,
    0x03, 0x00, 0x00, 0x00, 0x00, 0x60, 0x7c, 0x7e, 0x7e, 0x7e, 0x7f, 0x7f,
    0x00, 0x03, 0x1f, 0x3f, 0x3f, 0x3f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7e, 0x7e,
    0x7e, 0x7c, 0x60, 0x00, 0x7f, 0x7f, 0x3f, 0x3f, 0x3f, 0x1f, 0x03, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x40, 0x08, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x04, 0x00, 0x09, 0x28, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x08, 0x24,
    0x40, 0x12, 0x68, 0x60, 0x00, 0x04, 0x01, 0x14, 0x00, 0x2b, 0x03, 0x27,
    0x62, 0x48, 0x22, 0x28, 0x00, 0x10, 0x40, 0x00, 0x17, 0x09, 0x22, 0x15,
    0x00, 0x0a, 0x00, 0x00,
};

// ---- Apple hi-res framebuffer -------------------------------------------------

#define A2_SCREEN_MEM_SIZE 0x2000
#define MAX_SCREEN_ADDR    0x1fff
#define APPLE2_SCREEN_HEIGHT 192
#define APPLE2_SCREEN_COLS   40
#define COL_BITS 7
#define RIGHT_EXPAND_MASK 0x40

#define CALC_APPLE2_ADDRESS(y) ((((y / 8) & 0x07) << 7) + (((y / 8) & 0x18) * 5) + ((y & 7) << 10))

// Persistent across calls: item / overlay images are drawn straight on top of
// the room already rendered into the Apple hi-res page (the real interpreter
// never clears the screen for an item picture), so callers reset it explicitly
// via talismanResetScreen() only when starting a fresh room.
static uint8_t s_screenmem[A2_SCREEN_MEM_SIZE];

static inline bool valid_offset(uint16_t off) { return off <= MAX_SCREEN_ADDR; }

#ifdef TALISMAN_TRACE
// Diagnostic hooks for the offline pixtest harness (built with -DTALISMAN_TRACE,
// see test/pixtest.cpp). g_talismanWriteLog fires for every hi-res byte written,
// tagged with the current drawing primitive; g_talismanOnOp fires once per
// opcode, before it draws, with the opcode's stream position, raw bytes and pen.
// These are absent from the normal (non-traced) interpreter build.
void (*g_talismanWriteLog)(uint16_t, uint8_t, char) = nullptr;
void (*g_talismanOnOp)(int pos, int op, int b1, int b2, int x, int y) = nullptr;
static char g_currentPrim = '?';
static const uint8_t *g_imgBase = nullptr;
#endif

static void write_screen(uint16_t offset, uint8_t value) {
	if (valid_offset(offset)) {
		s_screenmem[offset] = value;
#ifdef TALISMAN_TRACE
		if (g_talismanWriteLog)
			g_talismanWriteLog(offset, value, g_currentPrim);
#endif
	}
}

// ---- Drawing context ----------------------------------------------------------
// (Opcode enum is shared via apple2_talisman.h.)

typedef struct {
	uint16_t HBAS;
	uint16_t HGR_X;
	uint8_t HGR_Y;
	uint8_t HMASK;
	uint8_t HGR_BITS;
	uint8_t HGR_HORIZ;
	uint8_t HGR_COLOR;     // current pen colour (hi-res byte)
	uint8_t BRUSH;         // current shape/brush type
	// current fill colour as even/odd-row pattern sub-indices ($08/$09),
	// set by op6 SET_FILL_COLOR; default (3,3) = white.
	uint8_t pat_even, pat_odd;
	// op15 fill rectangle bounds (column 0..39, row 0..191)
	uint8_t fill_left, fill_right, fill_top, fill_bottom;
} a2_ctx;

// State for the brush/shape blit (draw_brush -> draw_bitmap).
typedef struct {
	uint8_t scanline;
	uint8_t pattern_even;
	uint8_t pattern_odd;
	uint8_t seed_column;
	uint8_t pixel_offset;
	uint8_t bitmap_index;
	uint8_t brush_type;
} a2_brush_ctx;

static void set_color(uint8_t color_index, a2_ctx *ctx) {
	static const uint8_t COLORTBL[8] = {0x00, 0x2a, 0x55, 0x7f, 0x80, 0xaa, 0xd5, 0xff};
	if (color_index < 8)
		ctx->HGR_COLOR = COLORTBL[color_index];
}

static void set_draw_position(uint16_t xpos, uint8_t ypos, a2_ctx *ctx) {
	ctx->HGR_X = xpos;
	ctx->HGR_Y = ypos;
	ctx->HBAS = CALC_APPLE2_ADDRESS(ypos);
	ctx->HGR_HORIZ = xpos / COL_BITS;
	static const uint8_t masktable[COL_BITS] = {0x81, 0x82, 0x84, 0x88, 0x90, 0xa0, 0xc0};
	ctx->HMASK = masktable[xpos % COL_BITS];
	ctx->HGR_BITS = ctx->HGR_COLOR;
	if ((ctx->HGR_HORIZ & 1) == 1 && ((ctx->HGR_COLOR * 2 + 0x40) & 0x80))
		ctx->HGR_BITS = ctx->HGR_COLOR ^ 0x7f;
}

static void x_to_column_and_pixel(a2_brush_ctx *ctx, uint16_t x) {
	ctx->seed_column = x / COL_BITS;
	ctx->pixel_offset = x % COL_BITS;
}

// ---- Flood fill: byte-faithful port of the Apple II standard-hires Graphics
//      Magician fill (op14 -> $0cca). It is a queue-based span fill (32-entry
//      circular queue at $0F3B/$0F5B/$0F7B/$0F9B/$0FBB/$0FDB) with a span-merge
//      optimisation. The whole machine is transliterated 1:1 from the 6502 (via
//      Ghidra on the MAME RAM dump) because painting clears bits that later
//      boundary tests read, so the exact traversal/merge order is observable in
//      the output. The fill REPLACES connected "set" (white) pixels with the
//      current fill-colour dither pattern, bounded by clear (black) pixels.
//
// Zero-page mirror: f2=$02 scratch/existing, f3=$03 left-edge bit, f4=$04 queue
// head, f5=$05 pattern base, fa=$0a row, fb=$0b col, fc=$0c bit, fd=$0d scratch,
// fe=$0e left-edge col, ff=$0f queue tail, f10..f13 clip L/R/T/B, f14 row addr,
// f8/f9 even/odd fill sub-index. Tables: BIT=$0c69, LMASK=$10b6, RMASK=$10bd.

static uint8_t f2, f3, f4, f5, fa, fb, fc, fd, fe, ff, f10, f11, f12, f13, f8, f9;
static uint16_t f14;
static uint8_t Qcol[32], Qbit[32], Qcol2[32], Qbit2[32], Qy[32], Qdir[32];

static const uint8_t gmBIT[7] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40};
static const uint8_t gmLMASK[7] = {0x80, 0x81, 0x83, 0x87, 0x8f, 0x9f, 0xbf};
static const uint8_t gmRMASK[7] = {0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80};

static inline uint8_t neg(uint8_t a) { return a & 0x80; } // bit7 set after CMP => reg < op

static void gm_setaddr() {            // FUN_0c38
	f14 = CALC_APPLE2_ADDRESS(fa);
	f5 = (uint8_t)(((fa & 1) ? f9 : f8) << 2);
}
static uint8_t gm_pix() {             // FUN_0c70
	return (uint8_t)(gmBIT[fc] & s_screenmem[(uint16_t)(f14 + fb)]);
}
static uint8_t gm_rmask(uint8_t p) {  // FUN_0ca2 (sets f3)
	if ((p & 0x7f) == 0x7f) return 0xff;
	f3 = 7;
	uint8_t b;
	do { f3--; b = (uint8_t)(p & 0x7f); p = (uint8_t)(p << 1); } while (b >> 6);
	return gmRMASK[f3];
}
static uint8_t gm_lmask(uint8_t p) {  // FUN_0cb8 (sets fc)
	p &= 0x7f;
	if (p != 0x7f) {
		fc = 0xff;
		uint8_t c;
		do { fc++; c = (uint8_t)(p & 1); p = (uint8_t)(p >> 1); } while (c);
		return gmLMASK[fc];
	}
	return 0xff;
}
static void gm_write(uint8_t mask, uint8_t col) {  // FUN_0c7a
	f2 = (uint8_t)((mask ^ f2) & 0x7f);
	fd = mask;
	write_screen((uint16_t)(f14 + col),
		(uint8_t)((kPatternData[(col & 3) | f5] & mask) | f2));
}
static void gm_ebd() {                // FUN_0ebd
	fc--;
	if (fc & 0x80) { fc = 6; fb--; }
}
static void gm_enq(uint8_t dir) {     // FUN_0e80
	if (dir == 0) { if (fa == f12) return; }
	else { if (fa == f13) return; }
	uint8_t t = ff;
	Qy[t] = fa; Qcol[t] = fe; Qbit[t] = f3; Qbit2[t] = fc; Qcol2[t] = fb; Qdir[t] = dir;
	ff = (uint8_t)((ff + 1) & 0x1f);   // queue assumed never to overflow 32 spans
}

// FUN_0ec8: scan the maximal span containing (fb,fc) on row fa, painting it;
// returns its left edge in (fe,f3) and right edge in (fb,fc).
static void gm_scan() {
	uint8_t Y = fb;
	f3 = 0xff;
	f2 = s_screenmem[(uint16_t)(f14 + Y)];
	uint8_t mask = gm_rmask((uint8_t)(f2 | gmRMASK[fc]));   // sets f3
	fd = mask;
	mask = (uint8_t)(gmLMASK[fc] | f2);
	fc = 7;
	mask = gm_lmask(mask);                                  // sets fc
	mask = (uint8_t)(mask & fd);

	uint8_t rv;
	for (;;) {                                              // LAB_0eec
		gm_write(mask, Y);
		if (f3 != 0xff) { rv = f3; break; }
		Y = (uint8_t)(Y - 1);
		if (neg((uint8_t)(Y - f10))) { Y = (uint8_t)(Y + 1); rv = 0xff; break; }
		f2 = s_screenmem[(uint16_t)(f14 + Y)];
		mask = gm_rmask(f2);                               // sets f3
	}

	uint8_t X = (uint8_t)(rv + 1);                         // LAB_0f07
	if (X == 7) { X = 0; Y = (uint8_t)(Y + 1); }
	fe = Y; f3 = X;

	Y = fb;                                                // LAB_0f16 (right scan)
	for (;;) {
		if (fc == 7 && Y != f11) {
			Y = (uint8_t)(Y + 1);
			f2 = s_screenmem[(uint16_t)(f14 + Y)];
			uint8_t M = gm_lmask(f2);                      // sets fc
			gm_write(M, Y);
			continue;
		}
		fc = (uint8_t)(fc - 1);                            // LAB_0f2e
		if (fc & 0x80) { fc = 6; Y = (uint8_t)(Y - 1); }
		fb = Y;
		return;
	}
}

static void gm_fill_run() {           // op14_floodfill @ $0cca
	gm_setaddr();
	if (gm_pix() == 0) return;
	gm_scan();
	f4 = 0; ff = 0;
	gm_enq(0);
	gm_enq(1);

	long guard = 0;
	for (;;) {                                             // L_0ce7 (keyboard abort ignored)
		if (++guard > 2000000) return;                     // safety net against bugs
		uint8_t X = f4;
		if (X == ff) return;
		uint8_t Y = Qy[X];
		uint8_t d = Qdir[X];
		if (d == 0) Y = (uint8_t)(Y - 1);
		else if (d & 0x80) goto L_0d3e;
		else Y = (uint8_t)(Y + 1);
		fa = Y; fb = Qcol[X]; fc = Qbit[X];

		gm_setaddr();                                      // L_0d16
	L_0d19:
		if (gm_pix() != 0) goto L_0d49;
	L_0d1e:   // advance across the parent span looking for a set pixel
		{
			uint8_t XX = f4;
			if (fb == Qcol2[XX] && fc == Qbit2[XX]) goto L_0d3e;
			fc = (uint8_t)(fc + 1);
			if (fc != 7) goto L_0d19;
			fc = 0; fb = (uint8_t)(fb + 1);
			goto L_0d19;
		}

	L_0d49:
		{
			uint8_t XX = f4;                               // search the queue for an overlapping span
			for (;;) {
				XX = (uint8_t)(XX + 1);
				uint8_t idx = (uint8_t)(XX & 0x1f);
				if (idx == ff) goto L_0dfc;
				XX = idx;
				if (fa != Qy[XX]) continue;
				if (Qdir[f4] == Qdir[XX]) continue;
				if (neg((uint8_t)(fb - Qcol2[XX]))) goto L_0d7a;
				if (fb != Qcol2[XX]) continue;
				if (neg((uint8_t)(Qbit2[XX] - fc))) continue;
			L_0d7a:
				if (neg((uint8_t)(fb - Qcol[XX]))) continue;
				if (fb != Qcol[XX]) goto L_0d8a;
				if (neg((uint8_t)(fc - Qbit[XX]))) continue;
				if (fc == Qbit[XX]) goto L_0dc0;
			L_0d8a:
				fc = Qbit[f4]; fb = Qcol[f4];
				gm_ebd();
				if (neg((uint8_t)(fb - Qcol[XX]))) goto L_0dc0;
				if (fb != Qcol[XX]) goto L_0da7;
				if (!neg((uint8_t)(Qbit[XX] - fc))) goto L_0dc0;
			L_0da7:
				fe = Qcol[XX]; f3 = Qbit[XX];
				gm_ebd();
				{
					uint8_t dY = Qdir[XX];
					fd = XX;
					gm_enq(dY);
					XX = fd;
				}
			L_0dc0:
				fb = Qcol2[XX];
				if (neg((uint8_t)(Qcol2[XX] - Qcol2[f4]))) goto L_0dd4;
				if (Qcol2[XX] != Qcol2[f4]) goto L_0de1;
				if (neg((uint8_t)(Qbit2[f4] - Qbit2[XX]))) goto L_0de1;
			L_0dd4:
				Qdir[XX] = 0xff;
				fc = Qbit2[XX];
				goto L_0d1e_jump;
			L_0de1:
				Qcol[XX] = Qcol2[f4];
				{
					uint8_t b = (uint8_t)(Qbit2[f4] + 1);
					if (b == 7) { b = 0; Qcol[XX] = (uint8_t)(Qcol[XX] + 1); }
					Qbit[XX] = b;
				}
				goto L_0d3e;
			}
		}

	L_0dfc:
		gm_scan();
		{
			uint8_t dY = Qdir[f4];
			gm_enq(dY);
		}
		f2 = fb;        // save sub-span right col
		fd = fc;        // save sub-span right bit
		fb = Qcol[f4]; fc = Qbit[f4];
		gm_ebd();
		if (neg((uint8_t)(fb - fe))) goto L_0e39;
		if (fb != fe) goto L_0e2d;
		if (!neg((uint8_t)(f3 - fc))) goto L_0e39;
	L_0e2d:
		gm_ebd();
		gm_enq((uint8_t)(Qdir[f4] ^ 1));
	L_0e39:
		fc = fd;
		fb = f2;
		{
			fe = Qcol2[f4];
			uint8_t b = (uint8_t)(Qbit2[f4] + 1);
			if (b == 7) { fe = (uint8_t)(fe + 1); b = 0; }
			f3 = b;
		}
		if (neg((uint8_t)(fe - fb))) goto L_0e66;
		if (fe != fb) goto L_0d1e_jump;
		if (fc == f3) goto L_0d3e;
		if (neg((uint8_t)(fc - f3))) goto L_0d1e_jump;
	L_0e66:
		{
			uint8_t b = (uint8_t)(f3 + 1);
			if (b == 7) { fe = (uint8_t)(fe + 1); b = 0; }
			f3 = b;
			gm_enq((uint8_t)(Qdir[f4] ^ 1));
		}
		goto L_0d3e;

	L_0d1e_jump:
		// JMP $0d1e: re-enter the parent-span walk on the same row (f14 still
		// valid), which checks the right edge, advances, then re-tests.
		goto L_0d1e;

	L_0d3e:
		f4 = (uint8_t)((f4 + 1) & 0x1f);
		continue;
	}
}

static void apple2_flood_fill(uint16_t x, uint8_t y, uint8_t pat_even, uint8_t pat_odd,
		uint8_t left, uint8_t right, uint8_t top, uint8_t bottom) {
	f8 = pat_even; f9 = pat_odd;
	f10 = left; f11 = right; f12 = top; f13 = bottom;
	fa = (uint8_t)y;
	fb = (uint8_t)(x / COL_BITS);   // FUN_0c52: col = x/7
	fc = (uint8_t)(x % COL_BITS);   //           bit = x%7
	gm_fill_run();
}

// ---- Shape / brush drawing (ported) -------------------------------------------

// One 8-row brush quadrant, faithful to the Apple II standard-hires routine
// FUN_102e ($102e): for each row, spread the brush byte across two adjacent
// hi-res bytes by a 7-bit rotate (collecting bit 6 into the right byte), then
// blend it into the screen using the current fill colour pattern.
static void draw_bitmap(a2_brush_ctx *ctx) {
	for (int row = 0; row < 8; row++) {
		uint16_t address = CALC_APPLE2_ADDRESS(ctx->scanline) + ctx->seed_column;
		int idx = ctx->bitmap_index + row;
		if (idx < (int)sizeof(kBrushBitmaps) && valid_offset(address)) {
			uint8_t b = kBrushBitmaps[idx];
			if (b != 0) {
				// 7-bit spread by the pixel offset within the column.
				uint8_t carry = 0;
				for (uint8_t n = ctx->pixel_offset; n != 0; n--) {
					carry = (uint8_t)((carry << 1) | ((b & 0x7f) >> 6));
					b = (uint8_t)(b << 1);
				}
				uint8_t mask_a = b & 0x7f;   // bits painted into column `col`
				uint8_t mask_b = carry;      // bits carried into column `col+1`

				uint8_t pat_base = ((ctx->scanline & 1) ? ctx->pattern_odd
				                                        : ctx->pattern_even) * 4;
				uint8_t pat0 = kPatternData[(ctx->seed_column & 3) | pat_base];
				uint8_t pat1 = kPatternData[((ctx->seed_column + 1) & 3) | pat_base];

				if (mask_a != 0)
					write_screen(address,
						((mask_a ^ 0x7f) & s_screenmem[address]) | ((mask_a | 0x80) & pat0));
				if (mask_b != 0 && valid_offset(address + 1))
					write_screen(address + 1,
						((mask_b ^ 0x7f) & s_screenmem[address + 1]) | ((mask_b | 0x80) & pat1));
			}
		}
		ctx->scanline++;
	}
}

#define BRUSH_PART_SIZE 8
#define BRUSH_TOTAL_PARTS 4
#define BRUSH_PART_HEIGHT 8

static void draw_brush_quadrant(a2_brush_ctx *ctx, int column_offset, int line_offset) {
	ctx->seed_column += column_offset;
	ctx->scanline += line_offset;
	draw_bitmap(ctx);
	ctx->seed_column -= column_offset;
	ctx->scanline -= line_offset + BRUSH_PART_HEIGHT;
}

static void draw_brush(uint16_t x, uint8_t y, uint8_t brush, uint8_t pat_even, uint8_t pat_odd) {
	a2_brush_ctx ctx = {};
	ctx.scanline = y;
	ctx.brush_type = brush;
	ctx.pattern_even = pat_even;
	ctx.pattern_odd = pat_odd;
	x_to_column_and_pixel(&ctx, x);
	ctx.bitmap_index = ctx.brush_type * BRUSH_PART_SIZE * BRUSH_TOTAL_PARTS;
	if (ctx.bitmap_index + BRUSH_PART_SIZE * BRUSH_TOTAL_PARTS > (int)sizeof(kBrushBitmaps))
		return;
	draw_brush_quadrant(&ctx, 0, 0);
	ctx.bitmap_index += BRUSH_PART_SIZE;
	draw_brush_quadrant(&ctx, 1, 0);
	ctx.bitmap_index += BRUSH_PART_SIZE;
	draw_brush_quadrant(&ctx, 0, BRUSH_PART_HEIGHT);
	ctx.bitmap_index += BRUSH_PART_SIZE;
	draw_brush_quadrant(&ctx, 1, BRUSH_PART_HEIGHT);
}

// ---- Line drawing (ported) ----------------------------------------------------

static void move_left(a2_ctx *ctx) {
	if ((ctx->HMASK & 1) == 0) {
		ctx->HMASK = ctx->HMASK >> 1 ^ 0xc0;
		return;
	}
	ctx->HGR_HORIZ--;
	if ((int8_t)ctx->HGR_HORIZ < 0)
		ctx->HGR_HORIZ = 39;
	ctx->HMASK = 0xc0;
	if ((ctx->HGR_BITS * 2 + 0x40) & 0x80)
		ctx->HGR_BITS ^= 0x7f;
}

static void move_right(a2_ctx *ctx) {
	ctx->HMASK = ctx->HMASK << 1 ^ 0x80;
	if ((int8_t)ctx->HMASK < 0)
		return;
	ctx->HMASK = 0x81;
	ctx->HGR_HORIZ++;
	if (ctx->HGR_HORIZ >= 40)
		ctx->HGR_HORIZ = 0;
	if ((ctx->HGR_BITS * 2 + 0x40) & 0x80)
		ctx->HGR_BITS ^= 0x7f;
}

static void move_up_or_down(bool down, a2_ctx *ctx) {
	if (down) {
		ctx->HGR_Y++;
		if (ctx->HGR_Y > 191) ctx->HGR_Y = 0;
	} else {
		ctx->HGR_Y--;
		// Note:`(int8_t)HGR_Y < 0 will wrongly fire for every valid row 128..191.
		if (ctx->HGR_Y > 191) ctx->HGR_Y = 191;
	}
	ctx->HBAS = CALC_APPLE2_ADDRESS(ctx->HGR_Y);
}

static void move_left_or_right(bool left, a2_ctx *ctx) {
	if (left) move_left(ctx); else move_right(ctx);
}

static void plot_pixel(a2_ctx *ctx) {
	uint16_t offset = ctx->HBAS + ctx->HGR_HORIZ;
	if (offset > MAX_SCREEN_ADDR)
		return;
	write_screen(offset, ((s_screenmem[offset] ^ ctx->HGR_BITS) & ctx->HMASK) ^ s_screenmem[offset]);
}

#define CARRY_THRESHOLD 0x100

static void draw_line(uint16_t target_x, uint8_t target_y, a2_ctx *ctx) {
	const bool move_down = target_y > ctx->HGR_Y;
	const bool mleft = target_x < ctx->HGR_X;
	const uint16_t delta_x = (target_x > ctx->HGR_X) ? (target_x - ctx->HGR_X) : (ctx->HGR_X - target_x);
	const uint8_t delta_y = (target_y > ctx->HGR_Y) ? (target_y - ctx->HGR_Y) : (ctx->HGR_Y - target_y);
	plot_pixel(ctx);
	const int total_steps = delta_x + delta_y;
	if (total_steps == 0) return;
	const int minus_delta_y = (uint8_t)(~delta_y) + 1;
	bool carry = (uint8_t)delta_x + minus_delta_y < CARRY_THRESHOLD;
	bool moving_vertically = (delta_x >> 8) < carry;
	uint16_t error_term = delta_x + minus_delta_y - CARRY_THRESHOLD;
	for (int i = 0; i < total_steps; i++) {
		if (moving_vertically) {
			move_up_or_down(move_down, ctx);
			moving_vertically = error_term + delta_x < CARRY_THRESHOLD << 8;
			error_term += delta_x;
		} else {
			move_left_or_right(mleft, ctx);
			uint16_t nocarry = ((uint8_t)error_term + minus_delta_y < CARRY_THRESHOLD) << 8;
			moving_vertically = error_term < nocarry;
			uint8_t e_low = (uint8_t)error_term + minus_delta_y;
			error_term = ((error_term - nocarry) & 0xff00) | e_low;
		}
		plot_pixel(ctx);
	}
	ctx->HGR_X = target_x;
}

// ---- Circle (op11) -- faithful port of the Apple II routine $0ad3 -------------
// A midpoint circle plotting 8 symmetric points per step via HPLOT (HPOSN +
// plot). The point clip matches FUN_0bf3 ($0bf3): only x in 0..279, y in 0..159.
// The original saves/restores zero page around this ($0855/$0861); we use locals.

static void plot_circle_point(int x, int y, a2_ctx *ctx) {
	if (x < 0 || x >= 280 || y < 0 || y >= APPLE2_SCREEN_HEIGHT - 32)
		return;                       // FUN_0bf3: x<280 (x-high 0, or 1 && low<0x18), y<0xA0
	set_draw_position((uint16_t)x, (uint8_t)y, ctx);
	plot_pixel(ctx);
}

static void draw_circle(uint16_t cx, uint8_t cy, uint8_t radius, a2_ctx *ctx) {
	if (radius == 0)
		return;
	uint8_t r = radius;               // DAT_0002
	uint8_t dvar = (uint8_t)(r >> 1); // DAT_0003 (decision variable)
	uint8_t i = 0;                    // DAT_0004
	int xa = cx, xb = cx;             // DAT_0010 (+1), DAT_0014 (-1)
	int xr = (int)cx + r, xl = (int)cx - r;   // DAT_000e (-1), DAT_0012 (+1)
	int yt = cy, yb = cy;             // DAT_0007 (+1), DAT_000b (-1)
	int yp = (int)cy + r, ym = (int)cy - r;   // DAT_0005 (-1), DAT_0009 (+1)

	for (;;) {
		plot_circle_point(xb, ym, ctx);
		plot_circle_point(xa, ym, ctx);
		plot_circle_point(xa, yp, ctx);
		plot_circle_point(xb, yp, ctx);
		plot_circle_point(xr, yb, ctx);
		plot_circle_point(xl, yb, ctx);
		plot_circle_point(xl, yt, ctx);
		plot_circle_point(xr, yt, ctx);

		i++;
		uint8_t a = (uint8_t)(dvar - i);   // SBC dvar - i ($0b92)
		if (dvar >= i) {                   // BCS ($0b94): carry set -> inner extents only
			dvar = a;
			if (r < i)                     // CMP $02,$04; BCC done
				break;
		} else {                           // carry clear (borrow): step radius in + outer extents
			r--;
			dvar = (uint8_t)(a + r);       // ADC $02 (carry clear)
			if (r < i)
				break;
			yp--; ym++; xr--; xl++;        // $0ba2-$0bc1
		}
		xa++; xb--; yt++; yb--;            // $0bc2-$0be1 (always)
	}
}

// ---- op15 background rectangle fill (Talisman variant) ------------------------

static void fill_rect_pattern(a2_ctx *ctx, uint8_t pat_even, uint8_t pat_odd) {
	for (int row = ctx->fill_top; row <= ctx->fill_bottom && row < APPLE2_SCREEN_HEIGHT; row++) {
		uint16_t base = CALC_APPLE2_ADDRESS(row);
		uint8_t pat_base = ((row & 1) ? pat_odd : pat_even) * 4;
		for (int col = ctx->fill_left; col <= ctx->fill_right && col < APPLE2_SCREEN_COLS; col++)
			write_screen(base + col, kPatternData[(col & 3) | pat_base]);
	}
}

// ---- Opcode interpreter (Talisman variant) ------------------------------------

static bool doImageOp(const uint8_t **outptr, const uint8_t *end, a2_ctx *ctx) {
	const uint8_t *ptr = *outptr;
	if (ptr >= end) return true;
#ifdef TALISMAN_TRACE
	if (g_talismanOnOp)
		g_talismanOnOp((int)(ptr - g_imgBase), ptr[0],
			(ptr + 1 < end) ? ptr[1] : 0, (ptr + 2 < end) ? ptr[2] : 0,
			ctx->HGR_X, ctx->HGR_Y);
#endif
	uint8_t opcode = *ptr++;
	uint8_t param = opcode & 0xf;
	opcode >>= 4;
#ifdef TALISMAN_TRACE
	{
		// One tag per primitive (op >> 4), matching test/pixtest's opName order.
		static const char kPrim[16] = {'.','t','p','c','s','o','f','.',
		                               'm','B','L','C','S','d','F','R'};
		g_currentPrim = kPrim[opcode & 0xf];
	}
#endif
	uint16_t a, b;

	switch (opcode) {
	case OPCODE_END: // 0 -- only op0 ends a Talisman image
		*outptr = ptr;
		return true;

	case OPCODE_END2: // 7 -- Talisman: 2-operand no-op (1984: end)
		ptr += 2;
		break;

	case OPCODE_SET_TEXT_POS: // 1
		ptr += 2;
		break;

	case OPCODE_SET_PEN_COLOR: // 2
		set_color(param, ctx);
		break;

	case OPCODE_TEXT_OUTLINE: // 5
	case OPCODE_TEXT_CHAR: // 3
		ptr += 1;
		break;

	case OPCODE_SET_SHAPE: // 4
		ctx->BRUSH = param;
		break;

	case OPCODE_SET_FILL_COLOR: { // 6
		uint8_t idx = *ptr++;
		ctx->pat_even = kColorPatternSubindices[(uint8_t)(idx * 2)];
		ctx->pat_odd = kColorPatternSubindices[(uint8_t)(idx * 2 + 1)];
		break;
	}

	case OPCODE_MOVE_TO: // 8
		a = *ptr++ + (param & 1 ? 256 : 0);
		b = *ptr++;
		set_draw_position(a, b, ctx);
		break;

	case OPCODE_DRAW_BOX: { // 9 -- rectangle, byte-faithful to op9 ($095b)
		a = *ptr++ + (param & 1 ? 256 : 0);
		b = *ptr++;
		uint16_t x0 = ctx->HGR_X;
		uint8_t y0 = ctx->HGR_Y;
		// Four chained HLINEs around the rectangle (the four JSR $F53A calls).
		draw_line(x0, b, ctx);   // left edge   (x0,y0)->(x0,b)
		draw_line(a, b, ctx);    // bottom edge ->(a,b)
		draw_line(a, y0, ctx);   // right edge  ->(a,y0)
		draw_line(x0, y0, ctx);  // top edge    ->(x0,y0)
		// op9 then falls through to $0990, which HPOSNs to the far corner, so the
		// pen is left at (a,b) -- NOT the start corner. A following DRAW_LINE
		// continues from there; omitting this shifted the next line by a few
		// pixels (e.g. Talisman RA #1: 11 wrong bytes along one diagonal edge).
		set_draw_position(a, b, ctx);
		break;
	}

	case OPCODE_DRAW_LINE: // 10
		a = *ptr++ + (param & 1 ? 256 : 0);
		b = *ptr++;
		draw_line(a, b, ctx);
		break;

	case OPCODE_DRAW_CIRCLE: { // 11 -- circle centred on the current pen position
		uint8_t radius = *ptr++;
		uint16_t ccx = ctx->HGR_X;
		uint8_t ccy = ctx->HGR_Y;
		draw_circle(ccx, ccy, radius, ctx);
		set_draw_position(ccx, ccy, ctx);   // op11 restores the centre ($0944->$0990)
		break;
	}

	case OPCODE_DRAW_SHAPE: // 12
		a = *ptr++ + (param & 1 ? 256 : 0);
		b = *ptr++;
		draw_brush(a, b, ctx->BRUSH, ctx->pat_even, ctx->pat_odd);
		break;

	case OPCODE_DELAY: // 13
		ptr += 1;
		break;

	case OPCODE_PAINT: // 14
		a = *ptr++ + (param & 1 ? 256 : 0);
		b = *ptr++;
		apple2_flood_fill(a, b, ctx->pat_even, ctx->pat_odd,
			ctx->fill_left, ctx->fill_right, ctx->fill_top, ctx->fill_bottom);
		break;

	case OPCODE_RESET: // 15 -- Talisman: sub-op in low nibble
		switch (param) {
		case 1: // bounds = full screen
			ctx->fill_left = 0; ctx->fill_top = 0;
			ctx->fill_right = 39; ctx->fill_bottom = 0x9f;
			break;
		case 2: // read 4 bytes of bounds ($0A56: $11,$10,$13,$12)
			ctx->fill_right = *ptr++;
			ctx->fill_left = *ptr++;
			ctx->fill_bottom = *ptr++;
			ctx->fill_top = *ptr++;
			break;
		default: // param 3 (and 0): fill the rectangle with the current pattern
			fill_rect_pattern(ctx, ctx->pat_even, ctx->pat_odd);
			break;
		}
		break;
	}
	*outptr = ptr;
	return false;
}

// ---- Apple hi-res -> RGB (NTSC artifact colours, from apple2draw.c) -----------

static unsigned rotl4b(unsigned n, unsigned count) { return (n >> (-count & 3)) & 0x0f; }

static const uint8_t artifact_color_lut[128] = {
	0x00,0x00,0x00,0x00,0x88,0x00,0xcc,0x00,0x11,0x11,0x55,0x11,0x99,0x99,0xdd,0xff,
	0x22,0x22,0x66,0x66,0xaa,0xaa,0xee,0xee,0x33,0x33,0x33,0x33,0xbb,0xBB,0xff,0xff,
	0x00,0x00,0x44,0x44,0xcc,0xcc,0xcc,0xcc,0x55,0x55,0x55,0x55,0x99,0x99,0xdd,0xff,
	0x66,0x22,0x66,0x66,0xee,0xaa,0xee,0xee,0x77,0x77,0x77,0x77,0xff,0xFF,0xff,0xff,
	0x00,0x00,0x00,0x00,0x88,0x88,0x88,0x88,0x11,0x11,0x55,0x11,0x99,0x99,0xdd,0x99,
	0x00,0x22,0x66,0x66,0xaa,0xaa,0xaa,0xaa,0x33,0x33,0x33,0x33,0xbb,0xBB,0xff,0xff,
	0x00,0x00,0x44,0x44,0xcc,0xcc,0xcc,0xcc,0x11,0x11,0x55,0x55,0x99,0x99,0xdd,0xdd,
	0x00,0x22,0x66,0x66,0xee,0xaa,0xee,0xee,0xff,0x33,0xff,0x77,0xff,0xFF,0xff,0xff,
};

static const uint32_t apple2_palette[16] = {
	0x000000, 0xa70b40, 0x401cf7, 0xe628ff, 0x007440, 0x808080, 0x1990ff, 0xbf9cff,
	0x406300, 0xe66f00, 0x808080, 0xff8bbf, 0x19d700, 0xbfe308, 0x58f4bf, 0xffffff,
};

#define CONTEXTBITS 3

// Render one Apple scanline into colors560[0..559] (4-bit palette indices).
static void render_line_colors(const uint16_t *in, uint8_t *colors560) {
	uint32_t w = 0;
	w += in[0] << CONTEXTBITS;
	for (int col = 0; col < 40; col++) {
		if (col < 39)
			w += in[col + 1] << (14 + CONTEXTBITS);
		for (int b = 0; b < 14; b++) {
			uint8_t color = (uint8_t)rotl4b(artifact_color_lut[w & 0x7f], col * 14 + b);
			colors560[col * 14 + b] = color;
			w >>= 1;
		}
	}
}

static uint16_t Double7Bits(int i) {
	static uint16_t table[128];
	static bool init = false;
	if (!init) {
		for (unsigned k = 1; k < 128; k++)
			table[k] = table[k >> 1] * 4 + (k & 1) * 3;
		init = true;
	}
	return (i > 127) ? 0 : table[i];
}

// Convert s_screenmem -> RGBA into out[w*h] (0xRRGGBBAA). Renders Apple rows
// 0..h-1, sampling the 560-wide artifact colours down to w columns.
static void screenmem_to_rgba(uint32_t *out, int w, int h) {
	for (int row = 0; row < h; row++) {
		unsigned const address = (((row / 8) & 0x07) << 7) + (((row / 8) & 0x18) * 5) + ((row & 7) << 10);
		const uint8_t *vram_row = s_screenmem + address;
		uint16_t words[40];
		unsigned last_output_bit = 0;
		for (int col = 0; col < 40; col++) {
			unsigned word = Double7Bits(vram_row[col] & 0x7f);
			if (vram_row[col] & 0x80)
				word = (word * 2 + last_output_bit) & 0x3fff;
			words[col] = word;
			last_output_bit = word >> 13;
		}
		uint8_t colors560[560];
		render_line_colors(words, colors560);
		for (int x = 0; x < w; x++) {
			uint32_t rgb = apple2_palette[colors560[(2 * x < 560) ? 2 * x : 559] & 0x0f];
			out[row * w + x] = (rgb << 8) | 0xff;
		}
	}
}

// ---- Public entry -------------------------------------------------------------

void talismanResetScreen(bool white) {
	memset(s_screenmem, white ? 0xff : 0x00, A2_SCREEN_MEM_SIZE);
}

// Diagnostic accessor (offline pixel-diff harness): the raw 0x2000-byte page.
const uint8_t *talismanPagePtr() { return s_screenmem; }
void talismanSetPage(const uint8_t *p) { memcpy(s_screenmem, p, A2_SCREEN_MEM_SIZE); }

void talismanDrawImage(const uint8_t *data, size_t size) {
#ifdef TALISMAN_TRACE
	g_imgBase = data;
#endif
	a2_ctx ctx = {};
	set_color(4, &ctx);
	set_draw_position(140, 96, &ctx);
	// Default fill colour = idx $4d (the picture interpreter's init at $08a6),
	// which FUN_0c92 maps to the $1132 even/odd sub-index pair.
	ctx.pat_even = kColorPatternSubindices[0x4d * 2];
	ctx.pat_odd = kColorPatternSubindices[0x4d * 2 + 1];
	ctx.BRUSH = 5;
	ctx.fill_left = 0; ctx.fill_top = 0; ctx.fill_right = 39; ctx.fill_bottom = 0x9f;

	const uint8_t *ptr = data;
	const uint8_t *end = data + size;
	bool done = false;
	int guard = 0;
	while (!done && ptr < end && guard++ < 100000)
		done = doImageOp(&ptr, end, &ctx);
}

void talismanBlitToSurface(uint32_t *out, int w, int h) {
	screenmem_to_rgba(out, w, h);
}

} // namespace Comprehend
} // namespace Glk
