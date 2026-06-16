// No-op stubs for the headless V6 build. See headless_compat.h for why these
// are needed: the Spatterlight Glk front-end normally provides them, but they
// are #ifdef SPATTERLIGHT-guarded in the engine sources.
//
// These cover the *graphics/windowing* side of V6 only. The text and game
// logic (object tree, parser, menus via zshogun_menu, zget_wind_prop's
// non-Spatterlight fallbacks, etc.) all run from the real engine sources, so a
// V6 game's non-graphical behaviour is exercised faithfully.

#ifndef SPATTERLIGHT

// EXT:0x12 set_window_style. Window attributes are a display concern with no
// effect on the Z-machine's observable text/logic state, so this is a no-op.
void zwindow_style() {}

#endif
