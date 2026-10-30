/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
 * Copyright (C) 2010 by Ben Cressey.                                         *
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

/* osglktagban.c -- TADS 2's <BANNER> tags */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "os.h"
#include "glk.h"
#include "oshtml.h"
#include "osglk.h"
#include "osstyles.h"
#include "osglkban.h"

#include <math.h>
#ifdef SPATTERLIGHT
#include "glkimp.h"                 /* for-size to-contents hack */
#endif /* SPATTERLIGHT */

/* ------------------------------------------------------------------------ */
/*
 *   TADS 2's <BANNER> tags (oshtml.c).  Each id has a window of its own in
 *   the main window's space, opened when the banner first has text, and
 *   newer banners sit inside older ones, as HTML TADS adds each one last.
 *   The status line (id StatusLine) is a text grid and the others are text
 *   buffers.  A banner's text replaces what it showed, and a banner with no
 *   size, or 'previous' on its first showing, takes the size of its text.
 */

/* HTML TADS sizes banners in pixels: a line, and a column */
#define TAG_PIX_LINE 16
#define TAG_PIX_COL  8

typedef struct tagban_s
{
    char id[OSHTML_BANNER_ID];
    winid_t win;
    glui32 type;
    glui32 method;
    glui32 size;

    /*
     *   A size in pixels belongs to a graphics window, as Glk measures a
     *   split in its key window's units: 'win' fills all of it, and it is
     *   the key of the split from the main window
     */
    winid_t gfx;

    int fit;                        /* size the window to the text */
    int measure;                    /* this pass only counts the text */
    int bg;                         /* the window has a <BODY BGCOLOR> */

    /* the text so far: lines, the longest, and newlines not yet shown */
    int lines;
    int col, maxcol;
    int nl;
#ifdef OSGLK_PIXELS
    picture_fit pictures;           /* and what its pictures add */

    /*
     *   A side banner in pixels too wide for the game window shrinks to
     *   its pictures, if it has no text: the pixels it asked for, what it
     *   shrank to, its widest picture and whether it has text
     */
    int px_asked;
    glui32 px_shrunk;
    glui32 px_widest;
    int px_text;
    int fit_px;                 /* the text height it was last fitted to */

    /*
     *   A banner across in pixels with only pictures in it grows to them,
     *   if it's too short: what it grew to
     */
    glui32 px_grown;

    /*
     *   If it's one picture, that is drawn in the graphics window, as the
     *   pictures beside the page are (osglk.c), and the text window gets
     *   none of it: the pictures, the widest, and whether it's drawn so
     */
    int px_pics;
    oshtml_image px_img;
    int px_alone;

    /*
     *   A table of pictures at the end of the text, in a graphics window
     *   under it: the table, whether this showing had it, where it's drawn
     *   and the banner's background
     */
    winid_t tblwin;
    oshtml_pictable *tbl;
    int tbl_used;
    glui32 tbl_x, tbl_y;
    glui32 tbl_text;                /* the pixels of the text above it */
    glui32 screen;

    /*
     *   Under the text of a picture over it, the room the cell leaves: the
     *   text window is as tall as its text, as Gargoyle keeps a text
     *   buffer's lines at its bottom
     */
    winid_t padwin;

    /*
     *   A line of cells of text over a strip of pictures, as Threnody's
     *   status line is: the first cell is 'win', and each of the others a
     *   grid as wide as its column, at the right, in a graphics window that
     *   holds the room after it too.  The text goes to 'out', or 'win', a
     *   line cut to fit, its spaces run together; the style and colours it
     *   has carry over to the next cell.
     */
    int ncells;
    oshtml_textcell cell[OSHTML_CELLS_MAX];
    winid_t cellslot[OSHTML_CELLS_MAX], cellpad[OSHTML_CELLS_MAX];
    winid_t cellgrid[OSHTML_CELLS_MAX];
    winid_t out;
    int cellmode;
    int cell_text, cell_space;      /* text so far, and a space after it */
    int last_combo;
    glui32 last_fg, last_bg;
#endif /* OSGLK_PIXELS */

    struct tagban_s *next;
}
tagban_t;

static tagban_t *tagbans = NULL;

static int tag_vertical(glui32 method)
{
    glui32 dir = method & winmethod_DirMask;
    return dir == winmethod_Left || dir == winmethod_Right;
}

/* the window the text goes to */
static winid_t tag_out(const tagban_t *tb)
{
#ifdef OSGLK_PIXELS
    if (tb->out)
        return tb->out;
#endif /* OSGLK_PIXELS */
    return tb->win;
}

/* a grid has a column of margin on each side, but a cell starts where
   its text is aligned */
static int tag_margin(tagban_t *tb)
{
#ifdef OSGLK_PIXELS
    if (tb->cellmode)
        return 0;
#endif /* OSGLK_PIXELS */
    return tb->type == wintype_TextGrid ? 1 : 0;
}

static int tag_width(void *ctx)
{
    tagban_t *tb = ctx;
    glui32 wid = 0;

    if (!tb->win)
        return 0;
#ifdef OSGLK_PIXELS
    if (tb->cellmode)
    {
        glk_window_get_size(tag_out(tb), &wid, NULL);
        return (int)wid;
    }
#endif /* OSGLK_PIXELS */

    /* while we measure a banner at the side, its lines may be any length */
    if (tb->measure && tag_vertical(tb->method))
    {
        if (mainwin)
            glk_window_get_size(mainwin, &wid, NULL);
        return (int)wid;
    }

    glk_window_get_size(tb->win, &wid, NULL);
#ifdef SPATTERLIGHT
    if (tb->type == wintype_TextBuffer)
        wid = (glui32)win_cols(tb->win->peer);
#endif /* SPATTERLIGHT */
    return (int)wid - 2 * tag_margin(tb);
}

