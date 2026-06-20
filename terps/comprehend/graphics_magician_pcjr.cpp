/* graphics_magician_pcjr.cpp -- DOS (IBM PCjr / Tandy 16-colour) Graphics
 * Magician picture renderer.
 *
 * The PCjr counterpart of graphics_magician_cga.cpp.  Decodes the same Penguin
 * Graphics Magician vector stream as the Apple II and CGA renderers, into the
 * 320x200 16-colour BIOS mode 9 framebuffer the PCjr release of NOVEL.EXE
 * drives through JR_GRAPH.OVR.  Every primitive here was derived from the
 * Ghidra disassembly of JR_GRAPH.OVR and the Transylvania/Crimson-Crown
 * NOVEL.EXE (see graphics_magician_pcjr.h for the model summary).  The vector
 * geometry (lines, boxes, circles, fill regions, brush placement) is identical
 * to the CGA renderer -- the picture data files are shared -- so this file
 * differs from graphics_magician_cga.cpp only in the pixel model and tables:
 *
 *   - 4 bits/pixel, 2 pixels/byte; the leftmost pixel of a byte is its HIGH
 *     nibble (jr_plot_pixel: even (x+20) -> high nibble, odd -> low nibble).
 *   - Pen colours go through 8-entry colLow[]/colHigh[] nibble tables indexed
 *     by (pen & 7); white (entries 3 and 7) carries 0xff in colHigh, which the
 *     original write-modify-write leaves in the adjacent low nibble too -- a
 *     faithful one-pixel "bleed" we reproduce.
 *   - Fills use a 30-entry x 4-byte 16-colour dither table selected through a
 *     108-entry subindex (one pattern index per row parity), exactly like the
 *     v1 CGA path.
 *   - Brush bitmaps and the font are mode-independent masks loaded from
 *     NOVEL.EXE / CHARSET.GDA, byte-identical to the CGA renderer.
 *
 * Like graphics_magician_cga.cpp this file is self-contained (no Common::/
 * Graphics:: dependencies) so it can be exercised by an offline harness.
 */

#include "graphics_magician_pcjr.h"
#include "graphics_magician.h"         // Opcode enum
#include "graphics_magician_vector.h"  // shared line/circle rasterizers
#include "slow_draw_page.h"            // shared progressive-reveal helper
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>               // gmpcjr_flood_fill's scanline stack

