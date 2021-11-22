/* $Header: d:/cvsroot/tads/html/win32/w32font.h,v 1.2 1999/05/17 02:52:26 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32font.h - tads html win32 font implementation
Function
  
Notes
  
Modified
  01/31/98 MJRoberts  - Creation
*/

#ifndef W32FONT_H
#define W32FONT_H

#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif
#ifndef TADSFONT_H
#include "tadsfont.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   System font object 
 */
class CHtmlSysFont_win32: public CTadsFont, public CHtmlSysFont
{
public:
    CHtmlSysFont_win32(const CTadsLOGFONT *lf);
    ~CHtmlSysFont_win32();

    /* get metrics */
    void get_font_metrics(class CHtmlFontMetrics *);

    /* is this a fixed-pitch font? */
    int is_fixed_pitch() { return is_fixed_pitch_; }

    /* get the font's em size */
    int get_em_size() { return em_size_; }

private:
    /* get my Windows-specific metrics */
    void get_win_font_metrics(TEXTMETRIC *tm);

    /* am I a fixed-pitch (monospaced) font? */
    int is_fixed_pitch_;

    /* my em size, in pxels */
    int em_size_;
};


#endif /* W32FONT_H */