static int tag_rule_width(void *ctx, int combo)
{
    tagban_t *tb = ctx;

    if (!tb->win)
        return 0;
    if (tb->measure && tag_vertical(tb->method))
        return tag_width(ctx);
    return osglk_rule_width(tb->win, combo) - 2 * tag_margin(tb);
}

/* the newlines that wait for more text, now that some has come */
static void tag_put_newlines(tagban_t *tb, strid_t str)
{
    if (tb->lines == 0)
        tb->lines = 1;
    for ( ; tb->nl > 0 ; --tb->nl)
    {
        ++tb->lines;
        tb->col = 0;
        if (str && !tb->measure)
            glk_put_char_stream_uni(str, '\n');
    }
}

#ifdef OSGLK_PIXELS
/* a cell's text: one line, as much as fits from where it starts */
static void tag_put_cell(tagban_t *tb, strid_t str, const glui32 *buf,
                         size_t len)
{
    int wid = tag_width(tb);
    size_t i;

    for (i = 0 ; i < len ; ++i)
    {
        glui32 c = buf[i];

        if (c == '\n' || c == ' ')
        {
            tb->cell_space = tb->cell_text;
            continue;
        }
        if (tb->cell_space && tb->col < wid)
        {
            glk_put_char_stream_uni(str, ' ');
            ++tb->col;
        }
        tb->cell_space = 0;
        tb->cell_text = 1;
        if (tb->col >= wid)
            continue;
        glk_put_char_stream_uni(str, c);
        ++tb->col;
    }
    if (tb->lines == 0)
        tb->lines = 1;
}
#endif /* OSGLK_PIXELS */

static void tag_put(void *ctx, const glui32 *buf, size_t len)
{
    tagban_t *tb = ctx;
    strid_t str = tb->win ? glk_window_get_stream(tag_out(tb)) : NULL;
    int wid = tb->type == wintype_TextGrid ? tag_width(tb) : 0;
    size_t i;

#ifdef OSGLK_PIXELS
    if (tb->cellmode)
    {
        if (str)
            tag_put_cell(tb, str, buf, len);
        return;
    }
#endif /* OSGLK_PIXELS */
    for (i = 0 ; i < len ; ++i)
    {
        glui32 c = buf[i];

        /* the newlines wait for more text, so the last line isn't blank */
        if (c == '\n')
        {
            ++tb->nl;
            continue;
        }
        tag_put_newlines(tb, str);

        /* a grid wraps a line that's too long */
        if (wid > 0 && tb->col >= wid)
        {
            ++tb->lines;
            tb->col = 0;
            if (str && !tb->measure)
                glk_put_char_stream_uni(str, '\n');
        }
        if (tb->col == 0 && tag_margin(tb) && str && !tb->measure)
            glk_put_char_stream_uni(str, ' ');
        if (str && !tb->measure)
            glk_put_char_stream_uni(str, c);
#ifdef OSGLK_PIXELS
        tb->pictures.last = 0;
        if (c != ' ' && c != 0xA0)
            tb->px_text = 1;
#endif /* OSGLK_PIXELS */
        if (++tb->col > tb->maxcol)
            tb->maxcol = tb->col;
    }
}

static void tag_set_attr(void *ctx, int combo)
{
    tagban_t *tb = ctx;
#ifdef OSGLK_PIXELS
    tb->last_combo = combo;
#endif /* OSGLK_PIXELS */
    if (tb->win)
        glk_set_style_stream(glk_window_get_stream(tag_out(tb)),
                             osstyles_style(combo));
}

/*
 *   The status colours are the status line's in a text buffer, which the menu
 *   library's title and its hidden ">" are made of; the other parameter
 *   colours are the window's own, from the theme
 */
static glui32 tag_color(const tagban_t *tb, os_color_t color)
{
    if (tb->type == wintype_TextBuffer)
    {
        if (color == OS_COLOR_P_STATUSBG)
            return statusfg;
        if (color == OS_COLOR_P_STATUSLINE)
            return statusbg;
    }
    return os_color_is_param(color) ? zcolor_Default : (glui32)color;
}

static void tag_set_color(void *ctx, os_color_t fg, os_color_t bg)
{
    tagban_t *tb = ctx;
#ifdef OSGLK_PIXELS
    tb->last_fg = tag_color(tb, fg);
    tb->last_bg = tag_color(tb, bg);
#endif /* OSGLK_PIXELS */
    if (tb->win)
        osglk_zcolors_stream(glk_window_get_stream(tag_out(tb)),
                                  tag_color(tb, fg), tag_color(tb, bg));
}

static void tag_set_screen_color(void *ctx, os_color_t color)
{
    tagban_t *tb = ctx;
    if (!tb->win || tb->measure)
        return;
    osglk_set_window_bg(tb->win, tag_color(tb, color));
#ifdef OSGLK_PIXELS
    tb->screen = tag_color(tb, color);
#endif /* OSGLK_PIXELS */
    tb->bg = !os_color_is_param(color);
}

static int tag_centers(void *ctx, int combo)
{
    tagban_t *tb = ctx;
    return tb->win && osglk_centers(tag_out(tb), combo);
}

/* pictures go in a text-buffer banner, once it's writing for real */
static int tag_shows_pictures(const tagban_t *tb)
{
    return tb->type == wintype_TextBuffer && !tb->measure && tb->win;
}

