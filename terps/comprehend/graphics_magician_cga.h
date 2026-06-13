/* graphics_magician_cga.h -- DOS (CGA) Graphics Magician picture renderer.
 *
 * DOS/CGA counterpart of graphics_magician.cpp (Apple II); shared by every
 * Comprehend v2 DOS release (Talisman, OO-Topos, Transylvania v2, Coveted
 * Mirror), whose NOVEL.EXE images carry byte-identical CGA drawing tables, and
 * by the v1 releases (Crimson Crown, Transylvania v1) via the separate
 * gmcgaInstallV1DrawingTables() loader -- v1 splits the tables across NOVEL.EXE
 * (brushes) and PC_GRAPH.OVR (fill pattern + subindex) and uses a 4-phase-byte
 * fill pattern format, but the opcode interpreter and pixel model are identical.
 *
 * Renders the shared Graphics Magician vector stream into a 280×160, 2-bpp
 * (4-colour) logical framebuffer matching the native picture interpreter in the
 * DOS release NOVEL.EXE / NOVEL1.EXE, then converts that to RGBA for output.
 *
 * The drawing tables (fill patterns, brush bitmaps, font glyphs) are loaded at
 * runtime from the NOVEL.EXE image: read the file into a buffer and pass it to
 * gmcgaInstallDrawingTables(); check gmcgaHaveDrawingTables() before routing
 * pictures through this renderer.
 *
 * Opcode semantics, coordinate space (280×160), and the opcode stream format
 * are identical to the Apple II renderer in graphics_magician.cpp.  The key
 * differences are all in the pixel model:
 *   - 2 bits per pixel; values 0-3 are CGA palette-1 colour indices
 *     (black / cyan / magenta / grey), rendered at low intensity
 *   - 4 pixels per byte, LSB-first (pixel 0 in bits 1:0)
 *   - Fill patterns: 76-entry LUT indexed directly by op6 fill-colour byte;
 *     each entry is one 2-bpp byte (4-pixel tile, uniform across all columns)
 *   - Brush bytes: MSB-first (bit 7 = leftmost pixel of each 8-pixel column)
 *   - Brush quadrant order in NOVEL.EXE: Q0=upper-right, Q1=lower-right,
 *     Q2=upper-left, Q3=lower-left (column-major, right column first)
 *   - Text advances 8 px/char (not 7 like the Apple renderer)
 */

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

// True after a successful gmcgaInstall*DrawingTables() call.
bool gmcgaHaveDrawingTables();

// Clear the 280×160 2-bpp framebuffer.  Pass white=true for a room reset
// (white background), false for a dark/transparent base.
void gmcgaResetScreen(bool white);

// Run one image's vector-opcode stream onto the 2-bpp framebuffer.
void gmcgaDrawImage(const uint8_t *data, size_t size);

// Convert the 2-bpp framebuffer to RGBA (0xRRGGBBAA) into out[w*h].
// Scales the 280×160 logical page to the requested dimensions.
void gmcgaBlitToSurface(uint32_t *out, int w, int h);

// ---- Slow ("animated") draw -------------------------------------------------
// One-for-one with the gm* slow-draw API in graphics_magician.h: when enabled,
// gmcgaDrawImage records the order its bytes are plotted so the picture can be
// revealed progressively, and the host drives the reveal on a timer.
void gmcgaSetSlowDraw(bool on);
bool gmcgaSlowDrawActive();
bool gmcgaAdvanceSlowDraw(int budget);
void gmcgaFinishSlowDraw();
bool gmcgaSlowConsumeDirty(int *y0, int *y1);
void gmcgaBlitSlowToSurface(uint32_t *out, int w, int h);

} // namespace Comprehend
} // namespace Glk

#endif
