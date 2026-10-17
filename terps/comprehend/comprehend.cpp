/* Comprehend engine glue — Spatterlight port: lifetime, start-up and the
 * game factory. Saving/undo is in comprehend_state.cpp, text and input in
 * comprehend_io.cpp, pictures in comprehend_gfx.cpp. */

#include "comprehend.h"
#include "graphics_magician.h"
#include "pics.h"
#include "comprehend_internal.h"

namespace Glk {
namespace Comprehend {

// Fixed RNG seed used in determinism (testing) mode, matching scott/plus/taylor.
#define COMPREHEND_DETERMINISTIC_SEED 1234

Comprehend *g_comprehend;

static DebuggerStub s_debuggerStub;
DebuggerStub *g_debugger = &s_debuggerStub;

Comprehend::Comprehend() :
	_saveSlot(-1), _graphicsEnabled(true), _disableSaves(false), _shouldQuit(false),
	_topWindow(nullptr), _statusWindow(nullptr), _bottomWindow(nullptr),
	_drawSurface(nullptr), _game(nullptr), _pics(nullptr), _drawFlags(0),
	_pixelSize(SCALE_FACTOR), _slowDrawActive(false), _suppressSlowDraw(false) {
	g_comprehend = this;
}

Comprehend::~Comprehend() {
	delete _drawSurface;
	delete _game;
	delete _pics;
	g_comprehend = nullptr;
}

void Comprehend::runGame() {
	initialize();
	// Seed the shared erkyrath RNG, exactly like scott/taylormade/plus. In
	// determinism mode (the user's testing-mode theme option, and the headless
	// regression harness) use a fixed seed so the committed walkthroughs replay
	// identically on every host; otherwise seed 0 = the platform-native RNG.
	// gli_determinism is populated by the front-end during initialize()'s window
	// setup, so it is valid here.
	{
		bool deterministic = false;
#ifdef SPATTERLIGHT
		deterministic = gli_determinism;
#endif
#ifdef COMPREHEND_HEADLESS
		// The headless harness drives the game from a script file and never
		// arranges a window, so gli_determinism stays 0 there -- force the
		// reproducible RNG whenever a transcript is being replayed.
		if (getenv("COMPREHEND_SCRIPT"))
			deterministic = true;
		// Harness knobs for the picture log: which renderer (0 = more colours,
		// 1 = fewer) and whether pictures go through the slow-draw reveal.
		if (const char *g = getenv("COMPREHEND_GFX"))
			gli_comprehend_graphics = atoi(g);
		if (const char *sd = getenv("COMPREHEND_SLOWDRAW"))
			gli_slowdraw = atoi(sd);
#endif
		set_erkyrath_random(deterministic ? COMPREHEND_DETERMINISTIC_SEED : 0);
	}
	createGame();
	if (_game) {
		_game->loadGame();
		applyPreferredGraphicsMode();
		_game->playGame();
	}
	deinitialize();
}

void Comprehend::initialize() {
	// Centred style for text the original interpreter centres on screen (e.g.
	// the OO-Topos title credits). Hints must be set before the window opens.
	glk_stylehint_set(wintype_TextBuffer, style_User1,
					  stylehint_Justification, stylehint_just_Centered);

	_bottomWindow = glk_window_open(nullptr, 0, 0, wintype_TextBuffer, GLK_BUFFER_ROCK);
	glk_set_window(_bottomWindow);
	_statusWindow = glk_window_open(_bottomWindow,
		winmethod_Above | winmethod_Fixed, 1, wintype_TextGrid, GLK_STATUS_ROCK);
#ifdef COMPREHEND_HEADLESS
	if (const char *log = getenv("COMPREHEND_PICLOG"))
		_picLog = fopen(log, "w");
#endif
	showGraphics();   // closes and reopens _statusWindow around the graphics window

	// The Apple II disk image (if any) is loaded before runGame(), so the
	// renderer, and with it the surface width, is already known.
	_drawSurface = new DrawSurface();
	_drawSurface->setRenderWidth(renderWidth());
	_pics = new Pics();
}

void Comprehend::deinitialize() {
	// Deliberately do NOT close the windows here. runGame() calls this once,
	// right before the process exits, so closing every window only wipes the
	// final screen (e.g. Talisman's "THE END" / death-menu text and picture)
	// before the player can read it. Leaving the windows open lets Glk's
	// implicit exit keep the last frame on screen until the player dismisses
	// it -- the normal Glk convention. The OS reclaims the windows on exit.
}

void Comprehend::createGame() {
	if (!_traits)
		error("Unknown game id '%s'", _gameId.c_str());
	_game = _traits->create();

	// The Apple II hi-res renderer (graphics_magician.cpp) handles two Graphics
	// Magician dialects, both validated pixel-exact vs MAME. Talisman and OO-Topos
	// use op15 fill-bounds sub-ops (newer); Transylvania and Crimson Crown never
	// emit op15, end images on op7, and seed background fills in the bottom text
	// rows, so they need the full-screen fill clip (the legacy flag).
	gmSetLegacyFormat(_traits->gmLegacyFormat);
}

} // namespace Comprehend
} // namespace Glk
