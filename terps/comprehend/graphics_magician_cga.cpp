/* graphics_magician_cga.cpp -- DOS (CGA) Graphics Magician picture renderer.
 *
 * The DOS/CGA counterpart of graphics_magician.cpp (the Apple II renderer):
 * both decode the same Penguin Graphics Magician vector stream.  This one is
 * shared by every Comprehend v2 DOS release -- Talisman, OO-Topos,
 * Transylvania (v2) and The Coveted Mirror -- whose NOVEL.EXE images all carry
 * the byte-identical CGA drawing tables this renderer loads (the offsets quoted
 * below were first measured in Talisman's NOVEL1.EXE; gmcgaInstallDrawingTables
 * locates them by signature so they resolve in every v2 release).
 *
 * Renders the Penguin Graphics Magician vector stream into a 280×160, 2-bpp
 * (4-colour) logical raster that matches the native picture interpreter in the
 * DOS release NOVEL.EXE / NOVEL1.EXE.  On a colour adapter the interpreter
 * drives CGA 320×200 4-colour mode and selects palette 1, low intensity
 * (black / cyan / magenta / grey) -- see the picture init at 0x22ff (INT 10h
 * AH=0Bh).  Each primitive here was derived from, and cross-checked against,
 * the Ghidra disassembly of NOVEL1.EXE (analysis in the project memory notes):
 *
 *   PicInterpretImageOpcodes 1f56 -- dispatcher; 16-handler table at 0x9d5d,
 *       default brush 5, brush table pointer 0x9d81
 *   line = major-axis Bresenham, box = four chained lines, circle = 8-octant
 *   midpoint, brush/glyph = per-row bit blit, flood fill = mechanical port of
 *   the native word-granular span-queue fill (PicOp14Paint 0x2630; see below).
 *
 * Pixel model:
 *   - 2 bpp, 4 pixels per byte, LSB-first (pixel p in bits (2p+1):(2p))
 *   - Values are CGA palette-1 colour indices: 0=black, 1=cyan, 2=magenta, 3=grey
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
 * hands the bytes to gmcgaInstallDrawingTables().
 */

