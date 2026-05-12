/*
 *	  ScottFree Revision 1.14
 *
 *
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 *
 *
 *	  You must have an ANSI C compiler to build this program.
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
 *	 save game file on the command-line will no longer work.  Instead,
 *	 the verb "restore" has been special-cased to call GameLoad(), which
 *	 now prompts for a filename via Glk.
 *
 * o The local character set is expected to be compatible with ASCII, at
 *	 least in the printable character range.  Newlines are specially
 *	 handled, however, and converted to Glk's expected newline
 *	 character.
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"
#include "glkstart.h"
#include "saga.h"
#include "titleimage.h"

#include "detectgame.h"
#include "restorestate.h"
#include "sagagraphics.h"
#include "vector_common.h"
#include "rtpi_graphics.h"

#include "parser.h"

#include "game_specific.h"
#include "gremlins.h"
#include "hulk.h"
#include "robinofsherwood.h"
#include "randomness.h"

#include "bsd.h"
#include "scott.h"
#include "scott_actions.h"

#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif

/* Defines only used in this file */
#define READSTRING_BUFFER_SIZE 1024

#define NUM_SYSTEM_MESSAGES 60
#define NUM_BATTLE_MESSAGES 33
#define ENEMY_TABLE_SIZE 126

#define LAMP_WARNING_THRESHOLD 25
#define LAMP_WARNING_INTERVAL 5

#define HULK_DOS_ID 703
#define HULK_DOS_WIDTH 280
#define HULK_DOS_HEIGHT 158
#define DETERMINISTIC_SEED 1234

const char *game_file = NULL;
char *DirPath = "."; /* Directory containing the game file */

/* Core game data — populated by LoadDatabase or the SAGA/Plus loaders */
Header GameHeader;
Item *Items = NULL;
Room *Rooms = NULL;
char **Verbs = NULL;
char **Nouns = NULL;
char **Messages = NULL;
Action *Actions = NULL;
int LightRefill; /* Initial light duration, used when refilling the lamp */
int Counters[NUM_COUNTERS] =
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }; /* Range unknown */
int CurrentCounter;
int SavedRoom;
int RoomSaved[NUM_COUNTERS] =
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }; /* Range unknown */

long BitFlags = 0; /* Might be >32 flags - I haven't seen >32 yet */

int AutoInventory = 0; /* Auto-display inventory in room descriptions */
int Options; /* Option flags set */
glui32 TopWidth; /* Terminal width */
glui32 TopHeight; /* Height of top window */
int ImageWidth = 255;
int ImageHeight = 96;
int file_baseline_offset = 0; /* Byte offset adjustment for files with extra data prepended */
char *title_screen = NULL;

Command *CurrentCommand = NULL; /* Current node in the parsed command chain */
GameInfo *Game; /* Metadata for the detected game (from the game database) */
MachineType CurrentSys = SYS_UNKNOWN; /* Detected platform (DOS, C64, Spectrum, etc.) */

extern const char *sysdict[MAX_SYSMESS];
extern const char *sysdict_i_am[MAX_SYSMESS];

const char *sys[MAX_SYSMESS]; /* Active system messages (localized prompts, exits, etc.) */
const char *system_messages[NUM_SYSTEM_MESSAGES];

const char *battle_messages[NUM_BATTLE_MESSAGES]; /* Seas of Blood combat messages */
uint8_t enemy_table[ENEMY_TABLE_SIZE]; /* Seas of Blood enemy stat table */

uint8_t *entire_file; /* Raw game file loaded into memory */
size_t file_length;

int AnimationFlag = 0;

/* Display state flags — track what's currently shown to avoid redundant redraws */
int showing_inventory = 0;
int showing_closeup = 0;
int last_image_index = -1;
int lastwasnewline = 0;
int should_draw_image = 0;

extern SavedState *InitialState;

/* JustStarted is only used for the error message "Can't undo on first move" */
int JustStarted = 1;
int should_restart = 0;
int StopTime = 0; /* When > 0, suppresses automatic (implicit) actions and decrements each turn */

int should_look_in_transcript = 0;

int split_screen = 1;
winid_t Bottom, Top;
winid_t Graphics;

strid_t Transcript = NULL;

#define TRS80_LINE \
    "\n<------------------------------------------------------------>\n"

