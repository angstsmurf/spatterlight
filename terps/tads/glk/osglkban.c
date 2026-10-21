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

#include "os.h"
#include "glk.h"

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
    glui32 repad;                   /* text banner shown as a grid */

    glui32 cheight;                 /* glk char height */
    glui32 cwidth;                  /* glk char width */

    glui32 fgcolor;                 /* foreground color */
    glui32 bgcolor;                 /* background color */
    glui32 fgcustom;                /* custom colors */
    glui32 bgcustom;
    glui32 bgtrans;

    contentid_t contents;           /* window contents */
    glui32 style;                   /* active Glk style value */
    glui32 newline;                 /* active newline */
    glui32 move, x, y;              /* active cursor position */

    winid_t win;                    /* glk window object */
}
os_banner_t;

/* for reprinting banner contents */
typedef struct banner_contents_s
{
    osbanid_t banner;               /* content owner */
    contentid_t next;               /* next set of contents */

    glui32 style;                   /* stored contents style */
    glui32 newline;                 /* stored newline */
    glui32 move, x, y;              /* stored cursor position */
    glui32 width;                   /* banner width when written */

    char *chars;
    glui32 len;

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

void banner_contents_display(contentid_t contents);
void banner_contents_repad(osbanid_t banner);

extern glui32 os_parse_chars(unsigned char *buf, glui32 buflen,
                             glui32 *out, glui32 outlen);

/* Implementation-specific functions for managing banner windows */
/*
    os_banner_init();
    os_banner_insert();
    os_banner_styles_apply();
    os_banner_styles_reset();
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
    instance->repad = 0;

    instance->cheight = 0;
    instance->cwidth = 0;

    instance->contents = 0;
    instance->style = style_Normal;
    instance->newline = 0;
    instance->move = 0;
    instance->x = 0;
    instance->y = 0;

    instance->win = 0;

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
    baby->type = type;
    baby->status = status;
    baby->repad = repad;

    return baby;
}

void os_banner_styles_apply (osbanid_t banner)
{
    if (!banner || !(banner->valid))
        return;

    glui32 propval = banner->status ? 0 : (banner->type == wintype_TextGrid ? 0 : 1);
    glui32 bgcustom = banner->bgtrans ? banner->bgcolor : banner->bgcustom;

    banner->fgcolor = gsbgcol;
    banner->bgcolor = gsfgcol;

    /* font style: monospace for text grid and tab aligned buffers, else proportional */
    glk_stylehint_set(banner->type, style_Alert, stylehint_Proportional, propval);
    glk_stylehint_set(banner->type, style_Subheader, stylehint_Proportional, propval);
    glk_stylehint_set(banner->type, style_Emphasized, stylehint_Proportional, propval);
    glk_stylehint_set(banner->type, style_Normal, stylehint_Proportional, propval);
    glk_stylehint_set(banner->type, style_User1, stylehint_Proportional, propval);
    glk_stylehint_set(banner->type, style_User2, stylehint_Proportional, propval);
    glk_stylehint_set(banner->type, style_BlockQuote, stylehint_Proportional, propval);

    /* foreground color: user1 reverse, user2 custom, blockquote same as background, i.e. invisible */
    glk_stylehint_set(banner->type, style_Alert, stylehint_TextColor, banner->fgcolor);
    glk_stylehint_set(banner->type, style_Subheader, stylehint_TextColor, banner->fgcolor);
    glk_stylehint_set(banner->type, style_Emphasized, stylehint_TextColor, banner->fgcolor);
    glk_stylehint_set(banner->type, style_Normal, stylehint_TextColor, banner->fgcolor);
    glk_stylehint_set(banner->type, style_User1, stylehint_TextColor, banner->bgcolor);
    glk_stylehint_set(banner->type, style_User2, stylehint_TextColor, banner->fgcustom);
    glk_stylehint_set(banner->type, style_BlockQuote, stylehint_TextColor, banner->bgcolor);

    /* background color: user1 reverse, user2 custom, blockquote same as background, i.e. invisible */
    glk_stylehint_set(banner->type, style_Alert, stylehint_BackColor, banner->bgcolor);
    glk_stylehint_set(banner->type, style_Subheader, stylehint_BackColor, banner->bgcolor);
    glk_stylehint_set(banner->type, style_Emphasized, stylehint_BackColor, banner->bgcolor);
    glk_stylehint_set(banner->type, style_Normal, stylehint_BackColor, banner->bgcolor);
    glk_stylehint_set(banner->type, style_User1, stylehint_BackColor, banner->fgcolor);
    glk_stylehint_set(banner->type, style_User2, stylehint_BackColor, bgcustom);
    glk_stylehint_set(banner->type, style_BlockQuote, stylehint_BackColor, banner->bgcolor);
}

