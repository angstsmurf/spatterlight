//
//  titleimage.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-10-01.
//

#include <stdlib.h>
#include <string.h>

#include "apple2draw.h"
#include "glk.h"
#include "saga.h"
#include "sagagraphics.h"
#include "vector_common.h"
#include "scott.h"

#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif

#include "titleimage.h"

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

const char **GetRTPILines(const char *text) {
    char *lines[24];
    size_t linelenghts[24];
    uint8_t line[128];
    int lineidx = 0;
    int pos = 0;
    int linepos = 0;
    while (lineidx < 24 && text[pos] != '\0') {
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

    if (lineidx < 26) {
        for (int j = 0; j < lineidx; j++) {
            free (lines[j]);
        }
        return NULL;
    }

    const char **outlines = MemAlloc(26 * sizeof(char *));

    static const uint8_t order[26] =
    { 1, 2, 4, 3, 5, 8, 9, 11, 12, 14, 99, 14, 13, 15, 17, 19, 99, 19, 18, 20, 21, 23, 22, 99, 22, 0 };

    for (int j = 0; j < 26; j++) {
        if (order[j] == 99) {
            outlines[j] = "*\n";
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

void RTPITitle(void) {
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
    glk_stream_set_current(glk_window_get_stream(Bottom));
    const char **lines = GetRTPILines(title_screen);
    free((void *)title_screen);
    if (!lines)
        return;
    glk_set_style(style_User2);
    for (int i = 0; i < 26; i++) {
        Output(lines[i]);
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

    if (CurrentGame == RETURN_TO_PIRATES_ISLE) {
        RTPITitle();
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
