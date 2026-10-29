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
#include "osstyles.h"

extern glui32 os_prepare_chars(glui32 *buf, glui32 buflen,
                               unsigned char *out, glui32 outlen);

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
    os_color_t fg, bg;              /* active text colours, as the game set them */
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

extern winid_t mainwin;
extern winid_t statuswin;

extern glui32 mainfg;
extern glui32 mainbg;

extern glui32 statusfg;
extern glui32 statusbg;

#ifdef OSGLK_PIXELS
/*
 *   What the pictures in a text-buffer banner add to the size it fits its
 *   contents at.  Spatterlight counts a line with a picture in it as one
 *   line, however tall the picture, and a picture in the margin not at
 *   all.  Gargoyle's count has the lines its pictures take up, as it puts
 *   them on lines of their own, but not those beside a margin picture.
 */
typedef struct picture_fit_s
{
    int extra;                      /* lines the pictures make taller */
    int row, row_extra;             /* the last picture's line, its extra */
    int bottom;                     /* the line under the margin pictures */
    int unknown;                    /* a picture whose size isn't known */
    int last;                       /* nothing after the last picture */
    int count;                      /* the pictures */
}
picture_fit;

static void picture_fit_init(picture_fit *fit)
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
static int picture_fit_add(picture_fit *fit, winid_t win,
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
static int picture_fit_lines(const picture_fit *fit, int lines)
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
void os_banners_redraw(void);
void banner_contents_repad(osbanid_t banner);

extern glui32 os_parse_chars(unsigned char *buf, glui32 buflen,
                             glui32 *out, glui32 outlen);

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
                           glui32 method, glui32 size, glui32 type, glui32 status,
                           glui32 repad)
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
static void grid_hints_set(void)
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

static void grid_hints_clear(void)
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
            banner->win = glk_window_open(banner->parent->win, banner->method,
                                          banner->size, banner->type, banner->id);
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

    gbanner = os_banner_insert(gparent, where, other, gwinmeth, gwinsize, gwintype, gstatus,
                               grepad);

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

void os_banner_set_size(void *banner_handle, int siz, int siz_units, int is_advisory)
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

void os_banner_size_to_contents(void *banner_handle)
{
    osbanid_t banner = banner_handle;

    if (!banner || !banner->valid || !banner->win)
        return;

    /* a graphics banner is as wide as its picture, and empty without one */
    if (banner->type == wintype_Graphics)
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
        return;
    }
#ifdef OSGLK_PIXELS
    if (banner->type == wintype_TextBuffer)
    {
        picture_fit fit;
        int line = 0, col = 0, maxcol = 0, nl = 0, size;
        glui32 dir = banner->method & winmethod_DirMask;

        /* the lines of the contents, as banner_contents_display writes them */
        picture_fit_init(&fit);
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
                fit.last = 0;
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
                col += picture_fit_add(&fit, banner->win, &c->image, line);
                if (col > maxcol)
                    maxcol = col;
            }
        }

        /*
         *   at the side, as wide as the widest line, as for a tag banner;
         *   a picture we can't measure leaves the width as it is
         */
        if (dir == winmethod_Left || dir == winmethod_Right)
        {
            if (fit.unknown)
                return;
            size = maxcol ? maxcol + 2 : 0;
        }
        else
            size = picture_fit_lines(&fit, osglk_px_lines(banner->win));
        os_banner_set_size(banner, size, OS_BANNER_SIZE_ABS, 0);

        /* the text's own height, unless a picture in the margin is taller */
        if (dir != winmethod_Left && dir != winmethod_Right && size > 0
            && !fit.unknown && !fit.bottom && banner->win)
        {
            banner->textpx = osglk_px_text_height(banner->win);
            osglk_set_text_height(banner->win, banner->textpx);
        }
    }