/* the window pictures go in, after the newlines that are waiting */
static winid_t tag_window(void *ctx)
{
    tagban_t *tb = ctx;

    if (!tag_shows_pictures(tb))
        return NULL;
    tag_put_newlines(tb, glk_window_get_stream(tb->win));
    return tb->win;
}

#ifdef OSGLK_PIXELS
/*
 *   the widest picture of a banner at the side; one as wide as the banner
 *   asked to be only holds it open, as 1893's blank ones do
 */
static void tag_widest_add(tagban_t *tb, const oshtml_image *img)
{
    glui32 w, h;

    if (!tb->gfx || !tag_vertical(tb->method))
        return;
    if (!img->width_pct && img->width && (int)img->width >= tb->px_asked)
        return;
    ++tb->px_pics;
    if (!oshtml_image_size(img, 0, &w, &h))
        tb->px_text = 1;            /* we can't tell: leave it as it is */
    else if (w > tb->px_widest)
    {
        tb->px_widest = w;
        tb->px_img = *img;
    }
}

/*
 *   the pixels a side banner gets: as it asked, unless that's more than a
 *   quarter of the game window and it shows only pictures
 */
static glui32 tag_side_pixels(tagban_t *tb, int *alone)
{
    glui32 asked = tb->px_asked > 0 ? (glui32)tb->px_asked : 0;
    glui32 rootw = osglk_px_root_width(), mx = 0, w;

    osglk_px_margins(tb->win, &mx, NULL);
    w = tb->px_widest + 2 * mx;

    *alone = 0;
    if (tb->px_text || !tb->px_widest || !rootw
        || asked * 4 <= rootw || w >= asked)
        return asked;
    if (tb->px_pics == 1)
    {
        *alone = 1;
        return tb->px_widest;
    }
    return w;
}

/* the split of the text window from the graphics window it fills */
static winid_t tag_inner_split(tagban_t *tb)
{
    winid_t pair;

    for (pair = glk_window_get_parent(tb->win) ; pair ;
         pair = glk_window_get_parent(pair))
    {
        winid_t k = NULL;
        glk_window_get_arrangement(pair, NULL, NULL, &k);
        if (k == tb->win)
            break;
    }
    return pair;
}

/* the picture alone in the graphics window, or the text window over it */
static void tag_side_alone(tagban_t *tb, int alone)
{
    winid_t pair = tag_inner_split(tb);

    if (alone != tb->px_alone && pair)
        glk_window_set_arrangement(pair, (tb->method & winmethod_DirMask)
                                   | winmethod_Proportional
                                   | winmethod_NoBorder,
                                   alone ? 0 : 100, tb->win);
    tb->px_alone = alone && pair;
    if (tb->px_alone)
        osglk_draw_side_picture(tb->gfx, &tb->px_img, tb->screen);
}
#endif /* OSGLK_PIXELS */

/* a picture is drawn there too, and the banner fitted to it */
static int tag_image(void *ctx, const oshtml_image *img)
{
    tagban_t *tb = ctx;
    winid_t win;

    /*
     *   only asking whether it shows them: the newlines still wait, as the
     *   pictures may go in a table under the text
     */
    if (!img)
        return tag_shows_pictures(tb);
    if (!(win = tag_window(tb)))
        return 0;
    oshtml_draw_image(win, img);
#ifdef OSGLK_PIXELS
    tag_widest_add(tb, img);
    tb->col += picture_fit_add(&tb->pictures, win, img, tb->lines - 1);
    if (tb->col > tb->maxcol)
        tb->maxcol = tb->col;
#endif /* OSGLK_PIXELS */
    return 1;
}

static void tag_set_link(void *ctx, glui32 link)
{
    tagban_t *tb = ctx;
    if (tb->win && !tb->measure)
    {
        /* a link starts after the newlines that are waiting */
        if (link)
            tag_put_newlines(tb, glk_window_get_stream(tag_out(tb)));
        glk_set_hyperlink_stream(glk_window_get_stream(tag_out(tb)), link);
    }
}

static const oshtml_sink tag_sink =
{
    tag_put, tag_set_attr, tag_set_color, tag_set_screen_color,
    tag_width, tag_centers, tag_window, tag_set_link, tag_image,
    tag_rule_width
};

static tagban_t *tag_find(const char *id)
{
    tagban_t *tb;

    /* HTML TADS matches the ids exactly */
    for (tb = tagbans ; tb ; tb = tb->next)
        if (strcmp(tb->id, id) == 0)
            return tb;
    return NULL;
}

/* a size in pixels as characters, where there's no graphics window */
static glui32 tag_pix_chars(int num, int vertical)
{
    int per = vertical ? TAG_PIX_COL : TAG_PIX_LINE;
    return num <= 0 ? 0 : num < per ? 1 : (glui32)(num / per);
}

/* the window whose split from the main window gives the banner's size */
static winid_t tag_key(tagban_t *tb)
{
    return tb->gfx ? tb->gfx : tb->win;
}

/*
 *   the split of the key window from the main window: the splits of the
 *   banner's own windows inside it have other keys
 */
static winid_t tag_split(tagban_t *tb)
{
    winid_t key = tag_key(tb), pair;

    for (pair = glk_window_get_parent(key) ; pair ;
         pair = glk_window_get_parent(pair))
    {
        winid_t k = NULL;
        glk_window_get_arrangement(pair, NULL, NULL, &k);
        if (k == key)
            break;
    }
    return pair;
}

