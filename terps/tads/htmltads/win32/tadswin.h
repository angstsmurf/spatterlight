/* $Header: d:/cvsroot/tads/html/win32/tadswin.h,v 1.4 1999/07/11 00:46:48 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadswin.h - TADS window classes for win32
Function
  These classes define Win32 window objects.
Notes
  
Modified
  09/16/97 MJRoberts  - Creation
*/

#include <Ole2.h>
#include <Windows.h>
#include <commctrl.h>

#ifndef TADSWIN_H
#define TADSWIN_H

#ifndef TADSHTML_H
#include "tadshtml.h"
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
                             SIZE *txtsiz);


/* ------------------------------------------------------------------------ */
/*
 *   Command status codes
 *   
 *   TADSCMD_DO_NOT_CHANGE indicates that the status of a menu item or
 *   toolbar button should not be changed from what it's already set to.
 *   This can be used as the return value from CTadsWin::check_command(),
 *   for example, when the routine knows that the value has already been
 *   set properly elsewhere and does not need to be updated during the
 *   normal menu setup.  
 */
enum TadsCmdStat_t
{
    TADSCMD_UNKNOWN,                                /* unrecognized command */
    TADSCMD_ENABLED,                                     /* command enabled */
    TADSCMD_DISABLED,                                   /* command disabled */
    TADSCMD_CHECKED,                        /* command enabled and selected */
    TADSCMD_DISABLED_CHECKED,              /* command disabled and selected */
    TADSCMD_INDETERMINATE,    /* command enabled, state indeterminate/mixed */
    TADSCMD_DISABLED_INDETERMINATE,        /* disabled, state indeterminate */
    TADSCMD_DEFAULT,                      /* default item - implies enabled */
    TADSCMD_DO_NOT_CHANGE            /* leave the status as it is currently */
};


/* ------------------------------------------------------------------------ */
/*
 *   Convert an HTML_color_t to a COLORREF, and vice versa 
 */
inline COLORREF HTML_color_to_COLORREF(HTML_color_t htmlcolor)
{
    return RGB(HTML_color_red(htmlcolor),
               HTML_color_green(htmlcolor),
               HTML_color_blue(htmlcolor));
}

inline COLORREF_to_HTML_color(COLORREF rgb)
{
    return HTML_make_color(GetRValue(rgb), GetGValue(rgb), GetBValue(rgb));
}


/* ------------------------------------------------------------------------ */
/*
 *   System Window Class Interface.  Each of our window objects must have
 *   one of these helper objects to provide the interface to its
 *   underlying system window class.
 *   
 *   The main reason we need this plug-in object is to allow mixing of our
 *   class hierarchy and the Windows class hierarchy (such as it is).
 *   This helper bridges the gap created by the way Windows exposes
 *   specific features of its different window classes directly in the
 *   API; for example, our code must call one of a number of default
 *   window procedures, depending on what type of system window we're
 *   creating (MDI child, MDI frame, normal window).  This helper
 *   encapsulates the system API specializations for each type of
 *   underlying system window, so that we can use any type of underlying
 *   system window with any class in our hierarchy.
 *   
 *   The base syswin class is appropriate for any generic system window
 *   (i.e., any window that uses DefWindowProc as its default window
 *   procedure).  Do not use this class for MDI Frame or Child windows.  
 */
class CTadsSyswin
{
public:
    CTadsSyswin(class CTadsWin *win) { win_ = win; }
    
    /* process creation of the window */
    virtual void syswin_do_create() {}

    /* process destruction of the window */
    virtual void syswin_do_destroy() {}

    /* destroy this window handle (use instead of DestroyWindow) */
    virtual void syswin_destroy_handle(HWND hwnd)
    {
        /* by default, use DestroyWindow */
        DestroyWindow(hwnd);
    }

    /* forget my window long for 'this' */
    virtual void forget_this_ptr(HWND hwnd)
    {
        /* forget my window pointer */
        SetWindowLong(hwnd, 0, 0L);
    }

    /* set the window's menu */
    virtual void syswin_set_win_menu(HMENU menu, int win_menu_index);

    /*
     *   Determine if we should always pass a message to the default
     *   handler.  Certain types of system window classes require that
     *   certain messages always be passed to their default handler, even
     *   when the message is also handled by the user code.  
     */
    virtual int syswin_always_pass_message(int msg) const { return FALSE; }

    /*
     *   Handle a WM_SIZE event.  Returns true if this routine has fully
     *   handled the event, false if not.  By default, we'll simply return
     *   false to let the Windows default message handler get the message. 
     */
    virtual int syswin_do_resize(int /*mode*/, int /*x*/, int /*y*/)
        { return FALSE; }

    /* pass a message to the default window procedure */
    virtual LRESULT syswin_call_defwinproc(HWND hwnd, UINT msg,
                                           WPARAM wpar, LPARAM lpar)
    {
        /* call the standard default window procedure */
        return DefWindowProc(hwnd, msg, wpar, lpar);
    }

    /*
     *   Get the handle of the window to use when creating children of
     *   this window.  For standard windows, this is simply the handle of
     *   this window; for MDI frames, this is the handle of the client
     *   rather than of this window.  
     */
    virtual HWND syswin_get_parent_of_children() const;

    /*
     *   Create the system window object 
     */
    virtual HWND syswin_create_system_window(DWORD ex_style,
                                             const textchar_t *wintitle,
                                             DWORD style,
                                             int x, int y, int wid, int ht,
                                             HWND parent, HMENU menu,
                                             HINSTANCE inst, void *param);

    /* determine if I'm maximized */
    virtual int syswin_is_maximized(class CTadsWin *parent) const;

protected:
    /* the associated window object */
    class CTadsWin *win_;
};


/* ------------------------------------------------------------------------ */
/*
 *   System Window Class Interface for MDI Frame windows 
 */
class CTadsSyswinMdiFrame: public CTadsSyswin
{
public:
    /*
     *   Create the MDI Frame interface object.  win_menu_idx is the index
     *   within the window's main menu of the "Window" menu; if this is
     *   less than zero, we'll take it as the offset from the *last* menu
     *   (-1 is the last menu, -2 is the second to last menu, etc).
     *   win_menu_cmd is the first command in a reserved range of commands
     *   for the child windows in the "Window" menu.
     *   
     *   If 'win_resizes_client_area' is true, we will refrain from
     *   resizing the client area when the window is resized; we will
     *   expect that the window itself handles this on resize.  
     */
    CTadsSyswinMdiFrame(class CTadsWin *win,
                        class CTadsWin *client_win,
                        int win_menu_idx, int win_menu_cmd,
                        int win_resizes_client_area)
        : CTadsSyswin(win)
    {
        /* save the MDI creation parameters */
        win_menu_idx_ = win_menu_idx;
        win_menu_cmd_ = win_menu_cmd;

        /* remember the window object for the client window */
        client_win_ = client_win;

        /* we haven't created the client's system window yet */
        client_handle_ = 0;

        /* note the window-resizes-client status */
        win_resizes_client_ = win_resizes_client_area;
    }
    
    /* get the MDI client window handle */
    HWND get_client_handle() const { return client_handle_; }

    /* process creation */
    void syswin_do_create();

