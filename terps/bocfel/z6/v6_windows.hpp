// v6_windows.hpp
//
// Glk window primitives for V6 games: pixel placement and swapping the Glk
// window behind a Z-machine window.

#ifndef v6_windows_hpp
#define v6_windows_hpp

#include <cstdint>

#include "screen.h"

// Move the Glk window to the Z-machine window's origin and size, clamped to
// the screen.
void v6_sizewin(Window *win);
void v6_define_window(Window *win, uint16_t x, uint16_t y, uint16_t width, uint16_t height);

void v6_delete_win(Window *win);
void v6_delete_glk_win(winid_t win);

// Give win a new Glk window of the given type. A window it already has goes
// to *stored_win, shrunk to nothing, to be put back later.
void v6_remap_win(Window *win, int type, winid_t *stored_win);
// As above, but a window of another type is deleted, and a grid is kept.
void v6_remap_win_to_grid(Window *win);

// Set the interpreter number from the user's setting, in V6 games keeping to
// the machines that have graphics.
void v6_apply_interpreter_number(void);

#endif /* v6_windows_hpp */
