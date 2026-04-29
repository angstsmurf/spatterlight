//
//  plusmain.c
//  The Plus interpreter
//
//  Created by Petter Sjölund on 2022-05-06.
//

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "glk.h"
#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif
#include "glkstart.h"

#include "animations.h"
#include "apple2detect.h"
#include "apple2draw.h"
#include "atari8detect.h"
#include "c64detect.h"
#include "common.h"
#include "definitions.h"
#include "gameinfo.h"
#include "graphics.h"
#include "layouttext.h"
#include "loaddatabase.h"
#include "parseinput.h"
#include "randomness.h"
#include "restorestate.h"
#include "stdetect.h"

const char *game_file = NULL;    /* Path to the game file being played */
char *DirPath = ".";             /* Directory containing the game file (for image lookups) */
size_t DirPathLength = 1;

uint8_t *mem;                    /* Raw game file contents (for binary format detection) */
size_t memlen;

const char *sys[MAX_SYSMESS];    /* System message strings (indexed by SysMessageType) */

uint64_t BitFlags = 0;           /* 64 general-purpose bit flags used by the action table */
int Options;                     /* Runtime option flags (NO_DELAYS, FORCE_INVENTORY, etc.) */

int split_screen = 1;            /* Whether to use a separate upper window for room descriptions */
int lastwasnewline = 1;          /* Tracks whether the last output character was a newline */
int pendingcomma = 0;            /* A trailing comma was deferred from the last message */
int should_restart = 0;          /* Set by DoneIt() to trigger a restart at the top of the main loop */

glui32 TopWidth, TopHeight;      /* Current dimensions of the status/room-description window */

winid_t Bottom, Top, Graphics;   /* Glk windows: main text, status bar, graphics */

strid_t Transcript = NULL;

GameInfo *Game = NULL;           /* Detected game identity and metadata */

SystemType CurrentSys = SYS_UNKNOWN; /* Detected platform (C64, Apple II, ST, Atari 8-bit) */

uint16_t header[64];             /* Raw game database header values */

int16_t Counters[64];            /* General-purpose counter registers used by the action table */
#define MAX_LOOPS 32

int loops[MAX_LOOPS];            /* Loop stack: stores action indices for loop-back commands */
int loop_index;                  /* Current depth in the loop stack */
int keep_going = 0;              /* Continue scanning the action table after a match */
int startover = 0;               /* Restart scanning the action table from the beginning */
int found_match = 0;             /* An action matched the current input */

/* JustStarted is only used for the error message "Can't undo on first move" */
int JustStarted = 1;

/* JustRestarted is only used to adjust newlines and room descriptions in the transcript */
extern int JustRestarted;

int showing_inventory = 0;       /* Non-zero while the Claymorgue inventory display is active */

Header GameHeader;
Item *Items;
Room *Rooms;

ObjectImage *ObjectImages;       /* Mapping of objects to their image indices and rooms */
char **Messages;                 /* Message strings referenced by action commands */
Action *Actions;                 /* The game's action table (conditions + commands) */

int ImageWidth = 280;            /* Native image dimensions (platform-dependent) */
int ImageHeight = 158;

glui32 TimerRate = 0;            /* Current Glk timer rate (ms), 0 = stopped */

static char DelimiterChar = '_'; /* Character used to draw the status window separator line */

/* Close the transcript stream (if open) and exit via Glk. */
GLK_ATTRIBUTE_NORETURN void CleanupAndExit(void)
{
    if (Transcript)
        glk_stream_close(Transcript, NULL);
    glk_exit();
}

/* Bit flag accessors. The 64-bit BitFlags word holds general-purpose
   flags tested and set by the action table; several indices have fixed
   meanings (e.g. DARKBIT=15, DRAWBIT=34, GRAPHICSBIT=35, STOPTIMEBIT=63). */
void SetBit(int bit)
{
    if (bit >= 64 || bit < 0) {
        fprintf(stderr, "SetBit: bit %d out of range!\n", bit);
    } else {
        BitFlags |= (uint64_t)1 << bit;
    }
}

void ResetBit(int bit)
{
    if (bit >= 64 || bit < 0) {
        fprintf(stderr, "ResetBit: bit %d out of range!\n", bit);
    } else {
        BitFlags &= ~((uint64_t)1 << bit);
    }
}

int IsSet(int bit)
{
    if (bit >= 64 || bit < 0) {
        fprintf(stderr, "IsSet: bit %d out of range!\n", bit);
        return 0;
    }
    return ((BitFlags & ((uint64_t)1 << bit)) != 0);
}

/* Memory stream used to compose room descriptions before displaying
   them in the status window. Written to by WriteToRoomDescriptionStream(),
   flushed by FlushRoomDescription(). */
static strid_t room_description_stream = NULL;

/* Printf-style output to a specific Glk window. */
void Display(winid_t w, const char *fmt, ...)
{
    va_list ap;
    char msg[2048];

    int size = sizeof msg;

    va_start(ap, fmt);
    vsnprintf(msg, size, fmt, ap);
    va_end(ap);

    glk_put_string_stream(glk_window_get_stream(w), msg);
}

/* Calculate the largest integer scaling multiplier that fits the native
   image dimensions into the graphics window. Returns the multiplier and
   writes the scaled dimensions to *width and *height. */
static glui32 OptimalPictureSize(glui32 *width, glui32 *height)
{
    int w = ImageWidth;
    int h = ImageHeight;

    int multiplier = 1;
    glui32 graphwidth, graphheight;
    glk_window_get_size(Graphics, &graphwidth, &graphheight);
    multiplier = graphheight / h;
    if (w * multiplier > graphwidth)
        multiplier = graphwidth / w;

    if (multiplier == 0)
        multiplier = 1;

    *width = w * multiplier;
    *height = h * multiplier;

    return multiplier;
}

/* Open (or reconfigure) the Glk graphics window above the text windows.
   Calculates the optimal pixel scaling, centres the image horizontally,
   and sizes the window to fit exactly. If a status window already exists
   it is temporarily closed and reopened below the graphics window. */
void OpenGraphicsWindow(void)
{
    if (!IsSet(GRAPHICSBIT))
        return;
    glui32 graphwidth, graphheight, optimal_width, optimal_height;
    y_offset = 0;

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
        pixel_size = OptimalPictureSize(&optimal_width, &optimal_height);
        x_offset = ((int)graphwidth - (int)optimal_width) / 2;

        if (graphheight > optimal_height) {
            winid_t parent = glk_window_get_parent(Graphics);
            glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
                optimal_height, NULL);
        }

        /* Set the graphics window background to match
         * the main window background, best as we can,
         * and clear the window.
         */
        glui32 background_color;
        if (glk_style_measure(Bottom, style_Normal, stylehint_BackColor,
                &background_color)) {
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
        pixel_size = OptimalPictureSize(&optimal_width, &optimal_height);
        x_offset = (graphwidth - optimal_width) / 2;
        winid_t parent = glk_window_get_parent(Graphics);
        if (parent)
            glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
                optimal_height, NULL);
    }

    right_margin = optimal_width + x_offset;
}

/* Close the graphics window and restore the status window dimensions. */
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

/* Set the Glk timer rate and record it in TimerRate. */
void SetTimer(glui32 milliseconds)
{
    TimerRate = milliseconds;
    glk_request_timer_events(milliseconds);
}

/* Sync runtime options (delays, inventory display, graphics) with
   Spatterlight's user-facing preferences. */
void UpdateSettings(void)
{
#ifdef SPATTERLIGHT
    if (gli_sa_delays)
        Options &= ~NO_DELAYS;
    else
        Options |= NO_DELAYS;

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

    if (gli_enable_graphics) {
        SetBit(GRAPHICSBIT);
    } else {
        ResetBit(GRAPHICSBIT);
    }
#endif
}

static void FlushRoomDescription(char *buf, int transcript);
static void ListInventory(int upper);

/* Claymorgue Castle displays the inventory in the upper window alongside
   images of carried items. This renders the inventory text to a memory
   stream, flushes it to the status window, then redraws room 33's image
   with overlays for each carried object. */
static void UpdateClaymorgueInventory(void)
{
    char *buf = MemAlloc(1000);
    buf = memset(buf, 0, 1000);
    room_description_stream = glk_stream_open_memory(buf, 1000, filemode_Write, 0);
    ListInventory(1);
    FlushRoomDescription(buf, 0);
    DrawRoomImage(33);
    for (int ct = 0; ct <= GameHeader.NumObjImg; ct++)
        if (ObjectImages[ct].Room == 33 && Items[ObjectImages[ct].Object].Location == CARRIED) {
            DrawItemImage(ObjectImages[ct].Image);
        }
}

void UpdateColorCycling(void);

static int AnimationCounter = 0; /* Tick count for animation rate division */

/* Handle non-input Glk events: window rearrange (redraw everything) and
   timer ticks (advance animations and colour cycling). Animation updates
   are rate-divided so the animation timer can run faster than the frame
   rate (e.g. when colour cycling shares the same timer). */
