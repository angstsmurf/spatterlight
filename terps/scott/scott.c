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
#include <strings.h>

#include "glk.h"
#include "glkstart.h"
#include "irmak.h"
#include "saga.h"
#include "titleimage.h"

#include "detectgame.h"
#include "layouttext.h"
#include "line_drawing.h"
#include "restorestate.h"
#include "sagagraphics.h"
#include "vector_common.h"
#include "rtpi_graphics.h"

#include "parser.h"
#include "ti99_4a_terp.h"

#include "apple2draw.h"
#include "game_specific.h"
#include "gremlins.h"
#include "hulk.h"
#include "robinofsherwood.h"
#include "seasofblood.h"
#include "randomness.h"

#include "bsd.h"
#include "scott.h"

#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif

/* Number of exit directions per room (N/S/E/W/U/D) */
#define NUM_EXITS 6

/* Number of counter/room-flag save slots */
#define NUM_COUNTERS 16

/* Room image sentinel: no image assigned */
#define NO_IMAGE 255

/* Mask to extract the image index from a room/item image byte */
#define IMAGE_INDEX_MASK 127

/* Brian Howarth vector graphics format identifier */
#define HOWARTH_FORMAT 99

/* Action table encoding: vocab = verb * VOCAB_MULTIPLIER + noun,
   condition = param * CONDITION_MULTIPLIER + condition_code */
#define VOCAB_MULTIPLIER 150
#define CONDITION_MULTIPLIER 20
#define NUM_CONDITIONS 5
#define NUM_SUBCOMMANDS 4

/* Condition codes (0-19) — used in action table condition fields */
#define COND_PARAMETER       0
#define COND_CARRIED         1
#define COND_IN_ROOM         2
#define COND_PRESENT         3
#define COND_AT_LOC          4
#define COND_NOT_IN_ROOM     5
#define COND_NOT_CARRIED     6
#define COND_NOT_AT_LOC      7
#define COND_FLAG_SET        8
#define COND_FLAG_CLEAR      9
#define COND_CARRYING_ANY   10
#define COND_CARRYING_NONE  11
#define COND_NOT_PRESENT    12
#define COND_IN_PLAY        13
#define COND_NOT_IN_PLAY    14
#define COND_COUNTER_LE     15
#define COND_COUNTER_GT     16
#define COND_AT_INITIAL_LOC 17
#define COND_MOVED          18
#define COND_COUNTER_EQ     19

/* Subcommand message ranges: codes 1-51 print that message directly,
   codes 102+ print message (code - EXTENDED_MSG_OFFSET) */
#define FIRST_MESSAGE 1
#define LAST_DIRECT_MESSAGE 51
#define EXTENDED_MSG_BASE 102
#define EXTENDED_MSG_OFFSET 50

/* Subcommand opcodes (52-90) — game state operations */
#define CMD_GET             52
#define CMD_DROP_HERE       53
#define CMD_GOTO            54
#define CMD_DESTROY         55
#define CMD_SET_DARK        56
#define CMD_SET_LIGHT       57
#define CMD_SET_FLAG        58
#define CMD_DESTROY_2       59
#define CMD_CLEAR_FLAG      60
#define CMD_DIE             61
#define CMD_PUT_IN_ROOM     62
#define CMD_GAME_OVER       63
#define CMD_LOOK            64
#define CMD_SCORE           65
#define CMD_INVENTORY       66
#define CMD_SET_FLAG0       67
#define CMD_CLEAR_FLAG0     68
#define CMD_REFILL_LAMP     69
#define CMD_CLEAR_SCREEN    70
#define CMD_SAVE_GAME       71
#define CMD_SWAP_ITEMS      72
#define CMD_CONTINUE        73
#define CMD_FORCED_GET      74
#define CMD_MOVE_TO_LOC_OF  75
#define CMD_LOOK2           76
#define CMD_DEC_COUNTER     77
#define CMD_PRINT_COUNTER   78
#define CMD_SET_COUNTER     79
#define CMD_SWAP_LOC        80
#define CMD_SWAP_COUNTERS   81
#define CMD_ADD_COUNTER     82
#define CMD_SUB_COUNTER     83
#define CMD_PRINT_NOUN      84
#define CMD_PRINTLN_NOUN    85
#define CMD_NEWLINE         86
#define CMD_SWAP_ROOMFLAG   87
#define CMD_DELAY           88
#define CMD_GAME_SPECIFIC   89
#define CMD_DRAW_IMAGE      90

/* Display/buffer sizes */
#define DISPLAY_BUFFER_SIZE 2048
#define ROOM_DESC_BUFFER_SIZE 1000
#define READSTRING_BUFFER_SIZE 1024

/* Array sizes for Seas of Blood data */
#define NUM_SYSTEM_MESSAGES 60
#define NUM_BATTLE_MESSAGES 33
#define ENEMY_TABLE_SIZE 126

/* Lamp timer thresholds */
#define LAMP_WARNING_THRESHOLD 25
#define LAMP_WARNING_INTERVAL 5

/* Game-specific EXAMINE verb indices */
#define HULK_EXAMINE_VERB 39
#define COUNT_EXAMINE_VERB 8
#define VOODOO_EXAMINE_VERB 42
#define ADVENTURELAND_EXAMINE_VERB 29
#define PIRATE_EXAMINE_VERB 27
#define MISSION_EXAMINE_VERB 40
#define STRANGE_EXAMINE_VERB 37

/* Game-specific constants for workarounds */
#define SAVAGE_BEAR_ITEM 20
#define SAVAGE_BEACH_ROOM 8
#define SAVAGE_BEAR_IMAGE 9
#define RTPI_WIN_IMAGE 26
#define US_GAME_SPECIFIC_IMAGE 80
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
int Counters[NUM_COUNTERS] = { 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0 }; /* Range unknown */
int CurrentCounter;
int SavedRoom;
int RoomSaved[NUM_COUNTERS] = { 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0 }; /* Range unknown */

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
static int should_restart = 0;
int StopTime = 0; /* When > 0, suppresses automatic (implicit) actions and decrements each turn */

int should_look_in_transcript = 0;
static int print_look_to_transcript = 0;
static int pause_next_room_description = 0; /* Brief pause before next room description (for room transitions) */

int split_screen = 1;
winid_t Bottom, Top;
winid_t Graphics;

strid_t Transcript = NULL;

#define TRS80_LINE \
    "\n<------------------------------------------------------------>\n"

//#define DEBUG_ACTIONS

static void RestartGame(void);
static int YesOrNo(void);

static int PerformActions(int vb, int no);

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

/* Draw a decorative separator line at the bottom of the status window.
   Spectrum style uses asterisks; TRS-80 style uses <---...---> */
static void PrintWindowDelimiter(void)
{
    glk_window_get_size(Top, &TopWidth, &TopHeight);
    glk_window_move_cursor(Top, 0, TopHeight - 1);
    glk_stream_set_current(glk_window_get_stream(Top));
    if (Options & SPECTRUM_STYLE)
        for (unsigned i = 0; i < TopWidth; i++)
            glk_put_char('*');
    else {
        glk_put_char('<');
        for (unsigned i = 0; i < TopWidth - 2; i++)
            glk_put_char('-');
        glk_put_char('>');
    }
}

static strid_t room_description_stream = NULL;

/* Close the room description memory stream and render its contents.
   In split-screen mode, the text is line-broken to fit the top window;
   otherwise it's printed inline in the main window. Also handles
   transcript output, window delimiter drawing, and the inter-room pause. */
