#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadstab.cpp,v 1.3 1999/07/11 00:46:48 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadstab.cpp - tab control
Function
  
Notes
  
Modified
  03/15/98 MJRoberts  - Creation
*/

#include <Windows.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSWIN_H
#include "tadswin.h"
#endif
#ifndef TADSTAB_H
#include "tadstab.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif
#ifndef HTMLRES_H
#include "htmlres.h"
#endif
#ifndef TADSFONT_H
#include "tadsfont.h"
#endif

/*
 *   width and height of the edge bitmap cells
 */
const int CELL_BMP_WID = 8;
const int CELL_BMP_HT = 14;


CTadsTabctl::CTadsTabctl(int id, CTadsWin *parent, const RECT *init_pos)
{
    LOGBRUSH lbr;
    int pts;

    /* remember my ID */
    id_ = id;
    
    /* no tabs yet */
    first_tab_ = 0;
    sel_tab_ = 0;
    total_width_ = 0;

    /* load the bitmap we use for drawing the tabs */
    tabbmp_ = LoadBitmap(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDB_BMP_TABPIX));

    /* create our brushes */
    lbr.lbStyle = BS_SOLID;
    lbr.lbColor = RGB(192, 192, 192);
    lbr.lbHatch = 0;
    gray_br_ = CreateBrushIndirect(&lbr);
    lbr.lbColor = RGB(255, 255, 255);
    white_br_ = CreateBrushIndirect(&lbr);

    /* remember the parent window */
    parent_window_ = parent->get_handle();

    /* create our system window */
    create_system_window(parent, TRUE, "", init_pos);

    /* 
     *   set my default font to a proportional sans serif font that fits
     *   the height of the tab area 
     */
    pts = CTadsFont::calc_pointsize(CELL_BMP_HT - 2);
    font_ = CTadsApp::get_app()->get_font("MS Sans Serif", pts,
                                          FALSE, FALSE,
                                          GetSysColor(COLOR_BTNTEXT),
                                          FF_SWISS | VARIABLE_PITCH);

    /* register as a drop target */
    drop_target_register();
}

CTadsTabctl::~CTadsTabctl()
{
    CTadsTabctlTab *cur;
    CTadsTabctlTab *nxt;
    
    /* delete the tabs */
    for (cur = first_tab_ ; cur != 0 ; cur = nxt)
    {
        /* 
         *   remember the next tab, since we're about to delete our
         *   pointer to it 
         */
        nxt = cur->next_;

        /* delete the tab */
        delete cur;
    }

    /* done with the bitmap */
    DeleteObject(tabbmp_);

    /* done with our brushes */
    DeleteObject(gray_br_);
    DeleteObject(white_br_);
}

/*
 *   get the height 
 */
int CTadsTabctl::get_height() const
{
    return CELL_BMP_HT;
}

/* 
 *   get the currently selected tab's ID
 */
int CTadsTabctl::get_selected_tab() const
{
    return (sel_tab_ == 0 ? -1 : sel_tab_->id_);
}

/*
 *   Invalidate a tab's screen area 
 */
void CTadsTabctl::inval_tab(CTadsTabctlTab *tab)
{
    RECT rc;

    /* set up a rectangle with the tab's area, including separators */
    SetRect(&rc, tab->left_ - CELL_BMP_WID, 0,
            tab->right_ + CELL_BMP_WID, CELL_BMP_HT);

    /* invalidate it */
    InvalidateRect(handle_, &rc, FALSE);
}

/*
 *   find a tab given a mouse position in window coordinates
 */
CTadsTabctlTab *CTadsTabctl::find_tab(int x, int /*y*/)
{
    CTadsTabctlTab *cur;

    /* figure out which tab the click is in */
    for (cur = first_tab_ ; cur != 0 ; cur = cur->next_)
    {
        /* if it's in this tab, this is the one they want - return it */
        if (x >= cur->left_ - CELL_BMP_WID/2
            && x < cur->right_ + CELL_BMP_WID/2)
            return cur;
    }

    /* didn't find a tab containing the given position */
    return 0;
}

/*
 *   find a tab given a mouse position in screen coordinates 
 */
