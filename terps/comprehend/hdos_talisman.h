/* hdos_talisman.h -- DOS Talisman CGA picture renderer.
 *
 * Renders the shared Graphics Magician vector stream into a 280×160, 2-bpp
 * (4-colour) logical framebuffer matching the native picture interpreter in the
 * DOS release NOVEL.EXE / NOVEL1.EXE, then converts that to RGBA for output.
 *
 * The drawing tables (fill patterns, brush bitmaps, font glyphs) are loaded at
 * runtime from the NOVEL.EXE image: read the file into a buffer and pass it to
 * hdosInstallDrawingTables(); check hdosHaveDrawingTables() before routing
 * pictures through this renderer.
 *
 * Opcode semantics, coordinate space (280×160), and the opcode stream format
 * are identical to the Apple II renderer in graphics_magician.cpp.  The key
 * differences are all in the pixel model:
 *   - 2 bits per pixel; values 0-3 are CGA palette-1 colour indices
 *     (black / cyan / magenta / white)
 *   - 4 pixels per byte, LSB-first (pixel 0 in bits 1:0)
 *   - Fill patterns: 76-entry LUT indexed directly by op6 fill-colour byte;
 *     each entry is one 2-bpp byte (4-pixel tile, uniform across all columns)
 *   - Brush bytes: MSB-first (bit 7 = leftmost pixel of each 8-pixel column)
 *   - Brush quadrant order in NOVEL.EXE: Q0=upper-right, Q1=lower-right,
 *     Q2=upper-left, Q3=lower-left (column-major, right column first)
 *   - Text advances 8 px/char (not 7 like the Apple renderer)
 */

#ifndef GLK_COMPREHEND_HDOS_TALISMAN_H
#define GLK_COMPREHEND_HDOS_TALISMAN_H

#include <cstdint>
#include <cstddef>

namespace Glk {
namespace Comprehend {

// Load fill patterns, brush bitmaps, and font glyphs from a NOVEL.EXE /
// NOVEL1.EXE image already read into memory.  The tables are located by the
// fill-table signature, so this works for both files.  Returns true on
// success; sets hdosHaveDrawingTables().
bool hdosInstallDrawingTables(const uint8_t *exe, size_t size);

// True after a successful hdosInstallDrawingTables() call.
bool hdosHaveDrawingTables();

// Clear the 280×160 2-bpp framebuffer.  Pass white=true for a room reset
// (white background), false for a dark/transparent base.
void hdosResetScreen(bool white);

// Run one image's vector-opcode stream onto the 2-bpp framebuffer.
void hdosDrawImage(const uint8_t *data, size_t size);

// Convert the 2-bpp framebuffer to RGBA (0xRRGGBBAA) into out[w*h].
// Scales the 280×160 logical page to the requested dimensions.
void hdosBlitToSurface(uint32_t *out, int w, int h);

// ---- Slow ("animated") draw -------------------------------------------------
// One-for-one with the gm* slow-draw API in graphics_magician.h: when enabled,
// hdosDrawImage records the order its bytes are plotted so the picture can be
// revealed progressively, and the host drives the reveal on a timer.
void hdosSetSlowDraw(bool on);
bool hdosSlowDrawActive();
bool hdosAdvanceSlowDraw(int budget);
void hdosFinishSlowDraw();
bool hdosSlowConsumeDirty(int *y0, int *y1);
void hdosBlitSlowToSurface(uint32_t *out, int w, int h);

} // namespace Comprehend
} // namespace Glk

#endif
