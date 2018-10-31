#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/htmlsys.cpp,v 1.2 1999/05/17 02:52:22 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlsys.cpp - system class definitions - portable interfaces
Function
  
Notes
  
Modified
  09/15/97 MJRoberts  - Creation
*/

#include <string.h>
#include <memory.h>

#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif
#ifndef HTMLFMT_H
#include "htmlfmt.h"
#endif

/* ------------------------------------------------------------------------ */
/*
 *   font description 
 */

/*
 *   copy a font description into this font description 
 */
void CHtmlFontDesc::copy_from(const CHtmlFontDesc *src)
{
    memcpy(this, src, sizeof(*src));

#if 0 // $$$ no longer necessary - just copy the bytes now
    /* copy the members */
    pointsize = src->pointsize;
    weight = src->weight;
    italic = src->italic;
    underline = src->underline;
    strikeout = src->strikeout;
    default_color = src->default_color;
    default_bgcolor = src->default_bgcolor;
    color = src->color;
    bgcolor = src->bgcolor;
    memcpy(face, src->face, sizeof(face));
    fixed_pitch = src->fixed_pitch;
    serif = src->serif;
    htmlsize = src->htmlsize;
    superscript = src->superscript;
    subscript = src->subscript;
    pe_big = src->pe_big;
    pe_small = src->pe_small;
    pe_em = src->pe_em;
    pe_strong = src->pe_strong;
    pe_dfn = src->pe_dfn;
    pe_code = src->pe_code;
    pe_samp = src->pe_samp;
    pe_kbd = src->pe_kbd;
    pe_var = src->pe_var;
    pe_cite = src->pe_cite;
    pe_address = src->pe_address;
    charset = src->charset;
    default_charset = src->default_charset;
#endif
}

