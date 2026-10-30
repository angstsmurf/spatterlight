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

/* osansi4.c -- glk banner interface */

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

typedef struct os_banner_s *osbanid_t;
typedef struct banner_contents_s *contentid_t;

/* for tracking banner windows */
typedef struct os_banner_s
{
    glui32 id;                      /* unique identifier */
    glui32 valid;                   /* banner status */

    osbanid_t prev;                 /* previous sibling */
    osbanid_t next;                 /* next sibling */
    osbanid_t children;             /* child's descendents */
    osbanid_t parent;               /* child's parent */

    glui32 method;                  /* glk window method */
    glui32 size;                    /* glk window size */
    glui32 type;                    /* glk window type */
    glui32 status;                  /* glk status style */
    int textpx;                     /* fitted to its text's pixels, or 0 */
    glui32 asked_method;            /* the method and size the game asked */
    glui32 asked_size;              /* for, in its units (banner_gfx_size) */
    glui32 drawn_w, drawn_h;        /* the graphics window size drawn at */
    glui32 repad;                   /* text banner shown as a grid */

    glui32 cheight;                 /* glk char height */
    glui32 cwidth;                  /* glk char width */

    glui32 fgcolor;                 /* foreground color */
    glui32 bgcolor;                 /* background color */
    glui32 bgscreen;                /* the game set bgcolor */

    contentid_t contents;           /* window contents */
    glui32 style;                   /* active Glk style value */
    os_color_t fg, bg;              /* active text colours, as set by game */
    glui32 link;                    /* active hyperlink (HTML mode) */
    glui32 newline;                 /* active newline */
    glui32 move, x, y;              /* active cursor position */

    winid_t win;                    /* glk window object */

    oshtml_parser *html;            /* HTML mode: the markup parser */
}
os_banner_t;

/* for reprinting banner contents */
typedef struct banner_contents_s
{
    osbanid_t banner;               /* content owner */
    contentid_t next;               /* next set of contents */

    glui32 style;                   /* stored contents style */
    os_color_t fg, bg;              /* stored colours */
    glui32 link;                    /* stored hyperlink */
    glui32 newline;                 /* stored newline */
    glui32 move, x, y;              /* stored cursor position */
    glui32 width;                   /* banner width when written */

    char *chars;
    glui32 len;

    int has_image;                  /* HTML mode: a picture, not text */
    oshtml_image image;
    char *shown;                    /* chars re-padded to a new width */
    glui32 shownlen;
    int shift;                      /* byte offset of shown from chars */
}
banner_contents_t;

static osbanid_t os_banners = NULL;
static glui32 os_banner_count = 999;

#ifdef OSGLK_PIXELS
void picture_fit_init(picture_fit *fit)
{
    fit->extra = 0;
    fit->row = -1;
    fit->row_extra = 0;
    fit->bottom = 0;
    fit->unknown = 0;
    fit->last = 0;
    fit->count = 0;
}

/*
 *   a picture that starts on the given line (from 0): the columns it takes
 *   up, at the size it's drawn at in the window
 */
int picture_fit_add(picture_fit *fit, winid_t win,
                    const oshtml_image *img, int line)
{
    glui32 w, h, width = osglk_px_width(win);
    double cellw = osglk_px_cellw(), cellh = osglk_px_cellh();
    int lines;

    ++fit->count;
    if (cellw <= 0 || cellh <= 0
        || !oshtml_image_size(img, width, &w, &h))
    {
        fit->unknown = 1;
        return 0;
    }

    lines = (int)ceil(h / cellh);
    if (img->align == imagealign_MarginLeft
        || img->align == imagealign_MarginRight)
    {
        if (line + lines > fit->bottom)
            fit->bottom = line + lines;
    }
    else
    {
        fit->last = 1;

        /* the tallest picture on a line sets its height */
        if (line != fit->row)
        {
            fit->row = line;
            fit->row_extra = 0;
        }
        if (lines - 1 > fit->row_extra)
        {
            fit->extra += lines - 1 - fit->row_extra;
            fit->row_extra = lines - 1;
        }
    }
    return (int)ceil(w / cellw);
}

/* the lines a banner fits at, with the app's count of its text lines */
int picture_fit_lines(const picture_fit *fit, int lines)
{
#ifdef SPATTERLIGHT
    lines += fit->extra;
#else
    /* and the line under the last picture, where the next text would go */
    lines += fit->last;
#endif /* SPATTERLIGHT */
    return lines > fit->bottom ? lines : fit->bottom;
}
#endif /* OSGLK_PIXELS */

void banner_contents_display(contentid_t contents);
void banner_contents_repad(osbanid_t banner);

/* Implementation-specific functions for managing banner windows */
/*
    os_banner_init();
    os_banner_insert();
    os_banner_styles_apply();
    os_banners_close();
    os_banners_open();
    os_banners_redraw();
*/

osbanid_t os_banner_init(void)
{
    osbanid_t instance;
    instance = malloc(sizeof(os_banner_t));
    if (!instance)
        return 0;

    instance->id = ++ os_banner_count;
    instance->valid = 1;

    instance->prev = 0;
    instance->next = 0;
    instance->children = 0;
    instance->parent = 0;

    instance->method = 0;
    instance->size = 0;
    instance->type = 0;
    instance->status = 0;
    instance->textpx = 0;
    instance->asked_method = 0;
    instance->asked_size = 0;
    instance->drawn_w = 0;
    instance->drawn_h = 0;
    instance->repad = 0;

    instance->cheight = 0;
    instance->cwidth = 0;

    instance->fgcolor = 0;
    instance->bgcolor = 0;
    instance->bgscreen = 0;

    instance->contents = 0;
    instance->style = style_Normal;
    instance->fg = OS_COLOR_P_TEXT;
    instance->bg = OS_COLOR_P_TRANSPARENT;
    instance->link = 0;
    instance->newline = 0;
    instance->move = 0;
    instance->x = 0;
    instance->y = 0;

    instance->win = 0;
    instance->html = 0;

    return instance;
}

