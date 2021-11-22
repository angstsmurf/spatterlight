/* $Header: d:/cvsroot/tads/html/win32/tadsdock.h,v 1.2 1999/07/11 00:46:48 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1999 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsdock.h - TADS docking window class for Windows
Function
  
Notes
  
Modified
  05/22/99 MJRoberts  - Creation
*/

#ifndef TADSDOCK_H
#define TADSDOCK_H

#include "tadshtml.h"
#include "tadswin.h"
#include "htmlres.h"

/* ------------------------------------------------------------------------ */
/* 
 *   docking alignment types 
 */
enum tads_dock_align
{
    TADS_DOCK_ALIGN_LEFT,
    TADS_DOCK_ALIGN_RIGHT,
    TADS_DOCK_ALIGN_TOP,
    TADS_DOCK_ALIGN_BOTTOM
};

/*
 *   docked window orientation types 
 */
enum tads_dock_orient
{
    /* horizontal alignment - title bar is down left edge */
    TADS_DOCK_ORIENT_HORIZ,

    /* vertical alignment - title bar is across type edge */
    TADS_DOCK_ORIENT_VERT
};

/* ------------------------------------------------------------------------ */
/*
 *   Private Messages 
 */

/* destroy the docking window */
const int DOCKM_DESTROY = WM_USER;


/* ------------------------------------------------------------------------ */
/*
 *   Docking window.  This is a frame window that contains a subwindow;
 *   any type of window can be contained in a docking window.  The custom
 *   action takes place in the subwindow; this is just an empty shell that
 *   provides docking.
 *   
 *   This window behaves, apart from the docking behavior, as a normal
 *   pop-up window.
 *   
 *   When the window is docked, we move the subwindow to a CTadsWinDocked
 *   container.  
 */
class CTadsWinDock: public CTadsWin
{
public:
    /* 
     *   Instantiate.  docked_width is the default width to use when
     *   docked; docked_height is the default height to use; docked_align
     *   is the default alignment to use when docking the window.  
     */
    CTadsWinDock(CTadsWin *subwin, int docked_width, int docked_height,
                 tads_dock_align docked_align, const char *guid);

    /* instantiate, attaching to an existing model window */
    CTadsWinDock(CTadsWin *subwin, int docked_width, int docked_height,
                 tads_dock_align docked_align,
                 class CTadsDockModelWin *model_win);

    /* delete */
    ~CTadsWinDock();

    /* 
     *   Am I docked?  Returns true if the window is invisible and is set
     *   to dock on becoming visible, false otherwise. 
     */
    virtual int is_win_docked() const
    {
        /* 
         *   normally, this type of window is not docked; however, if I'm
         *   hidden and set to become a docked window when unhidden, treat
         *   this as a docked window 
         */
        return (!IsWindowVisible(handle_) && dock_on_unhide_);
    }

    /* am I dockable? */
    virtual int is_win_dockable() const
    {
        /* 
         *   normally, this type of window is dockable; however, if I'm
         *   hidden and set to convert to a non-docking view on being
         *   unhidden, treat this as a non-docking view 
         */
        return (IsWindowVisible(handle_) || !mdi_on_unhide_);
    }

    /* get my docking container - I'm not docked, so I have no container */
    virtual class CTadsDockedCont *get_dock_container() const
        { return 0; }

    /* get my subwindow */
    CTadsWin *get_subwin() const { return subwin_; }

    /* set my subwindow */
    void set_subwin(CTadsWin *subwin) { subwin_ = subwin; }

    /* get/set my model window */
    class CTadsDockModelWin *get_model_win() const { return model_win_; }
    void set_model_win(class CTadsDockModelWin *win);

    /* 
     *   set the default docking port - this is the docking port we'll use
     *   for auto-dock operations 
     */
    static void set_default_port(class CTadsDockport *port)
        { default_port_ = port; }

    /* add a window to the docking port registry */
    static void add_port(class CTadsDockport *port);

    /* remove a window from the docking port registry */
    static void remove_port(class CTadsDockport *port);

    /* 
     *   set the MDI client - if this is set, a docking view can be
     *   converted to an MDI view 
     */
    static void set_default_mdi_parent(class CTadsWin *mdi_parent)
        { default_mdi_parent_ = mdi_parent; }

    /* get the dfeault MDI parent window */
    static class CTadsWin *get_default_mdi_parent() 
        { return default_mdi_parent_; }
    
    /* window style */
    DWORD get_winstyle()
    {
        return (WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_THICKFRAME
                | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_SYSMENU);
    }

    /* extended window style */
    DWORD get_winstyle_ex() { return WS_EX_TOOLWINDOW; }

    /* on creation, we'll create the subwindow */
    void do_create();

    /* on closing, we'll notify the subwindow */
    int do_close();

    /* on destruction, we'll destroy the subwindow */
    void do_destroy();

    /* process a user message */
    int do_user_message(int msg, WPARAM wpar, LPARAM lpar);

    /* resize */
    void do_resize(int mode, int x, int y);

    /* show the window */
    int do_showwindow(int show, int status);

    /* set focus on this window */
    void do_setfocus(HWND previous_focus);

    /* on activation or deactivation, we'll notify the subwindow */
    int do_activate(int flag, int minimized, HWND other_win);

    /* child activation */
    int do_childactivate()
    {
        /* set focus on my child */
        if (subwin_ != 0)
            SetFocus(subwin_->get_handle());

        /* inherit default */
        return CTadsWin::do_childactivate();
    }

    /* key down */
    virtual int do_keydown(int virtual_key, long keydata);

    /* key up */
    virtual int do_keyup(int virtual_key, long keydata);

    /* system character */
    virtual int do_syschar(TCHAR ch, long keydata);

    /* handle non-client clicks for our special title dragging */
    int do_nc_leftbtn_down(int keys, int x, int y, int clicks, int hit_type);
    int do_leftbtn_up(int keys, int x, int y);

    /* non-client right click */
    int do_nc_rightbtn_down(int keys, int x, int y, int clicks, int hit_type);
        
    /* track mouse movement */
    int do_mousemove(int keys, int x, int y);

    /* receive notification of capture change */
    int do_capture_changed(HWND new_capture_win);

    /* process a command */
    int do_command(int notify_code, int command_id, HWND ctl);

    /* check the status of a command */
    TadsCmdStat_t check_command(int command_id);

    /* get the default docked width/height */
    int get_docked_width() const { return docked_width_; }
    int get_docked_height() const { return docked_height_; }

    /* get the default docked alignment */
    tads_dock_align get_docked_align() const { return docked_align_; }

    /* get the undocked position for this window */
    virtual void get_undocked_pos(RECT *rc) const
    {
        /* this class is undocked, so return my actual window area */
        GetWindowRect(handle_, rc);
    }

    /* get my undocked parent window */
    virtual class CTadsWin *get_undocked_parent() const
    {
        /* I'm not docked, so return my actual parent */
        return parent_;
    }

    /* get the handle of the parent window when undocked */
    virtual HWND get_undocked_parent_hdl() const
    {
        /* return my actual parent handle */
        return parent_->get_handle();
    }

    /* determine if I have an enabled closebox */
    virtual int has_closebox() const
    {
        /* 
         *   I have a closebox if my window class doesn't have the
         *   CS_NOCLOSE (no close box) style flag
         */
        return !(GetClassLong(handle_, GCL_STYLE) & CS_NOCLOSE);
    }

    /* get my undocked window title */
    virtual void get_undocked_title(char *buf, size_t buflen) const
    {
        /* I'm undocked, so simply get my window title from the system */
        GetWindowText(handle_, buf, buflen);
    }
    
