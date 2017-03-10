/* $Header: d:/cvsroot/tads/html/win32/w32expr.h,v 1.3 1999/07/11 00:46:51 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32expr.h - TADS debugger for 32-bit windows - expression window
Function
  
Notes
  
Modified
  02/17/98 MJRoberts  - Creation
*/

#ifndef W32EXPR_H
#define W32EXPR_H

#include <Windows.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSWIN_H
#include "tadswin.h"
#endif
#ifndef W32TDB_H
#include "w32tdb.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Watch window.  This is a simple frame window that holds an expression
 *   window as its contents. 
 */
class CHtmlDbgWatchWin: public CTadsWin, public CHtmlDbgSubWin
{
public:
    CHtmlDbgWatchWin();
    ~CHtmlDbgWatchWin();

    /* activate or deactivate */
    int do_ncactivate(int flag);

    /* parent activation */
    void do_parent_activate(int active);

    /* 
     *   Initialize the panels for the window.  This routine must be
     *   called immediately after the object is constructed. 
     */
    virtual void init_panels(class CHtmlSys_dbgmain *mainwin,
                             class CHtmlDebugHelper *helper);

    /* update all expressions */
    void update_all();

    /* clear a configuration of my data */
    void clear_config(class CHtmlDebugConfig *config,
                      const textchar_t *varname) const;

    /* 
     *   save my configuration - this should save both my window
     *   configuration and any savable contents of my panels 
     */
    void save_config(class CHtmlDebugConfig *config,
                     const textchar_t *varname);

    /* load my window configuration (does not load panel configuration) */
    void load_win_config(class CHtmlDebugConfig *config,
                         const textchar_t *varname,
                         RECT *pos, int *vis);

    /* load my panel configuration */
    void load_panel_config(class CHtmlDebugConfig *config,
                           const textchar_t *varname);

    /* add a new expression to the current panel */
    void add_expr(const textchar_t *expr, int can_edit_expr,
                  int select_new_item);

    /* -------------------------------------------------------------------- */
    /*
     *   CHtmlDbgSubWin implementation 
     */
    
    /* check the status of a command routed from the main window */
    virtual TadsCmdStat_t active_check_command(int command_id);

    /* execute a command routed from the main window */
    virtual int active_do_command(int notify_code, int command_id,
                                  HWND ctl);

    /* get my window handle */
    virtual HWND active_get_handle() { return handle_; }

    /* get the selection range - we don't have one */
    virtual int get_sel_range(class CHtmlFormatter **formatter,
                              unsigned long *sel_start,
                              unsigned long *sel_end)
    {
        return FALSE;
    }

protected:
    /* 
     *   flag indicating whether this window can be the debugger's main
     *   active window; by default, an expression window can be active 
     */
    virtual int can_be_main_active() const { return TRUE; }

    /* allocate space in the panel list array */
    void alloc_panel_list(size_t cnt);

    /* select a new active panel */
    void select_new_tab(int tab_id);
    
