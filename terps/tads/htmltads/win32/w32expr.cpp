#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/w32expr.cpp,v 1.4 1999/07/11 00:46:50 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32expr.cpp - TADS debugger for 32-bit windows - expression window
Function
  
Notes
  
Modified
  02/17/98 MJRoberts  - Creation
*/

#include <Windows.h>
#include <Ole2.h>
#include <string.h>

#ifndef HTMLRES_H
#include "htmlres.h"
#endif
#ifndef W32EXPR_H
#include "w32expr.h"
#endif
#ifndef W32TDB_H
#include "w32tdb.h"
#endif
#ifndef TADSFONT_H
#include "tadsfont.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif
#ifndef HTMLDBG_H
#include "htmldbg.h"
#endif
#ifndef HTMLDCFG_H
#include "htmldcfg.h"
#endif
#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif
#ifndef TADSTAB_H
#include "tadstab.h"
#endif
#ifndef TADSOLE_H
#include "tadsole.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   User messages for expression windows 
 */

/* set the expression to the string given by (const textchar_t *)lparam */
static const int DBGEXPRM_SET_EXPR = WM_USER;

/* ------------------------------------------------------------------------ */
/*
 *   Watch window implementation
 */

CHtmlDbgWatchWin::CHtmlDbgWatchWin()
{
    /* no panel yet */
    exprwin_ = 0;
    exprwin_list_ = 0;
    exprwin_list_cnt_ = 0;

    /* make the subwindow occupy the entire window - no insets */
    SetRect(&insets_, 0, 0, 0, 0);

    /* presume there's no tab control */
    tabctl_ = 0;
    sizebox_ = 0;
    graybox_ = 0;
    expr_hscroll_ = 0;
}

CHtmlDbgWatchWin::~CHtmlDbgWatchWin()
{
    /* delete the window list if we allocated one */
    if (exprwin_list_ != 0)
        th_free(exprwin_list_);
}

/*
 *   initialize panels 
 */
void CHtmlDbgWatchWin::init_panels(CHtmlSys_dbgmain *mainwin,
                                   CHtmlDebugHelper *helper)
{
    /* remember the main window, and add our reference to it */
    dbgmain_ = mainwin;
    dbgmain_->AddRef();
}


/*
 *   process window creation - creates the expression subwindow 
 */
void CHtmlDbgWatchWin::do_create()
{
    RECT pos;
    size_t i;
    
    /* do default creation work */
    CTadsWin::do_create();

    /* create the expression control's system window */
    GetClientRect(handle_, &pos);
    exprwin_->create_system_window(this, TRUE, "", &pos);

    /* create the inactive expression windows as well */
    for (i = 0 ; i < exprwin_list_cnt_ ; ++i)
    {
        if (exprwin_list_[i] != exprwin_)
            exprwin_list_[i]->create_system_window(this, FALSE, "", &pos);
    }

    /* set the scrollbar for the initial panel */
    if (exprwin_ != 0 && expr_hscroll_ != 0)
    {
        exprwin_->set_ext_scrollbar(0, expr_hscroll_);
        set_scroll_win(exprwin_);
    }
}

/*
 *   Allocate space in the subwindow list 
 */
void CHtmlDbgWatchWin::alloc_panel_list(size_t cnt)
{
    exprwin_list_cnt_ = cnt;
    exprwin_list_ = (CHtmlDbgExprWin **)
                    th_malloc(cnt * sizeof(CHtmlDbgExprWin *));
}

/*
 *   create a sizebox control 
 */
void CHtmlDbgWatchWin::create_sizebox()
{
    RECT rc;
    int sbwid, sbht;

    /* get my area */
    GetClientRect(handle_, &rc);

    /* get the scrollbar sizes */
    sbwid = GetSystemMetrics(SM_CXVSCROLL);
    sbht = GetSystemMetrics(SM_CYHSCROLL);

    /* create the sizebox */
    sizebox_ = CreateWindow("SCROLLBAR", "",
                            WS_CHILD | SBS_SIZEGRIP
                            | SBS_SIZEBOXBOTTOMRIGHTALIGN,
                            0, 0, 1, 1,
                            handle_, 0,
                            CTadsApp::get_app()->get_instance(), 0);

    /* create a gray box to display when the sizebox is invisible */
    graybox_ = CreateWindow("STATIC", "", WS_CHILD,
                            rc.right - sbwid, rc.bottom - sbht,
                            rc.right, rc.bottom,
                            handle_, 0,
                            CTadsApp::get_app()->get_instance(), 0);

    /* create the horizontal scrollbar */
    expr_hscroll_ = CreateWindow("SCROLLBAR", "",
                                 WS_CHILD | WS_VISIBLE | SBS_HORZ,
                                 0, 0, rc.right - sbwid, rc.bottom,
                                 handle_, 0,
                                 CTadsApp::get_app()->get_instance(), 0);

    /* show the scrollbar */
    ShowWindow(expr_hscroll_, SW_SHOW);

    /* set the scrollbar for the initial panel */
    if (exprwin_ != 0 && expr_hscroll_ != 0)
    {
        exprwin_->set_ext_scrollbar(0, expr_hscroll_);
        set_scroll_win(exprwin_);
    }

    /* initially show the sizebox and hide the gray box */
    ShowWindow(sizebox_, SW_SHOW);
    ShowWindow(graybox_, SW_HIDE);
}

/*
 *   process window deletion 
 */
void CHtmlDbgWatchWin::do_destroy()
{
    /* tell the debugger to clear my window if I'm the active one */
    dbgmain_->on_close_dbg_win(this);

    /* release our reference on the main window */
    dbgmain_->Release();
    
    /* inherit default */
    CTadsWin::do_destroy();
}

/*
 *   close the window - never really close it; simply hide it instead 
 */
int CHtmlDbgWatchWin::do_close()
{
    /* hide the window, rather than closing it */
    ShowWindow(handle_, SW_HIDE);

    /* tell the caller not to really close it */
    return FALSE;
}

/*
 *   resize the window - resizes the expression subwindow to cover the
 *   entire window area
 */
void CHtmlDbgWatchWin::do_resize(int mode, int x, int y)
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
            int sbwid = GetSystemMetrics(SM_CXVSCROLL);
            int sbht = GetSystemMetrics(SM_CYHSCROLL);
            int sizebox_vis;
            int bottom_x;

            /* if we're maximized, don't show the sizebox */
            if (mode == SIZE_MAXIMIZED || is_win_maximized())
                sizebox_vis = FALSE;

            /* resize the subwindow */
            SetRect(&rc, 0, 0, x, y);

            /* if we have a sizebox, move it */
            if (sizebox_ != 0)
            {
                /* 
                 *   if the sizebox is visible, show it and hide the gray
                 *   box; otherwise, show the gray box and hide the
                 *   sizebox 
                 */
                ShowWindow(sizebox_, sizebox_vis ? SW_SHOW : SW_HIDE);
                ShowWindow(graybox_, sizebox_vis ? SW_HIDE : SW_SHOW);

                /* move them both to the bottom right corner */
                MoveWindow(sizebox_, rc.right - sbwid,
                           rc.bottom - sbht, sbwid, sbht, TRUE);
                MoveWindow(graybox_, rc.right - sbwid,
                           rc.bottom - sbht, sbwid, sbht, TRUE);
            }

            /* 
             *   figure out how much space we have at the bottom - we have
             *   the whole bottom, unless there's a sizebox, in which case
             *   we must deduct its width 
             */
            bottom_x = x;
            if (sizebox_ != 0)
                bottom_x -= sbwid;

            /* 
             *   move my horizontal scrollbar (if present) so that it
             *   occupies the space to the right of the tab control 
             */
            if (expr_hscroll_ != 0)
            {
                int tabwid;
                
                /* 
                 *   if there's a tab control, put the scrollbar to its
                 *   right
                 */
                if (tabctl_ != 0)
                {
                    /* get the width of the tab control */
                    tabwid = tabctl_->get_width();

                    /* 
                     *   if that would make the scrollbar less than twice
                     *   the nominal scrollbar width, reduce the tab
                     *   control's width accordingly 
                     */
                    if (bottom_x - tabwid < sbwid * 2)
                    {
                        /* reduce the width to keep a minimum scrollbar size */
                        tabwid = bottom_x - sbwid*2;

                        /* 
                         *   if that leaves less than half of the
                         *   available width for the tab control, give
                         *   each control half of the available space 
                         */
                        if (tabwid < bottom_x/2)
                            tabwid = bottom_x/2;
                    }

                    /* move the tab control to its new position */
                    MoveWindow(tabctl_->get_handle(), 0, y + 1 - sbht,
                               tabwid, sbht, TRUE);

                    /* 
                     *   deduct the scrollbar height from the space
                     *   available to the subwindow 
                     */
                    rc.bottom -= sbht;
                }

                /* move the scrollbar */
                MoveWindow(expr_hscroll_, tabwid, y + 1 - sbht,
                           bottom_x - tabwid, sbht, TRUE);
            }
            else if (tabctl_ != 0)
            {
                /* 
                 *   we have only a tab control, not a scrollbar; move the
                 *   tab control along the entire bottom of the window 
                 */
                MoveWindow(tabctl_->get_handle(),
                           0, y + 1 - tabctl_->get_height(),
                           bottom_x, tabctl_->get_height(), TRUE);

                /* 
                 *   deduct the tab control height from the space
                 *   available to the subwindow 
                 */
                rc.bottom -= tabctl_->get_height();
            }

            /* move the expression subwindow */
            if (exprwin_ != 0)
                MoveWindow(exprwin_->get_handle(),
                           rc.left + insets_.left, rc.top + insets_.top,
                           rc.right - rc.left - insets_.right - insets_.left,
                           rc.bottom - rc.top - insets_.bottom - insets_.top,
                           TRUE);

            /* inherit default handling */
            CTadsWin::do_resize(mode, x, y);
        }
    }
}

/*
 *   gain focus 
 */
void CHtmlDbgWatchWin::do_setfocus(HWND /*prev_win*/)
{
    /* give focus to the expression subwindow */
    if (exprwin_ != 0)
        SetFocus(exprwin_->get_handle());
}

/*
 *   Receive a notification from a control 
 */
int CHtmlDbgWatchWin::do_notify(int command_id, int notify_code,
                                LPNMHDR nmhdr)
{
    /* see if this is a notification from the tab control */
    if (tabctl_ != 0 && command_id == tabctl_->get_id())
    {
        switch(notify_code)
        {
        case TADSTABN_SELECT:
            /* make sure we don't have an open edit field */
            if (exprwin_ != 0)
                exprwin_->close_entry_field(TRUE);
            
            /* selected a new tab */
            select_new_tab(tabctl_->get_selected_tab());

            /* handled */
            return TRUE;

        default:
            /* ignore other notifications */
            break;
        }
    }

    /* inherit default */
    return CTadsWin::do_notify(command_id, notify_code, nmhdr);
}

/*
 *   Select a new tab 
 */
void CHtmlDbgWatchWin::select_new_tab(int tab_id)
{
    CHtmlDbgExprWin *oldwin;
    RECT rc;

    /* remember the old window for a moment */
    oldwin = exprwin_;
    
    /* the tab ID is the index into our list - get the new window */
    exprwin_ = exprwin_list_[tab_id];

    /* make sure the window is sized properly */
    GetClientRect(handle_, &rc);
    do_resize(SIZE_RESTORED, rc.right, rc.bottom);

    /* make sure the new window's contents are accurate */
    update_all();

    /* show the new window */
    ShowWindow(exprwin_->get_handle(), SW_SHOW);

    /* hide the old window */
    ShowWindow(oldwin->get_handle(), SW_HIDE);

    /* set the horizontal scrollbar to control the new panel */
    exprwin_->set_ext_scrollbar(0, expr_hscroll_);
    set_scroll_win(exprwin_);
}

/*
 *   Update all expressions 
 */
void CHtmlDbgWatchWin::update_all()
{
    /* tell the subwindow to do the work */
    if (exprwin_ != 0)
        exprwin_->update_all();
}

/*
 *   Clear my data from a configuration 
 */
void CHtmlDbgWatchWin::clear_config(CHtmlDebugConfig *config,
                                    const textchar_t *varname) const
{
    /* tell my subwindows to clear their data from the configuration */
    if (exprwin_list_ != 0)
    {
        size_t i;

        /* clear each panel's configuration */
        for (i = 0 ; i < exprwin_list_cnt_ ; ++i)
            exprwin_list_[i]->clear_config(config, varname, i);
    }
    else if (exprwin_ != 0)
    {
        /* clear our single panel's configuration */
        exprwin_->clear_config(config, varname, 0);
    }
}

/*
 *   Save my configuration 
 */
void CHtmlDbgWatchWin::save_config(CHtmlDebugConfig *config,
                                   const textchar_t *varname)
{
    RECT rc;
    CHtmlRect hrc;

    /* save my visibility */
    config->setval(varname, "vis", IsWindowVisible(handle_));

    /* save my position */
    GetWindowRect(handle_, &rc);
    w32tdb_adjust_client_pos(dbgmain_, &rc);
    hrc.set(rc.left, rc.top, rc.right, rc.bottom);
    config->setval(varname, "pos", &hrc);
    
    /* tell my subwindows to save their configuration */
    if (exprwin_list_ != 0)
    {
        size_t i;

        /* save each panel's configuration */
        for (i = 0 ; i < exprwin_list_cnt_ ; ++i)
            exprwin_list_[i]->save_config(config, varname, i);
    }
    else if (exprwin_ != 0)
    {
        /* save our single panel's configuration */
        exprwin_->save_config(config, varname, 0);
    }
}

/*
 *   Load my window configuration
 */
void CHtmlDbgWatchWin::load_win_config(CHtmlDebugConfig *config,
                                       const textchar_t *varname,
                                       RECT *pos, int *vis)
{
    CHtmlRect hrc;
    
    /* get my position from the saved configuration */
    if (!config->getval(varname, "pos", &hrc))
        SetRect(pos, hrc.left, hrc.top,
                hrc.right - hrc.left, hrc.bottom - hrc.top);

    /* get my visibility from the saved configuration */
    if (config->getval(varname, "vis", vis))
        *vis = TRUE;
}

/*
 *   load my panel configuration 
 */
void CHtmlDbgWatchWin::load_panel_config(CHtmlDebugConfig *config,
                                         const textchar_t *varname)
{
    /* tell my subwindows to load their configurations */
    if (exprwin_list_ != 0)
    {
        size_t i;

        /* load each subwindow's configuration */
        for (i = 0 ; i < exprwin_list_cnt_ ; ++i)
            exprwin_list_[i]->load_config(config, varname, i);
    }
    else if (exprwin_ != 0)
    {
        /* load my single panel's configuration */
        exprwin_->load_config(config, varname, 0);
    }
}

/* 
 *   check the status of a command routed from the main window - we'll
 *   simply route the command to our subpanel 
 */
TadsCmdStat_t CHtmlDbgWatchWin::active_check_command(int command_id)
{
    if (exprwin_ != 0)
        return exprwin_->check_command(command_id);
    else
        return TADSCMD_UNKNOWN;
}

/* 
 *   execute a command routed from the main window - we'll simply route
 *   the command to our subpanel 
 */
int CHtmlDbgWatchWin::active_do_command(int notify_code, int command_id,
                                        HWND ctl)
{
    if (exprwin_ != 0)
        return exprwin_->do_command(notify_code, command_id, ctl);
    else
        return FALSE;
}

/*
 *   Non-client activation/deactivation.  Notify the debugger main window
 *   of the coming or going of this debugger window as the active debugger
 *   window, which will allow us to receive certain command messages from
 *   the main window.  
 */
int CHtmlDbgWatchWin::do_ncactivate(int flag)
{
    /* register or deregister with the main window */
    if (dbgmain_ != 0 && can_be_main_active())
    {
        if (flag)
            dbgmain_->set_active_dbg_win(this);
        else
            dbgmain_->clear_active_dbg_win();
    }
        
    /* inherit default handling */
    return CTadsWin::do_ncactivate(flag);
}

/*
 *   Receive notification of activation from parent 
 */
void CHtmlDbgWatchWin::do_parent_activate(int active)
{
    /* register or deregister with the main window */
    if (dbgmain_ != 0 && can_be_main_active())
    {
        if (active)
            dbgmain_->set_active_dbg_win(this);
        else
            dbgmain_->clear_active_dbg_win();
    }

    /* inherit default */
    CTadsWin::do_parent_activate(active);
}

