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
#include <cstring>
#include <vector>

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
// via gmResetScreen() only when starting a fresh room.
static uint8_t s_screenmem[A2_SCREEN_MEM_SIZE];

static inline bool valid_offset(uint16_t off) { return off <= MAX_SCREEN_ADDR; }

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

struct WriteOp { uint16_t offset; uint8_t value; };

// A WriteOp with this (out-of-page) offset is not a byte write but a DELAY
// marker: op13 emitted it, and value holds the delay's operand. The reveal
// pauses when it reaches one (the original Graphics Magician paused mid-paint).
#define DELAY_MARKER 0xffff
// Wall-clock length of one delay unit, in slow-draw ticks. Measured from the
// Graphics Magician op13 handler (the DELAY routine at $09bb in the Apple II
// std-hires interpreter, byte-identical across all four titles): a 3-level
// counting loop "LDX #$80; LDY #$A0; DEY/BNE; DEX/BNE; DEC operand; BNE" =
// 128 * 160 inner iterations per unit = ~103,176 6502 cycles, which at the
// Apple II's 1.0205 MHz is ~101 ms per unit. At the host's 10 ms slow-draw tick
// (GM_SLOW_TICK_MS) that is ~10 ticks per unit.
#define DELAY_TICKS_PER_UNIT 10

static uint8_t s_slowScreen[A2_SCREEN_MEM_SIZE]; // progressively-revealed page
static std::vector<WriteOp> s_ops;               // ordered byte writes of this image
static size_t s_opsDrawn = 0;                    // how many ops are on s_slowScreen
static bool s_recordOps = false;                 // record ops for slow reveal?
static int s_pauseTicks = 0;                      // reveal ticks still owed to a DELAY

// Bounding range of screen rows touched since the last blit, so the host can
// repaint just the changed band instead of the whole window each tick.
static int s_dirtyMin = 0x7fffffff, s_dirtyMax = -1;

// offset -> screen row (0..191), or 0xff for the inter-row gaps in the hi-res
// address space. Built once; lets a reveal map a byte write back to its row.
static uint8_t s_rowOfOffset[A2_SCREEN_MEM_SIZE];
static bool s_rowTableInit = false;