#endif /* OSGLK_PIXELS */
    if (banner->repad)
    {
        glui32 rows = banner_contents_rows(banner->contents);
        if (rows && !(rows == banner->size && (banner->method & winmethod_Fixed)))
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
    // CVmConsoleBanner's constructor calls us. Reporting success here without
    // populating *info left the caller reading an uninitialized info.os_line_wrap
    // off the stack, which flipped the banner's line-wrap mode (and hence the
    // per-character BANNERCOLS query storm) nondeterministically between runs.
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

static void banner_set_attr(osbanid_t banner, int attr);
static void banner_set_color(osbanid_t banner, os_color_t fg, os_color_t bg);

void os_banner_set_attr(void *banner_handle, int attr)
{
    osbanid_t banner = banner_handle;
    if (!banner || !banner->valid)
        return;

    if (banner->html)
        oshtml_set_attr(banner->html, attr);
    else
        banner_set_attr(banner, attr);
}

static void banner_set_attr(osbanid_t banner, int attr)
{
    banner->style = osstyles_style(((attr & OS_ATTR_BOLD) ? OSSTY_BOLD : 0)
                                   | ((attr & OS_ATTR_ITALIC) ? OSSTY_ITALIC : 0));
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

/* the text from now on; each piece of the contents keeps its own */
static void banner_set_color(osbanid_t banner, os_color_t fg, os_color_t bg)
{
    banner->fg = fg;
    banner->bg = bg;
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
    glui32 just;

    return banner->win
        && glk_style_measure(banner->win, osstyles_style(combo),
                             stylehint_Justification, &just)
        && just == stylehint_just_Centered;
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
    if (!banner || !banner->valid || banner->html)
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

/* a grid has a column of margin on each side */
static int tag_margin(tagban_t *tb)
{
    return tb->type == wintype_TextGrid ? 1 : 0;
}

static int tag_width(void *ctx)
{
    tagban_t *tb = ctx;
    glui32 wid = 0;

    if (!tb->win)
        return 0;

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

static void tag_put(void *ctx, const glui32 *buf, size_t len)
{
    tagban_t *tb = ctx;
    strid_t str = tb->win ? glk_window_get_stream(tb->win) : NULL;
    int wid = tb->type == wintype_TextGrid ? tag_width(tb) : 0;
    size_t i;

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
    if (tb->win)
        glk_set_style_stream(glk_window_get_stream(tb->win),
                             osstyles_style(combo));
}

/* the parameter colours are the window's own, from the theme */
static glui32 tag_color(os_color_t color)
{
    return os_color_is_param(color) ? zcolor_Default : (glui32)color;
}

static void tag_set_color(void *ctx, os_color_t fg, os_color_t bg)
{
    tagban_t *tb = ctx;
    if (tb->win)
        osglk_zcolors_stream(glk_window_get_stream(tb->win),
                                  tag_color(fg), tag_color(bg));
}

static void tag_set_screen_color(void *ctx, os_color_t color)
{
    tagban_t *tb = ctx;
    if (!tb->win || tb->measure)
        return;
    osglk_set_window_bg(tb->win, tag_color(color));
#ifdef OSGLK_PIXELS
    tb->screen = tag_color(color);
#endif /* OSGLK_PIXELS */
    tb->bg = !os_color_is_param(color);
}

static int tag_centers(void *ctx, int combo)
{
    tagban_t *tb = ctx;
    glui32 just;

    return tb->win
        && glk_style_measure(tb->win, osstyles_style(combo),
                             stylehint_Justification, &just)
        && just == stylehint_just_Centered;
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
            tag_put_newlines(tb, glk_window_get_stream(tb->win));
        glk_set_hyperlink_stream(glk_window_get_stream(tb->win), link);
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
static void tag_close_table(tagban_t *tb)
{
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
    tb->tbl_x = !t->center ? mx
        : ww > t->width ? (ww - t->width) / 2 : 0;
    tb->tbl_y = t->page ? my
        : t->above && wh > t->height ? wh - t->height : 0;

    /* the background, tiled from the top left as in HTML TADS */
    if (t->page && t->bg.id)
    {
        glui32 bw = 0, bh = 0, x, y;

        if (glk_image_get_info(t->bg.id, &bw, &bh) && bw && bh)
            for (y = 0 ; y < wh ; y += bh)
                for (x = 0 ; x < ww ; x += bw)
                    glk_image_draw(tb->tblwin, t->bg.id, (glsi32)x, (glsi32)y);
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
            tag_table_text(tb, 0);
            tag_table_place(tb);
            tag_table_draw(tb);
        }
    }
#endif /* OSGLK_PIXELS */
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
        osglk_set_text_height(tb->win,
                              tb->fit && tb->size && !tb->pictures.unknown
                              && !tb->pictures.bottom
                              ? osglk_px_text_height(tb->win) : 0);
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
            tag_table_text(tb, 1);
            tag_table_place(tb);
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
