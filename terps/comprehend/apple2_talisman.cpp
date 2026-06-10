/* apple2_talisman.cpp -- pixel-faithful renderer for the Apple II version of
 * Talisman (Polarware, 1987), STANDARD hi-res mode.
 *
 * Talisman's room/item pictures are drawn with Penguin Software's "Graphics
 * Magician" / "Picture Painter" vector format -- the SAME engine the Apple II
 * Scott Adams games use. The drawing primitives render into a real Apple hi-res
 * framebuffer (s_screenmem); a second pass converts that to RGBA using the
 * Apple NTSC artifact-colour algorithm (from terps/common_sagadraw/apple2draw.c).
 *
 * Each primitive here was transliterated from, and verified byte-for-byte
 * against, Talisman's own 6502 routines (Ghidra on a MAME RAM dump, plus running
 * the real ROM routines on identical inputs and diffing the resulting hi-res
 * page): line = Applesoft HLINE ($F53A); box = four HLINEs ($095B); circle =
 * midpoint circle ($0AD3); brush = $0FFB/$102E; flood fill = the queue-based
 * span fill ($0CCA); background fill = op15 sub-op3 ($0A2D). Output now matches
 * the hardware byte-exactly over every picture row in the regression set
 * (test/test_talisman_pics.cpp).
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
 *
 * The colour/pattern tables (s_patternData, s_colorPatternSubindices,
 * s_brushBitmaps) are not hard-coded: they are pulled at runtime out of the
 * Graphics Magician code file "T2" on the boot disk by
 * talismanInstallDrawingTables() (see below). apple2.cpp calls it once the
 * disks are mounted; the regression test loads the captured fixture
 * test/talisman/t2.bin.
 */

#include "apple2_talisman.h"
#include <cstring>

