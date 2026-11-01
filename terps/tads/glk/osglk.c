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

/*
 * osglk.c -- glk os adapter
 *
 * The contract of each os_* function here is in tads2/osifc.h; comments say
 * only what this port does.
 */

#include <assert.h>
#include <math.h>

#include "os.h"
#include "glk.h"
#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif
#include "oshtml.h"
#include "osglk.h"
#include "osstyles.h"
#include "ossound.h"

/* for version strings */
#include "trd.h"
#include "oem.h"
#include "vmvsn.h"

static void redraw_windows(void);
static void os_status_redraw(void);

/* the status line's text, at its left and its score at its right, kept
   to be drawn again whenever the line is (os_status_redraw) */
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

/* the <BODY TEXT> colour in the main window's style hints, if one is there */
static os_color_t body_text = OS_COLOR_P_TEXT;

glui32 statusfg;
glui32 statusbg;


/* ------------------------------------------------------------------------ */
static int osglk_links_ok(void);

static int os_get_sysinfo_text(int code, long *result);

int os_get_sysinfo(int code, void *param, long *result)
{
    if (!osglk_html_target())
        return os_get_sysinfo_text(code, result);

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
         *   which oshtml.c and osglktagban.c show, and adv3 (which keeps to
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

/*
 *   -nohtml: the answers of a text-only interpreter, which shows no
 *   pictures and plays no sounds (those come only in HTML)
 */
static int os_get_sysinfo_text(int code, long *result)
{
    switch (code)
    {
        case SYSINFO_TEXT_HILITE:
        case SYSINFO_BANNERS:
            *result = 1;
            return TRUE;
        case SYSINFO_TEXT_COLORS:
            *result = SYSINFO_TXC_RGB;
            return TRUE;
        case SYSINFO_INTERP_CLASS:
            *result = SYSINFO_ICLASS_TEXTGUI;
            return TRUE;

        case SYSINFO_HTML:
        case SYSINFO_JPEG:
        case SYSINFO_PNG:
        case SYSINFO_PNG_TRANS:
        case SYSINFO_PNG_ALPHA:
        case SYSINFO_MNG:
        case SYSINFO_MNG_TRANS:
        case SYSINFO_MNG_ALPHA:
        case SYSINFO_WAV:
        case SYSINFO_MIDI:
        case SYSINFO_WAV_MIDI_OVL:
        case SYSINFO_WAV_OVL:
        case SYSINFO_MPEG:
        case SYSINFO_MPEG1:
        case SYSINFO_MPEG2:
        case SYSINFO_MPEG3:
        case SYSINFO_OGG:
        case SYSINFO_AUDIO_FADE:
        case SYSINFO_AUDIO_CROSSFADE:
        case SYSINFO_PREF_IMAGES:
        case SYSINFO_PREF_SOUNDS:
        case SYSINFO_PREF_MUSIC:
        case SYSINFO_PREF_LINKS:
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

    /* not the body's, which the main window's style hints have */
    if (os_color_is_param(body_text)
        && !glk_style_measure(mainwin, style_Normal, stylehint_TextColor,
                              &mainfg))
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

/* os_init changes nothing that has to be restored */
void os_uninit(void)
{
}

void os_term(int status)
{
    glk_exit();
}

/* ------------------------------------------------------------------------ */
/* Display routines */

/*
 *   Status mode 0 is the main window and 1 the status line.  In mode 2 nothing
 *   is shown, but the text still goes through the parser.
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

/* score/turns, at the right of the status line */
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

/*
 *   HTML, unless the player asked for text only (-nohtml): then the
 *   formatters keep the markup to themselves, and the game hears that this
 *   is a text-only interpreter
 */
static int html_on = 1;

void osglk_set_html(int on)
{
    html_on = on;
    if (!on)
        strcpy(G_tads_oem_display_mode, "text-only");
}

int osglk_html_target(void)
{
    return html_on;
}

/* \H+ and \H- */
void os_start_html(void)
{
    if (html_on)
        oshtml_start();
}

void os_end_html(void)
{
    if (html_on)
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

int osglk_px_text_height(winid_t win)
{
    return win_text_height(win->peer);
}

/* as glkimp sizes a grid: whole cells, inside the margins */
glui32 osglk_px_grid_width(int chars)
{
    return (glui32)ceil((chars > 0 ? chars : 0) * gcellw - 0.001)
        + 2 * (glui32)ggridmarginx;
}

glui32 osglk_px_grid_line(void)
{
    return (glui32)ceil(gcellh - 0.001) + 2 * (glui32)ggridmarginy;
}
#else /* the GARGLKWINSIZE and GARGLKOVERLAY modules */
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

int osglk_px_text_height(winid_t win)
{
    (void)win;
    return 0;
}

glui32 osglk_px_grid_width(int chars)
{
    return (glui32)ceil((chars > 0 ? chars : 0) * osglk_px_cellw());
}

glui32 osglk_px_grid_line(void)
{
    return (glui32)ceil(osglk_px_cellh());
}
#endif /* SPATTERLIGHT */

/*
 *   the lines of a text buffer's text, wrapped as it shows them (Gargoyle
 *   takes any window for a text buffer)
 */
int osglk_px_lines(winid_t win)
{
    return glk_window_get_type(win) == wintype_TextBuffer
        ? garglk_tads_os_banner_size(win) : 0;
}
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

void osglk_set_text_height(winid_t win, int pixels)
{
#ifdef SPATTERLIGHT
    spatterlight_window_set_text_height(win, pixels > 0 ? (glui32)pixels : 0);
    osglk_api_banner_resized();
#else
    (void)win;
    (void)pixels;
#endif /* SPATTERLIGHT */
}

/* the main window's background, which the side pictures' match */
static glui32 main_bg = zcolor_Default;

glui32 osglk_screen_bg(void)
{
    return main_bg;
}

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

static void redraw_windows(void)
{
    measure_colors();
    oshtml_arrange();
    os_status_redraw();
    os_banners_redraw();
    osglk_side_pictures_draw();
    osglk_page_redraw();
    osglk_map_redraw();
    osglk_tag_banner_redraw();
    osglk_tag_banner_refit();
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
    osglk_side_pictures_remove();
    osglk_map_remove();
    glk_window_clear(mainwin);
}

/* ------------------------------------------------------------------------ */
void os_set_text_attr(int attr)
{
    curattr = attr;
    oshtml_set_os_attr(attr);
}

/*
 *   The parser holds text back, so the colour waits there with it, and comes
 *   out through osglk_set_text_color
 */
void os_set_text_color(os_color_t fg, os_color_t bg)
{
    oshtml_set_os_color(fg, bg);
}

void osglk_set_body_text(os_color_t fg)
{
#ifdef SPATTERLIGHT
    int s;

    if (fg == body_text || !mainwin)
        return;
    body_text = fg;
    for (s = 0 ; s < style_NUMSTYLES ; ++s)
    {
        if (os_color_is_param(fg))
            glk_stylehint_clear(wintype_TextBuffer, s, stylehint_TextColor);
        else
            glk_stylehint_set(wintype_TextBuffer, s, stylehint_TextColor,
                              (glsi32)fg);
    }
    win_refresh(mainwin->peer, 0, 0);
#else
    (void)fg;
#endif /* SPATTERLIGHT */
}

os_color_t osglk_body_color(os_color_t fg)
{
    return fg == body_text ? OS_COLOR_P_TEXT : fg;
}

/* os_set_text_color, once the HTML parser has caught up */
void osglk_set_text_color(os_color_t fg, os_color_t bg)
{
    int changedfg = 0;
    int changedbg = 0;

    /*
     *   0 on a transparent background means the theme's colour (Hill Ridge
     *   Lost & Found), so oshtml.c sends black as 1/1/1
     */
    if (fg == 0 && bg == OS_COLOR_P_TRANSPARENT) {
        fg = zcolor_Default;
        changedfg = 1;
    }

    switch (fg) {
        case OS_COLOR_P_TRANSPARENT:
            fg = zcolor_Default;
            changedfg = 1;
            break;
        case OS_COLOR_P_TEXT:
            fg = zcolor_Default;
            changedfg = 1;
            break;
        case OS_COLOR_P_TEXTBG:
            fprintf(stderr, "Ignoring attempt to set foreground color "
                    "to OS_COLOR_P_TEXTBG\n");
            break;
        case OS_COLOR_P_STATUSLINE:
            fprintf(stderr, "Ignoring attempt to set foreground color "
                    "to OS_COLOR_P_STATUSLINE\n");
            break;
        case OS_COLOR_P_STATUSBG:
            fprintf(stderr, "Ignoring attempt to set foreground color "
                    "to OS_COLOR_P_STATUSBG\n");
            break;
        case OS_COLOR_P_INPUT:
            fprintf(stderr, "Ignoring attempt to set foreground color "
                    "to OS_COLOR_P_INPUT\n");
            break;
        default:
            changedfg = 1;
            break;
    }

    switch (bg) {
        case OS_COLOR_P_TRANSPARENT:
            bg = zcolor_Default;
            changedbg = 1;
            break;
        case OS_COLOR_P_TEXT:
            fprintf(stderr, "Ignoring attempt to set background color "
                    "to OS_COLOR_P_TEXT\n");
            break;
        case OS_COLOR_P_TEXTBG:
            fprintf(stderr, "Trying to set background color "
                    "to OS_COLOR_P_TEXTBG\n");
            bg = zcolor_Default;
            changedbg = 1;
            break;
        case OS_COLOR_P_STATUSLINE:
            fprintf(stderr, "Ignoring attempt to set background color "
                    "to OS_COLOR_P_STATUSLINE\n");
            break;
        case OS_COLOR_P_STATUSBG:
            fprintf(stderr, "Ignoring attempt to set background color "
                    "to OS_COLOR_P_STATUSBG\n");
            break;
        case OS_COLOR_P_INPUT:
            fprintf(stderr, "Ignoring attempt to set background color "
                    "to OS_COLOR_P_INPUT\n");
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

void os_set_screen_color(os_color_t color)
{
    /* the parameter colours (text background, transparent...) are the
       theme's background */
    if (os_color_is_param(color))
        color = zcolor_Default;

    main_bg = (glui32)color;
    if (mainwin)
        osglk_set_window_bg(mainwin, main_bg);
    osglk_side_pictures_draw();
}

void os_set_title(const char *title)
{
#if defined(GARGLK) || defined(SPATTERLIGHT)
    garglk_set_story_title(title);
#endif
}

/* Blank lines in Spatterlight and [more] elsewhere, then wait for a key */
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
 *   Glk's own prompt, for the kind of file; the game's prompt text isn't used.
 *   Without glkunix_fileref_get_filename, there's no way to get a name.
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
    osglk_map_request_mouse();
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
            osglk_side_pictures_draw();
            osglk_map_redraw();
            osglk_tag_banner_redraw();
            osglk_api_banner_redraw();
            continue;
        }

        /* a click on a table of pictures or a map is a click on its link */
        if (event->type == evtype_MouseInput)
        {
            glui32 link = osglk_tag_banner_click(event->win, event->val1,
                                                 event->val2);
            if (!link)
                link = osglk_map_click(event->win, event->val1, event->val2);
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

/* A click on a link can supply the line (line_link) */
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

    osglk_map_remove();
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
 *   Needs Glk timers and control of line echo, or else OS_EVT_NOTIMEOUT.
 *   
 *   A timeout cancels the line request and keeps what was typed in 'timebuf'.
 *   The next call starts again from that text, and os_gets_cancel can show it
 *   as a finished line.  The window doesn't echo while a timer runs, so that
 *   cancelling shows nothing; a line that does finish is shown by echo_line.
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

    if (!timeout)
        osglk_map_remove();
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

/* Show the kept text as a finished line; 'reset' also forgets it */
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

/* The CMD_xxx code of a Glk special key, or 0 if it has none */
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
    osglk_page_show();
    osglk_tag_banner_refit();
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
    osglk_page_remove();

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

/* os_getc translates no keys for line editing, so raw is the same thing */
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
 *   A key, a timeout, or a click on a link (OS_EVT_HREF, through want_href).
 *   Without Glk timers, a timeout gets OS_EVT_NOTIMEOUT.
 */
int os_get_event(unsigned long timeout_in_milliseconds, int use_timeout,
                 os_event_info_t *info)
{
#ifdef GLK_TIMERS
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
    if (timer)
        request_timer(0);
#endif

    if (linkevt)
    {
        linkevt = 0;
        snprintf(info->href, sizeof(info->href), "%s", linkhref);
        return OS_EVT_HREF;
    }
    return timechar ? OS_EVT_TIMEOUT : OS_EVT_KEY;
}

