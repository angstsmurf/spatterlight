#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadswin.cpp,v 1.3 1999/07/11 00:46:48 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadswin.cpp - TADS window classes
Function
  
Notes
  
Modified
  09/16/97 MJRoberts  - Creation
*/


#include <Windows.h>
#include <mmsystem.h>
#include <commctrl.h>
#include <limits.h>


#ifndef HTMLRES_H
#include "htmlres.h"
#endif
#ifndef TADSWIN_H
#include "tadswin.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif
#ifndef TADSMIDI_H
#include "tadsmidi.h"
#endif

#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL   0x020A                            /* from WinUser.h */
#define WHEEL_DELTA     120                 /* Value for rolling one detent */
#define SPI_GETWHEELSCROLLLINES 104                       /* from WinUser.h */
#endif

/* ------------------------------------------------------------------------ */
/*
 *   Our special version of the Win32 API call GetTextExtentPoint32.  This
 *   API call is buggy on Windows 2000; some people have encountered odd
 *   problems that we traced to this API, and MS tech support has confirmed
 *   that the API has a problem calculating the size of some realized fonts.
 *   
 *   It seems that we can work around the problem by using the alternative
 *   API call GetTextExtentExPoint instead; that function seems to work
 *   consistently even in Win2k.  The only problem is that
 *   GetTextExtentExPoint is documented as being considerably slower than
 *   GetTextExtentPoint32.  So, we test the OS version, using
 *   GetTextExtentExPoint if we're on Win2k, and GetTextExtentPoint32
 *   everywhere else.  
 */
