/* graphics_magician.h -- standard hi-res renderer for the Apple II Comprehend
 * games (Transylvania, OO-Topos, Crimson Crown, Talisman).
 *
 * Renders their Penguin "Graphics Magician" vector pictures into a real Apple II
 * hi-res framebuffer and converts that to RGBA via the NTSC artifact-colour
 * algorithm. See graphics_magician.cpp for the full description.
 */

#ifndef GLK_COMPREHEND_GRAPHICS_MAGICIAN_H
#define GLK_COMPREHEND_GRAPHICS_MAGICIAN_H

#include <cstdint>
#include <cstddef>

namespace Glk {
namespace Comprehend {

// Graphics Magician / Picture Painter vector opcode (high nibble of each
// command byte). Shared by both picture renderers (pics.cpp and the Apple II
// renderer in graphics_magician.cpp).
enum Opcode {
	OPCODE_END = 0,
	OPCODE_SET_TEXT_POS = 1,
	OPCODE_SET_PEN_COLOR = 2,
	OPCODE_TEXT_CHAR = 3,
	OPCODE_SET_SHAPE = 4,
	OPCODE_TEXT_OUTLINE = 5,
	OPCODE_SET_FILL_COLOR = 6,
	OPCODE_END2 = 7,
	OPCODE_MOVE_TO = 8,
	OPCODE_DRAW_BOX = 9,
	OPCODE_DRAW_LINE = 10,
	OPCODE_DRAW_CIRCLE = 11,
	OPCODE_DRAW_SHAPE = 12,
	OPCODE_DELAY = 13,
	OPCODE_PAINT = 14,
	OPCODE_RESET = 15
};

// Clear the persistent Apple hi-res page (white = room background, black =
// transparent base for overlays). Call once before a fresh room picture; item
// pictures compose onto the existing page and must NOT reset it.
void gmResetScreen(bool white);

// Run one image's vector-opcode stream onto the persistent hi-res page.
void gmDrawImage(const uint8_t *data, size_t size);

// Convert the persistent hi-res page to RGBA (0xRRGGBBAA) into out[w*h],
// rendering Apple rows 0..h-1 sampled down to w columns.
void gmBlitToSurface(uint32_t *out, int w, int h);

// ---- Slow ("animated") draw -------------------------------------------------
// When enabled, gmDrawImage records the order its bytes are plotted so the
// picture can be revealed progressively (like the Apple II Scott Adams / Level 9
// renderers). The host turns recording on/off per draw, then, while
// gmSlowDrawActive() is true, repeatedly advances the reveal on a timer
// and blits the partially-revealed page with gmBlitSlowToSurface().

// Enable/disable op recording for the next image(s).
void gmSetSlowDraw(bool on);
// True while recorded bytes are still waiting to be revealed.
bool gmSlowDrawActive();
// Reveal up to `budget` more bytes (rounded up across adjacent runs); returns
// true while more remain.
bool gmAdvanceSlowDraw(int budget);
// Reveal everything left at once (resize / cancel).
void gmFinishSlowDraw();
// Report the inclusive row band [*y0,*y1] changed since the last call (so the
// host repaints just that band), and reset it. Returns false if unchanged.
bool gmSlowConsumeDirty(int *y0, int *y1);
// Blit the partially-revealed page, same format as gmBlitToSurface().
void gmBlitSlowToSurface(uint32_t *out, int w, int h);

// Diagnostic access to the raw 0x2000-byte Apple hi-res page (used by the
// regression test and the offline pixel-diff tool).
const uint8_t *gmPagePtr();
void gmSetPage(const uint8_t *page);

// The Coveted Mirror persistent right-hand panel (logo + hourglass). Capture
// saves columns [col0,col1] of the current page; overlay re-applies them on top
// of the current page. See gmCaptureCMPanel/gmOverlayCMPanel in the .cpp.
void gmCaptureCMPanel(int col0, int col1);
void gmOverlayCMPanel();
bool gmCMPanelValid();

// Rebuild the panel snapshot off to the side. The caller renders the logo, the
// hourglass frame and any collected mirror pieces between Begin and End; we save
// and restore both hi-res pages and the slow-draw recording flag so the rebuild
// neither disturbs the live picture being drawn nor records the panel draws into
// an in-flight reveal (the panel is always painted instantly). End captures
// columns 24..39 (logo + frame + pieces, with no hourglass grains) as the new
// persistent snapshot. Used by The Coveted Mirror to fold a newly found mirror
// piece into the panel so gmOverlayCMPanel() keeps showing it every turn.
void gmBeginCMPanelRebuild();
void gmEndCMPanelRebuild();

// Stamp the hourglass sand pile for The Coveted Mirror. `sand` is the live VM
// hourglass variable (0x11): the engine shows that many grains, draining one per
// turn. Each grain is a single-dot brush (brush 0) painted white at the position
// the original computes per grain index; call after gmOverlayCMPanel() so the
// pile sits on top of the static panel. Ported byte-faithfully from the Apple II
// interpreter's cm_draw_hourglass_grain ($4347) / cm_per_turn_graphics_step.
void gmDrawCMHourglass(int sand);

// Position of hourglass grain `idx` (0-based; idx 0 is a no-op). Fills the Apple
// hi-res coordinate (x 0..279, y 0..191) and returns false for grains that stamp
// nothing. Shared with the double-hi-res renderer, which doubles x onto its
// 560-wide pages. Ported from cm_draw_hourglass_grain ($4347).
bool gmCMHourglassGrainPos(uint8_t idx, uint16_t *outX, uint8_t *outY);

// Per-turn grain-fall animation (The Coveted Mirror). gmDrawCMHourglass() always
// renders the resting pile at the correct level and, on a single-grain drop,
// flags that a grain just drained; gmCMHourglassConsumeFallArmed() reports that
// once the turn's graphics have settled. The host then runs the embellishment --
// a single freed grain falling down the neck over a few timer frames -- via
// gmCMHourglassFallBegin() + repeated gmCMHourglassFallStep() (which returns the
// dirty row band and false when the last frame has cleared the grain).
// gmCMHourglassFallAbort() snaps to the clean resting state; gmCMHourglassReset()
// forgets the displayed level so the next draw snaps (after restore/undo/restart).
// Arm the per-turn fall from the current sand level (shared by the standard and
// DHGR hourglass draw paths). Sets the flag gmCMHourglassConsumeFallArmed() reads.
void gmCMHourglassArm(int sand);
void gmCMHourglassReset();
bool gmCMHourglassConsumeFallArmed();
void gmCMHourglassFallBegin();
bool gmCMHourglassFallActive();
bool gmCMHourglassFallStep(int *y0, int *y1);
void gmCMHourglassFallAbort();

// Install the Graphics Magician drawing tables (pattern data, fill-colour
// subindices, brush bitmaps) from the boot disk's "T2" file. T2 is a headerless
// ProDOS BIN that loads at $0800; the tables sit at fixed addresses inside it.
// Returns false if the buffer is too short to contain all three tables.
bool gmInstallDrawingTables(const uint8_t *t2, size_t size);

// Select the older Graphics Magician dialect used by the earlier Comprehend
// titles (Transylvania, OO-Topos, Crimson Crown): op7 ends the image, op15 is
// unused/ends, and fills paint against the full screen. Talisman (the default,
// false) keeps its op7 no-op / op15 fill-bounds behaviour.
void gmSetLegacyFormat(bool on);

} // namespace Comprehend
} // namespace Glk

#endif