/* Format and output text to a Glk window, converting to Unicode */
void Display(winid_t w, const char *fmt, ...)
{
    va_list ap;
    char msg[DISPLAY_BUFFER_SIZE];

    int size = sizeof msg;

    va_start(ap, fmt);
    vsnprintf(msg, size, fmt, ap);
    va_end(ap);

    int oldlastwasnewline = lastwasnewline;
    glui32 *unistring = ToUnicode(msg);
    if (w != Bottom)
        lastwasnewline = oldlastwasnewline;
    glk_put_string_stream_uni(glk_window_get_stream(w), unistring);
    free(unistring);
}

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

    switch (gli_sa_inventory) {
    case 0:
        Options &= ~(FORCE_INVENTORY | FORCE_INVENTORY_OFF);
        break;
    case 1:
        Options = (Options | FORCE_INVENTORY) & ~FORCE_INVENTORY_OFF;
        break;
    case 2:
        Options = (Options | FORCE_INVENTORY_OFF) & ~FORCE_INVENTORY;
        break;
    }

    switch (gli_sa_palette) {
    case 0:
        Options &= ~(FORCE_PALETTE_ZX | FORCE_PALETTE_C64);
        break;
    case 1:
        Options = (Options | FORCE_PALETTE_ZX) & ~FORCE_PALETTE_C64;
        break;
    case 2:
        Options = (Options | FORCE_PALETTE_C64) & ~FORCE_PALETTE_ZX;
        break;
    }
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
    if (ev.type == evtype_Arrange) {
        UpdateSettings();

        VectorState = NO_VECTOR_IMAGE;

        CloseGraphicsWindow();
        OpenGraphicsWindow();

        if (!gli_enable_graphics)
            glk_request_timer_events(0);

        if (split_screen && gli_enable_graphics) {
            int saved_slowdraw = gli_slowdraw;
            gli_slowdraw = 0;
            if (showing_inventory == 1) {
                UpdateUSInventory();
            } else {
                int lastimg = last_image_index;
                if (!showing_closeup)
                    should_draw_image = 1;
                Look();
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
        switch (Game->type) {
        case SHERWOOD_VARIANT:
            UpdateRobinOfSherwoodAnimations();
            break;
        case GREMLINS_VARIANT:
            UpdateGremlinsAnimations();
            break;
        case SECRET_MISSION_VARIANT:
            UpdateSecretAnimations();
            break;
        default:
            if (DrawingVector())
                DrawSomeVectorPixels((VectorState == NO_VECTOR_IMAGE));
            break;
        }
    }
}

/* Open or find the status text-grid window above the main window */
void OpenTopWindow(void)
{
    Top = FindGlkWindowWithRock(GLK_STATUS_ROCK);
    if (Top == NULL) {
        if (split_screen) {
            Top = glk_window_open(Bottom, winmethod_Above | winmethod_Fixed,
                TopHeight, wintype_TextGrid, GLK_STATUS_ROCK);
            if (Top == NULL) {
                split_screen = 0;
                Top = Bottom;
            } else {
                glk_window_get_size(Top, &TopWidth, NULL);
            }
        } else {
            Top = Bottom;
        }
    }
}

/* Compute the largest integer pixel multiplier that fits the game's
   native image dimensions within the available graphics area. */
glui32 OptimalPictureSize(glui32 graphwidth, glui32 graphheight, glui32 *outwidth, glui32 *outheight)
{
    int multiplier = 1;
    multiplier = graphheight / ImageHeight;
    if (ImageWidth * multiplier > graphwidth)
        multiplier = graphwidth / ImageWidth;

    if (multiplier == 0)
        multiplier = 1;

    *outwidth = ImageWidth * multiplier;
    *outheight = ImageHeight * multiplier;

    return multiplier;
}

/* Open the graphics window between the status window and the main text
   window, sized to fit the game's native image resolution at the best
   integer scale. Re-opens the status window above it if needed. */
void OpenGraphicsWindow(void)
{
#ifdef SPATTERLIGHT
    if (!gli_enable_graphics)
        return;
#endif
    glui32 graphwidth, graphheight, optimal_width, optimal_height;

    if (Top == NULL)
        Top = FindGlkWindowWithRock(GLK_STATUS_ROCK);
    if (Graphics == NULL)
        Graphics = FindGlkWindowWithRock(GLK_GRAPHICS_ROCK);
    if (Graphics == NULL && Top != NULL) {
        glk_window_get_size(Top, &TopWidth, &TopHeight);
        glk_window_close(Top, NULL);
        Graphics = glk_window_open(Bottom, winmethod_Above | winmethod_Proportional,
            60, wintype_Graphics, GLK_GRAPHICS_ROCK);
        glk_window_get_size(Graphics, &graphwidth, &graphheight);
        pixel_size = OptimalPictureSize(graphwidth, graphheight, &optimal_width, &optimal_height);
        x_offset = ((int)graphwidth - (int)optimal_width) / 2;

        if (graphheight > optimal_height) {
            winid_t parent = glk_window_get_parent(Graphics);
            if (parent)
                glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
                    optimal_height, NULL);
        }

    /* Set the graphics window background to match
       the main window background, best as we can,
       and clear the window. */
        glui32 background_color;
        if (Bottom && glk_style_measure(Bottom, style_Normal, stylehint_BackColor, &background_color)) {
            glk_window_set_background_color(Graphics, background_color);
            glk_window_clear(Graphics);
        }

        Top = glk_window_open(Bottom, winmethod_Above | winmethod_Fixed, TopHeight,
            wintype_TextGrid, GLK_STATUS_ROCK);
        glk_window_get_size(Top, &TopWidth, &TopHeight);
    } else {
        if (!Graphics)
            Graphics = glk_window_open(Bottom, winmethod_Above | winmethod_Proportional, 60,
                wintype_Graphics, GLK_GRAPHICS_ROCK);
        glk_window_get_size(Graphics, &graphwidth, &graphheight);
        pixel_size = OptimalPictureSize(graphwidth, graphheight, &optimal_width, &optimal_height);
        x_offset = (graphwidth - optimal_width) / 2;
        winid_t parent = glk_window_get_parent(Graphics);
        if (parent)
            glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
                optimal_height, NULL);
    }
    right_margin = optimal_width + x_offset;
}