    /* we're a child window */
    DWORD get_winstyle()
    {
        return (WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
    }
    DWORD get_winstyle_ex() { return 0; }

    /* resize */
    void do_resize(int mode, int x, int y);
    
    /* process system window creation */
    void do_create();

    /* create a sizebox */
    void create_sizebox();

    /* process window destruction */
    void do_destroy();

    /* close the window */
    int do_close();

    /* gain focus - move focus to the subwindow */
    void do_setfocus(HWND prev_win);

    /* process a notification message */
    int do_notify(int control_id, int notify_code, LPNMHDR nmhdr);

    /* the main debugger window */
    class CHtmlSys_dbgmain *dbgmain_;

    /* the current expression subwindow */
    class CHtmlDbgExprWin *exprwin_;

    /* 
     *   array of expression subwindows, if we have multiple ones;
     *   subclasses can leave this null if there's only one subwindow 
     */
    class CHtmlDbgExprWin **exprwin_list_;
    size_t exprwin_list_cnt_;

    /* insets of expression subwindow relative to our frame */
    RECT insets_;

    /* tab control for bottom of window, if we have one */
    class CTadsTabctl *tabctl_;
    HWND sizebox_;
    HWND graybox_;
    HWND expr_hscroll_;
};


/*
 *   Subclass of the expression window that shows the local variables 
 */
class CHtmlDbgWatchWinLocals: public CHtmlDbgWatchWin
{
public:
    /* initialize panels */
    void init_panels(class CHtmlSys_dbgmain *mainwin,
                     class CHtmlDebugHelper *helper);

protected:
    /* process system window creation */
    void do_create();
};


/*
 *   Subclass of the expression window that shows a user-editable set of
 *   watch expressions 
 */
class CHtmlDbgWatchWinExpr: public CHtmlDbgWatchWin
{
public:
    /* initialize panels */
    void init_panels(class CHtmlSys_dbgmain *mainwin,
                     class CHtmlDebugHelper *helper);

protected:
    /* process system window creation */
    void do_create();
};


/*
 *   Subclass of the expression window that allows a single user-editable
 *   expression.  This version is meant to be used in the modal dialog for
 *   evaluating an expression. 
 */
class CHtmlDbgWatchWinDlg: public CHtmlDbgWatchWin
{
public:
    /* run the dialog */
    static void run_as_dialog(class CHtmlSys_dbgmain *mainwin,
                              class CHtmlDebugHelper *helper,
                              const textchar_t *init_expr);
    
    /* initialize panels */
    void init_panels(class CHtmlSys_dbgmain *mainwin,
                     class CHtmlDebugHelper *helper);

    /* close the window */
    int do_close();

    /* paint the window */
    void do_paint_content(HDC hdc, const RECT *area);

    /* handle a character message */
    int do_char(TCHAR ch, long keydata);

    /* handle a keystroke message */
    int do_keydown(int vkey, long keydata);

    /* handle a command */
    int do_command(int notify_code, int command_id, HWND ctl);

protected:
    /* a dialog window cannot be the active debugger window */
    virtual int can_be_main_active() const { return FALSE; }
    
    /* create the window */
    void do_create();

    /* resize */
    void do_resize(int mode, int x, int y);
        
    /* leave off the min and max boxes and the size box */
    DWORD get_winstyle()
    {
        return (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME
                | WS_BORDER | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
    }

    /* make it a dialog frame */
    DWORD get_winstyle_ex() { return WS_EX_DLGMODALFRAME; }

    /* flag indicating that the window is being dismissed */
    int closing_;

    /* handle of "Close" button */
    HWND closebtn_;
};


/* ------------------------------------------------------------------------ */
/*
 *   enumeration for mouse tracking - identifies the part of the window
 *   that we're clicking on 
 */
enum htmlexpr_winpart_t
{
    HTMLEXPR_WINPART_HDR_EXPR,            /* the "expression" column header */
    HTMLEXPR_WINPART_HDR_VAL,                  /* the "value" column header */
    HTMLEXPR_WINPART_HDR_SEP,            /* the separator bar in the header */
    HTMLEXPR_WINPART_EXPR,                     /* an item's expression part */
    HTMLEXPR_WINPART_SEP,               /* the separator bar within an item */
    HTMLEXPR_WINPART_VAL,                           /* an item's value part */
    HTMLEXPR_WINPART_OPENCLOSE,                 /* an item's open/close box */
    HTMLEXPR_WINPART_EXPRMARGIN,    /* blank portion in a margin of an item */
    HTMLEXPR_WINPART_BLANK        /* empty portion at the end of the window */
};

/* ------------------------------------------------------------------------ */
/*
 *   Expression window class.  This class is meant to be used as a child
 *   window (essentially as a control) embedded within a debugger window.  
 */
class CHtmlDbgExprWin: public CTadsWinScroll
{
public:
    CHtmlDbgExprWin(int has_vscroll, int has_hscroll, int has_blank_entry,
                    int single_entry,
                    class CHtmlSys_dbgmain *mainwin,
                    class CHtmlDebugHelper *helper);
    ~CHtmlDbgExprWin();

    /* get the main debugger window */
    CHtmlSys_dbgmain *get_mainwin() const { return mainwin_; }

    /* update all expressions */
    virtual void update_all();

    /* get the font for the list items */
    class CTadsFont *get_list_font() const { return list_font_; }