CTadsTabctlTab *CTadsTabctl::find_tab_screen(int x, int y)
{
    POINT orig;

    /* adjust the position to window coordinates */
    orig.x = orig.y = 0;
    ClientToScreen(handle_, &orig);
    x -= orig.x;
    y -= orig.y;

    /* find the tab normally */
    return find_tab(x, y);
}

/*
 *   Handle a mouse event 
 */
int CTadsTabctl::do_leftbtn_down(int /*keys*/, int x, int y,
                                 int /*clicks*/)
{
    CTadsTabctlTab *cur;

    /* find the tab they clicked on */
    cur = find_tab(x, y);

    /* 
     *   if there's not a tab where they clicked, there's nothing to do,
     *   so return immediately (but indicate that the click was handled,
     *   since we handled it by doing nothing) 
     */
    if (cur == 0)
        return TRUE;

    /* if it's not already selected, select it */
    if (cur != sel_tab_)
        activate_tab(cur);
    
    /* there's nothing more to do */
    return TRUE;
}

/*
 *   Activate a tab 
 */
void CTadsTabctl::activate_tab(CTadsTabctlTab *tab)
{
    NMHDR nmhdr;

    /* update the previous selection */
    if (sel_tab_ != 0)
        inval_tab(sel_tab_);
    
    /* set the new selection */
    sel_tab_ = tab;
    
    /* invalidate it */
    inval_tab(tab);
    
    /* update the window immediately */
    UpdateWindow(handle_);
    
    /* notify the parent window of the change */
    nmhdr.hwndFrom = handle_;
    nmhdr.idFrom = id_;
    nmhdr.code = TADSTABN_SELECT;
    SendMessage(parent_window_, WM_NOTIFY, (WPARAM)id_,
                (LPARAM)&nmhdr);
}

/*
 *   Set up a source rectangle for a particular cell number 
 */
void CTadsTabctl::set_src_rc(RECT *rc, int cellnum)
{
    /* start at cell zero */
    SetRect(rc, 0, 0, CELL_BMP_WID, CELL_BMP_HT);

    /* offset to the appropriate point */
    OffsetRect(rc, cellnum * CELL_BMP_WID, 0);
}

/*
 *   Draw an edge cell into the given rectangle on the screen 
 */
void CTadsTabctl::draw_cell(const RECT *dstrc, int cellnum)
{
    RECT srcrc;

    /* set up with the source of the cell */
    set_src_rc(&srcrc, cellnum);

    /* draw it */
    draw_hbitmap(tabbmp_, dstrc, &srcrc);
}

/*
 *   Paint the window 
 */
