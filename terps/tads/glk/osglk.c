/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
 * Copyright (C) 2010-2011 by Ben Cressey.                                    *
 *                                                                            *
 * This file is part of Gargoyle.                                             *
 *                                                                            *
 * Gargoyle is free software; you can redistribute it and/or modify           *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation; either version 2 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * Gargoyle is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with Gargoyle; if not, write to the Free Software                    *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 *****************************************************************************/

/* osglk.c -- glk os adapter */

#include <assert.h>

#include "os.h"
#include "glk.h"
#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif
#include "oshtml.h"
#include "osstyles.h"
#include "ossound.h"

/* for version strings */
#include "trd.h"
#include "vmvsn.h"

extern glui32 os_parse_chars(unsigned char *buf, glui32 buflen,
                             glui32 *out, glui32 outlen);

static void redraw_windows(void);
static void os_status_redraw(void);
extern void os_banners_redraw(void);

static glui32 lbuf[256];
static size_t lbuflen = 0;
static char rbuf[256];
static int curwin = 0;
static int curattr = 0;

winid_t mainwin;
winid_t statuswin;

/*
 *   whether we echo the lines the player enters, not the window: then a
 *   click on a link while a line is read can cancel it without a newline
 *   after the prompt, and show the link's command there instead
 */
static int self_echo;

glui32 mainfg;
glui32 mainbg;

glui32 statusfg;
glui32 statusbg;


/* ------------------------------------------------------------------------ */
/*
 *   Get system information.  'code' is a SYSINFO_xxx code, which
 *   specifies what type of information to get.  The 'param' argument's
 *   meaning depends on which code is selected.  'result' is a pointer to
 *   an integer that is to be filled in with the result value.  If the
 *   code is not known, this function should return FALSE.  If the code is
 *   known, the function should fill in *result and return TRUE.
 */
static int osglk_links_ok(void);

int os_get_sysinfo(int code, void *param, long *result)
{
    switch (code)
    {
        case SYSINFO_TEXT_HILITE:
            *result = 1;
            return TRUE;
        case SYSINFO_BANNERS:
            *result = 1;
            return TRUE;
        case SYSINFO_TEXT_COLORS:
            *result = SYSINFO_TXC_RGB;
            return TRUE;

        /*
         *   Games get HTML: adv.t then puts its status line in a <BANNER>,
         *   which oshtml.c and osglkban.c show, and adv3 (which keeps to
         *   the banner API) writes its links, such as a menu's Exit
         */
        case SYSINFO_INTERP_CLASS:
            *result = SYSINFO_ICLASS_HTML;
            return TRUE;
        case SYSINFO_HTML:
            *result = 1;
            return TRUE;

        /*
         *   oshtml.c draws <IMG> pictures, in the main window and in
         *   banners of both kinds.  Glk has no MNG, and no gestalt for it,
         *   but the Spatterlight app turns an MNG into a GIF it animates.
         */
        case SYSINFO_JPEG:
        case SYSINFO_PNG:
        case SYSINFO_PNG_TRANS:
        case SYSINFO_PNG_ALPHA:
        case SYSINFO_PREF_IMAGES:
#ifdef SPATTERLIGHT
        case SYSINFO_MNG:
        case SYSINFO_MNG_TRANS:
        case SYSINFO_MNG_ALPHA:
#endif
            *result = glk_gestalt(gestalt_Graphics, 0)
                && glk_gestalt(gestalt_DrawImage, wintype_TextBuffer);
            return TRUE;

        case SYSINFO_PREF_LINKS:
            *result = osglk_links_ok();
            return TRUE;

        /*
         *   ossound.c plays <SOUND>, in all four formats, any of them at
         *   once, and fades them all
         */
        case SYSINFO_WAV:
        case SYSINFO_MIDI:
        case SYSINFO_WAV_MIDI_OVL:
        case SYSINFO_WAV_OVL:
        case SYSINFO_PREF_SOUNDS:
        case SYSINFO_PREF_MUSIC:
        case SYSINFO_MPEG:
        case SYSINFO_MPEG1:
        case SYSINFO_MPEG2:
        case SYSINFO_MPEG3:
        case SYSINFO_OGG:
            *result = glk_gestalt(gestalt_Sound2, 0);
            return TRUE;
        case SYSINFO_AUDIO_FADE:
        case SYSINFO_AUDIO_CROSSFADE:
            *result = glk_gestalt(gestalt_Sound2, 0)
                ? SYSINFO_AUDIOFADE_MPEG | SYSINFO_AUDIOFADE_OGG
                  | SYSINFO_AUDIOFADE_WAV | SYSINFO_AUDIOFADE_MIDI : 0;
            return TRUE;

        case SYSINFO_LINKS_HTTP:
        case SYSINFO_LINKS_FTP:
        case SYSINFO_LINKS_NEWS:
        case SYSINFO_LINKS_MAILTO:
        case SYSINFO_LINKS_TELNET:
            *result = 0;
            return TRUE;

        default:
            return FALSE;
    }
}


/* ------------------------------------------------------------------------ */

/*
 *   The colours of the main window and of a text grid, which the banners
 *   take.  They follow the theme, so we measure them again when it changes
 *   (an arrange event); without a status line, on a grid opened for it.
 */
static void measure_colors(void)
{
    winid_t grid = statuswin;

    if (!glk_style_measure(mainwin, style_Normal, stylehint_TextColor, &mainfg))
        mainfg = 0;

    if (!glk_style_measure(mainwin, style_Normal, stylehint_BackColor, &mainbg))
        mainbg = 0;

    if (!grid)
        grid = glk_window_open(mainwin, winmethod_Above | winmethod_Fixed, 1,
                               wintype_TextGrid, 0);

    if (!glk_style_measure(grid, style_Normal, stylehint_TextColor, &statusfg))
        statusfg = 0;

    if (!glk_style_measure(grid, style_Normal, stylehint_BackColor, &statusbg))
        statusbg = 0;

    if (grid != statuswin)
        glk_window_close(grid, 0);
}

/*
 *   Initialize.  This should be called during program startup to
 *   initialize the OS layer and check OS-specific command-line arguments.
 *
 *   If 'prompt' and 'buf' are non-null, and there are no arguments on the
 *   given command line, the OS code can use the prompt to ask the user to
 *   supply a filename, then store the filename in 'buf' and set up
 *   argc/argv to give a one-argument command string.  (This mechanism for
 *   prompting for a filename is obsolescent, and is retained for
 *   compatibility with a small number of existing implementations only;
 *   new implementations should ignore this mechanism and leave the
 *   argc/argv values unchanged.)
 */
