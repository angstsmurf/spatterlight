// DOS (CGA) Graphics Magician picture renderer: the counterpart of graphics_magician.cpp (Apple
// II), with the same opcodes, stream format and 280x160 coordinate space. The v2 releases
// (Talisman, OO-Topos, Transylvania v2, Coveted Mirror) carry byte-identical tables in NOVEL.EXE.
// The v1 releases (Crimson Crown, Transylvania v1) split them across NOVEL.EXE (brushes) and
// PC_GRAPH.OVR (fill patterns and subindex) and use a 4-phase-byte fill pattern format.
//
// Differences from the Apple renderer are in the pixel model:
//   - 2 bits per pixel; values 0-3 are low-intensity CGA palette-1 indices (black / cyan /
//     magenta / grey).
//   - 4 pixels per byte, LSB-first (pixel 0 in bits 1:0).
//   - Fill patterns: the op6 byte selects, through a subindex table, a pattern per row parity.
//   - Brush bytes: MSB-first (bit 7 = leftmost pixel); quadrants Q0..Q3 = upper-left,
//     lower-left, upper-right, lower-right.
//   - Text advances 8 px/char (not 7).

#ifndef GLK_COMPREHEND_GRAPHICS_MAGICIAN_CGA_H
#define GLK_COMPREHEND_GRAPHICS_MAGICIAN_CGA_H

#include <cstdint>
#include <cstddef>

namespace Glk {
namespace Comprehend {

// Load fill patterns, brush bitmaps, and font glyphs from a NOVEL.EXE /
// NOVEL1.EXE image already read into memory.  The tables are located by the
// fill-table signature, so this works for both files.  Returns true on
// success; sets gmcgaHaveDrawingTables().
bool gmcgaInstallDrawingTables(const uint8_t *exe, size_t size);

// Load the drawing tables for the v1 Comprehend DOS releases (Crimson Crown,
// Transylvania v1).  Their fill pattern + subindex tables live in the
// PC_GRAPH.OVR overlay (copied to NOVEL.EXE's DGROUP at load), and the brushes
// in NOVEL.EXE.  Pass both files; each table is located by signature.  Returns
// true on success; sets gmcgaHaveDrawingTables().  (v1 fill patterns use four
// explicit column-phase bytes per entry, unlike the v2 period-4 single byte.)
bool gmcgaInstallV1DrawingTables(const uint8_t *ovr, size_t ovrSize,
								 const uint8_t *exe, size_t exeSize);

// Load the in-picture op3/op5 font for the v1 releases from a CHARSET.GDA image
// already read into memory.  v1 NOVEL.EXE has no embedded picture font (unlike
// v2); its op3/op5 glyphs (e.g. the map-screen labels) come from CHARSET.GDA --
// version word 0x1100, then 96 glyphs (chars 32-127) of 8 bytes each at offset
// 4, stored LSB-first (bit 0 = leftmost pixel).  This reverses each byte into
// the MSB-first order draw_glyph expects.  Call after gmcgaInstallV1DrawingTables.
// Returns true on success.
bool gmcgaSetV1Font(const uint8_t *charsetGda, size_t size);

// Install the built-in copy of the drawing tables -- the v1 set (Crimson Crown,
// Transylvania v1) or the v2 one -- for a game folder that has lost the
// interpreter files the two loaders above read.  A v1 game still wants
// gmcgaSetV1Font() afterwards.
void gmcgaInstallBuiltinDrawingTables(bool v1);

// True after a gmcgaInstall*DrawingTables() call that succeeded.
bool gmcgaHaveDrawingTables();

// Clear the 280×160 2-bpp framebuffer.  Pass white=true for a room reset
// (white background), false for a dark/transparent base.
void gmcgaResetScreen(bool white);

// Run one image's vector-opcode stream onto the 2-bpp framebuffer.
void gmcgaDrawImage(const uint8_t *data, size_t size);

// Convert the 2-bpp framebuffer to RGBA (0xRRGGBBAA) into out[w*h].
// Scales the 280×160 logical page to the requested dimensions.
void gmcgaBlitToSurface(uint32_t *out, int w, int h);

// Slow ("animated") draw, one-for-one with the gm* API in graphics_magician.h: when enabled,
// gmcgaDrawImage records the order its bytes are plotted, and the host drives a progressive
// reveal on a timer.
void gmcgaSetSlowDraw(bool on);
bool gmcgaSlowDrawActive();
bool gmcgaAdvanceSlowDraw(int budget);
void gmcgaFinishSlowDraw();
bool gmcgaSlowConsumeDirty(int *y0, int *y1);
void gmcgaBlitSlowToSurface(uint32_t *out, int w, int h);

} // namespace Comprehend
} // namespace Glk

#endif
