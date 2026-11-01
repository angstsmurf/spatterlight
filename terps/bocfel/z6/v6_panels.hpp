// v6_panels.hpp
//
// The windows of V6 games other than Infocom's four. See v6_panels.cpp.

#ifndef v6_panels_hpp
#define v6_panels_hpp

#include "screen.h"
#include "spatterlight-autosave.h"
#include "v6_specific.h"
#include "zterp.h"

// A V6 game which isn't one of Infocom's: it gets panels, held newlines and
// tracked geometry instead of a game-specific screen model.
inline bool v6_is_generic()
{
    return zversion == 6 && !is_spatterlight_v6;
}

bool is_v6_panel(const Window *win);
bool is_v6_grid_panel(const Window *win);
bool v6_is_story_window(int window);

// The geometry has changed: lay the panels out again when next they matter.
void v6_mark_layout_dirty();
void v6_forget_layout();

void v6_touch_window(Window *win);
void v6_layout_before_input(bool line_input);

void v6_stash_layout(library_state_data *dat);
void v6_recover_layout(const library_state_data *dat);

#endif /* v6_panels_hpp */