int os_init(int *argc, char *argv[], const char *prompt,
            char *buf, int bufsiz)
{
    /* hints only reach windows opened after them */
    osstyles_set_hints(wintype_TextBuffer);
    mainwin = glk_window_open(0, 0, 0, wintype_TextBuffer, 0);

    if (!mainwin)
    {
        fprintf(stderr, "fatal: could not open window!\n");
        glk_exit();
    }

#ifdef GLK_MODULE_LINE_ECHO
    if (glk_gestalt(gestalt_LineInputEcho, 0))
    {
        glk_set_echo_line_event(mainwin, 0);
        self_echo = 1;
    }
#endif

    measure_colors();

    glk_set_window(mainwin);

    strcpy(rbuf, "");

    return 0;
}

/*
 *   Uninitialize.  This is called prior to progam termination to reverse
 *   the effect of any changes made in os_init().  For example, if
 *   os_init() put the terminal in raw mode, this should restore the
 *   previous terminal mode.  This routine should not terminate the
 *   program (so don't call exit() here) - the caller might have more
 *   processing to perform after this routine returns.
 */
void os_uninit(void)
{
}

void os_term(int status)
{
    glk_exit();
}

/* ------------------------------------------------------------------------ */
/*
 *   Display routines.
 *
 *   Our display model is a simple stdio-style character stream.
 *
 *   In addition, we provide an optional "status line," which is a
 *   non-scrolling area where a line of text can be displayed.  If the status
 *   line is supported, text should only be displayed in this area when
 *   os_status() is used to enter status-line mode (mode 1); while in status
 *   line mode, text is written to the status line area, otherwise (mode 0)
 *   it's written to the normal main text area.  The status line is normally
 *   shown in a different color to set it off from the rest of the text.
 *
 *   The OS layer can provide its own formatting (word wrapping in
 *   particular) if it wants, in which case it should also provide pagination
 *   using os_more_prompt().
 */

/*
 *   Print a string on the console.  These routines come in two varieties:
 *
 *   os_printz - write a NULL-TERMINATED string
 *.  os_print - write a COUNTED-LENGTH string, which may not end with a null
 *
 *   These two routines are identical except that os_printz() takes a string
 *   which is terminated by a null byte, and os_print() instead takes an
 *   explicit length, and a string that may not end with a null byte.
 *
 *   os_printz(str) may be implemented as simply os_print(str, strlen(str)).
 *
 *   The string is written in one of three ways, depending on the status mode
 *   set by os_status():
 *
 *   status mode == 0 -> write to main text window
 *.  status mode == 1 -> write to status line
 *.  anything else -> do not display the text at all
 *
 *   Implementations are free to omit any status line support, in which case
 *   they should simply suppress all output when the status mode is anything
 *   other than zero.
 *
 *   The following special characters must be recognized in the displayed
 *   text:
 *
 *   '\n' - newline: end the current line and move the cursor to the start of
 *   the next line.  If the status line is supported, and the current status
 *   mode is 1 (i.e., displaying in the status line), then two special rules
 *   apply to newline handling: newlines preceding any other text should be
 *   ignored, and a newline following any other text should set the status
 *   mode to 2, so that all subsequent output is suppressed until the status
 *   mode is changed with an explicit call by the client program to
 *   os_status().
 *
 *   '\r' - carriage return: end the current line and move the cursor back to
 *   the beginning of the current line.  Subsequent output is expected to
 *   overwrite the text previously on this same line.  The implementation
 *   may, if desired, IMMEDIATELY clear the previous text when the '\r' is
 *   written, rather than waiting for subsequent text to be displayed.
 *
 *   All other characters may be assumed to be ordinary printing characters.
 *   The routine need not check for any other special characters.
 *
 */

void os_printz(const char *str)
{
    os_print(str, strlen(str));
}

/*
 *   Open the status line of text mode, if it isn't open; in HTML mode, only
 *   for text that isn't all spaces ('text' null: in any case)
 */
static void open_statuswin(const glui32 *text, size_t len)
{
    size_t i;

    if (statuswin)
        return;
    if (text && oshtml_is_html())
    {
        for (i = 0 ; i < len && (text[i] == ' ' || text[i] == '\n') ; ++i)
            ;
        if (i == len)
            return;
    }
    glk_stylehint_set(wintype_TextGrid, style_User1, stylehint_ReverseColor, 1);
    statuswin = glk_window_open(mainwin, winmethod_Above | winmethod_Fixed, 1,
                                wintype_TextGrid, 0);
}

void os_print(const char *str, size_t len)
{
    osglk_not_idle();

    if (curwin == 0 && str)
        oshtml_print_main(str, len);

    if (curwin == 2 && str)
    {
        glui32 none;
        oshtml_print_status(str, len, &none, 0);
    }

    if (curwin == 1)
    {
        glui32 buf[256];
        const glui32 *p;
        size_t rem, max;

        rem = oshtml_print_status(str, len, buf, sizeof(buf) / sizeof(buf[0]));

        /* The string requires some fiddling for the status window */
        for (p = buf ; rem != 0 && *p == '\n'; p++, --rem)
            ;
        if (rem != 0 && p[rem-1] == '\n')
            --rem;

        /* if that leaves anything, update the statusline */
        if (rem != 0)
        {
            open_statuswin(p, rem);
            max = sizeof(lbuf) / sizeof(lbuf[0]) - lbuflen - 1;
            if (rem > max)
                rem = max;
            memcpy(lbuf + lbuflen, p, rem * sizeof(glui32));
            lbuflen += rem;
            os_status_redraw();
        }
    }
}


/*
 *   Set the status line mode.  There are three possible settings:
 *
 *   0 -> main text mode.  In this mode, all subsequent text written with
 *   os_print() and os_printz() is to be displayed to the main text area.
 *   This is the normal mode that should be in effect initially.  This mode
 *   stays in effect until an explicit call to os_status().
 *
 *   1 -> statusline mode.  In this mode, text written with os_print() and
 *   os_printz() is written to the status line, which is usually rendered as
 *   a one-line area across the top of the terminal screen or application
 *   window.  In statusline mode, leading newlines ('\n' characters) are to
 *   be ignored, and any newline following any other character must change
 *   the mode to 2, as though os_status(2) had been called.
 *
 *   2 -> suppress mode.  In this mode, all text written with os_print() and
 *   os_printz() must simply be ignored, and not displayed at all.  This mode
 *   stays in effect until an explicit call to os_status().
 */

void os_status(int stat)
{
    curwin = stat;

    if (stat == 1)
    {
        /* in HTML mode, the status line is usually a <BANNER> instead */
        if (!oshtml_is_html())
            open_statuswin(NULL, 0);
        lbuflen = 0;
    }
}

/* get the status line mode */
int os_get_status()
{
    return curwin;
}

/*
 *   Set the score value.  This displays the given score and turn counts on
 *   the status line.  In most cases, these values are displayed at the right
 *   edge of the status line, in the format "score/turns", but the format is
 *   up to the implementation to determine.  In most cases, this can simply
 *   be implemented as follows:
 *
 */
void os_score(int score, int turncount)
{
    char buf[40];
    snprintf(buf, sizeof(buf), "%d/%d", score, turncount);
    os_strsc(buf);
}

