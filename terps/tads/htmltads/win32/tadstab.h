/* $Header: d:/cvsroot/tads/html/win32/tadstab.h,v 1.3 1999/07/11 00:46:48 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadstab.h - bottom-of-window tab control
Function
  
Notes
  
Modified
  03/15/98 MJRoberts  - Creation
*/

#ifndef TADSTAB_H
#define TADSTAB_H

#include <Ole2.h>
#include <Windows.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSWIN_H
#include "tadswin.h"
#endif

/* 
 *   notification message ID's - these are passed as the NMHDR.code
 *   paramter in WM_NOTIFY messages to the parent 
 */

/* a new tab was selected */
const int TADSTABN_SELECT = 1;

/*
 *   tab control 
 */
class CTadsTabctl: public CTadsWin
{
public:
    /*
     *   Create the tab control as a child of the given parent window.
     *   The 'id' is a caller-defined integer value that we'll pass in
     *   WM_NOTIFY messages to the parent window to notify it of events in
     *   the tab control. 
     */
    CTadsTabctl(int id, CTadsWin *parent, const RECT *init_pos);
    ~CTadsTabctl();

    /* get my control ID */
    int get_id() const { return id_; }

    /*
     *   Add a tab at the given position in the list (0 means that it's
     *   inserted as the first tab, 1 means that it's inserted as the
     *   second tab, and so on).  The ID is used to identify the tab to
     *   the parent window in notification messages.  
     */
    void add_tab(int id, const textchar_t *label, int index,
                 IDropTarget *drop_target);

    /* get the height of the tab control */
    int get_height() const;

    /* 
     *   Get the display width.  This isn't the actual width of the
     *   control, but rather the minimum width needed to display all of
     *   our tabs.  The actual control window can be larger or smaller. 
     */
    int get_width() const { return total_width_; }
    
    /* 
     *   get the currently selected tab's ID; returns -1 if no tab is
     *   currently selected 
     */
    int get_selected_tab() const;

    /*
     *   IDropTarget implementation 
     */
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject __RPC_FAR *pDataObj,
                                        DWORD grfKeyState, POINTL pt,
                                        DWORD __RPC_FAR *pdwEffect);
    HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt,
                                       DWORD __RPC_FAR *pdwEffect);
    HRESULT STDMETHODCALLTYPE DragLeave();
    HRESULT STDMETHODCALLTYPE Drop(IDataObject __RPC_FAR *pDataObj,
                                   DWORD grfKeyState, POINTL pt,
                                   DWORD __RPC_FAR *pdwEffect);

protected:
    /* we're a child window */
    DWORD get_winstyle()
    {
        return (WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
    }
    DWORD get_winstyle_ex() { return 0; }

    int do_leftbtn_down(int keys, int x, int y, int clicks);

    /* paint the window */
    void do_paint_content(HDC hdc, const RECT *area);

    /* 
     *   erase the background - simply return true to indicate that
     *   background painting is unnecessary, since we completely cover the
     *   window during drawing and hence don't depend on the background
     *   being erased 
     */
    int do_erase_bkg(HDC) { return TRUE; }

    /* 
     *   set up a source rectangle for a particular cell in the drawing
     *   bitmap 
     */
    void set_src_rc(RECT *rc, int cellnum);

    /* invalidate a tab */
    void inval_tab(class CTadsTabctlTab *tab);

    /* activate a given tab */
    void activate_tab(class CTadsTabctlTab *tab);

    /* draw an edge cell into the given rectangle on the screen */
    void draw_cell(const RECT *dstrc, int cellnum);

    /* find the tab containing a given mouse position (local coordinates) */
    class CTadsTabctlTab *find_tab(int x, int y);

    /* find the tab containing the given screen coordinates */
    class CTadsTabctlTab *find_tab_screen(int x, int y);

    /* parent window - this window receives notifications */
    HWND parent_window_;

    /* list of tabs */
    class CTadsTabctlTab *first_tab_;

    /* the selected tab */
    class CTadsTabctlTab *sel_tab_;

    /* the bitmap of the tab edges */
    HBITMAP tabbmp_;

    /* brushes */
    HBRUSH white_br_;
    HBRUSH gray_br_;

    /* my text font */
    class CTadsFont *font_;

    /* my control ID (for WM_NOTIFY) */
    int id_;

    /* total width of the tabs (right edge of the rightmost tab) */
    int total_width_;

    /* for drag/drop - tab we're hovering over, and time hovering began */
    class CTadsTabctlTab *hover_tab_;
    DWORD hover_time_;
};

/*
 *   tab structure 
 */
class CTadsTabctlTab
{
public:
    CTadsTabctlTab(int id, const textchar_t *label, int txtwid,
                   IDropTarget *drop_target)
    {
        id_ = id;
        label_.set(label);
        wid_ = txtwid + 4;
        next_ = 0;
        drop_target_ = drop_target;
    }
    
    /* the displayed label */
    CStringBuf label_;

    /* 
     *   ID - this is used to identify the tab to the parent window in
     *   notification messages 
     */
    int id_;

    /* next tab in the list */
    CTadsTabctlTab *next_;

    /* width of the tab */
    int wid_;

    /* left and right edges of the tab, not including separators */
    int left_;
    int right_;

    /* associated drop target */
    IDropTarget *drop_target_;
};

#endif /* TADSTAB_H */