#ifdef OSGLK_PIXELS
static void tag_close_cells(tagban_t *tb)
{
    int i;

    for (i = 0 ; i < OSHTML_CELLS_MAX ; ++i)
    {
        if (tb->cellgrid[i])
            glk_window_close(tb->cellgrid[i], NULL);
        if (tb->cellpad[i])
            glk_window_close(tb->cellpad[i], NULL);
        if (tb->cellslot[i])
            glk_window_close(tb->cellslot[i], NULL);
        tb->cellgrid[i] = tb->cellpad[i] = tb->cellslot[i] = NULL;
    }
    tb->ncells = 0;
    tb->out = NULL;
    tb->cellmode = 0;
}

static void tag_close_table(tagban_t *tb)
{
    tag_close_cells(tb);
    if (tb->padwin)
        glk_window_close(tb->padwin, NULL);
    if (tb->tblwin)
        glk_window_close(tb->tblwin, NULL);
    tb->padwin = tb->tblwin = NULL;
}
#endif /* OSGLK_PIXELS */

static void tag_close_window(tagban_t *tb)
{
#ifdef OSGLK_PIXELS
    tag_close_table(tb);
#endif /* OSGLK_PIXELS */
    if (tb->win)
        glk_window_close(tb->win, NULL);
    if (tb->gfx)
        glk_window_close(tb->gfx, NULL);
    tb->win = tb->gfx = NULL;
}

static void tag_open_window(tagban_t *tb, int pixels)
{
    if (tb->type == wintype_TextGrid)
        grid_hints_set();

    if (pixels)
    {
        tb->gfx = glk_window_open(mainwin, tb->method, tb->size,
                                  wintype_Graphics, 0);
#ifdef OSGLK_PIXELS
        tb->px_alone = 0;
#endif /* OSGLK_PIXELS */
        if (tb->gfx)
            tb->win = glk_window_open(tb->gfx,
                                      (tb->method & winmethod_DirMask)
                                      | winmethod_Proportional
                                      | winmethod_NoBorder,
                                      100, tb->type, 0);
        if (tb->gfx && !tb->win)
        {
            glk_window_close(tb->gfx, NULL);
            tb->gfx = NULL;
        }
    }
    else
        tb->win = glk_window_open(mainwin, tb->method, tb->size, tb->type, 0);

    if (tb->type == wintype_TextGrid)
        grid_hints_clear();
}

/* the size of a banner fitted to its text */
static glui32 tag_fitted(tagban_t *tb)
{
#ifdef OSGLK_PIXELS
    if (tag_vertical(tb->method) && tb->pictures.unknown)
        return tb->size;
#endif /* OSGLK_PIXELS */
    if (tag_vertical(tb->method))
        return tb->maxcol ? (glui32)tb->maxcol + 2 : 0;
#ifdef OSGLK_PIXELS
    if (tb->type == wintype_TextBuffer && tb->win && !tb->measure)
        return (glui32)picture_fit_lines(&tb->pictures,
                                         osglk_px_lines(tb->win));
#endif /* OSGLK_PIXELS */
    return (glui32)tb->lines;
}

static void tag_resize(tagban_t *tb, glui32 size)
{
    if (size == tb->size || !tb->win)
        return;
    tb->size = size;
    glk_window_set_arrangement(tag_split(tb),
                               tb->method, size, tag_key(tb));
}

