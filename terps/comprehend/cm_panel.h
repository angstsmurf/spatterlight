/* cm_panel.h -- The Coveted Mirror's hourglass, the parts that do not depend on
 * which Apple II renderer draws it.
 *
 * The standard hi-res renderer (graphics_magician.cpp) and the double hi-res
 * one (graphics_magician_dhgr.cpp) each keep the panel on their own pages and
 * stamp grains with their own brush. What they share is here: where each grain
 * goes, the "did one grain just drain" bookkeeping, and the frame sequence of
 * the freed grain falling down the neck.
 *
 * Header-only, in the style of graphics_magician_vector.h and slow_draw_page.h.
 */

#ifndef GLK_COMPREHEND_CM_PANEL_H
#define GLK_COMPREHEND_CM_PANEL_H

#include <cstdint>
#include <cstring>
#include "gm_hires.h"

namespace Glk {
namespace Comprehend {

// Compute the Apple hi-res position (x 0..279, y 0..191) of hourglass grain
// `idx`, ported byte-faithfully from the original's cm_draw_hourglass_grain
// ($4347). Returns false for grains that stamp nothing (idx 0). The double
// hi-res renderer doubles x onto its 560-wide pages.
bool gmCMHourglassGrainPos(uint8_t idx, uint16_t *outX, uint8_t *outY);

// Per-turn grain-fall arming. Each hourglass draw calls gmCMHourglassArm() with
// the sand level it is about to show; a drop of exactly one grain since the last
// draw sets a flag that gmCMHourglassConsumeFallArmed() reports once, so the host
// can animate the freed grain. Any other change (first draw, bribe refill, catch,
// restore) snaps. gmCMHourglassReset() forgets the displayed level so the next
// draw snaps.
void gmCMHourglassArm(int sand);
void gmCMHourglassReset();
bool gmCMHourglassConsumeFallArmed();

// Call stamp(x, y) for each grain of a resting pile of `sand` grains (the
// original draws grains 0..sand-1; grain 0 is a no-op).
template <typename Stamp>
inline void cmForEachPileGrain(int sand, Stamp stamp) {
	for (int idx = 0; idx < sand && idx < 256; idx++) {
		uint16_t x;
		uint8_t y;
		if (gmCMHourglassGrainPos((uint8_t)idx, &x, &y))
			stamp(x, y);
	}
}

// The freed grain falling down the hourglass neck.
//
// The brush stamps a single dot with its top-left at (x,y) but the lit pixel
// lands ~7 px down and right, so a grain queued at (kNeckX, plotY) shows at
// screen (kNeckX+7, plotY+7). The resting pile's lowest visible grain is screen
// row ~99; the neck waist is rows ~100-101; the central sand stream (visible
// black channel with sparse orange/blue dots) runs ~102-123; the static bottom
// mound begins ~row 124. We drop the freed grain down that stream over a few
// frames -- an embellishment (the original simply deletes the top grain). The
// erase band must stay BELOW the pile (top >= 100) so re-overlaying the clean
// panel each frame never wipes the resting pile above the waist.
class CMGrainFall {
public:
	enum {
		kNeckTop = 95,    // first plot y -> screen row ~102 (below waist)
		kNeckStep = 4,    // plot-y advance per frame (~4 screen rows)
		kFrames = 6,      // grain positions; one extra clear frame follows
		kNeckX = 245,     // queued x -> screen x 252, the stream centre
		kBandTop = 100,   // erase band: waist down to the mound top,
		kBandBot = 125    // clear of the pile (which ends at row ~99)
	};

	void begin() { _frame = 0; }
	bool active() const { return _frame >= 0; }
	void reset() { _frame = -1; }

	// Render the current frame and advance. restoreBand(y0, y1) repaints the
	// clean panel (no grains) over those rows; stampGrain(x, y) draws the
	// falling grain. Sets *y0/*y1 to the dirty row band. Returns true while
	// more frames remain; the final frame paints no grain (just restores the
	// clean neck) so nothing lingers.
	template <typename RestoreBand, typename StampGrain>
	bool step(int *y0, int *y1, RestoreBand restoreBand, StampGrain stampGrain) {
		if (_frame < 0) {
			*y0 = *y1 = 0;
			return false;
		}
		restoreBand((int)kBandTop, (int)kBandBot);
		if (_frame < kFrames) {
			int gy = kNeckTop + _frame * kNeckStep;
			if (gy > kBandBot)
				gy = kBandBot;
			stampGrain((uint16_t)kNeckX, (uint8_t)gy);
		}
		*y0 = kBandTop;
		*y1 = kBandBot;

		_frame++;
		bool more = _frame <= kFrames;
		if (!more)
			_frame = -1;
		return more;
	}

