/* gm_backend.cpp -- the per-renderer entry point tables. See gm_backend.h. */

#include "gm_backend.h"
#include "graphics_magician.h"
#include "graphics_magician_cga.h"
#include "graphics_magician_dhgr.h"
#include "graphics_magician_pcjr.h"

namespace Glk {
namespace Comprehend {

static const GmCMPanelOps kHiresCM = {
	gmCMPanelValid, gmBeginCMPanelRebuild, gmEndCMPanelRebuild,
	gmOverlayCMPanel, gmDrawCMHourglass,
	gmCMHourglassFallBegin, gmCMHourglassFallStep, gmCMHourglassFallAbort
};

static const GmCMPanelOps kDhgrCM = {
	gmDhgrCMPanelValid, gmDhgrBeginCMPanelRebuild, gmDhgrEndCMPanelRebuild,
	gmDhgrOverlayCMPanel, gmDhgrDrawCMHourglass,
	gmDhgrCMHourglassFallBegin, gmDhgrCMHourglassFallStep, gmDhgrCMHourglassFallAbort
};

static const GmBackend kHires = {
	GFX_APPLE_HIRES,
	gmResetScreen, gmDrawImage, gmBlitToSurface,
	gmSetSlowDraw, gmSlowDrawActive, gmAdvanceSlowDraw, gmFinishSlowDraw,
	gmSlowConsumeDirty, gmBlitSlowToSurface, &kHiresCM
};

static const GmBackend kDhgr = {
	GFX_APPLE_DHGR,
	gmDhgrResetScreen, gmDhgrDrawImage, gmDhgrBlitToSurface,
	gmDhgrSetSlowDraw, gmDhgrSlowDrawActive, gmDhgrAdvanceSlowDraw, gmDhgrFinishSlowDraw,
	gmDhgrSlowConsumeDirty, gmDhgrBlitSlowToSurface, &kDhgrCM
};

static const GmBackend kCga = {
	GFX_DOS_CGA,
	gmcgaResetScreen, gmcgaDrawImage, gmcgaBlitToSurface,
	gmcgaSetSlowDraw, gmcgaSlowDrawActive, gmcgaAdvanceSlowDraw, gmcgaFinishSlowDraw,
	gmcgaSlowConsumeDirty, gmcgaBlitSlowToSurface, nullptr
};

static const GmBackend kPcjr = {
	GFX_DOS_PCJR,
	gmpcjrResetScreen, gmpcjrDrawImage, gmpcjrBlitToSurface,
	gmpcjrSetSlowDraw, gmpcjrSlowDrawActive, gmpcjrAdvanceSlowDraw, gmpcjrFinishSlowDraw,
	gmpcjrSlowConsumeDirty, gmpcjrBlitSlowToSurface, nullptr
};

const GmBackend *gmBackend(GfxBackend id) {
	switch (id) {
	case GFX_APPLE_HIRES: return &kHires;
	case GFX_APPLE_DHGR:  return &kDhgr;
	case GFX_DOS_CGA:     return &kCga;
	case GFX_DOS_PCJR:    return &kPcjr;
	}
	return &kHires;
}

void gmSetSlowDrawAll(bool on) {
	kHires.setSlowDraw(on);
	kCga.setSlowDraw(on);
	kDhgr.setSlowDraw(on);
	kPcjr.setSlowDraw(on);
}

const GmBackend *gmSlowDrawBackend() {
	if (kDhgr.slowDrawActive()) return &kDhgr;
	if (kCga.slowDrawActive())  return &kCga;
	if (kPcjr.slowDrawActive()) return &kPcjr;
	return &kHires;
}

} // namespace Comprehend
} // namespace Glk
