/* hdos_talisman.cpp -- DOS Talisman CGA picture renderer.
 *
 * Renders the Penguin Graphics Magician vector stream into a 280×160, 2-bpp
 * (4-colour) logical raster that matches the native picture interpreter in the
 * DOS release NOVEL.EXE / NOVEL1.EXE.  On a colour adapter the interpreter
 * drives CGA 320×200 4-colour mode and selects palette 1, high intensity
 * (black / cyan / magenta / white) -- see the picture init at 0x22ff (INT 10h
 * AH=0Bh).  Each primitive here was derived from, and cross-checked against,
 * the Ghidra disassembly of NOVEL1.EXE (analysis in the project memory notes):
 *
 *   PicInterpretImageOpcodes 1f56 -- dispatcher; 16-handler table at 0x9d5d,
 *       default brush 5, brush table pointer 0x9d81
 *   line = major-axis Bresenham, box = four chained lines, circle = 8-octant
 *   midpoint, brush/glyph = per-row bit blit, flood fill = scanline fill with a
 *   visited bitmap (boundary == black, matching the native per-pixel mask test).
 *
 * Pixel model:
 *   - 2 bpp, 4 pixels per byte, LSB-first (pixel p in bits (2p+1):(2p))
 *   - Values are CGA palette-1 colour indices: 0=black, 1=cyan, 2=magenta, 3=white
 *   - Fill table (DS:0x9860, file 0xc8d0): 76 entries × 2 bytes each
 *     (even_col / odd_col bytes, always equal in this release); entry i is
 *     the 4-pixel 2-bpp tile used when SET_FILL_COLOR (op6) byte = i.
 *   - Brush bitmaps (DS:0x9d81, file 0xcdf1): 8 × 32 bytes;
 *     quadrant order Q0..Q3 = upper-left, lower-left, upper-right, lower-right;
 *     within each quadrant bit-7 = leftmost pixel.
 *   - Font glyphs (DS:0x9e81, file 0xcef1): 96 chars × 8 bytes;
 *     same encoding as the brush bytes (bit-7 = leftmost pixel).
 *
 * All three tables are byte-identical between NOVEL.EXE and NOVEL1.EXE.
 *
 * Like graphics_magician.cpp (the Apple II equivalent, which shares the opcode
 * format) this file is self-contained -- no Common::/Graphics:: dependencies --
 * so it can be exercised by an offline harness; the host loads NOVEL.EXE and
 * hands the bytes to hdosInstallDrawingTables().
 */

#include "hdos_talisman.h"
#include "graphics_magician.h"  // Opcode enum
#include <cstdlib>
#include <cstring>
#include <vector>

