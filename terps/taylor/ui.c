//
//  restorestate.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "glk.h"
#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif
#include "glkstart.h"

#include "animations.h"
#include "layouttext.h"
#include "graphics.h"
#include "irmak.h"
#include "taylor.h"
#include "taylordraw.h"
#include "utility.h"

winid_t Bottom, Top, Graphics;
winid_t CurrentWindow;
static int OutCount;            /* Number of bytes currently buffered in OutWord. */
static char OutWord[128];   /* Per-word output buffer, flushed on whitespace. */

glui32 TopWidth; /* Terminal width */
glui32 TopHeight = 1; /* Height of top window */

int Options; /* Option flags */
int LineEvent = 0;
int GraphicsOff = 0;

/* printf-style wrapper that writes formatted text to a Glk window. */
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

/* Block until the user presses Return in the bottom window. Other keys
   are re-requested so the wait continues. */
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
        }
    } while (result == 0);

    return;
}

static void WordFlush(winid_t win);

static int JustWroteNewline = 0;

/* Lowest-level character output. Buffers characters into OutWord until a
   whitespace boundary, then dispatches the whole word to either the
   bottom (scrollback) window or the top (room description) buffer.
   Word-at-a-time buffering lets FlushRoomDescription wrap cleanly. */
void PrintCharacter(unsigned char c)
{
    /* Don't emit a stray nul before anything has been written. */
    if (OutCount == 0 && c == '\0')
        return;

    /* Collapse runs of '.' / '!' to a single character — Taylor's tokens
       sometimes contain doubled punctuation that should print as one. */
    if (c == '.' || c == '!') {
        if (JustWrotePeriod)
            return;
        JustWrotePeriod = 1;
    } else if (JustWrotePeriod && !isspace(c)) {
        JustWrotePeriod = 0;
    }

    if (CurrentWindow == Bottom) {
        if (isspace(c)) {
            /* Whitespace ends the current word. Newlines pass through
               (collapsed), other whitespace becomes a single space.
               JustWroteNewline suppresses runs of blank lines. */
            WordFlush(Bottom);
            if ((c == 10 || c == 13) && !JustWroteNewline) {
                Display(Bottom, "\n");
                JustWroteNewline = 1;
            } else if (!JustWroteNewline) {
                Display(Bottom, " ");
            }
            return;
        }
        JustWroteNewline = 0;
        OutWord[OutCount] = c;
        OutCount++;
        /* Flush before the buffer overflows; words longer than 128 chars
           will be split, but that's acceptable for game text. */
        if (OutCount > 127)
            WordFlush(Bottom);
        return;
    } else {
        /* Top window path: characters go to an in-memory stream that
           FlushRoomDescription later wraps and writes to the grid. */
        if (isspace(c)) {
            WordFlush(Top);
            WriteToRoomDescriptionStream(" ");
            if (c == '\n') {
                WriteToRoomDescriptionStream("\n");
            }
            return;
        }
        OutWord[OutCount] = c;
        OutCount++;
        /* In the top window we wrap at the actual terminal width so the
           subsequent layout pass can break lines correctly. */
        if (OutCount == TopWidth)
            WordFlush(Top);
    }
    return;
}

/* Block until the user types any character, then return its keycode.
   Non-input events (resize, timer) are dispatched while waiting so the
   UI stays responsive. */
unsigned char WaitCharacter(void)
{
    WordFlush(Bottom);
    glk_request_char_event(Bottom);

    event_t ev;
    do {
        glk_select(&ev);
        Updates(ev);
    } while (ev.type != evtype_CharInput);
    return ev.val1;
}

/* Emit the contents of the per-word buffer OutWord to the given window
   (or, for the top window, to the room-description memory stream that
   FlushRoomDescription later relays out). */
static void WordFlush(winid_t win)
{
    int i;
    for (i = 0; i < OutCount; i++) {
        if (win == Top)
            WriteToRoomDescriptionStream("%c", OutWord[i]);
        else
            Display(win, "%c", OutWord[i]);
    }
    OutCount = 0;
}

extern strid_t room_description_stream;
char *roomdescbuf = NULL;

/* Size of the memory buffer used to accumulate a single room description
   before it's wrapped and painted into the top window. */
#define ROOM_DESC_BUF_SIZE 1000

/* Switch the output target to the top (room description) window. Output
   is accumulated into a fixed-size memory stream so it can be reflowed
   and written to the text-grid window all at once in FlushRoomDescription. */
void TopWindow(void)
{
    WordFlush(Bottom);
    if (roomdescbuf != NULL) {
        fprintf(stderr, "roomdescbuf was not Null, so freeing it\n");
        free(roomdescbuf);
    }
    roomdescbuf = MemCalloc(ROOM_DESC_BUF_SIZE);
    room_description_stream = glk_stream_open_memory(roomdescbuf, ROOM_DESC_BUF_SIZE, filemode_Write, 0);

    CurrentWindow = Top;
    glk_window_clear(Top);
}

/* Draw the bottom-line separator across the top window using DelimiterChar
   (typically '_' or '=' depending on the game version). */
