// Standard hi-res renderer for the Apple II Comprehend games. Their pictures use Penguin
// Software's Graphics Magician vector format: the primitives plot into a real hi-res page
// (s_screenmem), which a second pass converts to RGBA with the NTSC artifact-colour kernel
// (gm_artifact.cpp). The model was derived from the games' own 6502 routines.
//
// There are two dialects, selected by gmSetLegacyFormat(); see
// https://6502disassembly.com/a2-graphics-magician/PICDRAWH_1984.html
//  - Later (Talisman, OO-Topos, Transylvania v2, The Coveted Mirror): op7 (0x7x) is a 2-operand
//    no-op and op15 takes a sub-op in its low nibble: 0 draws an RLE bitmap, 1 sets the fill
//    rectangle to the full picture, 2 reads 4 bytes of bounds, 3 fills the rectangle with the
//    current colour pattern (the per-image background).
//  - Older (Transylvania, Crimson Crown): op7 and op15 end the image, and fills clip to the full
//    screen, as the background PAINT seeds in the bottom text rows.
//
// The colour, pattern and brush tables come from the Graphics Magician code file "T2" on the
// boot disk (gmInstallDrawingTables).

#include "graphics_magician.h"
#include "gm_vector.h"
#include "gm_artifact.h"
#include "slow_draw_page.h"     // shared progressive-reveal helper
#include "gm_stream.h"          // shared opcode dispatcher
#include "gm_blit.h"            // shared 560-colour row sampling
#include "gm_span_fill.h"       // shared op14 flood fill
#include "graphics_magician_vector.h"  // shared circle stepper
#include <cstring>