    /* get the appropriate icon for drawing a hierarchical level */
    HICON get_openclose_icon(int is_open) const
        { return is_open ? ico_minus_ : ico_plus_; }

    /* get the vertical and horizontal dotted line icons */
    HBITMAP get_vdots_bmp() const { return bmp_vdots_; }
    HBITMAP get_hdots_bmp() const { return bmp_hdots_; }

    /* check a command's status */
    TadsCmdStat_t check_command(int command_id);

    /* handle a command */
    int do_command(int notify_code, int command_id, HWND ctl);

    /* 
     *   update evaluation of an expression - reads the expression text,
     *   and sets the item's value text accordingly 
     */
    void update_eval(class CHtmlDbgExpr *expr);

    /* change an expression's value */
    void change_expr_value(class CHtmlDbgExpr *expr,
                           const textchar_t *newval);

    /* 
     *   delete an expression from our list, including all children;
     *   returns the next expression remaining in the list after the
     *   deleted expression 
     */
    class CHtmlDbgExpr *delete_expr(class CHtmlDbgExpr *expr);

    /*
     *   Begin updating a child list of a given item.  If the parent is
     *   null, we'll update the root items.  In this routine, we'll mark
     *   all children of the given parent as pending refresh; during
     *   refresh, we'll mark children as updated as they're added back
     *   into the list.  When the refresh is finished, we'll remove any
     *   children that weren't refreshed, since these are no longer in the
     *   parent's list.  
     */
    void begin_refresh(class CHtmlDbgExpr *parent);

    /*
     *   Add or replace an expression to the window.  The item is added as
     *   a child of the given parent; if the parent is null, the item is
     *   added at the top level.  If the expression is already in the
     *   child list of the parent item, we'll simply update the value of
     *   the item.  
     */
    void do_refresh(class CHtmlDbgExpr *parent, const textchar_t *expr,
                    size_t exprlen, const textchar_t *relationship);

    /*
     *   End a refresh operation.  Any children of the parent that haven't
     *   been refreshed are deleted at this point, since they're evidently
     *   no longer members of the parent item.  
     */
    void end_refresh(class CHtmlDbgExpr *parent);

    /* clear a configuration of my data */
    void clear_config(class CHtmlDebugConfig *config,
                      const textchar_t *varname, size_t panel_num) const;

    /* save/load my configuration */
    virtual void save_config(class CHtmlDebugConfig *config,
                             const textchar_t *varname, size_t panel_num);
    virtual void load_config(class CHtmlDebugConfig *config,
                             const textchar_t *varname, size_t panel_num);

    /* close the entry field, if we have one */
    void close_entry_field(int accept_new_value);

    /* 
     *   Add a new expression at the end of our list (or just before the
     *   blank entry item, if we have one).  If select_new_item is true,
     *   we'll select new item and scroll it into view, otherwise we'll
     *   just add it without changing the selection.  
     */
    void add_expr(const textchar_t *expr, int can_edit_expr,
                  int select_new_item);

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

    /* process system window creation */
    void do_create();

    /* paint the window's contents */
    void do_paint_content(HDC hdc, const RECT *area_to_draw);

    /* always show the scrollbars */
    int vscroll_is_visible() const { return TRUE; }
    int hscroll_is_visible() const { return FALSE; }

    /* adjust the scrollbar ranges */
    void adjust_scrollbar_ranges();

    /* use vscroll units of 1 line */
    long get_vscroll_units() const { return vscroll_units_; }

    /* receive notification that scrolling is about to occur */
    void notify_pre_scroll(HWND hwnd);

    /* receive notification that scrolling has occurred */
    void notify_scroll(HWND hwnd, long oldpos, long newpos);

    /* get the scrolling area */
    void get_scroll_area(RECT *rc, int vertical) const;

    /* scroll the focus expression into view */
    void scroll_focus_into_view();

    /* handle mouse events */
    int do_leftbtn_down(int keys, int x, int y, int clicks);
    int do_leftbtn_up(int keys, int x, int y);
    int do_mousemove(int keys, int x, int y);
    int do_rightbtn_down(int keys, int x, int y, int clicks);

    /* handle capture change */
    int do_capture_changed(HWND new_capture_win);

