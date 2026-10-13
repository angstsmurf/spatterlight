/* Comprehend engine glue: declarations shared by the comprehend*.cpp files
 * (comprehend.cpp, comprehend_state.cpp, comprehend_io.cpp, comprehend_gfx.cpp). */

#ifndef GLK_COMPREHEND_COMPREHEND_INTERNAL_H
#define GLK_COMPREHEND_COMPREHEND_INTERNAL_H

// blit at 2x to give comfortable on-screen pictures.
#define SCALE_FACTOR 2

// Spatterlight user settings, exported by glkimp (the headless build defines
// them as 0 in cheapglk). gli_slowdraw enables the animated picture reveal;
// gli_determinism selects the reproducible RNG (and suppresses slow-draw).
extern "C" int gli_slowdraw;
extern "C" int gli_determinism;
// Comprehend preferred-graphics-mode: 0 = more colours (PCjr/DHGR), 1 = less.
// A user preference in Spatterlight. Other Glk back-ends start with fewer
// colours unless the -c option is given (see glkstart.c), and switch with the
// #dhgr / #pcjr commands after that (see game.cpp).
extern "C" int gli_comprehend_graphics;

// Total content area in pixels, exported by glkimp (0 in the headless build).
// Used to keep a rescaled picture from eating the whole window vertically.
extern "C" int gscreenh;

#endif
