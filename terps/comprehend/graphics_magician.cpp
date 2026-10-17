/* graphics_magician.cpp -- pixel-faithful renderer for the Apple II Comprehend
 * games (Transylvania, OO-Topos, Crimson Crown, Talisman), STANDARD hi-res mode.
 *
 * Their room/item/title pictures are drawn with Penguin Software's "Graphics
 * Magician" / "Picture Painter" vector format -- the SAME engine the Apple II
 * Scott Adams games use. The drawing primitives render into a real Apple hi-res
 * framebuffer (s_screenmem); a second pass converts that to RGBA using the
 * Apple NTSC artifact-colour algorithm (from terps/common_sagadraw/apple2draw.c).
 *
 * Each primitive here was transliterated from, and verified byte-for-byte
 * against, the games' own 6502 routines (Ghidra on a MAME RAM dump, plus running
 * the real ROM routines on identical inputs and diffing the resulting hi-res
 * page): line = Applesoft HLINE ($F53A); box = four HLINEs ($095B); circle =
 * midpoint circle ($0AD3); brush = $0FFB/$102E; flood fill = the queue-based
 * span fill ($0CCA); background fill = op15 sub-op3 ($0A2D); delay = the $09BB
 * counting loop. Output matches the hardware byte-exactly over every picture row
 * in the regression set (test/test_talisman_pics.cpp).
 *
 * This is the STANDARD hi-res renderer (the games' <S> boot option, where
 * offered); the canonical double hi-res renderer (<D>) is a separate, much
 * larger porting effort and is not used here.
 *
 * The four titles share one engine but in two dialects, selected by
 * gmSetLegacyFormat() (verified against MAME + the disassembly at
 * https://6502disassembly.com/a2-graphics-magician/PICDRAWH_1984.html):
 *   - Talisman and OO-Topos use the later variant: op7 (0x7x) is a 2-operand
 *     NO-OP and op15 (0xf, "RESET") is a low-nibble sub-op (param1 sets the fill
 *     rectangle to full screen, param2 reads 4 bytes of bounds, param3 fills the
 *     rectangle with the current colour pattern -- the per-image background).
 *   - Transylvania and Crimson Crown use the older variant (the legacy flag):
 *     op7 and op15 end the image, and fills clip to the full screen (their
 *     background PAINT seeds in the bottom text rows).
 *
 * The colour/pattern tables (s_patternData, s_colorPatternSubindices,
 * s_brushBitmaps) are not hard-coded: they are pulled at runtime out of the
 * Graphics Magician code file "T2" on the boot disk by gmInstallDrawingTables()
 * (located by signature, since each release places them at a different offset).
 * apple2.cpp calls it once the disks are mounted; the regression test loads each
 * game's captured T2 fixture.
 */

#include "graphics_magician.h"
#include "gm_vector.h"
#include "gm_artifact.h"
#include "slow_draw_page.h"     // shared progressive-reveal helper
#include "gm_span_fill.h"       // shared op14 flood fill
#include <cstring>