void os_banner_styles_reset (void)
{
    glk_stylehint_clear(wintype_AllTypes, style_Alert, stylehint_Proportional);
    glk_stylehint_clear(wintype_AllTypes, style_Subheader, stylehint_Proportional);
    glk_stylehint_clear(wintype_AllTypes, style_Emphasized, stylehint_Proportional);
    glk_stylehint_clear(wintype_AllTypes, style_Normal, stylehint_Proportional);
    glk_stylehint_clear(wintype_AllTypes, style_User1, stylehint_Proportional);
    glk_stylehint_clear(wintype_AllTypes, style_User2, stylehint_Proportional);
    glk_stylehint_clear(wintype_AllTypes, style_BlockQuote, stylehint_Proportional);

    glk_stylehint_clear(wintype_AllTypes, style_Alert, stylehint_TextColor);
    glk_stylehint_clear(wintype_AllTypes, style_Subheader, stylehint_TextColor);
    glk_stylehint_clear(wintype_AllTypes, style_Emphasized, stylehint_TextColor);
    glk_stylehint_clear(wintype_AllTypes, style_Normal, stylehint_TextColor);
    glk_stylehint_clear(wintype_AllTypes, style_User1, stylehint_TextColor);
    glk_stylehint_clear(wintype_AllTypes, style_User2, stylehint_TextColor);
    glk_stylehint_clear(wintype_AllTypes, style_BlockQuote, stylehint_TextColor);

    glk_stylehint_clear(wintype_AllTypes, style_Alert, stylehint_BackColor);
    glk_stylehint_clear(wintype_AllTypes, style_Subheader, stylehint_BackColor);
    glk_stylehint_clear(wintype_AllTypes, style_Emphasized, stylehint_BackColor);
    glk_stylehint_clear(wintype_AllTypes, style_Normal, stylehint_BackColor);
    glk_stylehint_clear(wintype_AllTypes, style_User1, stylehint_BackColor);
    glk_stylehint_clear(wintype_AllTypes, style_User2, stylehint_BackColor);
    glk_stylehint_clear(wintype_AllTypes, style_BlockQuote, stylehint_BackColor);

#ifdef SPATTERLIGHT
    /* reset our default colors with a superfluous hint */
    glk_stylehint_set(wintype_AllTypes, style_Normal, stylehint_TextColor, mainfg);
    glk_stylehint_set(wintype_AllTypes, style_Normal, stylehint_BackColor, mainbg);
#endif /* SPATTERLIGHT */
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
        if (banner->size && banner->parent && banner->parent->win)
        {
            os_banner_styles_apply(banner);
            banner->win = glk_window_open(banner->parent->win, banner->method,
                                          banner->size, banner->type, banner->id);
            banner_contents_repad(banner);
            banner_contents_display(banner->contents);
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
    os_banner_styles_reset();
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
    instance->newline = 0;
    instance->move = 0;
    instance->x = 0;
    instance->y = 0;
    instance->width = 0;

    instance->chars = 0;
    instance->len = 0;

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
    os_put_buffer((unsigned char *)chars, len);
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
             *   The VM lays out a tab-aligned banner in fixed-width columns
             *   itself, so unless it needs to scroll, show it in a grid
             *   window, which measures and styles it as one.
             */
            if (gstatus && !gscroll)
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
        gbanner->fgcolor = gstatus ? statusbg : mainfg;
        gbanner->bgcolor = gstatus ? statusfg : mainbg;
        gbanner->fgcustom = gbanner->fgcolor;
        gbanner->bgcustom = gbanner->bgcolor;
        gbanner->bgtrans = 1;
    }

    os_banners_redraw();

    return gbanner;
}

void os_banner_set_size(void *banner_handle, int siz, int siz_units, int is_advisory)
{
    osbanid_t banner = banner_handle;

    if (!banner || !banner->valid)
        return;

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

    banner->method = gwinmeth;
    banner->size = gwinsize;

    os_banners_redraw();
}

