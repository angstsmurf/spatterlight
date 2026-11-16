/*
 *	  Based on ScottFree Revision 1.14
 *
 *
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 *
 *
 *	  You must have a C11 compiler to build this program.
 */

/*
 * Parts of this source file (mainly the Glk parts) are copyright 2011
 * Chris Spiegel.
 *
 * Some notes about the Glk version:
 *
 * o Room descriptions, exits, and visible items can, as in the
 *	 original, be placed in a window at the top of the screen, or can be
 *	 inline with user input in the main window.	 The former is default,
 *	 and the latter can be selected with the -w flag.
 *
 * o Game saving and loading uses Glk, which means that providing a
 *	 save game file on the command-line does not work.  Instead,
 *	 the verb "restore" has been special-cased to call LoadGame(), which
 *	 prompts for a filename via Glk.
 *
 * o The local character set is expected to be compatible with ASCII, at
 *	 least in the printable character range.  Newlines are specially
 *	 handled, however, and converted to Glk's expected newline
 *	 character.
 */



#include <stdarg.h>
#include <stdlib.h>

#include "glk.h"
#include "us_display.h"
#include "title_image.h"

#include "detect_game.h"
#include "restore_state.h"
#include "sagagraphics.h"
#include "sagadraw_host.h"
#include "vector_common.h"
#include "rtpi_loader.h"

#include "parser.h"

#include "game_dispatch.h"
#include "sysmess_maps.h"

#include "scott.h"
#include "scott_meta.h"
#include "scott_actions.h"
#include "scott_display.h"
#include "scott_game_info.h"

#ifdef SPATTERLIGHT
#include "glkimp.h"
#else
/* Only Spatterlight has a setting for this: vector pictures and loading
   screens are drawn in one go unless the -g option is given. */
int gli_slowdraw = 0;
#endif

/* Defines only used in this file */

/* Status window size in characters */
#define TRS80_STATUS_WIDTH 64
#define TRS80_STATUS_HEIGHT 11
#define DEFAULT_STATUS_WIDTH 80
#define DEFAULT_STATUS_HEIGHT 10

#define LAMP_WARNING_THRESHOLD 25
#define LAMP_WARNING_INTERVAL 5

int LightRefill; /* Initial light duration, used when refilling the lamp */
int Counters[NUM_COUNTERS] =
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }; /* Range unknown */
int CurrentCounter;
int SavedRoom;
int RoomSaved[NUM_COUNTERS] =
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }; /* Range unknown */

uint64_t BitFlags = 0; /* Might be >32 flags - I haven't seen >32 yet */

int AutoInventory = 0; /* Auto-display inventory in room descriptions */
int Options; /* Option flags set */

Command *CurrentCommand = NULL; /* Current node in the parsed command chain */
GameInfo *Game; /* Metadata for the detected game (from the game database) */
MachineType CurrentSys = SYS_UNKNOWN; /* Detected platform (DOS, C64, Spectrum, etc.) */

const char *sys[MAX_SYSMESS]; /* Active system messages (localized prompts, exits, etc.) */

int AnimationFlag = 0;

/* JustStarted is only used for the error message "Can't undo on first move" */
int JustStarted = 1;
int should_restart = 0;
int StopTime = 0; /* When > 0, suppresses automatic (implicit) actions and decrements each turn */

int forced_style = -1; /* Display style number of the -t option, or -1 */

strid_t Transcript = NULL;

#define TRS80_LINE \
    "\n<------------------------------------------------------------>\n"

/* Convert a string to Unicode and write it to a Glk stream */
void PutUnicodeString(strid_t stream, const char *string)
{
    glui32 *unistring = ToUnicode(string);
    glk_put_string_stream_uni(stream, unistring);
    free(unistring);
}