void Updates(event_t ev)
{
    if (ev.type == evtype_Arrange) {
        SavedImgType = LastImgType;
        SavedImgIndex = LastImgIndex;
        CloseGraphicsWindow();
        UpdateSettings();
        OpenGraphicsWindow();
        if (AnimationRunning && LastAnimationBackground) {
            char buf[5];
            snprintf(buf, sizeof buf, "S0%02d", LastAnimationBackground);
            DrawImageWithName(buf);
        } else {
            SetBit(DRAWBIT);
            if (showing_inventory == 1) {
                UpdateClaymorgueInventory();
            } else {
                Look(0);
                if (SavedImgType == IMG_OBJECT)
                    DrawItemImage(SavedImgIndex);
                else if (SavedImgType == IMG_SPECIAL)
                    DrawCloseup(SavedImgIndex);
            }
        }
    } else if (ev.type == evtype_Timer) {
        AnimationCounter++;
        if (AnimationRunning) {
            int factor = MAX(TimerRate, 1);
            int rate = MAX(AnimTimerRate / factor, 1);
            if (!IsSet(GRAPHICSBIT)) {
                StopAnimation();
            } else if (AnimationCounter % rate == 0) {
                UpdateAnimation();
            }
        }
        if (ColorCyclingRunning && IsSet(GRAPHICSBIT))
            UpdateColorCycling();
    }
}

static int DelayCounter = 0; /* Tick count for auto-dismissing AnyKey prompts */

/* Wait for a keypress. If timeout is set and no animation is running,
   auto-dismiss after ~3 seconds. Continues processing timer events
   (animations, colour cycling) while waiting. Claymorgue disables
   the timeout to avoid auto-advancing plot text. */
void AnyKey(int timeout, int message)
{
    if (message) {
        SystemMessage(HIT_ANY);
    }

    glk_request_char_event(Bottom);

    event_t ev;
    int result = 0;

    int cancel_after_delay = 0;

    if (CurrentGame == CLAYMORGUE)
        timeout = 0;

    if (timeout)
        cancel_after_delay = (AnimationRunning == 0 && ColorCyclingRunning == 0);

    if (cancel_after_delay && TimerRate == 0)
        SetTimer(3000);

    do {
        glk_select(&ev);
        if (ev.type == evtype_CharInput) {
            result = 1;
        } else {
            DelayCounter++;
            if (cancel_after_delay && ev.type == evtype_Timer) {
                int factor = MAX(TimerRate, 1);
                int rate = MAX(3000 / factor, 1);
                if (DelayCounter % rate == 0) {
                    result = 1;
                    glk_cancel_char_event(Bottom);
                    if (TimerRate == 3000)
                        SetTimer(0);
                    DelayCounter = 0;
                }
            }

            if (!AnimationRunning && !cancel_after_delay && timeout) {
                if (TimerRate == 0)
                    SetTimer(3000);
                cancel_after_delay = 1;
            }

            Updates(ev);
        }
    } while (result == 0);

    return;
}

/* Draw a line of DelimiterChar across the bottom row of the status window. */
static void PrintWindowDelimiter(void)
{
    glk_window_get_size(Top, &TopWidth, &TopHeight);
    glk_window_move_cursor(Top, 0, TopHeight - 1);
    glk_stream_set_current(glk_window_get_stream(Top));
    for (int i = 0; i < TopWidth; i++)
        glk_put_char(DelimiterChar);
}

/* Close the room description memory stream, optionally write it to the
   transcript, then display it. In split-screen mode, the text is word-
   wrapped to the status window width and the window is resized to fit;
   otherwise it goes to the lower text buffer. Frees buf when done. */
static void FlushRoomDescription(char *buf, int transcript)
{
    glk_stream_close(room_description_stream, 0);

    if (Transcript && transcript) {
        if (!lastwasnewline)
            glk_put_string_stream(Transcript, "\n");
        glk_put_string_stream(Transcript, "\n");
        size_t buflen = strlen(buf);
        char *roomtranscript = MemAlloc(buflen);
        buflen -= 2;
        strncpy(roomtranscript, buf, buflen);
        roomtranscript[buflen] = '\0';
        glk_put_string_stream(Transcript, roomtranscript);
        free(roomtranscript);
        glk_put_string_stream(Transcript, " ");
    }

    int print_delimiter = 1;

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
        if (TopWidth > 2047)
            TopWidth = 2047;
        char string[2048];
        for (line = 0; line < rows && index < length; line++) {
            for (i = 0; i < TopWidth; i++) {
                string[i] = text_with_breaks[index++];
                if (string[i] == 10 || string[i] == 13 || index >= length)
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

    if (buf != NULL) {
        free(buf);
        buf = NULL;
    }
}

/* Printf-style output to the room description memory stream. */
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
    char msg[2048];

    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);

    glk_put_string_stream(room_description_stream, msg);
}

/* Append a natural-language list of available exits to the room description
   stream (e.g. "I see exits to the North, East, and Down."). */
static void ListExits(void)
{
    int ct = 0;
    int numexits = 0;

    for (int i = 0; i < 6; i++)
        if (Rooms[MyLoc].Exits[i] != 0)
            numexits++;

    if (numexits == 0) {
        WriteToRoomDescriptionStream("\n");
        return;
    }
    if (numexits > 1)
        WriteToRoomDescriptionStream(" I see exits ");
    else
        WriteToRoomDescriptionStream(" %s", sys[EXITS]);

    for (int i = 0; i < 6; i++)
        if (Rooms[MyLoc].Exits[i] != 0) {
            if (ct) {
                if (ct == numexits - 1)
                    WriteToRoomDescriptionStream(", and ");
                else
                    WriteToRoomDescriptionStream("%s", sys[EXITS_DELIMITER]);
            } else if (ct < 5 && numexits > 1)
                WriteToRoomDescriptionStream("to the ");
            WriteToRoomDescriptionStream("%s", sys[i]);
            ct++;
        }
    WriteToRoomDescriptionStream(".\n");
}

/* Return "a", "an", or empty string depending on the first character. */
static const char *IndefiniteArticle(const char *word)
{
    char c = word[0];
    char vowels[6] = "aiouey";
    if (c == ' ')
        return "\0";
    for (int i = 0; i < 6; i++)
        if (vowels[i] == c)
            return " an ";
    return " a ";
}

/* Compose and display the full room description: room text, visible items,
   exit list, and optionally the inventory. Draws the room image if DRAWBIT
   is set. The description is built in a memory stream and flushed to the
   status window (or lower window if split_screen is off). */