    /* set the cursor type */
    int do_setcursor(HWND win, int hittest, int mousemsg);

    /* resize the window */
    void do_resize(int mode, int x, int y);

    /* gain/lose focus */
    void do_setfocus(HWND prev_win);
    void do_killfocus(HWND next_win);

    /* handle keystrokes */
    int do_char(TCHAR ch, long keydata);
    int do_keydown(int vkey, long keydata);

    /* 
     *   handle "system" keystrokes (with the ALT key down) - we want to
     *   pass these on to the main debug window, so that the main debug
     *   window's menus can be accessed with the keyboard while this
     *   window has focus 
     */
    int do_syskeydown(int vkey, long keydata);

    /* process a user message */
    int do_user_message(int msg, WPARAM wpar, LPARAM lpar);

    /* 
     *   Determine what part of the window contains a given point; the
     *   point is in window coordinates.  Returns the window part
     *   identifier, and fills in *expr with the expression item
     *   containing the point, if the point is within an expression item.
     *   expr returns null if the point is not within an expression item.
     *   
     *   If text_area is not null, and the result is an expression item
     *   part with a text area, we'll fill in *text_area with the bounds
     *   (in window coordinates) of the item's text on the screen.  
     */
    htmlexpr_winpart_t pos_to_item(int x, int y, class CHtmlDbgExpr **expr,
                                   CHtmlRect *text_area);

    /*
     *   Calculate the part and screen area of a given x position in a
     *   given item.  Fills in *text_area as per pos_to_item(). 
     */
    htmlexpr_winpart_t pos_to_area(class CHtmlDbgExpr *expr, int xpos,
                                   CHtmlRect *text_area);

    /* 
     *   Set the selected item, removing any previous selection.  If
     *   set_focus is true, we'll move the focus into the new item, if
     *   possible. 
     */
    void set_selection(class CHtmlDbgExpr *expr, int set_focus,
                       htmlexpr_winpart_t focus_part);

    /* invalidate the screen area of an item */
    void inval_expr(class CHtmlDbgExpr *expr);

    /* draw or undraw the tracking separator bar */
    void draw_track_sep(int add);

    /* end separator tracking */
    void end_tracking_sep(int release_capture);

    /* open a new entry field */
    void open_entry_field(const class CHtmlRect *text_area,
                          class CHtmlDbgExpr *expr, htmlexpr_winpart_t part);

    /* handle focus loss in the entry field */
    void on_entry_field_killfocus();

    /* 
     *   subclassed window procedure for our text field - static version
     *   for passing to windows, and non-static version that we invoke
     *   after figuring out our 'this' pointer 
     */
    static LRESULT CALLBACK fld_proc(HWND hwnd, UINT msg,
                                     WPARAM wpar, LPARAM lpar);
    LRESULT do_fld_proc(HWND hwnd, UINT msg, WPARAM wpar, LPARAM lpar);

    /* set the internal focus */
    void set_focus_expr(class CHtmlDbgExpr *expr, htmlexpr_winpart_t part);

    /* draw or remove the current focus rectangle */
    void draw_focus(int draw);

    /* 
     *   Get the text area of the focus in window coordinates.  If
     *   on_screen is true, we'll clip the area so that it fits within the
     *   window, otherwise we'll allow the area to overhang the window
     *   boundaries. 
     */
    void get_focus_text_area(CHtmlRect *text_area, int on_screen);

    /* determine if we can delete the current focus item */
    int can_delete_focus() const;

    /*
     *   Move to the next focus position.  If allow_same is true, we'll
     *   keep focus in the same item if we wrap back to this item because
     *   there's nothing else that can have the focus, or because there's
     *   another field in the same item that will take focus.  If
     *   allow_same is false, we'll never return with focus set to this
     *   same item; this can be used prior to deleting an item to ensure
     *   it doesn't retain focus.  If 'backwards' is true, we'll go to the
     *   previous focus position, otherwise to the next position.  
     */
    void go_next_focus(int allow_same, int backwards);

    /* callback for building a child list of an item */
    static void update_eval_child_cb(void *ctx, const char *subitemname,
                                     int subitemnamelen,
                                     const char *relationship);