static void FlushRoomDescription(char *buf)
{
    glk_stream_close(room_description_stream, 0);

    if (Transcript && print_look_to_transcript) {
        glui32 *unistring = ToUnicode(buf);
        glk_put_string_stream_uni(Transcript, unistring);
        free(unistring);
    }

    int print_delimiter = (Options & (TRS80_STYLE | SPECTRUM_STYLE | TI994A_STYLE));

    if (split_screen) {
        glk_window_clear(Top);
        glk_window_get_size(Top, &TopWidth, &TopHeight);
        int rows, length;
        char *text_with_breaks = LineBreakText(buf, TopWidth, &rows, &length);

        glui32 bottomheight;
        glk_window_get_size(Bottom, NULL, &bottomheight);
        winid_t o2 = glk_window_get_parent(Top);
        if (!(bottomheight < 3 && TopHeight < rows)) {
            glk_window_get_size(Top, &TopWidth, &TopHeight);
            glk_window_set_arrangement(o2, winmethod_Above | winmethod_Fixed, rows,
                Top);
        } else {
            print_delimiter = 0;
        }

        int line = 0;
        int index = 0;
        int i;
        char string[DISPLAY_BUFFER_SIZE];
        if (TopWidth > DISPLAY_BUFFER_SIZE - 1)
            TopWidth = DISPLAY_BUFFER_SIZE - 1;
        for (line = 0; line < rows && index < length; line++) {
            for (i = 0; i < TopWidth; i++) {
                string[i] = text_with_breaks[index++];
                if (string[i] == '\n' || string[i] == '\r' || index >= length)
                    break;
            }
            if (i < TopWidth + 1) {
                string[i++] = '\n';
            }
            string[i] = 0;
            if (strlen(string) == 0)
                break;
            glk_window_move_cursor(Top, 0, line);
            Display(Top, "%s", string);
        }

        if (line < rows - 1) {
            glk_window_get_size(Top, &TopWidth, &TopHeight);
            glk_window_set_arrangement(o2, winmethod_Above | winmethod_Fixed,
                MIN(rows - 1, TopHeight - 1), Top);
        }

        free(text_with_breaks);
    } else {
        Display(Bottom, "%s", buf);
    }

    if (print_delimiter) {
        PrintWindowDelimiter();
    }

    if (pause_next_room_description) {
        Delay(0.8);
        pause_next_room_description = 0;
    }

    if (buf != NULL) {
        free(buf);
        buf = NULL;
    }
}