void *osglk_tag_banner_open(const oshtml_banner *ban,
                            const oshtml_sink **sink)
{
    tagban_t *tb = tag_find(ban->id);
    int isnew = tb == NULL || tb->win == NULL;
    glui32 method, size, type;
    int num, units, vertical, pixels;

    *sink = &tag_sink;
    if (!mainwin)
        return NULL;

    if (!tb)
    {
        tb = calloc(1, sizeof(tagban_t));
        if (!tb)
            return NULL;
        snprintf(tb->id, sizeof(tb->id), "%s", ban->id);
        tb->next = tagbans;
        tagbans = tb;
    }

    type = strcasecmp(ban->id, "statusline") == 0
        ? wintype_TextGrid : wintype_TextBuffer;

    switch (ban->align)
    {
        case OS_BANNER_ALIGN_BOTTOM: method = winmethod_Below; break;
        case OS_BANNER_ALIGN_LEFT:   method = winmethod_Left;  break;
        case OS_BANNER_ALIGN_RIGHT:  method = winmethod_Right; break;
        default:                     method = winmethod_Above; break;
    }
    method |= ban->border ? winmethod_Border : winmethod_NoBorder;
    vertical = tag_vertical(method);
    num = vertical ? ban->width : ban->height;
    units = vertical ? ban->width_units : ban->height_units;

    /* the size; one we take from the text starts out as it was */
    tb->fit = 0;
    size = tb->size;
    pixels = 0;
#ifdef OSGLK_PIXELS
    if (!ban->cells && tb->ncells)
        tag_close_cells(tb);
    if (ban->cells && !vertical && type == wintype_TextGrid
        && glk_gestalt(gestalt_Graphics, 0))
    {
        /* a line of cells over pictures, as tall as the two */
        method |= winmethod_Fixed;
        size = ban->cells;
        pixels = 1;
    }
    else
#endif /* OSGLK_PIXELS */
    if (units == OSHTML_SIZE_PREV && !isnew
        && tag_vertical(tb->method) == vertical)
    {
        method |= tb->method & winmethod_DivisionMask;
        pixels = tb->gfx != NULL;
    }
    else if (units == OSHTML_SIZE_PCT)
    {
        method |= winmethod_Proportional;
        size = num > 100 ? 100 : (glui32)num;
    }
    else if (units == OSHTML_SIZE_PIX && glk_gestalt(gestalt_Graphics, 0))
    {
        /* exactly, in a graphics window */
        method |= winmethod_Fixed;
        size = num <= 0 ? 0 : (glui32)num;
        pixels = 1;
#ifdef OSGLK_PIXELS
        /* as it shrank or grew the last time, so it needn't change first */
        if (!isnew && tb->px_asked == num
            && (vertical ? tb->px_shrunk : tb->px_grown))
            size = vertical ? tb->px_shrunk : tb->px_grown;
        tb->px_asked = num;
#endif /* OSGLK_PIXELS */
    }
    else if (units == OSHTML_SIZE_PIX)
    {
        method |= winmethod_Fixed;
        size = tag_pix_chars(num, vertical);
    }
    else
    {
        method |= winmethod_Fixed;
        if (isnew || (tb->method & winmethod_DivisionMask) != winmethod_Fixed)
            size = 1;
        tb->fit = 1;
    }

    /*
     *   a window can't turn into another kind, or to the other axis, or
     *   take its size from a graphics window or stop
     */
    if (tb->win && (type != tb->type
                    || tag_vertical(tb->method) != vertical
                    || pixels != (tb->gfx != NULL)))
        tag_close_window(tb);
    tb->type = type;

    if (!tb->win)
    {
        tb->method = method;
        tb->size = size;
        tag_open_window(tb, pixels);
        if (!tb->win)
        {
            if (!pixels)
                return NULL;

            /* no graphics window after all: the size in characters */
            tb->size = tag_pix_chars(num, vertical);
            tb->px_asked = num;
            tag_open_window(tb, 0);
            if (!tb->win)
                return NULL;
        }

        /* the status line of text mode makes way for this one */
        if (type == wintype_TextGrid && statuswin)
        {
            glk_window_close(statuswin, NULL);
            statuswin = NULL;
        }
    }
    else
    {
        if (method != tb->method || size != tb->size)
        {
            tb->method = method;
            tb->size = size;
            glk_window_set_arrangement(tag_split(tb),
                                       method, size, tag_key(tb));
        }
        glk_window_clear(tb->win);
    }

    /* the default colours, unless the text has a <BODY> */
    if (tb->bg)
    {
        osglk_set_window_bg(tb->win, zcolor_Default);
        tb->bg = 0;
    }
    osglk_zcolors_stream(glk_window_get_stream(tb->win),
                              zcolor_Default, zcolor_Default);
    glk_set_style_stream(glk_window_get_stream(tb->win), style_Normal);

    /* a grid can't keep lines below its last, so it counts them first */
    tb->measure = tb->fit && type == wintype_TextGrid;
    tb->lines = tb->col = tb->maxcol = tb->nl = 0;
#ifdef OSGLK_PIXELS
    picture_fit_init(&tb->pictures);
    tb->px_widest = 0;
    tb->px_text = 0;
    tb->px_pics = 0;
    tb->tbl_used = 0;
    tb->screen = zcolor_Default;
    tb->out = NULL;
    tb->cellmode = 0;
    tb->last_combo = 0;
    tb->last_fg = tb->last_bg = zcolor_Default;
#endif /* OSGLK_PIXELS */
    return tb;
}

#ifdef OSGLK_PIXELS
static void tag_table_draw(tagban_t *tb)
{
    const oshtml_pictable *t = tb->tbl;
    glui32 ww = 0, wh = 0, mx = 0, my = 0;
    int i;

    if (!tb->tblwin || !t)
        return;
    osglk_px_margins(tb->win, &mx, &my);
    osglk_set_window_bg(tb->tblwin, tb->screen);
    glk_window_clear(tb->tblwin);
    glk_window_get_size(tb->tblwin, &ww, &wh);
    tb->tbl_x = t->right ? (ww > t->width ? ww - t->width : 0)
        : !t->center ? mx
        : ww > t->width ? (ww - t->width) / 2 : 0;
    tb->tbl_y = t->page ? my
        : t->above && wh > t->height ? wh - t->height : 0;

    /* the background, tiled from the top left as in HTML TADS */
    if (t->bg.id)
    {
        glui32 bw = 0, bh = 0, x, y;
        /* a table's tiles start a little above it, so that the bevel along
           the top of a bar's picture (pill) is out of sight, as in QTads */
        glsi32 top = t->page ? 0 : -3;

        if (glk_image_get_info(t->bg.id, &bw, &bh) && bw && bh)
            for (y = 0 ; (glsi32)y + top < (glsi32)wh ; y += bh)
                for (x = 0 ; x < ww ; x += bw)
                    glk_image_draw(tb->tblwin, t->bg.id, (glsi32)x,
                                   (glsi32)y + top);
    }
    for (i = 0 ; i < t->ntiles ; ++i)
    {
        const oshtml_tile *tile = &t->tiles[i];
        if (tile->area)
            continue;
        glk_image_draw_scaled(tb->tblwin, tile->img.id,
                              (glsi32)(tb->tbl_x + tile->x),
                              (glsi32)(tb->tbl_y + tile->y),
                              tile->w, tile->h);
    }
}

/*
 *   the pixels a banner's text takes up: as the app measures it, or
 *   failing that, what the count of its lines and pictures comes to
 */
static int tag_text_pixels(tagban_t *tb, int estimate, glui32 *px)
{
    int h = osglk_px_text_height(tb->win);
    glui32 my = 0;

    osglk_px_margins(tb->win, NULL, &my);
    if (h > 0)
        *px = (glui32)h + 2 * my;
    else if (estimate)
        *px = (glui32)(picture_fit_lines(&tb->pictures,
                                         osglk_px_lines(tb->win))
                       * osglk_px_cellh()) + 2 * my;
    else
        return 0;
    return 1;
}