void CloseGraphicsWindow(void)
{
    if (Graphics == NULL)
        Graphics = FindGlkWindowWithRock(GLK_GRAPHICS_ROCK);
    if (Graphics) {
        glk_window_close(Graphics, NULL);
        Graphics = NULL;
        glk_window_get_size(Top, &TopWidth, &TopHeight);
    }
}

GLK_ATTRIBUTE_NORETURN void CleanupAndExit(void)
{
    if (Transcript)
        glk_stream_close(Transcript, NULL);
    if (DrawingVector()) {
#ifdef SPATTERLIGHT
        gli_slowdraw = 0;
#endif
        DrawSomeVectorPixels(0);
    }
    glk_exit();
}


/* Map a noun index to its canonical (non-synonym) word.
   Synonyms in the dictionary are prefixed with '*' and share
   the word number of the preceding canonical entry. */
const char *MapSynonym(int noun)
{
    int n = 1;
    const char *tp;
    static char lastword[16]; /* Last non synonym */
    while (n <= GameHeader.NumWords) {
        tp = Nouns[n];
        if (*tp == '*')
            tp++;
        else
            strncpy(lastword, tp, GameHeader.WordLength + 1);
        if (n == noun)
            return (lastword);
        n++;
    }
    return (NULL);
}

/* Find the item whose AutoGet word matches the given noun at a given location.
   Returns the item index, or -1 if no match. loc=0 matches any location. */
int MatchUpItem(int noun, int loc)
{
    const char *word = MapSynonym(noun);
    int ct = 0;

    if (word == NULL)
        word = Nouns[noun];

    while (ct <= GameHeader.NumItems) {
        if (Items[ct].AutoGet && (loc == 0 || Items[ct].Location == loc) && xstrncasecmp(Items[ct].AutoGet, word, GameHeader.WordLength) == 0)
            return (ct);
        ct++;
    }
    return (-1);
}

/* Read a double-quoted string from a ScottFree plaintext database file.
   Handles escaped quotes (""), backtick-to-quote conversion, and
   strips non-ASCII characters for Glk compatibility. */