namespace Glk {
namespace Comprehend {

// ---- Drawing tables ----------------------------------------------------------
// Populated by talismanInstallDrawingTables() from the boot disk's T2 file.
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
bool talismanInstallDrawingTables(const uint8_t *t2, size_t size) {
    if (!t2)
        return false;
    const uint16_t kT2LoadAddr = 0x0800;
    const struct {
        uint16_t addr;
        size_t len;
        uint8_t *dest;
    } tables[] = {
        { 0x1132, sizeof(s_colorPatternSubindices), s_colorPatternSubindices },
        { 0x120a, sizeof(s_patternData),            s_patternData },
        { 0x1282, sizeof(s_brushBitmaps),           s_brushBitmaps },
    };
    for (const auto &t : tables) {
        size_t off = (size_t)(t.addr - kT2LoadAddr);
        if (off + t.len > size)
            return false;
    }
    for (const auto &t : tables) {
        size_t off = (size_t)(t.addr - kT2LoadAddr);
        std::memcpy(t.dest, t2 + off, t.len);
    }
    // The op3/op5 picture font shares the brush table's base: glyph(ch) lives at
    // $1282 + ch*8 (so the brush bitmaps double as the unused control-char glyphs
    // $00..$1f, and the printable glyphs run from $1382 to ~$1682). Copy the whole
    // 128-char range; leave it zero (blank text) if a T2 variant is too short
    // rather than failing the critical tables above.
    {
        size_t off = (size_t)(0x1282 - kT2LoadAddr);
        if (off + sizeof(s_fontGlyphs) <= size)
            std::memcpy(s_fontGlyphs, t2 + off, sizeof(s_fontGlyphs));
    }
    return true;
}

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
	// op1 text cursor; op3/op5 draw a glyph here and advance text_x by 7
	uint16_t text_x;
	uint8_t text_y;
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

// ---- Flood fill ---------------------------------------------------------------
//
// op14 fills the connected region of "set" (white) pixels reachable from a seed,
// replacing them with the current fill-colour dither pattern and stopping at clear
// (black) pixels and the clip rectangle.
//
// It is a queue-based span fill: painting one horizontal run of pixels spawns
// child runs on the rows immediately above and below it, which are processed in
// turn from a 32-entry circular queue. A merge step folds together runs that an
// opposite-direction span has already claimed so no pixel is painted twice.
// Painting modifies the very screen bits that later boundary tests read, so the
// traversal/merge order is part of the algorithm and is preserved here.
//
// A pixel position is its horizontal coordinate, column*7 + bit, where `column`
// selects the hi-res byte (each byte holds 7 visible pixels) and `bit` is 0..6
// within it. Positions are therefore ordinary integers: comparisons are <, == and
// stepping one pixel is ++/--. Stepping left of column 0 yields a negative index,
// which correctly sorts below every on-screen pixel (matching the signed-byte
// boundary tests of the 6502 original) and is never used to address the screen.

#define QUEUE_SIZE   32
#define QUEUE_MASK   0x1f
#define DIR_UP       0     // child run continues onto the row above
#define DIR_DOWN     1     // child run continues onto the row below
#define DIR_DISABLED 0xff  // span was absorbed by a merge; skip it

struct Span {
	uint8_t row;
	int     left, right;   // inclusive pixel-coordinate edges
	uint8_t dir;
};

static Span    g_queue[QUEUE_SIZE];
static uint8_t g_head, g_tail;          // circular-queue head / tail indices

static uint8_t  g_row;                  // current row
static int      g_pos;                  // current pixel position (column*7 + bit)
static int      g_edge;                 // left edge produced by paint_span()
static uint8_t  g_bit, g_edge_bit;      // paint_span / edge-mask scratch
static uint16_t g_row_addr;             // hi-res base address of row g_row
static uint8_t  g_pat_base;             // fill-pattern table base for row g_row
static uint8_t  g_pat_even, g_pat_odd;  // fill-pattern sub-indices (even/odd row)
static uint8_t  g_clip_left, g_clip_right, g_clip_top, g_clip_bottom;

static const uint8_t gmBIT[7]   = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40};
static const uint8_t gmLMASK[7] = {0x80, 0x81, 0x83, 0x87, 0x8f, 0x9f, 0xbf};
static const uint8_t gmRMASK[7] = {0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80};

static inline int col_of(int pos) { return pos / COL_BITS; }
static inline int bit_of(int pos) { return pos % COL_BITS; }
static inline int to_pos(int col, int bit) { return col * COL_BITS + bit; }

// Compute the base address and pattern base for the current row (g_row); odd
// rows draw with the odd fill sub-index, even rows with the even one.
static void set_row_address() {
	g_row_addr = CALC_APPLE2_ADDRESS(g_row);
	g_pat_base = static_cast<uint8_t>(((g_row & 1) ? g_pat_odd : g_pat_even) << 2);
}

// Is the pixel at the current position set?
static bool pixel_is_set() {
	return (gmBIT[bit_of(g_pos)] & s_screenmem[g_row_addr + col_of(g_pos)]) != 0;
}

// Locate the right / left edge of a run of set bits within byte `p`, returning
// the paint mask and storing the boundary bit index (right_edge_mask -> g_edge_bit,
// left_edge_mask -> g_bit). When all seven bits are set the run continues past
// this byte (no edge here): return 0xff.
static uint8_t right_edge_mask(uint8_t p) {
	if ((p & 0x7f) == 0x7f)
		return 0xff;
	g_edge_bit = 7;
	uint8_t bit;
	do {
		g_edge_bit--;
		bit = static_cast<uint8_t>(p & 0x7f);
		p = static_cast<uint8_t>(p << 1);
	} while (bit >> 6);
	return gmRMASK[g_edge_bit];
}
static uint8_t left_edge_mask(uint8_t p) {
	p &= 0x7f;
	if (p == 0x7f)
		return 0xff;
	g_bit = 0xff;
	uint8_t bit;
	do {
		g_bit++;
		bit = static_cast<uint8_t>(p & 1);
		p = static_cast<uint8_t>(p >> 1);
	} while (bit);
	return gmLMASK[g_bit];
}

// Blend `mask` worth of the current fill pattern into screen column `col`,
// keeping the screen's existing pixels wherever the mask is clear.
static void blend_byte(uint8_t mask, int col) {
	uint16_t addr = static_cast<uint16_t>(g_row_addr + col);
	uint8_t kept = static_cast<uint8_t>((mask ^ s_screenmem[addr]) & 0x7f);
	uint8_t pattern = s_patternData[(col & 3) | g_pat_base];
	write_screen(addr, static_cast<uint8_t>((pattern & mask) | kept));
}

// Paint the maximal run of set pixels through the seed g_pos on row g_row,
// bounded by clear pixels or the clip edges. Leaves the run's left edge in
// g_edge and its right edge in g_pos.
static void paint_span() {
	int col = col_of(g_pos);
	g_bit = bit_of(g_pos);

	// Seed byte: intersect the right-edge and left-edge masks so we paint only
	// the run that actually touches the seed bit.
	g_edge_bit = 0xff;
	uint8_t existing = s_screenmem[g_row_addr + col];
	uint8_t right_mask = right_edge_mask(static_cast<uint8_t>(existing | gmRMASK[g_bit]));
	uint8_t left_input = static_cast<uint8_t>(gmLMASK[g_bit] | existing);
	g_bit = 7;
	uint8_t mask = static_cast<uint8_t>(left_edge_mask(left_input) & right_mask);

	// Walk left across whole columns, painting each, until a right-edge boundary
	// turns up or we reach the clip edge.
	uint8_t left_bit;
	for (;;) {
		blend_byte(mask, col);
		if (g_edge_bit != 0xff) { left_bit = g_edge_bit; break; }
		if (--col < g_clip_left) { col++; left_bit = 0xff; break; }
		mask = right_edge_mask(s_screenmem[g_row_addr + col]);
	}

	// The left edge sits one pixel past the boundary bit.
	uint8_t edge_bit = left_bit + 1;
	if (edge_bit == 7) { edge_bit = 0; col++; }
	g_edge = to_pos(col, edge_bit);

	// Walk right from the seed, painting whole columns, until the run ends (the
	// bit index runs off the left of a column with no continuation) or we reach
	// the clip edge.
	col = col_of(g_pos);
	for (;;) {
		if (g_bit == 7 && col != g_clip_right) {
			col++;
			blend_byte(left_edge_mask(s_screenmem[g_row_addr + col]), col);
			continue;
		}
		g_bit--;
		if (g_bit & 0x80) { g_bit = 6; col--; }
		g_pos = to_pos(col, g_bit);
		return;
	}
}

// Append a span (edges, row, direction) to the work queue, unless its row is the
// clip edge in that direction.
static void enqueue_span(uint8_t row, int left, int right, uint8_t dir) {
	uint8_t clip_row = (dir == DIR_UP) ? g_clip_top : g_clip_bottom;
	if (row == clip_row)
		return;

	Span &s = g_queue[g_tail];
	s.row = row;
	s.left = left;
	s.right = right;
	s.dir = dir;
	g_tail = (g_tail + 1) & QUEUE_MASK;   // never expected to overflow 32 spans
}

enum WalkAction {
	WALK_ADVANCE,   // keep walking the parent span for more runs
	WALK_DONE       // this entry is finished
};

// Paint the run at the current position and queue its follow-ups: a same-
// direction continuation covering the whole run, plus opposite-direction spans
// for any part of the run overhanging the parent span on the left or right.
static WalkAction paint_run(const Span &parent) {
	paint_span();
	enqueue_span(g_row, g_edge, g_pos, parent.dir);
	int run_right = g_pos;

	// Left overhang: the part of the run reaching left of the parent's left edge
	// is re-scanned in the opposite vertical direction.
	if (g_edge < parent.left - 1)
		enqueue_span(g_row, g_edge, parent.left - 2, parent.dir ^ 1);

	// Right overhang, likewise. If there is one we are finished with this run; if
	// the run ended inside the parent we keep walking from its right edge.
	if (parent.right + 1 < run_right) {
		enqueue_span(g_row, parent.right + 2, run_right, parent.dir ^ 1);
		return WALK_DONE;
	}
	if (parent.right + 1 == run_right)
		return WALK_DONE;

	g_pos = run_right;
	return WALK_ADVANCE;
}

// Handle a set pixel found while walking the parent span. If an opposite-
// direction span already queued covers this run, merge with it instead of
// painting twice; otherwise paint the run.
static WalkAction handle_run(const Span &parent) {
	for (uint8_t i = (g_head + 1) & QUEUE_MASK; i != g_tail; i = (i + 1) & QUEUE_MASK) {
		Span &other = g_queue[i];
		if (other.row != g_row)      continue;
		if (other.dir == parent.dir) continue;
		if (g_pos < other.left || g_pos > other.right)
			continue;                              // run not inside `other`'s extent

		// Extend `other` left to cover the parent run reaching past its left edge.
		if (g_pos != other.left && other.left < parent.left - 1)
			enqueue_span(g_row, other.left, parent.left - 2, other.dir);

		if (other.right <= parent.right) {
			// `other` lies entirely within the parent run: absorb it and keep
			// walking from its right edge.
			other.dir = DIR_DISABLED;
			g_pos = other.right;
			return WALK_ADVANCE;
		}

		// `other` extends past the parent run: trim its left edge to just past
		// the parent's right edge and stop (it will be painted on its own turn).
		other.left = parent.right + 1;
		return WALK_DONE;
	}

	return paint_run(parent);
}

// Process one queued span: walk the parent span on the adjacent row, painting
// (or merging) every connected run of set pixels it overlaps.
static void process_queue_head() {
	Span parent = g_queue[g_head];
	if (parent.dir == DIR_DISABLED)    // absorbed by an earlier merge
		return;

	g_row = (parent.dir == DIR_UP) ? parent.row - 1 : parent.row + 1;
	g_pos = parent.left;
	set_row_address();

	for (;;) {
		if (pixel_is_set() && handle_run(parent) == WALK_DONE)
			return;
		if (g_pos == parent.right)
			return;
		g_pos++;
	}
}

// op14 flood-fill driver: paint the seed run, queue its up/down children, then
// drain the queue.
static void run_fill() {
	set_row_address();
	if (!pixel_is_set())
		return;
	paint_span();

	g_head = g_tail = 0;
	enqueue_span(g_row, g_edge, g_pos, DIR_UP);
	enqueue_span(g_row, g_edge, g_pos, DIR_DOWN);

	long guard = 0;
	while (g_head != g_tail) {
		if (++guard > 2000000)         // safety net against pathological input
			return;
		process_queue_head();
		g_head = (g_head + 1) & QUEUE_MASK;
	}
}

// Public flood-fill entry point: seed point (x,y), even/odd fill patterns and
// the clip rectangle (left/right columns, top/bottom rows).
static void apple2_flood_fill(uint16_t x, uint8_t y, uint8_t pat_even, uint8_t pat_odd,
		uint8_t left, uint8_t right, uint8_t top, uint8_t bottom) {
	g_pat_even = pat_even;
	g_pat_odd  = pat_odd;
	g_clip_left = left;  g_clip_right  = right;
	g_clip_top  = top;   g_clip_bottom = bottom;

	g_row = y;
	g_pos = x;   // pixel coordinate == column*7 + bit
	run_fill();
}

// ---- Shape / brush drawing (ported) -------------------------------------------

// One 8-row brush quadrant: for each row, spread the brush byte across two adjacent
// hi-res bytes by a 7-bit rotate (collecting bit 6 into the right byte), then
// blend it into the screen using the current fill colour pattern.
static void draw_bitmap(a2_brush_ctx *ctx) {
	for (int row = 0; row < 8; row++) {
		uint16_t address = CALC_APPLE2_ADDRESS(ctx->scanline) + ctx->seed_column;
		int idx = ctx->bitmap_index + row;
		if (idx < (int)sizeof(s_brushBitmaps) && valid_offset(address)) {
			uint8_t b = s_brushBitmaps[idx];
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
				uint8_t pat0 = s_patternData[(ctx->seed_column & 3) | pat_base];
				uint8_t pat1 = s_patternData[((ctx->seed_column + 1) & 3) | pat_base];

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
	if (ctx.bitmap_index + BRUSH_PART_SIZE * BRUSH_TOTAL_PARTS > (int)sizeof(s_brushBitmaps))
		return;
	draw_brush_quadrant(&ctx, 0, 0);
	ctx.bitmap_index += BRUSH_PART_SIZE;
	draw_brush_quadrant(&ctx, 1, 0);
	ctx.bitmap_index += BRUSH_PART_SIZE;
	draw_brush_quadrant(&ctx, 0, BRUSH_PART_HEIGHT);
	ctx.bitmap_index += BRUSH_PART_SIZE;
	draw_brush_quadrant(&ctx, 1, BRUSH_PART_HEIGHT);
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
static void draw_text_glyph(uint16_t x, uint8_t y, uint8_t ch, bool normal,
                            uint8_t pat_even, uint8_t pat_odd) {
	const uint8_t *glyph = &s_fontGlyphs[(ch & 0x7f) * 8];
	uint8_t col = (uint8_t)(x / COL_BITS);
	uint8_t off = (uint8_t)(x % COL_BITS);
	uint8_t scan = y;
	for (int row = 0; row < 8; row++, scan++) {
		uint8_t g = glyph[row];
		if (g == 0)
			continue;
		uint8_t carry = 0, b = g;
		for (uint8_t n = off; n != 0; n--) {
			carry = (uint8_t)((carry << 1) | ((b & 0x7f) >> 6));
			b = (uint8_t)(b << 1);
		}
		uint8_t mask_a = (uint8_t)(b & 0x7f);   // bits painted into column `col`
		uint8_t mask_b = carry;                 // bits carried into column `col+1`
		uint16_t addr = (uint16_t)(CALC_APPLE2_ADDRESS(scan) + col);
		if (normal) {
			// FUN_102e $1099: XOR plot. mask_a/mask_b are 7-bit, so bit 7 (the
			// hi-res palette bit) is preserved and the text stays white.
			if (mask_a && valid_offset(addr))
				write_screen(addr, (uint8_t)(s_screenmem[addr] ^ mask_a));
			if (mask_b && valid_offset(addr + 1))
				write_screen(addr + 1, (uint8_t)(s_screenmem[addr + 1] ^ mask_b));
		} else {
			// FUN_102e $1057: blend with the current fill-colour pattern.
			uint8_t pat_base = (uint8_t)(((scan & 1) ? pat_odd : pat_even) << 2);
			if (mask_a && valid_offset(addr)) {
				uint8_t pat = s_patternData[(col & 3) | pat_base];
				write_screen(addr, (uint8_t)(((mask_a ^ 0x7f) & s_screenmem[addr]) | ((mask_a | 0x80) & pat)));
			}
			if (mask_b && valid_offset(addr + 1)) {
				uint8_t pat = s_patternData[((col + 1) & 3) | pat_base];
				write_screen(addr + 1, (uint8_t)(((mask_b ^ 0x7f) & s_screenmem[addr + 1]) | ((mask_b | 0x80) & pat)));
			}
		}
	}
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

// ---- Circle (op11) ----
// A midpoint circle plotting 8 symmetric points per step via HPLOT (HPOSN +
// plot). Only x in 0..279, y in 0..159.

static void plot_circle_point(int x, int y, a2_ctx *ctx) {
	if (x < 0 || x >= 280 || y < 0 || y >= APPLE2_SCREEN_HEIGHT - 32)
		return;
	set_draw_position((uint16_t)x, (uint8_t)y, ctx);
	plot_pixel(ctx);
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
		uint16_t base = CALC_APPLE2_ADDRESS(row);
		uint8_t pat_base = ((row & 1) ? pat_odd : pat_even) * 4;
		for (int col = ctx->fill_left; col <= ctx->fill_right && col < APPLE2_SCREEN_COLS; col++)
			write_screen(base + col, s_patternData[(col & 3) | pat_base]);
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
		ctx->text_x = (uint16_t)(*ptr++ + (param & 1 ? 256 : 0));
		ctx->text_y = *ptr++;
		break;

	case OPCODE_SET_PEN_COLOR: // 2
		set_color(param, ctx);
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
		case 2: // read 4 bytes of bounds
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

// ---- Apple hi-res -> RGB (NTSC artifact colours, from MAME) -----------

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
	ctx.pat_even = s_colorPatternSubindices[0x4d * 2];
	ctx.pat_odd = s_colorPatternSubindices[0x4d * 2 + 1];
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