static void tag_table_text(tagban_t *tb, int estimate)
{
    tag_text_pixels(tb, estimate, &tb->tbl_text);
}

#ifndef SPATTERLIGHT
/*
 *   A banner across with only pictures is at least as tall as they are:
 *   the game sized it to them as HTML TADS packs them, closer than
 *   Gargoyle's lines and margins do
 */
static void tag_fit_pictures(tagban_t *tb)
{
    glui32 asked = tb->px_asked > 0 ? (glui32)tb->px_asked : 0, h = 0;

    if (!tb->gfx || tag_vertical(tb->method) || tb->tblwin || tb->px_text
        || !tb->pictures.count || !tag_text_pixels(tb, 1, &h))
        return;
    tb->px_grown = h > asked ? h : 0;
    tag_resize(tb, h > asked ? h : asked);
}
#endif /* SPATTERLIGHT */

/*
 *   a text window's height in pixels: a text buffer gives its size in
 *   lines, but a graphics window of no width beside it is as tall as it is
 */
static glui32 tag_pixel_height(winid_t win)
{
    winid_t probe = glk_window_open(win, winmethod_Left | winmethod_Fixed
                                    | winmethod_NoBorder, 0,
                                    wintype_Graphics, 0);
    glui32 h = 0;

    if (!probe)
        return 0;
    glk_window_get_size(probe, NULL, &h);
    glk_window_close(probe, NULL);
    return h;
}

/*
 *   the table window is as tall as the table, or what the text leaves of
 *   the banner, so the table sits under the text as in HTML TADS; a page
 *   of pictures is all of the banner.  A picture over the text is at the
 *   bottom of its window, which is as tall as puts the two together where
 *   their cell aligns them, and what's left goes under the text.
 */
static void tag_table_place(tagban_t *tb)
{
    glui32 h = tb->tbl->height, th, wh = 0, ph = 0, total, room = 0, pad = 0;
    winid_t pair = glk_window_get_parent(tb->tblwin);

    th = tag_pixel_height(tb->win);
    glk_window_get_size(tb->tblwin, NULL, &wh);
    if (tb->padwin)
        glk_window_get_size(tb->padwin, NULL, &ph);
    total = th + wh + ph;
    if (total > tb->tbl_text && total - tb->tbl_text > h)
        room = total - tb->tbl_text - h;
    if (tb->tbl->page)
        h = total;
    else if (!tb->tbl->above || tb->tbl->valign == 2)
        h += room;
    else
    {
        pad = tb->tbl->valign == 1 ? room - room / 2 : room;
        h += room - pad;
    }

    /* the room under the text */
    if (pad && !tb->padwin)
        tb->padwin = glk_window_open(tb->win, winmethod_Below
                                     | winmethod_Fixed | winmethod_NoBorder,
                                     pad, wintype_Graphics, 0);
    else if (tb->padwin)
    {
        winid_t ppair = glk_window_get_parent(tb->padwin);
        if (ppair)
            glk_window_set_arrangement(ppair, winmethod_Below
                                       | winmethod_Fixed | winmethod_NoBorder,
                                       pad, tb->padwin);
    }
    if (tb->padwin)
    {
        osglk_set_window_bg(tb->padwin, tb->screen);
        glk_window_clear(tb->padwin);
    }
    if (pair)
        glk_window_set_arrangement(pair, (tb->tbl->above ? winmethod_Above
                                          : winmethod_Below)
                                   | winmethod_Fixed | winmethod_NoBorder,
                                   h, tb->tblwin);
}
#endif /* OSGLK_PIXELS */

glui32 osglk_banner_line_pixels(void)
{
#ifdef OSGLK_PIXELS
    double h = osglk_px_cellh();
    return h > 0 ? (glui32)ceil(h) : 0;
#else
    return 0;
#endif /* OSGLK_PIXELS */
}

int osglk_tag_banner_table(void *ctx, const oshtml_pictable *t)
{
#ifdef OSGLK_PIXELS
    tagban_t *tb = ctx;

    if (tb->fit || tb->measure || tb->type != wintype_TextBuffer || !tb->win
        || !glk_gestalt(gestalt_DrawImage, wintype_Graphics))
        return 0;
    if (!tb->tbl && !(tb->tbl = calloc(1, sizeof(*tb->tbl))))
        return 0;

    /* a picture over the text goes on the other side of it */
    if (tb->tblwin && tb->tbl->above != t->above)
        tag_close_table(tb);
    if (!tb->tblwin)
        tb->tblwin = glk_window_open(tb->win, (t->above ? winmethod_Above
                                               : winmethod_Below)
                                     | winmethod_Fixed | winmethod_NoBorder,
                                     t->height, wintype_Graphics, 0);
    if (!tb->tblwin)
        return 0;
    *tb->tbl = *t;
    tb->tbl_used = 1;
    return 1;
#else
    (void)ctx;
    (void)t;
    return 0;
#endif /* OSGLK_PIXELS */
}

#ifdef OSGLK_PIXELS
/* the cells' windows show the banner's background: the grids too, unless
   it's only to draw them again */
static void tag_cells_clear(tagban_t *tb, int grids)
{
    int i;

    for (i = 1 ; i < tb->ncells ; ++i)
    {
        winid_t w[3];
        int k;

        w[0] = tb->cellslot[i];
        w[1] = tb->cellpad[i];
        w[2] = grids ? tb->cellgrid[i] : NULL;
        for (k = 0 ; k < 3 ; ++k)
            if (w[k])
            {
                osglk_set_window_bg(w[k], tb->screen);
                glk_window_clear(w[k]);
            }
    }
}

