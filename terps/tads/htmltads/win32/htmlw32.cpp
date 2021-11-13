#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/htmlw32.cpp,v 1.4 1999/07/11 00:46:46 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlsys_w32.cpp - HTML system class implementation for Win32
Function
  
Notes
  
Modified
  09/13/97 MJRoberts  - Creation
*/

#include <windows.h>
#include <dsound.h>
#include <commctrl.h>
#include <htmlhelp.h>

#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

/* include TADS OS headers */
#include <os.h>
#include <osifcext.h>

/* include the GameInfo reader header */
#include <gameinfo.h>

#ifndef HTMLSYS_W32
#include "htmlw32.h"
#endif
#ifndef HTMLFMT_H
#include "htmlfmt.h"
#endif
#ifndef HTMLPRS_H
#include "htmlprs.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif
#ifndef TADSWIN_H
#include "tadswin.h"
#endif
#ifndef TADSFONT_H
#include "tadsfont.h"
#endif
#ifndef HTMLTAGS_H
#include "htmltags.h"
#endif
#ifndef HTMLINP_H
#include "htmlinp.h"
#endif
#ifndef HTMLDISP_H
#include "htmldisp.h"
#endif
#ifndef HTMLTXAR_H
#include "htmltxar.h"
#endif
#ifndef HTMLURL_H
#include "htmlurl.h"
#endif
#ifndef TADSSTAT_H
#include "tadsstat.h"
#endif
#ifndef HTMLRES_H
#include "htmlres.h"
#endif
#ifndef HTMLPREF_H
#include "htmlpref.h"
#endif
#ifndef HTMLRC_H
#include "htmlrc.h"
#endif
#ifndef W32FONT_H
#include "w32font.h"
#endif
#ifndef W32SND_H
#include "w32snd.h"
#endif
#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif
#ifndef TADSCAR_H
#include "tadscar.h"
#endif
#ifndef TADSOLE_H
#include "tadsole.h"
#endif
#ifndef W32MAIN_H
#include "w32main.h"
#endif
#ifndef W32FNDLG_H
#include "w32fndlg.h"
#endif
#ifndef TADSREG_H
#include "tadsreg.h"
#endif
#ifndef HTMLHASH_H
#include "htmlhash.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   a few random constants 
 */

/* flag: allow clicking on links during MORE mode */
#define HTMLW32_MORE_ALLOW_LINKS  TRUE

/*
 *   Our special fake character set ID for bullets.  We distinguish this
 *   from the regular symbol character set, because we use the symbol
 *   character set for the "Symbol" font, but we want to be able to use a
 *   different character set for the bullet font (for which we use
 *   "Wingdings").
 *   
 *   Warning - this MUST NOT OVERLAP with any xxx_CHARSET definition in
 *   wingdi.h.
 */
#define BULLET_CHARSET 10

/* ------------------------------------------------------------------------ */
/*
 *   HTML window panel implementation 
 */

/* ------------------------------------------------------------------------ */
/*
 *   direct sound constants not defined in all dsound.h headers (arg...)
 */
#ifndef DSBCAPS_CTRLDEFAULT
#define DSBCAPS_CTRLDEFAULT \
    ((DSBCAPS_CTRLVOLUME) | (DSBCAPS_CTRLPAN) | (DSBCAPS_CTRLFREQUENCY))
#endif


/* ------------------------------------------------------------------------ */
/*
 *   create 
 */
CHtmlSysWin_win32::CHtmlSysWin_win32(CHtmlFormatter *formatter,
                                     CHtmlSysWin_win32_owner *owner,
                                     int has_vscroll, int has_hscroll,
                                     CHtmlPreferences *prefs)
    : CHtmlSysWin(formatter), CTadsWinScroll(has_vscroll, has_hscroll)
{
    CHtmlRect margins;
    CHtmlFontDesc font_desc;
    static const textchar_t *wingdings = "Wingdings";
    size_t i;
    CTadsAudioPlayer **sndp;

    /* no timers yet */
    timer_list_head_ = 0;

    /* remember my owner */
    owner_ = owner;

    /* remember my preferences source, and set a reference on it */
    prefs_ = prefs;
    prefs_->add_ref();

    /* no status line yet */
    statusline_ = 0;

    /* formatting and pruning are not yet underway */
    formatting_msg_ = FALSE;
    pruning_msg_ = FALSE;

    /* formatting width is unknown */
    fmt_width_ = 0;

    /* remember the formatter */
    formatter_ = formatter;

    /* no content yet */
    content_height_ = 0;
    last_input_height_ = 0;
    more_mode_ = FALSE;
    eof_more_mode_ = FALSE;
    game_more_request_ = FALSE;
    clip_ypos_ = 0;

    /* presume MORE mode will not be enabled */
    more_mode_enabled_ = FALSE;
    nonstop_mode_ = FALSE;

    /* presume we won't ever show a caret */
    caret_enabled_ = FALSE;

    /* the caret isn't visible yet, even if we will be showing it */
    caret_vis_ = FALSE;

    /* we're not in a modal operation that hides the caret */
    caret_modal_hide_ = 0;

    /* set up a default caret shape */
    caret_pos_.set(-100, -100);
    caret_ht_ = 16;
    caret_ascent_ = 0;

    /* not tracking the mouse yet */
    tracking_mouse_ = FALSE;
    track_link_ = 0;
    caret_at_right_ = TRUE;

    /* not hovering over a status line item yet */
    status_link_ = 0;

    /* no command yet */
    cmdtag_ = 0;
    started_reading_cmd_ = FALSE;

    /* create a command buffer */
    cmdbuf_ = new CHtmlInputBuf(100);

    /* set a small inset from the margins */
    margins.set(8, 2, 8, 2);

    /* create the default font */
    font_desc.htmlsize = 3;
    font_desc.default_charset = FALSE;
    font_desc.charset = owner->owner_get_default_charset();
    default_font_ = (CHtmlSysFont_win32 *)get_font(&font_desc);

    /* load the I-beam cursor for text areas */
    ibeam_csr_ = LoadCursor(NULL, IDC_IBEAM);

    /* load the hand cursor */
    hand_csr_ = LoadCursor(CTadsApp::get_app()->get_instance(),
                           "HAND_CURSOR");

    /* load some strings */
    load_res_str(&more_prompt_str_, IDS_MORE_PROMPT);
    load_res_str(&more_status_str_, IDS_MORE_STATUS_MSG);
    load_res_str(&working_str_, IDS_WORKING_MSG);
    load_res_str(&press_key_str_, IDS_PRESS_A_KEY_MSG);
    load_res_str(&exit_pause_str_, IDS_EXIT_PAUSE_MSG);

    /* load the default edit area context menu */
    popup_container_ = 0;
    load_context_popup(IDR_EDIT_POPUP_MENU);

    /* set up the default background and text colors */
    bg_image_ = 0;
    bgbrush_ = 0;
    set_html_bg_color(0, TRUE);
    set_html_text_color(0, TRUE);

    /* 
     *   note whether Wingdings is available; if so, use it for drawing
     *   bullet characters; otherwise, draw all bullets in the prevailing
     *   font at the time of the drawing using the single bullet character
     *   available in most fonts 
     */
    if ((wingdings_avail_ =
        CTadsFont::font_is_present(wingdings, get_strlen(wingdings))) != 0)
    {
        /* use an appropriate character from wingdings for each bullet */
        bullet_square_ = (textchar_t)167;
        bullet_disc_ = (textchar_t)159;
        bullet_circle_ = (textchar_t)250;
    }
    else
    {
        /* don't use wingdings; map all bullets to the standard one */
        bullet_square_ = bullet_disc_ = bullet_circle_ = (char)149;
    }

    /* not pausing before exit or for a keystroke yet */
    exit_pause_ = FALSE;
    waiting_for_key_pause_ = FALSE;

    /* not in an interactive size/move yet */
    in_size_move_ = FALSE;

    /* assume we'll reformat on resizing */
    need_format_on_resize_ = TRUE;

    /* presume we don't have a border */
    has_border_ = FALSE;
    border_handle_ = 0;

    /* presume we will automatically scroll vertically */
    auto_vscroll_ = TRUE;

    /* presume we're not a banner */
    is_banner_win_ = FALSE;
    banner_alignment_ = HTML_BANNERWIN_POS_TOP;
    banner_type_ = HTML_BANNERWIN_BANNERTAG;

    /* we're not in a banner window tree yet */
    banner_parent_ = 0;
    banner_first_child_ = 0;
    banner_next_ = 0;

    /* tell the formatter about us */
    formatter_->set_win(this, &margins);

    /* we don't have a temporary link timer yet */
    temp_link_timer_id_ = 0;

    /* no timers yet */
    cb_timer_id_ = 0;
    bg_timer_id_ = 0;
    sys_timer_head_ = 0;

    /* no tooltip control yet */
    tooltip_ = 0;
    tooltip_vanished_ = FALSE;

    /* not doing a resize reformat yet */
    doing_resize_reformat_ = FALSE;

    /* create our palette */
    create_palette();

    /* clear out our array of active sounds */
    for (i = HTML_MAX_REG_SOUNDS, sndp = active_sounds_ ; i != 0 ;
         --i, ++sndp)
        *sndp = 0;
}

/*
 *   load a resource string into a CStringBuf 
 */
void CHtmlSysWin_win32::load_res_str(CStringBuf *str, int res)
{
    char buf[512];

    /* load the string into our local buffer */
    LoadString(CTadsApp::get_app()->get_instance(), res, buf, sizeof(buf));

    /* store it in the CStringBuf */
    str->set(buf);
}

/*
 *   create our palette 
 */
void CHtmlSysWin_win32::create_palette()
{
    /* logical palette structure, with space for up to 256 colors */
    struct
    {
        /* the Windows LOGPALETTE structure */
        LOGPALETTE lp;

        /* 
         *   space for another 255 entries (the LOGPALETTE has space for one
         *   entry built into it, so we only need another 255) 
         */
        PALETTEENTRY pal[255];
    } logpal;
    size_t i;

    /* 
     *   our fixed palette, with entries in order of priority (since this is
     *   how CreatePalette treats the entries) 
     */
    static struct
    {
        unsigned int r;
        unsigned int g;
        unsigned int b;
    } colors[] =
    {
        /* first 10 Windows standard system colors */
        { 0x00, 0x00, 0x00 },
        { 0x80, 0x00, 0x00 },
        { 0x00, 0x80, 0x00 },
        { 0x80, 0x80, 0x00 },
        { 0x00, 0x00, 0x80 },
        { 0x80, 0x00, 0x80 },
        { 0x00, 0x80, 0x80 },
        { 0xc0, 0xc0, 0xc0 },
        { 0xc0, 0xdc, 0xc0 },
        { 0xa6, 0xca, 0xf0 },

        /* last 10 Windows standard system colors */
        { 0xff, 0xfb, 0xf0 },
        { 0xa0, 0xa0, 0xa4 },
        { 0x80, 0x80, 0x80 },
        { 0xff, 0x00, 0x00 },
        { 0x00, 0xff, 0x00 },
        { 0xff, 0xff, 0x00 },
        { 0x00, 0x00, 0xff },
        { 0xff, 0x00, 0xff },
        { 0x00, 0xff, 0xff },
        { 0xff, 0xff, 0xff },

        /* web colors */
        { 0x99, 0x33, 0x00 },
        { 0xcc, 0x33, 0x00 },
        { 0xff, 0x33, 0x00 },
        { 0x00, 0x66, 0x00 },
        { 0x33, 0x66, 0x00 },
        { 0x66, 0x66, 0x00 },
        { 0x99, 0x66, 0x00 },
        { 0xcc, 0x66, 0x00 },
        { 0xff, 0x66, 0x00 },
        { 0x00, 0x99, 0x00 },
        { 0x33, 0x99, 0x00 },
        { 0x66, 0x99, 0x00 },
        { 0x99, 0x99, 0x00 },
        { 0xcc, 0x99, 0x00 },
        { 0xff, 0x99, 0x00 },
        { 0x00, 0xcc, 0x00 },
        { 0x33, 0xcc, 0x00 },
        { 0x66, 0xcc, 0x00 },
        { 0x99, 0xcc, 0x00 },
        { 0xcc, 0xcc, 0x00 },
        { 0xff, 0xcc, 0x00 },
        { 0xef, 0xd6, 0xc6 },
        { 0xad, 0xa9, 0x90 },
        { 0x66, 0xff, 0x00 },
        { 0x99, 0xff, 0x00 },
        { 0xcc, 0xff, 0x00 },
        { 0x66, 0x33, 0x00 },
        { 0x00, 0x00, 0x33 },
        { 0x33, 0x00, 0x33 },
        { 0x66, 0x00, 0x33 },
        { 0x99, 0x00, 0x33 },
        { 0xcc, 0x00, 0x33 },
        { 0xff, 0x00, 0x33 },
        { 0x00, 0x33, 0x33 },
        { 0x33, 0x33, 0x33 },
        { 0x66, 0x33, 0x33 },
        { 0x99, 0x33, 0x33 },
        { 0xcc, 0x33, 0x33 },
        { 0xff, 0x33, 0x33 },
        { 0x00, 0x66, 0x33 },
        { 0x33, 0x66, 0x33 },
        { 0x66, 0x66, 0x33 },
        { 0x99, 0x66, 0x33 },
        { 0xcc, 0x66, 0x33 },
        { 0xff, 0x66, 0x33 },
        { 0x00, 0x99, 0x33 },
        { 0x33, 0x99, 0x33 },
        { 0x66, 0x99, 0x33 },
        { 0x99, 0x99, 0x33 },
        { 0xcc, 0x99, 0x33 },
        { 0xff, 0x99, 0x33 },
        { 0x00, 0xcc, 0x33 },
        { 0x33, 0xcc, 0x33 },
        { 0x66, 0xcc, 0x33 },
        { 0x99, 0xcc, 0x33 },
        { 0xcc, 0xcc, 0x33 },
        { 0x00, 0x33, 0x00 },
        { 0xff, 0xcc, 0x33 },
        { 0x33, 0xff, 0x33 },
        { 0x66, 0xff, 0x33 },
        { 0x99, 0xff, 0x33 },
        { 0xcc, 0xff, 0x33 },
        { 0xff, 0xff, 0x33 },
        { 0x00, 0x00, 0x66 },
        { 0x33, 0x00, 0x66 },
        { 0x66, 0x00, 0x66 },
        { 0x99, 0x00, 0x66 },
        { 0xcc, 0x00, 0x66 },
        { 0xff, 0x00, 0x66 },
        { 0x00, 0x33, 0x66 },
        { 0x33, 0x33, 0x66 },
        { 0x66, 0x33, 0x66 },
        { 0x99, 0x33, 0x66 },
        { 0xcc, 0x33, 0x66 },
        { 0xff, 0x33, 0x66 },
        { 0x00, 0x66, 0x66 },
        { 0x33, 0x66, 0x66 },
        { 0x66, 0x66, 0x66 },
        { 0x99, 0x66, 0x66 },
        { 0xcc, 0x66, 0x66 },
        { 0xff, 0x66, 0x66 },
        { 0x00, 0x99, 0x66 },
        { 0x33, 0x99, 0x66 },
        { 0x66, 0x99, 0x66 },
        { 0x99, 0x99, 0x66 },
        { 0xcc, 0x99, 0x66 },
        { 0xff, 0x99, 0x66 },
        { 0x00, 0xcc, 0x66 },
        { 0x33, 0xcc, 0x66 },
        { 0x66, 0xcc, 0x66 },
        { 0x99, 0xcc, 0x66 },
        { 0xcc, 0xcc, 0x66 },
        { 0xff, 0xcc, 0x66 },
        { 0x00, 0xff, 0x66 },
        { 0x33, 0xff, 0x66 },
        { 0x66, 0xff, 0x66 },
        { 0x99, 0xff, 0x66 },
        { 0xcc, 0xff, 0x66 },
        { 0xff, 0xff, 0x66 },
        { 0x00, 0x00, 0x99 },
        { 0x33, 0x00, 0x99 },
        { 0x66, 0x00, 0x99 },
        { 0x99, 0x00, 0x99 },
        { 0xcc, 0x00, 0x99 },
        { 0xff, 0x00, 0x99 },
        { 0x00, 0x33, 0x99 },
        { 0x33, 0x33, 0x99 },
        { 0x66, 0x33, 0x99 },
        { 0x99, 0x33, 0x99 },
        { 0xcc, 0x33, 0x99 },
        { 0xff, 0x33, 0x99 },
        { 0x00, 0x66, 0x99 },
        { 0x33, 0x66, 0x99 },
        { 0x66, 0x66, 0x99 },
        { 0x99, 0x66, 0x99 },
        { 0xcc, 0x66, 0x99 },
        { 0xff, 0x66, 0x99 },
        { 0x00, 0x99, 0x99 },
        { 0x33, 0x99, 0x99 },
        { 0x66, 0x99, 0x99 },
        { 0x99, 0x99, 0x99 },
        { 0xcc, 0x99, 0x99 },
        { 0xff, 0x99, 0x99 },
        { 0x00, 0xcc, 0x99 },
        { 0x33, 0xcc, 0x99 },
        { 0x66, 0xcc, 0x99 },
        { 0x99, 0xcc, 0x99 },
        { 0xcc, 0xcc, 0x99 },
        { 0xff, 0xcc, 0x99 },
        { 0x00, 0xff, 0x99 },
        { 0x33, 0xff, 0x99 },
        { 0x66, 0xff, 0x99 },
        { 0x99, 0xff, 0x99 },
        { 0xcc, 0xff, 0x99 },
        { 0xff, 0xff, 0x99 },
        { 0x00, 0x00, 0xcc },
        { 0x33, 0x00, 0xcc },
        { 0x66, 0x00, 0xcc },
        { 0x99, 0x00, 0xcc },
        { 0xcc, 0x00, 0xcc },
        { 0xff, 0x00, 0xcc },
        { 0x00, 0x33, 0xcc },
        { 0x33, 0x33, 0xcc },
        { 0x66, 0x33, 0xcc },
        { 0x99, 0x33, 0xcc },
        { 0xcc, 0x33, 0xcc },
        { 0xff, 0x33, 0xcc },
        { 0x00, 0x66, 0xcc },
        { 0x33, 0x66, 0xcc },
        { 0x66, 0x66, 0xcc },
        { 0x99, 0x66, 0xcc },
        { 0xcc, 0x66, 0xcc },
        { 0xff, 0x66, 0xcc },
        { 0x00, 0x99, 0xcc },
        { 0x33, 0x99, 0xcc },
        { 0x66, 0x99, 0xcc },
        { 0x99, 0x99, 0xcc },
        { 0xcc, 0x99, 0xcc },
        { 0xff, 0x99, 0xcc },
        { 0x00, 0xcc, 0xcc },
        { 0x33, 0xcc, 0xcc },
        { 0x66, 0xcc, 0xcc },
        { 0x99, 0xcc, 0xcc },
        { 0xcc, 0xcc, 0xcc },
        { 0xff, 0xcc, 0xcc },
        { 0x00, 0xff, 0xcc },
        { 0x33, 0xff, 0xcc },
        { 0x66, 0xff, 0xcc },
        { 0x99, 0xff, 0xcc },
        { 0xcc, 0xff, 0xcc },
        { 0xff, 0xff, 0xcc },
        { 0xa5, 0x00, 0x21 },
        { 0xd6, 0x00, 0x93 },
        { 0x66, 0x00, 0xff },
        { 0x99, 0x00, 0xff },
        { 0xcc, 0x00, 0xff },
        { 0xff, 0x50, 0x50 },
        { 0x33, 0x33, 0x00 },
        { 0x33, 0x33, 0xff },
        { 0x66, 0x33, 0xff },
        { 0x99, 0x33, 0xff },
        { 0xcc, 0x33, 0xff },
        { 0xff, 0x33, 0xff },
        { 0x00, 0x66, 0xff },
        { 0x33, 0x66, 0xff },
        { 0x66, 0x66, 0xff },
        { 0x99, 0x66, 0xff },
        { 0xcc, 0x66, 0xff },
        { 0xff, 0x66, 0xff },
        { 0x00, 0x99, 0xff },
        { 0x33, 0x99, 0xff },
        { 0x66, 0x99, 0xff },
        { 0x99, 0x99, 0xff },
        { 0xcc, 0x99, 0xff },
        { 0xff, 0x99, 0xff },
        { 0x00, 0xcc, 0xff },
        { 0x33, 0xcc, 0xff },
        { 0x66, 0xcc, 0xff },
        { 0x99, 0xcc, 0xff },
        { 0xcc, 0xcc, 0xff },
        { 0xff, 0xcc, 0xff },
        { 0x33, 0xff, 0xff },
        { 0x66, 0xff, 0xff },
        { 0x99, 0xff, 0xff },
        { 0xcc, 0xff, 0xff },
        { 0xff, 0x7c, 0x80 },
        { 0x04, 0x04, 0x04 },
        { 0x08, 0x08, 0x08 },
        { 0x0c, 0x0c, 0x0c },
        { 0x11, 0x11, 0x11 },
        { 0x16, 0x16, 0x16 },
        { 0x1c, 0x1c, 0x1c },
        { 0x22, 0x22, 0x22 },
        { 0x29, 0x29, 0x29 },
        { 0x39, 0x39, 0x39 },
        { 0x42, 0x42, 0x42 },
        { 0x4d, 0x4d, 0x4d },
        { 0x55, 0x55, 0x55 },
        { 0x5f, 0x5f, 0x5f },
        { 0x77, 0x77, 0x77 },
        { 0x86, 0x86, 0x86 },
        { 0x96, 0x96, 0x96 },
        { 0xb2, 0xb2, 0xb2 },
        { 0xcb, 0xcb, 0xcb },
        { 0xd7, 0xd7, 0xd7 },
        { 0xdd, 0xdd, 0xdd },
        { 0xe3, 0xe3, 0xe3 },
        { 0xea, 0xea, 0xea },
        { 0xf1, 0xf1, 0xf1 },
        { 0xf8, 0xf8, 0xf8 },
        { 0xe7, 0xe7, 0xd6 },
        { 0xcc, 0xec, 0xff },
        { 0x33, 0x00, 0x00 },
        { 0x66, 0x00, 0x00 },
        { 0x99, 0x00, 0x00 },
        { 0xcc, 0x00, 0x00 }
    };

    /* populate the LOGPALETTE from our color list */
    for (i = 0 ; i < sizeof(colors)/sizeof(colors[0]) && i < 256 ; ++i)
    {
        /* populate this entry */
        logpal.lp.palPalEntry[i].peFlags = 0;
        logpal.lp.palPalEntry[i].peRed = (BYTE)colors[i].r;
        logpal.lp.palPalEntry[i].peGreen = (BYTE)colors[i].g;
        logpal.lp.palPalEntry[i].peBlue = (BYTE)colors[i].b;
    }

    /* set the palette size to the number of colors we copied */
    logpal.lp.palNumEntries = (WORD)i;

    /* set the logical palette version number */
    logpal.lp.palVersion = 0x0300;

    /* create the palette */
    hpal_ = CreatePalette(&logpal.lp);
}

/*
 *   destroy 
 */
CHtmlSysWin_win32::~CHtmlSysWin_win32()
{
    CHtmlSysWin_win32 *cur;

    /* delete the command buffer */
    delete cmdbuf_;

    /* we're done with the cursors */
    DestroyCursor(ibeam_csr_);
    DestroyCursor(hand_csr_);

    /* done with the context menus */
    if (popup_container_ != 0)
        DestroyMenu(popup_container_);

    /* remove my reference on the background image if I have one */
    if (bg_image_)
        bg_image_->remove_ref();

    /* destroy our background brush, if we created one */
    if (bgbrush_ != 0)
        DeleteObject(bgbrush_);

    /* release our reference on the preferences object */
    prefs_->release_ref();

    /* delete our palette, if we created one */
    if (hpal_ != 0)
        DeleteObject(hpal_);

    /* delete all of our system timers */
    while (sys_timer_head_ != 0)
        delete_timer(sys_timer_head_);

    /* 
     *   If we have any children, notify them that the parent is closing.
     *   Once the parent is closed, a child can no longer be displayed, so we
     *   must hide each child's system window.  We must also clear its
     *   reference to its parent, since the parent object is being deleted
     *   and thus the references to it in our children are no longer valid.
     *   Note that we don't actually destroy the children; they stay in
     *   memory until they're explicitly destroyed by the client.  
     */
    for (cur = banner_first_child_ ; cur != 0 ; cur = cur->banner_next_)
        cur->on_close_parent_banner(TRUE);

    /* remove myself from my banner parent's child list, if appropriate */
    if (banner_parent_ != 0)
        banner_parent_->remove_banner_child(this);
}

/*
 *   Load an edit popup, destroying any existing one 
 */
void CHtmlSysWin_win32::load_context_popup(int menu_id)
{
    /* destroy any existing edit popup */
    if (popup_container_ != 0)
        DestroyMenu(popup_container_);
    
    /* load the new one */
    popup_container_ = LoadMenu(CTadsApp::get_app()->get_instance(),
                                MAKEINTRESOURCE(menu_id));

    /* extract the context submenu from the menu container */
    edit_popup_ = GetSubMenu(popup_container_, 0);
}


/*
 *   Receive notification that the parent window is being destroyed.  This
 *   means that I'll be destroyed soon, but we need to remove any
 *   references to the parent window before the parent is deleted.  
 */
void CHtmlSysWin_win32::on_destroy_parent()
{
    /* if I have a status line, unregister myself as a source */
    if (statusline_ != 0)
    {
        /* tell the status line to forget about me */
        statusline_->unregister_source(this);

        /* forget about the status line so we don't unregister again later */
        statusline_ = 0;
    }

    /* 
     *   notify my formatter that I'm soon to be deleted - it may need to
     *   do some work to disentangle itself from me 
     */
    formatter_->unset_win();
}


/*
 *   Set the status line 
 */
void CHtmlSysWin_win32::set_status_line(CTadsStatusline *statusline)
{
    /* remember the status line */
    statusline_ = statusline;

    /* register myself as a status source */
    statusline->register_source(this);
}

/*
 *   create a font - used as a callback for CTadsApp::get_font.  We
 *   create and return a CHtmlSysFont_win32 object, rather than a basic
 *   CTadsFont object, since we want the additional interfaces provided by
 *   the Html subclass.  
 */
CTadsFont *CHtmlSysWin_win32::create_font(const CTadsLOGFONT *lf)
{
    return new CHtmlSysFont_win32(lf);
}

/*
 *   Get the system mapping for a parameterized font.  Returns true and
 *   fills in the logical font descriptor with the actual system font name
 *   if the font name is one of the parameterized font names, false if not.
 *   If we return true, then we'll also fill in *base_point_size with the
 *   base point size of the font as specified in the player preferences.  
 */
int CHtmlSysWin_win32::get_param_font(CTadsLOGFONT *tlf, int *base_point_size,
                                      const textchar_t *name, size_t len,
                                      CHtmlFontDesc *font_desc)
{
    static const textchar_t nm_serif[] = HTMLFONT_TADS_SERIF;
    static const textchar_t nm_sans[] = HTMLFONT_TADS_SANS;
    static const textchar_t nm_script[] = HTMLFONT_TADS_SCRIPT;
    static const textchar_t nm_typewriter[] = HTMLFONT_TADS_TYPEWRITER;
    static const textchar_t nm_input[] = HTMLFONT_TADS_INPUT;

    /* check each name */
    if (len == sizeof(nm_serif) - sizeof(textchar_t)
        && memicmp(nm_serif, name, len) == 0)
    {
        strcpy(tlf->lf.lfFaceName, prefs_->get_font_serif());
        *base_point_size = prefs_->get_serif_fontsz();
    }
    else if (len == sizeof(nm_sans) - sizeof(textchar_t)
             && memicmp(nm_sans, name, len) == 0)
    {
        strcpy(tlf->lf.lfFaceName, prefs_->get_font_sans());
        *base_point_size = prefs_->get_sans_fontsz();
    }
    else if (len == sizeof(nm_script) - sizeof(textchar_t)
             && memicmp(nm_script, name, len) == 0)
    {
        strcpy(tlf->lf.lfFaceName, prefs_->get_font_script());
        *base_point_size = prefs_->get_script_fontsz();
    }
    else if (len == sizeof(nm_typewriter) - sizeof(textchar_t)
             && memicmp(nm_typewriter, name, len) == 0)
    {
        strcpy(tlf->lf.lfFaceName, prefs_->get_font_typewriter());
        *base_point_size = prefs_->get_typewriter_fontsz();
    }
    else if (len == sizeof(nm_input) - sizeof(textchar_t)
             && memicmp(nm_input, name, len) == 0)
    {
        HTML_color_t inp_color;

        /* use the selected font name and size */
        strcpy(tlf->lf.lfFaceName, prefs_->get_inpfont_name());
        *base_point_size = prefs_->get_inpfont_size();

        /* 
         *   if the face name is empty, it means that we're to use the
         *   standard input font 
         */
        if (tlf->lf.lfFaceName[0] == '\0')
            strcpy(tlf->lf.lfFaceName, prefs_->get_prop_font());

        /* 
         *   Apply the color and style settings as well, if the face name
         *   was set explicitly.  If the face name wasn't set explicitly, it
         *   means that it was inherited from surrounding text, so we don't
         *   want to pick up the extra preference attributes. 
         */
        if (tlf->face_set_explicitly)
        {
            /* get the italics settings for the input font */
            tlf->lf.lfItalic = (BYTE)prefs_->get_inpfont_italic();

            /* get the weight settings for the input font */
            tlf->lf.lfWeight = (prefs_->get_inpfont_bold()
                                ? FW_BOLD : FW_NORMAL);

            /* get the input color settings for the input font */
            inp_color = prefs_->get_inpfont_color();
            tlf->color = HTML_color_to_COLORREF(inp_color);

            /* 
             *   set the color and attributes in the font descriptor, so that
             *   we know to use the explicit color setting 
             */
            font_desc->default_color = FALSE;
            font_desc->color = inp_color;
            font_desc->weight = (tlf->lf.lfWeight == FW_BOLD ? 700 : 400);
            font_desc->italic = tlf->lf.lfItalic;
        }
    }
    else
    {
        /* it's not a parameterized font name */
        return FALSE;
    }

    /* if we got here, we've mapped the font */
    return TRUE;
}

/*
 *   Get a font suitable for the given characteristics 
 */
CHtmlSysFont *CHtmlSysWin_win32::get_font(const CHtmlFontDesc *in_font_desc)
{
    CTadsLOGFONT tlf;
    int pointsize;
    CHtmlSysFont_win32 *new_font;
    int created;
    CHtmlFontDesc font_desc;
    int htmlsize;
    int base_point_size;

    /* make a copy of the original font descriptor, so we can change it */
    font_desc = *in_font_desc;

    /* 
     *   if they want the asked for the default character set, plug in the
     *   actual default character set now 
     */
    if (font_desc.default_charset)
        font_desc.charset = owner_->owner_get_default_charset();

    /* set up the default parts of the LOGFONT that we never change */
    memset(&tlf, 0, sizeof(tlf));
    tlf.lf.lfWidth = 0;
    tlf.lf.lfEscapement = 0;
    tlf.lf.lfOrientation = 0;
    tlf.lf.lfCharSet = (BYTE)font_desc.charset.charset;
    tlf.lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    tlf.lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    tlf.lf.lfQuality = DEFAULT_QUALITY;
    tlf.lf.lfPitchAndFamily = FF_DONTCARE;

    /*
     *   use the weight they provided (we may change this if a weight
     *   modifier is specified)
     */
    tlf.lf.lfWeight = font_desc.weight;

    /* presume we're not going to specify a face name explicitly */
    tlf.lf.lfFaceName[0] = '\0';

    /* apply the specific font attributes */
    tlf.lf.lfItalic = (BYTE)font_desc.italic;
    tlf.lf.lfUnderline = (BYTE)font_desc.underline;
    tlf.lf.lfStrikeOut = (BYTE)font_desc.strikeout;

    /* set the extended CTadsLOGFONT attributes */
    tlf.superscript = font_desc.superscript;
    tlf.subscript = font_desc.subscript;
    tlf.color = HTML_color_to_COLORREF(font_desc.color);
    tlf.color_set_explicitly = !font_desc.default_color;
    tlf.bgcolor = HTML_color_to_COLORREF(font_desc.bgcolor);
    tlf.bgcolor_set = !font_desc.default_bgcolor;
    tlf.face_set_explicitly = font_desc.face_set_explicitly;

    /* 
     *   note the HTML SIZE parameter requested - if this is zero, it
     *   indicates that we want to use a specific point size instead 
     */
    htmlsize = font_desc.htmlsize;

    /*
     *   Assume that we'll use the base point size of the default text font
     *   (the main proportional font) as the basis of the size.  If we find
     *   a specific named parameterized font name, we'll change to use the
     *   size as specified in the player preferences for that parameterized
     *   font; but if we have a particular name given, the player has no way
     *   to directly specify the base size for that font, so the best we can
     *   do is use the default text font size for guidance.  
     */
    base_point_size = prefs_->get_prop_fontsz();

    /* 
     *   If a face name is listed, try to find the given face in the
     *   system.  Note that we do not attempt to use the font name if
     *   we're in the symbol character set -- if we're in the symbol
     *   character set, it means that we are explicitly using an HTML
     *   symbol entity, and we map these to the character set "Symbol";
     *   other fonts in this character set do not use the same code point
     *   layout.
     *   
     *   Note that we wait until after setting all of the basic attributes
     *   (in particular, weight, color, and styles) before dealing with
     *   the face name; this is to allow parameterized face names to
     *   override the basic attributes.  For example, the "TADS-Input"
     *   font allows the player to specify color, bold, and italic styles
     *   in addition to the font name.  
     */
    if (font_desc.face[0] != 0
        && tlf.lf.lfCharSet != SYMBOL_CHARSET)
    {
        const char *start;
        const char *end;
        size_t rem;

        /* look at each name in the list */
        for (start = font_desc.face, rem = get_strlen(start) ; rem > 0 ; )
        {
            int endrem;
            
            /* find the end of this name */
            for (end = start, endrem = rem ; endrem > 0 && *end != ',' ;
                 ++end, --endrem) ;

            /* 
             *   check to see if this is a parameterized font name, and if
             *   so use it 
             */
            if (get_param_font(&tlf, &base_point_size, start, end - start,
                               &font_desc))
                break;

            /* see if this font exists on this system */
            if (CTadsFont::font_is_present(start, end - start))
            {
                size_t len;
                
                /*
                 *   This font is present - use this name.  Note that we
                 *   need to limit the length of the face to the available
                 *   size in the LOGFONT structure's face name buffer.  
                 */
                len = end - start;
                if (len > LF_FACESIZE - 1) len = LF_FACESIZE - 1;
                memcpy(tlf.lf.lfFaceName, start, len);
                tlf.lf.lfFaceName[len] = '\0';
                
                /* no need to keep looking */
                break;
            }

            /* move on to the next name in the list, if there is one */
            start = end;
            rem = endrem;

            /* skip the comma, if present */
            if (rem != 0 && *start == ',')
                ++start, --rem;

            /* skip any spaces after the comma */
            while (rem != 0 && is_space(*start))
                ++start, --rem;
        }
    }

    /* 
     *   if we're in the symbol character set, always use "Symbol" as the
     *   font 
     */
    if (tlf.lf.lfCharSet == SYMBOL_CHARSET)
        strcpy(tlf.lf.lfFaceName, "Symbol");

    /* 
     *   if we're in our fake BULLET character set, use the SYMBOL
     *   character set as the actual character set - this is the character
     *   set that the bullet font uses 
     */
    if (tlf.lf.lfCharSet == BULLET_CHARSET)
        tlf.lf.lfCharSet = SYMBOL_CHARSET;

    /*
     *   Apply characteristics only if the face wasn't specified 
     */
    if (tlf.lf.lfFaceName[0] == '\0')
    {
        /* see if fixed-pitch is desired */
        if (font_desc.fixed_pitch)
        {
            /* set the characteristics */
            tlf.lf.lfPitchAndFamily &= ~VARIABLE_PITCH;
            tlf.lf.lfPitchAndFamily |= FIXED_PITCH;

            /* use prefered monospaced font */
            strcpy(tlf.lf.lfFaceName, prefs_->get_mono_font());
            base_point_size = prefs_->get_mono_fontsz();
        }
        else
        {
            /* use prefered proportional font */
            strcpy(tlf.lf.lfFaceName, prefs_->get_prop_font());
            base_point_size = prefs_->get_prop_fontsz();
        }

        /* see if serifs are desired for a variable-pitch font */
        if (!font_desc.serif && !font_desc.fixed_pitch)
        {
            /* set the family information */
            tlf.lf.lfPitchAndFamily &= ~FF_ROMAN;
            tlf.lf.lfPitchAndFamily |= FF_SWISS;

            /* ask for the prefered sans-serif font */
            strcpy(tlf.lf.lfFaceName, prefs_->get_font_sans());
            base_point_size = prefs_->get_sans_fontsz();
        }

        /* see if emphasis (EM) is desired - render italic if so */
        if (font_desc.pe_em)
            tlf.lf.lfItalic = TRUE;

        /* see if strong emphasis (STRONG) is desired - render bold if so */
        if (font_desc.pe_strong && tlf.lf.lfWeight < 700)
            tlf.lf.lfWeight = 700;

        /* if we're in an address block, render in italics */
        if (font_desc.pe_address)
            tlf.lf.lfItalic = TRUE;

        /* see if this is a defining instance (DFN) - render in italics */
        if (font_desc.pe_dfn)
            tlf.lf.lfItalic = TRUE;

        /*
         *   see if this is sample code (SAMP), keyboard code (KBD), or a
         *   variable (VAR) - render these in a monospaced roman font if
         *   so 
         */
        if (font_desc.pe_samp || font_desc.pe_kbd || font_desc.pe_var)
        {
            /* set the characteristics for a fixed-pitch roman font */
            tlf.lf.lfPitchAndFamily = FIXED_PITCH | FF_ROMAN;

            /* render KBD in bold */
            if (font_desc.pe_kbd && tlf.lf.lfWeight < 700)
                tlf.lf.lfWeight = 700;

            /* ask for the prefered typewriter font */
            strcpy(tlf.lf.lfFaceName, prefs_->get_font_typewriter());
            base_point_size = prefs_->get_typewriter_fontsz();
        }

        /* see if this is a citation (CITE) - render in italics if so */
        if (font_desc.pe_cite)
            tlf.lf.lfItalic = TRUE;
    }

    /* if a valid HTML size is specified, map it to a point size */
    if (htmlsize >= 1 && htmlsize <= 7)
    {
        int scale;
        static const int size_pct[] = { 60, 80, 100, 120, 150, 200, 300 };
        
        /*
         *   An HTML size is specified -- if a BIG or SMALL attribute is
         *   present, bump the HTML size by 1 in the appropriate direction,
         *   if we're not already at a limit.  
         */
        if (font_desc.pe_big && htmlsize < 7)
            ++htmlsize;
        else if (font_desc.pe_small && htmlsize > 1)
            --htmlsize;

        /*
         *   Adjust for the current scale, if we're not at "medium."  Don't
         *   just adjust by one HTML size or one point size; instead, adjust
         *   up or down by one notch on the list of font point sizes we
         *   offer in the preferences dialog, so that the effect is the same
         *   as though the player had gone to the preferences dialog and
         *   moved all of the size popups up or down one or two notches.
         *   
         *   To do this, find the current base size in the list of possible
         *   base sizes that we offer in the preferences dialog, then move
         *   up or down one or two notches (smallest is down two notches,
         *   small down one notch, large up one, and largest up two).  
         */
        if ((scale = prefs_->get_font_scale()) != 2)
        {
            const int *p;
            
            /* find the original base point size in the list of sizes */
            for (p = CHtmlPreferences::font_pt_sizes ;
                 *p != 0 && *p != base_point_size ; ++p) ;

            /* if we found it, adjust up or down, as appropriate */
            if (scale < 2)
            {
                /* adjust down by (2 - scale) notches */
                for ( ; p != CHtmlPreferences::font_pt_sizes && scale != 2 ;
                      --p, ++scale) ;
            }
            else
            {
                /* adjust up by (scale - 2) notches */
                for ( ; *p != 0 && *(p+1) != 0 && scale != 2 ; ++p, --scale) ;
            }

            /* get the new base size */
            if (*p != 0)
                base_point_size = *p;
        }

        /*
         *   Adjust for the HTML SIZE setting.  There are 7 possible size
         *   settings, numbered 1 through 7.  Adjust the font size by
         *   applying a scaling factor for the font sizes.
         *   
         *   Our size factor table is in percentages, so multiply by the
         *   size factor, add in 50 so that we round properly, and divide by
         *   100 to get the new size.  
         */
        pointsize = ((base_point_size * size_pct[htmlsize - 1]) + 50) / 100;
    }
    else
    {
        /* there's no HTML size - use the explicit point size provided */
        pointsize = font_desc.pointsize;
    }

    /* map the point size to LOGFONT units */
    tlf.lf.lfHeight = CTadsFont::calc_lfHeight(pointsize);

    /*
     *   We've built up our font record -- create a font
     */
    new_font = (CHtmlSysFont_win32 *)
        CTadsApp::get_app()->get_font(&tlf, &create_font, &created);

    /* remember the font description used to create the font */
    if (created)
        new_font->set_font_desc(&font_desc);

    /* return the new font */
    return new_font;
}

/*
 *   Determine if the MORE prompt is showing.  Returns true if we're
 *   scrolled to the bottom, so that the latest text is showing, false if
 *   not. 
 */
int CHtmlSysWin_win32::moreprompt_is_at_bottom()
{
    RECT rcmore;
    RECT rcwin;
    
    /* get the MORE prompt rectangle and the window area */
    get_moreprompt_rect(&rcmore);
    get_scroll_area(&rcwin, TRUE);
    
    /* return true if the MORE prompt is at the bottom */
    return (rcmore.bottom <= rcwin.bottom);
}

/*
 *   determine if we can copy 
 */
int CHtmlSysWin_win32::can_copy()
{
    unsigned long startofs, endofs;

    /* get the current selection */
    formatter_->get_sel_range(&startofs, &endofs);

    /* we can copy only if there's something selected */
    return (startofs != endofs);
}


/*
 *   Copy text to a new global memory handle 
 */
HGLOBAL CHtmlSysWin_win32::copy_to_new_hglobal(UINT alloc_flags, size_t *lenp)
{
    unsigned long startofs, endofs;
    unsigned long len;
    HGLOBAL memhdl;
    textchar_t *memptr;
    unsigned long nlcnt;
    CStringBuf *buf;
    textchar_t *p;

    /* get the current selection */
    formatter_->get_sel_range(&startofs, &endofs);

    /* if there's nothing selected, there's nothing to do */
    if (startofs == endofs)
        return FALSE;

    /* figure out how much space we need */
    len = formatter_->get_chars_in_ofs_range(startofs, endofs);

    /* allocate space */
    buf = new CStringBuf(len);

    /* get the text in the internal format */
    formatter_->extract_text(buf, startofs, endofs);

    /* get the actual length of the string */
    len = get_strlen(buf->get());

    /* count newlines, since we'll need to expand them into CR-LF's */
    for (p = buf->get(), nlcnt = 0 ; *p != '\0' ; ++p)
    {
        /* if it's a newline, count it */
        if (*p == '\n')
            ++nlcnt;
    }

    /* allocate and lock a global memory object to hold the text */
    memhdl = GlobalAlloc(alloc_flags, (len + nlcnt + 1)*sizeof(textchar_t));
    memptr = (textchar_t *)GlobalLock(memhdl);

    /* tell the caller how much space we need, if they want to know */
    if (lenp != 0)
        *lenp = (len + nlcnt + 1)*sizeof(textchar_t);

    /* copy the result into the block */
    for (p = buf->get() ; *p != '\0' ; ++p)
    {
        if (*p == '\n')
        {
            *memptr++ = 13;
            *memptr++ = 10;
        }
        else
        {
            /* copy this character directly */
            *memptr++ = *p;
        }
    }

    /* add a terminating null byte */
    *memptr = '\0';

    /* we're done with the buffer */
    delete buf;

    /* done with the handle's memory - unlock it */
    GlobalUnlock(memhdl);

    /* return the new handle */
    return memhdl;
}

/*
 *   Copy text to clipboard 
 */
int CHtmlSysWin_win32::do_copy()
{
    HGLOBAL memhdl;

    /* take over the clipboard */
    if (!OpenClipboard(handle_))
        return FALSE;

    /* delete the current clipboard contents */
    EmptyClipboard();

    /* copy the current selection to a global memory handle */
    memhdl = copy_to_new_hglobal(GHND, 0);

    /* add the text to the clipboard */
    SetClipboardData(CF_TEXT, memhdl);

    /* done with the clipboard */
    CloseClipboard();

    /* success */
    return TRUE;
}

/*
 *   determine if we can cut 
 */
int CHtmlSysWin_win32::can_cut()
{
    unsigned long startofs, endofs;

    /* if there's no command, we can't do any cutting */
    if (cmdtag_ == 0)
        return FALSE;

    /* get the current selection */
    formatter_->get_sel_range(&startofs, &endofs);

    /*
     *   if there's no selection, or the selected text doesn't overlap
     *   the command line, there's nothing to cut; otherwise, cutting is
     *   possible 
     */
    return (startofs != endofs && endofs > cmdtag_->get_text_ofs());
}

/*
 *   Cut text to clipboard 
 */
int CHtmlSysWin_win32::do_cut()
{
    unsigned long startofs, endofs;

    /* if there's no command, we can't do any cutting */
    if (cmdtag_ == 0)
        return FALSE;

    /* get the current selection */
    formatter_->get_sel_range(&startofs, &endofs);

    /*
     *   if there's no selection, or the selected text doesn't overlap
     *   the command line, there's nothing to cut 
     */
    if (startofs == endofs || endofs <= cmdtag_->get_text_ofs())
        return FALSE;

    /* copy the text onto the clipboard */
    if (!do_copy())
        return FALSE;

    /* delete the selected range in the command line */
    if (cmdbuf_->del_right())
        update_input_display();

    /* we did some work */
    return TRUE;
}

/*
 *   determine if we can paste 
 */
int CHtmlSysWin_win32::can_paste()
{
    UINT fmt;
    int found;
    
    /* if we're not editing a command, we can't do any pasting */
    if (cmdtag_ == 0 || !started_reading_cmd_)
        return FALSE;

    /* get access to the clipboard */
    if (!OpenClipboard(handle_))
        return FALSE;

    /* see if "text" is on the clipboard */
    for (fmt = 0, found = FALSE ;; )
    {
        /* get the next format - stop if there are no more */
        fmt = EnumClipboardFormats(fmt);
        if (fmt == 0)
            break;

        /* if it's the one we want, note it */
        if (fmt == CF_TEXT)
        {
            found = TRUE;
            break;
        }
    }

    /* done with the clipboard */
    CloseClipboard();

    /* if we found our format, we can paste */
    return found;
}

/*
 *   Paste text from clipboard 
 */
int CHtmlSysWin_win32::do_paste()
{
    HANDLE hdl;
    const char *buf;
    int disp_change;
    
    /* if we're not editing a command, we can't do any pasting */
    if (cmdtag_ == 0 || !started_reading_cmd_)
        return FALSE;

    /* get access to the clipboard */
    if (!OpenClipboard(handle_))
        return FALSE;

    /* if there's no text in the clipboard, we can't do any pasting */
    hdl = GetClipboardData(CF_TEXT);
    if (hdl == 0)
        return FALSE;

    /* lock the handle to get the text */
    buf = (const char *)GlobalLock(hdl);
    if (buf == 0)
        return FALSE;

    /* insert the text from the hglobal */
    disp_change = insert_text_from_hglobal(buf);

    /* done with the clipboard data */
    GlobalUnlock(hdl);
    CloseClipboard();

    /* update the display if necessary */
    if (disp_change)
        update_input_display();

    /* if I don't have focus, set focus here */
    take_focus();

    /* return true if we changed the command buffer */
    return disp_change;
}

/*
 *   Insert text from an HGLOBAL in clipboard text format 
 */
int CHtmlSysWin_win32::insert_text_from_hglobal(const char *buf)
{
    const char *startp;
    const char *curp;
    int disp_change = FALSE;

    /*
     *   Go through the characters and insert the ones that we can.  If we
     *   run across any newlines or tabs, insert spaces instead.  
     */
    for (curp = startp = buf ; *curp != '\0' ; ++curp)
    {
        /* see if we have anything we need to convert */
        switch(*curp)
        {
        case 13:
        case 10:
        case 9:
            /* insert what we have so far */
            disp_change |= cmdbuf_->add_string(startp, curp - startp, TRUE);

            /* add a space in place of this character */
            disp_change |= cmdbuf_->add_char(' ');

            /*
             *   if we have a CR-LF sequence, or an LF-CR sequence, treat
             *   the combination as a single character 
             */
            if ((*curp == 13 && *(curp + 1) == 10)
                || (*curp == 10 && *(curp + 1) == 13))
                ++curp;

            /*
             *   remember the next character as the start of the next
             *   substring for later addition 
             */
            startp = curp + 1;
            break;

        default:
            /* let this character accumulate for later string insertion */
            break;
        }
    }

    /* if we still have a substring to add, add it now */
    if (curp > startp)
        disp_change |= cmdbuf_->add_string(startp, curp - startp, TRUE);

    /* tell the caller whether the display changed or not */
    return disp_change;
}

/*
 *   Create an IDataObject for dragging from this window.  We'll return a
 *   data object containing the text in the selection range. 
 */
IDataObject *CHtmlSysWin_win32::get_drag_dataobj()
{
    HGLOBAL memhdl;
    size_t len;

    /* copy the selection to a global memory handle */
    memhdl = copy_to_new_hglobal(GHND, &len);

    /* create and return the new text data object */
    return new CTadsDataObjText(memhdl, len);
}

/*
 *   Process a palette change request 
 */
int CHtmlSysWin_win32::do_querynewpalette()
{
    /* set our foreground palette */
    if (recalc_palette_fg())
    {
        InvalidateRect(get_handle(), 0, TRUE);
        return TRUE;
    }
    else
    {
        /* no change, but let secondaries realize */
        SendMessage(handle_, WM_PALETTECHANGED, (WPARAM)handle_, 0);
    }

    /* we didn't change anything */
    return FALSE;
}

/*
 *   Update for a palette change in another window 
 */
void CHtmlSysWin_win32::do_palettechanged(HWND srcwin)
{
    /* check whether we caused the palette change or someone else did */
    if (srcwin == get_handle())
    {
        /* we initiated the change - set our background palettes */
        if (recalc_palette_bg())
            InvalidateRect(get_handle(), 0, TRUE);
        return;
    }
    else
    {
        /* the change is from elsewhere - recalculate all of our palettes */
        recalc_palette_fg();
        recalc_palette_bg();

        /* redraw with the new palette */
        InvalidateRect(get_handle(), 0, TRUE);
    }
}

/*
 *   Handle a setcursor message 
 */
int CHtmlSysWin_win32::do_setcursor(HWND hwnd, int /*hittest*/,
                                    int /*mousemsg*/)
{
    POINT pt;
    CHtmlPoint docpt;
    RECT rc;
    CHtmlDisp *disp;
    unsigned long txtofs;
    unsigned long sel_start, sel_end;
    
    /* if it's not over my window area, ignore it */
    if (hwnd != handle_)
    {
        /* if I'm tracking a hover link, forget about it */
        set_hover_link(0);
        
        /* we didn't set a cursor */
        return FALSE;
    }

    /* 
     *   tell other subwindows to forget about any current hovering, since
     *   we're obviously no longer in any other subwindow 
     */
    owner_->clear_other_hover_info(this);

    /* get the mouse position */
    GetCursorPos(&pt);
    ScreenToClient(handle_, &pt);

    /*
     *   if I'm over the vertical scrollbar, ignore it; this can happen
     *   when the vertical scrollbar is disabled, since it won't show up
     *   for setcursor messages 
     */
    get_scroll_area(&rc, TRUE);
    if (pt.x >= rc.right)
        return FALSE;

    /* if I'm over the "MORE" prompt, show the hand cursor */
    get_moreprompt_rect(&rc);
    if (!prefs_->get_alt_more_style() && more_mode_ && PtInRect(&rc, pt))
    {
        SetCursor(hand_csr_);
        return TRUE;
    }

    /* see if we can find an item containing the given position */
    docpt = screen_to_doc(CHtmlPoint(pt.x, pt.y));
    txtofs = formatter_->find_textofs_by_pos(docpt);
    disp = formatter_->find_by_pos(docpt, TRUE);
    if (disp == 0)
    {
        /* if we have a status line link display, remove it */
        set_hover_link(0);
        
        /* we're not over a display list item - use the background cursor */
        return do_setcursor_bkg();
    }

    /* 
     *   If we're over the current selection range, use an arrow, since we
     *   could try to drag the selection; otherwise, use the current display
     *   item's cursor. 
     */
    formatter_->get_sel_range(&sel_start, &sel_end);
    if (txtofs >= sel_start && txtofs < sel_end)
    {
        /* it's over the selection - use an arrow */
        SetCursor(arrow_cursor_);
    }
    else
    {
        /* set the cursor based on the display item type */
        set_disp_item_cursor(disp, docpt.x, docpt.y);
    }

    /* set the current hovering link */
    set_hover_link(find_link_for_disp(disp, &docpt));

    /* handled */
    return TRUE;
}

/*
 *   Set the cursor based on the display item the mouse is positioned upon.  
 */
void CHtmlSysWin_win32::set_disp_item_cursor(CHtmlDisp *disp,
                                             int docx, int docy)
{
    /* check what type of cursor the display item wants */
    switch (disp->get_cursor_type(this, formatter_, docx, docy,
                                  !HTMLW32_MORE_ALLOW_LINKS && more_mode_))
    {
    case HTML_CSRTYPE_IBEAM:
        /* set the cursor to the I-Beam */
        SetCursor(ibeam_csr_);
        break;
        
    case HTML_CSRTYPE_HAND:
        /* set the hand cursor */
        SetCursor(hand_csr_);
        break;

    case HTML_CSRTYPE_ARROW:
    default:
        /* use default cursor */
        SetCursor(arrow_cursor_);
        break;
    }
}

/*
 *   Get the link, if any, for a given display item and mouse position.  
 */
CHtmlDispLink *CHtmlSysWin_win32::find_link_for_disp(
    CHtmlDisp *disp, const CHtmlPoint *pt)
{
    CHtmlDispLink *link;

    /* if there's no display item, there's obviously no link */
    if (disp == 0)
        return 0;

    /* get the link, if any, from the display item */
    link = disp->get_link(formatter_, pt->x, pt->y);

    /* if there's no link, there's nothing more to find out */
    if (link == 0)
        return 0;

    /* 
     *   If links are disabled in the preferences, and this link doesn't have
     *   the "forced" style, ignore the link.  
     */
    if (!get_html_show_links() && !link->is_forced())
        return 0;

    /* 
     *   if we're in MORE mode, and links are disabled in MORE mode, forget
     *   the link 
     */
    if (more_mode_ && !HTMLW32_MORE_ALLOW_LINKS)
        return 0;

    /* return the link */
    return link;
}

/*
 *   Process a mouse click 
 */
int CHtmlSysWin_win32::do_leftbtn_down(int keys, int x, int y, int clicks)
{
    RECT rc;
    CHtmlDisp *disp;
    CHtmlPoint pos;
    unsigned long curofs, oldofs;
    unsigned long startofs, endofs;
    unsigned long startofs2, endofs2;
    CHtmlDispLink *link;

    /* if I don't already have the focus, set focus here */
    take_focus();

    /*
     *   When the tooltip control sees a mouse click, it turns itself off
     *   until the mouse moves out of the tool.  We don't want this to
     *   happen, because we're taking over the whole window for the tool.
     *   To get the tooltip to keep popping up after a click, deactivate
     *   it and then immediately reactivate it.  
     */
    SendMessage(tooltip_, TTM_ACTIVATE, FALSE, 0);
    SendMessage(tooltip_, TTM_ACTIVATE, TRUE, 0);

    /* presume we won't defer clearing the selection */
    clear_sel_pending_ = FALSE;

    /*
     *   if it's in the vertical scrollbar, ignore it; we can get these
     *   events when the scrollbar is disabled, since it passes them
     *   through to the parent window 
     */
    get_scroll_area(&rc, TRUE);
    if (x >= rc.right)
        return FALSE;

    /* clear the selection ranges in other subwindows */
    owner_->clear_other_sel(this);

    /*
     *   if we're in "MORE" mode, and they clicked on the "MORE" prompt,
     *   show them more text 
     */
    if (!prefs_->get_alt_more_style() && more_mode_)
    {
        RECT rc;
        POINT pt;

        /* get the area of the "MORE" prompt */
        get_moreprompt_rect(&rc);

        /* see if the click is within the "MORE" prompt */
        pt.x = x;
        pt.y = y;
        if (PtInRect(&rc, pt))
        {
            /* it's in the prompt area - release the "MORE" mode */
            release_more_mode(HTMLW32_MORE_PAGE);

            /* we've handled the mouse event */
            return TRUE;
        }
    }

    /* get the position in document coordinates */
    pos = screen_to_doc(CHtmlPoint(x, y));

    /* get the display item containing the position */
    disp = formatter_->find_by_pos(pos, TRUE);

    /* get the link from the display item, if there is one */
    link = find_link_for_disp(disp, &pos);

    /* if we found a link, track the link click */
    if (link != 0 && link->is_clickable_link())
    {
        /* draw all of the items involved in the link in the hilited state */
        link->set_clicked(this, CHtmlDispLink_clicked);
        UpdateWindow(handle_);

        /* remember the link that we're tracking */
        track_link_ = link;

        /* remove any selection we have */
        cmdbuf_->set_sel_range(last_cmd_caret_, last_cmd_caret_,
                               last_cmd_caret_);
        update_caret_pos(FALSE, TRUE);
    }
    else
    {
        /*
         *   We're not clicking on a clickable link - treat this as a text
         *   range selection click 
         */

        /* get the text offset of this position */
        curofs = formatter_->find_textofs_by_pos(pos);
        
        /* see if we're extending the selection */
        if (keys & MK_SHIFT)
        {
            /*
             *   extend the old selection -- if the caret was at the right
             *   of the old selection, extend from the left of the old
             *   selection, otherwise extend from the right of the old
             *   selection 
             */
            formatter_->get_sel_range(&startofs, &endofs);
            if (caret_at_right_)
            {
                oldofs = startofs;
                endofs = curofs;
            }
            else
            {
                oldofs = endofs;
                startofs = curofs;
            }
        }
        else
        {
            /* we're starting a new selection */
            startofs = endofs = oldofs = curofs;

            /* start off with forward drag direction */
            caret_at_right_ = TRUE;
        }

        /*
         *   If this is the first click, and we're inside the selection,
         *   don't immediately reset the selection range, since they may
         *   want to drag or right-click the range.  
         */
        if (clicks <= 1)
        {
            unsigned long cur_start, cur_end;

            /* see if we're inside the selection */
            formatter_->get_sel_range(&cur_start, &cur_end);
            if (curofs >= cur_start && curofs < cur_end)
            {
                /* 
                 *   set 'clicks' to zero to indicate that we don't want
                 *   to clear the selection just yet, but also set the
                 *   clear_sel_pending_ flag so that, on mouse-up, if we
                 *   never ended up doing anything else interesting, we'll
                 *   still end up clearing the selection 
                 */
                clicks = 0;
                clear_sel_pending_ = TRUE;
            }
            else
            {
                /* we're outside the range - reset the selection */
                clicks = 1;
            }
        }

        /* if we have any clicks, select the initial range */
        if (clicks != 0)
        {
            /*
             *   select the range, then read it back to make sure we have
             *   the canonical ordering 
             */
            formatter_->set_sel_range(startofs, endofs);
            formatter_->get_sel_range(&startofs, &endofs);
        }

        /* remember the number of clicks */
        track_clicks_ = clicks;

        /* if we have a multiple click, select a larger region */
        switch(clicks)
        {
        case 0:
            /* we may be starting a drag operation */
            drag_prepare(TRUE);
            break;
            
        case 2:
            /* double click - select a word at each end of the range */
            formatter_->get_word_limits(&startofs2, &endofs2, startofs);
            formatter_->get_word_limits(&startofs, &endofs, endofs);

            /* set the ranges */
            goto set_ranges;

        case 3:
            /* triple click - select a line at each end of the range */
            formatter_->get_line_limits(&startofs2, &endofs2, startofs);
            formatter_->get_line_limits(&startofs, &endofs, endofs);

        set_ranges:
            /* set the maximum extent of the two ranges */
            startofs = (startofs < startofs2 ? startofs : startofs2);
            endofs = (endofs > endofs2 ? endofs : endofs2);
            formatter_->set_sel_range(startofs, endofs);
            break;

        case 4:
            /* quadruple click - select everything */
            formatter_->set_sel_range(0, formatter_->get_text_ofs_max());
            break;
        }

        /* set the appropriate caret direction */
        caret_at_right_ = (curofs >= oldofs);

        /* update the command selection range from the formatter range */
        fmt_sel_to_cmd_sel(caret_at_right_);

        /* remember the tracking offsets */
        track_start_left_ = startofs;
        track_start_right_ = endofs;
    }

    /* remember that we're tracking the mouse, and where we started */
    tracking_mouse_ = TRUE;
    track_start_ = pos;

    /* hide the caret during tracking */
    modal_hide_caret();

    /* capture the mouse while dragging */
    SetCapture(handle_);

    /* prepare for drag scrolling */
    start_drag_scroll();

    /* handled */
    return TRUE;
}


/*
 *   Process mouse movement 
 */
int CHtmlSysWin_win32::do_mousemove(int /*keys*/, int x, int y)
{
    /* 
     *   If the tooltip just disappeared, reset it - we need to do this
     *   because we have only one big tool area covering our whole window,
     *   even though we may have numerous effective tool areas.  When the
     *   tool window times out, the control will normally not show a tip
     *   again until the mouse moves into a new tool; since we don't have
     *   any other tools for it to move into, it would never reappear
     *   until the mouse moved out of the window if we didn't reset it.  
     */
    if (tooltip_vanished_)
    {
        /* power-cycle the control */
        SendMessage(tooltip_, TTM_ACTIVATE, FALSE, 0);
        SendMessage(tooltip_, TTM_ACTIVATE, TRUE, 0);

        /* only do this once per vanishing */
        tooltip_vanished_ = FALSE;
    }
    
    /* if we're tracking the mouse, keep doing so */
    if (tracking_mouse_)
    {
        CHtmlPoint newpos;
        unsigned long curofs;
        unsigned long oldofs;
        unsigned long startofs;
        unsigned long endofs;
        int caret_at_right;

        /* check for a drag/drop operation */
        if (drag_check())
        {
            /* 
             *   don't clear the selection when we're done, since the
             *   click ended up meaning something different from making a
             *   selection 
             */
            clear_sel_pending_ = FALSE;
            
            /* we're done */
            return TRUE;
        }

        /* do drag scrolling if appropriate */
        maybe_drag_scroll(x, y, 0, 0);

        /* get the mouse position in document coordinates */
        newpos = screen_to_doc(CHtmlPoint(x, y));

        /* see what kind of event we're tracking */
        if (track_link_ != 0)
        {
            CHtmlDisp *disp;
            CHtmlDispLink *link;
            int new_clicked;

            /*
             *   tracking a click on a linked item 
             */

            /* 
             *   See if we're still over an object linked to this item,
             *   and update the clicked status of the link accordingly.  
             */
            disp = formatter_->find_by_pos(newpos, TRUE);
            link = (disp != 0 ?
                    disp->get_link(formatter_, newpos.x, newpos.y) : 0);
            new_clicked = (link == track_link_);
            if ((new_clicked & CHtmlDispLink_clicked) !=
                (track_link_->get_clicked() & CHtmlDispLink_clicked))
            {
                /* status has changed - update the window immediately */
                track_link_->set_clicked(this, (new_clicked
                                                ? CHtmlDispLink_clicked
                                                : CHtmlDispLink_clickedoff));
                UpdateWindow(handle_);
            }
        }
        else
        {
            /*
             *   tracking text selection 
             */

            /* get the text offset of the mouse position */
            curofs = formatter_->find_textofs_by_pos(newpos);
            
            /* extend the selection */
            switch(track_clicks_)
            {
            case 0:
                /* leave the selection alone for now */
                return TRUE;
                
            default:
            case 1:
                /* extend the range to the current point */
                startofs = (caret_at_right_ ? track_start_left_
                          : track_start_right_);
                formatter_->set_sel_range(startofs, curofs);

                /* determine which end has the caret */
                caret_at_right = (curofs >= startofs);
                break;

            case 2:
                /* get the current word limits */
                formatter_->get_word_limits(&startofs, &endofs, curofs);

                /* extend the limits */
                goto extend_limits;

            case 3:
                /* get the current line limits */
                formatter_->get_line_limits(&startofs, &endofs, curofs);

            extend_limits:
                /*
                 *   extend the selection to enclose the lines at both ends
                 *   of the mouse range 
                 */
                oldofs = (caret_at_right_ ? track_start_left_
                          : track_start_right_);
                if (endofs > oldofs)
                {
                    startofs = oldofs;
                    caret_at_right = TRUE;
                }
                else
                {
                    endofs = oldofs;
                    caret_at_right = FALSE;
                }
                formatter_->set_sel_range(startofs, endofs);
                break;

            case 4:
                /* we've selected everything - leave the range alone */
                caret_at_right = TRUE;
                break;
            }

            /* remember the new positions as the new track starts */
            formatter_->get_sel_range(&track_start_left_,
                                      &track_start_right_);

            /* update the command selection range from the formatter range */
            fmt_sel_to_cmd_sel(caret_at_right);
        }

        /* handled */
        return TRUE;
    }

    /* we didn't handle it */
    return FALSE;
}

/*
 *   Process capture loss 
 */
int CHtmlSysWin_win32::do_capture_changed(HWND)
{
    /* terminate any tracking we're doing */
    return end_mouse_tracking(HTML_TRACK_CANCEL);
}

/*
 *   Process mouse left key release 
 */
int CHtmlSysWin_win32::do_leftbtn_up(int /*keys*/, int /*x*/, int /*y*/)
{
    int ret;
    
    /* finish tracking the mouse */
    ret = end_mouse_tracking(HTML_TRACK_LEFT);

    /* send focus back to the main HTML panel, if appropriate */
    owner_->focus_to_main();

    /* return the result from end_mouse_tracking */
    return ret;
}

/*
 *   Terminate tracking of the mouse 
 */
int CHtmlSysWin_win32::end_mouse_tracking(html_track_mode_t mode)
{
    /* check to see if we're tracking a button click-and-drag */
    if (tracking_mouse_)
    {
        unsigned long startofs, endofs;
        
        /* done with drag scrolling */
        end_drag_scroll();
        
        /* forget that we were tracking */
        tracking_mouse_ = FALSE;

        /* release mouse capture if capture wasn't canceled for us */
        if (mode != HTML_TRACK_CANCEL)
            ReleaseCapture();

        /* terminate drag/drop tracking if necessary */
        drag_end(TRUE);

        /* show the caret again */
        modal_show_caret();

        /* see what kind of tracking we were doing */
        if (track_link_ != 0 && mode == HTML_TRACK_LEFT)
        {
            const textchar_t *cmd;
            int append, enter;

            /*
             *   tracking a left-click on a linked item 
             */

            /* presume we won't need to execute any command */
            cmd = 0;

            /* 
             *   If (when last we checked) we were still over an object
             *   linked to this item, activate the item's HREF 
             */
            if ((track_link_->get_clicked() & CHtmlDispLink_clicked) != 0)
            {
                /* release any windows currently in MORE mode */
                owner_->release_all_moremode();

                /* 
                 *   It was still clicked - get the HREF, which we'll
                 *   process as a command.  Note that we do NOT want to
                 *   process it right away, because we want to finish up
                 *   our processing of the link tracking first -
                 *   processing the command could potentially clear the
                 *   screen, which would delete the link object, so we
                 *   must wait until after we've forgotten about the link
                 *   (in just a few lines) to process the command. 
                 */
                cmd = track_link_->href_.get_url();

                /* get the command attributes from the link */
                append = track_link_->get_append();
                enter = !track_link_->get_noenter();
            }

            /* 
             *   the mouse button has been released, so the item is back in
             *   'hover' mode now 
             */
            track_link_->set_clicked(this, CHtmlDispLink_hover);

            /* we're done tracking the link - forget about it */
            track_link_ = 0;

            /*
             *   If we found a command to process, process it now.  We
             *   wait until now, because we had to finish our work with
             *   the link object before we process its command, in case
             *   processing the command clears the screen and thereby
             *   deletes the link object.  
             */
            if (cmd != 0)
                process_command(cmd, get_strlen(cmd),
                                append, enter, OS_CMD_NONE);
        }
        else
        {
            /*
             *   Tracking text selection range
             */

            /* 
             *   If we were tracking "zero" clicks, it means that we
             *   clicked over the selection range; if the click never
             *   meant anything else (such as drag/drop or context menu),
             *   clear the selection now.  Note that we ignore this if
             *   capture is being forcibly rested from us, because it
             *   means that whatever was going on was somehow interrupted,
             *   hance the user didn't mean to change the selection.
             */
            if (clear_sel_pending_ && mode != HTML_TRACK_CANCEL)
            {
                /* set the new selection range */
                formatter_->set_sel_range(track_start_left_,
                                          track_start_right_);

                /* update the command selection */
                fmt_sel_to_cmd_sel(TRUE);
            }
            
            /*
             *   If we have a zero-length selection, and the selection
             *   isn't in the command line, move the selection back to the
             *   command line.  We don't show the caret outside of the
             *   command line, so there would be no visual feedback for
             *   the selection point in this case; plus, the user can't do
             *   anything with a zero-length selection outside the command
             *   line.  
             */
            if (cmdtag_ != 0)
            {
                /* check the selection to see if it's zero-length */
                formatter_->get_sel_range(&startofs, &endofs);
                if (startofs == endofs && endofs < cmdtag_->get_text_ofs())
                {
                    /* no selection - move back to the command line */
                    cmdbuf_->set_sel_range(last_cmd_caret_, last_cmd_caret_,
                                           last_cmd_caret_);
                    update_caret_pos(FALSE, TRUE);
                }
            }
            else
            {
                /* 
                 *   no command - if we have a caret, set its position to
                 *   the appropriate end of the new selection range 
                 */
                update_caret_pos(FALSE, TRUE);
            }
        }

        /* handled */
        return TRUE;
    }

    /* didn't handle it */
    return FALSE;
}

/*
 *   Pre-drag notification
 */
void CHtmlSysWin_win32::drag_pre()
{
    /* leave the current selection alone when the mouse is released */
    clear_sel_pending_ = FALSE;

    /* hide the caret while we're dragging */
    modal_hide_caret();
}

/*
 *   Post-drag notification 
 */
void CHtmlSysWin_win32::drag_post()
{
    /* show the caret again */
    modal_show_caret();
}

/*
 *   Right button down event 
 */
int CHtmlSysWin_win32::do_rightbtn_down(int keys, int x, int y,
                                        int /*clicks*/)
{
    /* 
     *   Do normal left button tracking - when the button is released,
     *   we'll display the context menu appropriate for what we're doing
     *   after the tracking.  Note that we pass a click count of zero; the
     *   left-click routine treats this as a single click but doesn't
     *   clear the current selection range until we start moving the
     *   mouse, so if they release the button before moving the mouse
     *   they'll get the current range.  
     */
    return do_leftbtn_down(keys, x, y, 0);
}

/*
 *   Right button up event 
 */
int CHtmlSysWin_win32::do_rightbtn_up(int keys, int x, int y)
{
    /* don't clear the selection */
    clear_sel_pending_ = FALSE;
    
    /* finish the normal button tracking */
    end_mouse_tracking(HTML_TRACK_RIGHT);

    /* run the edit context menu */
    track_context_menu(edit_popup_, x, y);

    /* return focus to the main window if appropriate */
    owner_->focus_to_main();

    /* handled */
    return TRUE;
}
    
/*
 *   Is the given command menu event enabled?  This applies to the system
 *   menu commands that we can send to the game via the command line - SAVE,
 *   RESTORE, QUIT, etc.
 *   
 *   A command of this type is enabled if we're reading a command line (such
 *   as via os_gets()), and this specific command is enabled for command-line
 *   return, and the game isn't paused in the debugger. 
 */
int CHtmlSysWin_win32::is_cmd_evt_enabled(int os_cmd_id) const
{
    /* if we're paused in the debugger, this command is disabled */
    if (owner_->get_game_paused())
        return FALSE;

    /* 
     *   if we're reading a command line, check to see if the command is
     *   enabled for command-line use 
     */
    if (started_reading_cmd_)
        return oss_is_cmd_event_enabled(os_cmd_id, OSCS_GTS_ON);

    /* in other contexts, these commands are disabled */
    return FALSE;
}

/*
 *   Translate a menu command (one of our ID_xxx_yyy command codes) to an
 *   os_cmd_id code. 
 */
int CHtmlSysWin_win32::cmd_id_to_os_cmd_id(int command_id)
{
    /* translate the menu command ID to an OS_CMD_xxx code */
    switch (command_id)
    {
    case ID_FILE_QUIT:
        return OS_CMD_QUIT;

    case ID_CMD_UNDO:
        return OS_CMD_UNDO;

    case ID_FILE_SAVEGAME:
        return OS_CMD_SAVE;

    case ID_FILE_RESTOREGAME:
        return OS_CMD_RESTORE;

    case ID_HELP_COMMAND:
        return OS_CMD_HELP;

    default:
        return OS_CMD_NONE;
    }
}

/*
 *   check a command to determine its current state 
 */
TadsCmdStat_t CHtmlSysWin_win32::check_command(int command_id)
{
    int paused;

    /* if we're paused in the debugger, disallow certain commands */
    paused = owner_->get_game_paused();

    /* check the command */
    switch(command_id)
    {
    case ID_FILE_QUIT:
    case ID_CMD_UNDO:
    case ID_FILE_SAVEGAME:
    case ID_FILE_RESTOREGAME:
    case ID_HELP_COMMAND:
        /*
         *   We send these commands to the game via the command line, if
         *   we're reading a command line.  We can also send them to the game
         *   via os_get_event() as OS_EVT_COMMAND events, if they're enabled.
         *   So, check which context we're in, and check to see if the
         *   command is enabled for that context.  Note that we can't return
         *   any of these events if we're paused in the debugger.  
         */
        return is_cmd_evt_enabled(cmd_id_to_os_cmd_id(command_id))
            ? TADSCMD_ENABLED : TADSCMD_DISABLED;

    case ID_EDIT_CUT:
        return can_cut() ? TADSCMD_ENABLED : TADSCMD_DISABLED;

    case ID_EDIT_COPY:
        return can_copy() ? TADSCMD_ENABLED : TADSCMD_DISABLED;

    case ID_EDIT_PASTE:
        return can_paste() ? TADSCMD_ENABLED : TADSCMD_DISABLED;

    case ID_EDIT_DELETE:
        return can_cut() ? TADSCMD_ENABLED : TADSCMD_DISABLED;

    case ID_EDIT_FIND:
    case ID_EDIT_FINDNEXT:
        return (get_formatter()->can_search_for_text()
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_EDIT_SELECTALL:
        return TADSCMD_ENABLED;

    case ID_EDIT_SELECTLINE:
        return started_reading_cmd_ ? TADSCMD_ENABLED : TADSCMD_DISABLED;

    case ID_EDIT_UNDO:
        return cmdbuf_->can_undo() && !paused
            ? TADSCMD_ENABLED : TADSCMD_DISABLED;

    case ID_MUTE_SOUND:
        return prefs_->get_mute_sound() ? TADSCMD_CHECKED : TADSCMD_ENABLED;

    case ID_VIEW_FONTS_SMALLEST:
        return prefs_->get_font_scale() == 0
            ? TADSCMD_CHECKED : TADSCMD_ENABLED;

    case ID_VIEW_FONTS_SMALLER:
        return prefs_->get_font_scale() == 1
            ? TADSCMD_CHECKED : TADSCMD_ENABLED;

    case ID_VIEW_FONTS_MEDIUM:
        return prefs_->get_font_scale() == 2
            ? TADSCMD_CHECKED : TADSCMD_ENABLED;

    case ID_VIEW_FONTS_LARGER:
        return prefs_->get_font_scale() == 3
            ? TADSCMD_CHECKED : TADSCMD_ENABLED;

    case ID_VIEW_FONTS_LARGEST:
        return prefs_->get_font_scale() == 4
            ? TADSCMD_CHECKED : TADSCMD_ENABLED;

    case ID_VIEW_FONTS_NEXT_SMALLER:
        return prefs_->get_font_scale() == 0
            ? TADSCMD_DISABLED : TADSCMD_ENABLED;

    case ID_VIEW_FONTS_NEXT_LARGER:
        return prefs_->get_font_scale() == 4
            ? TADSCMD_DISABLED : TADSCMD_ENABLED;

    case ID_VIEW_OPTIONS:
    case ID_MANAGE_PROFILES:
    case ID_SET_DEF_PROFILE:
        return TADSCMD_ENABLED;

    case ID_APPEARANCE_OPTIONS:
    case ID_THEMES_DROPDOWN:
        return TADSCMD_ENABLED;

    case ID_GO_NEXT:
        return owner_->exists_next_page()
            ? TADSCMD_ENABLED : TADSCMD_DISABLED;

    case ID_GO_PREVIOUS:
        return owner_->exists_previous_page()
            ? TADSCMD_ENABLED : TADSCMD_DISABLED;

    default:
        /* not known to us; inherit default handling */
        return CTadsWinScroll::check_command(command_id);
    }
}

/*
 *   Handle a command 
 */
int CHtmlSysWin_win32::do_command(int notify_code,
                                  int command_id, HWND ctl)
{
    /* 
     *   cancel any pending temporary highlighting, in case they activated
     *   the command through a keyboard accelerator 
     */
    set_temp_show_links(FALSE);

    /* check the command code for one we recognize */
    switch(command_id)
    {
    case ID_FILE_QUIT:
        /* send a "QUIT" command */
        process_command("quit", 4, FALSE, TRUE, OS_CMD_QUIT);
        return TRUE;

    case ID_CMD_UNDO:
        process_command("undo", 4, FALSE, TRUE, OS_CMD_UNDO);
        return TRUE;

    case ID_FILE_SAVEGAME:
        process_command("save", 4, FALSE, TRUE, OS_CMD_SAVE);
        return TRUE;
        
    case ID_FILE_RESTOREGAME:
        process_command("restore", 7, FALSE, TRUE, OS_CMD_RESTORE);
        return TRUE;

    case ID_EDIT_FIND:
    case ID_EDIT_FINDNEXT:
        /* process the search */
        do_find(command_id);
        return TRUE;

    case ID_EDIT_CUT:
        if (can_cut())
            do_cut();
        return TRUE;

    case ID_EDIT_COPY:
        if (can_copy())
            do_copy();
        return TRUE;

    case ID_EDIT_PASTE:
        if (can_paste())
            do_paste();
        return TRUE;

    case ID_EDIT_DELETE:
        if (can_cut() && cmdbuf_->del_right())
            update_input_display();
        return TRUE;

    case ID_EDIT_SELECTALL:
        /* clear selections in other subwindows */
        owner_->clear_other_sel(this);

        /* set the selection in the formatter and in the command buffer */
        formatter_->set_sel_range(0, formatter_->get_text_ofs_max());
        fmt_sel_to_cmd_sel(TRUE);

        /* make sure focus is in our window */
        take_focus();

        /* handled */
        return TRUE;

    case ID_EDIT_SELECTLINE:
        /* only proceed if we have a command */
        if (cmdtag_ != 0 && started_reading_cmd_)
        {
            /* clear selections in other subwindows */
            owner_->clear_other_sel(this);
            
            /* select from the start of the command to the end of the text */
            formatter_->set_sel_range(cmdtag_->get_text_ofs(),
                                      formatter_->get_text_ofs_max());

            /* update the selection and show the cursor */
            fmt_sel_to_cmd_sel(TRUE);
            update_caret_pos(TRUE, FALSE);

            /* make sure focus is in our window */
            take_focus();

            /* handled */
            return TRUE;
        }
        break;

    case ID_EDIT_UNDO:
        if (cmdbuf_->can_undo() && cmdbuf_->undo())
            update_input_display();
        return TRUE;

    case ID_VIEW_FONTS_SMALLEST:
        set_font_scale(0);
        return TRUE;

    case ID_VIEW_FONTS_SMALLER:
        set_font_scale(1);
        return TRUE;

    case ID_VIEW_FONTS_MEDIUM:
        set_font_scale(2);
        return TRUE;

    case ID_VIEW_FONTS_LARGER:
        set_font_scale(3);
        return TRUE;

    case ID_VIEW_FONTS_LARGEST:
        set_font_scale(4);
        return TRUE;

    case ID_VIEW_FONTS_NEXT_SMALLER:
        if (prefs_->get_font_scale() != 0)
            set_font_scale(prefs_->get_font_scale() - 1);
        return TRUE;

    case ID_VIEW_FONTS_NEXT_LARGER:
        if (prefs_->get_font_scale() != 4)
            set_font_scale(prefs_->get_font_scale() + 1);
        return TRUE;

    case ID_VIEW_OPTIONS:
        /* run the preferences dialog */
        prefs_->run_preferences_dlg(owner_->get_owner_frame_handle(), this);

        /* set the accelerator table for the Ctrl+V preferences */
        set_current_accel();

        /* handled */
        return TRUE;

    case ID_MANAGE_PROFILES:
        /* run the profiles dialog */
        prefs_->run_profiles_dlg(owner_->get_owner_frame_handle(), this);

        /* handled */
        return TRUE;

    case ID_SET_DEF_PROFILE:
        /* set the default profile to the active profile */
        prefs_->set_default_profile(prefs_->get_active_profile_name());

        /* handled */
        return TRUE;

    case ID_APPEARANCE_OPTIONS:
    case ID_THEMES_DROPDOWN:
        /* run the visual settings dialog */
        prefs_->run_appearance_dlg(owner_->get_owner_frame_handle(),
                                   this, TRUE);

        /* handled */
        return TRUE;

    case ID_MUTE_SOUND:
        /* update the preferences */
        prefs_->set_mute_sound(!prefs_->get_mute_sound());

        /* notify the registered sounds */
        mute_registered_sounds(prefs_->get_mute_sound());

        /* handled */
        return TRUE;

    case ID_GO_NEXT:
        /* tell the owner to switch to the next page */
        owner_->go_next_page();
        return TRUE;

    case ID_GO_PREVIOUS:
        /* tell the owner to switch to the previous page */
        owner_->go_previous_page();
        return TRUE;

    case ID_HELP_COMMAND:
        /* enter a "help" command to the game */
        process_command("help", 4, FALSE, TRUE, OS_CMD_HELP);
        return TRUE;
    }

    /* not handled */
    return FALSE;
}

/*
 *   Get the global sound muting status 
 */
int CHtmlSysWin_win32::get_mute_sound()
{
    /* get the setting from the preferences */
    return prefs_->get_mute_sound();
}

/*
 *   Mute/unmute the registered sounds 
 */
void CHtmlSysWin_win32::mute_registered_sounds(int mute)
{
    CTadsAudioPlayer **sndp;
    size_t i;

    /* notify each registered sound of the change */
    for (i = HTML_MAX_REG_SOUNDS, sndp = active_sounds_ ; i != 0 ;
         --i, ++sndp)
    {
        /* if this slot contains a valid sound object, notify it */
        if (*sndp != 0)
            (*sndp)->on_mute_change(mute);
    }
}

/*
 *   Register an active sound object 
 */
void CHtmlSysWin_win32::register_active_sound(CTadsAudioPlayer *s)
{
    CTadsAudioPlayer **sndp;
    size_t i;

    /* 
     *   Look for an empty slot.  If we don't find one, it's no big deal; it
     *   just means that we won't be able to send notifications to the sound
     *   object. 
     */
    for (i = HTML_MAX_REG_SOUNDS, sndp = active_sounds_ ; i != 0 ;
         --i, ++sndp)
    {
        /* if this slot is empty, take it */
        if (*sndp == 0)
        {
            /* 
             *   Put this sound in the slot.  Note that we don't have to
             *   bother adding a reference to the sound or anything like
             *   that; the sound is responsible for keeping a reference on
             *   *us*, since we're the audio controller, and for deleting
             *   this registration when it's no longer needed.  The sound
             *   object is required to stay valid as long as it's registered
             *   with us.  
             */
            *sndp = s;

            /* done */
            return;
        }
    }
}

/*
 *   Unregister an active sound object 
 */
void CHtmlSysWin_win32::unregister_active_sound(CTadsAudioPlayer *s)
{
    CTadsAudioPlayer **sndp;
    size_t i;

    /* search for the slot containing this sound */
    for (i = HTML_MAX_REG_SOUNDS, sndp = active_sounds_ ; i != 0 ;
         --i, ++sndp)
    {
        /* if this slot contains this object, clear the slot */
        if (*sndp == s)
        {
            /* this is it - clear the slot, and we're done */
            *sndp = 0;
            return;
        }
    }
}

/*
 *   process a search request 
 */
void CHtmlSysWin_win32::do_find(int command_id)
{
    const textchar_t *findstr;
    unsigned long sel_start, sel_end, start_ofs;
    HCURSOR oldcsr;
    int found;
    int exact_case;
    int start_at_top;
    int wrap;
    int dir;

    /* get the text to find */
    findstr = owner_->get_find_text(command_id, &exact_case,
                                    &start_at_top, &wrap, &dir);

    /* if they cancelled, there's nothing more to do */
    if (findstr == 0)
        return;

    /* get the starting position */
    find_get_start(start_at_top, &sel_start, &sel_end);

    /* show the busy cursor while working */
    oldcsr = SetCursor(wait_cursor_);

    /* 
     *   if we're searching forward, start at the end of the current
     *   selection range; otherwise, start at the start of the current range 
     */
    start_ofs = (dir == 1 ? sel_end : sel_start);

    /* 
     *   keep going until we find a range with a non-empty screen presence,
     *   or run out of matches 
     */
    for (;;)
    {
        CHtmlPoint pt_start;
        CHtmlPoint pt_end;

        /* 
         *   search in the text array, starting at the end of the selection
         *   range 
         */
        found = get_formatter()->search_for_text(
            findstr, get_strlen(findstr), exact_case,
            wrap, dir, start_ofs, &sel_start, &sel_end);

        /* if we didn't find it, display a message and stop looking */
        if (!found)
        {
            /* done working; restore the normal cursor */
            SetCursor(oldcsr);

            /* display a message box telling the user that we failed */
            find_not_found();

            /* we're done */
            break;
        }
        
        /* select the target that we found */
        get_formatter()->set_sel_range(sel_start, sel_end);

        /* get the display positions of the start and end of the match */
        pt_start = get_formatter()->get_text_pos(sel_start);
        pt_end = get_formatter()->get_text_pos(sel_end);

        /* 
         *   If the two points don't differ, this match doesn't have any
         *   screen presence, so it must be something hidden, such as text
         *   in a banner; if this is the case, skip it, since it's not a
         *   match the user can see and hence isn't a match the user is
         *   interested in.  If the two points don't match, we have a match
         *   we can show the user, so show it and stop looking.  
         */
        if (pt_start.x != pt_end.x || pt_start.y != pt_end.y)
        {
            /* update the command selection */
            fmt_sel_to_cmd_sel(TRUE);
            
            /* bring the caret position into sync with the new selection */
            update_caret_pos(TRUE, FALSE);
            
            /* 
             *   make sure the selection is in view, even if we don't have a
             *   caret 
             */
            scroll_to_show_selection();
            
            /* done working; restore the normal cursor */
            SetCursor(oldcsr);
            
            /* we have a match - we're done */
            break;
        }

        /* 
         *   we found a match that we don't like - continue searching from
         *   the end of the match text (or from the beginning, if we're
         *   going backwards) 
         */
        start_ofs = (dir == 1 ? sel_end : sel_start);
    }
}

/*
 *   get the starting position for a 'find' command 
 */
void CHtmlSysWin_win32::find_get_start(int start_at_top,
                                       unsigned long *sel_start,
                                       unsigned long *sel_end)
{
    /* 
     *   If we're starting at the top, search from offset zero; otherwise,
     *   get the current selection range and search from the end of the
     *   range.  
     */
    if (start_at_top)
        *sel_start = *sel_end = 0;
    else
        get_formatter()->get_sel_range(sel_start, sel_end);
}

/*
 *   show the text-not-found message for a 'find' command 
 */
void CHtmlSysWin_win32::find_not_found()
{
    char buf[256];
    
    /* display a message box telling the user that we failed */
    LoadString(CTadsApp::get_app()->get_instance(), IDS_FIND_NO_MORE,
               buf, sizeof(buf));
    MessageBox(0, buf, "TADS", MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);
}

/*
 *   Process a keystroke in MORE mode.  If we process the keystroke, we'll
 *   return true, otherwise we'll return false.  'vkey' is the virtual key,
 *   if this is a key-down event; if 'vkey' is zero, it means that this is a
 *   character event, in which case 'ch' has the ASCII character code.  
 */
int CHtmlSysWin_win32::process_moremode_key(int vkey, TCHAR ch)
{
    /* if we're in MORE mode, handle only scrolling keys */
    if (more_mode_)
    {
        /* treat ^V as a page-down key */
        if (ch == 22)
            vkey = VK_NEXT, ch = 0;

        /* 
         *   if we have a MORE prompt at the bottom, the cursor-down and
         *   page-down keys are special: they act like the Return and Space
         *   keys, respectively, releasing [more] mode to allow another line
         *   or page to be displayed` 
         */
        if (moreprompt_is_at_bottom())
        {
            /* apply the special translations */
            if (vkey == VK_DOWN)
                vkey = 0, ch = 13;
            else if (vkey == VK_NEXT)
                vkey = 0, ch = ' ';
        }

        /* 
         *   if we have a regular ASCII key, check for special [more]-mode
         *   meanings 
         */
        if (vkey == 0)
        {
            /* check the ASCII key */
            switch (ch)
            {
            case ' ':
                /* space - show the next screen */
                release_more_mode(HTMLW32_MORE_PAGE);
                break;

            case 10:
            case 13:
                /* return/enter - show one more line */
                release_more_mode(HTMLW32_MORE_LINE);
                break;

            case 3:
                /* ^C - allow copying when in [more] mode */
                do_copy();
                break;

            default:
                /* ignore other keys */
                break;
            }
        }
        else
        {
            /* handle everything else as a scrolling key */
            process_scrolling_key(vkey);
        }

        /* we've processed the key */
        return TRUE;
    }

    /* we didn't process the key */
    return FALSE;
}

/*
 *   Process a scrolling key 
 */
int CHtmlSysWin_win32::process_scrolling_key(int vkey)
{
    /* check the virtual key ID */
    switch (vkey)
    {
    case VK_UP:
        /* scroll up a line */
        do_scroll(TRUE, vscroll_, SB_LINEUP, 0, FALSE);
        return TRUE;

    case VK_DOWN:
        /* scroll down a line */
        do_scroll(TRUE, vscroll_, SB_LINEDOWN, 0, FALSE);
        return TRUE;

    case VK_PRIOR:
        /* scroll up a page */
        do_scroll(TRUE, vscroll_, SB_PAGEUP, 0, FALSE);
        return TRUE;

    case VK_NEXT:
        /* scroll down a page */
        do_scroll(TRUE, vscroll_, SB_PAGEDOWN, 0, FALSE);
        return TRUE;

    case VK_LEFT:
        /* scroll left */
        do_scroll(FALSE, hscroll_, SB_LINEUP, 0, FALSE);
        return TRUE;

    case VK_RIGHT:
        /* scroll right */
        do_scroll(FALSE, hscroll_, SB_LINEDOWN, 0, FALSE);
        return TRUE;

    case VK_END:
        /* scroll to bottom */
        do_scroll(TRUE, vscroll_, SB_BOTTOM, 0, FALSE);
        return TRUE;

    case VK_HOME:
        /* scroll to top */
        do_scroll(TRUE, vscroll_, SB_TOP, 0, FALSE);
        return TRUE;

    case VK_HELP:
    case VK_F1:
    case VK_INSERT:
    default:
        /* we don't handle any other keys */
        return FALSE;
    }
}

/*
 *   Process a keyboard virtual key event 
 */
int CHtmlSysWin_win32::do_keydown(int vkey, long /*keydata*/)
{
    /* 
     *   if it's not a control key, cancel any pending temporary link
     *   highlight mode 
     */
    switch(vkey)
    {
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
        /* turn on temporary links if appropriate */
        if (prefs_->get_links_ctrl() && temp_link_timer_id_ == 0)
        {
            /* 
             *   We want to make sure there's a slight delay before we
             *   show the links, to ensure that the screen doesn't flash
             *   annoyingly if the user only wants to activate an
             *   accelerator command.  Start a timer; if the key is still
             *   down when the timer goes off, we'll show the links at
             *   that point.  
             */
            temp_link_timer_id_ = alloc_timer_id();
            if (temp_link_timer_id_ != 0)
            {
                /* start the timer for a short delay */
                if (SetTimer(handle_, temp_link_timer_id_, 275,
                             (TIMERPROC)0) == 0)
                {
                    free_timer_id(temp_link_timer_id_);
                    temp_link_timer_id_ = 0;
                }
            }
        }
        return TRUE;

    default:
        /* cancel any pending highlighting */
        set_temp_show_links(FALSE);
        break;
    }

    /* check with the owner for special MORE-mode processing */
    if (owner_->owner_process_moremode_key(vkey, 0))
        return TRUE;

    /* process it as a scrolling key */
    return process_scrolling_key(vkey);
}

/*
 *   Process a key up event 
 */
int CHtmlSysWin_win32::do_keyup(int vkey, long keydata)
{
    switch(vkey)
    {
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
        /* turn off temporary links */
        set_temp_show_links(FALSE);
        return TRUE;

    default:
        /* inherit default processing */
        return CTadsWinScroll::do_keyup(vkey, keydata);
    }
}

/*
 *   Process an ALT key 
 */
int CHtmlSysWin_win32::do_syschar(TCHAR c, long /*keydata*/)
{
    /* check the key */
    switch(c)
    {
    case 'V':
    case 'v':
        /* 
         *   Alt-V - emacs-style command to scroll up a page; pass through
         *   to Windows if the emacs alt-V preference is turned off 
         */
        if (!prefs_->get_emacs_alt_v())
            return FALSE;

        /* scroll up a page */
        do_scroll(TRUE, vscroll_, SB_PAGEUP, 0, FALSE);
        break;

    default:
        /* this key was not processed */
        return FALSE;
    }

    /* if we got here, the key was processed */
    return TRUE;
}

/*
 *   user message handler 
 */
int CHtmlSysWin_win32::do_user_message(int msg, WPARAM wpar, LPARAM lpar)
{
    switch(msg)
    {
    case HTMLM_REFORMAT:
        /* reformat all of the windows */
        owner_->reformat_all_html((wpar & HTML_F_SHOW_STAT) != 0,
                                  (wpar & HTML_F_FREEZE_DISP) != 0,
                                  (wpar & HTML_F_RESET_SND) != 0);
        return TRUE;

    case HTMLM_ONRESIZE:
        /* reformat this window to adjust for resizing it */
        format_after_resize((long)lpar);
        return TRUE;

    case HTMLM_WAV_DONE:
        /* call the WAV object and tell it the wavefile is done */
        ((CHtmlSysSoundDigitized_win32 *)lpar)->on_sound_done((int)wpar);
        return TRUE;

    default:
        /* message not recognized */
        return FALSE;
    }
}

/*
 *   set the font scale 
 */
void CHtmlSysWin_win32::set_font_scale(int scale)
{
    CHtmlFontDesc font_desc;
    
    /* remember the new scale */
    prefs_->set_font_scale(scale);

    /* set a new default font */
    font_desc.htmlsize = 3;
    font_desc.default_charset = FALSE;
    font_desc.charset = owner_->owner_get_default_charset();
    default_font_ = (CHtmlSysFont_win32 *)get_font(&font_desc);

    /* reformat all of the windows */
    owner_->reformat_all_html(TRUE, TRUE, FALSE);

    /* reset the caret for the size change */
    set_caret_size(formatter_->get_font());
    update_caret_pos(FALSE, FALSE);
}


/*
 *   Process a keyboard character event 
 */
int CHtmlSysWin_win32::do_char(TCHAR c, long /*keydata*/)
{
    /* check with the owner for special MORE-mode processing */
    if (owner_->owner_process_moremode_key(0, c))
        return TRUE;

    /* see what we have */
    switch(c)
    {
    case 22:
        /* ^V - page down (Emacs mode) or Paste (Windows mode) */
        if (prefs_->get_emacs_ctrl_v())
        {
            /* emacs style - scroll down a page */
            do_scroll(TRUE, vscroll_, SB_PAGEDOWN, 0, FALSE);
            return TRUE;
        }
        break;

    case 3:
        /* ^C - copy */
        return do_copy();
    }

    /* we didn't handle the keystroke */
    return FALSE;
}


/*
 *   Turn "MORE" mode on or off 
 */
void CHtmlSysWin_win32::set_more_mode(int flag)
{
    /* if MORE mode isn't enabled, or the EOF flag is set, ignore it */
    if (flag && (!more_mode_enabled_ || eof_more_mode_))
        return;
    
    /* if it's not changing, ignore the request */
    if ((flag != 0) == (more_mode_ != 0))
        return;

    /* set the new mode */
    more_mode_ = flag;

    /* 
     *   if we're entering MORE mode, add myself to the owner list of windows
     *   awaiting MORE prompt responses; otherwise, remove myself from the
     *   owner's list of same 
     */
    if (flag)
        owner_->add_more_prompt_win(this);
    else
        owner_->remove_more_prompt_win(this);

    /* 
     *   if we're clearing MORE mode, make sure we exit any calling loop
     *   waiting for MORE mode to end 
     */
    released_more_ = TRUE;

    /* update the status line */
    if (statusline_ != 0)
        statusline_->update();

    /* make sure we redraw the prompt area */
    inval_more_area();
}

/*
 *   Wait for the user to acknowledge a MORE prompt displayed explicitly
 *   by the game. 
 */
void CHtmlSysWin_win32::do_game_more_prompt()
{
    int ret;
    int ht_increment;

    /* make sure the caret is positioned correctly */
    update_caret_pos(FALSE, FALSE);

    /* 
     *   Wait for the user to acknowledge any existing MORE prompt.  
     */
    while (more_mode_ && !eof_more_mode_)
    {
        /* 
         *   add a reference while we're processing events, since we need
         *   to keep my member variable (released_more_) around until the
         *   event loop ends 
         */
        AddRef();
        
        /* process events until we're no longer in "MORE" mode */
        released_more_ = FALSE;
        ret = CTadsApp::get_app()->event_loop(&released_more_);

        /* 
         *   If we encountered an EOF, return without further work.  Note
         *   that it's important that we check this before doing the
         *   Release(), because EOF is sometimes associated with the window
         *   closing, in which case the Release() could delete 'this' and
         *   make it impossible to check our member variables.  
         */
        if (eof_more_mode_)
        {
            /* release and return immediately */
            Release();
            return;
        }

        /* 
         *   release our self-reference - if this window is being closed,
         *   the event loop will have returned false, so we won't need to
         *   know about released_more_ any more 
         */
        Release();

        /* if we're quitting, give up now */
        if (ret == 0)
        {
            /* 
             *   post another quit message, in case there's an enclosing
             *   event loop - since we just ate this quit message, we need
             *   to send another one to the enclosing loop 
             */
            PostQuitMessage(0);
            return;
        }
    }

    /*
     *   If we're displaying an on-screen more prompt, temporarily
     *   increase the scrollable height of the text by the size of the
     *   more prompt, so that our more prompt doesn't overwrite the last
     *   part of the displayed text.  
     */
    if (!prefs_->get_alt_more_style())
    {
        RECT rc;

        /* get the height adjustment */
        get_moreprompt_rect(&rc);
        ht_increment = rc.bottom - rc.top + 2;

        /* adjust the formatter's total height */
        formatter_->inc_max_y_pos(ht_increment);

        /* make sure we're scrolled all the way down */
        fmt_adjust_vscroll();
    }
    else
        ht_increment = 0;

    /*
     *   Pretend that the last input position was at the top of the
     *   screen.  This is necessary because, when the more prompt is
     *   showing, we assume that we've reached the point where the last
     *   input position is about to scroll off the screen.  Since we're
     *   showing a more prompt manually here, we may have displayed less
     *   text than this, which will be a problem if we display a big pile
     *   of text *after* this manual more prompt, unless we adjust the
     *   input position to compensate. 
     */
    last_input_height_ = screen_to_doc_y(0);

    /* remember that we're processing a game "more" request */
    game_more_request_ = TRUE;

    /* make sure the caret is positioned correctly */
    update_caret_pos(FALSE, FALSE);

    /*
     *   Now display the MORE prompt one more time, and once again process
     *   events until the user acknowledges it. 
     */
    set_more_mode(TRUE);
    while (more_mode_ && !eof_more_mode_)
    {
        /* 
         *   add a self-reference while we're working, to ensure we can't be
         *   deleted until we're done here 
         */
        AddRef();

        /* process events until we're no longer in "MORE" mode */
        released_more_ = FALSE;
        ret = CTadsApp::get_app()->event_loop(&released_more_);

        /* if we encountered EOF, return immediately */
        if (eof_more_mode_)
        {
            /* release our self-reference and return */
            Release();
            return;
        }

        /* 
         *   Make sure the new input height doesn't exceed the actual height
         *   of the text displayed.  Since we could have paused after just a
         *   little text at the top of the screen, our normal page-forward
         *   advance could have taken us past the end of the actual text
         *   that's been displayed so far.  
         */
        if (last_input_height_ > (long)formatter_->get_max_y_pos())
        {
            RECT rc;
            long ht;

            /* set the height */
            last_input_height_ = (long)formatter_->get_max_y_pos();

            /* get the size of the "more" prompt area */
            get_moreprompt_rect(&rc);
            ht = rc.bottom - rc.top;

            /* 
             *   Back off from the maximum height by the prompt size, to
             *   ensure we have some context still on the screen the next
             *   time we stop.  We normally go by where the last input
             *   occurred, but this doesn't count as input in the normal
             *   sense since they didn't type anything.  So we want to leave
             *   a little extra of the text just before we stopped.  
             */
            if (last_input_height_ > ht * 2)
                last_input_height_ -= ht * 2;
            else
                last_input_height_ = 0;
        }

        /* release our self-reference */
        Release();

        /* if we're quitting, give up now */
        if (ret == 0)
        {
            /* post another quit message to the enclosing event loop */
            PostQuitMessage(0);

            /* done */
            return;
        }
    }

    /* done with the game more request */
    game_more_request_ = FALSE;

    /*
     *   If we temporarily increased the scrolling area size to make room
     *   for the prompt, we must now return things to their prior state. 
     */
    if (ht_increment != 0)
        formatter_->inc_max_y_pos(-ht_increment);
}

/*
 *   Set the exit-pause flag 
 */
void CHtmlSysWin_win32::set_exit_pause(int flag)
{
    /* note the flag */
    exit_pause_ = flag;

    /* hide the caret while in the exit pause */
    hide_caret();

    /* update the status line */
    if (statusline_ != 0)
        statusline_->update();
}

/*
 *   Mark this as a banner window (or not)
 */
void CHtmlSysWin_win32::set_is_banner_win(int is_banner,
                                          CHtmlSysWin_win32 *parent,
                                          int where, CHtmlSysWin_win32 *other,
                                          HTML_BannerWin_Type_t typ,
                                          HTML_BannerWin_Pos_t pos)
{
    /* remember the banner status */
    is_banner_win_ = is_banner;

    /* if it's a banner, set it to an initial size of zero */
    if (is_banner)
    {
        banner_width_ = 0;
        banner_width_units_ = HTML_BANNERWIN_UNITS_PIX;
        banner_height_ = 0;
        banner_height_units_ = HTML_BANNERWIN_UNITS_PIX;
    }

    /* remember the window typ */
    banner_type_ = typ;

    /* remember the alignment */
    banner_alignment_ = pos;

    /* remember our parent */
    banner_parent_= parent;

    /* create a window for the border */
    border_handle_ = CreateWindow("TADS.BannerBorder", "", WS_CHILD,
                                  0, 0, 1, 1, get_parent()->get_handle(), 0,
                                  CTadsApp::get_app()->get_instance(), 0);

    /* if there's a parent, link into the parent's child list */
    if (parent != 0)
        parent->add_banner_child(this, where, other);
}

/*
 *   Close this banner window.  This closes the system window displaying the
 *   banner.  
 */
void CHtmlSysWin_win32::close_banner_window()
{
    /* explicitly close my border window */
    if (border_handle_ != 0)
        SendMessage(border_handle_, WM_CLOSE, 0, 0);

    /* close my window handle */
    SendMessage(handle_, WM_CLOSE, 0, 0);
}

/*
 *   Add a child to my banner list 
 */
void CHtmlSysWin_win32::add_banner_child(CHtmlSysWin_win32 *chi,
                                         int where, CHtmlSysWin_win32 *other)
{
    CHtmlSysWin_win32 *cur;
    CHtmlSysWin_win32 *prv;

    /* add the child at the desired point in our child list */
    switch(where)
    {
    case OS_BANNER_FIRST:
        /* link at the head of my child list */
        prv = 0;
        cur = banner_first_child_;
        goto link_to_list;

    case OS_BANNER_LAST:
        /* 
         *   look for no entry at all in the parent's child list, so we find
         *   the last entry 
         */
        other = 0;
        goto find_other;

    case OS_BANNER_BEFORE:
    case OS_BANNER_AFTER:
    find_other:
        /* scan the list for 'other' */
        for (prv = 0, cur = banner_first_child_ ;
             cur != 0 && cur != other ;
             prv = cur, cur = cur->banner_next_) ;

        /* if we found it, and we're linking after it, move ahead one */
        if (cur != 0 && where == OS_BANNER_AFTER)
        {
            /* move ahead to the next element, so we link after 'cur' */
            prv = cur;
            cur = cur->banner_next_;
        }
        
    link_to_list:
        /* link the new child after 'prv' and before 'cur' */
        chi->banner_next_ = cur;
        if (prv == 0)
            banner_first_child_ = chi;
        else
            prv->banner_next_ = chi;
        
        /* done */
        break;
    }
}

/*
 *   Remove a banner from my child list 
 */
void CHtmlSysWin_win32::remove_banner_child(CHtmlSysWin_win32 *chi)
{
    CHtmlSysWin_win32 *cur;
    CHtmlSysWin_win32 *prv;
    
    /* scan my child list for the item to remove */
    for (prv = 0, cur = banner_first_child_ ; cur != 0 ;
         prv = cur, cur = cur->banner_next_)
    {
        /* if this is the one to remove, unlink it */
        if (cur == chi)
        {
            /* this is the one - unlink it */
            if (prv == 0)
                banner_first_child_ = cur->banner_next_;
            else
                prv->banner_next_ = cur->banner_next_;

            /* a child can only appear once in our list, so we're done */
            return;
        }
    }
}   

/*
 *   Receive notification that the parent banner window is being closed.  We
 *   must hide our system window.  If the parent window is being deleted, we
 *   must also forget our reference to it.  
 */
void CHtmlSysWin_win32::on_close_parent_banner(int deleting_parent)
{
    CHtmlSysWin_win32 *cur;

    /* if we're deleting the parent window, forget our reference to it */
    if (deleting_parent)
        banner_parent_ = 0;

    /* hide our system window, including our border window */
    ShowWindow(handle_, SW_HIDE);
    ShowWindow(border_handle_, SW_HIDE);

    /* 
     *   notify our children that their parent is closing (but note that
     *   we're not actually being deleted yet, so they can hold onto their
     *   references to us for now, if they want to) 
     */
    for (cur = banner_first_child_ ; cur != 0 ; cur = cur->banner_next_)
        cur->on_close_parent_banner(FALSE);
}

/*
 *   Set my banner layout, and lay out my children.  On entry, 'parent_rc'
 *   has the boundaries of the full parent window area; on return,
 *   'parent_rc' is updated to indicate the final boundaries of the parent
 *   window's area after deducting the space carved out for children.
 *   
 *   'first_chi' is the head of the banner child list.  This is normally the
 *   same as my own child list, but isn't always; in particular, the main
 *   history window uses the banner list from the active main window, since
 *   we keep the main window's banners when showing history (the history
 *   shows only previous main windows).  
 */
void CHtmlSysWin_win32::calc_banner_layout(RECT *parent_rc,
                                           CHtmlSysWin_win32 *first_chi)
{
    CHtmlSysWin_win32 *chi;
    RECT new_rc;
    RECT old_rc;

    /* get our current on-screen area */
    GetClientRect(handle_, &old_rc);

    /*
     *   Start off assuming we'll take the entire parent area.  If we're a
     *   top-level window, we'll leave it exactly like that.  If we're a
     *   banner, we'll be aligned on three sides with the parent area, since
     *   we always carve out a chunk by splitting the parent area; we'll
     *   adjust the fourth side according to our banner size and alignment
     *   specifications. 
     */
    new_rc = *parent_rc;

    /* 
     *   if we're a banner window, carve out our own area from the parent
     *   window; otherwise, we must be a top-level window, so take the
     *   entire available space for ourselves 
     */
    if (is_banner_win_)
    {
        int wid, ht;
        HTML_BannerWin_Units_t width_units, height_units;
        long stored_wid, stored_ht;
        CHtmlRect margins;
        int vborder, hborder;

        /* check for a vertical or horizontal border */
        switch(get_banner_alignment())
        {
        case HTML_BANNERWIN_POS_TOP:
        case HTML_BANNERWIN_POS_BOTTOM:
            /* if we have a border, it's a horizontal border */
            hborder = has_border_;
            vborder = FALSE;
            break;

        case HTML_BANNERWIN_POS_LEFT:
        case HTML_BANNERWIN_POS_RIGHT:
            /* if we have a border, it's a vertical border */
            vborder = has_border_;
            hborder = FALSE;
            break;
        }

        /* get our banner size specification */
        get_banner_size_info(&stored_wid, &width_units,
                             &stored_ht, &height_units);

        /* calculate our current on-screen size */
        wid = old_rc.right - old_rc.left;
        ht = old_rc.bottom - old_rc.top;

        /* get our margins from the formatter, if possible */
        if (formatter_ != 0)
            margins = formatter_->get_phys_margins();
        else
            margins.set(0, 0, 0, 0);

        /* 
         *   If this is a zero-size banner, don't use a border or margins.  A
         *   zero-size banner really should be a zero-size banner. 
         */
        if (stored_wid == 0)
        {
            /* the width is zero - don't use any horizontal decorations */
            vborder = FALSE;
            margins.left = margins.right = 0;
        }
        if (stored_ht == 0)
        {
            /* the height is zero - don't use any vertical decorations */
            hborder = FALSE;
            margins.top = margins.bottom = 0;
        }

        /* convert the width units to pixels */
        switch(width_units)
        {
        case HTML_BANNERWIN_UNITS_PIX:
            /* pixels - use the stored size directly */
            wid = stored_wid;

            /* add space for the border if we have one on a vertical edge */
            if (vborder)
                ++wid;
            break;

        case HTML_BANNERWIN_UNITS_CHARS:
            /* 
             *   character cells - calculate the size in terms of the width
             *   of a "0" character in the window's default font 
             */
            wid = stored_wid * measure_text(get_default_font(), "0", 1, 0).x;

            /* add space for the border if we have one on a vertical edge */
            if (vborder)
                ++wid;

            /* add in the horizontal margins */
            wid += margins.left + margins.right;
            break;

        case HTML_BANNERWIN_UNITS_PCT:
            /* 
             *   percentage - calculate the size as a percentage of the
             *   parent space
             */
            wid = (stored_wid * parent_rc->right) / 100;
            break;
        }

        /* convert the height units to pixels */
        switch(height_units)
        {
        case HTML_BANNERWIN_UNITS_PIX:
            /* pixels - use the stored size directly */
            ht = stored_ht;

            /* add space for the border if we have one on a horizontal edge */
            if (hborder)
                ++ht;
            break;

        case HTML_BANNERWIN_UNITS_CHARS:
            /* 
             *   character cells - calculate the size in terms of the height
             *   of a "0" character in the window's default font 
             */
            ht = stored_ht * measure_text(get_default_font(), "0", 1, 0).y;

            /* add space for the border if we have one on a horizontal edge */
            if (hborder)
                ++ht;

            /* add in the vertical margins */
            ht += margins.top + margins.bottom;
            break;

        case HTML_BANNERWIN_UNITS_PCT:
            /* 
             *   percentage - calculate the size as a percentage of the
             *   parent size 
             */
            ht = (stored_ht * parent_rc->bottom) / 100;
            break;
        }

        /* make sure that the banner doesn't exceed the available area */
        if (wid > new_rc.right - new_rc.left)
            wid = new_rc.right - new_rc.left;
        if (ht > new_rc.bottom - new_rc.top)
            ht = new_rc.bottom - new_rc.top;
        
        /* position the banner according to our alignment type */
        switch(get_banner_alignment())
        {
        case HTML_BANNERWIN_POS_TOP:
            /* align the banner at the top of the window */
            new_rc.bottom = new_rc.top + ht;

            /* adjust for the border, if we have one */
            if (has_border_)
            {
                /* take the space for the border out of the banner */
                new_rc.bottom -= 1;

                /* position the border window at the bottom of our area */
                MoveWindow(border_handle_, new_rc.left, new_rc.bottom,
                           new_rc.right - new_rc.left, 1, TRUE);
            }

            /* take the height out of the top of the parent window */
            parent_rc->top += ht;
            break;

        case HTML_BANNERWIN_POS_BOTTOM:
            /* align the banner at the bottom of the window */
            new_rc.top = new_rc.bottom - ht;

            /* adjust for the border, if we have one */
            if (has_border_)
            {
                /* take the space for the border out of the banner */
                new_rc.top += 1;

                /* position the border window at the top of our area */
                MoveWindow(border_handle_, new_rc.left, new_rc.top - 1,
                           new_rc.right - new_rc.left, 1, TRUE);
            }

            /* take the space out of the bottom of the parent area */
            parent_rc->bottom -= ht;
            break;

        case HTML_BANNERWIN_POS_LEFT:
            /* align the banner at the left of the window */
            new_rc.right = new_rc.left + wid;
            
            /* adjust for the border, if we have one */
            if (has_border_)
            {
                /* take the space for the border out of the banner */
                new_rc.right -= 1;

                /* position the border window at the right of our area */
                MoveWindow(border_handle_, new_rc.right, new_rc.top,
                           1, new_rc.bottom - new_rc.top, TRUE);
            }

            /* remove the space from the left of the parent window */
            parent_rc->left += wid;
            break;

        case HTML_BANNERWIN_POS_RIGHT:
            /* align the banner at the right of the window */
            new_rc.left = new_rc.right - wid;

            /* adjust for the border, if we have one */
            if (has_border_)
            {
                /* take the space for the border out of the banner */
                new_rc.left += 1;

                /* position the border window at the left of our area */
                MoveWindow(border_handle_, new_rc.left - 1, new_rc.top,
                           1, new_rc.bottom - new_rc.top, TRUE);
            }

            /* deduct the space from the right of the input window */
            parent_rc->right -= wid;
            break;
        }
    }
    else
    {
        /* 
         *   we're a top-level window, so just leave things as they are, so
         *   that we use the entire available space (which isn't actually
         *   the parent space, since we don't have a parent, but is simply
         *   the application window space available to us) 
         */
    }

    /*
     *   Now that we know our own full area, lay out our banner children.
     *   This will update new_rc to reflect our actual size after deducting
     *   space for our children.  
     */
    for (chi = first_chi ; chi != 0 ; chi = chi->banner_next_)
        chi->calc_banner_layout(&new_rc, chi->get_first_banner_child());

    /* 
     *   Set our new window position.  Avoid moving the window if the size
     *   isn't actually going to change, since doing so can cause
     *   convergence problems as we keep shifting windows around
     *   unnecessarily.  
     */
    if (memcmp(&new_rc, &old_rc, sizeof(new_rc)) != 0)
        MoveWindow(handle_,
                   new_rc.left, new_rc.top,
                   new_rc.right - new_rc.left,
                   new_rc.bottom - new_rc.top, TRUE);
}

/*
 *   Bring the window to the front.  We'll ask our owner to do the work,
 *   since we're meant to run as a child window.  
 */
void CHtmlSysWin_win32::bring_to_front()
{
    owner_->bring_owner_to_front();
}

/*
 *   Get the handle of the frame window.  We'll ask the owner to get the
 *   correct handle, since we're meant to run as a child window. 
 */
HWND CHtmlSysWin_win32::get_frame_handle() const
{
    return owner_->get_owner_frame_handle();
}

/*
 *   Process a command.  This default implementation passes the command to
 *   the owner for processing, since default windows have no native
 *   command processing abilities. 
 */
void CHtmlSysWin_win32::process_command(const textchar_t *cmd, size_t len,
                                        int append, int enter, int os_cmd_id)
{
    owner_->process_command(cmd, len, append, enter, os_cmd_id);
}

/*
 *   Update the status line 
 */
textchar_t *CHtmlSysWin_win32::get_status_message(int *caller_deletes)
{
    textchar_t *msg;

    /* find out what we should display */
    if (exit_pause_)
    {
        /* waiting for a key press before we exit the application */
        msg = exit_pause_str_.get();
    }
    else if (formatting_msg_)
    {
        /* formatting is underway - display "working..." */
        msg = working_str_.get();
    }
    else if (status_link_ != 0)
    {
        msg = status_link_->title_.get();
        if (msg == 0)
            msg = (textchar_t *)status_link_->href_.get_url();
    }
    else if (more_mode_ && !eof_more_mode_ && prefs_->get_alt_more_style())
    {
        /* we're in "MORE" mode - say so on the status line */
        msg = more_status_str_.get();
    }
    else if (waiting_for_key_pause_)
    {
        /* waiting for a key */
        msg = press_key_str_.get();
    }
    else if (pruning_msg_)
    {
        /* 
         *   to minimize the visual intrusiveness of pruning, don't show any
         *   status message while we're reformatting for pruning; in all
         *   likelihood, the user will never notice the pause for
         *   reformatting, so there's no point in calling attention to it
         *   with a message 
         */
        // was: msg = "Working (discarding old text)...";
        msg = 0;
    }
    else
    {
        /* we have nothing to display */
        msg = 0;
    }

    /* the message is static; no need for anyone to delete it */
    *caller_deletes = FALSE;

    /* return our message */
    return msg;
}

/*
 *   Release the "MORE" prompt, showing the next screen of text 
 */
void CHtmlSysWin_win32::release_more_mode(Html_more_t amount)
{
    RECT more_rc;
    RECT win_rc;
    int ht;
    int winht;
    int lineht;
    
    /* if we're not in MORE mode, ignore it */
    if (!more_mode_)
        return;

    /* 
     *   set a self-reference while we're working - releasing MORE mode can
     *   release the last reference to this object, allowing the window to be
     *   deleted 
     */
    AddRef();

    /* no longer in "MORE" mode */
    set_more_mode(FALSE);

    /*
     *   Before moving the window's pixels around, remove the "MORE" prompt
     *   and fill in any text that we clipped out.  We need to do this and
     *   actually redraw the window prior to scrolling the window contents,
     *   because Windows will assume that the part on the screen is all
     *   correct after the scroll.
     *   
     *   If we're destroying the window, don't bother with this, as the
     *   entire window is going to be removed anyway.  (And it can cause
     *   trouble to update it now, since its formatter might already have
     *   been detached from the window.)  
     */
    if (amount != HTMLW32_MORE_CLOSE)
    {
        inval_more_area();
        UpdateWindow(handle_);
    }

    /* 
     *   get the size of the "more" prompt area - we'll use this as the line
     *   height 
     */
    get_moreprompt_rect(&more_rc);
    lineht = more_rc.bottom - more_rc.top;

    /* note the new input position */
    switch(amount)
    {
    case HTMLW32_MORE_PAGE:
        /*
         *   Advance by a page, which is the window height - but reduce the
         *   window height by two line heights, so that we leave some context
         *   from the bottom of the screen at the top of the screen, to
         *   reassure the user that nothing scrolled by unseen.  
         */
        get_scroll_area(&win_rc, TRUE);
        winht = (win_rc.bottom - win_rc.top);
        ht = winht - 2*lineht;

        /* ...but always scroll by a minimum of half the window height */
        if (ht < winht/2)
            ht = winht/2;

        /* ...but in any case, scroll by a minimum of two pixels */
        if (ht < 2)
            ht = 2;

        /* scroll by the amount we calculated */
        last_input_height_ += ht;
        break;

    case HTMLW32_MORE_LINE:
        /* advance the input height by the size of the more prompt */
        last_input_height_ += lineht;
        break;

    case HTMLW32_MORE_ALL:
    case HTMLW32_MORE_CLOSE:
        /* advance to show all pending text */
        last_input_height_ = formatter_->get_max_y_pos();
        break;
    }

    /* 
     *   adjust the scrollbars and scroll to the bottom (but don't bother if
     *   we're destroying the window, as the display will go away anyway) 
     */
    if (amount != HTMLW32_MORE_CLOSE)
        fmt_adjust_vscroll();

    /*
     *   If the more prompt is still on the screen, make sure we redraw
     *   everything from the clipped position down, since this can't be
     *   properly reconstructed by a simple scroll redraw operation. 
     */
    if (more_mode_)
        inval_more_area();

    /*
     *   If we're no longer in MORE mode, set the flag to indicate that
     *   MORE mode has ended. 
     */
    if (!more_mode_)
        released_more_ = TRUE;

    /* release our self-reference */
    Release();
}

/*
 *   Invalidate the "more" prompt area.  This invalidates everything from
 *   the start of the text clipped out by the "more" prompt to the bottom
 *   of the screen.  This is needed before and after scrolling when the
 *   "more" prompt is visible, since the prompt can't be drawn during
 *   scrolling (as its position is not determined until the scrolling
 *   settings are finalized).  
 */
void CHtmlSysWin_win32::inval_more_area()
{
    RECT more_rc;
    RECT client_rc;

    /* 
     *   if we're not in MORE mode, or we're using the non-client-area
     *   more style, there's nothing to do 
     */
    if (prefs_->get_alt_more_style())
        return;

    /* invalidate from the clip position down */
    get_moreprompt_rect(&more_rc);
    GetClientRect(handle_, &client_rc);
    client_rc.top = doc_to_screen_y(clip_ypos_);
    if (more_rc.top < client_rc.top) client_rc.top = more_rc.top;
    InvalidateRect(handle_, &client_rc, FALSE);
}

/*
 *   Get the area occupied by the MORE prompt 
 */
void CHtmlSysWin_win32::get_moreprompt_rect(RECT *rc)
{
    HDC dc;
    SIZE txtsiz;
    int x, y;
    SCROLLINFO info;
    long vscroll_max;
    
    /* get the text area */
    get_scroll_area(rc, TRUE);

    /*
     *   The absolute position is bottom-aligned at the last scrolling
     *   position.  Figure out where this goes based on our current
     *   scrolling settings.  
     */
    if (vscroll_ != 0)
    {
        /* 
         *   there's a scrollbar - get the location where the scrollbar is
         *   scrolled all the way down 
         */
        info.cbSize = sizeof(info);
        info.fMask = SIF_ALL;
        GetScrollInfo(vscroll_, SB_CTL, &info);
        vscroll_max = info.nMax - info.nPage + 1;
    }
    else
    {
        /* there's no scrollbar, so we cannot be scrolled */
        vscroll_max = 0;
    }
    y = doc_to_screen_y(vscroll_max*get_vscroll_units() + rc->bottom);
    x = doc_to_screen_x(0);

    /* set the appropriate font */
    dc = GetDC(handle_);
    select_font(dc, get_default_font());

    /* get the size of the prompt */
    ht_GetTextExtentPoint32(dc, more_prompt_str_.get(),
                            strlen(more_prompt_str_.get()), &txtsiz);

    /*
     *   position it at the bottom of the text area when scrolled all the
     *   way to the bottom 
     */
    SetRect(rc, x, y - txtsiz.cy, rc->right, y);

    /* done with the device context */
    ReleaseDC(handle_, dc);
}

/*
 *   Gain focus - show the caret if appropriate.
 */
void CHtmlSysWin_win32::do_setfocus(HWND)
{
    /* show the caret if it's enabled */
    show_caret();

    /* 
     *   invalidate any selection range, so that we will redraw it with
     *   highlighting 
     */
    formatter_->inval_sel_range();

    /* notify our parent of the focus change */
    notify_parent_focus(TRUE);
}

/*
 *   Lose focus.  Hide the caret.
 */
void CHtmlSysWin_win32::do_killfocus(HWND)
{
    /* hide the caret if necessary */
    hide_caret();

    /* 
     *   proceed only if we have a formatter AND the formatter is still
     *   connected to us (we may not, or it may not, since we could be
     *   killing focus during window destruction) 
     */
    if (formatter_ != 0 && formatter_->get_win() != 0)
    {
        /* forget any temporary link highlighting we're doing */
        set_temp_show_links(FALSE);
        
        /* 
         *   invalidate any selection range, so that we will redraw it
         *   without highlighting 
         */
        formatter_->inval_sel_range();
    }

    /* notify our parent of the focus change */
    notify_parent_focus(FALSE);
}

/*
 *   Determine if I'm in the foreground.  I'm a foreground window if I'm the
 *   system foreground window or a child of the system foreground window. 
 */
int CHtmlSysWin_win32::is_in_foreground() const
{
    HWND fgwin;

    /* get the system foreground window */
    fgwin = GetForegroundWindow();

    /* 
     *   if we are the foreground window, or we're a child of the foreground
     *   window, we're in the foreground 
     */
    return (fgwin == handle_ || IsChild(fgwin, handle_));
}

/*
 *   Create and show the caret 
 */
void CHtmlSysWin_win32::show_caret()
{
    /* 
     *   show the caret only if it's not already visible, it's enabled, and
     *   we're the foreground and focus window 
     */
    if (!caret_vis_
        && caret_enabled_
        && GetFocus() == handle_
        && is_in_foreground())
    {
        CreateCaret(handle_, 0, 2, caret_ascent_);
        ShowCaret(handle_);
        caret_vis_ = TRUE;
        update_caret_pos(FALSE, FALSE);
    }
}

/*
 *   Hide and destroy the caret 
 */
void CHtmlSysWin_win32::hide_caret()
{
    if (caret_vis_)
    {
        HideCaret(handle_);
        DestroyCaret();
        caret_vis_ = FALSE;
    }
}

/*
 *   Hide the caret temporarily for a modal operation 
 */
void CHtmlSysWin_win32::modal_hide_caret()
{
    /* 
     *   only hide the caret if it's generally visible right now, and we're
     *   not already in a modal hide 
     */
    if (caret_vis_ && caret_modal_hide_ == 0)
        HideCaret(handle_);

    /* increment the caret hiding depth */
    ++caret_modal_hide_;
}

/*
 *   Restore the caret from a temporary modal operation 
 */
void CHtmlSysWin_win32::modal_show_caret()
{
    /* decrement the hiding depth */
    --caret_modal_hide_;

    /* 
     *   show the caret again if it was visible to start with and our hiding
     *   depth is back to zero 
     */
    if (caret_vis_ && caret_modal_hide_ == 0)
        ShowCaret(handle_);
}

/*
 *   Update the caret position 
 */
void CHtmlSysWin_win32::update_caret_pos(int bring_on_screen,
                                         int update_sel_range)
{
    unsigned long sel_start, sel_end;

    /* if there's no caret, ignore it */
    if (!caret_enabled_)
        return;

    /* set the caret to the end of the formatter's last object */
    formatter_->get_sel_range(&sel_start, &sel_end);
    caret_pos_ = formatter_->
                 get_text_pos(caret_at_right_ ? sel_end : sel_start);

    /* convert to screen coordinates */
    caret_pos_ = doc_to_screen(caret_pos_);

    /* don't bring anything on the screen if we're in MORE mode */
    if (more_mode_)
        bring_on_screen = FALSE;

    /* scroll as needed to bring the caret onto the screen if desired */
    if (bring_on_screen)
        scroll_to_show_caret();

    /* move the Windows caret, if we're showing it */
    if (caret_vis_)
    {
        if (more_mode_)
            SetCaretPos(-100, -100);
        else
            SetCaretPos(caret_pos_.x, caret_pos_.y + 2);
    }

    /* update the selection range in the formatter */
    if (update_sel_range)
    {
        /* clear any selections in other subwindows */
        owner_->clear_other_sel(this);
    }

    /* if we have an owner, notify it */
    owner_->owner_update_caret_pos();
}

/*
 *   Scroll as needed to bring the caret on-screen 
 */
void CHtmlSysWin_win32::scroll_to_show_caret()
{
    /* show the caret */
    scroll_to_show_point(&caret_pos_, 0, caret_ht_);
}

/*
 *   Scroll to show the selection 
 */
void CHtmlSysWin_win32::scroll_to_show_selection()
{
    unsigned long sel_start;
    unsigned long sel_end;
    CHtmlPoint pt;
    
    /* get the selection range */
    get_formatter()->get_sel_range(&sel_start, &sel_end);

    /* get the coordinates of the end of the selection range */
    pt = formatter_->get_text_pos(sel_end);

    /* adjust to screen coordinates */
    pt = doc_to_screen(pt);

    /* scroll to show the point */
    scroll_to_show_point(&pt, 0, 0);
}

/*
 *   scroll to show a particular point 
 */
void CHtmlSysWin_win32::scroll_to_show_point(const CHtmlPoint *pt,
                                             int wid, int ht)
{
    RECT rc;

    /*
     *   if the point is off-screen horizontally, and we want to bring it
     *   back on screen, scroll to move the point back within the window 
     */
    get_scroll_area(&rc, TRUE);
    if (pt->x < rc.left || pt->x + wid >= rc.right)
    {
        long target;
        long delta;
        long new_hscroll;

        /* scroll so that the point is in the middle of the window */
        target = (rc.left + (rc.right - rc.left)/2);
        delta = pt->x - target;
        new_hscroll = (hscroll_ofs_ * get_hscroll_units() + delta)
                      / get_hscroll_units();
        do_scroll(FALSE, hscroll_, SB_THUMBPOSITION, new_hscroll, TRUE);
    }

    /* likewise, scroll vertically if necessary */
    if (pt->y < rc.top || pt->y + ht >= rc.bottom)
    {
        long target;
        long delta;
        long new_vscroll;

        /* scroll so that the point is in the middle of the window */
        target = (rc.top + (rc.bottom - rc.top)/2);
        delta = pt->y - target;
        new_vscroll = (vscroll_ofs_ * get_vscroll_units() + delta)
                      / get_vscroll_units();
        do_scroll(TRUE, vscroll_, SB_THUMBPOSITION, new_vscroll, TRUE);
    }
}

/*
 *   Handle control notification messages 
 */
int CHtmlSysWin_win32::do_notify(int control_id, int notify_code,
                                 LPNMHDR nmhdr)
{
    TOOLINFO ti;
    POINT pt;
    CHtmlPoint pos;
    static char text_buf[512];
    CHtmlDisp *disp;
    
    switch(notify_code)
    {
    case TTN_NEEDTEXT:
        /* get the mouse position */
        GetCursorPos(&pt);
        ScreenToClient(handle_, &pt);

        /* figure out what we're over */
        pos = screen_to_doc(CHtmlPoint(pt.x, pt.y));
        disp = formatter_->find_by_pos(pos, TRUE);

        /* 
         *   if we're not over anything, or it has no ALT text, display
         *   nothing 
         */
        if (disp == 0 || disp->get_alt_text() == 0)
        {
            /* 
             *   turn the tooltip off and then on again, to keep it from
             *   displaying anything right now but still leave it active
             *   for another try later 
             */
            SendMessage(tooltip_, TTM_ACTIVATE, FALSE, 0);
            SendMessage(tooltip_, TTM_ACTIVATE, TRUE, 0);
        }
        else
        {
            TOOLTIPTEXT *ttt;

            /* use the ALT text as the result */
            ttt = (TOOLTIPTEXT *)nmhdr;
            ttt->lpszText = (char *)disp->get_alt_text();
        }

        /* handled */
        return TRUE;

    case TTN_SHOW:
        /* get the mouse position */
        GetCursorPos(&pt);
        ScreenToClient(handle_, &pt);

        /* figure out what we're over */
        pos = screen_to_doc(CHtmlPoint(pt.x, pt.y));
        disp = formatter_->find_by_pos(pos, TRUE);

        /* if we have an object, get its screen area */
        if (disp != 0)
        {
            CHtmlRect rc;
            
            /* get its screen coordinates */
            rc = doc_to_screen(disp->get_pos());

            /* set the bounds of the object's area */
            ti.cbSize = sizeof(ti);
            SetRect(&ti.rect, rc.left, rc.top, rc.right, rc.bottom);
            ti.hwnd = handle_;
            ti.uId = 1;

            /* set the tooltip area */
            SendMessage(tooltip_, TTM_NEWTOOLRECT, 0, (LPARAM)&ti);
        }

        /* handled - return false to use the default tooltip position */
        return FALSE;

    case TTN_POP:
        /* restore the full-window tool rectangle */
        ti.cbSize = sizeof(ti);
        SetRect(&ti.rect, 0, 0, 32767, 32767);
        ti.hwnd = handle_;
        ti.uId = 1;
        SendMessage(tooltip_, TTM_NEWTOOLRECT, 0, (LPARAM)&ti);

        /* 
         *   note that it just vanished - the next time we move the mouse,
         *   we'll reset the control 
         */
        tooltip_vanished_ = TRUE;

        /* handled */
        return TRUE;

    default:
        /* inherit default handling */
        return CTadsWinScroll::do_notify(control_id, notify_code, nmhdr);
    }
}


/*
 *   Clear the selection range 
 */
void CHtmlSysWin_win32::clear_sel_range()
{
    long start;
    
    /* 
     *   clear the selection range in the formatter by setting both ends
     *   of the range to the maximum text offset in the formatter's
     *   display list 
     */
    start = formatter_->get_text_ofs_max();
    formatter_->set_sel_range(start, start);
    fmt_sel_to_cmd_sel(TRUE);
}

/*
 *   Clear the hover information 
 */
void CHtmlSysWin_win32::clear_hover_info()
{
    /* forget any link tracking information */
    set_hover_link(0);
}

/*
 *   Clear any memory of the link we're hovering over 
 */
void CHtmlSysWin_win32::set_hover_link(CHtmlDispLink *link)
{
    /* if we're changing it, update everything */
    if (link != status_link_)
    {
        /* remove highlighting from the previous hovering item */
        if (status_link_ != 0)
            status_link_->set_clicked(this, CHtmlDispLink_none);

        /* set the new link */
        status_link_ = link;

        /* show highlighting on the new link if appropriate */
        if (status_link_ != 0)
            status_link_->set_clicked(this, CHtmlDispLink_hover);

        /* update the status line for the change */
        if (statusline_ != 0)
            statusline_->update();
    }
}

/*
 *   enter an interactive size/move operation 
 */
void CHtmlSysWin_win32::do_entersizemove()
{
    /* remember that we're doing an interactive size/move */
    in_size_move_ = TRUE;
}

/*
 *   finish an interactive size/move operation 
 */
void CHtmlSysWin_win32::do_exitsizemove()
{
    RECT rc;
    long new_width;
    
    /* forget about the operation */
    in_size_move_ = FALSE;

    /* 
     *   Now we can reformat, if necessary.  Note that we formerly
     *   formatted on resize only if the width changed, but some types of
     *   layout do require a reformat on height changes (tables whose
     *   heights are set to percentages of the window height, for
     *   example), so reformat any time the window size changes in either
     *   dimension.  
     */
    GetClientRect(handle_, &rc);
    new_width = rc.right - rc.left;
    if (/* new_width != fmt_width_ && */ formatter_ != 0)
        format_after_resize(new_width);
}

/*
 *   timer events 
 */
int CHtmlSysWin_win32::do_timer(int timer_id)
{
    /* see if we recognize the timer */
    if (timer_id == cb_timer_id_)
    {
        htmlw32_timer_t *cur;
        htmlw32_timer_t *nxt;
        
        /* 
         *   it's the callback timer - go through our list of registered
         *   callbacks and invoke each one 
         */
        for (cur = timer_list_head_ ; cur != 0 ; cur = nxt)
        {
            /* 
             *   remember the next timer in the list, in case the current
             *   one is deregistered within the callback (assume that one
             *   callback won't deregister another, though) 
             */
            nxt = cur->nxt_;

            /* invoke the callback */
            (*cur->func_)(cur->ctx_);
        }

        /* handled */
        return TRUE;
    }
    else if (timer_id == temp_link_timer_id_)
    {
        /* turn on temporary links */
        set_temp_show_links(TRUE);

        /* turn off the timer - it's done its job */
        KillTimer(handle_, temp_link_timer_id_);
        free_timer_id(temp_link_timer_id_);
        temp_link_timer_id_ = 0;

        /* handled */
        return TRUE;
    }
    else if (timer_id == bg_timer_id_)
    {
        /*
         *   It's our miscellaneous background event timer - call our idle
         *   routine to do any housekeeping chores that we need to perform
         *   periodically. 
         */
        do_idle();
        return TRUE;
    }
    else
    {
        CHtmlSysTimer_win32 *cur;
        
        /* look for the timer among our system timer objects */
        for (cur = sys_timer_head_ ; cur != 0 ; cur = cur->get_next())
        {
            /* if this is the one, invoke it */
            if (cur->get_id() == timer_id)
            {
                /* 
                 *   if this is a once-only timer, kill the system timer so
                 *   that we don't repeat the timer event
                 */
                if (!cur->is_repeating())
                    cancel_timer(cur);

                /* invoke the timer's callback */
                cur->invoke_callback();

                /* handled */
                return TRUE;
            }
        }
        
        /* not one of ours - inherit default handling */
        return CTadsWinScroll::do_timer(timer_id);
    }
}

/*
 *   Perform background tasks.  This is called periodically on a timer. 
 */
void CHtmlSysWin_win32::do_idle()
{
    POINT pos;
    
    /*
     *   Check the mouse location.  If the mouse is no longer in our
     *   window, remove any highlighted link. 
     */
    GetCursorPos(&pos);
    if (WindowFromPoint(pos) != handle_)
        set_hover_link(0);
}

/*
 *   resize 
 */
void CHtmlSysWin_win32::do_resize(int mode, int x, int /*y*/)
{
    RECT rc;

    /* move the scrollbars to their new positions */
    adjust_scrollbar_positions();

    /* update our memory of the display width */
    get_scroll_area(&rc, TRUE);
    disp_width_ = rc.right - rc.left;
    disp_height_ = rc.bottom - rc.top;

    /* see what we're doing */
    switch(mode)
    {
    case SIZE_MAXHIDE:
    case SIZE_MAXSHOW:
    case SIZE_MINIMIZED:
        /* ignore these modes */
        break;

    case SIZE_MAXIMIZED:
    case SIZE_RESTORED:
    default:
        /*
         *   If the width changed, reformat the text to account for the
         *   new size.  Only schedule the reformatting right now, since we
         *   can get a flurry of these messages if the system is set to
         *   show window contents while moving or resizing windows.
         *   
         *   If we're in the process of formatting after a resize, ignore
         *   the change entirely -- we're just fitting everything to the
         *   final size, and we don't want to change the size again.  It's
         *   important that we don't resize again just because we're doing
         *   a reformat, since this would prevent us from converging --
         *   we'd just keep resizing over and over on each pass.  
         */
        if (x != fmt_width_ && formatter_ != 0
            && !doing_resize_reformat_)
        {
            MSG msg;

            /* if we haven't already scheduled reformatting, do so now */
            if (!PeekMessage(&msg, handle_, HTMLM_ONRESIZE, HTMLM_ONRESIZE,
                             PM_NOREMOVE))
            {
                /* haven't scheduled it yet - post a message to myself */
                PostMessage(handle_, HTMLM_ONRESIZE, 0, (LPARAM)x);
            }
        }

        /* done */
        break;
    }
}

/*
 *   Reformat after a resize 
 */
void CHtmlSysWin_win32::format_after_resize(long new_width)
{
    int drawn;
    SCROLLINFO info;

    /* 
     *   if an interactive size/move is in progress, ignore the
     *   notification for now, and defer it until the size/move is done 
     */
    if (in_size_move_)
        return;

    /* 
     *   if we're currently scrolled to the bottom of the text, we'll want
     *   to show the bottom of the text after reformatting, so note this
     *   now 
     */
    get_scroll_info(TRUE, &info);
        
    /* reformat everything if necessary */
    if (need_format_on_resize_)
    {
        /* remember that we're doing a post-resize format pass */
        doing_resize_reformat_ = TRUE;
        
        /* forget any tracking links */
        status_link_ = track_link_ = 0;

        /* discard all current formatting */
        formatter_->start_at_top(FALSE);

        /* run the formatter */
        drawn = do_formatting(TRUE, FALSE, TRUE);

        /* if we didn't redraw the window already, do so now */
        if (!drawn)
            InvalidateRect(handle_, 0, FALSE);

        /* remember the new format width */
        fmt_width_ = new_width;

        /* we're done with the resize pass */
        doing_resize_reformat_ = FALSE;
    }

    /* adjust the caret and scrollbar settings */
    adjust_for_reformat();

    /* scroll to the bottom of the text if we were there previously */
    if (need_format_on_resize_
        && info.nPos + (int)info.nPage - 1 >= info.nMax)
    {
        /* scroll to the bottom */
        do_scroll(TRUE, vscroll_, SB_BOTTOM, 0, 0);
    }
}

/*
 *   Adjust the caret and scrollbars after reformatting the window 
 */
void CHtmlSysWin_win32::adjust_for_reformat()
{
    /* refigure the scrollbar ranges */
    if (!more_mode_ || prefs_->get_alt_more_style())
        content_height_ = formatter_->get_max_y_pos();
    adjust_scrollbar_ranges();

    /* figure the new caret position */
    update_caret_pos(FALSE, FALSE);
}

/*
 *   Get the basic scroll settings 
 */
int CHtmlSysWin_win32::get_scroll_info(int vert, SCROLLINFO *info)
{
    RECT rc;
    
    /* set up the basic elements of the structure */
    CTadsWinScroll::get_scroll_info(vert, info);
    
    /* get our scrolling area */
    get_scroll_area(&rc, TRUE);

    if (vert)
    {
        /* set the limits based on the current display size */
        info->nMin = 0;
        info->nMax = content_height_ / get_vscroll_units();
        info->nPage = rc.bottom / get_vscroll_units();
        info->nPos = vscroll_ofs_;

        /*
         *   disable the scrollbar if scrolling is impossible, rather
         *   than making it invisible 
         */
        info->fMask |= SIF_DISABLENOSCROLL;

        /* the vertical scroll is always visible */
        return TRUE;
    }
    else
    {
        int new_hscroll_vis;
        
        /* presume we won't show the horizontal scrollbar */
        new_hscroll_vis = FALSE;

        /* set a default range in case we don't have more information */
        info->nMin = 0;
        info->nPos = hscroll_ofs_;
        info->nPage = rc.right / get_hscroll_units();
        info->nMax = info->nPos + info->nPage + 1;

        if (formatter_ != 0)
        {
            /*
             *   show the horizontal scrollbar if we're smaller than the
             *   longest line 
             */
            if (rc.right < formatter_->get_outer_max_line_width()
                || hscroll_ofs_ != 0)
            {
                /* show the scrollbar */
                new_hscroll_vis = TRUE;
            }

            /* set its size */
            info->nMax = formatter_->get_outer_max_line_width()
                         / get_hscroll_units();

            /*
             *   if the max is below the current position, raise the max
             *   temporarily 
             */
            if (info->nMax < info->nPos + (int)info->nPage - 1)
                info->nMax = info->nPos + info->nPage - 1;
        }

        /* return visibility indication */
        return new_hscroll_vis;
    }
}

/*
 *   Adjust the scrollbar ranges
 */
void CHtmlSysWin_win32::adjust_scrollbar_ranges()
{
    int new_hscroll_vis;
    SCROLLINFO info;

    /* if we have a vertical scrollbar, check for visibility */
    if (vscroll_ != 0)
    {
        /* vertical scrollbar is always visible */
        vscroll_vis_ = TRUE;

        /* get the vertical scrollbar info and set it in the scrollbar */
        get_scroll_info(TRUE, &info);
        SetScrollInfo(vscroll_, SB_CTL, &info, TRUE);

        /* adjust our offset if it changed */
        GetScrollInfo(vscroll_, SB_CTL, &info);
        if (vscroll_ofs_ != info.nPos)
            do_scroll(TRUE, vscroll_, SB_THUMBPOSITION, info.nPos, TRUE);
    }

    /* if we have a horizontal scrollbar, check it */
    if (hscroll_ != 0)
    {
        /* get horizontal scrollbar info and set it in the scrollbar */
        new_hscroll_vis = get_scroll_info(FALSE, &info);
        SetScrollInfo(hscroll_, SB_CTL, &info, TRUE);

        /* adjust our offset if it changed */
        if (new_hscroll_vis)
        {
            GetScrollInfo(hscroll_, SB_CTL, &info);
            if (hscroll_ofs_ != info.nPos)
                do_scroll(FALSE, hscroll_, SB_THUMBPOSITION, info.nPos, TRUE);
        }
        
        /* see if we're changing the horizontal scrollbar's visibility */
        if ((hscroll_vis_ != 0) != (new_hscroll_vis != 0))
        {
            /* note the new visibility */
            hscroll_vis_ = new_hscroll_vis;
            
            /* show the scrollbars as appropriate */
            ShowWindow(hscroll_, hscroll_vis_ ? SW_SHOW : SW_HIDE);

            /* fix up the scrollbar positions */
            adjust_scrollbar_positions();
        }
    }
}


/*
 *   Receive notification that scrolling has occurred 
 */
void CHtmlSysWin_win32::notify_scroll(HWND, long /*oldpos*/, long /*newpos*/)
{
    /* turn off alternate more mode if we're at the bottom */
    if (prefs_->get_alt_more_style() && more_mode_)
    {
        SCROLLINFO info;

        /* 
         *   if we're at the bottom, cancel more mode; don't do this if
         *   we're processing an explicit game morePrompt request, since
         *   we want to make sure they hit a key for those 
         */
        get_scroll_info(TRUE, &info);
        if (info.nPos + (int)info.nPage - 1 >= info.nMax
            && !game_more_request_)
        {
            /* 
             *   we've scrolled to the bottom - cancel more mode if we're not
             *   in an explicitly-requested MORE prompt 
             */
            if (!game_more_request_)
                set_more_mode(FALSE);

            /* flag that we've released the more prompt */
            released_more_ = TRUE;

            /* check to see if we can start reading a command now */
            check_cmd_start();
        }
        else
        {
            RECT rc;

            /*
             *   set the input position to the current position, so that
             *   we'll continue scrolling from here if we scroll via the
             *   keyboard (with space or return) 
             */
            get_scroll_area(&rc, TRUE);
            last_input_height_ = screen_to_doc_y(0);
        }
    }

    /* move the caret */
    update_caret_pos(FALSE, FALSE);
}


/*
 *   process creation event 
 */
void CHtmlSysWin_win32::do_create()
{
    TOOLINFO ti;

    /* do default creation */
    CTadsWinScroll::do_create();

    /* set the default caret height according to the default font height */
    set_caret_size(default_font_);

    /* start the system timer if appropriate */
    start_sys_timer();

    /* 
     *   set up an idle timer to take care of some miscellaneous
     *   background tasks 
     */
    bg_timer_id_ = alloc_timer_id();
    if (bg_timer_id_ != 0
        && SetTimer(handle_, bg_timer_id_, 500, (TIMERPROC)0) == 0)
    {
        /* that failed - free our timer ID */
        free_timer_id(bg_timer_id_);
        bg_timer_id_ = 0;
    }

    /* 
     *   Determine the link toggle settings.  If links are currently on,
     *   toggle to off.  If they're currently off or in ctrl mode, toggle
     *   to on. 
     */
    toggle_link_ctrl_ = FALSE;
    toggle_link_on_ = !prefs_->get_links_on();

    /* create the tooltip control */
    tooltip_ = CreateWindowEx(0, TOOLTIPS_CLASS, 0, 0,
                              CW_USEDEFAULT, CW_USEDEFAULT,
                              CW_USEDEFAULT, CW_USEDEFAULT,
                              handle_, 0,
                              CTadsApp::get_app()->get_instance(), 0);
    
    /* 
     *   add the "tool" - the entire display area; we'll figure out where
     *   we actually are when we need to get tooltip text 
     */
    ti.cbSize = sizeof(ti);
    ti.uFlags = TTF_SUBCLASS;
    ti.hwnd = handle_;
    ti.uId = 1;
    ti.hinst = CTadsApp::get_app()->get_instance();
    ti.lpszText = LPSTR_TEXTCALLBACK;
    SetRect(&ti.rect, 0, 0, 32767, 32767);
    SendMessage(tooltip_, TTM_ADDTOOL, 0, (LPARAM)&ti);
}

/*
 *   process window deletion 
 */
void CHtmlSysWin_win32::do_destroy()
{
    /* if we have a status line, unregister for notifications */
    if (statusline_ != 0)
        statusline_->unregister_source(this);

    /* remove our miscellaneous background timer if we have one */
    if (bg_timer_id_ != 0)
    {
        KillTimer(handle_, bg_timer_id_);
        free_timer_id(bg_timer_id_);
    }
    
    /* inherit default */
    CTadsWinScroll::do_destroy();
}

/*
 *   erase the background 
 */
int CHtmlSysWin_win32::do_erase_bkg(HDC)
{
    /* 
     *   We'll erase the background in the main paint routine - do nothing
     *   here.  We can do the erasing more efficiently in the main paint
     *   routine, since we can limit the drawing to the clipping area.  
     */
    return TRUE;
}

/*
 *   paint the window contents
 */
void CHtmlSysWin_win32::do_paint_content(HDC win_dc, const RECT *rc)
{
    CHtmlRect area;
    RECT more_rc;
    int clip_lines;
    HDC dc;
    HBITMAP off_bmp;
    HBITMAP off_bmp_old;
    HDC old_win_dc;
    int win_paletted;

    /* 
     *   note whether or not the window is paletted (it is if the display
     *   mode uses 8 bits or fewer per pixel) 
     */
    win_paletted = (get_bits_per_pixel() <= 8);

    /* 
     *   create an in-memory DC compatible with the display DC - we'll
     *   render to this memory DC rather than directly to the display, then
     *   blt the result to the display, to avoid the flicker we'd get if we
     *   were to render directly on the display 
     */
    dc = CreateCompatibleDC(win_dc);

    /* if that succeeded, create the bitmap */
    if (dc != 0)
    {
        /* create a bitmap for the memory DC */
        if ((off_bmp = CreateCompatibleBitmap(win_dc, rc->right,
                                              rc->bottom)) != 0)
        {
            /* select the bitmap into the memory DC */
            off_bmp_old = (HBITMAP)SelectObject(dc, off_bmp);
            
            /* 
             *   remember the original window DC, and plug in our memory DC
             *   while we do the work - this will ensure that all of our
             *   drawing goes to the memory DC 
             */
            old_win_dc = hdc_;
            hdc_ = dc;

            /* 
             *   if we're paletted, realize our fixed palette into the memory
             *   DC so that the rendering matches our palette 
             */
            if (win_paletted)
            {
                SelectPalette(dc, hpal_, TRUE);
                RealizePalette(dc);
            }
        }
        else
        {
            /* 
             *   we couldn't create the bitmap - there's probably not
             *   enough memory; just draw directly into the display DC
             *   instead 
             */
            DeleteDC(dc);
            dc = win_dc;
        }
    }
    else
    {
        /* 
         *   we couldn't allocate the memory DC - draw into the display
         *   directly instead 
         */
        dc = win_dc;
    }

    /* adjust the paint area to local coordinates */
    area.set(rc->left, rc->top, rc->right, rc->bottom);

    /* 
     *   Fill the redraw area with the background brush.  Do this even if we
     *   have a background image, in case the image has any transparent
     *   sections.  
     */
    FillRect(dc, rc, bgbrush_);

    /* 
     *   First, draw the background.  If I have an image, draw it;
     *   otherwise, use the background color 
     */
    if (bg_image_ != 0)
    {
        CHtmlSysImage *img;

        /* figure out where the image goes, taking scrolling into account */
        img = bg_image_->get_image();
        if (img != 0)
        {
            unsigned long ht, wid;

            /* get the height and width of the image */
            ht = img->get_height();
            wid = img->get_width();
            if (ht != 0 && wid != 0)
            {
                CHtmlRect docrc;
                long x1, y1;
                long x, y;
                
                /* compute where we're drawing, in document coordinates */
                docrc = screen_to_doc(area);

                /* 
                 *   compute, in document coordinates, the location of the
                 *   tile that overlaps the rectangle, assuming that the
                 *   image is tiled from the upper left of the document 
                 */
                x1 = (docrc.left / wid) * wid;
                y1 = (docrc.top / ht) * ht;

                /* tile the image until we've filled up the drawing area */
                for (y = y1 ; y < docrc.bottom ; y += ht)
                {
                    /* tile across the current row */
                    for (x = x1 ; x < docrc.right ; x += wid)
                    {
                        CHtmlRect pos;
                        
                        /* draw this copy of the image */
                        pos.set(x, y, x + wid, y + ht);
                        img->draw_image(this, &pos, HTMLIMG_DRAW_CLIP);
                    }
                }
            }
        }
    }

    /* assume we'll allow the last line to show */
    clip_lines = FALSE;

    /*
     *   if we're in "MORE" mode, clip the drawing area to the line
     *   before the prompt 
     */
    if (more_mode_)
    {
        /* get the prompt area */
        get_moreprompt_rect(&more_rc);

        /*
         *   if the drawing area intersects the prompt area, clip out the
         *   prompt area and tell the formatter not to draw lines that are
         *   partially clipped 
         */
        if (area.bottom >= more_rc.top)
        {
            area.bottom = more_rc.top - 1;
            clip_lines = TRUE;
        }
    }

    /* offset by scrolling position */
    area = screen_to_doc(area);

    /* draw everything in the client area */
    if (formatter_ != 0)
        formatter_->draw(&area, clip_lines, &clip_ypos_);

    /* if we're in "MORE" mode, draw a prompt at the bottom */
    if (more_mode_ && !prefs_->get_alt_more_style())
    {
        COLORREF oldclr;
        int oldbkmode;

        /* write the prompt in white text in the default font */
        select_font(dc, get_default_font());
        oldclr = SetTextColor(dc, GetSysColor(COLOR_HIGHLIGHTTEXT));
        oldbkmode = SetBkMode(dc, TRANSPARENT);

        /* write the MORE prompt over a dark gray background */
        FillRect(dc, &more_rc, GetSysColorBrush(COLOR_HIGHLIGHT));
        ExtTextOut(dc, more_rc.left, more_rc.top, 0, 0,
                   more_prompt_str_.get(), strlen(more_prompt_str_.get()), 0);

        /* restore the old text color and mode */
        SetBkMode(dc, oldbkmode);
        SetTextColor(dc, oldclr);
    }

    /* 
     *   if we successfully created a memory DC for off-screen rendering,
     *   copy the off-screen bitmap's contents into the display DC now
     *   that we're finished painting it 
     */
    if (dc != win_dc)
    {
        /* realize the palette if we're in a paletted video mode */
        if (win_paletted)
        {
            /* realize the palette */
            SelectPalette(win_dc, hpal_, TRUE);
            RealizePalette(win_dc);
        }

        /* copy the off-screen bitmap into the window device context */
        BitBlt(win_dc, rc->left, rc->top,
               rc->right - rc->left, rc->bottom - rc->top,
               dc, rc->left, rc->top, SRCCOPY);
        
        /* delete the off-screen drawing objects */
        SelectObject(dc, off_bmp_old);
        DeleteObject(off_bmp);
        DeleteDC(dc);

        /* restore the window's device context */
        hdc_ = old_win_dc;
    }
}


/*
 *   Get the window group 
 */
CHtmlSysWinGroup *CHtmlSysWin_win32::get_win_group()
{
    /* return my owner's window group object */
    return owner_->get_owner_win_group();
}

/*
 *   Close the window 
 */
int CHtmlSysWin_win32::close_window(int force)
{
    /* ask the owner to close */
    return owner_->close_owner_window(force);
}

/*
 *   get the width of the display area 
 */
long CHtmlSysWin_win32::get_disp_width()
{
    return disp_width_;
}

/*
 *   get the height of the display area 
 */
long CHtmlSysWin_win32::get_disp_height()
{
    return disp_height_;
}

/*
 *   get number of pixels per inch 
 */
long CHtmlSysWin_win32::get_pix_per_inch()
{
    HDC deskdc;
    long ret;

    /* get the system horizontal pixels per inch for the desktop window */
    deskdc = GetDC(GetDesktopWindow());
    ret = GetDeviceCaps(deskdc, LOGPIXELSX);
    ReleaseDC(GetDesktopWindow(), deskdc);

    /* return the value we got from the system */
    return ret;
}

/*
 *   measure the size of a string of text in a given font
 */
CHtmlPoint CHtmlSysWin_win32::measure_text(CHtmlSysFont *font,
                                           const textchar_t *txt, size_t len,
                                           int *ascent)
{
    SIZE txtsiz;
    HDC dc;
    TEXTMETRIC tm;

    /* get my device context */
    dc = GetDC(handle_);

    /* select the font */
    select_font(dc, font);

    /* get the text width */
    ht_GetTextExtentPoint32(dc, txt, len, &txtsiz);

    /* get the height from the text metrics */
    GetTextMetrics(dc, &tm);
    txtsiz.cy = tm.tmHeight;

    /* return the ascender height if the caller wants it */
    if (ascent != 0)
        *ascent = tm.tmAscent;

    /* done with the DC */
    ReleaseDC(handle_, dc);

    /* return the value, reinterpreted into our own Point type */
    return CHtmlPoint(txtsiz.cx, txtsiz.cy);
}

/*
 *   Get the maximum number of characters that fit in a given width 
 */
size_t CHtmlSysWin_win32::get_max_chars_in_width(
    class CHtmlSysFont *font, const textchar_t *str, size_t len, long wid)
{
    HDC dc;
    INT fit_cnt;
    SIZE siz;

    /* get the device context */
    dc = GetDC(handle_);
    
    /* select the font */
    select_font(dc, font);

    /* measure the text */
    fit_cnt = 0;
    GetTextExtentExPoint(dc, str, len, wid, &fit_cnt, 0, &siz);

    /* done with the DC */
    ReleaseDC(handle_, dc);

    /* return the number that fit */
    return (size_t)fit_cnt;
}

/*
 *   select a font 
 */
HGDIOBJ CHtmlSysWin_win32::select_font(HDC dc, class CHtmlSysFont *font)
{
    return ((CHtmlSysFont_win32 *)font)->select(dc);
}

/*
 *   restore old font 
 */
void CHtmlSysWin_win32::unselect_font(HDC dc, class CHtmlSysFont *font,
                                      HGDIOBJ oldfont)
{
    ((CHtmlSysFont_win32 *)font)->unselect(dc, oldfont);
}


/*
 *   draw text 
 */
void CHtmlSysWin_win32::draw_text(int hilite, long x, long y,
                                  CHtmlSysFont *font,
                                  const textchar_t *str, size_t len)
{
    /* draw the text; no clipping area is needed */
    draw_text_clip(hilite, x, y, font, str, len, 0, 0);
}

/*
 *   draw typographical space
 */
void CHtmlSysWin_win32::draw_text_space(int hilite, long x, long y,
                                        CHtmlSysFont *font,
                                        long width)
{
    const textchar_t sp[] = "                ";
    const size_t spmax = sizeof(sp) - sizeof(textchar_t);
    CHtmlPoint spmaxsiz;
    size_t cnt;
    RECT cliprc;

    /* calculate the size of our string of spaces */
    spmaxsiz = measure_text(font, sp, spmax, 0);

    /*
     *   Draw complete chunks as many times as we can.  Keep doing this as
     *   long as the remainder of the width requested exceeds the total
     *   width we can draw in a single chunk.  
     */
    while (width >= spmaxsiz.x)
    {
        /* 
         *   draw this string of spaces; no clipping is needed for this
         *   chunk, since they want even more than we can draw at once 
         */
        draw_text_clip(hilite, x, y, font, sp, spmax, 0, 0);

        /* advance past the block we just drew */
        x += spmaxsiz.x;
        width -= spmaxsiz.x;
    }

    /*
     *   We now have less than the chunk size remaining.  If there's nothing
     *   left to do at all, we're done. 
     */
    if (width == 0)
        return;

    /* 
     *   Figure out how many spaces we can fit into the remaining width,
     *   then add one more space to make sure we draw at least the requested
     *   width.  We need to add the one extra space, because the max-fit
     *   function will round down if the requested size isn't an integral
     *   multiple of the size of a single space - it tells us the most we
     *   can fit in the space without overflowing it, and we really want to
     *   completely fill the space. 
     */
    cnt = get_max_chars_in_width(font, sp, spmax, width) + 1;

    /* 
     *   Now, since we had to add in an extra space to ensure we filled up
     *   the full extent of the space, we are now overflowing the available
     *   space.  So, we must clip our drawing to the actual requested space
     *   to make sure we don't draw too much.  Calculate the clipping
     *   rectangle based on the font height and desired width. 
     */
    SetRect(&cliprc, x, y, x + width, y + spmaxsiz.y);

    /* draw the final clipped chunk of spaces */
    draw_text_clip(hilite, x, y, font, sp, cnt, ETO_CLIPPED, &cliprc);
}

/*
 *   Internal text drawing implementation.  This draws text with optional
 *   clipping, and with the given Windows ExtTextOut drawing flags.  
 */
void CHtmlSysWin_win32::draw_text_clip(int hilite, long x, long y,
                                       CHtmlSysFont *font,
                                       const textchar_t *str, size_t len,
                                       unsigned int flags,
                                       const RECT *doc_cliprect)
{
    COLORREF oldbkcolor;
    COLORREF oldtxtcolor;
    int reset_bkcolor;
    int oldbkmode;
    RECT cliprect;
    
    /* select the font */
    select_font(hdc_, font);

    /* adjust for scrolling */
    x = doc_to_screen_x(x);
    y = doc_to_screen_y(y);

    /* if a clipping rectangle was given, translate it to local coordinates */
    if (doc_cliprect != 0)
        SetRect(&cliprect,
                doc_to_screen_x(doc_cliprect->left),
                doc_to_screen_y(doc_cliprect->top),
                doc_to_screen_x(doc_cliprect->right),
                doc_to_screen_y(doc_cliprect->bottom));

    /* if we don't have focus, never draw selected text with highlighting */
    if (GetFocus() != handle_ || !is_in_foreground())
        hilite = FALSE;

    /* presume we won't have to change the background color */
    reset_bkcolor = FALSE;

    /* 
     *   If highlighting, turn on text highlight colors.  If drawing with an
     *   explicit background color, fill the area with the color.  Otherwise,
     *   draw transparently on the existing background. 
     */
    if (hilite)
    {
        /* set highlight text color and background color, and opaque mode */
        oldtxtcolor = SetTextColor(hdc_, GetSysColor(COLOR_HIGHLIGHTTEXT));
        oldbkcolor = SetBkColor(hdc_, GetSysColor(COLOR_HIGHLIGHT));
        oldbkmode = SetBkMode(hdc_, OPAQUE);

        /* note that we changed the background color */
        reset_bkcolor = TRUE;
    }
    else
    {
        /* 
         *   if we have a background color explicitly set in the font, draw
         *   opaquely with the background color; otherwise, draw
         *   transparently on the existing background 
         */
        if (font->use_font_bgcolor())
        {
            /* draw with the explicit background color */
            oldbkmode = SetBkMode(hdc_, OPAQUE);
            oldbkcolor = SetBkColor(hdc_, HTML_color_to_COLORREF(
                font->get_font_bgcolor()));

            /* note that we changed the background color */
            reset_bkcolor = TRUE;
        }
        else
        {
            /* set transparent mode */
            oldbkmode = SetBkMode(hdc_, TRANSPARENT);
        }

        /*
         *   if the font specifies a color, use the font color, otherwise
         *   use the default text color 
         */
        if (font->use_font_color())
            oldtxtcolor =
                SetTextColor(hdc_,
                             HTML_color_to_COLORREF(font->get_font_color()));
        else
            oldtxtcolor = SetTextColor(hdc_, text_color_);
    }

    /* draw the text */
    ExtTextOut(hdc_, x, y, flags, &cliprect, str, len, 0);

    /* restore original background color scheme if we changed it */
    if (reset_bkcolor)
        SetBkColor(hdc_, oldbkcolor);

    /* restore original text color */
    SetTextColor(hdc_, oldtxtcolor);

    /* restore original background mode */
    SetBkMode(hdc_, oldbkmode);
}

/*
 *   draw a bullet 
 */
void CHtmlSysWin_win32::draw_bullet(int hilite, long x, long y,
                                    CHtmlSysFont *font,
                                    HTML_SysWin_Bullet_t style)
{
    textchar_t ch;

    /* get the appropriate bullet character */
    switch(style)
    {
    case HTML_SYSWIN_BULLET_SQUARE:
        ch = bullet_square_;
        break;
        
    case HTML_SYSWIN_BULLET_CIRCLE:
        ch = bullet_circle_;
        break;

    default:
    case HTML_SYSWIN_BULLET_DISC:
        ch = bullet_disc_;
        break;

    case HTML_SYSWIN_BULLET_PLAIN:
        /* nothing to draw */
        return;
    }

    /* draw the bullet */
    draw_text(hilite, x, y, font, &ch, 1);
}

/*
 *   Get the font to use to draw bullets.  If wingdings is present, we'll
 *   use wingdings at the current font size; otherwise, we'll just use the
 *   current font. 
 */
CHtmlSysFont *CHtmlSysWin_win32::get_bullet_font(CHtmlSysFont *font)
{
    CHtmlFontDesc desc;
    static const textchar_t symfont[] = "Wingdings";
    
    /* if wingdings isn't available, use the current font */
    if (!wingdings_avail_)
        return font;

    /* get Wingdings in the same size as the current font */
    font->get_font_desc(&desc);
    memcpy(desc.face, symfont, sizeof(symfont));
    desc.default_charset = FALSE;
    desc.charset = oshtml_charset_id_t(BULLET_CHARSET, CP_SYMBOL);
    return get_font(&desc);
}

/*
 *   Get the DirectSound interface 
 */
IDirectSound *CHtmlSysWin_win32::get_directsound()
{
    return owner_->get_directsound();
}

/*
 *   Register a timer function 
 */
void CHtmlSysWin_win32::register_timer_func(void (*timer_func)(void *),
                                            void *func_ctx)
{
    htmlw32_timer_t *rec;
    int need_sys_timer = FALSE;

    /* if we don't have a system timer, note that we need to start one */
    if (timer_list_head_ == 0)
        need_sys_timer = TRUE;
    
    /* allocate a new link record */
    rec = new htmlw32_timer_t(timer_func, func_ctx);

    /* add the link record to our list */
    rec->nxt_ = timer_list_head_;
    timer_list_head_ = rec;

    /* if necessary, start the system timer */
    if (need_sys_timer)
        start_sys_timer();
}

/*
 *   create a timer 
 */
CHtmlSysTimer *CHtmlSysWin_win32::create_timer(void (*timer_func)(void *),
                                               void *func_ctx)
{
    CHtmlSysTimer_win32 *timer;
    
    /* create a timer object, allocating a new system timer ID for it */
    timer = new CHtmlSysTimer_win32(timer_func, func_ctx, alloc_timer_id());

    /* link it into our timer list */
    timer->set_next(sys_timer_head_);
    sys_timer_head_ = timer;

    /* return the new timer */
    return timer;
}

/* 
 *   set a timer 
 */
void CHtmlSysWin_win32::set_timer(CHtmlSysTimer *timer0,
                                  long interval_ms, int repeat)
{
    CHtmlSysTimer_win32 *timer = (CHtmlSysTimer_win32 *)timer0;

    /* set the timer */
    SetTimer(handle_, timer->get_id(), interval_ms, (TIMERPROC)0);

    /* set the repeating mode */
    timer->set_repeating(repeat);

    /* mark it active */
    timer->set_active(TRUE);
}

/* 
 *   cancel a timer 
 */
void CHtmlSysWin_win32::cancel_timer(CHtmlSysTimer *timer0)
{
    CHtmlSysTimer_win32 *timer = (CHtmlSysTimer_win32 *)timer0;

    /* if the timer is active, kill it */
    if (timer->is_active())
    {
        /* kill the system timer */
        KillTimer(handle_, timer->get_id());

        /* mark it inactive */
        timer->set_active(FALSE);
    }
}

/* 
 *   delete a timer 
 */
void CHtmlSysWin_win32::delete_timer(CHtmlSysTimer *timer0)
{
    CHtmlSysTimer_win32 *timer = (CHtmlSysTimer_win32 *)timer0;
    CHtmlSysTimer_win32 *cur;
    CHtmlSysTimer_win32 *prv;

    /* kill the system timer if it's active */
    cancel_timer(timer);

    /* unlink it from our list */
    for (prv = 0, cur = sys_timer_head_ ; cur != 0 ;
         prv = cur, cur = cur->get_next())
    {
        /* if this is the one, unlink it */
        if (cur == timer)
        {
            /* unlink it from the list */
            if (prv == 0)
                sys_timer_head_ = cur->get_next();
            else
                prv->set_next(cur->get_next());

            /* no need to look any further */
            break;
        }
    }

    /* delete the timer object */
    delete timer;
}

/*
 *   Start the system timer for functions registered via
 *   register_time_func().  Since we can't start a timer until we have a
 *   window handle, we may need to start a timer when we create our
 *   window. 
 */
void CHtmlSysWin_win32::start_sys_timer()
{
    /* 
     *   if we don't have a window yet, we can't start the timer yet --
     *   we'll have to do this later when we actually create our window
     *   handle 
     */
    if (handle_ == 0)
        return;

    /* if there are no registered timer callbacks, ignore the request */
    if (timer_list_head_ == 0)
        return;
    
    /* allocate a system timer ID - if we can't, abort registration */
    cb_timer_id_ = alloc_timer_id();
    if (cb_timer_id_ == 0)
        return;
    
    /* start a system timer to go off once per second */
    if (SetTimer(handle_, cb_timer_id_, 1000, (TIMERPROC)0) == 0)
    {
        /* that failed - free our timer ID and give up */
        free_timer_id(cb_timer_id_);
        cb_timer_id_ = 0;
    }
}

/*
 *   Unregister a timer function 
 */
void CHtmlSysWin_win32::unregister_timer_func(void (*timer_func)(void *),
                                              void *func_ctx)
{
    htmlw32_timer_t *cur;
    htmlw32_timer_t *prv;
    
    /* find the given timer in our list of timer functions */
    for (prv = 0, cur = timer_list_head_ ; cur != 0 ;
         prv = cur, cur = cur->nxt_)
    {
        /* if this is the one, remove it */
        if (cur->func_ == timer_func && cur->ctx_ == func_ctx)
        {
            /* unlink the item from our list */
            if (prv == 0)
                timer_list_head_ = cur->nxt_;
            else
                prv->nxt_ = cur->nxt_;

            /* delete the record */
            delete cur;

            /* if the list is empty, kill the system timer */
            if (timer_list_head_ == 0)
            {
                /* remove the system timer */
                KillTimer(handle_, cb_timer_id_);

                /* release the timer identifier */
                free_timer_id(cb_timer_id_);
                cb_timer_id_ = 0;
            }

            /* done */
            return;
        }
    }
}

/*
 *   draw a horizontal rule 
 */
void CHtmlSysWin_win32::draw_hrule(const CHtmlRect *pos, int shade)
{
    HBRUSH shadow;
    HBRUSH hilite;
    RECT rc;
    CHtmlRect scrpos;

    /* convert the position to absolute coordinates */
    scrpos = doc_to_screen(*pos);

    /* set up a windows RECT structure for the position */
    SetRect(&rc, scrpos.left, scrpos.top, scrpos.right, scrpos.bottom);

    /* get system brushes for shadow and highlight colors */
    shadow = GetSysColorBrush(COLOR_3DSHADOW);
    hilite = GetSysColorBrush(COLOR_3DHILIGHT);

    /* see if we're shading */
    if (shade)
    {
        RECT lrc;
        
        /* fill the top line with shadow color */
        SetRect(&lrc, rc.left, rc.top, rc.right, rc.top + 1);
        FillRect(hdc_, &lrc, shadow);

        /* draw the left edge with shadow color */
        SetRect(&lrc, rc.left, rc.top, rc.left + 1, rc.bottom - 1);
        FillRect(hdc_, &lrc, shadow);

        /* fill the bottom line with hilite color */
        SetRect(&lrc, rc.left, rc.bottom - 1, rc.right, rc.bottom);
        FillRect(hdc_, &lrc, hilite);

        /* fill the right line with hilite color */
        SetRect(&lrc, rc.right - 1, rc.top, rc.right, rc.bottom);
        FillRect(hdc_, &lrc, hilite);
    }
    else
    {
        /* not shading - fill in the rectangle with the shadow color */
        FillRect(hdc_, &rc, shadow);
    }
}

/*
 *   Draw a table/cell background 
 */
void CHtmlSysWin_win32::draw_table_bkg(const CHtmlRect *pos,
                                       HTML_color_t bgcolor)
{
    HBRUSH hbr;
    LOGBRUSH lbr;
    RECT rc;
    CHtmlRect scrpos;

    /* create a brush with the given color */
    lbr.lbStyle = BS_SOLID;
    lbr.lbColor = HTML_color_to_COLORREF(bgcolor);
    lbr.lbHatch = 0;
    hbr = CreateBrushIndirect(&lbr);

    /* set up a Windows RECT structure in screen coordinates */
    scrpos = doc_to_screen(*pos);
    SetRect(&rc, scrpos.left, scrpos.top, scrpos.right, scrpos.bottom);

    /* fill the area with the given brush */
    FillRect(hdc_, &rc, hbr);

    /* we're done with the brush */
    DeleteObject(hbr);
}

/*
 *   Draw a table/cell border 
 */
void CHtmlSysWin_win32::draw_table_border(const CHtmlRect *pos, int width,
                                          int cell)
{
    HBRUSH shadow;
    HBRUSH hilite;
    HBRUSH tl_brush, br_brush;
    RECT rc;
    RECT lrc;
    CHtmlRect scrpos;

    /* convert the position to absolute coordinates */
    scrpos = doc_to_screen(*pos);

    /* set up a windows RECT structure for the position */
    SetRect(&rc, scrpos.left, scrpos.top, scrpos.right, scrpos.bottom);

    /* get system brushes for shadow and highlight colors */
    shadow = GetSysColorBrush(COLOR_3DSHADOW);
    hilite = GetSysColorBrush(COLOR_3DFACE);

    /* 
     *   select the drawing scheme according to whether we're drawing a table
     *   or a cell 
     */
    if (cell)
    {
        /* 
         *   cell - make these look indented, so use the shadow color for the
         *   top and left edges, and the highlight color for the bottom and
         *   right edges 
         */
        tl_brush = shadow;
        br_brush = hilite;
    }
    else
    {
        /* 
         *   table - make the whole table look embossed, so use the highlight
         *   color for the top and left edges, and the shadow color for the
         *   bottom and right edges 
         */
        tl_brush = hilite;
        br_brush = shadow;
    }

    /* draw the top and left lines in highlight color */
    SetRect(&lrc, rc.left, rc.top, rc.right - width, rc.top + width);
    FillRect(hdc_, &lrc, tl_brush);
    SetRect(&lrc, rc.left, rc.top, rc.left + width, rc.bottom - width);
    FillRect(hdc_, &lrc, tl_brush);

    /* draw the right and bottom lines in shadow color */
    SetRect(&lrc, rc.left, rc.bottom - width, rc.right, rc.bottom);
    FillRect(hdc_, &lrc, br_brush);
    SetRect(&lrc, rc.right - width, rc.top, rc.right, rc.bottom);
    FillRect(hdc_, &lrc, br_brush);

    /* 
     *   If the width is greater than 1, fill in the corners to make the
     *   edges meet diagonally.  We've already drawn the bottom and right
     *   lines so they fully overlap the corners, so just add little
     *   triangles extending from the left and bottom edges.  
     */
    if (!cell && width > 1)
    {
        HPEN null_pen;
        HPEN old_pen;
        HBRUSH old_brush;
        POINT pts[3];

        /* set up and select an empty pen */
        null_pen = CreatePen(PS_NULL, 0, 0);
        old_pen = (HPEN)SelectObject(hdc_, null_pen);

        /* select the top/left brush */
        old_brush = (HBRUSH)SelectObject(hdc_, tl_brush);

        /* draw the top right corner */
        pts[0].x = rc.right - width;  pts[0].y = rc.top;
        pts[1].x = rc.right;          pts[1].y = rc.top;
        pts[2].x = rc.right - width;  pts[2].y = rc.top + width;
        Polygon(hdc_, pts, 3);

        /* draw the bottom left corner */
        pts[0].x = rc.left;           pts[0].y = rc.bottom - width;
        pts[1].x = rc.left;           pts[1].y = rc.bottom;
        pts[2].x = rc.left + width;   pts[2].y = rc.bottom - width;
        Polygon(hdc_, pts, 3);

        /* restore the old pen and brush */
        SelectObject(hdc_, old_pen);
        SelectObject(hdc_, old_brush);
    }
}

/*
 *   get the default font for the window 
 */
CHtmlSysFont *CHtmlSysWin_win32::get_default_font()
{
    CHtmlFontDesc desc;

    /* get a font matching the current defaults */
    desc.htmlsize = 3;
    desc.default_charset = FALSE;
    desc.charset = owner_->owner_get_default_charset();
    return get_font(&desc);
}

/*
 *   Adjust the vertical scrollbar.  This is called whenever the
 *   formatter adds a new line. 
 */
void CHtmlSysWin_win32::fmt_adjust_vscroll()
{
    long max_y_pos;
    RECT rc;
    int moreht;
    int winht;
    long next_stop;
    long old_content_ht = content_height_;
    int scroll_cmd;
    long scroll_dst;
    int scroll_use_pos;

    /* presume we'll scroll to the bottom of the text */
    scroll_cmd = SB_BOTTOM;
    scroll_dst = 0;
    scroll_use_pos = 0;
    
    /* get the current vertical extent */
    max_y_pos = formatter_->get_max_y_pos();

    /*
     *   if we're currently reading a command, and our content height is
     *   already greater than the formatter's maximum y position, we must
     *   have deleted some text on the command line, causing the command
     *   line to take up fewer lines than it did before; ignore this type
     *   of shrinkage, because it would cause undesirable jumpiness by
     *   scrolling excessively during command line editing 
     */
    if (started_reading_cmd_ && (max_y_pos < content_height_))
        max_y_pos = content_height_;

    /* get the size of the "MORE" prompt */
    get_moreprompt_rect(&rc);
    moreht = rc.bottom - rc.top;

    /* get the vertical size of the window */
    get_scroll_area(&rc, TRUE);
    winht = rc.bottom - rc.top;

    /*
     *   Figure out how much room we have, minus the "MORE" prompt, from
     *   the last input position.  We don't want to show more information
     *   than this at once, to make sure the user has a chance to
     *   acknowledge the information we've displayed so far.
     */
    next_stop = last_input_height_ + winht - moreht;

    /*
     *   If there's room for the added information, extend our screen height
     *   and adjust scrollbars.  There's room only if we can fit this
     *   information onto the screen without scrolling the last input
     *   position off the screen.  If we've started reading a command, do not
     *   go into "MORE" mode, since the user has already seen everything that
     *   we've displayed so far, and must simply be scrolling back or
     *   resizing the window.
     *   
     *   As a special case, if our window height is very small, don't bother
     *   with more mode at all.  This probably indicates that we have a
     *   banner covering the whole window, in which case the game is probably
     *   doing something special and doesn't want normal "more" prompting.
     *   
     *   If we're in "non-stop" mode, do not pause for MORE prompts.
     *   Non-stop mode is used when we're reading a command script and want
     *   to read the whole thing without pausing.  
     */
    if (max_y_pos < next_stop
        || started_reading_cmd_
        || !more_mode_enabled_
        || winht < 10
        || nonstop_mode_)
    {
        /* it'll fit - include it in our content height */
        content_height_ = max_y_pos;

        /* we don't need to wait for user input to display more */
        if (!game_more_request_)
            set_more_mode(FALSE);
    }
    else if (prefs_->get_alt_more_style() || !auto_vscroll_)
    {
        /* 
         *   Either we're using the "alternative" more style, or we're not
         *   automatically scrolling to the bottom.  In either case, when the
         *   text overflows the window, just scroll to the next available
         *   position rather than to the bottom.  
         */
        content_height_ = max_y_pos;
        scroll_cmd = SB_THUMBPOSITION;
        scroll_dst = last_input_height_ / get_vscroll_units();
        scroll_use_pos = TRUE;

        /* we're in more mode */
        set_more_mode(TRUE);
    }
    else
    {
        /* go only up to the next stop */
        content_height_ = next_stop - moreht;
        
        /*
         *   there's more information than will fit on the screen --
         *   pause for user input before displaying more 
         */
        set_more_mode(TRUE);
    }

    /*
     *   Invalidate the window from the old content height to the new
     *   content height, since this area has changed 
     */
    get_scroll_area(&rc, TRUE);
    rc.top = doc_to_screen_y(old_content_ht);
    rc.bottom = doc_to_screen_y(content_height_);
    InvalidateRect(handle_, &rc, FALSE);

    /* adjust scrollbars accordingly */
    adjust_scrollbar_ranges();

    /* 
     *   scroll to the bottom of the available range if we're set up for
     *   auto vscroll mode 
     */
    if (auto_vscroll_)
        do_scroll(TRUE, vscroll_, scroll_cmd, scroll_dst, scroll_use_pos);

    /* check to see if we have started the command */
    check_cmd_start();
}

/*
 *   Load the command selection range from the formatter selection range.
 *   This should be called whenever the formatter range is updated
 *   directly, as when the user selects text using the mouse.  
 */
void CHtmlSysWin_win32::fmt_sel_to_cmd_sel(int caret_at_high_end)
{
    unsigned long inp_txt_ofs;
    unsigned long startofs, endofs;

    /* if we don't have a command tag yet, do nothing */
    if (cmdtag_ == 0)
    {
        /* 
         *   there's no command -- remember the selection direction, but
         *   otherwise there's nothing to do 
         */
        caret_at_right_ = caret_at_high_end;
        return;
    }

    /* get the current tag's text offset */
    inp_txt_ofs = cmdtag_->get_text_ofs();

    /* get the formatter selection range */
    formatter_->get_sel_range(&startofs, &endofs);

    /* check to see if the selection overlaps the command */
    if (endofs < inp_txt_ofs)
    {
        /*
         *   no overlap - move the caret to the end of the command, but
         *   hide it 
         */
        cmdbuf_->end_of_line(FALSE);
        cmdbuf_->hide_caret();
    }
    else if (startofs >= inp_txt_ofs)
    {
        /*
         *   The entire selection is in the command.  Select the range
         *   and set the caret to the appropriate end. 
         */
        cmdbuf_->set_sel_range(startofs - inp_txt_ofs, endofs - inp_txt_ofs,
                               caret_at_high_end ? endofs - inp_txt_ofs
                               : startofs - inp_txt_ofs);
    }
    else
    {
        /*
         *   The selection includes the command, but includes other text
         *   as well.  Select up to the end of the selection, and show the
         *   caret only if it's at the high end (which includes the
         *   command). 
         */
        cmdbuf_->set_sel_range(0, endofs - inp_txt_ofs, endofs - inp_txt_ofs);
        if (!caret_at_high_end)
            cmdbuf_->hide_caret();
    }

    /* remember the caret positioning */
    caret_at_right_ = caret_at_high_end;

    /* set the caret's displayed position */
    update_caret_pos(FALSE, FALSE);
}


/*
 *   Get the current scrolling position 
 */
void CHtmlSysWin_win32::get_scroll_doc_coords(class CHtmlRect *pos)
{
    RECT rc;
    
    /* get the scrolling area */
    get_scroll_area(&rc, TRUE);
    pos->set(rc.left, rc.top, rc.right, rc.bottom);

    /* adjust this to origin (0, 0) */
    pos->offset(-pos->left, -pos->top);

    /* 
     *   adjust to document coordinates - this will effectively give us
     *   the scrolling position, because document coordinates are offset
     *   from the screen coordinates by the scrolling position 
     */
    *pos = screen_to_doc(*pos);
}

/*
 *   Scroll a document position into view 
 */
void CHtmlSysWin_win32::scroll_to_doc_coords(const CHtmlRect *pos)
{
    RECT rc;
    CHtmlRect scrpos;

    /* get the current on-screen area */
    get_scroll_area(&rc, TRUE);

    /* get the desired position in screen coordinates */
    scrpos = doc_to_screen(*pos);
    
    /* scroll vertically if necessary */
    if (scrpos.top < rc.top || scrpos.bottom > rc.bottom)
    {
        long target;
        long delta;
        long new_vscroll;

        /* scroll so that the top is in the middle of the window */
        target = (rc.top + (rc.bottom - rc.top)/2);

        /* make sure we display as much of the rectangle as possible */
        if (target + scrpos.bottom - scrpos.top > rc.bottom)
        {
            target = scrpos.bottom - scrpos.top - rc.bottom;
            if (target < rc.top)
                target = rc.top;
        }

        /* figure out where we're going */
        delta = scrpos.top - target;
        new_vscroll = (vscroll_ofs_ * get_vscroll_units() + delta)
                      / get_vscroll_units();
        do_scroll(TRUE, vscroll_, SB_THUMBPOSITION, new_vscroll, TRUE);
    }

    /* do the same calculation horizontally */
    if (scrpos.left < rc.left || scrpos.right > rc.right)
    {
        long target;
        long delta;
        long new_hscroll;

        /* scroll so that the left is in the middle of the window */
        target = (rc.left + (rc.right - rc.left)/2);

        /* make sure we display as much of the rectangle as possible */
        if (target + scrpos.right - scrpos.left > rc.right)
        {
            target = scrpos.right - scrpos.left - rc.right;
            if (target < rc.left)
                target = rc.left;
        }

        /* figure out where we're going */
        delta = scrpos.left - target;
        new_hscroll = (hscroll_ofs_ * get_hscroll_units() + delta)
                      / get_hscroll_units();
        do_scroll(FALSE, hscroll_, SB_THUMBPOSITION, new_hscroll, TRUE);
    }
}

/*
 *   invalidate an area given in document coordinates 
 */
void CHtmlSysWin_win32::inval_doc_coords(const CHtmlRect *doc_area)
{
    CHtmlRect screen_area;
    RECT rc;

    /* adjust to window coordinates */
    screen_area = doc_to_screen(*doc_area);

    /* check for max values and adjust accordingly */
    GetClientRect(handle_, &rc);
    if (doc_area->right == HTMLSYSWIN_MAX_RIGHT)
        screen_area.right = rc.right;
    if (doc_area->bottom == HTMLSYSWIN_MAX_BOTTOM)
        screen_area.bottom = rc.bottom;

    /* 
     *   set the system rectangle - use a one-pixel overhang to ensure that
     *   we invalidate the entire area if the input rectangle is inclusive of
     *   its endpoints 
     */
    SetRect(&rc, screen_area.left, screen_area.top,
            screen_area.right + 1, screen_area.bottom + 1);

    /* invalidate the area on the screen */
    InvalidateRect(handle_, &rc, FALSE);
}

/*
 *   Receive notification that the formatter is clearing the display list.
 *   We must forget any display list items that we're referencing (such as
 *   the link the mouse is hovering over). 
 */
void CHtmlSysWin_win32::advise_clearing_disp_list()
{
    /* forget any link we're tracking */
    track_link_ = 0;

    /* 
     *   Forget any link we're hovering over.  Note that it shouldn't be
     *   necessary to update the window at this point -- the formatter is
     *   about to rebuild the display list, so the formatter should take
     *   care of any updating that needs to be done. 
     */
    status_link_ = 0;
}


/*
 *   Remove all banners 
 */
void CHtmlSysWin_win32::remove_all_banners()
{
    /* tell the formatter to remove the banners */
    formatter_->remove_all_banners(FALSE);
}

/*
 *   Do a complete reformatting of the window 
 */
void CHtmlSysWin_win32::do_reformat(int show_status, int freeze_display,
                                    int reset_sounds)
{
    /* forget any tracking links */
    status_link_ = track_link_ = 0;

    /* we're obviously not in MORE mode yet */
    set_more_mode(FALSE);

    /* restart the formatter */
    formatter_->start_at_top(reset_sounds);

    /* make sure we redraw everything */
    InvalidateRect(handle_, 0, FALSE);

    /* format the window contents */
    do_formatting(show_status, !freeze_display, freeze_display);

    /* adjust everything for the reformatting */
    adjust_for_reformat();
}

/*
 *   Schedule reformatting 
 */
void CHtmlSysWin_win32::schedule_reformat(int show_status, int freeze_display,
                                          int reset_sounds)
{
    MSG msg;
    WPARAM wpar;
    
    /* 
     *   if we already have a reformat message pending, ignore this
     *   request
     */
    if (PeekMessage(&msg, handle_, HTMLM_REFORMAT, HTMLM_REFORMAT,
                    PM_NOREMOVE))
        return;

    /* build the WPARAM from the flags */
    wpar = 0;
    if (show_status) wpar |= HTML_F_SHOW_STAT;
    if (freeze_display) wpar |= HTML_F_FREEZE_DISP;
    if (reset_sounds) wpar |= HTML_F_RESET_SND;

    /* post a REFORMAT message to myself */
    PostMessage(handle_, HTMLM_REFORMAT, wpar, 0);
}

/*
 *   Receive notification of a change in the sound preferences 
 */
void CHtmlSysWin_win32::notify_sound_pref_change()
{
    /* 
     *   if we're turning sounds on, and we don't have a valid DirectSound
     *   object, warn about it and leave sounds disabled 
     */
    if (get_directsound() == 0 && prefs_->get_sounds_on())
    {
        /* warn that there's no direct sound available */
        owner_->owner_show_directx_warning();

        /* turn sounds back off */
        prefs_->set_sounds_on(FALSE);
    }

    /* 
     *   if sound effects are now turned off, cancel sound playback in the
     *   effects layers 
     */
    if (!prefs_->get_sounds_on())
    {
        formatter_->cancel_sound(HTML_Attrib_ambient);
        formatter_->cancel_sound(HTML_Attrib_bgambient);
        formatter_->cancel_sound(HTML_Attrib_foreground);
    }

    /* if music is now turned off, cancel playback in the music layer */
    if (!prefs_->get_music_on())
        formatter_->cancel_sound(HTML_Attrib_background);
}

/*
 *   Schedule reloading the game chest data 
 */
void CHtmlSysWin_win32::schedule_reload_game_chest()
{
    MSG msg;

    /* if we already have a reload pending, ignore this request */
    if (PeekMessage(&msg, handle_, HTMLM_RELOAD_GC, HTMLM_RELOAD_GC,
                    PM_NOREMOVE))
        return;

    /* post a REFORMAT message to myself */
    PostMessage(handle_, HTMLM_RELOAD_GC, 0, 0);
}

/*
 *   Save the game chest database to the given file 
 */
void CHtmlSysWin_win32::save_game_chest_db_as(const char *fname)
{
    /* ask the owner to do the work */
    owner_->owner_write_game_chest_db(fname);
}


/*
 *   Run the formatter
 */
int CHtmlSysWin_win32::do_formatting(int show_status, int update_win,
                                     int freeze_display)
{
    RECT rc;
    int drawn;
    unsigned long win_bottom;
    HCURSOR old_cursor;
    DWORD start_ticks;

    /* note the starting tick count */
    start_ticks = GetTickCount();

    /* if desired, freeze display updating while we're working */
    if (freeze_display)
        formatter_->freeze_display(TRUE);

    /* 
     *   redraw the window before we start, so that something happens
     *   immediately and the window doesn't look frozen 
     */
    if (update_win)
        UpdateWindow(handle_);

    /* 
     *   Get the window area in document coordinates, so we'll know when
     *   we've formatted past the bottom of the current display area.
     *   Note that we'll ignore the presence or absence of a horizontal
     *   scrollbar, since it could come or go while we're formatting; by
     *   assuming that it won't be there, we'll be maximally conservative
     *   about redrawing the whole area.  
     */
    GetClientRect(handle_, &rc);
    win_bottom = screen_to_doc_y(rc.bottom);
    
    /* we don't have enough formatting done yet to draw the window */
    drawn = FALSE;

    /* reformat everything, drawing as soon as possible, if desired */
    if (update_win)
    {
        while (formatter_->more_to_do() && !drawn)
        {
            /* 
             *   If the caller wanted us to show busy status, and more
             *   than a brief time has elapsed since we started, show
             *   "wait" cursor and status message.  We delay showing these
             *   for a little while so that we don't flash busy status on
             *   and off during repeated short refreshes.  
             */
            if (show_status && !formatting_msg_
                && GetTickCount() > start_ticks + 200)
            {
                old_cursor = SetCursor(wait_cursor_);
                formatting_msg_ = TRUE;
                if (statusline_ != 0)
                    statusline_->source_to_front(this);
            }

            /* format another line */
            formatter_->do_formatting();
            
            /* 
             *   if we have enough content to do so, redraw the window;
             *   we're not really done with the formatting yet, but at
             *   least we'll update the window as soon as we can, so the
             *   user isn't staring at a blank window longer than
             *   necessary 
             */
            if (formatter_->get_max_y_pos() >= win_bottom)
            {
                /* 
                 *   if the formatter's updating is frozen, invalidate the
                 *   window -- the formatter won't have been invalidating
                 *   it as it goes 
                 */
                if (freeze_display)
                    InvalidateRect(handle_, 0, FALSE);
                
                /* redraw the window immediately */
                UpdateWindow(handle_);
                
                /* we've now drawn it */
                drawn = TRUE;
            }
        }
    }
    
    /* after drawing, keep going until we've formatted everything */
    while (formatter_->more_to_do())
    {
        /* 
         *   If the caller wanted us to show busy status, and more than a
         *   brief time has elapsed since we started, show "wait" cursor
         *   and status message.  We delay showing these for a little
         *   while so that we don't flash busy status on and off during
         *   repeated short refreshes.  
         */
        if (show_status && !formatting_msg_
            && GetTickCount() > start_ticks + 200)
        {
            old_cursor = SetCursor(wait_cursor_);
            formatting_msg_ = TRUE;
            if (statusline_ != 0)
                statusline_->source_to_front(this);
        }

        /* format the next line */
        formatter_->do_formatting();
    }

    /* 
     *   restore the original cursor and status line, if we showed a
     *   visual indication of our busy status 
     */
    if (formatting_msg_)
    {
        SetCursor(old_cursor);
        formatting_msg_ = FALSE;
        if (statusline_ != 0)
            statusline_->update();
    }

    /* unfreeze the display if we froze it */
    if (freeze_display)
        formatter_->freeze_display(FALSE);

    /* 
     *   If we didn't do any drawing, and we updated the window coming in,
     *   invalidate the window - the initial redraw will have validated
     *   everything, but we've just made it all invalid again and didn't
     *   get around to drawing it.  Do the same thing if we froze the
     *   display and we didn't do any updating.  
     */
    if (update_win && !drawn)
        InvalidateRect(handle_, 0, FALSE);
    else if (freeze_display)
        InvalidateRect(handle_, 0, FALSE);

    /* return an indication of whether we did any updating */
    return drawn;
}

/*
 *   Recalculate the palette.  The formatter calls this routine whenever
 *   it adds an image to the initial images list.  We'll post ourselves a
 *   palette recalculation message, so that when we're all done with the
 *   current set of changes we'll update the palette, accounting for all
 *   images that are in the list at that point.
 *   
 *   Note that we'll post the message to our owner frame, so that the
 *   message is processed at the application frame level rather than
 *   within an individual subwindow.  
 */
void CHtmlSysWin_win32::recalc_palette()
{
    MSG msg;
    HWND handle;

    /* get our owner frame handle */
    handle = owner_->get_owner_frame_handle();

    /* post a palette update message if there isn't one pending */
    if (!PeekMessage(&msg, handle, WM_QUERYNEWPALETTE, WM_QUERYNEWPALETTE,
                     PM_NOREMOVE))
        PostMessage(handle, WM_QUERYNEWPALETTE, 0, 0);
}

/*
 *   recalculate the palette for the foreground object 
 */
int CHtmlSysWin_win32::recalc_palette_fg()
{
    HDC dc;
    HPALETTE oldpal;
    int ret;

    /* if we're not in a paletted video mode, do nothing */
    if (get_bits_per_pixel() > 8)
        return FALSE;

    /* get my device context */
    dc = GetDC(get_handle());

    /* realize our fixed palette in the foreground */
    oldpal = SelectPalette(dc, hpal_, FALSE);
    ret = RealizePalette(dc);

    /* re-realize the old palette in the background */
    SelectPalette(dc, oldpal, TRUE);
    RealizePalette(dc);

    /* done with the DC */
    ReleaseDC(get_handle(), dc);

    /* return the change indication */
    return ret;
}

/*
 *   recalculate the palette for the background objects 
 */
int CHtmlSysWin_win32::recalc_palette_bg()
{
    /* 
     *   we have only one fixed palette, so there's nothing we need to do -
     *   there are no separate palettes for our background objects 
     */
    return FALSE;
}


/*
 *   Set the window's title 
 */
void CHtmlSysWin_win32::set_window_title(const textchar_t *title)
{
    /* set the caption of my parent, which is the main window */
    SetWindowText(parent_->get_handle(), title);
}

/*
 *   Set the background image 
 */
void CHtmlSysWin_win32::set_html_bg_image(CHtmlResCacheObject *image)
{
    /* if we're changing anything, schedule a redraw */
    if (image != bg_image_)
        InvalidateRect(handle_, 0, FALSE);

    /* add a reference to the new image, if there is a new image */
    if (image != 0)
        image->add_ref();

    /* if we currently have a background image, remove our reference on it */
    if (bg_image_ != 0)
        bg_image_->remove_ref();

    /* remember the new image */
    bg_image_ = image;
}

/*
 *   Invalidate a portion of the background image 
 */
void CHtmlSysWin_win32::inval_html_bg_image(unsigned int x, unsigned int y,
                                            unsigned int wid, unsigned int ht)
{
    CHtmlRect area;
    RECT clirc;
    CHtmlSysImage *image;
    int img_wid, img_ht;
    int xtile, ytile;

    /* if we don't have an image, don't bother */
    if (bg_image_ == 0 || (image = bg_image_->get_image()) == 0)
        return;

    /* get the size of the image, so we can calculate its tiling */
    img_wid = image->get_width();
    img_ht = image->get_height();

    /* if the image is of zero size, there's nothing to invalidate */
    if (img_wid == 0 || img_ht == 0)
        return;

    /* get our entire window's client area */
    GetClientRect(handle_, &clirc);

    /* get the visible area in document coordinates */
    area.set(clirc.left, clirc.top, clirc.right, clirc.bottom);
    area = screen_to_doc(area);

    /* 
     *   calculate, in document coordinates, the location of the first tile
     *   visible in the window: we tile the image from the upper left corner
     *   (coordinates 0,0) of the document coordinate space 
     */
    area.left = (area.left / img_wid) * img_wid;
    area.right = area.left + (clirc.right - clirc.left);
    area.top = (area.top / img_ht) * img_ht;
    area.bottom = area.top + (clirc.bottom - clirc.top);

    /* adjust back to window coordinates */
    area = doc_to_screen(area);

    /* loop over the tiled areas until we're outside the visible window */
    for (ytile = area.top ; ytile < area.bottom ; ytile += img_ht)
    {
        /* tile across the current row */
        for (xtile = area.left ; xtile < area.right ; xtile += img_wid)
        {
            RECT rc;

            /* invalidate the requested area within the current tile */
            SetRect(&rc, xtile + x, ytile + y,
                    xtile + x + wid, ytile + y + ht);
            InvalidateRect(handle_, &rc, FALSE);
        }
    }
}

/*
 *   Set the default background color 
 */
void CHtmlSysWin_win32::set_html_bg_color(HTML_color_t color, int use_default)
{
    COLORREF rgb;

    if (use_default || prefs_->get_override_colors())
    {
        /* ignore the provided color and use the default setting */
        if (prefs_->get_use_win_colors())
            rgb = GetSysColor(COLOR_WINDOW);
        else
            rgb = HTML_color_to_COLORREF(prefs_->get_bg_color());
    }
    else
    {
        /* use the color they provided */
        rgb = HTML_color_to_COLORREF(color);
    }

    /* set the color */
    internal_set_bg_color(rgb);
}


/*
 *   Note a change to the debugger color scheme.  The debugger will call this
 *   on its window subclasses that it uses to display debugger windows
 *   (source windows, debug log, stack).  
 */
void CHtmlSysWin_win32::note_debug_format_changes(
    HTML_color_t bkg_color, HTML_color_t text_color, int use_windows_colors)
{
    COLORREF bkg_rgb, text_rgb;

    /* if we're using the windows colors, get the defaults */
    if (use_windows_colors)
    {
        bkg_rgb = GetSysColor(COLOR_WINDOW);
        text_rgb = GetSysColor(COLOR_WINDOWTEXT);
    }
    else
    {
        bkg_rgb = HTML_color_to_COLORREF(bkg_color);
        text_rgb = HTML_color_to_COLORREF(text_color);
    }

    /* set the colors */
    internal_set_bg_color(bkg_rgb);
    internal_set_text_color(text_rgb);
}

/*
 *   Internal routine to set the background color 
 */
void CHtmlSysWin_win32::internal_set_bg_color(COLORREF rgb)
{
    LOGBRUSH lbr;

    /* if the color isn't changing, there's nothing more to do */
    if (bgbrush_ != 0 && rgb == bgcolor_)
        return;

    /* remember the new color */
    bgcolor_ = rgb;

    /* discard the old background brush */
    if (bgbrush_ != 0)
        DeleteObject(bgbrush_);

    /* create a new background brush */
    lbr.lbStyle = BS_SOLID;
    lbr.lbColor = rgb;
    lbr.lbHatch = 0;
    bgbrush_ = CreateBrushIndirect(&lbr);

    /* redraw the whole window so we draw in the new color */
    if (handle_ != 0)
        InvalidateRect(handle_, 0, FALSE);
}

/*
 *   Set the default text color 
 */
void CHtmlSysWin_win32::set_html_text_color(HTML_color_t color,
                                            int use_default)
{
    COLORREF new_color;
    
    if (use_default || prefs_->get_override_colors())
    {
        /* ignore the provided color and use the default setting */
        if (prefs_->get_use_win_colors())
            new_color = GetSysColor(COLOR_WINDOWTEXT);
        else
            new_color = HTML_color_to_COLORREF(prefs_->get_text_color());
    }
    else
    {
        /* use the color they provided */
        new_color = HTML_color_to_COLORREF(color);
    }

    /* set the color */
    internal_set_text_color(new_color);
}

/*
 *   Internal routine to set the text color 
 */
void CHtmlSysWin_win32::internal_set_text_color(COLORREF new_color)
{
    /* if we're not changing the color, there's nothing to do */
    if (new_color == text_color_)
        return;

    /* set the new color */
    text_color_ = new_color;

    /* invalidate the whole window so we draw in the new color */
    if (handle_ != 0)
        InvalidateRect(handle_, 0, FALSE);
}

/*
 *   Set the link colors 
 */
void CHtmlSysWin_win32::set_html_link_colors(HTML_color_t link_color,
                                             int link_use_default,
                                             HTML_color_t vlink_color,
                                             int vlink_use_default,
                                             HTML_color_t alink_color,
                                             int alink_use_default,
                                             HTML_color_t hlink_color,
                                             int hlink_use_default)
{
    COLORREF new_color;
    int changed;

    /* presume nothing will change */
    changed = FALSE;

    /* set the new main link color */
    if (link_use_default)
    {
        /* ignore the provided color and use the preferences setting */
        new_color = HTML_color_to_COLORREF(prefs_->get_link_color());
    }
    else
    {
        new_color = HTML_color_to_COLORREF(link_color);
    }
    if (new_color != link_color_)
    {
        changed = TRUE;
        link_color_ = new_color;
    }

    /* set the new "visited link" color */
    if (vlink_use_default)
    {
        /* ignore the provided color and use the preferences setting */
        new_color = HTML_color_to_COLORREF(prefs_->get_vlink_color());
    }
    else
    {
        new_color = HTML_color_to_COLORREF(vlink_color);
    }
    if (new_color != vlink_color_)
    {
        changed = TRUE;
        vlink_color_ = new_color;
    }

    /* set the new "active" link color */
    if (alink_use_default)
    {
        /* ignore the provided alink color and use the preferences setting */
        new_color = HTML_color_to_COLORREF(prefs_->get_alink_color());
    }
    else
    {
        new_color = HTML_color_to_COLORREF(alink_color);
    }
    if (new_color != alink_color_)
    {
        changed = TRUE;
        alink_color_ = new_color;
    }

    /* set the new "hover" link color */
    if (hlink_use_default)
    {
        /* ignore the provided hlink color and use the preferences setting */
        new_color = HTML_color_to_COLORREF(prefs_->get_hlink_color());
    }
    else
    {
        new_color = HTML_color_to_COLORREF(hlink_color);
    }
    if (hlink_color_ != new_color)
    {
        changed = TRUE;
        hlink_color_ = new_color;
    }

    /* update the window if needed */
    if (handle_ != 0 && changed)
        InvalidateRect(handle_, 0, FALSE);
}

/*
 *   Map a parameterized system color 
 */
HTML_color_t CHtmlSysWin_win32::map_system_color(HTML_color_t color)
{
    switch(color)
    {
    case HTML_COLOR_STATUSBG:
        return prefs_->get_color_status_bg();
        
    case HTML_COLOR_STATUSTEXT:
        return prefs_->get_color_status_text();

    case HTML_COLOR_LINK:
        return prefs_->get_link_color();

    case HTML_COLOR_ALINK:
        return prefs_->get_alink_color();

    case HTML_COLOR_HLINK:
        return prefs_->get_hlink_color();

    case HTML_COLOR_TEXT:
        return prefs_->get_use_win_colors()
            ? COLORREF_to_HTML_color(GetSysColor(COLOR_WINDOWTEXT))
            : prefs_->get_text_color();

    case HTML_COLOR_BGCOLOR:
        return prefs_->get_use_win_colors()
            ? COLORREF_to_HTML_color(GetSysColor(COLOR_WINDOW))
            : prefs_->get_bg_color();

    case HTML_COLOR_INPUT:
        return prefs_->get_inpfont_color();

    default:
        /* return black for other colors */
        return 0x00000000;
    }
}

/*
 *   Get the color for hyperlinked text with the mouse hovering over it. 
 */
HTML_color_t CHtmlSysWin_win32::get_html_hlink_color() const
{
    /*
     *   If hover highlighting is turned on, return the hover highlight
     *   color.  Otherwise, use the ordinary link color. 
     */
    return COLORREF_to_HTML_color(prefs_->get_hover_hilite()
                                  ? hlink_color_
                                  : link_color_);
}

/*
 *   determine if links should be underlined 
 */
int CHtmlSysWin_win32::get_html_link_underline() const
{
    return prefs_->get_underline_links();
}

/*
 *   Set the temporary link visibility 
 */
void CHtmlSysWin_win32::set_temp_show_links(int flag)
{
    /* 
     *   if we're cancelling visibility, and we have a timer that hasn't
     *   elapsed yet, kill the timer, since we don't need to receive the
     *   notification after all 
     */
    if (!flag && temp_link_timer_id_ != 0)
    {
        KillTimer(handle_, temp_link_timer_id_);
        free_timer_id(temp_link_timer_id_);
        temp_link_timer_id_ = 0;
    }
    
    /* update the window if the visibility has changed */
    if (owner_ != 0 && (flag != 0) != (owner_->get_temp_show_links() != 0))
    {
        /* set the new visibility */
        owner_->set_temp_show_links(flag);

        /* redraw links, since they'll look different now */
        owner_->owner_inval_links_on_screen();
    }
}

/*
 *   invalidate all visible links - this should be called whenever link
 *   visibility changes 
 */
void CHtmlSysWin_win32::inval_links_on_screen()
{
    RECT rc;
    CHtmlRect area;

    /* get the visible area of the window in document coordinates */
    get_scroll_area(&rc, TRUE);
    area = screen_to_doc(CHtmlRect(rc.left, rc.top, rc.right, rc.bottom));

    /* invalidate all links within the screen area */
    formatter_->inval_links_on_screen(&area);
}

/*
 *   determine if links are to be shown as links (if not, they're shown as
 *   ordinary text) 
 */
int CHtmlSysWin_win32::get_html_show_links() const
{
    return prefs_->get_links_on() || owner_->get_temp_show_links();
}

/*
 *   determine if we're allowed to show graphics 
 */
int CHtmlSysWin_win32::get_html_show_graphics() const
{
    return prefs_->get_graphics_on();
}

/*
 *   Set the size of this banner window 
 */
void CHtmlSysWin_win32::set_banner_size(long width,
                                        HTML_BannerWin_Units_t width_units,
                                        int use_width,
                                        long height,
                                        HTML_BannerWin_Units_t height_units,
                                        int use_height)
{
    /* remember our new settings */
    if (use_width)
    {
        banner_width_ = width;
        banner_width_units_ = width_units;
    }
    if (use_height)
    {
        banner_height_ = height;
        banner_height_units_ = height_units;
    }

    /* notify the owner window, so it can recalculate the layout */
    owner_->on_set_banner_size(this);
}

/*
 *   Set the border and alignment settings of this banner window 
 */
void CHtmlSysWin_win32::set_banner_info(HTML_BannerWin_Pos_t pos,
                                        unsigned long style)
{
    /* set the new border flag */
    set_has_border((style & OS_BANNER_STYLE_BORDER) != 0);

    /* set the new auto-vscroll flag */
    auto_vscroll_ = ((style & OS_BANNER_STYLE_AUTO_VSCROLL) != 0);

    /* note the new MORE mode setting */
    more_mode_enabled_ = ((style & OS_BANNER_STYLE_MOREMODE) != 0);

    /* MORE mode implies auto-vscroll mode */
    if (more_mode_enabled_)
        auto_vscroll_ = TRUE;

    /* set the new alignemnt */
    banner_alignment_ = pos;

    /* NB: ignore the scrollbar changes (for now, anyway) */
}

/*
 *   set our banner border status 
 */
void CHtmlSysWin_win32::set_has_border(int has_border)
{
    /* if the border status is changing, update it */
    if ((unsigned)has_border != has_border_)
    {
        /* remember the new border status */
        has_border_ = has_border;

        /* show or hide the border window accordingly */
        ShowWindow(border_handle_, has_border_ ? SW_SHOW : SW_HIDE);

        /* recalculate the banner layout for the border change */
        owner_->on_set_banner_size(this);
    }
}

/*
 *   Get information on the banner window 
 */
void CHtmlSysWin_win32::get_banner_info(HTML_BannerWin_Pos_t *pos,
                                        unsigned long *style)
{
    /* set the alignment */
    *pos = banner_alignment_;

    /* set the style flags */
    *style = 0;
    if (has_border_)
        *style |= OS_BANNER_STYLE_BORDER;
    if (vscroll_ != 0)
        *style |= OS_BANNER_STYLE_VSCROLL;
    if (hscroll_ != 0)
        *style |= OS_BANNER_STYLE_HSCROLL;
    if (auto_vscroll_)
        *style |= OS_BANNER_STYLE_AUTO_VSCROLL;
    if (more_mode_enabled_)
        *style |= OS_BANNER_STYLE_MOREMODE;

    /* we provide full HTML interpretation, so we support <TAB> */
    *style |= OS_BANNER_STYLE_TAB_ALIGN;
}


/* ------------------------------------------------------------------------ */
/*
 *   HTML History window implementation 
 */

/*
 *   Handle a keystroke.  When we get a keystroke in a history window, send
 *   it to the main window.  
 */
int CHtmlSysWin_win32_Hist::do_char(TCHAR c, long keydata)
{
    CHtmlSysWin_win32 *win;
    
    /* send it to the main window, if there is one */
    if (owner_ != 0 && (win = owner_->get_active_page_win()) != 0)
        return win->do_char(c, keydata);
    else
        return TRUE;
}

/*
 *   handle a keystroke 
 */
int CHtmlSysWin_win32_Hist::do_keydown(int vkey, long keydata)
{
    CHtmlSysWin_win32 *win;

    /* send it to the main window, if there is one */
    if (owner_ != 0 && (win = owner_->get_active_page_win()) != 0)
        return win->do_keydown(vkey, keydata);
    else
        return TRUE;
}

/* 
 *   process a command 
 */
int CHtmlSysWin_win32_Hist::do_command(int notify_code, int cmd, HWND ctl)
{
    CHtmlSysWin_win32 *win;

    /* check the command */
    switch (cmd)
    {
    case ID_EDIT_PASTE:
        /* send these to the main panel for processing */
        if (owner_ != 0 && (win = owner_->get_active_page_win()) != 0)
            return win->do_command(notify_code, cmd, ctl);
        break;

    default:
        /* use the inherited handling for others */
        break;
    }

    /* if we haven't already handled it, use the inherited handling */
    return CHtmlSysWin_win32::do_command(notify_code, cmd, ctl);
}

/*
 *   check command status 
 */
TadsCmdStat_t CHtmlSysWin_win32_Hist::check_command(int cmd)
{
    CHtmlSysWin_win32 *win;

    /* check the command */
    switch (cmd)
    {
    case ID_EDIT_PASTE:
        /* send these to the main panel for processing */
        if (owner_ != 0 && (win = owner_->get_active_page_win()) != 0)
            return win->check_command(cmd);
        break;

    default:
        /* use the inherited handling for others */
        break;
    }

    /* if we haven't already handled it, use the inherited handling */
    return CHtmlSysWin_win32::check_command(cmd);
}

/* ------------------------------------------------------------------------ */
/*
 *   HTML Input window implementation 
 */

CHtmlSysWin_win32_Input::CHtmlSysWin_win32_Input(
    CHtmlFormatterInput *formatter, CHtmlSysWin_win32_owner *owner,
    int has_vscroll, int has_hscroll, CHtmlPreferences *prefs)
    : CHtmlSysWin_win32(formatter, owner, has_vscroll, has_hscroll,
                        prefs)
{
    /* enable MORE mode in an input window */
    more_mode_enabled_ = TRUE;

    /* load accelerator tables */
    accel_emacs_ = LoadAccelerators(CTadsApp::get_app()->get_instance(),
                                    MAKEINTRESOURCE(IDR_ACCEL_EMACS));
    accel_win_ = LoadAccelerators(CTadsApp::get_app()->get_instance(),
                                  MAKEINTRESOURCE(IDR_ACCEL_WIN));

    /* set the correct accelerator for the current preferences */
    set_current_accel();

    /* use the caret in this window */
    caret_enabled_ = TRUE;

    /* we're not yet in single-key mode */
    waiting_for_key_ = FALSE;
    last_keystroke_ = 0;
    last_keystroke_cmd_ = 0;

    /* not waiting for an event */
    waiting_for_evt_ = FALSE;
    last_evt_info_ = 0;

    /* no input timeout timer yet */
    input_timer_id_ = 0;

    /* not at end of file yet */
    eof_ = FALSE;

    /* no game has started yet */
    game_started_ = FALSE;
    game_over_ = FALSE;

    /* not in game chest yet */
    in_game_chest_ = FALSE;

    /* no drop caret yet */
    drop_caret_ = 0;
    drop_caret_vis_ = FALSE;

    /* register the "UniformResourceLocator" clipboard format */
    url_clipboard_fmt_ = RegisterClipboardFormat("UniformResourceLocator");

    /* no input is in progress */
    input_in_progress_ = FALSE;

    /* load our subclass-specific status strings */
    load_res_str(&no_game_status_str_, IDS_NO_GAME_MSG);
    load_res_str(&game_over_status_str_, IDS_GAME_OVER_MSG);
}

/*
 *   get the starting position for a 'find' command 
 */
void CHtmlSysWin_win32_Input::find_get_start(int start_at_top,
                                             unsigned long *sel_start,
                                             unsigned long *sel_end)
{
    /* 
     *   if the selection is zero-length and is in the active command line,
     *   start at the top automatically; otherwise, use the normal starting
     *   point 
     */
    get_formatter()->get_sel_range(sel_start, sel_end);
    if (cmdtag_ != 0
        && *sel_start == *sel_end
        && *sel_start >= cmdtag_->get_text_ofs())
    {
        /* start at the top */
        *sel_start = *sel_end = 0;
    }
    else
    {
        /* use the normal starting point */
        CHtmlSysWin_win32::find_get_start(start_at_top, sel_start, sel_end);
    }
}


/*
 *   get our status message
 */
textchar_t *CHtmlSysWin_win32_Input::get_status_message(int *caller_deletes)
{
    textchar_t *msg;
    
    /* try inheriting a default message first */
    msg = CHtmlSysWin_win32::get_status_message(caller_deletes);
    if (msg != 0)
        return msg;

    /* presume the caller won't have to delete the message */
    *caller_deletes = FALSE;

    /* check our status flags */
    if (in_game_chest_)
        msg = 0;
    else if (!game_started_)
        msg = no_game_status_str_.get();
    else if (game_over_)
        msg = game_over_status_str_.get();
    else
        msg = 0;

    /* return the message */
    return msg;
}

/*
 *   notification of system window creation 
 */
void CHtmlSysWin_win32_Input::do_create()
{
    /* inherit default handling */
    CHtmlSysWin_win32::do_create();

    /* register as a drop target */
    drop_target_register();
}

/* 
 *   notification that the window is being destroyed 
 */
void CHtmlSysWin_win32_Input::do_destroy()
{
    /* inherit default handling */
    CHtmlSysWin_win32::do_destroy();
}

/*
 *   determine if links are to be shown as links (if not, they're shown as
 *   ordinary text) 
 */
int CHtmlSysWin_win32_Input::get_html_show_links() const
{
    /* 
     *   return the inherited setting, but always show links on the game
     *   chest page 
     */
    return in_game_chest_ || CHtmlSysWin_win32::get_html_show_links();
}

/*
 *   Handle a user message 
 */
int CHtmlSysWin_win32_Input::do_user_message(
    int msg, WPARAM wpar, LPARAM lpar)
{
    /* check messages */
    switch(msg)
    {
    case HTMLM_RELOAD_GC:
        /* reload the game chest data */
        if (in_game_chest_)
            owner_->owner_maybe_reload_game_chest();

        /* handled */
        return TRUE;

    default:
        /* not handled - go use default handling */
        break;
    }

    /* inherit default handling */
    return CHtmlSysWin_win32::do_user_message(msg, wpar, lpar);
}

/*
 *   handle timer events 
 */
int CHtmlSysWin_win32_Input::do_timer(int timer_id)
{
    if (timer_id == input_timer_id_)
    {
        /*
         *   it's the input timer - if we're waiting for input, set the
         *   flag indicating that the input timed out, and cancel the
         *   current input wait 
         */
        waiting_for_evt_timeout_ = TRUE;
        cancel_input_wait();

        /* handled */
        return TRUE;
    }
    else
    {
        /* not one of ours - inherit default handling */
        return CHtmlSysWin_win32::do_timer(timer_id);
    }
}

/*
 *   set the end-of-file flag 
 */
void CHtmlSysWin_win32_Input::set_eof_flag(int f)
{
    /* set the new flag status */
    eof_ = f;

    /* set the EOF flag for MORE mode as well */
    eof_more_mode_ = f;

    /* 
     *   if we're now at end of file, and we're awaiting some sort of
     *   input, immediately end the input wait
     */
    if (eof_)
        cancel_input_wait();

    /* cancel MORE mode in all windows */
    if (owner_ != 0)
        owner_->release_all_moremode();
}

/*
 *   Cancel the current input wait, if any 
 */
void CHtmlSysWin_win32_Input::cancel_input_wait()
{
    /* if we're reading a command line, indicate that it's finished */
    if (cmdtag_ != 0)
        command_read_ = TRUE;

    /* if we're waiting for a key or an event, return a BREAK command */
    set_key_event(0, CMD_BREAK);

    /* if we're waiting for a 'more' prompt, release 'more' mode */
    if (more_mode_)
    {
        /* release MORE mode */
        released_more_ = TRUE;
        more_mode_ = FALSE;

        /* notify the owner that we're no longer in MORE mode */
        if (owner_ != 0)
            owner_->remove_more_prompt_win(this);
    }
}


/*
 *   Set the correct accelerator table for the current preferences 
 */
void CHtmlSysWin_win32_Input::set_current_accel()
{
    textchar_t paste_key;
    
    /* check the Ctrl+V preferences */
    if (prefs_->get_emacs_ctrl_v())
    {
        /* emacs-style Ctrl+V */
        current_accel_ = accel_emacs_;
        paste_key = 'Y';
    }
    else
    {
        /* Windows-style Ctrl+V */
        current_accel_ = accel_win_;
        paste_key = 'V';
    }

    /* set the Edit/Paste menu item text for the change */
    owner_->set_paste_accel(paste_key);
}

/*
 *   Set the caret size for a given font 
 */
void CHtmlSysWin_win32::set_caret_size(CHtmlSysFont *font)
{
    CHtmlFontMetrics fm;

    /* hide the caret */
    hide_caret();

    /* get the font metrics */
    font->get_font_metrics(&fm);

    /* set the total size of the caret to the total size of the font */
    caret_ht_ = fm.total_height;

    /* set the visible portion of the caret to the ascender height */
    caret_ascent_ = fm.ascender_height;

    /* show the caret */
    show_caret();
}

/*
 *   Update the display after changing the command input buffer contents or
 *   length 
 */
void CHtmlSysWin_win32_Input::update_input_display()
{
    /* make sure we're on the active page */
    owner_->go_to_active_page();

    /* update the formatter display, if there's a valid input tag */
    if (cmdtag_ != 0)
    {
        cmdtag_->setlen((CHtmlFormatterInput *)formatter_, cmdbuf_->getlen());
        cmdtag_->format(this, formatter_);
    }

    /* update the cursor position */
    update_caret_pos(TRUE, TRUE);

    /* make sure focus is in our window */
    take_focus();
}

/*
 *   Update the caret position.  This should be used when starting a new
 *   command, scrolling, or otherwise altering the caret position.  
 */
void CHtmlSysWin_win32_Input::update_caret_pos(int bring_on_screen,
                                               int update_sel_range)
{
    unsigned long inp_txt_ofs;

    /* 
     *   if we're bringing the cursor on the screen, make sure we're on
     *   the active page 
     */
    if (bring_on_screen && owner_ != 0)
        owner_->go_to_active_page();

    /*
     *   if we don't have a command tag yet, or the caret is set invisible
     *   in the input buffer, or if we're not showing the active command
     *   input page, put the caret off-screen 
     */
    if (cmdtag_ == 0
        || (!cmdbuf_->is_caret_visible() && !update_sel_range)
        || owner_ == 0
        || !owner_->is_active_page())
    {
        /* move the caret somewhere where we won't see it */
        SetCaretPos(-100, -100);

        /* done */
        return;
    }

    /* get the current tag's text offset */
    inp_txt_ofs = cmdtag_->get_text_ofs();

    /* set the caret to the end of the formatter's last object */
    caret_pos_ = formatter_->get_text_pos(inp_txt_ofs + cmdbuf_->get_caret());

    /* convert to screen coordinates */
    caret_pos_ = doc_to_screen(caret_pos_);

    /* don't bring anything on the screen if we're in MORE mode */
    if (more_mode_)
        bring_on_screen = FALSE;

    /*
     *   if the caret is off-screen horizontally, and we want to bring it
     *   back on screen, scroll to move the caret back within the window 
     */
    if (bring_on_screen)
        scroll_to_show_caret();

    /* move the Windows caret, if we're showing it */
    if (caret_vis_)
    {
        if (more_mode_)
            SetCaretPos(-100, -100);
        else
            SetCaretPos(caret_pos_.x, caret_pos_.y + 2);
    }

    /* update the selection range in the formatter */
    if (update_sel_range)
    {
        size_t start, end, caret;

        /* get the command buffer selection range */
        cmdbuf_->get_sel_range(&start, &end, &caret);

        /* remember which end has the caret */
        caret_at_right_ = (caret == end);

        /* set this range in the formatter for the input item */
        formatter_->set_sel_range(start + inp_txt_ofs, end + inp_txt_ofs);

        /* clear any selections in other subwindows */
        if (owner_ != 0)
            owner_->clear_other_sel(this);
    }

    /*
     *   remember this as the most recent position that we've displayed
     *   the caret in the command line 
     */
    last_cmd_caret_ = cmdbuf_->get_caret();

    /* if we have an owner, notify it */
    if (owner_ != 0)
        owner_->owner_update_caret_pos();
}

/*
 *   Wait for a single keystroke 
 */
int CHtmlSysWin_win32_Input::wait_for_keystroke(
    textchar_t *ch, int pause_only, time_t timeout, int use_timeout)
{
    int ret;
    int ret_stat;

    /* presume that a game has started if we're asked for an event */
    game_started_ = TRUE;
    in_game_chest_ = FALSE;
    if (statusline_ != 0)
        statusline_->update();

    /* if we have a keystroke command to return, return it */
    if (last_keystroke_cmd_ != 0)
    {
        /* set the return value */
        *ch = (textchar_t)last_keystroke_cmd_;

        /* forget it now that it's been consumed */
        last_keystroke_cmd_ = 0;

        /* return it */
        return (eof_ ? OS_EVT_EOF : OS_EVT_KEY);
    }
    
    /* if we're at EOF, return EOF now */
    if (eof_)
    {
        /* return a (0 + CMD_EOF) extended command sequence */
        *ch = 0;
        last_keystroke_cmd_ = CMD_EOF;

        /* indicate EOF to our caller */
        return OS_EVT_EOF;
    }

    /* note that we're waiting for a single key */
    waiting_for_key_ = TRUE;
    waiting_for_key_pause_ = TRUE /*pause_only*/;

    /* update the status line */
    if (statusline_ != 0)
        statusline_->update();

    /* we haven't timed out yet */
    waiting_for_evt_timeout_ = FALSE;

    /* 
     *   bring this window to the front (this is useful mostly when we're
     *   running in the debugger, in which case a debugger window will
     *   probably be on top of the game window) 
     */
    owner_->bring_owner_to_front();

    /* make sure the caret is positioned correctly */
    update_caret_pos(FALSE, FALSE);

    /* if there's a timeout, start a timer for the timeout period */
    if (use_timeout)
    {
        /* start a timer for the timeout period */
        input_timer_id_ = alloc_timer_id();
        SetTimer(handle_, input_timer_id_, (DWORD)(timeout * 1000L),
                 (TIMERPROC)0);
    }

    /* 
     *   add a self-reference while we're processing events, to ensure our
     *   event-loop-ending variable remains valid until the event loop
     *   returns 
     */
    AddRef();

    /* wait for a valid keystroke */
    for (;;)
    {
        /* wait for a keystroke */
        ret = CTadsApp::get_app()->event_loop(&got_keystroke_);

        /* if we're quitting, we didn't get a keystroke */
        if (ret == 0)
        {
            /* no longer waiting for a key */
            waiting_for_key_ = FALSE;
            waiting_for_key_pause_ = FALSE;

            /* release our self-reference */
            Release();

            /* post another quit message for the enclosing event loop */
            PostQuitMessage(0);

            /* return a (0 + CMD_EOF) extended command sequence */
            *ch = 0;
            last_keystroke_cmd_ = CMD_EOF;

            /* return end-of-file indication */
            return OS_EVT_EOF;
        }

        /* if the keystroke is valid, or we've timed out, stop looping */
        if (last_keystroke_ != 0 || last_keystroke_cmd_ != 0
            || waiting_for_evt_timeout_ || eof_)
            break;
    }

    /* we're no longer in single-key mode */
    waiting_for_key_ = FALSE;
    waiting_for_key_pause_ = FALSE;
    if (statusline_ != 0)
        statusline_->update();

    /* 
     *   Reset the MORE prompt position to this point, since the user has
     *   obviously seen everything up to here.
     */
    last_input_height_ = formatter_->get_max_y_pos();

    /* if we reached end of file, return EOF indication */
    if (eof_)
    {
        /* return a (0 + CMD_EOF) extended command sequence */
        *ch = 0;
        last_keystroke_cmd_ = CMD_EOF;

        /* indicate EOF to our caller */
        ret_stat = OS_EVT_EOF;
        goto done;
    }

    /* if we timed out, so indicate */
    if (waiting_for_evt_timeout_)
    {
        /* return the timeout status */
        ret_stat = OS_EVT_TIMEOUT;
        goto done;
    }

    /* if we're merely pausing, discard any extended keystroke code */
    if (pause_only)
        last_keystroke_cmd_ = 0;

    /* return the keystroke we got */
    *ch = last_keystroke_;
    ret_stat = OS_EVT_KEY;

done:
    /* release our self-reference */
    Release();

    /* return the result */
    return ret_stat;
}

/*
 *   Read an event 
 */
int CHtmlSysWin_win32_Input::get_input_event(unsigned long timeout,
                                             int use_timeout,
                                             os_event_info_t *info)
{
    int ret;

    /* presume that a game has started if we're asked for an event */
    game_started_ = TRUE;
    in_game_chest_ = FALSE;
    if (statusline_ != 0)
        statusline_->update();

    /* if the EOF flag is set, return immediately */
    if (eof_)
        return OS_EVT_EOF;

    /* 
     *   note that we're waiting for an event, and remember the return
     *   info structure so that we'll fill it in when we get a suitable
     *   event 
     */
    waiting_for_evt_ = TRUE;
    last_evt_info_ = info;

    /* update the status line */
    if (statusline_ != 0)
        statusline_->update();

    /* we haven't timed out yet */
    waiting_for_evt_timeout_ = FALSE;

    /* 
     *   bring this window to the front (this is useful mostly when we're
     *   running in the debugger, in which case a debugger window will
     *   probably be on top of the game window) 
     */
    owner_->bring_owner_to_front();

    /* make sure the caret is positioned correctly */
    update_caret_pos(FALSE, FALSE);

    /* if there's a timeout, start a timer for the timeout period */
    if (use_timeout)
    {
        /* start a timer for the timeout period */
        input_timer_id_ = alloc_timer_id();
        SetTimer(handle_, input_timer_id_, (DWORD)timeout, (TIMERPROC)0);
    }

    /* 
     *   add a reference while we're in the event loop, to ensure that we
     *   can't be deleted until we're done with our 'this' object 
     */
    AddRef();

    /* wait for an event */
    ret = CTadsApp::get_app()->event_loop(&got_event_);

    /* if we're quitting, so indicate */
    if (ret == 0)
    {
        /* post another quit message to the enclosing event loop */
        PostQuitMessage(0);

        /* indicate end of file */
        ret = OS_EVT_EOF;
        goto done;
    }

    /* if we started a timer, stop it */
    if (use_timeout)
    {
        KillTimer(handle_, input_timer_id_);
        free_timer_id(input_timer_id_);
        input_timer_id_ = 0;
    }

    /* 
     *   Reset the MORE prompt position to this point, since the user has
     *   obviously seen everything up to here.  If we merely timed out, do
     *   not reset the MORE prompt position, since the user hasn't
     *   acknowledged anything yet.  
     */
    if (!waiting_for_evt_timeout_)
        last_input_height_ = formatter_->get_max_y_pos();

    /* no longer waiting for an event */
    waiting_for_evt_ = FALSE;

    /* forget the event info - it won't be valid after we return */
    last_evt_info_ = 0;

    /* if we reached end of file, so indicate */
    if (eof_)
    {
        /* indicate EOF */
        ret = OS_EVT_EOF;
        goto done;
    }

    /* if we timed out, so indicate */
    if (waiting_for_evt_timeout_)
    {
        /* indicate the timeout */
        ret = OS_EVT_TIMEOUT;
        goto done;
    }

    /* get the return code */
    ret = last_evt_type_;

done:
    /* release our self-reference */
    Release();

    /* return the event type */
    return ret;
}

/*
 *   Read a command line, with an optional timeout.
 */
int CHtmlSysWin_win32_Input::get_input_timeout(
    textchar_t *buf, size_t bufsiz, unsigned long timeout, int use_timeout)
{
    size_t copy_len;
    int ret_stat;

    /* if the EOF flag is set, return immediately */
    if (eof_)
        return OS_EVT_EOF;

    /* enter command editing mode */
    get_input_begin(bufsiz);

    /* we're not in single-key mode */
    waiting_for_key_ = FALSE;
    waiting_for_key_pause_ = FALSE;

    /* we haven't timed out yet */
    waiting_for_evt_timeout_ = FALSE;

    /* if there's a timeout, set up a timer for the timeout period */
    if (use_timeout)
    {
        /* start a timer for the timeout period */
        input_timer_id_ = alloc_timer_id();
        SetTimer(handle_, input_timer_id_, (DWORD)timeout, (TIMERPROC)0);
    }

    /* keep a self-reference to ensure we aren't deleted in the event loop */
    AddRef();

    /* read events until we've read a command */
    if (!CTadsApp::get_app()->event_loop(&command_read_))
    {
        /* release our self-reference */
        Release();

        /* post another quit message to the enclosing event loop */
        PostQuitMessage(0);
        
        /* notify the caller that the application is terminating */
        return OS_EVT_EOF;
    }

    /* if we started an event timer, we can kill it now */
    if (use_timeout)
    {
        KillTimer(handle_, input_timer_id_);
        free_timer_id(input_timer_id_);
        input_timer_id_ = 0;
    }

    /* make sure the result buffer is null-terminated */
    cmdbuf_->getbuf()[cmdbuf_->getlen()] = '\0';

    /* 
     *   copy the in-progress buffer to the caller's buffer, so the caller
     *   can see the results of the command input so far; limit the copying
     *   size to the size of the caller's buffer 
     */
    copy_len = cmdbuf_->getlen();
    if (copy_len > bufsiz - 1)
        copy_len = bufsiz - 1;

    /* copy the text to the caller's buffer */
    memcpy(buf, cmdbuf_->getbuf(), copy_len);

    /* null-terminate the result */
    buf[copy_len] = '\0';

    /* 
     *   Check the timeout and end-of-file flags - if we timed out, return a
     *   timeout indication; if we reached end of file, return EOF;
     *   otherwise return the LINE status to indicate that we read a line of
     *   text successfully.  
     */
    if (eof_)
    {
        /* close the input */
        get_input_done();

        /* error/application termination - indicate EOF status */
        ret_stat = OS_EVT_EOF;
    }
    else if (waiting_for_evt_timeout_)
    {
        /* 
         *   We timed out, so leave the input job unfinished.  The caller is
         *   required to call either get_input_cancel() or
         *   get_input_timeout() before performing any other display input
         *   or output operations, so we can simply leave things in the
         *   present half-finished state for now.  
         */

        /* 
         *   remember the selection range and caret position, so we can
         *   restore it later 
         */
        cmdbuf_->get_sel_range(&in_prog_sel_start_, &in_prog_sel_end_,
                               &in_prog_caret_);

        /* return the timeout status */
        ret_stat = OS_EVT_TIMEOUT;
    }
    else
    {
        /* add the completed command to the history list */
        if (cmdbuf_->getlen() != 0)
            cmdbuf_->add_hist();

        /* close the input */
        get_input_done();

        /* 
         *   clear the saved input buffer, since we have nothing to continue
         *   next time 
         */
        in_prog_buf_.set("");

        /* we read a line successfully - indicate this in the status */
        ret_stat = OS_EVT_LINE;
    }

    /* release our self-reference */
    Release();

    /* return the status */
    return ret_stat;
}

/*
 *   Set up for reading input.  When this returns, we'll be set up to
 *   process incoming events through command line editing mode.  
 */
void CHtmlSysWin_win32_Input::get_input_begin(size_t bufsiz)
{
    size_t initlen;
    int resuming;

    /* presume that a game has started if we're asked for an event */
    game_started_ = TRUE;
    in_game_chest_ = FALSE;
    if (statusline_ != 0)
        statusline_->update();

    /* 
     *   Check to see if we're resuming an interrupted session - if we never
     *   cancelled a previously interrupted input, or we have a non-empty
     *   saved buffer, we're resuming.  
     */
    resuming = (input_in_progress_
                || (in_prog_buf_.get() != 0
                    && in_prog_buf_.get()[0] != '\0'));

    /* correct any ill-formed HTML prior to input */
    ((CHtmlFormatterInput *)formatter_)->prepare_for_input();

    /* 
     *   if we're not continuing an uncancelled previous session, make sure
     *   we've formatted all available input 
     */
    while (!input_in_progress_ && formatter_->more_to_do())
        formatter_->do_formatting();

    /*
     *   If we have a previously interrupted input session, restore the
     *   buffer from the previous session.  
     */
    if (resuming)
    {
        /* store as much as we can, up to the buffer's length */
        initlen = strlen(in_prog_buf_.get());
        if (initlen > bufsiz - 1)
            initlen = bufsiz - 1;
    }
    else
    {
        /* 
         *   we have nothing to restore from a previous session, so our
         *   buffer is initially empty 
         */
        initlen = 0;
    }

    /* 
     *   make sure the in-progress buffer is big enough to hold the size of
     *   input buffer requested by the caller 
     */
    in_prog_buf_.ensure_size(bufsiz);

    /* 
     *   tell the formatter to begin a new input if we're not resuming an
     *   uncancelled interrupted session; otherwise, set up our existing
     *   input tag with the new buffer 
     */
    if (input_in_progress_)
    {
        /* set the existing command tag to use our new buffer */
        cmdtag_->change_buf((CHtmlFormatterInput *)formatter_,
                            in_prog_buf_.get());
    }
    else
    {
        /* create the input tag and format it */
        cmdtag_ = ((CHtmlFormatterInput *)formatter_)
                  ->begin_input(in_prog_buf_.get(), initlen);
        cmdtag_->format(this, formatter_);

        /* set the caret size for the input font */
        set_caret_size(formatter_->get_font());
    }

    /* 
     *   change or initialize the editor buffer, depending on whether we're
     *   resuming a session or starting a new session 
     */
    if (resuming)
    {
        /* change to the new buffer, but do not reset editor state */
        cmdbuf_->changebuf(in_prog_buf_.get(), bufsiz - 1);
        cmdbuf_->setlen(strlen(in_prog_buf_.get()));

        /* set the old selection range */
        cmdbuf_->set_sel_range(in_prog_sel_start_, in_prog_sel_end_,
                               in_prog_caret_);
    }
    else
    {
        /* initialize the editing buffer with the caller's buffer */
        cmdbuf_->setbuf(in_prog_buf_.get(), bufsiz - 1, initlen);

        /* set the caret to the end of the input buffer's contents */
        cmdbuf_->set_sel_range(initlen, initlen, initlen);

        /* 
         *   Bring this window to the front (this is useful mostly when
         *   we're running in the debugger, in which case a debugger window
         *   will probably be on top of the game window).  Note that we do
         *   this only when we're starting a new input session, so that we
         *   don't mysteriously change window layering at random times when
         *   we appear to be waiting passively for input.  
         */
        owner_->bring_owner_to_front();
    }

    /* do some additional initialization if we're starting a new session */
    if (!input_in_progress_)
    {
        /* 
         *   sync the on-screen caret with the buffer's caret if we're not
         *   resuming an uninterrupted session (in which case the caret will
         *   still be where we left it at the interruption, so we don't need
         *   or want to restore it now) 
         */
        update_caret_pos(TRUE, TRUE);

        /* make sure everything's adjusted */
        fmt_adjust_vscroll();
    }

    /* we haven't read a command yet */
    command_read_ = FALSE;

    /* input is now in progress */
    input_in_progress_ = TRUE;
}


/*
 *   Clean up after reading input.  This should be called after we
 *   successfully read an input line that is terminated normally (with
 *   Return, a hyperlink click, or the like), and also when we cancel input
 *   that was interrupted. 
 */
void CHtmlSysWin_win32_Input::get_input_done()
{
    /* 
     *   make sure we're on the active page (if we're at EOF, there's no
     *   point in doing this, as the application is shutting down and the
     *   owner might already have been destroyed) 
     */
    if (!eof_)
        owner_->go_to_active_page();

    /* tell the formatter that the input is finished */
    ((CHtmlFormatterInput *)formatter_)->end_input();

    /* set horizontal scrolling back to left edge */
    if (hscroll_ofs_ != 0)
        do_scroll(FALSE, hscroll_, SB_TOP, 0, FALSE);

    /* forget about the command */
    cmdbuf_->changebuf(0, 0);
    cmdtag_ = 0;
    started_reading_cmd_ = FALSE;

    /* don't prompt with "MORE" again until we scroll the command away */
    last_input_height_ = screen_to_doc_y(caret_pos_.y + caret_ht_);

    /* get rid of the caret by moving it off-screen */
    caret_pos_.set(-100, -100);
    SetCaretPos(-100, -100);

    /* input is no longer in progress */
    input_in_progress_ = FALSE;
}

/*
 *   Cancel input started with get_input_timeout() and interrupted by a
 *   timeout. 
 */
void CHtmlSysWin_win32_Input::get_input_cancel(int reset)
{
    /* if input was in progress, cancel it */
    if (input_in_progress_)
    {
        /* act as though the user interactively finished the input */
        get_input_done();
    }

    /* if they want to reset the input, forget the old input text */
    if (reset)
        in_prog_buf_.set("");
}

/*
 *   Check for a break key.  We'll look at the asynchronous key state to see
 *   if both the Ctrl and Break keys are being held down.  
 */
int CHtmlSysWin_win32_Input::check_break_key()
{
    /* if CTRL and BREAK are being held down, signal a break */
    return ((GetAsyncKeyState(VK_CANCEL) & 0x8000) != 0
            && ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0
                || (GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0));
}

/*
 *   Check to see if we've started reading a command.  Once we've started
 *   reading a command, we turn off MORE mode until the command has been
 *   entered.  
 */
void CHtmlSysWin_win32_Input::check_cmd_start()
{
    /*
     *   If we're not in "MORE" mode, and we have shown everything in the
     *   formatter's display list, and we have a command buffer, we must
     *   be ready to read a command.  
     */
    if (!started_reading_cmd_ && !more_mode_
        && cmdbuf_->getbuf() != 0 && cmdtag_ != 0
        && formatter_ != 0
        && !((CHtmlFormatterInput *)formatter_)->more_before_input())
    {
        /* this is it - we've started reading a command */
        started_reading_cmd_ = TRUE;

        /* clear out any prior "non-stop" mode */
        nonstop_mode_ = FALSE;
    }
}


/*
 *   Process a system (ALT) key 
 */
int CHtmlSysWin_win32_Input::do_syschar(TCHAR c, long keydata)
{
    /* if we're waiting for a keystroke, return the ALT key directly */
    if (isalpha(c) && is_waiting_for_key())
    {
        /* store the key event */
        set_key_event(0, CMD_ALT + c - (isupper(c) ? 'A' : 'a'));

        /* processed */
        return TRUE;
    }

    /* inherit default */
    return CHtmlSysWin_win32::do_syschar(c, keydata);
}

/*
 *   Process a keyboard virtual key event 
 */
int CHtmlSysWin_win32_Input::do_keydown(int vkey, long keydata)
{
    os_event_info_t event_info;
    int key_found;

    /* check with the owner for special MORE-mode processing */
    if (owner_->owner_process_moremode_key(vkey, 0))
        return TRUE;

    /*
     *   Translate the key into an osifc-level event.
     */
    
    /* presume we won't find a translation */
    key_found = FALSE;

    /* treat the ESC key specially */
    if (vkey == VK_ESCAPE)
    {
        /* return an escape keystroke */
        event_info.key[0] = 27;
        event_info.key[1] = 0;

        /* note that we translated it successfully */
        key_found = TRUE;
    }
    else
    {
        static struct
        {
            int vkey;
            int cmd;
            int ctl;
        } cmdmap[] =
        {
            { VK_UP,     CMD_UP,         FALSE },
            { VK_DOWN,   CMD_DOWN,       FALSE },
            { VK_RIGHT,  CMD_RIGHT,      FALSE },
            { VK_LEFT,   CMD_LEFT,       FALSE },
            { VK_END,    CMD_END,        FALSE },
            { VK_HOME,   CMD_HOME,       FALSE },
            { VK_DELETE, CMD_DEL,        FALSE },
            { VK_SCROLL, CMD_SCR,        FALSE },
            { VK_PRIOR,  CMD_PGUP,       FALSE },
            { VK_NEXT,   CMD_PGDN,       FALSE },
            { VK_HOME,   CMD_TOP,        TRUE  },
            { VK_END,    CMD_BOT,        TRUE  },
            { VK_F1,     CMD_F1,         FALSE },
            { VK_F2,     CMD_F2,         FALSE },
            { VK_F3,     CMD_F3,         FALSE },
            { VK_F4,     CMD_F4,         FALSE },
            { VK_F5,     CMD_F5,         FALSE },
            { VK_F6,     CMD_F6,         FALSE },
            { VK_F7,     CMD_F7,         FALSE },
            { VK_F8,     CMD_F8,         FALSE },
            { VK_F9,     CMD_F9,         FALSE },
            { VK_F10,    CMD_F10,        FALSE },
            { VK_LEFT,   CMD_WORD_LEFT,  TRUE  },
            { VK_RIGHT,  CMD_WORD_RIGHT, TRUE  }
        };
        int i;
        int ctl = get_ctl_key();
        
        /* find the keystroke in our table */
        for (i = 0 ; i < sizeof(cmdmap)/sizeof(cmdmap[0]) ; ++i)
        {
            /* if this one matches, use it */
            if (cmdmap[i].vkey == vkey && cmdmap[i].ctl == ctl)
            {
                /* got it - set up the event with the special command key */
                event_info.key[0] = 0;
                event_info.key[1] = cmdmap[i].cmd;

                /* note that we found a translation */
                key_found = TRUE;

                /* no need to look any further */
                break;
            }
        }
    }

    /* 
     *   if we're waiting for a keystroke event, and we have a valid osifc
     *   event mapping, return the osifc event 
     */
    if (is_waiting_for_key() && key_found)
    {
        /* set the keystroke event to what we found */
        set_key_event(event_info.key[0], event_info.key[1]);

        /* we don't need to process the keystroke any further */
        return TRUE;
    }

    /* 
     *   if it's not a control key, cancel any pending temporary link
     *   highlight mode 
     */
    switch(vkey)
    {
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
        /* inherit the default handling to turn on temporary links */
        CHtmlSysWin_win32::do_keydown(vkey, keydata);

        /* handle normally */
        break;

    default:
        /* cancel any pending highlighting */
        set_temp_show_links(FALSE);
        break;
    }

    /* if we didn't find a translation for the key, ignore it */
    if (!key_found)
        return FALSE;

    /* process other keys through the command editor */
    return process_input_edit_event(OS_EVT_KEY, &event_info);
}


/*
 *   Process a keyboard character event 
 */
int CHtmlSysWin_win32_Input::do_char(TCHAR c, long /*keydata*/)
{
    os_event_info_t event_info;
    
    /* check with the owner for special MORE-mode processing */
    if (owner_->owner_process_moremode_key(0, c))
        return TRUE;

    /* 
     *   if we're just waiting for a key, return the key event without
     *   further processing 
     */
    if (is_waiting_for_key())
    {
        /* 
         *   If it's an escape key, simply ignore it.  We process and return
         *   escape keys in do_keydown(), so we don't want to return the same
         *   key again here.  
         */
        if (c == 27)
            return TRUE;

        /* set the keystroke */
        set_key_event(c, 0);

        /* no further processing is required */
        return TRUE;
    }
    
    /* run the keystroke through the input editor */
    event_info.key[0] = c;
    event_info.key[1] = 0;
    return process_input_edit_event(OS_EVT_KEY, &event_info);
}

/*
 *   Process an event through the input editor. 
 */
int CHtmlSysWin_win32_Input::process_input_edit_event(
    int event_type, os_event_info_t *event_info)
{
    char c;
    int cmd;
    
    /* check the event type */
    switch(event_type)
    {
    case OS_EVT_KEY:
        /* check what we have */
        c = (char)event_info->key[0];
        switch(c)
        {
        case 0:
            /* it's a special command key - check the command ID */
            switch(cmd = event_info->key[1])
            {
            case CMD_UP:
                /*
                 *   if we're not reading command input, or using arrow keys
                 *   exclusively for scrolling, or on an inactive page, treat
                 *   it as a scrolling key; otherwise, treat it as a command
                 *   history selector 
                 */
                if (!started_reading_cmd_
                    || prefs_->get_arrow_keys_always_scroll()
                    || !owner_->is_active_page())
                    do_scroll(TRUE, vscroll_, SB_LINEUP, 0, FALSE);
                else if (cmdbuf_->select_prev_hist())
                    update_input_display();

                /* handled */
                return TRUE;

            case CMD_F8:
                /*
                 *   Select a previous history item that matches the current
                 *   command buffer up to the caret. 
                 */
                if (cmdbuf_->select_prev_hist_prefix())
                    update_input_display();

                /* handled */
                return TRUE;

            case CMD_DOWN:
                /*
                 *   If we're not reading command input, or using arrow keys
                 *   exclusively for scrolling, treat it as a scrolling key;
                 *   otherwise, treat it as a command history selector.  
                 */
                if (!started_reading_cmd_
                    || prefs_->get_arrow_keys_always_scroll()
                    || !owner_->is_active_page())
                    do_scroll(TRUE, vscroll_, SB_LINEDOWN, 0, FALSE);
                else if (cmdbuf_->select_next_hist())
                    update_input_display();
                
                /* handled */
                return TRUE;
                
            case CMD_PGUP:
                /* scroll up a page */
                do_scroll(TRUE, vscroll_, SB_PAGEUP, 0, FALSE);
                return TRUE;
                
            case CMD_PGDN:
                /* scroll down a page */
                do_scroll(TRUE, vscroll_, SB_PAGEDOWN, 0, FALSE);
                return TRUE;

            case CMD_LEFT:
            case CMD_WORD_LEFT:
                /* 
                 *   if in a command, move insertion point one character or
                 *   word left 
                 */
                cmdbuf_->move_left(get_shift_key(), cmd == CMD_WORD_LEFT);
                update_caret_pos(TRUE, TRUE);

                /* handled */
                return TRUE;

            case CMD_RIGHT:
            case CMD_WORD_RIGHT:
                /* 
                 *   if in a command, move insertion point one character or
                 *   one word right 
                 */
                cmdbuf_->move_right(get_shift_key(), cmd == CMD_WORD_RIGHT);
                update_caret_pos(TRUE, TRUE);

                /* handled */
                return TRUE;

            case CMD_END:
                if (!owner_->is_active_page())
                {
                    /* inactive page - scroll to bottom */
                    do_scroll(TRUE, vscroll_, SB_BOTTOM, 0, FALSE);
                }
                else if (get_ctl_key())
                {
                    /* delete to end of command */
                    if (cmdbuf_->del_eol())
                        update_input_display();
                }
                else
                {
                    /* move to end of command */
                    cmdbuf_->end_of_line(get_shift_key());
                    update_caret_pos(TRUE, TRUE);
                }
                return TRUE;

            case CMD_HOME:
                if (!owner_->is_active_page())
                {
                    /* inactive page - scroll to top */
                    do_scroll(TRUE, vscroll_, SB_TOP, 0, FALSE);
                }
                else
                {
                    /* move to start of comamnd */
                    cmdbuf_->start_of_line(get_shift_key());
                    update_caret_pos(TRUE, TRUE);
                }
                return TRUE;

            case CMD_DEL:
                /*
                 *   if we're in a command, delete selection or character to
                 *   right of cursor 
                 */
                if (cmdbuf_->del_right())
                    update_input_display();
                return TRUE;
            }

            /* done with the command key */
            break;
            
        case 1:
            /* ^A - start of line */
            cmdbuf_->start_of_line(FALSE);
            update_caret_pos(TRUE, TRUE);
            return TRUE;
            
        case 5:
            /* ^E - end of line */
            cmdbuf_->end_of_line(FALSE);
            update_caret_pos(TRUE, TRUE);
            return TRUE;
            
        case 4:
            /* ^D - delete character to right of cursor */
            if (cmdbuf_->del_right())
                update_input_display();
            return TRUE;
            
        case 6:
            /* ^F - forward a character */
            cmdbuf_->move_right(FALSE, FALSE);
            update_caret_pos(TRUE, TRUE);
            return TRUE;
            
        case 2:
            /* ^B - back a character */
            cmdbuf_->move_left(FALSE, FALSE);
            update_caret_pos(TRUE, TRUE);
            return TRUE;
            
        case 11:
            /* ^K - delete to end of line */
            if (cmdbuf_->del_eol())
                update_input_display();
            break;
            
        case 14:
            /* ^N - next line in history */
            if (cmdbuf_->select_next_hist())
                update_input_display();
            break;
            
        case 16:
            /* ^P - previous line in history or paste */
            if (cmdbuf_->select_prev_hist())
                update_input_display();
            break;
            
        case 21:
            /* ^U - delete entire line */
            if (cmdbuf_->del_line())
                update_input_display();
            return TRUE;
            
        case 22:
            /* ^V - page down (Emacs mode) or Paste (Windows mode) */
            if (prefs_->get_emacs_ctrl_v())
            {
                /* emacs style - scroll down a page */
                do_scroll(TRUE, vscroll_, SB_PAGEDOWN, 0, FALSE);
                return TRUE;
            }
            else
            {
                /* windows style - paste clipboard */
                return do_paste();
            }
            
        case 25:
            /* ^Y - yank - paste clipboard */
            return do_paste();
            
        case 24:
            /* ^X - cut */
            return do_cut();

        case 27:
            /* escape - delete entire line if editing a command */
            if (cmdbuf_->del_line())
                update_input_display();
            return TRUE;
            
        case 3:
            /* ^C - copy */
            return do_copy();
            
        case 26:
            /* ^Z - undo */
            if (cmdbuf_->undo())
                update_input_display();
            return TRUE;

        case 13:
        case 10:
            /* 
             *   return/enter key - end the command.  Make sure we're on the
             *   active page, then set the flag indicating that the current
             *   command has been successfully read.  
             */
            owner_->go_to_active_page();
            command_read_ = TRUE;
            return TRUE;

        case 8:
            /* process backspace key, and update the display if necessary */
            if (cmdbuf_->backspace())
                update_input_display();
            return TRUE;
            
        default:
            /* if it's a normal character, insert it into the buffer */
            if ((unsigned char)c >= 32)
            {
                /* insert it, and update the display if necessary */
                if (cmdbuf_->add_char(c))
                    update_input_display();
                
                /* keystroke handled */
                return TRUE;
            }
            break;
        }

        /* done with the keystroke event */
        break;
    }

    /* we didn't handle the event */
    return FALSE;
}

/*   
 *   Is the given command menu event enabled?  This applies to the system
 *   menu commands that we can send to the game via the command line - SAVE,
 *   RESTORE, QUIT, etc.
 *   
 *   We enable the command if the inherited handling says the command is
 *   enabled, or if we're reading an event (via os_get_event()), and this
 *   command is enabled for event return.  In any case, these commands are
 *   disabled if we're paused in the debugger.  
 */
int CHtmlSysWin_win32_Input::is_cmd_evt_enabled(int os_cmd_id) const
{
    /* if we're paused in the debugger, this command is disabled */
    if (owner_->get_game_paused())
        return FALSE;

    /* if the inherited handling says it's enabled, it's enabled */
    if (CHtmlSysWin_win32::is_cmd_evt_enabled(os_cmd_id))
        return TRUE;

    /* 
     *   if we're reading an event, check to see if the command is enabled
     *   for os_get_event() return 
     */
    if (waiting_for_evt_)
        return oss_is_cmd_event_enabled(os_cmd_id, OSCS_EVT_ON);

    /* in other contexts, these commands are disabled */
    return FALSE;
}

/*
 *   process a command 
 */
void CHtmlSysWin_win32_Input::process_command(
    const textchar_t *cmd, size_t len, int append, int enter, int os_cmd_id)
{
    /* 
     *   if the command starts with "http:", "ftp:", "news:" "mailto:", or
     *   "telnet:", fire it off to the web browser 
     */
    if (strnicmp(cmd, "http:", 5) == 0
        || strnicmp(cmd, "ftp:", 4) == 0
        || strnicmp(cmd, "news:", 5) == 0
        || strnicmp(cmd, "mailto:", 7) == 0
        || strnicmp(cmd, "telnet:", 7) == 0)
    {
        /* start the web browser */
        if ((unsigned long)ShellExecute(0, "open", cmd, 0, 0, 0) <= 32)
        {
            char buf[256];

            LoadString(CTadsApp::get_app()->get_instance(),
                       IDS_CANNOT_OPEN_HREF, buf, sizeof(buf));
            MessageBox(0, buf, "TADS",
                       MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
        }
        
        /* that's it */
        return;
    }
    
    /* 
     *   if we're showing the Game Chest page, process it as a game chest
     *   command 
     */
    if (in_game_chest_)
    {
        /* ask the owner to process the game chest command */
        owner_->owner_process_game_chest_cmd(cmd, len);

        /* that's all we need to do */
        return;
    }

    /* 
     *   if the game is over, check for the show-game-chest command - we add
     *   a hyperlink with this command at the end of a game to provide an
     *   easy way back to the main game chest page 
     */
    if (game_over_ && len == strlen(SHOW_GAME_CHEST_CMD) &&
        memcmp(cmd, SHOW_GAME_CHEST_CMD, strlen(SHOW_GAME_CHEST_CMD)) == 0)
    {
        /*
         *   tell the main window to return the game chest, reloading the
         *   file in case it's been edited while we were gone 
         */
        owner_->owner_show_game_chest(FALSE, TRUE, FALSE);
    }
    
    /* if we're waiting for an event, fill in the event information */
    if (waiting_for_evt_ && last_evt_info_ != 0)
    {
        /* 
         *   Note the event type.
         *   
         *   If it's a system CLOSE command, and the CLOSE command isn't
         *   enabled for os_get_event(), treat it as an EOF event, since
         *   we're closing the main window and thus terminating the
         *   application.  
         *   
         *   Otherwise, if it's a system menu command, return it as a COMMAND
         *   event, and return the command ID.
         *   
         *   Otherwise, it's a simple HREF event, with the hyperlink text as
         *   the 'href' argument.  
         */
        if (os_cmd_id == OS_CMD_CLOSE
            && !oss_is_cmd_event_enabled(OS_CMD_CLOSE, OSCS_EVT_ON))
        {
            /* 
             *   it's a CLOSE command, and we're not allowed to return a
             *   CLOSE command from os_get_event() - treat this as EOF 
             */
            last_evt_type_ = OS_EVT_EOF;
        }
        else if (os_cmd_id != OS_CMD_NONE)
        {
            /* it's a system menu command */
            last_evt_type_ = OS_EVT_COMMAND;
            last_evt_info_->cmd_id = os_cmd_id;
        }
        else
        {
            size_t copylen;

            /* it's an HREF event */
            last_evt_type_ = OS_EVT_HREF;

            /* 
             *   prepare to copy the HREF - limit the length of our copy to
             *   the available space 
             */
            copylen = len;
            if (copylen > sizeof(last_evt_info_->href) - sizeof(textchar_t))
                copylen = sizeof(last_evt_info_->href) - sizeof(textchar_t);

            /* copy the href */
            memcpy(last_evt_info_->href, cmd, copylen);

            /* fill in a null terminator */
            last_evt_info_->href[copylen / sizeof(textchar_t)] = '\0';
        }

        /* note that we have an event pending */
        got_event_ = TRUE;

        /* 
         *   we're done - do not process it as a command, since we're
         *   returning the raw event 
         */
        return;
    }
    
    /* if we're waiting for a single keystroke, ignore this */
    if (waiting_for_key_)
        return;

    /* release MORE mode in any windows currently in MORE mode */
    owner_->release_all_moremode();

    /* if we're not reading a command, ignore this */
    if (!started_reading_cmd_)
        return;

    /* 
     *   if we're not in APPEND mode, clear out the current command;
     *   otherwise, make sure we're at the end of the current text 
     */
    if (!append)
        cmdbuf_->del_line();
    else
        cmdbuf_->end_of_line(FALSE);

    /* add this string */
    cmdbuf_->add_string(cmd, len, TRUE);

    /* update the display */
    update_input_display();

    /* 
     *   If 'enter' is true, indicate that we've finished reading the
     *   command, so that get_input() or get_input_timeout() will return the
     *   new command as its result; otherwise, let the player continue
     *   editing this command.  
     */
    if (enter)
        command_read_ = TRUE;
}

/*
 *   Abort the current command 
 */
void CHtmlSysWin_win32_Input::abort_command()
{
    /* clear out anything currently in the command buffer */
    cmdbuf_->del_line();
    update_input_display();

    /* cancel any pending input */
    cancel_input_wait();
}

/*
 *   pre-scroll notification 
 */
void CHtmlSysWin_win32_Input::notify_pre_scroll(HWND hwnd)
{
    /* hide the drop caret if it's showing */
    if (drop_caret_vis_)
        drop_caret_->hide();

    /* inherit default handling */
    CHtmlSysWin_win32::notify_pre_scroll(hwnd);
}

/*
 *   post-scroll notification 
 */
void CHtmlSysWin_win32_Input::notify_scroll(HWND hwnd,
                                            long oldpos, long newpos)
{
    /* inherit default handling */
    CHtmlSysWin_win32::notify_scroll(hwnd, oldpos, newpos);

    /* turn on the drop caret if we hid it */
    if (drop_caret_vis_)
        drop_caret_->show();
}

/*
 *   Receive notification of a change in the hyperlink rendering preferences 
 */
void CHtmlSysWin_win32_Input::notify_link_pref_change()
{
    /*
     *   If we're showing the game chest page, warn that the change won't
     *   have any immediate effect in that Game Chest always displays links,
     *   no matter what the preference setting is.  
     */
    if (in_game_chest_)
    {
        char buf[256];

        LoadString(CTadsApp::get_app()->get_instance(),
                   IDS_LINK_PREF_CHANGE, buf, sizeof(buf));
        MessageBox(0, buf, "TADS", MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   CHtmlSysWin_win32_Input - IDropTarget implementation 
 */

/*
 *   start dragging over this window 
 */
HRESULT STDMETHODCALLTYPE
   CHtmlSysWin_win32_Input::DragEnter(IDataObject __RPC_FAR *dataobj,
                                      DWORD grfKeyState, POINTL pt,
                                      DWORD __RPC_FAR *effect)
{
    FORMATETC fmtetc;

    /* check for a game-chest drop */
    if (DragEnter_game_chest(dataobj, grfKeyState, pt, effect))
        return S_OK;

    /* start the drag-scroll timer */
    start_drag_scroll();

    /* presume we won't find a valid data source for the drop */
    drag_source_valid_ = FALSE;

    /* if we're not reading a command line, we can't accept a drop */
    if (cmdtag_ == 0 || !started_reading_cmd_)
    {
        /* 
         *   not reading a command - we can't take a drop right now, no
         *   matter what it is that's being dragged 
         */
        *effect = DROPEFFECT_NONE;
        return S_OK;
    }

    /* check to see if the data object can render text into an hglobal */
    fmtetc.cfFormat = CF_TEXT;
    fmtetc.ptd = 0;
    fmtetc.dwAspect = DVASPECT_CONTENT;
    fmtetc.lindex = -1;
    fmtetc.tymed = TYMED_HGLOBAL;
    if (dataobj->QueryGetData(&fmtetc) == S_OK)
    {
        /* note that the source is valid */
        drag_source_valid_ = TRUE;

        /* create a caret for the drop operation */
        drop_caret_ = new CTadsCaret(this);

        /* 
         *   the caret is invisible on creation - leave it invisible for
         *   now, until we determine whether we're in a good drop location
         *   or not 
         */
        drop_caret_vis_ = FALSE;

        /* set the size to our regular caret's size */
        drop_caret_->set_size(2, caret_ascent_);

        /* 
         *   go through the normal DragOver calculation to determine whether
         *   the location is valid 
         */
        DragOver(grfKeyState, pt, effect);
    }
    else
    {
        /* 
         *   the data being dragged can't be rendered the way we want, so we
         *   can't accept the drop 
         */
        *effect = DROPEFFECT_NONE;
    }

    /* success */
    return S_OK;
}

/*
 *   continue dragging over this window 
 */
HRESULT STDMETHODCALLTYPE
   CHtmlSysWin_win32_Input::DragOver(DWORD grfKeyState,
                                     POINTL pt,
                                     DWORD __RPC_FAR *effect)
{
    POINT loc;
    CHtmlPoint cmdloc;

    /* check to see if this is a game-chest drag */
    if (DragOver_game_chest(grfKeyState, pt, effect))
        return S_OK;

    /* convert the mouse position into client coordinates */
    loc.x = pt.x;
    loc.y = pt.y;
    ScreenToClient(handle_, &loc);

    /* perform drag-scrolling if appropriate */
    maybe_drag_scroll(loc.x, loc.y, 16, 16);

    /* 
     *   if we originally didn't allow the dropping on source grounds, don't
     *   allow dropping now 
     */
    if (!drag_source_valid_)
    {
        *effect = DROPEFFECT_NONE;
        return S_OK;
    }

    /* 
     *   Determine if the mouse is over the command line - only allow
     *   dropping into the command line area, or into any part of the window
     *   below the start of the command.  
     */

    /* get the position of the input area, in client coordinates */
    cmdloc = doc_to_screen(formatter_->get_text_pos(cmdtag_->get_text_ofs()));

    /* 
     *   make sure the drop is vertically below the command area in the
     *   window 
     */
    if (loc.y >= cmdloc.y)
    {
        /* it's in or below the command - accept the drop */
        *effect = DROPEFFECT_COPY;

        /* put the input buffer caret where they're pointing */
        set_drop_caret(pt);

        /* if the drop caret wasn't already visible, show it */
        if (!drop_caret_vis_)
        {
            /* show it */
            drop_caret_->show();

            /* note the new visibility */
            drop_caret_vis_ = TRUE;
        }
    }
    else
    {
        /* it's above the command - don't accept it */
        *effect = DROPEFFECT_NONE;

        /* if the drop caret was showing, remove it */
        if (drop_caret_vis_)
        {
            /* hide it */
            drop_caret_->hide();

            /* note that it's gone */
            drop_caret_vis_ = FALSE;
        }
    }

    /* success */
    return S_OK;
}

/*
 *   set the input buffer caret according to the mouse location 
 */
void CHtmlSysWin_win32_Input::set_drop_caret(POINTL pt)
{
    POINT loc;
    CHtmlPoint docpt;
    unsigned long txtofs;
    CHtmlPoint pos;

    /* convert the mouse position into client coordinates */
    loc.x = pt.x;
    loc.y = pt.y;
    ScreenToClient(handle_, &loc);

    /* figure out where we're pointing, and show the caret there */
    docpt = screen_to_doc(CHtmlPoint(loc.x, loc.y));
    txtofs = formatter_->find_textofs_by_pos(docpt);

    /* 
     *   if the text offset is before the command tag, locate at the
     *   beginning of the command tag 
     */
    if (txtofs < cmdtag_->get_text_ofs())
        txtofs = cmdtag_->get_text_ofs();

    /* set the input buffer caret at this position */
    cmdbuf_->set_caret(txtofs - cmdtag_->get_text_ofs());

    /* set the caret's position visually */
    docpt = formatter_->get_text_pos(txtofs);
    pos = doc_to_screen(docpt);
    drop_caret_->set_pos(pos.x, pos.y + 2);
}

/*
 *   leave this window with no drop having occurred 
 */
HRESULT STDMETHODCALLTYPE CHtmlSysWin_win32_Input::DragLeave()
{
    /* handle as a game-chest event, if appropriate */
    if (DragLeave_game_chest())
        return S_OK;

    /* end drag scrolling */
    end_drag_scroll();

    /* get rid of the drop caret */
    drop_caret_vis_ = FALSE;
    delete drop_caret_;
    drop_caret_ = 0;

    /* we don't have anything we need to do here */
    return S_OK;
}

/*
 *   drop in this window 
 */
HRESULT STDMETHODCALLTYPE
   CHtmlSysWin_win32_Input::Drop(IDataObject __RPC_FAR *dataobj,
                                 DWORD grfKeyState, POINTL pt,
                                 DWORD __RPC_FAR *effect)
{
    FORMATETC fmtetc;
    STGMEDIUM medium;

    /* handle as a game-chest drag/drop event, if appropriate */
    if (Drop_game_chest(dataobj, grfKeyState, pt, effect))
        return S_OK;

    /* end drag scrolling */
    end_drag_scroll();

    /* get the data being dropped, rendered as text into an hglobal */
    fmtetc.cfFormat = CF_TEXT;
    fmtetc.ptd = 0;
    fmtetc.dwAspect = DVASPECT_CONTENT;
    fmtetc.lindex = -1;
    fmtetc.tymed = TYMED_HGLOBAL;
    if (dataobj->GetData(&fmtetc, &medium) == S_OK)
    {
        textchar_t *buf;
        int disp_change;

        /* 
         *   set the caret position in the input buffer according to the
         *   final mouse position 
         */
        set_drop_caret(pt);

        /* update the window selection based on our caret position */
        update_caret_pos(TRUE, TRUE);

        /* get the text */
        buf = (textchar_t *)GlobalLock(medium.hGlobal);

        /* insert the text from the hglobal */
        disp_change = insert_text_from_hglobal(buf);

        /* we're done with the hglobal - unlock and delete it */
        GlobalUnlock(medium.hGlobal);
        if (medium.pUnkForRelease != 0)
            medium.pUnkForRelease->Release();
        else
            GlobalFree(medium.hGlobal);

        /* 
         *   we successfully accepted the drop - we always treat a drop as a
         *   copy of the data 
         */
        *effect = DROPEFFECT_COPY;

        /* update the display if necessary */
        if (disp_change)
            update_input_display();
    }
    else
    {
        /* they couldn't render the data the way we wanted - reject it */
        *effect = DROPEFFECT_NONE;
    }

    /* get rid of the drop caret */
    drop_caret_vis_ = FALSE;
    delete drop_caret_;
    drop_caret_ = 0;

    /* success */
    return S_OK;
}


/* ------------------------------------------------------------------------ */
/*
 *   Generic frame window base class implementation 
 */

/*
 *   remember my position in the preferences - this should be called any
 *   time the window is moved or resized 
 */
void CHtmlSys_framewin::set_winpos_prefs()
{
    /* if the window isn't iconic, remember its current position */
    if (!IsIconic(handle_))
    {
        RECT rc;
        CHtmlRect winpos;

        /* get the current position */
        GetWindowRect(handle_, &rc);

        /* translate the position into our own rectangle structure */
        winpos.set(rc.left, rc.top, rc.right, rc.bottom);

        /* save the new setting */
        set_winpos_prefs(&winpos);
    }
}


/*
 *   enter an interactive size/move operation 
 */
void CHtmlSys_framewin::do_entersizemove()
{
    HWND hdl;

    /* tell all my subwindows about this */
    for (hdl = GetWindow(handle_, GW_CHILD) ; hdl != 0 ;
         hdl = GetWindow(hdl, GW_HWNDNEXT))
        SendMessage(hdl, WM_ENTERSIZEMOVE, 0, 0);
}

/*
 *   finish an interactive size/move operation 
 */
void CHtmlSys_framewin::do_exitsizemove()
{
    HWND hdl;

    /* tell all my subwindows about this */
    for (hdl = GetWindow(handle_, GW_CHILD) ; hdl != 0 ;
         hdl = GetWindow(hdl, GW_HWNDNEXT))
        SendMessage(hdl, WM_EXITSIZEMOVE, 0, 0);
}

/*
 *   move the window 
 */
void CHtmlSys_framewin::do_move(int /*x*/, int /*y*/)
{
    /* remember my position in the preferences */
    set_winpos_prefs();
}

/* 
 *   resize the window 
 */
void CHtmlSys_framewin::do_resize(int mode, int /*x*/, int /*y*/)
{
    /* remember my position in the preferences */
    set_winpos_prefs();
}

/*
 *   MDI child activation 
 */
int CHtmlSys_framewin::do_childactivate()
{
    HWND chi;
    
    /* if we don't already have focus, set focus on me */
    if (GetFocus() != handle_)
        SetFocus(handle_);

    /* if I have a child window, propagate the message */
    chi = GetWindow(handle_, GW_CHILD);
    if (chi != 0)
        PostMessage(chi, WM_CHILDACTIVATE, 0, 0);

    /* inherit default */
    return CTadsWin::do_childactivate();
}


/* ------------------------------------------------------------------------ */
/*
 *   Superclassed window class for drawing banner borders.  We implement this
 *   as a simple "superclass" (Windows terminology) of the STATIC system
 *   class.  
 */

/* original window procedure for the STATIC base class */
static WNDPROC S_border_proc_base;

static LRESULT CALLBACK border_proc(
    HWND hwnd, UINT msg, WPARAM wpar, LPARAM lpar)
{
    HDC hdc;
    PAINTSTRUCT ps;

    /* check the message to see if we want to override the base handling */
    switch(msg)
    {
    case WM_PAINT:
        /* 
         *   paint the window - fill in the entire area with the "dark
         *   shadow" color 
         */
        hdc = BeginPaint(hwnd, &ps);
        FillRect(hdc, &ps.rcPaint, GetSysColorBrush(COLOR_3DDKSHADOW));
        EndPaint(hwnd, &ps);

        /* handled */
        return 0;

    case WM_ERASEBKGND:
        /* erase the background - do nothing */
        return 1;
    }

    /* inherit the base class's window procedure */
    return CallWindowProc(S_border_proc_base, hwnd, msg, wpar, lpar);
}


/* ------------------------------------------------------------------------ */
/*
 *   HTML main window implementation 
 */


/* 
 *   The global main window.  While the application is running and a
 *   window is open, we keep track of the primary window for the
 *   application.  Since TADS doesn't keep track of an active window, we
 *   need a global window that we can use for TADS input/output
 *   operations. 
 */
CHtmlSys_mainwin *CHtmlSys_mainwin::main_win_ = 0;

/*
 *   create the main window 
 */
CHtmlSys_mainwin::CHtmlSys_mainwin(CHtmlFormatterInput *formatter,
                                   CHtmlParser *parser, int in_debugger)
{
    int code_page;
    CHARSETINFO csinfo;
    HDC hdc;
    TEXTMETRIC tm;
    WNDCLASS wc;

    /* remember whether we're running under the debugger */
    in_debugger_ = in_debugger;

    /* the game is not yet paused in the debugger */
    game_paused_ = FALSE;

    /* we're not going directly from a game to the game chest yet */
    direct_to_game_chest_ = FALSE;

    /* no command-line input is in progress */
    input_in_progress_ = FALSE;

    /* presume we won't have a stand-alone executable */
    standalone_exe_ = FALSE;    

    /* create a text buffer */
    txtbuf_ = new CHtmlTextBuffer();
    
    /* remember my parser */
    parser_ = parser;

    /* create preferences object */
    prefs_ = new CHtmlPreferences();

    /* 
     *   read stored preference settings; this is the initial load, so load
     *   all properties, synchronized or not 
     */
    prefs_->restore(FALSE);
    
    /* no main panel offset yet */
    main_panel_yoffset_ = 0;

    /* no banners yet */
    banners_ = 0;
    banner_cnt_ = 0;
    banners_max_ = 0;

    /* no orphaned banners yet */
    orphaned_banners_ = 0;
    orphaned_banner_cnt_ = 0;
    orphaned_banners_max_ = 0;
    deleting_orphaned_banners_ = FALSE;
    
    /* set the global frame object pointer to point to me */
    CHtmlSysFrame::set_frame_obj(this);

    /* remember me as the main window */
    if (main_win_ == 0)
        main_win_ = this;

    /* initialize the first page's state structure */
    first_page_state_ = last_page_state_ = cur_page_state_ = 0;
    add_new_page_state();

    /* no DirectSound interface yet - create it on demand */
    directsound_ = 0;
    directsound_init_err_ = HTMLW32_DIRECTX_OK;

    /* no debug window yet */
    dbgwin_ = 0;

    /* create the main HTML display panel window */
    main_panel_ = new CHtmlSysWin_win32_Input(
        formatter, this, TRUE, TRUE, prefs_);

    /* create a parser and formatter for the history window */
    hist_parser_ = new CHtmlParser();
    hist_fmt_ = new CHtmlFormatterStandalone(hist_parser_);

    /* create the history panel window */
    hist_panel_ = new CHtmlSysWin_win32_Hist(
        hist_fmt_, this, TRUE, TRUE, prefs_);

    /* no "about this game" window yet */
    aboutbox_ = 0;

    /* no pending new game yet */
    is_new_game_pending_ = FALSE;

    /* 
     *   Find the character set that corresponds to the current system
     *   ANSI code page, and use this as the default character set. 
     */
    code_page = GetACP();
    TranslateCharsetInfo((DWORD *)code_page, &csinfo, TCI_SRCCODEPAGE);
    default_charset_.codepage = code_page;
    default_charset_.charset = csinfo.ciCharset;

    /* the game character set has not been set yet */
    game_internal_charset_.set("Unknown");

    /* 
     *   Get the default character in the default character set -- we'll
     *   use this whenever the game tries to display a character for which
     *   we can provide no character mapping.  Use the DC for the desktop
     *   window, which presumably has a font selected that uses the system
     *   default character set.  
     */
    hdc = GetDC(GetDesktopWindow());
    GetTextMetrics(hdc, &tm);
    invalid_char_val_ = tm.tmDefaultChar;
    ReleaseDC(GetDesktopWindow(), hdc);

    /* pause_for_exit is enabled by default */
    exit_pause_enabled_ = TRUE;

    /* no game is running until we've been told so */
    main_panel_->set_game_over(TRUE);
    main_panel_->set_game_started(FALSE);

    /* not in game chest yet */
    main_panel_->set_in_game_chest(FALSE);

    /* no idle timer yet */
    idle_timer_id_ = 0;

    /* create the "find" dialog */
    find_dlg_ = new CTadsDialogFind();

    /* initialize the dialog properties */
    search_exact_case_ = FALSE;
    search_wrap_ = FALSE;
    search_dir_ = 1;
    search_from_top_ = FALSE;

    /* no popup menu handles yet */
    popup_container_ = 0;

    /* no profile menu yet */
    profile_menu_ = 0;

    /* no game chest info yet */
    gch_info_ = 0;

    /* no active downloads yet */
    download_head_ = 0;

    /* links aren't temporarily activated yet */
    temp_show_links_ = FALSE;

    /* we don't have an .exe resource hash table yet */
    exe_res_table_ = 0;

    /* 
     *   Register our special banner border window, which is a simple
     *   "superclass" of the system STATIC class (Windows terminology for a
     *   window class that inherits the window procedure of another window
     *   class).  This class does almost the same thing as the standard
     *   system STATIC class, but we provide our own custom painting.
     *   
     *   First, to hook into the system STATIC class, get the window class
     *   information for that class.  
     */
    GetClassInfo(0, "STATIC", &wc);

    /* remember the STATIC class's original window procedure */
    S_border_proc_base = wc.lpfnWndProc;

    /* 
     *   register our new class, providing our custom background brush and
     *   window procedure 
     */
    wc.lpszClassName = "TADS.BannerBorder";
    wc.hInstance = CTadsApp::get_app()->get_instance();
    wc.lpfnWndProc = (WNDPROC)&border_proc;
    RegisterClass(&wc);
}

/* iterator callback object for releasing references on MORE windows */
class CReleaseRefMoreWinCB: public CIterCallback
{
public:
    virtual void invoke(void *arg)
    {
        /* release our reference on this window */
        ((CHtmlSysWin_win32 *)arg)->Release();
    }
};

/*
 *   deletion
 */
CHtmlSys_mainwin::~CHtmlSys_mainwin()
{
    CHtmlSys_mainwin_state_link *link;
    CHtmlSys_mainwin_state_link *nxt;
    CReleaseRefMoreWinCB cb;
    
    /* save preference settings */
    prefs_->save();

    /* delete all orphaned banners */
    delete_orphaned_banners();

    /* delete the orphaned banner list */
    if (orphaned_banners_ != 0)
        th_free(orphaned_banners_);

    /* delete the banner list */
    if (banners_ != 0)
    {
        CHtmlSysWin_win32 **win;
        int i;
        
        /* release our reference on each banner window */
        for (win = banners_, i = 0 ; i < banner_cnt_ ; ++i, ++win)
            (*win)->Release();
        
        /* free the array */
        th_free(banners_);
    }

    /* release our references on any [MORE] windows */
    more_wins_.iterate(&cb);

    /* remove our reference on the preferences object */
    prefs_->release_ref();

    /* delete the status line */
    if (statusline_ != 0)
        delete statusline_;

    /* clear out the global frame object pointer */
    CHtmlSysFrame::set_frame_obj(0);

    /* forget me if I'm the main window */
    if (main_win_ == this)
        main_win_ = 0;

    /* done with our text buffer */
    delete txtbuf_;

    /* delete all of the saved parse trees */
    for (link = first_page_state_ ; link != 0 ; link = nxt)
    {
        /* remember the next link, since we're going to delete this one */
        nxt = link->next_;

        /* delete this one */
        delete link;
    }

    /* release our DirectSound interface */
    if (directsound_ != 0)
        directsound_->Release();

    /* delete the 'find' dialog */
    delete find_dlg_;

    /* delete the popup menu, if we loaded one */
    if (popup_container_ != 0)
        DestroyMenu(popup_container_);

    /* if we have game chest information, delete it */
    delete_game_chest_info();

    /* delete the history window's parser and formatter */
    hist_fmt_->release_parser();
    delete hist_parser_;
    delete hist_fmt_;

    /* if we have an .exe resource hash table, delete it */
    if (exe_res_table_ != 0)
        delete exe_res_table_;

    /* terminate the application */
    PostQuitMessage(0);
}

/*
 *   Start a new game 
 */
void CHtmlSys_mainwin::start_new_game()
{
    /* flush output */
    flush_txtbuf(TRUE, FALSE);

    /* clear any pending EOF flag in the input window */
    main_panel_->set_eof_flag(FALSE);

    /* note that a game is running */
    main_panel_->set_game_over(FALSE);
    main_panel_->set_game_started(TRUE);

    /* we're no longer in the game chest */
    main_panel_->set_in_game_chest(FALSE);

    /* update the status line */
    if (statusline_ != 0)
        statusline_->update();

    /* put the parser into plain text mode */
    os_end_html();

    /* reset the elapsed time display */
    starting_ticks_ = GetTickCount();
    timer_paused_ = FALSE;

    /* clear out any previous interrupted input */
    get_input_cancel(TRUE);

    /* clear out all of the saved pages from any previous game */
    clear_all_pages();
}

/*
 *   Flag that the current game has ended 
 */
void CHtmlSys_mainwin::end_current_game()
{
    /* flush any pending output */
    flush_txtbuf(TRUE, FALSE);
    
    /* 
     *   set the EOF flag in the input window, so that we know that we
     *   don't have a game currently underway 
     */
    main_panel_->set_eof_flag(TRUE);

    /* remember that the game is over */
    main_panel_->set_game_over(TRUE);

    /* cancel sounds */
    main_panel_->get_formatter()->cancel_sound(HTML_Attrib_invalid);

    /* terminate playback of all active media (animations) */
    main_panel_->get_formatter()->cancel_playback();

    /* update the status message */
    if (statusline_ != 0)
        statusline_->update();

    /* update the caret position in the input window */
    main_panel_->update_caret_pos(FALSE, FALSE);

    /* update the title to remove the game name, since nothing is loaded */
    notify_load_game(0);
}


/*
 *   Wait for the user to select a new game or quit 
 */
int CHtmlSys_mainwin::wait_for_new_game(int quitting)
{
    int ret;
    
    /* if we already have a new game pending, return success */
    if (is_new_game_pending_)
        return TRUE;

    /* mark the current game as ended */
    end_current_game();

    /* 
     *   if we just quit another game, and our post-quit mode is "exit",
     *   it means that we simply wait for a keystroke and exit when the
     *   game ends, so we don't want to await a new game; in this case,
     *   simply return false to indicate that no new game selection is
     *   forthcoming 
     */
    if (quitting && prefs_->get_postquit_action() == HTML_PREF_POSTQUIT_EXIT)
        return FALSE;

    /* 
     *   If we're just starting, show the game chest page; otherwise, wait
     *   for the user to go to the game chest explicitly, so as not to clear
     *   the last page of the terminating game prematurely.  Never show the
     *   game chest window if we're in the debugger, though.  Don't show the
     *   game chest for a stand-alone game executable, since such an
     *   executable is dedicated to playing the single game.  
     */
    if (!in_debugger_ && !standalone_exe_)
    {
        if (!quitting || direct_to_game_chest_)
        {
            /* 
             *   No previous game - we can show the game chest immediately.
             *   Since we're just starting out, definitely read the game
             *   chest.  
             */
            show_game_chest(FALSE, TRUE, FALSE);

            /* 
             *   we've handled the go-to-game-chest command, so clear the
             *   flag for next time 
             */
            direct_to_game_chest_ = FALSE;
        }
        else
        {
            /* offer the game chest page */
            offer_game_chest();
        }
    }

    /* 
     *   process events until the user selects a new game or terminates
     *   the program 
     */
    AddRef();
    ret = CTadsApp::get_app()->event_loop(&is_new_game_pending_);
    Release();

    /* 
     *   return true if we didn't get a quit message, and a new game is
     *   pending 
     */
    if (!ret)
        return FALSE;
    else
        return is_new_game_pending_;
}

/*
 *   get the text for a 'find' command 
 */
const char *CHtmlSys_mainwin::get_find_text(int command_id, int *exact_case,
                                            int *start_at_top, int *wrap,
                                            int *dir)
{
    /*
     *   If this is a Find command, or if we don't have any text from the
     *   last "Find" command, run the dialog.  
     */
    if (command_id == ID_EDIT_FIND || find_text_[0] == '\0')
    {
        int new_exact_case = search_exact_case_;
        int new_search_from_top = search_from_top_;
        int new_wrap = search_wrap_;
        int new_dir = search_dir_;

        /* run the dialog - if they cancel, return null */
        if (!find_dlg_->run_find_dlg(get_handle(), find_text_,
                                     sizeof(find_text_),
                                     &new_exact_case, &new_search_from_top,
                                     &new_wrap, &new_dir))
            return 0;

        /* they accepted the dialog -- remember the new settings */
        search_exact_case_ = new_exact_case;
        search_from_top_ = new_search_from_top;
        search_wrap_ = new_wrap;
        search_dir_ = new_dir;

        /* use the search-from-top setting from the dialog */
        *start_at_top = search_from_top_;
    }
    else
    {
        /* 
         *   continuing an old search, so don't start over at the top,
         *   regardless of the last dialog settings 
         */
        *start_at_top = FALSE;
    }

    /* 
     *   fill in the flags from the last dialog settings, whether this is a
     *   new search or a continued search 
     */
    *exact_case = search_exact_case_;
    *wrap = search_wrap_;
    *dir = search_dir_;

    /* return the text to find */
    return find_text_;
}


/*
 *   Set my debug window 
 */
void CHtmlSys_mainwin::set_debug_win(CHtmlSys_dbglogwin *dbgwin,
                                     int add_menu_item)
{
    /* 
     *   if I don't have a debug window already, and we now have one, add
     *   a menu item to show it should it become hidden 
     */
    if (dbgwin_ == 0 && dbgwin != 0 && add_menu_item)
    {
        HMENU hmenu;
        int cnt;
        MENUITEMINFO info;
        char item_name[256];

        /* get the View menu */
        hmenu = GetMenu(handle_);
        hmenu = GetSubMenu(hmenu, 2);

        /* get the number of items in the View menu */
        cnt = GetMenuItemCount(hmenu);

        /* insert a separator at the end of the View menu */
        info.cbSize = menuiteminfo_size_;
        info.fMask = MIIM_TYPE;
        info.fType = MFT_SEPARATOR;
        InsertMenuItem(hmenu, cnt++, TRUE, &info);

        /* insert a new item for View Debug Window */
        LoadString(CTadsApp::get_app()->get_instance(),
                   IDS_SHOW_DBG_WIN_MENU, item_name, sizeof(item_name));
        info.fMask = MIIM_TYPE | MIIM_DATA | MIIM_ID;
        info.fType = MFT_STRING;
        info.wID = ID_SHOW_DEBUG_WIN;
        info.dwTypeData = item_name;
        info.cch = get_strlen(item_name);
        InsertMenuItem(hmenu, cnt, TRUE, &info);
    }

    /* remember the new setting */
    dbgwin_ = dbgwin;
}

/*
 *   Abort the current command 
 */
void CHtmlSys_mainwin::abort_command()
{
    /* tell the main window to abort its command */
    main_panel_->abort_command();
}

/*
 *   set my window position in the preferences 
 */
void CHtmlSys_mainwin::set_winpos_prefs(const CHtmlRect *winpos)
{
    prefs_->set_win_pos(winpos);
}

/*
 *   process creation event 
 */
void CHtmlSys_mainwin::do_create()
{
    RECT panel_pos;
    char title[256];

    /* inherit default creation */
    CHtmlSys_framewin::do_create();

    /* create our toolbar */
    create_toolbar();

    /* create and show the status line */
    statusline_ = new CTadsStatusline(this, TRUE, HTML_MAINWIN_ID_STATUS);

    /* tell the main panel about the status line */
    main_panel_->set_status_line(statusline_);

    /* create the history panel window, keeping it hidden for now */
    hist_panel_->create_system_window(this, FALSE, "", &panel_pos);

    /* create the main panel's system window */
    GetClientRect(handle_, &panel_pos);
    main_panel_->create_system_window(this, TRUE, "", &panel_pos);

    /* load my menu */
    SetMenu(handle_, LoadMenu(CTadsApp::get_app()->get_instance(),
                              MAKEINTRESOURCE(IDR_MAIN_MENU)));

    /* load the context menu container */
    popup_container_ = LoadMenu(CTadsApp::get_app()->get_instance(),
                                MAKEINTRESOURCE(IDR_MAINWIN_POPUP));

    /* get the statusline submenu */
    statusline_popup_ = GetSubMenu(popup_container_, 0);

    /* set up the recent game items in the menu */
    set_recent_game_menu();

    /* set up the correct accelerators the preferences */
    main_panel_->set_current_accel();

    /* 
     *   create the "about" box window -- we'll just create the window and
     *   keep it hidden for now 
     */
    aboutbox_ = new CHtmlSys_aboutgamewin(prefs_);
    SetRect(&panel_pos, 0, 0, 500, 300);
    LoadString(CTadsApp::get_app()->get_instance(),
               IDS_ABOUT_GAME_WIN_TITLE, title, sizeof(title));
    aboutbox_->create_system_window(this, FALSE, title, &panel_pos);

    /* establish our accelerator */
    CTadsApp::get_app()->set_accel(main_panel_->get_accel(), get_handle());

    /* create an idle timer to update the elapsed time display */
    idle_timer_id_ = alloc_timer_id();
    if (idle_timer_id_ != 0
        && SetTimer(handle_, idle_timer_id_, 1000, (TIMERPROC)0) == 0)
    {
        /* we failed to set the timer - free our timer ID */
        free_timer_id(idle_timer_id_);
        idle_timer_id_ = 0;
    }
}

/*
 *   initialize a pop-up menu 
 */
void CHtmlSys_mainwin::init_menu_popup(HMENU menuhdl, unsigned int pos,
                                       int sysmenu)
{
    /* 
     *   Check to see if this is the "profiles" submenu: detect this by
     *   checking to see if we find an item in the ID_PROFILE_FIRST to
     *   ID_PROFILE_LAST range.  
     */
    MENUITEMINFO info;

    /* get information on the first item on the menu */
    info.cbSize = menuiteminfo_size_;
    info.fMask = MIIM_ID;
    GetMenuItemInfo(menuhdl, 0, TRUE, &info);
    if (info.wID >= ID_PROFILE_FIRST && info.wID <= ID_PROFILE_LAST)
    {
        unsigned int pos;

        /* remember the profile menu handle */
        profile_menu_ = menuhdl;
        
        /* delete all of the items in the submenu */
        for (pos = GetMenuItemCount(menuhdl) - 1 ; ; --pos)
        {
            /* delete it */
            DeleteMenu(menuhdl, pos, MF_BYPOSITION);

            /* if that was the 0th item in the menu, we're done */
            if (pos == 0)
                break;
        }

        /* load the menu with profiles */
        load_menu_with_profiles(menuhdl);
    }

    /* inherit default handling */
    CHtmlSys_framewin::init_menu_popup(menuhdl, pos, sysmenu);
}

/*
 *   Load a menu with preferences profile names 
 */
void CHtmlSys_mainwin::load_menu_with_profiles(HMENU menuhdl)
{
    unsigned int pos;
    HKEY key;
    DWORD disposition;
    const textchar_t *active;
    const textchar_t *dflt;
    char base_key[256];
    char title[256];
    char buf[256];
    int key_idx;
    MENUITEMINFO info;

    /* get the active profile */
    active = prefs_->get_active_profile_name();

    /* note the default profile */
    dflt = prefs_->get_default_profile();

    /* enumerate the profiles, and add a menu item for each one */
    sprintf(base_key, "%s\\Profiles", w32_pref_key_name);
    key = CTadsRegistry::open_key(HKEY_CURRENT_USER, base_key,
                                  &disposition, TRUE);
    for (key_idx = 0, pos = 0 ; ; ++key_idx, ++pos)
    {
        char subkey[128];
        DWORD len;
        FILETIME ft;
        
        /* get the next profile name from the registry */
        len = sizeof(subkey);
        if (RegEnumKeyEx(key, key_idx, subkey, &len, 0, 0, 0, &ft)
            != ERROR_SUCCESS)
            break;
        
        /* set up to insert a menu item for the menu */
        info.cbSize = menuiteminfo_size_;
        info.fMask = MIIM_TYPE | MIIM_DATA | MIIM_ID | MIIM_STATE;
        info.fType = MFT_STRING | MFT_RADIOCHECK;
        info.fState = MFS_ENABLED;
        info.wID = ID_PROFILE_FIRST + key_idx + 1;
        info.dwTypeData = subkey;
        
        /* if this is the active profile, check it */
        if (stricmp(subkey, active) == 0)
            info.fState |= MFS_CHECKED;

        /* if it's the default, note it in the menu name */
        if (stricmp(subkey, dflt) == 0)
            strcat(subkey, " (Default for new games)");

        /* add the key to the menu */
        info.cch = strlen(subkey);
        InsertMenuItem(menuhdl, pos, TRUE, &info);
    }

    /* done with the key */
    CTadsRegistry::close_key(key);

    /* add a separator */
    info.cbSize = menuiteminfo_size_;
    info.fMask = MIIM_TYPE;
    info.fType = MFT_SEPARATOR;
    InsertMenuItem(menuhdl, pos++, TRUE, &info);

    /* add the "Add/Delete Themes" item */
    LoadString(CTadsApp::get_app()->get_instance(), IDS_MANAGE_PROFILES,
               title, sizeof(title));
    info.fMask = MIIM_TYPE | MIIM_DATA | MIIM_ID | MIIM_STATE;
    info.fType = MFT_STRING;
    info.fState = MFS_ENABLED;
    info.wID = ID_MANAGE_PROFILES;
    info.dwTypeData = title;
    info.cch = strlen(title);
    InsertMenuItem(menuhdl, pos++, TRUE, &info);

    /* add the "Set default to <current>" item */
    LoadString(CTadsApp::get_app()->get_instance(), IDS_SET_DEF_PROFILE,
               buf, sizeof(buf));
    sprintf(title, buf, active);
    info.fMask = MIIM_TYPE | MIIM_DATA | MIIM_ID | MIIM_STATE;
    info.fType = MFT_STRING;
    info.fState = MFS_ENABLED;
    info.wID = ID_SET_DEF_PROFILE;
    info.dwTypeData = title;
    info.cch = strlen(title);
    InsertMenuItem(menuhdl, pos++, TRUE, &info);

    /* add the "Customize Theme" item */
    LoadString(CTadsApp::get_app()->get_instance(), IDS_CUSTOMIZE_THEME,
               base_key, sizeof(base_key));
    sprintf(title, base_key, prefs_->get_active_profile_name());
    info.cch = strlen(title);
    info.wID = ID_APPEARANCE_OPTIONS;
    InsertMenuItem(menuhdl, pos++, TRUE, &info);
}

/*
 *   Get the DirectSound interface object.  We'll return the existing
 *   interface if we've already obtained it; if we haven't obtained it yet,
 *   we'll try to get it now.  
 */
IDirectSound *CHtmlSys_mainwin::get_directsound()
{
    HINSTANCE dsound;
    int warned;

    /* if we've already obtained the DirectSound interface, return it */
    if (directsound_ != 0)
        return directsound_;

    /* 
     *   if we've previously tried to create the DirectSound object and
     *   failed during this session, don't bother trying again 
     */
    if (directsound_init_err_ != HTMLW32_DIRECTX_OK)
        return 0;

    /* we haven't issued a warning yet */
    warned = FALSE;

    /* load the DirectSound DLL and get the DirectSoundCreate function */
    dsound = LoadLibrary("DSOUND.DLL");
    if (dsound != 0)
    {
        HRESULT (WINAPI *dscproc)(LPGUID, LPDIRECTSOUND *, LPUNKNOWN);

        dscproc = (HRESULT (WINAPI *)(LPGUID, LPDIRECTSOUND *, LPUNKNOWN))
                  GetProcAddress(dsound, "DirectSoundCreate");
        if (dscproc != 0)
        {
            /* create the Windows DirectSound interface */
            if ((*dscproc)(0, &directsound_, 0) == DS_OK)
            {
                WAVEFORMATEX fmt;
                DSBUFFERDESC bufdesc;
                IDirectSoundBuffer *buf;
                IDirectSoundNotify *inotify;
                int vsnok;

                /* 
                 *   set our DirectSound cooperative level to "normal", since
                 *   we don't need anything fancy 
                 */
                directsound_->SetCooperativeLevel(handle_, DSSCL_NORMAL);

                /* presume the version will be incorrect */
                vsnok = FALSE;

                /*
                 *   We've created the DirectSound interface object
                 *   successfully, but there's still the matter of the
                 *   DirectX version.  We need to be able to use the
                 *   IDirectSoundNotify interface, which only exists in
                 *   DirectX version 5 and higher.  Try creating a sound
                 *   buffer and creating a Notify interface; if that fails,
                 *   we can't use this DirectSound system because it's too
                 *   old a version.  Note that we're just making up a fake
                 *   sound descriptor here so that we can create a sound
                 *   buffer; this is a simple sound descriptor that should be
                 *   compatible with any directx installation.  
                 */
                memset(&bufdesc, 0, sizeof(bufdesc));
                bufdesc.dwSize = sizeof(bufdesc);
                bufdesc.dwFlags = DSBCAPS_CTRLPOSITIONNOTIFY
                                  | DSBCAPS_CTRLDEFAULT
                                  | DSBCAPS_GETCURRENTPOSITION2;
                bufdesc.dwBufferBytes = 10000;
                fmt.wFormatTag = WAVE_FORMAT_PCM;
                fmt.nChannels = 1;
                fmt.nSamplesPerSec = 8000;
                fmt.nAvgBytesPerSec = 8000;
                fmt.nBlockAlign = 1;
                fmt.wBitsPerSample = 8;
                fmt.cbSize = 0;
                bufdesc.lpwfxFormat = &fmt;
                if (directsound_->CreateSoundBuffer(&bufdesc, &buf, 0)
                    == DS_OK)
                {
                    /* 
                     *   we have a sound buffer - ask it for the
                     *   IDirectSoundNotify interface 
                     */
                    if (buf->QueryInterface(IID_IDirectSoundNotify,
                                            (void **)&inotify) == DS_OK)
                    {
                        /* we're done with the notifier - release it */
                        inotify->Release();

                        /* success */
                        vsnok = TRUE;

                        /*
                         *   If DirectX initialization failed in the past,
                         *   note that it's working now, and turn on sounds.
                         *   If there wasn't an error last time, respect the
                         *   sound settings from last time.  
                         */
                        if (prefs_->get_directx_error_code()
                            != HTMLW32_DIRECTX_OK)
                        {
                            /* remember that there's no error */
                            prefs_->
                                set_directx_error_code(HTMLW32_DIRECTX_OK);

                            /* turn on sounds */
                            prefs_->set_sounds_on(TRUE);
                        }
                    }
                    else
                    {
                        OSVERSIONINFO os_info;

                        /*
                         *   We couldn't create the IDirectSoundNotify
                         *   interface.  As a special case, if we're running
                         *   on NT 4, allow this (since we evidently have a
                         *   DirectSound installation that we can otherwise
                         *   use) without even warning about it, since NT 4
                         *   users can't have DirectX 5 even if they want it
                         *   (Microsoft doesn't support it).  
                         */
                        os_info.dwOSVersionInfoSize = sizeof(os_info);
                        GetVersionEx(&os_info);
                        if (os_info.dwPlatformId == VER_PLATFORM_WIN32_NT
                            && os_info.dwMajorVersion == 4)
                            vsnok = TRUE;
                    }

                    /* we're done with the buffer - release it */
                    buf->Release();
                }

                /*
                 *   If the version check failed, and notify the user of the
                 *   problem.  However, do keep the IDirectSound interface,
                 *   since we can use it in DirectX 3 mode.  This doesn't
                 *   work as well as DirectX 5 mode, which is why we take the
                 *   trouble to warn the user about it, but we'll still be
                 *   able to operate correctly anyway.  
                 */
                if (!vsnok)
                {
                    /* warn the user */
                    show_directx_warning(TRUE, HTMLW32_DIRECTX_BADVSN);
                    
                    /* we've warned the user */
                    warned = TRUE;
                }
            }
        }
    }

    /*
     *   check to see if direct sound initialization failed 
     */
    if (directsound_ == 0)
    {
        /* if we haven't warned yet, do so now */
        if (!warned)
            show_directx_warning(TRUE, HTMLW32_DIRECTX_MISSING);

        /* turn off sound in the preferences */
        prefs_->set_sounds_on(FALSE);

        /* if we successfully loaded the library, free it */
        if (dsound != 0)
            FreeLibrary(dsound);
    }

    /* return the interface, if we got one */
    return directsound_;
}


/*
 *   timer events 
 */
int CHtmlSys_mainwin::do_timer(int timer_id)
{
    /* see if we recognize the timer */
    if (timer_id == idle_timer_id_)
    {
        /* 
         *   it's our idle timer - update the elapsed time display if
         *   desired 
         */
        if (prefs_->get_show_timer() && statusline_ != 0)
        {
            char buf[128];
            
            /* 
             *   if a game is running, show the timer; otherwise, leave the
             *   display alone (leaving whatever we last displayed there, if
             *   anything) 
             */
            if (main_panel_->is_game_running())
            {
                DWORD elapsed;

                /* check to see if we're in a pause */
                if (timer_paused_)
                {
                    /* get the elapsed time until the pause */
                    elapsed = (pause_starting_ticks_ - starting_ticks_)/1000;
                }
                else
                {
                    /* get the elapsed seconds since the start of the game */
                    elapsed = (GetTickCount() - starting_ticks_)/1000;
                }

                /* format into hours, minutes, and seconds */
                if (prefs_->get_show_timer_seconds())
                {
                    /* format the full display, with seconds */
                    sprintf(buf, " %d:%02d:%02d",
                            (int)(elapsed / 3600),
                            (int)((elapsed % 3600) / 60),
                            (int)(elapsed % 60));
                }
                else
                {
                    /* only include the hours and minutes */
                    sprintf(buf, " %d:%02d",
                            (int)(elapsed / 3600),
                            (int)((elapsed % 3600) / 60));
                }

                /* set the text in panel 1 */
                SendMessage(statusline_->get_handle(), SB_SETTEXT,
                            1, (LPARAM)buf);
            }
        }

        /* handled */
        return TRUE;
    }
    else
    {
        /* not one of ours - inherit default handling */
        return CHtmlSys_framewin::do_timer(timer_id);
    }
}

/*
 *   Create our toolbar 
 */
void CHtmlSys_mainwin::create_toolbar()
{
    TBBUTTON buttons[30];
    int i;

    /*
     *   Set up the button descriptors 
     */

    /* start at the first button */
    i = 0;

    /* load game */
    buttons[i].iBitmap = 0;
    buttons[i].idCommand = ID_FILE_LOADGAME;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* save */
    buttons[i].iBitmap = 1;
    buttons[i].idCommand = ID_FILE_SAVEGAME;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* restore */
    buttons[i].iBitmap = 2;
    buttons[i].idCommand = ID_FILE_RESTOREGAME;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* separator */
    buttons[i].iBitmap = 0;
    buttons[i].idCommand = 0;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_SEP;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* cut */
    buttons[i].iBitmap = 3;
    buttons[i].idCommand = ID_EDIT_CUT;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* copy */
    buttons[i].iBitmap = 4;
    buttons[i].idCommand = ID_EDIT_COPY;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* paste */
    buttons[i].iBitmap = 5;
    buttons[i].idCommand = ID_EDIT_PASTE;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* separator */
    buttons[i].iBitmap = 0;
    buttons[i].idCommand = 0;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_SEP;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* undo command */
    buttons[i].iBitmap = 6;
    buttons[i].idCommand = ID_CMD_UNDO;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* separator */
    buttons[i].iBitmap = 0;
    buttons[i].idCommand = 0;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_SEP;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* decrease font size */
    buttons[i].iBitmap = 7;
    buttons[i].idCommand = ID_VIEW_FONTS_NEXT_SMALLER;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* increase font size */
    buttons[i].iBitmap = 8;
    buttons[i].idCommand = ID_VIEW_FONTS_NEXT_LARGER;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* profiles */
    buttons[i].iBitmap = 13;
    buttons[i].idCommand = ID_THEMES_DROPDOWN;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON | TBSTYLE_DROPDOWN;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* options */
    buttons[i].iBitmap = 9;
    buttons[i].idCommand = ID_VIEW_OPTIONS;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* separator */
    buttons[i].iBitmap = 0;
    buttons[i].idCommand = 0;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_SEP;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* previous page */
    buttons[i].iBitmap = 10;
    buttons[i].idCommand = ID_GO_PREVIOUS;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* next page */
    buttons[i].iBitmap = 11;
    buttons[i].idCommand = ID_GO_NEXT;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* find text */
    buttons[i].iBitmap = 12;
    buttons[i].idCommand = ID_EDIT_FIND;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* separator */
    buttons[i].iBitmap = 0;
    buttons[i].idCommand = 0;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_SEP;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* mute sound */
    buttons[i].iBitmap = (prefs_->get_mute_sound() ? 15 : 14);
    buttons[i].idCommand = ID_MUTE_SOUND;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* set up the toolbar */
    toolbar_ = CreateToolbarEx(handle_,
                               WS_CHILD | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT,
                               0, 1, CTadsApp::get_app()->get_instance(),
                               IDB_TERP_TOOLBAR, buttons, i,
                               16, 15, 16, 15, sizeof(buttons[0]));
    if (toolbar_ != 0)
    {
        /* set the draw-drop-arrows extended style */
        SendMessage(toolbar_, TB_SETEXTENDEDSTYLE, 0,
                    TBSTYLE_EX_DRAWDDARROWS);

        /* show the toolbar window */
        ShowWindow(toolbar_, prefs_->get_toolbar_vis() ? SW_SHOW : SW_HIDE);

        /* set up the toolbar to do idle status updating */
        add_toolbar_proc(toolbar_);
    }
}

/*
 *   Update the recent game menu in the File menu 
 */
void CHtmlSys_mainwin::set_recent_game_menu()
{
    HMENU file_menu;
    int i;
    int found;
    int id;
    const textchar_t *order;
    const textchar_t *fname;
    MENUITEMINFO info;
    int first_idx;
    const textchar_t *working_dir;

    /* ignore this if we're running as part of the debugger */
    if (in_debugger_)
        return;
    
    /* get the File menu - it's the first submenu */
    file_menu = GetSubMenu(GetMenu(handle_), 0);

    /* find the first recent game item in the current menu */
    for (i = 0, found = FALSE ; i < GetMenuItemCount(file_menu) ; ++i)
    {
        /* get this item's ID */
        id = GetMenuItemID(file_menu, i);

        /* note if it's one of the special recent game items */
        if ((id >= ID_FILE_RECENT_1 && id <= ID_FILE_RECENT_4)
            || id == ID_FILE_RECENT_NONE)
        {
            /* note that we found it */
            found = TRUE;

            /* stop searching */
            break;
        }
    }

    /* if we found a recent game item, delete all of them */
    if (found)
    {
        /* note the starting index */
        first_idx = i;
        
        /* delete all of the recent game items */
        for (;;)
        {
            /* delete the current item */
            DeleteMenu(file_menu, first_idx, MF_BYPOSITION);
            
            /* keep deleting until we've deleted them all */
            id = GetMenuItemID(file_menu, first_idx);

            /* if this one isn't one of the recent items, we're done */
            if ((id < ID_FILE_RECENT_1 || id > ID_FILE_RECENT_4)
                && id != ID_FILE_RECENT_NONE)
                break;
        }

        /* 
         *   if we're runing as a stand-alone executable, we don't want to
         *   offer load options at all - in this case, delete the
         *   separator after this section as well, since the entire
         *   section is being deleted 
         */
        if (standalone_exe_)
        {
            /* 
             *   delete the next item, which is the separator after the
             *   recent games section 
             */
            DeleteMenu(file_menu, first_idx, MF_BYPOSITION);

            /* 
             *   delete the "load new game" item and the separator that
             *   follows it 
             */
            DeleteMenu(file_menu, 0, MF_BYPOSITION);
            DeleteMenu(file_menu, 0, MF_BYPOSITION);
        }
    }

    /* 
     *   if we're running as a bundled stand-alone executable, do not add
     *   the recent games menu 
     */
    if (standalone_exe_)
        return;

    /* get the game ordering string */
    order = prefs_->get_recent_game_order();

    /* set up the basic MENUITEMINFO entries */
    info.cbSize = menuiteminfo_size_;
    info.fMask = MIIM_TYPE | MIIM_DATA | MIIM_ID;
    info.fType = MFT_STRING;

    /* 
     *   check to see if we have any recent games in the prefs - if the
     *   first entry is blank, there aren't any games 
     */
    fname = prefs_->get_recent_game(order[0]);
    if (fname[0] == '\0')
    {
        char title[256];
        
        /* re-insert the "no recent games" placeholder */
        LoadString(CTadsApp::get_app()->get_instance(),
                   IDS_NO_RECENT_GAMES_MENU, title, sizeof(title));
        info.wID = ID_FILE_RECENT_NONE;
        info.dwTypeData = title;
        info.cch = strlen(title);
        InsertMenuItem(file_menu, first_idx, TRUE, &info);
        
        /* we're done */
        return;
    }

    /*
     *   Get the working directory - we'll show all of the filenames in
     *   the recent file list relative to this directory.  Use the current
     *   open-file dialog start directory; if we don't have one defined,
     *   use the initial open-file directory from the preferences. 
     */
    working_dir = CTadsApp::get_app()->get_openfile_dir();
    if (working_dir == 0 || working_dir[0] == '\0')
        working_dir = prefs_->get_init_open_folder();

    /* insert as many items as we have defined */
    for (i = 0 ; i < 4 && i < (int)get_strlen(order) ; ++i)
    {
        char buf[OSFNMAX + 10];
        const textchar_t *title;
        
        /* get this filename */
        fname = prefs_->get_recent_game(order[i]);

        /* if this one's empty, we're done */
        if (fname[0] == '\0')
            break;

        /* build the menu string prefix */
        sprintf(buf, "&%d ", i + 1);

        /* see if we have a title in the game chest records */
        if ((title = look_up_fav_title_by_filename(fname)) != 0)
        {
            /* we got a title for the game - use it in the menu */
            sprintf(buf + strlen(buf), "%.*s", (int)OSFNMAX, title);
        }
        else
        {
            /* 
             *   No title for the game is available from the game chest, so
             *   the best we can do is show the filename.  Get the filename
             *   relative to the open-file-dialog directory. 
             */
            if (working_dir != 0)
            {
                /* get the path relative to the working directory */
                oss_get_rel_path(buf + strlen(buf),
                                 sizeof(buf) - strlen(buf),
                                 fname, working_dir);
            }
            else
            {
                /* no working directory - use the absolute path */
                strncpy(buf + strlen(buf), fname, sizeof(buf) - strlen(buf));
                buf[sizeof(buf) - 1] = '\0';
            }
        }

        /* add this item */
        info.wID = ID_FILE_RECENT_1 + i;
        info.dwTypeData = buf;
        info.cch = strlen(buf);
        InsertMenuItem(file_menu, first_idx + i, TRUE, &info);
    }
}

/*
 *   Add a game to the list of recent games.  The new game will become the
 *   most recent game in the list, and previous entries will be pushed
 *   down one to make room.  If we find the game in the list already, it
 *   will simply be moved to the front of the list.  
 */
void CHtmlSys_mainwin::add_recent_game(const textchar_t *new_game)
{
    int i;
    int j;
    const textchar_t *p;
    char order[10];
    const textchar_t *fname;
    char first;

    /* get the order string */
    p = prefs_->get_recent_game_order();
    strncpy(order, p, 4);
    order[4] = '\0';

    /* 
     *   First, search the current list to see if the game is already
     *   there.  If it is, we simply shuffle the list to move this game to
     *   the front of the list.  
     */
    for (i = 0 ; i < 4 ; ++i)
    {
        /* get this item */
        fname = prefs_->get_recent_game(order[i]);

        /* if this is the same game, we can simply shuffle the list */
        if (get_stricmp(new_game, fname) == 0)
        {
            /* 
             *   It's already in the list - simply reorder the list to put
             *   this item at the front of the list.  To do this, we
             *   simply move all the items down one until we get to its
             *   old position.  First, note the new first item.
             */
            first = order[i];

            /* move everything down one slot */
            for (j = i ; j > 0 ; --j)
                order[j] = order[j - 1];

            /* fill in the first item */
            order[0] = first;

            /* we're done */
            goto done;
        }
    }

    /* 
     *   Okay, it's not already in the list, so we need to add it to the
     *   list.  To do this, drop the last item off the list, and move
     *   everything else down one slot.  First, note the current last slot
     *   - we're going to drop the file that's currently there and make it
     *   the new first slot.  
     */
    first = order[3];

    /* move everything down one position */
    for (j = 3 ; j > 0 ; --j)
        order[j] = order[j - 1];

    /* re-use the old last slot as the new first slot */
    order[0] = first;

    /* set the new first filename */
    prefs_->set_recent_game(order[0], new_game);

done:
    /* set the new ordering */
    prefs_->set_recent_game_order(order);
    
    /* rebuild the game list */
    set_recent_game_menu();
}

/*
 *   Load a game from the recently-played list 
 */
void CHtmlSys_mainwin::load_recent_game(int idx)
{
    const textchar_t *order;
    const textchar_t *fname;

    /* warn about ending any current game */
    if (!query_end_game())
        return;
    
    /* get the order string */
    order = prefs_->get_recent_game_order();

    /* if the index is out of range, ignore it */
    if (idx < 0 || idx >= (int)get_strlen(order))
        return;

    /* get the filename for this item */
    fname = prefs_->get_recent_game(order[idx]);

    /* start the new game */
    load_new_game(fname);
}


/*
 *   Show a DirectX warning 
 */
void CHtmlSys_mainwin::show_directx_warning(int init,
                                            htmlw32_directx_err_t warn_type)
{
    CTadsDlg_dxwarn *dlg;
    
    /*
     *   Check to see if the user has previously told us to suppress this
     *   type of warning.  We'll only suppress the warnings on startup,
     *   since at other times, the user has explicitly done something
     *   requesting sound. 
     */
    if (init)
    {
        /* 
         *   remember the cause of the failure, in case we need to
         *   generate another message about this later 
         */
        directsound_init_err_ = warn_type;

        /* 
         *   if the error code is different than last time, turn off the
         *   "hide this warning" checkbox, since the user didn't tell us
         *   to hide this new warning 
         */
        if (warn_type != prefs_->get_directx_error_code())
            prefs_->set_directx_hidewarn(FALSE);

        /*
         *   if the user told us to hide this warning last time, skip the
         *   dialog 
         */
        if (prefs_->get_directx_hidewarn())
            return;
    }

    /* run the dialog */
    dlg = new CTadsDlg_dxwarn(prefs_, warn_type);
    dlg->run_modal(DLG_DIRECTX_WARNING, handle_,
                   CTadsApp::get_app()->get_instance());
    delete dlg;

    /* 
     *   Remember the error type persistently for the next startup.  Note
     *   that it's important to wait until *after* we've run the dialog,
     *   because the dialog needs to be able to check the *old* value of
     *   the error code in the preferences - this way, it can determine if
     *   the checkbox setting is still applicable, since it only applies
     *   when the current error is the same as the old error. 
     */
    prefs_->set_directx_error_code(warn_type);
}


/*
 *   Receive notification that a character mapping file has been loaded.
 *   For Windows character mapping table files, the extra system
 *   information is the Windows code page that should be used with the
 *   game's internal character set.  We'll find the Windows xxx_CHARSET
 *   value that corresponds to the Windows code page, and set that as the
 *   default character set. 
 */
void CHtmlSys_mainwin::advise_load_charmap(const char *ldesc,
                                           const char *sysinfo)
{
    int code_page;
    CHARSETINFO csinfo;

    /* remember the internal character set name */
    game_internal_charset_.set(ldesc);
    
    /* if the system information is not a valid code page, ignore it */
    code_page = atoi(sysinfo);
    // if (!IsValidCodePage(code_page)) // for now, allow uninstalled cp's
    if (code_page == 0)
        return;

    /* translate the code page to a character set identifier */
    TranslateCharsetInfo((DWORD *)code_page, &csinfo, TCI_SRCCODEPAGE);

    /* remember the default character set identifier */
    default_charset_.codepage = code_page;
    default_charset_.charset = csinfo.ciCharset;
}


/*
 *   process a close-window request 
 */
int CHtmlSys_mainwin::do_close()
{
    /* 
     *   check for downloads in progress, and confirm that we want to cancel
     *   them if we have any 
     */
    if (!do_close_check_downloads())
        return FALSE;

    /* send a QUIT command to the main command window, if possible */
    if (main_panel_ != 0)
    {
        char buf[256];

        /* 
         *   if the main panel is awaiting a keystroke before exiting,
         *   treat this as equivalent - simply send a space key so that we
         *   can exit 
         */
        if (main_panel_->get_exit_pause())
        {
            /* send a space key */
            PostMessage(main_panel_->get_handle(), WM_CHAR, (WPARAM)' ', 0);

            /* allow the window to close */
            goto allow_close;
        }

        /* 
         *   if the game is paused in the debugger, simply hide the
         *   window, but do not close it 
         */
        if (in_debugger_ && game_paused_)
        {
            /* hide it */
            ShowWindow(handle_, SW_HIDE);

            /* we're done - don't actually close the window */
            return FALSE;
        }

        /* 
         *   if we're already at EOF, there's nothing extra we need to do;
         *   simply close the window if we're in the normal interpreter,
         *   or do nothing at all if we're in the debugger 
         */
        if (main_panel_->get_eof_flag())
        {
            /* check to see if we're in the debugger */
            if (in_debugger_)
            {
                /* 
                 *   we're in the debugger - the game must already be
                 *   terminated, so do nothing 
                 */
                return FALSE;
            }
            else
            {
                /* 
                 *   we're in the normal interpreter, and we're at EOF -
                 *   simply close the window 
                 */
                goto allow_close;
            }
        }

        /* check the "quit" option setting */
        switch(prefs_->get_close_action())
        {
        case HTML_PREF_CLOSE_CMD:
        default:
            /* 
             *   Process a system CLOSE command.  If the game is asking for a
             *   command line, return it as "QUIT", but use the more forceful
             *   CLOSE as the event.  
             */
            process_command("quit", 4, FALSE, TRUE, OS_CMD_CLOSE);

            /* don't actually close the window yet */
            return FALSE;

        case HTML_PREF_CLOSE_PROMPT:
            /* ask the user whether they really want to quit */
            LoadString(CTadsApp::get_app()->get_instance(),
                       IDS_REALLY_QUIT_MSG, buf, sizeof(buf));
            switch(MessageBox(handle_, buf, "TADS",
                              MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2))
            {
            case IDYES:
            case 0:
                /* go close the window */
                goto do_close_window;

            default:
                /* ignore the close request */
                return FALSE;
            }

        case HTML_PREF_CLOSE_NOW:
        do_close_window:
            /* terminate input in the game window */
            main_panel_->set_eof_flag(TRUE);

            /*
             *   if we're in the debugger, let the game terminate by
             *   detecting the end-of-file on the next input; otherwise,
             *   simply close the window so that we terminate the
             *   interpreter 
             */
            if (in_debugger_)
            {
                /* hide the window, to simulate closing it */
                ShowWindow(handle_, SW_HIDE);
                
                /* we're done - don't actually close the game window */
                return FALSE;
            }
            else
            {
                /* close the game window and terminate execution */
                goto allow_close;
            }
        }

    allow_close:
        /* cancel all active sound and other media playback */
        main_panel_->get_formatter()->cancel_sound(HTML_Attrib_invalid);
        main_panel_->get_formatter()->cancel_playback();

        /* allow the closing to proceed */
        return TRUE;
    }
    else
    {
        /* no main panel - just close normally */
        return TRUE;
    }
}


/*
 *   destroy the window 
 */
void CHtmlSys_mainwin::do_destroy()
{
    int i;
    CHtmlSysWin_win32 **banner;
    
    /* remove our background timer */
    if (idle_timer_id_ != 0)
    {
        KillTimer(handle_, idle_timer_id_);
        free_timer_id(idle_timer_id_);
    }

    /* remove the toolbar timer */
    if (toolbar_ != 0)
        rem_toolbar_proc(toolbar_);

    /* tell my child windows to disengage from their parent */
    main_panel_->on_destroy_parent();
    main_panel_->forget_owner();
    hist_panel_->on_destroy_parent();
    hist_panel_->forget_owner();
    for (i = 0, banner = banners_ ; i < banner_cnt_ ; ++i, ++banner)
    {
        (*banner)->on_destroy_parent();
        (*banner)->forget_owner();
    }
    
    /* forget about our subwindow */
    main_panel_ = 0;
    hist_panel_ = 0;

    /* cancel any pending downloads */
    cancel_downloads();

    /* terminate the application */
    PostQuitMessage(0);
    
    /* inherit default */
    CTadsWin::do_destroy();
}

/*
 *   process a notification message 
 */
int CHtmlSys_mainwin::do_notify(int /*control_id*/, int notify_code,
                                LPNMHDR nmhdr)
{
    switch(notify_code)
    {
    case TTN_NEEDTEXT:
        /* retreive tooltip text for a button */
        {
            TOOLTIPTEXT *ttt = (TOOLTIPTEXT *)nmhdr;
            struct cmd_to_str_t
            {
                unsigned int cmd;
                unsigned int str;
            }
            static cmd_to_str[] =
            {
                { ID_FILE_LOADGAME,           IDS_FILE_LOADGAME },
                { ID_FILE_SAVEGAME,           IDS_FILE_SAVEGAME },
                { ID_FILE_RESTOREGAME,        IDS_FILE_RESTOREGAME },
                { ID_EDIT_CUT,                IDS_EDIT_CUT },
                { ID_EDIT_COPY,               IDS_EDIT_COPY },
                { ID_EDIT_PASTE,              IDS_EDIT_PASTE },
                { ID_CMD_UNDO,                IDS_CMD_UNDO },
                { ID_VIEW_FONTS_NEXT_SMALLER, IDS_VIEW_FONTS_NEXT_SMALLER },
                { ID_VIEW_FONTS_NEXT_LARGER,  IDS_VIEW_FONTS_NEXT_LARGER },
                { ID_VIEW_OPTIONS,            IDS_VIEW_OPTIONS },
                { ID_GO_PREVIOUS,             IDS_GO_PREVIOUS },
                { ID_GO_NEXT,                 IDS_GO_NEXT },
                { ID_EDIT_FIND,               IDS_EDIT_FIND },
                { ID_THEMES_DROPDOWN,         IDS_THEMES_DROPDOWN },
                { ID_MUTE_SOUND,              IDS_MUTE_SOUND },

                /* end-of-list marker */
                { 0, 0 }
            };
            cmd_to_str_t *p;

            /* get the strings from my application instance */
            ttt->hinst = CTadsApp::get_app()->get_instance();

            /* 
             *   if it's the 'customize xxx theme' item, we need to build the
             *   full string 
             */
            if (ttt->hdr.idFrom == ID_THEMES_DROPDOWN)
            {
                char buf[128];
                static char title[256];

                /* build the name */
                LoadString(ttt->hinst, IDS_THEMES_DROPDOWN, buf, sizeof(buf));
                sprintf(title, buf, prefs_->get_active_profile_name());

                /* return the static buffer */
                ttt->lpszText = title;
                return TRUE;
            }

            /* find the message to use for the given command */
            for (p = cmd_to_str ; p->cmd != 0 ; ++p)
            {
                /* is this is the one we're looking for? */
                if (p->cmd == ttt->hdr.idFrom)
                {
                    /* the command matches - set the string ID */
                    ttt->lpszText = MAKEINTRESOURCE(p->str);

                    /* 
                     *   no need to look any further - tell the caller that
                     *   the notification was successfully handled 
                     */
                    return TRUE;
                }
            }

            /* 
             *   we didn't find the command in our list - tell the caller
             *   that we didn't handle the notification 
             */
            return FALSE;
        }

    case NM_RCLICK:
        /* 
         *   right-click in a control - if it's the status bar, process the
         *   status bar menu 
         */
        if (statusline_ != 0 && nmhdr->hwndFrom == statusline_->get_handle())
        {
            POINT pt;
            
            /* get the mouse position */
            GetCursorPos(&pt);
            ScreenToClient(handle_, &pt);

            /* run the statusline popup menu */
            track_context_menu(statusline_popup_, pt.x, pt.y);

            /* handled */
            return TRUE;
        }

        /* not handled */
        return FALSE;

    case TBN_DROPDOWN:
        /* drop-down toolbar button */
        if (nmhdr->hwndFrom == toolbar_)
        {
            NMTOOLBAR *nmt;
            HMENU mnu;
            TBBUTTONINFO tbinfo;
            int idx;
            RECT brc;
            POINT pt;
            int cmd;
            
            /* get the notification structure, properly cast */
            nmt = (NMTOOLBAR *)nmhdr;

            /* check which button we're dropping down */
            switch(nmt->iItem)
            {
            case ID_THEMES_DROPDOWN:
                /* 
                 *   it's the profile manager - set up a popup menu with the
                 *   names of the available profiles. 
                 */
                mnu = CreatePopupMenu();
                load_menu_with_profiles(mnu);

                /* get the index of the button */
                tbinfo.cbSize = sizeof(tbinfo);
                tbinfo.dwMask = TBIF_COMMAND;
                idx = SendMessage(toolbar_, TB_GETBUTTONINFO,
                                  ID_THEMES_DROPDOWN, (LPARAM)&tbinfo);

                /* get the location of the button in our local coordinates */
                SendMessage(toolbar_, TB_GETITEMRECT, idx, (LPARAM)&brc);
                pt.x = brc.left;
                pt.y = brc.bottom;
                ClientToScreen(toolbar_, &pt);
                ScreenToClient(handle_, &pt);

                /* mark this as the active profile menu */
                profile_menu_ = mnu;

                /* run the menu as a popup */
                cmd = track_context_menu_ext(mnu, pt.x, pt.y,
                                             TPM_TOPALIGN | TPM_LEFTALIGN
                                             | TPM_RETURNCMD);

                /* 
                 *   If we got a valid command code, process it.  Note that
                 *   we explicitly send the command message to ourselves,
                 *   because this way we are sure to process it before we
                 *   destroy the menu; the menu is the source of the profile
                 *   name list, so we really must finish processing the
                 *   command before we can go on to destroy the menu. 
                 */
                if (cmd != 0)
                    SendMessage(handle_, WM_COMMAND, cmd, 0);

                /* done with the menu - destroy it */
                DestroyMenu(mnu);

                /* the profile menu is no longer valid */
                profile_menu_ = 0;

                /* handled as a drop-down */
                return TBDDRET_DEFAULT;
            }
        }

        /* not handled */
        return FALSE;

    default:
        /* not handled */
        return FALSE;
    }
}

/*
 *   process window activation 
 */
int CHtmlSys_mainwin::do_activate(int flag, int, HWND)
{
    /* 
     *   If we're activating, establish our accelerator.  This will ensure
     *   that our keyboard accelerators will be used while we're in the
     *   foreground; this is necessary if we're running under the debugger,
     *   since the debugger has its own set of accelerator keys that it
     *   installs while it's active.  
     */
    if (flag != WA_INACTIVE)
        CTadsApp::get_app()->set_accel(main_panel_->get_accel(),
                                       get_handle());

    /* continue with default processing */
    return FALSE;
}

/*
 *   process application activation 
 */
int CHtmlSys_mainwin::do_activate_app(int flag, DWORD)
{
    /* do any necessary game chest work on activation */
    do_activate_game_chest(flag);

    /* if we're deactivating, save our current preference settings */
    if (!flag)
    {
        /* if we have any unsaved preference changes, save them */
        if (prefs_ != 0 && !prefs_->equals_saved(TRUE))
        {
            /* save our preferences */
            prefs_->save();
        }
    }

    /* continue with default system processing */
    return FALSE;
}

/*
 *   erase the background 
 */
int CHtmlSys_mainwin::do_erase_bkg(HDC hdc)
{
    /* 
     *   if we have a toolbar, erase the area at the top of the window
     *   behind the toolbar 
     */
    if (toolbar_ != 0 && IsWindowVisible(toolbar_))
    {
        RECT tbrc;
        RECT rc;
        POINT pt;

        /* get the toolbar's area relative to our client area */
        GetWindowRect(toolbar_, &tbrc);
        pt.x = pt.y = 0;
        ClientToScreen(handle_, &pt);
        OffsetRect(&tbrc, -pt.x, -pt.y);

        /* 
         *   fill the area at the top of the window to the height of the
         *   toolbar (but all the way across our window) with the system
         *   button face color 
         */
        GetClientRect(handle_, &rc);
        rc.bottom = tbrc.bottom;
        FillRect(hdc, &rc, GetSysColorBrush(COLOR_3DFACE));
    }

    /* handled */
    return 1;
}

/*
 *   paint the window contents 
 */
void CHtmlSys_mainwin::do_paint_content(HDC hdc, const RECT *paintrc)
{
    HGDIOBJ oldpen;
    RECT clientrc;
    RECT panelrc;
    int y_offset;
    RECT rc;

    /* get the nominal main panel offset */
    y_offset = main_panel_yoffset_;

    /* adjust for the toolbar, if present */
    if (toolbar_ != 0 && IsWindowVisible(toolbar_))
    {
        RECT tbrc;
        POINT pt;

        /* get the toolbar's area relative to our client area */
        GetWindowRect(toolbar_, &tbrc);
        pt.x = pt.y = 0;
        ClientToScreen(handle_, &pt);
        OffsetRect(&tbrc, -pt.x, -pt.y);

        /* adjust our offset by the toolbar's height */
        y_offset += tbrc.bottom;
    }

    /* erase the background in white */
    FillRect(hdc, paintrc, (HBRUSH)GetStockObject(WHITE_BRUSH));

    /* select a black pen */
    oldpen = SelectObject(hdc, GetStockObject(BLACK_PEN));

    /* get the area left over between the main panel and the status line */
    GetClientRect(handle_, &clientrc);
    GetClientRect(statusline_->get_handle(), &panelrc);
    clientrc.top = clientrc.bottom - panelrc.bottom - 3;
    clientrc.bottom = clientrc.top + 2;

    /* paint a black line across the top and left edges of the main panel */
    MoveToEx(hdc, 0, clientrc.bottom - 1, 0);
    LineTo(hdc, 0, y_offset);
    LineTo(hdc, clientrc.right - 1, y_offset);

    /* put a gray line just below the main panel */
    FillRect(hdc, &clientrc, (HBRUSH)GetStockObject(LTGRAY_BRUSH));

    /* paint a white line down the right side */
    SelectObject(hdc, GetStockObject(WHITE_PEN));
    MoveToEx(hdc, clientrc.right, y_offset, 0);
    LineTo(hdc, clientrc.right, clientrc.bottom + 1);

    /* paint a gray line to the left of the white line */
    SetRect(&rc, clientrc.right - 2, y_offset + 1,
            clientrc.right - 1, clientrc.bottom + 2);
    FillRect(hdc, &rc, GetSysColorBrush(COLOR_3DLIGHT));

    /* done with the pen */
    SelectObject(hdc, oldpen);
}

/*
 *   resize 
 */
void CHtmlSys_mainwin::do_resize(int mode, int x, int y)
{
    /* notify the toolbar that it must resize itself */
    if (toolbar_ != 0 && IsWindowVisible(toolbar_))
        SendMessage(toolbar_, TB_AUTOSIZE, 0, 0);

    switch(mode)
    {
    case SIZE_MAXHIDE:
    case SIZE_MAXSHOW:
    case SIZE_MINIMIZED:
        /* don't bother resizing the subwindows on these changes */
        break;
        
    case SIZE_MAXIMIZED:
    case SIZE_RESTORED:
    default:
        /* notify the status line of the resize */
        statusline_->notify_parent_resize();

        /* recalculate the banner layout */
        recalc_banner_layout();
                
        /* inherit default handling to save position in prefs */
        CHtmlSys_framewin::do_resize(mode, x, y);

        /* done */
        break;
    }
}

/*
 *   exit a size/move operation 
 */
void CHtmlSys_mainwin::do_exitsizemove()
{
    /* inherit default handling */
    CHtmlSys_framewin::do_exitsizemove();
}

/*
 *   Recalculate the banner layout for a change in the size of one of the
 *   banners. 
 */
void CHtmlSys_mainwin::recalc_banner_layout()
{
    RECT statrc;
    RECT rc;
    int y_offset;
    CHtmlSysWin_win32 *first_banner;

    /* if we're deleting orphaned banners, don't bother */
    if (deleting_orphaned_banners_)
        return;

    /* recalculate the status line layout */
    adjust_statusbar_layout();

    /* get the nominal starting y offset */
    y_offset = main_panel_yoffset_;

    /* adjust for the toolbar, if visible */
    if (toolbar_ && IsWindowVisible(toolbar_))
    {
        RECT tbrc;
        POINT pt;

        /* get the toolbar's area relative to our client area */
        GetWindowRect(toolbar_, &tbrc);
        pt.x = pt.y = 0;
        ClientToScreen(handle_, &pt);
        OffsetRect(&tbrc, -pt.x, -pt.y);

        /* adjust our offset by the toolbar's height */
        y_offset += tbrc.bottom;
    }

    /* 
     *   Figure out the total area available to the subwindows.  The first
     *   pixel on the left and the first pixel on the top of our client area
     *   (less the toolbar area) are taken up by a single-pixel-width
     *   highlight, so start at (1,1 + toolbar_height).  We need two pixels
     *   at the right edge and three at the bottom edge for additional
     *   highlighting, and we also need to remove the area taken up by the
     *   status line.  
     */
    GetClientRect(statusline_->get_handle(), &statrc);
    GetClientRect(handle_, &rc);
    SetRect(&rc, 1, y_offset + 1,
            rc.right - 2, rc.bottom - statrc.bottom - 3);

    /* 
     *   Get the first banner window - this is always the head of the main
     *   panel's banner list, even when showing history, because history
     *   shows only the previous main panel contents, not the old banners.  
     */
    first_banner = (main_panel_ == 0
                    ? 0 : main_panel_->get_first_banner_child());

    /* size the main panel; it'll size its banner children */
    if (main_panel_ != 0 && IsWindowVisible(main_panel_->get_handle()))
        main_panel_->calc_banner_layout(&rc, first_banner);

    /* size the history panel to the same size */
    if (hist_panel_ != 0 && IsWindowVisible(hist_panel_->get_handle()))
        hist_panel_->calc_banner_layout(&rc, first_banner);
}

/*
 *   Adjust the status bar layout 
 */
void CHtmlSys_mainwin::adjust_statusbar_layout()
{
    /* proceed only if we have a status line */
    if (statusline_ != 0)
    {
        int widths[2];
        int parts;

        /* assume only one part */
        parts = 1;

        /* assume the first part gets the whole display */
        widths[0] = -1;

        /* add the timer, if desired */
        if (prefs_->get_show_timer())
        {
            RECT cli_rc;
            HGDIOBJ oldfont;
            SIZE txtsiz;
            HDC dc;

            /* count the timer in the parts list */
            ++parts;

            /* get the available area */
            GetClientRect(handle_, &cli_rc);

            /* get the desktop dc */
            dc = GetDC(handle_);

            /* calculate the width we need for the time panel */
            oldfont = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
            ht_GetTextExtentPoint32(dc, "  00:00:00  ", 12, &txtsiz);

            /* include the scrollbar size */
            txtsiz.cx += GetSystemMetrics(SM_CXVSCROLL);

            /* restore drawing context */
            SelectObject(dc, oldfont);
            ReleaseDC(handle_, dc);

            /* 
             *   add the timer to the widths - give the timer a fixed
             *   portion at the right large enough to show the time, and
             *   give everything else to the preceding parts 
             */
            widths[0] = cli_rc.right - txtsiz.cx;
            widths[1] = -1;
        }

        /* reset the panel layout */
        SendMessage(statusline_->get_handle(), SB_SETPARTS,
                    parts, (LPARAM)widths);
    }
}

/*
 *   Gain focus.  We simply move focus to the main panel window. 
 */
void CHtmlSys_mainwin::do_setfocus(HWND /*previous_focus*/)
{
    /* 
     *   set focus on the history panel if we're viewing a previous page,
     *   otherwise on the main panel 
     */
    if (cur_page_state_ != 0 && cur_page_state_->next_ != 0)
        SetFocus(hist_panel_->get_handle());
    else
        SetFocus(main_panel_->get_handle());
}

/*
 *   user message handler 
 */
int CHtmlSys_mainwin::do_user_message(int msg, WPARAM, LPARAM)
{
    /* check for a preference update */
    if ((UINT)msg == prefs_->get_prefs_updated_msg())
    {
        /* 
         *   if the preference settings in the registry were changed since
         *   the last time we looked, reload the preferences 
         */
        if (prefs_ != 0 && main_panel_ != 0 && !prefs_->equals_saved(TRUE))
        {
            CHtmlSysWin_win32 *panel;
            
            /* synchronize our memory preferences with the saved values */
            prefs_->restore(TRUE);

            /* update the main or history panel, as appropriate */
            if (cur_page_state_ != 0 && cur_page_state_->next_ != 0
                && hist_panel_ != 0)
                panel = hist_panel_;
            else
                panel = main_panel_;
            
            /* make sure we update everything that could be affected */
            panel->schedule_reformat(TRUE, TRUE, FALSE);
            panel->notify_sound_pref_change();
            panel->schedule_reload_game_chest();
        }

        /* handled */
        return TRUE;
    }

    /* not handled */
    return FALSE;
}

/*
 *   Handle a command 
 */
int CHtmlSys_mainwin::do_command(int notify_code,
                                 int command_id, HWND ctl)
{
    int ret;

    /* check to see if it's one of our own commands */
    switch(command_id)
    {
    case ID_FILE_EXIT:
        /* send myself a Close message */
        PostMessage(handle_, WM_CLOSE, 0, 0);

        /* handled */
        return TRUE;
        
    case ID_GO_TO_GAME_CHEST:
        /* 
         *   If a game is running, make sure they know that this will quit
         *   the game.  Otherwise, just show the game chest.  
         */
        if (main_panel_->is_game_running())
        {
            int id;
            char buf[256];

            /* make sure they want to quit immediately */
            LoadString(CTadsApp::get_app()->get_instance(),
                       IDS_REALLY_GO_GC_MSG, buf, sizeof(buf));
            id = MessageBox(handle_, buf, "TADS",
                            MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);

            /* if they answered "yes", proceed */
            if (id == IDYES)
            {
                /* end the current game */
                end_current_game();

                /* 
                 *   set the direct-to-game-chest flag, so that when the
                 *   game finishes terminating and we look to see what we're
                 *   meant to do next, we will know that we should display
                 *   the game chest page 
                 */
                direct_to_game_chest_ = TRUE;
            }
        }
        else
        {
            /* no game is running - show the game chest directly */
            show_game_chest(FALSE, TRUE, FALSE);
        }

        /* no matter what happened, we've fully handled the command */
        return TRUE;

    case ID_SHOW_DEBUG_WIN:
        /* show the debug window and bring it to the top */
        if (dbgwin_ != 0)
        {
            /* show it */
            ShowWindow(dbgwin_->get_handle(), SW_SHOW);

            /* bring it to the top */
            SetWindowPos(dbgwin_->get_handle(), HWND_TOP, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE);
        }
        return TRUE;

#if 0 // transcript viewing - not yet implemented
    case ID_FILE_VIEW_SCRIPT:
        view_script();
        return TRUE;
#endif

    case ID_HELP_ABOUT:
        /* run the "about" box */
        (new CHtmlSys_abouttadswin(this))->run_dlg(this);
        return TRUE;

    case ID_HELP_CONTENTS:
        {
            char buf[OSFNMAX];
            
            /* build the help file's name */
            GetModuleFileName(CTadsApp::get_app()->get_instance(),
                              buf, sizeof(buf));
            strcpy(os_get_root_name(buf), "htmltads.chm::/overview.htm");
            
            /* show the help */
            HtmlHelp(handle_, buf, HH_DISPLAY_TOPIC, 0);
            ShellExecute(0, "open", buf, 0, 0, 0);
        }

        /* handled */
        return TRUE;

    case ID_HELP_ABOUT_GAME:
        if (!main_panel_->get_eof_flag())
            aboutbox_->run_aboutbox(this);
        return TRUE;

    case ID_FILE_RECENT_NONE:
        /* 
         *   this is a placeholder for times when we have never loaded a
         *   game - it's always disabled 
         */
        return TRUE;

    case ID_FILE_LOADGAME:
        /* 
         *   allow this request only in the normal interpreter, and we're
         *   not pausing before termination 
         */
        if (!in_debugger_ && !main_panel_->get_exit_pause())
        {
            /* load a new game */
            load_new_game();
        }

        /* handled */
        return TRUE;

    case ID_FILE_RECENT_1:
    case ID_FILE_RECENT_2:
    case ID_FILE_RECENT_3:
    case ID_FILE_RECENT_4:
        /* load the recent game */
        load_recent_game(command_id - ID_FILE_RECENT_1);
        return TRUE;

    case ID_EDIT_CUT:
    case ID_EDIT_COPY:
    case ID_EDIT_DELETE:
    case ID_EDIT_SELECTALL:
        /* send these commands to the currently focused subwindow */
        {
            CHtmlSysWin_win32 *win;

            if ((win = get_focus_subwin()) != 0)
                return win->do_command(notify_code, command_id, ctl);
        }

        /* if we didn't find a focused subwindow, we didn't handle it */
        break;

    case ID_VIEW_TIMER:
        /* invert the timer visibility in the preferences */
        prefs_->set_show_timer(!prefs_->get_show_timer());

        /* adjust the status display accordingly */
        adjust_statusbar_layout();

        /* handled */
        return TRUE;

    case ID_TIMER_RESET:
        /* reset the timer to the current time */
        starting_ticks_ = GetTickCount();

        /* in case we're paused, reset the pause starting time as well */
        pause_starting_ticks_ = GetTickCount();

        /* handled */
        return TRUE;

    case ID_TIMER_PAUSE:
        /* invert the current pause status */
        timer_paused_ = !timer_paused_;

        /* are we entering or leaving a pause? */
        if (timer_paused_)
        {
            /* 
             *   entering a pause - note the time when the pause started;
             *   this will effectively freeze the timer here, and will tell
             *   us where to back-date the timer when we come out of the
             *   pause 
             */
            pause_starting_ticks_ = GetTickCount();
        }
        else
        {
            /* 
             *   leaving a pause - reset the starting position of the timer
             *   so that subsequent interval calculations will omit the time
             *   we were paused 
             */
            starting_ticks_ = GetTickCount()
                              - (pause_starting_ticks_ - starting_ticks_);
        }

        /* handled */
        return TRUE;

    case ID_VIEW_TIMER_HHMMSS:
        /* turn on seconds in the timer */
        prefs_->set_show_timer_seconds(TRUE);

        /* handled */
        return TRUE;

    case ID_VIEW_TIMER_HHMM:
        /* turn off seconds in the timer */
        prefs_->set_show_timer_seconds(FALSE);

        /* handled */
        return TRUE;

    case ID_VIEW_TOOLBAR:
        /* invert the toolbar visibility in the preferences */
        prefs_->set_toolbar_vis(!prefs_->get_toolbar_vis());

        /* show or hide the toolbar window */
        ShowWindow(toolbar_, prefs_->get_toolbar_vis() ? SW_SHOW : SW_HIDE);

        /* 
         *   synthesize a resize to recalculate the window layout, since
         *   adding or removing the toolbar will change the available area
         *   for the html windows 
         */
        synth_resize();

        /* handled */
        return TRUE;

    default:
        /* if it's a profile selector, change profiles */
        if (command_id >= ID_PROFILE_FIRST
            && command_id <= ID_PROFILE_LAST
            && profile_menu_ != 0)
        {
            MENUITEMINFO info;
            char profile[128];
            
            /* get the name of the profile from the profiles menu item */
            info.cbSize = menuiteminfo_size_;
            info.fMask = MIIM_TYPE;
            info.dwTypeData = profile;
            info.cch = sizeof(profile);
            if (GetMenuItemInfo(profile_menu_, command_id, FALSE, &info))
            {
                /* save the outgoing profile */
                prefs_->save();

                /* switch to the new profile */
                set_game_specific_profile(profile);
            }

            /* handled */
            return TRUE;
        }

        /* 
         *   pass the command to the main panel or to the history panel
         *   (whichever is active) for processing 
         */
        if (cur_page_state_ != 0
            && cur_page_state_->next_ != 0
            && hist_panel_ != 0)
        {
            /* the history panel is active - route the command there */
            ret = hist_panel_->do_command(notify_code, command_id, ctl);
        }
        else if (main_panel_ != 0)
        {
            /* the main panel is active - route the command there */
            ret = main_panel_->do_command(notify_code, command_id, ctl);
        }
        else
        {
            /* nothing's active */
            break;
        }

        /* 
         *   if we changed the mute status, update the toolbar - we use
         *   separate icons on the 'mute' button for the sound on and sound
         *   off states 
         */
        if (command_id == ID_MUTE_SOUND)
        {
            /* change the toolbar button image to reflect the new status */
            SendMessage(toolbar_, TB_CHANGEBITMAP, ID_MUTE_SOUND,
                        (prefs_->get_mute_sound() ? 15 : 14));
        }

        /* return the result */
        return ret;
    }

    /* didn't handle it */
    return FALSE;
}

/*
 *   Check the status of a command 
 */
TadsCmdStat_t CHtmlSys_mainwin::check_command(int command_id)
{
    /* check to see if it's one of our own commands */
    switch(command_id)
    {
    case ID_FILE_EXIT:
        /* we can always exit the application */
        return TADSCMD_ENABLED;
        
    case ID_GO_TO_GAME_CHEST:
        /* 
         *   allow going to the game chest if we're not already there;
         *   always disable the game chest if we're in the debugger 
         */
        return in_debugger_ || main_panel_->is_in_game_chest()
            ? TADSCMD_DISABLED : TADSCMD_ENABLED;

    case ID_VIEW_TOOLBAR:
        return prefs_->get_toolbar_vis() ? TADSCMD_CHECKED : TADSCMD_ENABLED;

    case ID_VIEW_TIMER:
        return prefs_->get_show_timer() ? TADSCMD_CHECKED : TADSCMD_ENABLED;

    case ID_TIMER_RESET:
        return main_panel_->is_game_running()
            ? TADSCMD_ENABLED : TADSCMD_DISABLED;

    case ID_TIMER_PAUSE:
        return (main_panel_->is_game_running()
                ? (timer_paused_ ? TADSCMD_CHECKED : TADSCMD_ENABLED)
                : TADSCMD_DISABLED);

    case ID_VIEW_TIMER_HHMMSS:
        return (prefs_->get_show_timer()
                ? (prefs_->get_show_timer_seconds()
                   ? TADSCMD_CHECKED : TADSCMD_ENABLED)
                : TADSCMD_DISABLED);
        
    case ID_VIEW_TIMER_HHMM:
        return (prefs_->get_show_timer()
                ? (prefs_->get_show_timer_seconds()
                   ? TADSCMD_ENABLED : TADSCMD_CHECKED)
                : TADSCMD_DISABLED);

    case ID_SHOW_DEBUG_WIN:
        return TADSCMD_ENABLED;

    case ID_HELP_CONTENTS:
    case ID_HELP_ABOUT:
        return TADSCMD_ENABLED;

    case ID_HELP_ABOUT_GAME:
        return (aboutbox_->get_html_subwin() != 0
                && !main_panel_->get_eof_flag()
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_FILE_VIEW_SCRIPT:
        return TADSCMD_ENABLED;

    case ID_FILE_RECENT_NONE:
        /* this is just a placeholder, so it's always disabled */
        return TADSCMD_DISABLED;

    case ID_FILE_LOADGAME:
        /* 
         *   we can load a new game only in the normal interpreter, not in
         *   the debugger - the debugger has control over this when we're
         *   running under it 
         */
        return in_debugger_ || main_panel_->get_exit_pause()
            ? TADSCMD_DISABLED : TADSCMD_ENABLED;

    case ID_FILE_RECENT_1:
    case ID_FILE_RECENT_2:
    case ID_FILE_RECENT_3:
    case ID_FILE_RECENT_4:
        /* these are always enabled */
        return TADSCMD_ENABLED;

    case ID_EDIT_CUT:
    case ID_EDIT_COPY:
    case ID_EDIT_DELETE:
    case ID_EDIT_SELECTALL:
        /* send these commands to the currently focused subwindow */
        {
            CHtmlSysWin_win32 *win;

            if ((win = get_focus_subwin()) != 0)
                return win->check_command(command_id);
        }
        break;

    default:
        /* 
         *   If it's a profile selector, leave its state alone - we'll have
         *   set the state up properly when we initialized the menu, and it's
         *   complicated to figure out the correct state again, so it's best
         *   to just leave it as we found it. 
         */
        if (command_id >= ID_PROFILE_FIRST && command_id <= ID_PROFILE_LAST)
            return TADSCMD_DO_NOT_CHANGE;
        
        /* 
         *   it's not one of our commands - pass the command to the main
         *   panel or to the history panel (whichever is active) for
         *   processing 
         */
        if (cur_page_state_ != 0
            && cur_page_state_->next_ != 0
            && hist_panel_ != 0)
        {
            /* history is active - send it to the history panel */
            return hist_panel_->check_command(command_id);
        }
        else if (main_panel_ != 0)
        {
            /* send it to the main panel */
            return main_panel_->check_command(command_id);
        }
        break;
    }

    /* if we got here, we didn't handle it */
    return TADSCMD_UNKNOWN;
}

/*
 *   Process a command.  We'll send the command to our main command input
 *   panel for processing.  This allows banner panels to set up links, and
 *   have them easily processed by the main command window.  
 */
void CHtmlSys_mainwin::process_command(const textchar_t *cmd, size_t len,
                                       int append, int enter, int os_cmd_id)
{
    /* send the command to the main panel for processing */
    main_panel_->process_command(cmd, len, append, enter, os_cmd_id);
}

/*
 *   Realize my palette.  We'll call the subwindows to do the real work.  
 */
int CHtmlSys_mainwin::do_querynewpalette()
{
    /* let the main panel set its palette */
    return main_panel_->do_querynewpalette();
}

/*
 *   Load a new game given the new game's filename 
 */
void CHtmlSys_mainwin::load_new_game(const textchar_t *fname)
{
    char dir[OSFNMAX];
    
    /* save the new open file dialog path to the new game's directory */
    CTadsApp::get_app()->set_openfile_dir(fname);

    /* change the working directory to the game's directory */
    os_get_path_name(dir, sizeof(dir), fname);
    SetCurrentDirectory(dir);

    /* set the EOF flag, so that the current game terminates */
    main_panel_->set_eof_flag(TRUE);

    /* remember that we have a new game to load */
    set_pending_new_game(fname);

    /* add the file to the recent game list */
    add_recent_game(fname);
}

/*
 *   Load a new game, asking the player via a standard file dialog for the
 *   name of the new game 
 */
void CHtmlSys_mainwin::load_new_game()
{
    OPENFILENAME info;
    char fullname[MAX_PATH];
    char prompt[256];

    /* warn about ending any current game */
    if (!query_end_game())
        return;

    /* set up to get the filename */
    info.lStructSize = sizeof(info);
    info.hwndOwner = handle_;
    info.hInstance = CTadsApp::get_app()->get_instance();
    info.lpstrFilter = w32_opendlg_filter;
    info.lpstrCustomFilter = 0;
    info.nFilterIndex = 0;
    info.lpstrFile = fullname;
    fullname[0] = '\0';
    info.nMaxFile = sizeof(fullname);
    info.lpstrFileTitle = 0;
    info.nMaxFileTitle = 0;
    info.lpstrInitialDir = CTadsApp::get_app()->get_openfile_dir();
    LoadString(CTadsApp::get_app()->get_instance(),
               IDS_CHOOSE_NEW_GAME, prompt, sizeof(prompt));
    info.lpstrTitle = prompt;
    info.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    info.nFileOffset = 0;
    info.nFileExtension = 0;
    info.lpstrDefExt = 0;
    info.lCustData = 0;
    info.lpfnHook = 0;
    info.lpTemplateName = 0;
    CTadsDialog::set_filedlg_center_hook(&info);

    /* get the filename - if they cancel, simply return with nothing done */
    if (!GetOpenFileName(&info))
        return;

    /* load the new game */
    load_new_game(fullname);
}

/*
 *   If a current game is underway, warn the player and ask if they want
 *   to end it.  Returns true if we can proceed with loading a new game,
 *   false if not.  If no game is underway, we'll simply return true. 
 */
int CHtmlSys_mainwin::query_end_game()
{
    /* if a game is currently underway, warn about it */
    if (!main_panel_->get_eof_flag())
    {
        char msg[256];

        LoadString(CTadsApp::get_app()->get_instance(),
                   IDS_REALLY_NEW_GAME_MSG, msg, sizeof(msg));
        switch(MessageBox(handle_, msg, "TADS",
                          MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2))
        {
        case IDYES:
        case 0:
            /* it's okay; tell the caller to proceed */
            return TRUE;

        default:
            /* tell the caller to cancel */
            return FALSE;
        }
    }

    /* there's no game underway, so there's no reason not to proceed */
    return TRUE;
}

/*
 *   Update the window for a system palette change.  We'll call the
 *   subwindows to do the real work.  
 */
void CHtmlSys_mainwin::do_palettechanged(HWND initiating_window)
{
    int i;
    CHtmlSysWin_win32 **banner;
    
    /* tell the main panel to adjust for the new palette */
    main_panel_->do_palettechanged(initiating_window);

    /* tell each banner subwindow to adjust */
    for (i = 0, banner = banners_ ; i < banner_cnt_ ; ++i, ++banner)
        (*banner)->do_palettechanged(initiating_window);
}


/*
 *   Prune the parse tree that we're displaying if we're using too much
 *   memory. 
 */
void CHtmlSys_mainwin::maybe_prune_parse_tree()
{
    static int check_count = 0;
    unsigned long max_mem = (unsigned long)prefs_->get_mem_text_limit();
    HCURSOR old_cursor;

    /* 
     *   don't do any pruning if the setting is "no limit", which is
     *   indicated by a limit of zero 
     */
    if (max_mem == 0)
        return;

    /* 
     *   skip this entirely most of the time; only check every so often,
     *   so that we don't waste a lot of time doing this too frequently 
     */
    ++check_count;
    if (check_count < 10)
        return;

    /* 
     *   reset the counter to zero, so that we don't check again until the
     *   next interval has elapsed 
     */
    check_count = 0;

    /* never do any pruning unless we're showing the active page */
    if (!is_active_page())
        return;

    /* 
     *   check to see if we're consuming too much memory - if not, there's
     *   nothing we need to do here 
     */
    if (get_parse_tree_mem() <= max_mem)
        return;

    /* this may take a while - provide a status display */
    old_cursor = SetCursor(wait_cursor_);
    main_panel_->set_pruning_msg(TRUE);
    if (statusline_ != 0)
        statusline_->update();

    /*
     *   First, delete old pages until we run out of pages or we free up
     *   enough memory.  Never delete the last page, since that's the
     *   active page.  
     */
    while (get_parse_tree_mem() > max_mem
           && first_page_state_ != 0 && first_page_state_ != last_page_state_)
    {
        CHtmlSys_mainwin_state_link *cur;

        /* remember the oldest page */
        cur = first_page_state_;

        /* unlink it from the list */
        first_page_state_ = cur->next_;
        first_page_state_->prev_ = 0;
        if (first_page_state_->next_ != 0)
            first_page_state_->next_->prev_ = 0;

        /* delete the page */
        delete cur;
    }

    /* if we're still over budget, prune the active page */
    if (get_parse_tree_mem() > max_mem)
    {
        /* prune the active page's parse tree */
        parser_->prune_tree(max_mem / 2);
        
        /* 
         *   Reformat the active page, since we've changed its contents.  Do
         *   not reset sounds, since we're reformatting only to apply any
         *   downstream changes that might result from our pruning at the top
         *   of the page.  Don't show any status line message update, to keep
         *   the pruning as visually unintrusive as possible (the user
         *   probably won't notice the reformatting otherwise, so don't call
         *   attention to it with a status line update).  
         */
        reformat_all_html(FALSE, TRUE, FALSE);
    }

    /* remove status display */
    SetCursor(old_cursor);
    main_panel_->set_pruning_msg(FALSE);
    if (statusline_ != 0)
        statusline_->update();
}

/*
 *   Get the total memory used by parse tree text arrays 
 */
unsigned long CHtmlSys_mainwin::get_parse_tree_mem() const
{
    CHtmlSys_mainwin_state_link *pg;
    unsigned long sum;
    
    /* add up the memory used by old pages */
    for (sum = 0, pg = first_page_state_ ; pg != 0 ; pg = pg->next_)
    {
        /* only consider inactive pages - we'll get the active page later */
        if (pg->state_->get_text_array() != 0)
            sum += pg->state_->get_text_array()->get_mem_in_use();
    }

    /* add the memory used by the active page */
    sum += parser_->get_text_array()->get_mem_in_use();

    /* return the total */
    return sum;
}


/*
 *   Read a command line, with optional timeout.
 */
int CHtmlSys_mainwin::get_input_timeout(
    textchar_t *buf, size_t bufsiz, unsigned long timeout, int use_timeout)
{
    int stat;

    /* 
     *   if we're not resuming an interrupted session, initialize the new
     *   session 
     */
    if (!input_in_progress_)
    {
        /* flush output and make sure it's displayed */
        flush_txtbuf(TRUE, FALSE);

        /* do some pruning if memory is too full */
        maybe_prune_parse_tree();

        /* input is now in progress */
        input_in_progress_ = TRUE;
    }

    /* set a self-reference while working */
    AddRef();

    /* let the main panel do the work */
    stat = main_panel_->get_input_timeout(buf, bufsiz, timeout, use_timeout);

    /* if we successfully read a line, finish the input */
    if (stat == OS_EVT_LINE || stat == OS_EVT_EOF)
        get_input_done();

    /* we're done with our self-reference */
    Release();
    
    /* return the status */
    return stat;
}

/*
 *   Cancel input previously started with get_input_timeout() and
 *   subsequently interrupted by a timeout. 
 */
void CHtmlSys_mainwin::get_input_cancel(int reset)
{
    /* cancel input in the underlying main panel window */
    main_panel_->get_input_cancel(reset);

    /* close out our own command line processing if necessary */
    get_input_done();
}

/*
 *   Finish reading a command line.  This should be called whenever a
 *   command line input is terminated, either by the user pressing Return
 *   (or finishing command line entry some other way, such as clicking a
 *   hyperlink or selecting a command-entering menu item), or by
 *   programmatic cancellation of an timeout-interrupted command input. 
 */
void CHtmlSys_mainwin::get_input_done()
{
    /* 
     *   If input was in progress, and we still have a main panel, finish the
     *   input.  Note that the main panel could have been deleted if we
     *   closed the app window during the read operation; in this case, we
     *   don't need to finish up anything with the main panel, obviously.  
     */
    if (input_in_progress_ && main_panel_ != 0)
    {
        /* add the line-break after the command */
        if (parser_->get_obey_markups())
        {
            /* we're in parsed mode, so write our sequence as HTML */
            txtbuf_->append("<br>", 4);
        }
        else
        {
            /* we're in literal mode, so write out a literal newline */
            txtbuf_->append("\n", 1);
        }

        /* 
         *   Flush the newline, and update the window immediately, in case
         *   the operation takes a while to complete.  
         */
        flush_txtbuf(TRUE, TRUE);

        /* 
         *   tell the formatter to add an extra line's worth of spacing, to
         *   ensure that we have some immediate visual feedback (in the form
         *   of scrolling the window up a line) when the user presses the
         *   Enter key 
         */
        main_panel_->get_formatter()->add_line_to_disp_height();
    }

    /* input is no longer in progress */
    input_in_progress_ = FALSE;
}

/*
 *   Read an event.  Follows the semantics of the TADS os_get_event()
 *   interface.  
 */
int CHtmlSys_mainwin::get_input_event(unsigned long timeout, int use_timeout,
                                      os_event_info_t *info)
{
    int evt;
    
    /* flush output and make sure it's displayed */
    flush_txtbuf(TRUE, FALSE);

    /* let the main panel do the work */
    evt = main_panel_->get_input_event(timeout, use_timeout, info);

    /* 
     *   If get_input_event() returned EOF, and our main panel object has
     *   been forgotten, we must be quitting - send another quit message,
     *   to make sure the main event loop receives it (since the one that
     *   was already posted was probably eaten by the nested event loop
     *   that we just exited) 
     */
    if (evt == OS_EVT_EOF && main_panel_ == 0)
        PostQuitMessage(0);

    /* return the event code */
    return evt;
}

/*
 *   Check for a break key 
 */
int CHtmlSys_mainwin::check_break_key()
{
    /* ask the main panel to check for a break key */
    return main_panel_->check_break_key();
}

/*
 *   Set non-stop mode 
 */
void CHtmlSys_mainwin::set_nonstop_mode(int flag)
{
    /* flush any buffered text */
    flush_txtbuf(TRUE, FALSE);

    /* let the main panel know about it */
    main_panel_->set_nonstop_mode(flag);
}

/*
 *   Flush buffered output 
 */
void CHtmlSys_mainwin::flush_txtbuf(int fmt, int immediate_redraw)
{
    int i;
    CHtmlSysWin_win32 **banner;

    /* parse what's in the buffer */
    parser_->parse(txtbuf_, this);

    /* we're done with the text in the buffer - clear it out */
    txtbuf_->clear();

    /* 
     *   if desired, run the parsed source through the formatter and
     *   display it 
     */
    if (fmt)
    {
        /* parse and format the text */
        main_panel_->do_formatting(FALSE, FALSE, FALSE);
    }

    /* run through the banner API windows and flush them as well */
    for (i = 0, banner = banners_ ; i < banner_cnt_ ; ++i, ++banner)
    {
        /* tell the banner's formatter to flush its buffers */
        (*banner)->get_formatter()->flush_txtbuf(fmt);
    }

    /* if desired, immediately update the display */
    if (immediate_redraw)
        UpdateWindow(get_handle());
}

#if 0 // transcript viewing is not yet implemented
/*
 *   View a transcript 
 */
void CHtmlSys_mainwin::view_script()
{
    OPENFILENAME info;
    char fullname[MAX_PATH];

    /* set up to get the filename */
    info.lStructSize = sizeof(info);
    info.hwndOwner = handle_;
    info.hInstance = CTadsApp::get_app()->get_instance();
    info.lpstrFilter = "Text Files\0*.txt\0"
                       "Log Files\0*.log\0"
                       "All Files\0*.*\0\0";
    info.lpstrCustomFilter = 0;
    info.nFilterIndex = 0;
    info.lpstrFile = fullname;
    fullname[0] = '\0';
    info.nMaxFile = sizeof(fullname);
    info.lpstrFileTitle = 0;
    info.nMaxFileTitle = 0;
    info.lpstrInitialDir = CTadsApp::get_app()->get_openfile_dir();
    info.lpstrTitle = "Choose a transcript file to view";
    info.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    info.nFileOffset = 0;
    info.nFileExtension = 0;
    info.lpstrDefExt = 0;
    info.lCustData = 0;
    info.lpfnHook = 0;
    info.lpTemplateName = 0;
    CTadsDialog::set_filedlg_center_hook(&info);

    /* get the filename - if they cancel, simply return with nothing done */
    if (!GetOpenFileName(&info))
        return;

    /* start a new page */
    start_new_page();
}
#endif /* 0 */

/*
 *   Clear the screen and start a new page.  This affects only the main HTML
 *   panel - this doesn't affect any banner API windows.  
 */
void CHtmlSys_mainwin::start_new_page()
{
    /* make sure the current page is completely flushed */
    flush_txtbuf(TRUE, FALSE);

    /* halt animations on the outgoing page */
    main_panel_->get_formatter()->cancel_playback();
    
    /* 
     *   Export the page to our current save structure.  This not only
     *   saves the current page, but clears out the parser and resets it
     *   to a new page. 
     */
    parser_->export_parse_tree(get_cur_page_state());
    
    /* save the current scrolling position */
    cur_page_state_->scrollpos_.x = main_panel_->get_hscroll_pos();
    cur_page_state_->scrollpos_.y = main_panel_->get_vscroll_pos();

    /* add a new page to our page list */
    add_new_page_state();

    /* remove all banners */
    main_panel_->remove_all_banners();

    /* note that we're clearing the content */
    main_panel_->notify_clear_contents();

    /* reformat the window for the new blank page */
    reformat_all_html(FALSE, FALSE, TRUE);

    /* reset the selection range in all panels */
    clear_other_sel(0);
}

/*
 *   Clear all pages
 */
void CHtmlSys_mainwin::clear_all_pages()
{
    CHtmlSys_mainwin_state_link *cur;
    CHtmlSys_mainwin_state_link *nxt;
    
    /* delete any about box */
    aboutbox_->delete_html_subwin();

    /* start a new blank page */
    start_new_page();

    /* delete all orphaned banners */
    delete_orphaned_banners();

    /* delete all previous pages */
    for (cur = first_page_state_ ; cur != 0 && cur != cur_page_state_ ;
         cur = nxt)
    {
        /* remember the next page */
        nxt = cur->next_;

        /* delete this one */
        delete cur;

        /* advance the head pointer */
        first_page_state_ = nxt;

        /* the new head has no back pointer now */
        nxt->prev_ = 0;
    }
}

/*
 *   Delete all orphaned banner windows 
 */
void CHtmlSys_mainwin::delete_orphaned_banners()
{
    int i;
    
    /* 
     *   Note that we're in the process of deleting orphans.  This will let
     *   us avoid needlessly recalculating the screen layout repeatedly
     *   while we're removing a group of windows. 
     */
    deleting_orphaned_banners_ = TRUE;
    
    /* delete each orphaned banner */
    for (i = 0 ; i < orphaned_banner_cnt_ ; ++i)
        os_banner_delete(orphaned_banners_[i]);

    /* we have no more orphaned banners */
    orphaned_banner_cnt_ = 0;

    /* done with the orphan deletion */
    deleting_orphaned_banners_ = FALSE;

    /* recalculate the screen layout now that we're done */
    recalc_banner_layout();
}

/*
 *   Get the current page's state structure 
 */
CHtmlParserState *CHtmlSys_mainwin::get_cur_page_state()
{
    return cur_page_state_->state_;
}

/*
 *   Add a new page state structure, and switch to the new page 
 */
void CHtmlSys_mainwin::add_new_page_state()
{
    CHtmlSys_mainwin_state_link *link;
    CHtmlParserState *new_state;
    
    /* create a new state structure for the new page */
    new_state = new CHtmlParserState();

    /* add it to our list */
    link = new CHtmlSys_mainwin_state_link(new_state);
    link->prev_ = last_page_state_;
    if (last_page_state_)
        last_page_state_->next_ = link;
    else
        first_page_state_ = link;
    last_page_state_ = link;

    /* make it the current page */
    cur_page_state_ = link;
}

/*
 *   Check to see if there's a next/previous page in our list 
 */
int CHtmlSys_mainwin::exists_next_page()
{
    return cur_page_state_->next_ != 0;
}

int CHtmlSys_mainwin::exists_previous_page()
{
    return cur_page_state_->prev_ != 0;
}

/*
 *   Switch to the next/previous page 
 */
void CHtmlSys_mainwin::go_next_page()
{
    go_to_page(cur_page_state_->next_);
}

void CHtmlSys_mainwin::go_previous_page()
{
    go_to_page(cur_page_state_->prev_);
}

/*
 *   go to the active page 
 */
void CHtmlSys_mainwin::go_to_active_page()
{
    /* if we're already on the active page, there's nothing to do */
    if (is_active_page())
        return;

    /* switch to the active page - this is always the last page */
    go_to_page(last_page_state_);
}

/*
 *   Switch to a page 
 */
void CHtmlSys_mainwin::go_to_page(CHtmlSys_mainwin_state_link *new_page)
{
    /* 
     *   if there's no new page, or it's the same as the current page,
     *   there's nothing we need to do 
     */
    if (new_page == 0 || new_page == cur_page_state_)
        return;

    /* 
     *   if we're leaving the active page, hide the main panel; otherwise,
     *   we must export the parser state from the current history page so
     *   that we can reuse the parser for the new page state 
     */
    if (cur_page_state_->next_ == 0)
    {
        /* we're leaving the active page - hide the main panel */
        ShowWindow(main_panel_->get_handle(), SW_HIDE);

        /* show the history panel, as we're going to a history page */
        ShowWindow(hist_panel_->get_handle(), SW_SHOW);

        /* recalculate the banner layout for change */
        recalc_banner_layout();
    }
    else
    {
        /* we're leaving a history page - save the scrolling position */
        cur_page_state_->scrollpos_.x = hist_panel_->get_hscroll_pos();
        cur_page_state_->scrollpos_.y = hist_panel_->get_vscroll_pos();

        /* export the current page's parser state */
        hist_parser_->export_parse_tree(get_cur_page_state());
    }

    /* move to the new page */
    cur_page_state_ = new_page;

    /* check whether we're going to the active page or a history page */
    if (cur_page_state_->next_ == 0)
    {
        /* we're going back to the active page - show it */
        ShowWindow(main_panel_->get_handle(), SW_SHOW);

        /* hide the history window */
        ShowWindow(hist_panel_->get_handle(), SW_HIDE);

        /* 
         *   set focus on our window, then on the main panel - this ensures
         *   we'll go through a focus loss/gain cycle 
         */
        SetFocus(handle_);
        SetFocus(main_panel_->get_handle());

        /* recalculate the banner layout for change */
        recalc_banner_layout();

        /* show the cursor if appropriate */
        main_panel_->update_caret_pos(TRUE, TRUE);
    }
    else
    {
        /* import the new page into the parser */
        hist_parser_->import_parse_tree(get_cur_page_state());

        /* reset the "more" prompt location on the main history page */
        hist_panel_->reset_last_input_height();

        /* reformat the window for the new page */
        hist_panel_->do_reformat(TRUE, TRUE, TRUE);

        /* 
         *   scroll the main history panel to the scrolling position that was
         *   in effect when the page was last viewed 
         */
        hist_panel_->do_scroll(TRUE, hist_panel_->get_vscroll_handle(),
                               SB_THUMBPOSITION,
                               cur_page_state_->scrollpos_.y, TRUE);
        hist_panel_->do_scroll(FALSE, hist_panel_->get_hscroll_handle(),
                               SB_THUMBPOSITION,
                               cur_page_state_->scrollpos_.x, TRUE);

        /* set focus on the history panel */
        SetFocus(hist_panel_->get_handle());
    }
}

/*
 *   Clear the selection ranges in all subwindows; optionally excludes a
 *   given subwindow. 
 */
void CHtmlSys_mainwin::clear_other_sel(CHtmlSysWin_win32 *exclude_win)
{
    int i;
    CHtmlSysWin_win32 **banner;
    
    /* clear the selection in the main panel if it's not excluded */
    if (main_panel_ != 0 && main_panel_ != exclude_win)
        main_panel_->clear_sel_range();

    /* clear the selection in each banner subwindow */
    for (i = 0, banner = banners_ ; i < banner_cnt_ ; ++i, ++banner)
    {
        /* if this subwindow isn't excluded, clear its selection */
        if (*banner != exclude_win)
            (*banner)->clear_sel_range();
    }
}


/*
 *   Move focus to the main HTML panel 
 */
void CHtmlSys_mainwin::focus_to_main()
{
    CHtmlSysWin_win32 *win;
    unsigned long start, end;
            
    /* get the focus window */
    if ((win = get_focus_subwin()) != 0)
    {
        /* 
         *   If it doesn't have a non-empty selection, move focus back to
         *   the main window.  If it has a selection, keep focus here;
         *   with a selection, we could perform operations like Copy in
         *   this window, hence we want input focus so that we receive
         *   such commands.  
         */
        win->get_formatter()->get_sel_range(&start, &end);
        if (start == end)
        {
            HWND hwnd;

            /* 
             *   There's no selection here, so there's no point in keeping
             *   focus in this window.  Move focus back to the main window,
             *   or to the history window if we're showing history.  
             */
            if (cur_page_state_ != 0 && cur_page_state_->next_ != 0)
                hwnd = hist_panel_->get_handle();
            else
                hwnd = main_panel_->get_handle();

            /* set focus on the appropriate window */
            SetFocus(hwnd);
        }
    }
}

/*
 *   Get the subwindow that currently has the input focus.  Returns null
 *   if none of our subwindows has input focus.  
 */
CHtmlSysWin_win32 *CHtmlSys_mainwin::get_focus_subwin()
{
    int i;
    CHtmlSysWin_win32 **banner;
    HWND cur_focus;

    /* get the current focus window */
    cur_focus = GetFocus();

    /* if the main panel has input focus, return it */
    if (main_panel_ != 0
        && (main_panel_->get_handle() == cur_focus
            || IsChild(main_panel_->get_handle(), cur_focus)))
    {
        /* focus is in the main panel - return it */
        return main_panel_;
    }

    /* if the history panel has focus, return it */
    if (hist_panel_ != 0
        && (hist_panel_->get_handle() == cur_focus
            || IsChild(hist_panel_->get_handle(), cur_focus)))
    {
        /* this is the one */
        return hist_panel_;
    }

    /* find the banner window containing the focus */
    for (i = 0, banner = banners_ ; i < banner_cnt_ ; ++i, ++banner)
    {
        /* check to see if the focus is in this banner */
        if ((*banner)->get_handle() == cur_focus
            || IsChild((*banner)->get_handle(), cur_focus))
        {
            /* this banner window has focus - return it */
            return *banner;
        }
    }

    /* we couldn't find the focus anywhere in our subwindows */
    return 0;
}


/*
 *   Clear hover information from all subwindows; optionally excludes a
 *   given subwindow. 
 */
void CHtmlSys_mainwin::clear_other_hover_info(CHtmlSysWin_win32 *exclude_win)
{
    int i;
    CHtmlSysWin_win32 **banner;

    /* clear the selection in the main panel if it's not excluded */
    if (main_panel_ != 0 && main_panel_ != exclude_win)
        main_panel_->clear_hover_info();

    /* clear the selection in each banner subwindow */
    for (i = 0, banner = banners_ ; i < banner_cnt_ ; ++i, ++banner)
    {
        /* if this subwindow isn't excluded, clear its selection */
        if (*banner != exclude_win)
            (*banner)->clear_hover_info();
    }
}

/*
 *   Display output 
 */
void CHtmlSys_mainwin::display_output(const textchar_t *buf, size_t len)
{
    /* make sure we're showint the active page */
    go_to_active_page();

    /* add the text to the buffer we're accumulating */
    txtbuf_->append(buf, len);
}

/*
 *   display quoted output 
 */
void CHtmlSys_mainwin::display_output_quoted(const textchar_t *buf,
                                             size_t len)
{
    const textchar_t *start;
    const textchar_t *p;

    /* write an open quote */
    display_output("\"", 1);

    /* write the given text, quoting any double quote characters */
    for (start = p = buf ; ; ++p, --len)
    {
        /* 
         *   if we're out of text, or we've reached a double quote character,
         *   write the unquoted bit preceding this 
         */
        if (len == 0 || *p == '"')
        {
            /* write out the quote-free part up to here */
            if (p != start)
                display_output(start, p - start);

            /* if we're out of text, we're done */
            if (len == 0)
                break;

            /* write out a double-quote ampersand sequence */
            display_output("&#34", 4);

            /* the next chunk starts after the quote mark */
            start = p + 1;
        }
    }

    /* write the close quote */
    display_output("\"", 1);
}

/*
 *   Display plain output, even if we're in HTML mode 
 */
void CHtmlSys_mainwin::display_plain_output(const textchar_t *buf,
                                            int break_long_lines)
{
    int old_obey_markups;
    int old_obey_end;
    int old_obey_whitespace;
    int old_break;

    /* 
     *   flush any buffered text to the parser, so we interpret text up to
     *   this point in the current mode 
     */
    flush_txtbuf(FALSE, FALSE);

    /* remember the old parser mode */
    old_obey_markups = parser_->get_obey_markups();
    old_obey_end = parser_->get_obey_end_markups();
    old_obey_whitespace = parser_->get_obey_whitespace();
    old_break = parser_->get_break_long_lines();

    /* switch to literal mode */
    parser_->obey_markups(FALSE);
    parser_->obey_end_markups(FALSE);
    parser_->obey_whitespace(TRUE, break_long_lines);

    /* buffer the plain text */
    display_output(buf, strlen(buf));

    /* flush the buffered text, so we interpret it in plain mode */
    flush_txtbuf(FALSE, FALSE);

    /* switch back to the original mode */
    parser_->obey_markups(old_obey_markups);
    parser_->obey_end_markups(old_obey_end);
    parser_->obey_whitespace(old_obey_whitespace, old_break);
}

/*
 *   Close the owner window 
 */
int CHtmlSys_mainwin::close_owner_window(int force)
{
    /* 
     *   if forcing, destroy the window without asking the user anything;
     *   otherwise, send a normal close message so that the user has a
     *   chance to object 
     */
    if (force)
    {
        /* destroy our underlying handle */
        destroy_handle();

        /* success */
        return 0;
    }
    else
        return !SendMessage(handle_, WM_CLOSE, 0, 0);
}

/*
 *   Bring the window to the front 
 */
void CHtmlSys_mainwin::bring_owner_to_front()
{
    /* if the window is minimized, restore it */
    if (IsIconic(handle_))
        ShowWindow(handle_, SW_RESTORE);

    /* if it's hidden, show it */
    if (!IsWindowVisible(handle_))
        ShowWindow(handle_, SW_SHOW);

    /* make it the top window */
    BringWindowToTop(handle_);
}

/*
 *   Create a banner subwindow.  We'll create a peer of the original main
 *   panel, and add it to our panel list.  
 */
CHtmlSysWin *CHtmlSys_mainwin::
   create_banner_window(CHtmlSysWin *parent,
                        HTML_BannerWin_Type_t typ,
                        CHtmlFormatter *formatter, 
                        int where, class CHtmlSysWin *other,
                        HTML_BannerWin_Pos_t pos,
                        unsigned long style)
{
    CHtmlSysWin_win32 *subwin;
    RECT rc;
    
    /* don't allow MORE mode in text grids */
    if (typ == HTML_BANNERWIN_TEXTGRID)
        style &= ~OS_BANNER_STYLE_MOREMODE;

    /* MORE mode implies auto vscroll */
    if ((style & OS_BANNER_STYLE_MOREMODE) != 0)
        style |= OS_BANNER_STYLE_AUTO_VSCROLL;

    /* if the parent is null, it means that it's a child of the main window */
    if (parent == 0)
        parent = main_panel_;

    /* create a new HTML window */
    subwin = new CHtmlSysWin_win32(formatter, this,
                                   (style & OS_BANNER_STYLE_VSCROLL) != 0,
                                   (style & OS_BANNER_STYLE_HSCROLL) != 0,
                                   prefs_);

    /* 
     *   create the system window - give it an arbitrary initial height,
     *   since this will be changed by the formatter anyway, but give it
     *   the correct initial width 
     */
    GetClientRect(handle_, &rc);
    rc.right -= 2;
    rc.bottom = rc.top;
    subwin->create_system_window(this, TRUE, "", &rc);

    /* let the window know it's being used as a banner */
    subwin->set_is_banner_win(TRUE, (CHtmlSysWin_win32 *)parent,
                              where, (CHtmlSysWin_win32 *)other, typ, pos);

    /* set the auto-vscroll mode as desired by the caller */
    subwin->set_auto_vscroll((style & OS_BANNER_STYLE_AUTO_VSCROLL) != 0);

    /* set the border as desired by the caller */
    subwin->set_has_border((style & OS_BANNER_STYLE_BORDER) != 0);

    /* enable MORE mode if appropriate */
    if ((style & OS_BANNER_STYLE_MOREMODE) != 0)
        subwin->enable_more_mode(TRUE);

    /* add it to our panel list */
    add_banner_to_list(subwin);

    /* set the banner to use our status line */
    subwin->set_status_line(statusline_);

    /* return it */
    return subwin;
}

/*
 *   Create a subwindow for the "about" box.
 */
CHtmlSysWin *CHtmlSys_mainwin::
   create_aboutbox_window(CHtmlFormatter *formatter)
{
    /* create (or replace) the HTML subwindow in the dialog window */
    aboutbox_->create_html_subwin(formatter);
    
    /* return the subwindow */
    return aboutbox_->get_html_subwin();
}

/*
 *   remove a banner window 
 */
void CHtmlSys_mainwin::remove_banner_window(CHtmlSysWin *subwin)
{
    CHtmlSysWin_win32 *syswin = (CHtmlSysWin_win32 *)subwin;

    /* if this is the about box, handle it specially */
    if (syswin == aboutbox_->get_html_subwin())
    {
        aboutbox_->delete_html_subwin();
        return;
    }

    /* make sure the window isn't stuck waiting for MORE mode */
    syswin->release_more_mode(HTMLW32_MORE_CLOSE);
    
    /* remove the window from our panel list */
    remove_banner_from_list(syswin);

    /* close the window */
    syswin->close_banner_window();

    /* refigure all of our panel sizes */
    recalc_banner_layout();
}

/*
 *   orphan a banner 
 */
void CHtmlSys_mainwin::orphan_banner_window(CHtmlFormatterBannerExt *fmt)
{
    /* add the banner to our orphan list */
    if (orphaned_banner_cnt_ >= orphaned_banners_max_)
    {
        /* make room for more orphaned banners */
        orphaned_banners_max_ += 10;

        /* allocate or reallocate the array */
        if (orphaned_banners_ == 0)
            orphaned_banners_ =
                (CHtmlFormatterBannerExt **)th_malloc(
                    orphaned_banners_max_ * sizeof(orphaned_banners_[0]));
        else
            orphaned_banners_ =
                (CHtmlFormatterBannerExt **)th_realloc(
                    orphaned_banners_,
                    orphaned_banners_max_ * sizeof(orphaned_banners_[0]));
    }

    /* add the banner */
    orphaned_banners_[orphaned_banner_cnt_++] = fmt;
}

/*
 *   Receive notification that we've changed the size of a banner subwindow.
 *   If the window isn't a banner, we'll ignore this call; otherwise, we'll
 *   recalculate the layout so that all of the other windows adjust to the
 *   change.  
 */
void CHtmlSys_mainwin::on_set_banner_size(CHtmlSysWin_win32 *banner)
{
    CHtmlSysWin_win32 **cur;
    int i;

    /* find it in our list */
    for (cur = banners_, i = 0 ; i < banner_cnt_ ; ++i, ++cur)
    {
        /* see if this is the one we're looking for */
        if (*cur == banner)
        {
            /* found it - recalculate the banner layout */
            recalc_banner_layout();
            
            /* done */
            return;
        }
    }
}

/*
 *   Reformat all HTML windows 
 */
void CHtmlSys_mainwin::reformat_all_html(int show_status, int freeze_display,
                                         int reset_sounds)
{
    CHtmlSysWin_win32 **cur;
    int i;

    /* 
     *   recalculate the banner layout, in case any of the underlying units
     *   (such as the default font size) changed 
     */
    recalc_banner_layout();

    /* reformat the history panel when viewing history */
    if (cur_page_state_ != 0 && cur_page_state_->next_ != 0
        && hist_panel_ != 0)
        hist_panel_->do_reformat(show_status, freeze_display, reset_sounds);

    /* always reformat the main panel window */
    main_panel_->do_reformat(show_status, freeze_display, reset_sounds);

    /* reformat each banner window */
    for (i = 0, cur = banners_ ; i < banner_cnt_ ; ++i, ++cur)
        (*cur)->do_reformat(show_status, freeze_display, FALSE);
}

/* 
 *   Add a window to our list of windows waiting at a [MORE] prompt 
 */
void CHtmlSys_mainwin::add_more_prompt_win(CHtmlSysWin_win32 *win)
{
    /* add a reference to the window */
    win->AddRef();

    /* add it to our array */
    more_wins_.add_ele(win);
}

/*
 *   Remove a window from our list of windows waiting at a [MORE] prompt 
 */
void CHtmlSys_mainwin::remove_more_prompt_win(CHtmlSysWin_win32 *win)
{
    /* remove it from our array */
    if (more_wins_.remove_ele(win))
    {
        /* found it - remove our reference on the window */
        win->Release();
    }
}

/*
 *   Get the first window awaiting a response to a [MORE] prompt 
 */
int CHtmlSys_mainwin::owner_process_moremode_key(int vkey, TCHAR ch)
{
    size_t i;
    size_t cnt;

    /* 
     *   run through the more-mode windows, and ask each if it wants to
     *   handle the event 
     */
    for (i = 0, cnt = more_wins_.get_count() ; i < cnt ; ++i)
    {
        CHtmlSysWin_win32 *win;
        int ret;

        /* get this window from the MORE-mode list */
        win = (CHtmlSysWin_win32 *)more_wins_.get_ele(i);

        /* 
         *   Check to see if this window wants to handle it.  Add a reference
         *   on the window while we're working, since releasing more mode can
         *   in some cases remove the last reference on the window, which can
         *   destroy it. 
         */
        win->AddRef();
        ret = win->process_moremode_key(vkey, ch);
        win->Release();

        /* if the window handled it, tell the caller we found a handler */
        if (ret)
            return TRUE;
    }

    /* we didn't find anyone interested */
    return FALSE;
}

/* iterator callback object for releasing MORE mode */
class CReleaseMoreModeCB: public CIterCallback
{
public:
    virtual void invoke(void *arg)
    {
        /* release MORE mode in this window */
        ((CHtmlSysWin_win32 *)arg)->release_more_mode(HTMLW32_MORE_ALL);
    }
};

/*
 *   Release MORE mode in all windows 
 */
void CHtmlSys_mainwin::release_all_moremode()
{
    CReleaseMoreModeCB cb;

    /* iterate over all elements, releasing MORE mode in each */
    more_wins_.iterate(&cb);
}

/* ------------------------------------------------------------------------ */
/*
 *   Hash table entry for an executable-embedded HTML resource. 
 */
class CHtmlHashEntryExeRes: public CHtmlHashEntryCI
{
public:
    CHtmlHashEntryExeRes(const textchar_t *name, size_t name_len,
                         unsigned long seek_ofs, unsigned long siz)
        : CHtmlHashEntryCI(name, name_len, TRUE)
    {
        /* remember the resource file location and size */
        seek_ofs_ = seek_ofs;
        siz_ = siz;
    }

    /* seek offset in .EXE file of the resource data */
    unsigned long seek_ofs_;

    /* size in bytes of the resource data */
    unsigned long siz_;
};

/*
 *   Load the executable-embedded HTML resources.  This loads the private
 *   resources that we use for our own purposes, such as in our "About" box
 *   and on the Game Chest page.  
 */
void CHtmlSys_mainwin::load_exe_resources(const char *exefile)
{
    osfildef *fp;
    char buf[256];
    unsigned int cnt;
    unsigned int i;
    unsigned long mres_ofs;
    
    /* 
     *   We keep our embedded HTML resources in an .exe file chunk tagged
     *   with the identifier "EXRS", using our private executable-embedding
     *   system.  Look for the EXRS chunk.  If we don't find an EXRS chunk,
     *   there are no embedded HTML resources, so there's nothing for us to
     *   do here.  
     */
    if ((fp = os_exeseek(exefile, "EXRS")) == 0)
        return;

    /* 
     *   Our embedded resource file must be a .t3r (T3 resource format) file
     *   containing a single MRES block.  Load the T3 image file header and
     *   verify that it has a valid signature.
     */
    if (osfrb(fp, buf, 69) || memcmp(buf, "T3-image\015\012\032", 11) != 0)
        goto done;

    /* load the first block header and verify that it's an MRES block */
    if (osfrb(fp, buf, 10) || memcmp(buf, "MRES", 4) != 0)
        goto done;

    /* 
     *   it looks like we have some resources, so remember the .exe file name
     *   - we'll need this whenever we load a resource from the file 
     */
    exe_fname_.set(exefile);

    /* create a hash table to store the table of contents for the resources */
    exe_res_table_ = new CHtmlHashTable(64, new CHtmlHashFuncCI());

    /* 
     *   Remember the file seek offset of the start of the MRES block (we're
     *   positioned at the start of the MRES data now, so this is simply the
     *   current seek location).  The entries in the MRES table of contents
     *   are given with seek positions relative to this base offset. 
     */
    mres_ofs = osfpos(fp);

    /* read and decode the number of entries in the MRES block */
    if (osfrb(fp, buf, 2))
        goto done;
    cnt = osrp2(buf);

    /* read the table of contents and populate the resource hash table */
    for (i = 0 ; i < cnt ; ++i)
    {
        unsigned long res_ofs;
        unsigned long res_size;
        unsigned char name_len;
        unsigned int j;
        char *p;
        
        /* read the fixed part of the entry */
        if (osfrb(fp, buf, 9))
            goto done;

        /* decode the resource description */
        res_ofs = mres_ofs + osrp4(buf);
        res_size = osrp4(buf + 4);
        name_len = buf[8];

        /* read the name */
        if (osfrb(fp, buf, name_len))
            goto done;

        /* de-obscure the resource name (by XOR'ing each byte with 0xFF) */
        for (p = buf, j = name_len ; j != 0 ; ++p, --j)
            *p ^= 0xFF;

        /* add this resource description to the resource hash table */
        exe_res_table_->add(new CHtmlHashEntryExeRes(
            buf, name_len, res_ofs, res_size));
    }

done:
    /* done - close the file */
    osfcls(fp);
}

/*
 *   Look up a resource embedded in the executable 
 */
int CHtmlSys_mainwin::get_exe_resource(
    const textchar_t *resname, size_t resnamelen,
    textchar_t *fname_buf, size_t fname_buf_len,
    unsigned long *seek_pos, unsigned long *siz)
{
    CHtmlHashEntryExeRes *entry;
    
    /* if the name doesn't start with "exe:", it's not one of ours */
    if (resnamelen <= 4 || memicmp(resname, "exe:", 4) != 0)
        return FALSE;

    /* skip the "exe:" prefix in the name */
    resname += 4;
    resnamelen -= 4;

    /* 
     *   if there's no embedded resource hash table, we're obviously not
     *   going to find the resource 
     */
    if (exe_res_table_ == 0)
        return FALSE;

    /* look up the entry in the hash table */
    entry = (CHtmlHashEntryExeRes *)exe_res_table_->find(resname, resnamelen);
    if (entry != 0)
    {
        /* got it - all executable resources are in the .EXE file */
        strcpy(fname_buf, exe_fname_.get());

        /* indicate the resource's location in the .EXE file */
        *seek_pos = entry->seek_ofs_;
        *siz = entry->siz_;

        /* indicate that we found the resource */
        return TRUE;
    }
    else
    {
        /* no such entry */
        return FALSE;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Set the accelerator key for the Edit/Paste menu item 
 */
void CHtmlSys_mainwin::set_paste_accel(textchar_t paste_key)
{
    HMENU edit_menu;
    textchar_t buf[128];
    textchar_t *p;
    
    /* get the old menu label */
    edit_menu = GetSubMenu(GetMenu(handle_), 1);
    GetMenuString(edit_menu, ID_EDIT_PASTE, buf, sizeof(buf), MF_BYCOMMAND);

    /* find the tab */
    for (p = buf ; *p != '\0' && *p != '\t' ; ++p) ;
    if (*p == '\t')
    {
        /* find the '+' */
        for ( ; *p != '\0' && *p != '+' ; ++p);

        /* if we found the '+', set the next character */
        if (*p == '+')
        {
            /* set the character */
            *++p = paste_key;

            /* update the menu label */
            ModifyMenu(edit_menu, ID_EDIT_PASTE,
                       MF_BYCOMMAND | MF_STRING, ID_EDIT_PASTE, buf);
        }
    }
}

/* 
 *   invalidate on-screen link displays in all windows
 */
void CHtmlSys_mainwin::owner_inval_links_on_screen()
{
    CHtmlSysWin_win32 **cur;
    int i;

    /* invalidate links in all child windows */
    main_panel_->inval_links_on_screen();
    for (i = 0, cur = banners_ ; i < banner_cnt_ ; ++i, ++cur)
        (*cur)->inval_links_on_screen();
}

/*
 *   Add a window to our list of banner windows 
 */
void CHtmlSys_mainwin::add_banner_to_list(CHtmlSysWin_win32 *win)
{
    /* reallocate our list if it's not big enough */
    if (banners_ == 0)
    {
        banners_max_ = 5;
        banners_ = (CHtmlSysWin_win32 **)
                   th_malloc(banners_max_ * sizeof(*banners_));
    }
    else if (banner_cnt_ == banners_max_)
    {
        banners_max_ += 5;
        banners_ = (CHtmlSysWin_win32 **)
                   th_realloc(banners_, banners_max_ * sizeof(*banners_));
    }

    /* insert the new banner at the end of the list */
    banners_[banner_cnt_++] = win;

    /* add a reference on the window as long as we have it in our list */
    win->AddRef();
}

/*
 *   Remove a window from our banner list 
 */
void CHtmlSys_mainwin::remove_banner_from_list(CHtmlSysWin_win32 *win)
{
    CHtmlSysWin_win32 **cur;
    int i;
    
    /* find it in our list */
    for (cur = banners_, i = 0 ; i < banner_cnt_ ; ++i, ++cur)
    {
        /* see if this is the one we're looking for */
        if (*cur == win)
        {
            /* move the remaining ones down a notch */
            for ( ; i + 1 < banner_cnt_ ; ++i, ++cur)
                *cur = *(cur + 1);

            /* that's one less banner */
            --banner_cnt_;

            /* remove our reference on this banner window */
            win->Release();

            /* no need to look further */
            return;
        }
    }
}

/*
 *   Pause before exiting 
 */
void CHtmlSys_mainwin::pause_for_exit()
{
    MSG msg;
    textchar_t ch;

    /* if the exit pause is disabled, do nothing */
    if (!exit_pause_enabled_)
        return;

    /* turn on the exit flag in the main window */
    main_panel_->set_exit_pause(TRUE);

    /* clear the EOF flag, so that we do wait for a key */
    main_panel_->set_eof_flag(FALSE);

    /* flush output */
    flush_txtbuf(TRUE, FALSE);

    /* keep a self-reference while we're working */
    AddRef();

    /* clear out any keystrokes currently in the message queue */
    while (PeekMessage(&msg, 0, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
        ;

    /* handle events until we get a keystroke */
    main_panel_->wait_for_keystroke(&ch, TRUE, 0, FALSE);

    /* 
     *   turn off the exit flag (note that the main panel could have been
     *   destroyed while we were waiting for a keystroke, although we
     *   could not ourselves be destroyed because we have a self reference
     *   active for now) 
     */
    if (main_panel_ != 0)
        main_panel_->set_exit_pause(FALSE);

    /* we're done with our self-reference */
    Release();
}

/*
 *   Pause for exit after successful termination of the game.  If the
 *   preferences are set so that we keep running after termination, this
 *   will do nothing.
 */
void CHtmlSys_mainwin::pause_after_game_quit()
{
    /* 
     *   if we're not in postquit "exit" mode, don't do anything here,
     *   since the exit keystroke pause is not used in other modes 
     */
    if (prefs_->get_postquit_action() != HTML_PREF_POSTQUIT_EXIT)
        return;

    /* pause for exit */
    pause_for_exit();
}


/*
 *   Display the MORE prompt 
 */
void CHtmlSys_mainwin::pause_for_more()
{
    /* flush output */
    flush_txtbuf(TRUE, FALSE);
    
    /* display the MORE prompt in the main window */
    main_panel_->do_game_more_prompt();
}

/*
 *   Receive notification that the TADS engine is loading a new game.
 *   We'll set the main window title, and add the game to the list of
 *   recently-loaded games.  
 */
void CHtmlSys_mainwin::notify_load_game(const char *fname)
{
    char fullname[OSFNMAX];
    char *root_name;
    char exe_name[OSFNMAX];

    /* save the game chest file, if necessary */
    save_game_chest_file();

    /* get the name of the executable file, in case we need it */
    GetModuleFileName(CTadsApp::get_app()->get_instance(),
                      exe_name, sizeof(exe_name));

    /* if there's a file, make sure we have a full path name */
    if (fname != 0 && fname[0] != '\0')
    {
        /*
         *   Check to see if the filename is the same as the executable
         *   filename.  If it is, we're a stand-alone executable, which
         *   changes a little of our user interface.  Just make a note of
         *   this for now, so we can make the proper adjustments at the
         *   proper times.  
         */
        standalone_exe_ = (stricmp(fname, exe_name) == 0);

        /* get the full path */
        GetFullPathName(fname, sizeof(fullname), fullname, &root_name);

        /* set the open-file path to the game's path */
        CTadsApp::get_app()->set_openfile_dir(fullname);
    }

    /* 
     *   Set the window title.  Add our application title as well, to
     *   conform to the usual Windows standard of showing the name of the
     *   document as well as the application.  If there's no filename,
     *   just set our application title with no document name.  
     */
    if (fname == 0 || fname[0] == '\0')
    {
        /* they don't have a title to set - set it to our app name */
        main_panel_->set_window_title(w32_titlebar_name);
    }
    else
    {
        char buf[OSFNMAX + 40];

        /* add the app name after the requested title */
        sprintf(buf, "%s - %s", root_name, w32_titlebar_name);
        main_panel_->set_window_title(buf);
    }

    /* 
     *   If we have a filename, add it to the list of recent games.  Note
     *   that we don't add stand-alone executables to the recent games
     *   menu, since we don't even show a recent-games menu in this case,
     *   and furthermore we wouldn't want to add an executable to the
     *   recent games menu for the separate interpreter.  
     */
    if (fname != 0 && fname[0] != '\0' && !standalone_exe_)
        add_recent_game(fullname);

    /* 
     *   if this is a stand-alone executable, make sure the 'file' menu is
     *   properly adjusted 
     */
    if (standalone_exe_)
        set_recent_game_menu();

    /* make sure we've loaded the correct preferences profile for the game */
    if (fname != 0 && fname[0] != '\0')
    {
        /* load the game's preferred profile */
        load_game_specific_profile(fullname);

#if 1
        /* reformat with the new settings */
        main_panel_->do_reformat(FALSE, TRUE, TRUE);
#else
        // Old way - reformat later; there's really no need to defer the
        // reformat, though, and it's better to do it immediately.  With
        // the immediate reformat, we won't have to do an unnecessary
        // redraw, and we'll have the correct fonts set up on the first
        // pass.  Having the correct fonts initially is important because
        // it will calculate the MORE mode data correctly the first time
        // through, avoiding any spurious flashing of a MORE prompt.

        /* 
         *   set the background color in the main panel, so that when we
         *   clear the main window in a moment, we'll clear it with the new
         *   profile's background color 
         */
        main_panel_->set_html_bg_color(0, TRUE);

        /* reformat with the new settings */
        main_panel_->schedule_reformat(FALSE, FALSE, FALSE);
#endif
    }
}

/*
 *   Set the new active profile, saving it as the game-specific profile for
 *   the currently active game. 
 */
void CHtmlSys_mainwin::set_game_specific_profile(const char *profile)
{
    /* if we have a current game, associate it with the profile */
    if (profile_game_.get() != 0 && profile_game_.get()[0] != '\0')
        set_profile_assoc(profile_game_.get(), profile);
    
    /* select the new active profile */
    prefs_->set_active_profile_name(profile);

    /* load the new profile */
    prefs_->restore(FALSE);

    /* update the display */
    reformat_all_html(TRUE, TRUE, FALSE);
}

/*
 *   Get the game-specific profile for the given game file.
 *   
 *   If we've remembered a specific profile setting for this game, we'll load
 *   the saved profile.  If not, we'll look for a GameInfo record, and check
 *   to see if it has a PresentationProfile setting; if so, use the profile
 *   it specifies.  If we can't find any of these, use the "Default" profile
 *   by default.
 *   
 *   We save the per-game profile in the registry under the preferences
 *   subkey "Game Profiles", with a value under the key: the value name is
 *   the filename, and the value's value is a string giving the profile name.
 *   
 *   If 'must_exist' is true, then we'll make sure the profile exists; if we
 *   find a profile association that's not valid, we'll return the "Default"
 *   profile.  If 'must_exist' is false, we'll return the profile association
 *   we find, even if it refers to a non-existent profile.  
 */
void CHtmlSys_mainwin::get_game_specific_profile(
    char *profile_buf, size_t profile_buf_len, const char *filename,
    int must_exist)
{
    int found_profile;
    int ok;
    char exe_name[OSFNMAX];

    /* 
     *   If we can find a saved association for the game, use that - a stored
     *   association takes precedence over anything else.  If we can't find a
     *   saved association, we'll look for GameInfo metadata with a default
     *   profile setting.  
     */
    if (get_profile_assoc(filename, profile_buf, profile_buf_len))
        return;

    /* use the "Default" profile by default */
    found_profile = FALSE;
    strcpy(profile_buf, "Default");

    /* get the name of the executable file, in case we need it */
    GetModuleFileName(CTadsApp::get_app()->get_instance(),
                      exe_name, sizeof(exe_name));

    /* set up a GameInfo reader */
    CTadsGameInfoLocal inf(exe_name);

    /* presume we won't find GameInfo metadata */
    ok = FALSE;

    /* 
     *   If this is a stand-alone executable, we need to look inside the file
     *   at the start of the embedded .gam/.t3 data; otherwise, we can just
     *   load the file directly.  
     */
    if (standalone_exe_)
    {
        osfildef *fp;

        /* find the embedded game data */
        if ((fp = os_exeseek(filename, "TGAM")) != 0)
        {
            /* read the GameInfo metadata from the game file */
            ok = inf.read_from_fp(fp);

            /* done with the game file */
            osfcls(fp);
        }
    }
    else
    {
        /* it's just a plain .gam/.t3 file - read it directly */
        ok = inf.read_from_file(filename);
    }

    /* 
     *   if we loaded the GameInfo metadata successfully, look for a
     *   PresentationProfile value in the metadata 
     */
    if (ok)
    {
        const char *val;

        /* look up the PresentationProfile value */
        val = inf.get_val("PresentationProfile");

        /* if we found it, use it */
        if (val != 0)
        {
            found_profile = TRUE;
            strncpy(profile_buf, val, profile_buf_len - 1);
            profile_buf[profile_buf_len - 1] = '\0';
        }
    }

    /* 
     *   if the caller requires a valid profile, and the selected profile
     *   doesn't exist, revert to the default 
     */
    if (must_exist
        && found_profile
        && !CHtmlPreferences::profile_exists(profile_buf))
        strcpy(profile_buf, "Default");
}

/*
 *   Load the game-specific preferences for the given game.  We'll look up
 *   the profile for the given game file, and load it.  
 */
void CHtmlSys_mainwin::load_game_specific_profile(const char *fullname)
{
    char profile[128];
    const textchar_t *old_profile;

    /* 
     *   Get the profile for this filename.  The profile must exist; if it
     *   doesn't, we'll want to use the default profile.  
     */
    get_game_specific_profile(profile, sizeof(profile), fullname, TRUE);

    /* check for a profile change */
    old_profile = prefs_->get_active_profile_name();
    if (stricmp(profile, old_profile) != 0)
    {
        /* save the outgoing profile */
        prefs_->save();

        /* select and load the new profile */
        prefs_->set_active_profile_name(profile);
        prefs_->restore(FALSE);
    }

    /* remember this as the game associated with the current profile */
    profile_game_.set(fullname);
}

/*
 *   Get the game-specific profile for the given game file 
 */
int CHtmlSys_mainwin::get_profile_assoc(
    const char *fname, char *profile_buf, size_t profile_buf_len)
{
    HKEY hkey;
    DWORD disp;
    char key_name[256];
    int found;

    /* build the Game Profiles key name */
    sprintf(key_name, "%s\\Game Profiles", w32_pref_key_name);

    /* look up the key */
    hkey = CTadsRegistry::open_key(HKEY_CURRENT_USER, key_name, &disp, FALSE);

    /* if there's no such key, there's nothing more to do */
    if (hkey == 0)
        return FALSE;

    /* look up the value for the given game file */
    found = CTadsRegistry::value_exists(hkey, fname);
    CTadsRegistry::query_key_str(hkey, fname, profile_buf, profile_buf_len);

    /* done with the key */
    CTadsRegistry::close_key(hkey);

    /* return an indication of whether the setting was found or not */
    return found;
}

/*
 *   Set the game-specific profile for the given game file 
 */
void CHtmlSys_mainwin::set_profile_assoc(
    const char *fname, const char *profile)
{
    HKEY hkey;
    DWORD disp;
    char key_name[256];

    /* build the Game Profiles key name */
    sprintf(key_name, "%s\\Game Profiles", w32_pref_key_name);

    /* look up the key */
    hkey = CTadsRegistry::open_key(HKEY_CURRENT_USER, key_name, &disp, TRUE);

    /* if there's no such key, there's nothing more to do */
    if (hkey == 0)
        return;

    /* store the key */
    CTadsRegistry::set_key_str(hkey, fname, profile, strlen(profile));

    /* done with the key */
    CTadsRegistry::close_key(hkey);
}


/*
 *   Rename all game references to a preferences profile 
 */
void CHtmlSys_mainwin::rename_profile_refs(const char *old_name,
                                           const char *new_name)
{
    HKEY hkey;
    DWORD disp;
    char key_name[256];
    DWORD idx;

    /* build the Game Profiles key name */
    sprintf(key_name, "%s\\Game Profiles", w32_pref_key_name);

    /* look up the key */
    hkey = CTadsRegistry::open_key(HKEY_CURRENT_USER, key_name, &disp, FALSE);

    /* if there's no such key, there's nothing more to do */
    if (hkey == 0)
        return;

    /* 
     *   run through the game references, and change each one that points to
     *   the old profile name so that it points to the new profile name
     *   instead 
     */
    for (idx = 0 ; ; ++idx)
    {
        char nm[OSFNMAX];
        DWORD nmlen = sizeof(nm);
        DWORD typ;
        BYTE val[OSFNMAX];
        DWORD vallen = sizeof(val);
        
        /* get the next value */
        if (RegEnumValue(hkey, idx, nm, &nmlen, 0, &typ, val, &vallen)
            != ERROR_SUCCESS)
            break;

        /* if the value matches the old profile name, rename it */
        if (typ == REG_SZ && stricmp((char *)val, old_name) == 0)
            CTadsRegistry::set_key_str(hkey, nm, new_name, strlen(new_name));
    }

    /* done with the key */
    CTadsRegistry::close_key(hkey);
}

/*
 *   Display a debug message.  If 'foreground' is true, we'll bring the
 *   window to the foreground; otherwise, we'll leave the window in the
 *   background (but we'll make sure it's visible).  
 */
void CHtmlSys_mainwin::dbg_print(const char *msg, int foreground)
{
    /* if we have a debug window, display the message there */
    if (dbgwin_ != 0)
    {
        /*
         *   Send our private DBG_PRINT message to the debug window.  Use
         *   a message rather than calling the debug window's disp_text()
         *   method directly -- this allows us to print debug messages
         *   from background threads without fear of any concurrency
         *   problems.  (A sound playback thread, for example, may want to
         *   display a message if it encounters an error decoding its
         *   sound file.)  
         */
        SendMessage(dbgwin_->get_handle(), HTMLM_DBG_PRINT,
                    (WPARAM)foreground, (LPARAM)msg);
    }
}

/*
 *   printf to debug window 
 */
void CHtmlSys_mainwin::dbg_printf(int foreground, const char *msg, ...)
{
    va_list va;
    char buf[4096];

    /* format the message into our buffer */
    va_start(va, msg);
    _vsnprintf(buf, sizeof(buf), msg, va);
    va_end(va);

    /* display the message */
    dbg_print(buf, foreground);
}

/*
 *   printf to debug window with a specific font 
 */
void CHtmlSys_mainwin::dbg_printf(const CHtmlFontDesc *font_desc,
                                  int foreground, const char *msg, ...)
{
    va_list va;
    char buf[4096];

    /* format the message into our buffer */
    va_start(va, msg);
    _vsnprintf(buf, sizeof(buf), msg, va);
    va_end(va);

    /* send the message to the debug window */
    if (dbgwin_ != 0)
    {
        CHtmlDbgPrintFontMsg info(font_desc, foreground, buf);
        
        /* send the message */
        SendMessage(dbgwin_->get_handle(), HTMLM_DBG_PRINT_FONT,
                    0, (LPARAM)&info);
    }
}

/*
 *   Get a keystroke 
 */
textchar_t CHtmlSys_mainwin::wait_for_keystroke(int pause_only)
{
    textchar_t ch;
    
    /* flush output */
    flush_txtbuf(TRUE, FALSE);

    /* get a keystroke */
    main_panel_->wait_for_keystroke(&ch, pause_only, 0, FALSE);
    return ch;
}

/*
 *   Get a keystroke with a timeout 
 */
int CHtmlSys_mainwin::wait_for_keystroke_timeout(
    textchar_t *ch, int pause_only, time_t timeout)
{
    /* flush output */
    flush_txtbuf(TRUE, FALSE);

    /* get the keystroke */
    return main_panel_->wait_for_keystroke(ch, pause_only, timeout, TRUE);
}

/*
 *   Map an HTML 4 code point value 
 */
size_t CHtmlSys_mainwin::xlat_html4_entity(textchar_t *result,
                                           size_t result_size,
                                           unsigned int charval,
                                           oshtml_charset_id_t *charset,
                                           int *changed_charset)
{
    oshtml_charset_id_t new_charset;
    size_t len;
    
    /* presume we're going to return a single byte */
    result[1] = '\0';
    len = 1;

    /* check the character to see what range it's in */
    if (charval < 128)
    {
        /* 
         *   it's in the ASCII range, so assume that it's part of the
         *   ASCII subset of every Windows character set -- return the
         *   value unchanged and in the current default character set 
         */
        result[0] = (textchar_t)charval;
        new_charset = default_charset_;
    }
    else if (charval >= 160 && charval <= 255)
    {
        /*
         *   It's in the 160-255 range, which is the ISO Latin-1 range for
         *   HTML 4.  These map trivially to Windows code page 1252
         *   characters, so leave the value unchanged and use the ANSI
         *   character set.  
         */
        result[0] = (textchar_t)charval;
        new_charset = oshtml_charset_id_t(ANSI_CHARSET, 1252);
        len = 1;
    }
    else
    {
        const int ANY_125X_CHARSET = -1;
        static struct
        {
            unsigned int html_val;
            unsigned int win_val;
            int          win_charset;
            int          codepage;
        } mapping[] =
        {
            /*
             *   IMPORTANT: This table must be sorted by html_val, because
             *   we perform a binary search on this key.  
             */

            /* 
             *   Latin-2 characters and additional alphabetic characters.
             *   Most of these characters exist only in the Windows
             *   Eastern European code page, but a few are in the common
             *   Windows extended code page set (code points 128-159) and
             *   a few are only in the ANSI code page.  
             */
            { 258, 0xC3, EASTEUROPE_CHARSET, 1250 },              /* Abreve */
            { 259, 0xE3, EASTEUROPE_CHARSET, 1250 },              /* abreve */
            { 260, 0xA5, EASTEUROPE_CHARSET, 1250 },               /* Aogon */
            { 261, 0xB9, EASTEUROPE_CHARSET, 1250 },               /* aogon */
            { 262, 0xC6, EASTEUROPE_CHARSET, 1250 },              /* Cacute */
            { 263, 0xE6, EASTEUROPE_CHARSET, 1250 },              /* cacute */
            { 268, 0xC8, EASTEUROPE_CHARSET, 1250 },              /* Ccaron */
            { 269, 0xE8, EASTEUROPE_CHARSET, 1250 },              /* ccaron */
            { 270, 0xCF, EASTEUROPE_CHARSET, 1250 },              /* Dcaron */
            { 271, 0xEF, EASTEUROPE_CHARSET, 1250 },              /* dcaron */
            { 272, 0xD0, EASTEUROPE_CHARSET, 1250 },              /* Dstrok */
            { 273, 0xF0, EASTEUROPE_CHARSET, 1250 },              /* dstrok */
            { 280, 0xCA, EASTEUROPE_CHARSET, 1250 },               /* Eogon */
            { 281, 0xEA, EASTEUROPE_CHARSET, 1250 },               /* eogon */
            { 282, 0xCC, EASTEUROPE_CHARSET, 1250 },              /* Ecaron */
            { 283, 0xEC, EASTEUROPE_CHARSET, 1250 },              /* ecaron */
            { 313, 0xC5, EASTEUROPE_CHARSET, 1250 },              /* Lacute */
            { 314, 0xE5, EASTEUROPE_CHARSET, 1250 },              /* lacute */
            { 317, 0xBC, EASTEUROPE_CHARSET, 1250 },              /* Lcaron */
            { 318, 0xBE, EASTEUROPE_CHARSET, 1250 },              /* lcaron */
            { 321, 0xA3, EASTEUROPE_CHARSET, 1250 },              /* Lstrok */
            { 322, 0xB3, EASTEUROPE_CHARSET, 1250 },              /* lstrok */
            { 323, 0xD1, EASTEUROPE_CHARSET, 1250 },              /* Nacute */
            { 324, 0xF1, EASTEUROPE_CHARSET, 1250 },              /* nacute */
            { 327, 0xD2, EASTEUROPE_CHARSET, 1250 },              /* Ncaron */
            { 328, 0xF2, EASTEUROPE_CHARSET, 1250 },              /* ncaron */
            { 336, 0xD5, EASTEUROPE_CHARSET, 1250 },              /* Odblac */
            { 337, 0xF5, EASTEUROPE_CHARSET, 1250 },              /* odblac */
            { 338,  140, ANSI_CHARSET, 1252 },                     /* OElig */
            { 339,  156, ANSI_CHARSET, 1252 },                     /* oelig */
            { 340, 0xC0, EASTEUROPE_CHARSET, 1250 },              /* Racute */
            { 341, 0xE0, EASTEUROPE_CHARSET, 1250 },              /* racute */
            { 344, 0xD8, EASTEUROPE_CHARSET, 1250 },              /* Rcaron */
            { 345, 0xF8, EASTEUROPE_CHARSET, 1250 },              /* rcaron */
            { 346, 0x8C, EASTEUROPE_CHARSET, 1250 },              /* Sacute */
            { 347, 0x9C, EASTEUROPE_CHARSET, 1250 },              /* sacute */
            { 350, 0xAA, EASTEUROPE_CHARSET, 1250 },              /* Scedil */
            { 351, 0xBA, EASTEUROPE_CHARSET, 1250 },              /* scedil */
            { 352,  138, ANSI_CHARSET, 1252 },                    /* Scaron */
            { 353,  154, ANSI_CHARSET, 1252 },                    /* scaron */
            { 354, 0xDE, EASTEUROPE_CHARSET, 1250 },              /* Tcedil */
            { 355, 0xFE, EASTEUROPE_CHARSET, 1250 },              /* tcedil */
            { 356, 0x8D, EASTEUROPE_CHARSET, 1250 },              /* Tcaron */
            { 357, 0x9D, EASTEUROPE_CHARSET, 1250 },              /* tcaron */
            { 366, 0xD9, EASTEUROPE_CHARSET, 1250 },               /* Uring */
            { 367, 0xF9, EASTEUROPE_CHARSET, 1250 },               /* uring */
            { 368, 0xDB, EASTEUROPE_CHARSET, 1250 },              /* Udblac */
            { 369, 0xFB, EASTEUROPE_CHARSET, 1250 },              /* udblac */
            { 376,  159, ANSI_CHARSET, 1252 },                      /* Yuml */
            { 377, 0x8F, EASTEUROPE_CHARSET, 1250 },              /* Zacute */
            { 378, 0x9F, EASTEUROPE_CHARSET, 1250 },              /* zacute */
            { 379, 0xAF, EASTEUROPE_CHARSET, 1250 },                /* Zdot */
            { 380, 0xBF, EASTEUROPE_CHARSET, 1250 },                /* zdot */
            { 381, 0x8E, EASTEUROPE_CHARSET, 1250 },              /* Zcaron */
            { 382, 0x9E, EASTEUROPE_CHARSET, 1250 },              /* zcaron */

            /* Latin-extended B */
            { 402, 166, SYMBOL_CHARSET, CP_SYMBOL },                /* fnof */

            /* Latin-2 accents */
            { 710, 136, ANSI_CHARSET, 1252 },                       /* circ */
            { 711, 0xA1, EASTEUROPE_CHARSET, 1250 },               /* caron */
            { 728, 0xA2, EASTEUROPE_CHARSET, 1250 },               /* breve */
            { 729, 0xFF, EASTEUROPE_CHARSET, 1250 },                 /* dot */
            { 731, 0xB2, EASTEUROPE_CHARSET, 1250 },                /* ogon */
            { 732, 152, ANSI_CHARSET, 1252 },                      /* tilde */
            { 733, 0xBD, EASTEUROPE_CHARSET, 1250 },               /* dblac */

            /* Greek letters */
            { 913, 'A', SYMBOL_CHARSET, CP_SYMBOL },               /* Alpha */
            { 914, 'B', SYMBOL_CHARSET, CP_SYMBOL },                /* Beta */
            { 915, 'G', SYMBOL_CHARSET, CP_SYMBOL },               /* Gamma */
            { 916, 'D', SYMBOL_CHARSET, CP_SYMBOL },               /* Delta */
            { 917, 'E', SYMBOL_CHARSET, CP_SYMBOL },             /* Epsilon */
            { 918, 'Z', SYMBOL_CHARSET, CP_SYMBOL },                /* Zeta */
            { 919, 'H', SYMBOL_CHARSET, CP_SYMBOL },                 /* Eta */
            { 920, 'Q', SYMBOL_CHARSET, CP_SYMBOL },               /* Theta */
            { 921, 'I', SYMBOL_CHARSET, CP_SYMBOL },                /* Iota */
            { 922, 'K', SYMBOL_CHARSET, CP_SYMBOL },               /* Kappa */
            { 923, 'L', SYMBOL_CHARSET, CP_SYMBOL },              /* Lambda */
            { 924, 'M', SYMBOL_CHARSET, CP_SYMBOL },                  /* Mu */
            { 925, 'N', SYMBOL_CHARSET, CP_SYMBOL },                  /* Nu */
            { 926, 'X', SYMBOL_CHARSET, CP_SYMBOL },                  /* Xi */
            { 927, 'O', SYMBOL_CHARSET, CP_SYMBOL },             /* Omicron */
            { 928, 'P', SYMBOL_CHARSET, CP_SYMBOL },                  /* Pi */
            { 929, 'R', SYMBOL_CHARSET, CP_SYMBOL },                 /* Rho */
            { 931, 'S', SYMBOL_CHARSET, CP_SYMBOL },               /* Sigma */
            { 932, 'T', SYMBOL_CHARSET, CP_SYMBOL },                 /* Tau */
            { 933, 'U', SYMBOL_CHARSET, CP_SYMBOL },             /* Upsilon */
            { 934, 'F', SYMBOL_CHARSET, CP_SYMBOL },                 /* Phi */
            { 935, 'C', SYMBOL_CHARSET, CP_SYMBOL },                 /* Chi */
            { 936, 'Y', SYMBOL_CHARSET, CP_SYMBOL },                 /* Psi */
            { 937, 'W', SYMBOL_CHARSET, CP_SYMBOL },               /* Omega */
            { 945, 'a', SYMBOL_CHARSET, CP_SYMBOL },               /* alpha */
            { 946, 'b', SYMBOL_CHARSET, CP_SYMBOL },                /* beta */
            { 947, 'g', SYMBOL_CHARSET, CP_SYMBOL },               /* gamma */
            { 948, 'd', SYMBOL_CHARSET, CP_SYMBOL },               /* delta */
            { 949, 'e', SYMBOL_CHARSET, CP_SYMBOL },             /* epsilon */
            { 950, 'z', SYMBOL_CHARSET, CP_SYMBOL },                /* zeta */
            { 951, 'h', SYMBOL_CHARSET, CP_SYMBOL },                 /* eta */
            { 952, 'q', SYMBOL_CHARSET, CP_SYMBOL },               /* theta */
            { 953, 'i', SYMBOL_CHARSET, CP_SYMBOL },                /* iota */
            { 954, 'k', SYMBOL_CHARSET, CP_SYMBOL },               /* kappa */
            { 955, 'l', SYMBOL_CHARSET, CP_SYMBOL },              /* lambda */
            { 956, 'm', SYMBOL_CHARSET, CP_SYMBOL },                  /* mu */
            { 957, 'n', SYMBOL_CHARSET, CP_SYMBOL },                  /* nu */
            { 958, 'x', SYMBOL_CHARSET, CP_SYMBOL },                  /* xi */
            { 959, 'o', SYMBOL_CHARSET, CP_SYMBOL },             /* omicron */
            { 960, 'p', SYMBOL_CHARSET, CP_SYMBOL },                  /* pi */
            { 961, 'r', SYMBOL_CHARSET, CP_SYMBOL },                 /* rho */
            { 962, 'V', SYMBOL_CHARSET, CP_SYMBOL },              /* sigmaf */
            { 963, 's', SYMBOL_CHARSET, CP_SYMBOL },               /* sigma */
            { 964, 't', SYMBOL_CHARSET, CP_SYMBOL },                 /* tau */
            { 965, 'u', SYMBOL_CHARSET, CP_SYMBOL },             /* upsilon */
            { 966, 'f', SYMBOL_CHARSET, CP_SYMBOL },                 /* phi */
            { 967, 'c', SYMBOL_CHARSET, CP_SYMBOL },                 /* chi */
            { 968, 'y', SYMBOL_CHARSET, CP_SYMBOL },                 /* psi */
            { 969, 'w', SYMBOL_CHARSET, CP_SYMBOL },               /* omega */
            { 977, 'J', SYMBOL_CHARSET, CP_SYMBOL },            /* thetasym */
            { 978, 161, SYMBOL_CHARSET, CP_SYMBOL },               /* upsih */
            { 982, 'v', SYMBOL_CHARSET, CP_SYMBOL },                 /* piv */

            /* additional punctuation */
            { 8211, 150, ANY_125X_CHARSET, 0 },                    /* ndash */
            { 8212, 151, ANY_125X_CHARSET, 0 },                    /* mdash */
            { 8216, 145, ANY_125X_CHARSET, 0 },                    /* lsquo */
            { 8217, 146, ANY_125X_CHARSET, 0 },                    /* rsquo */
            { 8218, 130, ANY_125X_CHARSET, 0 },                    /* sbquo */
            { 8220, 147, ANY_125X_CHARSET, 0 },                    /* ldquo */
            { 8221, 148, ANY_125X_CHARSET, 0 },                    /* rdquo */
            { 8222, 132, ANY_125X_CHARSET, 0 },                    /* bdquo */
            { 8224, 134, ANY_125X_CHARSET, 0 },                   /* dagger */
            { 8225, 135, ANY_125X_CHARSET, 0 },                   /* Dagger */
            { 8226, 149, ANY_125X_CHARSET, 0 },                     /* bull */
            { 8230, 133, ANY_125X_CHARSET, 0 },                   /* hellip */
            { 8240, 137, ANY_125X_CHARSET, 0 },                   /* permil */
            { 8242, 162, SYMBOL_CHARSET, CP_SYMBOL },              /* prime */
            { 8243, 178, SYMBOL_CHARSET, CP_SYMBOL },              /* Prime */
            { 8249, 139, ANY_125X_CHARSET, 1252 },                /* lsaquo */
            { 8250, 155, ANY_125X_CHARSET, 1252 },                /* rsaquo */
            { 8254, '`', SYMBOL_CHARSET, CP_SYMBOL },              /* oline */
            { 8260, 164, SYMBOL_CHARSET, CP_SYMBOL },              /* frasl */
            
            /* letter-like symbols */
            { 8465, 193, SYMBOL_CHARSET, CP_SYMBOL },              /* image */
            { 8472, 195, SYMBOL_CHARSET, CP_SYMBOL },             /* weierp */
            { 8476, 194, SYMBOL_CHARSET, CP_SYMBOL },               /* real */
            { 8482, 153, ANY_125X_CHARSET, 0 },                    /* trade */
            { 8501, 192, SYMBOL_CHARSET, CP_SYMBOL },            /* alefsym */
            
            /* arrows */
            { 8592, 172, SYMBOL_CHARSET, CP_SYMBOL },               /* larr */
            { 8593, 173, SYMBOL_CHARSET, CP_SYMBOL },               /* uarr */
            { 8594, 174, SYMBOL_CHARSET, CP_SYMBOL },               /* rarr */
            { 8595, 175, SYMBOL_CHARSET, CP_SYMBOL },               /* darr */
            { 8596, 171, SYMBOL_CHARSET, CP_SYMBOL },               /* harr */
            { 8629, 191, SYMBOL_CHARSET, CP_SYMBOL },              /* crarr */
            { 8656, 220, SYMBOL_CHARSET, CP_SYMBOL },               /* lArr */
            { 8657, 221, SYMBOL_CHARSET, CP_SYMBOL },               /* uArr */
            { 8658, 222, SYMBOL_CHARSET, CP_SYMBOL },               /* rArr */
            { 8659, 223, SYMBOL_CHARSET, CP_SYMBOL },               /* dArr */
            { 8660, 219, SYMBOL_CHARSET, CP_SYMBOL },               /* hArr */
            
            /* mathematical operators */
            { 8704, 34, SYMBOL_CHARSET, CP_SYMBOL },              /* forall */
            { 8706, 182, SYMBOL_CHARSET, CP_SYMBOL },               /* part */
            { 8707, '$', SYMBOL_CHARSET, CP_SYMBOL },              /* exist */
            { 8709, 198, SYMBOL_CHARSET, CP_SYMBOL },              /* empty */
            { 8711, 209, SYMBOL_CHARSET, CP_SYMBOL },              /* nabla */
            { 8712, 206, SYMBOL_CHARSET, CP_SYMBOL },               /* isin */
            { 8713, 207, SYMBOL_CHARSET, CP_SYMBOL },              /* notin */
            { 8715, '\'', SYMBOL_CHARSET, CP_SYMBOL },                /* ni */
            { 8719, 213, SYMBOL_CHARSET, CP_SYMBOL },               /* prod */
            { 8721, 229, SYMBOL_CHARSET, CP_SYMBOL },                /* sum */
            { 8722, '-', SYMBOL_CHARSET, CP_SYMBOL },              /* minus */
            { 8727, '*', SYMBOL_CHARSET, CP_SYMBOL },             /* lowast */
            { 8730, 214, SYMBOL_CHARSET, CP_SYMBOL },              /* radic */
            { 8733, 181, SYMBOL_CHARSET, CP_SYMBOL },               /* prop */
            { 8734, 165, SYMBOL_CHARSET, CP_SYMBOL },              /* infin */
            { 8736, 208, SYMBOL_CHARSET, CP_SYMBOL },                /* ang */
            { 8743, 217, SYMBOL_CHARSET, CP_SYMBOL },                /* and */
            { 8744, 218, SYMBOL_CHARSET, CP_SYMBOL },                 /* or */
            { 8745, 199, SYMBOL_CHARSET, CP_SYMBOL },                /* cap */
            { 8746, 200, SYMBOL_CHARSET, CP_SYMBOL },                /* cup */
            { 8747, 242, SYMBOL_CHARSET, CP_SYMBOL },                /* int */
            { 8756, '\\', SYMBOL_CHARSET, CP_SYMBOL },            /* there4 */
            { 8764, '~', SYMBOL_CHARSET, CP_SYMBOL },                /* sim */
            { 8773, '@', SYMBOL_CHARSET, CP_SYMBOL },               /* cong */
            { 8776, 187, SYMBOL_CHARSET, CP_SYMBOL },              /* asymp */
            { 8800, 185, SYMBOL_CHARSET, CP_SYMBOL },                 /* ne */
            { 8801, 186, SYMBOL_CHARSET, CP_SYMBOL },              /* equiv */
            { 8804, 163, SYMBOL_CHARSET, CP_SYMBOL },                 /* le */
            { 8805, 179, SYMBOL_CHARSET, CP_SYMBOL },                 /* ge */
            { 8834, 204, SYMBOL_CHARSET, CP_SYMBOL },                /* sub */
            { 8835, 201, SYMBOL_CHARSET, CP_SYMBOL },                /* sup */
            { 8836, 203, SYMBOL_CHARSET, CP_SYMBOL },               /* nsub */
            { 8838, 205, SYMBOL_CHARSET, CP_SYMBOL },               /* sube */
            { 8839, 202, SYMBOL_CHARSET, CP_SYMBOL },               /* supe */
            { 8853, 197, SYMBOL_CHARSET, CP_SYMBOL },              /* oplus */
            { 8855, 196, SYMBOL_CHARSET, CP_SYMBOL },             /* otimes */
            { 8869, '^', SYMBOL_CHARSET, CP_SYMBOL },               /* perp */
            { 8901, 215, SYMBOL_CHARSET, CP_SYMBOL },               /* sdot */
            { 8968, 233, SYMBOL_CHARSET, CP_SYMBOL },              /* lceil */
            { 8969, 249, SYMBOL_CHARSET, CP_SYMBOL },              /* rceil */
            { 8970, 235, SYMBOL_CHARSET, CP_SYMBOL },             /* lfloor */
            { 8971, 251, SYMBOL_CHARSET, CP_SYMBOL },             /* rfloor */
            { 9001, 225, SYMBOL_CHARSET, CP_SYMBOL },               /* lang */
            { 9002, 241, SYMBOL_CHARSET, CP_SYMBOL },               /* rang */

            /* geometric shapes */
            { 9674, 224, SYMBOL_CHARSET, CP_SYMBOL },                /* loz */

            /* miscellaneous symbols */
            { 9824, 170, SYMBOL_CHARSET, CP_SYMBOL },             /* spades */
            { 9827, 167, SYMBOL_CHARSET, CP_SYMBOL },              /* clubs */
            { 9829, 169, SYMBOL_CHARSET, CP_SYMBOL },             /* hearts */
            { 9830, 168, SYMBOL_CHARSET, CP_SYMBOL }            /* diamonds */
        };
        int hi, lo;

        /*
         *   Find the character in the mapping table 
         */
        lo = 0;
        hi = sizeof(mapping)/sizeof(mapping[0]) - 1;
        for (;;)
        {
            int cur;
            
            /* if there's nothing left in the table, we didn't find it */
            if (lo > hi)
            {
                /* use the invalid character in the default character set */
                result[0] = invalid_char_val_;
                new_charset = default_charset_;
                break;
            }

            /* split the difference */
            cur = lo + (hi - lo)/2;

            /* is this the one we're looking for? */
            if (mapping[cur].html_val == charval)
            {
                /* this is it */
                result[0] = (textchar_t)mapping[cur].win_val;
                new_charset.charset = mapping[cur].win_charset;
                new_charset.codepage = mapping[cur].codepage;

                /* 
                 *   If the character set to use is the default, use the
                 *   current system default.
                 */
                if (new_charset.charset == DEFAULT_CHARSET)
                    new_charset = default_charset_;

                /*
                 *   If the new character set is the special ANY_125X_CHARSET
                 *   value, it means any Windows 125x character set will
                 *   work, because it's a character that's in common (and at
                 *   the same code point) in all of these.  In these cases,
                 *   we can simply use the default character set if it's a
                 *   Windows 125x character set.  Otherwise, we'll have to
                 *   fall back on 1252.  
                 */
                if (new_charset.charset == ANY_125X_CHARSET)
                {
                    /* 
                     *   we can use the default character set if it's a 125x
                     *   code page; otherwise we have to use 1252 
                     */
                    if (default_charset_.codepage >= 1250
                        && default_charset_.codepage <= 1259)
                    {
                        /* use the current character set */
                        new_charset = default_charset_;
                    }
                    else
                    {
                        /* use code page 1252 */
                        new_charset.charset = ANSI_CHARSET;
                        new_charset.codepage = 1252;
                    }
                }

                /*
                 *   If the character set is not the default, and we're
                 *   not allowed to change character sets, return the
                 *   invalid character. 
                 */
                if (new_charset.charset != default_charset_.charset
                    && charset == 0)
                {
                    result[0] = invalid_char_val_;
                    new_charset = default_charset_;
                }

                /* done */
                break;
            }
            else if (mapping[cur].html_val < charval)
            {
                /* we want something higher - look in the upper half */
                lo = (cur == lo ? lo + 1 : cur);
            }
            else
            {
                /* we want something smaller - look in the lower half */
                hi = (cur == hi ? hi - 1 : cur);
            }
        }
    }

    /* return information on a character set change, if possible */
    if (charset != 0)
    {
        /* tell them the new character set */
        *charset = new_charset;

        /* tell them whether it's the same as the default character set */
        *changed_charset = !new_charset.equals(default_charset_);
    }
    else if (!new_charset.equals(default_charset_))
    {
        /* 
         *   We need to change to a non-default character set, but the caller
         *   doesn't allow this.  We cannot map the character in this case,
         *   because the character doesn't exist in the caller's character
         *   set.  Return a single missing-character mapping.  
         */
        result[0] = invalid_char_val_;
        len = 1;
    }

    /* return the number of bytes in the result */
    return len;
}

/* ------------------------------------------------------------------------ */
/*
 *   Parser state object link structure implementation
 */

CHtmlSys_mainwin_state_link::~CHtmlSys_mainwin_state_link()
{
    /* delete our parser state object */
    delete state_;
}

/* ------------------------------------------------------------------------ */
/*
 *   Debug window implementation 
 */

CHtmlSys_dbgwin::CHtmlSys_dbgwin(CHtmlParser *parser,
                                 CHtmlPreferences *prefs)
{
    /* remember my parser */
    parser_ = parser;

    /* remember my preferences, and add a reference to it */
    prefs_ = prefs;
    prefs_->add_ref();
}

/*
 *   initialize the HTML panel
 */
void CHtmlSys_dbgwin::init_html_panel(CHtmlFormatter *formatter)
{
    /* initialize the parser */
    init_parser();

    /* create the HTML display window */
    html_panel_ = new CHtmlSysWin_win32(formatter, this, TRUE, TRUE, prefs_);
}

CHtmlSys_dbgwin::~CHtmlSys_dbgwin()
{
    /* release our reference on the preferences object */
    prefs_->release_ref();
}

/*
 *   Initialize the sizebox.  By default, we'll simply turn on the sizebox in
 *   the underlying HTML panel.  
 */
void CHtmlSys_dbgwin::set_panel_has_sizebox()
{
    /* turn on the sizebox in the underlying HTML panel */
    get_html_panel()->set_has_sizebox(TRUE);
}

/*
 *   process creation event 
 */
void CHtmlSys_dbgwin::do_create()
{
    RECT pos;

    /* inherit default */
    CHtmlSys_framewin::do_create();
    
    /* create the main panel's system window */
    GetClientRect(handle_, &pos);
    html_panel_->create_system_window(this, TRUE, "", &pos);

    /* load my menu */
    load_menu();
}

/*
 *   destroy the window 
 */
void CHtmlSys_dbgwin::do_destroy()
{
    /* tell my child window to disengage from its parent */
    html_panel_->on_destroy_parent();
    html_panel_->forget_owner();

    /* forget my subwindow */
    html_panel_ = 0;

    /* inherit default */
    CTadsWin::do_destroy();
}

void CHtmlSys_dbgwin::do_resize(int mode, int x, int y)
{
    switch(mode)
    {
    case SIZE_MAXHIDE:
    case SIZE_MAXSHOW:
    case SIZE_MINIMIZED:
        /* don't bother resizing the subwindow on these changes */
        break;

    case SIZE_MAXIMIZED:
    case SIZE_RESTORED:
    default:
        {
            RECT rc;

            /* compute the area available for the subwindow */
            SetRect(&rc, 1, 1, x, y);

            /* place the main panel */
            if (html_panel_ != 0)
                MoveWindow(html_panel_->get_handle(),
                           rc.left, rc.top,
                           rc.right - rc.left, rc.bottom - rc.top, TRUE);
            
            /* inherit default handling to save position in prefs */
            CHtmlSys_framewin::do_resize(mode, x, y);
        }
        break;
    }
}

/*
 *   Close the owner window 
 */
int CHtmlSys_dbgwin::close_owner_window(int force)
{
    /* 
     *   if forcing, destroy the window without asking the user anything;
     *   otherwise, send a normal close message so that the user has a
     *   chance to object 
     */
    if (force)
    {
        /* destroy our handle unconditionally */
        destroy_handle();

        /* success */
        return 0;
    }
    else
        return !SendMessage(handle_, WM_CLOSE, 0, 0);
}

/*
 *   Bring the window to the front 
 */
void CHtmlSys_dbgwin::bring_owner_to_front()
{
    BringWindowToTop(handle_);
}

/*
 *   Gain focus - set focus to our main panel 
 */
void CHtmlSys_dbgwin::do_setfocus(HWND)
{
    /* move focus to our HTML panel */
    SetFocus(html_panel_->get_handle());
}

/*
 *   Get my selection range 
 */
int CHtmlSys_dbgwin::get_sel_range(CHtmlFormatter **formatter,
                                   unsigned long *sel_start,
                                   unsigned long *sel_end)
{
    /* get the selection range from my html panel's formatter */
    *formatter = ((CHtmlSysWin_win32 *)html_panel_)->get_formatter();
    (*formatter)->get_sel_range(sel_start, sel_end);

    /* we do have a selection range */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Debug log window implementation 
 */

CHtmlSys_dbglogwin::~CHtmlSys_dbglogwin()
{
    /* done with my text buffer */
    delete txtbuf_;
}

/*
 *   process destruction of the window 
 */
void CHtmlSys_dbglogwin::do_destroy()
{
    CHtmlSysWin_win32 *subwin;
    CHtmlSys_dbglogwin_hostifc *debugger_ifc;

    /* 
     *   Remember the debugger host interface and our subwindow in locals,
     *   so that these values survive our deletion.  
     */
    debugger_ifc = debugger_ifc_;
    subwin = get_html_panel();
    
    /* inherit default */
    CHtmlSys_dbgwin::do_destroy();

    /* 
     *   if I'm the current debug log window for the main game window,
     *   tell the game window that I'm gone 
     */
    if (CHtmlSys_mainwin::get_main_win() != 0
        && CHtmlSys_mainwin::get_main_win()->get_debug_win() == this)
        CHtmlSys_mainwin::get_main_win()->set_debug_win(0, TRUE);

    /*
     *   Notify the host interface of our destruction, if we have a host
     *   interface.  Note that we cached the member variable pointer to
     *   the host interface in a local prior to inheriting the superclass
     *   do_destroy, since 'this' will probably have been deleted by the
     *   time that returns.  It's necessary to let 'this' be deleted prior
     *   to invoking the callback, since the callback might delete other
     *   objects (such as the parser and formatter) that we depend on as
     *   long as this object exists.  
     */
    if (debugger_ifc != 0)
    {
        /* tell the debugger we've been destroyed */
        debugger_ifc->dbghostifc_on_destroy(this, subwin);

        /* release the interface */
        debugger_ifc->dbghostifc_remove_ref();
    }
}

/*
 *   initialize the HTML panel 
 */
void CHtmlSys_dbglogwin::init_html_panel(CHtmlFormatter *formatter)
{
    /* initialize the parser */
    init_parser();

    /* create the HTML display window - use a debug log window subclass */
    html_panel_ = new CHtmlSysWin_win32_dbglog(formatter, this,
                                               TRUE, TRUE, prefs_,
                                               debugger_ifc_);
}

/*
 *   load my menu 
 */
void CHtmlSys_dbglogwin::load_menu()
{
    /* load our menu */
    SetMenu(handle_, LoadMenu(CTadsApp::get_app()->get_instance(),
                              MAKEINTRESOURCE(IDR_DEBUGWIN_MENU)));
}

/*
 *   Process a close-window request.  Never actually close the debug
 *   window; merely hide it.  
 */
int CHtmlSys_dbglogwin::do_close()
{
    /* hide the window */
    ShowWindow(handle_, SW_HIDE);

    /* tell the caller not to really close the window */
    return FALSE;
}

/*
 *   Set up the parser 
 */
void CHtmlSys_dbglogwin::init_parser()
{
    static const textchar_t htmltxt[] = "<basefont size=4><listing>";

    /* create a text buffer */
    txtbuf_ = new CHtmlTextBuffer();

    /* 
     *   put the parser into <listing> mode, so that it displays in a
     *   monospaced font 
     */
    parser_->obey_markups(TRUE);
    txtbuf_->append(htmltxt, get_strlen(htmltxt));
    parser_->parse(txtbuf_, this);
    txtbuf_->clear();
    parser_->obey_markups(FALSE);
    parser_->obey_end_markups(FALSE);
}

/*
 *   get text for a FIND command 
 */
const textchar_t *CHtmlSys_dbglogwin::get_find_text(
    int command_id, int *exact_case, int *start_at_top, int *wrap, int *dir)
{
    /* if we have a debug host interface, ask it to get the text */
    if (debugger_ifc_ != 0)
        return debugger_ifc_->dbghostifc_get_find_text(
            command_id, exact_case, start_at_top, wrap, dir);

    /* no debug host interface - we can't ask for Find text */
    return 0;
}

/* 
 *   process a notification message 
 */
int CHtmlSys_dbglogwin::do_notify(int control_id, int notify_code, LPNMHDR nm)
{
    /* check the message */
    switch(notify_code)
    {
    case NM_SETFOCUS:
    case NM_KILLFOCUS:
        /* notify myself of an artificial activation */
        do_parent_activate(notify_code == NM_SETFOCUS);

        /* inherit the default handling as well */
        break;

    default:
        /* no special handling */
        break;
    }

    /* inherit default handling */
    return CHtmlSys_dbgwin::do_notify(control_id, notify_code, nm);
}

/*
 *   Non-client Activate/deactivate.  Notify the debugger main window of the
 *   coming or going of this debugger window as the active debugger window,
 *   which will allow us to receive certain command messages from the main
 *   window.
 *   
 *   We handle this in the non-client activation, since MDI doesn't send
 *   regular activation messages to child windows when the MDI frame itself
 *   is becoming active or inactive.  
 */
int CHtmlSys_dbglogwin::do_ncactivate(int flag)
{
    /* set active/inactive with the main window */
    if (debugger_ifc_ != 0)
        debugger_ifc_->dbghostifc_notify_active_dbg_win(this, flag);

    /* inherit default handling */
    return CHtmlSys_dbgwin::do_ncactivate(flag);
}

/*
 *   Receive notification of activation from parent 
 */
void CHtmlSys_dbglogwin::do_parent_activate(int flag)
{
    /* set active/inactive with the main window */
    if (debugger_ifc_ != 0)
        debugger_ifc_->dbghostifc_notify_active_dbg_win(this, flag);

    /* inherit default handling */
    CHtmlSys_dbgwin::do_parent_activate(flag);
}

/*
 *   process a user event 
 */
int CHtmlSys_dbglogwin::do_user_message(int msg, WPARAM wpar, LPARAM lpar)
{
    switch(msg)
    {
    case HTMLM_DBG_PRINT:
        /* display the text */
        disp_text((const textchar_t *)lpar);

        /* if the window isn't currently showing, make it visible */
        if (!IsWindowVisible(get_handle()))
            ShowWindow(get_handle(), SW_SHOW);

        /* bring the window to the foreground if desired */
        if (wpar != 0)
            BringWindowToTop(get_handle());

        /* update the window so the new text is shown immediately */
        UpdateWindow(handle_);

        /* processed */
        return TRUE;

    case HTMLM_DBG_PRINT_FONT:
        {
            CHtmlDbgPrintFontMsg *info;

            /* get the info structure from the LPARAM */
            info = (CHtmlDbgPrintFontMsg *)lpar;

            /* display the text using the given font */
            disp_text(info->font_, info->msg_);

            /* if the window isn't currently showing, make it visible */
            if (!IsWindowVisible(get_handle()))
                ShowWindow(get_handle(), SW_SHOW);

            /* bring the window to the foreground if desired */
            if (info->foreground_)
                BringWindowToTop(get_handle());

            /* update the window so the new text is shown immediately */
            UpdateWindow(handle_);
        }

        /* processed */
        return TRUE;

    default:
        /* inherit default behavior */
        return CHtmlSys_dbgwin::do_user_message(msg, wpar, lpar);
    }
}

/*
 *   Check a command status, for routing from the main debugger app frame
 *   window.  Pass the command to our HTML panel for processing.  
 */
TadsCmdStat_t CHtmlSys_dbglogwin::active_check_command(int command_id)
{
    return ((CHtmlSysWin_win32_dbglog *)html_panel_)
        ->check_command(command_id);
}

/*
 *   Process a command, for routing from the main window.  Pass the command
 *   to our HTML panel.  
 */
int CHtmlSys_dbglogwin::active_do_command(int notify_code, int command_id,
                                          HWND ctl)
{
    return ((CHtmlSysWin_win32_dbglog *)html_panel_)
        ->do_command(notify_code, command_id, ctl);
}

/*
 *   Display text 
 */
void CHtmlSys_dbglogwin::disp_text(const textchar_t *msg)
{
    /* parse the text */
    append_and_parse(msg);
    
    /* update the display */
    html_panel_->do_formatting(FALSE, FALSE, FALSE);
}

/*
 *   Display text with the given font 
 */
void CHtmlSys_dbglogwin::disp_text(const CHtmlFontDesc *font,
                                   const textchar_t *msg)
{
    CHtmlTagFontExplicit *font_tag;
    
    /* add a tag to set the new font */
    font_tag = new CHtmlTagFontExplicit(font);
    font_tag->on_parse(parser_);

    /* parse the text */
    append_and_parse(msg);

    /* close the font container tag */
    parser_->close_current_tag();

    /* update the display */
    html_panel_->do_formatting(FALSE, FALSE, FALSE);
}

/*
 *   Service routine for disp_text variants - add text to the parser
 *   buffer, parse the text, and clear the text from the parser buffer.
 *   We'll deal only in whole lines; if we have an unfinished line (i.e.,
 *   no '\n' at the end), we'll leave the part after the last '\n' in the
 *   message sitting in our internal text buffer for later, when the end
 *   of the line arrives.  
 */
void CHtmlSys_dbglogwin::append_and_parse(const textchar_t *msg)
{
    const textchar_t *p;
    const textchar_t *last_nl;
    
    /* 
     *   Find the last newline in the buffer.  We only want to display as
     *   far as the last newline, to ensure that we only send complete
     *   lines to the parser.  (Sending partial lines confuses the
     *   preformatted text mechanism in certain cases, since it assumes
     *   that it will see only complete lines in preformatted text.)  
     */
    for (last_nl = 0, p = msg ; *p != '\0' ; ++p)
    {
        /* 
         *   if this is a newline, it's the last one we've seen thus far,
         *   so note it 
         */
        if (*p == '\n')
            last_nl = p;
    }

    /* check to see if we found any newlines */
    if (last_nl != 0)
    {
        /* 
         *   we found at least one newline - put everything up to and
         *   including the last newline into the buffer 
         */
        txtbuf_->append(msg, (last_nl - msg) + 1);

        /* parse up to the last newline */
        parser_->parse(txtbuf_, this);

        /* we're done with this text - clear the buffer */
        txtbuf_->clear();

        /* 
         *   save everything after (not including) the last newline for
         *   later, so that we defer parsing it until we get the remainder
         *   of this line 
         */
        msg = last_nl + 1;
    }

    /* 
     *   if we have anything left, add it to the text buffer to process
     *   later, when the remainder of the last line arrives 
     */
    if (*msg != '\0')
        txtbuf_->append(msg, strlen(msg));
}

/*
 *   set my window position in the preferences 
 */
void CHtmlSys_dbglogwin::set_winpos_prefs(const CHtmlRect *winpos)
{
    prefs_->set_dbgwin_pos(winpos);
}

/*
 *   Handle a command 
 */
int CHtmlSys_dbglogwin::do_command(int notify_code,
                                   int command_id, HWND ctl)
{
    switch(command_id)
    {
    case ID_FILE_HIDE:
        /* hide the window */
        ShowWindow(get_handle(), SW_HIDE);
        return TRUE;

    default:
        /* pass the command to the main panel for processing */
        if (html_panel_ != 0)
            return html_panel_->do_command(notify_code, command_id, ctl);
        break;
    }

    /* didn't handle it */
    return FALSE;
}

/*
 *   Check the status of a command 
 */
TadsCmdStat_t CHtmlSys_dbglogwin::check_command(int command_id)
{
    switch(command_id)
    {
    case ID_FILE_HIDE:
        /* we can always hide the debug window */
        return TADSCMD_ENABLED;

    default:
        /* pass it to the main panel for processing */
        if (html_panel_ != 0)
            return html_panel_->check_command(command_id);
        break;
    }

    /* didn't handle it */
    return TADSCMD_UNKNOWN;
}

/* ------------------------------------------------------------------------ */
/*
 *   Debug Log HTML panel subclass 
 */

CHtmlSysWin_win32_dbglog::CHtmlSysWin_win32_dbglog
   (class CHtmlFormatter *formatter,
    class CHtmlSysWin_win32_owner *owner,
    int has_vscroll, int has_hscroll,
    class CHtmlPreferences *prefs,
    class CHtmlSys_dbglogwin_hostifc *debugger_ifc)
    : CHtmlSysWin_win32(formatter, owner, has_vscroll, has_hscroll, prefs)
{
    /* remember the debugger interface object */
    debugger_ifc_ = debugger_ifc;
    
    /* 
     *   if we have a debugger interface, load our extended context menu 
     */
    if (debugger_ifc_ != 0)
        load_context_popup(IDR_DEBUGLOG_POPUP);
}


/*
 *   handle a mouse click 
 */
int CHtmlSysWin_win32_dbglog::do_leftbtn_down(int keys, int x, int y,
                                              int clicks)
{
    int ret;
    
    /* inherit the default handling first */
    ret = CHtmlSysWin_win32::do_leftbtn_down(keys, x, y, clicks);

    /* 
     *   If that was handled, and this is a double-click, check to see if
     *   the selected line contains a compiler error message with a source
     *   file name and line number - if so, attempt to open the source
     *   file to the referenced line.  We can only do this if we have a
     *   debugger host interface.
     */
    if (ret && clicks == 2)
        show_error_line();

    /* return the result */
    return ret;
}

/*
 *   command status 
 */
TadsCmdStat_t CHtmlSysWin_win32_dbglog::check_command(int command_id)
{
    /* check for commands we recognize */
    switch(command_id)
    {
    case ID_DEBUG_SHOWERRORLINE:
        /* always enable this command */
        return TADSCMD_ENABLED;

    default:
        /* it's not one of ours; go inherit the default handling */
        break;
    }

    /* inherit default */
    return CHtmlSysWin_win32::check_command(command_id);
}

/*
 *   process a command 
 */
int CHtmlSysWin_win32_dbglog::do_command(int notify_code, int command_id,
                                         HWND ctl)
{
    /* check for commands we recognize */
    switch(command_id)
    {
    case ID_DEBUG_SHOWERRORLINE:
        /* show the error line, if applicable */
        show_error_line();
        return TRUE;

    default:
        /* it's not one of ours; go inherit the default handling */
        break;
    }

    /* inherit default */
    return CHtmlSysWin_win32::do_command(notify_code, command_id, ctl);
}

/*
 *   Open the source file and go to the line indicated by the error
 *   message in our window, if any, at the current cursor position.  
 */
void CHtmlSysWin_win32_dbglog::show_error_line()
{
    unsigned long sel_start, sel_end;
    unsigned long line_start, line_end;
    unsigned long cur_ofs;
    size_t len;
    char *txtp;
    char *p;

    /* if there's no debugger interface, there's nothing we can do */
    if (debugger_ifc_ == 0)
        return;

    /* get the selected text range */
    formatter_->get_sel_range(&sel_start, &sel_end);
    cur_ofs = caret_at_right_ ? sel_end : sel_start;

    /* work backwards until we find an error line */
    for ( ;; )
    {
        CHtmlDisp *disp;
        
        /* get the limits of the text at the cursor */
        formatter_->get_line_limits(&line_start, &line_end, cur_ofs);
    
        /* momentarily select the entire range */
        formatter_->set_sel_range(line_start, line_end);
    
        /* extract the text to a memory buffer */
        txtp = (char *)copy_to_new_hglobal(GMEM_FIXED, &len);

        /* restore the old selection range */
        formatter_->set_sel_range(sel_start, sel_end);

        /* if there's no text pointer, there's nothing to show */
        if (txtp == 0)
            return;
    
        /* 
         *   Scan for something that looks like a compiler error message:
         *   "filename.ext(linenum): ".  First, look for the '('.  
         */
        for (p = txtp ; p < txtp + len && *p != '\0' && *p != '(' ; ++p) ;
    
        /* 
         *   if we found the '(', look for the ')', and make sure only digits
         *   are between here and there 
         */
        if (p < txtp + len && *p == '(')
        {
            char *paren;
            
            /* 
             *   remember the paren location, since this is where the
             *   filename ends 
             */
            paren = p;
            for (++p ;
                 p < txtp + len && *p != '\0' && *p != ')' && isdigit(*p) ;
                 ++p) ;
            
            /* 
             *   if we stopped on a ')', and we found at least one digit,
             *   we're still okay - check for a colon and a space 
             */
            if (p + 2 < txtp + len && *p == ')'
                && *(p+1) == ':'
                && *(p+2) == ' ')
            {
                char *fname;
                long linenum;
                
                /* 
                 *   This looks like a compiler error message.  Get the
                 *   filename portion and null-terminate it, and get the line
                 *   number portion.  
                 */
                fname = txtp;
                *paren = '\0';
                linenum = atol(paren + 1);
                
                /* display the file */
                debugger_ifc_->dbghostifc_open_source(fname, linenum, 1);
                
                /* select the entire error line */
                formatter_->set_sel_range(line_start, line_end);
                fmt_sel_to_cmd_sel(TRUE);

                /* got it - stop looking */
                break;
            }
        }

        /* didn't find it - try moving to the previous line */
        if (cur_ofs > 0)
            cur_ofs = line_start - 1;
        else
            break;

        /* get the previous item */
        disp = formatter_->find_by_txtofs(cur_ofs, TRUE, FALSE);

        /* if we didn't get an item, give up */
        if (disp == 0)
            break;

        /* move to the start of the item */
        cur_ofs = disp->get_text_ofs();

        /* free the previous line's memory buffer */
        GlobalFree((HGLOBAL)txtp);
        txtp = 0;
    }
    
    /* free the memory buffer */
    GlobalFree((HGLOBAL)txtp);
}


/* ------------------------------------------------------------------------ */
/*
 *   Window class for the "about this game" dialog 
 */

/* dimensions of the "OK" button in the dialog */
const int okbtn_wid = 70;
const int okbtn_ht = 20;

void CHtmlSys_aboutgamewin::do_create()
{
    RECT rc;

    /* inherit default */
    CTadsWin::do_create();

    /* create the dismiss button */
    GetClientRect(handle_, &rc);
    okbtn_ = CreateWindow("BUTTON", "OK",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          (rc.right - okbtn_wid)/2,
                          rc.bottom - okbtn_ht - 2,
                          okbtn_wid, okbtn_ht, handle_, (HMENU)IDOK,
                          CTadsApp::get_app()->get_instance(), 0);

    /* set the button to use the default dialog font */
    PostMessage(okbtn_, WM_SETFONT,
                (WPARAM)(HFONT)GetStockObject(DEFAULT_GUI_FONT),
                MAKELPARAM(TRUE, 0));

    /* no subpanel yet */
    html_subwin_ = 0;
}

void CHtmlSys_aboutgamewin::do_destroy()
{
    /* notify the subwindow that I'm being destroyed */
    if (html_subwin_ != 0)
    {
        html_subwin_->on_destroy_parent();
        html_subwin_->forget_owner();
    }

    /* forget the subwindow */
    html_subwin_ = 0;
    
    /* inherit default */
    CTadsWin::do_destroy();
}

/*
 *   Create the panel subwindow 
 */
void CHtmlSys_aboutgamewin::create_html_subwin(CHtmlFormatter *formatter)
{
    RECT rc;

    /* if we already have a subpanel window, destroy it */
    if (html_subwin_ != 0)
    {
        html_subwin_->on_destroy_parent();
        html_subwin_->forget_owner();
        DestroyWindow(html_subwin_->get_handle());
    }
    
    /* create the HTML subpanel */
    html_subwin_ = new CHtmlSysWin_win32(formatter, this,
                                         FALSE, FALSE, prefs_);

    /* create the panel's system window */
    GetClientRect(handle_, &rc);
    rc.bottom -= okbtn_ht + 2;
    html_subwin_->create_system_window(this, TRUE, "", &rc);
}

/*
 *   Delete the subwindow 
 */
void CHtmlSys_aboutgamewin::delete_html_subwin()
{
    /* if we have a subpanel window, destroy it */
    if (html_subwin_ != 0)
    {
        /* notify it of our destruction */
        html_subwin_->on_destroy_parent();
        html_subwin_->forget_owner();

        /* destroy it */
        DestroyWindow(html_subwin_->get_handle());

        /* forget it */
        html_subwin_ = 0;
    }
}


/*
 *   Run the dialog 
 */
void CHtmlSys_aboutgamewin::run_aboutbox(CTadsWin *owner)
{
    void *ctx;
    RECT drc;
    RECT rc;
    RECT wrc;
    int wid, ht;
    CHtmlRect text_area;

    /* 
     *   disable windows owned by the main window while the dialog is
     *   running 
     */
    ctx = CTadsDialog::modal_dlg_pre(owner->get_handle(), TRUE);

    /* get my size */
    GetClientRect(handle_, &rc);
    wid = rc.right - rc.left;
    ht = rc.bottom - rc.top;

    /* 
     *   increase the window's height if necessary to make room for its
     *   contents 
     */
    if (ht < (int)html_subwin_->get_formatter()->get_max_y_pos() + 30)
        ht = (int)html_subwin_->get_formatter()->get_max_y_pos() + 30;

    /* adjust the height and width to include the non-client areas */
    GetWindowRect(handle_, &wrc);
    wid += (wrc.right - wrc.left) - (rc.right - rc.left);
    ht += (wrc.bottom - wrc.top) - (rc.bottom - rc.top);

    /* center my window on the screen */
    GetWindowRect(GetDesktopWindow(), &drc);
    MoveWindow(handle_, drc.left + (drc.right - drc.left - wid)/2,
               drc.top + (drc.bottom - drc.top - ht)/2, wid, ht, TRUE);

    /* 
     *   show the window and enable it - it will have been disabled by the
     *   modal_dlg_pre setup above, so we need to re-enable it now 
     */
    ShowWindow(handle_, SW_SHOW);
    EnableWindow(handle_, TRUE);

    /* enter a recursive event loop until the window is closed */
    AddRef();
    if (!CTadsApp::get_app()->event_loop(&closing_))
    {
        /* 
         *   terminating - post another quit message to the enclosing
         *   event loop 
         */
        PostQuitMessage(0);
    }

    /* re-enable the windows we disabled before running the dialog */
    CTadsDialog::modal_dlg_post(ctx);

    /* hide myself again */
    ShowWindow(handle_, SW_HIDE);

    /* release our self-reference */
    Release();
}

/*
 *   close the window - we won't do anything except set the "closing"
 *   flag, which will dismiss the window; we won't allow the window to
 *   actually close at this point 
 */
int CHtmlSys_aboutgamewin::do_close()
{
    /* set the flag to indicate the dialog has been dismissed */
    closing_ = TRUE;

    /* don't really close yet */
    return FALSE;
}

/*
 *   resize the window - move all of the controls to compensate 
 */
void CHtmlSys_aboutgamewin::do_resize(int mode, int x, int y)
{
    switch(mode)
    {
    case SIZE_MAXHIDE:
    case SIZE_MAXSHOW:
    case SIZE_MINIMIZED:
        /* don't bother resizing the subwindows on these changes */
        break;

    case SIZE_MAXIMIZED:
    case SIZE_RESTORED:
    default:
        {
            RECT rc;
            
            /* inherit default handling */
            CTadsWin::do_resize(mode, x, y);
            
            /* move the "OK" button */
            GetClientRect(handle_, &rc);
            MoveWindow(okbtn_, (rc.right - okbtn_wid) / 2,
                       rc.bottom - okbtn_ht - 2,
                       okbtn_wid, okbtn_ht, TRUE);

            /* move the main panel */
            MoveWindow(html_subwin_->get_handle(),
                       rc.left, rc.top, rc.right,
                       rc.bottom - okbtn_ht - 4, TRUE);
        }
    }
}

/*
 *   handle a keystroke 
 */
int CHtmlSys_aboutgamewin::do_char(TCHAR ch, long /*keydata*/)
{
    switch(ch)
    {
    case 10:
    case 13:
    case 27:
        /* close on return/enter/escape */
        closing_ = TRUE;
        return TRUE;

    default:
        /* eat other keys */
        return TRUE;
    }
}

/*
 *   handle a command 
 */
int CHtmlSys_aboutgamewin::do_command(int notify_code, int cmd, HWND ctl)
{
    /* if it's the "OK" button, set the closing flag */
    if (ctl == okbtn_)
    {
        closing_ = TRUE;
        return TRUE;
    }

    /* ignore other commands */
    return FALSE;
}

/*
 *   erase the background 
 */
void CHtmlSys_aboutgamewin::do_paint_content(HDC hdc, const RECT *paintrc)
{
    /* fill the background with gray */
    FillRect(hdc, paintrc, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
}

/* ------------------------------------------------------------------------ */
/*
 *   Window class for top-level dialog/popup menu windows 
 */

/*
 *   creation 
 */
CHtmlSys_top_win::CHtmlSys_top_win(CHtmlSys_mainwin *mainwin)
{
    /* remember our main window */
    mainwin_ = mainwin;
    
    /* we don't have an HTML subwindow yet */
    html_subwin_ = 0;

    /* create our formatter and parser */
    parser_ = new CHtmlParser();
    formatter_ = new CHtmlFormatterStandalone(parser_);

    /* we're not closing yet */
    closing_ = FALSE;

    /* we're not ready to close yet */
    ending_ = FALSE;
}

/*
 *   deletion 
 */
CHtmlSys_top_win::~CHtmlSys_top_win()
{
    /* delete our parser and formatter */
    formatter_->release_parser();
    delete parser_;
    delete formatter_;
}

/*
 *   process window creation 
 */
void CHtmlSys_top_win::do_create()
{
    RECT rc;
    CHtmlTextBuffer txtbuf;
    HMENU menu;
    
    /* inherit default */
    CTadsWin::do_create();

    /* create our HTML subwindow */
    create_html_subwin();

    /* give the subwindow the entire area, minus the scrollbars if any */
    GetClientRect(handle_, &rc);
    if (panel_has_vscroll())
        rc.right -= GetSystemMetrics(SM_CXVSCROLL);
    if (panel_has_hscroll())
        rc.bottom -= GetSystemMetrics(SM_CYHSCROLL);

    /* create the system window for the HTML panel */
    html_subwin_->create_system_window(this, TRUE, "", &rc);

    /* build, parse, and format our contents */
    build_contents(&txtbuf);
    parser_->parse(&txtbuf, mainwin_);
    html_subwin_->do_reformat(FALSE, FALSE, FALSE);

    /* scroll the window to the top if the panel scrolls at all */
    if (panel_has_vscroll())
        html_subwin_->do_scroll(TRUE, html_subwin_->get_vscroll_handle(),
                                SB_TOP, 0, FALSE);

    /* 
     *   delete the Size, Maximize, Minimize, and Restore items from the
     *   system menu 
     */
    menu = GetSystemMenu(handle_, FALSE);
    DeleteMenu(menu, SC_SIZE, MF_BYCOMMAND);
    DeleteMenu(menu, SC_RESTORE, MF_BYCOMMAND);
    DeleteMenu(menu, SC_MINIMIZE, MF_BYCOMMAND);
    DeleteMenu(menu, SC_MAXIMIZE, MF_BYCOMMAND);
}

/* create our HTML subwindow */
void CHtmlSys_top_win::create_html_subwin()
{
    /* create a basic HTML panel window */
    html_subwin_ = new CHtmlSysWin_win32(formatter_, this,
                                         panel_has_vscroll(),
                                         panel_has_hscroll(),
                                         mainwin_->get_prefs());
}

/*
 *   process window destruction 
 */
void CHtmlSys_top_win::do_destroy()
{
    /* notify the subwindow that I'm being destroyed */
    if (html_subwin_ != 0)
    {
        /* notify the subwindow of our deletion */
        html_subwin_->on_destroy_parent();
        html_subwin_->forget_owner();

        /* 
         *   explicitly destroy the subwindow, to ensure it doesn't outlive
         *   its parser and formatter 
         */
        DestroyWindow(html_subwin_->get_handle());

        /* forget the subwindow */
        html_subwin_ = 0;
    }

    /* inherit default */
    CTadsWin::do_destroy();
}

/*
 *   resize the window - move all of the controls to compensate 
 */
void CHtmlSys_top_win::do_resize(int mode, int x, int y)
{
    RECT rc;

    /* check what we're doing */
    switch(mode)
    {
    case SIZE_MAXHIDE:
    case SIZE_MAXSHOW:
    case SIZE_MINIMIZED:
        /* don't bother resizing the subwindows on these changes */
        break;

    case SIZE_MAXIMIZED:
    case SIZE_RESTORED:
    default:
        /* inherit default handling */
        CTadsWin::do_resize(mode, x, y);
            
        /* get the client area, and adjust to our desired subwindow size */
        GetClientRect(handle_, &rc);
        adjust_subwin_rect(&rc);

        /* move the subwindow to its new area */
        MoveWindow(html_subwin_->get_handle(),
                   rc.left, rc.top, rc.right, rc.bottom, TRUE);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Window class for the "about HTML TADS" dialog 
 */

/*
 *   handle a keystroke 
 */
int CHtmlSys_abouttadswin::do_char(TCHAR ch, long /*keydata*/)
{
    switch(ch)
    {
    case 27:
        /* close on Escape key */
        closing_ = TRUE;
        return TRUE;

    default:
        /* ignore other keys */
        return TRUE;
    }
}

/*
 *   Run the dialog modally, then delete myself when done.  
 */
void CHtmlSys_abouttadswin::run_dlg(CTadsWin *parent)
{
    void *ctx;
    RECT drc;
    RECT rc;
    int wid, ht;

    /* 
     *   disable existing windows owned by the parent window while the
     *   dialog is running 
     */
    ctx = CTadsDialog::modal_dlg_pre(parent->get_handle(), TRUE);

    /* set the width and height */
    get_dlg_size(&wid, &ht);

    /* center the window on the screen */
    GetWindowRect(GetDesktopWindow(), &drc);
    rc.left = (drc.right + drc.left - wid)/2;
    rc.right = wid;
    rc.top = (drc.bottom + drc.top - ht)/2;
    rc.bottom = ht;

    /* create the window */
    create_system_window(parent, TRUE, get_dlg_title(), &rc);

    /* enter a recursive event loop until the window is closed */
    if (!CTadsApp::get_app()->event_loop(&closing_))
    {
        /* 
         *   terminating - post another quit message to the enclosing event
         *   loop 
         */
        PostQuitMessage(0);
    }

    /* re-enable the windows we disabled before running the dialog */
    CTadsDialog::modal_dlg_post(ctx);

    /* 
     *   Mark us as ready to close for real, and post a CLOSE message to
     *   myself.  This deferred processing will ensure that we don't remove
     *   the window from the UI until we've had a chance to process the UI
     *   events relating to re-enabling the parent window, which is
     *   important to ensure that the right window gets activated again once
     *   we're removed.  
     */
    SetFocus(parent->get_handle());
    ending_ = TRUE;
    PostMessage(handle_, WM_CLOSE, 0, 0);
}

/*
 *   Build our contents 
 */
void CHtmlSys_abouttadswin::build_contents(CHtmlTextBuffer *txtbuf)
{
    char buf[256];
    static const char txt1[] =
        "<body background='exe:";
    static const char txt2[] =
        "' link=aqua hlink=red alink=fuchsia>"
        "<table cellspacing=0 cellpadding=0 height='100%' width='100%'>"
        "<tr valign=bottom><td><tab align=right>";
    static const char txt3[] =
        "<br></table>";

    /* write our initial fixed contents to the text buffer */
    txtbuf->append(txt1, sizeof(txt1) - 1);
    txtbuf->append(get_bkg_name(), strlen(get_bkg_name()));
    txtbuf->append(txt2, sizeof(txt2) - 1);

    /* add the version string prefix */
    LoadString(CTadsApp::get_app()->get_instance(),
               IDS_ABOUTBOX_1, buf, sizeof(buf));
    txtbuf->append(buf, strlen(buf));

    /* add the version number string */
    txtbuf->append(w32_version_string, strlen(w32_version_string));

    /* add the Credits/Close buttons */
    LoadString(CTadsApp::get_app()->get_instance(),
               IDS_ABOUTBOX_2, buf, sizeof(buf));
    txtbuf->append(buf, strlen(buf));

    /* add the closing fixed contents */
    txtbuf->append(txt3, sizeof(txt3) - 1);
}

/*
 *   process a command 
 */
void CHtmlSys_abouttadswin::process_command(
    const textchar_t *cmd, size_t cmdlen,
    int append, int enter, int os_cmd_id)
{
    /* check the command string */
    if (strcmp(cmd, "close") == 0)
    {
        /* close - set my closing flag */
        closing_ = TRUE;
    }
    else if (strcmp(cmd, "credits") == 0)
    {
        /* show the "Credits" dialog */
        show_credits_dlg();
    }
    else if (strnicmp(cmd, "http:", 5) == 0)
    {
        /* it's a web link - fire it off in a browser */
        if ((unsigned long)ShellExecute(0, "open", cmd, 0, 0, 0) <= 32)
        {
            char buf[256];

            LoadString(CTadsApp::get_app()->get_instance(),
                       IDS_CANNOT_OPEN_HREF, buf, sizeof(buf));
            MessageBox(handle_, buf, "TADS", MB_OK | MB_ICONEXCLAMATION);
        }
    }
}

/*
 *   show the "Credits" dialog 
 */
void CHtmlSys_abouttadswin::show_credits_dlg()
{
    /* create and run our Credits dialog */
    (new CHtmlSys_creditswin(mainwin_))->run_dlg(this);
}

/* ------------------------------------------------------------------------ */
/*
 *   "Credits" dialog.  This is a sub-dialog of the About box that shows the
 *   full credits for the system.  
 */

/*
 *   Build our contents 
 */
void CHtmlSys_creditswin::build_contents(CHtmlTextBuffer *txtbuf)
{
    static const char txt1[] =
        "<body bgcolor=navy text=white link=aqua alink=red hlink=fuchsia>"
        "<center>"
        "<br><br><br><br>";

    static const char txt2[] =
        "<br><br><hr><br><br>"
        
        "<b>Designed and Programmed by</b><br>"
        "Michael J. Roberts<br><br>"

        "<b>Windows Desktop Icons</b><br>"
        "M. D. Dollahite<br><br>"

        "<b>JPEG Implementation</b><br>"
        "The Independent JPEG Group<br><br>"

        "<b>PNG Implementation</b><br>"
        "<i>The PNG Reference Library</i><br>"
        "by Andreas Eric Dilger, Guy Eric Schalnat, and "
        "other Contributing Authors<br><br>"
        
        "<i>The ZLIB Compression Library</i><br>"
        "by Jean-loup Gailly and Mark Adler<br><br>"
        "<b>MNG Implementation</b><br>"
        "<i>libmng</i><br>"
        "by Gerard Juyn<br><br>"

        "<b>Ogg Vorbis Decoding</b><br>"
        "<i>The libvorbis SDK</i><br>"
        "by Xiphophorus<br><br>"

        "<b>MPEG Audio Implementation</b><br>"
        "<i>The amp MPEG Audio Decoder</i><br>"
        "by Tomislav Uzelac<br><br>"

        "<b>Mr. Roberts' Text Editor</b><br>"
        "<i>Epsilon</i><br>by Lugaru<br><br>"

        "Special thanks to Neil K. Guy, Stephen Granade, Kevin Forchione, "
        "TenthStone, Adam&nbsp;J. Thornton, Suzanne Britton, Iain Merrick, "
        "Andrew Pontious, Dan Shiovitz, Jesse Welton, Frantisek Fuka, "
        "Sean&nbsp;T. Barrett, Phil Lewis, Dan Schmidt, BrenBarn, "
        "Steve Breslin, Michel Nizette, Nikos Chantziaras, "
        "Guilherme De&nbsp;Sousa, Jo&atilde;o Mendes, Eric Eve, "
        "M.&nbsp;D. Dollahite, M.&nbsp;Uli Kusterer, "
        "S&oslash;ren&nbsp;J. L&oslash;vborg, and Marnie Parker "
        "for their advice and help creating this system.  Thanks also to "
        "Andrew Pontious, Iain Merrick, and Chris Nebel for creating the "
        "Macintosh versions of TADS; Stephen Granade for WinTADS; "
        "Andrew Plotkin for MaxTADS; and Nikos Chantziaras for QTads "
        "and FrobTADS.  "
        "The author would like to thank everyone else who has "
        "contributed to TADS with ideas, bug reports, code, and "
        "enthusiasm.<br><br>"

        "<br><br><a href='http://www.tads.org'>www.tads.org</a>"
        "<br><br><br><br>"

        /* copyright-date-string */
        "<b>Copyright &copy; 1997, 2005 by Michael J. Roberts</b><br>"
        "This software is copyrighted &ldquo;freeware,&rdquo which means "
        "that you can use and distribute it without charge, but subject "
        "to certain restrictions.  Please refer to the accompanying "
        "license text file for details.<br><br>"

        "<br><font size=-1>"
        "Portions of this software are based on the work of the "
        "Independent JPEG Group.<br><br>"

        "This software incorporates the PNG Reference Library, developed by "
        "Andreas Eric Dilger, Guy Eric Schalnat, and other Contributing " 
        "Authors.<br><br>"

        "This software includes the ZLIB Compression Library, written by "
        "Jean-loup Gailly and Mark Adler.<br><br>"

        "This software includes libmng, the reference MNG library, "
        "by Gerard Juyn.<br><br>"

        "This software includes the libvorbis Ogg Vorbis decoder, "
        "copyright 2001, Xiphophorus.<br><br>"

        "This software includes portions of the amp MPEG audio decoder, "
        "copyright 1996, 1997 by Tomislav Uzelac."
        "</font><br><br>"

        "<br>Coded entirely on location in Palo Alto, "
        "California, USA<br><br><br><br>"

        "<font face=Arial size=-1><b>"
        "<a forced href='close'>Close</a></b></font><br>"
        
        "</center>";

    /* write our contents to the text buffer */
    txtbuf->append(txt1, sizeof(txt1) - 1);
    txtbuf->append(get_prog_title(), strlen(get_prog_title()));
    txtbuf->append(txt2, sizeof(txt2) - 1);
}


/* ------------------------------------------------------------------------ */
/*
 *   Special system window class for pop-up menus.  This is almost identical
 *   to CTadsSyswin; the only difference is that we add the "drop-shadow" and
 *   "save-bits" styles to the OS window class, so that we look more like a
 *   standard menu window on XP.  Note that the drop-shadow style only works
 *   on XP, but only XP has the drop-shadow in its system menus, so
 *   everything ends up looking exactly like it should on each Windows
 *   version.  
 */
class CTadsSyswinMenu: public CTadsSyswin
{
public:
    CTadsSyswinMenu(class CTadsWin *win)
        : CTadsSyswin(win)
    {
        /* if we've never registered our system window class, do so now */
        if (!registered_class_)
        {
            register_class();
            registered_class_ = TRUE;
        }
    }

    /* the older MS headers don't define CS_DROPSHADOW, so just in case... */
#ifndef CS_DROPSHADOW
#define CS_DROPSHADOW  0x00020000
#endif

    /* register my window class */
    static void register_class()
    {
        WNDCLASS wc;
        CTadsApp *app = CTadsApp::get_app();

        /* 
         *   We're an almost trivial subclass of CTadsSyswin, so use its
         *   class definition as our template. 
         */
        GetClassInfo(app->get_instance(), CTadsWin::win_class_name, &wc);

        /* add the drop-shadow and save-bits class styles */
        wc.style |= CS_SAVEBITS | CS_DROPSHADOW;

        /* set up our own class name */
        wc.lpszClassName = menu_win_class_name;
        RegisterClass(&wc);
    }

    /* create the system window object */
    HWND syswin_create_system_window(DWORD ex_style,
                                     const textchar_t *wintitle,
                                     DWORD style,
                                     int x, int y, int wid, int ht,
                                     HWND parent, HMENU menu,
                                     HINSTANCE inst, void *param)
    {
        return CreateWindowEx(ex_style, menu_win_class_name,
                              wintitle, style,
                              x, y, wid, ht, parent, menu, inst, param);
    }

    /* 
     *   Flag: we've registered our system window class.  This is static,
     *   since we only need to register once in the course of an application
     *   session.  
     */
    static int registered_class_;

    /* our system window class name */
    static char menu_win_class_name[];
};

/* declare storage for the static class variables */
int CTadsSyswinMenu::registered_class_ = FALSE;
char CTadsSyswinMenu::menu_win_class_name[] = "HTMLTADS_PopupMenu";

/* ------------------------------------------------------------------------ */
/*
 *   Create a popup menu window.  
 */
int os_show_popup_menu(int default_pos, int x, int y,
                   const char *txt, size_t txtlen, os_event_info_t *evt)
{
    CHtmlSys_popup_menu_win *win;
    RECT rc;
    CHtmlTextBuffer txtbuf;

    /* flush any pending output in the main window before we proceed */
    os_flush();

    /*
     *   Create a new top-level window, but don't show it yet - we need to
     *   figure out the size based on the contents.  
     */
    win = new CHtmlSys_popup_menu_win(CHtmlSys_mainwin::get_main_win(),
                                      txt, txtlen);

    /* create the system window */
    SetRect(&rc, 0, 0, 1000, 100);
    win->create_system_window(CHtmlSys_mainwin::get_main_win(),
                              FALSE, "", &rc, new CTadsSyswinMenu(win));

    /* size to the window's contents */
    win->set_pos_and_size(default_pos, x, y);

    /* track the mouse in the pop-up window, and return the result */
    return ((CHtmlSysWin_win32_Popup *)win->get_html_subwin())
        ->track_as_popup_menu(evt);
}


/* ------------------------------------------------------------------------ */
/*
 *   Popup menu window 
 */

/* create our HTML subwindow */
void CHtmlSys_popup_menu_win::create_html_subwin()
{
    /* create a basic HTML panel window */
    html_subwin_ = new CHtmlSysWin_win32_Popup(formatter_, this,
                                               panel_has_vscroll(),
                                               panel_has_hscroll(),
                                               mainwin_->get_prefs());
}

/*
 *   Build our contents 
 */
void CHtmlSys_popup_menu_win::build_contents(CHtmlTextBuffer *txtbuf)
{
    /* add our text to the buffer */
    txtbuf->append(txt_, txtlen_);

    /* 
     *   While we're here, set our formatter's physical margins to one pixel.
     *   This will give us enough space to draw our border inside the window,
     *   but will otherwise give all the interior space to the HTML content.
     */
    formatter_->set_phys_margins(1, 1, 1, 1);
}

/*
 *   Paint our contents 
 */
void CHtmlSys_popup_menu_win::do_paint_content(HDC win_dc, const RECT *rc)
{
    /* do the normal drawing */
    CHtmlSys_top_win::do_paint_content(win_dc, rc);

    /* draw a one-pixel border */
    FrameRect(win_dc, rc, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
}

/*
 *   Set the position and size of the menu.  We'll set the size based on the
 *   contents of the window.  We'll set the position to the requested
 *   position, or, if the default position is requested, we'll set the upper
 *   left corner to the current mouse position.  
 */
void CHtmlSys_popup_menu_win::set_pos_and_size(int default_pos, int x, int y)
{
    RECT drc;
    int wid, ht;

    /* get the overall desktop window size */
    GetWindowRect(GetDesktopWindow(), &drc);

    /* get the size of our contents */
    wid = formatter_->get_max_line_width();
    ht = formatter_->get_layout_max_y_pos();

    /* adjust each dimension to leave space for our border */
    wid += 3;
    ht += 3;

    /* limit the size to the desktop size */
    if (wid > drc.right - drc.left)
        wid = drc.right - drc.left;
    if (ht > drc.bottom - drc.top)
        ht = drc.bottom - drc.top;

    /* if we need a default, use the current mouse position */
    if (default_pos)
    {
        POINT pt;

        GetCursorPos(&pt);
        x = pt.x;
        y = pt.y;
    }

    /* 
     *   if the bottom of the window would be off the bottom of the desktop
     *   window area, move up; likewise, if the right edge would be off the
     *   screen horizontally, move left 
     */
    if (y + ht > drc.bottom)
        y = drc.bottom - ht;
    if (x + wid > drc.right)
        x = drc.right - wid;

    /* move the window to the calculated position, and set the size */
    MoveWindow(handle_, x, y, wid, ht, FALSE);
    ShowWindow(handle_, SW_SHOWNA);
}

/* ------------------------------------------------------------------------ */
/*
 *   Popup menu subwindow 
 */

/*
 *   Track the mouse in this window using pop-up menu semantics.  Returns an
 *   OSPOP_xxx code to indicate what happened.  
 */
int CHtmlSysWin_win32_Popup::track_as_popup_menu(os_event_info_t *evt)
{
    CTadsApp *app = CTadsApp::get_app();
    int loop_ret;
    int ret;

    /* keep a self-reference until we're done processing events */
    AddRef();

    /* wait for a popup menu event in *evt */
    evtp_ = evt;

    /* capture the mouse while we're working */
    SetCapture(handle_);

    /* install our message filter while we're working */
    old_filter_ = app->set_msg_filter(this);

    /* process events until we dismiss the menu */
    dismissed_ = FALSE;
    loop_ret = app->event_loop(&dismissed_);

    /* 
     *   If we clicked on a link, we've made a menu selection, so we'll want
     *   to return true.  Otherwise, we must have canceled the menu without
     *   making a selection, so return false.  
     */
    ret = (clicked_link_ ? OSPOP_HREF : OSPOP_CANCEL);

    /* uninstall our message filter */
    app->set_msg_filter(old_filter_);

    /* release our self-reference */
    Release();

    /* if we're quitting, give up now */
    if (loop_ret == 0)
    {
        /* post a Quit message to any enclosing event loop, and abort */
        PostQuitMessage(0);
        return OSPOP_EOF;
    }

    /* we're now dismissed, so close our parent window */
    DestroyWindow(GetParent(handle_));
    
    /* return our menu selection indication */
    return ret;
}

/* 
 *   key press
 */
int CHtmlSysWin_win32_Popup::do_keydown(int vkey, long /*keydata*/)
{
    /* check what we have */
    switch (vkey)
    {
    case VK_ESCAPE:
    case VK_BACK:
        /* 
         *   on Escape or Backspace, cancel the menu - releasing mouse
         *   capture will set things into motion for us 
         */
        if (GetCapture() == handle_)
            ReleaseCapture();
        break;
    }

    /* handled */
    return TRUE;
}

/*
 *   left mouse button click 
 */
int CHtmlSysWin_win32_Popup::do_leftbtn_down(
    int /*keys*/, int x, int y, int /*clicks*/)
{
    CHtmlPoint pos;
    CHtmlDisp *disp;
    CHtmlDispLink *link;

    /* if the click is outside of our window, it's a summary dismissal */
    if (dismiss_on_click(x, y, MOUSEEVENTF_LEFTDOWN))
        return TRUE;

    /* get the position in document coordinates */
    pos = screen_to_doc(CHtmlPoint(x, y));

    /* get the display item containing the position */
    disp = formatter_->find_by_pos(pos, TRUE);

    /* get the link from the display item, if there is one */
    link = find_link_for_disp(disp, &pos);

    /* 
     *   If we found a link, click it.  In a popup menu window, a click takes
     *   effect immediately on mouse down - we don't wait for the button to
     *   be released, as we do in normal windows, since this is a menu. 
     */
    if (link != 0)
    {
        const textchar_t *cmd;
        size_t copylen;

        /* get the HREF text */
        cmd = link->href_.get_url();

        /* limit the length of our copy to the available space */
        copylen = strlen(cmd);
        if (copylen > sizeof(evtp_->href) - sizeof(textchar_t))
            copylen = sizeof(evtp_->href) - sizeof(textchar_t);

        /* copy the HREF */
        memcpy(evtp_->href, cmd, copylen);

        /* fill in a null terminator */
        evtp_->href[copylen / sizeof(textchar_t)] = '\0';

        /* note that we clicked a link */
        clicked_link_ = TRUE;

        /* release mouse capture - this will dismiss the menu */
        if (GetCapture() == handle_)
            ReleaseCapture();
    }

    /* handled */
    return TRUE;
}

/*
 *   right mouse button click 
 */
int CHtmlSysWin_win32_Popup::do_rightbtn_down(
    int keys, int x, int y, int clicks)
{
    /* if the click is outside of our window, it's a summary dismissal */
    dismiss_on_click(x, y, MOUSEEVENTF_RIGHTDOWN);

    /* 
     *   That's all we need to do.  If the event was in our window, we'll
     *   just ignore it, as a right-click in our pop-up menu has no effect. 
     */
    return TRUE;
}

/*
 *   Terminate mouse tracking 
 */
int CHtmlSysWin_win32_Popup::end_mouse_tracking(html_track_mode_t mode)
{
    /* 
     *   When we lose capture, cancel the menu without a selection.  Simply
     *   mark the event loop as done with no event.  
     */
    dismissed_ = TRUE;

    /* inherit the default handling */
    return CHtmlSysWin_win32::end_mouse_tracking(mode);
}

/*
 *   Handle mouse-move events 
 */
int CHtmlSysWin_win32_Popup::do_mousemove(int keys, int x, int y)
{
    POINT pt;

    /* convert the mouse click's client coordinates to screen coordinates */
    pt.x = x;
    pt.y = y;
    ClientToScreen(handle_, &pt);

    /* 
     *   if the mouse is over our window, handle this with the normal
     *   cursor-sensing code 
     */
    if (WindowFromPoint(pt) == handle_)
    {
        /* 
         *   It's our window, so handle this as a set-cursor operation.
         *   Windows doesn't send set-cursor events when we're in
         *   mouse-capture mode (which we always are when we're showing a
         *   pop-up menu window), so we need to do this explicitly. 
         */
        if (!do_setcursor(handle_, 0, WM_MOUSEMOVE))
            SetCursor(arrow_cursor_);

        /* handled */
        return TRUE;
    }
    else
    {
        /* it's not over our window, so just set a standard arrow cursor */
        SetCursor(arrow_cursor_);

        /* we're not hovering over any link */
        set_hover_link(0);

        /* handled */
        return TRUE;
    }
}

/*
 *   Set the cursor based on the display item the mouse is positioned upon.
 */
void CHtmlSysWin_win32_Popup::set_disp_item_cursor(
    CHtmlDisp *disp, int docx, int docy)
{
    /* check what type of cursor the display item wants */
    switch (disp->get_cursor_type(this, formatter_, docx, docy, FALSE))
    {
    case HTML_CSRTYPE_HAND:
        /* set the hand cursor */
        SetCursor(hand_csr_);
        break;

    default:
        /* 
         *   Use default cursor.  Note that we use this even for text items,
         *   which would normally use the I-beam cursor - we can't select
         *   text in this kind of window, so we don't want an I-beam. 
         */
        SetCursor(arrow_cursor_);
        break;
    }
}

/*
 *   Filter a system message.  We install ourselves as a message filter while
 *   we have capture, so that we can grab any keyboard messages before they
 *   go to our parent window.  The parent window would otherwise get the
 *   keyboard events, since we leave it as the active window (and thus leave
 *   it with keyboard focus) while we're running the menu.  
 */
int CHtmlSysWin_win32_Popup::filter_system_message(MSG *msg)
{
    /* check what we have */
    switch (msg->message)
    {
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_CHAR:
    case WM_DEADCHAR:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_SYSCHAR:
    case WM_SYSDEADCHAR:
        /* make sure keyboard events go to us, not the real focus window */
        msg->hwnd = handle_;
        break;
    }

    /* 
     *   If we get this far, it means that we didn't want to intercept the
     *   message, so pass it on to any previously-installed filter.  If there
     *   aren't any other filters, let the event loop apply the normal
     *   handling 
     */
    return (old_filter_ != 0
            ? old_filter_->filter_system_message(msg)
            : FALSE);
}


/*
 *   Check a click to see if it should result in summary dismissal.  If we
 *   have a click outside of our window, it simply dismisses the window (by
 *   releasing capture), and re-sends the click to the other window to which
 *   it should properly belong.  We have to re-send the click because capture
 *   sent the click to us rather than the window which contains the mouse. 
 */
int CHtmlSysWin_win32_Popup::dismiss_on_click(int x, int y, DWORD flags)
{
    POINT pt;

    /* convert the mouse click's client coordinates to screen coordinates */
    pt.x = x;
    pt.y = y;
    ClientToScreen(handle_, &pt);

    /* if the click is outside of our window, it's a summary dismissal */
    if (WindowFromPoint(pt) == handle_)
    {
        /* it's in our window, so we can proceed - it's not a dismissal */
        return FALSE;
    }
    else
    {
        /* 
         *   It's outside of our window, so it's a summary dismissal.
         *   Release mouse capture, which will initiate the dismissal. 
         */
        if (GetCapture() == handle_)
            ReleaseCapture();

        /* re-send the click now that we've released capture */
        mouse_event(flags, 0, 0, 0, 0);
        
        /* tell the caller this was a dismissal */
        return TRUE;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   DirectX warning dialog 
 */

void CTadsDlg_dxwarn::init()
{
    int ck;
    
    /* inherit default */
    CTadsDialog::init();

    /*
     *   Determine if the checkbox should be checked - it should be if the
     *   error code is the same as last time, and the "hide this warning"
     *   item was checked last time. 
     */
    ck = (warn_type_ == prefs_->get_directx_error_code()
          && prefs_->get_directx_hidewarn());
    CheckDlgButton(handle_, IDC_CK_SUPPRESS,
                   ck ? BST_CHECKED : BST_UNCHECKED);

    /* 
     *   Show the appropriate message, depending on the type of error that
     *   caused us to display the dialog 
     */
    switch(warn_type_)
    {
    default:
    case HTMLW32_DIRECTX_MISSING:
        ShowWindow(GetDlgItem(handle_, IDC_STATIC_DXINST), SW_SHOW);
        ShowWindow(GetDlgItem(handle_, IDC_STATIC_DXINST2), SW_SHOW);
        ShowWindow(GetDlgItem(handle_, IDC_STATIC_DXINST3), SW_SHOW);
        break;

    case HTMLW32_DIRECTX_BADVSN:
        ShowWindow(GetDlgItem(handle_, IDC_STATIC_DXVSN), SW_SHOW);
        ShowWindow(GetDlgItem(handle_, IDC_STATIC_DXVSN2), SW_SHOW);
        ShowWindow(GetDlgItem(handle_, IDC_STATIC_DXVSN3), SW_SHOW);
        break;
    }
}

/*
 *   process a command 
 */
int CTadsDlg_dxwarn::do_command(WPARAM id, HWND ctl)
{
    int ck;
    
    switch(id)
    {
    case IDC_CK_SUPPRESS:
        /* note the new value */
        ck = (IsDlgButtonChecked(handle_, IDC_CK_SUPPRESS) == BST_CHECKED);
        
        /* save the new setting in the preferences */
        prefs_->set_directx_hidewarn(ck);

        /* handled */
        return TRUE;

    default:
        /* inherit default handling */
        return CTadsDialog::do_command(id, ctl);
    }
}