    /* on docking or undocking, move my subwindow to the given new parent */
    virtual void reparent_subwin(CTadsWinDock *new_parent);

    /* 
     *   auto-dock - dock the window at the last docking position (or as
     *   close to the last position as possible) 
     */
    void auto_dock();

    /* convert to MDI */
    void cvt_to_mdi(int visible);

    /* set the window to re-dock when it's next un-hidden */
    void set_dock_on_unhide() { dock_on_unhide_ = TRUE; }

    /* set the window to convert to MDI when it's next un-hidden */
    void set_mdi_on_unhide() { mdi_on_unhide_ = TRUE; }

    /* determine if I'll convert to MDI when un-hidden */
    int is_mdi_on_unhide() const { return mdi_on_unhide_; }

protected:
    /* initialize */
    void init(CTadsWin *subwin, int docked_width, int docked_height,
              tads_dock_align docked_align,
              class CTadsDockModelWin *model_win);
    
    /* show our system menu */
    void show_ctx_menu(int x, int y);
    
    /* begin title-bar drag tracking */
    void begin_title_tracking(const RECT *undock_rc,
                              int start_xofs, int start_yofs);

    /* continue title tracking */
    void continue_title_tracking(int keys, int x, int y, int done);

    /* end title-bar drag tracking */
    void end_title_tracking();

    /* undock the window at the given position */
    virtual void undock_window(const RECT *pos, int show);

    /* xor the current drag rectangle onto the screen */
    void xor_drag_rect(const RECT *rc) const;

    /* find a docking port at the given mouse (screen) coordinates */
    class CTadsDockport *find_docking_port(POINT pt);

    /* 
     *   Adjust for removal of our subwindow.  This is called after our
     *   subwindow has been reparented into another winodw. 
     */
    virtual void adjust_for_no_subwin();

    /* my context menu ID */
    virtual int get_ctx_menu_id() const { return IDR_DOCKWIN_POPUP; }
    
    /* my subpanel */
    CTadsWin *subwin_;

    /* 
     *   Flag: when the window is un-hidden, immediately dock it.  This is
     *   set when we undock the window in order to hide its docked
     *   version; if the window is eventually shown again, we want to show
     *   it as docked rather than undocked.  (We hide only undocked
     *   windows to avoid having to deal with hidden windows in
     *   calculating the docking layout.)  
     */
    unsigned int dock_on_unhide_ : 1;

    /* Flag: when the window is un-hidden, convert it to an MDI child. */
    unsigned int mdi_on_unhide_ : 1;

    /* flag: dragging the window title bar */
    unsigned int in_title_drag_ : 1;

    /* last drag rectangle position */
    RECT drag_win_rect_;

    /* 
     *   original window rectangle during dragging - we need to keep this,
     *   so that we can restore the original size if we hover over a
     *   docking position but then go back to undocked dragging 
     */
    RECT drag_orig_win_rect_;

    /* initial offset of mouse in the caption bar during title dragging */
    int drag_mouse_xofs_;
    int drag_mouse_yofs_;

    /* last mouse and key state for title tracking */
    int drag_x_;
    int drag_y_;
    int drag_keys_;

    /* brush for drawing the drag rectangle outline */
    HBRUSH hbrush_;

    /* head of global docking port list */
    static class CTadsDockport *ports_;

    /* default dock port */
    static class CTadsDockport *default_port_;

    /* default MDI parent (for converting to MDI views) */
    static class CTadsWin *default_mdi_parent_;

    /* default docked width/height for next docking */
    int docked_width_;
    int docked_height_;

    /* default docked alignment for next docking */
    tads_dock_align docked_align_;

    /* my context menu */
    HMENU ctx_menu_;
    HMENU ctx_menu_popup_;

    /* my model window */
    class CTadsDockModelWin *model_win_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Docked Window.  This is the shell for a window when it is docked
 *   within a docking port.
 *   
 *   When the window is undocked, we move the subwindow to a CTadsWinDock
 *   container.  
 */
class CTadsWinDocked: public CTadsWinDock
{
    friend class CTadsDockedCont;
    
public:
    CTadsWinDocked(class CTadsDockedCont *cont, tads_dock_orient orientation,
                   int docked_width, int docked_height,
                   const RECT *undocked_rc, const char *undocked_title,
                   class CTadsWin *undocked_parent, HWND undocked_parent_hdl,
                   int has_closebox, class CTadsDockModelWin *model_win);
    ~CTadsWinDocked();
    
    /* am I docked? */
    virtual int is_win_docked() const { return TRUE; }

    /* am I dockable? */
    virtual int is_win_dockable() const { return TRUE; }

    /* window style: this is a child window with no title bar */
    virtual DWORD get_winstyle()
        { return (WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS); }

    /* get my docking container */
    virtual class CTadsDockedCont *get_dock_container() const
        { return cont_; }

    /* forget my docking container */
    void forget_dock_container() { cont_ = 0; }

    /* resize */
    void do_resize(int mode, int x, int y);

    /* draw */
    void do_paint_content(HDC hdc, const RECT *area_to_draw);

    /* erase the background - let the paint routine do the work */
    int do_erase_bkg(HDC) { return 1; }

    /* process a command */
    int do_command(int notify_code, int command_id, HWND ctl);

    /* check the status of a command */
    TadsCmdStat_t check_command(int command_id);

    /* process a notification message */
    int do_notify(int control_id, int notify_code, LPNMHDR nm);

    /* left click */
    int do_leftbtn_down(int keys, int x, int y, int clicks);
    int do_leftbtn_up(int keys, int x, int y);

    /* right click */
    int do_rightbtn_down(int keys, int x, int y, int clicks);

    /* mouse movement */
    int do_mousemove(int keys, int x, int y);

    /* handle capture change */
    int do_capture_changed(HWND new_capture_win);

    /* on docking or undocking, move my subwindow to the given new parent */
    virtual void reparent_subwin(CTadsWinDock *new_parent);

    /* get the undocked position for this window */
    virtual void get_undocked_pos(RECT *rc) const
    {
        /* 
         *   we're currently docked, so we must rely on our memory of the
         *   undocked position 
         */
        *rc = undocked_rc_;
    }

    /* get my undocked parent window */
    virtual class CTadsWin *get_undocked_parent() const
    {
        /* return my saved undocked parent */
        return undocked_parent_;
    }

    /* get my undocked parent handle */
    virtual HWND get_undocked_parent_hdl() const
    {
        /* return my saved undocked parent handle */
        return undocked_parent_hdl_;
    }

    /* determine if I have an enabled closebox */
    virtual int has_closebox() const { return has_closebox_; }

    /* get my undocked window title */
    virtual void get_undocked_title(char *buf, size_t buflen) const
    {
        size_t len;

        /* get the title from my buffer */
        len = undocked_title_.get() != 0 ? strlen(undocked_title_.get()) : 0;

        /* copy the title if we have space in the buffer */
        if (buflen >= len + 1)
        {
            /* if there's anything to copy, copy it */
            if (len != 0)
                memcpy(buf, undocked_title_.get(), len);

            /* null-terminate the result */
            buf[len] = '\0';
        }
        else if (buflen > 0)
        {
            /* the title won't fit, but we can at least fit a null byte */
            buf[0] = '\0';
        }
    }

    /* get the next docked window */
    CTadsWinDocked *get_next_docked() const { return next_docked_; }

    /* 
     *   get the docked extent - this is the size in the appropriate
     *   direction for our orientation: width for horizontal orientation,
     *   height for vertical orienation 
     */
    int get_docked_extent() const
    {
        RECT rc;

        /* get my client area */
        GetClientRect(handle_, &rc);

        /* return the appropriate dimension for my orientation */
        if (orientation_ == TADS_DOCK_ORIENT_HORIZ)
            return rc.right - rc.left;
        else
            return rc.bottom - rc.top;
    }

protected:
    /* undock the window at the given position */
    virtual void undock_window(const RECT *pos, int show);