    /* process destruction */
    void syswin_do_destroy();

    /* set the window's menu */
    virtual void syswin_set_win_menu(HMENU menu, int win_menu_index);

    /* handle a resize event */
    int syswin_do_resize(int mode, int x, int y);

    /*
     *   Create the system window object 
     */
    virtual HWND syswin_create_system_window(DWORD ex_style,
                                             const textchar_t *wintitle,
                                             DWORD style,
                                             int x, int y, int wid, int ht,
                                             HWND parent, HMENU menu,
                                             HINSTANCE inst, void *param);

    /*
     *   Determine if we should always pass a message to the default
     *   handler.  MDI Frame windows are required to pass certain messages
     *   to the default handler, even when they handle those messages
     *   themselves.  
     */
    virtual int syswin_always_pass_message(int msg) const
    {
        return (msg == WM_COMMAND || msg == WM_MENUCHAR
                || msg == WM_SETFOCUS || msg == WM_SIZE);
    }

    /* pass a message to the default window procedure */
    virtual LRESULT syswin_call_defwinproc(HWND hwnd, UINT msg,
                                           WPARAM wpar, LPARAM lpar)
    {
        /* call the MDI frame handler */
        return DefFrameProc(hwnd, client_handle_, msg, wpar, lpar);
    }

    /*
     *   Get the handle of the window to use when creating children of
     *   this window.  For MDI frames, this is the handle of the client
     *   rather than of the frame.  
     */
    virtual HWND syswin_get_parent_of_children() const
        { return client_handle_; }

protected:
    /* CTadsWin object for the client window, if present */
    class CTadsWin *client_win_;
    
    /* handle of the MDI client window */
    HWND client_handle_;

    /* index in the main menu bar of the "Window" menu */
    int win_menu_idx_;

    /* first command ID for child windows in the "Window" menu */
    int win_menu_cmd_;

    /* flag: our window handles client resizing */
    unsigned int win_resizes_client_ : 1;
};

/* ------------------------------------------------------------------------ */
/*
 *   System Window Class Interface for MDI Client windows 
 */
class CTadsSyswinMdiClient: public CTadsSyswin
{
public:
    /* 
     *   save the base class information for the internal Windows
     *   MDICLIENT class - this is called during class registration 
     */
    static void save_base_class_info(const WNDCLASS *wc)
    {
        /* remember the base class window procedure */
        base_proc_ = wc->lpfnWndProc;

        /* remember the size of the base class window extra data */
        base_wnd_extra_ = wc->cbWndExtra;
    }

    /* instantiate */
    CTadsSyswinMdiClient(class CTadsWin *win)
        : CTadsSyswin(win)
    {
    }

    /* forget my window long for 'this' */
    virtual void forget_this_ptr(HWND hwnd)
    {
        /* forget my window pointer */
        SetWindowLong(hwnd, base_wnd_extra_ + 0, 0L);
    }

    /* pass a message to the default window procedure */
    virtual LRESULT syswin_call_defwinproc(HWND hwnd, UINT msg,
                                           WPARAM wpar, LPARAM lpar)
    {
        /* call the base class procedure */
        return CallWindowProc((WNDPROC)base_proc_, hwnd, msg, wpar, lpar);
    }

    /* message handler ("superclass" window procedure) */
    static LRESULT CALLBACK message_handler(HWND hwnd, UINT msg,
                                            WPARAM wpar, LPARAM lpar);

    /* create the system window object */
    virtual HWND syswin_create_system_window(DWORD ex_style,
                                             const textchar_t *wintitle,
                                             DWORD style,
                                             int x, int y, int wid, int ht,
                                             HWND parent, HMENU menu,
                                             HINSTANCE inst, void *param);

    /* always pass certain messages to the internal MDI client window */
    virtual int syswin_always_pass_message(int msg) const
    {
        return (msg == WM_SETFOCUS || msg == WM_SIZE || msg == WM_MOVE);
    }

protected:
    /* base class (MDICLIENT) window procedure */
    static WNDPROC base_proc_;

    /* base class (MDICLIENT) window extra data size */
    static int base_wnd_extra_;
};


/* ------------------------------------------------------------------------ */
/*
 *   System Window Class Interface for MDI Child windows
 */
class CTadsSyswinMdiChild: public CTadsSyswin
{
public:
    CTadsSyswinMdiChild(class CTadsWin *win)
        : CTadsSyswin(win)
    {
    }

    /* destroy this window handle - process via the MDI client */
    virtual void syswin_destroy_handle(HWND hwnd)
    {
        /* ask the client window to destroy this child window */
        SendMessage(GetParent(hwnd), WM_MDIDESTROY, (WPARAM)hwnd, 0);
    }

    /* pass a message to the default window procedure */
    virtual LRESULT syswin_call_defwinproc(HWND hwnd, UINT msg,
                                           WPARAM wpar, LPARAM lpar)
    {
        /* call the MDI child handler */
        return DefMDIChildProc(hwnd, msg, wpar, lpar);
    }

    /* 
     *   MDI child windows must pass certain additional messages to the
     *   default handler 
     */
    virtual int syswin_always_pass_message(int msg) const
    {
        return (msg == WM_CHILDACTIVATE
                || msg == WM_GETMINMAXINFO
                || msg == WM_MENUCHAR
                || msg == WM_MOVE
                || msg == WM_SETFOCUS
                || msg == WM_SIZE
                || msg == WM_SYSCOMMAND);
    }

    /*
     *   Create the system window object 
     */
    virtual HWND syswin_create_system_window(DWORD ex_style,
                                             const textchar_t *wintitle,
                                             DWORD style,
                                             int x, int y, int wid, int ht,
                                             HWND parent, HMENU menu,
                                             HINSTANCE inst, void *param);

    /* determine if I'm maximized */
    virtual int syswin_is_maximized(class CTadsWin *parent) const;
};


/* ------------------------------------------------------------------------ */
/*
 *   Basic window class.  This class can be used as a base class for most
 *   types of Win32 windows. 
 */

/* maximum number of toolbars that can be registered for status updating */
const int TADSWIN_TBMAX = 5;

/* button click record */
struct BtnClick_t
{
    BtnClick_t() { time_ = 0; x_ = y_ = 0; cnt_ = 0; }
    
    /*
     *   Check a click to see if it's a multiple click.  We'll return the
     *   click count, and remember the statistics for the next click.
     *   'lpar' is the LPARAM value from the WM_xBUTTONDOWN message.  
     */
    int get_click_count(LPARAM lpar);
    
    long time_;                                   /* time of the last click */
    int x_, y_;                               /* position of the last click */
    int cnt_;                             /* number of clicks of last click */
};

class CTadsWin: public IDropSource, public IDropTarget
{
public:
    /* static class initialization/termination */
    static void class_init(class CTadsApp *app);
    static void class_terminate();

    /* creation/deletion */
    CTadsWin();
    virtual ~CTadsWin();

    /*
     *   Destroy the window.  Use this method instead of calling the
     *   DestroyWindow() function in the Windows API - this will use the
     *   proper destruction mechanism (for example, MDI child windows need
     *   special handling). 
     */
    void destroy_handle() { sysifc_->syswin_destroy_handle(handle_); }