    /* 
     *   fix positions of children of a given item, to adjust for
     *   insertions or deletions from the child list 
     */
    void fix_positions(CHtmlDbgExpr *parent);

    /* receive notification that dragging is starting */
    void drag_pre();

    /* get the drag data object */
    IDataObject *get_drag_dataobj();

    /* get the drag effect - allow only copying out of this window */
    ULONG get_drag_effects() { return DROPEFFECT_COPY; }

    /* main debugger window and helper object */
    class CHtmlSys_dbgmain *mainwin_;
    class CHtmlDebugHelper *helper_;

    /* splitter cursor */
    HCURSOR split_ew_cursor_;

    /* box-plus/box-minus icons (for open/close expression item) */
    HICON ico_plus_;
    HICON ico_minus_;

    /* vertical and horizontal dotted line icons */
    HBITMAP bmp_vdots_;
    HBITMAP bmp_hdots_;

    /* list of expression objects */
    class CHtmlDbgExpr *first_expr_;
    class CHtmlDbgExpr *last_expr_;

    /* the selected item */
    class CHtmlDbgExpr *selection_;

    /* 
     *   x offset from left edge of document of expression/value separator
     *   bar 
     */
    long sep_xpos_;

    /* 
     *   Flag indicating whether we keep around an empty expression item
     *   at the end of our list for entering new expressions.  If this is
     *   true, we'll keep around an empty item at all times.  
     */
    int has_blank_entry_ : 1;

    /*
     *   Flag indicating that the window has only a single top-level
     *   entry.  If this is set, has_blank_entry_ should also be set.  
     */
    int single_entry_ : 1;

    /* height of the header line */
    int hdr_height_;

    /* fonts for the header and list items */
    class CTadsFont *hdr_font_;
    class CTadsFont *list_font_;

    /* tracking information for moving the separator line */
    int tracking_sep_ : 1;
    int track_start_x_;
    int track_sep_x_;

    /* 
     *   tracking information for opening an editor field - we'll set up
     *   to open a field on mouse-down, and then open it on mouse-up
     *   unless something else happened (such as a drop/drop operation) 
     */
    class CHtmlDbgExpr *pending_open_expr_;
    htmlexpr_winpart_t pending_open_part_;

    /* the active data entry field */
    HWND entry_fld_;

    /* 
     *   flag indicating whether we're accepting changes from the entry
     *   field; if this is false, we'll ignore any changes made in the
     *   field when we close the field 
     */
    int accept_entry_update_ : 1;

    /* 
     *   original window procedure of entry field (we hook this so that we
     *   can intercept certain messages) 
     */
    WNDPROC entry_fld_defproc_;

    /* the expression being edited with the active field */
    class CHtmlDbgExpr *entry_expr_;
    htmlexpr_winpart_t entry_part_;

    /* 
     *   focus position - we retain a sense of internal focus, on either
     *   an expression or a value, even when we don't have an editing
     *   field open 
     */
    class CHtmlDbgExpr *focus_expr_;
    htmlexpr_winpart_t focus_part_;

    /* flag indicating that the window has focus */
    int have_focus_ : 1;

    /* flag indicating that the focus rectangle has been drawn */
    int drawn_focus_ : 1;

    /* units for vertical scrolling - scroll by height of a line */
    int vscroll_units_;

    /* context menu, and the submenu that we use for the context popup */
    HMENU ctx_menu_;
    HMENU expr_popup_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Subclass of the expression panel for local variables.  This window
 *   updates itself by replacing its list with a list of the local
 *   variables in the current stack frame.  
 */
class CHtmlDbgExprWinLocals: public CHtmlDbgExprWin
{
public:
    CHtmlDbgExprWinLocals(int has_vscroll, int has_hscroll,
                          class CHtmlSys_dbgmain *mainwin,
                          class CHtmlDebugHelper *helper)
        : CHtmlDbgExprWin(has_vscroll, has_hscroll, FALSE, FALSE,
                          mainwin, helper)
    {
    }

    /* update all expressions */
    void update_all();