static void PrintWindowDelimiter(void)
{
    glk_window_get_size(Top, &TopWidth, &TopHeight);
    glk_window_move_cursor(Top, 0, TopHeight - 1);
    glk_stream_set_current(glk_window_get_stream(Top));
    for (int i = 0; i < TopWidth; i++)
        glk_put_char(DelimiterChar);
}

/* Reflow the buffered room description and paint it into the top window.
   Steps: close the memory stream, mirror to transcript, line-wrap to the
   current top-window width, resize the top window to fit, blit line by
   line, and finally draw the delimiter bar at the bottom. */
static void FlushRoomDescription(void)
{
    if (!room_description_stream)
        return;

    glk_stream_close(room_description_stream, 0);

    /* Mirror the raw (pre-layout) description into the transcript so the
       log stays readable regardless of window resizes. */
    if (Transcript) {
        if (LastChar != '\n')
            glk_put_string_stream(Transcript, "\n");
        glk_put_string_stream(Transcript, roomdescbuf);
    }

    int print_delimiter = 1;

    glk_window_clear(Top);
    glk_window_get_size(Top, &TopWidth, &TopHeight);
    int rows, length;
    char *text_with_breaks = LineBreakText(roomdescbuf, TopWidth, &rows, &length);

    /* If the bottom window is tiny (less than MIN_BOTTOM_ROWS) and the
       room text would need more rows than the top window currently has,
       leave the layout alone (and skip the delimiter) rather than shrink
       the bottom window further. */
    enum { MIN_BOTTOM_ROWS = 3 };
    glui32 bottomheight;
    glk_window_get_size(Bottom, NULL, &bottomheight);
    winid_t o2 = glk_window_get_parent(Top);
    if (!(bottomheight < MIN_BOTTOM_ROWS && TopHeight < rows)) {
        glk_window_get_size(Top, &TopWidth, &TopHeight);
        glk_window_set_arrangement(o2, winmethod_Above | winmethod_Fixed, rows,
            Top);
    } else {
        print_delimiter = 0;
    }

    /* Walk the wrapped buffer one line at a time, emitting each into the
       text-grid window via cursor moves. Trailing empty lines are tracked
       so the final window height can be trimmed below. */
    enum { LINE_BUF_SIZE = 2048 };
    int line = 0;
    int index = 0;
    int i;
    int empty_lines = 0;
    char string[LINE_BUF_SIZE];
    if (TopWidth > LINE_BUF_SIZE - 1)
        TopWidth = LINE_BUF_SIZE - 1;
    for (line = 0; line < rows && index < length; line++) {
        for (i = 0; i < TopWidth; i++) {
            string[i] = text_with_breaks[index++];
            if (string[i] == '\n' || string[i] == '\r' || index >= length)
                break;
        }
        if (i < TopWidth + 1) {
            string[i++] = '\n';
            if (i < 2)
                empty_lines++;
            else
                empty_lines = 0;
        }
        string[i] = 0;
        if (strlen(string) == 0)
            break;
        glk_window_move_cursor(Top, 0, line);
        Display(Top, "%s", string);
    }

    /* Trim trailing blank lines from the window height so the delimiter
       sits flush against the actual text. */
    line -= empty_lines;

    if (line < rows - 1) {
        glk_window_get_size(Top, &TopWidth, &TopHeight);
        glk_window_set_arrangement(o2, winmethod_Above | winmethod_Fixed,
            MIN(rows - 1, TopHeight - 1), Top);
    }

    free(text_with_breaks);

    if (print_delimiter) {
        PrintWindowDelimiter();
    }

    if (roomdescbuf != NULL) {
        free(roomdescbuf);
        roomdescbuf = NULL;
    }
}

/* Switch back to the bottom (scrollback) window, flushing and rendering
   the accumulated top-window content first. */
void BottomWindow(void)
{
    WordFlush(Top);
    FlushRoomDescription();
    WordFlush(Top);
    PendSpace = 0;
    CurrentWindow = Bottom;
}

void Look(void);

void OpenGraphicsWindow(void);
void CloseGraphicsWindow(void);

/* Sync the local Options bitfield and palette selection with the user's
   Spatterlight preferences (delays, forced inventory mode, forced
   palette, graphics on/off). Switches the palette and re-defines colors
   if it changed. */
void UpdateSettings(void)
{
#ifdef SPATTERLIGHT
    /* Delay actions (the "delay" verb) unless the user opted out. */
    if (gli_sa_delays)
        Options &= ~NO_DELAYS;
    else
        Options |= NO_DELAYS;

    /* Tri-state inventory: 0 = auto (game's default), 1 = force on,
       2 = force off. The two FORCE_* bits are mutually exclusive. */
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

    /* Tri-state palette: 0 = auto (game's default), 1 = force ZX,
       2 = force C64. The two FORCE_PALETTE_* bits are mutually exclusive. */
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

    /* Re-arm graphics if the user just turned them back on; otherwise
       remember they're off so we don't reopen the window. */
    if (gli_enable_graphics && GraphicsOff) {
        GraphicsOff = 0;
        Resizing = 0;
    } else if (!gli_enable_graphics) {
        GraphicsOff = 1;
    }
#endif
    /* Resolve the active palette from the FORCE_* overrides, falling back
       to whatever the game's loader picked. QP3/Blizzard Pass need a
       distinct C64 palette variant from the rest of the Taylor games. */
    palette_type previous_pal = palchosen;
    if (Options & FORCE_PALETTE_ZX)
        palchosen = ZXOPT;
    else if (Options & FORCE_PALETTE_C64) {
        if (BaseGame == QUESTPROBE3 || BaseGame == BLIZZARD_PASS)
            palchosen = C64QP3;
        else
            palchosen = C64TAYLOR;
    } else
        palchosen = Game->palette;
    if (palchosen != previous_pal) {
        DefinePalette();
        Resizing = 0;
    }
}