static void mark_row_dirty(uint16_t offset) {
	if (!s_rowTableInit) {
		memset(s_rowOfOffset, 0xff, sizeof(s_rowOfOffset));
		for (int row = 0; row < 192; row++) {
			uint16_t base = CALC_APPLE2_ADDRESS(row);
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
		if (s_recordOps)
			s_ops.push_back({offset, value});
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
		uint8_t mask_a, mask_b;                 // mask_a -> col, mask_b -> col+1
		gm_spread7(g, off, &mask_a, &mask_b);
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
		uint16_t base = CALC_APPLE2_ADDRESS(row);
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
		if (s_recordOps && units)
			s_ops.push_back({ DELAY_MARKER, units });
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
						write_screen(CALC_APPLE2_ADDRESS(row) + col, b);
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
// 0..h-1, sampling the 560-wide artifact colours down to w columns.
static void screenmem_to_rgba(const uint8_t *src, uint32_t *out, int w, int h) {
	for (int row = 0; row < h; row++) {
		unsigned const address = (((row / 8) & 0x07) << 7) + (((row / 8) & 0x18) * 5) + ((row & 7) << 10);
		const uint8_t *vram_row = src + address;
		uint16_t words[40];
		gm_compute_row_words(vram_row, words);
		uint8_t colors560[560];
		gm_render_line_colors(words, colors560);
		for (int x = 0; x < w; x++) {
			uint32_t rgb = gm_apple2_palette[colors560[(2 * x < 560) ? 2 * x : 559] & 0x0f];
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
	s_ops.clear();
	s_opsDrawn = 0;
	s_pauseTicks = 0;
}

// ---- Slow-draw control (host-driven) ----------------------------------------

void gmSetSlowDraw(bool on) { s_recordOps = on; }

// True while there is reveal work left: recorded ops still to paint, or a DELAY
// pause still owed.
bool gmSlowDrawActive() {
	return s_recordOps && (s_opsDrawn < s_ops.size() || s_pauseTicks > 0);
}

// Bytes whose offset is adjacent to, or whose value matches, the previous one
// belong to the same visual run; revealing them together avoids seams.
static bool ops_adjacent(const WriteOp &a, const WriteOp &b) {
	return ((int)b.offset >= (int)a.offset - 1 && (int)b.offset <= (int)a.offset + 1) ||
	       b.value == a.value;
}

// Apply up to `budget` recorded ops onto the visible page, extending the chunk
// across adjacent runs. A DELAY marker halts this tick's reveal and owes a run
// of pause ticks (during which nothing is revealed). Returns true while any
// reveal work -- ops or an outstanding pause -- remains.
bool gmAdvanceSlowDraw(int budget) {
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
		s_slowScreen[s_ops[i].offset] = s_ops[i].value;
		mark_row_dirty(s_ops[i].offset);
		keep_going = (i + 1 < s_ops.size()) && s_ops[i + 1].offset != DELAY_MARKER &&
		             ops_adjacent(s_ops[i], s_ops[i + 1]);
	}
	s_opsDrawn = i;
	return s_opsDrawn < s_ops.size();
}

// Reveal the rest of the image at once (e.g. on a window resize, or when the
// reveal is cancelled). Leaves the visible page identical to the final page.
void gmFinishSlowDraw() {
	for (; s_opsDrawn < s_ops.size(); s_opsDrawn++) {
		if (s_ops[s_opsDrawn].offset == DELAY_MARKER)
			continue;                      // pauses collapse when finishing at once
		s_slowScreen[s_ops[s_opsDrawn].offset] = s_ops[s_opsDrawn].value;
		mark_row_dirty(s_ops[s_opsDrawn].offset);
	}
	s_pauseTicks = 0;
}

// Hand back the row band touched since the last call (inclusive), and reset it.
// Returns false if nothing changed.
bool gmSlowConsumeDirty(int *y0, int *y1) {
	if (s_dirtyMax < 0)
		return false;
	*y0 = s_dirtyMin;
	*y1 = s_dirtyMax;
	s_dirtyMin = 0x7fffffff;
	s_dirtyMax = -1;
	return true;
}

void gmBlitSlowToSurface(uint32_t *out, int w, int h) {
	screenmem_to_rgba(s_slowScreen, out, w, h);
}

// Diagnostic accessor (offline pixel-diff harness): the raw 0x2000-byte page.
const uint8_t *gmPagePtr() { return s_screenmem; }
void gmSetPage(const uint8_t *p) { memcpy(s_screenmem, p, A2_SCREEN_MEM_SIZE); }

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

void gmCaptureCMPanel(int col0, int col1) {
	memcpy(s_cmPanel, s_screenmem, A2_SCREEN_MEM_SIZE);
	s_cmPanelCol0 = col0;
	s_cmPanelCol1 = col1;
	s_cmPanelValid = true;
}

bool gmCMPanelValid() { return s_cmPanelValid; }

void gmOverlayCMPanel() {
	if (!s_cmPanelValid)
		return;
	for (int row = 0; row < APPLE2_SCREEN_HEIGHT; row++) {
		uint16_t base = CALC_APPLE2_ADDRESS(row);
		for (int col = s_cmPanelCol0; col <= s_cmPanelCol1; col++) {
			s_screenmem[base + col] = s_cmPanel[base + col];
			s_slowScreen[base + col] = s_cmPanel[base + col];
		}
	}
}

// Per-grain position, ported byte-faithfully from cm_draw_hourglass_grain
// ($4347). `idx` is the grain index (0-based); the original treats idx 0 as a
// no-op. Returns false for grains that stamp nothing. Apple hi-res x (0..279),
// y (0..191). The pile is centred at x=245 (column ~35): even grains step right
// (245+half), odd grains step left (245-half); the grain falls one "band" of 25
// lower (y -= 1 per band, then y -= half within the band).
static bool cmHourglassGrainPos(uint8_t idx, uint16_t *outX, uint8_t *outY) {
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

void gmDrawCMHourglass(int sand) {
	if (sand <= 0)
		return;

	gm_vector_ctx gv = {};
	gv.screenmem = s_screenmem;
	gv.write = gm_write_adapter;
	gv.user = nullptr;
	gv.pattern_data = s_patternData;
	gv.brush_bitmaps = s_brushBitmaps;

	// White grains: the engine's refill path sets SET_FILL_COLOR 0x34, which
	// maps to pattern sub-index (7,7) = the 0xff (all-pixels-white) pattern. (The
	// drain path instead uses 0x35 -> pattern 4 = 0x80 = black, to erase the top
	// grain; we redraw the whole pile each turn, so we only ever paint white.)
	uint8_t pat_even = s_colorPatternSubindices[0x34 * 2];
	uint8_t pat_odd = s_colorPatternSubindices[0x34 * 2 + 1];

	// The original draws grains 0..sand-1 (grain 0 is a no-op).
	for (int idx = 0; idx < sand && idx < 256; idx++) {
		uint16_t x;
		uint8_t y;
		if (!cmHourglassGrainPos((uint8_t)idx, &x, &y))
			continue;
		gm_draw_brush(x, y, 0, pat_even, pat_odd, &gv);
	}

	// Keep the progressively-revealed page in step so either blit path shows the
	// finished pile (the hourglass is not part of the slow reveal).
	memcpy(s_slowScreen, s_screenmem, A2_SCREEN_MEM_SIZE);
}

void gmDrawImage(const uint8_t *data, size_t size) {
#ifdef GM_TRACE
	g_imgBase = data;
#endif
	// Each image is its own reveal: drop the previous image's op list (a room
	// reset already cleared it; an item overlay reaches here without one, and
	// must record only its own bytes onto whatever the room left visible).
	s_ops.clear();
	s_opsDrawn = 0;
	s_pauseTicks = 0;

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
	if (!s_recordOps)
		memcpy(s_slowScreen, s_screenmem, A2_SCREEN_MEM_SIZE);
}

void gmBlitToSurface(uint32_t *out, int w, int h) {
	screenmem_to_rgba(s_screenmem, out, w, h);
}

void gmSetLegacyFormat(bool on) { s_legacyFormat = on; }

} // namespace Comprehend
} // namespace Glk
