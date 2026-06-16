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

// ---- Slow ("animated") draw -------------------------------------------------
// As in the standard-hi-res renderer (graphics_magician.h): when enabled,
// gmDhgrDrawImage records the order its bytes are plotted (and op13 DELAY
// markers) so the picture can be revealed progressively. The host turns
// recording on/off per draw, then, while gmDhgrSlowDrawActive() is true,
// repeatedly advances the reveal on a timer and blits the partially-revealed
// pages with gmDhgrBlitSlowToSurface().

// Enable/disable op recording for the next image(s).
void gmDhgrSetSlowDraw(bool on);
// True while recorded bytes (or a DELAY pause) are still waiting to be revealed.
bool gmDhgrSlowDrawActive();
// Reveal up to `budget` more bytes (rounded up across adjacent runs); returns
// true while more remain.
bool gmDhgrAdvanceSlowDraw(int budget);
// Reveal everything left at once (resize / cancel).
void gmDhgrFinishSlowDraw();
// Report the inclusive row band [*y0,*y1] changed since the last call (so the
// host repaints just that band), and reset it. Returns false if unchanged.
bool gmDhgrSlowConsumeDirty(int *y0, int *y1);
// Blit the partially-revealed pages, same format as gmDhgrBlitToSurface().
void gmDhgrBlitSlowToSurface(uint32_t *out, int w, int h);

// ---- The Coveted Mirror persistent right-hand panel (double hi-res) ----------
// Double-hi-res counterparts of gmCaptureCMPanel / gmOverlayCMPanel /
// gmDrawCMHourglass (graphics_magician.h), operating on the aux+main pages
// instead of the single standard-hi-res page. The panel (logo + hourglass) is
// built once into the pages, captured by byte-column [col0,col1] (same column
// indices as standard: the renderer doubles each source column into the matching
// aux+main byte), and re-composited on top of every in-game picture; the
// hourglass grains for the current sand level are stamped on top.
void gmDhgrCaptureCMPanel(int col0, int col1);
void gmDhgrOverlayCMPanel();
bool gmDhgrCMPanelValid();
void gmDhgrDrawCMHourglass(int sand);

// Double-hi-res counterpart of the standard per-turn grain fall
// (gmCMHourglassFallBegin/Step/Abort in graphics_magician.h): animates the freed
// grain down the neck on the aux+main pages. Arming is shared (gmCMHourglassArm /
// gmCMHourglassConsumeFallArmed), so the host drives whichever renderer is active.
void gmDhgrCMHourglassFallBegin();
bool gmDhgrCMHourglassFallActive();
bool gmDhgrCMHourglassFallStep(int *y0, int *y1);
void gmDhgrCMHourglassFallAbort();

// Diagnostic access to the raw main/aux pages (used by the regression test).
const uint8_t *gmDhgrMainPtr();
const uint8_t *gmDhgrAuxPtr();

} // namespace Comprehend
} // namespace Glk

#endif