static char *ReadString(FILE *f)
{
    char tmp[READSTRING_BUFFER_SIZE];
    int c;
    int ct = 0;
    do {
        c = fgetc(f);
    } while (c != EOF && isspace((unsigned char)c));
    if (c != '"')
        Fatal("Initial quote expected");

    for (;;) {
        c = fgetc(f);
        if (c == EOF)
            Fatal("EOF in string");
        if (c == '"') {
            int nc = fgetc(f);
            if (nc != '"') {
                ungetc(nc, f);
                break;
            }
        }
        if (c == '`')
            c = '"';
        /* Ignore CR (assume CRLF in DOS-formatted files) */
        if (c == '\r')
            continue;
        /* Pass ASCII and newline to Glk; replace anything else */
        if (c == '\n' || (c >= ' ' && c <= '~'))
            tmp[ct++] = c;
        else
            tmp[ct++] = '?';
    }
    tmp[ct] = 0;
    char *result = MemAlloc(ct + 1);
    memcpy(result, tmp, ct + 1);
    return result;
}

int header[24];

/* Free all dynamically allocated game data arrays (actions, items, rooms,
   dictionary, messages). Frees individual string elements that aren't
   the shared "." placeholder, then frees the arrays and NULLs them. */
void FreeDatabase(void)
{
    if (Actions != NULL)
        free(Actions);
    if (Items != NULL) {
        for (int i = 0; i <= GameHeader.NumItems; i++) {
            if (Items[i].Text[0] != '.') {
                free(Items[i].Text);
            }
        }
        free(Items);
    }
    if (Rooms != NULL) {
        for (int i = 0; i <= GameHeader.NumRooms; i++) {
            if (Rooms[i].Text[0] != '.') {
                free(Rooms[i].Text);
            }
        }
        free(Rooms);
    }
    if (Nouns != NULL) {
        for (int i = 0; i <= GameHeader.NumWords; i++) {
            if (Nouns[i][0] != '.') {
                free(Nouns[i]);
            }
        }
        free(Nouns);
    }
    if (Verbs != NULL) {
        for (int i = 0; i <= GameHeader.NumWords; i++) {
            if (Verbs[i][0] != '.') {
                free(Verbs[i]);
            }
        }
        free(Verbs);
    }
    if (Messages != NULL) {
        for (int i = 0; i < GameHeader.NumMessages; i++) {
            if (Messages[i][0] != '.') {
                free(Messages[i]);
            }
        }
        free(Messages);
    }

    Items = NULL;
    Actions = NULL;
    Verbs = NULL;
    Nouns = NULL;
    Rooms = NULL;
    Messages = NULL;
}

/* Load a ScottFree plaintext database from a FILE stream.

   Reads the header, actions, word pairs (verb/noun dictionary), rooms
   with exits, messages, items with locations, then the version/adventure
   number trailer. Returns SCOTTFREE on success, HULK_US if the DOS Hulk
   images are found, or UNKNOWN_GAME on parse failure.

   The file format is the standard Scott Adams plain-text format as
   used by the original IBM PC releases, the ScottKit toolkit,
   and compatible tools. */