	// Snap to the clean resting state (no falling grain).
	template <typename RestoreBand>
	void abort(RestoreBand restoreBand) {
		if (_frame < 0)
			return;
		restoreBand((int)kBandTop, (int)kBandBot);
		_frame = -1;
	}

private:
	int _frame = -1;   // active frame (0..kFrames); -1 idle
};

// The persistent right-hand panel (logo + hourglass frame), kept for one Apple II
// renderer.  `Planes` is the number of byte planes that renderer's page has: 1 for
// standard hi-res, 2 for double hi-res (aux and main).  The renderer owns its live
// pages and its progressively-revealed ("slow") pages and hands them in as arrays
// of plane pointers, so everything that is the same for one plane and for two --
// capture by byte column, overlay onto the final and slow pages, the off-screen
// rebuild and its save/restore, and the "skip the panel columns during a reveal"
// test -- is written once here.
template <int Planes>
class CMPanelStore {
public:
	typedef uint8_t *const (&Pages)[Planes];

	bool valid() const { return _valid; }

	// Save the live pages' contents as the panel; its columns are [col0,col1].
	void capture(Pages live, int col0, int col1) {
		for (int i = 0; i < Planes; i++)
			memcpy(_panel[i], live[i], GM_HIRES_PAGE_SIZE);
		_col0 = col0;
		_col1 = col1;
		_valid = true;
	}

	// Rebuild bracket: stash the live and slow pages and the caller's recording
	// flag so the rebuild is invisible to the picture being drawn.  The caller then
	// turns recording off and clears the live pages.  endRebuild() captures columns
	// 24..39 as the new panel, puts everything back and returns the saved flag.
	void beginRebuild(Pages live, Pages slow, bool recording) {
		for (int i = 0; i < Planes; i++) {
			memcpy(_saveLive[i], live[i], GM_HIRES_PAGE_SIZE);
			memcpy(_saveSlow[i], slow[i], GM_HIRES_PAGE_SIZE);
		}
		_saveRecording = recording;
	}

	bool endRebuild(Pages live, Pages slow) {
		capture(live, 24, 39);
		for (int i = 0; i < Planes; i++) {
			memcpy(live[i], _saveLive[i], GM_HIRES_PAGE_SIZE);
			memcpy(slow[i], _saveSlow[i], GM_HIRES_PAGE_SIZE);
		}
		return _saveRecording;
	}

	// Copy the saved panel columns onto rows [y0,y1] of both the final and the slow
	// pages.
	void toPages(Pages live, Pages slow, int y0, int y1) const {
		for (int row = y0; row <= y1; row++) {
			const int base = gm_row_address(row);
			for (int col = _col0; col <= _col1; col++) {
				const int a = base + col;
				if (a >= GM_HIRES_PAGE_SIZE) continue;
				for (int i = 0; i < Planes; i++)
					live[i][a] = slow[i][a] = _panel[i][a];
			}
		}
	}

	// Mirror the final pages' panel columns (e.g. with freshly stamped grains) onto
	// the slow pages only.
	void finalToSlow(Pages live, Pages slow, int y0, int y1) const {
		for (int row = y0; row <= y1; row++) {
			const int base = gm_row_address(row);
			for (int col = _col0; col <= _col1; col++) {
				const int a = base + col;
				if (a >= GM_HIRES_PAGE_SIZE) continue;
				for (int i = 0; i < Planes; i++)
					slow[i][a] = live[i][a];
			}
		}
	}

	// True when a recorded write to page offset `offset` falls in the panel and so
	// must not be replayed onto the slow pages during a reveal (a full-width RLE
	// room records writes across those columns too; replaying them would wipe the
	// overlaid panel).
	bool skipsReveal(unsigned offset) const {
		if (!_valid)
			return false;
		const int c = gm_col_of_offset(offset);
		return c >= _col0 && c <= _col1;
	}

private:
	uint8_t _panel[Planes][GM_HIRES_PAGE_SIZE];
	uint8_t _saveLive[Planes][GM_HIRES_PAGE_SIZE];
	uint8_t _saveSlow[Planes][GM_HIRES_PAGE_SIZE];
	bool _valid = false;
	int _col0 = 24, _col1 = 39;
	bool _saveRecording = false;
};

} // namespace Comprehend
} // namespace Glk

#endif