/* display a string in the score area in the status line */
void os_strsc(const char *p)
{
    snprintf(rbuf, sizeof rbuf, "%s", p);
    os_status_redraw();
}

/* append to the status line being composed, up to 255 characters */
static void status_add(glui32 *buf, size_t *len, const glui32 *s, size_t n)
{
    size_t max = 255 - *len;

    if (n > max)
        n = max;
    memcpy(buf + *len, s, n * sizeof(glui32));
    *len += n;
}

/* " left part, padded to the score   score " */
static void os_status_redraw(void)
{
    glui32 buf[256];
    glui32 score[256];
    glui32 space = ' ';
    size_t len = 0, scorelen, n;
    glui32 wid;
    int div;

    if (!statuswin)
        return;

    glk_window_get_size(statuswin, &wid, NULL);
    div = (int)(wid - strlen(rbuf) - 3);

    scorelen = os_parse_chars((unsigned char *)rbuf, (glui32)strlen(rbuf),
                              score, sizeof(score) / sizeof(score[0]));
    if (scorelen == 0)
        for ( ; rbuf[scorelen] != '\0' ; ++scorelen)
            score[scorelen] = (unsigned char)rbuf[scorelen];

    status_add(buf, &len, &space, 1);
    status_add(buf, &len, lbuf, lbuflen);
    for (n = lbuflen ; (int)n < div ; ++n)
        status_add(buf, &len, &space, 1);
    status_add(buf, &len, &space, 1);
    status_add(buf, &len, score, scorelen);
    status_add(buf, &len, &space, 1);

    glk_window_clear(statuswin);
    glk_set_window(statuswin);
    glk_set_style(style_User1);
    glk_put_buffer_uni(buf, (glui32)len);
    glk_set_window(mainwin);
}

/* \H+ and \H- */
void os_start_html(void)
{
    oshtml_start();
}

void os_end_html(void)
{
    oshtml_end();
}

/* ------------------------------------------------------------------------ */
/*
 *   Pixels and colours beyond the Glk API (oshtml.h)
 */
#ifdef OSGLK_PIXELS
#ifdef SPATTERLIGHT
glui32 osglk_px_width(winid_t win)
{
    int w = win->bbox.x1 - win->bbox.x0;

    if (glk_window_get_type(win) == wintype_TextBuffer)
        w -= 2 * gbuffermarginx;
    return w > 0 ? (glui32)w : 0;
}

glui32 osglk_px_root_width(void)
{
    winid_t root = glk_window_get_root();
    return root ? (glui32)(root->bbox.x1 - root->bbox.x0) : 0;
}

void osglk_px_margins(winid_t win, glui32 *x, glui32 *y)
{
    (void)win;
    if (x)
        *x = (glui32)gbuffermarginx;
    if (y)
        *y = (glui32)gbuffermarginy;
}

double osglk_px_cellw(void)
{
    return gbufcellw;
}

double osglk_px_cellh(void)
{
    return gbufcellh;
}

int osglk_px_lines(winid_t win)
{
    return win_lines(win->peer);
}

int osglk_px_text_height(winid_t win)
{
    return win_text_height(win->peer);
}
#else /* GLK_MODULE_GARGLKWINSIZE */
glui32 osglk_px_width(winid_t win)
{
    glui32 w = 0;

    if (glk_window_get_type(win) == wintype_Graphics)
        glk_window_get_size(win, &w, NULL);
    else
        garglk_window_get_size_pixels(win, &w, NULL);
    return w;
}

/* a graphics window of no height across the top is as wide as it is */
glui32 osglk_px_root_width(void)
{
    winid_t root = glk_window_get_root(), probe;
    glui32 w = 0;

    if (!root)
        return 0;
    probe = glk_window_open(root, winmethod_Above | winmethod_Fixed
                            | winmethod_NoBorder, 0, wintype_Graphics, 0);
    if (!probe)
        return 0;
    glk_window_get_size(probe, &w, NULL);
    glk_window_close(probe, NULL);
    return w;
}

/*
 *   a text buffer's origin is inside its margins, and that of a graphics
 *   window of no size at its top left corner is the corner
 */
void osglk_px_margins(winid_t win, glui32 *x, glui32 *y)
{
    glsi32 tx = 0, ty = 0, gx = 0, gy = 0;
    winid_t probe = NULL;

    if (win && glk_window_get_type(win) == wintype_TextBuffer)
        probe = glk_window_open(win, winmethod_Above | winmethod_Fixed
                                | winmethod_NoBorder, 0, wintype_Graphics, 0);
    if (probe)
    {
        garglk_window_get_origin_pixels(win, &tx, &ty);
        garglk_window_get_origin_pixels(probe, &gx, &gy);
        glk_window_close(probe, NULL);
    }
    if (x)
        *x = tx > gx ? (glui32)(tx - gx) : 0;
    if (y)
        *y = ty > gy ? (glui32)(ty - gy) : 0;
}

double osglk_px_cellw(void)
{
    double w = 0;
    garglk_get_cell_size_pixels(&w, NULL);
    return w;
}

double osglk_px_cellh(void)
{
    double h = 0;
    garglk_get_cell_size_pixels(NULL, &h);
    return h;
}

int osglk_px_lines(winid_t win)
{
    return glk_window_get_type(win) == wintype_TextBuffer
        ? garglk_tads_os_banner_size(win) : 0;
}

int osglk_px_text_height(winid_t win)
{
    (void)win;
    return 0;
}
#endif /* SPATTERLIGHT */
#endif /* OSGLK_PIXELS */

void osglk_set_window_bg(winid_t win, glui32 color)
{
#ifdef SPATTERLIGHT
    win_setbgnd(win->peer, color);
#else
    if (glk_window_get_type(win) == wintype_Graphics)
        glk_window_set_background_color(win, color == zcolor_Default
                                        ? mainbg : color);
#ifdef GARGLK
    /* Gargoyle's paper is the main window's, that of every window */
    else if (win == mainwin)
        garglk_set_zcolors_stream(glk_window_get_stream(win),
                                  zcolor_Current, color);
#endif /* GARGLK */
#endif /* SPATTERLIGHT */
}

/* the main window's background, which the side pictures' match */
static glui32 main_bg = zcolor_Default;

void osglk_zcolors(glui32 fg, glui32 bg)
{
    osglk_zcolors_stream(glk_stream_get_current(), fg, bg);
}

void osglk_zcolors_stream(strid_t str, glui32 fg, glui32 bg)
{
#ifndef SPATTERLIGHT
    /*
     *   Gargoyle's colours are the last ones set in any window: a
     *   background is every window's paper, and a text colour that of all
     *   text without one of its own.  So only the main window's text has a
     *   colour, and only the screen colour sets a background (which a
     *   default one mustn't undo)
     */
    if (!str || !mainwin || str != glk_window_get_stream(mainwin))
        return;
    bg = main_bg == zcolor_Default ? zcolor_Default : zcolor_Current;
#endif
    garglk_set_zcolors_stream(str, fg, bg);
}