osbanid_t os_banner_insert(osbanid_t parent, glui32 operation, osbanid_t other,
                           glui32 method, glui32 size, glui32 type,
                           glui32 status, glui32 repad)
{
    if (!parent || !(parent->valid))
        return 0;

    if (operation == OS_BANNER_BEFORE || operation == OS_BANNER_AFTER)
        if (!other || !(other->valid) || !(other->parent == parent))
            operation = OS_BANNER_LAST;

    osbanid_t baby = os_banner_init();
    if (!baby)
        return 0;
    baby->parent = parent;

    if (!(parent->children))
    {
        parent->children = baby;
    }
    else
    {
        osbanid_t child = parent->children;

        switch (operation)
        {
            case OS_BANNER_FIRST:
                parent->children = baby;
                baby->next = child;
                child->prev = baby;
                break;

            case OS_BANNER_BEFORE:
                while (child != other && child->next)
                    child = child->next;

                if (child->prev)
                {
                    child->prev->next = baby;
                    baby->prev = child->prev;
                }
                else
                {
                    parent->children = baby;
                }

                baby->next = child;
                child->prev = baby;
                break;

            case OS_BANNER_LAST:
                while(child->next)
                    child = child->next;

                baby->prev = child;
                child->next = baby;
                break;

            case OS_BANNER_AFTER:
                while (child != other && child->next)
                    child = child->next;

                if (child->next)
                {
                    child->next->prev = baby;
                    baby->next = child->next;
                }

                baby->prev = child;
                child->next = baby;
                break;

            default: break;
        }
    }

    baby->method = method;
    baby->size = size;
    baby->asked_method = method;
    baby->asked_size = size;
    baby->type = type;
    baby->status = status;
    baby->repad = repad;

    return baby;
}

/*
 *   The theme's colours, which osglk.c measures again when it changes: a
 *   status banner is a reversed grid, like the status line
 */
void os_banner_styles_apply (osbanid_t banner)
{
    if (!banner || !(banner->valid))
        return;

    banner->fgcolor = banner->status ? statusbg : mainfg;
    if (!banner->bgscreen)
        banner->bgcolor = banner->status ? statusfg : mainbg;
}

/*
 *   A banner shows the main window's styles (osstyles.c), whose hints text
 *   buffers already have.  A grid gets their bold and italic just while a
 *   banner's grid opens, as hints only reach the windows opened after them,
 *   and not the reversed User1 of the text-mode status line.
 */
void grid_hints_set(void)
{
    int s, c;

    for (s = 0 ; s < style_NUMSTYLES ; ++s)
        if ((c = osstyles_combo(s)) > 0)
        {
            glk_stylehint_set(wintype_TextGrid, s, stylehint_Weight,
                              (c & OSSTY_BOLD) ? 1 : 0);
            glk_stylehint_set(wintype_TextGrid, s, stylehint_Oblique,
                              (c & OSSTY_ITALIC) ? 1 : 0);
        }
    glk_stylehint_clear(wintype_TextGrid, style_User1, stylehint_ReverseColor);
}

void grid_hints_clear(void)
{
    int s;

    for (s = 0 ; s < style_NUMSTYLES ; ++s)
        if (osstyles_combo(s) > 0)
        {
            glk_stylehint_clear(wintype_TextGrid, s, stylehint_Weight);
            glk_stylehint_clear(wintype_TextGrid, s, stylehint_Oblique);
        }
}

/*
 *   A tab-aligned text banner has its styles in fixed pitch, for its
 *   columns, keeping their bold and italic
 */
static void fixed_hints_set(void)
{
    int s, c;

    for (s = 0 ; s < style_NUMSTYLES ; ++s)
        if ((c = osstyles_combo(s)) >= 0 && !(c & OSSTY_INPUT))
            glk_stylehint_set(wintype_TextBuffer, s, stylehint_Proportional, 0);
}

/* back to the main window's (osstyles_set_hints) */
static void fixed_hints_clear(void)
{
    int s, c;

    for (s = 0 ; s < style_NUMSTYLES ; ++s)
    {
        if ((c = osstyles_combo(s)) < 0 || (c & OSSTY_INPUT))
            continue;
        if (s == style_Normal)
            glk_stylehint_clear(wintype_TextBuffer, s, stylehint_Proportional);
        else
            glk_stylehint_set(wintype_TextBuffer, s, stylehint_Proportional,
                              (c & OSSTY_FIXED) ? 0 : 1);
    }
}

/*
 *   A colour as a zcolor.  The parameter colours are the banner's own, and
 *   the theme's where the banner has no colour of its own.
 */
static glui32 banner_zcolor(osbanid_t banner, os_color_t color, int isbg)
{
    int bglike, theme;

    if (!os_color_is_param(color))
        return (glui32)color;

    bglike = color == OS_COLOR_P_TEXTBG || color == OS_COLOR_P_STATUSBG
        || (isbg && color == OS_COLOR_P_TRANSPARENT);
    if (bglike)
        theme = isbg && !banner->status && !banner->bgscreen;
    else
        theme = !isbg && !banner->status;
    if (theme)
        return zcolor_Default;
    return bglike ? banner->bgcolor : banner->fgcolor;
}

/* the colours of the text that has none of its own, in the window's stream */
static void banner_default_zcolors(osbanid_t banner)
{
    osglk_zcolors_stream(glk_window_get_stream(banner->win),
                              banner_zcolor(banner, OS_COLOR_P_TEXT, 0),
                              banner_zcolor(banner, OS_COLOR_P_TRANSPARENT, 1));
}

/*
 *   A graphics banner has no text to reverse, so a status banner's paper is
 *   the theme's, as the text banners beside it show, unless the game gave
 *   it a colour
 */
static void banner_set_background(osbanid_t banner)
{
    glui32 bg = banner_zcolor(banner, OS_COLOR_P_TRANSPARENT, 1);

    if (banner->type == wintype_Graphics && !banner->bgscreen)
        bg = zcolor_Default;
    osglk_set_window_bg(banner->win, bg);
}

/*
 *   A text banner at the side is a graphics window, as wide as the picture
 *   it shows, until it shows anything else: then it's a text window again,
 *   as the game asked.  The game asks for a width in characters, and a
 *   picture alone in that many of the buffer's cells, which are wider than
 *   the game counted on, has only the banner's background beside it, as in
 *   According to Cain.  A graphics window also keeps the top of a picture
 *   taller than the banner in view, as HTML TADS does.
 */
