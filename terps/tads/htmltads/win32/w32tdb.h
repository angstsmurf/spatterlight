/* $Header: d:/cvsroot/tads/html/win32/w32tdb.h,v 1.4 1999/07/11 00:46:52 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32tdb.h - html tads win32 debugger
Function
  
Notes
  
Modified
  01/31/98 MJRoberts  - Creation
*/

#ifndef W32TDB_H
#define W32TDB_H

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSWIN_H
#include "tadswin.h"
#endif
#ifndef HTMLRES_H
#include "htmlres.h"
#endif
#ifndef HTMLW32_H
#include "htmlw32.h"
#endif
#ifndef HTMLDBG_H
#include "htmldbg.h"
#endif
#ifndef W32DBWIZ_H
#include "w32dbwiz.h"
#endif
#ifndef TADSSTAT_H
#include "tadsstat.h"
#endif



/* ------------------------------------------------------------------------ */
/*
 *   MDI/SDI selector.  This block of code encapsulates all of the
 *   differences between running the debugger as an SDI application vs. as
 *   an MDI application.
 *   
 *   This is currently set up so that the MDI/SDI selection is made at
 *   compile time.  The default is MDI.  Define W32TDB_SDI on the compiler
 *   command line to compile an SDI version of the debugger.
 *   
 *   It would be possible to make the MDI/SDI selection at run-time, by
 *   changing the code below so that the MDI and SDI versions of each
 *   function are integrated into a single function that selects the
 *   appropriate behavior based on a global variable.  For now, it seems
 *   sufficient to support only a single mode, since MDI seems clearly
 *   superior to SDI for this application, but this could be changed if
 *   users are divided over which mode they prefer and want to be able to
 *   select a mode at run-time.  
 */

#ifndef W32TDB_SDI
#define W32TDB_MDI
#endif

#ifdef W32TDB_MDI

/* 
 *   MDI mode 
 */

/*
 *   Debugger MDI client window class 
 */
class CHtmlWinDbgMdiClient: public CTadsWin
{
public:
    CHtmlWinDbgMdiClient(class CHtmlSys_dbgmain *dbgmain);
    ~CHtmlWinDbgMdiClient();

    /* 
     *   refresh the menu - we'll handle this ourselves here by doing
     *   nothing at all, since we rebuild the menu each time it's opened 
     */
    virtual int do_mdirefreshmenu() { return TRUE; }

    /* window styles for MDI client window */
    virtual DWORD get_winstyle()
    {
        return (WS_CHILD | WS_CLIPCHILDREN | WS_VSCROLL | WS_HSCROLL
                | MDIS_ALLCHILDSTYLES);
    }
    virtual DWORD get_winstyle_ex() { return WS_EX_CLIENTEDGE; }

    /* let the MDI client handle painting without our intervention */
    int do_paint() { return FALSE; }
    int do_paint_iconic() { return FALSE; }

    /* right button release - show context menu */
    virtual int do_rightbtn_up(int /*keys*/, int x, int y)
    {
        /* show our context menu */
        track_context_menu(GetSubMenu(ctx_menu_, 0), x, y);
        
        /* handled */
        return TRUE;
    }

    /* command handling */
    int do_command(int notify_code, int command_id, HWND ctl);
    TadsCmdStat_t check_command(int command_id);
    
protected:
    /* my context menu handle */
    HMENU ctx_menu_;

    /* main debugger window */
    class CHtmlSys_dbgmain *dbgmain_;
};

/* use the MDI child system interface object for children */
inline CTadsSyswin *w32tdb_new_child_syswin(CTadsWin *win)
{
    return new CTadsSyswinMdiChild(win);
}

/*
 *   Set the default frame window rectangle.  In MDI mode, make the frame
 *   window take up most of the desktop window.  
 */
inline void w32tdb_set_default_frame_rc(RECT *rc)
{
    /* get the desktop window area, and inset it a bit */
    SystemParametersInfo(SPI_GETWORKAREA, 0, (void *)rc, 0);
    InflateRect(rc, -16, -16);

    /* adjust the right and bottom elements to width and height */
    rc->right -= rc->left;
    rc->bottom -= rc->top;
}

/*
 *   Get the desktop window area to use for debugger child windows.  For
 *   MDI mode, we'll return the MDI client area.  
 */
inline void w32tdb_get_desktop_rc(CTadsWin *frame_win, RECT *rc)
{
    GetClientRect(frame_win->get_parent_of_children(), rc);
}

/*
 *   Set the default stack window position.  In MDI mode, we'll put the
 *   stack window at the top left of the client area.  
 */
inline void w32tdb_set_default_stk_rc(RECT *rc)
{
    SetRect(rc, 2, 2, 250, 350);
}

/* 
 *   style for main frame window - in MDI mode, make it a normal top-level
 *   window with all of the min/max controls 
 */
inline DWORD w32tdb_main_win_style()
{
    return (WS_OVERLAPPED | WS_BORDER | WS_THICKFRAME
            | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX
            | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_SIZEBOX);
}

/*
 *   Bring the debugger window group to the front.  In MDI mode, we simply
 *   bring the main frame to the front, since all other debugger windows
 *   are subwindows in the frame window.  
 */
inline void w32tdb_bring_group_to_top(HWND frame_handle)
{
    BringWindowToTop(frame_handle);
}

/*
 *   Adjust the coordinates of a debugger child window to coordinates
 *   relative to the containing main frame window, if appropriate.  In MDI
 *   mode, this adjusts the coordinates to be relative to the MDI client
 *   window that contains the debugger window.  
 */
inline void w32tdb_adjust_client_pos(CTadsWin *frame_win, RECT *rc)
{
    POINT ofs;
    
    /* get the offset of the MDI client window */
    ofs.x = ofs.y = 0;
    ScreenToClient(frame_win->get_parent_of_children(), &ofs);

    /* adjust the caller's rectangle to be relative to the client window */
    OffsetRect(rc, ofs.x, ofs.y);
}

#else /* W32TDB_MDI */

/*
 *   SDI mode 
 */

/* use the standard system interface object for child windows */
inline CTadsSyswin *w32tdb_new_child_syswin(CTadsWin *win)
{
    return new CTadsSyswin(win);
}

/*
 *   Set the default frame window rectangle.  In SDI mode, this window
 *   only contains the main menu and toolbar, so only make it big enough
 *   for the toolbar.  
 */
inline void w32tdb_set_default_frame_rc(RECT *rc)
{
    SetRect(rc, 2, 2, 440, 72);
}

/*
 *   Get the desktop window area to use for debugger child windows.  For
 *   SDI mode, we'll return the entire Windows desktop, since the debugger
 *   child windows are all top-level windows.  
 */
inline void w32tdb_get_desktop_rc(CTadsWin * /*frame_win*/, RECT *rc)
{
    /* get the desktop window working area */
    SystemParametersInfo(SPI_GETWORKAREA, 0, (void *)rc, 0);
}
/*
 *   Set the default stack window position.  In SDI mode, we'll put the
 *   stack window at the left of the screen, a ways below the main toolbar
 *   window.  
 */
inline void w32tdb_set_default_stk_rc(RECT *rc)
{
    SetRect(rc, 2, 120, 250, 350);
}

/* 
 *   style for main frame window - in SDI mode, there's no point in
 *   allowing this window to be maximized, so leave off the maximize
 *   button 
 */
inline DWORD w32tdb_main_win_style()
{
    return (WS_OVERLAPPED | WS_BORDER | WS_THICKFRAME
            | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX
            | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_SIZEBOX);
}


/*
 *   Bring the debugger window group to the front.  In SDI mode, we don't
 *   need to do anything here; we'll automatically bring the correct
 *   source window to the top, which will bring the entire window group to
 *   the top, since all debugger windows are part of a common ownership
 *   group.  
 */