    /* get my parent window */
    CTadsWin *get_parent() const { return parent_; }

    /* 
     *   set my parent window - this only sets our internal parent, not
     *   the Windows parent handle; a caller that reparents this window
     *   must take care of reparenting the Windows handle separately 
     */
    void set_parent(CTadsWin *parent) { parent_ = parent; }

    /* 
     *   Create the operating system window object.  Returns zero on
     *   success, non-zero on failure.  This routine uses the default
     *   system window interface object, so this call can only be used to
     *   create standard (non-MDI) windows.  
     */
    virtual int create_system_window(CTadsWin *parent, int show,
                                     const char *title, const RECT *pos)
    {
        return create_system_window(parent, show, title, pos,
                                    new CTadsSyswin(this));
    }

    /*
     *   Create the OS window object, using a particular system window
     *   interface.  Returns zero on success, non-zero on failure.
     *   
     *   The window takes ownership of the system interface object; the
     *   window will automatically delete the system interface object when
     *   the window is deleted.  Since the window owns the system
     *   interface object, each window must have its own unique instance
     *   of a system interface object -- windows cannot share these
     *   objects.  
     */
    virtual int create_system_window(CTadsWin *parent, int show,
                                     const char *title, const RECT *pos,
                                     CTadsSyswin *sysifc);

    /* create a system window given an explicit parent handle */
    int create_system_window(CTadsWin *parent, HWND parent_hwnd,
                             int show, const char *title,
                             const RECT *pos, CTadsSyswin *sysifc);

    /*
     *   Register window classes.  This routine must be called during
     *   program initialization so that we can identify our window class
     *   to the system when creating new instances of the window.  
     */
    static void register_win_class(class CTadsApp *app);

    /* get the window handle */
    HWND get_handle() { return handle_; }

    /* determine if I'm maximized */
    int is_win_maximized() const;

    /*
     *   Get the window handle to use as the parent handle when creating
     *   children of this window.  For most types of system windows, this
     *   will simply return my own handle; certain system window types,
     *   such as MDI Frame windows, will return a child handle.  
     */
    HWND get_parent_of_children() const
    { return sysifc_->syswin_get_parent_of_children(); }

    /*
     *   Set the window's menu.  Clients should use this rather than
     *   calling SetMenu directly.  If the menu has a "Window" menu (which
     *   is generally only applicable to MDI frame windows),
     *   win_menu_index is a non-zero value giving the index within the
     *   menu bar of the window menu: a positive value indicates an offset
     *   from the first menu (1 = first menu, 2 = second menu), and a
     *   negative value indicates an offset from the last menu (-1 = last
     *   [rightmost] menu, -2 = next to last menu).  0 indicates that
     *   there is no "Window" menu.  
     */
    void set_win_menu(HMENU menu, int win_menu_index)
        { sysifc_->syswin_set_win_menu(menu, win_menu_index); }

    /* invalidate the window, or a portion thereof */
    void inval();
    void inval(const RECT *area);

    /* determine the number of bits per pixel on the display */
    int get_bits_per_pixel()
    {
        /* simply return the bits per pixel of the desktop window */
        return get_desk_bits_per_pixel();
    }

    /* get the bits per pixel of the desktop window */
    static int get_desk_bits_per_pixel();

    /* Get our device context.  This is only valid during drawing. */
    HDC gethdc() { return hdc_; }

    /* 
     *   Get our display device context.  When we're drawing into a memory
     *   DC, we'll still keep track of the actual display device context;
     *   this is necessary on palette devices so that the palette can be
     *   syncrhonized between the memory DC and the device DC. 
     */
    HDC get_display_hdc() { return display_hdc_; }


    /* 
     *   Process a palette instantiation request.  Windows sends this
     *   message to a top-level window when it's about to become active,
     *   so that it can realize its palette as a foreground window.  By
     *   default, we do nothing; subclasses should override this to
     *   realize their palettes as appropriate.  Windows which contain
     *   subwindows which have palettes should generally call this method
     *   on their subwindows.  This routine should return true if the
     *   window realizes a new palette.  
     */
    virtual int do_querynewpalette() { return FALSE; }

    /*
     *   Process a palette change notification.  Windows sends this
     *   message to all top-level windows when the system palette changes.
     *   This routine should update this window, if needed, to adjust for
     *   the new palette.  Note that, by default, we'll only call this
     *   method on top-level windows; windows that contain subwindows with
     *   palettes should generally call this method on their subwindows.
     *   By default, this routine does nothing; subclasses should override
     *   it as needed to update for a palette change.
     *   
     *   Note that this routine should return without doing any work if
     *   called on the initiating window, since an infinite loop would
     *   result if the initating window responded to this message with
     *   another palette change.  
     */
    virtual void do_palettechanged(HWND /*initiating_window*/) { }
    
    /* 
     *   process a notification message; the return value is returned
     *   directly from the window procedure 
     */
    virtual int do_notify(int control_id, int notify_code, LPNMHDR nm);

    /* process a context-menu message */
    virtual int do_context_menu(HWND /*hwnd*/, int /*x*/, int /*y*/)
        { return FALSE; }

    /* process a command */
    virtual int do_command(int /*notify_code*/, int /*command_id*/,
                           HWND /*ctl*/)
        { return FALSE; }

    /* check the status of a command */
    virtual TadsCmdStat_t check_command(int /*command_id*/)
        { return TADSCMD_UNKNOWN; }

    /* 
     *   Update toolbar button states with the current command status.  We do
     *   this automatically on a timer that runs every half second by
     *   default, but this can be called explicitly if desired to update the
     *   toolbar status immediately at certain times.  By default, we call
     *   this right after calling do_command(), if do_command() did anything,
     *   to ensure that the toolbar is updated quickly after a user action
     *   that might have triggered a change. 
     */
    void update_toolbar_buttons();

    /* handle a mouse-wheel scroll event */
    virtual int do_mousewheel(int keys, int dist, int x, int y)
    {
        /* 
         *   if we're controlling another winodw, pass the message to that
         *   window; otherwise, do nothing, since the basic window doesn't
         *   have any scrolling capabilities defined
         */
        if (scroll_win_ != 0)
            return scroll_win_->do_mousewheel(keys, dist, x, y);
        else
            return FALSE;
    }

    /*
     *   Handle scrollbar events - if vert is true, it's a vertical
     *   scrollbar, otherwise it's a horizontal scrollbar.  By default, we
     *   ignore this method.  
     */
    virtual void do_scroll(int vert, HWND sb_handle,
                           int scroll_code, long pos, int use_pos)
    {
        /* 
         *   If we're controlling scrolling in another window, pass the
         *   message to that window; otherwise, do nothing, since the
         *   basic window doesn't have any ability to scroll.  
         */
        if (scroll_win_ != 0)
        {
            /* there's an external scrolling window - let it handle it */
            scroll_win_->do_scroll(vert, sb_handle,
                                   scroll_code, pos, use_pos);
        }
    }