static int banner_side_text(int wintype, int align)
{
#ifdef OSGLK_PIXELS
    return wintype == OS_BANNER_TYPE_TEXT
        && (align == OS_BANNER_ALIGN_LEFT || align == OS_BANNER_ALIGN_RIGHT)
        && glk_gestalt(gestalt_DrawImage, wintype_Graphics);
#else
    (void)wintype;
    (void)align;
    return 0;
#endif /* OSGLK_PIXELS */
}

/* the picture a graphics banner shows, if it has one */
static const oshtml_image *banner_gfx_image(osbanid_t banner)
{
    for (contentid_t c = banner->contents; c; c = c->next)
        if (c->has_image)
            return &c->image;
    return NULL;
}

/* a picture a graphics banner can show: one of a size of its own */
static int banner_gfx_size_of(const oshtml_image *img, glui32 *w, glui32 *h)
{
    return !img->width_pct && !img->height_pct
        && oshtml_image_size(img, 0, w, h) && *w && *h;
}

/*
 *   The margin beside a graphics banner's cells: HTML TADS's banner margin,
 *   as the game counted on, not the buffer's, which has no text to keep
 *   from the edge here
 */
#define BANNER_GFX_MARGIN 8

/*
 *   A graphics banner's method and size: as wide as its picture, or as the
 *   text window the game asked for would be in HTML TADS
 */
static glui32 banner_gfx_size(osbanid_t banner, glui32 *method)
{
    const oshtml_image *img = banner_gfx_image(banner);
    glui32 dir = banner->asked_method & winmethod_DirMask, w, h;

    /* a banner the game hides at no size stays hidden, picture or not */
    if (img && banner->asked_size && banner_gfx_size_of(img, &w, &h))
    {
        *method = dir | winmethod_Fixed;
        return w;
    }
    *method = banner->asked_method;
    if ((banner->asked_method & winmethod_DivisionMask) != winmethod_Fixed)
        return banner->asked_size;
#ifdef OSGLK_PIXELS
    if (banner->asked_size)
        return (glui32)ceil(banner->asked_size * osglk_px_cellw())
            + 2 * BANNER_GFX_MARGIN;
#endif /* OSGLK_PIXELS */
    return 0;
}

/* a graphics banner resized to its contents: true if it's redrawn so */
static int banner_gfx_fit(osbanid_t banner)
{
    glui32 method, size = banner_gfx_size(banner, &method);

    if (method == banner->method && size == banner->size)
        return 0;
    banner->method = method;
    banner->size = size;
    os_banners_redraw();
    return 1;
}

/* the picture from the top, at its own size, on the banner's background */
static void banner_gfx_draw(osbanid_t banner)
{
    const oshtml_image *img = banner_gfx_image(banner);
    glui32 w, h;

    glk_window_get_size(banner->win, &banner->drawn_w, &banner->drawn_h);
    banner_set_background(banner);
    glk_window_clear(banner->win);
    if (img && banner_gfx_size_of(img, &w, &h))
        glk_image_draw_scaled(banner->win, img->id, 0, 0, w, h);
}

/* the banner a text window again, with its contents */
static void banner_to_text(osbanid_t banner)
{
    banner->type = wintype_TextBuffer;
    banner->method = banner->asked_method;
    banner->size = banner->asked_size;
    os_banners_redraw();
}

/* text that a graphics banner needn't show: spaces and newlines */
static int banner_blank(const char *txt, size_t len)
{
    for (size_t i = 0; i < len; ++i)
        if (txt[i] != ' ' && txt[i] != '\n' && txt[i] != '\t')
            return 0;
    return 1;
}

/* a graphics banner's size in characters, counted as banner_gfx_size does */
static void banner_gfx_cells(osbanid_t banner)
{
#ifdef OSGLK_PIXELS
    double cellw = osglk_px_cellw(), cellh = osglk_px_cellh();
    glui32 mx = BANNER_GFX_MARGIN, my = 0;

    if (banner->type != wintype_Graphics || cellw <= 0 || cellh <= 0)
        return;
    osglk_px_margins(mainwin, NULL, &my);
    banner->cwidth = banner->cwidth > 2 * mx
        ? (glui32)((banner->cwidth - 2 * mx) / cellw) : 0;
    banner->cheight = banner->cheight > 2 * my
        ? (glui32)((banner->cheight - 2 * my) / cellh) : 0;
#else
    (void)banner;
#endif /* OSGLK_PIXELS */
}

void os_banners_close(osbanid_t banner)
{
    if (!banner)
        return;

    os_banners_close(banner->children);
    os_banners_close(banner->next);

    if (banner->win && (banner->win != mainwin))
    {
        glk_window_close(banner->win, 0);
        banner->win = 0;
    }
}

void os_banners_open(osbanid_t banner)
{
    if (!banner)
        return;

    if (banner->valid)
    {
        /*
         *   a banner of no size has a window too, to keep its text, as a
         *   banner fitted to its contents may be until it has some
         */
        if (banner->parent && banner->parent->win)
        {
            os_banner_styles_apply(banner);
            if (banner->type == wintype_TextGrid)
                grid_hints_set();
            else if (banner->status)
                fixed_hints_set();
            banner->win = glk_window_open(banner->parent->win,
                                          banner->method, banner->size,
                                          banner->type, banner->id);
            if (banner->type == wintype_TextGrid)
                grid_hints_clear();
            else if (banner->status)
                fixed_hints_clear();
            if (banner->win)
            {
                banner_contents_repad(banner);
                osglk_set_text_height(banner->win, banner->textpx);
                if (banner->status || banner->bgscreen)
                    banner_set_background(banner);
                banner_contents_display(banner->contents);
            }
        }
        os_banners_open(banner->children);
    }

    os_banners_open(banner->next);
}

void os_banners_redraw()
{
    if (!os_banners)
        return;

    os_banners_close(os_banners);
    os_banners_open(os_banners);
}

/* Implementation-specific functions for managing banner contents */
/*
    banner_contents_init();
    banner_contents_insert();
    banner_contents_display();
    banner_contents_clear();
*/