/*
 *   the windows of the cells after the first, from the right: each one's
 *   graphics window holds its grid and the room to the next one's
 */
static int tag_open_cells(tagban_t *tb, const oshtml_celltable *ct)
{
    glui32 edge = 0;
    int i;

    for (i = ct->ncells - 1 ; i >= 1 ; --i)
    {
        const oshtml_textcell *c = &ct->cells[i];
        glui32 gap = c->right > edge ? c->right - edge : 0;

        tb->cellslot[i] = glk_window_open(tb->win, winmethod_Right
                                          | winmethod_Fixed
                                          | winmethod_NoBorder,
                                          c->w + gap, wintype_Graphics, 0);
        if (!tb->cellslot[i])
            return 0;
        if (gap)
            tb->cellpad[i] = glk_window_open(tb->cellslot[i],
                                             winmethod_Right
                                             | winmethod_Fixed
                                             | winmethod_NoBorder,
                                             gap, wintype_Graphics, 0);
        grid_hints_set();
        tb->cellgrid[i] = glk_window_open(tb->cellslot[i], winmethod_Left
                                          | winmethod_Proportional
                                          | winmethod_NoBorder,
                                          100, wintype_TextGrid, 0);
        grid_hints_clear();
        if (!tb->cellgrid[i])
            return 0;
        edge = c->right + c->w;
    }
    return 1;
}
#endif /* OSGLK_PIXELS */

int osglk_tag_banner_cells(void *ctx, const oshtml_celltable *ct)
{
#ifdef OSGLK_PIXELS
    tagban_t *tb = ctx;
    int i, same;

    if (tb->type != wintype_TextGrid || !tb->gfx || !tb->win || tb->measure
        || ct->ncells < 2 || ct->ncells > OSHTML_CELLS_MAX
        || !glk_gestalt(gestalt_DrawImage, wintype_Graphics))
        return 0;
    if (!tb->tbl && !(tb->tbl = calloc(1, sizeof(*tb->tbl))))
        return 0;

    /* the windows are kept while the columns are as they were */
    same = tb->ncells == ct->ncells;
    for (i = 1 ; same && i < ct->ncells ; ++i)
        same = tb->cell[i].w == ct->cells[i].w
            && tb->cell[i].right == ct->cells[i].right;
    if (!same)
        tag_close_cells(tb);
    if (tb->tblwin && tb->tbl->above)
        tag_close_table(tb);
    if (!tb->tblwin)
        tb->tblwin = glk_window_open(tb->win, winmethod_Below
                                     | winmethod_Fixed | winmethod_NoBorder,
                                     ct->pics.height, wintype_Graphics, 0);
    else
    {
        winid_t pair = glk_window_get_parent(tb->tblwin);
        if (pair)
            glk_window_set_arrangement(pair, winmethod_Below
                                       | winmethod_Fixed | winmethod_NoBorder,
                                       ct->pics.height, tb->tblwin);
    }
    if (!tb->tblwin)
        return 0;
    if (!same && !tag_open_cells(tb, ct))
    {
        tag_close_table(tb);
        return 0;
    }
    tb->ncells = ct->ncells;
    memcpy(tb->cell, ct->cells, sizeof(tb->cell));
    tag_cells_clear(tb, 1);
    *tb->tbl = ct->pics;
    tb->tbl_used = 1;
    return 1;
#else
    (void)ctx;
    (void)ct;
    return 0;
#endif /* OSGLK_PIXELS */
}

void osglk_tag_banner_cell(void *ctx, int n)
{
#ifdef OSGLK_PIXELS
    tagban_t *tb = ctx;
    winid_t win;
    strid_t str;
    glui32 cols = 0, start = 0, chars;

    tb->out = n > 0 && n < tb->ncells ? tb->cellgrid[n] : NULL;
    tb->cellmode = n >= 0 && n < tb->ncells;
    tb->lines = tb->col = tb->nl = 0;
    tb->cell_text = tb->cell_space = 0;
    if (!tb->cellmode)
        return;

    /* where the text starts, as the cell aligns it */
    win = tag_out(tb);
    glk_window_get_size(win, &cols, NULL);
    chars = tb->cell[n].chars > 0 ? (glui32)tb->cell[n].chars : 0;
    if (n == 0 && tb->cell[n].align == 0)
        start = 1;                  /* the status line's margin */
    else if (chars < cols)
        start = tb->cell[n].align == 1 ? (cols - chars) / 2
            : tb->cell[n].align == 2 ? cols - chars : 0;
    glk_window_move_cursor(win, start, 0);
    tb->col = (int)start;

    /* in the style and colours the last cell left */
    str = glk_window_get_stream(win);
    glk_set_style_stream(str, osstyles_style(tb->last_combo));
    osglk_zcolors_stream(str, tb->last_fg, tb->last_bg);
#else
    (void)ctx;
    (void)n;
#endif /* OSGLK_PIXELS */
}

glui32 osglk_tag_banner_click(winid_t win, glui32 x, glui32 y)
{
#ifdef OSGLK_PIXELS
    tagban_t *tb;
    int i;

    for (tb = tagbans ; tb ; tb = tb->next)
        if (win && tb->tblwin == win && tb->tbl)
            break;
    if (!tb || x < tb->tbl_x || y < tb->tbl_y)
        return 0;
    x -= tb->tbl_x;
    y -= tb->tbl_y;
    for (i = 0 ; i < tb->tbl->ntiles ; ++i)
    {
        const oshtml_tile *tile = &tb->tbl->tiles[i];
        if (tile->link && x >= tile->x && x < tile->x + tile->w
            && y >= tile->y && y < tile->y + tile->h)
            return tile->link;
    }
#else
    (void)win;
    (void)x;
    (void)y;
#endif /* OSGLK_PIXELS */
    return 0;
}