    /* 
     *   Adjust for removal of our subwindow.  This is called after our
     *   subwindow has been reparented into another winodw.  
     */
    virtual void adjust_for_no_subwin();

    /* my context menu ID */
    virtual int get_ctx_menu_id() const { return IDR_DOCKEDWIN_POPUP; }

    /* 
     *   get the rectangle for the title bar, given the client area of the
     *   window - this is the entire title bar, including the space for
     *   controls such as close boxes 
     */
    void get_title_bar_rect(RECT *rc) const;

    /* 
     *   get the rectangle for the closebox, given the client area for the
     *   window 
     */
    void get_closebox_rect(RECT *rc) const;

    /* draw the closebox in its current state */
    void draw_closebox();

    /* draw a horizontal/vertical title bar line */
    void draw_title_line_v(HDC hdc, const RECT *rc) const;
    void draw_title_line_h(HDC hdc, const RECT *rc) const;
    
    /* 
     *   our window position when last undocked - if we undock this
     *   window, we'll restore the window to this original size 
     */
    RECT undocked_rc_;

    /* title of undocked window */
    CStringBuf undocked_title_;

    /* parent window and handle for our undocked counterpart */
    class CTadsWin *undocked_parent_;
    HWND undocked_parent_hdl_;

    /* our docking container */
    class CTadsDockedCont *cont_;

    /* our docking orientation */
    tads_dock_orient orientation_;
    
    /* next docked window in our dock port */
    CTadsWinDocked *next_docked_;

    /* drawing pens */
    HPEN face_pen_;
    HPEN shadow_pen_;
    HPEN dkshadow_pen_;
    HPEN hilite_pen_;

    /* close-box button bitmap */
    HBITMAP closebox_bmp_;

    /* title bar font */
    HFONT title_font_;

    /* flag: we have a close box */
    unsigned int has_closebox_ : 1;

    /* flag: tracking closebox */
    unsigned int tracking_closebox_ : 1;

    /* closebox state - true -> clicked */
    unsigned int closebox_down_ : 1;
};

/* ------------------------------------------------------------------------ */
/*
 *   MDI child window that can be converted to a docking view.  In some
 *   cases, we may want to make a docking window convertible to an MDI
 *   view, and vice versa; this class provides the MDI child which serves
 *   as the substitute parent (in place of a CTadsWinDock) for a window
 *   that has been converted to an MDI view.  
 */
class CTadsWinDockableMDI: public CTadsWinDock
{
public:
    CTadsWinDockableMDI(CTadsWin *subwin, int docked_width,
                        int docked_height, tads_dock_align docked_align,
                        CTadsDockModelWin *model_win,
                        CTadsWin *undocked_parent, HWND undocked_parent_hdl)
        : CTadsWinDock(subwin, docked_width, docked_height,
                       docked_align, model_win)
    {
        /* save the information on the undocked window */
        undocked_parent_ = undocked_parent;
        undocked_parent_hdl_ = undocked_parent_hdl;
    }