contentid_t banner_contents_init(void)
{
    contentid_t instance;
    instance = malloc(sizeof(banner_contents_t));
    if (!instance)
        return 0;

    instance->banner = 0;
    instance->next = 0;

    instance->style = style_Normal;
    instance->fg = OS_COLOR_P_TEXT;
    instance->bg = OS_COLOR_P_TRANSPARENT;
    instance->link = 0;
    instance->newline = 0;
    instance->move = 0;
    instance->x = 0;
    instance->y = 0;
    instance->width = 0;

    instance->chars = 0;
    instance->len = 0;
    instance->has_image = 0;

    instance->shown = 0;
    instance->shownlen = 0;
    instance->shift = 0;

    return instance;
}

void banner_contents_insert(contentid_t contents, const char *txt, size_t len)
{
    if (!contents)
        return;

    contents->chars = malloc(sizeof(char) * (len + 1));
    if (!(contents->chars))
        return;

    memcpy(contents->chars, txt, len);

    contents->chars[len] = '\0';
    contents->len = (glui32)len;
}

void banner_contents_display(contentid_t contents)
{
    if (!contents || !(contents->banner))
        return;

    /* a graphics banner has only its picture to show */
    if (contents->banner->type == wintype_Graphics)
    {
        if (contents->banner->win)
            banner_gfx_draw(contents->banner);
        return;
    }

    winid_t win = contents->banner->win;
    char *chars = contents->shown ? contents->shown : contents->chars;
    glui32 len = contents->shown ? contents->shownlen : contents->len;

    glk_set_window(win);

    if (contents->newline)
    {
        unsigned char ch = '\n';
        os_put_buffer((unsigned char *)&ch, 1);
    }
    
    if (len && (chars[len-1] == '\n'))
    {
        len --;
        contents->banner->newline = 1;
    }
    else
    {
        contents->banner->newline = 0;
    }

    if (contents->move)
    {
        glk_window_move_cursor(win, contents->x, contents->y);
        contents->banner->move = 0;
        contents->banner->x = 0;
        contents->banner->y = 0;
    }

    glk_set_style(contents->style);
    osglk_zcolors(banner_zcolor(contents->banner, contents->fg, 0),
                       banner_zcolor(contents->banner, contents->bg, 1));
    if (contents->link)
        glk_set_hyperlink(contents->link);
    if (contents->has_image)
        oshtml_draw_image(win, &contents->image);
    else
        os_put_buffer((unsigned char *)chars, len);
    if (contents->link)
        glk_set_hyperlink(0);
    glk_set_window(mainwin);
    banner_contents_display(contents->next);
}

/*
 *   A text banner shown as a grid holds lines the VM padded with spaces to
 *   the width at the time of writing, e.g. the right-aligned score on the
 *   status line. When the grid is reopened at another width, grow or shrink
 *   the longest run of spaces on each such line, so that it lines up with
 *   the new right edge instead of falling off it until the game redraws.
 */

static glui32 banner_text_columns(const char *txt, glui32 len)
{
    if (!len)
        return 0;

    glui32 *out = malloc(sizeof(glui32) * len);
    if (!out)
        return len;

    glui32 cols = os_parse_chars((unsigned char *)txt, len, out, len);
    free(out);

    /* os_put_buffer() prints the raw bytes when parsing fails */
    return cols ? cols : len;
}

static void banner_contents_adjust(contentid_t contents, glui32 pos, int adj)
{
    if (!contents->shown)
    {
        contents->shown = malloc(contents->len + 1);
        if (!contents->shown)
            return;
        memcpy(contents->shown, contents->chars, contents->len + 1);
        contents->shownlen = contents->len;
        contents->shift = 0;
    }

    pos += contents->shift;

    if (adj > 0)
    {
        char *grown = realloc(contents->shown, contents->shownlen + adj + 1);
        if (!grown)
            return;
        contents->shown = grown;
        memmove(grown + pos + adj, grown + pos, contents->shownlen - pos + 1);
        memset(grown + pos, ' ', adj);
    }
    else
    {
        memmove(contents->shown + pos, contents->shown + pos - adj,
                contents->shownlen - pos + adj + 1);
    }

    contents->shownlen += adj;
    contents->shift += adj;
}

typedef struct banner_line_s
{
    glui32 width;                   /* banner width when the line was written */
    glui32 cols;                    /* columns so far */
    contentid_t run;                /* longest run of spaces */
    glui32 runpos, runlen, runcol;
    contentid_t cur;                /* run of spaces being scanned */
    glui32 curpos, curlen, curcol;
}
banner_line_t;

static void banner_line_end(banner_line_t *line, glui32 width)
{
    /* a right-aligned tab pads the line to one short of the full width */
    if (line->run && line->width && line->width != width
        && line->cols + 1 == line->width)
    {
        int adj = (int)width - (int)line->width;
        int keep = line->runcol ? 1 : 0;
        if ((int)line->runlen + adj < keep)
            adj = keep - (int)line->runlen;
        if (adj)
            banner_contents_adjust(line->run, line->runpos, adj);
    }

    memset(line, 0, sizeof(*line));
}

void banner_contents_repad(osbanid_t banner)
{
    contentid_t contents;
    glui32 width, height;
    banner_line_t line;

    for (contents = banner->contents; contents; contents = contents->next)
    {
        if (contents->shown)
            free(contents->shown);
        contents->shown = 0;
        contents->shownlen = 0;
        contents->shift = 0;
    }

    if (!banner->repad || !banner->win)
        return;

    glk_window_get_size(banner->win, &width, &height);

    memset(&line, 0, sizeof(line));
    line.width = banner->contents ? banner->contents->width : 0;

    for (contents = banner->contents; contents; contents = contents->next)
    {
        glui32 len = contents->len;
        glui32 seg = 0;
        glui32 i;

        /* the same breaks banner_contents_display() makes */
        if (contents->newline || contents->move)
        {
            banner_line_end(&line, width);
            line.width = contents->width;
        }

        if (len && contents->chars[len-1] == '\n')
            len --;

        line.cur = 0;

        for (i = 0; i < len; i++)
        {
            char ch = contents->chars[i];

            if (ch != ' ' && ch != '\n')
                continue;

            line.cols += banner_text_columns(contents->chars + seg, i - seg);
            seg = i + 1;

            if (ch == '\n')
            {
                banner_line_end(&line, width);
                line.width = contents->width;
                continue;
            }

            if (!line.cur || line.curpos + line.curlen != i)
            {
                line.cur = contents;
                line.curpos = i;
                line.curlen = 0;
                line.curcol = line.cols;
            }
            line.curlen ++;
            line.cols ++;

            if (line.curlen >= line.runlen)
            {
                line.run = line.cur;
                line.runpos = line.curpos;
                line.runlen = line.curlen;
                line.runcol = line.curcol;
            }
        }

        line.cols += banner_text_columns(contents->chars + seg, len - seg);
    }

    banner_line_end(&line, width);
}