GameIDType LoadDatabase(FILE *f, int loud)
{
    /* Load the header */

    if (fscanf(f, "%*d %hd %hd %hd %hd %hd %hd %hd %hd %hd %hd %hd",
            &GameHeader.NumItems, &GameHeader.NumActions,
            &GameHeader.NumWords, &GameHeader.NumRooms,
            &GameHeader.MaxCarry, &GameHeader.PlayerRoom,
            &GameHeader.Treasures, &GameHeader.WordLength,
            &GameHeader.LightTime, &GameHeader.NumMessages,
            &GameHeader.TreasureRoom)
        < 10) {
        if (loud)
            debug_print("Invalid database(bad header)\n");
        return UNKNOWN_GAME;
    }
    LightRefill = GameHeader.LightTime;
    AllocateGameData();

    if (loud) {
        debug_print("Number of items: %d\n", GameHeader.NumItems);
        debug_print("Number of actions: %d\n", GameHeader.NumActions);
        debug_print("Number of words: %d\n", GameHeader.NumWords);
        debug_print("Word length: %d\n", GameHeader.WordLength);
        debug_print("Number of rooms: %d\n", GameHeader.NumRooms);
        debug_print("Number of messages: %d\n", GameHeader.NumMessages);
        debug_print("Max carried: %d\n", GameHeader.MaxCarry);
        debug_print("Starting location: %d\n", GameHeader.PlayerRoom);
        debug_print("Light time: %d\n", GameHeader.LightTime);
        debug_print("Number of treasures: %d\n", GameHeader.Treasures);
        debug_print("Treasure room: %d\n", GameHeader.TreasureRoom);
    }

    /* Load the actions */

    if (loud)
        debug_print("Reading %d actions.\n", GameHeader.NumActions);
    for (int ct = 0; ct <= GameHeader.NumActions; ct++) {
        if (fscanf(f, "%hu %hu %hu %hu %hu %hu %hu %hu",
                &Actions[ct].Vocab,
                &Actions[ct].Condition[0],
                &Actions[ct].Condition[1],
                &Actions[ct].Condition[2],
                &Actions[ct].Condition[3],
                &Actions[ct].Condition[4],
                &Actions[ct].Opcode[0],
                &Actions[ct].Opcode[1])
            != 8) {
            fprintf(stderr, "Bad action line (%d)\n", ct);
            FreeDatabase();
            return UNKNOWN_GAME;
        }

        if (loud) {
            debug_print("Action %d Vocab: %d (Verb:%d/NounOrChance:%d)\n", ct, Actions[ct].Vocab,
               Actions[ct].Vocab / VOCAB_MULTIPLIER, Actions[ct].Vocab % VOCAB_MULTIPLIER);
            for (int i = 0; i < NUM_CONDITIONS; i++)
                debug_print("Action %d Condition[%d]: %d (%d/%d)\n", ct, i,
                    Actions[ct].Condition[i], Actions[ct].Condition[i] % CONDITION_MULTIPLIER, Actions[ct].Condition[i] / CONDITION_MULTIPLIER);
            debug_print("Action %d Opcode [0]]: %d (%d/%d)\n", ct, Actions[ct].Opcode[0], Actions[ct].Opcode[0] % VOCAB_MULTIPLIER, Actions[ct].Opcode[0] / VOCAB_MULTIPLIER);
            debug_print("Action %d Opcode [1]]: %d (%d/%d)\n", ct, Actions[ct].Opcode[1], Actions[ct].Opcode[1] % VOCAB_MULTIPLIER, Actions[ct].Opcode[1] / VOCAB_MULTIPLIER);
        }
    }

    /* Load the verb/noun dictionary (word pairs) */
    if (loud)
        debug_print("Reading %d word pairs.\n", GameHeader.NumWords);
    for (int ct = 0; ct <= GameHeader.NumWords; ct++) {
        Verbs[ct] = ReadString(f);
        debug_print("Verbs %d:%s.\n", ct, Verbs[ct]);
        Nouns[ct] = ReadString(f);
        debug_print("Nouns %d:%s.\n", ct, Nouns[ct]);
    }
    /* Load rooms: 6 exit directions followed by a description string */
    if (loud)
        debug_print("Reading %d rooms.\n", GameHeader.NumRooms);
    for (int ct = 0; ct <= GameHeader.NumRooms; ct++) {
        int exits[NUM_EXITS];
        if (fscanf(f, "%d %d %d %d %d %d",
                &exits[0], &exits[1], &exits[2],
                &exits[3], &exits[4], &exits[5])
            != NUM_EXITS) {
            debug_print("Bad room line (%d)\n", ct);
            FreeDatabase();
            return UNKNOWN_GAME;
        }
        for (int i = 0; i < NUM_EXITS; i++)
            Rooms[ct].Exits[i] = exits[i];

        Rooms[ct].Text = ReadString(f);
        if (loud) {
            debug_print("Room %d: \"%s\"\n", ct, Rooms[ct].Text);
            debug_print("Room connections for room %d:\n", ct);
            for (int i = 0; i < NUM_EXITS; i++)
                debug_print("Exit %d: %d\n", i, Rooms[ct].Exits[i]);
        }
        Rooms[ct].Image = NO_IMAGE;
    }

    /* Load messages (printed by action ops 1-51 and 102+) */
    if (loud)
        debug_print("Reading %d messages.\n", GameHeader.NumMessages);
    for (int ct = 0; ct <= GameHeader.NumMessages; ct++) {
        Messages[ct] = ReadString(f);
        if (loud)
            debug_print("Message %d: \"%s\"\n", ct, Messages[ct]);
    }
    /* Load items: description string (with optional /AutoGet/ word) and location */
    if (loud)
        debug_print("Reading %d items.\n", GameHeader.NumItems);
    for (int ct = 0; ct <= GameHeader.NumItems; ct++) {
        Items[ct].Text = ReadString(f);
        if (loud)
            debug_print("Item %d: \"%s\"\n", ct, Items[ct].Text);
        ParseItemSlashAutoGet(ct);
        int location;
        if (fscanf(f, "%d", &location) != 1) {
            debug_print("Bad item line (%d)\n", ct);
            FreeDatabase();
            return UNKNOWN_GAME;
        }
        Items[ct].Location = (unsigned char)location;
        if (loud)
            debug_print("Location of item %d: %d\n", ct, Items[ct].Location);
        Items[ct].InitialLoc = Items[ct].Location;
    }
    /* Discard action comment strings (one per action, used only by
       the original authoring tools for documentation) */
    for (int ct = 0; ct <= GameHeader.NumActions; ct++)
        free(ReadString(f));
    /* Read the version and adventure number trailer */
    int val;
    if (fscanf(f, "%d", &val) != 1) {
        debug_print("Cannot read version\n");
        FreeDatabase();
        return UNKNOWN_GAME;
    }
    if (loud)
        debug_print("Version %d.%02d of Adventure ", val / 100, val % 100);
    if (fscanf(f, "%d", &val) != 1) {
        debug_print("Cannot read adventure number\n");
        FreeDatabase();
        return UNKNOWN_GAME;
    }
    if (loud)
        debug_print("%d.\nLoad Complete.\n\n", val);
    /* Extra value in at least Hulk */
    if (!fscanf(f, "%d", &val)) {
        debug_print("No extra value in file. This is not Hulk.\n");
    }

    fclose(f);
    if (val == HULK_DOS_ID && LoadDOSImages()) {
        CurrentSys = SYS_MSDOS;
        ImageWidth = HULK_DOS_WIDTH;
        ImageHeight = HULK_DOS_HEIGHT;
        return HULK_US;
    }
    return SCOTTFREE;
}

