//
//  title_image.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-10-01.
//

#include <stdlib.h>
#include <string.h>

#include "apple2draw.h"
#include "decompressz80.h"
#include "glk.h"
#include "saga.h"
#include "sagagraphics.h"
#include "vector_common.h"
#include "zx_title.h"
#include "scott.h"
#include "scott_display.h"

#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif

#include "title_image.h"

static void ResizeTitleImage(void)
{
    glui32 graphheight, optimal_height;
#ifdef SPATTERLIGHT
    glk_window_set_background_color(Graphics, gbgcol);
    glk_window_clear(Graphics);
#endif
    FitPictureToGraphicsWindow(&graphheight, &optimal_height);
    y_offset = ((int)graphheight - (int)optimal_height) / 3;
}

/* Replace the whole window tree with a single full-screen graphics window,
   plus a thin text buffer below it if the Glk library can't take key input
   in a graphics window. If background_color is non-NULL, it receives the
   background colour of the closed main window (left untouched if there was
   none). */
static void OpenTitleWindows(glui32 *background_color)
{
    Top = FindGlkWindowWithRock(GLK_STATUS_ROCK);
    if (Top) {
        glk_window_close(Top, NULL);
        Top = NULL;
    }

    Bottom = FindGlkWindowWithRock(GLK_BUFFER_ROCK);
    if (Bottom) {
        if (background_color)
            glk_style_measure(Bottom, style_Normal, stylehint_BackColor,
                background_color);
        glk_window_close(Bottom, NULL);
        Bottom = NULL;
    }

    Graphics = glk_window_open(0, 0, 0, wintype_Graphics, GLK_GRAPHICS_ROCK);

    if (glk_gestalt_ext(gestalt_GraphicsCharInput, 0, NULL, 0) == 0)
        Bottom = glk_window_open(Graphics, winmethod_Below | winmethod_Fixed,
            2, wintype_TextBuffer, GLK_BUFFER_ROCK);
}

/* Tear down the title windows and rebuild the normal main and status
   windows. */
static void CloseTitleWindows(void)
{
    glk_window_close(Graphics, NULL);
    Graphics = NULL;
    Bottom = FindGlkWindowWithRock(GLK_BUFFER_ROCK);
    if (Bottom != NULL)
        glk_window_close(Bottom, NULL);
    Bottom = glk_window_open(0, 0, 0, wintype_TextBuffer, GLK_BUFFER_ROCK);
    if (Bottom == NULL)
        glk_exit();
    glk_set_window(Bottom);
    OpenTopWindow();
}

/* Draw the US title picture (room image 99) fitted to the window */
static void DrawUSTitle(void)
{
    ResizeTitleImage();
    glk_window_clear(Graphics);
    DrawUSRoom(99);
    if (USImages->systype == SYS_APPLE2_LINES || USImages->systype == SYS_ATARI8_LINES) {
        DrawUSRoomObject(255);
    }
    DrawImageOrVector();
}

static void wait_for_key_on_title_screen(void) {

    if (Graphics != NULL) {
        glk_request_char_event(Graphics);
    } else if (Bottom != NULL) {
        glk_request_char_event(Bottom);
    } else {
        return;
    }

    event_t ev;
    do {
        glk_select(&ev);
        if (ev.type == evtype_Arrange) {
#ifdef SPATTERLIGHT
            if (!gli_enable_graphics)
                break;
#endif
            int stored_slowdraw = gli_slowdraw;
            gli_slowdraw = 0;
            DrawUSTitle();
            gli_slowdraw = stored_slowdraw;
        } else if (ev.type == evtype_Timer) {
            if (DrawingVector()) {
                DrawSomeVectorPixels((VectorState == NO_VECTOR_IMAGE));
            }
        }
    } while (ev.type != evtype_CharInput);
}

/* The loading screen is, in practice, always the first standard-length
   (6912-byte) data block on the tape. ExtractTapePayloads() returns only
   the 0xFF data blocks (the 0x00 header blocks are skipped) with the
   offset and length of each, so we just return the first that is exactly
   one SCREEN$ in size. */