/* the number of grid rows the contents reach down to */
static glui32 banner_contents_rows(contentid_t contents)
{
    glui32 row = 0;
    glui32 rows = 0;

    for (; contents; contents = contents->next)
    {
        glui32 len = contents->len;
        glui32 i;

        if (contents->newline)
            row ++;
        if (contents->move)
            row = contents->y;

        if (len && contents->chars[len-1] == '\n')
            len --;

        for (i = 0; i < len; i++)
        {
            if (contents->chars[i] == '\n')
                row ++;
            else if (row + 1 > rows)
                rows = row + 1;
        }
    }

    return rows;
}

void banner_contents_clear(contentid_t contents)
{
    if (!contents)
        return;

    banner_contents_clear(contents->next);

    if (contents->chars)
        free(contents->chars);

    if (contents->shown)
        free(contents->shown);

    free(contents);
}

/* Banner API functions */

void *os_banner_create(void *parent, int where, void *other, int wintype,
                       int align, int siz, int siz_units,
                       unsigned long style)
{
    osbanid_t gparent = parent;
    osbanid_t gbanner;
    glui32 gwinmeth = 0;
    glui32 gwinsize = siz;
    glui32 gwintype = 0;
    glui32 gstatus = (style & OS_BANNER_STYLE_TAB_ALIGN);
    glui32 gscroll = (style & (OS_BANNER_STYLE_VSCROLL |
                               OS_BANNER_STYLE_AUTO_VSCROLL |
                               OS_BANNER_STYLE_MOREMODE));
    glui32 grepad = 0;

    if (gparent && !(gparent->valid))
        return 0;

    if (!os_banners)
    {
        os_banners = os_banner_init();
        if (!os_banners)
            return 0;
        os_banners->win = mainwin;
    }

    if (!gparent)
        gparent = os_banners;

    switch (wintype)
    {
        case OS_BANNER_TYPE_TEXT:
            /*
             *   A tab-aligned banner is laid out in fixed-width columns, so
             *   unless it needs to scroll, show it in a grid window, which
             *   measures and styles it as one. Only one fitted to its
             *   contents, like a status line or a menu header: one given a
             *   size may hold prose and pictures (mazes.t3), which a grid
             *   would break mid-word and can't show.
             */
            if (gstatus && !gscroll && siz == 0)
            {
                gwintype = wintype_TextGrid;
                grepad = 1;
            }
            else
            {
                gwintype = wintype_TextBuffer;
            }
            break;
        case OS_BANNER_TYPE_TEXTGRID: gwintype = wintype_TextGrid; break;
        default: gwintype = wintype_TextGrid; break;
    }

    switch (align)
    {
        case OS_BANNER_ALIGN_TOP: gwinmeth = winmethod_Above; break;
        case OS_BANNER_ALIGN_BOTTOM: gwinmeth = winmethod_Below; break;
        case OS_BANNER_ALIGN_LEFT: gwinmeth = winmethod_Left; break;
        case OS_BANNER_ALIGN_RIGHT: gwinmeth = winmethod_Right; break;
        default: gwinmeth = winmethod_Above; break;
    }

    switch (siz_units)
    {
        case OS_BANNER_SIZE_PCT: gwinmeth |= winmethod_Proportional; break;
        case OS_BANNER_SIZE_ABS: gwinmeth |= winmethod_Fixed; break;
        default: gwinmeth |= winmethod_Fixed; break;
    }

    gbanner = os_banner_insert(gparent, where, other, gwinmeth, gwinsize,
                               gwintype, gstatus, grepad);

    if (gbanner)
    {
        if (banner_side_text(wintype, align))
        {
            gbanner->type = wintype_Graphics;
            gbanner->size = banner_gfx_size(gbanner, &gbanner->method);
        }
    }


    os_banners_redraw();

    return gbanner;
}

void os_banner_set_size(void *banner_handle, int siz, int siz_units,
                        int is_advisory)
{
    osbanid_t banner = banner_handle;

    if (!banner || !banner->valid)
        return;
#ifdef OSGLK_PIXELS
    /*
     *   the game will fit a text banner to its contents after, which we
     *   can, so the size until then would only make it jump
     */
    if (is_advisory && (banner->type != wintype_TextGrid || banner->repad))
        return;
#endif /* OSGLK_PIXELS */

    glui32 gwinsize = siz;
    glui32 gwinmeth = 0;

    gwinmeth = banner->method &
        (winmethod_Above | winmethod_Below |
         winmethod_Left  | winmethod_Right);

    switch (siz_units)
    {
        case OS_BANNER_SIZE_PCT: gwinmeth |= winmethod_Proportional; break;
        case OS_BANNER_SIZE_ABS: gwinmeth |= winmethod_Fixed; break;
        default: gwinmeth |= winmethod_Fixed; break;
    }

    banner->asked_method = gwinmeth;
    banner->asked_size = gwinsize;
    if (banner->type == wintype_Graphics)
    {
        banner_gfx_fit(banner);
        return;
    }

    /* a size the game gives is in lines */
    if (banner->textpx)
    {
        banner->textpx = 0;
        if (banner->win)
            osglk_set_text_height(banner->win, 0);
    }

    /* fitting a banner to the same contents again changes nothing */
    if (gwinmeth == banner->method && gwinsize == banner->size)
        return;

    banner->method = gwinmeth;
    banner->size = gwinsize;

    os_banners_redraw();
}

