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
#include "scott.h"
#include "scott_display.h"

#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif

#include "title_image.h"

void ResizeTitleImage(void)
{
    glui32 graphwidth, graphheight, optimal_width, optimal_height;
#ifdef SPATTERLIGHT
    glk_window_set_background_color(Graphics, gbgcol);
    glk_window_clear(Graphics);
#endif
    glk_window_get_size(Graphics, &graphwidth, &graphheight);
    pixel_size = OptimalPictureSize(graphwidth, graphheight, &optimal_width, &optimal_height);
    x_offset = ((int)graphwidth - (int)optimal_width) / 2;
    right_margin = optimal_width + x_offset;
    y_offset = ((int)graphheight - (int)optimal_height) / 3;
}

void InitTitleImage(void) {
#ifdef SPATTERLIGHT
    if (!gli_enable_graphics)
        return;
#endif
    Top = FindGlkWindowWithRock(GLK_STATUS_ROCK);
    if (Top) {
        glk_window_close(Top, NULL);
        Top = NULL;
    }

    glui32 background_color = -1;

    Bottom = FindGlkWindowWithRock(GLK_BUFFER_ROCK);
    if (Bottom) {
        glk_style_measure(Bottom, style_Normal, stylehint_BackColor,
                          &background_color);
        glk_window_close(Bottom, NULL);
    }

    Graphics = glk_window_open(0, 0, 0, wintype_Graphics, GLK_GRAPHICS_ROCK);

    if (glk_gestalt_ext(gestalt_GraphicsCharInput, 0, NULL, 0)) {
        glk_request_char_event(Graphics);
    } else {
        Bottom = glk_window_open(Graphics, winmethod_Below | winmethod_Fixed,
                                 2, wintype_TextBuffer, GLK_BUFFER_ROCK);
        glk_request_char_event(Bottom);
    }

    if (background_color != -1) {
        glk_window_set_background_color(Graphics, background_color);
        glk_window_clear(Graphics);
    }

    ResizeTitleImage();
}

void wait_for_key_on_title_screen(void) {

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
            ResizeTitleImage();
            glk_window_clear(Graphics);
            DrawUSRoom(99);
            if (USImages->systype == SYS_APPLE2_LINES || USImages->systype == SYS_ATARI8_LINES) {
                DrawUSRoomObject(255);
            }
            DrawImageOrVector();
            gli_slowdraw = stored_slowdraw;
        } else if (ev.type == evtype_Timer) {
            if (DrawingVector()) {
                DrawSomeVectorPixels((VectorState == NO_VECTOR_IMAGE));
            }
        }
    } while (ev.type != evtype_CharInput);
}

/* A ZX Spectrum loading screen (SCREEN$) is a 6912-byte dump of the
   Spectrum display file: 6144 bytes of bitmap followed by 768 bytes of
   colour attributes (one byte per 8x8 cell). The bitmap is laid out in
   the Spectrum's characteristic non-linear order, so the byte holding a
   given pixel row is found via the address scramble below. */
#define ZX_BITMAP_SIZE 6144
#define ZX_SCREEN_WIDTH 256
#define ZX_SCREEN_HEIGHT 192

/* A snapshot captured at a BASIC/text prompt (e.g. "Resume a saved
   game?") has no picture: every attribute cell holds the default black
   ink on white paper (0x38, ignoring the bright and flash bits). Such a
   screen isn't worth showing as a title, so callers discard it. */
int ZXScreenIsBlackOnWhite(const uint8_t *scr)
{
    if (!scr)
        return 1;
    for (int i = 0; i < ZX_SCREEN_SIZE - ZX_BITMAP_SIZE; i++)
        if ((scr[ZX_BITMAP_SIZE + i] & 0x3f) != 0x38)
            return 0;
    return 1;
}

/* Draw the 8 pixels of one bitmap byte (display-file address bmaddr) using the
   attribute currently in scr for that cell. */
