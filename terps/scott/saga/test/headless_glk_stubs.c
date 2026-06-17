// Shared no-op Glk stubs for the standalone byte-exact renderer harnesses
// (filltest / zxvectortest / apple2test). These tests link only a slice of the
// drawing code and never bring in a real Glk library, so the handful of Glk
// entry points the renderers reference are satisfied here with no-ops.
//
// Each test still owns its renderer-specific sinks (PutPixel / RectFill /
// SetColor / Fatal …) and its loader globals locally — only the Glk-library
// surface that was being copy-pasted between harnesses lives here.
//
// Tests that link a real Glk (cheapglk: extract_images, zxextract, titest, …)
// must NOT link this file — cheapglk provides glk_request_timer_events and
// gli_slowdraw, and the glkimp extras live in extract_stubs.c instead.

#include <stdint.h>
#include "glk.h"

// glkimp global the vector renderers consult under SPATTERLIGHT. 0 = emit every
// plotted pixel in a single pass (no progressive/slow draw), which is what the
// byte-exact goldens were captured with.
int gli_slowdraw = 0;

// The timer the live frontend uses to pace slow-draw; irrelevant headless.
void glk_request_timer_events(glui32 millisecs) { (void)millisecs; }

// Graphics-window fill used by the Apple II bitmap path's artifact-colour blit;
// the bitmap tests compare `screenmem` directly, so the on-screen fill is moot.
void glk_window_fill_rect(winid_t win, glui32 color,
                          glsi32 left, glsi32 top, glui32 width, glui32 height) {
    (void)win; (void)color; (void)left; (void)top; (void)width; (void)height;
}