void osglk_tag_banner_redraw(void)
{
#ifdef OSGLK_PIXELS
    tagban_t *tb;

    for (tb = tagbans ; tb ; tb = tb->next)
    {
        if (tb->px_alone && tb->gfx)
            osglk_draw_side_picture(tb->gfx, &tb->px_img, tb->screen);
        if (tb->tblwin && tb->tbl)
        {
            if (!tb->ncells)
            {
                tag_table_text(tb, 0);
                tag_table_place(tb);
            }
            tag_cells_clear(tb, 0);
            tag_table_draw(tb);
        }
    }
#endif /* OSGLK_PIXELS */
}

/*
 *   A banner fitted to its text was measured when its text was done, but
 *   the windows around it may have changed since (the side banners shrink to
 *   their pictures), and the text with them, so it would scroll: grow it to
 *   the text as it is now
 */
void osglk_tag_banner_refit(void)
{
    tagban_t *tb;

    for (tb = tagbans ; tb ; tb = tb->next)
    {
        int h;

        if (!tb->fit_px || !tb->win || tb->measure)
            continue;
        h = osglk_px_text_height(tb->win);
        if (h > tb->fit_px)
        {
            osglk_set_text_height(tb->win, h);
            tb->fit_px = h;
        }
    }
}

int osglk_tag_banner_done(void *ctx)
{
    tagban_t *tb = ctx;

    if (tb->fit)
        tag_resize(tb, tag_fitted(tb));
#ifdef OSGLK_PIXELS
#ifndef SPATTERLIGHT
    if (!tb->measure)
        tag_fit_pictures(tb);
#endif /* SPATTERLIGHT */
    /* the text's own height, unless a picture in the margin is taller */
    if (tb->type == wintype_TextBuffer && tb->win && !tag_vertical(tb->method))
    {
        int h = 0;

        if (!tb->measure && tb->gfx && !tb->tblwin && !tb->pictures.count)
        {
            /* a height in pixels is at least that of the text and margins */
            glui32 my = 0, want;

            osglk_px_margins(tb->win, NULL, &my);
            h = osglk_px_text_height(tb->win);
            want = (glui32)h + 2 * my;
            if (h > 0 && tb->px_asked > 0)
            {
                tb->px_grown = want > (glui32)tb->px_asked ? want : 0;
                tag_resize(tb, want > (glui32)tb->px_asked
                           ? want : (glui32)tb->px_asked);
            }
            h = 0;
        }
        if (tb->fit && tb->size && !tb->pictures.unknown
            && !tb->pictures.bottom)
            h = osglk_px_text_height(tb->win);
        else if (!tb->fit && !tb->gfx && tb->px_asked > 0
                 && (tb->method & winmethod_DivisionMask) == winmethod_Fixed
                 && !tb->pictures.count)
        {
            /* a height in pixels, in a window of lines: the text fits it */
            glui32 my = 0;
            int asked;

            osglk_px_margins(tb->win, NULL, &my);
            asked = tb->px_asked - 2 * (int)my;
            h = osglk_px_text_height(tb->win);
            if (h > 0 && h < asked)
                h = asked;
        }
        osglk_set_text_height(tb->win, h);
        tb->fit_px = tb->fit ? h : 0;
    }
    else if (tb->gfx && tag_vertical(tb->method) && !tb->measure)
    {
        int alone;
        glui32 size = tag_side_pixels(tb, &alone), mx = 0;

        /* wide enough for its table of pictures, whatever it asked for:
           the left margin it is drawn at, and its own width */
        osglk_px_margins(tb->win, &mx, NULL);
        if (tb->tbl_used && tb->tbl && size < tb->tbl->width + mx)
            size = tb->tbl->width + mx;
        tb->px_shrunk = size < (glui32)tb->px_asked ? size : 0;
        tag_resize(tb, size);
        tag_side_alone(tb, alone);
    }
    if (tb->tblwin && !tb->measure)
    {
        if (tb->tbl_used)
        {
            if (!tb->ncells)
            {
                tag_table_text(tb, 1);
                tag_table_place(tb);
            }
            tag_table_draw(tb);
        }
        else
            tag_close_table(tb);
    }
#endif /* OSGLK_PIXELS */
    tb->lines = tb->col = tb->maxcol = tb->nl = 0;
#ifdef OSGLK_PIXELS
    picture_fit_init(&tb->pictures);
#endif /* OSGLK_PIXELS */
    if (tb->measure)
    {
        /* now the text again, for real */
        tb->measure = 0;
        return 1;
    }
    return 0;
}

void osglk_tag_banner_request_links(void)
{
    tagban_t *tb;

    for (tb = tagbans ; tb ; tb = tb->next)
    {
        if (tb->win && !tb->measure)
            glk_request_hyperlink_event(tb->win);
#ifdef OSGLK_PIXELS
        if (tb->tblwin)
            glk_request_mouse_event(tb->tblwin);
#endif /* OSGLK_PIXELS */
    }
}

void osglk_tag_banner_remove(const char *id)
{
    tagban_t **link = &tagbans;

    while (*link)
    {
        tagban_t *tb = *link;
        if (id && strcmp(tb->id, id) != 0)
        {
            link = &tb->next;
            continue;
        }
        *link = tb->next;
        tag_close_window(tb);
#ifdef OSGLK_PIXELS
        free(tb->tbl);
#endif /* OSGLK_PIXELS */
        free(tb);
    }
}