void ht_GetTextExtentPoint32(HDC dc, const textchar_t *txt, size_t len,
                             SIZE *txtsiz)
{
    if (CTadsApp::get_app()->is_win2k())
    {
        /* 
         *   We're on Windows 2000, where GetTextExtentPoint32 is known to be
         *   buggy - use the more reliable (but slower) GetTextExtentExPoint
         *   instead.
         *   
         *   On *some* versions of win2k, GetTextExtentExPoint is itself
         *   buggy in one odd case: it fails when presented with a
         *   zero-length string.  (I call this behavior "buggy" because it's
         *   not documented, this function accepts zero-length strings on
         *   other versions of Windows and even on some versions of Win2k.)
         *   Fortunately, this one is easy enough to work around: check to
         *   see if we have a zero-length string, and return a zero-by-zero
         *   extent if so.  
         */
        if (len == 0)
        {
            /* it's an empty string, so return a 0x0 extent */
            txtsiz->cx = txtsiz->cy = 0;
            return;
        }

        /* get the text extent */
        GetTextExtentExPoint(dc, txt, len, 0, 0, 0, txtsiz);
    }
    else
    {
        /* we're on anything but win2k, so GetTextExtentPoint32 is safe */
        GetTextExtentPoint32(dc, txt, len, txtsiz);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Button click record 
 */

/*
 *   Determine if a click is a double, triple, etc click, and record the
 *   click information so that we can make the same type of determination
 *   for the next click.
 */
int BtnClick_t::get_click_count(LPARAM lpar)
{
    int x = LOWORD(lpar);
    int y = HIWORD(lpar);
    int dx = GetSystemMetrics(SM_CXDOUBLECLK);
    int dy = GetSystemMetrics(SM_CYDOUBLECLK);
    unsigned long curtime = GetTickCount();
    int cnt;

    /*
     *   if the click is within the distance and time parameters of the
     *   last click, it's a mulitple click; otherwise, it's the first
     *   click 
     */
    if (curtime <= time_ + GetDoubleClickTime()
        && x >= x_ - dx && x <= x_ + dx
        && y >= y_ - dx && y <= y_ + dy)
    {
        /*
         *   it's close enough to the last click in time and space, so it
         *   counts as the second click of a double click, third of a
         *   triple click, or whatever -- up the count and return it 
         */
        cnt = ++cnt_;
    }
    else
    {
        /* it's the first click */
        cnt = cnt_ = 1;
    }

    /* remember the new settings */
    x_ = x;
    y_ = y;
    time_ = curtime;

    /* return the count we calculated for this click */
    return cnt;
}

/* ------------------------------------------------------------------------ */
/*
 *   Timer allocation record.  We use this structure to track free timer
 *   ID's.  
 */
struct tadswin_timer_alo
{
    /* ID of this timer */
    int id;

    /* next in list */
    tadswin_timer_alo *nxt;
};

/* ------------------------------------------------------------------------ */
/*
 *   Basic window implementation 
 */

/* window class names for system registration */
const char CTadsWin::win_class_name[] = "TADS_Window";
const char CTadsWin::mdichild_win_class_name[] = "TADS_MDIChild_Window";
const char CTadsWin::mdiframe_win_class_name[] = "TADS_MDIFrame_Window";
const char CTadsWin::mdiclient_win_class_name[] = "TADS_MDIClient_Window";

/* statics */
int CTadsWin::next_timer_id_ = 1;
tadswin_timer_alo *CTadsWin::free_timers_ = 0;
tadswin_timer_alo *CTadsWin::inuse_timers_ = 0;
size_t CTadsWin::menuiteminfo_size_ = sizeof(MENUITEMINFO);

/*
 *   static class initialization 
 */
void CTadsWin::class_init(CTadsApp *app)
{
    /* 
     *   If we're on Win95 or NT4, force the MENUITEMINFO structure size to
     *   44 bytes - the old API's can't handle the new larger size that the
     *   more recent Microsoft header files use for this structure.  For
     *   other versions of the OS, use the current structure size.  
     */
    if (app->is_win95_or_nt4())
        menuiteminfo_size_ = 44;
}

/*
 *   static class termination 
 */
void CTadsWin::class_terminate()
{
    tadswin_timer_alo *cur;
    tadswin_timer_alo *nxt;

    /* delete all of our timer allocation tracking structures */
    for (cur = free_timers_ ; cur != 0 ; cur = nxt)
    {
        nxt = cur->nxt;
        th_free(cur);
    }
    for (cur = inuse_timers_ ; cur != 0 ; cur = nxt)
    {
        nxt = cur->nxt;
        th_free(cur);
    }
}

/*
 *   create a window object
 */
CTadsWin::CTadsWin()
{
    /* no handle or device context yet */
    handle_ = 0;
    hdc_ = 0;
    display_hdc_ = 0;

    /* no system interface object yet */
    sysifc_ = 0;

    /* load the basic cursors */
    arrow_cursor_ = LoadCursor(0, IDC_ARROW);
    wait_cursor_ = LoadCursor(0, IDC_WAIT);

    /* not yet tracking a popup menu */
    tracking_popup_menu_ = FALSE;

    /* no toolbars yet */
    toolbar_cnt_ = 0;
    tb_timer_id_ = 0;

    /* no drag in progress yet */
    drag_ready_ = FALSE;
    drag_capture_ = FALSE;

    /* not registered as a drop target yet */
    drop_target_regd_ = FALSE;

    /* 
     *   Start off with an OLE reference count of one - we'll use this as
     *   the reference from the system window to this object, and release
     *   it when the system window is destroyed.  Note that we add the
     *   reference here rather than in do_create, since we don't want to
     *   be in a state with a zero reference count, and we presume that
     *   the system window will eventually be created.  
     */
    ole_refcnt_ = 1;

    /* presume we're not yet maximized */
    maximized_ = FALSE;

    /* no external scrolling window yet */
    scroll_win_ = 0;

    /* no scroll wheel accumulation yet */
    wheel_accum_ = 0;
}

CTadsWin::~CTadsWin()
{
    /* delete basic cursors */
    DestroyCursor(arrow_cursor_);
    DestroyCursor(wait_cursor_);

    /* delete the system interface object */
    if (sysifc_ != 0)
        delete sysifc_;

    /* 
     *   if we're controlling scrolling in another window, release the
     *   other window reference 
     */
    if (scroll_win_ != 0)
        scroll_win_->Release();
}

/*
 *   Register the window class with the operating system.  This must be
 *   called once during program initialization, before any windows of this
 *   class are created.  
 */
void CTadsWin::register_win_class(CTadsApp *app)
{
    WNDCLASS wc;

    /* register the standard window class */
    wc.style         = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = sizeof(void *);
    wc.hInstance     = app->get_instance();
    wc.hIcon         = LoadIcon(app->get_instance(),
                                MAKEINTRESOURCE(IDI_MAINWINICON));
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName  = NULL;
    wc.lpfnWndProc   = std_message_handler;
    wc.lpszClassName = win_class_name;
    RegisterClass(&wc);

    /* register the MDI child window class */
    wc.lpfnWndProc   = (WNDPROC)mdichild_message_handler;
    wc.lpszClassName = mdichild_win_class_name;
    RegisterClass(&wc);

    /* register the MDI frame window class */
    wc.lpfnWndProc   = (WNDPROC)mdiframe_message_handler;
    wc.lpszClassName = mdiframe_win_class_name;
    RegisterClass(&wc);

    /* 
     *   register the MDI client window class - this is a "superclass"
     *   (see windows documentation) of the internal windows MDICLIENT
     *   window class, so we must obtain information on the MDICLIENT
     *   class and remember its window procedure pointer and extra window
     *   storage, then register our new class as an extension of the
     *   MDICLIENT class 
     */

    /* get the MDICLIENT class information */
    GetClassInfo(0, "MDICLIENT", &wc);

    /* statically save the window procedure and window extra data */
    CTadsSyswinMdiClient::save_base_class_info(&wc);

    /* add space for our extra window data */
    wc.cbWndExtra += sizeof(void *);

    /* set up our "superclass" registration changes */
    wc.lpfnWndProc = &CTadsSyswinMdiClient::message_handler;
    wc.lpszClassName = mdiclient_win_class_name;
    wc.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE + 1);

    /* register the class */
    RegisterClass(&wc);
}

/*
 *   Create the system window for this object
 */
int CTadsWin::create_system_window(CTadsWin *parent, int show,
                                   const char *title,
                                   const RECT *pos, CTadsSyswin *sysifc)
{
    /* use the default parent handle for the parent object */
    return create_system_window(parent,
                                parent != 0
                                ? parent->get_parent_of_children() : 0,
                                show, title, pos, sysifc);
}

/*
 *   Create a system window given an explicit parent handle 
 */
int CTadsWin::create_system_window(CTadsWin *parent, HWND parent_hwnd,
                                   int show, const char *title,
                                   const RECT *pos, CTadsSyswin *sysifc)
{
    /* note my parent */
    parent_ = parent;

    /* remember the system interface object */
    sysifc_ = sysifc;

    /* create the child window */
    handle_ = sysifc->syswin_create_system_window(
        get_winstyle_ex(), title, get_winstyle(),
        pos->left, pos->top, pos->right, pos->bottom, parent_hwnd,
        0, CTadsApp::get_app()->get_instance(), this);

    /* if that failed, return a failure indication */
    if (handle_ == 0)
        return 1;

    /* show the window if desired */
    if (show)
        ShowWindow(handle_, SW_SHOW);

    /* success */
    return 0;
}


/*
 *   Windows message handler callback for MDI Frame windows 
 */
LRESULT CALLBACK CTadsWin::
   mdiframe_message_handler(HWND hwnd, UINT msg, WPARAM wpar, LPARAM lpar)
{
    CTadsWin *win;

    /* get the window from the window class */
    win = (CTadsWin *)GetWindowLong(hwnd, 0);

    /* see if the window handle has been set yet */
    if (win == 0)
    {
        switch(msg)
        {
        case WM_NCCREATE:
            /* creating the window - get the handle from the parameters */
            win = (CTadsWin *)((CREATESTRUCT *)lpar)->lpCreateParams;
            break;

        default:
            /* 
             *   use default handling for any messages we receive before
             *   we get our window object 
             */
            return DefFrameProc(hwnd, 0, msg, wpar, lpar);
        }

        /* set the window handle in window long #0 */
        SetWindowLong(hwnd, 0, (long)win);
    }

    /* use the common message dispatcher */
    return win->common_msg_handler(hwnd, msg, wpar, lpar);
}


/*
 *   Windows message handler callback for MDI child windows 
 */
LRESULT CALLBACK CTadsWin::
   mdichild_message_handler(HWND hwnd, UINT msg, WPARAM wpar, LPARAM lpar)
{
    CTadsWin *win;

    /* get the window from the window class */
    win = (CTadsWin *)GetWindowLong(hwnd, 0);

    /* see if the window handle has been set yet */
    if (win == 0)
    {
        switch(msg)
        {
        case WM_NCCREATE:
        case WM_CREATE:
            /* 
             *   For MDI child windows, we get a WM_CREATE message instead
             *   of a WM_NCCREATE.  We need to look inside the
             *   MDICREATESTRUCT structure instead of the CREATESTRUCT, as
             *   we do for non-MDI windows.  
             */
            win = (CTadsWin *)
                  ((MDICREATESTRUCT *)
                   (((CREATESTRUCT *)lpar)->lpCreateParams))->lParam;
            break;

        default:
            /* 
             *   return default handling for any messages we receive
             *   before we find our window object 
             */
            return DefMDIChildProc(hwnd, msg, wpar, lpar);
        }

        /* set the window handle in window long #0 */
        SetWindowLong(hwnd, 0, (long)win);
    }

    /* use the common message dispatcher */
    return win->common_msg_handler(hwnd, msg, wpar, lpar);
}


/*
 *   Windows message handler callback for standard windows
 */
LRESULT CALLBACK CTadsWin::
   std_message_handler(HWND hwnd, UINT msg, WPARAM wpar, LPARAM lpar)
{
    CTadsWin *win;

    /* get the window from the window class */
    win = (CTadsWin *)GetWindowLong(hwnd, 0);

    /* see if the window handle has been set yet */
    if (win == 0)
    {
        switch(msg)
        {
        case WM_NCCREATE:
            /* creating the window - get the handle from the parameters */
            win = (CTadsWin *)((CREATESTRUCT *)lpar)->lpCreateParams;
            break;
            
        default:
            /* 
             *   use default handling for any messages we receive before
             *   we get our window object 
             */
            return DefWindowProc(hwnd, msg, wpar, lpar);
        }

        /* set the window handle in window long #0 */
        SetWindowLong(hwnd, 0, (long)win);
    }

    /* use the common message dispatcher */
    return win->common_msg_handler(hwnd, msg, wpar, lpar);
}

/*
 *   Common message handler used by all window types 
 */
LRESULT CALLBACK CTadsWin::
   common_msg_handler(HWND hwnd, UINT msg, WPARAM wpar, LPARAM lpar)
{
    /*
     *   make sure the window handle is initialized in the window object,
     *   in case we got a message before we got back from CreateWindowEx
     *   (which is where the handle would normally first be stored in the
     *   window) 
     */
    handle_ = hwnd;

    /* see what message we got */
    switch(msg)
    {
    case WM_CREATE:
        do_create();
        break;

    case WM_CLOSE:
        if (!do_close())
            return 0;
        break;

    case WM_DESTROY:
        do_destroy();
        return 0;

    case WM_ACTIVATE:
        if (do_activate(LOWORD(wpar), HIWORD(wpar), (HWND)lpar))
            return 0;
        break;

    case WM_ACTIVATEAPP:
        if (do_activate_app((int)wpar, (DWORD)lpar))
            return 0;
        break;

    case WM_NCACTIVATE:
        if (do_ncactivate((int)wpar))
            return 0;
        break;

    case WM_MDIACTIVATE:
        if (do_mdiactivate((HWND)wpar, (HWND)lpar))
            return 0;
        break;

    case WM_CHILDACTIVATE:
        /* process the message */
        do_childactivate();

        /* always pass this to MDI */
        break;

    case WM_MDIREFRESHMENU:
        /* MDI menu refresh */
        if (do_mdirefreshmenu())
            return 0;
        break;

    case WM_MOUSEACTIVATE:
        {
            LRESULT result;

            /* see what the window thinks */
            if (do_mouseactivate((HWND)wpar, (int)LOWORD(lpar),
                                 (unsigned int)HIWORD(lpar), &result))
            {
                /* the window specified an explicit result - return it */
                return result;
            }
        }
        break;

    case WM_INITMENU:
        // $$$
        break;
        
    case WM_INITMENUPOPUP:
        init_menu_popup((HMENU)wpar, LOWORD(lpar), HIWORD(lpar));
        return 0;

    case WM_CONTEXTMENU:
        if (do_context_menu((HWND)wpar, LOWORD(lpar), HIWORD(lpar)))
            return 0;
        break;

    case WM_COMMAND:
        /* try running the command */
        if (do_command(HIWORD(wpar), LOWORD(wpar), (HWND)lpar))
        {
            /* 
             *   the command was handled - update any toolbars immediately,
             *   so that there's no delay in refreshing the toolbar
             *   appearance after an explicit user command action 
             */
            update_toolbar_buttons();

            /* tell Windows we handled it */
            return 0;
        }
        break;

    case WM_SYSCOMMAND:
        break;

    case WM_NOTIFY:
        /* 
         *   Send the notification and return the result.  Note that we don't
         *   invoke the default window procedure, no matter what the notify
         *   handler returns; default window procedures never have any use
         *   for notifications.  
         */
        return do_notify((int)wpar, ((LPNMHDR)lpar)->code, (LPNMHDR)lpar);

    case WM_SETFOCUS:
        /* call the virtual handler */
        do_setfocus((HWND)wpar);

        /* 
         *   Check to see if we must always pass this message to the
         *   default handler -- for MDI frame windows, for example, we
         *   must pass it to the default handler even if we handle it
         *   ourselves. 
         */
        if (always_pass_message(WM_SETFOCUS))
            break;

        /* handled */
        return 0;

    case WM_KILLFOCUS:
        do_killfocus((HWND)wpar);
        return 0;

    case WM_PAINT:
        /* paint the window (iconically or normally, depending on state) */
        if (IsIconic(hwnd) ? do_paint_iconic() : do_paint())
            return 0;
        break;

    case WM_ERASEBKGND:
        if (do_erase_bkg((HDC)wpar))
            return 1;
        break;

    case WM_QUERYDRAGICON:
        break;

    case WM_LBUTTONDBLCLK:
        break;

    case WM_LBUTTONDOWN:
        if (do_leftbtn_down(wpar, (short)LOWORD(lpar), (short)HIWORD(lpar),
                            lbtn_click.get_click_count(lpar)))
            return 0;
        break;

    case WM_LBUTTONUP:
        if (do_leftbtn_up(wpar, (short)LOWORD(lpar), (short)HIWORD(lpar)))
            return 0;
        break;

    case WM_RBUTTONDOWN:
        if (do_rightbtn_down(wpar, (short)LOWORD(lpar), (short)HIWORD(lpar),
                             rbtn_click.get_click_count(lpar)))
            return 0;
        break;

    case WM_RBUTTONUP:
        if (do_rightbtn_up(wpar, (short)LOWORD(lpar), (short)HIWORD(lpar)))
            return 0;
        break;

    case WM_NCLBUTTONDOWN:
        if (do_nc_leftbtn_down(get_key_mk_state(),
                               MAKEPOINTS(lpar).x, MAKEPOINTS(lpar).y,
                               lbtn_click.get_click_count(lpar), (int)wpar))
            return 0;
        break;

    case WM_NCLBUTTONDBLCLK:
        if (do_nc_leftbtn_down(get_key_mk_state(),
                               MAKEPOINTS(lpar).x, MAKEPOINTS(lpar).y,
                               2, (int)wpar))
            return 0;
        break;

    case WM_NCLBUTTONUP:
        if (do_nc_leftbtn_up(get_key_mk_state(),
                             MAKEPOINTS(lpar).x, MAKEPOINTS(lpar).y,
                             (int)wpar))
            return 0;
        break;

    case WM_NCRBUTTONDOWN:
        if (do_nc_rightbtn_down(get_key_mk_state(),
                                MAKEPOINTS(lpar).x, MAKEPOINTS(lpar).y,
                                rbtn_click.get_click_count(lpar), (int)wpar))
            return 0;
        break;

    case WM_NCRBUTTONUP:
        if (do_nc_rightbtn_up(get_key_mk_state(),
                              MAKEPOINTS(lpar).x, MAKEPOINTS(lpar).y,
                              (int)wpar))
            return 0;
        break;

    case WM_NCMOUSEMOVE:
        if (do_nc_mousemove(get_key_mk_state(),
                            MAKEPOINTS(lpar).x, MAKEPOINTS(lpar).y,
                            (int)wpar))
            return 0;
        break;

    case WM_MOUSEMOVE:
        if (do_mousemove(wpar, (short)LOWORD(lpar), (short)HIWORD(lpar)))
            return 0;
        break;

    case WM_CAPTURECHANGED:
        if (do_capture_changed((HWND)lpar))
            return 0;
        break;

    case WM_TIMER:
        if (do_timer(wpar))
            return 0;
        break;

    case WM_CHAR:
        if (do_char((TCHAR)wpar, lpar))
            return 0;
        break;

    case WM_HOTKEY:
        if (do_hotkey((int)wpar, (unsigned int)LOWORD(lpar),
                      (unsigned int)HIWORD(lpar)))
            return 0;
        break;
        
    case WM_KEYDOWN:
        if (do_keydown((int)wpar, lpar))
            return 0;
        break;

    case WM_KEYUP:
        if (do_keyup((int)wpar, lpar))
            return 0;
        break;

    case WM_SYSCHAR:
        if (do_syschar((TCHAR)wpar, lpar))
            return 0;
        break;

    case WM_SYSKEYDOWN:
        if (do_syskeydown((int)wpar, lpar))
            return 0;
        break;

    case WM_MENUCHAR:
        break;

    case WM_MENUSELECT:
        if ((HMENU)lpar == 0 && (UINT)HIWORD(wpar) == 0xffff)
        {
            if (menu_close((UINT)LOWORD(wpar)))
                return 0;
        }
        else
        {
            if (menu_item_select((unsigned int)LOWORD(wpar),
                                 (unsigned int)HIWORD(wpar), (HMENU)lpar))
            return 0;
        }
        break;

    case WM_SETCURSOR:
        /* if tracking a popup, use the default cursor setting */
        if (tracking_popup_menu_)
            break;

        /* ask the window to set the cursor */
        if (do_setcursor((HWND)wpar, (short)LOWORD(lpar),
                         (short)HIWORD(lpar)))
            return TRUE;
        break;

    case WM_VSCROLL:
        do_scroll(TRUE, (HWND)lpar,
                  (short)LOWORD(wpar), (short)HIWORD(wpar), FALSE);
        break;

    case WM_HSCROLL:
        do_scroll(FALSE, (HWND)lpar, (short)LOWORD(wpar),
                  (short)HIWORD(wpar), FALSE);
        break;

    case WM_MOUSEWHEEL:
        if (do_mousewheel(LOWORD(wpar), (int)(short)HIWORD(wpar),
                          LOWORD(lpar), HIWORD(lpar)))
            return 0;
        break;

    case WM_ENTERSIZEMOVE:
        do_entersizemove();
        break;

    case WM_EXITSIZEMOVE:
        do_exitsizemove();
        break;

    case WM_WINDOWPOSCHANGING:
        if (do_windowposchanging((WINDOWPOS *)lpar))
            return 0;
        break;

    case WM_WINDOWPOSCHANGED:
        if (do_windowposchanged((WINDOWPOS *)lpar))
            return 0;
        break;

    case WM_SHOWWINDOW:
        if (do_showwindow((int)wpar, (int)lpar))
            return 0;
        break;

    case WM_SIZE:
        /* let the subclass handle it */
        do_resize(wpar, (short)LOWORD(lpar), (short)HIWORD(lpar));

        /* 
         *   send the message to the system handler as well - if that
         *   handles it, do not send it to the default message handler 
         */
        if (sysifc_->syswin_do_resize(wpar, (short)LOWORD(lpar),
                                      (short)HIWORD(lpar)))
            return 0;
        break;

    case WM_MOVE:
        do_move(LOWORD(lpar), HIWORD(lpar));
        break;

    case WM_GETMINMAXINFO:
        break;

    case WM_QUERYNEWPALETTE:
        return do_querynewpalette();

    case WM_PALETTECHANGED:
        do_palettechanged((HWND)wpar);
        return 0;

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
        {
            HBRUSH br;

            /* try the override */
            br = do_ctlcolor(msg, (HDC)wpar, (HWND)lpar);

            /* 
             *   if that returned something, return it; otherwise, go on
             *   to the default handling 
             */
            if (br != 0)
                return (LPARAM)br;
        }
        break;

    case WM_SETTINGCHANGE:
        /* handle the system setting change notification */
        if (do_sys_setting_change(wpar, (const char *)lpar))
            return 0;
        break;

    case WM_SYSCOLORCHANGE:
        /* handle the system color change notification */
        if (do_sys_color_change())
            return 0;
        break;

    case MM_MOM_DONE:
    case MM_MOM_CLOSE:
    case MM_MOM_OPEN:
        /* handle the MIDI event */
        return do_midi_event(msg, wpar, lpar);

    default:
        /* check for a user message */
        if ((msg >= WM_USER && msg <= 0x7FFF)
            || (msg >= 0xC000 && msg <= 0xFFFF))
            return do_user_message(msg, wpar, lpar);
        break;
    }

    /*
     *   if we didn't override the processing, let the default window
     *   procedure process it 
     */
    return call_defwinproc(hwnd, msg, wpar, lpar);
}

/*
 *   Handle a notification message. 
 */
int CTadsWin::do_notify(int control_id, int notify_code, LPNMHDR nm)
{
    switch(notify_code)
    {
    case NM_SETFOCUS:
    case NM_KILLFOCUS:
        /* 
         *   Focus change message - by default, simply reflect these to
         *   our parent window, if we have one.  This allows windows to
         *   know when the focus is contained within one of their children
         *   and when it leaves.  
         */
        if (GetParent(handle_) != 0)
            SendMessage(GetParent(handle_), WM_NOTIFY, control_id,
                        (LPARAM)nm);

        /* handled */
        return TRUE;

    default:
        /* not handled */
        return FALSE;
    }
}

/*
 *   Update the toolbar buttons 
 */
void CTadsWin::update_toolbar_buttons()
{
    int i;

    /* go through each registered toolbar */
    for (i = 0 ; i < toolbar_cnt_ ; ++i)
    {
        int btncnt;
        int btn;

        /* go through this toolbar's buttons */
        btncnt = SendMessage(toolbars_[i], TB_BUTTONCOUNT, 0, 0);
        for (btn = 0 ; btn < btncnt ; ++btn)
        {
            TBBUTTON info;

            /* get information on this button */
            if (SendMessage(toolbars_[i], TB_GETBUTTON, btn, (LPARAM)&info))
            {
                /* if it's a command button, update its status */
                if ((info.fsStyle & TBSTYLE_SEP) == 0)
                {
                    unsigned oldstate;
                    unsigned newstate;

                    /* get the status of this command */
                    switch(check_command(info.idCommand))
                    {
                    case TADSCMD_ENABLED:
                        newstate = TBSTATE_ENABLED;
                        break;

                    case TADSCMD_DISABLED:
                        newstate = 0;
                        break;

                    case TADSCMD_CHECKED:
                        newstate = TBSTATE_ENABLED | TBSTATE_CHECKED;
                        break;

                    case TADSCMD_DISABLED_CHECKED:
                        newstate = TBSTATE_CHECKED;
                        break;

                    case TADSCMD_INDETERMINATE:
                        newstate = TBSTATE_ENABLED | TBSTATE_INDETERMINATE;
                        break;

                    case TADSCMD_DISABLED_INDETERMINATE:
                        newstate = TBSTATE_INDETERMINATE;
                        break;

                    default:
                        break;
                    }

                    /* get the current state */
                    oldstate = SendMessage(toolbars_[i], TB_GETSTATE,
                                           info.idCommand, 0);

                    /*
                     *   if the button is being pressed, don't change that in
                     *   the new state 
                     */
                    if ((oldstate & TBSTATE_PRESSED) != 0)
                        newstate |= TBSTATE_PRESSED;

                    /*
                     *   update the state only if it differs, to avoid
                     *   flicker or other overhead of unnecessary changes 
                     */
                    if (oldstate != newstate)
                    {
                        /* set the new state */
                        SendMessage(toolbars_[i], TB_SETSTATE,
                                    info.idCommand,
                                    (LPARAM)MAKELONG(newstate, 0));
                    }
                }
            }
        }
    }
}

/*
 *   Handle a MIDI event.  By default, we'll just let the MIDI player
 *   handle it for us.  
 */
int CTadsWin::do_midi_event(UINT msg, WPARAM wpar, LPARAM lpar)
{
    /* let the MIDI player handle it */
    return CTadsMidiFilePlayer::handle_mm_message(msg, wpar, lpar);
}


/*
 *   Initialize a popup menu that's about to be opened 
 */
void CTadsWin::init_menu_popup(HMENU menuhdl, unsigned int /*pos*/,
                               int sysmenu)
{
    int i;
    int cnt;
    UINT id;
    
    /* if it's the system menu, ignore it */
    if (sysmenu)
        return;

    /* run through each item in the menu, and check its command status */
    cnt = GetMenuItemCount(menuhdl);
    for (i = 0 ; i < cnt ; ++i)
    {
        TadsCmdStat_t stat;
        UINT flags;
        MENUITEMINFO info;
        
        /* get the ID for this item */
        id = GetMenuItemID(menuhdl, i);

        /* if it's a submenu, ignore this item */
        if (id == 0xffffffff)
            continue;

        /* get this item's information */
        info.cbSize = menuiteminfo_size_;
        info.fMask = MIIM_STATE;
        GetMenuItemInfo(menuhdl, i, MF_BYPOSITION, &info);

        /* check this item's ID */
        stat = check_command(id);

        /* 
         *   if they want to leave it unchanged (presumably because
         *   they've set the status already elsewhere), leave it as it is 
         */
        if (stat == TADSCMD_DO_NOT_CHANGE)
            continue;

        /* if it wasn't otherwise set, disable it by default */
        if (stat == TADSCMD_UNKNOWN)
            stat = TADSCMD_DISABLED;

        /* check the item if appropriate */
        flags = (stat == TADSCMD_CHECKED || stat == TADSCMD_DISABLED_CHECKED)
                ? MF_CHECKED : MF_UNCHECKED;
        CheckMenuItem(menuhdl, i, MF_BYPOSITION | flags);

        /* enable or disable the item as appropriate */
        flags = (stat == TADSCMD_ENABLED || stat == TADSCMD_CHECKED
                 || stat == TADSCMD_INDETERMINATE || stat == TADSCMD_DEFAULT)
                ? MF_ENABLED : MF_GRAYED;
        EnableMenuItem(menuhdl, i, MF_BYPOSITION | flags);

        /* if the default state is changing, make the change */
        if ((stat == TADSCMD_DEFAULT && (info.fState & MFS_DEFAULT) == 0)
            || (stat != TADSCMD_DEFAULT && (info.fState & MFS_DEFAULT) != 0))
        {
            /* set the new state */
            if (stat == TADSCMD_DEFAULT)
                info.fState |= MFS_DEFAULT;
            else
                info.fState &= ~MFS_DEFAULT;
            SetMenuItemInfo(menuhdl, i, MF_BYPOSITION, &info);
        }
    }
}

/*
 *   Set up a menu so that its items all use radio button checkmarks 
 */
void CTadsWin::set_menu_radiocheck(HMENU submenu)
{
    set_menu_radiocheck(submenu, 0, GetMenuItemCount(submenu) - 1);
}

/*
 *   Set up a menu so that a range of its items (by position, inclusive of
 *   first and last) use radio button checkmarks 
 */
void CTadsWin::set_menu_radiocheck(HMENU submenu, int first, int last)
{
    int i;

    /* loop through each item in the submenu */
    for (i = first ; i <= last ; ++i)
    {
        char buf[256];

        MENUITEMINFO minfo;

        /* 
         *   set up the menu item structure to receive information on the
         *   menu item -- all we need is the type, but getting the type
         *   also gets the label string, so we need to set up space to
         *   receive the string 
         */
        minfo.cbSize = menuiteminfo_size_;
        minfo.fMask = MIIM_TYPE;
        minfo.cch = sizeof(buf);
        minfo.dwTypeData = buf;

        /* retrieve the information on this item */
        GetMenuItemInfo(submenu, i, TRUE, &minfo);

        /* add the radio button check style */
        minfo.fType |= MFT_RADIOCHECK;

        /* set the item */
        SetMenuItemInfo(submenu, i, TRUE, &minfo);
    }
}


/*
 *   paint the window 
 */
int CTadsWin::do_paint()
{
    PAINTSTRUCT ps;

    /* hide the caret while painting, if we have a caret */
    HideCaret(handle_);

    /* set up painting, and remember the dc */
    display_hdc_ = hdc_ = BeginPaint(handle_, &ps);

    /* paint the affected area */
    do_paint_content(hdc_, &ps.rcPaint);

    /* finish painting */
    display_hdc_ = hdc_ = 0;
    EndPaint(handle_, &ps);

    /* show the caret again */
    ShowCaret(handle_);

    /* handled */
    return TRUE;
}

/*
 *   paint the window minimized 
 */
int CTadsWin::do_paint_iconic()
{
    PAINTSTRUCT ps;

    /* set up painting, and remember the dc */
    display_hdc_ = hdc_ = BeginPaint(handle_, &ps);

    /* paint the affected area */
    do_paint_content(hdc_, &ps.rcPaint);

    /* finish painting */
    display_hdc_ = hdc_ = 0;
    EndPaint(handle_, &ps);

    /* handled */
    return TRUE;
}


/*
 *   paint the content of the window
 */
void CTadsWin::do_paint_content(HDC hdc, const RECT *area_to_draw)
{
    /* fill the affected area with a white background */
    FillRect(hdc, area_to_draw, (HBRUSH)GetStockObject(WHITE_BRUSH));
}

/*
 *   resize the window 
 */
void CTadsWin::do_resize(int /*mode*/, int /*x*/, int /*y*/)
{
    /* nothing to do by default */
}

/*
 *   synthesize a resize event 
 */
void CTadsWin::synth_resize()
{
    RECT rc;

    /* get our current size */
    GetWindowRect(handle_, &rc);

    /* run through the normal resizing code at our current size */
    do_resize(is_win_maximized()
              ? SIZE_MAXIMIZED
              : IsIconic(handle_)
              ? SIZE_MINIMIZED
              : SIZE_RESTORED,
              rc.right - rc.left, rc.top - rc.bottom);
}

/*
 *   move the window 
 */
void CTadsWin::do_move(int /*x*/, int /*y*/)
{
    /* nothing to do by default */
}

/*
 *   Process window creation message 
 */
void CTadsWin::do_create()
{
    /* tell the system window interface about the creation */
    sysifc_->syswin_do_create();
}

/*
 *   Process a window close message.
 */
int CTadsWin::do_close()
{
    /* 
     *   default doesn't do any extra work - return true to indicate that
     *   we should go ahead and close the window 
     */
    return TRUE;
}

/*
 *   Process a destroy window message 
 */
void CTadsWin::do_destroy()
{
    /* tell the system interface object about it */
    sysifc_->syswin_do_destroy();

    /* forget my 'this' pointer */
    sysifc_->forget_this_ptr(handle_);

    /* if we still have a toolbar timer, kill it */
    if (tb_timer_id_ != 0)
    {
        KillTimer(handle_, tb_timer_id_);
        free_timer_id(tb_timer_id_);
        tb_timer_id_ = 0;
    }

    /* if we're registered as a drop target, unregister now */
    drop_target_unregister();

    /* release the reference the system window has on the object */
    Release();
}

/*
 *   Register as an OLE drop target 
 */
void CTadsWin::drop_target_register()
{
    /* if we're already registered, there's nothing to do */
    if (drop_target_regd_)
        return;

    /* register with OLE - if that fails, give up */
    if (RegisterDragDrop(handle_, this) != S_OK)
        return;

    /* note that we're registered */
    drop_target_regd_ = TRUE;
}

/*
 *   Unregister as an OLE drop target 
 */
void CTadsWin::drop_target_unregister()
{
    /* if we're not registered, there's nothing to do */
    if (!drop_target_regd_)
        return;

    /* unregister with OLE */
    RevokeDragDrop(handle_);

    /* note that we're no longer registered */
    drop_target_regd_ = FALSE;
}

/*
 *   process activation/deactivation 
 */
int CTadsWin::do_activate(int /*flag*/, int /*minimized*/, HWND /*other_win*/)
{
    /* inherit default processing */
    return FALSE;
}

/*
 *   invalidate the window 
 */
void CTadsWin::inval()
{
    /* invalidate the entire window */
    InvalidateRect(handle_, 0, FALSE);
}

void CTadsWin::inval(const RECT *area)
{
    /* invalidate the given area */
    InvalidateRect(handle_, area, FALSE);
}

/* timer ID tracking bit array */
unsigned long CTadsWin::timers_alloced_ = 0;

/*
 *   Allocate a new timer ID.  Returns zero if no timer is available.
 */
int CTadsWin::alloc_timer_id()
{
    tadswin_timer_alo *t;
    int id;

    /* if we have anything in the free list, allocate from the free list */
    if (free_timers_ != 0)
    {
        /* take the first item off the free list */
        t = free_timers_;

        /* unlink it from the free list */
        free_timers_ = t->nxt;
    }
    else
    {
        /* there's nothing in the free list, so allocate a new timer */
        t = (tadswin_timer_alo *)th_malloc(sizeof(tadswin_timer_alo));

        /* use and consume the next available never-before-allocated ID */
        t->id = next_timer_id_++;
    }

    /* get its ID */
    id = t->id;

    /* link our timer into the in-use list */
    t->nxt = inuse_timers_;
    inuse_timers_ = t;

    /* 
     *   Clear the ID in the item - the in-use list is not a record of the
     *   timers that are in use but merely a pool of allocation records that
     *   can be returned to the free list.  We don't care what ID's are in
     *   the in-use records, but emphasize this (to make sure we don't
     *   accidentally start relying on it somewhere) by clearing every ID as
     *   we move items to the in-use list.  
     */
    t->id = 0;

    /* return the ID we allocated */
    return id;
}

/*
 *   Free a timer ID 
 */
void CTadsWin::free_timer_id(int id)
{
    tadswin_timer_alo *t;

    /*
     *   Take the first allocation record off the in-use list.  The records
     *   in the in-use list are simply placeholders awaiting return to the
     *   free list, so we don't care about finding the ID we're freeing; just
     *   take the first available record and move it to the free list.  
     */
    t = inuse_timers_;
    inuse_timers_ = t->nxt;

    /* set the record's ID to the ID we're freeing */
    t->id = id;

    /* move the record onto the free list */
    t->nxt = free_timers_;
    free_timers_ = t;
}

/*
 *   run a popup context menu 
 */
int CTadsWin::track_context_menu_ext(HMENU menuhdl, int x, int y, DWORD flags)
{
    POINT pt;
    int ret;

    /* get screen coordinates for the popup menu */
    pt.x = x;
    pt.y = y;
    ClientToScreen(handle_, &pt);

    /* note that we're in the menu */
    begin_tracking_popup_menu();

    /* show the menu and run it */
    ret = TrackPopupMenu(menuhdl, flags, pt.x, pt.y, 0, handle_, 0);

    /* done with the tracking */
    end_tracking_popup_menu();

    /* return the result */
    return ret;
}


/*
 *   Determine if I'm maximized 
 */
int CTadsWin::is_win_maximized() const
{
    /* if my internal maximized flag is set, I'm maximized */
    if (maximized_)
        return TRUE;

    /* ask my system interface to make the call */
    return sysifc_->syswin_is_maximized(parent_);
}

/* 
 *   get the display color resolution in bits per pixel in the desktop window
 */
int CTadsWin::get_desk_bits_per_pixel()
{
    HDC deskdc;
    int bits_per_pixel;

    /* get the desktop device context */
    deskdc = GetDC(GetDesktopWindow());

    /* check the device capabilities to determine the color resolution */
    bits_per_pixel = GetDeviceCaps(deskdc, BITSPIXEL)
                     * GetDeviceCaps(deskdc, PLANES);

    /* done with the desktop dc */
    ReleaseDC(GetDesktopWindow(), deskdc);

    /* return our results */
    return bits_per_pixel;
}

/*
 *   Draw a bitmap 
 */
void CTadsWin::draw_hbitmap(HBITMAP bmp, const RECT *dstrc,
                            const RECT *srcrc)
{
    struct
    {
        BITMAPINFO bi;
        RGBQUAD colors[256];
    } bmphdr;
    char pix[1024];
    LPVOID pixptr;

    /* get the bitmap descriptor */
    bmphdr.bi.bmiHeader.biSize = sizeof(bmphdr.bi.bmiHeader);
    bmphdr.bi.bmiHeader.biBitCount = 0;
    GetDIBits(hdc_, bmp, 0, 0, 0,
              (LPBITMAPINFO)&bmphdr.bi, DIB_RGB_COLORS);

    /* 
     *   Allocate space for the pixel map.  If it'll fit in our stack
     *   buffer, use that, otherwise allocate space.  
     */
    if (bmphdr.bi.bmiHeader.biSizeImage <= sizeof(pix))
        pixptr = pix;
    else
        pixptr = th_malloc(bmphdr.bi.bmiHeader.biSizeImage);

    /* get the bits */
    GetDIBits(hdc_, bmp, 0, bmphdr.bi.bmiHeader.biHeight, pixptr,
              (LPBITMAPINFO)&bmphdr.bi, DIB_RGB_COLORS);
    
    /* draw it */
    StretchDIBits(hdc_, dstrc->left, dstrc->top,
                  dstrc->right - dstrc->left, dstrc->bottom - dstrc->top,
                  srcrc->left, srcrc->top,
                  srcrc->right - srcrc->left, srcrc->bottom - srcrc->top,
                  pixptr, (LPBITMAPINFO)&bmphdr.bi, DIB_RGB_COLORS, SRCCOPY);

    /* free space if we allocated it */
    if (pixptr != pix)
        th_free(pixptr);
}

/*
 *   Add a toolbar idle status processor 
 */
void CTadsWin::add_toolbar_proc(HWND toolbar)
{
    /* if we don't have room, ignore the request */
    if (toolbar_cnt_ == TADSWIN_TBMAX)
        return;

    /* add the toolbar to our list */
    toolbars_[toolbar_cnt_++] = toolbar;

    /* if this is our first toolbar, set up the timer */
    if (toolbar_cnt_ == 1)
    {
        tb_timer_id_ = alloc_timer_id();
        if (tb_timer_id_ != 0
            && SetTimer(handle_, tb_timer_id_, 500, (TIMERPROC)0) == 0)
        {
            /* couldn't set the timer with Windows - free it */
            free_timer_id(tb_timer_id_);
            tb_timer_id_ = 0;
        }
    }
}

/*
 *   Remove a toolbar idle status processor 
 */
void CTadsWin::rem_toolbar_proc(HWND toolbar)
{
    int i;
    
    /* find the toolbar in our list */
    for (i = 0 ; i < toolbar_cnt_ && toolbars_[i] != toolbar ; ++i) ;

    /* if we didn't find it, there's nothing to do */
    if (i == toolbar_cnt_)
        return;

    /* close the gap */
    for ( ; i + 1 < toolbar_cnt_ ; ++i)
        toolbars_[i] = toolbars_[i+1];

    /* decrement the count */
    --toolbar_cnt_;

    /* if there are no more toolbars, kill the timer */
    if (toolbar_cnt_ == 0 && tb_timer_id_ != 0)
    {
        /* tell Windows to stop the timer */
        KillTimer(handle_, tb_timer_id_);

        /* we're done with the timer ID, so deallocate it */
        free_timer_id(tb_timer_id_);
        tb_timer_id_ = 0;
    }
}

/*
 *   Handle timer events 
 */
int CTadsWin::do_timer(int timer_id)
{
    /* if it's the toolbar timer, handle it */
    if (timer_id == tb_timer_id_)
    {
        /* update the toolbar command buttons */
        update_toolbar_buttons();

        /* handled */
        return TRUE;
    }

    /* not handled */
    return FALSE;
}

/*
 *   Handle mouse movement 
 */
int CTadsWin::do_mousemove(int /*keys*/, int /*x*/, int /*y*/)
{
    /* continue any drag operation */
    drag_check();
    
    /* handled */
    return TRUE;
}

/*
 *   Mouse up 
 */
int CTadsWin::do_leftbtn_up(int /*keys*/, int /*x*/, int /*y*/)
{
    /* end any drag that we've started */
    drag_end(FALSE);

    /* handled */
    return TRUE;
}

/*
 *   Receive notification of a capture change 
 */
int CTadsWin::do_capture_changed(HWND)
{
    /* end any drag operation currently in progress */
    drag_end(TRUE);

    /* handled */
    return TRUE;
}

/*
 *   Prepare to begin a drag operation 
 */
void CTadsWin::drag_prepare(int already_have_capture)
{
    /* capture the mouse if we haven't done so already */
    if (!already_have_capture)
    {
        /* capture the mouse */
        SetCapture(handle_);

        /* note that we set the capture */
        drag_capture_ = TRUE;
    }

    /* note that we're watching for mouse dragging */
    drag_ready_ = TRUE;

    /* note the current cursor location */
    GetCursorPos(&drag_start_pos_);
}

/*
 *   Check dragging 
 */
int CTadsWin::drag_check()
{
    POINT curpos;
    DWORD effect;

    /* if we weren't ready to drag, ignore it */
    if (!drag_ready_)
        return FALSE;

    /* get the current cursor position */
    GetCursorPos(&curpos);

    /* check to see if the cursor has moved from the initial position */
    if (curpos.x < drag_start_pos_.x - 3
        || curpos.x > drag_start_pos_.x + 3
        || curpos.y < drag_start_pos_.y - 3
        || curpos.y > drag_start_pos_.y + 3)
    {
        IDataObject *dataobj;
        
        /* release the capture */
        ReleaseCapture();
        drag_capture_ = FALSE;
        
        /* we're no longer waiting for a drag (since we really have one) */
        drag_ready_ = FALSE;

        /* note the mouse key state at the start of the drag operation */
        drag_start_key_ = 0;
        if (get_lbtn_key()) drag_start_key_ |= MK_LBUTTON;
        if (get_rbtn_key()) drag_start_key_ |= MK_RBUTTON;
        if (get_mbtn_key()) drag_start_key_ |= MK_MBUTTON;

        /* get the data object for the source data */
        dataobj = get_drag_dataobj();

        /* notify myself that dragging is about to begin */
        drag_pre();

        /* start the drag operation */
        DoDragDrop(dataobj, this, get_drag_effects(), &effect);

        /* notify myself that dragging has ended */
        drag_post();

        /* remove our reference on the data object */
        dataobj->Release();

        /* indicate that drag-and-drop occurred */
        return TRUE;
    }

    /* didn't do any dragging yet */
    return FALSE;
}

/*
 *   End dragging 
 */
void CTadsWin::drag_end(int already_lost_capture)
{
    /* if we're not set up for dragging, there's nothing to do */
    if (!drag_ready_)
        return;

    /* release capture if we had it and we didn't already release it */
    if (!already_lost_capture && drag_capture_)
    {
        /* release the capture */
        ReleaseCapture();

        /* note that we no longer have capture */
        drag_capture_ = FALSE;
    }

    /* the drag is over */
    drag_ready_ = FALSE;
}


/* ------------------------------------------------------------------------ */
/*
 *   CTadsWin - IDragSource implementation 
 */

HRESULT STDMETHODCALLTYPE CTadsWin::QueryInterface(REFIID iid, void **ifc)
{
    /* check to see if it's one of our supported interfaces */
    if (iid == IID_IUnknown)
        *ifc = (void *)(IUnknown *)(IDropSource *)this;
    else if (iid == IID_IDropSource)
        *ifc = (void *)(IDropSource *)this;
    else if (iid == IID_IDropTarget)
        *ifc = (void *)(IDropTarget *)this;
    else
        return E_NOINTERFACE;

    /* add a reference on behalf of the caller */
    AddRef();

    /* success */
    return S_OK;
}

ULONG STDMETHODCALLTYPE CTadsWin::Release()
{
    ULONG ret;

    /* decrement the reference count and note it for returning */
    ret = --ole_refcnt_;

    /* if the reference count has dropped to zero, delete me */
    if (ole_refcnt_ == 0)
        delete this;

    /* return the new reference count */
    return ret;
}

HRESULT STDMETHODCALLTYPE
   CTadsWin::QueryContinueDrag(BOOL esc_pressed, DWORD key_state)
{
    /* if they hit the escape key, cancel the operation */
    if (esc_pressed)
        return DRAGDROP_S_CANCEL;

    /* 
     *   check the key state - if the original key that started the
     *   operation is no longer down, do the drop 
     */
    if ((key_state & drag_start_key_) == 0)
        return DRAGDROP_S_DROP;

    /* 
     *   if other mouse buttons besides the original key that started the
     *   operation are now down, abort the drop 
     */
    if ((key_state & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON))
        != drag_start_key_)
        return DRAGDROP_S_CANCEL;

    /* proceed with the operation */
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CTadsWin::GiveFeedback(DWORD effect)
{
    /* tell the system to use the default feedback */
    return DRAGDROP_S_USEDEFAULTCURSORS;
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
   CTadsWin::DragEnter(IDataObject __RPC_FAR * /*dataObj*/,
                       DWORD /*grfKeyState*/, POINTL /*pt*/,
                       DWORD __RPC_FAR *pdwEffect)
{
    /* by default, we won't allow dropping here */
    *pdwEffect = DROPEFFECT_NONE;
    return S_OK;
}

/*
 *   continue dragging over this window 
 */
HRESULT STDMETHODCALLTYPE CTadsWin::DragOver(DWORD /*grfKeyState*/,
                                             POINTL /*pt*/,
                                             DWORD __RPC_FAR *pdwEffect)
{
    /* by default, we won't allow dropping here */
    *pdwEffect = DROPEFFECT_NONE;
    return S_OK;
}

/*
 *   leave this window with no drop having occurred 
 */
HRESULT STDMETHODCALLTYPE CTadsWin::DragLeave()
{
    /* by default, we don't have anything we need to do here */
    return S_OK;
}

/*
 *   drop in this window 
 */
HRESULT STDMETHODCALLTYPE CTadsWin::Drop(IDataObject __RPC_FAR * /*dataObj*/,
                                         DWORD /*grfKeyState*/, POINTL /*pt*/,
                                         DWORD __RPC_FAR *pdwEffect)
{
    /* by default, don't allow dropping here */
    *pdwEffect = DROPEFFECT_NONE;
    return S_OK;
}

/* ------------------------------------------------------------------------ */
/*
 *   scrolling window implementation 
 */

/* static variables */
HHOOK CTadsWinScroll::sb_filter_hook_ = 0;
int CTadsWinScroll::sb_thumb_track_ = FALSE;


/*
 *   static class initialization 
 */
void CTadsWinScroll::class_init(CTadsApp *app)
{
    /* install a hook to capture scrollbar events */
    sb_filter_hook_ = SetWindowsHookEx(WH_MSGFILTER,
                                       (HOOKPROC)sb_filter_proc,
                                       app->get_instance(),
                                       GetCurrentThreadId());
}

/*
 *   static class termination 
 */
void CTadsWinScroll::class_terminate()
{
    /* remove our windows hook */
    UnhookWindowsHookEx(sb_filter_hook_);
}

/*
 *   construction
 */
CTadsWinScroll::CTadsWinScroll(int has_vscroll, int has_hscroll)
{
    /* no scrollbars yet */
    hscroll_ = 0;
    vscroll_ = 0;
    ext_hscroll_ = FALSE;
    ext_vscroll_ = FALSE;
    graybox_ = 0;
    sizebox_ = 0;
    vscroll_vis_ = TRUE;
    hscroll_vis_ = TRUE;

    /* position at top left */
    vscroll_ofs_ = 0;
    hscroll_ofs_ = 0;

    /* note whether the scrollbars are desired */
    has_vscroll_ = (has_vscroll != 0);
    has_hscroll_ = (has_hscroll != 0);

    /* presume there will be no sizebox */
    has_sizebox_ = FALSE;

    /* not drag-scrolling currently */
    in_drag_scroll_ = FALSE;
}

/*
 *   deletion 
 */
CTadsWinScroll::~CTadsWinScroll()
{
}

/*
 *   window creation 
 */
void CTadsWinScroll::do_create()
{
    RECT rc;
    int sbwid, sbht;

    /* inherit default */
    CTadsWin::do_create();

    /* get my area */
    GetClientRect(handle_, &rc);

    /* get the scrollbar sizes */
    sbwid = GetSystemMetrics(SM_CXVSCROLL);
    sbht = GetSystemMetrics(SM_CYHSCROLL);

    /* create the vertical scrollbar, if one was requested at construction */
    if (has_vscroll_)
    {
        /* create the scrollbar */
        vscroll_ = CreateWindow("SCROLLBAR", "",
                                WS_CHILD | WS_VISIBLE
                                | SBS_VERT | SBS_RIGHTALIGN,
                                0, 0, rc.right, rc.bottom - sbht,
                                handle_, 0,
                                CTadsApp::get_app()->get_instance(), 0);

        /* display it */
        ShowWindow(vscroll_, SW_SHOW);
    }

    /* create the horizontal scrollbar, if one was requested */
    if (has_hscroll_)
    {
        /* create the scrollbar */
        hscroll_ = CreateWindow("SCROLLBAR", "",
                                WS_CHILD | WS_VISIBLE
                                | SBS_HORZ | SBS_BOTTOMALIGN,
                                0, 0, rc.right - sbwid, rc.bottom,
                                handle_, 0,
                                CTadsApp::get_app()->get_instance(), 0);

        /* display it */
        ShowWindow(hscroll_, SW_SHOW);
    }

    /* create a sizebox control if required */
    if (has_sizebox_)
        init_sizebox();

    /* create gray box for the bottom corner if both are present */
    if (has_vscroll_ && has_hscroll_)
    {
        /* 
         *   Create a gray box for the corner gap.  We create this even if
         *   we have a size box, since in the maximized state we want to
         *   hide the size box and show an insert gray box instead. 
         */
        graybox_ = CreateWindowEx(WS_EX_WINDOWEDGE, "STATIC", "", WS_CHILD,
                                  rc.right - sbwid, rc.bottom - sbht,
                                  rc.right, rc.bottom,
                                  handle_, 0,
                                  CTadsApp::get_app()->get_instance(), 0);
    }

    /* show the appropriate corner control */
    if (sizebox_ != 0)
        ShowWindow(sizebox_, SW_SHOW);
    else if (graybox_ != 0)
        ShowWindow(graybox_, SW_HIDE);

    /* adjust initial scrollbar positions */
    adjust_scrollbar_ranges();
    adjust_scrollbar_positions();
}

/*
 *   Turn the sizebox on or off.
 */
void CTadsWinScroll::set_has_sizebox(int f)
{
    /* if the setting isn't changing, there's nothing to do */
    if ((f != 0) == (has_sizebox_ != 0))
        return;
    
    /* note the new setting */
    has_sizebox_ = f;

    /* create or delete the sizebox */
    if (has_sizebox_ && sizebox_ == 0)
    {
        /* we need to create a sizebox */
        init_sizebox();
    }
    else if (!has_sizebox_ && sizebox_ != 0)
    {
        /* we need to destroy the existing sizebox */
        DestroyWindow(sizebox_);
        sizebox_ = 0;
    }

    /* 
     *   refigure the scrollbar positions so that we set up the sizebox in
     *   the right place, or hide it, as appropriate 
     */
    adjust_scrollbar_positions();
}

/*
 *   Initialize the sizebox 
 */
void CTadsWinScroll::init_sizebox()
{
    /* if we already have a sizebox control, ignore this */
    if (sizebox_ != 0)
        return;

    /* create the sizebox control */
    sizebox_ = CreateWindow("SCROLLBAR", "",
                            WS_CHILD | SBS_SIZEGRIP
                            | SBS_SIZEBOXBOTTOMRIGHTALIGN,
                            0, 0, 1, 1,
                            handle_, 0,
                            CTadsApp::get_app()->get_instance(), 0);
}

/*
 *   set external scrollbars 
 */
void CTadsWinScroll::set_ext_scrollbar(HWND ext_vscroll, HWND ext_hscroll)
{
    /* set the external vertical scrollbar, if provided */
    if (ext_vscroll != 0)
    {
        vscroll_ = ext_vscroll;
        ext_vscroll_ = TRUE;
    }

    /* set the external horizontal scrollbar, if provided */
    if (ext_hscroll != 0)
    {
        hscroll_ = ext_hscroll;
        ext_hscroll_ = TRUE;
    }

    /* adjust the scrollbar ranges now that we've changed scrollbars */
    adjust_scrollbar_ranges();
}

/*
 *   handle a WM_MOUSEWHEEL (scroll wheel) event 
 */
int CTadsWinScroll::do_mousewheel(int keys, int dist, int x, int y)
{
    UINT lines;
    
    /* if we have no scrollbars, do nothing */
    if (!has_vscroll_ && !has_hscroll_)
        return FALSE;

    /* add the distance to the wheel accumulator */
    wheel_accum_ += dist;

    /* get the number of lines to scroll per increment */
    if (!SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &lines, 0))
    {
        /* failed to get the parameter - the default is 3 lines */
        lines = 3;
    }

    /* scroll by WHEEL_DELTA increments */
    while (wheel_accum_ >= WHEEL_DELTA || wheel_accum_ <= -WHEEL_DELTA)
    {
        UINT i;
        
        /* 
         *   Process a scroll up or down, as appropriate.  Apply the
         *   scrolling to the vertical scrollbar if we have one, or to the
         *   horizontal scrollbar if not. 
         */
        for (i = 0 ; i < lines ; ++i)
            do_scroll(has_vscroll_, has_vscroll_ ? vscroll_ : hscroll_,
                      wheel_accum_ > 0 ? SB_LINEUP : SB_LINEDOWN, 0, FALSE);

        /* remove one increment from the accumulated total */
        if (wheel_accum_ > 0)
            wheel_accum_ -= WHEEL_DELTA;
        else
            wheel_accum_ += WHEEL_DELTA;
    }

    /*  handled */
    return TRUE;
}

/*
 *   perform scrolling, either in response to a scrollbar input event or
 *   programmatically 
 */
void CTadsWinScroll::do_scroll(int vert, HWND hwnd, int scroll_code,
                               long pos, int use_pos)
{
    long newpos;
    long oldpos;
    SCROLLINFO info;

    /* if there's no control, ignore it */
    if (hwnd == 0)
        return;

    /* get the information on the scrollbar */
    info.cbSize = sizeof(info);
    info.fMask = SIF_ALL;
    GetScrollInfo(hwnd, SB_CTL, &info);

    /* presume the position won't change */
    newpos = info.nPos;

    /* figure out what we're doing */
    switch(scroll_code)
    {
    case SB_BOTTOM:
        newpos = info.nMax;
        break;

    case SB_ENDSCROLL:
        newpos = info.nPos;
        break;

    case SB_LINEUP:
        if (info.nPos > info.nMin)
            newpos = info.nPos - 1;
        break;

    case SB_LINEDOWN:
        newpos = info.nPos + 1;
        break;

    case SB_PAGEUP:
        newpos = info.nPos - info.nPage;
        if (newpos < info.nMin)
            newpos = info.nMin;
        break;

    case SB_PAGEDOWN:
        newpos = info.nPos + info.nPage;
        break;

    case SB_THUMBPOSITION:
        /*
         *   Get the new position.  Use the position passed by the caller if
         *   they asked us to, otherwise use the scrollbar's internal
         *   control position.  
         */
        newpos = (use_pos ? pos : info.nTrackPos);

        /* note that thumb tracking is done */
        sb_thumb_track_ = FALSE;
        break;

    case SB_THUMBTRACK:
        /*
         *   if we track the thumb continuously, update immediately;
         *   otherwise ignore thumb tracking and wait for the final
         *   THUMBPOSITION message 
         */
        newpos = active_scroll_thumb_tracking(hwnd) ? info.nTrackPos
                 : info.nPos;

        /* note that thumb tracking is in progress */
        sb_thumb_track_ = TRUE;
        break;

    case SB_TOP:
        newpos = info.nMin;
        break;
    }

    /* remember the original position */
    oldpos = (vert ? vscroll_ofs_ : hscroll_ofs_);

    /* 
     *   if we're not tracking the thumb, update the scrollbar (updating
     *   isn't necessary when tracking the thumb, because the scrollbar is
     *   doing the moving in the first place; it's also undesirable, since
     *   rounding from pixels to scroll units and then going back to pixels
     *   on the update can result in jumpiness) 
     */
    if (scroll_code != SB_THUMBTRACK)
    {
        /* set the new position */
        info.nPos = newpos;
        info.fMask = SIF_ALL | SIF_DISABLENOSCROLL;
        SetScrollInfo(hwnd, SB_CTL, &info, TRUE);

        /*
         *   check the new position, which Windows will limit to the valid
         *   range set in the scrollbar 
         */
        GetScrollInfo(hwnd, SB_CTL, &info);
    }
    else
    {
        /* 
         *   tracking the thumb - use the current position without updating
         *   the scrollbar, since doing so could cause jumpiness due to
         *   rounding from pixels to scroll units and back 
         */
        info.nPos = newpos;
    }

    /* if the position changed, do the scrolling */
    if (info.nPos != oldpos)
    {
        RECT rc;
        int  dx;
        int  dy;
        int  draw_now;

        /*
         *   Determine if we're going to redraw the window at its new
         *   scrolled position.  If the old scrolling position was valid,
         *   we'll update the window immediately; otherwise, we'll simply
         *   invalidate the window for a later refresh.  If the old scrolling
         *   position was invalid, we can't reliably refresh the previous
         *   contents of the window, so we'll need to redraw the whole thing;
         *   this is most likely to happen when we're making a drastic change
         *   to the window's contents anyway, so it's generally not worth the
         *   trouble to try to minimize redrawing in such cases.  
         */
        draw_now = (oldpos >= info.nMin && oldpos <= info.nMax);

        /* get the area to be scrolled */
        get_scroll_area(&rc, vert);

        /*
         *   Make sure any invalid area of the window is drawn immediately
         *   prior to scrolling, since Windows won't relocate the invalid
         *   region to compensate for the scrolling.  Do this only if we're
         *   immediately updating; if not, invalidate the window's entire
         *   scrolling area so we'll update the affected region at next
         *   regular painting opportunity.  
         */
        if (draw_now)
            UpdateWindow(handle_);
        else
            inval(&rc);

        /* hide the caret while scrolling, if we have one */
        HideCaret(handle_);

        /* do any pre-scrolling work */
        notify_pre_scroll(hwnd);

        /* do the scrolling if we're actually redrawing */
        if (draw_now)
        {
            /* calculate the change in position, in pixels */
            dx = (vert ? 0 : (oldpos - info.nPos) * get_hscroll_units());
            dy = (vert ? (oldpos - info.nPos) * get_vscroll_units() : 0);

            /* scroll the window by the offset */
            ScrollWindow(handle_, dx, dy, &rc, &rc);
        }

        /* remember the new scrolling position */
        if (vert)
        {
            /* fix up vertical scrolling position */
            vscroll_ofs_ = info.nPos;
        }
        else
        {
            /* fix up horizontal scrolling position */
            hscroll_ofs_ = info.nPos;
        }

        /* redraw the exposed area immediately if we're drawing in-line */
        if (draw_now)
            UpdateWindow(handle_);

        /* okay to show the caret again */
        ShowCaret(handle_);

        /* do any subclass-specific handling */
        notify_scroll(hwnd, oldpos, info.nPos);
    }

    /*
     *   Get the new scroll settings.  If they differ from the current
     *   settings in the min, max, or page values, we need to udpate the
     *   scrollbar.  This can happen when, for example, we're scrolled off
     *   too far to the right for the current window size (which can be
     *   the case after we resize the window while it's scrolled), then
     *   scroll up or left.  Only do this on scrolling left or up or
     *   setting the position explicitly (but not during tracking), since
     *   these parameters should not be subject to change when scrolling
     *   down or right, and we don't want them changing dynamically during
     *   tracking.  
     */
    switch(scroll_code)
    {
    case SB_BOTTOM:
    case SB_LINELEFT:
    case SB_PAGELEFT:
    case SB_THUMBPOSITION:
    case SB_TOP:
        {
            SCROLLINFO newinfo;
            int old_vis, new_vis;

            /* get the new scrollbar information */
            new_vis = get_scroll_info(vert, &newinfo);

            /* 
             *   if there's any change in the scrollbar range or visibility,
             *   adjust the scrollbar to reflect the new settings 
             */
            old_vis = ((vert ? hscroll_vis_ : vscroll_vis_) != 0);
            if (newinfo.nMin != info.nMin
                || newinfo.nMax != info.nMax
                || newinfo.nPage != info.nPage
                || new_vis != old_vis)
                adjust_scrollbar_ranges();
        }
        break;
        
    default:
        /* don't adjust on other changes */
        break;
    }
}

/*
 *   Windows hook procedure for the message filter hook.
 *   
 *   We use this hook to process scrollbar events.  When the scrollbar is
 *   tracking the mouse for thumb movement, we'll note this when we see the
 *   initial SB_THUMBTRACK message in the parent window.  Once we see this
 *   event in the parent, we'll constrain subsequent mouse events in the
 *   scrollbar, which are filtered through this hook, to stay within the
 *   scrollbar area.  
 */
LRESULT CALLBACK CTadsWinScroll::sb_filter_proc(int code, WPARAM wpar,
                                                LPARAM lpar)
{
    MSG *msg;

    /* get the message object */
    msg = (MSG *)lpar;

    /* 
     *   check the code - if it's a mouse-move message in the scrollbar,
     *   constrain the mouse position so that it lies within the area of the
     *   scrollbar, even if the mouse has strayed 
     */
    if (code == MSGF_SCROLLBAR
        && sb_thumb_track_
        && (msg->message == WM_MOUSEMOVE
            || msg->message == WM_LBUTTONUP
            || msg->message == WM_RBUTTONUP
            || msg->message == WM_MBUTTONUP))
    {
        RECT rc;
        int x;
        int y;

        /* get the original coordinates from the message */
        x = (short)LOWORD(msg->lParam);
        y = (short)HIWORD(msg->lParam);

        /* get the client area of the scrollbar */
        GetClientRect(msg->hwnd, &rc);

        /* force the x position to lie within the scrollbar */
        if (x < 0)
            x = 0;
        else if (x > rc.right)
            x = rc.right;

        /* force the y position to lie within the scrollbar */
        if (y < 0)
            y = 0;
        else if (y > rc.bottom)
            y = rc.bottom;

        /* reconstruct the lparam with the constrained position */
        msg->lParam = MAKELONG(x, y);
    }

    /* invoke the next hook */
    return CallNextHookEx(sb_filter_hook_, code, wpar, lpar);
}

/*
 *   Get the scrolling area 
 */
void CTadsWinScroll::get_scroll_area(RECT *rc, int /*vertical*/) const
{
    /* start off with the entire client area */
    GetClientRect(handle_, rc);

    /* subtract out the scrollbars if present */
    if (vscroll_is_visible())
        rc->right -= GetSystemMetrics(SM_CXVSCROLL);

    if (hscroll_is_visible())
        rc->bottom -= GetSystemMetrics(SM_CYHSCROLL);
}

/*
 *   Adjust scrollbars to their proper positions 
 */
void CTadsWinScroll::adjust_scrollbar_positions()
{
    RECT rc;
    int sbwid;
    int sbht;
    int graybox_vis;
    int sizebox_vis;

    /* get the area of the window and the scrollbar metrics */
    GetClientRect(handle_, &rc);
    sbwid = GetSystemMetrics(SM_CXVSCROLL);
    sbht = GetSystemMetrics(SM_CYHSCROLL);

    /* 
     *   If there's a sizebox control, figure out whether to show or hide
     *   it.  Show the sizebox only if the window isn't maximized, and
     *   both the horizontal and vertical scrollbars are visible. 
     */
    sizebox_vis = (sizebox_ != 0
                   && !is_win_maximized()
                   && hscroll_ != 0 && !ext_hscroll_ && hscroll_vis_
                   && vscroll_ != 0 && !ext_vscroll_ && vscroll_vis_);

    /* show or hide the sizebox according to what we've decided */
    ShowWindow(sizebox_, sizebox_vis ? SW_SHOW : SW_HIDE);

    /* figure the position of the vertical scrollbar */
    if (vscroll_ != 0 && !ext_vscroll_)
        MoveWindow(vscroll_, rc.right - sbwid, 0, sbwid,
                   rc.bottom
                   - (((hscroll_vis_ && hscroll_ != 0 && !ext_hscroll_)
                       || sizebox_vis) ? sbht : 0),
                   TRUE);

    /* figure the position of the horizontal scrollbar */
    if (hscroll_ != 0 && !ext_hscroll_)
        MoveWindow(hscroll_, 0, rc.bottom - sbht,
                   rc.right
                   - (((vscroll_vis_ && vscroll_ != 0 && !ext_vscroll_)
                       || sizebox_vis) ? sbwid : 0),
                   sbht, TRUE);

    /* 
     *   Determine if the gray box will be visible.  For now, assume it'll
     *   be visible if we have a corner gap.  If we also have a size box,
     *   we won't show the gray box, but we'll figure that out
     *   momentarily. 
     */
    graybox_vis = (hscroll_vis_ && vscroll_vis_
                   && !ext_hscroll_ && !ext_vscroll_);
    
    /* move the size box, if we have one */
    if (sizebox_ != 0)
    {
        /* 
         *   If we have a sizing box, disable it on maximize and enable it
         *   on restore.  If we're inside a parent window, use the
         *   parent's current mode rather than our own.  
         */
        if (has_sizebox_)
        {
            /* move the box to the appropriate new location */
            MoveWindow(sizebox_, rc.right - sbwid, rc.bottom - sbht,
                       sbwid, sbht, TRUE);

            /* hide the gray box if we're showing the size box */
            if (sizebox_vis)
                graybox_vis = FALSE;
        }
    }

    /* move the gray box if necessary */
    if (graybox_ != 0)
    {
        /* move the box to the appropriate new location */
        MoveWindow(graybox_, rc.right - sbwid, rc.bottom - sbht,
                   sbwid, sbht, TRUE);

        /* show or hide it as appropriate */
        ShowWindow(graybox_, graybox_vis ? SW_SHOW : SW_HIDE);
    }
}

/*
 *   resize the window 
 */
void CTadsWinScroll::do_resize(int mode, int, int)
{
    switch(mode)
    {
    case SIZE_MAXHIDE:
    case SIZE_MAXSHOW:
    case SIZE_MINIMIZED:
        /* ignore these modes */
        break;

    case SIZE_MAXIMIZED:
    case SIZE_RESTORED:
    default:
        /* position the scrollbars and the gray box */
        adjust_scrollbar_positions();

        /* adjust the scrollbar settings and positions */
        adjust_scrollbar_ranges();
        break;
    }
}

/*
 *   Set control colors 
 */
HBRUSH CTadsWinScroll::do_ctlcolor(UINT msg, HDC hdc, HWND hwnd)
{
    /* if it's the little box at the scrollbar junction, paint gray */
    if (!has_sizebox_ && msg == WM_CTLCOLORSTATIC && hwnd == graybox_)
        return (HBRUSH)GetStockObject(LTGRAY_BRUSH);

    /* use the default for anything else */
    return CTadsWin::do_ctlcolor(msg, hdc, hwnd);
}


/*
 *   Initialize drag scrolling.  Sets a timer that will be called to
 *   simulate mouse move events when the mouse is being held outside the
 *   window.  
 */
void CTadsWinScroll::start_drag_scroll()
{
    /* remember that we're doing drag scrolling */
    in_drag_scroll_ = TRUE;

    /* allocate a timer for the drag scroll operations */
    if ((drag_scroll_timer_id_ = alloc_timer_id()) == 0)
        return;

    /* set up to receive timer events */
    SetTimer(handle_, drag_scroll_timer_id_, 20, 0);

    /* set the time for the next drag scroll */
    drag_scroll_time_ = GetTickCount() + TADSWIN_DRAG_SCROLL_WAIT;
}

/*
 *   Perform drag scrolling if appropriate 
 */
int CTadsWinScroll::maybe_drag_scroll(long x, long y,
                                      int x_inset, int y_inset)
{
    RECT vrc, hrc;
    int scrolled = FALSE;
    SCROLLINFO oldinfo;
    SCROLLINFO newinfo;

    /*
     *   if insufficient time has elapsed since the last drag scroll, do
     *   nothing 
     */
    if (GetTickCount() < drag_scroll_time_)
        return FALSE;

    /*
     *   if the mouse is outside the scrolling area, scroll in the
     *   appropriate direction 
     */
    get_scroll_area(&vrc, TRUE);
    get_scroll_area(&hrc, FALSE);
    if (hscroll_ != 0
        && IsWindowVisible(hscroll_)
        && IsWindowEnabled(hscroll_))
    {
        /* note the old horizontal scroll position */
        get_scroll_info(FALSE, &oldinfo);

        /* see which way we need to scroll, if any */
        if (x < hrc.left + x_inset)
            do_scroll(FALSE, hscroll_, SB_LINEUP, 0, FALSE);
        else if (x > hrc.right - x_inset)
            do_scroll(FALSE, hscroll_, SB_LINEDOWN, 0, FALSE);

        /* get the new position and note whether it changed */
        get_scroll_info(FALSE, &newinfo);
        scrolled = (oldinfo.nPos != newinfo.nPos);
    }

    if (vscroll_ != 0
        && IsWindowVisible(vscroll_)
        && IsWindowEnabled(vscroll_))
    {
        /* note the old horizontal scroll position */
        get_scroll_info(TRUE, &oldinfo);

        if (y < vrc.top + y_inset)
            do_scroll(TRUE, vscroll_, SB_LINEUP, 0, FALSE);
        else if (y > vrc.bottom - y_inset)
            do_scroll(TRUE, vscroll_, SB_LINEDOWN, 0, FALSE);

        /* get the new position and note whether it changed */
        get_scroll_info(TRUE, &newinfo);
        scrolled |= (oldinfo.nPos != newinfo.nPos);
    }

    /* if we scrolled, set next scrolling time */
    drag_scroll_time_ += TADSWIN_DRAG_SCROLL_WAIT;

    /* return indication of whether scrolling occurred */
    return scrolled;
}

/*
 *   End drag scrolling.  This should be called when capture is released
 *   to remove the drag-scroll timer.  
 */
void CTadsWinScroll::end_drag_scroll()
{
    /* remove the timer */
    if (drag_scroll_timer_id_ != 0)
    {
        KillTimer(handle_, drag_scroll_timer_id_);
        free_timer_id(drag_scroll_timer_id_);
    }

    /* we're no longer doing drag scrolling */
    in_drag_scroll_ = FALSE;
}

/*
 *   Process a timer event.  If we're doing drag scrolling, we'll act as
 *   though we'd received a mouse-moved event 
 */
int CTadsWinScroll::do_timer(int timer_id)
{
    /*
     *   if we're doing drag scrolling, and the this is the drag-scroll
     *   timer, handle it 
     */
    if (in_drag_scroll_ && timer_id == drag_scroll_timer_id_)
    {
        POINT pt;
        RECT hrc, vrc;
        
        /*
         *   if the mouse is outside our scrolling area, generate a
         *   mouse-moved event even if the mouse is in the same place it's
         *   been 
         */
        GetCursorPos(&pt);
        ScreenToClient(handle_, &pt);
        get_scroll_area(&vrc, TRUE);
        get_scroll_area(&hrc, FALSE);
        if (!PtInRect(&hrc, pt) || !PtInRect(&vrc, pt))
        {
            /* generate a mouse-moved event */
            do_mousemove(get_key_mk_state(), pt.x, pt.y);
        }

        /* handled */
        return TRUE;
    }
    
    /* inherit default */
    return CTadsWin::do_timer(timer_id);
}

/* ------------------------------------------------------------------------ */
/*
 *   Basic Window System Interface class 
 */

/*
 *   get the handle to use as the parent of children of this window -
 *   we'll simply return our window's handle 
 */
HWND CTadsSyswin::syswin_get_parent_of_children() const
{
    /* return my window's handle */
    return win_->get_handle();
}

/*
 *   create the system window object 
 */
HWND CTadsSyswin::syswin_create_system_window(DWORD ex_style,
                                              const textchar_t *wintitle,
                                              DWORD style,
                                              int x, int y, int wid, int ht,
                                              HWND parent, HMENU menu,
                                              HINSTANCE inst, void *param)
{
    return CreateWindowEx(ex_style, CTadsWin::win_class_name,
                          wintitle, style,
                          x, y, wid, ht, parent, menu, inst, param);
}

/*
 *   Set the menu bar for the window.  For default windows, we merely call
 *   SetMenu.
 */
void CTadsSyswin::syswin_set_win_menu(HMENU menu, int /*win_menu_index*/)
{
    SetMenu(win_->get_handle(), menu);
}

/*
 *   Determine if I'm maximized 
 */
int CTadsSyswin::syswin_is_maximized(CTadsWin *parent) const
{
    WINDOWPLACEMENT winpl;
    HWND par;
    HWND nxt;

    /* 
     *   if I'm a child window, use my parent's information, since I'm
     *   dependent on my parent's layout 
     */
    if ((GetWindowLong(win_->get_handle(), GWL_STYLE) & WS_CHILD) != 0)
    {
        /* if I have a CTadsWin parent, ask it to make the call */
        if (parent != 0)
            return parent->is_win_maximized();

        /* find the outermost parent handle that's a popup window */
        for (par = win_->get_handle() ; par != 0 ; par = nxt)
        {
            /* if this window is not a child, stop looking */
            if ((GetWindowLong(par, GWL_STYLE) & WS_CHILD) == 0)
                break;
            
            /* move on to the parent */
            nxt = GetParent(par);
            
            /* if there's not another parent, stop searching */
            if (nxt == 0)
                break;
        }
    }
    else
    {
        /* I'm not a popup window, so we can make our own determination */
        par = win_->get_handle();
    }

    /* get the window's placement to determine if it's maximized */
    winpl.length = sizeof(winpl);
    GetWindowPlacement(par, &winpl);
    return (winpl.showCmd == SW_SHOWMAXIMIZED);
}

/* ------------------------------------------------------------------------ */
/*
 *   MDI Frame Window System Interface subclass 
 */

/*
 *   create the system window object 
 */
HWND CTadsSyswinMdiFrame::
   syswin_create_system_window(DWORD ex_style,
                               const textchar_t *wintitle,
                               DWORD style,
                               int x, int y, int wid, int ht,
                               HWND parent, HMENU menu,
                               HINSTANCE inst, void *param)
{
    return CreateWindowEx(ex_style, CTadsWin::mdiframe_win_class_name,
                          wintitle, style, x, y, wid, ht,
                          parent, menu, inst, param);
}

/*
 *   Process frame creation.  We must create the client window as part of
 *   the creation of the frame. 
 */
void CTadsSyswinMdiFrame::syswin_do_create()
{
    CLIENTCREATESTRUCT ccs;
    
    /* retrieve the Window menu handle */
    ccs.hWindowMenu = GetSubMenu(GetMenu(win_->get_handle()), win_menu_idx_);

    /* set the first command ID */
    ccs.idFirstChild = win_menu_cmd_;

    /* 
     *   Create the MDI client object.  If the caller provided a CTadsWin
     *   object for the client window, use that; otherwise, create a
     *   default MDICLIENT window. 
     */
    if (client_win_ != 0)
    {
        RECT rc;

        /* 
         *   the initial position doesn't matter; the frame will resize
         *   the client after creation 
         */
        SetRect(&rc, 0, 0, 0, 0);

        /* create the system window for the CTadsWin object */
        client_win_
            ->create_system_window(win_, win_->get_handle(), TRUE, "", &rc,
                                   new CTadsSyswinMdiClient(client_win_));

        /* note the client handle from the client window */
        client_handle_ = client_win_->get_handle();
    }
    else
    {
        /* 
         *   there's no CTadsWin object for the client, so create a
         *   default Windows internal MDICLIENT window 
         */
        client_handle_ = CreateWindowEx(WS_EX_CLIENTEDGE, "MDICLIENT", 0,
                                        WS_CHILD | WS_CLIPCHILDREN
                                        | WS_VSCROLL | WS_HSCROLL
                                        | MDIS_ALLCHILDSTYLES ,
                                        0, 0, 0, 0, win_->get_handle(),
                                        (HMENU)0,
                                        CTadsApp::get_app()->get_instance(),
                                        (LPSTR)&ccs);

        /* show the client window */
        ShowWindow(client_handle_, SW_SHOW);
    }

    /* inform the application that we're here */
    CTadsApp::get_app()->set_mdi_win(this);
}

/*
 *   process window destruction
 */
void CTadsSyswinMdiFrame::syswin_do_destroy()
{
    /* inform the application that we're not around any more */
    CTadsApp::get_app()->set_mdi_win(0);
}

/* 
 *   Handle a resize event.  We will not allow the default MDI frame
 *   procedure to receive the message, since it will simply grab all of
 *   the available client space for the MDI client window.  Instead, we'll
 *   reduce the available client space by the sizes of any adorning
 *   windows in the frame (such as toolbars and status lines), then resize
 *   the client area to take any remaining space.  
 */
int CTadsSyswinMdiFrame::syswin_do_resize(int mode, int x, int y)
{
    RECT rc;
    HWND chi;
    POINT ofs;

    /* if the window does this calculation itself, we need do nothing here */
    if (win_resizes_client_)
        return TRUE;

    /* 
     *   start off assuming that we'll give the entire client area to the
     *   MDI client window 
     */
    SetRect(&rc, 0, 0, x, y);

    /* 
     *   get the screen coordinates of the upper left of the frame's
     *   client area, so that we can offset the screen coordinates of
     *   children to get their frame-relative positions 
     */
    ofs.x = ofs.y = 0;
    ScreenToClient(win_->get_handle(), &ofs);

    /* 
     *   Go through our immediate children.  For each child window that is
     *   not the MDI client, reduce the available space to exclude the
     *   area consumed by the child window.
     */
    for (chi = GetWindow(win_->get_handle(), GW_CHILD) ;
         chi != 0 ; chi = GetWindow(chi, GW_HWNDNEXT))
    {
        RECT chi_rc;
        
        /* skip the MDI client window, of course */
        if (chi == client_handle_)
            continue;

        /* if the window is hidden, skip it */
        if (!IsWindowVisible(chi))
            continue;

        /* get this window's area (in screen coordinates) */
        GetWindowRect(chi, &chi_rc);

        /* adjust to frame-relative coordinates */
        OffsetRect(&chi_rc, ofs.x, ofs.y);

        /* subtract this area from the available space */
        SubtractRect(&rc, &rc, &chi_rc);
    }

    /* resize the MDI client window to the available space */
    MoveWindow(client_handle_, rc.left, rc.top,
               rc.right - rc.left, rc.bottom - rc.top, TRUE);
    
    /* handled - do not let the default MDI frame procedure do anything */
    return TRUE;
}

/*
 *   Set the menu bar for the window.  For MDI frame windows, we send the
 *   WM_MDISETMENU message to the client window.  
 */
void CTadsSyswinMdiFrame::syswin_set_win_menu(HMENU menu, int win_menu_index)
{
    HMENU win_menu;
    
    /* get the "Window" menu, if appropriate */
    if (win_menu_index == 0)
        win_menu = 0;
    else if (win_menu_index > 0)
        win_menu = GetSubMenu(menu, win_menu_index - 1);
    else
        win_menu = GetSubMenu(menu, GetMenuItemCount(menu) + win_menu_index);

    /* tell the MDI client window about the new menu */
    SendMessage(client_handle_, WM_MDISETMENU,
                (WPARAM)menu, (WPARAM)win_menu);

    /* make sure the new menu gets drawn correctly */
    DrawMenuBar(win_->get_handle());
}

/* ------------------------------------------------------------------------ */
/*
 *   MDI Child Window System Interface Object 
 */

/*
 *   create the system window 
 */
HWND CTadsSyswinMdiChild::
   syswin_create_system_window(DWORD ex_style,
                               const textchar_t *wintitle,
                               DWORD style,
                               int x, int y, int wid, int ht,
                               HWND parent, HMENU menu,
                               HINSTANCE inst, void *param)
{
    /* 
     *   go create the window, adding the WS_EX_MDICHILD extended style
     *   and using the MDICREATESTRUCT as the user-defined parameter
     *   pointer 
     */
    return CreateWindowEx(ex_style | WS_EX_MDICHILD,
                          CTadsWin::mdichild_win_class_name, wintitle, style,
                          x, y, wid, ht, parent, menu, inst, param);
}

/*
 *   Determine if I'm maximized 
 */
int CTadsSyswinMdiChild::syswin_is_maximized(CTadsWin *parent) const
{
    HWND hwnd;
    BOOL maximized;
    
    /*
     *   To determine if an MDI child is maximized, ask the MDI client
     *   window for information on its active child window.  If I'm the
     *   active child window, and the active child window is maximized,
     *   then I'm maximized; otherwise I'm not maximized.  The MDI client
     *   is our parent handle.  
     */
    hwnd = (HWND)SendMessage(GetParent(win_->get_handle()),
                             WM_MDIGETACTIVE, 0, (LPARAM)&maximized);
    return (hwnd == win_->get_handle() && maximized);
}

/* ------------------------------------------------------------------------ */
/*
 *   MDI Client "superclass" 
 */

/*
 *   statics 
 */
WNDPROC CTadsSyswinMdiClient::base_proc_ = 0;
int CTadsSyswinMdiClient::base_wnd_extra_ = 0;

/*
 *   create the system window object 
 */
HWND CTadsSyswinMdiClient::
   syswin_create_system_window(DWORD ex_style,
                               const textchar_t *wintitle,
                               DWORD style,
                               int x, int y, int wid, int ht,
                               HWND parent, HMENU menu,
                               HINSTANCE inst, void *param)
{
    return CreateWindowEx(ex_style, CTadsWin::mdiclient_win_class_name,
                          wintitle, style, x, y, wid, ht,
                          parent, menu, inst, param);
}

/*
 *   message handler 
 */
LRESULT CALLBACK CTadsSyswinMdiClient::
   message_handler(HWND hwnd, UINT msg, WPARAM wpar, LPARAM lpar)
{
    CTadsWin *win;
    
    /* 
     *   get the window from the window extra data - note that our extra
     *   data comes after the MDICLIENT extra data 
     */
    win = (CTadsWin *)GetWindowLong(hwnd, base_wnd_extra_ + 0);

    /* see if the window handle has been set yet */
    if (win == 0)
    {
        switch(msg)
        {
        case WM_NCCREATE:
            /* get the window from the creation parameters */
            win = (CTadsWin *)((CREATESTRUCT *)lpar)->lpCreateParams;
            break;

        default:
            /* 
             *   return default handling for any messages we receive
             *   before we find our window object 
             */
            return CallWindowProc((WNDPROC)base_proc_,
                                  hwnd, msg, wpar, lpar);
        }

        /* 
         *   set the window handle in window long #0 - note that our #0
         *   goes after the MDICLIENT extra data 
         */
        SetWindowLong(hwnd, base_wnd_extra_ + 0, (long)win);
    }

    /* use the common message dispatcher */
    return win->common_msg_handler(hwnd, msg, wpar, lpar);
}