static void DrawZXByte(const uint8_t *scr, uint16_t bmaddr)
{
    int offset = bmaddr - 0x4000;
    int col = offset & 0x1f;
    int y = ((offset & 0x0700) >> 8) | ((offset & 0x00e0) >> 2) | ((offset & 0x1800) >> 5);

    uint8_t bits = scr[offset];
    uint8_t attr = scr[ZX_BITMAP_SIZE + (y >> 3) * 32 + col];
    int bright = (attr & 0x40) ? 8 : 0;
    int ink = (attr & 0x07) + bright;
    int paper = ((attr >> 3) & 0x07) + bright;
    /* The flash bit (0x80) is ignored for a static title image. */

    for (int b = 0; b < 8; b++) {
        int set = (bits >> (7 - b)) & 1;
        PutPixel(col * 8 + b, y, set ? ink : paper);
    }
}

/* Redraw the cell at a screen address: one bitmap byte, or (for an attribute
   address) the whole 8x8 character it colours. */
static void RedrawZXCell(const uint8_t *scr, uint16_t addr)
{
    if (addr < 0x5800) {
        DrawZXByte(scr, addr);
        return;
    }
    int cell = addr - 0x5800;
    int crow = cell >> 5, ccol = cell & 0x1f;
    for (int py = 0; py < 8; py++) {
        int y = crow * 8 + py;
        int offset = ((y & 0xc0) << 5) | ((y & 0x38) << 2) | ((y & 0x07) << 8) | ccol;
        DrawZXByte(scr, (uint16_t)(0x4000 + offset));
    }
}

static void DrawZXScreen(const uint8_t *scr)
{
    if (!scr || !Graphics)
        return;

    for (int y = 0; y < ZX_SCREEN_HEIGHT; y++) {
        int bitmap_row = ((y & 0xc0) << 5) | ((y & 0x38) << 2) | ((y & 0x07) << 8);
        for (int col = 0; col < 32; col++)
            DrawZXByte(scr, (uint16_t)(0x4000 + bitmap_row + col));
    }
}

/* Reveal the screen progressively, in the linear order a real Spectrum loaded
   it from tape (bitmap top-to-bottom, then the attributes), on a timer. A
   keypress dismisses it. Scott Adams Spectrum games use ordinary loaders, so
   the linear order is the authentic one. */
#define ZX_REVEAL_TICK_MS 30
#define ZX_REVEAL_TICKS   120

static void ZXSlowReveal(void)
{
    if (!ZXLoadingScreen || !Graphics)
        return;

    /* Start from the unrevealed background: black bitmap, attributes at the
       screen's uniform fill (its most common attribute byte). */
    static uint8_t work[ZX_SCREEN_SIZE];
    int counts[256] = { 0 };
    for (int i = ZX_BITMAP_SIZE; i < ZX_SCREEN_SIZE; i++)
        counts[ZXLoadingScreen[i]]++;
    int fill = 0;
    for (int v = 1; v < 256; v++)
        if (counts[v] > counts[fill])
            fill = v;
    memset(work, 0x00, ZX_BITMAP_SIZE);
    memset(work + ZX_BITMAP_SIZE, (uint8_t)fill, ZX_SCREEN_SIZE - ZX_BITMAP_SIZE);

    glk_window_clear(Graphics);
    DrawZXScreen(work);

    int per_tick = ZX_SCREEN_SIZE / ZX_REVEAL_TICKS;
    if (per_tick < 1)
        per_tick = 1;
    int pos = 0;

    winid_t keywin = Graphics ? Graphics : Bottom;
    if (keywin == NULL)
        return;
    glk_request_char_event(keywin);
    glk_request_timer_events(ZX_REVEAL_TICK_MS);

    event_t ev;
    do {
        glk_select(&ev);
        if (ev.type == evtype_Timer) {
            int end = pos + per_tick;
            if (end > ZX_SCREEN_SIZE)
                end = ZX_SCREEN_SIZE;
            for (; pos < end; pos++) {
                uint16_t a = (uint16_t)(0x4000 + pos);
                work[pos] = ZXLoadingScreen[pos];
                RedrawZXCell(work, a);
            }
            if (pos >= ZX_SCREEN_SIZE)
                glk_request_timer_events(0);
        } else if (ev.type == evtype_Arrange) {
#ifdef SPATTERLIGHT
            if (!gli_enable_graphics)
                break;
#endif
            ResizeTitleImage();
            glk_window_clear(Graphics);
            DrawZXScreen(work);
        }
    } while (ev.type != evtype_CharInput);

    glk_request_timer_events(0);
}