void Output(const char *a) { Display(Bottom, "%s", a); }


int RunTests(const char *supportpath) {
    return RunVectorTests(supportpath);
}

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
    { "-t", glkunix_arg_NoValue,
        "-t        Generate TRS80 style display (terminal width is 64 characters; "
        "a line <-----------------> is displayed after the top stuff; objects "
        "have periods after them instead of hyphens" },
    { "-p", glkunix_arg_NoValue,
        "-p        Use for prehistoric databases which don't use bit 16" },
    { "-w", glkunix_arg_NoValue, "-w        Disable upper window" },
    { "-n", glkunix_arg_NoValue, "-n        No delays" },
    { "", glkunix_arg_ValueFollows, "filename    file to load" },

    { NULL, glkunix_arg_End, NULL }
};

/* Parse command-line arguments (display style, debugging, delays, etc.)
   and extract the game file path and its parent directory. */
int glkunix_startup_code(glkunix_startup_t *data)
{
    int argc = data->argc;
    char **argv = data->argv;

    if (argc < 1)
        return 0;

    if (argc > 1)
        while (argv[1]) {
            if (*argv[1] != '-')
                break;
            switch (argv[1][1]) {
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
                Options |= TRS80_STYLE;
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
            case 'z':
                // Run the test suite RunTests() with the support file path as parameter,
                // call win_testresult() with the return value as parameter,
                // exit.
                fprintf(stderr, "Running tests\n");
                win_testresult(RunTests(argv[2]));
                glk_exit();
            }
            argv++;
            argc--;
        }

#ifdef GARGLK
    garglk_set_program_name("ScottFree 1.14");
    garglk_set_program_info("ScottFree 1.14 by Alan Cox\n"
                            "Glk port by Chris Spiegel\n");
#endif

    if (argc == 2) {
        game_file = argv[1];

#ifdef GARGLK
        const char *s;
        if ((s = strrchr(game_file, '/')) != NULL || (s = strrchr(game_file, '\\')) != NULL) {
            garglk_set_story_name(s + 1);
        } else {
            garglk_set_story_name(game_file);
        }
#endif

        if (game_file) {
            const char *n;
            int dirlen = 0;
            if ((n = strrchr(game_file, '/')) != NULL || (n = strrchr(game_file, '\\')) != NULL) {
                dirlen = (int)(n - game_file + 1);
            }
            if (dirlen) {
                DirPath = MemAlloc(dirlen + 1);
                memcpy(DirPath, game_file, dirlen);
                DirPath[dirlen] = 0;
            }
        }
    }

    return 1;
}