/* Format and output text to a Glk window, converting to Unicode */
void vDisplay(winid_t w, const char *fmt, va_list ap)
{
    char msg[DISPLAY_BUFFER_SIZE];
    vsnprintf(msg, sizeof msg, fmt, ap);

    glui32 *unistring = ToUnicode(msg);
    size_t length = 0;
    while (unistring[length] != 0)
        length++;
    glk_put_string_stream_uni(glk_window_get_stream(w), unistring);
    /* Only text in the main window counts: "take all" asks whether that
       window is at the start of a line. */
    if (w == Bottom && length > 0)
        lastwasnewline = (unistring[length - 1] == '\n');
    free(unistring);
}

void Display(winid_t w, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vDisplay(w, fmt, ap);
    va_end(ap);
}

#ifdef SPATTERLIGHT
/* Apply a three-way UI setting to a pair of mutually exclusive option
   flags: 0 clears both, 1 sets the first, 2 sets the second. */
static void SetExclusiveOptions(int setting, int first, int second)
{
    switch (setting) {
    case 0:
        Options &= ~(first | second);
        break;
    case 1:
        Options = (Options | first) & ~second;
        break;
    case 2:
        Options = (Options | second) & ~first;
        break;
    }
}
#endif

/* Sync game options from the Spatterlight UI settings (delays, flicker,
   inventory display mode, palette override) and refresh the display. */
void UpdateSettings(void)
{
#ifdef SPATTERLIGHT
    if (gli_sa_delays)
        Options &= ~NO_DELAYS;
    else
        Options |= NO_DELAYS;

    if (gli_flicker)
        Options |= FLICKER_ON;
    else
        Options &= ~FLICKER_ON;

    SetExclusiveOptions(gli_sa_inventory, FORCE_INVENTORY, FORCE_INVENTORY_OFF);
    SetExclusiveOptions(gli_sa_palette, FORCE_PALETTE_ZX, FORCE_PALETTE_C64);
#endif

    if (DrawingVector())
        glk_request_timer_events(TimerDelay());

    palette_type previous_pal = palchosen;
    if (Options & FORCE_PALETTE_ZX)
        palchosen = ZXOPT;
    else if (Options & FORCE_PALETTE_C64) {
        if (Game->picture_format_version == HOWARTH_FORMAT)
            palchosen = C64A;
        else
            palchosen = C64B;
    } else
        palchosen = Game->palette;
    if (palchosen != previous_pal) {
        DefinePalette();
        if (VectorState != NO_VECTOR_IMAGE)
            DrawSomeVectorPixels(1);
    }
    should_draw_image = 1;
}

/* Handle Glk events: window arrangement changes trigger a full redraw,
   timer ticks advance animations or vector drawing. */
void Updates(event_t ev)
{
    /* A multi-game disk's menu runs before any game is loaded, so there is
       nothing to redraw yet. */
    if (Rooms == NULL)
        return;

    if (ev.type == evtype_Arrange) {
        UpdateSettings();

        VectorState = NO_VECTOR_IMAGE;

        CloseGraphicsWindow();
        OpenGraphicsWindow();

        if (!GraphicsEnabled())
            glk_request_timer_events(0);

        /* Without split screen the room description and inventory are in
           the main window's scrollback: redraw only the picture. */
        if (GraphicsEnabled()) {
            int saved_slowdraw = gli_slowdraw;
            gli_slowdraw = 0;
            if (showing_inventory == 1) {
                if (split_screen)
                    UpdateUSInventory();
                else
                    InventoryUS();
            } else {
                int lastimg = last_image_index;
                if (!showing_closeup)
                    should_draw_image = 1;
                if (split_screen)
                    Look();
                else
                    DrawRoomImage();
                last_image_index = lastimg;
                if (showing_closeup) {
                    should_draw_image = 1;
                    if (Game->type == US_VARIANT) {
                        DrawUSRoom(last_image_index);
                        DrawImageOrVector();
                    } else
                        DrawImage(last_image_index);
                }
            }
            gli_slowdraw = saved_slowdraw;
        }
    } else if (ev.type == evtype_Timer) {
        if (!GameTimer() && DrawingVector())
            DrawSomeVectorPixels((VectorState == NO_VECTOR_IMAGE));
    }
}