/* Wait for a keypress on the ZX loading-screen title. Unlike
   wait_for_key_on_title_screen(), the arrange handler redraws the ZX
   screen (there is no US image / vector state to fall back on, so the
   shared version would dereference a NULL USImages). */
static void wait_for_key_on_zx_title(void)
{
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
            ResizeTitleImage();
            glk_window_clear(Graphics);
            DrawZXScreen(ZXLoadingScreen);
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

    int storedwidth = ImageWidth;
    int storedheight = ImageHeight;
    palette_type storedpal = palchosen;

    /* Force the ZX palette (whatever the game's own platform palette is)
       and size the window to the native 256x192 loading-screen geometry. */
    palchosen = ZXOPT;
    DefinePalette();
    ImageWidth = ZX_SCREEN_WIDTH;
    ImageHeight = ZX_SCREEN_HEIGHT;

    Top = FindGlkWindowWithRock(GLK_STATUS_ROCK);
    if (Top) {
        glk_window_close(Top, NULL);
        Top = NULL;
    }

    Bottom = FindGlkWindowWithRock(GLK_BUFFER_ROCK);
    if (Bottom) {
        glk_window_close(Bottom, NULL);
        Bottom = NULL;
    }

    Graphics = glk_window_open(0, 0, 0, wintype_Graphics, GLK_GRAPHICS_ROCK);

    if (glk_gestalt_ext(gestalt_GraphicsCharInput, 0, NULL, 0) == 0)
        Bottom = glk_window_open(Graphics, winmethod_Below | winmethod_Fixed,
            2, wintype_TextBuffer, GLK_BUFFER_ROCK);

    ResizeTitleImage();
    glk_window_clear(Graphics);

    /* Reveal the picture slowly (in tape-load order) when the slow-draw
       setting is on; otherwise paint it at once. */
    int slow = gli_slowdraw;
#ifdef SPATTERLIGHT
    slow = slow && !gli_determinism;
#endif
    if (slow) {
        ZXSlowReveal();
    } else {
        DrawZXScreen(ZXLoadingScreen);
        wait_for_key_on_zx_title();
    }

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

    /* Restore the game's own palette and image geometry. */
    palchosen = storedpal;
    DefinePalette();
    ImageWidth = storedwidth;
    ImageHeight = storedheight;
    y_offset = 0;

    free(ZXLoadingScreen);
    ZXLoadingScreen = NULL;
}

#define RTPI_TITLE_LINES 24

const char **GetRTPILines(const char *text) {
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

void initRTPITitle(void) {
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

void RTPITitle(void) {
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
    Top = FindGlkWindowWithRock(GLK_STATUS_ROCK);
    if (Top) {
        glk_window_close(Top, NULL);
        Top = NULL;
    }

    glui32 background_color = -1;

    Bottom = FindGlkWindowWithRock(GLK_BUFFER_ROCK);
    if (Bottom) {
        glk_style_measure(Bottom, style_Normal, stylehint_BackColor,
            &background_color);
        glk_window_close(Bottom, NULL);
    }

    Graphics = glk_window_open(0, 0, 0, wintype_Graphics, GLK_GRAPHICS_ROCK);

    if (glk_gestalt_ext(gestalt_GraphicsCharInput, 0, NULL, 0) == 0) {
        Bottom = glk_window_open(Graphics, winmethod_Below | winmethod_Fixed,
            2, wintype_TextBuffer, GLK_BUFFER_ROCK);
    }

    initRTPITitle();

    if (background_color != -1) {
        glk_window_set_background_color(Graphics, background_color);
        glk_window_clear(Graphics);
    }

    ResizeTitleImage();

    if (DrawUSRoom(99)) {
        ResizeTitleImage();
        glk_window_clear(Graphics);

        DrawUSRoom(99);
        if (USImages->systype == SYS_APPLE2_LINES || USImages->systype == SYS_ATARI8_LINES) {
            DrawUSRoomObject(255);
        }
        DrawImageOrVector();

        if (CurrentGame == RETURN_TO_PIRATES_ISLE) {
            RTPITitle();
        }

        wait_for_key_on_title_screen();
    }

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