int Resizing = 0;

/* Handle Glk events that arrive while we're not waiting for line input:
   window resizes trigger a re-layout, and timer ticks drive the
   per-game animation loops (Rebel Planet, Kayleth). */
void Updates(event_t ev)
{
    if (ev.type == evtype_Arrange) {
        /* Set Resizing so DrawRoomImage uses the buffered image rather
           than re-running the full render pipeline. */
        Resizing = 1;
        UpdateSettings();
        CloseGraphicsWindow();
        Look();
        Resizing = 0;
    } else if (ev.type == evtype_Timer && TAYLOR_GRAPHICS_ENABLED) {
        switch (BaseGame) {
        case REBEL_PLANET:
            UpdateRebelAnimations();
            break;
        case KAYLETH:
            UpdateKaylethAnimations();
            break;
        default:
            break;
        }
    }
}

/* Native size of the in-game picture. Output is always scaled by an
   integer multiplier to keep the original pixel grid crisp. */
#define IMAGE_WIDTH_IN_PIXELS 255
#define IMAGE_HEIGHT_IN_PIXELS 96

/* Compute the largest integer multiplier that lets the native-size image
   fit in the current graphics window. Returns the multiplier and writes
   the scaled width/height through the out-parameters. */
static glui32 OptimalPictureSize(glui32 *width, glui32 *height)
{
    int multiplier = 1;
    glui32 graphwidth, graphheight;
    glk_window_get_size(Graphics, &graphwidth, &graphheight);
    /* Start from the height-fit multiplier, then clamp down if that
       would make the image wider than the window. */
    multiplier = graphheight / IMAGE_HEIGHT_IN_PIXELS;
    if (IMAGE_WIDTH_IN_PIXELS * multiplier > graphwidth)
        multiplier = graphwidth / IMAGE_WIDTH_IN_PIXELS;

    /* Floor at 1× so we still draw something on very small windows. */
    if (multiplier == 0)
        multiplier = 1;

    *width = IMAGE_WIDTH_IN_PIXELS * multiplier;
    *height = IMAGE_HEIGHT_IN_PIXELS * multiplier;

    return multiplier;
}

/* Tear down the graphics window if one exists. Looks up the window by
   rock id in case we lost the cached handle (e.g. after a restart). */
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

/* Create (or re-create) the graphics window above the bottom text
   buffer, sized to fit the game image. If no graphics window exists yet
   we need to tear down and re-open the top status window so the
   stacking order ends up as: bottom (buffer) → graphics → top (grid). */
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
        glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
            optimal_height, NULL);
    }
}

/* Ensure the top (status / room-description) text-grid window exists,
   reusing it if Glk already has one with our rock. Falls back to the
   bottom window if a status window can't be opened. */
void OpenTopWindow(void)
{
    Top = FindGlkWindowWithRock(GLK_STATUS_ROCK);
    if (Top == NULL) {
        Top = glk_window_open(Bottom, winmethod_Above | winmethod_Fixed,
            TopHeight, wintype_TextGrid, GLK_STATUS_ROCK);
        if (Top == NULL) {
            Top = Bottom;
        } else {
            glk_window_get_size(Top, &TopWidth, NULL);
        }
    }
}

/* Fill the picture area of the graphics window with black — used when
   the player is in a dark room (no image to show). */
void DrawBlack(void)
{
    glk_window_fill_rect(Graphics, 0, x_offset, 0, 32 * 8 * pixel_size,
        12 * 8 * pixel_size);
}

/* Render the current room's picture into the graphics window. Skipped
   for the intro pseudo-room, Kayleth's room 91, or when graphics are
   disabled. Animations restart whenever a new room is drawn. */
void DrawRoomImage(void)
{
    if (MyLoc == 0 || (BaseGame == KAYLETH && MyLoc == 91) || NoGraphics) {
        return;
    }
    ClearGraphMem();
    DrawTaylor(MyLoc, MyLoc);
    StartAnimations();
    DrawIrmakPictureFromBuffer();
}

/* Open the main scrollback text-buffer window. */
void OpenBottomWindow(void)
{
    Bottom = glk_window_open(0, 0, 0, wintype_TextBuffer, GLK_BUFFER_ROCK);
}

/* One-shot UI bootstrap called at startup: ensure both text windows
   exist and pull in the user's preferences. */
void DisplayInit(void)
{
    if (!Bottom)
        OpenBottomWindow();
    OpenTopWindow();
    UpdateSettings();
}
