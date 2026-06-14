/* graphics_magician_dhgr.h -- Apple II DOUBLE hi-res ("<D>" mode) renderer for
 * the V2 Comprehend / Polarware games (Talisman, OO-Topos, Transylvania v2, The
 * Coveted Mirror).
 *
 * Companion to graphics_magician.h (standard hi-res). Same Graphics Magician
 * vector stream, rendered into a 560-wide aux/main double-hi-res framebuffer and
 * converted to RGBA with MAME's composite-DHGR NTSC kernel. See the .cpp.
 */

#ifndef GLK_COMPREHEND_GRAPHICS_MAGICIAN_DHGR_H
#define GLK_COMPREHEND_GRAPHICS_MAGICIAN_DHGR_H

#include <cstdint>
#include <cstddef>

namespace Glk {
namespace Comprehend {

// Install the double-hi-res drawing tables (colour subindex, sub-table, pattern
// rows, brush bitmaps) from the boot disk's "T5" file -- the headerless <D>
// interpreter, which loads at $4000; the tables sit at fixed offsets inside it.
// Returns false if the buffer is too short. NOTHING is baked: per-image colour
// overrides arrive at draw time as op7 commands.
bool gmDhgrInstallDrawingTables(const uint8_t *t5, size_t size);
bool gmDhgrHaveDrawingTables();

// Clear the persistent double-hi-res pages (white = room background, black =
// transparent base for overlays). Call before a fresh room; item pictures
// compose onto the existing pages and must NOT reset them.
void gmDhgrResetScreen(bool white);

// Run one image's Graphics Magician vector stream onto the persistent pages.
void gmDhgrDrawImage(const uint8_t *data, size_t size);

// Convert the pages to RGBA (0xRRGGBBAA) into out[w*h], rendering rows 0..h-1.
// The native 560 DHGR columns are sampled to the surface width w (1:1 at w>=560).
void gmDhgrBlitToSurface(uint32_t *out, int w, int h);

// Diagnostic access to the raw main/aux pages (used by the regression test).
const uint8_t *gmDhgrMainPtr();
const uint8_t *gmDhgrAuxPtr();

} // namespace Comprehend
} // namespace Glk

#endif