/* ------------------------------------------------------------------------ */
/*
 *   Pictures beside the page: the picture alone in a table's first or last
 *   cell, which HTML TADS shows as a border down the side of the text
 *   (1893).  Each goes in a graphics window at the main window's left or
 *   right, as wide as the picture, until the page is cleared.
 */
static struct
{
    winid_t win;
    oshtml_image img;
} side_pics[2];


void osglk_draw_side_picture(winid_t win, const oshtml_image *img,
                             glui32 bg)
{
    glui32 ww, wh, w, h;

    osglk_set_window_bg(win, bg);
    glk_window_clear(win);
    glk_window_get_size(win, &ww, &wh);
    if (!oshtml_image_size(img, ww, &w, &h) || !w || !h)
        return;

    /* the whole of it, if it's taller than the window */
    if (wh && h > wh)
    {
        w = w * wh / h;
        h = wh;
    }
    glk_image_draw_scaled(win, img->id,
                          (glsi32)(ww > w ? (ww - w) / 2 : 0), 0, w, h);
}

static void side_picture_draw(int right)
{
    if (side_pics[right].win)
        osglk_draw_side_picture(side_pics[right].win, &side_pics[right].img,
                                main_bg);
}

static void side_pictures_draw(void)
{
    side_picture_draw(0);
    side_picture_draw(1);
}

int osglk_side_picture(int right, const oshtml_image *img)
{
    glui32 mw, w, h;
    glui32 method = (right ? winmethod_Right : winmethod_Left)
        | winmethod_Fixed | winmethod_NoBorder;

    if (!mainwin || !glk_gestalt(gestalt_DrawImage, wintype_Graphics))
        return 0;

    /* not if it would leave the text too little room */
#ifdef OSGLK_PIXELS
    {
        glui32 mx = 0;
        osglk_px_margins(mainwin, &mx, NULL);
        mw = osglk_px_width(mainwin) + 2 * mx;
    }
#else
    mw = 0;
#endif
    if (!oshtml_image_size(img, mw, &w, &h) || !w || w * 4 > mw)
        return 0;

    right = right != 0;
    if (side_pics[right].win)
        glk_window_set_arrangement(glk_window_get_parent(side_pics[right].win),
                                   method, w, side_pics[right].win);
    else
        side_pics[right].win = glk_window_open(mainwin, method, w,
                                               wintype_Graphics, 0);
    if (!side_pics[right].win)
        return 0;
    side_pics[right].img = *img;
    side_picture_draw(right);
    return 1;
}

static void side_pictures_remove(void)
{
    int i;

    for (i = 0 ; i < 2 ; ++i)
    {
        if (side_pics[i].win)
            glk_window_close(side_pics[i].win, NULL);
        side_pics[i].win = NULL;
    }
}

static void redraw_windows(void)
{
    measure_colors();
    oshtml_arrange();
    os_status_redraw();
    os_banners_redraw();
    side_pictures_draw();
    osglk_tag_banner_redraw();
}

/*
 *   Clear the screen.  As HTML TADS starts a new page, the windows of
 *   <BANNER> tags go too (not those of the banner API), and so do the
 *   pictures beside the page.
 */
void oscls(void)
{
    oshtml_clear_main();
    osglk_tag_banner_remove(NULL);
    side_pictures_remove();
    glk_window_clear(mainwin);
}

/* ------------------------------------------------------------------------ */
/*
 *   Set text attributes.  Text subsequently displayed through os_print() and
 *   os_printz() are to be displayed with the given attributes.
 *
 *   'attr' is a (bitwise-OR'd) combination of OS_ATTR_xxx values.  A value
 *   of zero indicates normal text, with no extra attributes.
 */
void os_set_text_attr(int attr)
{
    curattr = attr;
    oshtml_set_os_attr(attr);
}

/*
 *   Set the text foreground and background colors.  This sets the text
 *   color for subsequent os_printf() and os_vprintf() calls.
 *
 *   The background color can be OS_COLOR_P_TRANSPARENT, in which case the
 *   background color is "inherited" from the current screen background.
 *   Note that if the platform is capable of keeping old text for
 *   "scrollback," then the transparency should be a permanent attribute of
 *   the character - in other words, it should not be mapped to the current
 *   screen color in the scrollback buffer, because doing so would keep the
 *   current screen color even if the screen color changes in the future.
 *
 *   Text color support is optional.  If the platform doesn't support text
 *   colors, this can simply do nothing.  If the platform supports text
 *   colors, but the requested color or attributes cannot be displayed, the
 *   implementation should use the best available approximation.
 */
void os_set_text_color(os_color_t fg, os_color_t bg)
{
    oshtml_set_os_color(fg, bg);
}

/* os_set_text_color, once the HTML parser has caught up */
void osglk_set_text_color(os_color_t fg, os_color_t bg)
{
//    fprintf(stderr, "os_set_text_color fg:%lu (%lx), bg:%lu (%lx)\n", fg, fg, bg, bg);

    int changedfg = 0;
    int changedbg = 0;

    // Fix for Hill Ridge Lost & Found
    if (fg == 0 && bg == OS_COLOR_P_TRANSPARENT) {
//        fprintf(stderr, "Trying to set foreground color to hard black\n");
        fg = zcolor_Default;
        changedfg = 1;
    }

    switch (fg) {
        case OS_COLOR_P_TRANSPARENT:
//            fprintf(stderr, "Trying to set foreground color to OS_COLOR_P_TRANSPARENT\n");
            fg = zcolor_Default;
            changedfg = 1;
            break;
        case OS_COLOR_P_TEXT:
//            fprintf(stderr, "Trying to set foreground color to OS_COLOR_P_TEXT\n");
            fg = zcolor_Default;
            changedfg = 1;
            break;
        case OS_COLOR_P_TEXTBG:
            fprintf(stderr, "Ignoring attempt to set foreground color to OS_COLOR_P_TEXTBG\n");
            break;
        case OS_COLOR_P_STATUSLINE:
            fprintf(stderr, "Ignoring attempt to set foreground color to OS_COLOR_P_STATUSLINE\n");
            break;
        case OS_COLOR_P_STATUSBG:
            fprintf(stderr, "Ignoring attempt to set foreground color to OS_COLOR_P_STATUSBG\n");
            break;
        case OS_COLOR_P_INPUT:
            fprintf(stderr, "Ignoring attempt to set foreground color to OS_COLOR_P_INPUT\n");
            break;
        default:
            changedfg = 1;
            break;
    }

    switch (bg) {
        case OS_COLOR_P_TRANSPARENT:
//            fprintf(stderr, "Trying to set background color to OS_COLOR_P_TRANSPARENT\n");
            bg = zcolor_Default;
            changedbg = 1;
            break;
        case OS_COLOR_P_TEXT:
            fprintf(stderr, "Ignoring attempt to set background color to OS_COLOR_P_TEXT\n");
            break;
        case OS_COLOR_P_TEXTBG:
            fprintf(stderr, "Trying to set background color to OS_COLOR_P_TEXTBG\n");
            bg = zcolor_Default;
            changedbg = 1;
            break;
        case OS_COLOR_P_STATUSLINE:
            fprintf(stderr, "Ignoring attempt to set background color to OS_COLOR_P_STATUSLINE\n");
            break;
        case OS_COLOR_P_STATUSBG:
            fprintf(stderr, "Ignoring attempt to set background color to OS_COLOR_P_STATUSBG\n");
            break;
        case OS_COLOR_P_INPUT:
            fprintf(stderr, "Ignoring attempt to set background color to OS_COLOR_P_INPUT\n");
            break;
        default:
            changedbg = 1;
            break;
    }

    if (changedfg && changedbg) {
        osglk_zcolors((glui32)fg, (glui32)bg);
    } else if (changedfg) {
        osglk_zcolors((glui32)fg, zcolor_Current);
    } else if (changedbg) {
        osglk_zcolors(zcolor_Current, (glui32)bg);
    }
}

