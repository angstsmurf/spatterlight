#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadscbtn.cpp,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadscbtn.cpp - color button implementation
Function
  
Notes
  
Modified
  03/14/98 MJRoberts  - Creation
*/

#include <Windows.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif
#ifndef TADSCBTN_H
#include "tadscbtn.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Color button implementation 
 */

/*
 *   process a command in the color button: shows the color picker dialog,
 *   and changes the button's face color if the user selects a new color 
 */
void CColorBtn::do_command()
{
    CHOOSECOLOR cc;

    /* set up the dialog with our current color as the default */
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = GetParent(gethdl());
    cc.hInstance = 0;
    cc.rgbResult = get_color();
    cc.lpCustColors = cust_colors_;
    cc.Flags = CC_RGBINIT | CC_SOLIDCOLOR;
    cc.lCustData = 0;
    cc.lpfnHook = 0;
    cc.lpTemplateName = 0;

    /* show the color dialog */
    if (ChooseColor(&cc))
    {
        /* user selected OK - select the new color */
        set_color(cc.rgbResult);
    }
}

/*
 *   draw a color button 
 */
void CColorBtn::draw(DRAWITEMSTRUCT *dis)
{
    RECT rc;

    /* do the default button drawing first */
    CTadsOwnerDrawnBtn::draw(dis);

    /* if it's not disabled, draw the color */
    if (!(dis->itemState & ODS_DISABLED))
    {
        HBRUSH br = CreateSolidBrush(get_color());

        /* draw the color area; offset it slightly if clicked */
        rc = dis->rcItem;
        InflateRect(&rc, -6, -5);
        if (dis->itemState & ODS_SELECTED)
            OffsetRect(&rc, 1, 1);
        FillRect(dis->hDC, &rc, br);

        /* draw the focus rectangle if we have focus */
        if (dis->itemState & ODS_FOCUS)
        {
            InflateRect(&rc, 1, 1);
            DrawFocusRect(dis->hDC, &rc);
        }

        /* done with the brush */
        DeleteObject(br);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Property page color button implementation 
 */

/*
 *   process the command 
 */
void CColorBtnPropPage::do_command()
{
    HTML_color_t old_color;

    /* note the old color */
    old_color = *color_;

    /* process the command normally */
    CColorBtn::do_command();

    /* if the color changed, notify the property sheet */
    if (*color_ != old_color)
        page_->set_changed(TRUE);
}