/*
 *   Add an expression to the current active panel 
 */
void CHtmlDbgWatchWin::add_expr(const textchar_t *expr, int can_edit_expr,
                                int select_new_item)
{
    if (exprwin_ != 0)
        exprwin_->add_expr(expr, can_edit_expr, select_new_item);
}


/* ------------------------------------------------------------------------ */
/*
 *   Watch window for local variables 
 */

/*
 *   initialize panels 
 */
void CHtmlDbgWatchWinLocals::init_panels(CHtmlSys_dbgmain *mainwin,
                                         CHtmlDebugHelper *helper)
{
    /* inherit default */
    CHtmlDbgWatchWin::init_panels(mainwin, helper);

    /* create space for two subpanels */
    alloc_panel_list(2);

    /* allocate the local variable panel */
    exprwin_list_[0] = new CHtmlDbgExprWinLocals(TRUE, FALSE, mainwin, helper);

    /* allocate the 'this' subpanel */
    exprwin_list_[1] = new CHtmlDbgExprWinSelf(TRUE, FALSE, mainwin, helper);

    /* set the first panel active */
    exprwin_ = exprwin_list_[0];
}

/*
 *   handle creation 
 */
void CHtmlDbgWatchWinLocals::do_create()
{
    RECT rc;

    /* inherit default */
    CHtmlDbgWatchWin::do_create();

    /* create my tab control */
    GetClientRect(handle_, &rc);
    tabctl_ = new CTadsTabctl(1, this, &rc);
    tabctl_->add_tab(0, "Locals", 0, 0);
    tabctl_->add_tab(1, "self", 1, 0);

    /* create a sizebox */
    create_sizebox();
}

/* ------------------------------------------------------------------------ */
/*
 *   Watch window for user-editable list of expressions 
 */

/*
 *   initialize panels - create a standard panel that allows new items to
 *   be added through an extra data entry item 
 */
void CHtmlDbgWatchWinExpr::init_panels(CHtmlSys_dbgmain *mainwin,
                                       CHtmlDebugHelper *helper)
{
    int i;
    
    /* inherit default */
    CHtmlDbgWatchWin::init_panels(mainwin, helper);

    /* create space for four subpanels */
    alloc_panel_list(4);

    /* create the panels */
    for (i = 0 ; i < 4 ; ++i)
        exprwin_list_[i] = new CHtmlDbgExprWin(TRUE, FALSE, TRUE,
                                               FALSE, mainwin, helper);

    /* set the first one active */
    exprwin_ = exprwin_list_[0];
}

/*
 *   handle creation 
 */
void CHtmlDbgWatchWinExpr::do_create()
{
    RECT rc;

    /* inherit default */
    CHtmlDbgWatchWin::do_create();

    /* create my tab control */
    GetClientRect(handle_, &rc);
    tabctl_ = new CTadsTabctl(1, this, &rc);
    tabctl_->add_tab(0, "Watch 1", 0, exprwin_list_[0]);
    tabctl_->add_tab(1, "Watch 2", 1, exprwin_list_[1]);
    tabctl_->add_tab(2, "Watch 3", 2, exprwin_list_[2]);
    tabctl_->add_tab(3, "Watch 4", 3, exprwin_list_[3]);

    /* create a sizebox */
    create_sizebox();
}

/* ------------------------------------------------------------------------ */
/*
 *   Expression evaluator dialog implementation 
 */

/* height and width of close button */
const int closebtn_wid = 70;
const int closebtn_ht = 20;

/*
 *   initialize panels - create a standard panel that allows new items to
 *   be added through an extra data entry item 
 */
void CHtmlDbgWatchWinDlg::init_panels(CHtmlSys_dbgmain *mainwin,
                                      CHtmlDebugHelper *helper)
{
    /* inherit default */
    CHtmlDbgWatchWin::init_panels(mainwin, helper);

    /* create the single-expression window */
    exprwin_ = new CHtmlDbgExprWin(TRUE, TRUE, TRUE, TRUE, mainwin, helper);

    /* inset the bottom by a bit to leave room for the dismiss button */
    insets_.bottom = closebtn_ht + 4;

    /* no close button yet */
    closebtn_ = 0;
}

/*
 *   creation 
 */
void CHtmlDbgWatchWinDlg::do_create()
{
    RECT rc;
    
    /* inherit default */
    CHtmlDbgWatchWin::do_create();

    /* create the dismiss button */
    GetClientRect(handle_, &rc);
    closebtn_ = CreateWindow("BUTTON", "Close",
                             WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                             (rc.right - closebtn_wid)/2,
                             rc.bottom - closebtn_ht,
                             closebtn_wid, closebtn_ht, handle_, (HMENU)IDOK,
                             CTadsApp::get_app()->get_instance(), 0);

    /* set the button to use the default dialog font */
    PostMessage(closebtn_, WM_SETFONT,
                (WPARAM)(HFONT)GetStockObject(DEFAULT_GUI_FONT),
                MAKELPARAM(TRUE, 0));
}

/*
 *   run the window as a modal dialog 
 */
void CHtmlDbgWatchWinDlg::run_as_dialog(CHtmlSys_dbgmain *mainwin,
                                        CHtmlDebugHelper *helper,
                                        const textchar_t *init_expr)
{
    void *ctx;
    CHtmlDbgWatchWinDlg *win;
    RECT drc;
    RECT rc;
    int wid, ht;
    CHtmlRect text_area;
    
    /* 
     *   disable windows owned by the main window while the dialog is
     *   running 
     */
    ctx = CTadsDialog::modal_dlg_pre(mainwin->get_handle(), TRUE);

    /* create our window */
    win = new CHtmlDbgWatchWinDlg();
    win->init_panels(mainwin, helper);

    /* center the window on the screen */
    GetWindowRect(GetDesktopWindow(), &drc);
    wid = 500;
    ht = 250;
    SetRect(&rc, drc.left + (drc.right - drc.left - wid)/2,
            drc.top + (drc.bottom - drc.top - ht)/2, wid, ht);

    /* open the window */
    win->create_system_window(mainwin, TRUE, "Evaluate", &rc);

    /* set the initial expression, if one was provided */
    if (init_expr != 0 && init_expr[0] != '\0')
        PostMessage(win->exprwin_->get_handle(),
                    DBGEXPRM_SET_EXPR, 0, (LPARAM)init_expr);

    /* immediately open the field by sending an enter character */
    PostMessage(win->exprwin_->get_handle(), WM_CHAR, (WPARAM)13, 0);

    /* enter a recursive event loop until the window is closed */
    win->AddRef();
    if (!CTadsApp::get_app()->event_loop(&win->closing_))
    {
        /* post another quit message to the enclosing event loop */
        PostQuitMessage(0);
    }

    /* re-enable the windows we disabled before running the dialog */
    CTadsDialog::modal_dlg_post(ctx);

    /* 
     *   Destroy the dialog window.  Note that we wait until we've
     *   re-enabled all of the other windows, so that we'll have something
     *   to activate when this window is closed. 
     */
    DestroyWindow(win->get_handle());

    /* release our reference on the window */
    win->Release();
}

/*
 *   On closing the window, set the flag that indicates that the dialog is
 *   being dismissed, but don't actually close the window. 
 */
int CHtmlDbgWatchWinDlg::do_close()
{
    /* set the "closing" flag so that we exit the modal event loop */
    closing_ = TRUE;
    
    /* do not actually close the window */
    return FALSE;
}

/*
 *   resize 
 */
void CHtmlDbgWatchWinDlg::do_resize(int mode, int x, int y)
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
            CHtmlDbgWatchWin::do_resize(mode, x, y);
            
            /* move the "Close" button */
            GetClientRect(handle_, &rc);
            MoveWindow(closebtn_, (rc.right - closebtn_wid) / 2,
                       rc.bottom - closebtn_ht,
                       closebtn_wid, closebtn_ht, TRUE);
        }
    }
}

/* 
 *   handle a keystroke message 
 */
int CHtmlDbgWatchWinDlg::do_keydown(int vkey, long keydata)
{
    switch(vkey)
    {
    case VK_ESCAPE:
        /* dismiss the window by setting the "closing" flag */
        closing_ = TRUE;
        return TRUE;

    default:
        /* inherit default handling */
        return CHtmlDbgWatchWin::do_keydown(vkey, keydata);
    }
}

/*
 *   handle a character message 
 */
int CHtmlDbgWatchWinDlg::do_char(TCHAR ch, long keydata)
{
    switch(ch)
    {
    case 27:
        /* escape - dismiss the dialog */
        closing_ = TRUE;

        /* inherit the default to close the entry field */
        break;

    default:
        /* no special handling */
        break;
    }
    
    /* inherit default handling */
    return CHtmlDbgWatchWin::do_char(ch, keydata);
}


/*
 *   handle a command 
 */
int CHtmlDbgWatchWinDlg::do_command(int notify_code, int cmd, HWND ctl)
{
    /* if this is the "Close" button, set the closing flag */
    if (ctl == closebtn_)
    {
        closing_ = TRUE;
        return TRUE;
    }

    /* inherit default */
    return CHtmlDbgWatchWin::do_command(notify_code, cmd, ctl);
}

/*
 *   paint the window 
 */
void CHtmlDbgWatchWinDlg::do_paint_content(HDC hdc,
                                           const RECT *area)
{
    /* fill in the background with gray */
    FillRect(hdc, area, GetSysColorBrush(COLOR_3DFACE));
}

/* ------------------------------------------------------------------------ */
/*
 *   Expression window implementation
 */

CHtmlDbgExprWin::CHtmlDbgExprWin(int has_vscroll, int has_hscroll,
                                 int has_blank_entry, int single_entry,
                                 CHtmlSys_dbgmain *mainwin,
                                 CHtmlDebugHelper *helper)
    : CTadsWinScroll(has_vscroll, has_hscroll)
{
    /* remember the main window and helper object */
    mainwin_ = mainwin;
    helper_ = helper;
    helper_->add_ref();
    
    /* start out with no expressions in the list */
    first_expr_ = last_expr_ = 0;
    selection_ = 0;

    /* we don't know where the separator goes yet, but zero it for now */
    sep_xpos_ = 0;

    /* no data entry field yet */
    entry_fld_ = 0;

    /* don't have focus yet */
    have_focus_ = FALSE;

    /* no focus item yet */
    focus_expr_ = 0;
    drawn_focus_ = FALSE;

    /* not tracking anything yet */
    tracking_sep_ = FALSE;

    /* note whether we have a blank entry */
    has_blank_entry_ = has_blank_entry;

    /* note whether we allow just a single top-level entry */
    single_entry_ = single_entry;

    /* create a small monospaced font for the list items */
    list_font_ = CTadsApp::get_app()->get_font("Courier New", 9, FALSE, FALSE,
                                               GetSysColor(COLOR_WINDOWTEXT),
                                               FF_ROMAN | FIXED_PITCH);

    /* create a small sans serif font for the header font */
    hdr_font_ = CTadsApp::get_app()->get_font("MS Sans Serif", 8, TRUE, FALSE,
                                              GetSysColor(COLOR_BTNTEXT),
                                              FF_SWISS | VARIABLE_PITCH);

    /* load the splitter bar cursor */
    split_ew_cursor_ = LoadCursor(CTadsApp::get_app()->get_instance(),
                                  MAKEINTRESOURCE(IDC_SPLIT_EW_CSR));

    /* load the box-plus/box-minus icons */
    ico_plus_ = (HICON)LoadImage(CTadsApp::get_app()->get_instance(),
                                 MAKEINTRESOURCE(IDI_BOX_PLUS), IMAGE_ICON,
                                 9, 9, 0);
    ico_minus_ = (HICON)LoadImage(CTadsApp::get_app()->get_instance(),
                                  MAKEINTRESOURCE(IDI_BOX_MINUS), IMAGE_ICON,
                                  9, 9, 0);
    bmp_vdots_ = LoadBitmap(CTadsApp::get_app()->get_instance(),
                            MAKEINTRESOURCE(IDB_VDOTS));
    bmp_hdots_ = LoadBitmap(CTadsApp::get_app()->get_instance(),
                            MAKEINTRESOURCE(IDB_HDOTS));

    /* load my context menu */
    ctx_menu_ = LoadMenu(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDR_DEBUG_EXPR_POPUP));
    expr_popup_ = GetSubMenu(ctx_menu_, 0);
}

CHtmlDbgExprWin::~CHtmlDbgExprWin()
{
    /* if there's an entry field, act as though we're leaving it */
    if (entry_fld_ != 0)
        on_entry_field_killfocus();
    
    /* delete our context menu */
    DestroyMenu(ctx_menu_);
    
    /* delete our cursor */
    DestroyCursor(split_ew_cursor_);

    /* delete the icons */
    DeleteObject(ico_plus_);
    DeleteObject(ico_minus_);
    DeleteObject(bmp_hdots_);
    DeleteObject(bmp_vdots_);

    /* if we still have an entry field, remove it */
    close_entry_field(FALSE);
    
    /* delete any remaining expression items */
    while (first_expr_ != 0)
    {
        CHtmlDbgExpr *expr;
        
        /* remember this expression while unlinking it from the list */
        expr = first_expr_;

        /* unlink it */
        first_expr_ = first_expr_->get_next_expr();

        /* delete it now that it's safely out of the list */
        delete expr;
    }

    /* release our helper reference */
    helper_->remove_ref();
}

/*
 *   process system window creation 
 */
void CHtmlDbgExprWin::do_create()
{
    RECT rc;
    HDC dc;
    TEXTMETRIC tm;
    CHtmlDbgExpr *expr;
    HGDIOBJ oldfont;

    /* calculate the height of the header line */
    dc = GetDC(handle_);
    oldfont = hdr_font_->select(dc);
    GetTextMetrics(dc, &tm);
    hdr_height_ = tm.tmHeight + 2;
    hdr_font_->unselect(dc, oldfont);
    ReleaseDC(handle_, dc);

    /* create a blank entry */
    expr = new CHtmlDbgExpr(1, "", 0, 0, "", "", 0, FALSE, TRUE, 0, this);

    /* set the vertical scrolling units to the height of the item */
    vscroll_units_ = expr->get_height();

    /* do default creation */
    CTadsWinScroll::do_create();
    
    /* start out with the separator line halfway across the window */
    GetClientRect(handle_, &rc);
    sep_xpos_ = (rc.right - rc.left) / 2;

    /* see if we're keeping a blank entry at the end of the list */
    if (has_blank_entry_)
    {
        /* use the entry we just created as the initial blank entry */
        first_expr_ = last_expr_ = expr;

        /* set focus on its expression part */
        set_focus_expr(first_expr_, HTMLEXPR_WINPART_EXPR);
    }
    else
    {
        /* 
         *   we're not keeping a blank entry, so discard the entry we
         *   created above 
         */
        delete expr;
    }

    /* update scrollbars now that we have the window set up */
    adjust_scrollbar_ranges();

    /*
     *   If I have a blank expression line, and I'm not a single-entry
     *   window, allow dropping a new expression onto the window - to do
     *   this, simply register as a drop target 
     */
    if (has_blank_entry_ && !single_entry_)
        drop_target_register();
}

/*
 *   Update all expressions 
 */
void CHtmlDbgExprWin::update_all()
{
    CHtmlDbgExpr *cur;
    
    /* 
     *   Run through all top-level expressions, and tell them to update
     *   themselves.  Updating a top-level expression may affect child
     *   expressions, but will not affect other top-level expressions.
     */
    for (cur = first_expr_ ; cur != 0 ; )
    {
        int level;

        /* if this is the blank last item, leave the value blank */
        if (has_blank_entry_ && cur == last_expr_)
            break;
        
        /* update this expression */
        cur->update_eval(this);

        /* find the next expression at the same level */
        level = cur->get_level();
        for (cur = cur->get_next_expr() ;
             cur != 0 && cur->get_level() > level ;
             cur = cur->get_next_expr()) ;
    }

    /* update scrollbars */
    adjust_scrollbar_ranges();
}

/*
 *   Set focus on an expression 
 */
