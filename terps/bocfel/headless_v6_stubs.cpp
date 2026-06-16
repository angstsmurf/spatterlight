// Headless V6 entrypoint-system stubs.
//
// The headless harness compiles and runs the real z6/entrypoints.cpp so that
// find_entrypoints() performs its routine detection and rtrue byte-patching,
// and check_entrypoints() dispatches per instruction. But entrypoints.cpp's
// table of C++ replacement functions, and the per-game global aggregates that
// its find_*_globals() read/write, normally live in the Glk/SPATTERLIGHT-only
// z6 sources (arthur.cpp, journey.cpp, shogun.cpp, zorkzero.cpp, v6_shared.cpp),
// which can't be compiled without Glk.
//
// So we provide here:
//   * the is_spatterlight_* flags (normally defined in screen.cpp under
//     #ifdef SPATTERLIGHT),
//   * zero-initialised instances of every per-game global aggregate the
//     find_*_globals() functions touch,
//   * the scalar v6_shared globals they populate,
//   * no-op definitions for every replacement function referenced by the
//     entrypoints table.
//
// The replacement functions are intentionally no-ops: they implement the
// graphics/windowing side of these V6 games, which the dumb terminal can't
// render anyway. The behaviour that matters for headless text testing is the
// rtrue byte-patching done in find_entrypoints(), which neutralises the
// original Z-code graphics routines.
//
// This file is only built by Makefile.headless (HEADLESS_V6). It is never part
// of the real Spatterlight build.

#ifdef HEADLESS_V6

// Include the core engine headers first (as entrypoints.cpp does), so the C++
// standard library is pulled in *before* the z6 game headers open their
// `extern "C"` blocks — otherwise <stdexcept> templates land inside extern "C"
// and the compile fails with "templates must have C++ linkage".
#include "zterp.h"
#include "screen.h"
#include "memory.h"
#include "dict.h"

#include "v6_specific.h"
#include "arthur.hpp"
#include "journey.hpp"
#include "shogun.hpp"
#include "zorkzero.hpp"
#include "v6_shared.hpp"

// --- V6 game-selection flags (normally screen.cpp, #ifdef SPATTERLIGHT) ------
bool is_spatterlight_arthur = false;
bool is_spatterlight_journey = false;
bool is_spatterlight_shogun = false;
bool is_spatterlight_zork0 = false;
bool is_spatterlight_v6 = false;

// --- Per-game global aggregates (normally arthur/journey/shogun/zorkzero.cpp).
// find_arthur_globals()/find_journey_globals()/find_shogun_globals()/
// find_zork0_globals() in entrypoints.cpp write anchor addresses into these.
ArthurGlobals ag;
ArthurRoutines ar;
ArthurTables at;

JourneyGlobals jg;
JourneyRoutines jr;
JourneyObjects jo;
JourneyAttributes ja;

ShogunGlobals sg;
ShogunRoutines sr;
ShogunTables st;
ShogunMenus sm;
ShogunObjects so;

ZorkGlobals zg;
ZorkRoutines zr;
ZorkTables zt;
ZorkObjects zo;
ZorkProperties zp;

// --- Scalar v6_shared globals the per-game finders populate ------------------
// (fg_global_idx / bg_global_idx are defined in entrypoints.cpp itself.)
uint8_t hint_chapter_global_idx = 0, hint_quest_global_idx = 0;
uint16_t hints_table_addr = 0;
uint16_t seen_hints_table_addr = 0;
uint16_t fkeys_table_addr = 0;
uint16_t fnames_table_addr = 0;

// --- No-op replacement functions --------------------------------------------
// (definitions appended below by the build/maintenance process; see
//  HEADLESS_V6_STUB_FUNCS block)
#define STUB(name) void name() {}

// BEGIN HEADLESS_V6_STUB_FUNCS
// 77 no-op replacement functions
STUB(ARTHUR_UPDATE_STATUS_LINE) STUB(BOLD_CURSOR) STUB(BOLD_OBJECT_CURSOR)
STUB(BOLD_PARTY_CURSOR) STUB(B_MOUSE_PEG_PICK) STUB(B_MOUSE_WEIGHT_PICK)
STUB(CENTER_1) STUB(CENTER_2) STUB(CENTER_3)
STUB(CENTER_PIC) STUB(CENTER_PIC_X) STUB(CHANGE_NAME)
STUB(DEFAULT_COLORS) STUB(DISPLAY_MAZE) STUB(DISPLAY_MAZE_PIC)
STUB(DISPLAY_MOVES) STUB(DIVIDER) STUB(DO_HINTS)
STUB(DRAW_FLOWERS) STUB(DRAW_PILE) STUB(DRAW_SN_BOXES)
STUB(DRAW_TOWER) STUB(ERASE_COMMAND) STUB(FANUCCI)
STUB(GET_FROM_MENU) STUB(GRAPHIC_STAMP) STUB(INIT_SCREEN)
STUB(INIT_STATUS_LINE) STUB(INTERLUDE_STATUS_LINE) STUB(J_PLAY)
STUB(MAC_II) STUB(MARGINAL_PIC) STUB(MAZE_MOUSE_F)
STUB(PBOZ_CLICK) STUB(PBOZ_WIN_CHECK) STUB(PEG_GAME)
STUB(PEG_GAME_READ_CHAR) STUB(PRINT_CHARACTER_COMMANDS) STUB(PRINT_COLUMNS)
STUB(READ_ELVISH) STUB(REFRESH_CHARACTER_COMMAND_AREA) STUB(REFRESH_SCREEN)
STUB(RT_AUTHOR_OFF) STUB(RT_HOT_KEY) STUB(RT_UPDATE_DESC_WINDOW)
STUB(RT_UPDATE_INVT_WINDOW) STUB(RT_UPDATE_MAP_WINDOW) STUB(RT_UPDATE_PICT_WINDOW)
STUB(RT_UPDATE_STAT_WINDOW) STUB(SCENE_SELECT) STUB(SETUP_FANUCCI)
STUB(SETUP_PBOZ) STUB(SETUP_SN) STUB(SETUP_TEXT_AND_STATUS)
STUB(SMALL_DOOR_F) STUB(SNARFEM) STUB(SN_CLICK)
STUB(TOWER_MODE) STUB(V_CREDITS) STUB(V_DEFINE)
STUB(V_MAP_LOOP) STUB(V_MODE) STUB(V_REFRESH)
STUB(V_VERSION) STUB(WCENTER) STUB(after_BUILDMAZE)
STUB(after_GET_FROM_MENU) STUB(after_INTRO) STUB(after_MAC_II)
STUB(after_SPLIT_BY_PICTURE) STUB(after_V_COLOR) STUB(after_V_CREDITS)
STUB(after_V_VERSION) STUB(arthur_INIT_STATUS_LINE) STUB(shogun_DISPLAY_BORDER)
STUB(shogun_UPDATE_STATUS_LINE) STUB(z0_UPDATE_STATUS_LINE)
// END HEADLESS_V6_STUB_FUNCS

#endif // HEADLESS_V6