namespace Glk {
namespace Comprehend {

// ---- Drawing tables ----------------------------------------------------------

static uint8_t s_colLow[8];        // pen (0-7) -> colour in low nibble
static uint8_t s_colHigh[8];       // pen (0-7) -> colour in high nibble (white=0xff)
static uint8_t s_maskHigh = 0x0f;  // AND mask when writing the high nibble (even x)
static uint8_t s_maskLow  = 0xf0;  // AND mask when writing the low nibble  (odd x)
static uint8_t s_fillPat[30][4];   // 16-colour 4bpp dither tiles (4 column phases)
static uint8_t s_fillSubindex[256];// op6 byte -> (even,odd) pattern index pair
static uint8_t s_brushBitmaps[256];// 8 brushes x 32 bytes (shared with CGA)
static uint8_t s_fontGlyphs[96 * 8];// chars 32-127, 8 bytes each, MSB-first

static bool s_haveDrawingTables = false;

bool gmpcjrHaveDrawingTables() { return s_haveDrawingTables; }

// Reverse the 8 bits of a byte (LSB-first charset.gda row -> MSB-first glyph).
static inline uint8_t mirror_byte(uint8_t b) {
    b = (uint8_t)((b >> 4) | (b << 4));
    b = (uint8_t)(((b & 0xcc) >> 2) | ((b & 0x33) << 2));
    b = (uint8_t)(((b & 0xaa) >> 1) | ((b & 0x55) << 1));
    return b;
}

bool gmpcjrInstallDrawingTables(const uint8_t *ovr, size_t ovrSize,
                                const uint8_t *exe, size_t exeSize) {
    if (!ovr || !exe)
        return false;

    // Locate the overlay's data block by the bankStride+nibble-mask signature
    // that immediately precedes the pen colour tables (DGROUP layout: word
    // [0x90fa]=bankStride=0x2000, byte [0x90fc]=maskHigh=0x0f,
    // [0x90fd]=maskLow=0xf0, then colLow[8] at [0x90fe], colHigh[8] at [0x9106],
    // the fill pattern table at [0x9129] and the subindex at [0x91a1]).  This
    // signature is game-independent (identical in Transylvania and Crimson
    // Crown); the tables it anchors are read out per game.
    static const uint8_t kSig[] = { 0x00, 0x20, 0x0f, 0xf0 };
    size_t sig = SIZE_MAX;
    for (size_t i = 0; i + sizeof(kSig) <= ovrSize; i++) {
        if (std::memcmp(ovr + i, kSig, sizeof(kSig)) == 0) {
            sig = i;
            break;
        }
    }
    if (sig == SIZE_MAX)
        return false;

    const size_t base = sig + 4;          // colLow (DGROUP 0x90fe)
    const size_t fillOff = base + 0x2b;   // DGROUP 0x9129
    const size_t subOff  = base + 0xa3;   // DGROUP 0x91a1
    if (subOff + 108 * 2 > ovrSize)
        return false;

    s_maskHigh = ovr[sig + 2];
    s_maskLow  = ovr[sig + 3];
    std::memcpy(s_colLow,  ovr + base,     8);
    std::memcpy(s_colHigh, ovr + base + 8, 8);
    std::memcpy(s_fillPat, ovr + fillOff, sizeof(s_fillPat));   // 30 x 4 bytes
    std::memset(s_fillSubindex, 0, sizeof(s_fillSubindex));
    std::memcpy(s_fillSubindex, ovr + subOff, 108 * 2);

    // Brush bitmaps live in NOVEL.EXE, byte-identical to the CGA renderer:
    // locate brush 5 (the filled-circle default) by its distinctive left column
    // and step back to brush 0.
    static const uint8_t kBrush5Sig[] = {
        0x00,0x03,0x1f,0x3f,0x3f,0x3f,0x7f,0x7f,
        0x7f,0x7f,0x3f,0x3f,0x3f,0x1f,0x03,0x00,
    };
    size_t b5 = SIZE_MAX;
    for (size_t i = 0; i + sizeof(kBrush5Sig) <= exeSize; i++) {
        if (std::memcmp(exe + i, kBrush5Sig, sizeof(kBrush5Sig)) == 0) {
            b5 = i;
            break;
        }
    }
    if (b5 == SIZE_MAX || b5 < 5 * 32 || (b5 - 5 * 32) + sizeof(s_brushBitmaps) > exeSize)
        return false;
    std::memcpy(s_brushBitmaps, exe + (b5 - 5 * 32), sizeof(s_brushBitmaps));

    // No embedded picture font (as in v1 CGA): op3/op5 glyphs come from
    // CHARSET.GDA via gmpcjrSetFont().  Leave zeroed until then.
    std::memset(s_fontGlyphs, 0, sizeof(s_fontGlyphs));

    s_haveDrawingTables = true;
    return true;
}

bool gmpcjrSetFont(const uint8_t *charsetGda, size_t size) {
    if (!charsetGda || size < 4 + 96 * 8)
        return false;
    const uint16_t version = (uint16_t)(charsetGda[0] | (charsetGda[1] << 8));
    if (version != 0x1100)
        return false;
    for (int idx = 0; idx < 96; idx++)
        for (int row = 0; row < 8; row++)
            s_fontGlyphs[idx * 8 + row] = mirror_byte(charsetGda[4 + idx * 8 + row]);
    return true;
}

// ---- 16-colour framebuffer ---------------------------------------------------
//
// Stored as the mode-9 logical page, 2 pixels/byte (high nibble = left pixel),
// 160 bytes per row.  The 4-bank VRAM interleave is purely an addressing detail
// of the real hardware; the drawn pixel content is identical with a flat layout,
// so we keep a flat buffer (and record flat byte offsets for the slow reveal).
// The picture window is at x+20 px (global byte column (x+20)>>1), spanning the
// same 280x160 logical area as the CGA/Apple renderers.

#define PCJR_BYTES_PER_ROW 160
#define PCJR_ROWS          200
#define PCJR_PIC_W         280   // logical picture width  (x 0..279)
#define PCJR_PIC_H         160   // logical picture height (y 0..159)
#define PCJR_X_OFFSET      20    // picture window offset in pixels

static uint8_t s_screen[PCJR_ROWS * PCJR_BYTES_PER_ROW];

// ---- Slow ("animated") draw -- shared SlowDrawPage helper --------------------
// s_screen is the fully-drawn page; s_slowScreen is the page the host reveals.

static uint8_t s_slowScreen[PCJR_ROWS * PCJR_BYTES_PER_ROW];
static SlowDrawPage s_slow(s_screen, s_slowScreen, sizeof(s_screen),
                           PCJR_BYTES_PER_ROW);

static inline void store_byte(int off, uint8_t value) {
    s_screen[off] = value;
    s_slow.record(off, value);
}

// Read the colour index (0-15) of the pixel at logical picture (x,y).
static inline uint8_t read_pixel(int x, int y) {
    if ((unsigned)x >= PCJR_PIC_W || (unsigned)y >= PCJR_PIC_H) return 0;
    const int gx = x + PCJR_X_OFFSET;
    const uint8_t b = s_screen[y * PCJR_BYTES_PER_ROW + (gx >> 1)];
    return (gx & 1) ? (uint8_t)(b & 0x0f) : (uint8_t)(b >> 4);
}

// Write the nibble for logical picture (x,y) to colour `col` (0-15), one nibble
// (read-modify-write of the shared byte).
static inline void write_pixel(int x, int y, uint8_t col) {
    if ((unsigned)x >= PCJR_PIC_W || (unsigned)y >= PCJR_PIC_H) return;
    const int gx = x + PCJR_X_OFFSET;
    const int off = y * PCJR_BYTES_PER_ROW + (gx >> 1);
    uint8_t b = s_screen[off];
    if (gx & 1) b = (uint8_t)((b & 0xf0) | (col & 0x0f));
    else        b = (uint8_t)((b & 0x0f) | (col << 4));
    store_byte(off, b);
}

// Pen plot (jr_plot_pixel @ OVR 0x0e): the pen colour byte (colLow/colHigh) is
// OR'd into the target nibble after masking.  For pen white (colHigh==0xff) the
// OR also whitens the adjacent low nibble -- the original's behaviour, kept.
static inline void plot_pen(int x, int y, uint8_t pen) {
    if ((unsigned)x >= PCJR_PIC_W || (unsigned)y >= PCJR_PIC_H) return;
    const int gx = x + PCJR_X_OFFSET;
    const int off = y * PCJR_BYTES_PER_ROW + (gx >> 1);
    const uint8_t p = (uint8_t)(pen & 7);
    uint8_t b = s_screen[off];
    if (gx & 1) b = (uint8_t)((b & s_maskLow)  | s_colLow[p]);
    else        b = (uint8_t)((b & s_maskHigh) | s_colHigh[p]);
    store_byte(off, b);
}

void gmpcjrResetScreen(bool white) {
    memset(s_screen,     white ? 0xff : 0x00, sizeof(s_screen));
    memset(s_slowScreen, white ? 0xff : 0x00, sizeof(s_slowScreen));
    s_slow.clear();
}

// ---- Fill pattern ------------------------------------------------------------
//
// op6 selector -> subindex[sel*2 + rowParity] -> 4bpp dither tile.  The tile's
// byte for a given global byte column is s_fillPat[idx][byteCol & 3]; the pixel
// nibble is the high or low half by (x+20) parity.  (Matches the CGA path with
// 4bpp tiles instead of 2bpp.)
static inline uint8_t fill_pixel(uint8_t sel, int x, int y) {
    const uint8_t idx = s_fillSubindex[(unsigned)sel * 2 + (y & 1)];
    if (idx >= 30) return 0;
    const int gx = x + PCJR_X_OFFSET;
    const uint8_t pb = s_fillPat[idx][(gx >> 1) & 3];
    return (gx & 1) ? (uint8_t)(pb & 0x0f) : (uint8_t)(pb >> 4);
}

// ---- Flood fill (op14 PAINT) -------------------------------------------------
//
// The fillable region is the 4-connected run of white (15) pixels reachable
// from the seed, bounded by any non-white pixel -- identical to the CGA fill's
// "white == fillable" predicate.  Because the boundary geometry is drawn pixel-
// for-pixel like the CGA renderer (shared vector data + identical line/box/
// circle code), this region matches the original; we then stamp the per-row-
// parity dither pattern over it.  A standard scanline flood with a visited mask
// (the dither can re-introduce white, so we must not rely on the colour alone)
// reproduces the filled set; the original's exact span-queue traversal order
// only affects sub-pixel dither self-limiting at boundaries, which is not
// observable without hardware ground truth.
static uint8_t s_visited[PCJR_PIC_W * PCJR_PIC_H];

static void gmpcjr_flood_fill(int seedx, int seedy, uint8_t sel) {
    if ((unsigned)seedx >= PCJR_PIC_W || (unsigned)seedy >= PCJR_PIC_H) return;
    if (read_pixel(seedx, seedy) != 15) return;

    memset(s_visited, 0, sizeof(s_visited));
    struct Seed { int x, y; };
    std::vector<Seed> stack;
    stack.push_back({ seedx, seedy });

    while (!stack.empty()) {
        Seed s = stack.back();
        stack.pop_back();
        int x = s.x, y = s.y;
        if ((unsigned)y >= PCJR_PIC_H) continue;

        // Walk left to the start of the white run on this row.
        while (x > 0 && read_pixel(x - 1, y) == 15 && !s_visited[y * PCJR_PIC_W + (x - 1)])
            x--;

        bool spanAbove = false, spanBelow = false;
        for (; x < PCJR_PIC_W; x++) {
            const int vi = y * PCJR_PIC_W + x;
            if (s_visited[vi] || read_pixel(x, y) != 15)
                break;
            s_visited[vi] = 1;
            write_pixel(x, y, fill_pixel(sel, x, y));

            if (y > 0) {
                bool up = read_pixel(x, y - 1) == 15 && !s_visited[(y - 1) * PCJR_PIC_W + x];
                if (up && !spanAbove) { stack.push_back({ x, y - 1 }); spanAbove = true; }
                else if (!up) spanAbove = false;
            }
            if (y < PCJR_PIC_H - 1) {
                bool dn = read_pixel(x, y + 1) == 15 && !s_visited[(y + 1) * PCJR_PIC_W + x];
                if (dn && !spanBelow) { stack.push_back({ x, y + 1 }); spanBelow = true; }
                else if (!dn) spanBelow = false;
            }
        }
    }
}

// ---- Line (op10) / box (op9) -------------------------------------------------
// Geometry lives in gmvDrawLine (shared with the CGA renderer); here we bind it
// to the 4-bpp pen writer (white-bleed and all).
static void draw_line(int x0, int y0, int x1, int y1, uint8_t pen) {
    gmvDrawLine(x0, y0, x1, y1,
                [pen](int x, int y) { plot_pen(x, y, pen); });
}

// ---- Circle (op11) -----------------------------------------------------------
// Geometry lives in gmvDrawCircle (shared with the CGA renderer; draw_circle @
// JR_GRAPH 0x1c6 is identical), bound to the 4-bpp pen writer.
static void draw_circle(int cx, int cy, int radius, uint8_t pen) {
    gmvDrawCircle(cx, cy, radius,
                  [pen](int x, int y) { plot_pen(x, y, pen); });
}

// ---- Brush stamp (op12) ------------------------------------------------------
//
// Each brush is 32 bytes = four 8x8 quadrants (Q0 upper-left, Q1 lower-left,
// Q2 upper-right, Q3 lower-right), bytes MSB-first (bit 7 = leftmost pixel).
// A set bit takes the current fill-pattern colour at that pixel; clear bits
// leave the background.  (Same quadrant order as the CGA renderer.)
static void blit_quadrant(const uint8_t *bytes8, int px, int py, uint8_t fill_sel) {
    for (int row = 0; row < 8; row++, py++) {
        const uint8_t b = bytes8[row];
        if (!b) continue;
        for (int p = 0; p < 8; p++) {
            if (!((b >> (7 - p)) & 1)) continue;
            const int x = px + p;
            write_pixel(x, py, fill_pixel(fill_sel, x, py));
        }
    }
}

static void draw_brush(int x, int y, uint8_t brush, uint8_t fill_sel) {
    if (brush >= 8) return;
    const uint8_t *base = &s_brushBitmaps[brush * 32];
    blit_quadrant(base,      x,     y,     fill_sel); // Q0 upper-left
    blit_quadrant(base + 8,  x,     y + 8, fill_sel); // Q1 lower-left
    blit_quadrant(base + 16, x + 8, y,     fill_sel); // Q2 upper-right
    blit_quadrant(base + 24, x + 8, y + 8, fill_sel); // Q3 lower-right
}

// ---- Text glyph (op3 / op5) --------------------------------------------------
//
// op3 (TEXT_CHAR): each set glyph pixel inverts the background nibble
// (val ^ 0xf), as in the CGA renderer's XOR branch -- white background -> black
// letters.  op5 (TEXT_OUTLINE): each set pixel takes the fill-pattern colour.
// Glyph bytes are MSB-first; the text cursor advances 8 px/char.
static void draw_glyph(int x, int y, uint8_t ch, bool invert, uint8_t fill_sel) {
    if (ch < 32 || ch >= 128) return;
    const uint8_t *glyph = &s_fontGlyphs[(ch - 32) * 8];
    for (int row = 0; row < 8; row++) {
        const uint8_t b = glyph[row];
        if (!b) continue;
        const int py = y + row;
        for (int p = 0; p < 8; p++) {
            if (!((b >> (7 - p)) & 1)) continue;
            const int px = x + p;
            if (invert) write_pixel(px, py, (uint8_t)(read_pixel(px, py) ^ 0x0f));
            else        write_pixel(px, py, fill_pixel(fill_sel, px, py));
        }
    }
}

// ---- op15 fill-rectangle / clip ----------------------------------------------
// As in v1 CGA, op15 in these games is flood-fill clip-rect setup, not a paint:
// param 1 resets the clip to the window, param >=2 reads 4 bytes.  Inert for the
// shipped corpus; we consume the bytes to keep the stream aligned.

// ---- Drawing context ---------------------------------------------------------
struct PcjrCtx {
    int16_t  pen_x, pen_y;
    uint8_t  pen;       // pen colour index (0-15; & 7 selects colLow/colHigh)
    uint8_t  brush;     // brush index 0-7
    uint8_t  fill_sel;  // op6 selector byte
    uint16_t text_x;
    uint8_t  text_y;
};

// ---- Opcode interpreter ------------------------------------------------------
static bool doImageOp(const uint8_t **ptr, const uint8_t *end, PcjrCtx *ctx) {
    if (*ptr >= end) return true;
    const uint8_t op_byte = *(*ptr)++;
    const uint8_t param   = op_byte & 0xf;
    const uint8_t op      = op_byte >> 4;
    uint16_t a, b;

    switch (op) {
    case OPCODE_END:  // op0
        return true;

    case OPCODE_END2: // op7: 2-operand no-op
        *ptr += 2;
        break;

    case OPCODE_SET_TEXT_POS: // op1
        ctx->text_x = (uint16_t)(*(*ptr)++ + (param & 1 ? 256 : 0));
        ctx->text_y = *(*ptr)++;
        break;

    case OPCODE_SET_PEN_COLOR: // op2
        ctx->pen = (uint8_t)(param & 0xf);
        break;

    case OPCODE_TEXT_CHAR: // op3 -- invert (black on white)
        draw_glyph(ctx->text_x, ctx->text_y, *(*ptr)++, true, ctx->fill_sel);
        ctx->text_x += 8;
        break;

    case OPCODE_TEXT_OUTLINE: // op5 -- fill-pattern text
        draw_glyph(ctx->text_x, ctx->text_y, *(*ptr)++, false, ctx->fill_sel);
        ctx->text_x += 8;
        break;

    case OPCODE_SET_SHAPE: // op4
        ctx->brush = param;
        break;

    case OPCODE_SET_FILL_COLOR: // op6
        ctx->fill_sel = *(*ptr)++;
        break;

    case OPCODE_MOVE_TO: // op8
        ctx->pen_x = (int16_t)(*(*ptr)++ + (param & 1 ? 256 : 0));
        ctx->pen_y = (int16_t)*(*ptr)++;
        break;

    case OPCODE_DRAW_BOX: { // op9 -- four chained lines; pen left at far corner
        a = *(*ptr)++ + (param & 1 ? 256 : 0);
        b = *(*ptr)++;
        const int x0 = ctx->pen_x, y0 = ctx->pen_y;
        draw_line(x0, y0, x0, (int)b, ctx->pen);
        draw_line(x0, (int)b, (int)a, (int)b, ctx->pen);
        draw_line((int)a, (int)b, (int)a, y0, ctx->pen);
        draw_line((int)a, y0, x0, y0, ctx->pen);
        ctx->pen_x = (int16_t)a; ctx->pen_y = (int16_t)b;
        break;
    }

    case OPCODE_DRAW_LINE: // op10
        a = *(*ptr)++ + (param & 1 ? 256 : 0);
        b = *(*ptr)++;
        draw_line(ctx->pen_x, ctx->pen_y, (int)a, (int)b, ctx->pen);
        ctx->pen_x = (int16_t)a; ctx->pen_y = (int16_t)b;
        break;

    case OPCODE_DRAW_CIRCLE: // op11
        draw_circle(ctx->pen_x, ctx->pen_y, *(*ptr)++, ctx->pen);
        break;

    case OPCODE_DRAW_SHAPE: // op12 -- brush stamp (one px left, like CGA)
        a = *(*ptr)++ + (param & 1 ? 256 : 0);
        b = *(*ptr)++;
        draw_brush((int)a - 1, (int)b, ctx->brush, ctx->fill_sel);
        break;

    case OPCODE_DELAY: { // op13 -- pacing hint
        uint8_t units = *(*ptr)++;
        s_slow.recordDelay(units);
        break;
    }

    case OPCODE_PAINT: // op14 -- flood fill from seed point
        a = *(*ptr)++ + (param & 1 ? 256 : 0);
        b = *(*ptr)++;
        gmpcjr_flood_fill((int)a, (int)b, ctx->fill_sel);
        break;

    case OPCODE_RESET: // op15 -- flood-fill clip setup (inert; consume operands)
        if (param >= 2)
            *ptr += 4;
        break;
    }
    return false;
}

void gmpcjrDrawImage(const uint8_t *data, size_t size) {
    // The op list is dropped by gmpcjrResetScreen() when a new page starts, NOT
    // here: item overlays must APPEND to the room's still-pending reveal so the
    // whole scene reveals in paint order (see gmDrawImage for the full rationale).
    PcjrCtx ctx = {};
    ctx.pen      = 0;
    ctx.pen_x    = 140;   // per-image init (JR_GRAPH 0xa33): pen (140,6), brush 5
    ctx.pen_y    = 6;
    ctx.brush    = 5;
    ctx.fill_sel = 0;
    ctx.text_x   = 140; ctx.text_y = 96;

    const uint8_t *ptr = data;
    const uint8_t *end = data + size;
    int guard = 0;
    while (ptr < end && ++guard < 100000)
        if (doImageOp(&ptr, end, &ctx)) break;

    s_slow.syncIfNotRecording();
}

// ---- 16-colour -> RGBA conversion --------------------------------------------
//
// Standard PCjr / CGA RGBI 16-colour palette (the game programs no custom
// palette).  Output the 280x160 picture window (logical x 0..279 -> global
// byte column (x+20)>>1), scaled to the requested surface size.
static const uint32_t kPcjrPalette[16] = {
    0x000000ffu, // 0  black
    0x0000aaffu, // 1  blue
    0x00aa00ffu, // 2  green
    0x00aaaaffu, // 3  cyan
    0xaa0000ffu, // 4  red
    0xaa00aaffu, // 5  magenta
    0xaa5500ffu, // 6  brown
    0xaaaaaaffu, // 7  light grey
    0x555555ffu, // 8  dark grey
    0x5555ffffu, // 9  light blue
    0x55ff55ffu, // 10 light green
    0x55ffffffu, // 11 light cyan
    0xff5555ffu, // 12 light red
    0xff55ffffu, // 13 light magenta
    0xffff55ffu, // 14 yellow
    0xffffffffu, // 15 white
};

static void blit_page(const uint8_t *page, uint32_t *out, int w, int h) {
    for (int y = 0; y < h; y++) {
        const int sy = (y * PCJR_PIC_H) / h;
        const uint8_t *row = &page[sy * PCJR_BYTES_PER_ROW];
        for (int x = 0; x < w; x++) {
            const int sx = (x * PCJR_PIC_W) / w;       // logical picture x
            const int gx = sx + PCJR_X_OFFSET;
            const uint8_t byteval = row[gx >> 1];
            const uint8_t col = (gx & 1) ? (byteval & 0x0f) : (byteval >> 4);
            out[y * w + x] = kPcjrPalette[col];
        }
    }
}

void gmpcjrBlitToSurface(uint32_t *out, int w, int h) {
    blit_page(s_screen, out, w, h);
}

// ---- Slow-draw control (host-driven) -- delegate to the shared helper -------

void gmpcjrSetSlowDraw(bool on) { s_slow.setRecording(on); }
bool gmpcjrSlowDrawActive() { return s_slow.active(); }
bool gmpcjrAdvanceSlowDraw(int budget) { return s_slow.advance(budget); }
void gmpcjrFinishSlowDraw() { s_slow.finish(); }
bool gmpcjrSlowConsumeDirty(int *y0, int *y1) { return s_slow.consumeDirty(y0, y1); }

void gmpcjrBlitSlowToSurface(uint32_t *out, int w, int h) {
    blit_page(s_slowScreen, out, w, h);
}

} // namespace Comprehend
} // namespace Glk
