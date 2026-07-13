/* Focused reproducer for the compressed-room-descriptions off-by-one
   (detect_game.c, "for (ct = 0; ct < GameHeader.NumRooms; ...)").

   Seas of Blood is a FOUR_LETTER_COMPRESSED game with 83 rooms, so the Rooms
   array holds 84 entries (indices 0..83) but the compressed loader only fills
   Text for 0..82. Room 83 (== GameHeader.NumRooms) is exactly where
   PlayerIsDead() and the dark-move death path put the player. This driver
   reproduces that: load the real game, jump the player into room 83, and Look().

   It provides its own glk_main so it can reuse every engine object except
   scott.o (which owns the real glk_main). Run under ASan with a non-zero
   malloc fill so the uninitialised Rooms[83].Text is a wild pointer rather than
   a lucky NULL:

     ASAN_OPTIONS=malloc_fill_byte=0xaa:max_malloc_fill_size=1073741824 \
       ./scott_death_test "/path/to/Seas of Blood.z80"

   Expected on the UNFIXED engine: ASan/UBSan fault dereferencing Rooms[83].Text
   inside Look(). Expected on the FIXED engine (loader uses <=, allocation via
   MemCalloc): clean "You are ..." with an empty/placeholder room 83. */

#include <stdio.h>
#include <string.h>

#include "glk.h"
#include "glkstart.h"

#include "detect_game.h"
#include "scott.h"
#include "scott_actions.h"
#include "scott_display.h"

/* scott.o is linked too (it owns all the engine globals), but compiled with
   -Dglk_main=scott_real_glk_main so its game loop is renamed out of the way and
   the glk_main below is the one CheapGlk's main() calls. scott.c's
   glkunix_startup_code / glkunix_arguments are reused as-is to stash game_file. */

void glk_main(void)
{
    glk_stylehint_set(wintype_TextBuffer, style_User1, stylehint_Proportional, 0);
    Bottom = glk_window_open(0, 0, 0, wintype_TextBuffer, GLK_BUFFER_ROCK);
    if (Bottom == NULL) {
        fprintf(stderr, "death_test: could not open window\n");
        return;
    }
    glk_set_window(Bottom);

    if (game_file == NULL) {
        fprintf(stderr, "death_test: no game file given\n");
        return;
    }

    /* Populate the sys[] prompt table the way glk_main does, so Look() can
       print "You are ..." without dereferencing NULL system messages. */
    extern const char *sysdict_i_am[];
    for (int i = 0; i < MAX_SYSMESS; i++)
        sys[i] = sysdict_i_am[i] ? sysdict_i_am[i] : "";

    GameIDType id = DetectGame(game_file);
    if (id == UNKNOWN_GAME) {
        fprintf(stderr, "death_test: DetectGame failed\n");
        return;
    }

    fprintf(stderr, "death_test: loaded game id=%d, NumRooms=%d\n",
            id, GameHeader.NumRooms);

    /* This is exactly what PlayerIsDead() / the dark-move death path do. */
    MyLoc = GameHeader.NumRooms;
    fprintf(stderr, "death_test: forcing MyLoc = %d, Rooms[%d].Text = %p\n",
            MyLoc, MyLoc, (void *)Rooms[MyLoc].Text);

    split_screen = 0; /* keep the room description inline in the buffer window */
    Look();

    fprintf(stderr, "death_test: survived Look() at the death room\n");
    glk_exit();
}