/* Main entry point: detect the game, configure display style, show the
   title screen, then run the main game loop (implicit actions → Look →
   get input → explicit actions → lamp timer). */
void glk_main(void)
{
    int vb, no;

    glk_stylehint_set(wintype_TextBuffer, style_User1, stylehint_Proportional, 0);
    glk_stylehint_set(wintype_TextBuffer, style_User1, stylehint_Indentation, 20);
    glk_stylehint_set(wintype_TextBuffer, style_User1, stylehint_ParaIndentation,
        20);
    glk_stylehint_set(wintype_TextBuffer, style_Preformatted, stylehint_Justification, stylehint_just_Centered);

    Bottom = glk_window_open(0, 0, 0, wintype_TextBuffer, GLK_BUFFER_ROCK);
    if (Bottom == NULL)
        glk_exit();
    glk_set_window(Bottom);

    if (game_file == NULL)
        Fatal("No game provided");

    /* Select "You are" vs "I am" system message set */
    const char **dictpointer;

    if (Options & YOUARE)
        dictpointer = sysdict;
    else
        dictpointer = sysdict_i_am;

    for (int i = 0; i < MAX_SYSMESS; i++) {
        if (dictpointer[i] != NULL)
            sys[i] = dictpointer[i];
        else
            sys[i] = sysdict[i];
    }

    /* Detect the game format and load its database */
    GameIDType game_type = DetectGame(game_file);

    if (game_type == UNKNOWN_GAME)
        Fatal("Unsupported game!");

    if (game_type != SCOTTFREE && game_type != TI994A) {
        Options |= SPECTRUM_STYLE;
        split_screen = 1;
    } else {
        if (game_type != TI994A)
            Options |= TRS80_STYLE;
        split_screen = 1;
    }

    if (title_screen != NULL && CurrentGame != RETURN_TO_PIRATES_ISLE) {
        if (split_screen)
            PrintTitleScreenGrid();
        else
            PrintTitleScreenBuffer();
    }

    if (Options & TRS80_STYLE) {
        TopWidth = 64;
        TopHeight = 11;
    } else {
        TopWidth = 80;
        TopHeight = 10;
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

            if (CurrentGame == RETURN_TO_PIRATES_ISLE)
                UpdateRTPISystemMessages();

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

#ifdef SPATTERLIGHT
    UpdateSettings();
    if (gli_determinism)
        set_erkyrath_random(DETERMINISTIC_SEED);
    else
#endif
    set_erkyrath_random(0);

    InitialState = SaveCurrentState();

    while (1) {
        glk_tick();

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
            continue;

        if (GetInput(&vb, &no) == 1)
            continue;

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
            // Failed actions should interrupt chains of commands
            // but not TAKE ALL and DROP ALL
            if (!CurrentCommand->allflag)
                FreeCommands();
            break;
        default:
            JustStarted = 0;
        }

        /* Decrement the lamp timer each turn. Brian Howarth games use -1
           for infinite light. Warn when below 25 turns; extinguish at 0. */
        if (Items[LIGHT_SOURCE].Location != DESTROYED && GameHeader.LightTime != -1 && !StopTime) {
            GameHeader.LightTime--;
            if (GameHeader.LightTime < 1) {
                SetBitFlag(LIGHTOUTBIT);
                if (Items[LIGHT_SOURCE].Location == CARRIED || Items[LIGHT_SOURCE].Location == MyLoc) {
                    Output(sys[LIGHT_HAS_RUN_OUT]);
                }
                if ((Options & PREHISTORIC_LAMP) || (Game->subtype & MYSTERIOUS))
                    Items[LIGHT_SOURCE].Location = DESTROYED;
            } else if (GameHeader.LightTime < LAMP_WARNING_THRESHOLD) {
                if (Items[LIGHT_SOURCE].Location == CARRIED || Items[LIGHT_SOURCE].Location == MyLoc) {
                    if ((Options & SCOTTLIGHT) || (Game->subtype & MYSTERIOUS)) {
                        Display(Bottom, "%s %d %s\n", sys[LIGHT_RUNS_OUT_IN], GameHeader.LightTime, sys[TURNS]);
                    } else {
                        if (GameHeader.LightTime % LAMP_WARNING_INTERVAL == 0)
                            Output(sys[LIGHT_GROWING_DIM]);
                    }
                }
            }
        }
        if (StopTime)
            StopTime--;
    }
}
