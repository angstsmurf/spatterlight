/*
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* aslxglk-map.hh -- the Quest 5 grid map pane (aslxglk-map.cc), as the Glk
 * frontend (aslxglk.cc) drives it. */

#ifndef QUESTION_ASLXGLK_MAP_HH
#define QUESTION_ASLXGLK_MAP_HH

#include "aslx-runtime.hh"

extern "C" {
#include "glk.h"
}

/* The map's graphics window, or null while the pane is hidden.  Autosave
 * records its tag and autorestore re-attaches it. */
extern winid_t gmapwin;

/* The grid_draw host hook: fold one engine paint command into the display
 * list (and open or close the pane on a ShowGrid). */
void grid_map_command(const aslx::GridDraw &g);

/* Rasterise and blit whatever changed since the last call. */
void redraw_grid_map();
/* Arrange/Redraw events: the window was blanked, send everything again. */
void grid_map_arrange();
/* Forget what is on screen, so the next redraw sends the whole surface (the
 * window is fresh, or no longer holds what was last blitted). */
void gm_screen_drop();
/* Session teardown: close the pane and drop the display list. */
void grid_map_reset();

/* The "you are here" glide.  While grid_map_animating(), the frontend runs
 * its Glk timer at GM_ANIM_TICK_MS and calls grid_map_anim_tick() on each
 * tick; grid_map_anim_snap() finishes the glide at once. */
const glui32 GM_ANIM_TICK_MS = 33;
bool grid_map_animating();
void grid_map_anim_tick();
void grid_map_anim_snap();

#endif  /* QUESTION_ASLXGLK_MAP_HH */