    /* 
     *   Set the external scrolling window.  We will control scrolling in
     *   this window via child scrollbar controls contained in this
     *   window.  (Normally, a window contains its scrollbars as its own
     *   child controls; in some cases, however, it may be desirable for
     *   one window to contain scrollbars for another window, in order to
     *   achieve special layout effects.  For example, a parent window may
     *   want to contain the scrollbars for one of its children, so that
     *   the scrollbars can be positioned specially for other controls
     *   contained in the border around the child window.)
     */
    void set_scroll_win(CTadsWin *scroll_win)
    {
        CTadsWin *old_scroll_win;

        /* remember the old scrolling window for a moment */
        old_scroll_win = scroll_win_;
        
        /* set the new scrolling window */
        scroll_win_ = scroll_win;

        /* 
         *   Add a reference to the new window, and remove a reference
         *   from the old one.  We keep a reference on the window so that
         *   we can be sure it doesn't get deleted as long as we have a
         *   reference to it.  
         */
        if (scroll_win_ != 0)
            scroll_win_->AddRef();
        if (old_scroll_win != 0)
            old_scroll_win->Release();
    }

    /*
     *   Draw a bitmap, given a bitmap handle 
     */
    void draw_hbitmap(HBITMAP bmp, const RECT *dstrc,
                      const RECT *srcrc);

    /*
     *   Add a toolbar idle status processor.  We'll set up a timer
     *   routine that will scan each toolbar periodically and update the
     *   status of each button to correspond to the current status for its
     *   associated command.  Up to TADSWIN_TBMAX toolbar status
     *   processors can be set.  
     */
    void add_toolbar_proc(HWND toolbar);

    /* 
     *   Remove a toolbar status processor.  If a toolbar is registered
     *   with add_toolbar_proc(), it must be deregistered here before the
     *   toolbar is destroyed. 
     */
    void rem_toolbar_proc(HWND toolbar);

    /*
     *   Process a private activation message from the parent window.
     *   Certain types of container windows might want to notify child
     *   windows of "artificial" activation (in other words, activation
     *   that is not considered by Windows to be true activation, and thus
     *   doesn't result in a WM_ACTIVATE, WM_NCACTIVATE, WM_MDIACTIVATE,
     *   or any other similar message).  For example, the docking window
     *   container can use this.  
     */
    virtual void do_parent_activate(int /*active*/) { }

    /*
     *   IDropSource implementation 
     */
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ifc);
    ULONG STDMETHODCALLTYPE AddRef() { return ++ole_refcnt_; }
    ULONG STDMETHODCALLTYPE Release();
    HRESULT STDMETHODCALLTYPE
        QueryContinueDrag(BOOL esc_pressed, DWORD key_state);
    HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD effect);

    /*
     *   IDropTarget implementation (shares IUnknown with IDropSource) 
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

    /* names of window classes registered with the system */
    static const char win_class_name[];
    static const char mdichild_win_class_name[];
    static const char mdiframe_win_class_name[];
    static const char mdiclient_win_class_name[];

    /* common message handler for all window types */
    LRESULT CALLBACK common_msg_handler(HWND hwnd, UINT msg,
                                        WPARAM wpar, LPARAM lpar);