namespace Glk {
namespace Comprehend {

// ---- Drawing tables ----------------------------------------------------------
// Populated by gmInstallDrawingTables() from the boot disk's T2 file.
// Until then they stay zero, which renders nothing (matches the behaviour of an
// Apple II that hasn't loaded the Graphics Magician code yet).

static uint8_t s_patternData[120];              // 30 colour patterns x 4 bytes
static uint8_t s_colorPatternSubindices[216];   // 108 fills x (even,odd) index
static uint8_t s_brushBitmaps[256];             // 8 brushes x 4 quadrants x 8
static uint8_t s_fontGlyphs[128 * 8];           // op3/op5 picture font, 8 bytes/char

// T2 is a headerless ProDOS BIN that loads at $0800. The three drawing tables
// live at fixed absolute addresses inside it (matches the MAME RAM dump):
//   $1132 -> s_colorPatternSubindices (216 bytes)
//   $120A -> s_patternData            (120 bytes)
//   $1282 -> s_brushBitmaps           (256 bytes)
bool gmInstallDrawingTables(const uint8_t *t2, size_t size) {
	if (!t2)
		return false;

	// The three drawing tables are byte-identical across every Graphics Magician
	// title we support (Talisman, Transylvania, ...) and sit contiguously as
	//   [ colour subindices : 216 ][ pattern data : 120 ][ brush bitmaps : 256 ]
	// but at a load-address-dependent offset inside T2 (Talisman puts pattern
	// data at $120a, Transylvania at $1278). Rather than hard-code per-game
	// offsets, locate the pattern-data table by its unmistakable signature -- the
	// Apple hi-res colour patterns black/green-violet/white/orange-blue -- and
	// derive the neighbours from it. pattern_data[0..3] are zero, so the run that
	// actually identifies it starts at byte 4.
	static const uint8_t kPatternSig[] = {
		0x55,0x2a,0x55,0x2a, 0x2a,0x55,0x2a,0x55, 0x7f,0x7f,0x7f,0x7f,
		0x80,0x80,0x80,0x80, 0xd5,0xaa,0xd5,0xaa, 0xaa,0xd5,0xaa,0xd5,
		0xff,0xff,0xff,0xff,
	};
	const size_t kSubLen   = sizeof(s_colorPatternSubindices); // 216
	const size_t kPatLen   = sizeof(s_patternData);            // 120
	const size_t kBrushLen = sizeof(s_brushBitmaps);           // 256

	size_t sig = SIZE_MAX;
	if (size >= sizeof(kPatternSig)) {
		for (size_t i = 0; i + sizeof(kPatternSig) <= size; i++) {
			if (std::memcmp(t2 + i, kPatternSig, sizeof(kPatternSig)) == 0) {
				sig = i;
				break;
			}
		}
	}
	if (sig == SIZE_MAX || sig < 4)
		return false;
	size_t patBase = sig - 4;          // pattern_data[0..3] == 0 precede the sig
	if (patBase < kSubLen)             // need room for the subindices before it
		return false;
	size_t subBase   = patBase - kSubLen;
	size_t brushBase = patBase + kPatLen;
	if (brushBase + kBrushLen > size)
		return false;

	std::memcpy(s_colorPatternSubindices, t2 + subBase,   kSubLen);
	std::memcpy(s_patternData,            t2 + patBase,   kPatLen);
	std::memcpy(s_brushBitmaps,           t2 + brushBase, kBrushLen);

	// The op3/op5 picture font shares the brush table's base: glyph(ch) lives at
	// brushBase + ch*8 (so the brush bitmaps double as the unused control-char
	// glyphs $00..$1f, and the printable glyphs follow). Copy the whole 128-char
	// range; leave it zero (blank text) if T2 ends early rather than failing the
	// critical tables above.
	if (brushBase + sizeof(s_fontGlyphs) <= size)
		std::memcpy(s_fontGlyphs, t2 + brushBase, sizeof(s_fontGlyphs));
	return true;
}

// ---- Apple hi-res framebuffer -------------------------------------------------

#define A2_SCREEN_MEM_SIZE   GM_HIRES_PAGE_SIZE
#define APPLE2_SCREEN_HEIGHT GM_HIRES_ROWS
#define APPLE2_SCREEN_COLS   GM_HIRES_COLS
#define RIGHT_EXPAND_MASK 0x40


// Persistent across calls: item / overlay images are drawn straight on top of
// the room already rendered into the Apple hi-res page (the real interpreter
// never clears the screen for an item picture), so callers reset it explicitly
// via gmResetScreen() only when starting a fresh room.
static uint8_t s_screenmem[A2_SCREEN_MEM_SIZE];

static inline bool valid_offset(uint16_t off) { return off <= GM_MAX_SCREEN_ADDR; }

// ---- Slow ("animated") draw -------------------------------------------------
//
// Like the Apple II Scott Adams and Level 9 renderers, the picture can be
// revealed progressively in the order the interpreter actually plotted it,
// imitating the few seconds the real Graphics Magician took to paint a hi-res
// page. When recording is enabled (gmSetSlowDraw(true)), every hi-res
// byte written goes both into the final page (s_screenmem, as before) and onto
// an ordered op list. The visible page (s_slowScreen) starts at the background
// and the host re-applies the ops a chunk at a time on a Glk timer, re-blitting
// after each chunk. Item overlays start from whatever is already on s_slowScreen
// (the room just finished), so they compose exactly as on the real machine.

// The reveal state machine, op list and dirty band live in the shared
// SlowDrawEngine (slow_draw_page.h); the Apple II policy below supplies the only
// renderer-specific behaviour -- the hi-res offset -> row mapping for the dirty
// band.  s_screenmem is the fully-drawn page; s_slowScreen is the page the host
// progressively reveals (and onto which CM-panel overlays composite directly).
static uint8_t s_slowScreen[A2_SCREEN_MEM_SIZE]; // progressively-revealed page

// offset -> screen row (0..191), or 0xff for the inter-row gaps in the hi-res
// address space; offset -> byte column (0..39), or 0xff in the gaps. Built once;
// lets a reveal map a byte write back to its row (dirty band) and column (so the
// CM panel columns can be excluded from the reveal -- see apple_apply_slow).
static uint8_t s_rowOfOffset[A2_SCREEN_MEM_SIZE];
static uint8_t s_colOfOffset[A2_SCREEN_MEM_SIZE];
static bool s_rowTableInit = false;

static void init_offset_tables() {
	if (s_rowTableInit)
		return;
	memset(s_rowOfOffset, 0xff, sizeof(s_rowOfOffset));
	memset(s_colOfOffset, 0xff, sizeof(s_colOfOffset));
	for (int row = 0; row < 192; row++) {
		uint16_t base = gm_row_address(row);
		for (int col = 0; col < 40; col++) {
			s_rowOfOffset[base + col] = (uint8_t)row;
			s_colOfOffset[base + col] = (uint8_t)col;
		}
	}
	s_rowTableInit = true;
}

static int row_of_offset(uint16_t offset) {
	init_offset_tables();
	uint8_t r = s_rowOfOffset[offset];
	return (r == 0xff) ? -1 : (int)r;        // -1 = inter-row gap, skip
}

static int col_of_offset(uint16_t offset) {
	init_offset_tables();
	uint8_t c = s_colOfOffset[offset];
	return (c == 0xff) ? -1 : (int)c;        // -1 = inter-row gap, skip
}

// Apply one recorded byte to the slow page; defined after the CM-panel state it
// consults (forward-declared here for the policy).
static bool apple_apply_slow(const SlowWriteOp &op);

// Apple II hi-res page: one flat buffer, but rows are address-interleaved, so
// the dirty band maps offsets through row_of_offset (not offset/stride).
struct AppleSlowPolicy {
	bool apply(const SlowWriteOp &op) { return apple_apply_slow(op); }
	int rowOf(const SlowWriteOp &op) const { return row_of_offset(op.offset); }
	bool adjacent(const SlowWriteOp &a, const SlowWriteOp &b) const {
		return ((int)b.offset >= (int)a.offset - 1 && (int)b.offset <= (int)a.offset + 1) ||
			   b.value == a.value;
	}
	void sync() { memcpy(s_slowScreen, s_screenmem, A2_SCREEN_MEM_SIZE); }
};
static AppleSlowPolicy s_slowPolicy;
static SlowDrawEngine<AppleSlowPolicy> s_slow(s_slowPolicy);

#ifdef GM_TRACE
// Diagnostic hooks for the offline pixtest harness (built with -DGM_TRACE,
// see test/pixtest.cpp). g_gmWriteLog fires for every hi-res byte written,
// tagged with the current drawing primitive; g_gmOnOp fires once per
// opcode, before it draws, with the opcode's stream position, raw bytes and pen.
// These are absent from the normal (non-traced) interpreter build.
void (*g_gmWriteLog)(uint16_t, uint8_t, char) = nullptr;
void (*g_gmOnOp)(int pos, int op, int b1, int b2, int x, int y) = nullptr;
static char g_currentPrim = '?';
static const uint8_t *g_imgBase = nullptr;
#endif

static void write_screen(uint16_t offset, uint8_t value) {
	if (valid_offset(offset)) {
		s_screenmem[offset] = value;
		s_slow.record(offset, 0, value);
#ifdef GM_TRACE
		if (g_gmWriteLog)
			g_gmWriteLog(offset, value, g_currentPrim);
#endif
	}
}

// Write hook handed to the shared gm_vector line primitives.
static void gm_write_adapter(uint16_t offset, uint8_t value, void *user) {
	(void)user;
	write_screen(offset, value);
}

// ---- Drawing context ----------------------------------------------------------
// (Opcode enum is shared via graphics_magician.h.)

typedef struct {
	gm_vector_ctx gv;      // shared cursor/colour + line-drawing state
	uint8_t BRUSH;         // current shape/brush type
	// current fill colour as even/odd-row pattern sub-indices ($08/$09),
	// set by op6 SET_FILL_COLOR; default (3,3) = white.
	uint8_t pat_even, pat_odd;
	// op15 fill rectangle bounds (column 0..39, row 0..191)
	uint8_t fill_left, fill_right, fill_top, fill_bottom;
	// op1 text cursor; op3/op5 draw a glyph here and advance text_x by 7
	uint16_t text_x;
	uint8_t text_y;
} a2_ctx;

// set_color / set_draw_position / draw_brush now live in
// common_sagadraw/gm_vector.c.

// ---- Flood fill ---------------------------------------------------------------
//
// op14 PAINT. The span-fill engine is shared with the double hi-res renderer
// (gm_span_fill.h); this is the standard hi-res page it runs on: one byte per
// column, patterned from a 4-byte-wide dither selected by row parity.

struct AppleFillPage {
	static const bool kDropSinglePixelSpans = false;