namespace Glk {
namespace Comprehend {

// Populated by gmInstallDrawingTables() from the boot disk's T2 file. Until then they stay
// zero, which renders nothing.

static uint8_t s_patternData[120];              // 30 colour patterns x 4 bytes
static uint8_t s_colorPatternSubindices[216];   // 108 fills x (even,odd) index
static uint8_t s_brushBitmaps[256];             // 8 brushes x 4 quadrants x 8
static uint8_t s_fontGlyphs[128 * 8];           // op3/op5 picture font, 8 bytes/char

// T2 is a headerless ProDOS BIN that loads at $0800.
bool gmInstallDrawingTables(const uint8_t *t2, size_t size) {
	if (!t2)
		return false;

	// The tables are byte-identical across titles and sit contiguously as
	//   [ colour subindices : 216 ][ pattern data : 120 ][ brush bitmaps : 256 ]
	// but at a different offset in each release's T2, so the pattern data is located by its
	// signature, the Apple hi-res colour patterns. Its first four bytes are zero, so the
	// signature starts at byte 4.
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

	// The op3/op5 picture font shares the brush table's base: glyph(ch) is at
	// brushBase + ch*8, so the brushes double as the unused control-char glyphs $00..$1f.
	// If T2 ends early the font stays blank rather than failing the tables above.
	if (brushBase + sizeof(s_fontGlyphs) <= size)
		std::memcpy(s_fontGlyphs, t2 + brushBase, sizeof(s_fontGlyphs));
	return true;
}

#define A2_SCREEN_MEM_SIZE   GM_HIRES_PAGE_SIZE
#define APPLE2_SCREEN_HEIGHT GM_HIRES_ROWS
#define APPLE2_SCREEN_COLS   GM_HIRES_COLS
#define RIGHT_EXPAND_MASK 0x40


// Persistent across calls: item pictures draw on top of the room already on the page, as the
// original never clears the screen for them. gmResetScreen() starts a fresh room.
static uint8_t s_screenmem[A2_SCREEN_MEM_SIZE];

static inline bool valid_offset(uint16_t off) { return off <= GM_MAX_SCREEN_ADDR; }

// Slow draw. While recording (gmSetSlowDraw), every byte written goes to the final page
// (s_screenmem) and onto the op list of the shared SlowDrawEngine (slow_draw_page.h). The host
// replays the ops onto s_slowScreen a chunk at a time on a Glk timer, starting from the
// background. Item overlays start from whatever is already on s_slowScreen, so they compose as
// on the real machine, and CM panel overlays composite onto it directly.
static uint8_t s_slowScreen[A2_SCREEN_MEM_SIZE]; // progressively-revealed page

// Apply one recorded byte to the slow page; defined after the CM panel state it consults.
static bool apple_apply_slow(const SlowWriteOp &op);

// Apple II hi-res page: one flat buffer, but rows are address-interleaved, so
// the dirty band maps offsets through gm_row_of_offset (not offset/stride).
struct AppleSlowPolicy {
	bool apply(const SlowWriteOp &op) { return apple_apply_slow(op); }
	int rowOf(const SlowWriteOp &op) const { return gm_row_of_offset(op.offset); }
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


typedef struct {
	gm_vector_ctx gv;      // shared cursor/colour + line-drawing state
	uint8_t BRUSH;         // current shape/brush type
	// current fill colour as even/odd-row pattern sub-indices,
	// set by op6 SET_FILL_COLOR; default (3,3) = white.
	uint8_t pat_even, pat_odd;
	// op15 fill rectangle bounds (column 0..39, row 0..191)
	GmFillBounds fill;
	// op1 text cursor; op3/op5 draw a glyph here and advance text_x by 7
	uint16_t text_x;
	uint8_t text_y;
} a2_ctx;

// op14 PAINT. The span-fill engine is shared with the double hi-res and CGA renderers
// (gm_span_fill.h); this is the standard hi-res page it runs on: one byte per column,
// patterned from a 4-byte-wide dither selected by row parity.

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
static GmHiresSpanFill<AppleFillPage> s_spanFill(s_fillPage);

// Flood-fill entry point: seed point (x,y), even/odd fill patterns and the clip
// rectangle (columns left..right, rows top..bottom).
static void apple2_flood_fill(uint16_t x, uint8_t y, uint8_t pat_even, uint8_t pat_odd,
		const GmFillBounds &clip) {
	s_fillPage.patEven = pat_even;
	s_fillPage.patOdd  = pat_odd;
	s_spanFill.fill(x, y, clip.left, clip.right, clip.top, clip.bottom);   // x == column*7 + bit
}

// Text glyphs (op3 TEXT_CHAR, op5 TEXT_OUTLINE) go through the same per-row blitter as brushes.
// Each glyph byte is spread across two adjacent columns by the pixel offset within the column.

// Blit one 7-bit glyph half (`mask`) into screen byte `addr` / column `col`. op3 (normal) XORs
// it on, preserving bit 7 (the palette bit) so the text stays white; op5 blends it through the
// fill pattern, like a brush quadrant.
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

// op11: the shared 6502 midpoint stepper (gmvAppleCircleSteps) plots eight symmetric points per
// step. Only x in 0..279, y in 0..159 are drawn.

static void plot_circle_point(int x, int y, a2_ctx *ctx) {
	if (x < 0 || x >= 280 || y < 0 || y >= APPLE2_SCREEN_HEIGHT - 32)
		return;
	gm_set_draw_position((uint16_t)x, (uint8_t)y, &ctx->gv);
	gm_plot_pixel(&ctx->gv);
}

static void draw_circle(uint16_t cx, uint8_t cy, uint8_t radius, a2_ctx *ctx) {
	auto plot = [ctx](int x, int y) { plot_circle_point(x, y, ctx); };
	gmvAppleCircleSteps(radius, [&](int i, int r) {
		gmvCirclePlot8(cx, cy, i, r, r, i, plot);
	});
}

// op15 sub-op 3: the background rectangle fill.

static void fill_rect_pattern(a2_ctx *ctx, uint8_t pat_even, uint8_t pat_odd) {
	for (int row = ctx->fill.top; row <= ctx->fill.bottom && row < APPLE2_SCREEN_HEIGHT; row++) {
		uint16_t base = gm_row_address(row);
		uint8_t pat_base = ((row & 1) ? pat_odd : pat_even) * 4;
		for (int col = ctx->fill.left; col <= ctx->fill.right && col < APPLE2_SCREEN_COLS; col++)
			write_screen(base + col, s_patternData[(col & 3) | pat_base]);
	}
}

// The dialect (see the top of this file), selected per game via gmSetLegacyFormat().
static bool s_legacyFormat = false;

// The loop and operand decode are the shared gmRunStream (gm_stream.h); this is
// the standard hi-res side of it.
struct AppleRenderer {
	a2_ctx c;