inline void w32tdb_bring_group_to_top(HWND frame_handle)
{
#if 0 // $$$ no longer necessary - debugger windows are an ownership group now
    CHtmlSys_mainwin *mainwin;
    if ((mainwin = CHtmlSys_mainwin::get_main_win()) != 0
        && GetActiveWindow() == mainwin->get_handle())
        SetWindowPos(mainwin->get_handle(),
                     HWND_BOTTOM, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#endif
}

/*
 *   Adjust the coordinates of a debugger child window to coordinates
 *   relative to the containing main frame window, if appropriate.  In SDI
 *   mode, the debugger windows are all top-level windows, so this doesn't
 *   do anything.  
 */
inline void w32tdb_adjust_client_pos(HWND /*frame_handle*/, RECT * /*rc*/)
{
    /* leave the coordinates unchanged in SDI mode */
}

#endif /* W32TDB_MDI */

/*
 *   Create the main MDI frame/control window 
 */
CTadsSyswin *w32tdb_new_frame_syswin(class CHtmlSys_dbgmain *win,
                                     int winmnuidx);

/*
 *   Create the docking port in the MDI frame, if we're running in MDI
 *   mode 
 */
class CTadsDockportMDI
   *w32tdb_create_dockport(class CHtmlSys_dbgmain *mainwin,
                           HWND mdi_client);



/* ------------------------------------------------------------------------ */
/*
 *   New project template types 
 */
enum w32tdb_tpl_t
{
    W32TDB_TPL_INTRO,                          /* introductory starter game */
    W32TDB_TPL_ADVANCED,                           /* advanced starter game */
    W32TDB_TPL_NOLIB,                       /* non-library project template */
    W32TDB_TPL_CUSTOM                             /* custom source template */
};

/* ------------------------------------------------------------------------ */
/*
 *   Debugger main control window.  This is a top-level frame window that
 *   we create to hold the debugger menu and toolbar, and to control
 *   operations while the debugger is active.  
 */

/* private message: refresh source windows */
static const int HTMLM_REFRESH_SOURCES = HTMLM_LAST + 1;

/* 
 *   private message: build done (posted from compiler thread to the main
 *   window to tell it the compilation is finished) 
 */
static const int HTMLM_BUILD_DONE = HTMLM_LAST + 2;

/* private message: show deferred visibility windows (after loading) */
static const int HTMLM_LOAD_CONFIG_DONE = HTMLM_LAST + 3;

/* private message: save window position in preferences after move/size */
static const int HTMLM_SAVE_WIN_POS = HTMLM_LAST + 4;


class CHtmlSys_dbgmain:
   public CHtmlSys_framewin, public CHtmlDebugSysIfc_win,
   public CHtmlSysWin_win32_owner_null, public CHtmlSys_dbglogwin_hostifc,
   public CTadsStatusSource
{
public:
    CHtmlSys_dbgmain(struct dbgcxdef *ctx, const char *argv_last);
    ~CHtmlSys_dbgmain();

    /* get the application-wide debugger main window object */
    static CHtmlSys_dbgmain *get_main_win();

    /* activate or deactivate */
    int do_activate(int flag, int minimized, HWND other_win);

    /* 
     *   perform additional initialization required after the .GAM file
     *   has been loaded 
     */
    void post_load_init(struct dbgcxdef *ctx);

    /* get the reload execution status */
    int get_reloading_exec() const { return reloading_exec_ != 0; }

    /* get the name of the loaded game */
    const textchar_t *get_gamefile_name() const
        { return game_filename_.get(); }

    /* get the local configuration filename */
    const textchar_t *get_local_config_filename() const
        { return local_config_filename_.get(); }

    /* 
     *   Debugger event loop.  This is called whenever we enter the
     *   debugger.  We retain control and handle events until the user
     *   resumes execution of the game.  Returns true if execution should
     *   continue, false if the application was terminated from within the
     *   event loop.
     *   
     *   If *restart is set to true on return, the caller should signal a
     *   TADS restart condition to cause the game to start over from the
     *   beginning.  If *abort is set to true on return, the caller should
     *   signal a TADS abort-command condition to cause the game to abort
     *   the current command and start a new command.
     *   
     *   If *terminate is set to true on return, the caller should signal
     *   a TADS quit-game condition to cause the game to terminate.
     *   
     *   If *reload is set to true on return, the caller should signal
     *   TADS to quit, and exit to the outer loader loop.  The outer
     *   loader loop will re-start TADS with a new game.  
     */
    int dbg_event_loop(struct dbgcxdef *ctx, int bphit, int err,
                       int *restart, int *reload,
                       int *terminate, int *abort,
                       unsigned int *exec_ofs);

    /*
     *   Terminate the debugger.  Closes all debugger windows. 
     */
    void terminate_debugger();

    /* run the wait-for-build dialog to wait for the build to finish */
    void run_wait_for_build_dlg();

    /* load the configuration for the given game */
    int load_config(const textchar_t *game_file, int game_loaded);

    /* save our configuration */
    void save_config();

    /* update the recent-game list in the file menu */
    void update_recent_game_menu();

    /* add a game to the list of recent games in the file menu */
    void add_recent_game(const char *new_game);

    /* save the internal source directory list to the configuration */
    void srcdir_list_to_config();

    /* rebuild the internal source directory list from the configuration */
    void rebuild_srcdir_list();

    /* receive notification that TADS is shutting down */
    void notify_tads_shutdown();

    /* initialize a popup menu that's about to be displayed */
    void init_menu_popup(HMENU menuhdl, unsigned int pos, int sysmenu);

    /* receive notification that we've closed a menu */
    int menu_close(unsigned int item);
    
    /* process a command */
    int do_command(int notify_code, int command_id, HWND ctl);

    /* process notification */
    int do_notify(int control_id, int notify_code, LPNMHDR nmhdr);

    /* process a user message */
    int do_user_message(int msg, WPARAM wpar, LPARAM lpar);

    /* check a command status */
    TadsCmdStat_t check_command(int command_id);

    /* get my statusline object */
    class CTadsStatusline *get_statusline() const { return statusline_; }

    /*
     *   CHtmlDebugSysIfc_win implementation.  This is an interface that
     *   allows the portable helper code to call back into the
     *   system-dependent user interface code to create new windows. 
     */
    virtual class CHtmlSysWin
        *dbgsys_create_win(class CHtmlParser *parser,
                           class CHtmlFormatter *formatter,
                           const textchar_t *win_title,
                           const textchar_t *full_path,
                           HtmlDbg_wintype_t win_type);

    /*
     *   Select a source font 
     */
    virtual void dbgsys_set_srcfont(class CHtmlFontDesc *font_desc);

    /* compare filenames */
    virtual int dbgsys_fnames_eq(const char *fname1, const char *fname2);

    /* compare a filename to a path */
    virtual int dbgsys_fname_eq_path(const char *path, const char *fname);

    /* find a source file */
    virtual int dbgsys_find_src(const char *origname, int origlen,
                                char *fullname, size_t full_len,
                                int must_find_file);

    /*
     *   CHtmlSys_win32_owner implementation.  We only override a few
     *   methods, since we implement most of this interface with the null
     *   version. 
     */
    virtual void bring_to_front();

    /* get/set the TADS debugger context */
    struct dbgcxdef *get_dbg_ctx() const { return dbgctx_; }
    void set_dbg_ctx(struct dbgcxdef *ctx) { dbgctx_ = ctx; }

    /* get the pointer to the current execution offset in the debugger */
    unsigned int *get_exec_ofs_ptr() const { return exec_ofs_ptr_; }

    /* get the debugger helper object */
    class CHtmlDebugHelper *get_helper() const { return helper_; }

    /* get the configuration objects */
    class CHtmlDebugConfig *get_local_config() const
        { return local_config_; }
    class CHtmlDebugConfig *get_global_config() const
        { return global_config_; }

    /* get an integer variable value from the global configuration data */
    int get_global_config_int(const char *varname, const char *subvar,
                              int default_val) const;

    /*
     *   Get the active debugger window for command routing 
     */
    class CHtmlDbgSubWin *get_active_dbg_win();
    
    /* 
     *   notify me of a change in active debugger window status; a debugger
     *   window should call this whenever it's activated or deactivated 
     */
    void notify_active_dbg_win(class CHtmlDbgSubWin *win, int active)
    {
        /* activate or deactivate, as appropriate */
        if (active)
            set_active_dbg_win(win);
        else
            clear_active_dbg_win();
    }

    /*
     *   Set the active debugger window.  Source, stack, and other
     *   debugger windows should set themselves active when they are
     *   activated, and should clear the active window when they're
     *   deactivated. 
     */
    void set_active_dbg_win(class CHtmlDbgSubWin *win);
    void clear_active_dbg_win();

    /*
     *   Run the main debugger window's menu, activating it with the given
     *   system keydown message parameters.  This can be used when a
     *   window wants to let the keyboard menu interface in its window run
     *   the main debugger window's menu.  'srcwin' is the top-level
     *   window that's currently active; we'll re-activate this menu when
     *   we're done with the menu.  
     */
    void run_menu_kb_ifc(int vkey, long keydata, HWND srcwin);

    /*
     *   Receive notification that a debugger window is closing.  If I'm
     *   keeping track of the window as a background window for command
     *   routing, I'll forget about it now. 
     */
    void on_close_dbg_win(class CHtmlDbgSubWin *win);

    /* determine if the debugger has control */
    int get_in_debugger() const { return in_debugger_; }

    /* run the compiler */
    void run_compiler(int command_id);

    /* static entrypoint for compiler thread */
    static DWORD WINAPI run_compiler_entry(void *ctx);

    /* member function entrypoint for compiler thread */
    void run_compiler_th();

    /* run a command line tool */
    int run_command(const textchar_t *exe_printable_name,
                    const textchar_t *exe_file, const textchar_t *cmdline,
                    const textchar_t *cmd_dir);

    /* 
     *   Run a command - returns but does not display the error code.  The
     *   return value indicates the system result: zero indicates success,
     *   and non-zero indicates failure.  If the return value is zero
     *   (success), (*process_result) will contain the result code from
     *   the process itself; the interpretation of of the process result
     *   code is dependent upon the program invoked, but generally zero
     *   indicates success and non-zero indicates failure.  
     */
    int run_command_sub(const textchar_t *exe_printable_name,
                        const textchar_t *exe_file, const textchar_t *cmdline,
                        const textchar_t *cmd_dir, DWORD *process_result);

    /*
     *   Load a new game, or reload the current game.  The caller must
     *   return from the debugger event loop after invoking this in order
     *   to force the main loop to start over with the new game.
     */
    void load_game(const textchar_t *fname);

    /*
     *   Load a recent game, given the ID_GAME_RECENT_n command ID of the
     *   game to load.  Returns true if successful, false if there is no such
     *   recent game to be loaded.  
     */
    int load_recent_game(int command_id);

    /* 
     *   Create and load a new game.  We can optionally create the source
     *   file, by copying a templatefile to the given new source file.  We'll
     *   create a configuration for the new game based on the current
     *   configuration as a default, add build settings for the given source
     *   file and game file, then load the new game and kick off a compile.
     *   
     *   'custom_template' is the filename of a source file to use if the
     *   template type is W32TDB_TPL_CUSTOM.  
     */
    void create_new_game(const textchar_t *srcfile,
                         const textchar_t *gamefile,
                         int create_source, w32tdb_tpl_t template_type,
                         const textchar_t *custom_template,
                         const class CHtmlBiblio *biblio);

    /* copy a file, fixing up $xxx$ fields found in the text */
    int copy_insert_fields(const char *srcfile, const char *dstfile,
                           const class CHtmlBiblio *biblio);

    /* update expression windows */
    void update_expr_windows();

    /* 
     *   Add a new watch expression.  This will add the new expression at
     *   the end of the currently selected panel of the watch window.  
     */
    void add_watch_expr(const textchar_t *expr);

    /*
     *   Get the text for a Find command.  If the command is "Find", we'll
     *   run the dialog to get text from the user.  If the command is
     *   "Find Next," we'll simply return the text from the last Find
     *   command, if there was one; if not, we'll run the dialog as though
     *   this were a "Find" command.  If we run the dialog and the user
     *   cancels, we'll return a null pointer. 
     */
    const textchar_t *get_find_text(int command_id, int *exact_case,
                                    int *start_at_top, int *wrap, int *dir);

    /*
     *   Receive notification that source window preferences have changed.
     *   We'll update the source windows accordingly.  
     */
    void on_update_srcwin_options();

    /*
     *   Resume execution.  This is provided as a public method so that
     *   subwindows can tell the main window to start running. 
     */
    void exec_go();

    /*
     *   Resume execution for a single step. 
     */
    void exec_step_into();

    /* 
     *   Determine if we have a game workspace open.  If we have yet to
     *   open a game, we have no workspace.  Note that we can have a
     *   workspace open even when the associated game isn't loaded - this
     *   allows the user to modify a game's configuration (breakpoints,
     *   window layout, etc) even while the game isn't running.  
     */
    int get_workspace_open() const { return workspace_open_; }
    void set_workspace_open(int flag) { workspace_open_ = flag; }

    /*
     *   Get/Set the "game loaded" flag.  This flag is true when the game
     *   is loaded and running.  After the game terminates and the
     *   debugger context is no longer valid, this is false.  
     */
    int get_game_loaded() const { return game_loaded_; }
    void set_game_loaded(int flag)
    {
        /* remember the new game-loaded state */
        game_loaded_ = flag;

        /* profiling necessarily ends when the game ends */
        if (!flag)
            profiling_ = FALSE;
    }

    /* 
     *   get the "quitting" flag - if this is true, the debugger is in the
     *   process of terminating in response to an explicit user command to
     *   quit 
     */
    int is_quitting() const { return quitting_; }

    /* get the "reloading" flag */
    int is_reloading() const { return reloading_; }

    /* get the "terminating game" flag */
    int is_terminating() const { return term_in_progress_; }

    /* schedule an unconditional reload of all libraries */
    void schedule_lib_reload();

    /* open a source or other text file in its own MDI child window */
    void open_text_file(const char *fname);

    /* is the current MDI child window maximized? */
    int is_mdi_child_maximized() const
    {
        BOOL maxed;

        /* get the currently active MDI child */
        get_active_mdi_child(&maxed);

        /* return the 'maximized' status */
        return maxed;
    }

    /* get the current active MDI child, and it's 'maximized' status */
    HWND get_active_mdi_child(BOOL *maxed) const
    {
        return (HWND)SendMessage(get_parent_of_children(),
                                 WM_MDIGETACTIVE, 0, (LPARAM)maxed);
    }

    /*
     *   Open a file in the external text editor.  Returns true on
     *   success, false on failure.
     *   
     *   If 'local_if_no_linenum' is true, and the configured text editor
     *   doesn't have a line number setting in its configuration, we won't
     *   attempt to open the file in an external text editor at all, but
     *   will instead open the file in the local source viewer.  This can
     *   be used if it's more important that the actual line be shown,
     *   rather than that the file be opened in the external text editor;
     *   we use this to choose the priority when a choice must be made.
     *   When the editor is configured with a "%n" in its command line, we
     *   can have both, so we'll always open the file in the editor in
     *   this case.  
     */
    int open_in_text_editor(const textchar_t *fname,
                            long linenum, long colnum,
                            int local_if_no_linenum);

    /* get the project window, if any */
    class CHtmlDbgProjWin *get_proj_win() const { return projwin_; }

    /* update the game file name for a change in the preferences */
    void vsn_change_pref_gamefile_name(textchar_t *fname);

    /* 
     *   CHtmlSys_dbglogwin_hostifc implementation 
     */
    /* add a reference */
    virtual void dbghostifc_add_ref() { AddRef(); }

    /* remember a reference */
    virtual void dbghostifc_remove_ref() { Release(); }

    /* open a source file at a given line number */
    virtual void dbghostifc_open_source(const textchar_t *fname,
                                        long linenum, long colnum)
        { open_in_text_editor(fname, linenum, colnum, TRUE); }

    /* receive notification that the window is being destroyed */
    virtual void dbghostifc_on_destroy(class CHtmlSys_dbglogwin *win,
                                       class CHtmlSysWin_win32 *subwin);

    /* receive notification of child window activation change */
    virtual void dbghostifc_notify_active_dbg_win(
        class CHtmlSys_dbglogwin *win, int active)
        { notify_active_dbg_win(win, active); }

    /* get find text */
    const textchar_t *dbghostifc_get_find_text(
        int command_id, int *exact_case, int *start_at_top,
        int *wrap, int *dir)
    {
        /* use our normal FIND dialog */
        return get_find_text(command_id, exact_case, start_at_top, wrap, dir);
    }

    /*
     *   CTadsStatusSource implementation 
     */
    virtual textchar_t *get_status_message(int *caller_deletes);
    
protected:
    /* save the docking configuration */
    void save_dockwin_config(class CHtmlDebugConfig *config);

    /* load the docking configuration */
    void load_dockwin_config(class CHtmlDebugConfig *config);

    /* load the docking configuration for a given window */
    void load_dockwin_config_win(class CHtmlDebugConfig *config,
                                 const char *guid,
                                 class CTadsWin *subwin,
                                 const char *win_title,
                                 int visible,
                                 enum tads_dock_align default_dock_align,
                                 int default_docked_width,
                                 int default_docked_height,
                                 const RECT *default_undocked_pos,
                                 int default_is_docked);

    /* 
     *   Assign a docking window its GUID based on the loaded serial number.
     *   This is used for reverse compatibility, to convert a file from the
     *   old "dock id" serial numbering scheme to the new GUID scheme, where
     *   we store a global identifier for each docking window in the docking
     *   model itself.  
     */
    void assign_dockwin_guid(const char *guid);

    /* window style - make it a document window without a maximize box */
    DWORD get_winstyle() { return w32tdb_main_win_style(); }

    /* window creation */
    void do_create();

    /* window destruction */
    void do_destroy();

    /* close the window */
    int do_close();

    /* erase the background */
    int do_erase_bkg(HDC hdc);

    /* resize */
    void do_resize(int mode, int x, int y);

    /* adjust the statusline layout */
    void adjust_statusline_layout();

    /* ask the user if they want to quit - returns true if so */
    int ask_quit();

    /* store a position in the appropriate preferences entry */
    virtual void set_winpos_prefs(const class CHtmlRect *pos);

    /* 
     *   if we're currently waiting for the user to enter a command in the
     *   game, abort the command entry and force entry into the debugger 
     */
    void abort_game_command_entry();

    /*
     *   Terminate the game.  This can be used in the main debugger event
     *   loop to set the necessary state so that the game is terminated as
     *   soon as the debugger event loop returns.  This doesn't have
     *   immediate effect; the caller must return from the debugger event
     *   loop before the game will actually be terminated.  In the
     *   do_command() routine or another message handler, the caller can
     *   simply return after calling this, and the debugger event loop
     *   will terminate.  
     */
    void terminate_current_game()
    {
        /* set the game-termination flag */
        terminating_ = TRUE;
        term_in_progress_ = TRUE;
        
        /* set the "go" flag so that we leave our event loop */
        go_ = TRUE;
        
        /* wrest control from the game if a command is being read */
        abort_game_command_entry();
    }

    /* clear non-default configuration information */
    void clear_non_default_config();

    /* 
     *   build service routine - get the name of an external resource
     *   file, and determine if it should be included in the build;
     *   returns true if so, false if not 
     */
    int build_get_extres_file(char *buf, int cur_extres, int build_command,
                              const char *extres_base_name);

    /* delete a temporary directory and all of its contents */
    int del_temp_dir(const char *dirname);

private:
    /* 
     *   Methods that vary based on the debugger version.  The TADS 2 and
     *   TADS 3 debuggers have some minor UI differences; we factor these
     *   differences into these routines, the appropriate implementations
     *   of which can be chosen with the linker when building the
     *   debugger executable.
     */

    /* 
     *   adjust a program argument to the proper file type for initial
     *   loading via command-line parameters 
     */
    void vsn_adjust_argv_ext(textchar_t *fname);

    /* set my window title, based on the loaded project */
    void vsn_set_win_title();

    /* 
     *   determine if we need the "srcfiles" list (this is passed to the
     *   CHtmlDebugHelper object during construction) 
     */
    int vsn_need_srcfiles() const;

    /* 
     *   Validate a project file we're attempting to load via "Open Project."
     *   If this routine fully handles the loading, returns true; otherwise,
     *   returns false.  Note that if the project cannot be loaded (because
     *   the file type is invalid, for example), this routine should show an
     *   appropriate error alert and then return true; it returns true on
     *   failure because displaying the error message is all that's needed in
     *   such cases, and thus fully handles the load.  
     */
    int vsn_validate_project_file(const textchar_t *fname);

    /* 
     *   Determine if a project file is a valid project file.  Returns true
     *   if so, false if not.  This differs from vsn_validate_project_file()
     *   in that there is no interactive attempt to load the correct file or
     *   show an error; we simply indicate whether or not we'll be able to
     *   proceed with loading this type of project file.  
     */
    int vsn_is_valid_proj_file(const textchar_t *fname);

    /* get the "load profile" */
    int vsn_get_load_profile(char **game_filename,
                             char *game_filename_buf,
                             char *config_filename,
                             const char *load_filename,
                             int game_loaded);

    /* get the game filename from the loaded profile */
    int vsn_get_load_profile_gamefile(char *buf);

    /* load a configuration file */
    int vsn_load_config_file(CHtmlDebugConfig *config,
                             const textchar_t *fname);

    /* save a configuration file */
    int vsn_save_config_file(CHtmlDebugConfig *config,
                             const textchar_t *fname);

    /* 
     *   during configuration loading, close windows specific to this
     *   debugger version (common windows, such as the local variables and
     *   watch expressions windows, will be closed by the common code) 
     */
    void vsn_load_config_close_windows();

    /* during configuration loading, open version-specific windows */
    void vsn_load_config_open_windows(const RECT *descrc);

    /* save version-specific configuration data */
    void vsn_save_config();

    /* main debug window version-specific command processing */
    TadsCmdStat_t vsn_check_command(int command_id);
    int vsn_do_command(int notify_code, int command_id, HWND ctl);

    /* 
     *   check to see if it's okay to proceed with a compile command - if
     *   so, return true; if not, show an error message and return false 
     */
    int vsn_ok_to_compile();

    /* clear version-specific non-default configuration information */
    void vsn_clear_non_default_config();

    /* run the debugger about box */
    void vsn_run_dbg_about();

    /* run the program-arguments dialog box */
    void vsn_run_dbg_args_dlg();

    /* 
     *   Process captured build output - this is called with each complete
     *   line of text read from the standard output of the build subprocess
     *   as it runs.  The routine can modify the text in the buffer if
     *   desired.  If the text is to be displayed, return true; otherwise,
     *   return false.  
     */
    int vsn_process_build_output(CStringBuf *txt);

private:
    /* notify a window of formatting option changes */
    void note_formatting_changes(class CHtmlSysWin_win32 *win);

    /* 
     *   Load the file menu - we'll call this on debugger entry if we
     *   haven't yet loaded the file menu.  This sets up the quick-open
     *   file submenu to include the game's source files.  
     */
    void load_file_menu();
    
    /* callback for building the file menu by enumerating line sources */
    static void load_menu_cb(void *ctx, const textchar_t *fname,
                             int line_source_id);

    /* callback for enumerating open windows during activation */
    static void enum_win_activate_cb(void *ctx0, int idx, CHtmlSysWin *win,
                                     int line_source_id,
                                     HtmlDbg_wintype_t win_type);

    /* callback for enumerating open windows to close the source windows */
    static void enum_win_close_cb(void *ctx0, int idx, CHtmlSysWin *win,
                                  int line_source_id,
                                  HtmlDbg_wintype_t win_type);

    /* callback for enumerating open windows for saving them to config */
    static void enum_win_save_cb(void *ctx0, int idx, CHtmlSysWin *win,
                                 int line_source_id,
                                 HtmlDbg_wintype_t win_type);

    /* callback for enumerating open windows for building the Window menu */
    static void winmenu_win_enum_cb(void *ctx0, int idx, CHtmlSysWin *win,
                                    int line_source_id,
                                    HtmlDbg_wintype_t win_type);

    /* callback for enumerating windows for updating option settings */
    static void options_enum_srcwin_cb(void *ctx, int idx,
                                       class CHtmlSysWin *win,
                                       int line_source_id,
                                       HtmlDbg_wintype_t win_type);

    /* clear the list of build comands */
    void clear_build_cmds();

    /* 
     *   Add a build command at the end of the list.  If 'required' is
     *   true, the command will be executed even if a previous command
     *   fails; otherwise, the command will be skipped if any previous
     *   command fails. 
     */
    void add_build_cmd(const textchar_t *exe_name,
                       const textchar_t *exe,
                       const textchar_t *cmdline,
                       const textchar_t *cmd_dir,
                       int required);

    /* save configuration information for the 'find' dialog */
    int save_find_config(class CHtmlDebugConfig *config,
                         const textchar_t *varname);

    /* load configuration information for the 'find' dialog */
    int load_find_config(class CHtmlDebugConfig *config,
                         const textchar_t *varname);

    /* enumeration callback for writing profiling data */
    static void write_prof_data(void *ctx0, const char *func_name,
                                unsigned long time_direct,
                                unsigned long time_in_children,
                                unsigned long call_cnt);
    
    
    /* 
     *   Context structure from TADS debugger.  We set this on each call
     *   from TADS, so that we have the correct TADS debugger context
     *   whenever we're active.  
     */
    struct dbgcxdef *dbgctx_;

    /*
     *   Pointer to the execution offset in the debugger.  By changing
     *   this, we can change the next instruction that will be executed.
     *   Note that we must not change this so that it's outside of the
     *   object.method or function that contained the original offset upon
     *   the most recent entry into the debugger.  
     */
    unsigned int *exec_ofs_ptr_;

    /*
     *   Debugger helper 
     */
    class CHtmlDebugHelper *helper_;

    /*
     *   Main HTMLTADS preferences object
     */
    class CHtmlPreferences *prefs_;

    /*
     *   The "Find" dialog object 
     */
    class CTadsDialogFind *find_dlg_;

    /* text of current/last find */
    textchar_t find_text_[512];

    /* flag indicating whether exact case should be used in searches */
    int search_exact_case_;

    /* flag indicating whether the search wraps at the end of the file */
    int search_wrap_;

    /* search direction: 1 for forwards, -1 for backwards */
    int search_dir_;

    /* new searches start from top of document */
    int search_from_top_;

    /* name of the game file */
    CStringBuf game_filename_;

    /* debugger global configuration object, and configuration filename */
    class CHtmlDebugConfig *global_config_;
    CStringBuf global_config_filename_;

    /* local (program-specific) configuration */
    class CHtmlDebugConfig *local_config_;
    CStringBuf local_config_filename_;

    /* 
     *   Flag indicating that we're stepping execution.  We use this to
     *   exit the debugger event loop when the user steps or runs, so that
     *   control leaves the debugger event loop and returns to TADS to
     *   continue game execution.  
     */
    int go_;

    /*
     *   Flag indicating that we're in the debugger.  This is set to true
     *   whenever the debugger UI takes control, and set to false when the
     *   debugger returns control to TADS. 
     */
    int in_debugger_ : 1;

    /*
     *   Flag indicating that we're quitting.  If a quit command is
     *   entered, we'll set this flag and then exit the event loop as
     *   though we were about to resume execution, aborting the program at
     *   the last moment.  (We can't simply quit, because we need to be
     *   sure to properly exit out of the event loop.)
     */
    int quitting_ : 1;

    /*
     *   Flag indicating that we're restarting.  If a restart command is
     *   entered, we'll set this flag and then exit the event loop,
     *   flagging to the event loop caller that the game is to be
     *   restarted. 
     */
    int restarting_ : 1;

    /*
     *   Flag indicating that we have a pending reload. 
     */
    int reloading_ : 1;

    /* 
     *   flag indicating that the reload is a different game than was
     *   previously loaded 
     */
    int reloading_new_ : 1;

    /*
     *   Flag indicating how we're to proceed upon reloading a new game.
     *   If reloading_go_ is true, we'll jump right in and start executing
     *   the newly-loaded game until we hit a breakpoint, or something
     *   else interrupts execution; otherwise, we'll stop at the first
     *   instruction upon loading the new game. 
     */
    int reloading_go_ : 1;

    /*
     *   Flag indicating whether or not we're to begin execution at all
     *   when loading or reloading.  This is set only when the user enters
     *   a Go, Step, or other command that explicitly begins execution.
     */
    int reloading_exec_ : 1;

    /* 
     *   flag indicating that we're forcing termination of the game (but
     *   not quitting out of the debugger itself) 
     */
    int terminating_ : 1;

    /* flag indicating termination is in progress (for status messages) */
    int term_in_progress_ : 1;

    /*
     *   Flag indicating that we're aborting the current command.  This
     *   works like the 'quitting' and 'restarting' flags. 
     */
    int aborting_cmd_ : 1;

    /* 
     *   Flag: we're in the process of refreshing our sources after
     *   re-activation of our window 
     */
    int refreshing_sources_ : 1;

    /* Flag: the profiler is running */
    int profiling_ : 1;

    /* my accelerator table */
    HACCEL accel_;

    /* 
     *   Current active debugger window.  Each debugger window that wants
     *   to receive commands must register itself here when it becomes
     *   active, and deregister itself on becoming inactive. 
     */
    class CHtmlDbgSubWin *active_dbg_win_;

    /* 
     *   Last active debugger window.  We keep track of this so that we
     *   can route commands that are intended for debugger windows to the
     *   appropriate window when the main window itself is active.  When
     *   the main window is active, we'll route commands to the last
     *   active window if it's the window immediately below the main
     *   window in Z order.  
     */
    class CHtmlDbgSubWin *last_active_dbg_win_;

    /*
     *   Menu delegator window.  If one of the debugger windows sent us a
     *   menu keystroke (with the ALT key) to run our menu, it wants to be
     *   re-activated as soon as the menu is finished.  We'll remember the
     *   window here, and re-activate it again when we're finished. 
     */
    HWND post_menu_active_win_;

    /*
     *   Watch window, local variable window
     */
    class CHtmlDbgWatchWin *watchwin_;
    class CHtmlDbgWatchWin *localwin_;

    /* project window (TADS 3 only) */
    class CHtmlDbgProjWin *projwin_;

    /* MDI client dock port */
    class CTadsDockportMDI *mdi_dock_port_;

    /* the MDI "Window" menu handle */
    HMENU win_menu_hdl_;

    /* toolbar handle */
    HWND toolbar_;

    /* statusline */
    class CTadsStatusline *statusline_;

    /* flag: status line is visible */
    int show_statusline_;

    /*
     *   Build thread handle.  While a build is running, this is the
     *   handle of the background thread handling the build. 
     */
    HANDLE build_thread_;

    /* compiler process handle and PID - valid while a build is running */
    HANDLE compiler_hproc_;
    DWORD compiler_pid_;

    /* current compiler status message */
    CStringBuf compile_status_;

    /* head and tail of list of build commands */
    class CHtmlDbgBuildCmd *build_cmd_head_;
    class CHtmlDbgBuildCmd *build_cmd_tail_;

    /* 
     *   Build wait dialog.
     *   
     *   This dialog is displayed when the user attempts to close the
     *   program and a build is still running; we use the dialog to keep the
     *   UI open until the build completes, at which point we terminate the
     *   program normally.
     *   
     *   In addition, this can be used to run a build modally.  Most builds
     *   run modelessly, so that the user can continue operating most parts
     *   of the user interface while a build proceeds in the background.
     *   This is not always desirable; for example, when we scan for
     *   includes, we want to run a build modally.  By showing a modal
     *   dialog while building, the code can effectively make a build modal.
     */
    class CTadsDialog *build_wait_dlg_;

    /* 
     *   the command ID of the running build, if any (this is valid only if
     *   build_running_ is true) 
     */
    int build_cmd_;

    /*
     *   Flag indicating that the game is loaded.  This is set while the
     *   game is running, and cleared when the game quits and the TADS
     *   engine shuts down.  
     */
    int game_loaded_ : 1;

    /*
     *   Flag indicating that a workspace is open.  The workspace is open
     *   even after a game terminates, since it can be reloaded with the
     *   current workspace context (breakpoints, window placement, etc).  
     */
    int workspace_open_ : 1;

    /*
     *   Flag indicating that a build is in progress (in a background
     *   thread).  We can't do certain things when we're building. 
     */
    int build_running_ : 1;

    /* 
     *   flag indicating that we're capturing build output for internal
     *   processing rather than displaying it on the console - if this is
     *   set, we won't show any build output, but just run it all through our
     *   filter routine 
     */
    int capture_build_output_ : 1;

    /* 
     *   flag indicating that we're filtering build output through our filter
     *   routine - if this is set, we'll show all build output that the
     *   filter function says we should show 
     */
    int filter_build_output_ : 1;

    /* 
     *   flag indicating that we should execute the program as soon as
     *   we're finished building it 
     */
    int run_after_build_ : 1;

    /* 
     *   Flag indicating whether or not we automatically flush game window
     *   output upon entering the debugger.  This is a preference setting,
     *   but we keep it cached here since we need to read the setting
     *   frequently.  
     */
    unsigned int auto_flush_gamewin_ : 1;

    /*
     *   Flag: we're in the process of loading a configuration.  If this
     *   is set, we'll defer showing MDI windows until all of the frame
     *   adornments have been created, so that the MDI window positions
     *   relative to the MDI client will be correctly adjusted for all the
     *   frame adornments. 
     */
    unsigned int loading_win_config_ : 1;

    /*
     *   List of windows which we're waiting to show until we've loaded
     *   all of the other windows. 
     */
    struct html_dbg_win_list *loading_win_defer_list_;

    /* 
     *   the original TADSLIB environment variable we inherited from our
     *   parent environment 
     */
    CStringBuf orig_tadslib_var_;
};

/*
 *   Window list entry
 */
struct html_dbg_win_list
{
    html_dbg_win_list(HWND h)
    {
        hwnd = h;
        nxt = 0;
    }
    
    /* current window */
    HWND hwnd;

    /* next entry in list */
    html_dbg_win_list *nxt;
};

/* ------------------------------------------------------------------------ */
/*
 *   Build Command list entry.  When we prepare to run a build, we create
 *   a list of these build command entries, which we pass to the
 *   background thread for execution.
 *   
 *   We build these in the main thread, so that the background process
 *   doesn't have to look at configuration data or perform memory
 *   management; this removes the need to worry about threading issues
 *   with any of our major subsystems. 
 */
class CHtmlDbgBuildCmd
{
public:
    CHtmlDbgBuildCmd(const textchar_t *exe_name,
                     const textchar_t *exe,
                     const textchar_t *cmdline,
                     const textchar_t *cmd_dir, int required)
    {
        /* remember the build data */
        exe_name_.set(exe_name);
        exe_.set(exe);
        args_.set(cmdline);
        dir_.set(cmd_dir);
        required_ = required;

        /* we're not in a list yet */
        next_ = 0;
    }
    
    /* next build command in the list */
    CHtmlDbgBuildCmd *next_;

    /* display name of the command */
    CStringBuf exe_name_;

    /* executable filename */
    CStringBuf exe_;

    /* command line arguments */
    CStringBuf args_;

    /* directory in which to execute the command */
    CStringBuf dir_;

    /* flag: execute the command even if a prior command fails */
    int required_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Window frame class for debugger source windows.  This is a simple
 *   window frame that contains an HTML pane. 
 */
class CHtmlSys_dbgsrcwin: public CHtmlSys_dbgwin
{
public:
    CHtmlSys_dbgsrcwin(class CHtmlParser *parser,
                       class CHtmlPreferences *prefs,
                       class CHtmlDebugHelper *helper,
                       class CHtmlSys_dbgmain *dbgmain,
                       const textchar_t *full_path);
    ~CHtmlSys_dbgsrcwin();

    /* initialize the HTML panel */
    virtual void init_html_panel(class CHtmlFormatter *formatter);

    /* 
     *   Store my position in the preferences, on changing the window
     *   position.  We don't actually store anything here; we save our
     *   position on certain other events, such as closing the window or
     *   closing the project.
     *   
     *   We don't store anything here because the order of events in MDI mode
     *   makes it impossible to tell whether or not we're maximized when a
     *   move/size event fires.  We can tell later on, after the overall
     *   size/move has finished, so we post a message to ourselves to save
     *   our preferences later.  
     */
    void set_winpos_prefs(const class CHtmlRect *)
    {
        /* post ourselves our private deferred-save-position message */
        PostMessage(handle_, HTMLM_SAVE_WIN_POS, 0, 0);
    }

    /* process a user message */
    int do_user_message(int msg, WPARAM wpar, LPARAM lpar);

    /* destroy the window */
    void do_destroy();

    /* bring the window to the front */
    void bring_owner_to_front();

    /* get the frame handle */
    virtual HWND get_owner_frame_handle() const { return handle_; }

    /* non-client activate or deactivate */
    int do_ncactivate(int flag);

    /* artificial activation from parent */
    void do_parent_activate(int active);

    /* check the status of a command */
    TadsCmdStat_t check_command(int command_id);

    /* execute a command */
    int do_command(int notify_code, int command_id, HWND ctl);

    /* check the status of a command routed from the main window */
    TadsCmdStat_t active_check_command(int command_id);

    /* execute a command routed from the main window */
    int active_do_command(int notify_code, int command_id, HWND ctl);

    /* get my window handle */
    HWND active_get_handle() { return get_handle(); }

    /* receive notifications */
    int do_notify(int control_id, int notify_code, LPNMHDR nm);

    /* get the text for a 'find' command */
    const textchar_t *get_find_text(int command_id, int *exact_case,
                                    int *start_at_top, int *wrap, int *dir);

    /* receive notification of a caret position change */
    virtual void owner_update_caret_pos();

protected:
    /* 
     *   Do the work of storing my window position in the preferences, on
     *   closing the window or closing the project.  If we are the current
     *   maximized MDI child window, we will not store the information - the
     *   maximized size is entirely dependent on the frame window size, so we
     *   don't need to remember it, and we do want to remember the 'restored'
     *   size without overwriting it with the maximized size.  
     */
    void set_winpos_prefs_deferred();

    /* 
     *   internal routine: store the given MDI-frame-relative position in the
     *   preferences as our window position 
     */
    virtual void store_winpos_prefs(const CHtmlRect *pos);

    /* update the statline cursor position indicator */
    void update_statusline_linecol();

    /* format my line/column information for statusline display */
    virtual void format_linecol(char *buf, size_t buflen);

    /* 
     *   adjust screen coordinates to frame-relative coordinates, if this
     *   window is contained in another window
     */
    void adjust_coords_to_frame(CHtmlRect *newpos, const CHtmlRect *pos);

    /* create the HTML panel - called during initialization */
    virtual void create_html_panel(class CHtmlFormatter *formatter);

    /* my debugger helper object */
    class CHtmlDebugHelper *helper_;

    /* the main debugger window */
    class CHtmlSys_dbgmain *dbgmain_;

    /* full path name of this file */
    CStringBuf path_;
};

/* ------------------------------------------------------------------------ */
/*
 *   HTML window subclass for debugger source windows.  The debugger
 *   source window displays some extra information in the left margin of
 *   each line to show the status of the line (currently executing line,
 *   breakpoint location, etc).  
 */
class CHtmlSysWin_win32_dbgsrc: public CHtmlSysWin_win32
{
public:
    CHtmlSysWin_win32_dbgsrc(class CHtmlFormatter *formatter,
                             class CHtmlSysWin_win32_owner *owner,
                             int has_vscroll, int has_hscroll,
                             class CHtmlPreferences *prefs,
                             class CHtmlDebugHelper *helper,
                             class CHtmlSys_dbgmain *dbgmain,
                             const textchar_t *path);

    ~CHtmlSysWin_win32_dbgsrc();

    /* draw a debugger source line icon */
    void draw_dbgsrc_icon(const CHtmlRect *pos, unsigned int stat);

    /* measure a debugger source window margin icon */
    CHtmlPoint measure_dbgsrc_icon();

    /* check the status of a command */
    TadsCmdStat_t check_command(int command_id);

    /* execute a command */
    int do_command(int notify_code, int command_id, HWND ctl);

    /* check the status of a command routed from the main window */
    virtual TadsCmdStat_t active_check_command(int command_id);

    /* execute a command routed from the main window */
    virtual int active_do_command(int notify_code, int command_id, HWND ctl);

    /* set the background cursor */
    virtual int do_setcursor_bkg();

    /* note a change in the debugger color/font preferences */
    virtual void note_debug_format_changes(HTML_color_t bkg_color,
                                           HTML_color_t text_color,
                                           int use_windows_colors);

    /* 
     *   ignore HTML-based color changes in source windows - we use our
     *   own direct color settings 
     */
    void set_html_bg_color(HTML_color_t, int) { }
    void set_html_text_color(HTML_color_t, int) { }

    /* handle keystrokes */
    int do_keydown(int virtual_key, long keydata);

    /* handle an arrow keystroke */
    unsigned long do_keydown_arrow(int vkey, unsigned long caret_pos,
                                   int cnt);
    
    /* 
     *   handle "system" keystrokes (with the ALT key down) - we want to
     *   pass these on to the main debug window, so that the main debug
     *   window's menus can be accessed with the keyboard while this
     *   window has focus 
     */
    int do_syskeydown(int vkey, long keydata);

    /* 
     *   check the file's current timestamp against the timestamp when we
     *   loaded the file; returns true if the file has changed since we
     *   loaded it 
     */
    int has_file_changed() const;

    /*
     *   Check the file to see if it's been explicitly opened for editing.
     *   If it has, we won't prompt for reloading, but will simply reload;
     *   if the user explicitly edited the file, they almost certainly
     *   want to reload it. 
     */
    int was_file_opened_for_editing() const { return opened_for_editing_; }

    /* update our copy of the file timestamp to its current timestamp */
    void update_file_time();

    /* get the full path to the file, if known */
    const textchar_t *get_path() const { return path_.get(); }
    
    /* receive notification that the parent is being destroyed */
    void on_destroy_parent()
    {
        /* inherit default */
        CHtmlSysWin_win32::on_destroy_parent();

        /* 
         *   forget my formatter - the debug helper will delete the
         *   formatter as soon as it receives notification that the source
         *   window is being destroyed, so we need to forget about the
         *   formatter when we hear about it 
         */
        formatter_ = 0;
    }

    /* get my caret line/column position */
    void get_caret_linecol(long *lin, long *col);

protected:
    /* 
     *   calculate the icon size based on the current font, if we haven't
     *   already done so 
     */
    void calc_icon_size();

    /* window creation */
    void do_create();

    /* process a mouse click */
    int do_leftbtn_down(int keys, int x, int y, int clicks);

    /* process a right mouse click */
    int do_rightbtn_down(int keys, int x, int y, int clicks);

    /* process a common left/right click */
    int common_btn_down(int keys, int x, int y, int clicks);

    /* process a notification */
    int do_notify(int control_id, int notify_code, LPNMHDR nmhdr);

    /* show the 'not found' message for a 'find' command */
    void find_not_found();

    /* draw the margin background */
    virtual void draw_margin_bkg(const RECT *rc);

    /* handle a 'go to line' command */
    void do_goto_line();
    
    /* normal (16x16) icons for debugger source windows */
    HICON ico_dbg_curline_;
    HICON ico_dbg_ctxline_;
    HICON ico_dbg_bp_;
    HICON ico_dbg_bpdis_;
    HICON ico_dbg_curbp_;
    HICON ico_dbg_curbpdis_;
    HICON ico_dbg_ctxbp_;
    HICON ico_dbg_ctxbpdis_;

    /* small (8x8) icons for debugger source windows */
    HICON smico_dbg_curline_;
    HICON smico_dbg_ctxline_;
    HICON smico_dbg_bp_;
    HICON smico_dbg_bpdis_;
    HICON smico_dbg_curbp_;
    HICON smico_dbg_curbpdis_;
    HICON smico_dbg_ctxbp_;
    HICON smico_dbg_ctxbpdis_;

    /* debugger helper */
    class CHtmlDebugHelper *helper_;

    /* the main debugger window */
    class CHtmlSys_dbgmain *dbgmain_;

    /* icon size */
    int icon_size_;
#define W32TDB_ICON_UNKNOWN   1                    /* font not yet measured */
#define W32TDB_ICON_SMALL     2                        /* small (8x8) icons */
#define W32TDB_ICON_NORMAL    3                     /* normal (16x16) icons */

    /* 
     *   target column for up/down arrow operation - this is only valid
     *   after an up/down operation when there has been no invervening
     *   keystroke 
     */
    int target_col_valid_ : 1;
    unsigned int target_col_;

    /* full path name of this file */
    CStringBuf path_;

    /* 
     *   modification date of the file, if we have a path -- we use this
     *   to track when the file is changed externally when we're
     *   activated, so that we can reload it when the user modifies it
     *   from another process 
     */
    FILETIME file_time_;

    /* flag: the file was opened for editing since last loaded */
    int opened_for_editing_ : 1;
};

/* ------------------------------------------------------------------------ */
/*
 *   Stack window frame.  This is the same as a source window frame,
 *   except that it uses a stack panel. 
 */
class CHtmlSys_dbgstkwin: public CHtmlSys_dbgsrcwin
{
public:
    CHtmlSys_dbgstkwin(class CHtmlParser *parser,
                       class CHtmlPreferences *prefs,
                       class CHtmlDebugHelper *helper,
                       class CHtmlSys_dbgmain *dbgmain)
        : CHtmlSys_dbgsrcwin(parser, prefs, helper, dbgmain, 0)
    {
    }

    /* this is a docking window - make it a child */
    DWORD get_winstyle()
    {
        return (WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
    }
    DWORD get_winstyle_ex() { return 0; }

    /* bring the window to the front */
    void bring_owner_to_front()
    {
        /* this is a docking window - bring my parent to the front */
        ShowWindow(GetParent(handle_), SW_SHOW);
        BringWindowToTop(GetParent(handle_));
        
        /* inherit default */
        CHtmlSys_dbgsrcwin::bring_owner_to_front();
    }

    /* close the owner window */
    int close_owner_window(int force);

protected:
    /* store the window position in the preferences */
    void store_winpos_prefs(const CHtmlRect *pos);

    /* format my line/column information for statusline display */
    virtual void format_linecol(char *buf, size_t)
    {
        /* don't display a cursor position in the stack window */
        buf[0] = '\0';
    }

private:
    /* create the HTML panel - called during initialization */
    virtual void create_html_panel(class CHtmlFormatter *formatter);
};

/*
 *   HTML window subclass for debugger stack window.  This is the same as
 *   a source window, except for a slightly different display style.  
 */
class CHtmlSysWin_win32_dbgstk: public CHtmlSysWin_win32_dbgsrc
{
public:
    CHtmlSysWin_win32_dbgstk(class CHtmlFormatter *formatter,
                             class CHtmlSysWin_win32_owner *owner,
                             int has_vscroll, int has_hscroll,
                             class CHtmlPreferences *prefs,
                             class CHtmlDebugHelper *helper,
                             class CHtmlSys_dbgmain *dbgmain)
        : CHtmlSysWin_win32_dbgsrc(formatter, owner, has_vscroll,
                                   has_hscroll, prefs, helper, dbgmain, 0)
    {
    }

    /* process a mouse click */
    int do_leftbtn_down(int keys, int x, int y, int clicks);

    /* check the status of a command routed from the main window */
    TadsCmdStat_t active_check_command(int command_id);

    /* execute a command routed from the main window */
    int active_do_command(int notify_code, int command_id, HWND ctl);

protected:
    /* 
     *   draw the margin background - we'll just leave it in the window's
     *   background color 
     */
    virtual void draw_margin_bkg(const RECT *) { }
};


/* ------------------------------------------------------------------------ */
/*
 *   History window frame.  This is the based on the source window frame,
 *   but has some differences for call history.  
 */
class CHtmlSys_dbghistwin: public CHtmlSys_dbgsrcwin
{
public:
    CHtmlSys_dbghistwin(class CHtmlParser *parser,
                        class CHtmlPreferences *prefs,
                        class CHtmlDebugHelper *helper,
                        class CHtmlSys_dbgmain *dbgmain)
        : CHtmlSys_dbgsrcwin(parser, prefs, helper, dbgmain, 0)
    {
    }
    
    /* this is a docking window - make it a child */
    DWORD get_winstyle()
    {
        return (WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
    }
    DWORD get_winstyle_ex() { return 0; }

    /* bring the window to the front */
    void bring_owner_to_front()
    {
        /* this is a docking window - bring my parent to the front */
        ShowWindow(GetParent(handle_), SW_SHOW);
        BringWindowToTop(GetParent(handle_));

        /* inherit default */
        CHtmlSys_dbgsrcwin::bring_owner_to_front();
    }

    /* close the owner window */
    int close_owner_window(int force);

protected:
    /* store the window position in the preferences */
    void store_winpos_prefs(const CHtmlRect *pos);

    /* format my line/column information for statusline display */
    virtual void format_linecol(char *buf, size_t)
    {
        /* don't display a cursor position in the this window */
        buf[0] = '\0';
    }

private:
    /* create the HTML panel - called during initialization */
    virtual void create_html_panel(class CHtmlFormatter *formatter);
};

/*
 *   HTML window subclass for history trace window.  This is the same as a
 *   source window, except for a slightly different display style.  
 */
class CHtmlSysWin_win32_dbghist: public CHtmlSysWin_win32_dbgsrc
{
public:
    CHtmlSysWin_win32_dbghist(class CHtmlFormatter *formatter,
                              class CHtmlSysWin_win32_owner *owner,
                              int has_vscroll, int has_hscroll,
                              class CHtmlPreferences *prefs,
                              class CHtmlDebugHelper *helper,
                              class CHtmlSys_dbgmain *dbgmain)
        : CHtmlSysWin_win32_dbgsrc(formatter, owner, has_vscroll,
                                   has_hscroll, prefs, helper, dbgmain, 0)
    {
    }
    
    /* check the status of a command routed from the main window */
    TadsCmdStat_t active_check_command(int command_id);
    
    /* execute a command routed from the main window */
    int active_do_command(int notify_code, int command_id, HWND ctl);
    
protected:
    /* 
     *   draw the margin background - we'll just leave it in the window's
     *   background color 
     */
    virtual void draw_margin_bkg(const RECT *) { }
};

/* ------------------------------------------------------------------------ */
/*
 *   Dialog for the debugger about box 
 */

class CTadsDlg_dbgabout: public CTadsDialog
{
public:
    /* initialize */
    void init();
};

/* ------------------------------------------------------------------------ */
/*
 *   Dialog for waiting for the build to terminate 
 */
class CTadsDlg_buildwait: public CTadsDialog
{
public:
    CTadsDlg_buildwait(HANDLE comp_hproc)
    {
        /* remember the compiler's process handle */
        comp_hproc_ = comp_hproc;
    }

    /* handle a command */
    virtual int do_command(WPARAM id, HWND ctl);

private:
    /* process handle of the compiler subprocess */
    HANDLE comp_hproc_;
};

/* ------------------------------------------------------------------------ */
/*
 *   special CStringBuf subclass, to add a little functionality for
 *   convenience in building command lines 
 */
class CStringBufCmd: public CStringBuf
{
public:
    CStringBufCmd(size_t siz) : CStringBuf(siz) { }

    /* append a string, adding double quotes around the string */
    void append_qu(const textchar_t *str, int leading_space)
    {
        size_t len;
        
        /* add a optional leading space and the quote */
        append_inc(leading_space ? " \"" : "\"", 512);

        /* add the string */
        append_inc(str, 512);

        /* 
         *   If the last character of the string is a backslash, add an extra
         *   backslash.  The VC++ argv parsing rules are a bit weird and
         *   inconsistent: a backslash quotes a quote, or quotes a backslash,
         *   but only if the double-backslash is immediately before a quote.
         *   Backslashes aren't needed to quote backslashes elsewhere - only
         *   before quotes.  So, to avoid having the backslash quote the
         *   quote, quote the backslash.  
         */
        if ((len = strlen(str)) != 0 && str[len-1] == '\\')
            append_inc("\\", 512);

        /* add the closing quote */
        append_inc("\"", 512);
    }

    /* 
     *   Append an option an and argument.  We place the argument in
     *   quotes, and add a leading space before the option specifier and
     *   another between the option specifier and its argument.  
     */
    void append_opt(const textchar_t *opt, const textchar_t *arg)
    {
        /* append the leading space */
        append_inc(" ", 512);

        /* append the option */
        append_inc(opt, 512);

        /* append the argument, in quotes, with a leading space */
        append_qu(arg, TRUE);
    }
};

#endif /* W32TDB_H */

