#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadsfont.cpp,v 1.3 1999/07/11 00:46:48 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsfont.cpp - TADS font implementation for Win 32
Function
  
Notes
  
Modified
  09/20/97 MJRoberts  - Creation
*/

#include <Windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

#ifndef TADSFONT_H
#include "tadsfont.h"
#endif

/*
 *   Create a TADS font object for a given logical font description
 */
CTadsFont::CTadsFont(const CTadsLOGFONT *logfont)
{
    /* store a canonical copy of the LOGFONT for later comparison */
    copy_canonical_logfont(&logfont_, logfont);
    
    /* create the system font */
    handle_ = CreateFontIndirect(&logfont->lf);
}

CTadsFont::~CTadsFont()
{
    /* we're done with the font */
    if (handle_ != 0)
    {
        DeleteObject(handle_);
        handle_ = 0;
    }
}

/*
 *   select the font into a device context, returning the previously
 *   selected font's handle 
 */
HGDIOBJ CTadsFont::select(HDC dc)
{
    return SelectObject(dc, handle_);
}

/*
 *   restore a font that was in effect before selecting this font 
 */
void CTadsFont::unselect(HDC dc, HGDIOBJ oldfont)
{
    SelectObject(dc, oldfont);
}


/*
 *   Calculate a setting for LOGFONT.lfHeight, given a point size 
 */
long CTadsFont::calc_lfHeight(int pointsize)
{
    HDC deskdc;
    long sz;
    long pixperinch;
    
    /* get the system vertical pixels per inch for the desktop window */
    deskdc = GetDC(GetDesktopWindow());
    pixperinch = GetDeviceCaps(deskdc, LOGPIXELSY);
    ReleaseDC(GetDesktopWindow(), deskdc);

    /* one inch is 72 points - calculate how many pixels that is */
    sz = (pointsize * pixperinch)/72;

    /* return a negative value to tell Windows to use character size */
    return -sz;
}

/*
 *   Calculate a point size for a given pixel height 
 */
int CTadsFont::calc_pointsize(int pix_height)
{
    HDC deskdc;
    long pixperinch;

    /* get the system vertical pixels per inch for the desktop window */
    deskdc = GetDC(GetDesktopWindow());
    pixperinch = GetDeviceCaps(deskdc, LOGPIXELSY);
    ReleaseDC(GetDesktopWindow(), deskdc);

    /* 
     *   there are 72 points in an inch, so calculate our pixel height in
     *   inches and multiply the result by 72 
     */
    return (int)(((long)pix_height * 72L) / pixperinch);
}

/*
 *   check if we match a logical font 
 */
int CTadsFont::matches(const CTadsLOGFONT *lf)
{
    CTadsLOGFONT canon_lf;

    /*
     *   make a private copy in canonical form, with the lfFaceName array
     *   set to nulls after the terminator 
     */
    copy_canonical_logfont(&canon_lf, lf);
    
    /* if we match the LOGFONT structure exactly, it's a match */
    return memcmp(&canon_lf, &logfont_, sizeof(logfont_)) == 0;
}

/*
 *   Enumeration procedure for font_is_present 
 */
class enum_proc_ctx_t
{
public:
    enum_proc_ctx_t() { count = 0; }
    
    /* count of number of fonts of the family that were enumerated */
    int count;
};

static int CALLBACK font_enum_proc(ENUMLOGFONTEX *, NEWTEXTMETRIC *,
                                   DWORD, LPARAM lpar)
{
    enum_proc_ctx_t *ctx = (enum_proc_ctx_t *)lpar;

    /* increase the count of fonts found */
    ctx->count++;

    /* return non-zero to continue enumeration */
    return 1;
}
   
/*
 *   Determine if a font is present on the system. 
 */
int CTadsFont::font_is_present(const char *fontname, size_t len)
{
    HDC deskdc;
    enum_proc_ctx_t enum_proc_ctx;
    LOGFONT lf;

    /* make a null-terminated version of the font name */
    if (len > sizeof(lf.lfFaceName) - 1)
        len = sizeof(lf.lfFaceName) - 1;
    memcpy(lf.lfFaceName, fontname, len);
    lf.lfFaceName[len] = '\0';

    /* get the desktop window device context */
    deskdc = GetDC(GetDesktopWindow());

    /* set up the LOGFONT to describe the font families were interested in */
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfPitchAndFamily = 0;

    /* enumerate fonts matching the given name */
    EnumFontFamiliesEx(deskdc, &lf, (FONTENUMPROC)font_enum_proc,
                       (LPARAM)&enum_proc_ctx, 0);

    /* done with the desktop dc */
    ReleaseDC(GetDesktopWindow(), deskdc);

    /* if we enumerated any fonts, this face name exists */
    return enum_proc_ctx.count != 0;
}

/*
 *   Make a canonical copy of a LOGFONT structure for comparison purposes
 */
void CTadsFont::copy_canonical_logfont(CTadsLOGFONT *dst,
                                       const CTadsLOGFONT *src)
{
    char *p;
    int rem;

    /* store a copy of the logfont for later comparison */
    memcpy(dst, src, sizeof(*dst));

    /*
     *   Make sure the portion of lfFaceName after the null terminator is
     *   all nulls for easy comparison later.  First, find the null
     *   terminator in the existing name array.  
     */
    for (p = dst->lf.lfFaceName, rem = sizeof(dst->lf.lfFaceName) ;
         *p != 0 && rem > 0 ; ++p, --rem);

    /* now zero out all the bytes from there to the end of the array */
    for ( ; rem > 0 ; ++p, --rem)
        *p = '\0';
}
