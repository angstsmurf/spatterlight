#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/w32font.cpp,v 1.2 1999/05/17 02:52:26 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32font.cpp - html tads win32 font implementation
Function
  
Notes
  
Modified
  01/31/98 MJRoberts  - Creation
*/

#include <Windows.h>
#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif
#ifndef W32FONT_H
#include "w32font.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Font implementation 
 */

CHtmlSysFont_win32::CHtmlSysFont_win32(const CTadsLOGFONT *lf)
    : CTadsFont(lf)
{
    TEXTMETRIC tm;

    /* get my metrics */
    get_win_font_metrics(&tm);

    /* 
     *   if the FIXED_PITCH flag is set, the font is variable pitch (yes,
     *   someone at MS defined this name to be the opposite of its apparent
     *   meaning) 
     */
    is_fixed_pitch_ = !(tm.tmPitchAndFamily & TMPF_FIXED_PITCH);

    /* remember the 'em' size as the ascender height of the font */
    em_size_ = tm.tmAscent;
}

CHtmlSysFont_win32::~CHtmlSysFont_win32()
{
}

void CHtmlSysFont_win32::get_font_metrics(CHtmlFontMetrics *metrics)
{
    TEXTMETRIC tm;

    /* get my metrics */
    get_win_font_metrics(&tm);
    
    /* return the required information */
    metrics->ascender_height = tm.tmAscent;
    metrics->descender_height = tm.tmDescent;
    metrics->total_height = tm.tmHeight;
}

/*
 *   Get my Windows font metrics 
 */
void CHtmlSysFont_win32::get_win_font_metrics(TEXTMETRIC *tm)
{
    HDC dc;
    HGDIOBJ oldfont;

    /* select the font into the display */
    dc = GetDC(GetDesktopWindow());
    oldfont = (HFONT)SelectObject(dc, handle_);

    /* get the metrics for the font */
    GetTextMetrics(dc, tm);

    /* done with the font and the desktop device context */
    SelectObject(dc, oldfont);
    ReleaseDC(GetDesktopWindow(), dc);
}