/* a graphics banner is as wide as its picture, and empty without one */
static void banner_gfx_size_to_contents(osbanid_t banner)
{
    if (!banner_gfx_image(banner))
        os_banner_set_size(banner, 0, OS_BANNER_SIZE_ABS, 0);
    else
    {
        /* any size but none: the picture's width is the one it gets */
        banner->asked_method = (banner->asked_method & winmethod_DirMask)
                               | winmethod_Fixed;
        banner->asked_size = 1;
        banner_gfx_fit(banner);
    }
}

#ifdef OSGLK_PIXELS
/*
 *   the widest line of a banner's contents in characters, as
 *   banner_contents_display writes them, with their pictures added to fit
 */
static int banner_contents_columns(osbanid_t banner, picture_fit *fit)
{
    int line = 0, col = 0, maxcol = 0, nl = 0;

    picture_fit_init(fit);
    for (contentid_t c = banner->contents; c; c = c->next)
    {
        for (glui32 i = 0; i < c->len; ++i)
        {
            unsigned char ch = (unsigned char)c->chars[i];

            /* a newline waits for more, so the last one is never shown */
            if (ch == '\n')
            {
                ++nl;
                continue;
            }
            if (nl)
            {
                line += nl;
                col = nl = 0;
            }
            /* a character, but not the rest of one in UTF-8 */
            fit->last = 0;
            if ((ch & 0xC0) != 0x80 && ++col > maxcol)
                maxcol = col;
        }
        if (c->has_image)
        {
            if (nl)
            {
                line += nl;
                col = nl = 0;
            }
            col += picture_fit_add(fit, banner->win, &c->image, line);
            if (col > maxcol)
                maxcol = col;
        }
    }
    return maxcol;
}

/*
 *   fit a text buffer banner to its contents; FALSE when it is at the side
 *   and has a picture we can't measure, which leaves the width as it is
 */
static int banner_text_size_to_contents(osbanid_t banner)
{
    picture_fit fit;
    glui32 dir = banner->method & winmethod_DirMask;
    int side = (dir == winmethod_Left || dir == winmethod_Right);
    int maxcol = banner_contents_columns(banner, &fit);
    int size;

    /* at the side, as wide as the widest line, as for a tag banner */
    if (side)
    {
        if (fit.unknown)
            return FALSE;
        size = maxcol ? maxcol + 2 : 0;
    }
    else
        size = picture_fit_lines(&fit, osglk_px_lines(banner->win));
    os_banner_set_size(banner, size, OS_BANNER_SIZE_ABS, 0);

    /* the text's own height, unless a picture in the margin is taller */
    if (!side && size > 0 && !fit.unknown && !fit.bottom && banner->win)
    {
        banner->textpx = osglk_px_text_height(banner->win);
        osglk_set_text_height(banner->win, banner->textpx);
    }
    return TRUE;
}
#endif /* OSGLK_PIXELS */

void os_banner_size_to_contents(void *banner_handle)
{
    osbanid_t banner = banner_handle;

    if (!banner || !banner->valid || !banner->win)
        return;

    if (banner->type == wintype_Graphics)
    {
        banner_gfx_size_to_contents(banner);
        return;
    }
#ifdef OSGLK_PIXELS
    if (banner->type == wintype_TextBuffer
        && !banner_text_size_to_contents(banner))
        return;
#endif /* OSGLK_PIXELS */
    if (banner->repad)
    {
        glui32 rows = banner_contents_rows(banner->contents);
        if (rows && !(rows == banner->size
                      && (banner->method & winmethod_Fixed)))
            os_banner_set_size(banner, rows, OS_BANNER_SIZE_ABS, 0);
    }
}

void os_banner_delete(void *banner_handle)
{
    osbanid_t banner = banner_handle;
    if (!banner || !(banner->valid))
        return;

    banner->valid = 0;
    os_banners_redraw();

    if (banner->parent && banner->parent->children == banner)
        banner->parent->children = banner->next;

    if (banner->next)
        banner->next->prev = banner->prev;

    if (banner->prev)
        banner->prev->next = banner->next;

    /*
     *   The children stay valid until the caller deletes them, but out of
     *   sight, and mustn't point back at us once we're freed
     */
    for (osbanid_t child = banner->children; child; child = child->next)
        child->parent = 0;

    banner_contents_clear(banner->contents);

    if (banner->html)
        oshtml_delete(banner->html);

    free(banner);
}

void os_banner_orphan(void *banner_handle)
{
    os_banner_delete(banner_handle);
}

int os_banner_getinfo(void *banner_handle, os_banner_info_t *info)
{
    osbanid_t banner = banner_handle;
    // Return FALSE (not 1) when we have no window to measure yet: a
    // size-to-contents banner is created at size 0, so os_banners_open()
    // skips glk_window_open() and banner->win is still 0 when
    // CVmConsoleBanner's constructor calls us. Reporting success here
    // without populating *info left the caller reading an uninitialized
    // info.os_line_wrap off the stack, which flipped the banner's line-wrap
    // mode (and hence the per-character BANNERCOLS query storm)
    // nondeterministically between runs.
    // Returning FALSE makes the caller fall back to its own os_line_wrap
    // (TRUE for a text banner: see CVmConsoleBanner in vmconsol.cpp).
    if (!banner || !banner->valid || !banner->win)
        return FALSE;

//    winid_t win = banner->win;
    glui32 gwintype = banner->type;
    glui32 gwinmeth = banner->method;
    glui32 gstyletab = banner->status;

    if (gwinmeth & winmethod_Above)
        info->align = OS_BANNER_ALIGN_TOP;
    if (gwinmeth & winmethod_Below)
        info->align = OS_BANNER_ALIGN_BOTTOM;
    if (gwinmeth & winmethod_Left)
        info->align = OS_BANNER_ALIGN_LEFT;
    if (gwinmeth & winmethod_Right)
        info->align = OS_BANNER_ALIGN_RIGHT;

    info->style = gstyletab ? OS_BANNER_STYLE_TAB_ALIGN : 0;

    glk_window_get_size(banner->win, &(banner->cwidth), &(banner->cheight));

#ifdef SPATTERLIGHT
    if (banner->type == wintype_TextBuffer)
    {
        banner->cwidth = win_cols(banner->win->peer);
    }
#endif /* SPATTERLIGHT */
    banner_gfx_cells(banner);

    info->rows = banner->cheight;
    info->columns = banner->cwidth;

    info->pix_width = 0;
    info->pix_height = 0;

    /*
     *   in HTML mode, oshtml.c does the formatter's work: tags take no room;
     *   a text banner shown as a grid is still text to the VM
     */
    info->os_line_wrap = (gwintype != wintype_TextGrid || banner->repad);

    return 1;
}