void os_banner_size_to_contents(void *banner_handle)
{
    osbanid_t banner = banner_handle;

    if (!banner || !banner->valid || !banner->win)
        return;
#ifdef SPATTERLIGHT
    if (banner->type == wintype_TextBuffer)
    {
        int size = win_lines(banner->win->peer);
        os_banner_set_size(banner, size, OS_BANNER_SIZE_ABS, 0);
    }
#endif /* SPATTERLIGHT */

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

    banner_contents_clear(banner->contents);

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
    // Returning FALSE makes the caller fall back to os_line_wrap = FALSE.
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

    info->rows = banner->cheight;
    info->columns = banner->cwidth;

    info->pix_width = 0;
    info->pix_height = 0;

    info->os_line_wrap = gstyletab ? 0 : (gwintype == wintype_TextBuffer);

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
    
    return banner->cwidth;
}

int os_banner_get_charheight(void *banner_handle)
{
    osbanid_t banner = banner_handle;
    if (!banner || !banner->valid || !banner->win)
        return 0;

    glk_window_get_size(banner->win, &(banner->cwidth), &(banner->cheight));

    return banner->cheight;
}

void os_banner_clear(void *banner_handle)
{
    osbanid_t banner = banner_handle;
    if (!banner || !banner->valid)
        return;

    if (banner->win)
    {
        winid_t win = banner->win;
        glk_window_clear(win);
    }

    banner_contents_clear(banner->contents);
    banner->contents = 0;
    banner->newline = 0;
    banner->move = 0;
    banner->x = 0;
    banner->y = 0;
}

void os_banner_disp(void *banner_handle, const char *txt, size_t len)
{
    osbanid_t banner = banner_handle;
    if (!banner || !banner->valid || !banner->win)
        return;

    contentid_t update = banner_contents_init();
    if (!update)
        return;
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
    update->newline = banner->newline;
    update->move = banner->move;
    update->x = banner->x;
    update->y = banner->y;

    if (banner->repad)
    {
        glk_window_get_size(banner->win, &(banner->cwidth), &(banner->cheight));
        update->width = banner->cwidth;
    }

    banner_contents_insert(update, txt, len);
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

    if (attr & OS_ATTR_BOLD && attr & OS_ATTR_ITALIC)
        banner->style = style_Alert;
    else if (attr & OS_ATTR_BOLD)
        banner->style = style_Subheader;
    else if (attr & OS_ATTR_ITALIC)
        banner->style = style_Emphasized;
    else
        banner->style = style_Normal;
}

void os_banner_set_color(void *banner_handle, os_color_t fg, os_color_t bg)
{
    osbanid_t banner = banner_handle;
    if (!banner || !banner->valid)
        return;

    glui32 reversed = 0;
    glui32 normal = 0;
    glui32 transparent = 0;
    glui32 invisible = 0;

    /* evaluate parameters */

    if (os_color_is_param(fg))
    {
        switch(fg)
        {
            case OS_COLOR_P_TEXTBG:
                if (bg == OS_COLOR_P_TRANSPARENT) {
                    invisible = 1;
                    break;
                }
            case OS_COLOR_P_STATUSBG:
                reversed = 1;
                break;

            case OS_COLOR_P_TEXT:
            case OS_COLOR_P_STATUSLINE:
            case OS_COLOR_P_INPUT:
                normal = 1;
                break;

            default:
                break;
        }
    }

    if (os_color_is_param(bg))
    {
        switch (bg)
        {
            case OS_COLOR_P_TRANSPARENT:
                transparent = 1;
                break;

            default:
                break;
        }
    }

    /* choose a style */

    if (normal && transparent)
        banner->style = style_Normal;
    else if (reversed)
        banner->style = style_User1;
    else if (invisible)
        banner->style = style_BlockQuote;
    else
        banner->style = style_User2;

    /* process our custom color */

    if (banner->style == style_User2)
    {
        /* store current settings */
        glui32 oldfg = banner->fgcustom;
        glui32 oldbg = banner->bgcustom;
        glui32 oldtr = banner->bgtrans;

        /* reset custom color parameters */
        banner->fgcustom = banner->fgcolor;
        banner->bgcustom = banner->bgcolor;
        banner->bgtrans = 1;

        if (!normal)
            banner->fgcustom = fg;

        if (!transparent)
        {
            banner->bgcustom = bg;
            banner->bgtrans = 0;
        }

        if (!(banner->fgcustom == oldfg
            && banner->bgcustom == oldbg
            && banner->bgtrans == oldtr))
            os_banners_redraw();
    }
}

void os_banner_set_screen_color(void *banner_handle, os_color_t color)
{
    osbanid_t banner = banner_handle;
    if (!banner || !banner->valid)
        return;

    if (!(os_color_is_param(color)))
        banner->bgcolor = color;

    os_banners_redraw();
}

void os_banner_flush(void *banner_handle) {}
void os_banner_start_html(void *banner_handle) {}
void os_banner_end_html(void *banner_handle) {}