GLK_ATTRIBUTE_NORETURN void CleanupAndExit(void)
{
    if (Transcript)
        glk_stream_close(Transcript, NULL);
    if (DrawingVector()) {
        gli_slowdraw = 0;
        DrawSomeVectorPixels(0);
    }
    glk_exit();
}

void Output(const char *a) { Display(Bottom, "%s", a); }

/* Decrement the lamp timer each turn. Brian Howarth games use -1
   for infinite light. Warn when below 25 turns; extinguish at 0. */
static void TickLampTimer(void)
{
    if (Items[LIGHT_SOURCE].Location != DESTROYED && GameHeader.LightTime != -1 && !StopTime) {
        GameHeader.LightTime--;
        if (GameHeader.LightTime < 1) {
            SetBitFlag(LIGHTOUTBIT);
            if (IsPresent(LIGHT_SOURCE)) {
                Output(sys[LIGHT_HAS_RUN_OUT]);
            }
            if ((Options & PREHISTORIC_LAMP) || (Game->subtype & MYSTERIOUS))
                Items[LIGHT_SOURCE].Location = DESTROYED;
        } else if (GameHeader.LightTime < LAMP_WARNING_THRESHOLD) {
            if (IsPresent(LIGHT_SOURCE)) {
                if ((Options & SCOTTLIGHT) || (Game->subtype & MYSTERIOUS)) {
                    Display(Bottom, "%s %d %s\n", sys[LIGHT_RUNS_OUT_IN], GameHeader.LightTime, sys[TURNS]);
                } else {
                    if (GameHeader.LightTime % LAMP_WARNING_INTERVAL == 0)
                        Output(sys[LIGHT_GROWING_DIM]);
                }
            }
        }
    }
}

/* The system messages of the US releases with graphics. */
static void ApplyUSSysmess(void)
{
    sys[MESSAGE_DELIMITER] = " ";
    sys[ITEM_DELIMITER] = ". ";
    sys[EXITS_DELIMITER] = " ";
    sys[YOU_ARE] = "I am in a ";
    sys[YOU_SEE] = ". Visible: ";
    sys[INVENTORY] = "I've got: ";
    sys[NOTHING] = "Nothing at all. ";
    sys[EXITS] = "Some exits: ";
    if (CurrentSys == SYS_MSDOS)
        sys[WHAT_NOW] = "Command me? ";
    else
        sys[WHAT_NOW] = "What shall I do? ";
    sys[NORTH] = "NORTH";
    sys[EAST] = "EAST";
    sys[SOUTH] = "SOUTH";
    sys[WEST] = "WEST";
    sys[UP] = "UP";
    sys[DOWN] = "DOWN";
}

/* The -t option: replace the display style the game file asked for.
   The system messages stay the game's own. */
static void ApplyForcedStyle(void)
{
    static const int styles[] = {
        0,                         /* 0: plain ScottFree */
        TRS80_STYLE,               /* 1: TRS-80 */
        SPECTRUM_STYLE,            /* 2: ZX Spectrum */
        TI994A_STYLE,              /* 3: TI-99/4A */
        SPECTRUM_STYLE | PC_STYLE, /* 4: IBM PC */
    };

    if (forced_style < 0 || forced_style >= (int)(sizeof styles / sizeof styles[0]))
        return;

    Options &= ~(TRS80_STYLE | SPECTRUM_STYLE | TI994A_STYLE | PC_STYLE);
    Options |= styles[forced_style];
}

/* Select the "You are" or the "I am" system message set */
void SelectSystemMessages(void)
{
    const char **dictpointer = (Options & YOUARE) ? sysdict : sysdict_i_am;

    for (int i = 0; i < MAX_SYSMESS; i++) {
        if (dictpointer[i] != NULL)
            sys[i] = dictpointer[i];
        else
            sys[i] = sysdict[i];
    }
}

/* Detect the game format, load its database, show the title screen
   and set up the display style and windows the game wants. */
