/* Comprehend engine glue: rendering pictures and blitting them to the
 * graphics window, slow-draw and the Coveted Mirror hourglass ticks, and
 * graphics-mode switching. */

#include "comprehend.h"
#include "graphics_magician.h"
#include "graphics_magician_cga.h"
#include "graphics_magician_dhgr.h"
#include "graphics_magician_pcjr.h"
#include "pics.h"
#include "comprehend_internal.h"

namespace Glk {
namespace Comprehend {

// Apple II Talisman slow-draw pacing. The byte count per tick pairs with the
// tick interval — change them together. Matches the Apple II Scott Adams
// renderer's feel (10 ms ticks, tens of bytes a tick).
#define GM_SLOW_TICK_MS       10
#define GM_SLOW_BYTES_PER_TICK 50

// The Coveted Mirror grain fall is paced down from the slow-draw tick: one grain
// step every HG_TICKS_PER_FRAME ticks (~70 ms), so the freed grain takes about
// half a second to fall through the neck.
#define HG_TICKS_PER_FRAME    7

// Pull a pixel out of _drawSurface in the 0xRRGGBB form Glk expects.
static uint32 surfaceColorAt(DrawSurface *ds, int x, int y) {
	const uint32 *p = (const uint32 *)ds->getBasePtr(x, y);
	return *p;
}

bool Comprehend::applyPreferredGraphicsMode() {
	// Re-applied on every arrange (see onArrange), so guard against doing work
	// when the preference hasn't actually changed: a window resize lands here
	// too. -1 is the initial "never applied" sentinel, so the first call runs.
	// Outside Spatterlight the preference is set once, by the -c option, so
	// only that first call does anything and the #dhgr / #pcjr commands (see
	// game.cpp) have the last word.
	if (gli_comprehend_graphics == _appliedComprehendGraphics)
		return false;
	_appliedComprehendGraphics = gli_comprehend_graphics;

	// More colours (0): PCjr on DOS v1, double hi-res on Apple II. Fewer colours
	// (1): the 4-colour CGA / standard hi-res renderers, which are the defaults.
	// Switch in whichever direction is needed; games without the higher-colour
	// tables have nothing to toggle and stay on the (fewer-colour) default.
	bool more = (gli_comprehend_graphics == 0);
	if (!moreColoursAvailable() || more == _moreColours)
		return false;
	setMoreColours(more);
	return true;
}

void Comprehend::repaintCurrentScene() {
	if (!_topWindow)
		return;
	// A room scene (location + items): the picture numbers in _screenComposition
	// encode their own clear/overlay semantics, so replaying them via
	// paintBackground re-renders the scene through the now-active renderer.
	// Passing the same list makes paintBackground suppress the slow-draw reveal,
	// so the mode switch repaints in one step.
	if (!_screenComposition.empty()) {
		Common::Array<uint> pics = _screenComposition;
		paintBackground(pics);
		return;
	}
	// No room scene -- e.g. the title screen, which beforeGame() draws via
	// drawPicture() directly. Re-render that last single picture through the new
	// renderer (instantly), so a mode switch on the title screen redraws it
	// rather than leaving it cleared (DHGR) or stale (PCjr).
	if (_lastDrawnPicture != (uint)-1) {
		bool wasSuppressed = _suppressSlowDraw;
		_suppressSlowDraw = true;
		drawPicture(_lastDrawnPicture);
		_suppressSlowDraw = wasSuppressed;
		return;
	}
	// Nothing drawn yet: just refresh whatever the surface holds.
	blitSurfaceToWindow();
}

void Comprehend::drawPicture(uint pictureNum) {
	Common::Array<uint> pics;
	pics.push_back(pictureNum);
	drawPictureList(pics);
}

void Comprehend::drawPictureList(const Common::Array<uint> &pics) {
	if (!hasPictureTarget() || !_pics || !_drawSurface || pics.empty()) return;

	// Ensure the pixel scale matches the actual window width before drawing.
	// recomputeGraphicsScale() is normally called from onArrange() on resize
	// events, but the first drawPicture() call (the title screen) runs before
	// any such event fires, leaving _pixelSize at the initial SCALE_FACTOR=2
	// which can make the image wider than the window and clip it on the right.
	recomputeGraphicsScale();

	// If a background reveal from the previous scene is still running
	// (e.g. the player moved before it finished), complete it instantly.
	// Must happen before gmSetSlowDrawAll(), which would clear
	// s_recordOps and break the active-renderer check inside finishSlowDraw().
	finishSlowDraw();

	// Likewise snap any in-flight grain fall before repainting -- the new picture
	// restamps the pile at the current level, so a lingering falling grain from
	// the previous turn would be drawn over stale geometry.
	if (_hourglassFalling) {
		cmPanelOps()->fallAbort();
		_hourglassFalling = false;
		updateTimerRequest();
	}
	// A fall deferred behind the previous reveal is stale once a new picture is
	// drawn (this picture re-arms its own); drop it so it can't fire late.
	_hourglassFallPending = false;

	// All three Talisman renderers (Apple II hi-res, Apple II double hi-res and
	// DOS CGA) can reveal a picture progressively, the way the original painted
	// the page. Record the draw order when the user has slow-draw on. Only the
	// renderer that actually runs records anything.
	//
	// _suppressSlowDraw is set by update_graphics() when it repaints a scene
	// identical to the one already on screen: there is nothing new to reveal, so
	// paint instantly rather than re-animate the same image (looks like a stutter).
	bool slow = gli_slowdraw && !_suppressSlowDraw;
	gmSetSlowDrawAll(slow);

	// Render every picture of the scene through the Pics opcode interpreter into
	// the pixel buffer. The first (room) picture clears the page and records its
	// draw ops; each following item overlay appends its ops to the SAME reveal,
	// so the whole composite is revealed in paint order. Rendering them as
	// separate drawPicture() calls would make each one finishSlowDraw() (snap) the
	// previous picture's reveal, leaving only the last picture animated -- the
	// room would paint instantly and only the item overlay would slow-draw.
	for (uint i = 0; i < pics.size(); i++) {
		// Remember the last picture so repaintCurrentScene() can re-render it
		// after a graphics-mode switch when there's no room scene to replay
		// (e.g. the title screen).
		_lastDrawnPicture = pics[i];
		_pics->renderPicture((int)pics[i]);
	}
	// Blit the initial (blank or reset) state to the Glk window.
	blitSurfaceToWindow();

	// If the render queued a slow-draw reveal, start a Glk timer and let the
	// input loops (readLine / readChar) advance it tick by tick in the
	// background. The game proceeds immediately: room description is shown
	// and the prompt appears while the picture is still being revealed.
	if (gmSlowDrawBackend()->slowDrawActive()) {
		glk_request_timer_events(GM_SLOW_TICK_MS);
		_slowDrawActive = true;
	}
#ifdef COMPREHEND_HEADLESS
	drainAnimations();
#endif
}

#ifdef COMPREHEND_HEADLESS
// No timer events arrive in the headless build, so when the picture log is on
// run any reveal / grain fall to completion here, logging each tick's blit.
void Comprehend::drainAnimations() {
	if (!_picLog)
		return;
	while (_slowDrawActive)
		tickSlowDraw();
	while (_hourglassFalling)
		tickHourglass();
}
#endif

// Advance one timer tick of the background slow-draw reveal.
void Comprehend::tickSlowDraw() {
	const GmBackend *gm = gmSlowDrawBackend();
	int y0, y1;
	bool more = gm->advanceSlowDraw(GM_SLOW_BYTES_PER_TICK);
	gm->blitSlowToSurface((uint32 *)_drawSurface->getPixels(),
						  _drawSurface->w, _drawSurface->h);
	if (gm->slowConsumeDirty(&y0, &y1))
		blitSurfaceRowsToWindow(y0, y1);
	if (!more) {
		_slowDrawActive = false;
		// The reveal that was holding back a move-turn grain fall has finished:
		// start it now so the freed grain drops in over the freshly painted room.
		if (_hourglassFallPending && !_hourglassFalling)
			beginHourglassFall();
		updateTimerRequest();
	}
}

// Request the Glk timer iff some background animation still needs ticking.
// Both the slow-draw reveal and the grain fall share the one timer, so the last
// one to finish turns it off.
void Comprehend::updateTimerRequest() {
	glk_request_timer_events((_slowDrawActive || _hourglassFalling)
							 ? GM_SLOW_TICK_MS : 0);
}

// Start The Coveted Mirror's per-turn grain fall once the turn's graphics have
// settled (called as input is about to be read). gmDrawCMHourglass() flags a
// single-grain drop; we animate it only with the picture window open and
// slow-draw enabled, and only when no room reveal is already running (the room
// paint-in owns the timer then and the hourglass just snaps, matching the
// original's behaviour on a room change). The armed flag is consumed either way.
void Comprehend::maybeStartHourglassFall() {
	if (!gmCMHourglassConsumeFallArmed())
		return;
	if (_hourglassFalling)
		return;
	if (!hasPictureTarget() || !gli_slowdraw)
		return;
	// A move repaints the room with a slow-draw reveal that owns the timer and
	// redraws the panel band each frame; running the fall at the same time would
	// fight it. Defer the fall until the reveal finishes (tickSlowDraw) so the
	// grain still drops on a move, just after the new room has painted in.
	if (_slowDrawActive) {
		_hourglassFallPending = true;
		return;
	}
	beginHourglassFall();
#ifdef COMPREHEND_HEADLESS
	drainAnimations();
#endif
}

void Comprehend::beginHourglassFall() {
	_hourglassFallPending = false;
	cmPanelOps()->fallBegin();
	_hourglassFalling = true;
	_hgTickAccum = 0;
	updateTimerRequest();
}

// Advance the grain fall, paced down from the fast slow-draw tick, and blit just
// the neck band it changed.
void Comprehend::tickHourglass() {
	if (++_hgTickAccum < HG_TICKS_PER_FRAME)
		return;
	_hgTickAccum = 0;

	int y0, y1;
	const GmBackend *gm = cmBackend();
	bool more = gm->cm->fallStep(&y0, &y1);
	gm->blitToSurface((uint32 *)_drawSurface->getPixels(),
					  _drawSurface->w, _drawSurface->h);
	blitSurfaceRowsToWindow(y0, y1);

	if (!more) {
		_hourglassFalling = false;
		updateTimerRequest();
	}
}

// Redraw The Coveted Mirror's hourglass panel for the current sand level without
// repainting the room. update_graphics() only repaints when the room or its
// items change, so on a turn where the player stays put nothing redrew the
// draining sand -- the pile never visibly fell and the freed-grain animation was
// never armed. The original interpreter's per-turn step (cm_per_turn_graphics_step
// $42f8) updates the hourglass every turn regardless; this is the host-side
// equivalent. Re-stamping the pile via gmDrawCMHourglass() arms the fall when the
// level dropped by one (the normal per-turn drain), which the next readLine()
// animates through maybeStartHourglassFall(). The caller skips this on full-
// repaint turns, where drawPicture() already draws the hourglass (snapped).
void Comprehend::refreshCMHourglass() {
	if (!hasPictureTarget() || !_pics || !_drawSurface || !_graphicsEnabled)
		return;
	if (!traits().hasHourglassPanel)
		return;

	int sand = 0;
	if (_game)
		sand = _game->_variables[0x11];

	// Snap any still-in-flight fall from the previous turn before re-stamping the
	// pile, exactly as drawPicture() does, so a lingering grain isn't left drawn
	// over the freshly stamped geometry.
	if (_hourglassFalling) {
		cmPanelOps()->fallAbort();
		_hourglassFalling = false;
		updateTimerRequest();
	}

	const GmBackend *gm = cmBackend();
	if (!gm->cm->panelValid())
		return;
	gm->cm->overlayPanel();
	gm->cm->drawHourglass(sand);
	gm->blitToSurface((uint32 *)_drawSurface->getPixels(),
					  _drawSurface->w, _drawSurface->h);
	// Only the panel columns changed on the surface (the room columns still hold
	// the last-drawn room), so a full-height blit just repaints the room as-is
	// plus the updated hourglass -- simplest and cheap enough for one turn.
	blitSurfaceRowsToWindow(0, G_RENDER_HEIGHT - 1);
}

// Complete the slow-draw reveal instantly and sync _drawSurface to the final
// fully-drawn page. The caller is responsible for blitting to the window.
void Comprehend::finishSlowDraw() {
	if (!_slowDrawActive) return;
	_slowDrawActive = false;
	updateTimerRequest();
	const GmBackend *gm = gmSlowDrawBackend();
	gm->finishSlowDraw();
	gm->blitToSurface((uint32 *)_drawSurface->getPixels(),
					  _drawSurface->w, _drawSurface->h);
}

// Paint a scene that replaces the whole screen. If the requested set of
// pictures matches what is already composited, suppress the slow-draw reveal so
// an unchanged repaint (e.g. the room redrawn each turn, or Talisman's
// OPCODE_DRAW_ROOM after a speech) appears instantly instead of re-animating.
void Comprehend::paintBackground(const Common::Array<uint> &pics) {
	if (pics.empty())
		return;
	_suppressSlowDraw = (pics == _screenComposition);
	_screenComposition = pics;
	drawPictureList(pics);
	_suppressSlowDraw = false;
}

// Layer pictures on top of the current screen without clearing it. Pictures
// already present are repainted, but only the reveal animation is skipped when
// nothing new is added (so an already-visible object isn't re-animated).
void Comprehend::paintOverlay(const Common::Array<uint> &pics) {
	if (pics.empty())
		return;
	bool allPresent = true;
	for (uint i = 0; i < pics.size(); i++) {
		bool present = false;
		for (uint j = 0; j < _screenComposition.size(); j++)
			if (_screenComposition[j] == pics[i]) { present = true; break; }
		if (!present) {
			allPresent = false;
			_screenComposition.push_back(pics[i]);
		}
	}
	_suppressSlowDraw = allPresent;
	drawPictureList(pics);
	_suppressSlowDraw = false;
}

void Comprehend::drawLocationPicture(int pictureNum, bool clearBg) {
	Common::Array<uint> pics;
	pics.push_back((uint)(pictureNum + (clearBg ? LOCATIONS_OFFSET : LOCATIONS_NO_BG_OFFSET)));
	if (clearBg)
		paintBackground(pics);
	else
		paintOverlay(pics);
}

void Comprehend::drawItemPicture(int pictureNum) {
	Common::Array<uint> pics;
	pics.push_back((uint)(pictureNum + ITEMS_OFFSET));
	paintOverlay(pics);
}

void Comprehend::clearScreen(bool isBright) {
	Common::Array<uint> pics;
	pics.push_back((uint)(isBright ? BRIGHT_ROOM : DARK_ROOM));
	paintBackground(pics);
}

void Comprehend::hideGraphics() {
	// The status window stays where showGraphics() reopened it (between the
	// picture and the buffer); only the picture window goes away, leaving a
	// text+status layout.
	if (_topWindow) {
		glk_window_close(_topWindow, nullptr);
		_topWindow = nullptr;
	}
	_graphicsEnabled = false;
	_closedBySetting = false;
}

bool Comprehend::toggleGraphics() {
	if (_topWindow) {
		glk_window_close(_topWindow, nullptr);
		_topWindow = nullptr;
		_graphicsEnabled = false;
		return false;
	} else {
		showGraphics();
		return true;
	}
}

void Comprehend::setGraphicsMode(bool graphicsMode) {
	// Switch logical mode without destroying the picture window, so the layout
	// doesn't reflow on every switch (Talisman flips modes several times during
	// its intro). Turning graphics off just blanks the window; turning it back
	// on lets the next update() redraw into the still-open window.
	if (graphicsMode) {
		// Only enter graphics mode if a picture window actually exists. When it
		// doesn't (headless build, or the player hid it), stay in text mode so
		// behaviour matches the old toggleGraphics()/showGraphics() path.
		if (hasPictureTarget())
			_graphicsEnabled = true;
	} else {
		_graphicsEnabled = false;
		if (_topWindow)
			glk_window_clear(_topWindow);
	}
}

void Comprehend::showGraphics() {
	if (_topWindow) return;
	// Graphics switched off in the host (Spatterlight's setting, Gargoyle's
	// "graphics 0"): leave the layout as it is. onArrange() opens the window
	// if the setting is turned on later.
	if (!glk_gestalt(gestalt_Graphics, 0)) {
		_graphicsEnabled = hasPictureTarget();
		_closedBySetting = true;
		return;
	}
	_closedBySetting = false;
	if (_statusWindow) {
		glk_window_close(_statusWindow, nullptr);
		_statusWindow = nullptr;
	}
	_topWindow = glk_window_open(_bottomWindow,
		winmethod_Above | winmethod_Fixed,
		(glui32)(G_RENDER_HEIGHT * _pixelSize),
		wintype_Graphics, GLK_GRAPHICS_ROCK);
	_graphicsEnabled = hasPictureTarget();
	// Graphics windows default to a white background (glkimp window.c), and
	// matching the theme's text background is no better: it shows in the
	// centring margins and wherever a picture leaves the screen untouched (The
	// Coveted Mirror's rooms get stray light lines around them). The original
	// machines showed these pictures on a black screen, so the window is always
	// black, whatever the theme.
	if (_topWindow) {
		const glui32 background = 0x00000000;
		glk_window_set_background_color(_topWindow, background);
		glk_window_clear(_topWindow);
	}
	// Reopen status above _bottomWindow — inserts it between graphics and buffer.
	// Height is set to fit the room description on the next printRoomDesc().
	_statusWindow = glk_window_open(_bottomWindow,
		winmethod_Above | winmethod_Fixed, 1, wintype_TextGrid, GLK_STATUS_ROCK);
}

GfxBackend Comprehend::gfxBackend() const {
	if (Common::DiskImageFS::active())
		return (_moreColours && gmDhgrHaveDrawingTables()) ? GFX_APPLE_DHGR : GFX_APPLE_HIRES;
	if (_moreColours && gmpcjrHaveDrawingTables())
		return GFX_DOS_PCJR;
	// Pics::Pics always installs CGA tables for a DOS game: the game's own,
	// or the built-in copy.
	return GFX_DOS_CGA;
}

int Comprehend::renderWidth() const {
	return appleRender() ? G_RENDER_WIDTH_APPLE : G_RENDER_WIDTH;
}

// The Apple II renderer holding The Coveted Mirror's panel and hourglass: the
// double hi-res one when that mode is on, the standard hi-res one otherwise.
const GmBackend *Comprehend::cmBackend() const {
	return gmBackend(gfxBackend() == GFX_APPLE_DHGR ? GFX_APPLE_DHGR : GFX_APPLE_HIRES);
}

bool Comprehend::moreColoursAvailable() const {
	return gmpcjrHaveDrawingTables() ||
		   (Common::DiskImageFS::active() && gmDhgrHaveDrawingTables());
}

void Comprehend::setMoreColours(bool on) {
	if (on == _moreColours)
		return;
	_moreColours = on;
	// Both Apple II modes fill the same 560-wide surface, and PCjr shares the
	// CGA renderer's 280-wide one, so no resize is needed; just clear the
	// window so the caller repaints in the new mode.
	if (_topWindow)
		glk_window_clear(_topWindow);
}

void Comprehend::blitSurfaceToWindow() {
	blitSurfaceRowsToWindow(0, G_RENDER_HEIGHT - 1);
}

// Blit Apple rows [y0,y1] (inclusive) of _drawSurface to the window. The slow
// reveal calls this with just the band it changed this tick; a full picture
// blit goes through blitSurfaceToWindow() above.
void Comprehend::blitSurfaceRowsToWindow(int y0, int y1) {
#ifdef COMPREHEND_HEADLESS
	if (_picLog && _drawSurface) {
		if (y0 < 0) y0 = 0;
		if (y1 > G_RENDER_HEIGHT - 1) y1 = G_RENDER_HEIGHT - 1;
		uint32 h = 2166136261u;   // FNV-1a over the rows' pixels
		for (int y = y0; y <= y1; ++y)
			for (int x = 0; x < _drawSurface->w; ++x) {
				uint32 c = surfaceColorAt(_drawSurface, x, y);
				for (int b = 0; b < 4; ++b, c >>= 8)
					h = (h ^ (c & 0xff)) * 16777619u;
			}
		fprintf(_picLog, "blit pic=%u rows=%d-%d w=%d fnv=%08x\n",
				_lastDrawnPicture, y0, y1, (int)_drawSurface->w, h);
		fflush(_picLog);
		// COMPREHEND_PICDUMP=<dir>: also save the whole surface after each
		// blit as <dir>/<blit number>.ppm, for comparing what the game shows
		// (room plus items) against emulator screenshots.
		static const char *dumpDir = getenv("COMPREHEND_PICDUMP");
		static int dumpCount = 0;
		if (dumpDir) {
			char path[1024];
			snprintf(path, sizeof(path), "%s/%05d.ppm", dumpDir, dumpCount++);
			if (FILE *f = fopen(path, "wb")) {
				fprintf(f, "P6\n%d %d\n255\n", (int)_drawSurface->w, G_RENDER_HEIGHT);
				for (int y = 0; y < G_RENDER_HEIGHT; ++y)
					for (int x = 0; x < _drawSurface->w; ++x) {
						uint32 c = surfaceColorAt(_drawSurface, x, y);
						fputc((c >> 24) & 0xff, f);
						fputc((c >> 16) & 0xff, f);
						fputc((c >> 8) & 0xff, f);
					}
				fclose(f);
			}
		}
		return;
	}
#endif
	if (!_topWindow || !_drawSurface) return;
	glui32 winW = 0, winH = 0;
	glk_window_get_size(_topWindow, &winW, &winH);
	if (winW == 0 || winH == 0) return;

	if (y0 < 0) y0 = 0;
	if (y1 > G_RENDER_HEIGHT - 1) y1 = G_RENDER_HEIGHT - 1;
	if (y0 > y1) return;

	// Centre the render horizontally (matches ScummVM's 20*SCALE_FACTOR x offset).
	// The physical width is the same on every machine: DOS is
	// G_RENDER_WIDTH*_pixelSize; the Apple II is its 560 columns at half-width
	// (560 * _pixelSize/2 == 280 * _pixelSize), so the 280-unit centring holds.
	int xOff = ((int)winW - G_RENDER_WIDTH * _pixelSize) / 2;
	if (xOff < 0) xOff = 0;
	const int surfaceW = _drawSurface->w;   // 280 (DOS) or 560 (Apple II)
	const int stepX = pixelStepX();          // _pixelSize (DOS) or _pixelSize/2 (Apple II)

	// Run-length encode along each row so we issue one glk_window_fill_rect
	// per same-coloured horizontal run instead of one per pixel — about 30x
	// fewer Glk calls for typical Comprehend rooms.
	//
	// Pixels are 0xRRGGBBAA; an alpha of 0 marks the transparent background
	// left by DrawSurface::clear(0) for item / no-background images. We skip
	// those runs so overlay images compose onto the room already shown in the
	// window instead of wiping it to black. Room images clear to an opaque
	// colour first, so every pixel is drawn.
	for (int y = y0; y <= y1; ++y) {
		int x = 0;
		while (x < surfaceW) {
			uint32 c = surfaceColorAt(_drawSurface, x, y);
			int x0 = x;
			do { ++x; } while (x < surfaceW && surfaceColorAt(_drawSurface, x, y) == c);
			if ((c & 0xff) == 0)
				continue;   // transparent — leave the background untouched
			// Strip alpha and map to Glk's 0xRRGGBB.
			glui32 rgb = (glui32)((c >> 8) & 0xffffff);
			glk_window_fill_rect(_topWindow, rgb,
				xOff + x0 * stepX,
				y * _pixelSize,
				(x - x0) * stepX,
				_pixelSize);
		}
	}

	// The Coveted Mirror's room (a bright wall) butts directly against the black
	// hourglass panel at byte column 24. On an NTSC screen a white run carries on
	// for one dot past its last lit pixel, so where the wall's last column is
	// white, the panel's first dot (dot 336 of 560) is white too. MAME shows the
	// same thin line down the panel's left edge. We paint it out, a deliberate
	// difference from the original: the edge looks like a drawing fault, not
	// part of the panel. Columns 24-25 are always black in the panel itself (the
	// logo starts at column 26), so only that dot changes. Standard hi-res only.
	// gmCMPanelValid() gates this to when the panel is actually present (false
	// on the title screen, whose columns 24-25 are real picture content).
	if (gfxBackend() != GFX_APPLE_DHGR && traits().hasHourglassPanel && gmCMPanelValid()) {
		const int kCMPanelCol = 24;            // pics.cpp gmCaptureCMPanel(24, 39)
		const int dotsPerCol = surfaceW / 40;  // 14 on the 560-wide Apple surface
		int px0 = xOff + (kCMPanelCol * dotsPerCol) * stepX;
		int w = 2 * dotsPerCol * stepX;        // columns 24-25
		glk_window_fill_rect(_topWindow, 0x000000,
			px0, y0 * _pixelSize, w, (y1 - y0 + 1) * _pixelSize);
	}
}

bool Comprehend::recomputeGraphicsScale() {
	if (!_topWindow) return false;
	glui32 winW = 0, winH = 0;
	glk_window_get_size(_topWindow, &winW, &winH);
	if (winW == 0) return false;

	// Largest integer multiple of the 280-wide render that fits the window...
	int scale = (int)(winW / G_RENDER_WIDTH);
	// ...but never let the picture take more than ~60% of the screen height.
	if (gscreenh > 0) {
		int scaleByHeight = (gscreenh * 3 / 5) / G_RENDER_HEIGHT;
		if (scaleByHeight >= 1 && scale > scaleByHeight)
			scale = scaleByHeight;
	}
	if (scale < 1) scale = 1;
	// The Apple II halves the horizontal step (pixelStepX == scale/2), so the
	// vertical scale must be even (and >= 2) for the 560 columns to land on
	// whole window pixels.
	if (appleRender()) {
		if (scale < 2) scale = 2;
		scale &= ~1;
	}
	if (scale == _pixelSize) return false;

	_pixelSize = scale;
	winid_t parent = glk_window_get_parent(_topWindow);
	if (parent)
		glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
								   (glui32)(G_RENDER_HEIGHT * _pixelSize), _topWindow);
	return true;
}

// Repaint after a window resize / redraw request: rescale and re-blit the
// current picture (the persistent surface still holds it — Talisman's full
// hi-res page is opaque, so this restores it exactly), and re-wrap the status
// description to the new width.
void Comprehend::onArrange() {
	// Complete any background reveal so the final image is what gets rescaled.
	finishSlowDraw();
	// Re-read the host's graphics-quality preference. This is what makes the
	// setting take effect (the real theme can arrive in an arrange after
	// loadGame()) and what makes a mid-game change switch the renderer live --
	// mirroring Bocfel's window_change() and Scott's UpdateSettings().
	bool modeChanged = applyPreferredGraphicsMode();
	// Follow the host's graphics on/off setting when it changes mid-game.
	if (!glk_gestalt(gestalt_Graphics, 0)) {
		if (_topWindow) {
			hideGraphics();
			_closedBySetting = true;
		}
	} else if (_closedBySetting) {
		showGraphics();
		if (_topWindow && _game && !_lastRoomDesc.empty()) {
			// Nothing was drawn while the window was closed.
			_screenComposition.clear();
			_game->_updateFlags |= UPDATE_GRAPHICS;
			bool wasSuppressed = _suppressSlowDraw;
			_suppressSlowDraw = true;
			_game->update_graphics();
			_suppressSlowDraw = wasSuppressed;
			modeChanged = false;
		}
	}
	if (_topWindow) {
		recomputeGraphicsScale();
		glk_window_clear(_topWindow);
		// On a mode switch the surface still holds the previous renderer's
		// output, so re-render the scene instead of re-blitting stale pixels.
		if (modeChanged)
			repaintCurrentScene();
		else
			blitSurfaceToWindow();
	}
	if (!_lastRoomDesc.empty())
		printRoomDesc(_lastRoomDesc);
}

} // namespace Comprehend
} // namespace Glk