protected:
    /*
     *   Allocate/free a timer.  Whenever an application wants to set a
     *   system timer with Windows, it must use a unique identifier for
     *   the timer.  alloc_timer_id() allocates timer identifier values;
     *   it returns zero if no more timer ID's are available.  When the
     *   timer is no longer needed, the identifier should be freed with
     *   free_timer_id().  These routines are static because timer ID's
     *   are global to the application.  
     */
    static int alloc_timer_id();
    static void free_timer_id(int id);
    
    /* track a popup context menu */
    void track_context_menu(HMENU menuhdl, int x, int y)
    {
        /* track the menu, using the standard top-left alignment flags */
        track_context_menu_ext(menuhdl, x, y, TPM_TOPALIGN | TPM_LEFTALIGN);
    }

    /* track a popup menu, specifying full flags */
    int track_context_menu_ext(HMENU menuhdl, int x, int y, DWORD flags);

    /*
     *   Begin/end tracking a popup menu.  While we're tracking a popup
     *   menu, we'll ignore WM_SETCURSOR messages, and simply set the
     *   cursor to an arrow.  Note that a caller that uses
     *   track_context_menu() to run a popup menu doesn't need to call
     *   these routines; these only need to be used if the caller shows a
     *   popup menu directly through the Windows API.  
     */
    void begin_tracking_popup_menu() { tracking_popup_menu_ = TRUE; }
    void end_tracking_popup_menu() { tracking_popup_menu_ = FALSE; }

    /*
     *   Initialize a submenu so that all of the items in the submenu use
     *   radio button checkmarks rather than ordinary checkmarks.  This
     *   should be used to initialize a menu that displays mutually
     *   exclusive choices. 
     */
    void set_menu_radiocheck(HMENU submenu);

    /* 
     *   set the radio button checkmark style on a range of menu items,
     *   inclusive of the first and last index 
     */
    void set_menu_radiocheck(HMENU submenu, int first, int last);

    /* determine whether the shift key is down */
    static int get_shift_key()
        { return (GetKeyState(VK_SHIFT) & 0x8000) != 0; }

    /* determine whether the control key is down */
    static int get_ctl_key()
        { return (GetKeyState(VK_CONTROL) & 0x8000) != 0; }

    /* determine whether the mouse buttons are down */
    static int get_lbtn_key()
        { return (GetKeyState(VK_LBUTTON) & 0x8000) != 0; }
    static int get_mbtn_key()
        { return (GetKeyState(VK_MBUTTON) & 0x8000) != 0; }
    static int get_rbtn_key()
        { return (GetKeyState(VK_RBUTTON) & 0x8000) != 0; }

    /* 
     *   synthesize a set of MK_xxx flags for the current key state - this
     *   provides flags equivalent to the values that WM_LBUTTONDOWN
     *   passes in its WPARAM parameter 
     */
    static int get_key_mk_state()
    {
        return (get_shift_key() ? MK_SHIFT : 0)
            + (get_ctl_key() ? MK_CONTROL : 0)
            + (get_lbtn_key() ? MK_LBUTTON : 0)
            + (get_rbtn_key() ? MK_RBUTTON : 0)
            + (get_mbtn_key() ? MK_MBUTTON : 0);
    }

    /* message handler for standard (non-MDI) windows */
    static LRESULT CALLBACK std_message_handler(HWND hwnd, UINT msg,
                                                WPARAM wpar, LPARAM lpar);

    /* message handler for MDI frame windows */
    static LRESULT CALLBACK mdiframe_message_handler(
        HWND hwnd, UINT msg, WPARAM wpar, LPARAM lpar);

    /* message handler for MDI child windows */
    static LRESULT CALLBACK mdichild_message_handler(
        HWND hwnd, UINT msg, WPARAM wpar, LPARAM lpar);
    
    /* paint the window */
    virtual int do_paint();

    /*
     *   Get the window style flags, and the extended style flags (for
     *   window creation).  The default flags are suitable for a main
     *   window with a document border, resizability, a caption, and a
     *   system menu. 
     */
    virtual DWORD get_winstyle()
        { return (WS_OVERLAPPEDWINDOW | WS_BORDER
                  | WS_CAPTION | WS_SYSMENU
                  | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
                  | WS_SIZEBOX); }

    virtual DWORD get_winstyle_ex() { return WS_EX_WINDOWEDGE; }

    /*
     *   Paint the content area.  Most windows will not need to override
     *   do_paint(), which does the standard painting setup, but will
     *   instead override only this routine, which does the actual
     *   drawing.  The default routine simply erases the area with a white
     *   background. 
     */
    virtual void do_paint_content(HDC hdc, const RECT *area_to_draw);

    /* paint the window when it's minimized */
    virtual int do_paint_iconic();

    /* erase the background */
    virtual int do_erase_bkg(HDC) { return FALSE; }

    /*
     *   Process mouse events.  These routines should return TRUE if the
     *   event was handled.  By default, these all simply return FALSE to
     *   indicate that the default Windows handling should apply.  
     */
    virtual int do_leftbtn_down(int /*keys*/, int /*x*/, int /*y*/,
                                int /*clicks*/)
        { return FALSE; }
    virtual int do_leftbtn_up(int keys, int x, int y);
    virtual int do_rightbtn_down(int /*keys*/, int /*x*/, int /*y*/,
                                 int /*clicks*/)
        { return FALSE; }
    virtual int do_rightbtn_up(int /*keys*/, int /*x*/, int /*y*/)
        { return FALSE; }
    virtual int do_mousemove(int keys, int x, int y);

    /* receive notification that we've lost capture */
    virtual int do_capture_changed(HWND new_capture_win);

    /*
     *   Process mouse events in the non-client area.  These routines
     *   return TRUE if the event was handled, false if not.  By default,
     *   these all simply return FALSE to indicate that the default
     *   Windows handling should apply.  
     */
    virtual int do_nc_leftbtn_down(int /*keys*/, int /*x*/, int /*y*/,
                                   int /*clicks*/, int /*hit_type*/)
        { return FALSE; }
    virtual int do_nc_leftbtn_up(int /*keys*/, int /*x*/, int /*y*/,
                                 int /*hit_type*/)
        { return FALSE; }
    virtual int do_nc_rightbtn_down(int /*keys*/, int /*x*/, int /*y*/,
                                    int /*clicks*/, int /*hit_type*/)
        { return FALSE; }
    virtual int do_nc_rightbtn_up(int /*keys*/, int /*x*/, int /*y*/,
                                  int /*hit_type*/)
        { return FALSE; }
    virtual int do_nc_mousemove(int /*keys*/, int /*x*/, int /*y*/,
                                int /*hit_type*/)
        { return FALSE; }

    /*
     *   Hit-test a mouse position in the non-client area.  Fill in
     *   (*hit_type) and return true if processed; return false to defer
     *   to the Windows default behavior.  
     */
    virtual int do_nc_hit_test(int /*x*/, int /*y*/, int * /*hit_type*/)
        { return FALSE; }


    /* 
     *   Process a user message - any message in the range WM_USER to 0x7fff
     *   (window-class-private messages internal to the application) or in
     *   the range 0xC000 to 0xFFFF (registered global messages, for
     *   inter-process communication) is passed to this routine.  We don't do
     *   anything by default.  
     */
    virtual int do_user_message(int /*msg*/, WPARAM, LPARAM)
        { return FALSE; }

    /*
     *   Handle a MIDI event.  By default, we'll coordinate with the basic
     *   MIDI file player class to handle the message; most subclasses
     *   shouldn't need to do anything with this. 
     */
    virtual int do_midi_event(UINT msg, WPARAM wpar, LPARAM lpar);

    /*
     *   Process a set-cursor event.  Returns true if the message was
     *   handled.  
     */
    virtual int do_setcursor(HWND, int /*hittest*/, int /*mousemsg*/)
        { return FALSE; }

    /*
     *   Handle WM_WINDOWPOSCHANGING and WM_WINDOWPOSCHANGED messages.
     *   Returns true if the message is handled, false if not.  
     */
    virtual int do_windowposchanging(WINDOWPOS *wp) { return FALSE; }
    virtual int do_windowposchanged(WINDOWPOS *wp) { return FALSE; }

    /* move the window */
    virtual void do_move(int x, int y);

    /* resize */
    virtual void do_resize(int mode, int x, int y);

    /* 
     *   synthesize a resize event - this can be used when some internal
     *   layout has changed and we want to recalculate positions using the
     *   normal resize code 
     */
    void synth_resize();

    /* enter/exit size/move operation */
    virtual void do_entersizemove() { }
    virtual void do_exitsizemove() { }

    /*
     *   Handle a WM_SHOWWINDOW event.  'show' is true if the window is
     *   being shown, false otherwise.  'status' is the function code:
     *   zero indicates that this is a simple ShowWindow() call; SW_xxx
     *   values (see the Win32 documentation for WM_SHOWWINDOW) indicate
     *   other causes.  Returns true if the message is handled, false if
     *   the default window procedure should be invoked.  
     */
    virtual int do_showwindow(int /*show*/, int /*status*/) { return FALSE; }

    /*
     *   Process a WM_COLORxxx message.  Returns a brush, if desired,
     *   which is used to paint the background of the control.  Returns
     *   null by default to paint with the default background color. 
     */
    virtual HBRUSH do_ctlcolor(UINT /*msg*/, HDC, HWND) { return 0; }

    /* process window creation message */
    virtual void do_create();

    /* 
     *   Process a close-window message.  Return true if closing should
     *   proceed, false if not. 
     */
    virtual int do_close();

    /* process a destroy window message */
    virtual void do_destroy();

    /*
     *   Process application activation/deactivation.  Returns true to
     *   suppress default handling by Windows, false to continue with system
     *   default processing.  We do nothing by default.  
     */
    virtual int do_activate_app(int /*flag*/, DWORD /*thread_id*/)
        { return FALSE; }

    /* 
     *   Process an activation/deactivation message.  Return true to
     *   suppress the default handling by Windows, false to continue with
     *   Windows default processing.  
     */
    virtual int do_activate(int flag, int minimized, HWND other_win);

    /* process non-client activation */
    virtual int do_ncactivate(int flag) { return FALSE; }

    /*
     *   Process an MDI activation/deactivation message; this message is
     *   received by MDI child windows.  By default, we'll call the normal
     *   do_activate routine so that processing in MDI vs. SDI mode can be
     *   handled by the same code; in situations where MDI and SDI
     *   activation need different behavior, this routine should be
     *   overridden.  Returns true if the activation was handled, false to
     *   continue with Windows default processing.  
     */
    virtual int do_mdiactivate(HWND old_active, HWND new_active)
    {
        if (old_active == handle_)
            return do_activate(WA_INACTIVE, FALSE, new_active);
        else if (new_active == handle_)
            return do_activate(WA_ACTIVE, FALSE, old_active);
        else
            return FALSE;
    }

    /*
     *   Process an MDI child activation message.  This message is
     *   received by MDI child windows when the user clicks on the
     *   window's title bar or moves or resizes it. 
     */
    virtual int do_childactivate() { return FALSE; }

    /*
     *   Process an MDI menu refresh message.  This message is passed to
     *   an MDI client window to refresh its "Window" menu.  Return true
     *   if handled, false if the message should be passed to the default
     *   window procedure. 
     */
    virtual int do_mdirefreshmenu() { return FALSE; }

    /* handle timer messages */
    virtual int do_timer(int timer_id);

    /* pass a message to the default window procedure */
    virtual LRESULT call_defwinproc(HWND hwnd, UINT msg,
                                    WPARAM wpar, LPARAM lpar)
    {
        /* let the system interface object handle it */
        return sysifc_->syswin_call_defwinproc(hwnd, msg, wpar, lpar);
    }

    /*
     *   Test whether we should always pass a given message to the default
     *   window procedure.  Certain window types -- in particular, MDI
     *   frame windows -- must always pass certain messages to default
     *   window procedure, whether or not they handle the message
     *   separately.  By default, this returns false, since we normally
     *   use the result of the virtual associated with the message to
     *   determine if we should pass the message to the default handler or
     *   not.  
     */
    int always_pass_message(int msg) const
        { return sysifc_->syswin_always_pass_message(msg); }

    /*
     *   Process keystrokes.  do_char() processes an ASCII character
     *   keystroke.  do_keydown() processes a raw virtual key event.
     *   do_syschar() processes a keystroke that had the ALT key down.
     *   do_hotkey() handles a registered hot-key message.  All of these
     *   should return true if they handled the event, false if not.  
     */
    virtual int do_char(TCHAR, long /*keydata*/) { return FALSE; }
    virtual int do_keydown(int /*virtual_key*/, long /*keydata*/)
        { return FALSE; }
    virtual int do_keyup(int /*virtual_key*/, long /*keydata*/)
        { return FALSE; }
    virtual int do_syskeydown(int /*virtual_key*/, long /*keydata*/)
        { return FALSE; }
    virtual int do_syschar(TCHAR, long /*keydata*/) { return FALSE; }
    virtual int do_hotkey(int /*id*/, unsigned int /*modifiers*/,
                          unsigned int /*vkey*/)
        { return FALSE; }

    /*
     *   Gain/lose focus
     */
    virtual void do_setfocus(HWND /*previous_focus*/)
    {
        /* notify my parent */
        notify_parent_focus(TRUE);
    }
    virtual void do_killfocus(HWND /*previous_focus*/)
    {
        /* notify my parent */
        notify_parent_focus(FALSE);
    }

    /*
     *   Notify the parent window of a focus change affecting this window.
     *   The do_setfocus() and do_killfocus() routines can use this method
     *   if parent notification is desired.  We notify the parent via a
     *   WM_NOTIFY message. 
     */
    void notify_parent_focus(int setting)
    {
        NMHDR nm;

        /* if there's no parent, there's no one to notify */
        if (GetParent(handle_) == 0)
            return;

        /* set up the NMHDR structure */
        nm.hwndFrom = handle_;
        nm.idFrom = 0;
        nm.code = (setting ? NM_SETFOCUS : NM_KILLFOCUS);

        /* send the message */
        SendMessage(GetParent(handle_), WM_NOTIFY, 0, (LPARAM)&nm);
    }

    /* 
     *   Process a WM_MOUSEACTIVATE message.  If this routine returns
     *   false, we'll call the default window procedure as usual.  If this
     *   returns true, we'll return the result value stored in '*result'. 
     */
    virtual int do_mouseactivate(HWND /*top_level_parent*/,
                                 int /*nc_hit_test*/,
                                 unsigned int /*mouse_message*/,
                                 LRESULT * /*result*/)
    {
        /* use the default window procedure's processing */
        return FALSE;
    }

    /* handle a system setting change notification */
    virtual int do_sys_setting_change(WPARAM /*system_param*/,
                                      const char * /*param_area*/)
    {
        return FALSE;
    }

    /* handle a system color change notification */
    virtual int do_sys_color_change()
    {
        /* not handled - invoke default window procedure */
        return FALSE;
    }

    /* receive notification that we've selected a menu item */
    virtual int menu_item_select(unsigned int /*item*/,
                                 unsigned int /*flags*/, HMENU /*menu*/)
        { return FALSE; }

    /* receive notification that we've closed a menu */
    virtual int menu_close(unsigned int /*item*/) { return FALSE; }

    /* initialize a popup menu that's about to be opened */
    virtual void init_menu_popup(HMENU menuhdl, unsigned int pos,
                                 int sysmenu);

    /*
     *   Register this window as an OLE drop target.  We won't do this by
     *   default, but a subclass can use this routine if it wants to be an
     *   OLE drop target.  If this routine is called, we'll automatically
     *   call drop_target_unregister before destroying the window. 
     */
    void drop_target_register();

    /*
     *   Unregister the window as an OLE drop target.  We'll automatically
     *   call this before destroying the window if the window was
     *   registered as a drop target, but a subclass can explicitly
     *   unregister with OLE at any time by calling this routine. 
     */
    void drop_target_unregister();

    /*
     *   Prepare to begin dragging out of this window.  This can be called
     *   in a mouse button down or mouse move routine to set up for
     *   dragging.  This doesn't actually initiate dragging, but records
     *   the current cursor position so that subsequent calls to
     *   drag_check() can determine if and when to start dragging.
     *   
     *   'already_have_capture' should be set to true if the window has
     *   already captured the mouse; if this is false, we'll capture the
     *   mouse ourselves.  
     */
    void drag_prepare(int already_have_capture);

    /*
     *   Check dragging.  After drag_prepare() has been called, this
     *   routine should be called whenever the mouse moves and the button
     *   is still down.  If dragging hasn't yet begun, but the mouse has
     *   now moved enough for dragging to begin, we'll begin dragging; if
     *   we're currently doing dragging, we'll continue it here.  Returns
     *   true if drag-and-drop occurred, false if not.  
     */
    int drag_check();

    /*
     *   End dragging.  If drag_prepare was called and dragging never
     *   occurred (i.e., drag_check never returned true), this must be
     *   called when the mouse is released or capture is forced away from
     *   this window.  It's harmless to call this if drag_check did return
     *   true.
     *   
     *   'already_released_capture' should be set to true if this routine
     *   was called as a result of notification of capture loss.  
     */
    void drag_end(int already_released_capture);


    /*
     *   Drag source support - get the IDataObject to be dragged.  We'll
     *   call this when we've decided we want to start a drag operation
     *   out of this window.  This must be overridden by windows that act
     *   as drag sources to provide a valid IDataObject implementation.
     *   
     *   This routine must add a reference to the data object on our
     *   behalf.  We will release our reference on the data object after
     *   the drag/drop operation is completed.  
     */
    virtual IDataObject *get_drag_dataobj() { return 0; }

    /*
     *   Get the valid drop effects.  We'll call this when we've decided
     *   we want to start a drag operation out of this window.  This can
     *   be overridden by windows that act as drag sources to customize
     *   the available operations; by default, we'll allow all of the drag
     *   effects. 
     */
    virtual ULONG get_drag_effects()
    {
        return (DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK
                | DROPEFFECT_SCROLL);
    }

    /*
     *   Receive notification that dragging is about to begin, and that it
     *   just ended.  drag_check() calls drag_pre when it's about to begin
     *   dragging, and drag_post when dragging is done.  We don't do
     *   anything by default, but subclasses can do any special processing
     *   to prepare for or clean up after dragging here.  
     */
    virtual void drag_pre() { }
    virtual void drag_post() { }