#include "graphics_magician_cga.h"
#include "graphics_magician.h"         // Opcode enum
#include "graphics_magician_vector.h"  // shared line/circle rasterizers
#include "slow_draw_page.h"            // shared progressive-reveal helper
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Glk {
namespace Comprehend {

// ---- Drawing tables -----------------------------------------------------------

static uint8_t s_fillTable[76];       // pattern[idx] = 2-bpp byte (4-pixel tile), LSB-first
static uint8_t s_fillTableRaw[76];    // same, in the native MSB-first bit order
static uint8_t s_fillSubindex[512];   // SET_FILL_COLOR byte -> (even,odd) pattern idx
static uint8_t s_brushBitmaps[256];   // 8 brushes × 32 bytes
static uint8_t s_fontGlyphs[96 * 8];  // chars 32-127, 8 bytes each

// v1 (Crimson Crown / Transylvania v1) fill patterns.  Each of the 30 table
// entries holds FOUR bytes.  The native word painter (PC_GRAPH.OVR FUN_0461)
// fetches pattern[idx*4 + (CGA_byte_column & 3)] -- it *is* indexed by byte
// column (a period-16 model) -- BUT the fill always paints a word at a time at an
// EVEN byte offset, so the phase is only ever 0 or 2, and the >4-px dithers are
// shipped so that phase 0 == phase 2 (e.g. idx13 "40 80 40 80", idx16
// "20 10 20 10"): the off-phase (1/3) bytes are never reached by the fill.  The
// two-colour dithers that need a half-byte shift ship as *separate* indices
// (12 "80 40.." / 13 "40 80..", 16/17, 20/21) and the subindex picks the aligned
// one.  Net effect: reading entry[0] is exactly what the native computes here, so
// that is what fill_pattern()/pf_pattern_word() do.  (Re-confirmed 2026-06-13:
// forcing true per-column phasing regresses crypt to ~2824 px at every offset 0-3;
// see TODO_v1_cga_graphics.md for the full OVR disassembly.)  s_v1PatRaw keeps
// the native MSB-first byte (used by the word-based flood fill, which works in
// global byte coords); s_v1PatLsb is the mirrored copy the LSB-first framebuffer
// stores directly.  The subindex (s_fillSubindex) format is identical to v2
// (low=even row, high=odd).
static uint8_t s_v1PatRaw[30][4];
static uint8_t s_v1PatLsb[30][4];
static bool    s_v1 = false;

static inline uint8_t mirror_2bpp_byte(uint8_t b);  // defined below

// Reverse the 8 bits of a byte (bit 0 <-> bit 7): converts an LSB-first glyph
// row (charset.gda) to the MSB-first order draw_glyph expects.
static inline uint8_t mirror_byte(uint8_t b) {
    b = (uint8_t)((b >> 4) | (b << 4));
    b = (uint8_t)(((b & 0xcc) >> 2) | ((b & 0x33) << 2));
    b = (uint8_t)(((b & 0xaa) >> 1) | ((b & 0x55) << 1));
    return b;
}

static bool s_haveDrawingTables = false;

bool gmcgaHaveDrawingTables() { return s_haveDrawingTables; }

bool gmcgaInstallDrawingTables(const uint8_t *exe, size_t size) {
    if (!exe)
        return false;

    // Locate the three drawing tables inside the DOS interpreter's DGROUP data
    // segment.  In the Talisman release (NOVEL.EXE / NOVEL1.EXE, which carry
    // byte-identical data tables) they sit at DS offsets 0x9860 / 0x9d81 /
    // 0x9e81, i.e. file offsets 0xc8d0 / 0xcdf1 / 0xcef1.  Rather than hash the
    // whole binary -- which breaks on every re-release -- identify the fill
    // table by its unmistakable signature (the first eight 2-bpp CGA fill
    // patterns: black / cyan / magenta / grey / and the four single-bit
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
        s_fillTableRaw[i] = b;
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

    s_v1 = false;
    s_haveDrawingTables = true;
    return true;
}

// Install the drawing tables for the v1 Comprehend DOS releases (Crimson Crown,
// Transylvania v1).  v1 splits the Graphics Magician across NOVEL.EXE (game
// logic + the static brush bitmaps) and the PC_GRAPH.OVR overlay (the picture
// primitives + the fill pattern and subindex tables).  Unlike v2 -- where the
// whole interpreter and its tables live in NOVEL.EXE and are signature-located
// there -- v1's fill pattern table is BSS in NOVEL.EXE (built at overlay load by
// copying it out of PC_GRAPH.OVR), so it is *only* present in the OVR file.  We
// therefore read the fill tables from the OVR and the brushes from the EXE,
// each by signature so the offsets resolve across the v1 releases.  (Verified
// by live DOSBox dump of NOVEL1.EXE + Ghidra; the OVR bytes are byte-identical
// to the DGROUP copy the engine reads.)
//
//   OVR fill pattern table: 30 entries x 4 column-phase bytes, found by the
//     unmistakable first four entries 00.. aa.. 55.. ff.. (each byte x4).
//   OVR subindex table:     immediately follows, 109 words (selector -> idx pair).
//   EXE brushes:            8+ x 32 bytes, located via brush 5 (the filled-circle
//     default), then backed up 5*32 bytes to brush 0.
bool gmcgaInstallV1DrawingTables(const uint8_t *ovr, size_t ovrSize,
                                 const uint8_t *exe, size_t exeSize) {
    if (!ovr || !exe)
        return false;

    // Fill pattern table signature: the first four patterns are solid black /
    // magenta-ish (aa) / cyan-ish (55) / white (ff), each replicated across all
    // four phase bytes.
    static const uint8_t kPatSig[] = {
        0x00,0x00,0x00,0x00, 0xaa,0xaa,0xaa,0xaa,
        0x55,0x55,0x55,0x55, 0xff,0xff,0xff,0xff,
    };
    size_t patBase = SIZE_MAX;
    for (size_t i = 0; i + sizeof(kPatSig) <= ovrSize; i++) {
        if (std::memcmp(ovr + i, kPatSig, sizeof(kPatSig)) == 0) {
            patBase = i;
            break;
        }
    }
    if (patBase == SIZE_MAX)
        return false;
    const size_t kPatBytes = 30 * 4;             // 120
    const size_t kSubBytes = 109 * 2;            // 218 (selectors 0..108)
    if (patBase + kPatBytes + kSubBytes > ovrSize)
        return false;

    for (int idx = 0; idx < 30; idx++) {
        for (int ph = 0; ph < 4; ph++) {
            const uint8_t b = ovr[patBase + idx * 4 + ph];
            s_v1PatRaw[idx][ph] = b;
            s_v1PatLsb[idx][ph] = mirror_2bpp_byte(b);
        }
    }
    std::memset(s_fillSubindex, 0, sizeof(s_fillSubindex));
    std::memcpy(s_fillSubindex, ovr + patBase + kPatBytes, kSubBytes);

    // Brush table in NOVEL.EXE: locate brush 5 (filled circle), whose left
    // column (bytes 0-7) is a distinctive run, then step back to brush 0.
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
    if (b5 == SIZE_MAX || b5 < 5 * 32)
        return false;
    const size_t brushBase = b5 - 5 * 32;
    if (brushBase + sizeof(s_brushBitmaps) > exeSize)
        return false;
    std::memcpy(s_brushBitmaps, exe + brushBase, sizeof(s_brushBitmaps));

    // v1 NOVEL.EXE has no embedded picture font; its op3/op5 glyphs come from
    // CHARSET.GDA, loaded separately via gmcgaSetV1Font().  Leave zeroed until
    // then (in-picture text stays blank, as it would without the file).
    std::memset(s_fontGlyphs, 0, sizeof(s_fontGlyphs));

    s_v1 = true;
    s_haveDrawingTables = true;
    return true;
}

bool gmcgaSetV1Font(const uint8_t *charsetGda, size_t size) {
    if (!charsetGda || size < 4 + 96 * 8)
        return false;
    const uint16_t version = (uint16_t)(charsetGda[0] | (charsetGda[1] << 8));
    if (version != 0x1100)
        return false;
    // charset.gda is LSB-first (bit 0 = leftmost); draw_glyph reads MSB-first,
    // so reverse each glyph byte as we copy it in.
    for (int idx = 0; idx < 96; idx++)
        for (int row = 0; row < 8; row++)
            s_fontGlyphs[idx * 8 + row] = mirror_byte(charsetGda[4 + idx * 8 + row]);
    return true;
}

// ---- 2-bpp (CGA) framebuffer -------------------------------------------------
//
// 280 pixels wide × 160 rows.  Each byte holds 4 pixels in two-bit pairs,
// LSB-first: pixel p occupies bits (2p+1):(2p) of byte (x>>2) in row y.
// Values are CGA colour indices in palette 1, low intensity:
//   0 = black, 1 = cyan, 2 = magenta, 3 = grey.

#define GMCGA_WIDTH   280
#define GMCGA_HEIGHT  160
#define GMCGA_STRIDE  70   // 280 × 2 bpp / 8 = 70 bytes/row

static uint8_t s_screenmem[GMCGA_HEIGHT * GMCGA_STRIDE];

// ---- Slow ("animated") draw -------------------------------------------------
//
// Mirrors graphics_magician.cpp: when recording is on (gmcgaSetSlowDraw(true)),
// every framebuffer byte mutation is appended to an ordered op list as well as
// applied to the final page.  The host re-applies those ops onto a separate
// visible page (s_slowScreen) a chunk at a time on a Glk timer, re-blitting
// after each chunk, so the picture is revealed in the order it was painted --
// the few seconds the real interpreter took to fill the CGA page.  op13 emits a
// DELAY marker that pauses the reveal.  See the gm* counterparts for the host
// driver; the two renderers expose the same slow-draw contract.

// The reveal state machine, op list and dirty band live in the shared
// SlowDrawPage helper (slow_draw_page.h).  s_screenmem is the fully-drawn page;
// s_slowScreen is the page the host progressively reveals.
static uint8_t s_slowScreen[GMCGA_HEIGHT * GMCGA_STRIDE]; // progressively revealed
static SlowDrawPage s_slow(s_screenmem, s_slowScreen, sizeof(s_screenmem),
                           GMCGA_STRIDE);

// Apply one byte to the final page and, while recording, log it for the reveal.
static inline void store_byte(int off, uint8_t value) {
    s_screenmem[off] = value;
    s_slow.record(off, value);
}

static inline void write_2bpp(int x, int y, uint8_t val) {
    if ((unsigned)x >= GMCGA_WIDTH || (unsigned)y >= GMCGA_HEIGHT) return;
    int off = y * GMCGA_STRIDE + (x >> 2);
    int shift = (x & 3) << 1;
    store_byte(off, (uint8_t)((s_screenmem[off] & ~(0x3u << shift)) | ((val & 0x3u) << shift)));
}

static inline uint8_t read_2bpp(int x, int y) {
    if ((unsigned)x >= GMCGA_WIDTH || (unsigned)y >= GMCGA_HEIGHT) return 0;
    return (s_screenmem[y * GMCGA_STRIDE + (x >> 2)] >> ((x & 3) << 1)) & 0x3;
}

void gmcgaResetScreen(bool white) {
    memset(s_screenmem, white ? 0xff : 0x00, sizeof(s_screenmem));
    // A reset starts a fresh page: it appears instantly on the visible page too,
    // and any pending reveal is dropped.
    memset(s_slowScreen, white ? 0xff : 0x00, sizeof(s_slowScreen));
    s_slow.clear();
}

// ---- Fill helpers ------------------------------------------------------------

// Resolve a SET_FILL_COLOR (op6) selector byte to the 4-pixel pattern tile for
// row `y` at logical byte column `logByte`.  The selector indexes the subindex
// table for a pair of pattern-table indices; even and odd rows use different
// ones, which is what makes the two-row CGA dithers (e.g. the cyan/black
// checkerboard background).  This mirrors FUN_23dd + the per-row pattern fetch
// in the DOS interpreter.
//
// v1 pattern entries hold all four column-phase bytes.  The per-pixel blitters
// (brush op12, fill-pattern text op5, fill-rect op15) stamp at ARBITRARY byte
// columns, so the phase = the GLOBAL CGA screen byte & 3 matters: PC_GRAPH.OVR's
// brush blitter (0x88c) indexes `pattern[idx*4 + ([0x9382] & 3)]` where [0x9382]
// = (x+20)>>2 is the global byte, advancing one phase per byte written (0x94c).
// The picture window is offset +20 px = +5 bytes ≡ +1 (mod 4), so the global
// byte phase = (logByte + 1) & 3.  (The flood fill is a separate path,
// pf_pattern_word, which paints even-byte-aligned words; this function is only
// the brush/glyph/rect path.)
static inline uint8_t fill_pattern(uint8_t sel, int y, int logByte) {
    uint8_t sub = s_fillSubindex[(unsigned)sel * 2 + (y & 1)];
    if (s_v1)
        return (sub < 30) ? s_v1PatLsb[sub][(logByte + 1) & 3] : 0x00;
    return (sub < 76) ? s_fillTable[sub] : 0x00;
}

// Fill pixels [l..r] on row y with the current fill pattern (selector `sel`).
// The pattern tile covers 4 pixels and is aligned to the byte grid by absolute
// x phase (x & 3), so adjacent fills tile seamlessly.
static void fill_hline(int y, int l, int r, uint8_t sel) {
    if ((unsigned)y >= GMCGA_HEIGHT) return;
    if (l < 0) l = 0;
    if (r >= GMCGA_WIDTH) r = GMCGA_WIDTH - 1;
    if (l > r) return;

    const int base = y * GMCGA_STRIDE;
    int bl = l >> 2, br = r >> 2;

    if (bl == br) {
        const uint8_t fb = fill_pattern(sel, y, bl);
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
        const uint8_t fb = fill_pattern(sel, y, bl);
        uint8_t v = s_screenmem[base + bl];
        for (int x = l, xe = (bl + 1) << 2; x < xe; x++) {
            int shift = (x & 3) << 1;
            v = (v & ~(0x3u << shift)) | (((fb >> shift) & 0x3u) << shift);
        }
        store_byte(base + bl, v);
    }
    // Whole bytes in the middle (period-4: same pattern byte every column)
    for (int b = bl + 1; b < br; b++) store_byte(base + b, fill_pattern(sel, y, b));
    // Right partial byte
    {
        const uint8_t fb = fill_pattern(sel, y, br);
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
// Mechanical port of the native PicOp14Paint (0x2630) and its helpers, from the
// Ghidra disassembly of NOVEL.EXE.  The native fill is NOT a per-pixel flood:
//
//   * The cursor is a (byte, pixel) pair: an even byte offset into the 80-byte
//     CGA row plus a pixel index 0-7 across that 16-bit word.  Byte offsets are
//     GLOBAL screen bytes -- the picture window occupies bytes 5..74 (x+20
//     pixels, [0xb02d]); the margins outside it are never written and read
//     back as black.
//   * The fillable predicate is "pixel == white (3)" (FUN_29a0 compares the
//     masked word with the full per-pixel mask); ANY non-white pixel is a
//     boundary.
//   * Painting happens one 16-bit word at a time (FUN_29be):
//         screen = (pattern & M) | (screen ^ M)
//     where M is the mask of the white run found in that word (the boundary
//     scanners FUN_29e1 / FUN_2a07 return it); white == all 1s makes the XOR
//     preserve every pixel outside M exactly.
//   * A row span is grown by FUN_2cd0: paint the seed word's white run, extend
//     left word by word until a word contains a non-white pixel at/before the
//     seed column, then extend right (bounded by [0x9d58] == 0x4a) while words
//     are entirely white; boundary words get their white run painted up to the
//     boundary.
//   * Propagation uses a 32-entry circular FIFO of spans (head 0xb0d1, tail
//     0xb0d2; arrays LB/LP/RB/RP/Y/DIR at 0xb0d3/0xb0f3/0xb113/0xb133/0xb153/
//     0xb173).  Each entry re-scans its parent extent one row up (dir 0) or
//     down (dir 1), with overlap-merge logic against other queued spans on the
//     same row (the 0x2700-0x2808 block) and turn-around pushes for span
//     overhang (0x2848-0x28ee).  Because spans re-read the screen as they scan,
//     freshly painted dither-black pixels self-limit later spans; reproducing
//     that traversal order exactly is what makes the fill pixel-exact.
//
// Labels in the code below are the CS-relative addresses of the corresponding
// native instructions.

// Per-pixel 2-bit masks over the byteswapped (MSB-first) screen word, and the
// "everything right of pixel i" / "everything left of pixel i" masks used to
// blank out the far side of the seed during the in-word boundary scans.
// (DS:0xb1a0 / 0xb1b0 / 0xb1c0.)
static const uint16_t kPfPixMask[8] = {
    0xC000, 0x3000, 0x0C00, 0x0300, 0x00C0, 0x0030, 0x000C, 0x0003
};
static const uint16_t kPfRightOf[8] = {
    0x3FFF, 0x0FFF, 0x03FF, 0x00FF, 0x003F, 0x000F, 0x0003, 0x0000
};
static const uint16_t kPfLeftOf[8] = {
    0x0000, 0xC000, 0xF000, 0xFC00, 0xFF00, 0xFFC0, 0xFFF0, 0xFFFC
};

// Window bounds, set before every picture by FUN_2556 (called from the picture
// entry FUN_1f40): rows 0..0x9f, global bytes 5..0x4a (CGA branch, [0xabd6]=0).
#define PF_RIGHT_BYTE 0x4a
#define PF_BOTTOM_ROW 0x9f

// Flood-fill clip rectangle (native [0x9387]=left byte, [0x9388]=right byte,
// [0x9389]=top row, [0x938a]=bottom row).  In the v1 games op15 (RESET) sets
// these -- it is fill-clip setup, NOT a rectangle paint (PC_GRAPH.OVR 0x2de).
// They default to the picture window so the flood fill is unbounded within it
// even when op15 is never emitted (which is the case for the whole v1 corpus).
static int s_pfLeftByte   = 5;
static int s_pfRightByte  = PF_RIGHT_BYTE;
static int s_pfTopRow     = 0;
static int s_pfBottomRow  = PF_BOTTOM_ROW;

static inline uint8_t mirror_2bpp_byte(uint8_t b) {
    return (uint8_t)(((b & 0x03) << 6) | ((b & 0x0c) << 2) |
                     ((b & 0x30) >> 2) | ((b & 0xc0) >> 6));
}

// Read / write the logical screen word at global byte offset `gb` (even) of row
// `y`: pixel k (0..7, left to right) sits in bits (15-2k):(14-2k), matching the
// native's MOV AX,ES:[BX+SI] / XCHG AH,AL.  Bytes outside the picture window
// read as black and ignore writes, like the untouched CGA margins.
static uint16_t pf_get_word(int y, int gb) {
    uint16_t w = 0;
    if ((unsigned)y < GMCGA_HEIGHT) {
        for (int i = 0; i < 2; i++) {
            const int b = gb + i;
            if (b >= 5 && b <= 74)
                w |= (uint16_t)mirror_2bpp_byte(s_screenmem[y * GMCGA_STRIDE + (b - 5)])
                     << (8 - 8 * i);
        }
    }
    return w;
}

static void pf_put_word(int y, int gb, uint16_t w) {
    if ((unsigned)y >= GMCGA_HEIGHT) return;
    for (int i = 0; i < 2; i++) {
        const int b = gb + i;
        if (b >= 5 && b <= 74)
            store_byte(y * GMCGA_STRIDE + (b - 5),
                       mirror_2bpp_byte((uint8_t)(w >> (8 - 8 * i))));
    }
}

// FUN_29a0: is the pixel at (byte `B`, pixel `P`) of row `y` white?
static inline bool pf_white(int y, uint8_t B, uint8_t P) {
    const uint16_t w = pf_get_word(y, B);
    return (w & kPfPixMask[P]) == kPfPixMask[P];
}

// FUN_24a4 pattern fetch: the fill-pattern word for row `y` under selector
// `sel`, in the same byteswapped bit order as pf_get_word.  Row parity picks
// the even/odd subindex ([0xb031] == 0); both bytes of every pattern entry are
// equal in this release, so the word is just the byte doubled.
// `gb` is the (even) global CGA byte column of the word being painted.  For v2
// (period-4 patterns) every column is the same byte, so gb is irrelevant; for
// v1 (period-16) the high byte uses phase gb&3 and the low byte phase (gb+1)&3,
// matching the global byte column each half of the word lands at.
static inline uint16_t pf_pattern_word(uint8_t sel, int y, int gb) {
    const uint8_t sub = s_fillSubindex[(unsigned)sel * 2 + (y & 1)];
    if (s_v1) {
        if (sub >= 30) return 0;
        const uint8_t b = s_v1PatRaw[sub][0];   // period-4: entry[0] both halves
        return (uint16_t)((b << 8) | b);
    }
    const uint8_t m = (sub < 76) ? s_fillTableRaw[sub] : 0x00;
    return (uint16_t)((m << 8) | m);
}

// Fill cursor / span state ("registers" of the native fill).
struct PfState {
    int     y;        // 0x9d36 -- current row (transiently -1 / >159 at edges)
    uint8_t B, P;     // 0x9d52 / 0x9d53 -- cursor byte (even) + pixel 0-7
    uint8_t lB, lP;   // 0xb0d0 / 0xb1d4 -- span left edge during scan / push
};

// FUN_2df7: step the cursor one pixel left.
static inline void pf_dec(PfState &s) {
    if (s.P == 0) { s.P = 7; s.B = (uint8_t)(s.B - 2); }
    else s.P--;
}

// Lexicographic compare of two (byte, pixel) positions.
static inline int pf_cmp(uint8_t b1, uint8_t p1, uint8_t b2, uint8_t p2) {
    if (b1 != b2) return b1 < b2 ? -1 : 1;
    if (p1 != p2) return p1 < p2 ? -1 : 1;
    return 0;
}

// FUN_29e1: scan the word in `A` from pixel 7 leftward for the first non-white
// pixel.  On a hit, records its index in `*edge` and returns the "pixels right
// of it" mask; if `A` is all white it is returned unchanged (and *edge keeps
// its previous value -- the native leaves [0xb1d4] alone too).
static inline uint16_t pf_scan_l(uint16_t A, uint8_t *edge) {
    if (A != 0xffff)
        for (int i = 7; i >= 0; i--)
            if ((A & kPfPixMask[i]) != kPfPixMask[i]) {
                *edge = (uint8_t)i;
                return kPfRightOf[i];
            }
    return A;
}

// FUN_2a07: mirror image of pf_scan_l -- scan from pixel 0 rightward, return
// the "pixels left of the boundary" mask.
static inline uint16_t pf_scan_r(uint16_t A, uint8_t *edge) {
    if (A != 0xffff)
        for (int i = 0; i < 8; i++)
            if ((A & kPfPixMask[i]) != kPfPixMask[i]) {
                *edge = (uint8_t)i;
                return kPfLeftOf[i];
            }
    return A;
}

static bool s_pfTrace = false;  // debug: log queue pushes (GMCGA_TRACE_FILL)
static int  s_pfWordSeq = 0;    // debug: per-fill word-paint sequence counter

// FUN_29be: paint the word at (row `y`, byte `gb`): every pixel selected by
// mask `M` -- always a run of white pixels -- takes the pattern, the rest of
// the word is preserved ((pat & M) | (S ^ M), exploiting white == all 1s).
static inline void pf_paint_word(int y, int gb, uint8_t sel, uint16_t S, uint16_t M) {
    const uint16_t pat = pf_pattern_word(sel, y, gb);
    if (s_pfTrace) {
        const uint8_t sub = s_fillSubindex[(unsigned)sel * 2 + (y & 1)];
        fprintf(stderr, "WORD seq=%d row=%d gb=%d sub=%d pat=%04x M=%04x\n",
                ++s_pfWordSeq, y, gb, sub, pat, M);
    }
    pf_put_word(y, gb, (uint16_t)((pat & M) | (S ^ M)));
}

// FUN_2cd0: grow and paint the span around the seed in s.B/s.P on row s.y.
// On return s.lB/s.lP hold the left edge and s.B/s.P the right edge (both
// inclusive, first/last white pixel).  Painting is word-at-a-time, masked to
// the white run found in each word.
static void pf_scan_paint(PfState &s, uint8_t sel) {
    int si = s.B;
    s.lP = 0xff;                                         // 0xff = no left edge yet
    uint16_t S = pf_get_word(s.y, si);

    // Seed word: find the nearest boundary on each side of the seed pixel (the
    // far side blanked to white for each scan) and paint the run between them.
    const uint8_t seedP = s.P;
    const uint16_t mL = pf_scan_l((uint16_t)(S | kPfRightOf[seedP]), &s.lP); // [0xb1d0]
    s.P = 8;                                             // 8 = open to the right
    const uint16_t mR = pf_scan_r((uint16_t)(kPfLeftOf[seedP] | S), &s.P);
    pf_paint_word(s.y, si, sel, S, (uint16_t)(mR & mL)); // 29be

    while (s.lP == 0xff) {                               // 2d1d: extend left
        si -= 2;
        S = pf_get_word(s.y, si);
        const uint16_t M = pf_scan_l(S, &s.lP);
        pf_paint_word(s.y, si, sel, S, M);
    }
    {                                                    // 2d43: finalise left edge
        int p = s.lP + 1;
        if (p == 8) { p = 0; si += 2; }
        s.lB = (uint8_t)si;
        s.lP = (uint8_t)p;
    }

    si = s.B;                                            // 2d5e: extend right
    while (s.P == 8 && si < s_pfRightByte) {
        si += 2;
        S = pf_get_word(s.y, si);
        const uint16_t M = pf_scan_r(S, &s.P);
        pf_paint_word(s.y, si, sel, S, M);
    }
    if (s.P == 0) { s.P = 7; si -= 2; }                  // 2d88: last white pixel
    else s.P--;
    s.B = (uint8_t)si;
}

static void gmcga_flood_fill(int seed_x, int seed_y, uint8_t sel) {
    if ((uint8_t)seed_y > s_pfBottomRow)                 // 263b
        return;

    PfState s;
    s.y = seed_y;
    const int gx = seed_x + 20;                          // 24d1: + [0xb02d]
    s.B = (uint8_t)(gx >> 2);
    s.P = (uint8_t)(gx & 3);
    if (s.B & 1) { s.P += 4; s.B--; }                    // 264b: word-align
    s.lB = s.lP = 0;

    if (!pf_white(s.y, s.B, s.P))                        // 265b: seed must be white
        return;

    // 32-entry circular span queue (LB/LP/RB/RP/Y/DIR arrays at 0xb0d3..0xb173).
    uint8_t q_lb[32], q_lp[32], q_rb[32], q_rp[32], q_y[32], q_d[32];
    uint8_t head = 0, tail = 0;                          // 0xb0d1 / 0xb0d2
    bool overflow = false;                               // 0xb193

    // FUN_2da0: append the current span going `dir` (0 = up, 1 = down).  The
    // up-push bound ([0x9d59] == 0, unsigned compare) never rejects, so spans on
    // row 0 do push an up entry; its (clamped) target row reads black here and
    // the entry retires without effect, as on hardware.
    auto push = [&](uint8_t dir) {
        const uint8_t yb = (uint8_t)s.y;
        if (s_pfTrace)
            fprintf(stderr, "PUSH y=%d l=%d.%d r=%d.%d d=%d h=%d t=%d\n",
                    yb, s.lB, s.lP, s.B, s.P, dir, head, tail);
        if (dir != 0 && yb >= s_pfBottomRow)
            return;
        q_y[tail] = yb;
        q_lb[tail] = s.lB; q_lp[tail] = s.lP;
        q_rb[tail] = s.B;  q_rp[tail] = s.P;
        q_d[tail] = dir;
        tail = (uint8_t)((tail + 1) & 0x1f);
        if (tail == head)
            overflow = true;                             // fill aborts
    };

    pf_scan_paint(s, sel);                               // 266e: seed row
    push(0);                                             // 2675
    push(1);                                             // 2679

    long guard = 0;
    for (;;) {                                           // 267c: main queue loop
        if (head == tail || ++guard > 1000000)
            return;
        {
            const uint8_t d = q_d[head];
            if (d == 0xff) {                             // 2699: dead entry
                head = (uint8_t)((head + 1) & 0x1f);
                continue;
            }
            // 269e/269b: target row above or below the parent span's row.
            s.y = (int)q_y[head] + (d == 0 ? -1 : 1);
            s.B = q_lb[head];                            // 26a3: start at parent left
            s.P = q_lp[head];
        }

    scan_26ba: // hunt for a white seed within the parent extent
        if (pf_white(s.y, s.B, s.P))
            goto found_2700;
    advance_26bf:
        if (s.B == q_rb[head] && s.P == q_rp[head])      // parent extent exhausted
            goto retire_26ee;
        s.P++;
        if (s.P == 8) { s.P = 0; s.B = (uint8_t)(s.B + 2); }
        goto scan_26ba;

    found_2700: // before scanning, reconcile with other queued spans on this row
        {
            uint8_t E = head;
            for (;;) {
                E = (uint8_t)((E + 1) & 0x1f);           // 2706
                if (E == tail)
                    goto newspan_2808;                   // nothing overlaps
                if (q_y[E] != (uint8_t)s.y)              // 271a
                    continue;
                if (q_d[E] == q_d[head])                 // 272a: opposite dir only
                    continue;
                if (pf_cmp(s.B, s.P, q_rb[E], q_rp[E]) > 0)  // 2730: seed right of E
                    continue;
                const int c = pf_cmp(s.B, s.P, q_lb[E], q_lp[E]); // 2748
                if (c < 0)                               // seed left of E
                    continue;
                if (c > 0) {
                    // 275b: seed strictly inside E.  If E sticks out left of the
                    // parent extent, requeue that remainder under E's direction.
                    s.B = q_lb[head]; s.P = q_lp[head];
                    pf_dec(s);                           // 2769: parent.left - 1
                    if (pf_cmp(s.B, s.P, q_lb[E], q_lp[E]) > 0) {
                        s.lB = q_lb[E]; s.lP = q_lp[E];  // 2781
                        pf_dec(s);                       // 278f: parent.left - 2
                        push(q_d[E]);                    // 279e
                        if (overflow)
                            return;                      // 27a8
                    }
                }
                // 27b9: resume after E's right edge, or split E around the parent.
                s.B = q_rb[E];
                if (pf_cmp(q_rb[E], q_rp[E], q_rb[head], q_rp[head]) <= 0) {
                    q_d[E] = 0xff;                       // 27d2: E fully shadowed
                    s.P = q_rp[E];
                    goto advance_26bf;
                }
                q_lb[E] = q_rb[head];                    // 27e2: clip E to start
                {                                        //  after the parent extent
                    uint8_t p = (uint8_t)(q_rp[head] + 1);
                    if (p == 8) { p = 0; q_lb[E] = (uint8_t)(q_lb[E] + 2); }
                    q_lp[E] = p;
                }
                goto retire_26ee;
            }
        }

    newspan_2808: // grow + paint a fresh span around the seed
        {
            pf_scan_paint(s, sel);
            push(q_d[head]);                             // 2819: keep propagating
            if (overflow)
                return;                                  // 2823
            const uint8_t savB = s.B, savP = s.P;        // 2828: save right edge

            // 2834: left overhang -- if the new span reaches 2+ pixels left of
            // the parent extent, push the overhang back the way we came.
            s.B = q_lb[head]; s.P = q_lp[head];
            pf_dec(s);                                   // 2848: parent.left - 1
            if (pf_cmp(s.B, s.P, s.lB, s.lP) > 0) {
                pf_dec(s);                               // 285f: parent.left - 2
                push((uint8_t)(q_d[head] ^ 1));          // 286c
                if (overflow)
                    return;
            }
            s.B = savB; s.P = savP;                      // 287b: restore right edge

            // 288d: right overhang vs parent.right + 1.
            s.lB = q_rb[head];
            uint8_t p = (uint8_t)(q_rp[head] + 1);
            if (p == 8) { p = 0; s.lB = (uint8_t)(s.lB + 2); }
            s.lP = p;
            const int c = pf_cmp(s.lB, s.lP, s.B, s.P);
            if (c < 0) {                                 // 28c3: 2+ pixel overhang
                p++;
                if (p == 8) { p = 0; s.lB = (uint8_t)(s.lB + 2); }
                s.lP = p;
                push((uint8_t)(q_d[head] ^ 1));          // 28e0
                if (overflow)
                    return;
                goto retire_26ee;                        // 28ef
            }
            if (c == 0)                                  // ends exactly at +1
                goto retire_26ee;
            goto advance_26bf;                           // 28f2: keep hunting
        }

    retire_26ee:
        head = (uint8_t)((head + 1) & 0x1f);
    }
}

// ---- Line drawing (op10 DRAW_LINE / op9 DRAW_BOX) ----------------------------
// Geometry lives in gmvDrawLine (shared with the PCjr renderer); here we bind it
// to the 2-bpp pixel writer.
static void draw_line(int x0, int y0, int x1, int y1, uint8_t pen_val) {
    gmvDrawLine(x0, y0, x1, y1,
                [pen_val](int x, int y) { write_2bpp(x, y, pen_val); });
}

// ---- Circle (op11 DRAW_CIRCLE) -----------------------------------------------
// Geometry lives in gmvDrawCircle (shared with the PCjr renderer); bound to the
// 2-bpp pixel writer.  (Verified against a DOSBox VRAM trace of the title's
// lamp-knob circle.)
static void draw_circle(int cx, int cy, int radius, uint8_t pen_val) {
    gmvDrawCircle(cx, cy, radius,
                  [pen_val](int x, int y) { write_2bpp(x, y, pen_val); });
}

// ---- Brush / glyph column blitter --------------------------------------------
//
// Blits 8 rows of 8-pixel data onto the framebuffer.  Each byte encodes the
// row's pixels MSB-first: bit 7 = leftmost pixel (pixel 0), bit 0 = rightmost
// (pixel 7).  A set bit writes the fill-pattern pixel at that position; a
// clear bit leaves the background unchanged.

static void blit_col(const uint8_t *bytes8, int px, int py, uint8_t fill_sel) {
    // Faithful port of PC_GRAPH.OVR's brush blitter (0x88c + spill helper 0x94c).
    // The native bit-doubles each 8-px row to a 16-bit 2-bpp mask (pixel 0 in
    // bits 15:14, MSB-first), shifts it right by the sub-pixel offset so it
    // spills across up to THREE output bytes, then paints the fill pattern
    // through the mask one output byte at a time.
    //
    // The fill-pattern dither PHASE does NOT track the byte column: the spill
    // helper increments the pattern index only for bytes it actually paints
    // (its `cmp dl,0; je` returns before `inc di`), while the byte cursor
    // advances unconditionally.  So a brush whose first/middle output byte is
    // blank lands its spill byte one phase earlier than its column would imply
    // -- which is what colours the off-grid Crimson Crown crypt spill byte
    // magenta (phase 3) instead of cyan (phase 0).  Replicate the painted-byte
    // walk so the dithered spill matches the interpreter.
    const int lb0 = px >> 2;          // logical byte column of the brush anchor
    const int sub = px & 3;           // sub-pixel offset (== (px+20) & 3)
    const int p0  = (lb0 + 1) & 3;    // global byte phase of the first output byte
    for (int row = 0; row < 8; row++, py++) {
        if ((unsigned)py >= GMCGA_HEIGHT) continue;
        const uint8_t b = bytes8[row];
        if (!b) continue;
        uint16_t mask16 = 0;
        for (int p = 0; p < 8; p++)
            mask16 = (uint16_t)((mask16 << 2) | (((b >> (7 - p)) & 1) ? 0x3 : 0x0));
        const uint32_t vs = ((uint32_t)mask16 << 8) >> (sub << 1);
        const uint8_t m[3] = { (uint8_t)(vs >> 16), (uint8_t)(vs >> 8), (uint8_t)vs };
        int phase = p0;
        for (int k = 0; k < 3; k++) {
            if (k == 0) {
                if (!m[0]) continue;          // first byte uses p0 directly
            } else {
                if (!m[k]) continue;          // blank byte: cursor moves, phase does not
                phase = (phase + 1) & 3;       // painted byte: advance the dither phase
            }
            const int lb = lb0 + k;
            // Feed fill_pattern a synthetic column so its (logByte+1)&3 yields
            // `phase`; for v2 the pattern is phase-independent, so this is inert.
            const uint8_t fb = fill_pattern(fill_sel, py, (phase + 3) & 3);
            for (int pos = 0; pos < 4; pos++) {
                if (((m[k] >> ((3 - pos) << 1)) & 0x3) == 0) continue;
                const int x = (lb << 2) + pos;
                if ((unsigned)x >= GMCGA_WIDTH) continue;
                const uint8_t pix = (fb >> ((x & 3) << 1)) & 0x3;
                write_2bpp(x, py, pix);
            }
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
        if ((unsigned)py >= GMCGA_HEIGHT) continue;
        for (int p = 0; p < 8; p++) {
            if (!((b >> (7 - p)) & 1)) continue;
            const int px = x + p;
            if ((unsigned)px >= GMCGA_WIDTH) continue;
            const uint8_t fb = normal ? 0 : fill_pattern(fill_sel, py, px >> 2);
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
    for (int y = s_fill_top; y <= s_fill_bottom && y < GMCGA_HEIGHT; y++)
        fill_hline(y, lp, rp, fill_idx);
}

// ---- Pen colour mapping ------------------------------------------------------
//
// SET_PEN_COLOR (op2) carries a colour index in its low nibble; the DOS
// interpreter stores it as the pen colour (0x9d38) and draws lines/solid text in
// it as a plain index & 3 (the CGA palette has only four colours).  Verified
// against the title (uses 2, 4, 6) and room RG_07, whose pen-colour-5 (-> cyan)
// strokes over a magenta fill band were the last non-exact room: an earlier
// guess mapped 5 -> magenta, which left those strokes invisible and exposed the
// fill beneath.

static const uint8_t kPenTo2bpp[8] = { 0, 1, 2, 3, 0, 1, 2, 3 };

// ---- Drawing context ---------------------------------------------------------

struct GmcgaCtx {
    int16_t  pen_x, pen_y;
    uint8_t  pen_val;   // 2-bpp pixel value (0–3) for lines / op3 text
    uint8_t  brush;     // brush index 0–7
    uint8_t  fill_idx;  // SET_FILL_COLOR selector byte (-> subindex table)
    uint16_t text_x;
    uint8_t  text_y;
};

// ---- Opcode interpreter ------------------------------------------------------

static bool doImageOp(const uint8_t **ptr, const uint8_t *end, GmcgaCtx *ctx) {
    if (*ptr >= end) return true;
    const uint8_t op_byte = *(*ptr)++;
    const uint8_t param   = op_byte & 0xf;
    const uint8_t op      = op_byte >> 4;
    uint16_t a, b;

    if (getenv("GMCGA_OPLOG")) {
        static int oc = 0;
        fprintf(stderr, "OP #%d op=%u param=%u nextbytes=%02x %02x %02x\n",
                ++oc, op, param,
                (*ptr < end) ? (*ptr)[0] : 0,
                (*ptr + 1 < end) ? (*ptr)[1] : 0,
                (*ptr + 2 < end) ? (*ptr)[2] : 0);
    }

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
        // PicStampBrushShape (0x2967) starts with DEC AX: brushes stamp one
        // pixel left of the stream coordinate (text glyphs do not).
        draw_brush((int)a - 1, (int)b, ctx->brush, ctx->fill_idx);
        break;

    case OPCODE_DELAY: { // op13 -- pacing hint; no effect on the final page
        uint8_t units = *(*ptr)++;
        s_slow.recordDelay(units);
        break;
    }

    case OPCODE_PAINT: // op14 -- flood fill from seed point
        a = *(*ptr)++ + (param & 1 ? 256 : 0);
        b = *(*ptr)++;
        // Debug aid: dump the framebuffer before AND after each fill for
        // comparison against a DOSBox trace of the native interpreter.  The
        // before-dumps confirm the canvas going into a fill matches; the
        // after-dumps isolate whether a divergence is the fill itself (vs a
        // later primitive).  See test/gmcgav1/compare_op14.py.
        {
            static int n = 0;
            n++;
            const char *td = getenv("GMCGA_TRACE_DIR");
            if (td) {
                char p[512];
                snprintf(p, sizeof(p), "%s/fill_%03d.raw", td, n);
                if (FILE *f = fopen(p, "wb")) {
                    fwrite(s_screenmem, 1, sizeof(s_screenmem), f);
                    fclose(f);
                }
            }
            const char *tf = getenv("GMCGA_TRACE_FILL");
            s_pfTrace = tf && atoi(tf) == n;
            s_pfWordSeq = 0;
            gmcga_flood_fill((int)a, (int)b, ctx->fill_idx);
            if (td) {
                char p[512];
                snprintf(p, sizeof(p), "%s/fill_%03d_after.raw", td, n);
                if (FILE *f = fopen(p, "wb")) {
                    fwrite(s_screenmem, 1, sizeof(s_screenmem), f);
                    fclose(f);
                }
            }
        }
        break;

    case OPCODE_RESET: // op15
        if (s_v1) {
            // v1 (PC_GRAPH.OVR 0x2de): op15 is flood-fill CLIP-RECT setup, not a
            // rectangle paint.  param 0 = no-op; param 1 = reset the clip to the
            // window defaults; param >=2 = read 4 bytes into the clip rectangle
            // ([0x9388] right byte, [0x9387] left byte, [0x938a] bottom row,
            // [0x9389] top row).  No v1 picture in the corpus emits it, so this
            // is faithful-but-inert today; the 4-byte consume keeps the stream
            // pointer aligned for any picture that ever does.
            switch (param) {
            case 1:
                s_pfLeftByte = 4; s_pfRightByte = PF_RIGHT_BYTE;
                s_pfTopRow = 0;   s_pfBottomRow = PF_BOTTOM_ROW;
                break;
            default:
                if (param >= 2) {
                    s_pfRightByte  = *(*ptr)++;
                    s_pfLeftByte   = *(*ptr)++;
                    s_pfBottomRow  = *(*ptr)++;
                    s_pfTopRow     = *(*ptr)++;
                }
                break; // param 0: nothing
            }
            break;
        }
        // v2 / Apple Talisman: op15 is the fill-rectangle opcode.
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

    // Debug: report which op first writes a watched pixel (GMCGA_WATCH="x,y").
    {
        static int opn = 0;
        opn++;
        if (const char *w = getenv("GMCGA_WATCH")) {
            int wx, wy;
            if (sscanf(w, "%d,%d", &wx, &wy) == 2) {
                static int last = -2;
                int cur = read_2bpp(wx, wy);
                if (cur != last) {
                    fprintf(stderr, "WATCH op#%d op=%u param=%u fill_idx=%u "
                            "-> pixel(%d,%d) %d->%d\n",
                            opn, op, param, ctx->fill_idx, wx, wy, last, cur);
                    last = cur;
                }
            }
        }
    }
    return false;
}

void gmcgaDrawImage(const uint8_t *data, size_t size) {
    // Each image is its own reveal: drop the previous image's op list (a room
    // reset already cleared it; an item overlay reaches here without one and
    // records only its own bytes onto whatever the room left visible).
    s_slow.clear();

    GmcgaCtx ctx = {};
    ctx.pen_val  = 0;    // pen colour 4 (black) maps to 2-bpp 0
    // The dispatcher init (NOVEL.EXE 0x1f56 / Transylvania 0x1e46) seeds the
    // pen position with 0x8c,6; Transylvania's title stream draws its first
    // line before any MOVE_TO, so this matters.
    ctx.pen_x    = 140;
    ctx.pen_y    = 6;
    ctx.brush    = 5;    // default brush matches Apple renderer init
    ctx.fill_idx = 0;    // cold-start fill selector 0 -> solid white (0xff);
                         // matches NOVEL.EXE's statically-zeroed [0x9d46]
    ctx.text_x   = 140; ctx.text_y = 96;
    s_fill_left = 0; s_fill_top = 0; s_fill_right = 39; s_fill_bottom = 159;
    // Flood-fill clip rectangle resets to the picture window before each image
    // (native FUN_2556); op15 may then narrow it (v1 only).
    s_pfLeftByte = 4; s_pfRightByte = PF_RIGHT_BYTE;
    s_pfTopRow = 0;   s_pfBottomRow = PF_BOTTOM_ROW;

    const uint8_t *ptr = data;
    const uint8_t *end = data + size;
    int guard = 0;
    while (ptr < end && ++guard < 100000)
        if (doImageOp(&ptr, end, &ctx)) break;

    // When not animating, keep the visible page in step with the final page so a
    // later slow-drawn overlay composes onto the right background.
    s_slow.syncIfNotRecording();
}

// ---- 2-bpp → RGBA conversion -------------------------------------------------
//
// The four pixel values are CGA palette 1 (low intensity) colour indices, the
// palette NOVEL1.EXE selects on a colour adapter (INT 10h AH=0Bh in the picture
// init at 0x22ff): black / cyan / magenta / grey.  These are the exact RGB
// values DOSBox emits for the game (verified against the test/gmcga goldens);
// the original ran in low intensity, not the brighter 55ffff/ff55ff/ffffff.
// The 280×160 DrawSurface matches the logical raster exactly, so no scaling is
// needed in the common case.

static const uint32_t kGmcgaColor[4] = {
    0x000000ffu, // 0: black
    0x00aaaaffu, // 1: cyan
    0xaa00aaffu, // 2: magenta
    0xaaaaaaffu, // 3: grey (CGA "white" at low intensity)
};

static void blit_page(const uint8_t *page, uint32_t *out, int w, int h) {
    for (int y = 0; y < h; y++) {
        const int sy = (y * GMCGA_HEIGHT) / h;
        const uint8_t *row = &page[sy * GMCGA_STRIDE];
        for (int x = 0; x < w; x++) {
            const int sx = (x * GMCGA_WIDTH) / w;
            out[y * w + x] = kGmcgaColor[(row[sx >> 2] >> ((sx & 3) << 1)) & 0x3];
        }
    }
}

void gmcgaBlitToSurface(uint32_t *out, int w, int h) {
    blit_page(s_screenmem, out, w, h);
}

// ---- Slow-draw control (host-driven) ----------------------------------------
// These mirror the gm* entry points in graphics_magician.h one-for-one, so the
// host (Comprehend::drawPicture / runSlowDraw) drives whichever renderer is
// active with the same logic.  The machinery lives in the shared SlowDrawPage.

void gmcgaSetSlowDraw(bool on) { s_slow.setRecording(on); }
bool gmcgaSlowDrawActive() { return s_slow.active(); }
bool gmcgaAdvanceSlowDraw(int budget) { return s_slow.advance(budget); }
void gmcgaFinishSlowDraw() { s_slow.finish(); }
bool gmcgaSlowConsumeDirty(int *y0, int *y1) { return s_slow.consumeDirty(y0, y1); }

void gmcgaBlitSlowToSurface(uint32_t *out, int w, int h) {
    blit_page(s_slowScreen, out, w, h);
}

} // namespace Comprehend
} // namespace Glk