uint8_t *FindTapeLoadingScreen(const uint8_t *raw, size_t raw_len, int is_tzx)
{
#define MAX_TAPE_BLOCKS 64
    size_t out_len = 0;
    size_t blk_off[MAX_TAPE_BLOCKS], blk_len[MAX_TAPE_BLOCKS];
    int n_blk = 0;

    uint8_t *payloads = ExtractTapePayloads(raw, raw_len, is_tzx, &out_len,
        blk_off, blk_len, &n_blk, MAX_TAPE_BLOCKS);
    if (!payloads)
        return NULL;

    uint8_t *screen = NULL;
    for (int i = 0; i < n_blk; i++) {
        if (blk_len[i] == ZX_SCREEN_SIZE) {
            screen = MemAlloc(ZX_SCREEN_SIZE);
            memcpy(screen, payloads + blk_off[i], ZX_SCREEN_SIZE);
            break;
        }
    }
    free(payloads);
    return screen;
}

void DrawZXTitleImage(void)
{
#ifdef SPATTERLIGHT
    if (!gli_enable_graphics)
        return;
#endif
    if (!ZXLoadingScreen)
        return;

    /* Force the ZX palette, whatever the game's own platform palette is. */
    palette_type storedpal = palchosen;
    palchosen = ZXOPT;
    DefinePalette();

    OpenTitleWindows(NULL);

    if (Graphics) {
#ifdef SPATTERLIGHT
        glk_window_set_background_color(Graphics, gbgcol);
#endif
        glk_window_clear(Graphics);

        /* Reveal the picture slowly when the slow-draw setting is on,
           otherwise paint it at once. The reveal is linear, which is the
           order a real Spectrum loaded it from tape: Scott Adams Spectrum
           games use ordinary loaders. The picture sits a third of the way
           down the window. */
        ZXTitle title = { Graphics, pal, 3, 0, 0, 0 };
        ZXTitleShow(&title, ZXLoadingScreen, Graphics, gli_slowdraw, NULL, 0);
    }

    CloseTitleWindows();

    /* Restore the game's own palette. */
    palchosen = storedpal;
    DefinePalette();

    free(ZXLoadingScreen);
    ZXLoadingScreen = NULL;
}

#define RTPI_TITLE_LINES 24

static const char **GetRTPILines(const char *text) {
    char *lines[RTPI_TITLE_LINES];
    size_t linelenghts[RTPI_TITLE_LINES];
    uint8_t line[128];
    int lineidx = 0;
    int pos = 0;
    int linepos = 0;
    while (lineidx < RTPI_TITLE_LINES && text[pos] != '\0') {
        // Skip leading spaces
        while (text[pos] == ' ') {
            pos++;
        }
        while (text[pos] != '\n' && text[pos] != '\r' && text[pos] != '\0') {
            line[linepos++] = text[pos++];
        }
        pos++;
        lines[lineidx] = MemAlloc(linepos + 2);
        memcpy(lines[lineidx], line, linepos);
        lines[lineidx][linepos] = '\n';
        lines[lineidx][linepos + 1] = '\0';
        linelenghts[lineidx] = linepos + 2;
        lineidx++;
        linepos = 0;
    }

    if (lineidx < RTPI_TITLE_LINES) {
        for (int j = 0; j < lineidx; j++) {
            free (lines[j]);
        }
        return NULL;
    }

    const char **outlines = MemAlloc(RTPI_TITLE_LINES * sizeof(char *));

    static const uint8_t order[RTPI_TITLE_LINES] =
    { 1, 2, 4, 3, 5, 8, 9, 11, 12, 14, 0, 99, 13, 15, 17, 19, 0, 99, 18, 20, 21, 23, 22, 0 };

    for (int j = 0; j < RTPI_TITLE_LINES; j++) {
        if (order[j] == 99) {
            outlines[j] = NULL;
        } else {
            outlines[j] = lines[order[j]];
        }
    }
    return outlines;
}