void CTadsTabctl::do_paint_content(HDC hdc, const RECT *area_to_draw)
{
    CTadsTabctlTab *cur;
    RECT dstrc;
    HGDIOBJ oldfont;
    int oldbkmode;
    HPEN oldpen;

    /* start off at the left edge for drawing */
    SetRect(&dstrc, 0, 0, CELL_BMP_WID, CELL_BMP_HT);

    /* set the text font and drawing mode */
    oldfont = font_->select(hdc);
    oldbkmode = SetBkMode(hdc, TRANSPARENT);
    oldpen = (HPEN)SelectObject(hdc, (HPEN)GetStockObject(BLACK_PEN));

    /* 
     *   draw the left edge - use cell #0 if the first tab is unselected,
     *   or cell #5 if the first tab is selected 
     */
    draw_cell(&dstrc, sel_tab_ == first_tab_ ? 5 : 0);

    /* draw the tabs */
    for (cur = first_tab_ ; cur != 0 ; cur = cur->next_)
    {
        int cellnum;
        
        /* offset to the start of this tab's body area */
        OffsetRect(&dstrc, CELL_BMP_WID, 0);

        /* extend the destination rectangle to the full width */
        dstrc.right = dstrc.left + cur->wid_;

        /* fill the rectangle with the appropriate color */
        FillRect(hdc, &dstrc, cur == sel_tab_ ? white_br_ : gray_br_);

        /* draw the bottom border */
        MoveToEx(hdc, dstrc.left, dstrc.bottom - 1, 0);
        LineTo(hdc, dstrc.right, dstrc.bottom - 1);

        /* if it's not selected, draw the top border */
        if (cur != sel_tab_)
        {
            MoveToEx(hdc, dstrc.left, 0, 0);
            LineTo(hdc, dstrc.right, 0);
        }

        /* draw our label */
        ExtTextOut(hdc, dstrc.left + 2, dstrc.top, 0, 0,
                   cur->label_.get(), get_strlen(cur->label_.get()), 0);

        /* advance to the separator */
        dstrc.left += cur->wid_;
        dstrc.right = dstrc.left + CELL_BMP_WID;

        /*
         *   Figure out what to use as the next edge 
         */
        if (cur->next_ == 0)
        {
            /*
             *   This is the last tab.  If it's selected, use #6,
             *   otherwise use #2 
             */
            if (cur == sel_tab_)
                cellnum = 6;
            else
                cellnum = 2;
        }
        else if (cur == sel_tab_)
        {
            /* 
             *   this is the selected tab, but it's not the last tab - use
             *   #4 
             */
            cellnum = 4;
        }
        else if (cur->next_ == sel_tab_)
        {
            /* the next tab is selected - use #3 */
            cellnum = 3;
        }
        else
        {
            /* 
             *   we're in the middle somewhere, and neither this one nor
             *   the next one is selected - use #1 
             */
            cellnum = 1;
        }

        /* draw the separator */
        draw_cell(&dstrc, cellnum);
    }

    /* offset past the last separator */
    OffsetRect(&dstrc, CELL_BMP_WID, 0);

    /*
     *   If there's anything left in the drawing area, fill it with gray 
     */
    if (area_to_draw->right > dstrc.left)
    {
        /* fill the remaining rectangle with gray */
        dstrc.right = area_to_draw->right;
        FillRect(hdc, &dstrc, gray_br_);

        /* draw the top border line */
        MoveToEx(hdc, dstrc.left, 0, 0);
        LineTo(hdc, dstrc.right, 0);
    }

    /* fill in anything left at the bottom as well */
    if (area_to_draw->bottom > dstrc.bottom)
    {
        SetRect(&dstrc, area_to_draw->left, dstrc.bottom,
                area_to_draw->right, area_to_draw->bottom);
        FillRect(hdc, &dstrc, gray_br_);
    }

    /* restore the old font and background mode */
    font_->unselect(hdc, oldfont);
    SetBkMode(hdc, oldbkmode);
    SelectObject(hdc, oldpen);
}

/*
 *   Add a tab 
 */
void CTadsTabctl::add_tab(int id, const textchar_t *label, int index,
                          IDropTarget *drop_target)
{
    HDC hdc;
    SIZE txtsiz;
    CTadsTabctlTab *new_tab;
    CTadsTabctlTab *prv;
    CTadsTabctlTab *cur;
    int x;
    HGDIOBJ oldfont;

    /* get our device context */
    hdc = GetDC(handle_);

    /* select our font */
    oldfont = font_->select(hdc);

    /* get this tab's width */
    ht_GetTextExtentPoint32(hdc, label, get_strlen(label), &txtsiz);

    /* done with our device context */
    font_->unselect(hdc, oldfont);
    ReleaseDC(handle_, hdc);

    /* create the new tab */
    new_tab = new CTadsTabctlTab(id, label, (int)txtsiz.cx, drop_target);

    /*
     *   Find the insertion point 
     */
    if (index == 0 || first_tab_ == 0)
    {
        /* it's the first item - insert it at the head of the list */
        new_tab->next_ = first_tab_;
        first_tab_ = new_tab;

        /* start fixing up at the left edge of the window */
        x = 0;
    }
    else
    {
        /* find the item in the list before this item */
        for (prv = first_tab_ ; prv != 0 && prv->next_ != 0 && index != 0 ;
             prv = prv->next_, --index) ;

        /* insert after this item */
        new_tab->next_ = prv->next_;
        prv->next_ = new_tab;

        /* start fixing up at the previous tab's right edge */
        x = prv->right_;
    }

    /*
     *   Go through this tab and all following tabs and fix up their
     *   positions 
     */
    for (cur = new_tab ; cur != 0 ; cur = cur->next_)
    {
        /* advance past the separator at the left edge of this tab */
        x += CELL_BMP_WID;

        /* set this tab's position */
        cur->left_ = x;
        cur->right_ = x + cur->wid_;

        /* advance to the next position */
        x = cur->right_;

        /* make sure we draw the tab at its new position */
        inval_tab(cur);
    }

    /* note the new width */
    total_width_ = x + CELL_BMP_WID;

    /*
     *   If we had no selection before, select this tab 
     */
    if (sel_tab_ == 0)
        sel_tab_ = new_tab;
}