namespace Glk {
namespace Comprehend {

// ---- Drawing tables -----------------------------------------------------------

static uint8_t s_fillTable[76];       // pattern[idx] = 2-bpp byte (4-pixel tile)
static uint8_t s_fillSubindex[512];   // SET_FILL_COLOR byte -> (even,odd) pattern idx
static uint8_t s_brushBitmaps[256];   // 8 brushes × 32 bytes
static uint8_t s_fontGlyphs[96 * 8];  // chars 32-127, 8 bytes each

static bool s_haveDrawingTables = false;

bool hdosHaveDrawingTables() { return s_haveDrawingTables; }

bool hdosInstallDrawingTables(const uint8_t *exe, size_t size) {
    if (!exe)
        return false;

    // Locate the three drawing tables inside the DOS interpreter's DGROUP data
    // segment.  In the Talisman release (NOVEL.EXE / NOVEL1.EXE, which carry
    // byte-identical data tables) they sit at DS offsets 0x9860 / 0x9d81 /
    // 0x9e81, i.e. file offsets 0xc8d0 / 0xcdf1 / 0xcef1.  Rather than hash the
    // whole binary -- which breaks on every re-release -- identify the fill
    // table by its unmistakable signature (the first eight 2-bpp CGA fill
    // patterns: black / cyan / magenta / white / and the four single-bit
    // dithers) and derive the brush and font tables from it.  These offsets
    // were cross-checked against the binary loaded in Ghidra (fill table at
    // DS:0x9860, brush pointer 0x9d81 in the picture dispatcher at 0x1f56).
    static const uint8_t kFillSig[] = {
        0x00,0x00, 0x55,0x55, 0xaa,0xaa, 0xff,0xff,
        0x11,0x11, 0x44,0x44, 0x22,0x22, 0x88,0x88,
    };
    const size_t kFillToSub   = 0xca5c - 0xc8d0; // 0x18c (DS:0x99ec)
    const size_t kFillToBrush = 0xcdf1 - 0xc8d0; // 0x521
    const size_t kFillToFont  = 0xcef1 - 0xc8d0; // 0x621

    size_t fillBase = SIZE_MAX;
    if (size >= 0xc8d0 + sizeof(kFillSig) &&
        std::memcmp(exe + 0xc8d0, kFillSig, sizeof(kFillSig)) == 0) {
        fillBase = 0xc8d0;  // fast path: the known Talisman offset
    } else {
        for (size_t i = 0; i + sizeof(kFillSig) <= size; i++) {
            if (std::memcmp(exe + i, kFillSig, sizeof(kFillSig)) == 0) {
                fillBase = i;
                break;
            }
        }
    }
    if (fillBase == SIZE_MAX)
        return false;
    if (fillBase + kFillToFont + sizeof(s_fontGlyphs) > size)
        return false;

    // Fill table: 76 entries × 2 bytes each (even_col, odd_col); even==odd for
    // every entry in this release, so keep just one byte per entry.
    //
    // The interpreter writes pixels MSB-first within a CGA byte (pixel 0 in bits
    // 7:6, pixel 3 in bits 1:0 -- see PicPlotPixel2bpp at 0x2470, which shifts by
    // ((x&3)^3)*2), so a stored fill byte's leftmost pixel is its top bit pair.
    // This renderer keeps its framebuffer LSB-first (pixel 0 in bits 1:0), so
    // mirror each entry's four 2-bit groups once here; afterwards every LSB-first
    // sample -- including fill_hline's direct whole-byte store -- lands the
    // pattern at the correct column phase.  Solid tiles (0x00/0x55/0xaa/0xff) are
    // palindromic and unaffected; only true dithers change phase.
    const uint8_t *fill = exe + fillBase;
    for (int i = 0; i < 76; i++) {
        uint8_t b = fill[i * 2];
        s_fillTable[i] = (uint8_t)(((b & 0x03) << 6) | ((b & 0x0c) << 2) |
                                   ((b & 0x30) >> 2) | ((b & 0xc0) >> 6));
    }

    // Fill-colour subindex table (DS:0x99ec): the SET_FILL_COLOR (op6) byte
    // indexes this for a pair of pattern-table indices, one per row parity.
    // The interpreter (FUN_23dd) reads subtable[byte*2] as a little-endian word
    // and uses its two bytes for even/odd rows.
    std::memcpy(s_fillSubindex, exe + fillBase + kFillToSub, sizeof(s_fillSubindex));

    std::memcpy(s_brushBitmaps, exe + fillBase + kFillToBrush, sizeof(s_brushBitmaps));
    std::memcpy(s_fontGlyphs,   exe + fillBase + kFillToFont,  sizeof(s_fontGlyphs));

    s_haveDrawingTables = true;
    return true;
}

// ---- 2-bpp (CGA) framebuffer -------------------------------------------------
//
// 280 pixels wide × 160 rows.  Each byte holds 4 pixels in two-bit pairs,
// LSB-first: pixel p occupies bits (2p+1):(2p) of byte (x>>2) in row y.
// Values are CGA colour indices in palette 1, high intensity:
//   0 = black, 1 = cyan, 2 = magenta, 3 = white.

#define HDOS_WIDTH   280
#define HDOS_HEIGHT  160
#define HDOS_STRIDE  70   // 280 × 2 bpp / 8 = 70 bytes/row

static uint8_t s_screenmem[HDOS_HEIGHT * HDOS_STRIDE];

// ---- Slow ("animated") draw -------------------------------------------------
//
// Mirrors graphics_magician.cpp: when recording is on (hdosSetSlowDraw(true)),
// every framebuffer byte mutation is appended to an ordered op list as well as
// applied to the final page.  The host re-applies those ops onto a separate
// visible page (s_slowScreen) a chunk at a time on a Glk timer, re-blitting
// after each chunk, so the picture is revealed in the order it was painted --
// the few seconds the real interpreter took to fill the CGA page.  op13 emits a
// DELAY marker that pauses the reveal.  See the gm* counterparts for the host
// driver; the two renderers expose the same slow-draw contract.

struct WriteOp { uint16_t offset; uint8_t value; };

// A WriteOp with this (out-of-page) offset is a DELAY marker rather than a byte
// write: op13 emitted it and value holds the delay's operand.
#define DELAY_MARKER 0xffff
// Wall-clock length of one op13 delay unit, in slow-draw ticks.  Kept equal to
// the Apple renderer's pacing so both titles animate at a comparable speed at
// the host's 10 ms tick (GM_SLOW_TICK_MS).
#define DELAY_TICKS_PER_UNIT 10

static uint8_t s_slowScreen[HDOS_HEIGHT * HDOS_STRIDE]; // progressively revealed
static std::vector<WriteOp> s_ops;        // ordered byte writes of this image
static size_t s_opsDrawn = 0;             // how many ops are on s_slowScreen
static bool s_recordOps = false;          // record ops for slow reveal?
static int s_pauseTicks = 0;              // reveal ticks still owed to a DELAY
static int s_dirtyMin = 0x7fffffff, s_dirtyMax = -1; // changed row band

// Apply one byte to the final page and, while recording, log it for the reveal.
static inline void store_byte(int off, uint8_t value) {
    s_screenmem[off] = value;
    if (s_recordOps)
        s_ops.push_back({ (uint16_t)off, value });
}

static inline void write_2bpp(int x, int y, uint8_t val) {
    if ((unsigned)x >= HDOS_WIDTH || (unsigned)y >= HDOS_HEIGHT) return;
    int off = y * HDOS_STRIDE + (x >> 2);
    int shift = (x & 3) << 1;
    store_byte(off, (uint8_t)((s_screenmem[off] & ~(0x3u << shift)) | ((val & 0x3u) << shift)));
}

static inline uint8_t read_2bpp(int x, int y) {
    if ((unsigned)x >= HDOS_WIDTH || (unsigned)y >= HDOS_HEIGHT) return 0;
    return (s_screenmem[y * HDOS_STRIDE + (x >> 2)] >> ((x & 3) << 1)) & 0x3;
}

void hdosResetScreen(bool white) {
    memset(s_screenmem, white ? 0xff : 0x00, sizeof(s_screenmem));
    // A reset starts a fresh page: it appears instantly on the visible page too,
    // and any pending reveal is dropped.
    memset(s_slowScreen, white ? 0xff : 0x00, sizeof(s_slowScreen));
    s_ops.clear();
    s_opsDrawn = 0;
    s_pauseTicks = 0;
}

// ---- Fill helpers ------------------------------------------------------------

// Resolve a SET_FILL_COLOR (op6) selector byte to the 4-pixel pattern tile for
// row `y`.  The selector indexes the subindex table for a pair of pattern-table
// indices; even and odd rows use different ones, which is what makes the
// two-row CGA dithers (e.g. the cyan/black checkerboard background).  This
// mirrors FUN_23dd + the per-row pattern fetch in the DOS interpreter.
static inline uint8_t fill_pattern(uint8_t sel, int y) {
    uint8_t sub = s_fillSubindex[(unsigned)sel * 2 + (y & 1)];
    return (sub < 76) ? s_fillTable[sub] : 0x00;
}

// Fill pixels [l..r] on row y with the current fill pattern (selector `sel`).
// The pattern tile covers 4 pixels and is aligned to the byte grid by absolute
// x phase (x & 3), so adjacent fills tile seamlessly.
static void fill_hline(int y, int l, int r, uint8_t sel) {
    if ((unsigned)y >= HDOS_HEIGHT) return;
    if (l < 0) l = 0;
    if (r >= HDOS_WIDTH) r = HDOS_WIDTH - 1;
    if (l > r) return;

    const uint8_t fb = fill_pattern(sel, y);
    const int base = y * HDOS_STRIDE;
    int bl = l >> 2, br = r >> 2;

    if (bl == br) {
        uint8_t v = s_screenmem[base + bl];
        for (int x = l; x <= r; x++) {
            int shift = (x & 3) << 1;
            v = (v & ~(0x3u << shift)) | (((fb >> shift) & 0x3u) << shift);
        }
        store_byte(base + bl, v);
        return;
    }
    // Left partial byte
    {
        uint8_t v = s_screenmem[base + bl];
        for (int x = l, xe = (bl + 1) << 2; x < xe; x++) {
            int shift = (x & 3) << 1;
            v = (v & ~(0x3u << shift)) | (((fb >> shift) & 0x3u) << shift);
        }
        store_byte(base + bl, v);
    }
    // Whole bytes in the middle
    for (int b = bl + 1; b < br; b++) store_byte(base + b, fb);
    // Right partial byte
    {
        uint8_t v = s_screenmem[base + br];
        for (int x = br << 2; x <= r; x++) {
            int shift = (x & 3) << 1;
            v = (v & ~(0x3u << shift)) | (((fb >> shift) & 0x3u) << shift);
        }
        store_byte(base + br, v);
    }
}

// ---- Flood fill (op14 PAINT) -------------------------------------------------
//
// Pattern-aware scanline flood fill matching the native PicOp14Paint (0x2630).
// The native boundary test (FUN_29a0 at 0x29a0) masks the screen word per
// pixel and treats any pixel whose value is NOT 0 (i.e. not black) as fillable;
// black is the only boundary -- black drawn lines hem the region in, but the
// fill flows freely over any previously-drawn cyan / magenta / white detail.
//
// The fill lays down a row-parity dither (FUN_24a4: pattern selected by
// (y + [0xb031]) & 1, with [0xb031] == 0, i.e. plain y&1), so the pixels it
// writes are themselves a mix of colours including black.  The native code
// avoids re-treating those freshly-painted black pixels as new boundaries via
// its 32-entry span queue + per-entry "done" flags; the equivalent here is a
// single visited bitmap.  Predicate: a pixel is fillable iff it is in bounds,
// not yet visited, and currently non-black.  Because every pixel we paint is
// marked visited in the same step, an unvisited pixel has never been touched by
// this fill -- so its value still reflects the original picture, and "non-black"
// means "not an original boundary line."  This is a faithful, leak-free port of
// the native "boundary == black" semantics without the circular-queue
// bookkeeping.

static uint8_t s_fillVisited[HDOS_HEIGHT * HDOS_WIDTH];

static void hdos_flood_fill(int seed_x, int seed_y, uint8_t fill_idx) {
    auto fillable = [&](int x, int y) -> bool {
        if ((unsigned)x >= HDOS_WIDTH || (unsigned)y >= HDOS_HEIGHT) return false;
        if (s_fillVisited[y * HDOS_WIDTH + x]) return false;
        return read_2bpp(x, y) != 0;       // black is the only boundary
    };
    if (!fillable(seed_x, seed_y)) return;

    memset(s_fillVisited, 0, sizeof(s_fillVisited));

    struct Pt { int16_t x, y; };
    std::vector<Pt> stack;
    stack.reserve(1024);
    stack.push_back({ (int16_t)seed_x, (int16_t)seed_y });

    long guard = 0;
    while (!stack.empty() && ++guard < 4000000) {
        Pt p = stack.back(); stack.pop_back();
        if (!fillable(p.x, p.y)) continue;

        int l = p.x, r = p.x;
        while (fillable(l - 1, p.y)) l--;
        while (fillable(r + 1, p.y)) r++;

        uint8_t *vis = &s_fillVisited[p.y * HDOS_WIDTH];
        for (int x = l; x <= r; x++) vis[x] = 1;   // claim before painting
        fill_hline(p.y, l, r, fill_idx);

        for (int x = l; x <= r; x++) {
            if (fillable(x, p.y - 1)) stack.push_back({ (int16_t)x, (int16_t)(p.y - 1) });
            if (fillable(x, p.y + 1)) stack.push_back({ (int16_t)x, (int16_t)(p.y + 1) });
        }
    }
}

// ---- Line drawing (op10 DRAW_LINE / op9 DRAW_BOX) ----------------------------

// Major-axis Bresenham, ported from PicDrawLineBresenham (0x2c00): the longer
// axis is stepped every pixel; the error term (initialised to minor - major)
// decides when the minor axis advances.  This exact formulation matters because
// the symmetric two-axis variant tie-breaks the other way on 45deg-ish lines,
// shifting a line -- and therefore the fill bounded by it -- by one pixel.
static void draw_line(int x0, int y0, int x1, int y1, uint8_t pen_val) {
    write_2bpp(x0, y0, pen_val);
    const int dx = abs(x1 - x0), dy = abs(y1 - y0);
    const int sx = (x0 < x1) ? 1 : -1;
    const int sy = (y0 < y1) ? 1 : -1;
    int x = x0, y = y0;
    if (dx >= dy) {              // x is the major axis
        int err = dy - dx;
        for (int i = 0; i < dx; i++) {
            x += sx;
            if (err < 0) err += dy;
            else { y += sy; err += dy - dx; }
            write_2bpp(x, y, pen_val);
        }
    } else {                     // y is the major axis
        int err = dx - dy;
        for (int i = 0; i < dy; i++) {
            y += sy;
            if (err < 0) err += dx;
            else { x += sx; err += dx - dy; }
            write_2bpp(x, y, pen_val);
        }
    }
}

// ---- Circle (op11 DRAW_CIRCLE) -----------------------------------------------
//
// Midpoint (Bresenham) circle algorithm, 8-octant, identical to the Apple
// renderer's draw_circle (graphics_magician.cpp) -- the Ghidra disassembly
// of PicDrawCircleBresenham 2b10 matches the same loop structure.

static void draw_circle(int cx, int cy, int radius, uint8_t pen_val) {
    if (radius == 0) return;
    uint8_t r = (uint8_t)radius;
    uint8_t dvar = r >> 1, i = 0;
    int xa = cx, xb = cx;
    int xr = cx + r, xl = cx - r;
    int yt = cy, yb = cy;
    int yp = cy + r, ym = cy - r;

    for (;;) {
        write_2bpp(xb, ym, pen_val); write_2bpp(xa, ym, pen_val);
        write_2bpp(xa, yp, pen_val); write_2bpp(xb, yp, pen_val);
        write_2bpp(xr, yb, pen_val); write_2bpp(xl, yb, pen_val);
        write_2bpp(xl, yt, pen_val); write_2bpp(xr, yt, pen_val);

        i++;
        const uint8_t a = (uint8_t)(dvar - i);
        if (dvar >= i) {
            dvar = a;
            if (r < i) break;
        } else {
            r--; dvar = (uint8_t)(a + r);
            if (r < i) break;
            yp--; ym++; xr--; xl++;
        }
        xa++; xb--; yt++; yb--;
    }
}

// ---- Brush / glyph column blitter --------------------------------------------
//
// Blits 8 rows of 8-pixel data onto the framebuffer.  Each byte encodes the
// row's pixels MSB-first: bit 7 = leftmost pixel (pixel 0), bit 0 = rightmost
// (pixel 7).  A set bit writes the fill-pattern pixel at that position; a
// clear bit leaves the background unchanged.

static void blit_col(const uint8_t *bytes8, int px, int py, uint8_t fill_sel) {
    for (int row = 0; row < 8; row++, py++) {
        if ((unsigned)py >= HDOS_HEIGHT) continue;
        const uint8_t b = bytes8[row];
        if (!b) continue;
        const uint8_t fb = fill_pattern(fill_sel, py);
        for (int p = 0; p < 8; p++) {
            if (!((b >> (7 - p)) & 1)) continue;
            const int x = px + p;
            if ((unsigned)x >= HDOS_WIDTH) continue;
            const uint8_t pix = (fb >> ((x & 3) << 1)) & 0x3;
            write_2bpp(x, py, pix);
        }
    }
}

// ---- Brush stamp (op12 DRAW_SHAPE) -------------------------------------------
//
// Each brush occupies 32 bytes in s_brushBitmaps.  The quadrant layout in
// NOVEL.EXE (derived from comparing DOS and Apple brush 5 byte-by-byte):
//
//   bytes  0– 7: Q0 = upper-left   → rendered at (x+0, y+0)
//   bytes  8–15: Q1 = lower-left   → rendered at (x+0, y+8)
//   bytes 16–23: Q2 = upper-right  → rendered at (x+8, y+0)
//   bytes 24–31: Q3 = lower-right  → rendered at (x+8, y+8)
//
// Derived from PicStampBrushShape (0x2967): it blits four 8-row columns,
// advancing the row by +8 between the first pair and the second, and the byte
// column by [0x9d80] (two bytes = 8 px) before the second pair -- i.e. the left
// column (bytes 0-15) is drawn before the right column (bytes 16-31), each top
// half then bottom half.  The 8-bit glyph bytes are bit-doubled to 8 pixels.

static void draw_brush(int x, int y, uint8_t brush, uint8_t fill_idx) {
    if (brush >= 8) return;
    const uint8_t *base = &s_brushBitmaps[brush * 32];
    blit_col(base,      x,     y,     fill_idx); // Q0: upper-left
    blit_col(base + 8,  x,     y + 8, fill_idx); // Q1: lower-left
    blit_col(base + 16, x + 8, y,     fill_idx); // Q2: upper-right
    blit_col(base + 24, x + 8, y + 8, fill_idx); // Q3: lower-right
}

// ---- Text glyph (op3 TEXT_CHAR / op5 TEXT_OUTLINE) --------------------------
//
// Both opcodes route through PicBlitBrushColumn2bpp (0x2a30), bit-doubling the
// glyph into 2-bpp pixels.  They differ only by the [0x9d51] mode flag:
//   op3 (TextChar, flag 1):  XOR branch -- each set glyph pixel inverts the
//       background pixel (value ^= 0b11).  On the white (3) subtitle cloud this
//       paints the letters black (3^3=0); the pen colour is NOT used.
//   op5 (TextOutline, flag 0): fill branch -- each set glyph pixel takes the
//       current fill-pattern pixel, like a brush stamp.
// Glyph encoding matches the brush encoding (bit 7 = leftmost pixel).
// Text cursor advances 8 pixels per character.

static void draw_glyph(int x, int y, uint8_t ch, bool normal,
                        uint8_t pen_val, uint8_t fill_sel) {
    (void)pen_val;
    if (ch < 32 || ch >= 128) return;
    const uint8_t *glyph = &s_fontGlyphs[(ch - 32) * 8];
    for (int row = 0; row < 8; row++) {
        const uint8_t b = glyph[row];
        if (!b) continue;
        const int py = y + row;
        if ((unsigned)py >= HDOS_HEIGHT) continue;
        const uint8_t fb = normal ? 0 : fill_pattern(fill_sel, py);
        for (int p = 0; p < 8; p++) {
            if (!((b >> (7 - p)) & 1)) continue;
            const int px = x + p;
            if ((unsigned)px >= HDOS_WIDTH) continue;
            const uint8_t pix = normal ? (uint8_t)(read_2bpp(px, py) ^ 0x3)
                                       : (fb >> ((px & 3) << 1)) & 0x3;
            write_2bpp(px, py, pix);
        }
    }
}

// ---- op15 fill rectangle (OPCODE_RESET) -------------------------------------
//
// Bounds are stored in Apple hi-res column units (0..39) for left/right and
// pixel rows (0..159) for top/bottom, because the shared picture data was
// authored for the Apple screen layout.  Convert columns to pixel range:
// Apple column c covers pixels [c*7 .. c*7+6], so pixel_right = (c+1)*7 - 1.

static uint8_t s_fill_left = 0, s_fill_right = 39;
static uint8_t s_fill_top = 0, s_fill_bottom = 159;

static void fill_rect(uint8_t fill_idx) {
    const int lp = s_fill_left * 7;
    const int rp = (s_fill_right + 1) * 7 - 1;
    for (int y = s_fill_top; y <= s_fill_bottom && y < HDOS_HEIGHT; y++)
        fill_hline(y, lp, rp, fill_idx);
}

// ---- Pen colour mapping ------------------------------------------------------
//
// SET_PEN_COLOR (op2) carries a colour index in its low nibble; the DOS
// interpreter stores it as the pen colour (0x9d38).  Map the 8 indices the
// picture data uses to the 4 CGA palette values for lines / solid text -- the
// values the title actually uses (2, 4, 6) agree with a plain index & 3.

static const uint8_t kPenTo2bpp[8] = { 0, 1, 2, 3, 0, 2, 2, 3 };

// ---- Drawing context ---------------------------------------------------------

struct HdosCtx {
    int16_t  pen_x, pen_y;
    uint8_t  pen_val;   // 2-bpp pixel value (0–3) for lines / op3 text
    uint8_t  brush;     // brush index 0–7
    uint8_t  fill_idx;  // SET_FILL_COLOR selector byte (-> subindex table)
    uint16_t text_x;
    uint8_t  text_y;
};

// ---- Opcode interpreter ------------------------------------------------------

static bool doImageOp(const uint8_t **ptr, const uint8_t *end, HdosCtx *ctx) {
    if (*ptr >= end) return true;
    const uint8_t op_byte = *(*ptr)++;
    const uint8_t param   = op_byte & 0xf;
    const uint8_t op      = op_byte >> 4;
    uint16_t a, b;

    switch (op) {
    case OPCODE_END:  // op0: end of image
        return true;

    case OPCODE_END2: // op7: 2-operand no-op in Talisman (not an end marker)
        *ptr += 2;
        break;

    case OPCODE_SET_TEXT_POS: // op1
        ctx->text_x = (uint16_t)(*(*ptr)++ + (param & 1 ? 256 : 0));
        ctx->text_y = *(*ptr)++;
        break;

    case OPCODE_SET_PEN_COLOR: // op2
        ctx->pen_val = (param < 8) ? kPenTo2bpp[param] : 0;
        break;

    case OPCODE_TEXT_CHAR: // op3 -- solid text (pen colour)
        draw_glyph(ctx->text_x, ctx->text_y, *(*ptr)++, true,
                   ctx->pen_val, ctx->fill_idx);
        ctx->text_x += 8;
        break;

    case OPCODE_TEXT_OUTLINE: // op5 -- fill-pattern text
        draw_glyph(ctx->text_x, ctx->text_y, *(*ptr)++, false,
                   ctx->pen_val, ctx->fill_idx);
        ctx->text_x += 8;
        break;

    case OPCODE_SET_SHAPE: // op4
        ctx->brush = param;
        break;

    case OPCODE_SET_FILL_COLOR: // op6
        ctx->fill_idx = *(*ptr)++;
        break;

    case OPCODE_MOVE_TO: // op8
        ctx->pen_x = (int16_t)(*(*ptr)++ + (param & 1 ? 256 : 0));
        ctx->pen_y = (int16_t)*(*ptr)++;
        break;

    case OPCODE_DRAW_BOX: { // op9 -- four chained lines; pen left at far corner
        a = *(*ptr)++ + (param & 1 ? 256 : 0);
        b = *(*ptr)++;
        const int x0 = ctx->pen_x, y0 = ctx->pen_y;
        draw_line(x0, y0, x0, (int)b, ctx->pen_val);  // left edge  → (x0, b)
        draw_line(x0, (int)b, (int)a, (int)b, ctx->pen_val); // bottom → (a, b)
        draw_line((int)a, (int)b, (int)a, y0, ctx->pen_val); // right  → (a, y0)
        draw_line((int)a, y0, x0, y0, ctx->pen_val);          // top    → (x0, y0)
        ctx->pen_x = (int16_t)a; ctx->pen_y = (int16_t)b;    // pen at far corner
        break;
    }

    case OPCODE_DRAW_LINE: // op10
        a = *(*ptr)++ + (param & 1 ? 256 : 0);
        b = *(*ptr)++;
        draw_line(ctx->pen_x, ctx->pen_y, (int)a, (int)b, ctx->pen_val);
        ctx->pen_x = (int16_t)a; ctx->pen_y = (int16_t)b;
        break;

    case OPCODE_DRAW_CIRCLE: // op11 -- centred on current pen; pen restored
        draw_circle(ctx->pen_x, ctx->pen_y, *(*ptr)++, ctx->pen_val);
        break;

    case OPCODE_DRAW_SHAPE: // op12
        a = *(*ptr)++ + (param & 1 ? 256 : 0);
        b = *(*ptr)++;
        draw_brush((int)a, (int)b, ctx->brush, ctx->fill_idx);
        break;

    case OPCODE_DELAY: { // op13 -- pacing hint; no effect on the final page
        uint8_t units = *(*ptr)++;
        if (s_recordOps && units)
            s_ops.push_back({ DELAY_MARKER, units });
        break;
    }

    case OPCODE_PAINT: // op14 -- flood fill from seed point
        a = *(*ptr)++ + (param & 1 ? 256 : 0);
        b = *(*ptr)++;
        hdos_flood_fill((int)a, (int)b, ctx->fill_idx);
        break;

    case OPCODE_RESET: // op15 -- fill-rect sub-ops (same as Apple Talisman)
        switch (param) {
        case 1: // set rect to full picture area
            s_fill_left = 0; s_fill_top = 0;
            s_fill_right = 39; s_fill_bottom = 159;
            break;
        case 2: // read 4 bytes: right, left, bottom, top
            s_fill_right  = *(*ptr)++;
            s_fill_left   = *(*ptr)++;
            s_fill_bottom = *(*ptr)++;
            s_fill_top    = *(*ptr)++;
            break;
        default: // param 0 / 3: fill the rect with the current pattern
            fill_rect(ctx->fill_idx);
            break;
        }
        break;
    }
    return false;
}

void hdosDrawImage(const uint8_t *data, size_t size) {
    // Each image is its own reveal: drop the previous image's op list (a room
    // reset already cleared it; an item overlay reaches here without one and
    // records only its own bytes onto whatever the room left visible).
    s_ops.clear();
    s_opsDrawn = 0;
    s_pauseTicks = 0;

    HdosCtx ctx = {};
    ctx.pen_val  = 0;    // pen colour 4 (black) maps to 2-bpp 0
    ctx.brush    = 5;    // default brush matches Apple renderer init
    ctx.fill_idx = 0;    // cold-start fill selector 0 -> solid white (0xff);
                         // matches NOVEL.EXE's statically-zeroed [0x9d46]
    ctx.text_x   = 140; ctx.text_y = 96;
    s_fill_left = 0; s_fill_top = 0; s_fill_right = 39; s_fill_bottom = 159;

    const uint8_t *ptr = data;
    const uint8_t *end = data + size;
    int guard = 0;
    while (ptr < end && ++guard < 100000)
        if (doImageOp(&ptr, end, &ctx)) break;

    // When not animating, keep the visible page in step with the final page so a
    // later slow-drawn overlay composes onto the right background.
    if (!s_recordOps)
        memcpy(s_slowScreen, s_screenmem, sizeof(s_slowScreen));
}

// ---- 2-bpp → RGBA conversion -------------------------------------------------
//
// The four pixel values are CGA palette 1 (high intensity) colour indices, the
// palette NOVEL1.EXE selects on a colour adapter (INT 10h AH=0Bh in the picture
// init at 0x22ff): black / cyan / magenta / white.  The 280×160 DrawSurface
// matches the logical raster exactly, so no scaling is needed in the common
// case.

static const uint32_t kHdosColor[4] = {
    0x000000ffu, // 0: black
    0x55ffffffu, // 1: cyan
    0xff55ffffu, // 2: magenta
    0xffffffffu, // 3: white
};

static void blit_page(const uint8_t *page, uint32_t *out, int w, int h) {
    for (int y = 0; y < h; y++) {
        const int sy = (y * HDOS_HEIGHT) / h;
        const uint8_t *row = &page[sy * HDOS_STRIDE];
        for (int x = 0; x < w; x++) {
            const int sx = (x * HDOS_WIDTH) / w;
            out[y * w + x] = kHdosColor[(row[sx >> 2] >> ((sx & 3) << 1)) & 0x3];
        }
    }
}

void hdosBlitToSurface(uint32_t *out, int w, int h) {
    blit_page(s_screenmem, out, w, h);
}

// ---- Slow-draw control (host-driven) ----------------------------------------
// These mirror the gm* entry points in graphics_magician.h one-for-one, so the
// host (Comprehend::drawPicture / runSlowDraw) drives whichever renderer is
// active with the same logic.

void hdosSetSlowDraw(bool on) { s_recordOps = on; }

// True while there is reveal work left: recorded ops still to paint, or a DELAY
// pause still owed.
bool hdosSlowDrawActive() {
    return s_recordOps && (s_opsDrawn < s_ops.size() || s_pauseTicks > 0);
}

// Bytes whose offset is adjacent to, or whose value matches, the previous one
// belong to the same visual run; revealing them together avoids seams.
static bool ops_adjacent(const WriteOp &a, const WriteOp &b) {
    return ((int)b.offset >= (int)a.offset - 1 && (int)b.offset <= (int)a.offset + 1) ||
           b.value == a.value;
}

static void mark_row_dirty(uint16_t offset) {
    int r = offset / HDOS_STRIDE;
    if (r < s_dirtyMin) s_dirtyMin = r;
    if (r > s_dirtyMax) s_dirtyMax = r;
}

// Apply up to `budget` recorded ops onto the visible page, extending the chunk
// across adjacent runs.  A DELAY marker halts this tick's reveal and owes a run
// of pause ticks.  Returns true while any reveal work -- ops or an outstanding
// pause -- remains.
bool hdosAdvanceSlowDraw(int budget) {
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
            s_opsDrawn = i + 1;            // consume the marker
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

// Reveal everything left at once (resize / cancel).  Leaves the visible page
// identical to the final page.
void hdosFinishSlowDraw() {
    for (; s_opsDrawn < s_ops.size(); s_opsDrawn++) {
        if (s_ops[s_opsDrawn].offset == DELAY_MARKER)
            continue;                     // pauses collapse when finishing at once
        s_slowScreen[s_ops[s_opsDrawn].offset] = s_ops[s_opsDrawn].value;
        mark_row_dirty(s_ops[s_opsDrawn].offset);
    }
    s_pauseTicks = 0;
}

// Hand back the row band touched since the last call (inclusive) and reset it.
// Returns false if nothing changed.
bool hdosSlowConsumeDirty(int *y0, int *y1) {
    if (s_dirtyMax < 0)
        return false;
    *y0 = s_dirtyMin;
    *y1 = s_dirtyMax;
    s_dirtyMin = 0x7fffffff;
    s_dirtyMax = -1;
    return true;
}

void hdosBlitSlowToSurface(uint32_t *out, int w, int h) {
    blit_page(s_slowScreen, out, w, h);
}

} // namespace Comprehend
} // namespace Glk