void CHtmlDbgExprWin::set_focus_expr(CHtmlDbgExpr *expr,
                                     htmlexpr_winpart_t part)
{
    /* remove the current focus drawing */
    if (focus_expr_ != 0)
        inval_expr(focus_expr_);

    /* remember the new focus */
    focus_expr_ = expr;
    focus_part_ = part;

    /* draw the new focus */
    if (focus_expr_ != 0)
        inval_expr(focus_expr_);
}

/*
 *   draw the focus indicator
 */
void CHtmlDbgExprWin::draw_focus(int draw)
{
    RECT rc;
    CHtmlRect text_area;
    HDC dc;
    
    /* if the window doesn't have focus, there's nothing to draw */
    if (!have_focus_)
        return;

    /* 
     *   if the focus is already drawn in the desired state, there's
     *   nothing to do 
     */
    if ((drawn_focus_ != 0) == (draw != 0))
        return;

    /* if we don't have a focus item, there's nothing to do */
    if (focus_expr_ == 0)
        return;

    /* remember the new state */
    drawn_focus_ = draw;

    /* get the area to draw */
    get_focus_text_area(&text_area, FALSE);

    /* move it into a windows RECT */
    SetRect(&rc, text_area.left, text_area.top,
            text_area.right, text_area.bottom);

    /* inset it slightly so that it draws inside the ara */
    InflateRect(&rc, -1, -1);

    /* draw it */
    dc = GetDC(handle_);
    DrawFocusRect(dc, &rc);
    ReleaseDC(handle_, dc);
}

/*
 *   Get the text area of the current focus part (in window coordinates)
 */
void CHtmlDbgExprWin::get_focus_text_area(CHtmlRect *text_area,
                                          int on_screen)
{
    RECT rc;
    long xl, xr;
    
    /* if we don't have a focus item, there's nothing to do */
    if (focus_expr_ == 0)
        return;

    /* get the window horizontal boundaries in doc coordinates */
    GetClientRect(handle_, &rc);
    xl = screen_to_doc_x(rc.left);
    xr = screen_to_doc_x(rc.right - GetSystemMetrics(SM_CXVSCROLL));

    /* calculate the horizontal extent */
    focus_expr_->part_to_area(focus_part_, &xl, &xr, sep_xpos_, on_screen);

    /* fill in the rectangle with the document coordinates */
    text_area->left = xl;
    text_area->right = xr;
    text_area->top = focus_expr_->get_y_pos();
    text_area->bottom = text_area->top + focus_expr_->get_height();

    /* adjust to window coodinates, and adjust for header offset */
    *text_area = doc_to_screen(*text_area);
    text_area->offset(0,  hdr_height_);
}

/*
 *   Move to the next focus 
 */
void CHtmlDbgExprWin::go_next_focus(int allow_same, int backwards)
{
    CHtmlDbgExpr *orig_expr;
    htmlexpr_winpart_t orig_part;

    /* 
     *   remember the initial focus position; if we loop back here, we'll
     *   give up 
     */
    orig_expr = focus_expr_;
    orig_part = focus_part_;

    /* if there are no items, there's nothing to do */
    if (first_expr_ == 0)
        return;

    /* remove the current focus drawing */
    if (focus_expr_ != 0)
        inval_expr(focus_expr_);

    /* keep going until we run out of focus opportunities */
    for (;;)
    {
        /* 
         *   Move on to the next item.  If we're in the expression field
         *   of an item, we may be able to move to the value portion of
         *   the item; otherwise, move to the expression portion of the
         *   next item. 
         */
        if (focus_expr_ == 0)
        {
            /* we don't have an expression currently - wrap around */
            focus_expr_ = (backwards ? last_expr_ : first_expr_);
            focus_part_ = (backwards ? HTMLEXPR_WINPART_VAL
                           : HTMLEXPR_WINPART_EXPR);
        }
        else if (!backwards)
        {
            if (focus_part_ == HTMLEXPR_WINPART_EXPR)
            {
                /* stay in same item, and move to the value portion */
                focus_part_ = HTMLEXPR_WINPART_VAL;
            }
            else
            {
                /* move to expression portion of next item */
                focus_expr_ = focus_expr_->get_next_expr();
                focus_part_ = HTMLEXPR_WINPART_EXPR;
                
                /* if that's the last one, loop to the first one */
                if (focus_expr_ == 0)
                {
                    /* 
                     *   if we were nowhere originally, we have now
                     *   visited all of the items unsuccessfully 
                     */
                    if (orig_expr == 0)
                        break;

                    /* wrap around */
                    focus_expr_ = first_expr_;
                }
            }
        }
        else
        {
            if (focus_part_ == HTMLEXPR_WINPART_VAL)
            {
                /* stay in same item, and move to the expression part */
                focus_part_ = HTMLEXPR_WINPART_EXPR;
            }
            else
            {
                /* move to value portion of previous item */
                focus_expr_ = focus_expr_->get_prev_expr();
                focus_part_ = HTMLEXPR_WINPART_VAL;

                /* if that's the first one, loop to the last one */
                if (focus_expr_ == 0)
                {
                    /* 
                     *   if we were nowhere originally, we have now
                     *   visited all of the items unsuccessfully 
                     */
                    if (orig_expr == 0)
                        break;

                    /* wrap around */
                    focus_expr_ = last_expr_;
                }
            }
        }
            
        /*
         *   If we're back at the original position, there's nothing else
         *   with focus, so stop scanning.  
         */
        if (focus_expr_ == orig_expr && focus_part_ == orig_part)
        {
            /*
             *   If we're not allowed to stay on the same item, eliminate
             *   the focus. 
             */
            if (!allow_same)
                focus_expr_ = 0;

            /* done */
            break;
        }

        /*
         *   Determine if this part allows editing.  If so, we're done;
         *   otherwise, we must continue scanning.  However, don't stop
         *   here if we're on the same item as where we started and we're
         *   not allowed to stay on the same item.  
         */
        if ((allow_same || focus_expr_ != orig_expr)
            && focus_expr_->can_edit_part(focus_part_))
            break;
    }

    /* move the selection to the new item */
    if (focus_expr_ != 0)
        set_selection(focus_expr_, FALSE, HTMLEXPR_WINPART_BLANK);

    /* draw the new focus item */
    if (focus_expr_ != 0)
        inval_expr(focus_expr_);
}

/*
 *   check a command status 
 */
