/* $Header: d:/cvsroot/tads/html/win32/tadsfont.h,v 1.3 1999/07/11 00:46:48 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsfont.h - TADS font class for 32-bit windows
Function
  
Notes
  
Modified
  09/20/97 MJRoberts  - Creation
*/

#ifndef TADSFONT_H
#define TADSFONT_H

#include <windows.h>

/*
 *   Extended logical font.  We include attributes that we use for
 *   rendering, such as color and superscript, that aren't in a standard
 *   windows LOGFONT structure. 
 */
struct CTadsLOGFONT
{
    /* standard windows logical font structure */
    LOGFONT lf;

    /* flag: face name is set explicitly in LOGFONT */
    int face_set_explicitly : 1;

    /* flag: color is set explicitly in LOGFONT */
    int color_set_explicitly : 1;

    /* 
     *   flag: we have a background color (if not, we draw transparently on
     *   the existing background) 
     */
    int bgcolor_set : 1;

    /* extended attributes */
    int superscript : 1;
    int subscript : 1;
    COLORREF color;
    COLORREF bgcolor;
};

class CTadsFont
{
public:
    CTadsFont(const CTadsLOGFONT *logfont);
    virtual ~CTadsFont();

    /*
     *   select this font into a DC - returns the old font object, which
     *   should be popped with unselect when the caller is done with this
     *   font 
     */
    HGDIOBJ select(HDC dc);

    /* restore the previous font to a DC */
    void unselect(HDC dc, HGDIOBJ oldfont);

    /* check if I exactly match a logical font description */
    int matches(const CTadsLOGFONT *logfont);

    /* calculate a setting for LOGFONT.lfHeight, given a point size */
    static long calc_lfHeight(int pointsize);

    /* calculate the point size for a given pixel height */
    static int calc_pointsize(int pix_height);

    /* determine if a font is present on the system */
    static int font_is_present(const char *fontname, size_t namelen);

    /* get the system font handle */
    HFONT get_handle() const { return handle_; }

protected:
    /* make sure no one uses the default constructor */
    CTadsFont();

    /* make a canonical copy of a LOGFONT structure */
    void copy_canonical_logfont(CTadsLOGFONT *dst, const CTadsLOGFONT *src);
    
    /* my font handle */
    HFONT handle_;

    /* logical font description */
    CTadsLOGFONT logfont_;
};


#endif /* TADSFONT_H */