/* ------------------------------------------------------------------------ */
/*
 *   CTadsWin - IDropTarget implementation (we share the IUnknown
 *   implementation with IDragSource) 
 */

/*
 *   start dragging over this window 
 */
HRESULT STDMETHODCALLTYPE
   CTadsTabctl::DragEnter(IDataObject __RPC_FAR *dataobj,
                          DWORD keystate, POINTL pt,
                          DWORD __RPC_FAR *effect)
{
    CTadsTabctlTab *cur;
    
    /* if they're over a tab, note the tab and the time */
    if ((cur = find_tab_screen((int)pt.x, (int)pt.y)) != 0)
    {
        hover_tab_ = cur;
        hover_time_ = GetTickCount();
    }

    /* if we have an associated drop target, let it handle the rest */
    if (cur != 0 && cur->drop_target_ != 0)
        return cur->drop_target_->DragEnter(dataobj, keystate, pt, effect);

    /* by default, we won't allow dropping here */
    *effect = DROPEFFECT_NONE;
    return S_OK;
}

/*
 *   continue dragging over this window 
 */
HRESULT STDMETHODCALLTYPE CTadsTabctl::DragOver(DWORD keystate,
                                                POINTL pt,
                                                DWORD __RPC_FAR *effect)
{
    CTadsTabctlTab *cur;
    
    /* 
     *   if they're still hovering over the same tab they were before, and
     *   enough time has elapsed, activate the new tab 
     */
    cur = find_tab_screen((int)pt.x, (int)pt.y);
    if (cur != 0 && cur == hover_tab_ && cur != sel_tab_)
    {
        /* if enough time has elapsed, select the tab */
        if (GetTickCount() - hover_time_ > 350)
        {
            /* activate the tab */
            activate_tab(cur);
        }
    }
    else
    {
        /* 
         *   remember the new place we're hovering, and the time we
         *   started hovering here 
         */
        hover_tab_ = cur;
        hover_time_ = GetTickCount();
    }
    
    /* if we have an associated drop target, let it handle the rest */
    if (cur != 0 && cur->drop_target_ != 0)
        return cur->drop_target_->DragOver(keystate, pt, effect);

    /* by default, we won't allow dropping here */
    *effect = DROPEFFECT_NONE;
    return S_OK;
}

/*
 *   leave this window with no drop having occurred 
 */
HRESULT STDMETHODCALLTYPE CTadsTabctl::DragLeave()
{
    /* if we have an associated drop target, let it handle the rest */
    if (hover_tab_ != 0 && hover_tab_->drop_target_ != 0)
        return hover_tab_->drop_target_->DragLeave();

    /* we don't have anything to do here */
    return S_OK;
}

/*
 *   drop in this window 
 */
HRESULT STDMETHODCALLTYPE
   CTadsTabctl::Drop(IDataObject __RPC_FAR *dataobj,
                     DWORD keystate, POINTL pt,
                     DWORD __RPC_FAR *effect)
{
    CTadsTabctlTab *cur;

    /* find the tab we're over */
    cur = find_tab_screen((int)pt.x, (int)pt.y);

    /* if we have an associated drop target, let it handle the rest */
    if (cur != 0 && cur->drop_target_ != 0)
    {
        HRESULT ret;
        
        /* let the tab handle the drop */
        ret = cur->drop_target_->Drop(dataobj, keystate, pt, effect);

        /* 
         *   if they accepted the drop, and the tab wasn't already
         *   selected, select the tab now
         */
        if (ret == S_OK && *effect != DROPEFFECT_NONE && cur != sel_tab_)
            activate_tab(cur);

        /* return the result */
        return ret;
    }

    /* by default, don't allow dropping here */
    *effect = DROPEFFECT_NONE;
    return S_OK;
}