int os_banner_get_charwidth(void *banner_handle)
{
    osbanid_t banner = banner_handle;
    if (!banner || !banner->valid || !banner->win)
        return 0;

    glk_window_get_size(banner->win, &(banner->cwidth), &(banner->cheight));

#ifdef SPATTERLIGHT
    if (banner->type == wintype_TextBuffer)
    {
        banner->cwidth = win_cols(banner->win->peer);
    }
#endif /* SPATTERLIGHT */
    banner_gfx_cells(banner);
    
    return banner->cwidth;
}

int os_banner_get_charheight(void *banner_handle)
{
    osbanid_t banner = banner_handle;
    if (!banner || !banner->valid || !banner->win)
        return 0;

    glk_window_get_size(banner->win, &(banner->cwidth), &(banner->cheight));
    banner_gfx_cells(banner);

    return banner->cheight;
}

void os_banner_clear(void *banner_handle)
{
    osbanid_t banner = banner_handle;
    if (!banner || !banner->valid)
        return;

    if (banner->win && banner->type == wintype_Graphics)
        glk_window_clear(banner->win);
    else if (banner->win)
    {
        /* a cleared window takes the colours of the text, as in Z-code */
        banner_default_zcolors(banner);
        glk_window_clear(banner->win);
    }

    banner_contents_clear(banner->contents);
    banner->contents = 0;
    if (banner->html)
        oshtml_clear(banner->html);
    banner->link = 0;
    banner->newline = 0;
    banner->move = 0;
    banner->x = 0;
    banner->y = 0;
}

static void banner_disp(osbanid_t banner, const char *txt, size_t len);

void os_banner_disp(void *banner_handle, const char *txt, size_t len)
{
    osbanid_t banner = banner_handle;
    if (!banner || !banner->valid || !banner->win)
        return;

    if (banner->html)
        oshtml_write(banner->html, txt, len);
    else
        banner_disp(banner, txt, len);
}

/* the contents so far, with a new chunk after them */
static contentid_t banner_contents_add(osbanid_t banner)
{
    contentid_t update = banner_contents_init();
    if (!update)
        return 0;
    update->banner = banner;

    if (!(banner->contents))
    {
        banner->contents = update;
    }
    else
    {
        contentid_t contents = banner->contents;
        while (contents->next)
            contents = contents->next;
        contents->next = update;
    }

    update->style = banner->style;
    update->fg = banner->fg;
    update->bg = banner->bg;
    update->link = banner->link;
    update->newline = banner->newline;
    update->move = banner->move;
    update->x = banner->x;
    update->y = banner->y;
    return update;
}

static void banner_disp(osbanid_t banner, const char *txt, size_t len)
{
    contentid_t update;
    int blank = banner->type == wintype_Graphics && banner_blank(txt, len);

    if (banner->type == wintype_Graphics && !blank)
        banner_to_text(banner);
    update = banner_contents_add(banner);
    if (!update)
        return;

    if (banner->repad)
    {
        glk_window_get_size(banner->win, &(banner->cwidth), &(banner->cheight));
        update->width = banner->cwidth;
    }

    banner_contents_insert(update, txt, len);
    if (!blank)
        banner_contents_display(update);
}

void os_banner_goto(void *banner_handle, int row, int col)
{
    osbanid_t banner = banner_handle;
    if (!banner || !banner->valid || !banner->win)
        return;

    if (banner->type == wintype_TextGrid)
    {
        banner->move = 1;
        banner->x = col;
        banner->y = row;
    }
}

void os_banner_set_attr(void *banner_handle, int attr)
{
    osbanid_t banner = banner_handle;
    if (!banner || !banner->valid)
        return;

    if (banner->html)
        oshtml_set_attr(banner->html, attr);
    else
        banner->style = osstyles_style(
            ((attr & OS_ATTR_BOLD) ? OSSTY_BOLD : 0)
            | ((attr & OS_ATTR_ITALIC) ? OSSTY_ITALIC : 0));
}

/* the text from now on; each piece of the contents keeps its own */
static void banner_set_color(osbanid_t banner, os_color_t fg, os_color_t bg)
{
    banner->fg = fg;
    banner->bg = bg;
}

void os_banner_set_color(void *banner_handle, os_color_t fg, os_color_t bg)
{
    osbanid_t banner = banner_handle;
    if (!banner || !banner->valid)
        return;

    if (banner->html)
        oshtml_set_color(banner->html, fg, bg);
    else
        banner_set_color(banner, fg, bg);
}