/*

0. (HIT ANY KEY)
1. Texas Instruments and
2. Adventure International
3.
4. Proudly present
5. Scott Adams Graphic Adventure
6.
7.
8. “RETURN TO PIRATE’S ISLE”
9.
10.
11. Copyright 1983
12. by Scott Adams
13. Written by Scott Adams.
14.
15. With special assistance from: Eric White, Scott Smith, and Linda Paladino.
16.
17. With love to my Mom who made it all possible!
18. The next picture you see is correct!
19.
20. Do not try to adjust your TV. Just use your Adventuring skills!
21.
22.
23. GOOD LUCK !

*/

static void initRTPITitle(void) {
    if (!title_screen)
        return;
    Graphics = FindGlkWindowWithRock(GLK_GRAPHICS_ROCK);
    if (!Graphics)
        return;
    Bottom = FindGlkWindowWithRock(GLK_BUFFER_ROCK);
    if (Bottom)
        glk_window_close(Bottom, NULL);
    glk_stylehint_set(wintype_TextBuffer, style_User2, stylehint_Justification, stylehint_just_Centered);
    Bottom = glk_window_open(Graphics, winmethod_Below | winmethod_Proportional, 50, wintype_TextBuffer, GLK_BUFFER_ROCK);
}

static void RTPITitle(void) {
    glk_stream_set_current(glk_window_get_stream(Bottom));
    const char **lines = GetRTPILines(title_screen);
    free((void *)title_screen);
    if (!lines)
        return;
    glk_set_style(style_User2);
    for (int i = 0; i < RTPI_TITLE_LINES; i++) {
        if (lines[i] == NULL) {
            wait_for_key_on_title_screen();
            glk_window_clear(Bottom);
        } else {
            Output(lines[i]);
        }
    }
    glk_set_style(style_Normal);
}

void DrawTitleImageScott(void)
{
    int storedwidth = ImageWidth;
    int storedheight = ImageHeight;
#ifdef SPATTERLIGHT
    if (!gli_enable_graphics)
        return;
#endif
    glui32 background_color = -1;
    OpenTitleWindows(&background_color);

    initRTPITitle();

    if (background_color != -1) {
        glk_window_set_background_color(Graphics, background_color);
        glk_window_clear(Graphics);
    }

    ResizeTitleImage();

    if (DrawUSRoom(99)) {
        DrawUSTitle();

        if (CurrentGame == RETURN_TO_PIRATES_ISLE) {
            RTPITitle();
        }

        wait_for_key_on_title_screen();
    }

    CloseTitleWindows();
    OpenGraphicsWindow();
    ResizeTitleImage();
    ImageWidth = storedwidth;
    ImageHeight = storedheight;
    y_offset = 0;
    CloseGraphicsWindow();
}

void PrintTitleScreenBuffer(void)
{
    glk_stream_set_current(glk_window_get_stream(Bottom));
    glk_set_style(style_User1);
    glk_window_clear(Graphics);
    Output(title_screen);
    free((void *)title_screen);
    glk_set_style(style_Normal);
    HitEnter();
    glk_window_clear(Graphics);
}

void PrintTitleScreenGrid(void)
{
    int title_length = strlen(title_screen);
    int rows = 0;

    // Count rows
    for (int i = 0; i < title_length; i++)
        if (title_screen[i] == '\n')
            rows++;

    winid_t titlewin = glk_window_open(Bottom, winmethod_Above | winmethod_Fixed, rows + 2,
        wintype_TextGrid, 0);
    glui32 width, height;
    glk_window_get_size(titlewin, &width, &height);

    // If the screen size is too small to fit the entire text,
    // just flush the text to the buffer window and be done with it.
    if (width < 40 || height < rows + 2) {
        glk_window_close(titlewin, NULL);
        PrintTitleScreenBuffer();
        return;
    }
    int offset = (width - 40) / 2;
    int pos = 0;
    char row[40];
    row[39] = 0;
    for (int i = 1; i <= rows; i++) {
        glk_window_move_cursor(titlewin, offset, i);
        while (title_screen[pos] != '\n' && pos < title_length)
            Display(titlewin, "%c", title_screen[pos++]);
        pos++;
    }
    free((void *)title_screen);
    HitEnter();
    glk_window_close(titlewin, NULL);
}
