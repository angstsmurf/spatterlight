// Compatibility shim for the headless (non-Glk, non-Spatterlight) build with
// V6 enabled. Force-included via -include in Makefile.headless.
//
// process.cpp wires EXT:0x12 (set_window_style) to zwindow_style() inside an
// "#ifndef ZTERP_NO_V6" block, but in this fork both the declaration (screen.h)
// and the definition (screen.cpp) of zwindow_style() are guarded by
// "#ifdef SPATTERLIGHT". A V6-enabled build that is *not* SPATTERLIGHT (i.e.
// this headless harness) therefore can't see the prototype and won't link.
//
// We declare it here so process.cpp compiles; headless_stubs.cpp supplies a
// no-op definition (window styling is a pure display concern with no bearing
// on game logic). The engine sources are left untouched.
#pragma once

#ifndef SPATTERLIGHT
void zwindow_style();

// In a non-SPATTERLIGHT build, the full `struct Window` is defined TU-locally
// inside screen.cpp; screen.h's richer Window (and several other types) are
// #ifdef SPATTERLIGHT. The z6 headers that entrypoints.cpp pulls in (e.g.
// zorkzero.hpp) mention `Window *` in prototypes the headless build never
// calls, so an incomplete forward declaration is all that's needed for them to
// parse. (screen.cpp's later full definition is compatible with this.)
struct Window;
#endif