/*
 *   Set the screen background color.  This sets the text color for the
 *   background of the screen.  If possible, this should immediately redraw
 *   the main text area with this background color.  The color is given as an
 *   OS_COLOR_xxx value.
 *
 *   If the platform is capable of redisplaying the existing text, then any
 *   existing text that was originally displayed with 'transparent'
 *   background color should be redisplayed with the new screen background
 *   color.  In other words, the 'transparent' background color of previously
 *   drawn text should be a permanent attribute of the character - the color
 *   should not be mapped on display to the then-current background color,
 *   because doing so would lose the transparency and thus retain the old
 *   screen color on a screen color change.
 */
void os_set_screen_color(os_color_t color)
{
    /* the parameter colours (text background, transparent...) are the
       theme's background */
    if (os_color_is_param(color))
        color = zcolor_Default;

    main_bg = (glui32)color;
    if (mainwin)
        osglk_set_window_bg(mainwin, main_bg);
    side_pictures_draw();
}

/*
 *   Set the game title.  The output layer calls this routine when a game
 *   sets its title (via an HTML <title> tag, for example).  If it's
 *   convenient to do so, the OS layer can use this string to set a window
 *   caption, or whatever else makes sense on each system.  Most
 *   character-mode implementations will provide an empty implementation,
 *   since there's not usually any standard way to show the current
 *   application title on a character-mode display.
 */
void os_set_title(const char *title)
{
#if defined(GARGLK) || defined(SPATTERLIGHT)
    garglk_set_story_title(title);
#endif
}

/*
 *   Show the system-specific MORE prompt, and wait for the user to respond.
 *   Before returning, remove the MORE prompt from the screen.
 *
 *   This routine is only used and only needs to be implemented when the OS
 *   layer takes responsibility for pagination; this will be the case on
 *   most systems that use proportionally-spaced (variable-pitch) fonts or
 *   variable-sized windows, since on such platforms the OS layer must do
 *   most of the formatting work, leaving the standard output layer unable
 *   to guess where pagination should occur.
 *
 *   If the portable output formatter handles the MORE prompt, which is the
 *   usual case for character-mode or terminal-style implementations, this
 *   routine is not used and you don't need to provide an implementation.
 *   Note that HTML TADS provides an implementation of this routine, because
 *   the HTML renderer handles line breaking and thus must handle
 *   pagination.
 */
void os_more_prompt()
{
#ifdef SPATTERLIGHT
    oshtml_put_plain("\n\n\n", 3);
#else
    oshtml_put_plain("\n[more]\n", 8);
#endif
    os_waitc();
}

/* ------------------------------------------------------------------------ */
/*
 *   User Input Routines
 */

/*
 *   Ask the user for a filename, using a system-dependent dialog or other
 *   mechanism.  Returns one of the OS_AFE_xxx status codes (see below).
 *
 *   prompt_type is the type of prompt to provide -- this is one of the
 *   OS_AFP_xxx codes (see below).  The OS implementation doesn't need to
 *   pay any attention to this parameter, but it can be used if desired to
 *   determine the type of dialog to present if the system provides
 *   different types of dialogs for different types of operations.
 *
 *   file_type is one of the OSFTxxx codes for system file type.  The OS
 *   implementation is free to ignore this information, but can use it to
 *   filter the list of files displayed if desired; this can also be used
 *   to apply a default suffix on systems that use suffixes to indicate
 *   file type.  If OSFTUNK is specified, it means that no filtering
 *   should be performed, and no default suffix should be applied.
 */
int os_askfile(const char *prompt, char *fname_buf, int fname_buf_len,
               int prompt_type, os_filetype_t file_type)
{
#ifndef GLKUNIX_FILEREF_GET_FILENAME
    return OS_AFE_FAILURE;
#else
    frefid_t fileref;
    glui32 gprompt, gusage;

    if (prompt_type == OS_AFP_OPEN)
        gprompt = filemode_Read;
    else
        gprompt = filemode_ReadWrite;

    if (file_type == OSFTSAVE || file_type == OSFTT3SAV)
        gusage = fileusage_SavedGame;
    else if (file_type == OSFTLOG || file_type == OSFTTEXT)
        gusage = fileusage_Transcript;
    else
        gusage = fileusage_Data;

    fileref = glk_fileref_create_by_prompt(gusage, gprompt, 0);
    if (fileref == NULL)
        return OS_AFE_CANCEL;

    strncpy(fname_buf, glkunix_fileref_get_filename(fileref), fname_buf_len);

    glk_fileref_destroy(fileref);

    return OS_AFE_SUCCESS;
#endif
}

/* ------------------------------------------------------------------------ */
/*
 *   Hyperlinks (<A HREF>).  The links of every window share one table, so a
 *   link in a status line that is drawn every turn is added only once.
 */

#define LINKS_MAX 8192

typedef struct link_s
{
    char *href;
    int flags;
} link_t;

static link_t *links = NULL;
static glui32 nlinks = 0, links_cap = 0;

static int osglk_links_ok(void)
{
    static int ok = -1;

    if (ok < 0)
        ok = glk_gestalt(gestalt_Hyperlinks, 0)
            && glk_gestalt(gestalt_HyperlinkInput, wintype_TextBuffer);
    return ok;
}

/* an href with a scheme is for the browser, not a command */
static int is_url(const char *s)
{
    static const char *const schemes[] =
        { "http:", "https:", "ftp:", "news:", "mailto:", "telnet:", 0 };
    int i;

    for (i = 0 ; schemes[i] ; ++i)
        if (strncasecmp(s, schemes[i], strlen(schemes[i])) == 0)
            return 1;
    return 0;
}