void os_banner_set_screen_color(void *banner_handle, os_color_t color)
{
    osbanid_t banner = banner_handle;
    glui32 bgscreen;

    if (!banner || !banner->valid)
        return;

    /* a parameter colour is the theme's own, as near as we come to it */
    bgscreen = !os_color_is_param(color);
    if (bgscreen == banner->bgscreen
        && (!bgscreen || banner->bgcolor == (glui32)color))
        return;

    banner->bgscreen = bgscreen;
    if (bgscreen)
        banner->bgcolor = (glui32)color;
    os_banner_styles_apply(banner);

    /* the text with the screen's colour behind it takes the new one */
    if (banner->win)
    {
        banner_default_zcolors(banner);
        glk_window_clear(banner->win);
        banner_set_background(banner);
        banner_contents_display(banner->contents);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   HTML mode: a text-buffer banner's formatter writes HTML markup, which
 *   goes through a parser of the banner's own (oshtml.c)
 */

static void html_put(void *ctx, const glui32 *buf, size_t len)
{
    osbanid_t banner = ctx;
    unsigned char *out;
    glui32 n;

    if (len == 0 || !banner->valid || !banner->win)
        return;
    /* back to the game's character set, as os_banner_disp takes it */
    out = malloc(len * 4 + 1);
    if (!out)
        return;
    n = os_prepare_chars((glui32 *)buf, (glui32)len, out, (glui32)len * 4);
    banner_disp(banner, (char *)out, n);
    free(out);
}

static void html_set_attr(void *ctx, int combo)
{
    osbanid_t banner = ctx;
    banner->style = osstyles_style(combo);
}

static void html_set_color(void *ctx, os_color_t fg, os_color_t bg)
{
    banner_set_color(ctx, fg, bg);
}

static void html_set_screen_color(void *ctx, os_color_t color)
{
    os_banner_set_screen_color(ctx, color);
}

static int html_width(void *ctx)
{
    return os_banner_get_charwidth(ctx);
}

/*
 *   Glk's width of a window counts characters as wide as its cells, but a
 *   box-drawing character is an em wide, the font's size, and in a text
 *   buffer that can be more than a cell: Helvetica 13's "W" is 12.3 wide.
 *   So in Spatterlight no more of them than ems fit in the window's pixels.
 *   (A font without the character may draw it narrower, from another.)
 */
int osglk_rule_width(winid_t win, int combo)
{
    glui32 wid = 0;
#ifdef SPATTERLIGHT
    glui32 em = 0;
    int pixels = win->bbox.x1 - win->bbox.x0 - 2 * gbuffermarginx;
#endif /* SPATTERLIGHT */

    glk_window_get_size(win, &wid, NULL);
#ifdef SPATTERLIGHT
    if (glk_window_get_type(win) == wintype_TextBuffer
        && glk_style_measure(win, osstyles_style(combo), stylehint_Size, &em)
        && em > 0 && pixels / (int)em < (int)wid)
        wid = pixels > 0 ? (glui32)(pixels / (int)em) : 0;
#endif /* SPATTERLIGHT */
    return (int)wid;
}

int osglk_centers(winid_t win, int combo)
{
    glui32 just;

    return win
        && glk_style_measure(win, osstyles_style(combo),
                             stylehint_Justification, &just)
        && just == stylehint_just_Centered;
}

static int html_rule_width(void *ctx, int combo)
{
    osbanid_t banner = ctx;

    if (!banner || !banner->valid || !banner->win)
        return 0;
    return osglk_rule_width(banner->win, combo);
}

static int html_centers(void *ctx, int combo)
{
    osbanid_t banner = ctx;
    return osglk_centers(banner->win, combo);
}

/* a link goes with the text, which keeps it when the banner is redrawn */
static void html_set_link(void *ctx, glui32 link)
{
    osbanid_t banner = ctx;
    banner->link = link;
}

/*
 *   A banner's contents are kept, and written again when it's redrawn, so
 *   it has no window to draw in directly: its pictures are kept with the
 *   text (html_image).  Nor does it play sounds, as in HTML TADS.
 */
static winid_t html_window(void *ctx)
{
    (void)ctx;
    return NULL;
}

static int html_image(void *ctx, const oshtml_image *img)
{
    osbanid_t banner = ctx;
    contentid_t update;
    glui32 w, h;

    if (!banner->valid || !banner->win)
        return 0;
    if (!img)
        return banner->type != wintype_TextGrid;

    /*
     *   a graphics banner shows one picture of a size of its own, which no
     *   link needs a click on, resized to it before it's drawn
     */
    if (banner->type == wintype_Graphics)
    {
        if (banner->link || banner_gfx_image(banner)
            || !banner_gfx_size_of(img, &w, &h))
            banner_to_text(banner);
        else
        {
            update = banner_contents_add(banner);
            if (!update)
                return 0;
            update->has_image = 1;
            update->image = *img;
            if (!banner_gfx_fit(banner))
                banner_gfx_draw(banner);
            return 1;
        }
        if (!banner->win)
            return 0;
    }

    if (banner->type != wintype_TextBuffer)
        return 0;
    update = banner_contents_add(banner);
    if (!update)
        return 0;
    update->has_image = 1;
    update->image = *img;
    banner_contents_display(update);
    return 1;
}

static const oshtml_sink html_sink =
{
    html_put, html_set_attr, html_set_color, html_set_screen_color,
    html_width, html_centers, html_window, html_set_link, html_image,
    html_rule_width
};

/* a request is used up by its click: osglk.c asks again before each wait */
static void banners_request_links(osbanid_t banner)
{
    for ( ; banner ; banner = banner->next)
    {
        if (banner->valid && banner->html && banner->win
            && banner->win != mainwin && banner->type != wintype_Graphics)
            glk_request_hyperlink_event(banner->win);
        banners_request_links(banner->children);
    }
}

void osglk_api_banner_request_links(void)
{
    if (os_banners)
        banners_request_links(os_banners->children);
}

static void banners_gfx_redraw(osbanid_t banner, int resized)
{
    glui32 w, h;

    for ( ; banner ; banner = banner->next)
    {
        if (banner->valid && banner->win && banner->type == wintype_Graphics)
        {
            glk_window_get_size(banner->win, &w, &h);
            if (!resized || w != banner->drawn_w || h != banner->drawn_h)
                banner_gfx_draw(banner);
        }
        banners_gfx_redraw(banner->children, resized);
    }
}

void osglk_api_banner_redraw(void)
{
    if (os_banners)
        banners_gfx_redraw(os_banners->children, 0);
}

void osglk_api_banner_resized(void)
{
    if (os_banners)
        banners_gfx_redraw(os_banners->children, 1);
}

void os_banner_flush(void *banner_handle)
{
    osbanid_t banner = banner_handle;
    if (banner && banner->valid && banner->html)
        oshtml_flush_parser(banner->html);
}

void os_banner_start_html(void *banner_handle)
{
    osbanid_t banner = banner_handle;
    if (!banner || !banner->valid || banner->html || !osglk_html_target())
        return;
    banner->html = oshtml_new(&html_sink, banner);
}

void os_banner_end_html(void *banner_handle)
{
    osbanid_t banner = banner_handle;
    if (!banner || !banner->valid || !banner->html)
        return;
    oshtml_flush_parser(banner->html);
    oshtml_delete(banner->html);
    banner->html = 0;
}
