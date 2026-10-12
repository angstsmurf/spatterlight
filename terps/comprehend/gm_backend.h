/* gm_backend.h -- one table of entry points per Graphics Magician renderer.
 *
 * The four renderers (Apple II hi-res, Apple II double hi-res, DOS CGA, DOS
 * PCjr) expose the same verbs under different prefixes. The host code
 * (pics.cpp, comprehend_gfx.cpp) picks a GmBackend once and calls through it
 * instead of repeating a four-way ladder at every call site.
 */

#ifndef GLK_COMPREHEND_GM_BACKEND_H
#define GLK_COMPREHEND_GM_BACKEND_H

#include <cstdint>
#include <cstddef>

namespace Glk {
namespace Comprehend {

// Which renderer draws the pictures.
enum GfxBackend {
	GFX_APPLE_HIRES,
	GFX_APPLE_DHGR,
	GFX_DOS_CGA,
	GFX_DOS_PCJR
};

// The Coveted Mirror's persistent logo/hourglass panel. Apple II only.
struct GmCMPanelOps {
	bool (*panelValid)();
	void (*beginPanelRebuild)();
	void (*endPanelRebuild)();
	void (*overlayPanel)();
	void (*drawHourglass)(int sand);
	void (*fallBegin)();
	bool (*fallStep)(int *y0, int *y1);
	void (*fallAbort)();
};

struct GmBackend {
	GfxBackend id;
	void (*resetScreen)(bool white);
	void (*drawImage)(const uint8_t *data, size_t size);
	void (*blitToSurface)(uint32_t *out, int w, int h);
	void (*setSlowDraw)(bool on);
	bool (*slowDrawActive)();
	bool (*advanceSlowDraw)(int budget);
	void (*finishSlowDraw)();
	bool (*slowConsumeDirty)(int *y0, int *y1);
	void (*blitSlowToSurface)(uint32_t *out, int w, int h);
	const GmCMPanelOps *cm; // nullptr on the DOS renderers

	// Blit the page the player should currently see: the partially revealed
	// one while a slow-draw reveal is queued, the finished one otherwise.
	void blitVisible(uint32_t *out, int w, int h) const {
		if (slowDrawActive())
			blitSlowToSurface(out, w, h);
		else
			blitToSurface(out, w, h);
	}
};

// The table for a renderer.
const GmBackend *gmBackend(GfxBackend id);

// Turn draw-order recording on or off in every renderer. Only the one that
// actually draws records anything.
void gmSetSlowDrawAll(bool on);

// The renderer with a reveal queued, or the Apple II hi-res one when none has.
const GmBackend *gmSlowDrawBackend();

} // namespace Comprehend
} // namespace Glk

#endif
