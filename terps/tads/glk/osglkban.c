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

    oshtml_parser *html;            /* HTML mode: the markup parser */
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

    char *chars;
    glui32 len;
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

    instance->cheight = 0;
    instance->cwidth = 0;

    instance->contents = 0;
    instance->style = style_Normal;
    instance->newline = 0;
    instance->move = 0;
    instance->x = 0;
    instance->y = 0;

    instance->win = 0;
    instance->html = 0;

    return instance;
}

osbanid_t os_banner_insert(osbanid_t parent, glui32 operation, osbanid_t other,
                           glui32 method, glui32 size, glui32 type, glui32 status)
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

    /* banners keep the plain styles; os_banner_styles_reset puts ours back */
    osstyles_clear_hints(banner->type);

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

    osstyles_set_hints(wintype_TextBuffer);
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

    instance->chars = 0;
    instance->len = 0;

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
    glui32 len = contents->len;

    glk_set_window(win);

    if (contents->newline)
    {
        unsigned char ch = '\n';
        os_put_buffer((unsigned char *)&ch, 1);
    }
    
    if (len && (contents->chars[len-1] == '\n'))
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
    os_put_buffer((unsigned char *)contents->chars, len);
    glk_set_window(mainwin);
    banner_contents_display(contents->next);
}

