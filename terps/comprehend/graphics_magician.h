// Standard hi-res renderer for the Apple II Comprehend games: Graphics Magician vector pictures
// drawn into an Apple II hi-res framebuffer and converted to RGBA. See graphics_magician.cpp.

#ifndef GLK_COMPREHEND_GRAPHICS_MAGICIAN_H
#define GLK_COMPREHEND_GRAPHICS_MAGICIAN_H

#include <cstdint>
#include <cstddef>
#include "cm_panel.h"

namespace Glk {
namespace Comprehend {

// Graphics Magician / Picture Painter vector opcode (high nibble of each
// command byte). Shared by all the Graphics Magician renderers, which decode
// the stream through the common dispatcher in gm_stream.h.
enum Opcode {
	OPCODE_END = 0,
	OPCODE_SET_TEXT_POS = 1,
	OPCODE_SET_PEN_COLOR = 2,
	OPCODE_TEXT_CHAR = 3,
	OPCODE_SET_SHAPE = 4,
	OPCODE_TEXT_OUTLINE = 5,
	OPCODE_SET_FILL_COLOR = 6,
	OPCODE_OP7 = 7,          // 2-operand no-op in Talisman-era pictures, end of
	                         // image in the older dialect, SET_PALETTE_SUBINDEX
	                         // in the double hi-res interpreter
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
// pictures compose onto the existing page and must not reset it.
void gmResetScreen(bool white);

// Run one image's vector-opcode stream onto the persistent hi-res page.
void gmDrawImage(const uint8_t *data, size_t size);

// Convert the persistent hi-res page to RGBA (0xRRGGBBAA) into out[w*h],
// rendering Apple rows 0..h-1 sampled down to w columns.
void gmBlitToSurface(uint32_t *out, int w, int h);

// Slow draw. When enabled, gmDrawImage records the order its bytes are plotted so the picture
// can be revealed progressively. The host turns recording on/off per draw, then, while
// gmSlowDrawActive() is true, advances the reveal on a timer and blits the partially revealed
// page with gmBlitSlowToSurface().

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

// Diagnostic access to the raw 0x2000-byte Apple hi-res page.
const uint8_t *gmPagePtr();
void gmSetPage(const uint8_t *page);
// Diagnostic access to the progressively-revealed (slow-draw) page.
const uint8_t *gmSlowPagePtr();

// The Coveted Mirror persistent right-hand panel (logo + hourglass). Capture
// saves columns [col0,col1] of the current page; overlay re-applies them on top
// of the current page. See gmCaptureCMPanel/gmOverlayCMPanel in the .cpp.
void gmCaptureCMPanel(int col0, int col1);
void gmOverlayCMPanel();
bool gmCMPanelValid();

// Rebuild the panel snapshot off to the side, so that a newly found mirror piece stays in the
// panel. The caller renders the logo, frame and collected pieces between Begin and End. Both
// hi-res pages and the recording flag are saved and restored, so the live picture is undisturbed
// and the panel draws stay out of any reveal. End captures columns 24..39 (no grains).
void gmBeginCMPanelRebuild();
void gmEndCMPanelRebuild();

// Stamp The Coveted Mirror's sand pile. `sand` is the live VM hourglass variable (0x11): that
// many grains, draining one per turn, each a white single-dot brush (brush 0). Call after
// gmOverlayCMPanel() so the pile sits on top of the static panel.
void gmDrawCMHourglass(int sand);

// Per-turn grain fall (The Coveted Mirror). gmDrawCMHourglass() always renders the resting pile;
// on a single-grain drop it also flags that one drained, which gmCMHourglassConsumeFallArmed()
// reports once the turn's graphics have settled. The host then animates the freed grain with
// FallBegin and repeated FallStep (returns the dirty row band, and false when the last frame has
// cleared the grain). FallAbort snaps to the resting state. Grain positions and arming are in
// cm_panel.h, shared with the double hi-res renderer.
void gmCMHourglassFallBegin();
bool gmCMHourglassFallActive();
bool gmCMHourglassFallStep(int *y0, int *y1);
void gmCMHourglassFallAbort();

// Install the Graphics Magician drawing tables (pattern data, fill-colour subindices, brush
// bitmaps) from the boot disk's "T2" file, locating them by signature. Returns false if they
// are not found or the buffer is too short to contain all three.
bool gmInstallDrawingTables(const uint8_t *t2, size_t size);

// Select the older Graphics Magician dialect (Transylvania, Crimson Crown): op7 and op15 end the
// image and fills paint against the full screen. The default (false) is the later dialect, with
// op7 a no-op and op15 the fill-bounds sub-ops.
void gmSetLegacyFormat(bool on);

} // namespace Comprehend
} // namespace Glk

#endif