protected:
    /* system window interface object */
    CTadsSyswin *sysifc_;
    
    /* our handle */
    HWND handle_;

    /* our device context handle, valid during painting */
    HDC hdc_;

    /* 
     *   Our display device context handle.  When we're drawing directly
     *   onto the device, this will be the same as hdc_; when we're
     *   drawing into a memory DC, this will be the underlying display
     *   device context. 
     */
    HDC display_hdc_;

    /* parent window */
    CTadsWin *parent_;

    /*
     *   last click times and positions for the mouse buttons, for
     *   calculating double/triple/etc clicks 
     */
    BtnClick_t lbtn_click;
    BtnClick_t mbtn_click;
    BtnClick_t rbtn_click;

    /* basic cursor objects */
    HCURSOR arrow_cursor_;
    HCURSOR wait_cursor_;

    /* tracking popup */
    int tracking_popup_menu_ : 1;

    /* timer being used for drag-scroll tracking */
    int drag_scroll_timer_id_;

    /* 
     *   Timer ID list - this is a set of bits indicating whether timers
     *   have been allocated.  We allocate 32 bits for 32 timers ID's.
     */
    static unsigned long timers_alloced_;

    /* list of toolbars for which we're processing status events */
    HWND toolbars_[TADSWIN_TBMAX];
    int toolbar_cnt_;

    /* timer ID for the toolbar status processor */
    int tb_timer_id_;

    /* 
     *   external scrolling window - this is the window whose scrolling
     *   position we control through our child scrollbar controls 
     */
    CTadsWin *scroll_win_;

    /* 
     *   Accumulated scroll increment.  When we get WM_MOUSEWHEEL events
     *   smaller than WHEEL_DELTA, we accumulate the partial scrolls here
     *   until we have enough to scroll by one increment.  
     */
    int wheel_accum_;

    /* 
     *   starting cursor position of current drag operation; if
     *   drag_ready_ is true, we're watching for movement from this
     *   position, and will start a drag if the mouse moves far enough
     *   with the button down 
     */
    POINT drag_start_pos_;
    int drag_ready_ : 1;

    /* flag indicating that we captured the mouse for a drag operation */
    int drag_capture_ : 1;

    /* original keyboard state at start of drag operation */
    ULONG drag_start_key_;

    /* OLE IUnknown reference count */
    ULONG ole_refcnt_;

    /* flag indicating that we're registered with OLE as a drop target */
    int drop_target_regd_ : 1;

    /* 
     *   Flag: the window is maximized.  This flag provides a mechanism
     *   for keeping track of our maximized state independently of the
     *   normal Windows GetWindowPlacement() mechanism.  In some cases,
     *   GetWindowPlacement() won't give us useful information; for
     *   example, if we're an MDI child window, we need to find our
     *   maximized status through other means.
     */
    int maximized_ : 1;

    /* high water mark for allocated timers */
    static int next_timer_id_;

    /* free timer list */
    static struct tadswin_timer_alo *free_timers_;

    /* in-use timer list */
    static struct tadswin_timer_alo *inuse_timers_;

    /*
     *   Size of the MENUITEMINFO structure, for use in GetMenuItemInfo and
     *   the like.  On win95 and NT4, the API's are intolerant of the larger
     *   structure size defined in the newer headers from Microsoft, so we
     *   must avoid using the true structure size and hard-code the old
     *   structure size instead.  This is an OS bug, but what can you do...  
     */
    static size_t menuiteminfo_size_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Window with scrollbars 
 */