    /* window style */
    DWORD get_winstyle()
    {
        return (WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
                | WS_SYSMENU | WS_CAPTION | WS_THICKFRAME
                | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
    }

    /* extended window style */
    DWORD get_winstyle_ex() { return 0; }

    /* this type of window is not docked */
    virtual int is_win_docked() const { return FALSE; }

    /* am I dockable? */
    virtual int is_win_dockable() const { return FALSE; }

    /* command processing */
    int do_command(int notify_code, int command_id, HWND ctl);
    TadsCmdStat_t check_command(int command_id);

    /* process a user message */
    int do_user_message(int msg, WPARAM wpar, LPARAM lpar);

protected:
    /* adjust for disappearance of our subwindow */
    virtual void adjust_for_no_subwin();

    /* context menu ID */
    virtual int get_ctx_menu_id() const { return IDR_DOCKABLEMDI_POPUP; }

    /* convert to a docking view */
    void cvt_to_docking_view();

    /* parent of the undocked version of the window */
    class CTadsWin *undocked_parent_;
    HWND undocked_parent_hdl_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Docking port.  For each window that can act as a docking port, an
 *   instance of this interface must be created and registered via
 *   CTadsWinDock::add_port().  
 */
class CTadsDockport
{
    friend class CTadsWinDock;
    
public:
    /* delete */
    virtual ~CTadsDockport();

    /* get my window handle */
    virtual HWND dockport_get_handle() = 0;

    /* 
     *   Determine if we should dock when dragging a window over this
     *   port.  The given point is the mouse position in client
     *   coordinates for this window.
     *   
     *   If the port wants to dock the window, it should fill in
     *   '*dock_pos' with the client coordinates (in the docking port)
     *   where the window would end up if it were docked here, fill in
     *   '*alignment' with the alignment type if we were docking here, and
     *   then return true.  If the port isn't interested in docking this
     *   window at this position, simply return false.
     *   
     *   This routine merely determines if we should dock; it shouldn't
     *   actually do any docking.  
     */
    virtual int dockport_query_dock(class CTadsWinDock *win_to_dock,
                                    POINT mousepos, RECT *dock_pos) const = 0;

    /*
     *   Dock a window.  This is called only after a previous
     *   dockport_query_dock() returned true, and is called with the same
     *   port-local point for which dockport_query_dock returned true.  
     */
    virtual void dockport_dock(class CTadsWinDock *win_to_dock,
                               POINT mousepos) = 0;

    /*
     *   Auto-dock a window.  This is called in response to an operation
     *   that causes the window to dock without the user specifying where
     *   it should go.  To the extent possible, the window should end up
     *   in the same configuration it was in the last time it was docked. 
     */
    virtual void dockport_auto_dock(class CTadsWinDock *win_to_dock) = 0;

protected:
    /* next in list */
    CTadsDockport *next_dock_port_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Docked Window Container.  This is a container for docked windows.
 *   This is different from a docking port, because a docking port might
 *   not actually contain docked windows, but merely act as a drop target
 *   for docking windows which, when a window actually docks, creates a
 *   new window to contain the docked window.
 */
class CTadsDockedCont
{
public:
    CTadsDockedCont()
    {
        /* no docked windows yet */
        docked_win_first_ = docked_win_last_ = 0;
        docked_win_cnt_ = 0;
    }
    
    /* get the number of docked windows */
    int get_docked_win_cnt() const { return docked_win_cnt_; }

    /* add a docked window */
    void add_docked_win(class CTadsWinDocked *win);

    /* insert a docked window at a given index */
    void add_docked_win(class CTadsWinDocked *win, int idx);

    /* remove a docked window */
    virtual void remove_docked_win(class CTadsWinDocked *win);

    /* get my docking alignment */
    virtual tads_dock_align get_docked_alignment() const = 0;

protected:
    /* 
     *   unlink a window from our list, returning the window's index in
     *   the list (returns -1 if the window is not in the list) 
     */
    int unlink_docked_win(class CTadsWinDocked *win);

    /* list of my docked windows */
    class CTadsWinDocked *docked_win_first_;
    class CTadsWinDocked *docked_win_last_;

    /* number of docked windows */
    int docked_win_cnt_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Docking port for an MDI client window.
 */
class CTadsDockportMDI: public CTadsDockport
{
public:
    CTadsDockportMDI(class CTadsWin *mdi_frame, HWND client_handle);
    ~CTadsDockportMDI();

    /* get my model frame */
    class CTadsDockModelFrame *get_model_frame() const
        { return model_frame_; }

    /*
     *   Handle resizing.  The frame window should call this routine
     *   whenever the MDI frame is resized to adjust the positions of
     *   docked windows.  'client_rect' is the available client area after
     *   removing non-docking-bar adornments, such as toolbars and status
     *   bars.  On return, client_rect is updated to reflect the area
     *   remaining after removing the space occupied by the docking bars.  
     */
    void dockport_mdi_resize(RECT *client_rect);

    /* report our target handle */
    HWND dockport_get_handle() { return client_handle_; }

    /* try docking */
    int dockport_query_dock(class CTadsWinDock *win_to_dock,
                            POINT mousepos, RECT *dock_pos) const;

    /* do docking */
    void dockport_dock(class CTadsWinDock *win_to_dock, POINT mousepos);

    /* do auto-docking */
    void dockport_auto_dock(class CTadsWinDock *win_to_dock);

    /* add a docking bar at the end of our list */
    void add_docking_bar(class CTadsDockportMDIBar *bar);

    /* add a docking bar before the given docking bar */
    void add_docking_bar_before(class CTadsDockportMDIBar *bar,
                                class CTadsDockportMDIBar *before_bar);

    /* add a docking bar at the position indicated by the model */
    void add_docking_bar(class CTadsDockportMDIBar *bar,
                         class CTadsDockModelCont *model_cont);

    /* remove a docking bar from our list */
    void remove_docking_bar(class CTadsDockportMDIBar *bar);

    /* adjust the containing frame's layout */
    static void adjust_frame_layout(HWND frame_hdl);

    /* get my MDI frame window */
    class CTadsWin *get_frame_win() const { return mdi_frame_; }

protected:
    /* create a new docking bar at a given position and orientation */
    CTadsDockportMDIBar *create_bar(tads_dock_align align, const RECT *pos,
                                    class CTadsDockModelCont *model_cont);

    /* create a docking bar with the given alignment and width or height */
    CTadsDockportMDIBar *create_bar(tads_dock_align align,
                                    int width, int height,
                                    class CTadsDockModelCont *model_cont);

    /* create a docking bar based on a model docking bar */
    CTadsDockportMDIBar *create_bar(CTadsDockModelCont *model_cont,
                                    int width, int height);

    /* get docking information for a given mouse position */
    int get_dock_info(class CTadsWinDock *win_to_dock,
                      POINT mousepos, RECT *dock_pos,
                      tads_dock_align *align) const;
    
    /* MDI frame window */
    class CTadsWin *mdi_frame_;

    /* MDI client window handle */
    HWND client_handle_;

    /* head of our list of docking bars */
    class CTadsDockportMDIBar *first_bar_;
    class CTadsDockportMDIBar *last_bar_;

    /* model frame */
    class CTadsDockModelFrame *model_frame_;
};

/* ------------------------------------------------------------------------ */
/*
 *   MDI docking bar.  Within the MDI frame window, we create docking
 *   bars.  A docking bar is an edge of the client area; one or more
 *   docking windows go inside the docking bar.
 *   
 *   A docking bar is itself a dock port.  
 */

/* size of our sizing widget */
static const int MDI_WIDGET_SIZE = 3;

/* size of our splitter */
static const int MDI_SPLITTER_SIZE = 3;

/* 
 *   MDI docking bar class 
 */
class CTadsDockportMDIBar: public CTadsWin, public CTadsDockport,
    public CTadsDockedCont
{
    friend class CTadsDockportMDI;

public:
    CTadsDockportMDIBar(class CTadsDockportMDI *frame, int siz,
                        tads_dock_align alignment,
                        class CTadsDockModelCont *model_cont);
    ~CTadsDockportMDIBar();
    
    /* get my alignment type */
    virtual tads_dock_align get_bar_alignment() const = 0;

    /* for interface CTadsDockedCont - get alignment */
    virtual tads_dock_align get_docked_alignment() const
        { return get_bar_alignment(); }

    /* get the orientation type for my subwindows */
    virtual tads_dock_orient get_win_orientation() const = 0;

    /* get my size */
    int get_bar_size() const { return siz_; }
    
    /* 
     *   Receive notification that the parent window is being resize.
     *   Move this bar to the appropriate position, and remove the space
     *   it occupies from the rectangle. 
     */
    void dockport_mdi_bar_resize(RECT *avail_area);

    /* window style: this is a child window with no title bar */
    virtual DWORD get_winstyle()
        { return (WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS); }

    /* handle a window-pos-changing event */
    virtual int do_windowposchanging(WINDOWPOS *pos);

    /* paint */
    void do_paint_content(HDC hdc, const RECT *area_to_draw);

    /* erase the background - let the paint routine do the work */
    int do_erase_bkg(HDC) { return 1; }

    /* left click */
    int do_leftbtn_down(int keys, int x, int y, int clicks);
    int do_leftbtn_up(int keys, int x, int y);

    /* track mouse movement */
    int do_mousemove(int keys, int x, int y);

    /* capture change notification */
    int do_capture_changed(HWND new_capture_win);

    /* report our target handle */
    HWND dockport_get_handle() { return get_handle(); }

    /* try docking */
    int dockport_query_dock(class CTadsWinDock *win_to_dock,
                            POINT mousepos, RECT *dock_pos) const;

    /* do docking */
    void dockport_dock(class CTadsWinDock *win_to_dock, POINT mousepos);

    /* 
     *   we don't support auto docking (a bar should never be the default
     *   dockport, so this should never be needed) 
     */
    void dockport_auto_dock(class CTadsWinDock *) { }

    /* add a window to the bar at the given index */
    void add_docked_win_to_bar(class CTadsWinDock *win_to_dock,
                               int idx, class CTadsWin *frame_win,
                               int update_model);

    /* remove a docked window */
    virtual void remove_docked_win(class CTadsWinDocked *win);

    /* 
     *   forget my frame - the frame will call this if it's deleted before
     *   its bars, to tell the bars that they don't need to clean up the
     *   frame because it's going to be deleted anyway 
     */
    void forget_frame() { frame_ = 0; }

protected:
    /* calculate the new layout for a resize of the bar */
    void calc_win_resize(RECT old_bar_rc, RECT new_bar_rc);

    /* calculate the new layout after removing a window */
    void calc_layout_for_removal(class CTadsWinDocked *win);

    /* get docking information for a given mouse position */
    int get_dock_info(class CTadsWinDock *win_to_dock,
                      POINT mousepos, RECT *dock_pos, int *idx) const;

    /* determine whether a mouse position is over the sizing widget */
    int over_sizing_widget(int x, int y) const;

    /* 
     *   determine whether a mouse position is in a splitter; returns the
     *   window preceding the splitter if the point is indeed within a
     *   splitter, null if the point is not within a splitter 
     */
    class CTadsWinDocked *find_splitter(int x, int y) const;

    /* xor the sizing bar into the frame during mouse tracking */
    void xor_size_bar();

    /* xor the splitter during mouse tracking */
    void xor_splitter();

    /* calculate where we go based on our size and the given available area */
    virtual void calc_resize(RECT *rc) const = 0;

    /* calculate our insets for borders and widgets */
    virtual void calc_insets(RECT *rc, int win_cnt) const = 0;

    /* draw the sizing bar, given the location of the bar */
    virtual void draw_sizing_bar(HDC hdc, const RECT *bar_rc) const = 0;

    /* draw a splitter control, given the offset of the splitter */
    virtual void draw_splitter(HDC hdc, int ofs) const = 0;

    /* get the rectangle occupied by the sizing bar, given the client rect */
    virtual void get_sizing_bar_rect(RECT *rc) const = 0;

    /* offset the sizing bar rectangle for tracking */
    virtual void offset_sizing_bar_rect_tracking(RECT *rc) const = 0;

    /* offset the splitter bar rectangle for tracking */
    virtual void offset_splitter_rect_tracking(RECT *rc) const = 0;

    /* 
     *   Resize windows based on split tracking.  rc_prv is the window
     *   that precedes the splitter; rc_nxt is the window after the
     *   splitter.  This routine should limit the size change so that
     *   neither rectangle's size becomes negative.  
     */
    virtual void resize_for_split_track(RECT *rc_prv, RECT *rc_nxt) const = 0;

    /* 
     *   adjust the size for mouse tracking, giving the difference from
     *   the last tracking mouse position to the current position 
     */
    virtual void adjust_size_tracking(int delta_x, int delta_y) = 0;

    /*
     *   Allocate space to a window on resizing the bar.  Fills in
     *   new_win_rc with the window's new position and size, and updates
     *   rem to remove the space allocated to the window.  old_win_rc is
     *   the window's old position, old_bar_rc is the bar's original size,
     *   and new_bar_rc is the bar's new size; space should be allocated
     *   to the window in the new layout in proportion to the space it
     *   used in the old layout.  
     */
    virtual void alloc_win_share(RECT *new_win_rc,
                                 const RECT *old_win_rc,
                                 const RECT *new_bar_rc,
                                 const RECT *old_bar_rc,
                                 RECT *rem) const = 0;

    /*
     *   Calculate the shape of a window when the window is to be added to
     *   the bar.  new_win_cnt is the number of windows that we'll have
     *   after adding the new window to the bar.  
     */
    virtual void get_new_win_shape(RECT *new_shape, int new_cnt) const = 0;

    /*
     *   Determine if a mouse position, relative to the bar's upper left
     *   corner, is within the given subwindow.  If it is within the
     *   subwindow, set (*second_half) to false if it's within the first
     *   (left or top, depending on the orientation) half of the
     *   subwindow, true if it's in the second half.  (*ofs) on entry
     *   contains the offset of the start of this window; upon return,
     *   we'll increment (*ofs) by the size of this window (width or
     *   height, depending on the orientation), including any splitter
     *   bars or other ornaments, so that (*ofs) is the offset of the next
     *   window after this one.  Returns true if the point is in this
     *   window, false if not.  
     */
    virtual int pt_in_win_extent(class CTadsWinDocked *win,
                                 POINT mousepos, int *ofs,
                                 int *second_half) const = 0;

    /* offset a rectangle by a window's extent for our orientation */
    virtual void offset_by_win_extent(RECT *rc, int ofs) const = 0;

    /* increase a window's extent by the given amount */
    virtual void add_win_extent(RECT *rc, int delta) const = 0;

    /* 
     *   Subtract a window's extent from the given rectangle.  Adjusts
     *   only the width or height of the rectangle, as appropriate to the
     *   orientation. 
     */
    virtual void sub_win_extent(RECT *client_rect,
                                const RECT *win_to_sub) const = 0;

    /* 
     *   set a rectangle's width or height, as appropriate, to the size of
     *   a splitter control 
     */
    virtual void set_splitter_size(RECT *rc) const = 0;

    /* my containing MDI frame window */
    class CTadsDockportMDI *frame_;

    /* 
     *   Width or height.  Only one dimension is free, since the other
     *   dimension is constrained to occupy the full extent of the
     *   available space in the parent frame window. 
     */
    int siz_;
    
    /* next bar in my frame's list */
    CTadsDockportMDIBar *next_mdi_bar_;

    /* drawing pens */
    HPEN hilite_pen_;
    HPEN shadow_pen_;
    HPEN dkshadow_pen_;

    /* brush for drawing sizing bar */
    HBRUSH size_brush_;

    /* sizing bar cursor */
    HCURSOR sizing_csr_;

    /* splitter cursor */
    HCURSOR split_csr_;

    /* flag: tracking drag of the resize bar */
    unsigned int tracking_size_ : 1;

    /* flag: tracking drag of a splitter */
    unsigned int tracking_split_ : 1;

    /* tracking position - last mouse position */
    int track_x_;
    int track_y_;

    /* tracking position - start of mouse tracking */
    int track_start_x_;
    int track_start_y_;

    /* tracking size - current size of tracking */
    int track_siz_;

    /* window preceding splitter we're tracking */
    class CTadsWinDocked *track_splitwin_;

    /* model docking container */
    class CTadsDockModelCont *model_cont_;
};

/*
 *   Subclasses of the MDI docking bar for vertical and horizontal bars.
 *   A vertical bar is aligned at the left or right edge of the window; a
 *   horizontal bar is aligned at the top or bottom. 
 */
class CTadsDockportMDIBarV: public CTadsDockportMDIBar
{
public:
    CTadsDockportMDIBarV(class CTadsDockportMDI *frame, int siz,
                         tads_dock_align alignment,
                         class CTadsDockModelCont *model_cont);

    /* draw the sizing bar */
    virtual void draw_sizing_bar(HDC hdc, const RECT *bar_rc) const
    {
        /* draw the left border */
        SelectObject(hdc, hilite_pen_);
        MoveToEx(hdc, bar_rc->left, bar_rc->top, 0);
        LineTo(hdc, bar_rc->left, bar_rc->bottom);

        /* draw the right border */
        SelectObject(hdc, shadow_pen_);
        MoveToEx(hdc, bar_rc->left + 1 + MDI_WIDGET_SIZE, bar_rc->top, 0);
        LineTo(hdc, bar_rc->left + 1 + MDI_WIDGET_SIZE, bar_rc->bottom);
    }

    /* draw a splitter */
    virtual void draw_splitter(HDC hdc, int ofs) const
    {
        RECT rc;
        
        /* get my client area */
        GetClientRect(handle_, &rc);
        calc_insets(&rc, 0);

        /* draw the top border */
        SelectObject(hdc, hilite_pen_);
        MoveToEx(hdc, rc.left, rc.top + ofs, 0);
        LineTo(hdc, rc.right - 1, rc.top + ofs);

        /* draw the bottom border */
        SelectObject(hdc, dkshadow_pen_);
        MoveToEx(hdc, rc.left, rc.top + ofs + 1 + MDI_SPLITTER_SIZE, 0);
        LineTo(hdc, rc.right - 1, rc.top + ofs + 1 + MDI_SPLITTER_SIZE);
    }

    /* offset the splitter bar rectangle for tracking */
    virtual void offset_splitter_rect_tracking(RECT *rc) const
    {
        /* offset vertically */
        OffsetRect(rc, 0, track_y_ - track_start_y_);
    }

    /* resize a window based on the splitter tracking position */
    virtual void resize_for_split_track(RECT *rc_prv, RECT *rc_nxt) const
    {
        int delta;

        /* get the change */
        delta = track_y_ - track_start_y_;
        
        /* limit the change so that neither rect gets a negative height */
        if (rc_prv->bottom + delta < rc_prv->top)
            delta = -(rc_prv->bottom - rc_prv->top);
        else if (rc_nxt->bottom - delta < rc_nxt->top)
            delta = (rc_nxt->bottom - rc_nxt->top);

        /* adjust both sizes */
        rc_prv->bottom += delta;
        rc_nxt->bottom -= delta;
    }

    /* get the orientation type for my subwindows */
    virtual tads_dock_orient get_win_orientation() const
        { return TADS_DOCK_ORIENT_VERT; }

    /* allocate space to the window */
    virtual void alloc_win_share(RECT *new_win_rc,
                                 const RECT *old_win_rc,
                                 const RECT *new_bar_rc,
                                 const RECT *old_bar_rc,
                                 RECT *rem) const
    {
        int new_ht;
        
        /* start giving it the whole remaining space */
        *new_win_rc = *rem;

        /* if the new bar size is zero, there's nothing to calculate */
        if (new_bar_rc->bottom == new_bar_rc->top
            || old_bar_rc->bottom == old_bar_rc->top)
            return;

        /* allocate vertical space in proportion to its old vertical size */
        new_ht = (int)((double)(old_win_rc->bottom - old_win_rc->top)
                       / (double)(old_bar_rc->bottom - old_bar_rc->top)
                       * (double)(new_bar_rc->bottom - new_bar_rc->top)
                      + 0.5);

        /* set the new height, less the size of the splitter */
        new_win_rc->bottom = new_win_rc->top + new_ht;

        /* deduct this space from the remaining space */
        rem->top += new_ht + MDI_SPLITTER_SIZE + 2;
        rem->bottom += MDI_SPLITTER_SIZE + 2;
    }

    /* calcualte the shape of a new window */
    virtual void get_new_win_shape(RECT *new_shape, int new_cnt) const
    {
        /* get my client area */
        GetClientRect(handle_, new_shape);

        /* subtract out space for the splitters and insets */
        calc_insets(new_shape, new_cnt);

        /* 
         *   Give the new window a vertical extent equal to its fraction
         *   of the overall height 
         */
        new_shape->bottom /= new_cnt;
    }

    /* determine if a point is within this subwindow */
    virtual int pt_in_win_extent(class CTadsWinDocked *win,
                                 POINT mousepos, int *ofs,
                                 int *second_half) const;

    /* offset a rectangle by a window's extent for our orientation */
    virtual void offset_by_win_extent(RECT *rc, int ofs) const
    {
        /* offset vertically */
        OffsetRect(rc, 0, ofs);
    }

    /* increase a window's extent by the given amount */
    virtual void add_win_extent(RECT *rc, int delta) const
    {
        rc->bottom += delta;
    }

    /* subtract a window extent */
    virtual void sub_win_extent(RECT *client_rect,
                                const RECT *win_to_sub) const
    {
        /* subtract the vertical extent from the client height */
        client_rect->bottom -= (win_to_sub->bottom - win_to_sub->top);
    }

    /* set a rectangle's width or height to the splitter size */
    virtual void set_splitter_size(RECT *rc) const
    {
        rc->bottom = rc->top + MDI_SPLITTER_SIZE + 2;
    }
};

class CTadsDockportMDIBarH: public CTadsDockportMDIBar
{
public:
    CTadsDockportMDIBarH(class CTadsDockportMDI *frame, int siz,
                         tads_dock_align alignment,
                         class CTadsDockModelCont *model_cont);

    /* draw the sizing bar */
    virtual void draw_sizing_bar(HDC hdc, const RECT *bar_rc) const
    {
        /* draw the top border */
        SelectObject(hdc, hilite_pen_);
        MoveToEx(hdc, bar_rc->left, bar_rc->top, 0);
        LineTo(hdc, bar_rc->right, bar_rc->top);

        /* draw the bottom border */
        SelectObject(hdc, shadow_pen_);
        MoveToEx(hdc, bar_rc->left, bar_rc->top + 1 + MDI_WIDGET_SIZE, 0);
        LineTo(hdc, bar_rc->right, bar_rc->top + 1 + MDI_WIDGET_SIZE);
    }

    /* draw a splitter */
    virtual void draw_splitter(HDC hdc, int ofs) const
    {
        RECT rc;

        /* get my client area */
        GetClientRect(handle_, &rc);
        calc_insets(&rc, 0);

        /* draw the left border */
        SelectObject(hdc, hilite_pen_);
        MoveToEx(hdc, rc.left + ofs, rc.top, 0);
        LineTo(hdc, rc.left + ofs, rc.bottom - 1);

        /* draw the bottom border */
        SelectObject(hdc, dkshadow_pen_);
        MoveToEx(hdc, rc.left + ofs + 1 + MDI_SPLITTER_SIZE, rc.top, 0);
        LineTo(hdc, rc.left + ofs + 1 + MDI_SPLITTER_SIZE, rc.bottom - 1);
    }

    /* offset the splitter bar rectangle for tracking */
    virtual void offset_splitter_rect_tracking(RECT *rc) const
    {
        OffsetRect(rc, track_x_ - track_start_x_, 0);
    }

    /* resize a window based on the splitter tracking position */
    virtual void resize_for_split_track(RECT *rc_prv, RECT *rc_nxt) const
    {
        int delta;

        /* get the change */
        delta = track_x_ - track_start_x_;

        /* limit the change so that neither rect gets a negative width */
        if (rc_prv->right + delta < rc_prv->left)
            delta = -(rc_prv->right - rc_prv->left);
        else if (rc_nxt->right - delta < rc_nxt->left)
            delta = (rc_nxt->right - rc_nxt->left);

        /* adjust both sizes */
        rc_prv->right += delta;
        rc_nxt->right -= delta;
    }

    /* get the orientation type for my subwindows */
    virtual tads_dock_orient get_win_orientation() const
        { return TADS_DOCK_ORIENT_HORIZ; }

    /* allocate space to the window */
    virtual void alloc_win_share(RECT *new_win_rc,
                                 const RECT *old_win_rc,
                                 const RECT *new_bar_rc,
                                 const RECT *old_bar_rc,
                                 RECT *rem) const
    {
        int new_wid;

        /* start giving it the whole remaining space */
        *new_win_rc = *rem;

        /* if the new bar size is zero, there's nothing to calculate */
        if (new_bar_rc->right == new_bar_rc->left
            || old_bar_rc->right == old_bar_rc->left)
            return;

        /* allocate horizontal space in proportion to its old width */
        new_wid = (int)((double)(old_win_rc->right - old_win_rc->left)
                        / (double)(old_bar_rc->right - old_bar_rc->left)
                        * (double)(new_bar_rc->right - new_bar_rc->left)
                       + 0.5);

        /* set the new width, less the splitter size */
        new_win_rc->right = new_win_rc->left + new_wid;
        
        /* deduct this space from the remaining space */
        rem->left += new_wid + MDI_SPLITTER_SIZE + 2;
        rem->right += MDI_SPLITTER_SIZE + 2;
    }

    /* calcualte the shape of a new window */
    virtual void get_new_win_shape(RECT *new_shape, int new_cnt) const
    {
        /* get my client area */
        GetClientRect(handle_, new_shape);

        /* subtract out space for the splitters and insets */
        calc_insets(new_shape, new_cnt);

        /* 
         *   Give the new window a horizontal extent equal to its fraction
         *   of the overall width
         */
        new_shape->right /= new_cnt;
    }

    /* determine if a point is within this subwindow */
    virtual int pt_in_win_extent(class CTadsWinDocked *win,
                                 POINT mousepos, int *ofs,
                                 int *second_half) const;

    /* offset a rectangle by a window's extent for our orientation */
    virtual void offset_by_win_extent(RECT *rc, int ofs) const
    {
        /* offset horizontally */
        OffsetRect(rc, ofs, 0);
    }

    /* increase a window's extent by the given amount */
    virtual void add_win_extent(RECT *rc, int delta) const
    {
        rc->right += delta;
    }

    /* subtract a window extent */
    virtual void sub_win_extent(RECT *client_rect,
                                const RECT *win_to_sub) const
    {
        /* subtract the horizontal extent from the client width */
        client_rect->right -= (win_to_sub->right - win_to_sub->left);
    }

    /* set a rectangle's width or height to the splitter size */
    virtual void set_splitter_size(RECT *rc) const
    {
        rc->right = rc->left + MDI_SPLITTER_SIZE + 2;
    }
};

/*
 *   Subclasses of MDI docking bars for left, right, top, and bottom
 *   alignment 
 */
class CTadsDockportMDIBarTop: public CTadsDockportMDIBarH
{
public:
    CTadsDockportMDIBarTop(class CTadsDockportMDI *frame, int siz,
                           class CTadsDockModelCont *model_cont)
        : CTadsDockportMDIBarH(frame, siz, TADS_DOCK_ALIGN_TOP, model_cont)
        { }

    /* get my alignment type */
    virtual tads_dock_align get_bar_alignment() const
        { return TADS_DOCK_ALIGN_TOP; }

    /* calculate where we go based on our size and the given available area */
    virtual void calc_resize(RECT *rc) const
    {
        /* put our window at the top of the available area */
        if (rc->top + siz_ > rc->bottom - 8)
            rc->bottom -= 8;
        else
            rc->bottom = rc->top + siz_;
    }

    /* calculate insets for borders and widgets */
    virtual void calc_insets(RECT *rc, int win_cnt) const
    {
        /* sizing bar is at the bottom */
        rc->bottom -= MDI_WIDGET_SIZE + 2;

        /* subtract splitter bars */
        rc->right -= (win_cnt - 1) * (MDI_SPLITTER_SIZE + 2);
    }

    /* get the rectangle occupied by the sizing bar, given the client rect */
    virtual void get_sizing_bar_rect(RECT *rc) const
    {
        /* sizing bar is at the bottom */
        rc->top = rc->bottom - 1 - MDI_WIDGET_SIZE - 2;
    }

    /* offset the sizing bar rectangle for tracking */
    virtual void offset_sizing_bar_rect_tracking(RECT *rc) const
    {
        OffsetRect(rc, 0, track_siz_ - siz_);
    }

    /* adjust size for tracking */
    virtual void adjust_size_tracking(int delta_x, int delta_y)
    {
        /* grow as the mouse moves down */
        track_siz_ += delta_y;
    }
};

class CTadsDockportMDIBarBottom: public CTadsDockportMDIBarH
{
public:
    CTadsDockportMDIBarBottom(class CTadsDockportMDI *frame, int siz,
                              class CTadsDockModelCont *model_cont)
        : CTadsDockportMDIBarH(frame, siz, TADS_DOCK_ALIGN_BOTTOM, model_cont)
        { }

    /* get my alignment type */
    virtual tads_dock_align get_bar_alignment() const
        { return TADS_DOCK_ALIGN_BOTTOM; }

    /* calculate where we go based on our size and the given available area */
    virtual void calc_resize(RECT *rc) const
    {
        /* put our window at the bottom of the avaiable area */
        if (rc->bottom - siz_ < rc->top + 8)
            rc->top += 8;
        else
            rc->top = rc->bottom - siz_;
    }

    /* calculate insets for borders and widgets */
    virtual void calc_insets(RECT *rc, int win_cnt) const
    {
        /* sizing bar is at the top */
        rc->top += MDI_WIDGET_SIZE + 2;

        /* subtract splitter bars */
        rc->right -= (win_cnt - 1) * (MDI_SPLITTER_SIZE + 2);
    }

    /* get the rectangle occupied by the sizing bar, given the client rect */
    virtual void get_sizing_bar_rect(RECT *rc) const
    {
        /* sizing bar is at the top */
        rc->bottom = rc->top + 1 + MDI_WIDGET_SIZE + 2;
    }

    /* offset the sizing bar rectangle for tracking */
    virtual void offset_sizing_bar_rect_tracking(RECT *rc) const
    {
        OffsetRect(rc, 0, siz_ - track_siz_);
    }

    /* adjust size for tracking */
    virtual void adjust_size_tracking(int delta_x, int delta_y)
    {
        /* shrink as the mouse moves down */
        track_siz_ -= delta_y;
    }
};

class CTadsDockportMDIBarLeft: public CTadsDockportMDIBarV
{
public:
    CTadsDockportMDIBarLeft(class CTadsDockportMDI *frame, int siz,
                            class CTadsDockModelCont *model_cont)
        : CTadsDockportMDIBarV(frame, siz, TADS_DOCK_ALIGN_LEFT, model_cont)
        { }

    /* get my alignment type */
    virtual tads_dock_align get_bar_alignment() const
        { return TADS_DOCK_ALIGN_LEFT; }

    /* calculate where we go based on our size and the given available area */
    virtual void calc_resize(RECT *rc) const
    {
        /* put our window at the left of the avaiable area */
        if (rc->left + siz_ > rc->right - 8)
            rc->right -= 8;
        else
            rc->right = rc->left + siz_;
    }

    /* calculate insets for borders and widgets */
    virtual void calc_insets(RECT *rc, int win_cnt) const
    {
        /* sizing bar is at the right */
        rc->right -= MDI_WIDGET_SIZE + 2;

        /* subtract splitter bars */
        rc->bottom -= (win_cnt - 1) * (MDI_SPLITTER_SIZE + 2);
    }

    /* get the rectangle occupied by the sizing bar, given the client rect */
    virtual void get_sizing_bar_rect(RECT *rc) const
    {
        /* sizing bar is at the right */
        rc->left = rc->right - 1 - MDI_WIDGET_SIZE - 2;
    }

    /* offset the sizing bar rectangle for tracking */
    virtual void offset_sizing_bar_rect_tracking(RECT *rc) const
    {
        OffsetRect(rc, track_siz_ - siz_, 0);
    }

    /* adjust size for tracking */
    virtual void adjust_size_tracking(int delta_x, int delta_y)
    {
        /* grow as the mouse moves right */
        track_siz_ += delta_x;
    }
};

class CTadsDockportMDIBarRight: public CTadsDockportMDIBarV
{
public:
    CTadsDockportMDIBarRight(class CTadsDockportMDI *frame, int siz,
                             class CTadsDockModelCont *model_cont)
        : CTadsDockportMDIBarV(frame, siz, TADS_DOCK_ALIGN_RIGHT, model_cont)
        { }

    /* get my alignment type */
    virtual tads_dock_align get_bar_alignment() const
        { return TADS_DOCK_ALIGN_RIGHT; }

    /* calculate where we go based on our size and the given available area */
    virtual void calc_resize(RECT *rc) const
    {
        /* put our window at the right of the avaiable area */
        if (rc->right - siz_ < rc->left + 8)
            rc->left += 8;
        else
            rc->left = rc->right - siz_;
    }

    /* calculate insets for borders and widgets */
    virtual void calc_insets(RECT *rc, int win_cnt) const
    {
        /* sizing bar is at the left */
        rc->left += MDI_WIDGET_SIZE + 2;

        /* subtract splitter bars */
        rc->bottom -= (win_cnt - 1) * (MDI_SPLITTER_SIZE + 2);
    }

    /* get the rectangle occupied by the sizing bar, given the client rect */
    virtual void get_sizing_bar_rect(RECT *rc) const
    {
        /* sizing bar is at the left */
        rc->right = rc->left + 1 + MDI_WIDGET_SIZE + 2;
    }

    /* adjust size for tracking */
    virtual void adjust_size_tracking(int delta_x, int delta_y)
    {
        /* shrink as the mouse moves right */
        track_siz_ -= delta_x;
    }

    /* offset the sizing bar rectangle for tracking */
    virtual void offset_sizing_bar_rect_tracking(RECT *rc) const
    {
        OffsetRect(rc, siz_ - track_siz_, 0);
    }

};

/* ------------------------------------------------------------------------ */
/*
 *   Docking Layout Model.
 *   
 *   In parallel with the actual docked windows, we maintain a "model" of
 *   the docking layout.  The difference between the actual windows and
 *   the model is that the model always contains each dockable window,
 *   whether or not it's currently docked.  This allows us to restore a
 *   dockable window at its previous docked location.
 */

/*
 *   Model dockable window.  Each actual window has a corresponding
 *   associated model window.  
 */
class CTadsDockModelWin
{
public:
    CTadsDockModelWin(const char *guid)
    {
        /* initialize members */
        init(guid);

        /* docked and undocked data are unknown */
        dock_width_ = 200;
        dock_height_ = 300;
        SetRect(&undock_pos_, 100, 100, 200, 300);
        dock_align_ = TADS_DOCK_ALIGN_BOTTOM;
        start_docked_ = FALSE;
        start_dockable_ = TRUE;
    }

    CTadsDockModelWin(int dock_width, int dock_height,
                      tads_dock_align dock_align, const RECT *undock_pos,
                      int start_docked, int start_dockable, const char *guid)
    {
        /* initialize members */
        init(guid);

        /* save window configuration */
        dock_width_ = dock_width;
        dock_height_ = dock_height;
        dock_align_ = dock_align;
        undock_pos_ = *undock_pos;
        start_docked_ = start_docked;
        start_dockable_ = start_dockable;
    }
    
    void init(const char *guid)
    {
        /* not in a list yet */
        next_ = 0;

        /* no container yet */
        cont_ = 0;

        /* no references yet */
        refcnt_ = 0;

        /* no associated real window yet */
        win_ = 0;

        /* we don't have a valid serialization ID yet */
        serial_id_ = -1;

        /* set the GUID if we got one */
        if (guid != 0)
            guid_.set(guid);
    }
    
    /* delete */
    ~CTadsDockModelWin();

    /* add a reference */
    void add_ref() { ++refcnt_; }

    /* remove a reference */
    void remove_ref()
    {
        /* decrement the counter */
        --refcnt_;

        /* if that leaves no references, delete me */
        if (refcnt_ == 0)
            delete this;
    }

    /* get my container */
    class CTadsDockModelCont *get_container() const { return cont_; }

    /* set a new container */
    void set_container(class CTadsDockModelCont *cont) { cont_ = cont; }

    /* forget about my container */
    void forget_container() { cont_ = 0; }

    /* forget my associated window */
    void forget_window();

    /* remove the window from my container */
    void remove_from_container(int allow_container_delete);

    /* next window in same docking container */
    CTadsDockModelWin *next_;

    /* my actual window */
    class CTadsWinDock *win_;

    /* current docking container */
    class CTadsDockModelCont *cont_;

    /* our reference count */
    int refcnt_;

    /* my serialization ID */
    int serial_id_;

    /* 
     *   docked position - this is used as a template if we must create a
     *   real window from the model; this information comes from a saved
     *   configuration 
     */
    int dock_width_;
    int dock_height_;
    tads_dock_align dock_align_;

    /* undocked position */
    RECT undock_pos_;

    /* global identifier for the window */
    CStringBuf guid_;

    /* initial docking configuration from load file */
    int start_docked_ : 1;
    int start_dockable_ : 1;
};

/*
 *   Model docking container.  Each actual docking container has a
 *   corresponding associated model docking container.
 */
class CTadsDockModelCont
{
public:
    CTadsDockModelCont(class CTadsDockModelFrame *frame,
                       tads_dock_align align);
    
    ~CTadsDockModelCont();

    /* forget about my frame */
    void forget_frame() { frame_ = 0; }

    /*
     *   Prepare for serialization by assigning an ID to myself and to my
     *   windows.  We will take (*id) for the container ID, then assign
     *   the next N values to our N windows.  On return, (*id) will
     *   contain the next available ID number.  
     */
    void prep_for_serialization(int *id);

    /* next container */
    CTadsDockModelCont *next_;

    /* model frame */
    class CTadsDockModelFrame *frame_;

    /* alignment of this container */
    tads_dock_align align_;

    /* head of list of model windows inside this container */
    class CTadsDockModelWin *first_win_;

    /* 
     *   Add a model window to my list before the given model window.  If
     *   before_win is null, the new window goes at the end of the list. 
     */
    void add_win_before(class CTadsDockModelWin *win_to_add,
                        class CTadsDockModelWin *before_win);

    /* 
     *   Find and remove a model window from my list.  If this leaves
     *   nothing in my list, delete this container.  Returns true if the
     *   window was found in my list, false if not.  
     */
    int remove_win(class CTadsDockModelWin *win, int allow_container_delete);

    /* my serialization ID */
    int serial_id_;
};

/*
 *   Model Frame.  A frame contains docking containers, which in turn
 *   contains windows.  
 */
class CTadsDockModelFrame
{
public:
    CTadsDockModelFrame()
    {
        /* no docking containers yet */
        first_cont_ = 0;
    }
    ~CTadsDockModelFrame();

    /*
     *   Serialize.  We'll invoke the given callback to save information
     *   on each container and window.  We'll provide the information as a
     *   simple null-terminated string value; the callback should treat
     *   this value as opaque and simply store it in its format of choice.
     *   
     *   The 'id' value is an index; it'll start at zero and increment by
     *   one on each call.  The caller must store the ID along with the
     *   string, so that the ID's can be used during the re-loading
     *   process to identify the strings.  
     */
    void serialize(void (*cbfunc)(void *cbctx, int id, const char *info),
                   void *cbctx);

    /*
     *   Given a window handle, get the serialization ID for the
     *   containing docking window.  This can only be used after a call to
     *   serialize() with no intervening docking window creation or
     *   deletion, because serial numbers are assigned by serialize() and
     *   do not remain valid after creation or deletion of a docking
     *   window.  This ID number can be used after loading a docking
     *   configuration to obtain the docking container for the saved
     *   window.
     *   
     *   Returns -1 if the window doesn't exist in our model.  
     */
    int find_window_serial_id(HWND child_hwnd);

    /*
     *   Load from a previously serialized configuration.  The callback
     *   must be able to load the strings previously saved by serialize();
     *   the given number of strings will be loaded.  The callback should
     *   return zero on success, non-zero on error.  
     */
    void load(int (*cbfunc)(void *cbctx, int id, char *buf, size_t buflen),
              void *cbctx, int string_count);

    /*
     *   Given a GUID, find a docking window.  This can be used after a call
     *   to load() to find a given loaded window.  Returns null if no such
     *   window is present.  
     */
    class CTadsWinDock
        *find_window_by_guid(class CTadsWin *dockable_window_parent,
                             const char *guid);

    /*
     *   Assign a GUID to a window identified by docking model serial number.
     *   This can be used after a call to load() if the stored configuration
     *   uses the old format, which keeps a separate list associating GUID's
     *   with model serial numbers.  The new model simply stores the GUID
     *   with each window in the model, so there's no need to call this with
     *   new-format data.  The purpose of this routine is to update
     *   old-format data to properly tie serial numbers to windows.  
     */
    void assign_guid(int serial_id, const char *guid);

    /* head of container list */
    class CTadsDockModelCont *first_cont_;

    /* add a container at the end of the list */
    void append_container(class CTadsDockModelCont *cont);

    /* remove a container */
    void remove_container(class CTadsDockModelCont *cont);

protected:
    /* 
     *   Prepare for serialization - assign ID's to all of our elements.
     *   Each model container will receive a unique identifying number:
     *   containers will be numbered with consecutive integers from 0.
     *   Likewise, each model window will be numbered with a unique
     *   integer starting at 0.  
     */
    void prep_for_serialization();

    /*
     *   Clear our model, in preparation for loading from a saved state
     *   file.  
     */
    void clear_model();

    /* find a container by serialization ID */
    class CTadsDockModelCont *find_cont_by_id(int id);
};

#endif /* TADSDOCK_H */