void SetUpGame(void)
{
    GameIDType game_type = DetectGame(game_file);

    if (game_type == UNKNOWN_GAME)
        Fatal("Unsupported game!");

    if (game_type != SCOTTFREE && game_type != TI994A)
        Options |= SPECTRUM_STYLE;
    else if (game_type != TI994A)
        Options |= TRS80_STYLE;

    ApplyForcedStyle();

    if (ZXLoadingScreen != NULL && Game->type != US_VARIANT)
        DrawZXTitleImage();

    if (title_screen != NULL && CurrentGame != RETURN_TO_PIRATES_ISLE) {
        if (split_screen)
            PrintTitleScreenGrid();
        else
            PrintTitleScreenBuffer();
    }

    if (Options & TRS80_STYLE) {
        TopWidth = TRS80_STATUS_WIDTH;
        TopHeight = TRS80_STATUS_HEIGHT;
    } else {
        TopWidth = DEFAULT_STATUS_WIDTH;
        TopHeight = DEFAULT_STATUS_HEIGHT;
    }

    if (game_type == TI994A) {
        Display(Bottom, "In this adventure, you may abbreviate any word \
by typing its first %d letters, and directions by typing \
one letter.\n\nDo you want to restore previously saved game?\n",
            GameHeader.WordLength);
        if (YesOrNo())
            LoadGame();
        glk_window_clear(Bottom);
    }

    /* US-variant games use a distinct display style with first-person
       messages and DOS/Apple II graphics. If no graphics are found,
       fall back to plain ScottFree mode. */
    if (Game->type == US_VARIANT) {
        if (HasGraphics()) {
            DrawTitleImageScott();
            OpenGraphicsWindow();
            ApplyUSSysmess();

            if (CurrentGame == RETURN_TO_PIRATES_ISLE)
                UpdateRTPISystemMessages();

            if (forced_style < 0)
                Options |= PC_STYLE;
        } else {
            CurrentGame = SCOTTFREE;
            Game->type = NO_TYPE;
            game_type = SCOTTFREE;
        }
    }

    if (game_type == SCOTTFREE)
        Output("\
Scott Free, A Scott Adams game driver in C.\n\
Release 1.14, (c) 1993,1994,1995 Swansea University Computer Society.\n\
Distributed under the GNU software license\n\n");

    OpenTopWindow();
}

/* One round of the game loop: implicit actions → Look → get input →
   explicit actions → lamp timer. */
void PlayTurn(void)
{
    int vb, no;

    if (should_restart)
        RestartGame();

    /* Run implicit (automatic) actions — these have vocab 0 and
       fire based on random chance or game state, not player input */
    if (!StopTime)
        PerformActions(0, 0);
    /* Refresh the room description (skip during TAKE ALL / DROP ALL) */
    if (!(CurrentCommand && CurrentCommand->allflag && !(CurrentCommand->allflag & LASTALL))) {
        print_look_to_transcript = should_look_in_transcript;
        Look();
        print_look_to_transcript = should_look_in_transcript = 0;
        if (!StopTime && !should_restart)
            SaveUndo();
    }

    if (should_restart)
        return;

    if (GetInput(&vb, &no) == 1)
        return;

    /* Run explicit (player-triggered) actions */
    switch (PerformActions(vb, no)) {
    case ER_RAN_ALL_LINES_NO_MATCH:
        if (!RecheckForExtraCommand()) {
            Output(sys[I_DONT_UNDERSTAND]);
            FreeCommands();
        }
        break;
    case ER_RAN_ALL_LINES:
        Output(sys[YOU_CANT_DO_THAT_YET]);
        /* Failed actions should interrupt chains of commands
           but not TAKE ALL and DROP ALL */
        if (CurrentCommand == NULL || !CurrentCommand->allflag)
            FreeCommands();
        break;
    default:
        JustStarted = 0;
    }

    TickLampTimer();
    if (StopTime)
        StopTime--;
}