/* time (milliseconds) to wait between drag scrolling */
const long TADSWIN_DRAG_SCROLL_WAIT = 50;


class CTadsWinScroll: public CTadsWin
{
public:
    /* static class initialization/termination */
    static void class_init(class CTadsApp *app);
    static void class_terminate();

    /* construction/destruction */
    CTadsWinScroll(int has_vscroll, int has_hscroll);
    ~CTadsWinScroll();

    /* 
     *   Set sizebox style on or off.  This can be called at any time; we'll
     *   adjust our style accordingly.  
     */
    void set_has_sizebox(int f);

    /*
     *   Adjust coordinates from document coordinates to client window
     *   coordinates, by adjusting for the scrolling offset.  
     */
    long doc_to_screen_x(long x) const
        { return x - hscroll_ofs_ * get_hscroll_units(); }
    long doc_to_screen_y(long y) const
        { return y - vscroll_ofs_ * get_vscroll_units(); }
    CHtmlPoint doc_to_screen(CHtmlPoint pt) const
    {
        pt.x = doc_to_screen_x(pt.x); pt.y = doc_to_screen_y(pt.y);
        return pt;
    }
    CHtmlRect doc_to_screen(CHtmlRect r) const
    {
        r.offset(-hscroll_ofs_ * get_hscroll_units(),
                 -vscroll_ofs_ * get_vscroll_units());
        return r;
    }

    /*
     *   Adjust coordinates from client window coordinates to document
     *   coordinates 
     */
    long screen_to_doc_x(long x)
        { return x + hscroll_ofs_ * get_hscroll_units(); }
    long screen_to_doc_y(long y)
        { return y + vscroll_ofs_ * get_vscroll_units(); }
    CHtmlPoint screen_to_doc(CHtmlPoint pt)
        { pt.x = screen_to_doc_x(pt.x); pt.y = screen_to_doc_y(pt.y);
    return pt; }
    CHtmlRect screen_to_doc(CHtmlRect r)
        { r.offset(hscroll_ofs_ * get_hscroll_units(),
                   vscroll_ofs_ * get_vscroll_units());
          return r; }

    /* get the current scrollbar positions */
    long get_vscroll_pos() const { return vscroll_ofs_; }
    long get_hscroll_pos() const { return hscroll_ofs_; }

    /* handle a mouse-wheel scroll event */
    virtual int do_mousewheel(int keys, int dist, int x, int y);

    /*
     *   Do scrolling.  If use_pos is true, we'll ignore the scrollbar's
     *   internal control settings and use the pos value; otherwise, we'll
     *   use the scrollbar's settings.  When called directly (rather than
     *   in response to an event) to set the position, set use_pos to
     *   true; we normally set use_pos to false when handling an event,
     *   since we get better precision from the scrollbar internal control
     *   data than we can from the windows message.  
     */
    virtual void do_scroll(int vert, HWND sb_handle,
                           int scroll_code, long pos, int use_pos);

    /* get the default scrollbar handles */
    HWND get_vscroll_handle() const { return vscroll_; }
    HWND get_hscroll_handle() const { return hscroll_; }

    /*
     *   Set external scrollbars.  If the parent window (or another
     *   window) contains our scrollbar or scrollbars, it can tell us
     *   about the scrollar or scrollbars with this method.
     *   
     *   If ext_vscroll is null, the current scrollbar (internal or
     *   external) will not be affected.  Likewise ext_hscroll.
     *   
     *   The external window is responsible for reflecting WM_xSCROLL
     *   messages from the external scrollbars to our do_scroll() method.
     *   See CTadsWin::set_scroll_win().  The external window is also
     *   responsible for managing the location and size of the external
     *   scrollbars, since they belong to the external window and not to
     *   us.  We will, however, keep the scrollbar ranges adjusted, since
     *   we own the content that the scrollbars control.  
     */
    void set_ext_scrollbar(HWND ext_vscroll, HWND ext_hscroll);