glui32 osglk_link_add(const char *href, int flags)
{
    glui32 i;

    if (!osglk_links_ok() || is_url(href))
        return 0;
    for (i = 0 ; i < nlinks ; ++i)
        if (links[i].flags == flags && strcmp(links[i].href, href) == 0)
            return i + 1;
    if (nlinks == LINKS_MAX)
        return 0;
    if (nlinks == links_cap)
    {
        glui32 cap = links_cap ? links_cap * 2 : 64;
        link_t *grown = realloc(links, cap * sizeof(link_t));
        if (!grown)
            return 0;
        links = grown;
        links_cap = cap;
    }
    links[nlinks].href = strdup(href);
    if (!links[nlinks].href)
        return 0;
    links[nlinks].flags = flags;
    return ++nlinks;
}

static const link_t *link_get(glui32 n)
{
    return n >= 1 && n <= nlinks ? &links[n - 1] : NULL;
}

/* a request is used up by its click, so ask again before each wait */
static void request_links(void)
{
    if (!nlinks)
        return;
    osglk_tag_banner_request_links();
    osglk_api_banner_request_links();
    if (mainwin)
        glk_request_hyperlink_event(mainwin);
}

/*
 *   a line the player entered, shown as the window would have shown it.
 *   What the formatter holds goes out first, in its own style, and then
 *   the line goes straight to the window, as the formatter would set its
 *   style again.
 */
static void echo_line(const char *buf)
{
    oshtml_put_plain("", 0);
    glk_set_style(style_Input);
    os_put_buffer((unsigned char *)buf, strlen(buf));
    glk_put_char('\n');
    glk_set_style(style_Normal);
}

/*
 *   A link was clicked while a line was being read.  The href becomes the
 *   player's command, as in HTML TADS: for an ordinary link the line is
 *   complete and 'buf' holds it (returns 1).  If it is "noenter" or
 *   "append", the line goes on, with the href added to the command line
 *   (returns 0), and so does a click we ignore.
 */
static int line_link(const event_t *click, unsigned char *buf, size_t buflen)
{
    const link_t *l = link_get(click->val1);
    event_t typed;
    size_t len;

    if (!l)
        return 0;

    /*
     *   the typed text isn't echoed, as the window doesn't echo lines
     *   (self_echo): it's either dropped or shown again
     */
    typed.type = evtype_None;
    glk_cancel_line_event(mainwin, &typed);
    if (typed.type != evtype_LineInput)
        return 0;
    os_fill_buffer(buf, typed.val1);

    if (l->flags & (OSLINK_NOENTER | OSLINK_APPEND))
    {
        len = (l->flags & OSLINK_APPEND) ? strlen((char *)buf) : 0;
        strncpy((char *)buf + len, l->href, buflen - 1 - len);
        buf[buflen - 1] = '\0';
        os_get_buffer(buf, buflen, strlen((char *)buf));
        return 0;
    }

    strncpy((char *)buf, l->href, buflen - 1);
    buf[buflen - 1] = '\0';
    echo_line((char *)buf);
    return 1;
}

#ifdef GLK_TIMERS
/*
 *   The timer, which under the determinism testing theme moves the virtual
 *   clock (osmisc.c) on by its interval each time it fires
 */
static glui32 timer_interval;

static void request_timer(glui32 ms)
{
    timer_interval = ms;
    glk_request_timer_events(ms);
}

static void timer_fired(void)
{
    if (osglk_vclock())
        osglk_vclock_advance(timer_interval);
}
#endif

/*
 *   Wait for an event for the input in hand.  The ends of sounds and of
 *   their fades are taken care of here (ossound.c), and so is a sound
 *   waiting for its RANDOM chance, which has a one-second timer while the
 *   wait has none of its own.
 */
static void select_event(event_t *event)
{
#ifdef GLK_TIMERS
    int sound_timer = 0;
#endif

    for (;;)
    {
#ifdef GLK_TIMERS
        if (!timer_interval && sound_timer != ossound_wants_timer())
        {
            sound_timer = !sound_timer;
            glk_request_timer_events(sound_timer ? 1000 : 0);
        }
#endif
        glk_select(event);
        osglk_not_idle();
        if (ossound_event(event))
            continue;
        if (event->type == evtype_Redraw)
        {
            side_pictures_draw();
            osglk_tag_banner_redraw();
            continue;
        }

        /* a click on a table of pictures is a click on its link */
        if (event->type == evtype_MouseInput)
        {
            glui32 link = osglk_tag_banner_click(event->win, event->val1,
                                                 event->val2);
            if (!link)
            {
                glk_request_mouse_event(event->win);
                continue;
            }
            event->type = evtype_Hyperlink;
            event->val1 = link;
            event->val2 = 0;
        }
#ifdef GLK_TIMERS
        if (event->type == evtype_Timer && sound_timer)
        {
            ossound_timer();
            continue;
        }
        if (sound_timer)
            glk_request_timer_events(0);
#endif
        return;
    }
}

#ifdef GLK_TIMERS
/*
 *   Sleep (os_sleep_ms) on a Glk timer.  The window shows what the game
 *   drew only once we wait for an event, so a slideshow paced with sleeps
 *   would otherwise show nothing but its last picture.  No input is in
 *   progress, so the timer is ours; a click on a link is dropped, and the
 *   links are asked for again at the next input.
 */
void osglk_sleep_ms(unsigned long ms)
{
    event_t event;

    if (ms == 0)
        return;
    request_timer((glui32)ms);
    do
    {
        select_event(&event);
        if (event.type == evtype_Arrange)
            redraw_windows();
    }
    while (event.type != evtype_Timer);
    request_timer(0);
}
#endif

/*
 *   Read a string of input.  Fills in the buffer with a null-terminated
 *   string containing a line of text read from the standard input.  The
 *   returned string should NOT contain a trailing newline sequence.  On
 *   success, returns 'buf'; on failure, including end of file, returns a
 *   null pointer.
 */
unsigned char *os_gets(unsigned char *buf, size_t buflen)
{
    event_t event;
    int linked = 0;

    oshtml_input();
    request_links();
    os_get_buffer(buf, buflen, 0);

    do
    {
        select_event(&event);
        if (event.type == evtype_Arrange)
            redraw_windows();
        else if (event.type == evtype_Hyperlink)
        {
            if (line_link(&event, buf, buflen))
            {
                linked = 1;
                event.type = evtype_LineInput;
            }
            else
                request_links();
        }
    }
    while (event.type != evtype_LineInput);

    if (!linked)
    {
        os_fill_buffer(buf, event.val1);
        if (self_echo)
            echo_line((char *)buf);
    }
    oshtml_input_done();
    return buf;
}