	uint16_t scaleX(uint16_t x) const { return x; }
	int penX() const { return c.gv.HGR_X; }
	int penY() const { return c.gv.HGR_Y; }

	void begin() {
		// The op list is dropped by gmResetScreen() when a new page starts, not here: a scene
		// is a room plus any item overlays, and the overlays append to the room's pending
		// reveal so the whole composite is revealed in paint order.
		c = a2_ctx();
		c.gv.screenmem = s_screenmem;
		c.gv.write = gm_write_adapter;
		c.gv.user = nullptr;
		c.gv.pattern_data = s_patternData;
		c.gv.brush_bitmaps = s_brushBitmaps;
		gm_set_color(4, &c.gv);
		gm_set_draw_position(140, 96, &c.gv);
		// Default fill colour is index $4d, as in the interpreter's per-image init.
		c.pat_even = s_colorPatternSubindices[0x4d * 2];
		c.pat_odd = s_colorPatternSubindices[0x4d * 2 + 1];
		c.BRUSH = 5;
		// The later dialect clips fills to the picture rows and changes the rectangle via
		// op15; the older one fills against the whole screen, seeding its background fill in
		// the bottom text rows (e.g. PAINT at y=191).
		c.fill.reset(s_legacyFormat ? 0xbf : 0x9f);
	}
	// When not animating, keep the visible page in step with the final page so a
	// later slow-drawn overlay composes onto the right background.
	void end() { s_slow.syncIfNotRecording(); }

	void beforeOp(const uint8_t *p, const uint8_t *end, uint8_t op, uint8_t) {
#ifdef GM_TRACE
		if (g_gmOnOp)
			g_gmOnOp((int)(p - 1 - g_imgBase), p[-1],
				(p < end) ? p[0] : 0, (p + 1 < end) ? p[1] : 0,
				c.gv.HGR_X, c.gv.HGR_Y);
		// One tag per primitive (op >> 4), matching test/pixtest's opName order.
		static const char kPrim[16] = {'.','t','p','c','s','o','f','.',
									   'm','B','L','C','S','d','F','R'};
		g_currentPrim = kPrim[op & 0xf];
#else
		(void)p; (void)end; (void)op;
#endif
	}
	void afterOp(uint8_t, uint8_t) {}

	void setTextPos(uint16_t x, uint8_t y) { c.text_x = x; c.text_y = y; }
	// op3 is solid (XOR) text, op5 pattern-blended text.
	void drawText(uint8_t ch, bool normal) {
		draw_text_glyph(c.text_x, c.text_y, ch, normal, c.pat_even, c.pat_odd);
	}
	void advanceText() { c.text_x += 7; }
	void setPen(uint8_t param) { gm_set_color(param, &c.gv); }
	void setShape(uint8_t param) { c.BRUSH = param; }
	void setFill(uint8_t idx) {
		c.pat_even = s_colorPatternSubindices[(uint8_t)(idx * 2)];
		c.pat_odd = s_colorPatternSubindices[(uint8_t)(idx * 2 + 1)];
	}
	// Later dialect: op7 is a 2-operand no-op; the older dialect ends the image on it.
	bool op7(const uint8_t *&p, const uint8_t *e) { return s_legacyFormat || gmSkipOp7(p, e); }
	void moveTo(uint16_t x, uint8_t y) { gm_set_draw_position(x, y, &c.gv); }
	void lineTo(uint16_t x, uint8_t y) { gm_draw_line(x, y, &c.gv); }
	// op11: circle centred on the current pen position, which is restored afterwards.
	void circle(uint8_t radius) {
		uint16_t ccx = c.gv.HGR_X;
		uint8_t ccy = c.gv.HGR_Y;
		draw_circle(ccx, ccy, radius, &c);
		gm_set_draw_position(ccx, ccy, &c.gv);
	}
	void drawBrush(uint16_t x, uint8_t y) {
		gm_draw_brush(x, y, c.BRUSH, c.pat_even, c.pat_odd, &c.gv);
	}
	// Pause: no effect on the final page; paces the reveal.
	void delay(uint8_t units) { s_slow.recordDelay(units); }
	void paint(uint16_t x, uint8_t y) {
		apple2_flood_fill(x, y, c.pat_even, c.pat_odd, c.fill);
	}