TadsCmdStat_t CHtmlDbgExprWin::check_command(int command_id)
{
    switch(command_id)
    {
    case ID_EDIT_DELETE:
    case ID_EDIT_CUT:
    case ID_EDIT_COPY:
        /* 
         *   if an entry field is open, this is possible if there's a
         *   non-empty selection 
         */
        if (entry_fld_ != 0)
        {
            DWORD start, end;

            /* get the selection */
            SendMessage(entry_fld_, EM_GETSEL, (WPARAM)&start, (WPARAM)&end);

            /* the operation is allowed if the field is non-empty */
            return (start != end ? TADSCMD_ENABLED : TADSCMD_DISABLED);
        }

        /* if it's DELETE, see if we can delete the current focus item */
        if (command_id == ID_EDIT_DELETE)
            return (can_delete_focus() ? TADSCMD_ENABLED : TADSCMD_DISABLED);

        /* otherwise, it's not allowed */
        return TADSCMD_DISABLED;

    case ID_EDIT_PASTE:
        /* this is possible if an edit field is open */
        return (entry_fld_ != 0 ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_EDIT_UNDO:
        /* if an edit field is open, we can undo if it can */
        if (entry_fld_ != 0)
            return (SendMessage(entry_fld_, EM_CANUNDO, 0, 0)
                    ? TADSCMD_ENABLED : TADSCMD_DISABLED);

        /* otherwise, it's not allowed */
        return TADSCMD_DISABLED;
        
    case ID_DBGEXPR_NEW:
        /* 
         *   allow new items only if this window has a blank entry item
         *   and it's not a single-item window 
         */
        return (has_blank_entry_ && !single_entry_
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_DBGEXPR_EDITEXPR:
        /* allow it only if the expression is editable */
        return ((focus_expr_ != 0
                 && focus_expr_->can_edit_part(HTMLEXPR_WINPART_EXPR))
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_DBGEXPR_EDITVAL:
        /* allow it only if the value is editable */
        return ((focus_expr_ != 0
                 && focus_expr_->can_edit_part(HTMLEXPR_WINPART_VAL))
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_DEBUG_ADDTOWATCH:
        /* allow only if we have a non-empty selection */
        return ((focus_expr_ != 0 && !focus_expr_->is_expr_empty())
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);
        
    default:
        /* inherit default handling */
        return CTadsWinScroll::check_command(command_id);
    }
}


/* 
 *   handle a command 
 */
int CHtmlDbgExprWin::do_command(int notify_code, int command_id, HWND ctl)
{
    /* if this is a notify message from the entry field, handle it */
    if (entry_fld_ != 0 && ctl == entry_fld_)
    {
        switch(notify_code)
        {
        case EN_KILLFOCUS:
            /* focus is leaving the entry field - close it */
            on_entry_field_killfocus();

            /* handled */
            return TRUE;
        }
    }

    /* if there's an edit field, try handling it via the field */
    if (entry_fld_ != 0)
    {
        int edit_cmd;

        /* check the command */
        switch(command_id)
        {
        case ID_EDIT_DELETE:
            edit_cmd = WM_CLEAR;
            goto send_to_entry_fld;
            
        case ID_EDIT_CUT:
            edit_cmd = WM_CUT;
            goto send_to_entry_fld;
            
        case ID_EDIT_COPY:
            edit_cmd = WM_COPY;
            goto send_to_entry_fld;
            
        case ID_EDIT_PASTE:
            edit_cmd = WM_PASTE;
            goto send_to_entry_fld;
            
        case ID_EDIT_UNDO:
            edit_cmd = EM_UNDO;
            
        send_to_entry_fld:
            /* send the message to the field */
            SendMessage(entry_fld_, edit_cmd, 0, 0);
            
            /* we've handled it */
            return TRUE;

        default:
            /* other messages skip the field */
            break;
        }
    }

    /* check the command */
    switch (command_id)
    {
    case ID_EDIT_DELETE:
        /* process as though they hit the delete key */
        do_keydown(VK_DELETE, 0);
        return TRUE;

    case ID_DBGEXPR_NEW:
        /* set focus to the blank entry, and edit the expression part */
        if (has_blank_entry_)
        {
            set_focus_expr(last_expr_, HTMLEXPR_WINPART_EXPR);
            SendMessage(handle_, WM_CHAR, (WPARAM)13, 0);
        }
        return TRUE;

    case ID_DBGEXPR_EDITEXPR:
        /* set focus to the current expression and edit it */
        if (focus_expr_ != 0
            && focus_expr_->can_edit_part(HTMLEXPR_WINPART_EXPR))
        {
            set_focus_expr(focus_expr_, HTMLEXPR_WINPART_EXPR);
            SendMessage(handle_, WM_CHAR, (WPARAM)13, 0);
        }
        return TRUE;

    case ID_DBGEXPR_EDITVAL:
        /* set focus to the current value and edit it */
        if (focus_expr_ != 0
            && focus_expr_->can_edit_part(HTMLEXPR_WINPART_VAL))
        {
            set_focus_expr(focus_expr_, HTMLEXPR_WINPART_VAL);
            SendMessage(handle_, WM_CHAR, (WPARAM)13, 0);
        }
        return TRUE;

    case ID_DEBUG_ADDTOWATCH:
        /* 
         *   ask the main window to add the selected item's full
         *   expression text to the watch window as a new item (note that
         *   we don't just add it to myself, since I may not be the watch
         *   window) 
         */
        if (focus_expr_ != 0 && !focus_expr_->is_expr_empty())
            mainwin_->add_watch_expr(focus_expr_->get_full_expr());
        return TRUE;
        
    default:
        /* inherit default handling */
        return CTadsWinScroll::do_command(notify_code, command_id, ctl);
    }
}

/*
 *   Create and open a new entry field. 
 */
void CHtmlDbgExprWin::open_entry_field(const CHtmlRect *text_area,
                                       CHtmlDbgExpr *expr,
                                       htmlexpr_winpart_t part)
{
    /* create the field */
    entry_fld_ = CreateWindow("EDIT", "",
                              WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL
                              | WS_BORDER | WS_TABSTOP,
                              text_area->left, text_area->top,
                              text_area->right - text_area->left,
                              text_area->bottom - text_area->top,
                              handle_, 0,
                              CTadsApp::get_app()->get_instance(), 0);

    /* subclass the field instance so we can hook certain messages */
    entry_fld_defproc_ =
        (WNDPROC)SetWindowLong(entry_fld_, GWL_WNDPROC, (LONG)&fld_proc);

    /* 
     *   set a property with my 'this' pointer, so that we can find it
     *   from our hook routine 
     */
    SetProp(entry_fld_, "CHtmlDbgExprWin.parent.this", (HANDLE)this);

    /* tell the field to use the expression font */
    SendMessage(entry_fld_, WM_SETFONT, (WPARAM)list_font_->get_handle(),
                MAKELPARAM(TRUE, 0));

    /* tell the expression to set up the field */
    expr->on_open_entry_field(entry_fld_, part);

    /* remember the expression being edited */
    entry_expr_ = expr;
    entry_part_ = part;

    /* set focus to the window and select the whole of its text */
    SetFocus(entry_fld_);
    SendMessage(entry_fld_, EM_SETSEL, 0, 32767);
}

/*
 *   subclassed window procedure - static version; we'll figure out our
 *   'this' pointer and call the non-static member function
 */
LRESULT CALLBACK CHtmlDbgExprWin::fld_proc(HWND hwnd, UINT msg, WPARAM wpar,
                                           LPARAM lpar)
{
    CHtmlDbgExprWin *win;

    /* get the 'this' pointer from the window properties */
    win = (CHtmlDbgExprWin *)
          GetProp(hwnd, "CHtmlDbgExprWin.parent.this");

    /* there's nothing we can do if we couldn't get the window */
    if (win == 0)
        return 0;

    /* invoke the non-static version */
    return win->do_fld_proc(hwnd, msg, wpar, lpar);
}

/*
 *   Subclassed window procedure for the edit field 
 */
LRESULT CHtmlDbgExprWin::do_fld_proc(HWND hwnd, UINT msg, WPARAM wpar,
                                     LPARAM lpar)
{
    /* check the message */
    switch(msg)
    {
    case WM_CHAR:
        switch(wpar)
        {
        case 9:
        case 10:
        case 13:
        case 27:
            /* 
             *   tab, enter, escape - send the parent window the key for
             *   handling 
             */
            PostMessage(handle_, WM_CHAR, wpar, lpar);

            /* no more handling required */
            return 0;
        }
        break;
    }
    
    /* call original handler */
    return CallWindowProc((WNDPROC)entry_fld_defproc_, hwnd, msg, wpar, lpar);
}

/*
 *   Receive notification that focus is leaving the entry field.  We'll
 *   close the entry field and apply changes. 
 */
void CHtmlDbgExprWin::on_entry_field_killfocus()
{
    HWND fld;

    /* remember the entry field locally, so we can forget the member */
    fld = entry_fld_;

    /* 
     *   immediately forget the entry field in the member, so that if
     *   there should be any re-entry, we won't try to recursively delete
     *   the field 
     */
    entry_fld_ = 0;
    
    /* update the value */
    entry_expr_->on_close_entry_field(this, fld, entry_part_,
                                      accept_entry_update_);

    /* restore the text field's original window procedure */
    SetWindowLong(fld, GWL_WNDPROC, (LONG)entry_fld_defproc_);

    /* remove our property from the text field */
    RemoveProp(fld, "CHtmlDbgExprWin.parent.this");

    /* 
     *   Focus might be going somewhere else entirely - since we draw the
     *   window as though we have focus when our entry field has focus, we
     *   need to make sure to redraw when the entry field loses focus.
     *   Focus might indeed be coming back to our window at this point,
     *   but in case it's not, make sure we redraw without focus; if the
     *   focus does come back, we'll be notified of that and everything
     *   will be fixed up properly, so this extra precaution will be
     *   harmless.  
     */
    do_killfocus(0);
    
    /* destroy the field's window */
    DestroyWindow(fld);

    /*
     *   If this was the last entry, and it's now has a non-empty
     *   expression part, and we're keeping around a blank last entry, add
     *   a new blank entry at the end of our list, since the last one has
     *   just been used up.  Don't add another entry if this window only
     *   allows a single top-level expression.  
     */
    if (has_blank_entry_
        && (last_expr_ == 0
            || (!last_expr_->is_expr_empty() && !single_entry_)))
    {
        CHtmlDbgExpr *new_expr;
        long ypos;
        
        /* create a new blank expression */
        ypos = (last_expr_ == 0 ? 0
                : last_expr_->get_y_pos() + last_expr_->get_height());
        new_expr = new CHtmlDbgExpr(1, "", 0, 0, "", "", 0,
                                    FALSE, TRUE, ypos, this);

        /* link it into our list */
        new_expr->prev_ = last_expr_;
        new_expr->next_ = 0;
        if (last_expr_ == 0)
            first_expr_ = new_expr;
        else
            last_expr_->next_ = new_expr;
        last_expr_ = new_expr;

        /* make sure it gets drawn */
        inval_expr(new_expr);

        /* update scrollbars for the change */
        adjust_scrollbar_ranges();
    }

    /* 
     *   immediately update the window, in case we lost focus due to some
     *   kind of tracking event that will perform XOR-style drawing (such
     *   as moving a docking window) 
     */
    UpdateWindow(handle_);
}

/*
 *   Add an expression at the end of our list.  If there's a blank entry
 *   expression, we'll insert the new expression just before the blank
 *   entry, otherwise we'll put it at the very end of the list.  
 */
void CHtmlDbgExprWin::add_expr(const textchar_t *expr, int can_edit_expr,
                               int select_new_item)
{
    CHtmlDbgExpr *new_expr;
    long ypos;
    CHtmlDbgExpr *prv;
    CHtmlDbgExpr *nxt;

    /* 
     *   figure the position of the new entry - if there's a blank item,
     *   it's at the position that the blank entry currently has,
     *   otherwise it's just below the last item 
     */
    ypos = (last_expr_ == 0 ? 0 : last_expr_->get_y_pos());
    if (!has_blank_entry_ && last_expr_ != 0)
        ypos += last_expr_->get_height();

    /* create the new expression */
    new_expr = new CHtmlDbgExpr(1, "", 0, 0, "", "", 0,
                                FALSE, can_edit_expr, ypos, this);

    /*
     *   figure out where it goes in the list: just before the last item
     *   if we have a blank item, otherwise at the end of the list 
     */
    if (has_blank_entry_ && last_expr_ != 0)
    {
        prv = last_expr_->prev_;
        nxt = last_expr_;
    }
    else
    {
        prv = last_expr_;
        nxt = 0;
    }

    /* link it into our list */
    new_expr->prev_ = prv;
    new_expr->next_ = nxt;
    if (prv != 0)
        prv->next_ = new_expr;
    else
        first_expr_ = new_expr;
    if (nxt != 0)
        nxt->prev_ = new_expr;
    else
        last_expr_ = new_expr;

    /* fix positions of everything below this one */
    fix_positions(new_expr);

    /* evalute the new expression */
    new_expr->accept_expr(this, expr, TRUE);

    /* select the new item and scroll it into view if desired */
    if (select_new_item)
    {
        set_selection(new_expr, TRUE, HTMLEXPR_WINPART_EXPR);
        scroll_focus_into_view();
    }
}

/*
 *   paint the window 
 */
void CHtmlDbgExprWin::do_paint_content(HDC hdc,
                                       const RECT *area_to_draw)
{
    long sep_xpos;
    CHtmlDbgExpr *expr;
    RECT rc, textrc;
    HPEN oldpen;
    static const char expr_lbl[] = "Expression";
    static const char val_lbl[] = "Value";
    int oldbkmode;
    COLORREF oldtextcolor;
    HGDIOBJ oldfont;

    /* get the separator x position in screen coordinates */
    sep_xpos = doc_to_screen_x(sep_xpos_);

    /* select the list font for drawing the list items */
    oldfont = list_font_->select(hdc);

    /* 
     *   run through the list, and paint each item overlapping the drawing
     *   area 
     */
    for (expr = first_expr_ ; expr != 0 ; expr = expr->get_next_expr())
        expr->draw(hdc, this, area_to_draw, sep_xpos, hdr_height_,
                   expr == selection_ && have_focus_);

    /* 
     *   redraw the focus rectangle - it's xor'd over the window, so it's
     *   gone (in the repainted part) even if we had already drawn it in
     *   the past 
     */
    drawn_focus_ = FALSE;
    draw_focus(TRUE);

    /* 
     *   Draw the header line.  Note that this doesn't scroll, so we
     *   always draw this at the top of the window.  
     */
    GetClientRect(handle_, &rc);
    rc.bottom = rc.top + hdr_height_;
    FillRect(hdc, &rc, GetSysColorBrush(COLOR_3DFACE));

    /* select the header font */
    hdr_font_->select(hdc);

    /* draw text transparently in the normal text color */
    oldtextcolor = SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
    oldbkmode = SetBkMode(hdc, TRANSPARENT);

    /* put in the header labels */
    textrc = rc;
    textrc.left = doc_to_screen_x(textrc.left);
    textrc.right = sep_xpos - 1;
    ExtTextOut(hdc, textrc.left + 2, textrc.top, ETO_CLIPPED, &textrc,
               expr_lbl, strlen(expr_lbl), 0);
    textrc = rc;
    textrc.left = sep_xpos + 1;
    ExtTextOut(hdc, textrc.left + 2, textrc.top, ETO_CLIPPED, &textrc,
               val_lbl, strlen(val_lbl), 0);

    /* draw the separator line in the header */
    oldpen = (HPEN)SelectObject(hdc, (HPEN)GetStockObject(BLACK_PEN));
    MoveToEx(hdc, sep_xpos, rc.top, 0);
    LineTo(hdc, sep_xpos, rc.bottom);
    SelectObject(hdc, (HPEN)GetStockObject(WHITE_PEN));
    MoveToEx(hdc, sep_xpos + 1, rc.top, 0);
    LineTo(hdc, sep_xpos + 1, rc.bottom);

    /* draw a white line at the top and left of the header */
    SelectObject(hdc, (HPEN)GetStockObject(WHITE_PEN));
    MoveToEx(hdc, doc_to_screen_x(rc.left), rc.bottom - 1, 0);
    LineTo(hdc, doc_to_screen_x(rc.left), rc.top);
    LineTo(hdc, rc.right, rc.top);

    /* draw the black line at the bottom and right of the header */
    SelectObject(hdc, (HPEN)GetStockObject(BLACK_PEN));
    MoveToEx(hdc, doc_to_screen_x(1), rc.bottom - 1, 0);
    LineTo(hdc, rc.right - 1, rc.bottom - 1);
    LineTo(hdc, rc.right - 1, rc.top + 1);

    /* restore original pen, background mode, and text color */
    SelectObject(hdc, oldpen);
    SetBkMode(hdc, oldbkmode);
    SetTextColor(hdc, oldtextcolor);

    /* restore old font */
    list_font_->unselect(hdc, oldfont);
}

/*
 *   scroll the focus item into view
 */
void CHtmlDbgExprWin::scroll_focus_into_view()
{
    RECT rc;
    CHtmlRect win_bounds;
    long item_top, item_bottom;
    long target_left, target_right;
    
    /* if there's no focus item, there's nothing to do */
    if (focus_expr_ == 0)
        return;

    /* get the bounds of the window area, in document coordinates */
    get_scroll_area(&rc, TRUE);
    win_bounds.set(rc.left, rc.top - hdr_height_,
                   rc.right, rc.bottom - hdr_height_);
    win_bounds = screen_to_doc(win_bounds);

    /* determine if we need to scroll vertically */
    item_top = focus_expr_->get_y_pos();
    item_bottom = item_top + focus_expr_->get_height();
    if (item_top < win_bounds.top)
    {
        /* 
         *   the top of the item is above the top of the window - scroll
         *   so that the item is at the top of the window 
         */
        do_scroll(TRUE, vscroll_, SB_THUMBPOSITION,
                  item_top / vscroll_units_, TRUE);
    }
    else if (item_bottom > win_bounds.bottom)
    {
        /*
         *   The bottom of the item is below the bottom of the window -
         *   scroll so that the item is some integral number of scrolling
         *   units from the top of the window, and entirely fits.  If the
         *   item's top is already at the top of the window, don't do
         *   anything, since in this case the item simply won't fit in the
         *   window; leave it hanging below in this case.  
         */
        if (item_top > win_bounds.top)
        {
            long delta;

            /* scroll down until we're in view */
            for (delta = vscroll_units_ ;
                 item_bottom - delta > win_bounds.bottom
                     && item_top - delta > win_bounds.top ;
                 delta += vscroll_units_) ;

            /* set the new position */
            do_scroll(TRUE, vscroll_, SB_THUMBPOSITION,
                      vscroll_ofs_ + delta/vscroll_units_, TRUE);
                }
    }

    /*
     *   If the entire focus part is out of view horizontally, scroll
     *   horizontally to the start of its column.  
     */
    switch(focus_part_)
    {
    case HTMLEXPR_WINPART_EXPR:
        /* scroll to left edge */
        if (hscroll_ofs_ != 0)
            do_scroll(FALSE, hscroll_, SB_THUMBPOSITION, 0, TRUE);
        break;

    case HTMLEXPR_WINPART_VAL:
        /* figure out where the area goes */
        target_left = sep_xpos_;
        target_right = screen_to_doc_x(rc.right);

        /* 
         *   Make sure the target area is in view, and that at least 100
         *   pixels are available.  If we're scrolled to the right of the
         *   separator bar, scroll to the separator bar.  
         */
        if (target_left > win_bounds.right
            || target_right - target_left < 100
            || target_left < win_bounds.left)
            do_scroll(FALSE, hscroll_, SB_THUMBPOSITION,
                      target_left / get_hscroll_units(), TRUE);
        break;

    default:
        /* nothing else counts - don't do any scrolling */
        break;
    }

}

/*
 *   Receive notification that scrolling is about to occur.  Remove the
 *   focus rectangle, since it uses xor drawing and hence doesn't
 *   translate well during scrolling. 
 */
void CHtmlDbgExprWin::notify_pre_scroll(HWND)
{
    draw_focus(FALSE);
}

/*
 *   Receive notification that scrolling has occurred.  If we have an open
 *   entry field, move and resize it to account for the scrolling. 
 */
void CHtmlDbgExprWin::notify_scroll(HWND hwnd, long oldpos, long newpos)
{
    /* put back the focus drawing */
    draw_focus(TRUE);

    /* simply close any active entry field */
    close_entry_field(TRUE);
    
#if 0 // leave this out for now; it doesn't quite work like you'd want it to
    RECT fldrc;
    POINT fldpos;

    /* inherit the default behavior */
    CTadsWinScroll::notify_scroll(hwnd, oldpos, newpos);
    
    /* if there's no active entry field, there's nothing more to do */
    if (entry_fld_ == 0)
        return;
    
    /* get the current position of the field relative to the parent */
    GetWindowRect(entry_fld_, &fldrc);
    fldpos.x = fldrc.left;
    fldpos.y = fldrc.top;
    ScreenToClient(handle_, &fldpos);

    /* determine whether it's horizontal or vertical */
    if (hwnd == vscroll_)
    {
        /* it's vertical - move the field vertically to compensate */
        MoveWindow(entry_fld_, fldpos.x,
                   fldpos.y + (oldpos - newpos) * get_vscroll_units(),
                   fldrc.right - fldrc.left,
                   fldrc.bottom - fldrc.top, TRUE);
    }
    else
    {
        RECT winrc;

        /* get the parent window area */
        GetClientRect(handle_, &winrc);

        /* 
         *   move the field, but keep either edge where it is if it's
         *   aligned against the window's edge 
         */
        if (fldpos.x > 0)
            fldpos.x += (oldpos - newpos) * get_hscroll_units();
        if (fldpos.x <= 0)
            fldpos.x = 0;
        if (fldrc.right >= winrc.right)
            fldrc.right -= (oldpos - newpos) * get_hscroll_units();

        /* move the field to its new position */
        MoveWindow(entry_fld_,
                   fldpos.x + (oldpos - newpos)*get_hscroll_units(),
                   fldpos.y, fldrc.right - fldrc.left,
                   fldrc.bottom - fldrc.top, TRUE);
    }
#endif
}

/*
 *   Adjust scrollbar ranges 
 */
void CHtmlDbgExprWin::adjust_scrollbar_ranges()
{
    long height;
    long width;
    long pageheight;
    SCROLLINFO info;
    RECT rc;

    /* get my client area */
    GetClientRect(handle_, &rc);
    
    /* get the bottom y offset of the last item */
    height = (last_expr_ != 0
              ? last_expr_->get_y_pos() + last_expr_->get_height() : 0);
    height /= (vscroll_units_ == 0 ? 1 : vscroll_units_);

    /* figure out how many items fit in the window */
    pageheight = (rc.bottom - rc.top - hdr_height_) / vscroll_units_;

    /* use a fixed scrolling width */
    width = 1000;

    /* set the generic scroll info */
    get_scroll_info(TRUE, &info);
    info.nMin = 0;
    info.fMask |= SIF_DISABLENOSCROLL;

    /* if the current scroll offset is too high, move it within range */
    if (vscroll_ofs_ > height - (pageheight == 0 ? 0 : pageheight - 1))
    {
        /* put it within range */
        vscroll_ofs_ = height - (pageheight == 0 ? 0 : pageheight - 1);
        if (vscroll_ofs_ < 0)
            vscroll_ofs_ = 0;

        /* update the window to account for the change */
        InvalidateRect(handle_, 0, TRUE);
    }

    /* set the vertical scroll info */
    if (vscroll_ != 0)
    {
        info.nMax = height;
        info.nPage = pageheight;
        info.nPos = vscroll_ofs_;
        SetScrollInfo(vscroll_, SB_CTL, &info, TRUE);
    }

    /* set the horizontal scroll info */
    if (hscroll_ != 0)
    {
        info.nMax = width;
        info.nPage = rc.right;
        info.nPos = hscroll_ofs_;
        SetScrollInfo(hscroll_, SB_CTL, &info, TRUE);
    }
}

/*
 *   get the scrolling area 
 */
void CHtmlDbgExprWin::get_scroll_area(RECT *rc, int vertical) const
{
    /* get the normal scrolling area */
    CTadsWinScroll::get_scroll_area(rc, vertical);

    /* don't scroll the header area if scrolling vertically */
    if (vertical)
        rc->top += hdr_height_;
}

/*
 *   determine what part of the window contains a given point 
 */
htmlexpr_winpart_t CHtmlDbgExprWin::pos_to_item(int x, int y,
                                                CHtmlDbgExpr **expr,
                                                CHtmlRect *text_area)
{
    CHtmlPoint pos;

    /* get the position in document coordinates */
    pos.set(x, y);
    pos = screen_to_doc(pos);

    /* offset the document position to account for the header size */
    pos.y -= hdr_height_;
    
    /* 
     *   Check to see if it's in the header; if not, determine which
     *   expression line contains it.  Note that the header doesn't
     *   scroll, so we need to check the position in window coordinates
     *   (not doc coordinates) to determine if we're in the header.  
     */
    if (y < hdr_height_)
    {
        /* no expression */
        if (expr != 0)
            *expr = 0;

        /* determine which part we're in horizontally */
        if (pos.x < sep_xpos_ - 1)
        {
            /* we're in the expression portion */
            return HTMLEXPR_WINPART_HDR_EXPR;
        }
        else if (pos.x > sep_xpos_ + 2)
        {
            /* we're in the value portion */
            return HTMLEXPR_WINPART_HDR_VAL;
        }
        else
        {
            /* we're in the separator bar */
            return HTMLEXPR_WINPART_HDR_SEP;
        }
    }
    else
    {
        CHtmlDbgExpr *cur;
        
        /* find the expression */
        for (cur = first_expr_ ; cur != 0 ; cur = cur->get_next_expr())
        {
            /* check if it's in this item - if so, stop looking */
            if (pos.y >= cur->get_y_pos()
                && pos.y < cur->get_y_pos() + cur->get_height())
                break;
        }

        /* tell the caller the item we found */
        if (expr != 0)
            *expr = cur;

        /* if we didn't find the item, return the null part ID */
        if (cur == 0)
            return HTMLEXPR_WINPART_BLANK;

        /* translate the position */
        return pos_to_area(cur, pos.x, text_area);
    }
}

/*
 *   Get the part and screen area of a given x position in a given item 
 */
htmlexpr_winpart_t CHtmlDbgExprWin::pos_to_area(CHtmlDbgExpr *expr,
                                                int xpos,
                                                CHtmlRect *text_area)
{
    RECT rc;
    long xl, xr;
    htmlexpr_winpart_t ret;

    /* 
     *   calculate the left and right bounds of the window in document
     *   coordinates 
     */
    GetClientRect(handle_, &rc);
    xl = screen_to_doc_x(rc.left);
    xr = screen_to_doc_x(rc.right - GetSystemMetrics(SM_CXVSCROLL));
    
    /* ask the item for the appropriate area */
    ret = expr->pos_to_area(xpos, sep_xpos_, &xl, &xr);

    /* fill in the caller's text area, if desired */
    if (text_area != 0)
    {
        /* set up the area */
        text_area->left = xl;
        text_area->right = xr;
        text_area->top = expr->get_y_pos();
        text_area->bottom = text_area->top + expr->get_height();

        /* switch to screen coordinates, and adjust for header offset */
        *text_area = doc_to_screen(*text_area);
        text_area->offset(0, hdr_height_);
    }

    /* return the result we got from the item */
    return ret;
}

/*
 *   Set the selection, removing any previous selection 
 */
void CHtmlDbgExprWin::set_selection(CHtmlDbgExpr *expr, int set_focus,
                                    htmlexpr_winpart_t focus_part)
{
    /* 
     *   if we currently have a selection, invalidate it so that it gets
     *   redrawn without selection status 
     */
    if (selection_ != 0)
        inval_expr(selection_);

    /* set the new selection */
    selection_ = expr;

    /* if we have a new selection, make sure it's redrawn */
    if (selection_ != 0)
        inval_expr(selection_);

    /* set the focus to the new selection, if desired */
    if (set_focus)
    {
        /* see if we have a selection */
        if (selection_ == 0)
        {
            /* no selection - clear the focus */
            set_focus_expr(0, HTMLEXPR_WINPART_EXPR);
        }
        else
        {
            /* see if this part allows focus */
            if (selection_->can_edit_part(focus_part))
            {
                /* set focus here */
                set_focus_expr(selection_, focus_part);
            }
            else
            {
                /* try the other part */
                focus_part = (focus_part == HTMLEXPR_WINPART_EXPR
                              ? HTMLEXPR_WINPART_VAL : HTMLEXPR_WINPART_EXPR);
                if (selection_->can_edit_part(focus_part))
                {
                    /* set focus here */
                    set_focus_expr(selection_, focus_part);
                }
                else
                {
                    /* nothing takes focus - clear the focus */
                    set_focus_expr(0, HTMLEXPR_WINPART_EXPR);
                }
            }
        }
    }
}

/*
 *   Begin updating a child list of a given item.  If the parent is null,
 *   we'll update the root items.  In this routine, we'll mark all
 *   children of the given parent as pending refresh; during refresh,
 *   we'll mark children as updated as they're added back into the list.
 *   When the refresh is finished, we'll remove any children that weren't
 *   refreshed, since these are no longer in the parent's list. 
 */
void CHtmlDbgExprWin::begin_refresh(CHtmlDbgExpr *parent)
{
    int parent_level;
    CHtmlDbgExpr *cur;

    /* get the parent's level - stop when we reach it again */
    parent_level = (parent == 0 ? 0 : parent->get_level());

    /* go through the child list, and mark each child as pending refresh */
    for (cur = (parent == 0 ? first_expr_ : parent->get_next_expr()) ;
         cur != 0 && cur->get_level() > parent_level ;
         cur = cur->get_next_expr())
        cur->set_pending_refresh(TRUE);
}

/*
 *   Add or replace an expression to the window.  The item is added as a
 *   child of the given parent; if the parent is null, the item is added
 *   at the top level.  If the expression is already in the child list of
 *   the parent item, we'll simply update the value of the item. 
 */
void CHtmlDbgExprWin::do_refresh(CHtmlDbgExpr *parent,
                                 const textchar_t *expr, size_t exprlen,
                                 const textchar_t *relationship)
{
    int parent_level;
    CHtmlDbgExpr *cur;
    CHtmlDbgExpr *prv;
    long ypos;
    CHtmlDbgExpr *new_expr;

    /* get the parent's level - stop when we reach it again */
    parent_level = (parent == 0 ? 0 : parent->get_level());

    /*
     *   Find the item in the child list of the given parent.  If we find
     *   that it's already present, mark it as refreshed and update its
     *   value; otherwise, we must add a new child.  
     */
    for (cur = (parent == 0 ? first_expr_ : parent->get_next_expr()) ;
         cur != 0 && cur->get_level() > parent_level ;
         cur = cur->get_next_expr())
    {
        /* check if this one's expression matches the new one */
        if (get_strlen(cur->get_expr()) == exprlen
            && memcmp(expr, cur->get_expr(), exprlen) == 0)
        {
            /* update this item's value */
            update_eval(cur);
            
            /* this one has been fully refreshed */
            cur->set_pending_refresh(FALSE);

            /* no need to look any further */
            return;
        }
    }

    /*
     *   This item is not in the parent's child list.  cur points to the
     *   first item following the parent's child list; insert a new item
     *   before cur.  Note that the expression isn't editable, because
     *   it's generated by an enumeration of some kind.  
     */
    prv = (cur == 0 ? last_expr_ : cur->get_prev_expr());
    ypos = (prv == 0 ? 0 : prv->get_y_pos() + prv->get_height());
    new_expr = new CHtmlDbgExpr(parent_level + 1, expr, exprlen,
                                parent, relationship, "", 0,
                                FALSE, FALSE, ypos, this);

    /* link the item in after the previous item */
    new_expr->prev_ = prv;
    new_expr->next_ = cur;
    if (prv == 0)
        first_expr_ = new_expr;
    else
        prv->next_ = new_expr;
    if (cur == 0)
        last_expr_ = new_expr;
    else
        cur->prev_ = new_expr;

    /* update this item's value */
    update_eval(new_expr);

    /* this item has been refreshed */
    new_expr->set_pending_refresh(FALSE);
}

/*
 *   End a refresh operation.  Any children of the parent that haven't
 *   been refreshed are deleted at this point, since they're evidently no
 *   longer members of the parent item.
 */
void CHtmlDbgExprWin::end_refresh(CHtmlDbgExpr *parent)
{
    int parent_level;
    CHtmlDbgExpr *cur;
    CHtmlDbgExpr *nxt;

    /* get the parent's level - stop when we reach it again */
    parent_level = (parent == 0 ? 0 : parent->get_level());

    /* 
     *   go through the child list; any child that is still pending
     *   refresh refresh is to be deleted, since it's no longer in the
     *   parent's list 
     */
    for (cur = (parent == 0 ? first_expr_ : parent->get_next_expr()) ;
         cur != 0 && cur->get_level() > parent_level ;
         cur = nxt)
    {
        /* remember the next item, in case we delete this one */
        nxt = cur->get_next_expr();
        
        /* 
         *   If this one is still pending refresh, delete it.  Keep it if
         *   it's the blank last item in a window with a blank entry item. 
         */
        if (cur->is_pending_refresh()
            && !(cur->get_next_expr() == 0 && cur->get_level() == 1
                 && has_blank_entry_ && get_strlen(cur->get_expr()) == 0))
        {
            /* delete this item */
            nxt = delete_expr(cur);
        }
    }

    /* fix up the positions */
    fix_positions(parent);
}

/*
 *   Fix positions of children of a given parent.  This should be called
 *   whenever items are inserted or deleted from a parent's list. 
 */
void CHtmlDbgExprWin::fix_positions(CHtmlDbgExpr *parent)
{
    CHtmlDbgExpr *cur;
    long ypos;

    /*
     *   Since the child list may have grown or shrunk, make sure
     *   everything is positioned properly 
     */
    ypos = (parent == 0 ? 0 : parent->get_y_pos() + parent->get_height());
    for (cur = (parent == 0 ? first_expr_ : parent->get_next_expr()) ;
         cur != 0 ; cur = cur->get_next_expr())
    {
        /* if this item is at the wrong position, move it */
        if (cur->get_y_pos() != ypos)
        {
            /* invalidate its old position */
            inval_expr(cur);
            
            /* move it to its new position */
            cur->set_y_pos(ypos);

            /* make sure it gets redrawn at the new position */
            inval_expr(cur);
        }

        /* get the next position */
        ypos += cur->get_height();
    }

    /* adjust scrollbars for the change */
    adjust_scrollbar_ranges();
}


/* callback context for update_eval_child_cb */
struct update_eval_cbctx_t
{
    /* expression window */
    CHtmlDbgExprWin *win_;

    /* parent expression */
    CHtmlDbgExpr *parent_;
};

/*
 *   Update evaluation of an expression 
 */
void CHtmlDbgExprWin::update_eval(CHtmlDbgExpr *expr)
{
    textchar_t buf[2048];
    int is_lval;
    int is_openable;
    int err;
    update_eval_cbctx_t cbctx;
    
    /* we can only evaluate expressions if the debugger has control */
    if (!mainwin_->get_in_debugger())
        return;

    /* start a refresh of the child list */
    begin_refresh(expr);

    /* ask the helper to evaluate the expression */
    cbctx.win_ = this;
    cbctx.parent_ = expr;
    err = helper_->eval_expr(mainwin_->get_dbg_ctx(), buf, sizeof(buf),
                             expr->get_full_expr(), &is_lval, &is_openable,
                             expr->is_open() ? update_eval_child_cb : 0,
                             &cbctx, FALSE);

    /* finish the child list refresh */
    end_refresh(expr);

    /* if the openable state of the object changed, change it on-screen */
    if ((is_openable != 0) != (expr->is_openable() != 0))
    {
        /* set the new state */
        expr->set_openable(is_openable);

        /* if it was previously open and it's no longer openable, close it */
        if (!is_openable && expr->is_open())
            expr->set_open(FALSE);
    }

    /* set the value in the item */
    expr->set_val(buf);

    /* invalidate it */
    inval_expr(expr);

    /* 
     *   if the expression is an lval, allow editing the value; otherwise
     *   make the value read-only 
     */
    expr->set_can_edit_val(err == 0 && is_lval);
}


/*
 *   Change the value of an expression.  This will attempt to assign the
 *   new value specified into the expression. 
 */
void CHtmlDbgExprWin::change_expr_value(CHtmlDbgExpr *expr,
                                        const textchar_t *newval)
{
    /* execute the assignment */
    helper_->eval_asi_expr(mainwin_->get_dbg_ctx(),
                           expr->get_full_expr(), newval);

    /* 
     *   re-evaluate the expression, which will reflect the new value
     *   (formatted canonically, which may not be the case with what the
     *   user entered), or will reflect the original value if an error
     *   occurred setting the new value 
     */
    expr->update_eval(this);
}

/*
 *   Callback for building a child list of an item 
 */
void CHtmlDbgExprWin::update_eval_child_cb(void *ctx0,
                                           const char *subitemname,
                                           int subitemnamelen,
                                           const char *relationship)
{
    update_eval_cbctx_t *ctx = (update_eval_cbctx_t *)ctx0;

    /* refresh this child item */
    ctx->win_->do_refresh(ctx->parent_, subitemname, subitemnamelen,
                          relationship);
}

/*
 *   Delete an expression and its children from the list 
 */
CHtmlDbgExpr *CHtmlDbgExprWin::delete_expr(CHtmlDbgExpr *expr)
{
    CHtmlDbgExpr *retval;
    CHtmlDbgExpr *nxt;
    long ypos;

    /* if this is the pending open expression, forget that */
    if (expr == pending_open_expr_)
        pending_open_expr_ = 0;

    /* 
     *   Find the next item that isn't a child of this expression - this
     *   is the next item at or outside the indent level of this item.  
     */
    for (nxt = expr->get_next_expr() ; nxt != 0 ; nxt = nxt->get_next_expr())
    {
        /* 
         *   if this one's level is at or outside the original one, it's
         *   not a child 
         */
        if (nxt->get_level() <= expr->get_level())
            break;
    }

    /* remember the position of the first deleted item */
    ypos = expr->get_y_pos();

    /* 
     *   delete each item from the first to but not including the next
     *   non-child 
     */
    while (expr != nxt)
    {
        CHtmlDbgExpr *cur;

        /* move focus out of this item */
        if (focus_expr_ == expr)
            go_next_focus(FALSE, FALSE);

        /* 
         *   if we're deleting the current selection, eliminate any
         *   selection (usually, moving the focus will also move the
         *   selection; the only time we won't have already updated the
         *   selection is when we're deleting the last item, in which case
         *   there's nothing left to focus on and hence nothing left to
         *   select) 
         */
        if (selection_ == expr)
            set_selection(0, FALSE, HTMLEXPR_WINPART_BLANK);

        /* remember this one */
        cur = expr;
        
        /* update the screen area of the deleted item */
        inval_expr(cur);

        /* unlink this one from the list */
        if (expr == first_expr_)
            first_expr_ = expr->next_;
        else
            expr->prev_->next_ = expr->next_;
        if (expr == last_expr_)
            last_expr_ = expr->prev_;
        else
            expr->next_->prev_ = expr->prev_;

        /* advance our counter to the next one */
        expr = expr->next_;

        /* delete this one */
        delete cur;
    }

    /* we return the next value expression remaining after the one deleted */
    retval = nxt;

    /*
     *   Now, go through the expressions starting with the one immediately
     *   following the deletion, and fix up the positions to fill the gap
     *   left by the deletion. 
     */
    for ( ; nxt != 0 ; nxt = nxt->get_next_expr())
    {
        /* invalidate the original area of this item */
        inval_expr(nxt);
        
        /* move this one to the next available position */
        nxt->set_y_pos(ypos);

        /* invalidate the new area of this item */
        inval_expr(nxt);

        /* advance our counter to the next position */
        ypos += nxt->get_height();
    }

    /* update scrollbars for the change */
    adjust_scrollbar_ranges();

    /* return the next valid expression remaining */
    return retval;
}

/*
 *   invalidate the screen area of an expression item 
 */
void CHtmlDbgExprWin::inval_expr(CHtmlDbgExpr *expr)
{
    RECT rc;
    
    /* 
     *   Calculate the item's screen position.  The item goes across the
     *   whole window horizontally.  We can calculate the vertical
     *   position by adjusting the item's y position to screen
     *   coordinates, then offsetting by the header height (since the
     *   header never scrolls).  
     */
    GetClientRect(handle_, &rc);
    rc.top = doc_to_screen_y(expr->get_y_pos()) + hdr_height_;
    rc.bottom = rc.top + expr->get_height();

    /* invalidate the area */
    InvalidateRect(handle_, &rc, TRUE);
}

/*
 *   Draw the separator bar during tracking.  If 'add' is true, we'll add
 *   the separator, otherwise we'll remove it.  
 */
void CHtmlDbgExprWin::draw_track_sep(int /*add*/)
{
    long x;
    RECT rc;
    int oldrop;
    HPEN oldpen, pen;
    HDC hdc;

    /* get my DC */
    hdc = GetDC(handle_);
    
    /* calculate the position in screen coordinates */
    x = doc_to_screen_x(track_sep_x_);

    /* get the window's bounding box */
    GetClientRect(handle_, &rc);

    /* draw the line in xor mode */
    oldrop = SetROP2(hdc, R2_XORPEN);

    /* create a pen for drawing the line */
    pen = CreatePen(PS_SOLID, 2, RGB(0xff, 0xff, 0xff));
    oldpen = (HPEN)SelectObject(hdc, pen);

    /* draw the line */
    MoveToEx(hdc, x, 0, 0);
    LineTo(hdc, x, rc.bottom);

    /* done with the pen */
    SelectObject(hdc, oldpen);
    DeleteObject(pen);

    /* restore old mode */
    SetROP2(hdc, oldrop);

    /* done with the dc */
    ReleaseDC(handle_, hdc);
}

/*
 *   close the entry field, if we have one 
 */
void CHtmlDbgExprWin::close_entry_field(int accept_new_val)
{
    /* note whether we're accepting the change */
    accept_entry_update_ = accept_new_val;

    /* if we have a field, close it by setting focus to myself */
    if (entry_fld_ != 0)
        SetFocus(handle_);
}

/* 
 *   mouse button down 
 */
int CHtmlDbgExprWin::do_leftbtn_down(int /*keys*/, int x, int y,
                                     int /*clicks*/)
{
    CHtmlDbgExpr *expr;
    CHtmlRect text_area;
    htmlexpr_winpart_t part;

    /* set focus on me */
    SetFocus(handle_);

    /* presume we will not open anything on mouse up */
    pending_open_expr_ = 0;
    
    /* if we have an active entry field, close it */
    close_entry_field(TRUE);

    /* find out what we're clicking on */
    part = pos_to_item(x, y, &expr, &text_area);
    switch(part)
    {
    case HTMLEXPR_WINPART_SEP:
    case HTMLEXPR_WINPART_HDR_SEP:
        /* capture the mouse while tracking */
        SetCapture(handle_);
        
        /* begin tracking the separator line */
        track_start_x_ = screen_to_doc_x(x);
        track_sep_x_ = screen_to_doc_x(x);
        tracking_sep_ = TRUE;

        /* draw the separator at the current position  */
        draw_track_sep(TRUE);
        break;

    case HTMLEXPR_WINPART_OPENCLOSE:
        /*
         *   it's an open/close box - select the item, and reverse its
         *   open/close state 
         */
        if (expr != selection_)
            set_selection(expr, TRUE, HTMLEXPR_WINPART_EXPR);
        expr->invert_open_state(this);

        /* update the scrollbars */
        adjust_scrollbar_ranges();
        break;

    case HTMLEXPR_WINPART_EXPRMARGIN:
        /* 
         *   we're in the margin of an expression - select the expression
         *   item 
         */
        if (expr != selection_)
            set_selection(expr, TRUE, HTMLEXPR_WINPART_EXPR);

        /* allow dragging, if this isn't the empty one */
        if (!has_blank_entry_ || expr != last_expr_)
            drag_prepare(FALSE);
        break;

    case HTMLEXPR_WINPART_EXPR:
    case HTMLEXPR_WINPART_VAL:
        /* check if the item is selected */
        if (expr == selection_)
        {
            /* 
             *   the item is already selected - activate the part they
             *   clicked on if possible 
             */
            if (expr->can_edit_part(part))
            {
                /* set up to open this part on mouse up */
                pending_open_expr_ = expr;
                pending_open_part_ = part;

                /* set focus on this part */
                focus_part_ = part;
            }
        }
        else
        {
            /* 
             *   the item isn't selected - remove the current selection
             *   and select the new item 
             */
            set_selection(expr, TRUE, part);
        }

        /* allow dragging, if this isn't the empty last entry */
        if (!has_blank_entry_ || expr != last_expr_)
            drag_prepare(FALSE);
        break;

    case HTMLEXPR_WINPART_BLANK:
        /* remove any current selection */
        set_selection(0, TRUE, HTMLEXPR_WINPART_BLANK);
        break;

    default:
        /* ignore clicks in other parts */
        return FALSE;
    }

    /* handled */
    return TRUE;
}

/*
 *   mouse button up 
 */
int CHtmlDbgExprWin::do_leftbtn_up(int /*keys*/, int /*x*/, int /*y*/)
{
    /* if we're tracking the separator bar, end it now */
    if (tracking_sep_)
    {
        end_tracking_sep(TRUE);
        return TRUE;
    }

    /* tell the drag tracker we released the mouse */
    drag_end(FALSE);

    /* if we were to open a field on mouse up, do so now */
    if (pending_open_expr_ != 0)
    {
        CHtmlRect text_area;

        /* scroll the focus into view before opening the next field */
        scroll_focus_into_view();
        
        /* open it */
        get_focus_text_area(&text_area, TRUE);
        open_entry_field(&text_area, pending_open_expr_,
                         pending_open_part_);

        /* the pending open expression is now consumed */
        pending_open_expr_ = 0;
    }

    /* not handled */
    return FALSE;
}

/*
 *   finish tracking the separator 
 */
void CHtmlDbgExprWin::end_tracking_sep(int release_capture)
{
    /* release capture if necessary */
    if (release_capture)
        ReleaseCapture();

    /* undraw the line */
    draw_track_sep(FALSE);
    
    /* note that we're done */
    tracking_sep_ = FALSE;
    
    /* set the new separator position, and redraw everything */
    sep_xpos_ = track_sep_x_;
    InvalidateRect(handle_, 0, TRUE);
}

/*
 *   mouse move 
 */
int CHtmlDbgExprWin::do_mousemove(int /*keys*/, int x, int /*y*/)
{
    /* continue tracking the separator line if appropriate */
    if (tracking_sep_)
    {
        /* un-draw the separator at its old position */
        draw_track_sep(FALSE);

        /* move it */
        track_sep_x_ += screen_to_doc_x(x) - track_start_x_;
        track_start_x_ = screen_to_doc_x(x);

        /* draw it at the new position */
        draw_track_sep(TRUE);

        /* handled */
        return TRUE;
    }

    /* check to see if a drag operation has begun */
    if (drag_check())
        return TRUE;

    /* not handled */
    return FALSE;
}


/*
 *   capture change 
 */
int CHtmlDbgExprWin::do_capture_changed(HWND /*new_capture_win*/)
{
    /* if we're tracking the separator bar, end it now */
    if (tracking_sep_)
    {
        end_tracking_sep(FALSE);
        return TRUE;
    }

    /* didn't do anything */
    return FALSE;
}

/*
 *   receive notification that dragging is about to start 
 */
void CHtmlDbgExprWin::drag_pre()
{
    /* don't open the current field after all */
    pending_open_expr_ = 0;
}

/*
 *   Get the data object to drag 
 */
IDataObject *CHtmlDbgExprWin::get_drag_dataobj()
{
    const textchar_t *full_expr;

    /* if there's no selection, we can't get anything to drag */
    if (selection_ == 0)
        return 0;
    
    /* get the full expression text of the current selection */
    full_expr = selection_->get_full_expr();

    /* create and return a text data object with the full expression */
    return new CTadsDataObjText(full_expr, get_strlen(full_expr));
}

/*
 *   right button down event 
 */
int CHtmlDbgExprWin::do_rightbtn_down(int keys, int x, int y,
                                      int /*clicks*/)
{
    CHtmlDbgExpr *expr;
    CHtmlRect text_area;
    htmlexpr_winpart_t part;

    /* if we have an active entry field, close it */
    close_entry_field(TRUE);

    /* find out what we're clicking on */
    part = pos_to_item(x, y, &expr, &text_area);
    switch(part)
    {
    case HTMLEXPR_WINPART_OPENCLOSE:
    case HTMLEXPR_WINPART_EXPRMARGIN:
    case HTMLEXPR_WINPART_EXPR:
    case HTMLEXPR_WINPART_VAL:
        /* select this item, and proceed to the context menu */
        set_selection(expr, TRUE, part);
        break;

    case HTMLEXPR_WINPART_BLANK:
        /* remove any current selection, and proceed to the context menu */
        set_selection(0, TRUE, HTMLEXPR_WINPART_BLANK);
        break;
        
    default:
        /* don't do any extra processing in other parts */
        return TRUE;
    }

    /* run the context menu */
    track_context_menu(expr_popup_, x, y);

    /* handled */
    return TRUE;
}


/*
 *   set cursor shape 
 */
int CHtmlDbgExprWin::do_setcursor(HWND hwnd, int /*hittest*/,
                                  int /*mousemsg*/)
{
    POINT pt;

    /* if it's not in my window, ignore the request */
    if (hwnd != handle_)
        return FALSE;
    
    /* if we're tracking the separator bar, use the splitter cursor */
    if (tracking_sep_)
    {
        SetCursor(split_ew_cursor_);
        return TRUE;
    }

    /* get the mouse position */
    GetCursorPos(&pt);
    ScreenToClient(handle_, &pt);

    /* get the part */
    switch(pos_to_item(pt.x, pt.y, 0, 0))
    {
    case HTMLEXPR_WINPART_HDR_SEP:
    case HTMLEXPR_WINPART_SEP:
        /* use the left-right splitter cursor for these */
        SetCursor(split_ew_cursor_);
        break;

    default:
        /* for all others, use the arrow cursor */
        SetCursor(arrow_cursor_);
        break;
    }

    /* handled */
    return TRUE;
}

/*
 *   resize the window 
 */
void CHtmlDbgExprWin::do_resize(int mode, int x, int y)
{
    /* kill the entry field, if we have one */
    close_entry_field(TRUE);

    /* inherit default behavior */
    CTadsWinScroll::do_resize(mode, x, y);
}

/*
 *   gain focus 
 */
void CHtmlDbgExprWin::do_setfocus(HWND /*prev_win*/)
{
    /* if we already have focus, there's nothing more to do */
    if (have_focus_)
        return;
    
    /* note that we have focus */
    have_focus_ = TRUE;

    /* redraw focused item */
    if (focus_expr_ != 0)
        inval_expr(focus_expr_);
    else if (selection_ != 0)
        inval_expr(selection_);

    /* 
     *   notify the parent window - if we're in a docking window, this
     *   lets the docking parent keep track of its activation status 
     */
    notify_parent_focus(TRUE);
}

/*
 *   lose focus 
 */
void CHtmlDbgExprWin::do_killfocus(HWND next_win)
{
    /* 
     *   if the focus is going to our entry field, continue to draw this
     *   window as though it has focus 
     */
    if (next_win == entry_fld_ && entry_fld_ != 0)
        return;
    
    /* redraw focused item */
    if (focus_expr_ != 0)
        inval_expr(focus_expr_);
    else if (selection_ != 0)
        inval_expr(selection_);
    
    /* note that we don't have focus */
    have_focus_ = FALSE;

    /* 
     *   notify the parent window - if we're in a docking window, this
     *   lets the docking parent keep track of its activation status 
     */
    notify_parent_focus(FALSE);

    /* 
     *   immediately update our window, in case we lost focus to something
     *   that will do XOR-style drawing during tracking (such as docking
     *   window motion)
     */
    UpdateWindow(handle_);
}

/*
 *   handle keystrokes 
 */
int CHtmlDbgExprWin::do_char(TCHAR ch, long keydata)
{
    CHtmlRect text_area;
    
    switch(ch)
    {
    case 27:
        /* escape - close the entry field, ignoring any change in the value */
        if (entry_fld_ != 0)
            close_entry_field(FALSE);

        /* 
         *   pass the key up to my parent window - if the parent is a
         *   dialog, it might want to use this keystroke to dismiss the
         *   dialog 
         */
        PostMessage(GetParent(handle_), WM_CHAR, (WPARAM)ch, (LPARAM)keydata);

        /* handled */
        return TRUE;

    case 8:
        /* process as though they hit the delete key */
        return do_keydown(VK_DELETE, 0);

    case 9:
        /* tab - move to the next field, closing the entry field if open */
        {
            int backtab;

            /* 
             *   note whether the shift key is down - if so, this is a
             *   backwards tab 
             */
            backtab = ((GetKeyState(VK_SHIFT) & 0x8000) != 0);

            /* if a field is open, close it */
            if (entry_fld_ != 0)
            {
                CHtmlDbgExpr *old_focus;
                htmlexpr_winpart_t old_part;

                /* remember the current field */
                old_focus = focus_expr_;
                old_part = focus_part_;
                
                /* 
                 *   if we're tabbing backwards, go to the previous item
                 *   now; otherwise, wait until after we finish closing
                 *   the field to tab to the next item, since we may add a
                 *   new item after this item (in which case we can't
                 *   figure out where to go next until after we finish
                 *   with the close) 
                 */
                if (backtab)
                    go_next_focus(TRUE, backtab);

                /* close the field */
                close_entry_field(TRUE);
                
                /*
                 *   If the focus is no longer on the same field, closing
                 *   the field must have modified the list (it may have
                 *   deleted the field or created a new one), in which
                 *   case focus will already be moved to the proper new
                 *   point.  If focus is still where it was, though, we
                 *   need to advance it now.  
                 */
                if (focus_expr_ == old_focus && focus_part_ == old_part)
                    go_next_focus(TRUE, backtab);
            }
            else
            {
                /* advance the focus to the next editable field */
                go_next_focus(TRUE, backtab);
            }
        }

        /* scroll the focus into view before opening the next field */
        scroll_focus_into_view();
        
        /* open a new entry field on the next field */
        if (focus_expr_ != 0)
        {
            get_focus_text_area(&text_area, TRUE);
            open_entry_field(&text_area, focus_expr_, focus_part_);
        }

        /* handled */
        return TRUE;

    case 10:
    case 13:
        /* 
         *   if there's no focus expression, and we have a blank last item
         *   for inserting new expressions, select the last field 
         */
        if (entry_fld_ == 0 && focus_expr_ == 0 && has_blank_entry_)
            set_selection(last_expr_, TRUE, HTMLEXPR_WINPART_EXPR);

        /* scroll to show the focus */
        scroll_focus_into_view();

        /* enter - if we have an entry field, close it */
        if (entry_fld_ != 0)
        {
            /* close the field */
            close_entry_field(TRUE);
        }
        else if (focus_expr_ != 0)
        {
            /* set this as the selected item */
            set_selection(focus_expr_, FALSE, focus_part_);

            /* open a new entry field on the current focus */
            get_focus_text_area(&text_area, TRUE);
            open_entry_field(&text_area, focus_expr_, focus_part_);
        }

        /* handled */
        return TRUE;

    default:
        /* 
         *   if there's no focus expression, and we have a blank last item
         *   for inserting new expressions, select the last field 
         */
        if (entry_fld_ == 0 && focus_expr_ == 0 && has_blank_entry_)
            set_selection(last_expr_, TRUE, HTMLEXPR_WINPART_EXPR);

        /*
         *   If we have a printable character, and we have a valid focus
         *   control, open the focus control for editing and send it to
         *   the control. 
         */
        if (entry_fld_ == 0 && ch >= ' ' && focus_expr_ != 0)
        {
            /* scroll to show the focus */
            scroll_focus_into_view();

            /* open a new entry field on the current focus */
            get_focus_text_area(&text_area, TRUE);
            open_entry_field(&text_area, focus_expr_, focus_part_);

            /* send the keystroke to the new control */
            if (entry_fld_ != 0)
                PostMessage(entry_fld_, WM_CHAR, (WPARAM)ch, (LPARAM)keydata);
        }
    }

    /* inherit default */
    return CTadsWinScroll::do_char(ch, keydata);
}

/*
 *   determine if we can delete the focus expression (if any); returns
 *   false if the expression cannot be deleted, or there is no focus
 *   expression 
 */
int CHtmlDbgExprWin::can_delete_focus() const
{
    /*
     *   We'll only allow deleting if the current expression is editable,
     *   and it's not the last expression in a list with a blank final
     *   expression.  Don't allow deleting the entry if it's the only one
     *   (i.e., single_entry_ is true).  
     */
    return (focus_expr_ != 0
            && focus_expr_->can_edit_part(HTMLEXPR_WINPART_EXPR)
            && (!has_blank_entry_ || focus_expr_ != last_expr_)
            && !single_entry_);
}

/*
 *   Handle system keystrokes - send them to the main window for menu
 *   processing (this lets the user access the main debug window's menus
 *   from the keyboard while this window has focus).  Note that we must
 *   make the debugger window active in order to display its menu.  
 */
int CHtmlDbgExprWin::do_syskeydown(int vkey, long keydata)
{
    mainwin_->run_menu_kb_ifc(vkey, keydata, GetParent(handle_));
    return TRUE;
}

/*
 *   handle keystrokes
 */
int CHtmlDbgExprWin::do_keydown(int vkey, long keydata)
{
    CHtmlDbgExpr *expr;
    RECT rc;
    int pgrows;

    /* check which key we have */
    switch(vkey)
    {
    case VK_ESCAPE:
        /* escape key - notify parent window */
        SendMessage(GetParent(handle_), WM_KEYDOWN, vkey, keydata);
        return TRUE;
        
    case VK_DELETE:
        /* delete the current expression, if possible */
        if (can_delete_focus())
        {
            /* scroll to show the focus */
            scroll_focus_into_view();

            /* delete this expression */
            delete_expr(focus_expr_);
        }

        /* handled */
        return TRUE;

    case VK_UP:
    do_up_key:
        /* move to the previous item in the list */
        if (selection_ == 0)
        {
            /* no selection yet - select the first one */
            expr = first_expr_;
        }
        else if (selection_->get_prev_expr() == 0)
        {
            /* already at the top of the list - go nowhere */
            expr = selection_;
        }
        else
        {
            /* move to the previous selection */
            expr = selection_->get_prev_expr();
        }

        /* move to the selected expression */
        set_selection(expr, TRUE, HTMLEXPR_WINPART_EXPR);

        /* scroll to show the focus */
        scroll_focus_into_view();

        /* handled */
        return TRUE;

    case VK_DOWN:
    do_down_key:
        /* move to the next item in the list */
        if (selection_ == 0)
        {
            /* no selection - move to the first one */
            expr = first_expr_;
        }
        else if (selection_->get_next_expr() == 0)
        {
            /* already at the end of the list - go nowhere */
            expr = selection_;
        }
        else
        {
            /* move to the next expression */
            expr = selection_->get_next_expr();
        }

        /* select the new expression */
        set_selection(expr, TRUE, HTMLEXPR_WINPART_EXPR);

        /* scroll to show the focus */
        scroll_focus_into_view();

        /* handled */
        return TRUE;

    case VK_PRIOR:
        /* scroll up a page - calculate the rows per page */
        get_scroll_area(&rc, TRUE);
        pgrows = (rc.bottom - rc.top) / vscroll_units_;

        /* keep one row in view if we can, but move at least one row */
        if (pgrows > 1)
            --pgrows;
        else if (pgrows == 0)
            pgrows = 1;

        /* if the list is completely empty, there's nothing to do */
        if (first_expr_ == 0)
            return TRUE;

        /* move ahead by the desired number of rows */
        for (expr = (selection_ == 0 ? first_expr_ : selection_) ;
             pgrows != 0 && expr->get_prev_expr() != 0 ;
             --pgrows, expr = expr->get_prev_expr()) ;

        /* select the expression and make sure it's in view */
        set_selection(expr, TRUE, HTMLEXPR_WINPART_EXPR);
        scroll_focus_into_view();

        /* handled */
        return TRUE;

    case VK_NEXT:
        /* scroll down a page - calculate the rows per page */
        get_scroll_area(&rc, TRUE);
        pgrows = (rc.bottom - rc.top) / vscroll_units_;

        /* keep one row in view if we can, but move at least one row */
        if (pgrows > 1)
            --pgrows;
        else if (pgrows == 0)
            pgrows = 1;


        /* if the list is completely empty, there's nothing to do */
        if (first_expr_ == 0)
            return TRUE;

        /* move ahead by the desired number of rows */
        for (expr = (selection_ == 0 ? first_expr_ : selection_) ;
             pgrows != 0 && expr->get_next_expr() != 0 ;
             --pgrows, expr = expr->get_next_expr()) ;

        /* select the expression and make sure it's in view */
        set_selection(expr, TRUE, HTMLEXPR_WINPART_EXPR);
        scroll_focus_into_view();

        /* handled */
        return TRUE;

    case VK_HOME:
        /* scroll to the first item */
        set_selection(first_expr_, TRUE, HTMLEXPR_WINPART_EXPR);
        scroll_focus_into_view();

        /* handled */
        return TRUE;

    case VK_END:
        /* scroll to the last item */
        set_selection(last_expr_, TRUE, HTMLEXPR_WINPART_EXPR);
        scroll_focus_into_view();

        /* handled */
        return TRUE;

    case VK_LEFT:
        /* 
         *   If this item has children and is expanded, close it;
         *   otherwise, move to the parent item.  
         */
        if (selection_ != 0)
        {
            /* check to see if the current item is expanded */
            if (selection_->is_openable() && selection_->is_open())
            {   
                /* this selection is open - close it */
                selection_->invert_open_state(this);
            }
            else if (selection_->get_parent() != 0)
            {
                /* this selection is closed - move to the parent */
                set_selection(selection_->get_parent(), TRUE,
                              HTMLEXPR_WINPART_EXPR);

                /* scroll to show the focus */
                scroll_focus_into_view();
            }
            else
            {
                /* 
                 *   there are no children and no parent - treat it the
                 *   same as the up arrow
                 */
                goto do_up_key;
            }
        }
        else
        {
            /* there's no selection - treat it like an 'up' key */
            goto do_up_key;
        }

        /* handled */
        return TRUE;

    case VK_RIGHT:
        /*
         *   If this item has children and is expanded, move to the first
         *   child.  If it has children and it's closed, open it.  If it
         *   has no children, move to the next item. 
         */
        if (selection_ != 0 && selection_->is_openable()
            && !selection_->is_open())
        {
            /* open it */
            selection_->invert_open_state(this);
        }
        else
        {
            /* 
             *   in any other case, the effect is the same as the 'down'
             *   key (if it has children and is open, we want to go to the
             *   first child, which is the next item down; if it has no
             *   children, we want to go to the next peer, which is the
             *   next item down; and if there's not a selection, we want
             *   to do the same thing the 'down' key does) 
             */
            goto do_down_key;
        }

        /* handled */
        return TRUE;

    case VK_F2:
        /* 
         *   if there's no focus expression, and we have a blank last item
         *   for inserting new expressions, select the last field 
         */
        if (entry_fld_ == 0 && focus_expr_ == 0 && has_blank_entry_)
            set_selection(last_expr_, TRUE, HTMLEXPR_WINPART_EXPR);

        /* 
         *   if the current field isn't already open, and we have a valid
         *   field, open it for editing 
         */
        if (entry_fld_ == 0 && focus_expr_ != 0)
        {
            CHtmlRect text_area;
            
            /* scroll to show the focus */
            scroll_focus_into_view();

            /* open a new entry field on the current focus */
            get_focus_text_area(&text_area, TRUE);
            open_entry_field(&text_area, focus_expr_, focus_part_);
        }
        return TRUE;

    default:
        break;
    }

    /* inherit default */
    return CTadsWinScroll::do_keydown(vkey, keydata);
}

/*
 *   process a user message 
 */
int CHtmlDbgExprWin::do_user_message(int msg, WPARAM wpar, LPARAM lpar)
{
    switch(msg)
    {
    case DBGEXPRM_SET_EXPR:
        {
            CHtmlDbgExpr *expr;
            
            /* 
             *   get the focus expression, or the first expression if
             *   there's no focus 
             */
            expr = focus_expr_;
            if (expr == 0)
                expr = first_expr_;

            /* set the new expression text */
            if (expr != 0)
                expr->accept_expr(this, (const textchar_t *)lpar, FALSE);

            /* handled */
            return TRUE;
        }
        
    default:
        /* inherit default */
        return CTadsWinScroll::do_user_message(msg, wpar, lpar);
    }
}


/*
 *   Clear my data from a configuration 
 */
void CHtmlDbgExprWin::clear_config(CHtmlDebugConfig *config,
                                   const textchar_t *varname,
                                   size_t panel_num) const
{
    textchar_t element[50];

    /* build our element name */
    sprintf(element, "expression list %d", panel_num);

    /* clear out any existing list */
    config->clear_strlist(varname, element);
}

/*
 *   Save my configuration 
 */
void CHtmlDbgExprWin::save_config(CHtmlDebugConfig *config,
                                  const textchar_t *varname,
                                  size_t panel_num)
{
    CHtmlDbgExpr *cur;
    int cnt;
    textchar_t element[50];

    /* 
     *   build our element name - base it on the panel number, so that we
     *   can distinguish this expression list from those of other panels
     *   within this main window 
     */
    sprintf(element, "expression list %d", panel_num);

    /* clear out any existing list */
    config->clear_strlist(varname, element);

    /*
     *   Build the expression list.  We'll save top-level expressions
     *   only, since they're the only ones directly enterable by the user. 
     */
    for (cur = first_expr_, cnt = 0 ; cur != 0 ; cur = cur->get_next_expr())
    {
        /* 
         *   if this item is at the root level, save it, unless it's the
         *   blank last item 
         */
        if (cur->get_level() == 1
            && !(cur->get_next_expr() == 0 && has_blank_entry_
                 && get_strlen(cur->get_expr()) == 0))
        {
            /* save this item */
            config->setval(varname, element, cnt, cur->get_expr());

            /* count it */
            ++cnt;
        }
    }
}

/*
 *   Load my configuration 
 */
void CHtmlDbgExprWin::load_config(CHtmlDebugConfig *config,
                                  const textchar_t *varname,
                                  size_t panel_num)
{
    int cnt;
    textchar_t element[50];

    /* 
     *   build our element name - base it on the panel number, so that we
     *   can distinguish this expression list from those of other panels
     *   within this main window 
     */
    sprintf(element, "expression list %u", panel_num);

    /* clear out the list */
    begin_refresh(0);
    end_refresh(0);

    /* run through the saved list */
    cnt = config->get_strlist_cnt(varname, element);
    while (cnt != 0)
    {
        textchar_t buf[1024];
        size_t result_len;

        /* move on to the next item */
        --cnt;
        
        /* get this item from the saved configuration information */
        if (!config->getval(varname, element, cnt,
                            buf, sizeof(buf), &result_len))
        {
            CHtmlDbgExpr *new_expr;
            
            /* create the new expression item */
            new_expr = new CHtmlDbgExpr(1, buf, result_len, 0, "",
                                        "", 0, FALSE, TRUE, 0, this);

            /* link it at the head of the list */
            new_expr->prev_ = 0;
            new_expr->next_ = first_expr_;
            if (first_expr_ != 0)
                first_expr_->prev_ = new_expr;
            else
                last_expr_ = new_expr;
            first_expr_ = new_expr;

            /* mark it as done with refresh */
            new_expr->set_pending_refresh(FALSE);
        }
    }

    /* fix up postions */
    fix_positions(0);
}

/* ------------------------------------------------------------------------ */
/*
 *   CHtmlDbgExprWin - IDropTarget implementation 
 */

/*
 *   start dragging over this window 
 */
HRESULT STDMETHODCALLTYPE
   CHtmlDbgExprWin::DragEnter(IDataObject __RPC_FAR *dataobj,
                              DWORD /*grfKeyState*/, POINTL /*pt*/,
                              DWORD __RPC_FAR *effect)
{
    FORMATETC fmtetc;
    
    /* check to see if the data object can render text into an hglobal */
    fmtetc.cfFormat = CF_TEXT;
    fmtetc.ptd = 0;
    fmtetc.dwAspect = DVASPECT_CONTENT;
    fmtetc.lindex = -1;
    fmtetc.tymed = TYMED_HGLOBAL;
    if (dataobj->QueryGetData(&fmtetc) == S_OK)
    {
        /* 
         *   it can provide what we need - allow dropping the text; the
         *   drop will always count as a copy 
         */
        *effect = DROPEFFECT_COPY;
    }
    else
    {
        /* whatever it is, we can't accept a drop */
        *effect = DROPEFFECT_NONE;
    }

    /* success */
    return S_OK;
}

/*
 *   continue dragging over this window 
 */
HRESULT STDMETHODCALLTYPE
   CHtmlDbgExprWin::DragOver(DWORD /*grfKeyState*/,
                             POINTL /*pt*/,
                             DWORD __RPC_FAR * /*effect*/)
{
    /* keep the same setting we decided upon in DragEnter */
    return S_OK;
}

/*
 *   leave this window with no drop having occurred 
 */
HRESULT STDMETHODCALLTYPE CHtmlDbgExprWin::DragLeave()
{
    /* we don't have anything we need to do here */
    return S_OK;
}

/*
 *   drop in this window 
 */
HRESULT STDMETHODCALLTYPE
   CHtmlDbgExprWin::Drop(IDataObject __RPC_FAR *dataobj,
                         DWORD /*grfKeyState*/, POINTL /*pt*/,
                         DWORD __RPC_FAR *effect)
{
    FORMATETC fmtetc;
    STGMEDIUM medium;

    /* check to see if the data object can render text into an hglobal */
    fmtetc.cfFormat = CF_TEXT;
    fmtetc.ptd = 0;
    fmtetc.dwAspect = DVASPECT_CONTENT;
    fmtetc.lindex = -1;
    fmtetc.tymed = TYMED_HGLOBAL;
    if (dataobj->GetData(&fmtetc, &medium) == S_OK)
    {
        textchar_t *buf;

        /* get the text */
        buf = (textchar_t *)GlobalLock(medium.hGlobal);

        /* add the new entry to our list */
        add_expr(buf, TRUE, TRUE);

        /* we're done with the hglobal - unlock and delete it */
        GlobalUnlock(medium.hGlobal);
        if (medium.pUnkForRelease != 0)
            medium.pUnkForRelease->Release();
        else
            GlobalFree(medium.hGlobal);
        
        /* 
         *   we successfully accepted the drop - we always treat a drop as
         *   a copy of the data 
         */
        *effect = DROPEFFECT_COPY;
    }
    else
    {
        /* we couldn't get the data, so the drop failed */
        *effect = DROPEFFECT_NONE;
    }

    /* success */
    return S_OK;
}


/* ------------------------------------------------------------------------ */
/*
 *   Local variable watch window 
 */

/*
 *   Callback context structure for local variable enumeration 
 */
struct enum_locals_cbctx
{
    /* the window we're refreshing */
    CHtmlDbgExprWin *win_;

    /* parent expression of the items being updated */
    CHtmlDbgExpr *parent_;
};

void CHtmlDbgExprWinLocals::update_all()
{
    enum_locals_cbctx cbctx;

    /* start the update */
    begin_refresh(0);

    /* reload the locals */
    cbctx.win_ = this;
    cbctx.parent_ = 0;
    helper_->enum_locals(mainwin_->get_dbg_ctx(), &enum_locals_cb, &cbctx);

    /* finish the update */
    end_refresh(0);

    /* update scrollbars */
    adjust_scrollbar_ranges();
}

/*
 *   Local variable enumerator callback 
 */
void CHtmlDbgExprWinLocals::
   enum_locals_cb(void *ctx0, const textchar_t *lclnam, size_t lclnamlen)
{
    enum_locals_cbctx *ctx = (enum_locals_cbctx *)ctx0;

    /* refresh the local in the window */
    ctx->win_->do_refresh(ctx->parent_, lclnam, lclnamlen, "");
}

/* ------------------------------------------------------------------------ */
/*
 *   'self' expression window 
 */

/*
 *   update the list 
 */
void CHtmlDbgExprWinSelf::update_all()
{
    /* start the update */
    begin_refresh(0);

    /* add 'self' */
    do_refresh(0, "self", 4, "");

    /* finish the update */
    end_refresh(0);

    /* make sure the 'self' item is open */
    if (first_expr_ != 0 && !first_expr_->is_open())
        first_expr_->invert_open_state(this);

    /* update scrollbars */
    adjust_scrollbar_ranges();
}


/* ------------------------------------------------------------------------ */
/*
 *   Debugger window expression item 
 */

CHtmlDbgExpr::CHtmlDbgExpr(int level, const textchar_t *expr, size_t exprlen,
                           CHtmlDbgExpr *parent,
                           const textchar_t *parent_relationship, 
                           const textchar_t *val, size_t vallen,
                           int is_openable,  int can_edit_expr, long ypos,
                           CHtmlDbgExprWin *win)
{
    HDC dc;
    TEXTMETRIC tm;
    HGDIOBJ oldfont;
    
    /* not in the list yet */
    next_ = prev_ = 0;

    /* remember my expression and value */
    expr_.set(expr, exprlen);
    val_.set(val, vallen);

    /* 
     *   build my full expression as my parent expression plus the parent
     *   relationship operator plus my expression 
     */
    parent_relationship_.set(parent_relationship);
    build_full_expr(parent);

    /* remember whether I'm openable */
    is_openable_ = is_openable;

    /* remember whether the expression is editable */
    can_edit_expr_ = can_edit_expr;

    /* presume we won't be able to edit the value */
    can_edit_val_ = FALSE;

    /* not open yet */
    is_open_ = FALSE;

    /* remember my position */
    y_ = ypos;

    /* remember my level */
    level_ = level;

    /* not pending refresh yet */
    pending_refresh_ = FALSE;

    /* 
     *   calculate my height using the height of text in the containing
     *   window's current font 
     */
    dc = GetDC(win->get_handle());
    oldfont = win->get_list_font()->select(dc);
    GetTextMetrics(dc, &tm);
    height_ = tm.tmHeight + 3;
    win->get_list_font()->unselect(dc, oldfont);
    ReleaseDC(win->get_handle(), dc);
}

CHtmlDbgExpr::~CHtmlDbgExpr()
{
}

/*
 *   Build the full expression, given the parent object 
 */
void CHtmlDbgExpr::build_full_expr(CHtmlDbgExpr *parent)
{
    if (parent != 0)
    {
        fullexpr_.set("(");
        fullexpr_.append(parent->get_full_expr());
        fullexpr_.append(")");
        fullexpr_.append(parent_relationship_.get());
        fullexpr_.append(expr_.get());
    }
    else
    {
        /* there's no parent - my expression is all there is */
        fullexpr_.set(expr_.get());
    }
}

/*
 *   Determine if I'm the last item at my level.  I'm the last at my level
 *   if there are no items after me in the list at my level before the
 *   next item above my level. 
 */
int CHtmlDbgExpr::is_last_in_level() const
{
    CHtmlDbgExpr *cur;

    /* scan the list, looking for another item at my level */
    for (cur = next_ ; cur != 0 ; cur = cur->next_)
    {
        /* if this one's above my level, stop looking - I'm the last one */
        if (cur->level_ < level_)
            return TRUE;

        /* if this one's at my level, stop looking - I'm not the last one */
        if (cur->level_ == level_)
            return FALSE;
    }

    /* didn't find any others - I'm the last one */
    return TRUE;
}

/*
 *   Get my parent item.  My parent is the closest item before me in the
 *   list with a hierarchy level less than mine. 
 */
CHtmlDbgExpr *CHtmlDbgExpr::get_parent() const
{
    CHtmlDbgExpr *cur;

    /* scan the items before me */
    for (cur = prev_ ; cur != 0 ; cur = cur->prev_)
    {
        /* if this item is above me in the hierarchy, it's my parent */
        if (cur->level_ < level_)
            return cur;
    }

    /* didn't find a parent item - I must be at the root level */
    return 0;
}


/*
 *   Draw the expression 
 */
void CHtmlDbgExpr::draw(HDC hdc, CHtmlDbgExprWin *win,
                        const RECT *area_to_draw, long sep_xpos,
                        int hdr_height, int is_selected)
{
    int level;
    CHtmlDbgExpr *cur;
    int start_x;
    int x;
    CHtmlRect pos;
    RECT rc;
    RECT textrc;
    HPEN oldpen, pen;
    COLORREF oldtxtcolor, oldbkcolor;
    int oldbkmode;

    /* calculate my position in window coordinates */
    GetClientRect(win->get_handle(), &rc);
    pos.set(0, y_, rc.right, y_ + height_);
    pos = win->doc_to_screen(pos);

    /* offset myself by the size of the header */
    pos.top += hdr_height;
    pos.bottom += hdr_height;

    /* 
     *   always fill across the entire width of the screen, even after
     *   scrolling 
     */
    pos.right = rc.right;

    /* set up a windows RECT structure with the same information */
    SetRect(&rc, pos.left, pos.top, pos.right, pos.bottom);

    /*
     *   Erase the background in the appropriate color.  If I'm selected,
     *   use the text selection background color, otherwise use the normal
     *   window background color. 
     */
    oldbkmode = SetBkMode(hdc, OPAQUE);
    if (is_selected)
    {
        oldtxtcolor = SetTextColor(hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
        oldbkcolor = SetBkColor(hdc, GetSysColor(COLOR_HIGHLIGHT));
        FillRect(hdc, &rc, GetSysColorBrush(COLOR_HIGHLIGHT));
    }
    else
    {
        oldtxtcolor = SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
        oldbkcolor = SetBkColor(hdc, GetSysColor(COLOR_WINDOW));
        FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));
    }

    /* calculate where this item starts, based on my indent level */
    start_x = win->doc_to_screen_x((level_ - 1) * HTMLDBGEXPR_LEVEL_INDENT);

    /* if I don't overlap the drawing area, ignore this */
    if (rc.bottom < area_to_draw->top || rc.top > area_to_draw->bottom)
        return;

    /*
     *   Draw our hierarchy line.  This line connects us to our parent,
     *   and to the next sibling at our level.  If we're the last item at
     *   our level, we only need to draw a line to our parent.  If we're
     *   at the root level, we don't need to draw anything at all. 
     */
    if (level_ > 1)
    {
        RECT dstrc, srcrc;
        
        /* I have a parent - draw a line to the parent */
        SetRect(&srcrc, 0, 0, 1, (is_last_in_level() ? height_/2 : height_));
        dstrc = srcrc;
        OffsetRect(&dstrc, start_x - HTMLDBGEXPR_LEVEL_INDENT + 2 + 4,
                   rc.top + (y_ & 1));
        win->draw_hbitmap(win->get_vdots_bmp(), &dstrc, &srcrc);

        /* draw the horizontal connector line */
        SetRect(&srcrc, 0, 0, HTMLDBGEXPR_LEVEL_INDENT - 2 - 4 + 10, 1);
        dstrc = srcrc;
        OffsetRect(&dstrc, start_x - HTMLDBGEXPR_LEVEL_INDENT + 2 + 4,
                   rc.top + height_/2);
        win->draw_hbitmap(win->get_hdots_bmp(), &dstrc, &srcrc);
    }

    /* draw our open/close box, if we have one */
    if (is_openable_)
    {
        /* 
         *   if I'm open, and I have any children, draw the line to my
         *   first child 
         */
        if (is_open_ && get_next_expr() != 0
            && get_next_expr()->get_level() > get_level())
        {
            RECT dstrc, srcrc;

            SetRect(&srcrc, 0, 0, 1, height_/2);
            dstrc = srcrc;
            OffsetRect(&dstrc, start_x + 2 + 4,
                       rc.top + height_/2 + ((y_ + height_/2) & 1));
            win->draw_hbitmap(win->get_vdots_bmp(), &dstrc, &srcrc);
        }

        /* I'm openable, so draw my open/close box */
        DrawIconEx(hdc, start_x + 2, rc.top + (rc.bottom - rc.top - 9)/2,
                   win->get_openclose_icon(is_open_), 9, 9, 0, 0, DI_NORMAL);
    }
    
    
    /*
     *   Draw the hierarchy lines for containing levels.  Scan up the
     *   parent list until we reach the root of the hierarchy, drawing a
     *   hierarchy line as needed at each enclosing level.  The lines
     *   we're drawing connect our parent to children below us, so we only
     *   draw them at a level if the parent at that level has more
     *   children after us.  
     */
    for (x = start_x - HTMLDBGEXPR_LEVEL_INDENT, level = level_ - 1,
         cur = get_parent() ; cur != 0 && cur->get_parent() != 0 ;
         --level, cur = cur->get_parent(), x -= HTMLDBGEXPR_LEVEL_INDENT)
    {
        /* 
         *   If this is the last item at this level, there's nothing to
         *   draw at this level; otherwise, draw a line going by at this
         *   level. 
         */
        if (!cur->is_last_in_level())
        {
            RECT dstrc, srcrc;

            /* draw the vertical line */
            SetRect(&srcrc, 0, 0, 1, height_);
            dstrc = srcrc;
            OffsetRect(&dstrc, x - HTMLDBGEXPR_LEVEL_INDENT + 2 + 4,
                       rc.top + (y_ & 1));
            win->draw_hbitmap(win->get_vdots_bmp(), &dstrc, &srcrc);
        }
    }

    /* draw my expression text */
    textrc = rc;
    textrc.left = start_x + HTMLDBGEXPR_TEXT_OFFSET;
    textrc.right = sep_xpos - 1;
    ExtTextOut(hdc, textrc.left, textrc.top, ETO_CLIPPED, &textrc,
               expr_.get(), get_strlen(expr_.get()), 0);

    /* draw my value text */
    textrc = rc;
    textrc.left = sep_xpos + 1;
    ExtTextOut(hdc, textrc.left, textrc.top, ETO_CLIPPED, &textrc,
               val_.get(), get_strlen(val_.get()), 0);

    /* create a pen for drawing the lines */
    pen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DFACE));
    oldpen = (HPEN)SelectObject(hdc, pen);

    /* draw the vertical separator bar */
    MoveToEx(hdc, sep_xpos, rc.top, 0);
    LineTo(hdc, sep_xpos, rc.top + height_);

    /* draw the separator line at the bottom */
    MoveToEx(hdc, rc.left, rc.bottom - 1, 0);
    LineTo(hdc, rc.right, rc.bottom - 1);

    /* done with the line pen */
    SelectObject(hdc, oldpen);
    DeleteObject(pen);

    /* restore text attributes */
    SetBkColor(hdc, oldbkcolor);
    SetTextColor(hdc, oldtxtcolor);
    SetBkMode(hdc, oldbkmode);
}

/*
 *   Determine which part of the item contains the given position
 */
htmlexpr_winpart_t CHtmlDbgExpr::pos_to_area(int x, int sep_xpos,
                                             long *xl, long *xr)
{
    long box_left = (level_ - 1) * HTMLDBGEXPR_LEVEL_INDENT;
    long text_left = box_left + HTMLDBGEXPR_TEXT_OFFSET;

    /* determine where the point is */
    if (x >= sep_xpos - 1 && x <= sep_xpos + 2)
    {
        /* it's in the separator bar */
        return HTMLEXPR_WINPART_SEP;
    }
    else if (x > sep_xpos + 2)
    {
        /* it's in the value portion */
        part_to_area(HTMLEXPR_WINPART_VAL, xl, xr, sep_xpos, TRUE);

        /* return the value code */
        return HTMLEXPR_WINPART_VAL;
    }
    else if (x < box_left)
    {
        /* it's in the margin */
        return HTMLEXPR_WINPART_EXPRMARGIN;
    }
    else if (x < text_left)
    {
        /* it's in the open/close box */
        return HTMLEXPR_WINPART_OPENCLOSE;
    }
    else
    {
        /* it's in the expression portion */
        part_to_area(HTMLEXPR_WINPART_EXPR, xl, xr, sep_xpos, TRUE);
        
        /* return the expression code */
        return HTMLEXPR_WINPART_EXPR;
    }
}

/*
 *   Get the horizontal bounds of a given text area
 */
void CHtmlDbgExpr::part_to_area(htmlexpr_winpart_t part, long *xl, long *xr,
                                int sep_xpos, int on_screen)
{
    long box_left = (level_ - 1) * HTMLDBGEXPR_LEVEL_INDENT;
    long text_left = box_left + HTMLDBGEXPR_TEXT_OFFSET;

    switch(part)
    {
    case HTMLEXPR_WINPART_EXPR:
        /* 
         *   expression part - the left edge is the open/close box, and
         *   the right edge is the separator bar 
         */
        if (text_left > *xl || !on_screen)
            *xl = text_left;
        if (sep_xpos < *xr || !on_screen)
            *xr = sep_xpos;
        break;

    case HTMLEXPR_WINPART_VAL:
        /* value portion - the left edge is the separator bar */
        if (sep_xpos > *xl || !on_screen)
            *xl = sep_xpos;
        break;

    default:
        break;
    }
}

/*
 *   Determine if we can edit a part of the expression 
 */
int CHtmlDbgExpr::can_edit_part(htmlexpr_winpart_t part) const
{
    switch(part)
    {
    case HTMLEXPR_WINPART_EXPR:
        /* allow editing if the creator so indicated */
        return can_edit_expr_;

    case HTMLEXPR_WINPART_VAL:
        /* allow editing if the creator so indicated */
        return can_edit_val_;

    default:
        /* can't edit other parts */
        return FALSE;
    }
}

/*
 *   Receive notification that we're opening an edit field on this item 
 */
void CHtmlDbgExpr::on_open_entry_field(HWND fldctl, htmlexpr_winpart_t part)
{
    const textchar_t *str;
    
    /* get my text value */
    switch(part)
    {
    case HTMLEXPR_WINPART_EXPR:
        str = expr_.get();
        break;

    case HTMLEXPR_WINPART_VAL:
        str = val_.get();
        break;

    default:
        /* can't edit other parts; ignore */
        return;
    }

    /* initialize the field with my text */
    SendMessage(fldctl, EM_REPLACESEL, FALSE, (LPARAM)str);
}

/*
 *   Receive notification that we're closing an edit field on this item 
 */
void CHtmlDbgExpr::on_close_entry_field(CHtmlDbgExprWin *win,
                                        HWND fldctl, htmlexpr_winpart_t part,
                                        int accept_changes)
{
    textchar_t buf[2048];

    /* if we're not accepting changes, there's nothing to do */
    if (!accept_changes)
        return;
    
    /* get the text from the field */
    SendMessage(fldctl, WM_GETTEXT, sizeof(buf), (LPARAM)buf);

    /* set the new value */
    switch(part)
    {
    case HTMLEXPR_WINPART_EXPR:
        /* remember the new expression */
        accept_expr(win, buf, TRUE);
        break;

    case HTMLEXPR_WINPART_VAL:
        /* 
         *   save ourselves some work - if the value didn't change, don't
         *   bother re-evaluating it 
         */
        if (get_strcmp(get_val(), buf) != 0)
        {
            /* it's changed - set the new value */
            win->change_expr_value(this, buf);
        }
        break;

    default:
        break;
    }
}

/*
 *   Accept new expression text
 */
void CHtmlDbgExpr::accept_expr(CHtmlDbgExprWin *win, const textchar_t *buf,
                               int delete_on_empty)
{
    /* set the new expression text */
    expr_.set(buf);

    /* generate my full expression */
    if (level_ > 1)
    {
        CHtmlDbgExpr *parent;
        
        /* find my parent */
        for (parent = prev_ ;
             parent != 0 && parent->level_ >= level_ ;
             parent = parent->prev_) ;
        
        /* build my new full expression */
        build_full_expr(parent);
    }
    else
        build_full_expr(0);
    
    /* 
     *   if the new expression is empty, delete this item; otherwise,
     *   evaluate it and set the new value 
     */
    if (is_expr_empty())
    {
        /* I'm empty - delete me from the list if the caller allows it */
        if (delete_on_empty)
            win->delete_expr(this);
    }
    else
    {
        /* evaluate the new expression */
        update_eval(win);
    }
}

/*
 *   Open the item if it's closed, close it if it's open 
 */
void CHtmlDbgExpr::invert_open_state(CHtmlDbgExprWin *win)
{
    /* if I'm not openable, ignore the request */
    if (!is_openable())
        return;
    
    /* set the new state */
    is_open_ = !is_open_;

    /* check to see if the debugger has control */
    if (win->get_mainwin()->get_in_debugger())
    {
        /* 
         *   the debugger has control, so we can reevaluate my expression to
         *   rebuild the child list 
         */
        win->update_eval(this);
    }
    else
    {
        /* 
         *   The debugger doesn't have control, so we can't reevaluate the
         *   expression.  This means that we can't build a new child list if
         *   we're trying to open the item, so simply don't allow opening the
         *   item.  If we're closing the item, do allow it - just delete the
         *   child list in this case.  
         */
        if (is_open_)
        {
            /* trying to open the item, which we can't do - disallow it */
            is_open_ = FALSE;
        }
        else
        {
            /* 
             *   Closing the item - delete the child list.  Do this by doing
             *   an empty refresh of myself - this will clear out the child
             *   list by virtue of the fact that we won't have refreshed any
             *   of the children by the end of the refresh. 
             */
            win->begin_refresh(this);
            win->end_refresh(this);
        }
    }
}

/*
 *   Determine if the expression is empty 
 */
int CHtmlDbgExpr::is_expr_empty() const
{
    const textchar_t *p;

    /* scan the expression; if all we find is whitespace, it's empty */
    for (p = expr_.get() ; *p != 0 ; ++p)
    {
        /* if it's not whitespace, it's not empty */
        if (!is_space(*p))
            return FALSE;
    }

    /* didn't find anything but whitespace - it's empty, all right */
    return TRUE;
}

/*
 *   Update the evaluation of my expression 
 */
void CHtmlDbgExpr::update_eval(CHtmlDbgExprWin *win)
{
    /* ask the window to reevaluate it */
    win->update_eval(this);
}