/*
 *   Read a string of input with an optional timeout.  This behaves like
 *   os_gets(), in that it allows the user to edit a line of text (ideally
 *   using the same editing keys that os_gets() does), showing the line of
 *   text under construction during editing.  This routine differs from
 *   os_gets() in that it returns if the given timeout interval expires
 *   before the user presses Return (or the local equivalent).
 *
 *   If the user presses Return before the timeout expires, we store the
 *   command line in the given buffer, just as os_gets() would, and we
 *   return OS_EVT_LINE.  We also update the display in the same manner that
 *   os_gets() would, by moving the cursor to a new line and scrolling the
 *   displayed text as needed.
 *
 *   If a timeout occurs before the user presses Return, we store the
 *   command line so far in the given buffer, statically store the cursor
 *   position, insert mode, buffer text, and anything else relevant to the
 *   editing state, and we return OS_EVT_TIMEOUT.
 *
 *   If the implementation does not support the timeout operation, this
 *   routine should simply return OS_EVT_NOTIMEOUT immediately when called;
 *   the routine should not allow the user to perform any editing if the
 *   timeout is not supported.  Callers must use the ordinary os_gets()
 *   routine, which has no timeout capabilities, if the timeout is not
 *   supported.
 *
 *   When we return OS_EVT_TIMEOUT, the caller is responsible for doing one
 *   of two things.
 *
 *   The first possibility is that the caller performs some work that
 *   doesn't require any display operations (in other words, the caller
 *   doesn't invoke os_printf, os_getc, or anything else that would update
 *   the display), and then calls os_gets_timeout() again.  In this case, we
 *   will use the editing state that we statically stored before we returned
 *   OS_EVT_TIMEOUT to continue editing where we left off.  This allows the
 *   caller to perform some computation in the middle of user command
 *   editing without interrupting the user - the extra computation is
 *   transparent to the user, because we act as though we were still in the
 *   midst of the original editing.
 *
 *   The second possibility is that the caller wants to update the display.
 *   In this case, the caller must call os_gets_cancel() BEFORE making any
 *   display changes.  Then, the caller must do any post-input work of its
 *   own, such as updating the display mode (for example, closing HTML font
 *   tags that were opened at the start of the input).  The caller is now
 *   free to do any display work it wants.
 *
 *   If we have information stored from a previous call that was interrupted
 *   by a timeout, and os_gets_cancel(TRUE) was never called, we will resume
 *   editing where we left off when the cancelled call returned; this means
 *   that we'll restore the cursor position, insertion state, and anything
 *   else relevant.  Note that if os_gets_cancel(FALSE) was called, we must
 *   re-display the command line under construction, but if os_gets_cancel()
 *   was never called, we will not have to make any changes to the display
 *   at all.
 *
 *   Note that when resuming an interrupted editing session (interrupted via
 *   os_gets_cancel()), the caller must re-display the prompt prior to
 *   invoking this routine.
 *
 *   Note that we can return OS_EVT_EOF in addition to the other codes
 *   mentioned above.  OS_EVT_EOF indicates that an error occurred reading,
 *   which usually indicates that the application is being terminated or
 *   that some hardware error occurred reading the keyboard.
 *
 *   If 'use_timeout' is false, the timeout should be ignored.  Without a
 *   timeout, the function behaves the same as os_gets(), except that it
 *   will resume editing of a previously-interrupted command line if
 *   appropriate.  (This difference is why the timeout is optional: a caller
 *   might not need a timeout, but might still want to resume a previous
 *   input that did time out, in which case the caller would invoke this
 *   routine with use_timeout==FALSE.  The regular os_gets() would not
 *   satisfy this need, because it cannot resume an interrupted input.)
 */
static char * timebuf = NULL;
static size_t timelen = 0;

int os_gets_timeout(unsigned char *buf, size_t bufl,
                    unsigned long timeout_in_milliseconds, int use_timeout)
{
#if defined GLK_TIMERS && defined GLK_MODULE_LINE_ECHO
    int timer = use_timeout ? timeout_in_milliseconds : 0;
    int timeout = 0;
    int initlen = 0;
    int linked = 0;
    event_t event;

    /* restore saved buffer contents */
    if (timebuf)
    {
        assert(timelen && timelen <= bufl);
        memcpy(buf, timebuf, timelen);
        initlen = timelen - 1;
        buf[initlen] = 0;
        free(timebuf);
        timebuf = 0;
    }

    /* start timer and turn off line echo */
    if (timer)
    {
        request_timer(timer);
        if (!self_echo)
            glk_set_echo_line_event(mainwin, 0);
    }

    oshtml_input();
    request_links();
    os_get_buffer(buf, bufl, initlen);

    do
    {
        select_event(&event);
        if (event.type == evtype_Arrange)
            redraw_windows();
        else if (event.type == evtype_Timer && (timeout = 1))
        {
            timer_fired();
            glk_cancel_line_event(mainwin, &event);
        }
        else if (event.type == evtype_Hyperlink)
        {
            if (line_link(&event, buf, bufl))
            {
                linked = 1;
                event.type = evtype_LineInput;
            }
            else
                request_links();
        }
    }
    while (event.type != evtype_LineInput);

    unsigned char *res = linked ? buf : os_fill_buffer(buf, event.val1);

    /* stop timer and turn on line echo */
    if (timer)
    {
        request_timer(0);
        if (!self_echo)
            glk_set_echo_line_event(mainwin, 1);
    }

    /* save or print buffer contents */
    if (res && (timer || self_echo) && !linked)
    {
        if (timeout)
        {
            timelen = strlen((char *)buf) + 1;
            timebuf = malloc(timelen);
            memcpy(timebuf, buf, timelen);
        }
        else
            echo_line((char *)buf);
    }

    if (!timeout)
        oshtml_input_done();
    return timeout ? OS_EVT_TIMEOUT : res ? OS_EVT_LINE : OS_EVT_EOF;
#else
    return OS_EVT_NOTIMEOUT;
#endif
}

/*
 *   Cancel an interrupted editing session.  This MUST be called if any
 *   output is to be displayed after a call to os_gets_timeout() returns
 *   OS_EVT_TIMEOUT.
 *
 *   'reset' indicates whether or not we will forget the input state saved
 *   by os_gets_timeout() when it last returned.  If 'reset' is true, we'll
 *   clear the input state, so that the next call to os_gets_timeout() will
 *   start with an empty input buffer.  If 'reset' is false, we will retain
 *   the previous input state, if any; this means that the next call to
 *   os_gets_timeout() will re-display the same input buffer that was under
 *   construction when it last returned.
 *
 *   This routine need not be called if os_gets_timeout() is to be called
 *   again with no other output operations between the previous
 *   os_gets_timeout() call and the next one.
 *
 *   Note that this routine needs only a trivial implementation when
 *   os_gets_timeout() is not supported (i.e., the function always returns
 *   OS_EVT_NOTIMEOUT).
 */
void os_gets_cancel(int reset)
{
#if defined GLK_TIMERS && defined GLK_MODULE_LINE_ECHO
    if (timebuf)
    {
        echo_line(timebuf);

        if (reset)
        {
            free(timebuf);
            timebuf = 0;
        }
    }
#endif
}