	uint8_t  patEven, patOdd;   // fill-pattern sub-indices (even/odd row)
	uint16_t rowAddr;           // hi-res base address of the current row
	uint8_t  patBase;           // fill-pattern table base for the current row

	// Odd rows draw with the odd fill sub-index, even rows with the even one.
	void setRow(uint8_t row) {
		rowAddr = gm_row_address(row);
		patBase = static_cast<uint8_t>(((row & 1) ? patOdd : patEven) << 2);
	}
	uint8_t read(int col) const { return s_screenmem[rowAddr + col]; }
	void write(int col, uint8_t v) { write_screen(static_cast<uint16_t>(rowAddr + col), v); }
	uint8_t pattern(int col) const { return s_patternData[(col & 3) | patBase]; }
};

static AppleFillPage s_fillPage;
static GmSpanFill<AppleFillPage> s_spanFill(s_fillPage);

// Flood-fill entry point: seed point (x,y), even/odd fill patterns and the clip
// rectangle (left/right columns, top/bottom rows).
static void apple2_flood_fill(uint16_t x, uint8_t y, uint8_t pat_even, uint8_t pat_odd,
		uint8_t left, uint8_t right, uint8_t top, uint8_t bottom) {
	s_fillPage.patEven = pat_even;
	s_fillPage.patOdd  = pat_odd;
	s_spanFill.fill(x, y, left, right, top, bottom);   // x == column*7 + bit
}

// ---- Text glyph drawing (op3 TEXT_CHAR / op5 TEXT_OUTLINE) ---------------------
//
// The picture interpreter draws characters through the same per-row blitter as
// brushes (FUN_102e), reading an 8-byte hi-res glyph from the font table that
// shares the brush table's base: glyph(ch) = $1282 + ch*8. The byte is spread
// across two adjacent columns by the pixel offset within the column (bit 6
// collected into the next column), exactly like a brush quadrant.
//
// op3 (TEXT_CHAR) sets the "normal" flag ($0839=1), so FUN_102e takes the $1099
// path: the glyph is XOR-ed straight onto the screen, leaving bit 7 untouched,
// which renders solid white text. op5 (TEXT_OUTLINE) leaves the flag clear and
// takes the $1057 path: the glyph is colour-blended with the current fill
// pattern, just like a brush.
// Blit one 7-bit glyph half (`mask`) into screen byte `addr` / column `col`.
// normal (op3, FUN_102e $1099) XORs it on, preserving bit 7 (the hi-res palette
// bit) so the text stays white; op5 ($1057) blends it through the fill pattern,
// just like a brush quadrant.
static void glyph_blit_half(uint16_t addr, int col, uint8_t mask, bool normal, uint8_t pat_base) {
	if (!mask || !valid_offset(addr))
		return;
	if (normal) {
		write_screen(addr, (uint8_t)(s_screenmem[addr] ^ mask));
	} else {
		uint8_t pat = s_patternData[(col & 3) | pat_base];
		write_screen(addr, (uint8_t)(((mask ^ 0x7f) & s_screenmem[addr]) | ((mask | 0x80) & pat)));
	}
}
static void draw_text_glyph(uint16_t x, uint8_t y, uint8_t ch, bool normal,
							uint8_t pat_even, uint8_t pat_odd) {
	const uint8_t *glyph = &s_fontGlyphs[(ch & 0x7f) * 8];
	uint8_t col = (uint8_t)(x / GM_PIXELS_PER_BYTE);
	uint8_t off = (uint8_t)(x % GM_PIXELS_PER_BYTE);
	uint8_t scan = y;
	for (int row = 0; row < 8; row++, scan++) {
		uint8_t g = glyph[row];
		if (g == 0)
			continue;
		uint8_t mask_a, mask_b;                 // mask_a -> col, mask_b -> col+1
		gm_spread7(g, off, &mask_a, &mask_b);
		uint16_t addr = (uint16_t)(gm_row_address(scan) + col);
		uint8_t pat_base = (uint8_t)(((scan & 1) ? pat_odd : pat_even) << 2);
		glyph_blit_half(addr,     col,     mask_a, normal, pat_base);
		glyph_blit_half(addr + 1, col + 1, mask_b, normal, pat_base);
	}
}

// ---- Circle (op11) ----
// A midpoint circle plotting 8 symmetric points per step via HPLOT (HPOSN +
// plot). Only x in 0..279, y in 0..159.

static void plot_circle_point(int x, int y, a2_ctx *ctx) {
	if (x < 0 || x >= 280 || y < 0 || y >= APPLE2_SCREEN_HEIGHT - 32)
		return;
	gm_set_draw_position((uint16_t)x, (uint8_t)y, &ctx->gv);
	gm_plot_pixel(&ctx->gv);
}

static void draw_circle(uint16_t cx, uint8_t cy, uint8_t radius, a2_ctx *ctx) {
	if (radius == 0)
		return;
	uint8_t r = radius;
	uint8_t dvar = (uint8_t)(r >> 1);
	uint8_t i = 0;
	int xa = cx, xb = cx;
	int xr = (int)cx + r, xl = (int)cx - r;
	int yt = cy, yb = cy;
	int yp = (int)cy + r, ym = (int)cy - r;

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
		uint16_t base = gm_row_address(row);
		uint8_t pat_base = ((row & 1) ? pat_odd : pat_even) * 4;
		for (int col = ctx->fill_left; col <= ctx->fill_right && col < APPLE2_SCREEN_COLS; col++)
			write_screen(base + col, s_patternData[(col & 3) | pat_base]);
	}
}

// ---- Opcode interpreter -------------------------------------------------------

// The Graphics Magician vector format has two dialects sharing the same drawing
// tables. Talisman (the default) repurposes op7 as a 2-operand no-op and op15 as
// a fill-bounds sub-op, and sets its fill clip rectangle explicitly. The older
// dialect used by the earlier Comprehend titles (Transylvania, OO-Topos, Crimson
// Crown) ends an image on op7 as well as op0, never emits op15, and fills against
// the full screen (its background PAINT seeds in the bottom text rows). Selected
// per game via gmSetLegacyFormat().
static bool s_legacyFormat = false;

static bool doImageOp(const uint8_t **outptr, const uint8_t *end, a2_ctx *ctx) {
	const uint8_t *ptr = *outptr;
	if (ptr >= end) return true;
#ifdef GM_TRACE
	if (g_gmOnOp)
		g_gmOnOp((int)(ptr - g_imgBase), ptr[0],
			(ptr + 1 < end) ? ptr[1] : 0, (ptr + 2 < end) ? ptr[2] : 0,
			ctx->gv.HGR_X, ctx->gv.HGR_Y);
#endif
	uint8_t opcode = *ptr++;
	uint8_t param = opcode & 0xf;
	opcode >>= 4;
#ifdef GM_TRACE
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

	case OPCODE_END2: // 7 -- Talisman: 2-operand no-op; older dialect: end
		if (s_legacyFormat) {
			*outptr = ptr;
			return true;
		}
		ptr += 2;
		break;

	case OPCODE_SET_TEXT_POS: // 1
		ctx->text_x = (uint16_t)(*ptr++ + (param & 1 ? 256 : 0));
		ctx->text_y = *ptr++;
		break;

	case OPCODE_SET_PEN_COLOR: // 2
		gm_set_color(param, &ctx->gv);
		break;

	case OPCODE_TEXT_CHAR: // 3 -- solid (XOR) text
		draw_text_glyph(ctx->text_x, ctx->text_y, *ptr++, true, ctx->pat_even, ctx->pat_odd);
		ctx->text_x += 7;
		break;

	case OPCODE_TEXT_OUTLINE: // 5 -- pattern-blended text
		draw_text_glyph(ctx->text_x, ctx->text_y, *ptr++, false, ctx->pat_even, ctx->pat_odd);
		ctx->text_x += 7;
		break;

	case OPCODE_SET_SHAPE: // 4
		ctx->BRUSH = param;
		break;

	case OPCODE_SET_FILL_COLOR: { // 6
		uint8_t idx = *ptr++;
		ctx->pat_even = s_colorPatternSubindices[(uint8_t)(idx * 2)];
		ctx->pat_odd = s_colorPatternSubindices[(uint8_t)(idx * 2 + 1)];
		break;
	}

	case OPCODE_MOVE_TO: // 8
		a = *ptr++ + (param & 1 ? 256 : 0);
		b = *ptr++;
		gm_set_draw_position(a, b, &ctx->gv);
		break;

	case OPCODE_DRAW_BOX: { // 9 -- rectangle, byte-faithful to op9 ($095b)
		a = *ptr++ + (param & 1 ? 256 : 0);
		b = *ptr++;
		uint16_t x0 = ctx->gv.HGR_X;
		uint8_t y0 = ctx->gv.HGR_Y;
		// Four chained HLINEs around the rectangle (the four JSR $F53A calls).
		gm_draw_line(x0, b, &ctx->gv);   // left edge   (x0,y0)->(x0,b)
		gm_draw_line(a, b, &ctx->gv);    // bottom edge ->(a,b)
		gm_draw_line(a, y0, &ctx->gv);   // right edge  ->(a,y0)
		gm_draw_line(x0, y0, &ctx->gv);  // top edge    ->(x0,y0)
		// op9 then falls through to $0990, which HPOSNs to the far corner, so the
		// pen is left at (a,b) -- NOT the start corner. A following DRAW_LINE
		// continues from there; omitting this shifted the next line by a few
		// pixels (e.g. Talisman RA #1: 11 wrong bytes along one diagonal edge).
		gm_set_draw_position(a, b, &ctx->gv);
		break;
	}

	case OPCODE_DRAW_LINE: // 10
		a = *ptr++ + (param & 1 ? 256 : 0);
		b = *ptr++;
		gm_draw_line(a, b, &ctx->gv);
		break;

	case OPCODE_DRAW_CIRCLE: { // 11 -- circle centred on the current pen position
		uint8_t radius = *ptr++;
		uint16_t ccx = ctx->gv.HGR_X;
		uint8_t ccy = ctx->gv.HGR_Y;
		draw_circle(ccx, ccy, radius, ctx);
		gm_set_draw_position(ccx, ccy, &ctx->gv);   // op11 restores the centre ($0944->$0990)
		break;
	}

	case OPCODE_DRAW_SHAPE: // 12
		a = *ptr++ + (param & 1 ? 256 : 0);
		b = *ptr++;
		gm_draw_brush(a, b, ctx->BRUSH, ctx->pat_even, ctx->pat_odd, &ctx->gv);
		break;

	case OPCODE_DELAY: { // 13 -- pause (no effect on the final page; paces the reveal)
		uint8_t units = *ptr++;
		s_slow.recordDelay(units);
		break;
	}

	case OPCODE_PAINT: // 14
		a = *ptr++ + (param & 1 ? 256 : 0);
		b = *ptr++;
		apple2_flood_fill(a, b, ctx->pat_even, ctx->pat_odd,
			ctx->fill_left, ctx->fill_right, ctx->fill_top, ctx->fill_bottom);
		break;

	case OPCODE_RESET: // 15 -- Talisman: sub-op in low nibble; older dialect: end
		if (s_legacyFormat) {
			*outptr = ptr;
			return true;
		}
		switch (param) {
		case 0: { // RLE-compressed bitmap rectangle (The Coveted Mirror; $0a3f)
			// Reads 4 bounds bytes exactly like sub-op 2, then streams raw
			// hi-res page bytes column-major (top->bottom within a column,
			// then the next column). A 0x80 byte escapes a run: the next two
			// bytes are count and value, written count+1 times. The handler
			// returns the moment the bottom-right cell is written, even
			// mid-run ($0a6e).
			uint8_t right  = *ptr++;
			uint8_t left   = *ptr++;
			uint8_t bottom = *ptr++;
			uint8_t top    = *ptr++;
			ctx->fill_right = right; ctx->fill_left = left;
			ctx->fill_bottom = bottom; ctx->fill_top = top;
			int col = left, row = top;
			bool done = false;
			while (!done && ptr < end) {
				uint8_t b = *ptr++;
				int reps = 1;
				if (b == 0x80) {
					if (ptr + 2 > end) break;
					reps = *ptr++ + 1;
					b = *ptr++;
				}
				while (reps--) {
					if (row < APPLE2_SCREEN_HEIGHT && col < APPLE2_SCREEN_COLS)
						write_screen(gm_row_address(row) + col, b);
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
			break;
		}
		case 1: // bounds = full screen
			ctx->fill_left = 0; ctx->fill_top = 0;
			ctx->fill_right = 39; ctx->fill_bottom = 0x9f;
			break;
		case 2: // read 4 bytes of bounds
			ctx->fill_right = *ptr++;
			ctx->fill_left = *ptr++;
			ctx->fill_bottom = *ptr++;
			ctx->fill_top = *ptr++;
			break;
		default: // param 3: fill the rectangle with the current pattern
			fill_rect_pattern(ctx, ctx->pat_even, ctx->pat_odd);
			break;
		}
		break;
	}
	*outptr = ptr;
	return false;
}

// ---- Apple hi-res -> RGB --------------------------------------------------
// The NTSC artifact-colour kernel (rotl4b / artifact_color_lut / apple2_palette
// / Double7Bits / per-row word + colour expansion) now lives in
// common_sagadraw/gm_artifact.c, shared with apple2draw.c.

// Convert a hi-res page -> RGBA into out[w*h] (0xRRGGBBAA). Renders Apple rows
// 0..h-1, sampling the 560-wide artifact colours down to w columns. The game
// uses w = 560, which keeps every colour edge where MAME puts it; a narrower
// w drops the odd half-pixels, moving some edges by half an Apple pixel.
static void screenmem_to_rgba(const uint8_t *src, uint32_t *out, int w, int h) {
	for (int row = 0; row < h; row++) {
		unsigned const address = gm_row_address(row);
		const uint8_t *vram_row = src + address;
		uint16_t words[40];
		gm_compute_row_words(vram_row, words);
		uint8_t colors560[560];
		gm_render_line_colors(words, colors560);
		for (int x = 0; x < w; x++) {
			int sx = (x * 560) / w;
			if (sx > 559) sx = 559;
			uint32_t rgb = gm_apple2_palette[colors560[sx] & 0x0f];
			out[row * w + x] = (rgb << 8) | 0xff;
		}
	}
}

// ---- Public entry -------------------------------------------------------------

void gmResetScreen(bool white) {
	memset(s_screenmem, white ? 0xff : 0x00, A2_SCREEN_MEM_SIZE);
	// A reset starts a fresh page: the background appears instantly on the
	// visible page too, and any pending reveal is dropped.
	memset(s_slowScreen, white ? 0xff : 0x00, A2_SCREEN_MEM_SIZE);
	s_slow.clear();
}

// ---- Slow-draw control (host-driven) ----------------------------------------
// The machinery lives in the shared SlowDrawEngine (slow_draw_page.h).

void gmSetSlowDraw(bool on) { s_slow.setRecording(on); }
bool gmSlowDrawActive() { return s_slow.active(); }
bool gmAdvanceSlowDraw(int budget) { return s_slow.advance(budget); }
void gmFinishSlowDraw() { s_slow.finish(); }
bool gmSlowConsumeDirty(int *y0, int *y1) { return s_slow.consumeDirty(y0, y1); }

void gmBlitSlowToSurface(uint32_t *out, int w, int h) {
	screenmem_to_rgba(s_slowScreen, out, w, h);
}

// Diagnostic accessor (offline pixel-diff harness): the raw 0x2000-byte page.
const uint8_t *gmPagePtr() { return s_screenmem; }
void gmSetPage(const uint8_t *p) { memcpy(s_screenmem, p, A2_SCREEN_MEM_SIZE); }
// Diagnostic accessor: the progressively-revealed (slow-draw) page.
const uint8_t *gmSlowPagePtr() { return s_slowScreen; }

// --- The Coveted Mirror persistent right-hand panel ----------------------
// CM draws a fixed right-hand panel -- the "The Coveted Mirror" logo above an
// hourglass timer -- into screen columns 24..39 once at boot, then every room
// picture is drawn into the left columns only, leaving the panel untouched.
// Our room images are decoded straight from the disk and span the full width,
// so after each in-game picture we re-composite the saved panel columns on top,
// reproducing the original's always-present panel.
static uint8_t s_cmPanel[A2_SCREEN_MEM_SIZE];
static bool s_cmPanelValid = false;
static int s_cmPanelCol0 = 24, s_cmPanelCol1 = 39;

// Per-turn grain-fall animation state. The original interpreter
// (cm_per_turn_graphics_step $42f8) tracks the displayed grain count separately
// from the live sand variable and, on a normal turn, erases just the one top
// grain. We mirror that "displayed" counter so we can tell a single-grain
// per-turn drop (animate the freed grain through the neck) from a big jump
// (bribe refill to 74, catch, save/restore -> snap to the new level).
static int s_cmDisplayedSand = -1;   // grains currently shown; -1 = not yet drawn
static bool s_cmFallArmed = false;   // a single-grain drop is waiting to animate
static CMGrainFall s_cmFall;         // the freed grain's trip down the neck (cm_panel.h)

void gmCaptureCMPanel(int col0, int col1) {
	memcpy(s_cmPanel, s_screenmem, A2_SCREEN_MEM_SIZE);
	s_cmPanelCol0 = col0;
	s_cmPanelCol1 = col1;
	s_cmPanelValid = true;
}

bool gmCMPanelValid() { return s_cmPanelValid; }

// Scratch state for gmBeginCMPanelRebuild()/gmEndCMPanelRebuild(): the live
// pages and recording flag are stashed across the rebuild so it is invisible to
// the picture currently being drawn.
static uint8_t s_cmRebuildSaveScreen[A2_SCREEN_MEM_SIZE];
static uint8_t s_cmRebuildSaveSlow[A2_SCREEN_MEM_SIZE];
static bool s_cmRebuildRecording = false;

void gmBeginCMPanelRebuild() {
	memcpy(s_cmRebuildSaveScreen, s_screenmem, A2_SCREEN_MEM_SIZE);
	memcpy(s_cmRebuildSaveSlow, s_slowScreen, A2_SCREEN_MEM_SIZE);
	s_cmRebuildRecording = s_slow.recording();
	s_slow.setRecording(false);
	memset(s_screenmem, 0x00, A2_SCREEN_MEM_SIZE);   // black panel background
}

void gmEndCMPanelRebuild() {
	gmCaptureCMPanel(24, 39);
	memcpy(s_screenmem, s_cmRebuildSaveScreen, A2_SCREEN_MEM_SIZE);
	memcpy(s_slowScreen, s_cmRebuildSaveSlow, A2_SCREEN_MEM_SIZE);
	s_slow.setRecording(s_cmRebuildRecording);
}

// Copy the saved static panel columns onto rows [y0,y1] of both the final page
// (s_screenmem) and the progressively-revealed page (s_slowScreen).
static void cm_panel_to_pages(int y0, int y1) {
	for (int row = y0; row <= y1; row++) {
		uint16_t base = gm_row_address(row);
		for (int col = s_cmPanelCol0; col <= s_cmPanelCol1; col++) {
			s_screenmem[base + col]  = s_cmPanel[base + col];
			s_slowScreen[base + col] = s_cmPanel[base + col];
		}
	}
}

void gmOverlayCMPanel() {
	if (!s_cmPanelValid)
		return;
	cm_panel_to_pages(0, APPLE2_SCREEN_HEIGHT - 1);
}

// Apply one recorded byte to the slow page, skipping the CM panel columns.
// Returns false (suppressing dirty marking) when the byte is skipped.
//
// The Coveted Mirror's panel occupies columns [col0,col1] and is composited on
// top of every finished picture (gmOverlayCMPanel writes it to both pages). A
// room drawn with a full-width op15/0 RLE bitmap, however, records writes across
// those columns too; replaying them during the reveal would wipe the panel off
// s_slowScreen. Skip panel-column ops so the overlaid panel survives the reveal
// (it is already correct on the final page). Mirrors DHGR's dhgr_apply_slow.
static bool apple_apply_slow(const SlowWriteOp &op) {
	if (s_cmPanelValid) {
		int c = col_of_offset(op.offset);
		if (c >= s_cmPanelCol0 && c <= s_cmPanelCol1)
			return false;
	}
	s_slowScreen[op.offset] = op.value;
	return true;
}

// Per-grain position, ported byte-faithfully from cm_draw_hourglass_grain
// ($4347). `idx` is the grain index (0-based); the original treats idx 0 as a
// no-op. Returns false for grains that stamp nothing. Apple hi-res x (0..279),
// y (0..191). The pile is centred at x=245 (column ~35): even grains step right
// (245+half), odd grains step left (245-half); the grain falls one "band" of 25
// lower (y -= 1 per band, then y -= half within the band). Exposed (non-static)
// so the double-hi-res renderer can place the same grains on its own pages.
bool gmCMHourglassGrainPos(uint8_t idx, uint16_t *outX, uint8_t *outY) {
	if (idx == 0)
		return false;
	uint8_t scan = 0x5c;              // DAT_43a2, the descending y cursor
	uint8_t b = idx;                  // bVar1
	while (b >= 0x1a) {               // do { if (b<0x1a) break; b-=0x19; scan--; } while (scan)
		b -= 0x19;
		scan--;
		if (scan == 0)
			break;
	}
	uint8_t half = (uint8_t)(b >> 1); // DAT_00fa
	uint8_t col;                      // DAT_42f6
	if (b & 1)
		col = (uint8_t)(0x0c - half); // odd grain: left of centre
	else
		col = (uint8_t)(half + 0x0c); // even grain: right of centre
	scan = (uint8_t)(scan - half);    // DAT_43a2 -= half
	uint16_t x = (uint8_t)(col - 0x17);
	if (col > 0x16)                   // DAT_43a0 = 0xc1: x >= 256
		x += 256;
	*outX = x;
	*outY = scan;
	return true;
}

// White grains use SET_FILL_COLOR 0x34, which maps to pattern sub-index (7,7) =
// the 0xff (all-pixels-white) pattern. (The original's drain path instead uses
// 0x35 -> pattern 4 = 0x80 = black, to erase the top grain; we redraw the whole
// pile each turn, so we only ever paint white.)
static void cm_grain_brush_ctx(gm_vector_ctx *gv, uint8_t *patEven, uint8_t *patOdd) {
	*gv = gm_vector_ctx{};
	gv->screenmem = s_screenmem;
	gv->write = gm_write_adapter;
	gv->user = nullptr;
	gv->pattern_data = s_patternData;
	gv->brush_bitmaps = s_brushBitmaps;
	*patEven = s_colorPatternSubindices[0x34 * 2];
	*patOdd = s_colorPatternSubindices[0x34 * 2 + 1];
}

// Stamp the resting pile of `sand` grains onto the current page.
static void cm_stamp_hourglass_pile(int sand) {
	gm_vector_ctx gv;
	uint8_t pat_even, pat_odd;
	cm_grain_brush_ctx(&gv, &pat_even, &pat_odd);
	cmForEachPileGrain(sand, [&](uint16_t x, uint8_t y) {
		gm_draw_brush(x, y, 0, pat_even, pat_odd, &gv);
	});
}

// Restore columns [col0,col1] of the saved static panel (logo + hourglass frame
// + bottom mound, with NO grains) for rows [y0,y1] onto both pages. Used to wipe
// the previous animation frame's falling grain before drawing the next.
static void cmOverlayPanelBand(int y0, int y1) {
	if (!s_cmPanelValid)
		return;
	if (y0 < 0)
		y0 = 0;
	if (y1 > APPLE2_SCREEN_HEIGHT - 1)
		y1 = APPLE2_SCREEN_HEIGHT - 1;
	cm_panel_to_pages(y0, y1);
}

// Renderer-independent arm bookkeeping: track the displayed sand level so the
// host can tell a per-turn single-grain drop (animate) from a snap (first draw,
// bribe refill, catch, restore). A same-level repaint (item overlays redraw the
// panel mid-turn) leaves the armed flag alone so a fall queued on the room repaint
// survives. Shared by the standard gmDrawCMHourglass() and the DHGR
// gmDhgrDrawCMHourglass() so the armed flag is set whichever renderer is active.
void gmCMHourglassArm(int sand) {
	if (sand < 0)
		sand = 0;
	int prev = s_cmDisplayedSand;
	if (prev < 0)
		s_cmFallArmed = false;            // first draw: snap
	else if (sand == prev - 1)
		s_cmFallArmed = true;             // normal per-turn drop: animate
	else if (sand != prev)
		s_cmFallArmed = false;            // big jump: snap
	s_cmDisplayedSand = sand;
}

void gmDrawCMHourglass(int sand) {
	if (sand < 0)
		sand = 0;

	gmCMHourglassArm(sand);

	cm_stamp_hourglass_pile(sand);

	// Keep the progressively-revealed page in step so either blit path shows the
	// finished pile (the hourglass is not part of the slow reveal).
	memcpy(s_slowScreen, s_screenmem, A2_SCREEN_MEM_SIZE);
}

// Forget the displayed level so the next gmDrawCMHourglass() snaps instead of
// animating. Called after a restore / undo / restart, where the sand jumps to
// an unrelated value, and on a fresh panel build.
void gmCMHourglassReset() {
	s_cmDisplayedSand = -1;
	s_cmFallArmed = false;
	s_cmFall.reset();
}

// Consume the "a single grain just drained" flag set by gmDrawCMHourglass().
// The host checks this once the turn's graphics have settled to decide whether
// to run the falling-grain animation.
bool gmCMHourglassConsumeFallArmed() {
	bool armed = s_cmFallArmed;
	s_cmFallArmed = false;
	return armed;
}

// Begin / advance / abort the falling-grain animation. The resting pile is
// already at the post-drop level (gmDrawCMHourglass stamped it); these only
// paint the single freed grain travelling down the neck, confined to the
// CMGrainFall band so the pile above is never touched.
void gmCMHourglassFallBegin() {
	s_cmFall.begin();
}

bool gmCMHourglassFallActive() {
	return s_cmFall.active();
}

bool gmCMHourglassFallStep(int *y0, int *y1) {
	return s_cmFall.step(y0, y1, cmOverlayPanelBand, [](uint16_t x, uint8_t y) {
		gm_vector_ctx gv;
		uint8_t pat_even, pat_odd;
		cm_grain_brush_ctx(&gv, &pat_even, &pat_odd);
		gm_draw_brush(x, y, 0, pat_even, pat_odd, &gv);
	});
}

void gmCMHourglassFallAbort() {
	s_cmFall.abort(cmOverlayPanelBand);
}

void gmDrawImage(const uint8_t *data, size_t size) {
#ifdef GM_TRACE
	g_imgBase = data;
#endif
	// The op list is dropped by gmResetScreen() when a new page starts (room /
	// dark / bright / title), NOT here: a scene is a room picture plus any item
	// overlays, and the overlays must APPEND their bytes to the room's still-
	// pending reveal so the whole composite is revealed in paint order. Clearing
	// here would discard the room's not-yet-revealed bytes the moment the first
	// item draws, leaving only the last picture animated on a blank page.
	a2_ctx ctx = {};
	ctx.gv.screenmem = s_screenmem;
	ctx.gv.write = gm_write_adapter;
	ctx.gv.user = nullptr;
	ctx.gv.pattern_data = s_patternData;
	ctx.gv.brush_bitmaps = s_brushBitmaps;
	gm_set_color(4, &ctx.gv);
	gm_set_draw_position(140, 96, &ctx.gv);
	// Default fill colour = idx $4d (the picture interpreter's init at $08a6),
	// which FUN_0c92 maps to the $1132 even/odd sub-index pair.
	ctx.pat_even = s_colorPatternSubindices[0x4d * 2];
	ctx.pat_odd = s_colorPatternSubindices[0x4d * 2 + 1];
	ctx.BRUSH = 5;
	// Talisman clips fills to the picture rows (0..0x9f) and widens the rectangle
	// via op15; the older dialect has no op15 and paints the whole screen, seeding
	// its background fill in the bottom text rows (e.g. PAINT at y=191).
	ctx.fill_left = 0; ctx.fill_top = 0; ctx.fill_right = 39;
	ctx.fill_bottom = s_legacyFormat ? 0xbf : 0x9f;

	const uint8_t *ptr = data;
	const uint8_t *end = data + size;
	bool done = false;
	int guard = 0;
	while (!done && ptr < end && guard++ < 100000)
		done = doImageOp(&ptr, end, &ctx);

	// When not animating, keep the visible page in step with the final page so a
	// later slow-drawn overlay composes onto the right background.
	s_slow.syncIfNotRecording();
}

void gmBlitToSurface(uint32_t *out, int w, int h) {
	screenmem_to_rgba(s_screenmem, out, w, h);
}

void gmSetLegacyFormat(bool on) { s_legacyFormat = on; }

} // namespace Comprehend
} // namespace Glk
