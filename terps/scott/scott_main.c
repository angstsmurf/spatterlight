/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  scott_main.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  The program entry points: command line options, start-up and the
 *  main loop.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"
#include "glkstart.h"

#include "randomness.h"
#include "restore_state.h"

#include "scott.h"
#include "detect_game.h"
#include "scott_display.h"

#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif

#define DETERMINISTIC_SEED 1234

glkunix_argumentlist_t glkunix_arguments[] = {
    { "-y", glkunix_arg_NoValue,
        "-y        Generate 'You are', 'You are carrying' type messages for games "
        "that use these instead (eg Robin Of Sherwood)" },
    { "-i", glkunix_arg_NoValue,
        "-i        Generate 'I am' type messages (default)" },
    { "-d", glkunix_arg_NoValue, "-d        Debugging info on load " },
    { "-s", glkunix_arg_NoValue,
        "-s        Generate authentic Scott Adams driver light messages rather "
        "than other driver style ones (Light goes out in n turns..)" },
    { "-t", glkunix_arg_NumberValue,
        "-t N      Lay out the room description in style N, whatever the game "
        "file: 0 plain ScottFree, 1 TRS-80, 2 ZX Spectrum, 3 TI-99/4A, 4 IBM PC" },
    { "-p", glkunix_arg_NoValue,
        "-p        Use for prehistoric databases which don't use bit 16" },
    { "-w", glkunix_arg_NoValue, "-w        Disable upper window" },
    { "-n", glkunix_arg_NoValue, "-n        No delays" },
    { "-f", glkunix_arg_NoValue, "-f        Flicker the room description like some original games" },
#ifndef SPATTERLIGHT
    { "-g", glkunix_arg_NoValue,
        "-g        Draw vector pictures and loading screens gradually, "
        "as the original games do" },
    { "-c", glkunix_arg_NoValue,
        "-c        Draw pictures in the Commodore 64 palette, whatever the game file" },
    { "-x", glkunix_arg_NoValue,
        "-x        Draw pictures in the ZX Spectrum palette, whatever the game file" },
    { "-a", glkunix_arg_NoValue,
        "-a        Always list what you carry in the upper window" },
#else
#endif
    { "", glkunix_arg_ValueFollows, "filename    file to load" },

    { NULL, glkunix_arg_End, NULL }
};

/* Parse the leading -options. Returns the number of arguments consumed
   after argv[0]; the game file, if any, follows them. */
static int ParseOptions(int argc, char **argv)
{
    int i = 1;
    for (; i < argc && argv[i][0] == '-'; i++) {
        const char *opt = argv[i];
        switch (opt[1]) {
        case 'y':
            Options |= YOUARE;
            break;
        case 'i':
            Options &= ~YOUARE;
            break;
        case 'd':
            Options |= DEBUGGING;
            break;
        case 's':
            Options |= SCOTTLIGHT;
            break;
        case 't':
            /* -tN or -t N. A bare -t is the TRS-80 style, as it
               always was. */
            if (isdigit((unsigned char)opt[2])) {
                forced_style = atoi(opt + 2);
            } else if (i + 1 < argc && isdigit((unsigned char)argv[i + 1][0])
                && argv[i + 1][1] == '\0') {
                forced_style = atoi(argv[++i]);
            } else {
                forced_style = 1;
            }
            break;
        case 'p':
            Options |= PREHISTORIC_LAMP;
            break;
        case 'w':
            split_screen = 0;
            break;
        case 'n':
            Options |= NO_DELAYS;
            break;
        case 'f':
            Options |= FLICKER_ON;
            break;
#ifndef SPATTERLIGHT
        case 'g':
            gli_slowdraw = 1;
            break;
        case 'c':
            Options = (Options | FORCE_PALETTE_C64) & ~FORCE_PALETTE_ZX;
            break;
        case 'x':
            Options = (Options | FORCE_PALETTE_ZX) & ~FORCE_PALETTE_C64;
            break;
        case 'a':
            Options = (Options | FORCE_INVENTORY) & ~FORCE_INVENTORY_OFF;
            break;
#endif
        }
    }
    return i - 1;
}

/* Parse command-line arguments (display style, debugging, delays, etc.)
   and extract the game file path and its parent directory. */
int glkunix_startup_code(glkunix_startup_t *data)
{
    int argc = data->argc;
    char **argv = data->argv;

    if (argc < 1)
        return 0;

    int consumed = ParseOptions(argc, argv);
    argc -= consumed;
    argv += consumed;

#ifdef GARGLK
    garglk_set_program_name("ScottFree 1.15");
    garglk_set_program_info("ScottFree 1.15 by Alan Cox\n"
                            "Glk port by Chris Spiegel\n");
#endif

    if (argc == 2) {
        game_file = argv[1];

        const char *base = strrchr(game_file, '/');
        if (base == NULL)
            base = strrchr(game_file, '\\');
        base = base ? base + 1 : game_file;

#ifdef GARGLK
        garglk_set_story_name(base);
#endif

        int dirlen = (int)(base - game_file);
        if (dirlen) {
            DirPath = MemAlloc(dirlen + 1);
            memcpy(DirPath, game_file, dirlen);
            DirPath[dirlen] = 0;
        }
    }

    return 1;
}

/* Main entry point: set the game up, then run the game loop. */
void glk_main(void)
{
    OpenBottomWindow();

    if (game_file == NULL)
        Fatal("No game provided");

    SelectSystemMessages();
    SetUpGame();

    /* Not only for Spatterlight's preferences: this is also where the
       palette is first chosen, from the game file or the -c / -x options. */
    UpdateSettings();
#ifdef SPATTERLIGHT
    if (gli_determinism)
        set_erkyrath_random(DETERMINISTIC_SEED);
    else
#endif
    set_erkyrath_random(0);

    InitialState = SaveCurrentState();

    while (1) {
        glk_tick();
        PlayTurn();
    }
}