/*
 *   Read a character from the keyboard.  For extended keystrokes, this
 *   function returns zero, and then returns the CMD_xxx code for the
 *   extended keystroke on the next call.  For example, if the user
 *   presses the up-arrow key, the first call to os_getc() should return
 *   0, and the next call should return CMD_UP.  Refer to the CMD_xxx
 *   codes below.
 *
 *   os_getc() should return a high-level, translated command code for
 *   command editing.  This means that, where a functional interpretation
 *   of a key and the raw key-cap interpretation both exist as CMD_xxx
 *   codes, the functional interpretation should be returned.  For
 *   example, on Unix, Ctrl-E is conventionally used in command editing to
 *   move to the end of the line, following Emacs key bindings.  Hence,
 *   os_getc() should return CMD_END for this keystroke, rather than
 *   (CMD_CTRL + 'E' - 'A'), because CMD_END is the high-level command
 *   code for the operation.
 *
 *   The translation ability of this function allows for system-dependent
 *   key mappings to functional meanings.
 */
static int glktotads(unsigned int key)
{
    if (key < 256)
        return key;
    switch (key)
    {
        case keycode_Up:
            return CMD_UP;
        case keycode_Down:
            return CMD_DOWN;
        case keycode_Left:
            return CMD_LEFT;
        case keycode_Right:
            return CMD_RIGHT;
        case keycode_PageUp:
            return CMD_PGUP;
        case keycode_PageDown:
            return CMD_PGDN;
        case keycode_Home:
            return CMD_HOME;
        case keycode_End:
            return CMD_END;
        case keycode_Func1:
            return CMD_F1;
        case keycode_Func2:
            return CMD_F2;
        case keycode_Func3:
            return CMD_F3;
        case keycode_Func4:
            return CMD_F4;
        case keycode_Func5:
            return CMD_F5;
        case keycode_Func6:
            return CMD_F6;
        case keycode_Func7:
            return CMD_F7;
        case keycode_Func8:
            return CMD_F8;
        case keycode_Func9:
            return CMD_F9;
        case keycode_Func10:
            return CMD_F10;
        default:
            return 0;
    }
}

static int bufchar = 0;
static int waitchar = 0;
static int timechar = 0;

/*
 *   os_get_event wants a click on a link as an event of its own: 'linkevt'
 *   says there was one, and 'linkhref' is its href.  Anywhere else a click
 *   counts as a key.
 */
static int want_href = 0;
static int linkevt = 0;
static char linkhref[256];

static int getglkchar(void)
{
    event_t event;
    const link_t *l;

    timechar = 0;
    linkevt = 0;

    oshtml_input();
    request_links();
    glk_request_char_event(mainwin);

    do
    {
        select_event(&event);
        if (event.type == evtype_Arrange)
            redraw_windows();
        else if (event.type == evtype_Hyperlink && (l = link_get(event.val1)))
        {
            if (want_href)
            {
                snprintf(linkhref, sizeof(linkhref), "%s", l->href);
                linkevt = 1;
            }
            else
            {
                event.type = evtype_CharInput;
                event.val1 = ' ';
            }
        }
        else if (event.type == evtype_Hyperlink)
            request_links();
        else if (event.type == evtype_Timer)
        {
#ifdef GLK_TIMERS
            timer_fired();
#endif
            timechar = 1;
        }
    }
    while (event.type != evtype_CharInput && event.type != evtype_Timer
           && !linkevt);

    glk_cancel_char_event(mainwin);

    return timechar || linkevt ? 0 : event.val1;
}

int os_getc(void)
{
    unsigned int c;

    if (bufchar)
    {
        c = bufchar;
        bufchar = 0;
        return c;
    }

    c = waitchar ? waitchar : getglkchar();
    waitchar = 0;

    if (c == keycode_Return)
        c = '\n';
    else if (c == keycode_Tab)
        c = '\t';
    else if (c == keycode_Escape)
        c = '\e';

    if (c < 256)
        return c;

    bufchar = glktotads(c);

    return 0;
}

/*
 *   Read a character from the keyboard, following the same protocol as
 *   os_getc() for CMD_xxx codes (i.e., when an extended keystroke is
 *   encountered, os_getc_raw() returns zero, then returns the CMD_xxx code
 *   on the subsequent call).
 *
 *   This function differs from os_getc() in that this function returns the
 *   low-level, untranslated key code whenever possible.  This means that,
 *   when a functional interpretation of a key and the raw key-cap
 *   interpretation both exist as CMD_xxx codes, this function returns the
 *   key-cap interpretation.  For the Unix Ctrl-E example in the comments
 *   describing os_getc() above, this function should return 5 (the ASCII
 *   code for Ctrl-E), because the CMD_CTRL interpretation is the low-level
 *   key code.
 *
 *   This function should return all control keys using their ASCII control
 *   codes, whenever possible.  Similarly, this function should return ASCII
 *   27 for the Escape key, if possible.
 *
 *   For keys for which there is no portable ASCII representation, this
 *   should return the CMD_xxx sequence.  So, this function acts exactly the
 *   same as os_getc() for arrow keys, function keys, and other special keys
 *   that have no ASCII representation.  This function returns a
 *   non-translated version ONLY when an ASCII representation exists - in
 *   practice, this means that this function and os_getc() vary only for
 *   CTRL keys and Escape.
 */
int os_getc_raw(void)
{
    return os_getc();
}

/* wait for a character to become available from the keyboard */
void os_waitc(void)
{
    waitchar = getglkchar();
}

/*
 *   Get an input event.  The event types are shown above.  If use_timeout
 *   is false, this routine should simply wait until one of the events it
 *   recognizes occurs, then return the appropriate information on the
 *   event.  If use_timeout is true, this routine should return
 *   OS_EVT_TIMEOUT after the given number of milliseconds elapses if no
 *   event occurs first.
 *
 *   This function is not obligated to obey the timeout.  If a timeout is
 *   specified and it is not possible to obey the timeout, the function
 *   should simply return OS_EVT_NOTIMEOUT.  The trivial implementation
 *   thus checks for a timeout, returns an error if specified, and
 *   otherwise simply waits for the user to press a key.
 */
int os_get_event(unsigned long timeout_in_milliseconds, int use_timeout,
                 os_event_info_t *info)
{
#ifdef GLK_TIMERS
    /* start timer */
    int timer = use_timeout ? timeout_in_milliseconds : 0;
    if (timer)
        request_timer(timer);
#else
    /* we can't handle timeouts */
    if (use_timeout)
        return OS_EVT_NOTIMEOUT;
#endif

    /* get a key, or a click on a link */
    want_href = 1;
    info->key[0] = os_getc_raw();
    if (info->key[0] == 0 && timechar == 0 && !linkevt)
        info->key[1] = os_getc_raw();
    want_href = 0;

#ifdef GLK_TIMERS
    /* stop timer */
    if (timer)
        request_timer(0);
#endif

    /* return the event */
    if (linkevt)
    {
        linkevt = 0;
        snprintf(info->href, sizeof(info->href), "%s", linkhref);
        return OS_EVT_HREF;
    }
    return timechar ? OS_EVT_TIMEOUT : OS_EVT_KEY;
}