    /* process MDI child activation */
    virtual int do_childactivate()
    {
        /* fix up scrollbars, in case we resized */
        adjust_scrollbar_positions();

        /* inherit default */
        return CTadsWin::do_childactivate();
    }

protected:
    /* process window creation message */
    void do_create();

    /* initialize the sizebox */
    void init_sizebox();

    /* resize the window */
    void do_resize(int mode, int x, int y);

    /* set control colors */
    HBRUSH do_ctlcolor(UINT msg, HDC hdc, HWND hwnd);

    /* handle timer messages */
    int do_timer(int timer_id);

    /* remember my position in the preferences */
    void set_pos_prefs();

    /*
     *   Get the scrollbar information.  This routine must be overridden
     *   to provide the proper ranges for the window.  Returns true if the
     *   scrollbar should be visible, false if not.  The default routine
     *   simply initializes the header of the structure.  
     */
    virtual int get_scroll_info(int /*vert*/, SCROLLINFO *info)
    {
        info->cbSize = sizeof(SCROLLINFO);
        info->fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
        return TRUE;
    }

    /*
     *   Adjust scrollbar ranges.  This is called after the window is
     *   resized, and should be called whenever the content is updated in
     *   such a way as to change the scrolling ranges.  The default
     *   routine has no effect; this must be overridden to calculate
     *   scrollbar ranges as appropriate for the window content.  
     */
    virtual void adjust_scrollbar_ranges() { }
    
    /*
     *   Set the positions of the scrollbars.  This is used after
     *   resizing the window to keep the scrollbars at the edges of the
     *   window, and also can be used when one of the scrollbars is being
     *   made visible or invisible, so that the other adjusts to fill the
     *   correct space. 
     */
    virtual void adjust_scrollbar_positions();

    /*
     *   Determine whether we track a scrollbar's thumb actively while
     *   the thumb is being dragged - if true, we scroll during thumb
     *   tracking, otherwise we wait until the thumb has been released to
     *   do the scrolling.  By default, we track directly, but when it's
     *   not practical to keep the window updated repeatedly during
     *   scrolling (which would be the case if it's slow to redraw the
     *   content), this should be overridden to return false.  
     */
    virtual int active_scroll_thumb_tracking(HWND)
        { return TRUE; }

    /*
     *   Get the area to be scrolled.  By default, we'll use the entire
     *   client area minus visible scrollbars.  If other non-scrolling
     *   frame features are present in the window's client area, this must
     *   be overridden to exclude those features.
     *   
     *   If 'vertical' is true, return the area for vertical scrolling;
     *   otherwise, return the area for horizontal scrolling.  These might
     *   be different in some cases; for example, if there's a header that
     *   scrolls horizontally but is fixed in place with respect to
     *   vertical scrolling, the header would be included in the
     *   horizontal scrolling area but not the vertical area.  
     */
    virtual void get_scroll_area(RECT *rc, int vertical) const;

    /*
     *   determine whether vertical and horizontal scrollbars are present
     *   and visible 
     */
    virtual int vscroll_is_visible() const { return FALSE; }
    virtual int hscroll_is_visible() const { return FALSE; }

    /*
     *   Get the number of pixels per scrolling unit.  When we scroll the
     *   window, we'll use this to determine how much we have to move the
     *   pixels on the screen for a given change in the scrollbar
     *   position.  The default is 16 pixels per unit, which is about the
     *   size of a character cell in a 12-point font.  
     */
    virtual long get_vscroll_units() const { return 16; }
    virtual long get_hscroll_units() const { return 16; }

    /*
     *   Notify the window that scrolling is about to occur.  The window
     *   may need to do some cleanup work, such as removing any xor
     *   drawing, that doesn't translate well across scrolling operations. 
     */
    virtual void notify_pre_scroll(HWND) { }

    /*
     *   Notify the window that scrolling has occurred.  The window
     *   should take note of the new position, but the scrollbar has
     *   already been updated and the window's pixels have been scrolled.
     *   The default implementation does nothing; this routine is provided
     *   so that the window subclass can fix up its internal positioning
     *   after scrolling has occurred.  
     */
    virtual void notify_scroll(HWND, long /*oldpos*/, long /*newpos*/) { }

    /*
     *   Initialize drag scrolling.  Sets a timer that will be called to
     *   simulate mouse move events when the mouse is being held outside
     *   the window.  
     */
    void start_drag_scroll();

    /*
     *   End drag scrolling.  This should be called when capture is
     *   released to remove the drag-scroll timer.  
     */
    void end_drag_scroll();

    /*
     *   Perform drag scrolling if appropriate.  We'll scroll by one unit in
     *   the appropriate direction if the mouse is outside the scrolling
     *   rectangle of the window, and enough time has elapsed since the last
     *   drag scrolling we did.  Returns true if we did any scrolling, false
     *   if not.
     *   
     *   If x_inset or y_inset are positive, they give the insets into the
     *   scrolling area for the neutral zone.  Normally these are zero, to
     *   indicate that we should scroll the window's contents only when the
     *   mouse is dragged outside the normal scrolling area.  However, it is
     *   sometimes desirable, such as during drag-and-drop operations, to
     *   scroll when the mouse strays into a border around the window frame,
     *   rather than requiring the mouse to leave the window frame; use the
     *   insets to establish the size of this interior border area, if
     *   desired.  
     */
    int maybe_drag_scroll(long x, long y, int x_inset, int y_inset);

    /* message filter hook for scrollbar events */
    static LRESULT CALLBACK sb_filter_proc(int code, WPARAM wpar,
                                           LPARAM lpar);

protected:
    /* scrollbars */
    HWND vscroll_;
    HWND hscroll_;

    /*
     *   Flags: scrollbars are external.  These are set to true when the
     *   scrollbars belong to an external window (such as our parent
     *   window), but still scroll content contained within our client
     *   area 
     */
    int ext_vscroll_ : 1;
    int ext_hscroll_ : 1;

    /* vertical and horizontal scrolling offsets */
    long vscroll_ofs_;
    long hscroll_ofs_;

    /* scrollbar visibility flags */
    int vscroll_vis_ : 1;
    int hscroll_vis_ : 1;

    /* sizebox control */
    HWND sizebox_;

    /* gray box at bottom right corner - used when both scrollbars present */
    HWND graybox_;

    /* flags indicating whether scrollbars are desired */
    int has_vscroll_ : 1;
    int has_hscroll_ : 1;

    /* 
     *   flag indicating whether a sizebox is desired in the corner
     *   between the vertical and horizontal scrollbars 
     */
    int has_sizebox_ : 1;

    /* flag indicating that we're doing drag scrolling */
    int in_drag_scroll_ : 1;

    /* last drag scroll time */
    DWORD drag_scroll_time_;

    /* our windows hook for message filter events */
    static HHOOK sb_filter_hook_;

    /* flag: scrollbar thumb tracking in progress */
    static int sb_thumb_track_;
};


#endif /* TADSWIN_H */