    /* save/load my configuration - locals don't save any config info */
    virtual void save_config(class CHtmlDebugConfig *,
                             const textchar_t *, size_t) { }
    virtual void load_config(class CHtmlDebugConfig *,
                             const textchar_t *, size_t) { }

private:
    /* callback for update the local variable list */
    static void enum_locals_cb(void *ctx, const textchar_t *lclnam,
                               size_t lclnamlen);
};

/* ------------------------------------------------------------------------ */
/*
 *   Expression window subclass for 'self' display 
 */
class CHtmlDbgExprWinSelf: public CHtmlDbgExprWin
{
public:
    CHtmlDbgExprWinSelf(int has_vscroll, int has_hscroll,
                        class CHtmlSys_dbgmain *mainwin,
                        class CHtmlDebugHelper *helper)
        : CHtmlDbgExprWin(has_vscroll, has_hscroll, FALSE, FALSE,
                          mainwin, helper)
    {
    }
    
    /* update all expressions */
    void update_all();
    
    /* save/load my configuration - 'self' doesn't save any config info */
    virtual void save_config(class CHtmlDebugConfig *,
                             const textchar_t *, size_t) { }
    virtual void load_config(class CHtmlDebugConfig *,
                             const textchar_t *, size_t) { }
};


    
/* ------------------------------------------------------------------------ */
/*
 *   Debugger expression item.  The expression window keeps a list of
 *   these items; each item contains an expression, and each has a
 *   hierarchy level, which allows expressions to be displayed as a tree
 *   (for example, an object value can have a set of subnodes for the
 *   properties of the object, and each property value can in turn be an
 *   object with a property sublist of its own, and so on).  
 */

/* indent amount per hierarchy level */
const int HTMLDBGEXPR_LEVEL_INDENT = 10;

/* offset of text from indentation point (room for the open/close box) */
const int HTMLDBGEXPR_TEXT_OFFSET = 13;

class CHtmlDbgExpr
{
    friend class CHtmlDbgExprWin;
    
public:
    CHtmlDbgExpr(int indent_level, const textchar_t *expr, size_t exprlen,
                 CHtmlDbgExpr *parent, const textchar_t *parent_relationship,
                 const textchar_t *val, size_t vallen,
                 int is_openable, int can_edit_expr, long ypos,
                 class CHtmlDbgExprWin *win);
    ~CHtmlDbgExpr();

    /* determine if this item is the last at its level in the hierarchy */
    int is_last_in_level() const;

    /* get my parent item */
    CHtmlDbgExpr *get_parent() const;

    /* get/set my expression */
    const textchar_t *get_expr() const { return expr_.get(); }
    void set_expr(const textchar_t *expr) { expr_.set(expr); }

    /* get my full expression text */
    const textchar_t *get_full_expr() const { return fullexpr_.get(); }

    /* determine if the expression is empty */
    int is_expr_empty() const;

    /* 
     *   Update my evaluation.  This will evaluate the expression and set
     *   the new value, and will then adjust children as needed; if the
     *   item was open before the evaluation and can still be open
     *   afterwards, we'll recurse into each child and evaluate it (so
     *   it's not necessary for the caller to re-evaluate any of my
     *   children or their children). 
     */
    void update_eval(class CHtmlDbgExprWin *win);

    /* get/set my value */
    const textchar_t *get_val() const { return val_.get(); }
    void set_val(const textchar_t *val) { val_.set(val); }
    void set_val(const textchar_t *val, size_t len) { val_.set(val, len); }

    /* change my value editability */
    void set_can_edit_val(int can_edit) { can_edit_val_ = can_edit; }

    /* get/set my openable status */
    int is_openable() const { return is_openable_; }
    void set_openable(int openable) { is_openable_ = openable; }

    /* get/set my open/closed status */
    int is_open() const { return is_open_; }
    void set_open(int open) { is_open_ = open; }

    /* 
     *   Get/set my y position.  The containing window is responsible for
     *   updating our position whenever the list changes in such a way as
     *   to affect this item (for example, when another item is inserted
     *   before this item in the list).  
     */
    long get_y_pos() const { return y_; }
    void set_y_pos(long ypos) { y_ = ypos; }

    /* get my height */
    int get_height() const { return height_; }

