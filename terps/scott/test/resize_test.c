/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/* Regression test for the crash on resizing the window while a multi-game
   disk's selection menu is up (pull request #212).

   The menu (SelectGameFromMenu in scott_meta.c) runs from inside DetectGame(),
   before any database is loaded, and its key loop hands every event that is
   not a key press to Updates(). An evtype_Arrange there made Updates() redraw
   the room -- Look() or DrawRoomImage() -- with Rooms, Items and Game all
   still NULL. Updates() now returns at once while Rooms is NULL.

   This driver recreates that state (only the buffer window open, nothing
   loaded) and calls Updates() with the events the menu loop would forward.
   CheapGlk has no graphics, which would keep Updates() away from the redraw;
   the Glk gestalt is answered here instead so the engine believes graphics
   are available, as in the app. Without the guard the process dies on a NULL
   dereference; with it the run prints "survived" and exits 0.

     ./scott_resize_test

   Its own glk_main links against every engine object except scott.o's game
   loop, which the headless Makefile renames out of the way (see TOOL). */

#include <stdio.h>

#include "glk.h"
#include "glkstart.h"

#include "scott.h"
#include "scott_display.h"
#include "scott_game_info.h"
#include "glkimp.h"

/* The headless link takes these from here rather than from CheapGlk's
   cggestal.o, which defines nothing else. Only the graphics query matters to
   the engine on this path; the rest is answered as CheapGlk would, "no". */
glui32 glk_gestalt_ext(glui32 id, glui32 val, glui32 *arr, glui32 arrlen)
{
    (void)val;
    (void)arr;
    (void)arrlen;
    return id == gestalt_Graphics;
}

glui32 glk_gestalt(glui32 id, glui32 val)
{
    return glk_gestalt_ext(id, val, NULL, 0);
}

static void SendEvent(glui32 type, const char *name)
{
    event_t ev = { type, Bottom, 0, 0 };
    fprintf(stderr, "resize_test: Updates(%s) with Rooms=%p Game=%p\n",
            name, (void *)Rooms, (void *)Game);
    Updates(ev);
}

void glk_main(void)
{
    OpenBottomWindow();

    /* glk_main fills these before the menu can appear. */
    for (int i = 0; i < MAX_SYSMESS; i++)
        sys[i] = sysdict_i_am[i] ? sysdict_i_am[i] : "";

    gli_enable_graphics = 1;
    if (!GraphicsEnabled()) {
        fprintf(stderr, "resize_test: graphics not enabled, test is void\n");
        glk_exit();
    }

    if (Rooms != NULL || Game != NULL) {
        fprintf(stderr, "resize_test: a game is loaded, test is void\n");
        glk_exit();
    }

    SendEvent(evtype_Arrange, "evtype_Arrange");
    SendEvent(evtype_Timer, "evtype_Timer");

    fprintf(stderr, "resize_test: survived\n");
    glk_exit();
}