void banner_contents_clear(contentid_t contents)
{
    if (!contents)
        return;

    banner_contents_clear(contents->next);

    if (contents->chars)
        free(contents->chars);

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
        case OS_BANNER_TYPE_TEXT: gwintype = wintype_TextBuffer; break;
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

    gbanner = os_banner_insert(gparent, where, other, gwinmeth, gwinsize, gwintype, gstatus);

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

    info->rows = banner->cheight;
    info->columns = banner->cwidth;

    info->pix_width = 0;
    info->pix_height = 0;

    /* in HTML mode, oshtml.c does the formatter's work: tags take no room */
    info->os_line_wrap = (gwintype == wintype_TextBuffer);

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
    if (banner->html)
        oshtml_clear(banner->html);
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

static void banner_disp(osbanid_t banner, const char *txt, size_t len)
{
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

    if (banner->html)
        oshtml_set_color(banner->html, fg, bg);
    else
        banner_set_color(banner, fg, bg);
}

static void banner_set_color(osbanid_t banner, os_color_t fg, os_color_t bg)
{
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
    banner_set_attr(ctx, ((combo & OSSTY_BOLD) ? OS_ATTR_BOLD : 0)
                    | ((combo & OSSTY_ITALIC) ? OS_ATTR_ITALIC : 0));
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

/* banners keep their old look until they have the style table */
static int html_centers(void *ctx, int combo)
{
    (void)ctx;
    (void)combo;
    return 1;
}

/*
 *   A banner's contents are kept as text, and written again when it's
 *   redrawn, which a picture wouldn't survive: it shows its ALT text
 */
static winid_t html_window(void *ctx)
{
    (void)ctx;
    return NULL;
}

static const oshtml_sink html_sink =
{
    html_put, html_set_attr, html_set_color, html_set_screen_color,
    html_width, html_centers, html_window
};

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

    int fit;                        /* size the window to the text */
    int measure;                    /* this pass only counts the text */
    int bg;                         /* the window has a <BODY BGCOLOR> */

    /* the text so far: lines, the longest, and newlines not yet shown */
    int lines;
    int col, maxcol;
    int nl;

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
        if (tb->lines == 0)
            tb->lines = 1;
        for ( ; tb->nl > 0 ; --tb->nl)
        {
            ++tb->lines;
            tb->col = 0;
            if (str && !tb->measure)
                glk_put_char_stream_uni(str, '\n');
        }

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
        garglk_set_zcolors_stream(glk_window_get_stream(tb->win),
                                  tag_color(fg), tag_color(bg));
}

static void tag_set_screen_color(void *ctx, os_color_t color)
{
    tagban_t *tb = ctx;
    if (!tb->win || tb->measure)
        return;
#ifdef SPATTERLIGHT
    win_setbgnd(tb->win->peer, tag_color(color));
#endif /* SPATTERLIGHT */
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
static winid_t tag_window(void *ctx)
{
    tagban_t *tb = ctx;

    if (tb->type != wintype_TextBuffer || tb->measure)
        return NULL;
    return tb->win;
}

static const oshtml_sink tag_sink =
{
    tag_put, tag_set_attr, tag_set_color, tag_set_screen_color,
    tag_width, tag_centers, tag_window
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

static void tag_open_window(tagban_t *tb)
{
    int s, c;

    if (tb->type == wintype_TextGrid)
    {
        /* the grid shows bold and italic in the main window's styles */
        for (s = 0 ; s < style_NUMSTYLES ; ++s)
            if ((c = osstyles_combo(s)) > 0)
            {
                glk_stylehint_set(wintype_TextGrid, s, stylehint_Weight,
                                  (c & OSSTY_BOLD) ? 1 : 0);
                glk_stylehint_set(wintype_TextGrid, s, stylehint_Oblique,
                                  (c & OSSTY_ITALIC) ? 1 : 0);
            }
        glk_stylehint_clear(wintype_TextGrid, style_User1,
                            stylehint_ReverseColor);
    }

    tb->win = glk_window_open(mainwin, tb->method, tb->size, tb->type, 0);

    if (tb->type == wintype_TextGrid)
        for (s = 0 ; s < style_NUMSTYLES ; ++s)
            if (osstyles_combo(s) > 0)
            {
                glk_stylehint_clear(wintype_TextGrid, s, stylehint_Weight);
                glk_stylehint_clear(wintype_TextGrid, s, stylehint_Oblique);
            }
}

/* the size of a banner fitted to its text */
static glui32 tag_fitted(tagban_t *tb)
{
    if (tag_vertical(tb->method))
        return tb->maxcol ? (glui32)tb->maxcol + 2 : 0;
#ifdef SPATTERLIGHT
    if (tb->type == wintype_TextBuffer && tb->win && !tb->measure)
        return (glui32)win_lines(tb->win->peer);
#endif /* SPATTERLIGHT */
    return (glui32)tb->lines;
}

static void tag_resize(tagban_t *tb, glui32 size)
{
    if (size == tb->size || !tb->win)
        return;
    tb->size = size;
    glk_window_set_arrangement(glk_window_get_parent(tb->win),
                               tb->method, size, tb->win);
}

void *osglk_tag_banner_open(const oshtml_banner *ban,
                            const oshtml_sink **sink)
{
    tagban_t *tb = tag_find(ban->id);
    int isnew = tb == NULL || tb->win == NULL;
    glui32 method, size, type;
    int num, units, vertical;

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
    if (units == OSHTML_SIZE_PREV && !isnew
        && tag_vertical(tb->method) == vertical)
        method |= tb->method & winmethod_DivisionMask;
    else if (units == OSHTML_SIZE_PCT)
    {
        method |= winmethod_Proportional;
        size = num > 100 ? 100 : (glui32)num;
    }
    else if (units == OSHTML_SIZE_PIX)
    {
        int per = vertical ? TAG_PIX_COL : TAG_PIX_LINE;
        method |= winmethod_Fixed;
        size = num <= 0 ? 0 : num < per ? 1 : (glui32)(num / per);
    }
    else
    {
        method |= winmethod_Fixed;
        if (isnew || (tb->method & winmethod_DivisionMask) != winmethod_Fixed)
            size = 1;
        tb->fit = 1;
    }

    /* a window can't turn into another kind, or to the other axis */
    if (tb->win && (type != tb->type
                    || tag_vertical(tb->method) != vertical))
    {
        glk_window_close(tb->win, NULL);
        tb->win = NULL;
    }
    tb->type = type;

    if (!tb->win)
    {
        tb->method = method;
        tb->size = size;
        tag_open_window(tb);
        if (!tb->win)
            return NULL;

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
            glk_window_set_arrangement(glk_window_get_parent(tb->win),
                                       method, size, tb->win);
        }
        glk_window_clear(tb->win);
    }

    /* the default colours, unless the text has a <BODY> */
    if (tb->bg)
    {
#ifdef SPATTERLIGHT
        win_setbgnd(tb->win->peer, zcolor_Default);
#endif /* SPATTERLIGHT */
        tb->bg = 0;
    }
    garglk_set_zcolors_stream(glk_window_get_stream(tb->win),
                              zcolor_Default, zcolor_Default);
    glk_set_style_stream(glk_window_get_stream(tb->win), style_Normal);

    /* a grid can't keep lines below its last, so it counts them first */
    tb->measure = tb->fit && type == wintype_TextGrid;
    tb->lines = tb->col = tb->maxcol = tb->nl = 0;
    return tb;
}

int osglk_tag_banner_done(void *ctx)
{
    tagban_t *tb = ctx;

    if (tb->fit)
        tag_resize(tb, tag_fitted(tb));
    tb->lines = tb->col = tb->maxcol = tb->nl = 0;
    if (tb->measure)
    {
        /* now the text again, for real */
        tb->measure = 0;
        return 1;
    }
    return 0;
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
        if (tb->win)
            glk_window_close(tb->win, NULL);
        free(tb);
    }
}