	// op15: later dialect sub-op in the low nibble; the older dialect ends the image.
	bool opReset(uint8_t param, const uint8_t *&ptr, const uint8_t *end) {
		if (s_legacyFormat)
			return true;
		switch (param) {
		case 0: // RLE-compressed bitmap rectangle (The Coveted Mirror)
			// Reads 4 bounds bytes like sub-op 2, then streams raw hi-res page bytes.
			if (!c.fill.read(ptr, end))
				return true;
			gmDecodeRleBitmap(c.fill, ptr, end, [](int row, int col, uint8_t v) {
				if (row < APPLE2_SCREEN_HEIGHT && col < APPLE2_SCREEN_COLS)
					write_screen(gm_row_address(row) + col, v);
			});
			break;
		case 1: // bounds = full screen
			c.fill.reset(0x9f);
			break;
		case 2: // read 4 bytes of bounds
			if (!c.fill.read(ptr, end))
				return true;
			break;
		default: // param 3: fill the rectangle with the current pattern
			fill_rect_pattern(&c, c.pat_even, c.pat_odd);
			break;
		}
		return false;
	}
};

// Convert a hi-res page to RGBA (0xRRGGBBAA) into out[w*h]: Apple rows 0..h-1, with the 560-wide
// artifact colours sampled down to w columns. The game uses w = 560, which keeps every colour
// edge in place; a narrower w drops the odd half-pixels, moving some edges by half an Apple pixel.
static void screenmem_to_rgba(const uint8_t *src, uint32_t *out, int w, int h) {
	for (int row = 0; row < h; row++) {
		unsigned const address = gm_row_address(row);
		const uint8_t *vram_row = src + address;
		uint16_t words[40];
		gm_compute_row_words(vram_row, words);
		uint8_t colors560[560];
		gm_render_line_colors(words, colors560);
		gmSampleRow560(colors560, out + row * w, w, false);
	}
}

void gmResetScreen(bool white) {
	memset(s_screenmem, white ? 0xff : 0x00, A2_SCREEN_MEM_SIZE);
	// A reset starts a fresh page: the background appears instantly on the
	// visible page too, and any pending reveal is dropped.
	memset(s_slowScreen, white ? 0xff : 0x00, A2_SCREEN_MEM_SIZE);
	s_slow.clear();
}

void gmSetSlowDraw(bool on) { s_slow.setRecording(on); }
bool gmSlowDrawActive() { return s_slow.active(); }
bool gmAdvanceSlowDraw(int budget) { return s_slow.advance(budget); }
void gmFinishSlowDraw() { s_slow.finish(); }
bool gmSlowConsumeDirty(int *y0, int *y1) { return s_slow.consumeDirty(y0, y1); }

void gmBlitSlowToSurface(uint32_t *out, int w, int h) {
	screenmem_to_rgba(s_slowScreen, out, w, h);
}

// Diagnostic accessors: the raw 0x2000-byte page.
const uint8_t *gmPagePtr() { return s_screenmem; }
void gmSetPage(const uint8_t *p) { memcpy(s_screenmem, p, A2_SCREEN_MEM_SIZE); }
// Diagnostic accessor: the progressively-revealed (slow-draw) page.
const uint8_t *gmSlowPagePtr() { return s_slowScreen; }

// The Coveted Mirror's right-hand panel: the logo above an hourglass timer, in screen columns
// 24..39. The original draws it once at boot and draws rooms in the left columns only. Room
// images here span the full width, so the saved panel columns are composited back on top after
// each in-game picture.
static CMPanelStore<1> s_cmPanel;

// Per-turn grain fall. The original tracks the displayed grain count separately from the live
// sand variable and normally erases just the top grain. This counter tells a single-grain drop
// (animate the freed grain through the neck) from a big jump (bribe refill to 74, catch,
// save/restore: snap to the new level).
static int s_cmDisplayedSand = -1;   // grains currently shown; -1 = not yet drawn
static bool s_cmFallArmed = false;   // a single-grain drop is waiting to animate
static CMGrainFall s_cmFall;         // the freed grain's trip down the neck (cm_panel.h)

void gmCaptureCMPanel(int col0, int col1) {
	uint8_t *const live[1] = { s_screenmem };
	s_cmPanel.capture(live, col0, col1);
}

bool gmCMPanelValid() { return s_cmPanel.valid(); }

// The rebuild stashes the live pages and recording flag so it is invisible to the
// picture currently being drawn (see CMPanelStore).
void gmBeginCMPanelRebuild() {
	uint8_t *const live[1] = { s_screenmem };
	uint8_t *const slow[1] = { s_slowScreen };
	s_cmPanel.beginRebuild(live, slow, s_slow.recording());
	s_slow.setRecording(false);
	memset(s_screenmem, 0x00, A2_SCREEN_MEM_SIZE);   // black panel background
}

void gmEndCMPanelRebuild() {
	uint8_t *const live[1] = { s_screenmem };
	uint8_t *const slow[1] = { s_slowScreen };
	s_slow.setRecording(s_cmPanel.endRebuild(live, slow));
}

// Copy the saved static panel columns onto rows [y0,y1] of both the final page
// (s_screenmem) and the progressively-revealed page (s_slowScreen).
static void cm_panel_to_pages(int y0, int y1) {
	uint8_t *const live[1] = { s_screenmem };
	uint8_t *const slow[1] = { s_slowScreen };
	s_cmPanel.toPages(live, slow, y0, y1);
}

void gmOverlayCMPanel() {
	if (!s_cmPanel.valid())
		return;
	cm_panel_to_pages(0, APPLE2_SCREEN_HEIGHT - 1);
}

// Apply one recorded byte to the slow page. Returns false (no dirty marking) for CM panel
// columns: a room drawn with a full-width op15/0 RLE bitmap records writes across them, and
// replaying those would wipe the overlaid panel off s_slowScreen. The final page is already
// correct, as gmOverlayCMPanel writes to both.
static bool apple_apply_slow(const SlowWriteOp &op) {
	if (s_cmPanel.skipsReveal(op.offset))
		return false;
	s_slowScreen[op.offset] = op.value;
	return true;
}

// Position of grain `idx` in Apple hi-res coordinates; idx 0 stamps nothing (returns false).
// The pile is centred at x=245: even grains step right (245+half), odd grains left (245-half).
// Each band of 25 grains moves y up by one, then y -= half within the band. Non-static so the
// double hi-res renderer can place the same grains on its own pages.
bool gmCMHourglassGrainPos(uint8_t idx, uint16_t *outX, uint8_t *outY) {
	if (idx == 0)
		return false;
	uint8_t scan = 0x5c;              // the descending y cursor
	uint8_t b = idx;
	while (b >= 0x1a) {
		b -= 0x19;
		scan--;
		if (scan == 0)
			break;
	}
	uint8_t half = (uint8_t)(b >> 1);
	uint8_t col;
	if (b & 1)
		col = (uint8_t)(0x0c - half); // odd grain: left of centre
	else
		col = (uint8_t)(half + 0x0c); // even grain: right of centre
	scan = (uint8_t)(scan - half);
	uint16_t x = (uint8_t)(col - 0x17);
	if (col > 0x16)                   // x >= 256
		x += 256;
	*outX = x;
	*outY = scan;
	return true;
}

// White grains use SET_FILL_COLOR 0x34, the all-pixels-white pattern. The original's drain path
// erases the top grain in black (0x35); here the whole pile is redrawn each turn, so only white
// is ever painted.
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

// Restore columns [col0,col1] of the saved static panel (logo + hourglass frame + bottom mound,
// with no grains) for rows [y0,y1] onto both pages. This wipes the previous animation frame's
// falling grain before the next is drawn.
static void cmOverlayPanelBand(int y0, int y1) {
	if (!s_cmPanel.valid())
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
	AppleRenderer r;
	gmRunStream(r, data, size);
}

void gmBlitToSurface(uint32_t *out, int w, int h) {
	screenmem_to_rgba(s_screenmem, out, w, h);
}

void gmSetLegacyFormat(bool on) { s_legacyFormat = on; }

} // namespace Comprehend
} // namespace Glk