void Look(int transcript)
{
    showing_inventory = 0;
    found_match = 1;
    if (IsSet(DRAWBIT)) {
        ResetBit(DRAWBIT);
        DrawCurrentRoom();
    }

    char *buf = MemCalloc(1000);
    room_description_stream = glk_stream_open_memory(buf, 1000, filemode_Write, 0);

    Room *r;
    int ct;

    int dark = IsSet(DARKBIT);
    for (int i = 0; i <= GameHeader.NumItems; i++) {
        if (Items[i].Flag & 2) {
            if (Items[i].Location == MyLoc || Items[i].Location == CARRIED)
                dark = 0;
        }
    }

    if (dark) {
        WriteToRoomDescriptionStream("%s", sys[TOO_DARK_TO_SEE]);
        FlushRoomDescription(buf, transcript);
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

    ct = 0;
    int numobj = 0;
    for (int i = 0; i <= GameHeader.NumItems; i++)
        if (Items[i].Location == MyLoc && Items[i].Text[0] != 0)
            numobj++;
    for (int i = 0; i <= GameHeader.NumItems; i++) {
        if (Items[i].Location == MyLoc && Items[i].Text[0] != 0) {
            if (ct == 0) {
                WriteToRoomDescriptionStream("%s", sys[YOU_SEE]);
            } else if (ct == numobj - 1) {
                WriteToRoomDescriptionStream(", and");
            } else {
                WriteToRoomDescriptionStream("%s", sys[ITEM_DELIMITER]);
            }
            WriteToRoomDescriptionStream("%s%s", IndefiniteArticle(Items[i].Text), Items[i].Text);
            ct++;
        }
    }

    WriteToRoomDescriptionStream(".");

    ListExits();

    if ((Options & FORCE_INVENTORY) && !(Options & FORCE_INVENTORY_OFF)) {
        WriteToRoomDescriptionStream("\n");
        ListInventory(1);
    }

    WriteToRoomDescriptionStream(" ");
    FlushRoomDescription(buf, transcript);
}

void Output(const char *string)
{
    Display(Bottom, "%s", string);
}

/* Output a system message (e.g. "OK", "I don't understand"), preceded
   by a space if we're not at the start of a line. */
void SystemMessage(SysMessageType message)
{
    if (!lastwasnewline)
        Output(" ");
    lastwasnewline = 0;
    Output(sys[message]);
}

/* Open (or find) the status/room-description text grid window. Falls
   back to using the main text buffer if a grid window can't be created. */
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

/* Create all three Glk windows: text buffer, status grid, and graphics. */
static void DisplayInit(void)
{
    Bottom = glk_window_open(0, 0, 0, wintype_TextBuffer, GLK_BUFFER_ROCK);
    OpenTopWindow();
    OpenGraphicsWindow();
}

static void OutputNumber(int a)
{
    if (lastwasnewline) {
        Display(Bottom, "%d", a);
        lastwasnewline = 0;
    } else {
        Display(Bottom, " %d", a);
    }
}

/* Pause for the given number of seconds, continuing to process timer
   events (so animations keep running). Skipped entirely if NO_DELAYS
   is set or an animation is already running. */
static void Delay(float seconds)
{
    if (Options & NO_DELAYS || AnimationRunning)
        return;

    event_t ev;

    if (!glk_gestalt(gestalt_Timer, 0))
        return;

    glk_request_char_event(Bottom);
    glk_cancel_char_event(Bottom);

    SetTimer(1000 * seconds);

    do {
        glk_select(&ev);
        Updates(ev);
    } while (ev.type != evtype_Timer);

    if (!AnimationRunning && !ColorCyclingRunning)
        SetTimer(0);
}

int RandomPercent(int n)
{
   return erkyrath_random() % 100 < n;
}

/* Count items in a room (0 = CARRIED). Also stores the carried count
   in Counter[33] when checking carried items. */
static int CountItemsInRoom(int room)
{
    int ct = 0;
    int n = 0;
    if (room == 0)
        room = CARRIED;
    while (ct <= GameHeader.NumItems) {
        if (Items[ct].Location == room)
            n++;
        ct++;
    }
    if (room == CARRIED)
        Counters[33] = n;
    return (n);
}

/* Find the first takeable item whose dictionary word matches 'noun' and
   is in location 'loc' (-1 = any location). Returns 0 if not found. */
static int MatchUpItem(int noun, int loc)
{
    for (int i = 0; i <= GameHeader.NumItems; i++)
        if (Items[i].Dictword == noun && (Items[i].Location == loc || loc == -1) && (Items[i].Flag & 1))
            return i;
    return (0);
}

/* Print a message from the Messages table. Handles leading backslashes
   (suppress space), trailing commas (defer to next message), and trailing
   single spaces (strip). Also writes to the transcript if active. */
static void PrintMessage(int index)
{
    if (JustRestarted && Transcript) {
        glk_put_string_stream(Transcript, "\n\n");
        JustRestarted = 0;
    }
    const char *message = Messages[index];
    debug_print("Print message %d: \"%s\"\n", index, message);
    if (lastwasnewline)
        pendingcomma = 0;
    if (message != NULL && message[0] != 0) {
        int i = 0;
        if (message[0] != '\\' && message[0] != ' ') {
            if (pendingcomma)
                Output(",");
            if (!lastwasnewline)
                Output(" ");
        } else {
            while (message[i] == '\\')
                i++;
        }
        pendingcomma = 0;
        while (message[i] != 0 && !(message[i] == ' ' && message[i + 1] == 0)) {
            if (message[i] == ',' && message[i + 1] == 0)
                pendingcomma = 1;
            else
                Display(Bottom, "%c", message[i]);
            i++;
        }
        lastwasnewline = 0;
    }
}

/* Print the dictionary word for the current noun (CurrentNoun). */
static void PrintNoun(void)
{
    DictWord *dict = Nouns;
    for (int i = 0; dict->Word != NULL; i++) {
        if (dict->Group == CurrentNoun) {
            Display(Bottom, "%s%s", lastwasnewline ? "" : " ", dict->Word);
            lastwasnewline = 0;
            return;
        }
        dict++;
    }
}

/* Find the index of the first word in a dictionary list that belongs
   to the given synonym group. Used for debug printing of action verbs/nouns. */
static int GetAnyDictWord(int group, DictWord *dict)
{
    for (int i = 0; dict->Word != NULL; i++) {
        if (dict->Group == group) {
            return i;
        }
        dict++;
    }
    return 0;
}

int GetDictWord(int group)
{
    DictWord *dict = Nouns;
    for (int i = 0; dict->Word != NULL; i++) {
        if (dict->Group == group) {
            return i;
        }
        dict++;
    }
    return 0;
}

/* Handle player death: print the death message, turn on lights, move
   to the start room (Counter 35), reset the death counter, and redraw. */
static void PlayerIsDead(void)
{
    debug_print("Player is dead\n");
    SystemMessage(IM_DEAD);
    ResetBit(DARKBIT);
    SetBit(DRAWBIT);
    MyLoc = Counters[35];
    Counters[17] = Counters[17] & 0x0f;
    Look(0);
}

/* If the given object has a room image overlay, set DRAWBIT so the
   room will be redrawn with the updated object state. */
void CheckForObjectImage(int obj)
{
    for (int i = 0; i <= GameHeader.NumObjImg; i++)
        if (ObjectImages[i].Object == obj) {
            SetBit(DRAWBIT);
            return;
        }
}

static void PutItemAInRoomB(int itemA, int roomB)
{
    debug_print("Item %d (%s) is put in room %d. MyLoc: %d (%s)\n", itemA, Items[itemA].Text, roomB, MyLoc, Rooms[MyLoc].Text);
    Items[itemA].Location = roomB;
    if (roomB == MyLoc)
        CheckForObjectImage(itemA);
}

static void SwapCounters(int index)
{
    debug_print("Main counter (47) is swapped with counter %d\n", index);
    if (index > 63) {
        debug_print("ERROR! parameter out of range. Max 63, got %d\n", index);
        index = 63;
    }
    int temp = CurrentCounter;

    CurrentCounter = Counters[index];
    Counters[index] = temp;
    debug_print("Value of new selected counter is %d\n", CurrentCounter);
}

static void SwapItemLocations(int itemA, int itemB)
{
    int temp = Items[itemA].Location;
    Items[itemA].Location = Items[itemB].Location;
    Items[itemB].Location = temp;
}

static void MoveItemAToLocOfItemB(int itemA, int itemB)
{
    Items[itemA].Location = Items[itemB].Location;
}

/* Game over: ask if the player wants to play again, then either
   set should_restart or exit. */
static void DoneIt(void)
{
    Output("\n");
    lastwasnewline = 1;
    SystemMessage(PLAY_AGAIN);
    Output(" ");
    if (YesOrNo()) {
        should_restart = 1;
    } else {
        CleanupAndExit();
    }
}

/* Printf-style output to the lower text buffer, mirrored to the
   transcript if active. Used by ListInventory for lower-window output. */
static void WriteToLowerWindow(const char *fmt, ...)
{
    va_list ap;
    char msg[2048];

    int size = sizeof msg;

    va_start(ap, fmt);
    vsnprintf(msg, size, fmt, ap);
    va_end(ap);

    glk_put_string_stream(glk_window_get_stream(Bottom), msg);
    if (Transcript)
        glk_put_string_stream(Transcript, msg);
}

/* Print the player's inventory. If 'upper' is set, output goes to the
   room description stream (for the status window); otherwise it goes to
   the lower text buffer. Items with empty text are invisible and skipped. */
static void ListInventory(int upper)
{
    int i, ct = 0;
    int numcarried = 0;

    for (i = 0; i <= GameHeader.NumItems; i++) {
        if (Items[i].Location == CARRIED && Items[i].Text[0] != 0)
            numcarried++;
    }

    void (*print_function)(const char *fmt, ...);
    if (upper) {
        print_function = WriteToRoomDescriptionStream;
    } else {
        print_function = WriteToLowerWindow;
    }

    if (upper)
        print_function("%s", sys[INVENTORY]);
    else
        SystemMessage(INVENTORY);

    for (i = 0; i <= GameHeader.NumItems; i++) {
        if (Items[i].Location == CARRIED) {
            if (Items[i].Text[0] == 0) {
                debug_print("Invisible item in inventory: %d\n", i);
                i++;
                continue;
            }

            if (ct) {
                if (ct == numcarried - 1)
                    print_function(", and");
                else
                    print_function("%s", sys[ITEM_DELIMITER]);
            }

            print_function("%s%s", IndefiniteArticle(Items[i].Text), Items[i].Text);
            ct++;
        }
    }
    if (ct == 0)
        print_function("%s", sys[NOTHING]);
    else
        print_function(".");

    if (upper)
        print_function("\n");
}

static void ClearScreen(void)
{
    glk_window_clear(Bottom);
}

/* Execute a system command (action opcode 110). These handle animation,
   image drawing, save/restore, and item flag manipulation — operations
   outside the standard Scott Adams action set. */
static void SysCommand(int arg1, int arg2)
{
    switch (arg1) {
    case 1:
        SystemMessage(RESUME_A_SAVED_GAME);
        if (YesOrNo())
            LoadGame();
        break;
    case 2:
        debug_print("Add special picture %d to animate buffer\n", Counters[arg2]);
        AddSpecialImage(Counters[arg2]);
        break;
    case 3:
        debug_print("Clear animate buffer\n");
        ClearAnimationBuffer();
        break;
    case 4:
        debug_print("Animate counter %d (%d)\n", arg2, Counters[arg2]);
        Animate(Counters[arg2]);
        break;
    case 5:
        debug_print("Unpack special picture %d (Set animation background)\n", arg2);
        AnimationBackground = arg2;
        break;
    case 6:
        debug_print("Beep and wait for key\n");
        AnyKey(1, 1);
        break;
    case 7:
        debug_print("DrawItemImage %d\n", Counters[arg2]);
        DrawItemImage(Counters[arg2]);
        if (CurrentSys == SYS_APPLE2)
            DrawApple2ImageFromVideoMemWithFlip(upside_down);
        break;
    case 8:
        debug_print("DrawRoomImage %d\n", Counters[arg2]);
        DrawRoomImage(Counters[arg2]);
        break;
    case 9: {
        debug_print("Swap item %d flags with counter 63\n", arg2);
        int temp = Counters[63];
        Counters[63] = Items[arg2].Flag;
        Items[arg2].Flag = temp;
        break;
    }
    case 10:
        debug_print("Add item image %d to animate buffer\n", Counters[arg2]);
        AddItemImage(Counters[arg2]);
        break;
    case 11:
        debug_print("Add room image %d to animate buffer\n", Counters[arg2]);
        AddRoomImage(Counters[arg2]);
        break;
    default:
        debug_print("Unknown system command %d\n", arg1);
        break;
    }
}

typedef enum {
    NO_PAREN,
    LEFT_PAREN,
    RIGHT_PAREN
} ParenType;

/* Process a condition-table logic control flag (condition code 31).
   These modify how subsequent conditions are combined:
   0=AND, 1=OR, 3=negate next, 4=negate until cancelled, 5=cancel negate,
   6/7=parentheses, 8=always true, 9=fuzzy match, 10=strict match. */
static ParenType SetLogicControlFlags(int mode, int *not_mode, int *not_continuous, int *or_mode, int *fuzzy_match)
{
    ParenType paren = NO_PAREN;
    debug_print("Mode %d, ", mode);
    switch (mode) {
    case 0:
        *or_mode = 0;
        break;
    case 1:
        debug_print("OR mode!\n");
        *or_mode = 1;
        break;
    case 2:
        Fatal("Unimplemented XOR mode");
    case 3:
        debug_print("Negate single!\n");
        *not_mode = 1;
        break;
    case 4:
        debug_print("Negate multiple!\n");
        *not_continuous = 1;
        *not_mode = 1;
        break;
    case 5:
        debug_print("Cancel negate!\n");
        *not_continuous = 0;
        *not_mode = 0;
        break;
    case 6:
        debug_print("Left paren!\n");
        paren = LEFT_PAREN;
        break;
    case 7:
        debug_print("Right paren!\n");
        paren = RIGHT_PAREN;
        break;
    case 8:
        debug_print("Keep going (Always true)!\n");
        break;
    case 9:
        debug_print("Fuzzy match!\n");
        *fuzzy_match = 1;
        break;
    case 10:
        debug_print("Strict match!\n");
        *fuzzy_match = 0;
        break;
    default:
        debug_print("Unhandled logic control flag %d\n", mode);
    }
    return paren;
}

/* Combine two condition results: AND when or_condition is 0, OR when 1. */
int CalculateConditionResult(int x, int y, int or_condition)
{
    if (or_condition == 0) {
        return (x && y);
    }

    return (x || y);
}

int parens_depth = 0;    /* Current nesting depth of parenthesised condition groups */
int parens_stack[5];     /* Saved intermediate results for each open parenthesis */

/* Evaluate the condition list of an action. Conditions are pairs of
   (code, argument) terminated by 255. Supports AND/OR/NOT logic,
   parenthetical grouping, fuzzy dictionary matching, and special
   argument values 998/999 for the current noun/noun2 objects.
   Returns ACT_SUCCESS if all conditions pass, ACT_FAILURE otherwise. */
static ActionResultType TestConditions(uint16_t *ptr)
{
    int negate_condition = 0;
    int negate_multiple = 0;
    int or_condition = 0;
    int fuzzy_match = 0;
    int last_result = 1;
    int current_result = 1;
    int one_success = 0;

    int cc = 0;

    int reached_end = 0;

    int cv = 0, dv = 0, dv2, originaldv;

    while (ptr[cc] != 255 && !reached_end) {
        cv = ptr[cc++];
        dv = ptr[cc++];

        originaldv = dv;

        if (dv == 998) {
            dv = NounObject;
        } else if (dv == 999) {
            dv = Noun2Object;
        } else if (dv == 997) {
            Fatal("With list unimplemented");
        }

        debug_print("Testing condition %d: ", cv);
        current_result = 1;

        switch (cv) {
        case 0:
            break;
        case 1:
            debug_print("Does the player carry %s?\n", Items[dv].Text);
            if (Items[dv].Location != CARRIED) {
                current_result = 0;
            }
            break;
        case 2:
            if (dv < 0 || dv > GameHeader.NumItems) {
                debug_print("Parameter out of range! (%d)\n", dv);
                current_result = 0;
                break;
            }

            debug_print("Is %s in location?\n", Items[dv].Text);
            if (Items[dv].Location != MyLoc) {
                current_result = 0;
            }
            break;
        case 3:
            debug_print("Is %s held or in location?\n", Items[dv].Text);
            if (Items[dv].Location != CARRIED && Items[dv].Location != MyLoc) {
                current_result = 0;
            }
            break;
        case 4:
            debug_print("Is location room %d, %s?\n", dv, Rooms[dv].Text);
            if (MyLoc != dv)
                current_result = 0;
            break;
        case 5:
            cc++;
            dv2 = ptr[cc++];
            debug_print("Is description of room %d (counter %d) (%d) == Message %d?\n", Counters[dv], dv, Rooms[Counters[dv]].Exits[6], dv2);
            if (Rooms[Counters[dv]].Exits[6] != dv2)
                current_result = 0;
            break;
        case 8:
            debug_print("Is bitflag %d set?\n", dv);
            if (!IsSet(dv)) {
                current_result = 0;
            }
            break;
        case 9:
            debug_print("Is bitflag %d NOT set?\n", dv);
            if (IsSet(dv)) {
                current_result = 0;
            }
            break;
        case 10:
            debug_print("Does the player carry anything?\n");
            if (CountItemsInRoom(dv) == 0) {
                current_result = 0;
            }
            break;
        case 13:
            debug_print("Is %s (%d) in play?\n", Items[dv].Text, dv);
            if (Items[dv].Location == 0) {
                current_result = 0;
            }
            break;
        case 14:
            debug_print("Is %s NOT in play?\n", Items[dv].Text);
            if (Items[dv].Location) {
                current_result = 0;
            }
            break;
        case 15:
            debug_print("Is CurrentCounter <= %d?\n", dv);
            if (CurrentCounter > dv) {
                current_result = 0;
            }
            break;
        case 16:
            debug_print("Is CurrentCounter > %d?\n", dv);
            if (CurrentCounter <= dv) {
                current_result = 0;
            }
            break;
        case 17:
            cc++;
            dv2 = ptr[cc++];
            debug_print("Is Counter %d == Counter %d?\n", dv, dv2);

            if (Counters[dv] != Counters[dv2]) {
                current_result = 0;
            }
            break;
        case 18:
            cc++;
            dv2 = ptr[cc++];
            debug_print("Is counter %d (%d) greater than counter %d (%d)?\n", dv, Counters[dv], dv2, Counters[dv2]);
            if (Counters[dv] <= Counters[dv2]) {
                current_result = 0;
            }
            break;
        case 19:
            debug_print("Is current counter == %d?\n", dv);
            if (CurrentCounter != dv) {
                current_result = 0;
            }
            break;
        case 20:
            cc++;
            dv2 = ptr[cc++];
            debug_print("Is counter %d (%d) == %d?\n", dv, Counters[dv], dv2);
            if (Counters[dv] != dv2)
                current_result = 0;
            break;

        case 21:
            cc++;
            dv2 = ptr[cc++];
            debug_print("Is counter %d (%d) >= %d?\n", dv, Counters[dv], dv2);
            if (Counters[dv] < dv2)
                current_result = 0;
            break;
        case 22:
            debug_print("Is counter %d (%d) == 0?\n", dv, Counters[dv]);
            if (Counters[dv] != 0)
                current_result = 0;
            break;
        case 23:
            cc++;
            dv2 = ptr[cc++];
            debug_print("Is item %d (%s) in room (counter %d) %d?\n", dv, Items[dv].Text, dv2, Counters[dv2]);
            if (Items[dv].Location != Counters[dv2])
                current_result = 0;
            break;
        case 26:
            cc++;
            dv2 = ptr[cc++];
            debug_print("Is object %d in room %d?\n", dv, dv2);
            if (Items[dv].Location != dv2)
                current_result = 0;

            break;
        case 28:
            cc++;
            dv2 = ptr[cc++];
            if (dv < 1 || dv2 < 1 || dv > GameHeader.NumItems + 1 || dv2 > GameHeader.NumItems + 1)
                debug_print("Is dictword of item (dv) %d == dictword group (dv2) %d?\n", dv, dv2);
            else
                debug_print("Is dictword (%d) of item %d (%s) == dictword (%d) of item %d (%s)?\n", Items[dv].Dictword, dv, Items[dv].Text, Items[dv2].Dictword, dv2, Items[dv2].Text);
            if (originaldv == 999) {
                if (CurrentNoun2 == dv2)
                    break;
                current_result = 0;
            } else if (originaldv == 998) {
                if (CurrentNoun == dv2)
                    break;
                current_result = 0;
            } else if (originaldv == 997) {
                Fatal("With list unimplemented");
            } else {
                if (Items[dv].Dictword == 0)
                    break;
                if (dv > GameHeader.NumItems + 1 || dv2 > GameHeader.NumItems + 1 || Items[dv].Dictword != Items[dv2].Dictword) {
                    current_result = 0;
                    if (dv > GameHeader.NumItems + 1 || dv2 > GameHeader.NumItems + 1)
                        debug_print("Error: Argument out of bounds\n!");
                }
            }
            break;
        case 29:
            cc++;
            dv2 = ptr[cc++];
            if (dv2 == 998)
                dv2 = CurrentNoun;
            else if (dv2 == 999)
                dv2 = CurrentNoun2;
            else if (dv2 == 997)
                Fatal("With list unimplemented");
            debug_print("Is dictword of object %d (%s) %d? (%s)\n", dv, Nouns[GetDictWord(Items[NounObject].Dictword)].Word, dv2, Nouns[GetDictWord(dv2)].Word);
            if (fuzzy_match) {
                debug_print("(Fuzzy match)\n");
                char *dictword1 = Nouns[GetDictWord(Items[NounObject].Dictword)].Word;
                char *dictword2 = Nouns[GetDictWord(dv2)].Word;
                if (!CompareUpToHashSign(dictword1, dictword2))
                    current_result = 0;
            } else {
                debug_print("(Exact match)\n");
                if (Items[NounObject].Dictword != dv2) {
                    current_result = 0;
                }
            }
            break;
        case 30:
            cc++;
            dv2 = ptr[cc++];
            if (dv > GameHeader.NumItems)
                debug_print("Bug! dv Out of bounds! dv:%d GameHeader.NumItems:%d\n", dv, GameHeader.NumItems);
            debug_print("Is Object flag %d & %d != 0\n", dv, dv2);
            if ((Items[dv].Flag & dv2) == 0)
                current_result = 0;
            break;
        case 31: /* logics control flags */
        {

            if (dv == 1 && one_success == 0) {
                last_result = 0;
            }

            ParenType paren = SetLogicControlFlags(dv, &negate_condition, &negate_multiple, &or_condition, &fuzzy_match);

            if (paren == LEFT_PAREN) {
                if (parens_depth >= 4)
                    Fatal("Too many nested parentheses in conditions");
                parens_stack[parens_depth] = last_result;
                debug_print("left parent.n\nSet parens_stack[%d] to last result %d\n", parens_depth, last_result);
                last_result = (or_condition == 0);
                parens_depth++;
            } else if (paren == RIGHT_PAREN) {
                if (parens_depth <= 0)
                    Fatal("Mismatched parentheses");
                parens_depth--;
                debug_print("right parens.\n\nGetting pushed result %d from parens_stack[%d]\n", parens_stack[parens_depth], parens_depth);
                current_result = last_result;
                last_result = parens_stack[parens_depth];
                last_result = CalculateConditionResult(last_result, current_result, or_condition);
            }
            break;
        }
        default:
            debug_print("Unknown condition %d, arg == %d\n", cv, dv);
            current_result = 0;
            break;
        }

        if (cv != 31) {
            if (negate_condition) {
                debug_print("Negating condition!\n");
                if (!current_result)
                    debug_print("Was fail, now success!\n");
                else
                    debug_print("Was success, now fail!\n");
                current_result = (current_result == 0);
                if (!negate_multiple)
                    negate_condition = 0;
            }

            if (current_result == 1 && cv != 31) {
                debug_print("YES\n");
            }

            last_result = CalculateConditionResult(last_result, current_result, or_condition);
            if (last_result)
                one_success = 1;
        }
    }

    if (last_result == 1)
        return ACT_SUCCESS;
    else
        return ACT_FAILURE;
}

/* Debug helper: print the meaning of well-known bit flag indices. */
static void PrintFlagInfo(int arg)
{
    switch (arg) {
    case 15:
        debug_print("15, darkbit\n");
        break;
    case 33:
        debug_print("33, a command matched on vocabulary, but its conditions failed\n");
        break;
    case 34:
        debug_print("34, room picture changed\n");
        break;
    case 35:
        debug_print("35, graphics on/off\n");
        break;
    case 60:
        debug_print("60, first action matches regardless of input\n");
        break;
    case 63:
        debug_print("63, skip automatics (stop time)\n");
        break;
    default:
        break;
    }
}

/* Copy the current parsed input words into their counter-register aliases
   (Counters 48–53) so the action table can read/modify them. */
void SetCountersFromInput(void) {
    VerbCounter = CurrentVerb;
    NounCounter = CurrentNoun;
    AdverbCounter = CurrentAdverb;
    PartpCounter = CurrentPartp;
    PrepCounter = CurrentPrep;
    Noun2Counter = CurrentNoun2;
}

/* Execute a single action table entry: test its conditions, then run
   its command sequence. Commands include item manipulation, player
   movement, counter arithmetic, message printing, image drawing, loops,
   and system commands. Returns an ActionResultType indicating whether
   to continue scanning (ACT_CONTINUE), stop (ACT_DONE), loop back
   (ACT_LOOP/ACT_LOOP_BEGIN), or that the game ended (ACT_GAMEOVER). */
static ActionResultType PerformLine(int ct)
{
    debug_print("\nPerforming line %d: (", ct);
    if (Actions[ct].Verb) {
        debug_print("%s %s) ", Verbs[GetAnyDictWord(Actions[ct].Verb, Verbs)].Word, Nouns[GetAnyDictWord(Actions[ct].NounOrChance, Nouns)].Word);
    } else {
        debug_print("0, %d) ", Actions[ct].NounOrChance);
    }
    int continuation = 0, dead = 0, done = 0;
    int loop = 0;

    int cc = 0;

    if (TestConditions(Actions[ct].Conditions) == ACT_FAILURE) {
        return ACT_FAILURE;
    }

#if defined(__clang__)
#pragma mark Commands
#endif

    /* Commands */
    cc = 0;
    uint8_t *commands = Actions[ct].Commands;
    int length = Actions[ct].CommandLength;
    while (cc <= length) {
        int cmd = commands[cc++];
        int16_t arg1 = 0, arg2 = 0, arg3 = 0,
                ca1 = 0, ca2 = 0, ca3 = 0;

        int object = 0;
        int plus_one_arg = 0;

        if (cc <= length) {
            arg1 = commands[cc];
            object = arg1;
            if (cc + 1 <= length) {
                arg2 = commands[cc + 1];
                if (cc + 2 <= length) {
                    arg3 = commands[cc + 2];
                }
            }

            if (arg1 < 64)
                ca1 = Counters[arg1];
            if (arg2 < 64)
                ca2 = Counters[arg2];
            if (arg3 < 64)
                ca3 = Counters[arg3];

            if (arg1 & 128) {
                arg1 = (arg1 & 127) * 256 + arg2;
                arg2 = arg3;
                plus_one_arg = 1;
            }
            if (arg1 == 998) {
                arg1 = CurrentNoun;
                object = NounObject;
            } else if (arg1 == 999) {
                arg1 = CurrentNoun2;
                object = Noun2Object;
            } else if (arg1 == 997) {
                Fatal("With list unimplemented");
            }
        }

        debug_print("\nPerforming command %d: ", cmd);
        if (cmd >= 1 && cmd < 52) {
            PrintMessage(cmd);
        } else
            switch (cmd) {
            case 0: /* NOP */
                break;
            case 52:
                if (CountItemsInRoom(0) >= GameHeader.MaxCarry) {
                    SystemMessage(YOURE_CARRYING_TOO_MUCH);
                    lastwasnewline = 1;
                    return ACT_SUCCESS;
                }
                Items[object].Location = CARRIED;
                CheckForObjectImage(object);
                cc += 1 + plus_one_arg;
                break;
            case 53:
                debug_print("Item %d (\"%s\") is now in location.\n", object,
                    Items[object].Text);
                Items[object].Location = MyLoc;
                CheckForObjectImage(object);
                cc += 1 + plus_one_arg;
                break;
            case 54:
                debug_print("Player location is now room %d (%s).\n", arg1,
                    Rooms[arg1].Text);
                MyLoc = commands[cc++];
                SetBit(DRAWBIT);
                break;
            case 55:
                debug_print("The command is changed to ");
                PrintDictWord(arg1, Verbs);
                debug_print(" ");
                PrintDictWord(arg2, Nouns);
                debug_print(".\n");

                CurrentVerb = arg1;
                CurrentNoun = arg2;
                CurrentPartp = 0;
                CurrentPrep = 0;
                CurrentNoun2 = 0;
                CurrentAdverb = 0;

                SetCountersFromInput();

                keep_going = 1;
                done = 1;
                found_match = 0;
                cc += 2;
                break;
            case 56:
                SetBit(DARKBIT);
                break;
            case 57:
                ResetBit(DARKBIT);
                break;
            case 58:
                debug_print("Bitflag %d is set\n", arg1);
#ifdef DEBUG_ACTIONS
                PrintFlagInfo(arg1);
#endif
                SetBit(commands[cc++]);
                break;
            case 59:
                debug_print("Item %d (%s) is removed from play.\n", object,
                    Items[object].Text);
                Items[object].Location = 0;
                CheckForObjectImage(object);
                cc += 1 + plus_one_arg;
                break;
            case 60:
                debug_print("BitFlag %d is cleared\n", arg1);
                PrintFlagInfo(arg1);
                ResetBit(commands[cc++]);
                break;
            case 61:
                PlayerIsDead();
                break;
            case 62:
                cc += 1 + plus_one_arg;
                PutItemAInRoomB(object, commands[cc++]);
                break;
            case 63:
                debug_print("Dead\n");
                DoneIt();
                dead = 1;
                break;
            case 64:
                debug_print("Counter %d == %d\n", arg2, arg1);
                Counters[arg2] = arg1;
                cc += 2 + plus_one_arg;
                break;
            case 65:
                debug_print("Multiply: Counter %d = Counter %d (%d) * Counter %d (%d) == %d\n", arg2, arg1, ca1, arg2, ca2, ca1 * ca2);
                Counters[arg2] = ca1 * ca2;
                cc += 2;
                break;
            case 66:
                ListInventory(0);
                if (CurrentGame == CLAYMORGUE)
                    UpdateClaymorgueInventory();
                break;
            case 67:
                debug_print("Set bitflag 0\n");
                SetBit(0);
                break;
            case 68:
                debug_print("Clear bitflag 0\n");
                ResetBit(0);
                break;
            case 69:
                debug_print("Done (stop reading lines)\n");
                done = 1;
                break;
            case 70:
                ClearScreen();
                break;
            case 71:
                SaveGame();
                break;
            case 72:
                cc += 1 + plus_one_arg;
                CheckForObjectImage(object);
                CheckForObjectImage(commands[cc]);
                SwapItemLocations(object, commands[cc++]);
                break;
            case 73:
                debug_print("Continue with next line\n");
                continuation = 1;
                break;
            case 74:
                Items[object].Location = CARRIED;
                CheckForObjectImage(object);
                cc += 1 + plus_one_arg;
                break;
            case 75:
                cc += 1 + plus_one_arg;
                CheckForObjectImage(object);
                MoveItemAToLocOfItemB(object, commands[cc++]);
                break;
            case 76:
                debug_print("LOOK\n");
                Look(1);
                break;
            case 77:
                if (CurrentCounter >= 1)
                    CurrentCounter--;
                debug_print("Decrementing current counter. Current counter is now %d.\n",
                    CurrentCounter);
                break;
            case 78:
                OutputNumber(CurrentCounter);
                break;
            case 79:
                debug_print("CurrentCounter is set to %d.\n", arg1);
                CurrentCounter = arg1;
                cc += 1 + plus_one_arg;
                break;
            case 80:
                debug_print("Counter %d is set to location of object %d.\n", arg2, object);
                Counters[arg2] = Items[object].Location;
                cc += 2 + plus_one_arg;
                break;
            case 81:
                SwapCounters(commands[cc++]);
                break;
            case 82:
                debug_print("Add %d to current counter(%d)\n", arg1, CurrentCounter);
                CurrentCounter += arg1;
                cc += 1 + plus_one_arg;
                break;
            case 83:
                CurrentCounter -= arg1;
                if (CurrentCounter < -1)
                    CurrentCounter = -1;
                cc += 1 + plus_one_arg;
                break;
            case 84:
                PrintNoun();
                break;
            case 85:
                debug_print("Print name of item %d (%s)\n", object, Items[object].Text);
                Display(Bottom, " %s", Items[object].Text);
                cc += 1 + plus_one_arg;
                break;
            case 86:
                debug_print("Output dictword of item (%d) + space.\n", arg1);
                Display(Bottom, "%s ", Nouns[GetDictWord(Items[arg1].Dictword)].Word);
                cc++;
                break;
            case 88:
                debug_print("Delay\n");
                Delay(1);
                break;
            case 89:
                debug_print("Draw current room image\n");
                DrawCurrentRoom();
                ResetBit(DRAWBIT);
                break;
            case 90:
                debug_print("Draw closeup image %d\n", arg1);
                DrawCloseup(commands[cc++]);
                SetBit(DRAWBIT);
                AnyKey(0, 1);
                break;
            case 91:
                debug_print("Divide: Counter %d = Counter %d (%d) / counter %d (%d), Counter %d = Counter %d (%d) / counter %d (%d)\n", arg2, arg2, ca2, arg1, ca1, arg3, arg2, ca2, arg1, ca1);
                if (ca1) {
                    Counters[arg3] = ca2 % ca1;
                    Counters[arg2] = ca2 / ca1;
                }
                cc += 3;
                break;
            case 92:
                debug_print("Domore reset\n");
                keep_going = 0;
                break;
            case 93:
                debug_print("Delay Counter %d (%d) * .25 seconds (%f)\n", arg1, Counters[arg1], Counters[arg1] * 0.25);
                if (CurrentGame == CLAYMORGUE)
                    Delay(Counters[arg1] * 0.125);
                else
                    Delay(Counters[arg1] * 0.25);
                cc++;
                break;
            case 94:
                debug_print("Counter %d = exit %d from room %d\n", arg1, ca3, ca2);
                Counters[arg1] = Rooms[ca2].Exits[ca3];
                cc += 3;
                break;
            case 95:
                debug_print("Exit %d of room %d is set to %d\n", ca2, ca1, ca3);
                Rooms[ca1].Exits[ca2] = ca3;
                cc += 3;
                break;
            case 97:
                debug_print("Swap counters %d %d \n", arg1, arg2);
                /* Draw room image after jumping off Claymorgue crate */
                if (arg1 == 32 && ca2 != MyLoc)
                    SetBit(DRAWBIT);
                Counters[arg1] = ca2;
                Counters[arg2] = ca1;
                cc += 2;
                break;
            case 98:
                debug_print("Counter %d = counter %d\n", arg2, arg1);
                Counters[arg2] = ca1;
                cc += 2;
                break;
            case 99:
                debug_print("Counter %d is set to %d \n", arg2, arg1);
                Counters[arg2] = arg1;
                cc += 2 + plus_one_arg;
                break;
            case 100:
                debug_print("Add %d to counter %d\n", arg1, arg2);
                Counters[arg2] += arg1;
                cc += 2 + plus_one_arg;
                break;
            case 101:
                debug_print("Decrease counter %d by %d\n", arg2, arg1);
                Counters[arg2] -= arg1;
                cc += 2 + plus_one_arg;
                break;
            case 102:
                debug_print("Print counter %d\n", arg1);
                OutputNumber(Counters[arg1]);
                cc++;
                break;
            case 103:
                debug_print("ABS counter %d\n", arg1);
                if (ca1 < 0)
                    Counters[arg1] = -ca1;
                cc++;
                break;
            case 104:
                debug_print("NEG counter %d\n", arg1);
                Counters[arg1] = -Counters[arg1];
                cc++;
                break;
            case 105:
                debug_print("AND counter %d with counter %d\n", arg2, arg1);
                Counters[arg2] = ca1 & ca2;
                cc += 2;
                break;
            case 106:
                debug_print("OR counter %d with counter %d\n", arg2, arg1);
                Counters[arg2] = ca1 | ca2;
                cc += 2;
                break;
            case 107:
                debug_print("XOR counter %d with counter %d\n", arg2, arg1);
                Counters[arg2] = ca1 ^ ca2;
                cc += 2;
                break;
            case 108:
                debug_print("Put random number (1 to 100) in counter %d\n", arg1);
                Counters[arg1] = 1 + erkyrath_random() % 100;
                cc++;
                break;
            case 109:
                debug_print("Clear counter %d\n", arg1);
                Counters[arg1] = 0;
                cc++;
                break;
            case 110:
                debug_print("System command %d %d\n", arg1, arg2);
                SysCommand(arg1, arg2);
                cc += 2;
                break;
            case 111:
                debug_print("Start over\n");
                keep_going = 1;
                done = 1;
                startover = 1;
                break;
            case 112:
                debug_print("Start loop\n");
                loop = 1;
                break;
            case 113:
                debug_print("Loop back\n");
                return ACT_LOOP;
            case 114:
                // 114 and 115 are now functionally the same, so at least one of them is wrong.
                // 114 is supposed to be instant while 115 is supposed to
                // do a 114 at the next loopback (113).
                // Things seems to work fine this way, however, and I'm not even sure what
                // the difference would be in practice.
                debug_print("Remove a loop from stack\n");
                loops[loop_index] = 0;
                if (loop_index > 0)
                    loop_index--;
                break;
            case 115:
                debug_print("Remove a loop from stack at next loopback\n");
                if (loop_index > 0)
                    loop_index--;
                break;
            case 116:
                debug_print("Continue looking for commands\n");
                debug_print("found_match = 0, domore = 1, done = 1\n");
                found_match = 0;
                keep_going = 1;
                done = 1;
                break;
            case 117:
                debug_print("Set found_match to 1\n");
                found_match = 1;
                SetBit(MATCHBIT);
                break;
            case 118:
                debug_print("Set done to 1\n");
                done = 1;
                break;
            case 119:
                cc++;
                if (arg1 >= 128) {
                    arg1 = commands[cc++] - 76;
                }
                debug_print("Set command prompt to string arg %d (%s)\n", arg1, Messages[arg1]);
                ProtagonistString = arg1;
                break;
            case 120:
                debug_print("counter 48: verb, counter 49: noun, counter 50: preposition, counter 51: adverb, counter 52: participle, counter 53: noun 2\n");
                SetCountersFromInput();
                break;
            case 121:
                debug_print("Set input words to counters 48 to 53\n");
                CurrentVerb = VerbCounter;
                CurrentNoun = NounCounter;
                CurrentAdverb = AdverbCounter;
                CurrentPartp = PartpCounter;
                CurrentPrep = PrepCounter;
                CurrentNoun2 = Noun2Counter;
                keep_going = 1;
                done = 1;
                found_match = 0;
                break;
            case 122:
                debug_print("Print object %d with article\n", object);
                Display(Bottom, "%s%s", IndefiniteArticle(Items[object].Text), Items[object].Text);
                cc += 1 + plus_one_arg;
                break;
            case 123:
                debug_print("Add counter %d to counter %d.\n", arg1, arg2);
                Counters[arg2] += ca1;
                cc += 2;
                break;
            case 124:
                debug_print("Reset all loops.\n");
                loop_index = 0;
                break;
            case 125:
                debug_print("Flag %d = Flag %d.\n", arg2, arg1);
                if (IsSet(arg1))
                    SetBit(arg2);
                else
                    ResetBit(arg2);
                cc += 2;
                break;
            case 126:
                debug_print("Counter %d = Counter (Counter %d (%d))\n", arg1, arg2, ca2);
                Counters[arg1] = Counters[ca2];
                cc += 2;
                break;
            case 127:
                debug_print("Counter (Counter %d (%d)) = Counter %d\n", arg2, ca2, arg1);
                Counters[ca2] = Counters[arg1];
                cc += 2;
                break;
            case 128:
                PrintMessage(commands[cc++] - 76);
                break;
            default:
                if (cc < length - 1)
                    debug_print("Unknown action %d [Param begins %d %d]\n", cmd,
                        commands[cc], commands[cc + 1]);
                break;
            }
    }

    if (dead) {
        debug_print("PerformLine: Returning ACT_GAMEOVER\n");
        return ACT_GAMEOVER;
    } else if (loop) {
        debug_print("PerformLine: Returning ACT_LOOP_BEGIN\n");
        return ACT_LOOP_BEGIN;
    } else if (continuation) {
        debug_print("PerformLine: Returning ACT_CONTINUE\n");
        return ACT_CONTINUE;
    } else if (done) {
        debug_print("PerformLine: Returning ACT_DONE\n");
        return ACT_DONE;
    } else {
        debug_print("PerformLine: Returning ACT_SUCCESS\n");
        return ACT_SUCCESS;
    }
}

/* Check whether the player's input matches the "extra words" (adverbs,
   prepositions, participles + second nouns) attached to action entry ct.
   The Plus format stores these as typed word records alongside the main
   verb/noun, enabling commands like "CAREFULLY THROW BALL AT WALL". */
static int IsExtraWordMatch(int ct)
{
    /* If the action has no "extra words" the command must contain no participle */
    if (Actions[ct].NumWords == 0) {
        return (CurrentPartp == 0);
    }

    int foundprep = 0;
    int foundpartp = 0;
    int foundadverb = 0;

    int matchedprep = 0;
    int matchedpartp = 0;
    int matchedadverb = 0;

    debug_print("Action %d (%s %s) has %d extra words\n", ct, Verbs[GetAnyDictWord(Actions[ct].Verb, Verbs)].Word, Nouns[GetAnyDictWord(Actions[ct].NounOrChance, Nouns)].Word, Actions[ct].NumWords);

    for (int i = 0; i < Actions[ct].NumWords; i++) {
        uint8_t word = Actions[ct].Words[i];
        debug_print("Extra word %d: %d ", i, word);
        uint8_t wordval = word & 63;
        switch (word >> 6) {
        case 1: /* adverb */
            debug_print("(adverb \"%s\")\n", Adverbs[GetAnyDictWord(wordval, Adverbs)].Word);
            foundadverb = 1;
            if (wordval == CurrentAdverb || wordval == 0 || (wordval == 1 && CurrentAdverb == 0))
                matchedadverb = 1;
            debug_print("Matched adverb %d\n", wordval);
            break;
        case 2: /* participle */
            debug_print("(participle \"%s\")\n", Prepositions[GetAnyDictWord(wordval, Prepositions)].Word);
            i++;
            foundpartp = 1;
            uint8_t n2 = Actions[ct].Words[i];
                debug_print("(object (word %d) \"%s\")\n", i, Nouns[GetAnyDictWord(n2, Nouns)].Word);
            if (CurrentPartp == 0 && wordval > 1 && !IsNextParticiple(wordval, n2)) {
                break;
            }
            if (wordval == CurrentPartp || wordval == 0 || (wordval == 1 && CurrentPartp == 0)) { /* 1 means NONE */
                debug_print("Matched participle %d\n", wordval);
                if (n2 == CurrentNoun2 || n2 == 0) {
                    debug_print("Matched object %d (%s)\n", n2, Nouns[GetDictWord(n2)].Word);
                    matchedpartp = 1;
                }
            }
            break;
        case 3: /* preposition */
                debug_print("(preposition \"%s\")\n",  Prepositions[GetAnyDictWord(wordval, Prepositions)].Word);
            foundprep = 1;
            if (CurrentPrep == wordval || wordval == 0 || (wordval == 1 && CurrentPrep == 0)) {
                debug_print("Matched preposition %d\n", wordval);
                matchedprep = 1;
            }
            break;
        default:
            debug_print("(unknown type)\n");
            debug_print("word >> 6: %d\n", word >> 6);
            break;
        }
    }

    if ((foundprep && !matchedprep) || (foundpartp && !matchedpartp) || (foundadverb && !matchedadverb)) {
        debug_print("IsExtraWordMatch failed\n");
        return 0;
    }

    return 1;
}

/* Check if an implicit (verb=0) action should fire. These are triggered
   by a random percentage chance (NounOrChance), or unconditionally if
   doagain is set (chained from a previous action via ACT_CONTINUE). */
static int IsImplicitMatch(int ct, int doagain)
{
    if (RandomPercent(Actions[ct].NounOrChance)) {
        return 1;
    }

    if (doagain)
        return 1;

    return 0;
}

/* Check if an explicit (verb!=0) action matches the current input.
   Matches when the verb matches and either the noun matches or is
   a wildcard (0). Also checks extra words (adverbs, prepositions).
   When doagain is set, verb-0 continuation entries also match. */
static int IsMatch(int ct, int doagain)
{
    int verbvalue = Actions[ct].Verb;
    int nounvalue = Actions[ct].NounOrChance;

    if (verbvalue != CurrentVerb && !(doagain && verbvalue == 0))
        return 0;

    if (verbvalue == 0) {
        if (RandomPercent(nounvalue)) {
            return 1;
        } else if (nounvalue != 0) {
            return 0;
        }
    }

    if (!(doagain && verbvalue == 0) && !IsExtraWordMatch(ct)) {
        return 0;
    }

    if (verbvalue == CurrentVerb && verbvalue != 0 && (nounvalue == CurrentNoun || nounvalue == 0)) {
        found_match = 1;
        return 1;
    }

    if (doagain)
        return 1;

    return 0;
}

/* Run all implicit (automatic) actions — those with verb=0 at the start
   of the action table. These fire each turn before the player is prompted,
   handling timed events, NPC behaviour, and game-state updates. Supports
   chaining (ACT_CONTINUE), loops (ACT_LOOP), and restart (startover).
   Stops early if STOPTIMEBIT is set and no chain is active. */
static CommandResultType PerformImplicit(void)
{
    int ct = 0;
    CommandResultType result = ER_RAN_ALL_LINES_NO_MATCH;
    int chain_on = 0;
    startover = 0;
    loop_index = 0;

    while (ct <= GameHeader.NumActions) {
        int verbvalue = Actions[ct].Verb;

        if (verbvalue != 0)
            break;

        if (IsImplicitMatch(ct, chain_on)) {
            ActionResultType actresult = PerformLine(ct);

            if (actresult != ACT_FAILURE) {
                result = ER_SUCCESS;
                switch (actresult) {
                case ACT_CONTINUE:
                    chain_on = 1;
                    break;
                case ACT_DONE:
                    chain_on = 0;
                    loops[loop_index] = 0;
                    if (loop_index)
                        loop_index--;
                    break;
                case ACT_LOOP_BEGIN:
                    loop_index++;
                    if (loop_index >= MAX_LOOPS)
                        Fatal("Loop stack overflow");
                    loops[loop_index] = ct;
                    chain_on = 1;
                    break;
                case ACT_LOOP:
                    if (loop_index) {
                        ct = loops[loop_index];
                    }
                    break;
                case ACT_GAMEOVER:
                    return ER_SUCCESS;
                default:
                    break;
                }
            }
        }

        ct++;

        if (startover) {
            ct = 0;
            startover = 0;
        }

        if (ct <= GameHeader.NumActions && (Actions[ct].Verb + Actions[ct].NounOrChance != 0))
            chain_on = 0;

        if (IsSet(STOPTIMEBIT) && chain_on == 0) {
            ResetBit(STOPTIMEBIT);
            return result;
        }
    }

    return result;
}

/* Run the explicit (player-input) action table. Scans for actions whose
   verb/noun match the current input, tests their conditions, and executes
   commands. Sets MATCHBIT when vocabulary matches but conditions fail
   (so the engine can report "You can't do that yet" vs "I don't understand").
   Supports the same chaining, loops, and restart mechanisms as PerformImplicit. */
static CommandResultType PerformExplicit(void)
{
    int ct = 0;
    CommandResultType result = ER_RAN_ALL_LINES_NO_MATCH;
    int chain_on = 0;
    keep_going = 1;
    startover = 0;
    loop_index = 0;
    found_match = 0;
    ResetBit(MATCHBIT);
    NounObject = MatchUpItem(CurrentNoun, -1);
    Noun2Object = MatchUpItem(CurrentNoun2, -1);

    while (ct <= GameHeader.NumActions) {
        int verbvalue, nounvalue;
        verbvalue = Actions[ct].Verb;
        nounvalue = Actions[ct].NounOrChance;

        if (chain_on && verbvalue != 0) {
            chain_on = 0;
        }

        if (IsMatch(ct, chain_on)) {
            ActionResultType actresult = PerformLine(ct);

            if (actresult != ACT_FAILURE) {
                if (actresult != ACT_DONE)
                    ResetBit(MATCHBIT);
                if (found_match) {
                    keep_going = 0;
                    result = ER_SUCCESS;
                } else {
                    result = ER_RAN_ALL_LINES;
                }

                switch (actresult) {
                case ACT_CONTINUE:
                    chain_on = 1;
                    break;
                case ACT_DONE:
                    chain_on = 0;
                    loops[loop_index] = 0;
                    if (loop_index)
                        loop_index--;
                    break;
                case ACT_LOOP_BEGIN:
                    loop_index++;
                    if (loop_index >= MAX_LOOPS)
                        Fatal("Loop stack overflow");
                    loops[loop_index] = ct;
                    chain_on = 1;
                    break;
                case ACT_LOOP:
                    if (loop_index) {
                        ct = loops[loop_index];
                    }
                    break;
                case ACT_GAMEOVER:
                    return ER_SUCCESS;
                default:
                    break;
                }
            } else {
                if (verbvalue == CurrentVerb && (nounvalue == CurrentNoun || nounvalue == 0))
                    SetBit(MATCHBIT);
            }
        }

        ct++;

        if (startover) {
            ct = 0;
            startover = 0;
        }

        if (ct <= GameHeader.NumActions && (Actions[ct].Verb != 0)) {
            chain_on = 0;
        }

        if (found_match && !chain_on && !keep_going)
            break;
    }

    if (result == ER_RAN_ALL_LINES_NO_MATCH && IsSet(MATCHBIT))
        result = ER_RAN_ALL_LINES;

    return result;
}

glkunix_argumentlist_t glkunix_arguments[10];

/* Glk startup hook: extract the game file path from command-line arguments
   and split it into DirPath + filename for image file lookups. */
int glkunix_startup_code(glkunix_startup_t *data)
{
    int argc = data->argc;
    char **argv = data->argv;

    if (argc < 1)
        return 0;

#ifdef GARGLK
    garglk_set_program_name("Plus 1.0");
    garglk_set_program_info("Plus 1.0 by Petter Sjölund");
#endif

    if (argc == 2) {
        game_file = argv[1];

        const char *s;
        DirPathLength = 0;
        if ((s = strrchr(game_file, '/')) != NULL || (s = strrchr(game_file, '\\')) != NULL) {
            DirPathLength = (int)(s - game_file + 1);
#ifdef GARGLK
            garglk_set_story_name(s + 1);
        } else {
            garglk_set_story_name(game_file);
#endif
        }
        if (DirPathLength) {
            DirPath = MemAlloc(DirPathLength + 1);
            memcpy(DirPath, game_file, DirPathLength);
            DirPath[DirPathLength] = 0;
            debug_print("Directory path: \"%s\"\n", DirPath);
        }
    }

    return 1;
}

/* Recalculate scaling and offsets for the title screen image, centring
   it both horizontally and vertically (⅓ from top). */
void ResizeTitleImage(void)
{
    glui32 graphwidth, graphheight, optimal_width, optimal_height;
#ifdef SPATTERLIGHT
    glk_window_set_background_color(Graphics, gbgcol);
    glk_window_clear(Graphics);
#endif
    glk_window_get_size(Graphics, &graphwidth, &graphheight);
    pixel_size = OptimalPictureSize(&optimal_width, &optimal_height);
    x_offset = ((int)graphwidth - (int)optimal_width) / 2;
    right_margin = optimal_width + x_offset;
    y_offset = ((int)graphheight - (int)optimal_height) / 3;
}

/* Display the title screen (image "S000") in a full-window graphics view.
   Waits for a keypress to proceed, handling resize and colour-cycling
   events while waiting. Closes all windows and reinitialises the normal
   three-window layout when done. */
void DrawTitleImage(void)
{
    DisplayInit();
#ifdef SPATTERLIGHT
    if (!gli_enable_graphics)
        return;
#endif
    glk_window_close(Top, NULL);
    Top = NULL;
    glk_window_close(Bottom, NULL);
    Bottom = NULL;

    ResizeTitleImage();

    if (glk_gestalt_ext(gestalt_GraphicsCharInput, 0, NULL, 0)) {
        glk_request_char_event(Graphics);
    } else {
        Bottom = glk_window_open(Graphics, winmethod_Below | winmethod_Fixed,
            2, wintype_TextBuffer, GLK_BUFFER_ROCK);
        glk_request_char_event(Bottom);
    }

    if (DrawImageWithName("S000")) {

        if (CurrentSys == SYS_APPLE2)
            DrawApple2ImageFromVideoMem();

        event_t ev;
        do {
            glk_select(&ev);
            if (ev.type == evtype_Arrange) {
#ifdef SPATTERLIGHT
                if (!gli_enable_graphics)
                    break;
#endif
                ResizeTitleImage();
                glk_window_clear(Graphics);
                DrawImageWithName("S000");
                if (CurrentSys == SYS_APPLE2)
                    DrawApple2ImageFromVideoMem();
            } else if (ev.type == evtype_Timer) {
                if (ColorCyclingRunning)
                    UpdateColorCycling();
            }
        } while (ev.type != evtype_CharInput);
    }

    if (Bottom != NULL)
        glk_window_close(Bottom, NULL);
    glk_window_close(Graphics, NULL);
    Graphics = NULL;
    DisplayInit();
}

/* Main entry point. Loads the game database (trying plaintext first, then
   binary formats for Atari 8-bit, Atari ST, Apple II, and C64), shows the
   title screen, and enters the main game loop:
     1. Run implicit (automatic) actions
     2. Display the room
     3. Get player input
     4. Run explicit (player-command) actions
     5. Report errors if no action matched */
void glk_main(void)
{
    if (game_file == NULL)
        Fatal("No game file");

    /* Initialize system messages, preferring first-person ("I am") forms */
    for (int i = 0; i < MAX_SYSMESS; i++) {
        sys[i] = sysdict[i];
        if (sysdict_i_am[i])
            sys[i] = sysdict_i_am[i];
    }

    FILE *f = fopen(game_file, "r");
    if (f == NULL)
        Fatal("Cannot open game");

    size_t filesize = GetFileLength(f);

    if (filesize == 0) {
        fclose(f);
        Fatal("Game file empty");
    }

    /* Try loading as plaintext database first; if that fails, attempt
       binary format detection for each supported platform */
    if (LoadDatabasePlaintext(f, DEBUG_PRINT) == UNKNOWN_GAME) {
        fseek(f, 0, SEEK_SET);
        mem = MemAlloc(filesize);
        memlen = fread(mem, 1, filesize, f);
        fclose(f);

        if (!DetectAtari8(&mem, &memlen)) {
            if (!DetectST(&mem, &memlen) && !DetectApple2(&mem, &memlen) && !DetectC64(&mem, &memlen)) {
                Fatal("Could not detect game type");
            }

            if (!LoadDatabaseBinary()) {
                Fatal("Could not load binary database");
            }
        }

        if (CurrentSys == SYS_APPLE2)
            LookForApple2Images();

        if (CurrentSys == SYS_C64 && CurrentGame == SPIDERMAN)
            PatchSpidermanImages();
    }

#ifdef SPATTERLIGHT
    UpdateSettings();
    if (gli_determinism)
        set_erkyrath_random(1234);
    else
#endif
    set_erkyrath_random(0);

    DrawTitleImage();
    UpdateSettings();

    InitAnimationBuffer();
    InitialState = SaveCurrentState();

    /* --- Main game loop --- */
    while (1) {
        glk_tick();

        if (should_restart)
            RestartGame();

        /* Run automatic actions unless time is stopped or we just restored */
        if (IsSet(STOPTIMEBIT) || JustRestored) {
            ResetBit(STOPTIMEBIT);
        } else {
            PerformImplicit();
            if (!should_restart)
                SaveUndo();
        }

        Look(0);

        if (should_restart)
            continue;

        /* Restore a saved object image overlay (e.g. Thing in Fantastic Four) */
        if (JustRestored && SavedImgType == IMG_OBJECT) {
            DrawItemImage(SavedImgIndex);
        }

        JustRestored = 0;
        JustRestarted = 0;

        if (GetInput() == 1)
            continue;

        SetCountersFromInput();

        /* Save current input as "last" for AGAIN/repeat commands */
        LastVerb = CurrentVerb;
        LastNoun = CurrentNoun;
        LastPrep = CurrentPrep;
        LastPartp = CurrentPartp;
        LastNoun2 = CurrentNoun2;
        LastAdverb = CurrentAdverb;

        ClearFrames();

        CommandResultType result = PerformExplicit();

        /* If no action fully matched, report the appropriate error */
        if (result == ER_RAN_ALL_LINES_NO_MATCH || result == ER_RAN_ALL_LINES || IsSet(MATCHBIT)) {
            if (result == ER_RAN_ALL_LINES_NO_MATCH)
                SystemMessage(I_DONT_UNDERSTAND);
            else
                SystemMessage(YOU_CANT_DO_THAT_YET);
            StopProcessingCommand();
        }

        JustStarted = 0;
    }
}