/* Render inventory into the upper window for US-variant games */
static void UpdateUSInventory(void)
{
    char *buf = MemAlloc(ROOM_DESC_BUFFER_SIZE);
    buf = memset(buf, 0, ROOM_DESC_BUFFER_SIZE);
    room_description_stream = glk_stream_open_memory(buf, ROOM_DESC_BUFFER_SIZE, filemode_Write, 0);
    ListInventory(1);
    FlushRoomDescription(buf);
    InventoryUS();
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

/* Pause for a given duration using Glk timer events.
   If vector drawing is in progress, waits for it to finish first. */
void Delay(float seconds)
{
    if (Options & NO_DELAYS)
        return;

    event_t ev;

    if (!glk_gestalt(gestalt_Timer, 0))
        return;

    glk_request_char_event(Bottom);
    glk_cancel_char_event(Bottom);

    if (DrawingVector()) {
        do {
            glk_select(&ev);
            Updates(ev);
        } while (DrawingVector());
#ifdef SPATTERLIGHT
        if (gli_slowdraw)
            seconds = 0.5;
#endif
    }

    glk_request_timer_events(1000 * seconds);

    do {
        glk_select(&ev);
        Updates(ev);
    } while (ev.type != evtype_Timer);

    glk_request_timer_events(0);
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

static void ClearScreen(void)
{
    glk_window_clear(Bottom);
}

int RandomPercent(int n)
{
    return erkyrath_random() % 100 < n;
}

int CountCarried(void)
{
    int ct = 0;
    int n = 0;
    while (ct <= GameHeader.NumItems) {
        if (Items[ct].Location == CARRIED)
            n++;
        ct++;
    }
    return (n);
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
static int MatchUpItem(int noun, int loc)
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

//static int SanityCheckScottFreeHeader(int ni, int na, int nw, int nr, int mc)
//{
//    int16_t v = header[1]; // items
//    if (v < 10 || v > 500)
//        return 0;
//    v = header[2]; // actions
//    if (v < 100 || v > 500)
//        return 0;
//    v = header[3]; // word pairs
//    if (v < 50 || v > 200)
//        return 0;
//    v = header[4]; // Number of rooms
//    if (v < 10 || v > 100)
//        return 0;
//    v = header[5]; // Number of Messages
//    if (v < 10 || v > 255)
//        return 0;
//
//    return 1;
//}

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
                &Actions[ct].Subcommand[0],
                &Actions[ct].Subcommand[1])
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
            debug_print("Action %d Subcommand [0]]: %d (%d/%d)\n", ct, Actions[ct].Subcommand[0], Actions[ct].Subcommand[0] % VOCAB_MULTIPLIER, Actions[ct].Subcommand[0] / VOCAB_MULTIPLIER);
            debug_print("Action %d Subcommand [1]]: %d (%d/%d)\n", ct, Actions[ct].Subcommand[1], Actions[ct].Subcommand[1] % VOCAB_MULTIPLIER, Actions[ct].Subcommand[1] / VOCAB_MULTIPLIER);
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

    /* Load messages (printed by action subcommands 1-51 and 102+) */
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

void OutputNumber(int a) { Display(Bottom, "%d", a); }

#if defined(__clang__)
#pragma mark Room description
#endif

static int ItIsDark(void)
{
    return ((BitFlags & (1 << DARKBIT)) && Items[LIGHT_SOURCE].Location != CARRIED && Items[LIGHT_SOURCE].Location != MyLoc);
}

void DrawBlack(void)
{
    glk_window_fill_rect(Graphics, 0, x_offset, 0, 32 * 8 * (glui32)pixel_size,
        12 * 8 * (glui32)pixel_size);
}

/* Draw a game image using the appropriate rendering method:
   Howarth vector graphics (format 99) or bitmap picture number. */
void DrawImage(int image)
{
#ifdef SPATTERLIGHT
    if (!gli_enable_graphics)
        return;
#endif
    OpenGraphicsWindow();
    if (Graphics == NULL) {
        debug_print("DrawImage: Graphic window NULL?\n");
        return;
    }
    if (Game->picture_format_version == HOWARTH_FORMAT)
        DrawHowarthVectorPicture(image);
    else
        DrawPictureNumber(image, (Game->type == SEAS_OF_BLOOD_VARIANT));
}

/* Draw the image for the current room, with special handling for
   darkness (black screen or fuzzy image), US-variant layout, and
   game-specific overrides (Seas of Blood, Robin of Sherwood, Hulk,
   Gremlins, Savage Island). Also draws visible item overlays. */
void DrawRoomImage(void)
{
    if (CurrentGame == ADVENTURELAND || CurrentGame == ADVENTURELAND_C64) {
        AdventurelandDarkness();
    }

    int dark = ItIsDark();

    if (dark && Graphics != NULL && (Rooms[MyLoc].Image != NO_IMAGE || Game->type == US_VARIANT)) {
        vector_image_shown = -1;
        VectorState = NO_VECTOR_IMAGE;
        glk_request_timer_events(0);
        if (Game->type == US_VARIANT) {
            glk_window_clear(Graphics);
            if (CurrentGame == RETURN_TO_PIRATES_ISLE) {
                if (!DrawFuzzyRoom(MyLoc)) {
                    ClearVDP();
                }
            } else {
                DrawUSRoom(0);
            }
            DrawImageOrVector();
        } else {
            DrawBlack();
        }
        return;
    }

    if (Game->type == US_VARIANT) {
        LookUS();
        return;
    }

    switch (CurrentGame) {
    case SEAS_OF_BLOOD:
    case SEAS_OF_BLOOD_C64:
        SeasOfBloodRoomImage();
        return;
    case ROBIN_OF_SHERWOOD:
    case ROBIN_OF_SHERWOOD_C64:
        RobinOfSherwoodLook();
        return;
    case HULK:
    case HULK_C64:
        HulkLook();
        return;
    default:
        break;
    }

    if (Rooms[MyLoc].Image == NO_IMAGE) {
        CloseGraphicsWindow();
        return;
    }

    if (dark)
        return;

    if (Game->picture_format_version == HOWARTH_FORMAT) {
        DrawImage(MyLoc - 1);
        return;
    }

    if (Game->type == GREMLINS_VARIANT) {
        GremlinsLook();
    } else {
        DrawImage(Rooms[MyLoc].Image & IMAGE_INDEX_MASK);
    }
    for (int ct = 0; ct <= GameHeader.NumItems; ct++)
        if (Items[ct].Image && Items[ct].Location == MyLoc) {
            if ((Items[ct].Flag & IMAGE_INDEX_MASK) == MyLoc) {
                DrawImage(Items[ct].Image);
                /* Draw the correct image of the bear on the beach */
            } else if (Game->type == SAVAGE_ISLAND_VARIANT && ct == SAVAGE_BEAR_ITEM && MyLoc == SAVAGE_BEACH_ROOM) {
                DrawImage(SAVAGE_BEAR_IMAGE);
            }
        }
}

static void WriteToRoomDescriptionStream(const char *fmt, ...)
#ifdef __GNUC__
    __attribute__((__format__(__printf__, 1, 2)))
#endif
    ;

static void WriteToRoomDescriptionStream(const char *fmt, ...)
{
    if (room_description_stream == NULL)
        return;
    va_list ap;
    char msg[DISPLAY_BUFFER_SIZE];

    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);

    glk_put_string_stream(room_description_stream, msg);
}

/* List room exits in ZX Spectrum style (only shown if exits exist) */
static void ListExitsSpectrumStyle(void)
{
    int ct = 0;
    int f = 0;

    while (ct < NUM_EXITS) {
        if ((&Rooms[MyLoc])->Exits[ct] != 0) {
            if (f == 0) {
                if (!(Options & PC_STYLE))
                    WriteToRoomDescriptionStream("\n\n");
                WriteToRoomDescriptionStream("%s", sys[EXITS]);
            } else {
                WriteToRoomDescriptionStream("%s", sys[EXITS_DELIMITER]);
            }
            /* sys[] begins with the exit names */
            WriteToRoomDescriptionStream("%s", sys[ct]);
            f = 1;
        }
        ct++;
    }
    WriteToRoomDescriptionStream("\n");
    return;
}

/* List room exits in standard format (always shows header, "None" if empty) */
static void ListExits(void)
{
    int ct = 0;
    int f = 0;

    WriteToRoomDescriptionStream("\n\n%s", sys[EXITS]);

    while (ct < NUM_EXITS) {
        if ((&Rooms[MyLoc])->Exits[ct] != 0) {
            if (f) {
                WriteToRoomDescriptionStream("%s", sys[EXITS_DELIMITER]);
            }
            /* sys[] begins with the exit names */
            WriteToRoomDescriptionStream("%s", sys[ct]);
            f = 1;
        }
        ct++;
    }
    if (f == 0)
        WriteToRoomDescriptionStream("%s", sys[NONE]);
    return;
}

static int ItemEndsWithPeriod(int item)
{
    if (item < 0 || item > GameHeader.NumItems)
        return 0;
    const char *desc = Items[item].Text;
    if (desc != NULL && desc[0] != 0) {
        const char lastchar = desc[strlen(desc) - 1];
        if (lastchar == '.' || lastchar == '!') {
            return 1;
        }
    }
    return 0;
}

/* Build and display the full room description: room image, room text,
   exits, visible items, and optionally the inventory. The description
   is assembled in a memory stream, then flushed to the appropriate window. */
void Look(void)
{
    showing_inventory = 0;
    DrawRoomImage();

    if (split_screen && Top == NULL)
        return;

    char *buf = MemAlloc(ROOM_DESC_BUFFER_SIZE);
    buf = memset(buf, 0, ROOM_DESC_BUFFER_SIZE);
    room_description_stream = glk_stream_open_memory(buf, ROOM_DESC_BUFFER_SIZE, filemode_Write, 0);

    Room *r;
    int ct, f;

    if (!split_screen) {
        WriteToRoomDescriptionStream("\n");
    } else if (Transcript && print_look_to_transcript) {
        glk_put_char_stream_uni(Transcript, '\n');
    }

    if (ItIsDark()) {
        WriteToRoomDescriptionStream("%s", sys[TOO_DARK_TO_SEE]);
        FlushRoomDescription(buf);
        return;
    }

    r = &Rooms[MyLoc];

    if (!r->Text)
        return;
    /* An initial asterisk means the room description should not */
    /* start with "You are" or equivalent */
    if (*r->Text == '*') {
        WriteToRoomDescriptionStream("%s", r->Text + 1);
    } else {
        WriteToRoomDescriptionStream("%s%s", sys[YOU_ARE], r->Text);
    }

    if (!(Options & SPECTRUM_STYLE)) {
        ListExits();
        WriteToRoomDescriptionStream(".\n");
    }

    ct = 0;
    f = 0;
    while (ct <= GameHeader.NumItems) {
        if (Items[ct].Location == MyLoc) {
            if (Items[ct].Text[0] == 0) {
                fprintf(stderr, "Invisible item in room: %d\n", ct);
                ct++;
                continue;
            }
            if (f == 0) {
                WriteToRoomDescriptionStream("%s", sys[YOU_SEE]);
                f++;
                if (Options & SPECTRUM_STYLE && !(Options & PC_STYLE))
                    WriteToRoomDescriptionStream("\n");
            } else if (!(Options & (TRS80_STYLE | SPECTRUM_STYLE))) {
                WriteToRoomDescriptionStream("%s", sys[ITEM_DELIMITER]);
            }
            WriteToRoomDescriptionStream("%s", Items[ct].Text);
            if (Options & (TRS80_STYLE | SPECTRUM_STYLE)) {
                WriteToRoomDescriptionStream("%s", sys[ITEM_DELIMITER]);
            }
        }
        ct++;
    }

    if ((Options & TI994A_STYLE) && f) {
        WriteToRoomDescriptionStream(".");
    } else if (Options & PC_STYLE && !f)
        WriteToRoomDescriptionStream(". ");

    if (Options & SPECTRUM_STYLE) {
        ListExitsSpectrumStyle();
    } else if (f) {
        WriteToRoomDescriptionStream("\n");
    }

    if ((AutoInventory || (Options & FORCE_INVENTORY)) && !(Options & FORCE_INVENTORY_OFF)) {
        WriteToRoomDescriptionStream("\n");
        ListInventory(1);
    }

    FlushRoomDescription(buf);
}

/* Save the current game state to a file via Glk file prompts.
   Writes counters, room flags, bit flags, location, and item positions. */
void SaveGame(void)
{
    strid_t file;
    frefid_t ref;
    int ct;
    char buf[128];

    ref = glk_fileref_create_by_prompt(fileusage_TextMode | fileusage_SavedGame,
        filemode_Write, 0);
    if (ref == NULL)
        return;

    file = glk_stream_open_file(ref, filemode_Write, 0);
    glk_fileref_destroy(ref);
    if (file == NULL)
        return;

    for (ct = 0; ct < NUM_COUNTERS; ct++) {
        snprintf(buf, sizeof buf, "%d %d\n", Counters[ct], RoomSaved[ct]);
        glk_put_string_stream(file, buf);
    }
    snprintf(buf, sizeof buf, "%ld %d %hd %d %d %hd %d\n", BitFlags,
        (BitFlags & (1 << DARKBIT)) ? 1 : 0, MyLoc, CurrentCounter,
        SavedRoom, GameHeader.LightTime, AutoInventory);
    glk_put_string_stream(file, buf);
    for (ct = 0; ct <= GameHeader.NumItems; ct++) {
        snprintf(buf, sizeof buf, "%hd\n", (short)Items[ct].Location);
        glk_put_string_stream(file, buf);
    }

    glk_stream_close(file, NULL);
    Output(sys[SAVED]);
}

/* Load a saved game state from a file via Glk file prompts.
   Validates all loaded values against the current database limits
   and rolls back to a snapshot if any value is out of range. */
static void LoadGame(void)
{
    strid_t file;
    frefid_t ref;
    char buf[128];
    int ct = 0;
    short lo;
    short DarkFlag;

    int PreviousAutoInventory = AutoInventory;

    ref = glk_fileref_create_by_prompt(fileusage_TextMode | fileusage_SavedGame,
        filemode_Read, 0);
    if (ref == NULL)
        return;

    file = glk_stream_open_file(ref, filemode_Read, 0);
    glk_fileref_destroy(ref);
    if (file == NULL)
        return;

    SavedState *state = SaveCurrentState();

    int result;

    for (ct = 0; ct < NUM_COUNTERS; ct++) {
        glk_get_line_stream(file, buf, sizeof buf);
        result = sscanf(buf, "%d %d", &Counters[ct], &RoomSaved[ct]);
        if (result != 2 || RoomSaved[ct] > GameHeader.NumRooms) {
            RecoverFromBadRestore(state);
            return;
        }
    }
    glk_get_line_stream(file, buf, sizeof buf);
    result = sscanf(buf, "%ld %hd %hd %d %d %hd %d\n", &BitFlags, &DarkFlag,
        &MyLoc, &CurrentCounter, &SavedRoom, &GameHeader.LightTime,
        &AutoInventory);
    if (result == 6)
        AutoInventory = PreviousAutoInventory;
    if ((result != 7 && result != 6) || MyLoc > GameHeader.NumRooms || MyLoc < 1 || SavedRoom > GameHeader.NumRooms) {
        RecoverFromBadRestore(state);
        return;
    }

    /* Backward compatibility */
    if (DarkFlag)
        SetBitFlag(DARKBIT);
    for (ct = 0; ct <= GameHeader.NumItems; ct++) {
        glk_get_line_stream(file, buf, sizeof buf);
        result = sscanf(buf, "%hd\n", &lo);
        Items[ct].Location = (unsigned char)lo;
        if (result != 1 || (Items[ct].Location > GameHeader.NumRooms && Items[ct].Location != CARRIED)) {
            RecoverFromBadRestore(state);
            return;
        }
    }

    glui32 position = glk_stream_get_position(file);
    glk_stream_set_position(file, 0, seekmode_End);
    glui32 end = glk_stream_get_position(file);
    if (end != position) {
        RecoverFromBadRestore(state);
        return;
    }

    SaveUndo();
    JustStarted = 0;
    StopTime = 1;
    should_look_in_transcript = should_draw_image =  1;
}

static void RestartGame(void)
{
    if (CurrentCommand)
        FreeCommands();
    RestoreState(InitialState);
    JustStarted = 0;
    StopTime = 0;
    Display(Bottom, "\n\n");
    glk_window_clear(Bottom);
    OpenTopWindow();
    should_restart = 0;
}

static void TranscriptOn(void)
{
    frefid_t ref;

    if (Transcript) {
        Output(sys[TRANSCRIPT_ALREADY]);
        return;
    }

    ref = glk_fileref_create_by_prompt(fileusage_TextMode | fileusage_Transcript,
        filemode_Write, 0);
    if (ref == NULL)
        return;

    Transcript = glk_stream_open_file_uni(ref, filemode_Write, 0);

    glk_fileref_destroy(ref);

    if (Transcript == NULL) {
        Output(sys[FAILED_TRANSCRIPT]);
        return;
    }

    glui32 *start_of_transcript = ToUnicode(sys[TRANSCRIPT_START]);
    glk_put_string_stream_uni(Transcript, start_of_transcript);
    free(start_of_transcript);

    print_look_to_transcript = 1;
    Look();

    glk_put_string_stream(glk_window_get_stream(Bottom),
        (char *)sys[TRANSCRIPT_ON]);
    glk_window_set_echo_stream(Bottom, Transcript);
}

static void TranscriptOff(void)
{
    if (Transcript == NULL) {
        Output(sys[NO_TRANSCRIPT]);
        return;
    }

    glk_window_set_echo_stream(Bottom, NULL);

    glui32 *end_of_transcript = ToUnicode(sys[TRANSCRIPT_END]);
    glk_put_string_stream_uni(Transcript, end_of_transcript);
    free(end_of_transcript);

    glk_stream_close(Transcript, NULL);
    Transcript = NULL;
    Output(sys[TRANSCRIPT_OFF]);
}

static void FlickerOn(void)
{
    if (Options & FLICKER_ON) {
        Output("Flicker is already on");
    } else {
        Output("Flicker is now on");
    }
    if (Options & NO_DELAYS)
        Output(" (but delays are off, so it won't have any effect.)");
    else Output(".");
    Output("\n");
    Options |= FLICKER_ON;
}

static void FlickerOff(void)
{
    if (Options & FLICKER_ON) {
        Output("Flicker is now off.");
    } else {
        Output("Flicker is already off.");
    }
    Options &= ~FLICKER_ON;
}

/* Handle meta-commands that aren't part of the game's action table:
   save, restore, restart, undo, RAM save/load, transcript, and flicker.
   Returns 1 if the command was handled, 0 otherwise. */
int PerformExtraCommand(int extra_stop_time)
{
    Command command = *CurrentCommand;
    int verb = command.verb;
    if (verb > GameHeader.NumWords)
        verb -= GameHeader.NumWords;
    int noun = command.noun;
    if (noun > GameHeader.NumWords)
        noun -= GameHeader.NumWords;
    else if (noun) {
        const char *NounWord = CharWords[CurrentCommand->nounwordindex];
        int newnoun = WhichWord(NounWord, ExtraNouns, strlen(NounWord),
            NUMBER_OF_EXTRA_NOUNS);
        newnoun = ExtraNounsKey[newnoun];
        if (newnoun)
            noun = newnoun;
    }

    StopTime = 1 + extra_stop_time;

    switch (verb) {
    case RESTORE:
        if (noun == 0 || noun == GAME) {
            LoadGame();
            return 1;
        }
        break;
    case RESTART:
        if (noun == 0 || noun == GAME) {
            Output(sys[ARE_YOU_SURE]);
            if (YesOrNo()) {
                should_restart = 1;
            }
            return 1;
        }
        break;
    case SAVE:
        if (noun == 0 || noun == GAME) {
            SaveGame();
            return 1;
        }
        break;
    case UNDO:
        if (noun == 0 || noun == COMMAND) {
            RestoreUndo();
            return 1;
        }
        break;
    case RAM:
        if (noun == RAMLOAD) {
            RamRestore();
            return 1;
        } else if (noun == RAMSAVE) {
            RamSave();
            return 1;
        }
        break;
    case RAMSAVE:
        if (noun == 0) {
            RamSave();
            return 1;
        }
        break;
    case RAMLOAD:
        if (noun == 0) {
            RamRestore();
            return 1;
        }
        break;
    case SCRIPT:
        if (noun == ON || noun == 0) {
            TranscriptOn();
            return 1;
        } else if (noun == OFF) {
            TranscriptOff();
            return 1;
        }
        break;
    case EXCEPT:
        FreeCommands();
    case FLICKER:
        if (noun == ON || noun == 0) {
            FlickerOn();
            return 1;
        } else if (noun == OFF) {
            FlickerOff();
            return 1;
        }
        break;
    }

    StopTime = 0;
    return 0;
}

/* Prompt for a yes/no response via single-character input.
   Uses the localized first characters of sys[YES] and sys[NO]. */
static int YesOrNo(void)
{
    glk_request_char_event_uni(Bottom);

    event_t ev;
    int result = 0;
    const char y = tolower((unsigned char)sys[YES][0]);
    const char n = tolower((unsigned char)sys[NO][0]);

    do {
        glk_select(&ev);
        if (ev.type == evtype_CharInput) {
            if ((glsi32)ev.val1 > ' ') {
                glk_put_char_stream_uni(glk_window_get_stream(Bottom), ev.val1);
            }
            const char reply = tolower((char)ev.val1);
            if (reply == y) {
                result = 1;
            } else if (reply == n) {
                result = 2;
            } else {
                Output("\n");
                Output(sys[ANSWER_YES_OR_NO]);
                glk_request_char_event_uni(Bottom);
            }
        } else
            Updates(ev);
    } while (result == 0);

    return (result == 1);
}

void HitEnter(void)
{
    glk_request_char_event(Bottom);

    event_t ev;
    int result = 0;
    do {
        glk_select(&ev);
        if (ev.type == evtype_CharInput) {
            if (ev.val1 == keycode_Return) {
                result = 1;
            } else {
                glk_request_char_event(Bottom);
            }
        } else
            Updates(ev);
    } while (result == 0);
    showing_closeup = 0;
    should_draw_image = 1;
    return;
}

static void WriteToLowerWindow(const char *fmt, ...)
{
    va_list ap;
    char msg[DISPLAY_BUFFER_SIZE];

    int size = sizeof msg;

    va_start(ap, fmt);
    vsnprintf(msg, size, fmt, ap);
    va_end(ap);

    glui32 *unistring = ToUnicode(msg);
    glk_put_string_stream_uni(glk_window_get_stream(Bottom), unistring);
    free(unistring);
}

/* List the player's carried items. When upper=1, writes to the room
   description stream (status window); otherwise writes to the main window. */
void ListInventory(int upper)
{
    void (*print_function)(const char *fmt, ...);
    if (upper) {
        print_function = WriteToRoomDescriptionStream;
    } else {
        print_function = WriteToLowerWindow;
    }

    int i = 0;
    int lastitem = -1;
    print_function("%s", sys[INVENTORY]);
    while (i <= GameHeader.NumItems) {
        if (Items[i].Location == CARRIED) {
            if (Items[i].Text[0] == 0) {
                fprintf(stderr, "Invisible item in inventory: %d\n", i);
                i++;
                continue;
            }
            if (lastitem > -1 && (Options & (TRS80_STYLE | SPECTRUM_STYLE)) == 0) {
                print_function("%s", sys[ITEM_DELIMITER]);
            }
            lastitem = i;
            print_function("%s", Items[i].Text);
            if (Options & (TRS80_STYLE | SPECTRUM_STYLE)) {
                print_function("%s", sys[ITEM_DELIMITER]);
            }
        }
        i++;
    }
    if (lastitem == -1)
        print_function("%s", sys[NOTHING]);
    else if (Options & TI994A_STYLE) {
        if (!ItemEndsWithPeriod(lastitem))
            print_function(".");
        print_function(" ");
    }
    if (upper) {
        WriteToRoomDescriptionStream("\n");
    } else if (Transcript) {
        glk_put_char_stream_uni(Transcript, '\n');
    }
}

/* Perform a Look with a brief pause, used during multi-command
   sequences so the player can see intermediate room descriptions. */
static void LookWithPause(void)
{
    char fc = Rooms[MyLoc].Text[0];
    if (Rooms[MyLoc].Text == NULL || MyLoc == 0 || fc == 0 || fc == '.' || fc == ' ')
        return;
    should_look_in_transcript = should_draw_image = 1;
    pause_next_room_description = 1;
    Look();
}

void DoneIt(void)
{
    if (split_screen && Top)
        Look();
    Output("\n\n");
    Output(sys[PLAY_AGAIN]);
    Output("\n");
    if (YesOrNo()) {
        should_restart = 1;
    } else {
        CleanupAndExit();
    }
}

/* Count and display treasures stored in the treasure room.
   If all treasures are found, triggers the victory sequence. */
int PrintScore(void)
{
    int i = 0;
    int n = 0;
    while (i <= GameHeader.NumItems) {
        if (Items[i].Location == GameHeader.TreasureRoom && *Items[i].Text == '*')
            n++;
        i++;
    }
    Display(Bottom, "%s %d %s%s %d.\n", sys[IVE_STORED], n, sys[TREASURES],
        sys[ON_A_SCALE_THAT_RATES], (n * 100) / GameHeader.Treasures);
    if (n == GameHeader.Treasures) {
        Output(sys[YOUVE_SOLVED_IT]);
        if (CurrentGame == RETURN_TO_PIRATES_ISLE) {
            ShowUSCloseup(RTPI_WIN_IMAGE, 0);
        }
        DoneIt();
        return 1;
    }
    return 0;
}

void PrintNoun(void)
{
    if (CurrentCommand)
        glk_put_string_stream_uni(glk_window_get_stream(Bottom),
            UnicodeWords[CurrentCommand->nounwordindex]);
}

void MoveItemAToLocOfItemB(int itemA, int itemB)
{
    Items[itemA].Location = Items[itemB].Location;
    if (Items[itemB].Location == MyLoc) {
        should_look_in_transcript = 1;
        DrawUSRoomObject(itemB);
        DrawImageOrVector();
    }
}

void GoTo(int loc)
{
#ifdef DEBUG_ACTIONS
    debug_print("player location is now room %d (%s).\n", loc,
                Rooms[loc].Text);
#endif
    int oldloc = MyLoc;
    MyLoc = loc;
    should_look_in_transcript = should_draw_image = 1;
    Look();
    if (oldloc != MyLoc && (Options & FLICKER_ON))
        Delay(0.2);
}

void GoToStoredLoc(void)
{
#ifdef DEBUG_ACTIONS
    debug_print("switch location to stored location (%d) (%s).\n",
        SavedRoom, Rooms[SavedRoom].Text);
#endif
    int t = MyLoc;
    MyLoc = SavedRoom;
    SavedRoom = t;
    should_look_in_transcript = should_draw_image = 1;
}

void SetBitFlag(int bit) {
#ifdef DEBUG_ACTIONS
    debug_print("Bitflag %d is set\n", bit);
#endif
    BitFlags |= (uint64_t)1 << bit;
}

void ClearBitFlag(int bit) {
#ifdef DEBUG_ACTIONS
    debug_print("Bitflag %d is cleared\n", bit);
#endif
    BitFlags &= ~((uint64_t)1 << bit);
}

/* Set or clear the darkness flag, tracking whether the lighting
   state actually changed so the display is only refreshed when needed. */
static void ChangeDarkness(int dark) {
    int was_dark = ItIsDark();
    if (dark) {
        SetBitFlag(DARKBIT);
        should_look_in_transcript = (should_look_in_transcript == 1 || was_dark == 0);
    } else {
        ClearBitFlag(DARKBIT);
        should_look_in_transcript = (should_look_in_transcript == 1 || was_dark == 1);
    }
    if (should_draw_image == 0) {
        should_draw_image = should_look_in_transcript;
    }
}

void SetDark(void) {
    ChangeDarkness(1);
}

void SetLight(void) {
    ChangeDarkness(0);
}

void SwapLocAndRoomflag(int index)
{
#ifdef DEBUG_ACTIONS
    debug_print("swap location<->roomflag[%d]\n", index);
#endif
    int temp = MyLoc;
    MyLoc = RoomSaved[index];
    RoomSaved[index] = temp;
    should_look_in_transcript = should_draw_image = 1;
    Look();
}

void SwapItemLocations(int itemA, int itemB)
{
    int temp = Items[itemA].Location;
    Items[itemA].Location = Items[itemB].Location;
    Items[itemB].Location = temp;
    if (Items[itemA].Location == MyLoc || Items[itemB].Location == MyLoc)
        should_look_in_transcript = should_draw_image = 1;
}

void PutItemAInRoomB(int itemA, int roomB)
{
#ifdef DEBUG_ACTIONS
    debug_print("Item %d (%s) is put in room %d (%s). MyLoc: %d (%s)\n",
        itemA, Items[itemA].Text, roomB, Rooms[roomB].Text, MyLoc,
        Rooms[MyLoc].Text);
#endif
    if (Items[itemA].Location == MyLoc)
        LookWithPause();
    Items[itemA].Location = roomB;
}

void SwapCounters(int index)
{
#ifdef DEBUG_ACTIONS
    debug_print(
        "Select a counter. Current counter is swapped with backup "
        "counter %d\n",
        index);
#endif
    if (index > NUM_COUNTERS - 1) {
        debug_print("ERROR! parameter out of range. Max %d, got %d\n", NUM_COUNTERS - 1, index);
        index = NUM_COUNTERS - 1;
    }
    int temp = CurrentCounter;

    CurrentCounter = Counters[index];
    Counters[index] = temp;
#ifdef DEBUG_ACTIONS
    debug_print("Value of new selected counter is %d\n",
        CurrentCounter);
#endif
}

void PrintMessage(int index)
{
#ifdef DEBUG_ACTIONS
    debug_print("Print message %d: \"%s\"\n", index,
        Messages[index]);
#endif
    const char *message = Messages[index];
    if (message != NULL && message[0] != 0) {
        Output(message);
        const char lastchar = message[strlen(message) - 1];
        if (lastchar != '\r' && lastchar != '\n')
            Output(sys[MESSAGE_DELIMITER]);
    }
}

void PlayerIsDead(void)
{
#ifdef DEBUG_ACTIONS
    debug_print("Player is dead\n");
#endif
    Output(sys[IM_DEAD]);
    SetLight();
    should_look_in_transcript = should_draw_image = 1;
    MyLoc = GameHeader.NumRooms; /* It seems to be what the code says! */
}

/* Evaluate one action line: check up to 5 conditions, and if all pass,
   execute the 4 subcommands encoded in the action's two Subcommand words.

   Conditions are encoded as (value * 20 + condition_code). Condition
   code 0 stores a parameter for later use by subcommands. Subcommands
   1-51 and 102+ print messages; 52-90 are game state operations.

   Returns ACT_FAILURE if a condition fails, ACT_CONTINUE for action 73
   (continue to next line), ACT_GAMEOVER on game-ending actions, or
   ACT_SUCCESS if all subcommands completed normally. */
static ActionResultType PerformLine(int ct)
{
#ifdef DEBUG_ACTIONS
    debug_print("Performing line %d: ", ct);
#endif
    int continuation = 0, dead = 0;
    int param[NUM_CONDITIONS], pptr = 0;
    int p = 0;
    int act[NUM_SUBCOMMANDS];
    int cc = 0;
    while (cc < NUM_CONDITIONS) {
        int cv, dv;
        cv = Actions[ct].Condition[cc];
        dv = cv / CONDITION_MULTIPLIER;
        cv %= CONDITION_MULTIPLIER;
#ifdef DEBUG_ACTIONS
        debug_print("Testing condition %d: ", cv);
#endif
        switch (cv) {
        case COND_PARAMETER:
            param[pptr++] = dv;
            break;
        case COND_CARRIED:
#ifdef DEBUG_ACTIONS
            debug_print("Does the player carry %s?\n", Items[dv].Text);
#endif
            if (Items[dv].Location != CARRIED)
                return ACT_FAILURE;
            break;
        case COND_IN_ROOM:
#ifdef DEBUG_ACTIONS
            debug_print("Is %s in location?\n", Items[dv].Text);
#endif
            if (Items[dv].Location != MyLoc)
                return ACT_FAILURE;
            break;
        case COND_PRESENT:
#ifdef DEBUG_ACTIONS
            debug_print("Is %s held or in location?\n", Items[dv].Text);
#endif
            if (Items[dv].Location != CARRIED && Items[dv].Location != MyLoc)
                return ACT_FAILURE;
            break;
        case COND_AT_LOC:
#ifdef DEBUG_ACTIONS
            debug_print("Is location %s?\n", Rooms[dv].Text);
#endif
            if (MyLoc != dv)
                return ACT_FAILURE;
            break;
        case COND_NOT_IN_ROOM:
#ifdef DEBUG_ACTIONS
            debug_print("Is %s NOT in location?\n", Items[dv].Text);
#endif
            if (Items[dv].Location == MyLoc)
                return ACT_FAILURE;
            break;
        case COND_NOT_CARRIED:
#ifdef DEBUG_ACTIONS
            debug_print("Does the player NOT carry %s?\n", Items[dv].Text);
#endif
            if (Items[dv].Location == CARRIED)
                return ACT_FAILURE;
            break;
        case COND_NOT_AT_LOC:
#ifdef DEBUG_ACTIONS
            debug_print("Is location NOT %s?\n", Rooms[dv].Text);
#endif
            if (MyLoc == dv)
                return ACT_FAILURE;
            break;
        case COND_FLAG_SET:
#ifdef DEBUG_ACTIONS
            debug_print("Is bitflag %d set?\n", dv);
#endif
            if ((BitFlags & ((uint64_t)1 << dv)) == 0)
                return ACT_FAILURE;
            break;
        case COND_FLAG_CLEAR:
#ifdef DEBUG_ACTIONS
            debug_print("Is bitflag %d NOT set?\n", dv);
#endif
            if (BitFlags & ((uint64_t)1 << dv))
                return ACT_FAILURE;
            break;
        case COND_CARRYING_ANY:
#ifdef DEBUG_ACTIONS
            debug_print("Does the player carry anything?\n");
#endif
            if (CountCarried() == 0)
                return ACT_FAILURE;
            break;
        case COND_CARRYING_NONE:
#ifdef DEBUG_ACTIONS
            debug_print("Does the player carry nothing?\n");
#endif
            if (CountCarried())
                return ACT_FAILURE;
            break;
        case COND_NOT_PRESENT:
#ifdef DEBUG_ACTIONS
            debug_print("Is %s neither carried nor in room?\n", Items[dv].Text);
#endif
            if (Items[dv].Location == CARRIED || Items[dv].Location == MyLoc)
                return ACT_FAILURE;
            break;
        case COND_IN_PLAY:
            if (dv > GameHeader.NumItems + 1)
                Fatal("Broken database!");
#ifdef DEBUG_ACTIONS
            debug_print("Is %s (%d) in play?\n", Items[dv].Text, dv);
#endif
            if (Items[dv].Location == 0)
                return ACT_FAILURE;
            break;
        case COND_NOT_IN_PLAY:
#ifdef DEBUG_ACTIONS
            debug_print("Is %s NOT in play?\n", Items[dv].Text);
#endif
            if (Items[dv].Location)
                return ACT_FAILURE;
            break;
        case COND_COUNTER_LE:
#ifdef DEBUG_ACTIONS
            debug_print("Is CurrentCounter <= %d?\n", dv);
#endif
            if (CurrentCounter > dv)
                return ACT_FAILURE;
            break;
        case COND_COUNTER_GT:
#ifdef DEBUG_ACTIONS
            debug_print("Is CurrentCounter > %d?\n", dv);
#endif
            if (CurrentCounter <= dv)
                return ACT_FAILURE;
            break;
        case COND_AT_INITIAL_LOC:
#ifdef DEBUG_ACTIONS
            debug_print("Is %s still in initial room?\n", Items[dv].Text);
#endif
            if (Items[dv].Location != Items[dv].InitialLoc)
                return ACT_FAILURE;
            break;
        case COND_MOVED:
#ifdef DEBUG_ACTIONS
            debug_print("Has %s been moved?\n", Items[dv].Text);
#endif
            if (Items[dv].Location == Items[dv].InitialLoc)
                return ACT_FAILURE;
            break;
        case COND_COUNTER_EQ:
#ifdef DEBUG_ACTIONS
            debug_print("Is current counter == %d?\n", dv);
            if (CurrentCounter != dv)
                debug_print("Nope, current counter is %d\n", CurrentCounter);
#endif
            if (CurrentCounter != dv)
                return ACT_FAILURE;
            break;
        }
#ifdef DEBUG_ACTIONS
        debug_print("YES\n");
#endif
        cc++;
    }
#if defined(__clang__)
#pragma mark Subcommands
#endif

    /* Decode the 4 subcommands from two 16-bit words.
       Each word encodes two actions: high = word / VOCAB_MULTIPLIER,
       low = word % VOCAB_MULTIPLIER. */
    act[0] = Actions[ct].Subcommand[0];
    act[2] = Actions[ct].Subcommand[1];
    act[1] = act[0] % VOCAB_MULTIPLIER;
    act[3] = act[2] % VOCAB_MULTIPLIER;
    act[0] /= VOCAB_MULTIPLIER;
    act[2] /= VOCAB_MULTIPLIER;
    cc = 0;
    pptr = 0;
    while (cc < NUM_SUBCOMMANDS) {
#ifdef DEBUG_ACTIONS
        debug_print("Performing action %d: ", act[cc]);
#endif
        if (act[cc] >= FIRST_MESSAGE && act[cc] <= LAST_DIRECT_MESSAGE) {
            PrintMessage(act[cc]);
        } else if (act[cc] >= EXTENDED_MSG_BASE) {
            PrintMessage(act[cc] - EXTENDED_MSG_OFFSET);
        } else
            switch (act[cc]) {
            case 0: /* NOP */
                break;
            case CMD_GET:
                if (CountCarried() >= GameHeader.MaxCarry) {
                    Output(sys[YOURE_CARRYING_TOO_MUCH]);
                    return ACT_SUCCESS;
                } else if (Items[param[pptr]].Location == MyLoc) {
                    should_draw_image = 1;
                }
                Items[param[pptr++]].Location = CARRIED;
                break;
            case CMD_DROP_HERE:
#ifdef DEBUG_ACTIONS
                debug_print("item %d (\"%s\") is now in location.\n", param[pptr],
                    Items[param[pptr]].Text);
#endif
                DrawUSRoomObject(param[pptr]);
                DrawImageOrVector();
                Items[param[pptr++]].Location = MyLoc;
                should_look_in_transcript = 1;
                break;
            case CMD_GOTO:
                GoTo(param[pptr++]);
                break;
            case CMD_DESTROY:
            case CMD_DESTROY_2:
#ifdef DEBUG_ACTIONS
                debug_print("Item %d (%s) is removed from the game (put in room 0).\n",
                    param[pptr], Items[param[pptr]].Text);
#endif
                if (Items[param[pptr]].Location == MyLoc) {
                    should_look_in_transcript = should_draw_image = 1;
                }
                Items[param[pptr++]].Location = 0;
                break;
            case CMD_SET_DARK:
                SetDark();
                break;
            case CMD_SET_LIGHT:
                SetLight();
                break;
            case CMD_SET_FLAG:
                SetBitFlag(param[pptr++]);
                break;
            case CMD_CLEAR_FLAG:
                ClearBitFlag(param[pptr++]);
                break;
            case CMD_DIE:
                PlayerIsDead();
                break;
            case CMD_PUT_IN_ROOM:
                p = param[pptr++];
                PutItemAInRoomB(p, param[pptr++]);
                break;
            case CMD_GAME_OVER:
#ifdef DEBUG_ACTIONS
                debug_print("Game over.\n");
#endif
                DoneIt();
                dead = 1;
                break;
            case CMD_LOOK:
                break;
            case CMD_SCORE:
                dead = PrintScore();
                StopTime = 2;
                break;
            case CMD_INVENTORY:
                if (Game->type == SEAS_OF_BLOOD_VARIANT)
                    AdventureSheet();
                else
                    ListInventory(0);
                if (Game->type == US_VARIANT && HasGraphics()) {
                    UpdateUSInventory();
                }
                StopTime = 2;
                break;
            case CMD_SET_FLAG0:
#ifdef DEBUG_ACTIONS
                debug_print("Set flag 0\n");
#endif
                SetBitFlag(0);
                break;
            case CMD_CLEAR_FLAG0:
#ifdef DEBUG_ACTIONS
                debug_print("Clear flag 0\n");
#endif
                ClearBitFlag(0);
                break;
            case CMD_REFILL_LAMP:
#ifdef DEBUG_ACTIONS
                debug_print("Refill lamp\n");
#endif
                GameHeader.LightTime = LightRefill;
                Items[LIGHT_SOURCE].Location = CARRIED;
                ClearBitFlag(LIGHTOUTBIT);
                break;
            case CMD_CLEAR_SCREEN:
#ifdef DEBUG_ACTIONS
                    debug_print("Clear screen\n");
#endif
                ClearScreen(); /* pdd. */
                break;
            case CMD_SAVE_GAME:
#ifdef DEBUG_ACTIONS
                    debug_print("Save game\n");
#endif
                SaveGame();
                StopTime = 2;
                break;
            case CMD_SWAP_ITEMS:
                p = param[pptr++];
#ifdef DEBUG_ACTIONS
                    debug_print("Swap locations of item %d (%s) (currently at %d) and item %d (%s) (currently at %d).\n", p, Items[p].Text, Items[p].Location, param[pptr], Items[param[pptr]].Text, Items[param[pptr]].Location);
#endif
                SwapItemLocations(p, param[pptr++]);
                break;
            case CMD_CONTINUE:
#ifdef DEBUG_ACTIONS
                debug_print("Continue with next line\n");
#endif
                continuation = 1;
                break;
            case CMD_FORCED_GET:
                if (Items[param[pptr]].Location == MyLoc) {
                    should_look_in_transcript = should_draw_image = 1;
                }
                Items[param[pptr++]].Location = CARRIED;
                break;
            case CMD_MOVE_TO_LOC_OF:
                p = param[pptr++];
                MoveItemAToLocOfItemB(p, param[pptr++]);
                break;
            case CMD_LOOK2:
#ifdef DEBUG_ACTIONS
                debug_print("LOOK\n");
#endif
                print_look_to_transcript = should_draw_image = 1;
                if (split_screen)
                    Look();
                print_look_to_transcript =
                should_look_in_transcript = 0;
                break;
            case CMD_DEC_COUNTER:
                if (CurrentCounter >= 1)
                    CurrentCounter--;
#ifdef DEBUG_ACTIONS
                debug_print(
                    "decrementing current counter. Current counter is now %d.\n",
                    CurrentCounter);
#endif
                break;
            case CMD_PRINT_COUNTER:
                OutputNumber(CurrentCounter);
                Output(" ");
                break;
            case CMD_SET_COUNTER:
#ifdef DEBUG_ACTIONS
                debug_print("CurrentCounter is set to %d.\n", param[pptr]);
#endif
                CurrentCounter = param[pptr++];
                break;
            case CMD_SWAP_LOC:
                GoToStoredLoc();
                break;
            case CMD_SWAP_COUNTERS:
                SwapCounters(param[pptr++]);
                break;
            case CMD_ADD_COUNTER:
                CurrentCounter += param[pptr++];
                break;
            case CMD_SUB_COUNTER:
                CurrentCounter -= param[pptr++];
                if (CurrentCounter < -1)
                    CurrentCounter = -1;
                /* Note: This seems to be needed. I don't yet
                         know if there is a maximum value to limit too */
                break;
            case CMD_PRINT_NOUN:
                PrintNoun();
                break;
            case CMD_PRINTLN_NOUN:
                PrintNoun();
                Output("\n");
                break;
            case CMD_NEWLINE:
                if (!(Options & SPECTRUM_STYLE))
                    Output("\n");
                break;
            case CMD_SWAP_ROOMFLAG:
                SwapLocAndRoomflag(param[pptr++]);
                break;
            case CMD_DELAY:
#ifdef DEBUG_ACTIONS
                debug_print("Delay\n");
#endif
                Delay(1);
                break;
            case CMD_GAME_SPECIFIC:
#ifdef DEBUG_ACTIONS
                debug_print("Action CMD_GAME_SPECIFIC, parameter %d\n", param[pptr]);
#endif
                fprintf(stderr, "Action CMD_GAME_SPECIFIC, parameter %d\n", param[pptr]);
                if (Game->type == US_VARIANT) {
                    fprintf(stderr, "CMD_GAME_SPECIFIC called in US game, do not read parameter!\n");
                    ShowUSCloseup(US_GAME_SPECIFIC_IMAGE, 0);
                } else {
                    p = param[pptr++];
                }
                switch (CurrentGame) {
                case SPIDERMAN:
                case SPIDERMAN_C64:
                    DrawBlack();
                    break;
                case SECRET_MISSION:
                case SECRET_MISSION_C64:
                    SecretAction(p);
                    break;
                case ADVENTURELAND:
                case ADVENTURELAND_C64:
                    AdventurelandAction(p);
                    break;
                case SEAS_OF_BLOOD:
                case SEAS_OF_BLOOD_C64:
                    BloodAction(p);
                    break;
                case ROBIN_OF_SHERWOOD:
                case ROBIN_OF_SHERWOOD_C64:
                    SherwoodAction(p);
                    break;
                case GREMLINS:
                case GREMLINS_SPANISH:
                case GREMLINS_SPANISH_C64:
                case GREMLINS_GERMAN:
                case GREMLINS_GERMAN_C64:
                    GremlinsAction();
                    break;
                default:
                    break;
                }
                break;
            case CMD_DRAW_IMAGE:
#ifdef DEBUG_ACTIONS
                debug_print("Draw Hulk image, parameter %d\n", param[pptr]);
#endif
                    p = param[pptr++];
                    if (!ItIsDark() && (CurrentGame == HULK || CurrentGame == HULK_C64)) {
                        DrawHulkImage(p);
                    } else if (Game->type == US_VARIANT) {
                        ShowUSCloseup(p, 0);
                    }
                break;
            default:
                debug_print("Unknown action %d [Param begins %d %d]\n", act[cc],
                    param[pptr], param[pptr + 1]);
                break;
            }
        cc++;
    }

    if (dead) {
        return ACT_GAMEOVER;
    } else if (continuation) {
        return ACT_CONTINUE;
    } else {
        return ACT_SUCCESS;
    }
}

static void PrintTakenOrDropped(int index)
{
    Output(sys[index]);
    int length = strlen(sys[index]);
    char last = sys[index][length - 1];
    if (last == '\n' || last == '\r')
        return;
    Output(" ");
    if ((!(CurrentCommand->allflag & LASTALL))
        || split_screen == 0) {
        Output("\n");
    }
}

/* Main action dispatch: process player input (verb/noun pair) or
   implicit actions (vb=0).

   Handles movement (GO + direction), game-specific examine actions,
   the action table scan, and built-in TAKE/DROP with auto-matching.
   Action 73 (continue) chains consecutive lines with vocab 0,0.

   For TI-99/4A games, delegates to the TI-specific action handler. */
static ExplicitResultType PerformActions(int vb, int no)
{
    int dark = ItIsDark();
    int ct = 0;
    ExplicitResultType flag;
    int doagain = 0;
    int found_match = 0;
#if defined(__clang__)
#pragma mark GO
#endif
    if (vb == GO && no == -1) {
        Output(sys[DIRECTION]);
        return ER_SUCCESS;
    }
    if (vb == GO && no >= 1 && no <= NUM_EXITS) {
        int nl;
        if (dark)
            Output(sys[DANGEROUS_TO_MOVE_IN_DARK]);
        nl = Rooms[MyLoc].Exits[no - 1];
        if (nl != 0) {
            /* Seas of Blood needs this to be able to flee back to the last room */
            if (Game->type == SEAS_OF_BLOOD_VARIANT)
                SavedRoom = MyLoc;
            if (Options & (SPECTRUM_STYLE | TI994A_STYLE))
                Output(sys[OK]);
            MyLoc = nl;
            should_look_in_transcript = should_draw_image = 1;
            if (CurrentCommand && CurrentCommand->next) {
                LookWithPause();
            }
            return ER_SUCCESS;
        }
        if (dark) {
            SetLight();
            MyLoc = GameHeader.NumRooms; /* It seems to be what the code says! */
            Output(sys[YOU_FELL_AND_BROKE_YOUR_NECK]);
            should_look_in_transcript = should_draw_image = 1;
            return ER_SUCCESS;
        }
        Output(sys[YOU_CANT_GO_THAT_WAY]);
        return ER_SUCCESS;
    }

    if (!dark) {
        switch (CurrentGame) {
            case HULK:
            case HULK_C64:
            case HULK_US:
                if (vb == HULK_EXAMINE_VERB)
                    HulkShowImageOnExamine(no);
                break;
            case COUNT_US:
                if (vb == COUNT_EXAMINE_VERB)
                    CountShowImageOnExamineUS(no);
                break;
            case VOODOO_CASTLE_US:
                if (vb == VOODOO_EXAMINE_VERB)
                    VoodooShowImageOnExamineUS(no);
                break;
            case ADVENTURELAND_US:
                if (vb == ADVENTURELAND_EXAMINE_VERB)
                    AdventurelandShowImageOnExamineUS(no);
                break;
            case PIRATE_US:
                fprintf(stderr, "vb:%d no:%d\n", vb, no);
                if (vb == PIRATE_EXAMINE_VERB) {
                    PirateShowImageOnExamineUS(no);
                }
                break;
            case SECRET_MISSION_US:
                fprintf(stderr, "vb:%d no:%d\n", vb, no);
                if (vb == MISSION_EXAMINE_VERB)
                    MissionShowImageOnExamineUS(no);
                break;
            case STRANGE_ODYSSEY_US:
                fprintf(stderr, "vb:%d no:%d\n", vb, no);
                if (vb == STRANGE_EXAMINE_VERB)
                    StrangeShowImageOnExamineUS(no);
                break;
            default:
                break;
        }
    }

    if (CurrentCommand && CurrentCommand->allflag && vb == CurrentCommand->verb && !(dark && vb == TAKE)) {
        if (!lastwasnewline)
            Output("\n");
        Output(Items[CurrentCommand->item].Text);
        Output("....");
    }
    flag = ER_RAN_ALL_LINES_NO_MATCH;
    if (CurrentGame != TI994A) {
        while (ct <= GameHeader.NumActions) {
            int verbvalue, nounvalue;
            verbvalue = Actions[ct].Vocab;
            /* Think this is now right. If a line we run has an action 73
           run all following lines with vocab of 0,0 */
            if (vb != 0 && (doagain && verbvalue != 0))
                break;
            /* Oops.. added this minor cockup fix 1.11 */
            if (vb != 0 && !doagain && flag == 0)
                break;
            nounvalue = verbvalue % VOCAB_MULTIPLIER;
            verbvalue /= VOCAB_MULTIPLIER;
            if ((verbvalue == vb) || (doagain && Actions[ct].Vocab == 0)) {
                if ((verbvalue == 0 && RandomPercent(nounvalue)) || doagain || (verbvalue != 0 && (nounvalue == no || nounvalue == 0))) {
                    if (verbvalue == vb && vb != 0 && nounvalue == no)
                        found_match = 1;
                    ActionResultType flag2;
                    if (flag == ER_RAN_ALL_LINES_NO_MATCH)
                        flag = ER_RAN_ALL_LINES;
                    if ((flag2 = PerformLine(ct)) != ACT_FAILURE) {
                        /* ahah finally figured it out ! */
                        flag = ER_SUCCESS;
                        if (flag2 == ACT_CONTINUE)
                            doagain = 1;
                        else if (flag2 == ACT_GAMEOVER)
                            return ER_SUCCESS;
                        if (vb != 0 && doagain == 0)
                            return ER_SUCCESS;
                    }
                }
            }

            ct++;

            // clang-format off
      /* Previously this did not check ct against
       * GameHeader.NumActions and would read past the end of
       * Actions.  I don't know what should happen on the last
       * action, but doing nothing is better than reading one
       * past the end.
       * --Chris
       */
            // clang-format on

            if (ct <= GameHeader.NumActions && Actions[ct].Vocab != 0)
                doagain = 0;
        }
    } else {
        if (vb == 0) {
            RunImplicitTI99Actions();
            return ER_NO_RESULT;
        } else {
            flag = RunExplicitTI99Actions(vb, no);
        }
    }

    if (found_match)
        return flag;

    if (flag != ER_SUCCESS) {
        int item = 0;
#if defined(__clang__)
#pragma mark TAKE
#endif
        if (vb == TAKE || vb == DROP) {
            if (CurrentCommand && CurrentCommand->allflag) {
                if (vb == TAKE && dark) {
                    Output(sys[TOO_DARK_TO_SEE]);
                    while (!(CurrentCommand->allflag & LASTALL)) {
                        CurrentCommand = CurrentCommand->next;
                    }
                    return ER_SUCCESS;
                }
                item = CurrentCommand->item;
                int location = CARRIED;
                if (vb == TAKE)
                    location = MyLoc;
                while (Items[item].Location != location && !(CurrentCommand->allflag & LASTALL)) {
                    CurrentCommand = CurrentCommand->next;
                }
                if (Items[item].Location != location)
                    return ER_SUCCESS;
            }

            /* Yes they really _are_ hardcoded values */
            if (vb == TAKE) {
                if (no == -1) {
                    Output(sys[WHAT]);
                    return ER_SUCCESS;
                }
                if (CountCarried() >= GameHeader.MaxCarry) {
                    Output(sys[YOURE_CARRYING_TOO_MUCH]);
                    return ER_SUCCESS;
                }
                if (!item)
                    item = MatchUpItem(no, MyLoc);
                if (item == -1) {
                    item = MatchUpItem(no, CARRIED);
                    if (item == -1) {
                        item = MatchUpItem(no, 0);
                        if (item == -1) {
                            Output(sys[THATS_BEYOND_MY_POWER]);
                        } else {
                            Output(sys[YOU_DONT_SEE_IT]);
                        }
                    } else {
                        Output(sys[YOU_HAVE_IT]);
                    }
                    return ER_SUCCESS;
                }
                Items[item].Location = CARRIED;
                should_draw_image = 1;
                PrintTakenOrDropped(TAKEN);
                return ER_SUCCESS;
            }
#if defined(__clang__)
#pragma mark DROP
#endif
            if (vb == DROP) {
                if (no == -1) {
                    Output(sys[WHAT]);
                    return ER_SUCCESS;
                }
                if (!item)
                    item = MatchUpItem(no, CARRIED);
                if (item == -1) {
                    item = MatchUpItem(no, 0);
                    if (item == -1) {
                        Output(sys[THATS_BEYOND_MY_POWER]);
                    } else {
                        Output(sys[YOU_HAVENT_GOT_IT]);
                    }
                    return ER_SUCCESS;
                }
                Items[item].Location = MyLoc;
                DrawUSRoomObject(item);
                DrawImageOrVector();
                PrintTakenOrDropped(DROPPED);
                return ER_SUCCESS;
            }
        }
    }
    return flag;
}

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
        ClearScreen();
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