    /* 
     *   Get my indent level.  Note that the root is at level 1. 
     */
    int get_level() const { return level_; }

    /* get the next/previous expression in the list */
    CHtmlDbgExpr *get_next_expr() const { return next_; }
    CHtmlDbgExpr *get_prev_expr() const { return prev_; }

    /* 
     *   Given an x position (in document coordinates) within the
     *   expression, figure out which part of the expression contains the
     *   item; returns an appropriate part code.  *xl and *xr on entry
     *   should be the left and right bounds of the window in document
     *   coordinates; these are adjusted if necessary to the bounds of the
     *   text area that we find, if we find a text area at all.  
     */
    htmlexpr_winpart_t pos_to_area(int x, int sep_xpos, long *xl, long *xr);

    /*
     *   Given a text area part (i.e., value or expression), fill in the
     *   horizontal bounds of the part's text area.  *xl and *xr on entry
     *   should be the left and right bounds of the window in document
     *   coordiantes; these are adjusted if necessary to the bounds of the
     *   given text area.  If on_screen is true, we'll leave these limited
     *   to the available screen area; otherwise, we'll let them go off
     *   the edges of the screen.  
     */
    void part_to_area(htmlexpr_winpart_t part, long *xl, long *xr,
                      int sep_xpos, int on_screen);

    /* draw the item */
    void draw(HDC hdc, CHtmlDbgExprWin *win, const RECT *area_to_draw,
              long sep_xpos, int hdr_height_, int is_selected);

    /* determine if we can edit a part of this expression */
    int can_edit_part(htmlexpr_winpart_t part) const;

    /*
     *   Receive notification that we're opening an entry field on the
     *   expression.  We'll fill in the value in the entry field from our
     *   value.  
     */
    void on_open_entry_field(HWND fldctl, htmlexpr_winpart_t part);

    /*
     *   Receive notification that the entry field for this item is being
     *   closed.  We'll update the value from the entry field. 
     */
    void on_close_entry_field(class CHtmlDbgExprWin *win,
                              HWND fldctl, htmlexpr_winpart_t part,
                              int accept_changes);

    /*
     *   Accept new expression text.  If update_val is true, we'll
     *   evaluate the new expression, otherwise we'll just remember it for
     *   future evaluation.  
     */
    void accept_expr(class CHtmlDbgExprWin *win, const textchar_t *buf,
                     int delete_on_empty);

    /*
     *   Open the item if it's closed, or close it if it's open 
     */
    void invert_open_state(class CHtmlDbgExprWin *win);

    /*
     *   Get/set refresh pending.  When we refresh an item's child list,
     *   we'll use this flag to determine if the item still exists after
     *   the update; any item that is still pending refresh after
     *   refreshing all current children of a given item is no longer
     *   present in the child list, and hence can be deleted.  
     */
    int is_pending_refresh() const { return pending_refresh_; }
    void set_pending_refresh(int flag) { pending_refresh_ = flag; }

protected:
    /*
     *   Build my full expression, given the parent object 
     */
    void build_full_expr(CHtmlDbgExpr *parent);

    /* display text of the expression, and text of its current value */
    CStringBuf expr_;
    CStringBuf val_;

    /* operator relating my expression to the parent */
    CStringBuf parent_relationship_;

    /* full text of the expression, including parent string */
    CStringBuf fullexpr_;

    /* next/previous items in my list */
    CHtmlDbgExpr *next_;
    CHtmlDbgExpr *prev_;

    /* 
     *   flag indicating whether the expression is an lvalue; if so, the
     *   value can be edited, otherwise the value is read-only 
     */
    unsigned int is_lval_ : 1;

    /* flag indicating whether I have a sublist at all */
    unsigned int is_openable_ : 1;

    /* flag indicating whether my sublist is showing */
    unsigned int is_open_ : 1;

    /* hierarchy level (0 is the root) */
    int level_;

    /* y position (document coordinates) and height */
    long y_;
    int height_;

    /* flags indicating whether the individual parts are editable */
    unsigned int can_edit_expr_ : 1;
    unsigned int can_edit_val_ : 1;

    /* flag: this item is pending refresh */
    unsigned int pending_refresh_ : 1;
};

#endif /* W32EXPR_H */

