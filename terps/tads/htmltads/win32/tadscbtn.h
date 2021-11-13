/* $Header: d:/cvsroot/tads/html/win32/tadscbtn.h,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadscbtn.h - color selector buttons
Function
  
Notes
  
Modified
  03/14/98 MJRoberts  - Creation
*/

#ifndef TADSCBTN_H
#define TADSCBTN_H

#include <Windows.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif
#ifndef TADSWIN_H
#include "tadswin.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Color selector button.  This is a specialization of the owner-drawn
 *   button class that displays a color on its face, and lets the user
 *   select a color (via the standard color picker dialog) when the button
 *   is clicked.  
 */

class CColorBtn: public CTadsOwnerDrawnBtn
{
public:
    CColorBtn(HWND hdl, HTML_color_t *color, COLORREF *cust_colors)
        : CTadsOwnerDrawnBtn(hdl)
        { color_ = color; cust_colors_  = cust_colors; }
    CColorBtn(HWND dlg, int id, HTML_color_t *color, COLORREF *cust_colors)
        : CTadsOwnerDrawnBtn(dlg, id)
        { color_ = color; cust_colors_ = cust_colors;}

    virtual void draw(DRAWITEMSTRUCT *dis);
    virtual void do_command();

    COLORREF get_color() { return HTML_color_to_COLORREF(*color_); }
    void set_color(COLORREF color)
    {
        *color_ = COLORREF_to_HTML_color(color);
        InvalidateRect(gethdl(), 0, TRUE);
    }

protected:
    HTML_color_t *color_;
    COLORREF *cust_colors_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Specialized version of color button for property sheet page use 
 */

class CColorBtnPropPage: public CColorBtn
{
public:
    CColorBtnPropPage(HWND dlg, int id, HTML_color_t *color,
                      COLORREF *cust_colors, class CTadsDialogPropPage *page)
        : CColorBtn(dlg, id, color, cust_colors) { page_ = page; }

    void do_command();

private:
    /* property sheet page containing the button */
    class CTadsDialogPropPage *page_;
};

#endif /* TADSCBTN_H */
