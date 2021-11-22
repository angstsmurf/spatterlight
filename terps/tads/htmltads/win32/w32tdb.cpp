#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/w32tdb.cpp,v 1.4 1999/07/11 00:46:51 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32tdb.cpp - win32 HTML TADS debugger configuration 
Function
  
Notes
  
Modified
  01/31/98 MJRoberts  - Creation
*/

#include <ctype.h>
#include <stdio.h>

#include <Windows.h>
#include <commctrl.h>

/* include TADS OS interface */
#include "os.h"

#ifndef W32MAIN_H
#include "w32main.h"
#endif
#ifndef HTML_OS_H
#include "html_os.h"
#endif
#ifndef TADSWIN_H
#include "tadswin.h"
#endif
#ifndef HTMLW32_H
#include "htmlw32.h"
#endif
#ifndef W32TDB_H
#include "w32tdb.h"
#endif
#ifndef HTMLPREF_H
#include "htmlpref.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif
#ifndef HTMLRES_H
#include "htmlres.h"
#endif
#ifndef HTMLDBG_H
#include "htmldbg.h"
#endif
#ifndef HTMLFMT_H
#include "htmlfmt.h"
#endif
#ifndef W32EXPR_H
#include "w32expr.h"
#endif
#ifndef W32PRJ_H
#include "w32prj.h"
#endif
#ifndef HTMLDCFG_H
#include "htmldcfg.h"
#endif
#ifndef W32BPDLG_H
#include "w32bpdlg.h"
#endif
#ifndef W32FNDLG_H
#include "w32fndlg.h"
#endif
#ifndef HTMLTXAR_H
#include "htmltxar.h"
#endif
#ifndef W32DBGPR_H
#include "w32dbgpr.h"
#endif
#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif
#ifndef HTMLDISP_H
#include "htmldisp.h"
#endif
#ifndef FOLDSEL_H
#include "foldsel.h"
#endif
#ifndef TADSDOCK_H
#include "tadsdock.h"
#endif
#ifndef TADSDDE_H
#include "tadsdde.h"
#endif

#ifndef TADSIFID_H
#include "tadsifid.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Main window statusline panel indices 
 */

/* the line/column number indicator */
#define STATPANEL_LINECOL    1



/* ------------------------------------------------------------------------ */
/*
 *   MDI/SDI conditional code 
 */

#ifdef W32TDB_MDI

/*
 *   MDI version 
 */

/*
 *   create the main docking port in the MDI frame 
 */
CTadsDockportMDI *w32tdb_create_dockport(CHtmlSys_dbgmain *mainwin,
                                         HWND mdi_client)
{
    CTadsDockportMDI *port;

    /* create the port */
    port = new CTadsDockportMDI(mainwin, mdi_client);

    /* register the port and make it the default port */
    CTadsWinDock::add_port(port);
    CTadsWinDock::set_default_port(port);
    CTadsWinDock::set_default_mdi_parent(mainwin);

    /* return the new port */
    return port;
}

/* use the MDI frame system interface object for the frame */
CTadsSyswin *w32tdb_new_frame_syswin(CHtmlSys_dbgmain *win,
                                     int winmnuidx)
{
    return new CTadsSyswinMdiFrame(win, new CHtmlWinDbgMdiClient(win),
                                   winmnuidx, ID_WINDOW_FIRST, TRUE);
}

/* ------------------------------------------------------------------------ */
/*
 *   MDI client window for debugger 
 */

/*
 *   create 
 */
CHtmlWinDbgMdiClient::CHtmlWinDbgMdiClient(CHtmlSys_dbgmain *dbgmain)
{
    /* load my context menu */
    ctx_menu_ = LoadMenu(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDR_MDICLIENT_POPUP));

    /* remember the debugger main window */
    dbgmain_ = dbgmain;
    dbgmain_->AddRef();
}

/*
 *   destroy 
 */
CHtmlWinDbgMdiClient::~CHtmlWinDbgMdiClient()
{
    /* delete my context menu */
    DestroyMenu(ctx_menu_);

    /* release our reference on the debugger main window */
    dbgmain_->Release();
}

/*
 *   command status check 
 */
TadsCmdStat_t CHtmlWinDbgMdiClient::check_command(int command_id)
{
    /* send the command to the main debugger window for processing */
    return dbgmain_->check_command(command_id);
}

/*
 *   command processing 
 */
int CHtmlWinDbgMdiClient::do_command(int notify_code,
                                     int command_id, HWND ctl)
{
    /* send the command to the main debugger window for processing */
    return dbgmain_->do_command(notify_code, command_id, ctl);
}


#else /* ------------------------ W32TDB_MDI ----------------------------- */

/* 
 *   non-MDI version 
 */

/*
 *   Create the main docking port.  The SDI version does not support
 *   docking, since there's no main window for a docking port. 
 */
CTadsDockportMDI *w32tdb_create_dockport(CHtmlSys_dbgmain *mainwin,
                                         HANDLE mdi_client)
{
    /* no docking port in SDI mode */
    return 0;
}

/* use the standard system interface object for the frame */
CTadsSyswin *w32tdb_new_frame_syswin(CHtmlSys_dbgmain *win,
                                     int /*winmnuidx*/)
{
    return new CTadsSyswin(win);
}

#endif /* W32TDB_MDI */

/* ------------------------------------------------------------------------ */
/*
 *   Main debugger window.  We create only one of these for the entire
 *   application.  
 */
static CHtmlSys_dbgmain *S_main_win = 0;

/* delete the static source directory list */
static void delete_source_dir_list();

/* ------------------------------------------------------------------------ */
/*
 *   Pre-start processing.  If there are no arguments, simply start up in
 *   idle mode, with no workspace.  
 */
int w32_pre_start(int iter, int argc, char **argv)
{
    int restart;
    int reload;
    int terminate;
    int abort;
    CHtmlSys_mainwin *win;

    /* 
     *   On the first time through, create the debugger window with no
     *   engine context.  If there's a game name command line argument,
     *   load the game's configuration, but not the game.  If there's no
     *   game on the command line, simply load the default configuration.  
     */
    if (iter == 0)
        S_main_win = new CHtmlSys_dbgmain(0, argc > 1 ? argv[argc-1] : 0);

    /* get the game window */
    win = CHtmlSys_mainwin::get_main_win();

    /* set the initial "open" dialog directory on the first iteration */
    if (iter == 0 && win != 0 && win->get_prefs() != 0
        && win->get_prefs()->get_init_open_folder() != 0)
    {
        /* set the open file path */
        CTadsApp::get_app()
            ->set_openfile_path(win->get_prefs()->get_init_open_folder());
    }

    /* read events until the user tells us to start running the game */
    for (;;)
    {
        /* 
         *   if there's a game pending loading, load its configuration
         *   (this will have no effect if we've already loaded this game's
         *   configuration on another pass through this loop, since the
         *   configuration loader always checks for and skips loading the
         *   same file) 
         */
        if (win != 0 && win->get_pending_new_game() != 0)
            S_main_win->load_config(win->get_pending_new_game(), FALSE);

        /* run the event loop */
        if (!S_main_win->dbg_event_loop(0, 0, 0, &restart, &reload,
                                        &terminate, &abort, 0))
        {
            /* they want to shut down - delete everything */
            S_main_win->terminate_debugger();
            delete_source_dir_list();
            
            /* tell the caller to terminate */
            return FALSE;
        }

        /* 
         *   if they wish to begin execution, return so that the caller
         *   can enter TADS; otherwise, keep going, since we do not yet
         *   wish to begin execution
         */
        if (S_main_win->get_reloading_exec())
            return TRUE;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Perform post-quit processing.  Returns true if we should start over
 *   for another loop through the TADS main entrypoint, false if we should
 *   terminate the program.  
 */
int w32_post_quit(int main_ret_code)
{
    /* if there's no debugger main window, terminate */
    if (S_main_win == 0)
    {
        /* 
         *   if an error occurred, pause for a key so that the user has a
         *   chance to view the error 
         */
        if (main_ret_code != OSEXSUCC)
            os_expause();

        /* tell the caller to terminate */
        return FALSE;
    }

    /* the game is no longer loaded */
    S_main_win->set_game_loaded(FALSE);

    /* tell the caller to start over */
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/*
 *   Static list of directories we've seen in dbgu_find_src().  Each time
 *   we find a file, we'll remember the directory where we found it, since
 *   it's likely that there will be more files in the same place.  Then,
 *   if we're asked to locate another file, we'll check in all of the
 *   places we've found files previously before bothering the user with a
 *   new dialog.  
 */
class CDbg_win32_srcdir
{
public:
    /* the directory prefix */
    CStringBuf dir_;

    /* next item in our list */
    CDbg_win32_srcdir *next_;
};

/* head of our list of directories */
static CDbg_win32_srcdir *srcdirs_ = 0;

/* ------------------------------------------------------------------------ */
/*
 *   Delete static variables 
 */
static void delete_source_dir_list()
{
    /* delete all of the source directories we might have allocated */
    while (srcdirs_ != 0)
    {
        CDbg_win32_srcdir *nxt;

        /* remember the next item */
        nxt = srcdirs_->next_;

        /* delete this item */
        delete srcdirs_;

        /* move on to the next item */
        srcdirs_ = nxt;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Debugger user interface entrypoints.  The TADS code calls these
 *   entrypoints to let the debugger user interface take control at
 *   appropriate points. 
 */

/*
 *   This UI implementation allows moving the instruction pointer, so we
 *   can resume from an error. 
 */
int html_dbgui_err_resume(dbgcxdef *, void *)
{
    return TRUE;
}

/*
 *   Debugger user interface initialization.  The debugger calls this
 *   routine during startup to let the user interface set itself up.  
 */
void *html_dbgui_ini(dbgcxdef *ctx, const char *game_filename)
{
    /* 
     *   if we've already created the debugger window, use the existing
     *   one; otherwise, create a new one 
     */
    if (S_main_win != 0)
    {
        /* use the existing window */
        S_main_win->set_dbg_ctx(ctx);

        /* load the configuration for the new game */
        S_main_win->load_config(game_filename, TRUE);
    }
    else
    {
        /* allocate the main debugger control window */
        S_main_win = new CHtmlSys_dbgmain(ctx, game_filename);
    }

    /* return the main control window as the UI object */
    return S_main_win;
}

/*
 *   Debugger user interface initialization, phase two.  The debugger
 *   calls this routine during startup, after the .GAM file has been
 *   loaded, to let the user interface do any additional setup that
 *   depends on the .GAM file having been loaded. 
 */
void html_dbgui_ini2(dbgcxdef *ctx, void *ui_object)
{
    CHtmlSys_dbgmain *dbgmain;

    /* get the main control window from the debugger context */
    dbgmain = (CHtmlSys_dbgmain *)ui_object;

    /* tell the main control window to finish initialization */
    dbgmain->post_load_init(ctx);
}

/*
 *   Debugger user interface termination 
 */
void html_dbgui_term(dbgcxdef *ctx, void *ui_object)
{
    CHtmlSys_dbgmain *dbgmain;

    /* get the main control window from the debugger context */
    dbgmain = (CHtmlSys_dbgmain *)ui_object;

    /* if we found the main window, shut it down */
    if (dbgmain != 0)
    {
        /* add a reference to the main window while we're working */
        dbgmain->AddRef();

        /* 
         *   close the main debugger control window only if we're quitting
         *   out of the debugger 
         */
        if (dbgmain->is_quitting())
        {
            /* terminate the debugger */
            dbgmain->terminate_debugger();
            
            /* delete the static source directory list */
            delete_source_dir_list();
        }

        /* 
         *   since TADS is terminating, we must tell the main window to
         *   forget about its context 
         */
        dbgmain->notify_tads_shutdown();

        /* release our reference to the main window */
        dbgmain->Release();
    }
}

/*
 *   Debugger user interface - display an error 
 */
void html_dbgui_err(dbgcxdef *ctx, void * /*ui_object*/, int errno, char *msg)
{
    char buf[256];
    
    /* display the message in the debug log window */
    oshtml_dbg_printf("[TADS-%d: %s]\n", errno, msg);

    /* display it in the main game window as well */
    sprintf(buf, "[TADS-%d: %s]\n", errno, msg);
    os_printz(buf);
}

/*
 *   Callback for the folder selector dialog.  We'll look in the selected
 *   path for the desired file, and if it exists allow immediate dismissal
 *   of the dialog.  If they clicked the OK button on a folder that
 *   doesn't contain our file, we'll show a message box complaining about
 *   it and refuse to allow dismissal of the dialog.  
 */
static int dbgu_find_src_cb(void *ctx, const char *path, int okbtn, HWND dlg)
{
    const char *origname;
    char buf[OSFNMAX + 128];
    size_t len;
    
    /* get the original name - we used this as the callback context */
    origname = (const char *)ctx;

    /* get the path */
    len = strlen(path);
    memcpy(buf, path, len);

    /* append backslash if it doesn't end in one already */
    if (len == 0 || buf[len-1] != '\\')
        buf[len++] = '\\';

    /* append the filename */
    strcpy(buf + len, origname);

    /* 
     *   check to see if the file exists - if it does, dismiss the dialog
     *   by returning true 
     */
    if (!osfacc(buf))
        return TRUE;

    /* 
     *   We didn't find the file - we can't dismiss the dialog yet.  If
     *   the user clicked OK, complain about it.  (Don't complain if they
     *   merely double-clicked a directory path, since they may just be
     *   navigating.)  
     */
    if (okbtn)
    {
        sprintf(buf, "This folder does not contain the file \"%s\".",
                origname);
        MessageBox(dlg, buf, "File Not Found", MB_OK | MB_ICONEXCLAMATION);
    }

    /* don't allow dismissal */
    return FALSE;
}

/*
 *   Find a source file using UI-dependent mechanisms.  If we aren't
 *   required to find the file, we'll do nothing except return success, to
 *   tell the caller to defer asking us to find the file until they really
 *   need it.  If we are required to find the file, we'll use a standard
 *   file open dialog to ask the user to locate the file. 
 */
int html_dbgui_find_src(const char *origname, int origlen,
                        char *fullname, size_t full_len, int must_find_file)
{
    char title_buf[OSFNMAX + 128];
    char prompt_buf[OSFNMAX + 128];
    CDbg_win32_srcdir *srcdir;
    size_t len;
    CHtmlDbgProjWin *projwin;

    /* 
     *   if we don't *really* need to find it, return success, to indicate
     *   to the caller that they should ask us again when it's actually
     *   important -- this way, we'll avoid pestering the user with a
     *   bunch of file-open requests on startup, and instead ask the user
     *   for files one at a time as they're actually needed, which is much
     *   less irritating 
     */
    if (!must_find_file)
    {
        fullname[0] = '\0';
        return TRUE;
    }

    /* check for an absolute path */
    if (os_is_file_absolute(origname))
    {
        /* 
         *   we have an absolute path - first, check to see if the file
         *   exists at the path, and if so, just use the given path 
         */
        if (!osfacc(origname))
        {
            /* this is the correct path - use the original name as-is */
            strcpy(fullname, origname);
            return TRUE;
        }

        /*
         *   The file doesn't exist at this path.  Drop the path part and
         *   look for the filename in the usual manner.  
         */
        origname = os_get_root_name((char *)origname);
    }

    /*
     *   They do require the file.  First, scan the current project list to
     *   see if we can find a file with the same root name.  If we can, then
     *   use the path to that file.  
     */
    if (S_main_win != 0 && (projwin = S_main_win->get_proj_win()) != 0)
    {
        /* ask the project window to find the file */
        if (projwin->find_file_path(fullname, full_len, origname))
            return TRUE;
    }

    /*
     *   We didn't find the file in the project list, so check our cached
     *   directory list, to see if it's in a directory we've already
     *   visited.  
     */
    for (srcdir = srcdirs_ ; srcdir != 0 ; srcdir = srcdir->next_)
    {
        /* build the full path to this file */
        os_build_full_path(fullname, full_len, srcdir->dir_.get(), origname);

        /* if the file exists, return success */
        if (!osfacc(fullname))
            return TRUE;
    }

    /*
     *   We couldn't find it in our cached directory list.  Put up the
     *   folder-browser dialog to ask the user to locate the file.
     */
    sprintf(title_buf, "%s - Please Locate File", origname);
    sprintf(prompt_buf, "&Path to file \"%s\":", origname);
    if (!CTadsDialogFolderSel2::
        run_select_folder(S_main_win != 0 ? S_main_win->get_handle() : 0,
                          CTadsApp::get_app()->get_instance(),
                          prompt_buf, title_buf, fullname, full_len, 0,
                          dbgu_find_src_cb, (void *)origname))
    {
        /* they cancelled - return failure */
        fullname[0] = '\0';
        return FALSE;
    }
        
    /* if the path doesn't end in a backslash, add one */
    len = get_strlen(fullname);
    if (len == 0 || (fullname[len-1] != '\\' && fullname[len-1] != '/'))
        fullname[len++] = '\\';

    /* append the filename to the path */
    strcpy(fullname + len, origname);
    
    /* add the prefix to our list */
    srcdir = new CDbg_win32_srcdir;
    srcdir->dir_.set(fullname, len);
    srcdir->next_ = srcdirs_;
    srcdirs_ = srcdir;
    
    /* success */
    return TRUE;
}


/*
 *   Debugger user interface main command entrypoint
 */
void html_dbgui_cmd(dbgcxdef *ctx, void *ui_object,
                    int bphit, int err, unsigned int *exec_ofs)
{
    int restart;
    int abort;
    int reload;
    int terminate;
    CHtmlSys_dbgmain *dbgmain;

    /* get the main control window from the debugger context */
    dbgmain = (CHtmlSys_dbgmain *)ui_object;

    /* invoke the debugger event loop in the debugger control window */
    restart = abort = FALSE;
    if (dbgmain != 0
        && !dbgmain->dbg_event_loop(ctx, bphit, err, &restart,
                                    &reload, &terminate, &abort, exec_ofs))
    {
        /* 
         *   the application was terminated within the event loop - signal
         *   a TADS error so that TADS shuts itself down 
         */
        CHtmlDebugHelper::signal_quit(ctx);
    }

    /* 
     *   if the event loop told us to reload a new game, shut down TADS so
     *   that we can start over with the new game file 
     */
    if (reload || terminate)
        CHtmlDebugHelper::signal_quit(ctx);

    /* if the event loop said to restart, do so now */
    if (restart)
        CHtmlDebugHelper::signal_restart(ctx);

    /* if the event loop said to abort, do so now */
    if (abort)
        CHtmlDebugHelper::signal_abort(ctx);
}

/*
 *   Quitting.  Enter the debugger user interface event loop as normal.
 *   If the event loop says to keep going, restart the game. 
 */
void html_dbgui_quitting(dbgcxdef *ctx, void *ui_object)
{
    CHtmlSys_dbgmain *mainwin;

    /* get our UI main window from the engine context */
    mainwin = (CHtmlSys_dbgmain *)ui_object;

    /* 
     *   show a message about the game quitting; if the debugger itself is
     *   quitting, there's no need to do this, since the user already
     *   knows that we're quitting 
     */
    if (!mainwin->is_quitting() && !mainwin->is_reloading()
        && !mainwin->is_terminating()
        && mainwin->get_global_config_int("gameover dlg", 0, TRUE))
        MessageBox(0, "The game has terminated.", "TADS Workbench",
                   MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);

    /* 
     *   if a re-load or game termination is in progress, simply return,
     *   so that TADS can shut down, allowing the outer loop to start a
     *   new TADS session 
     */
    if (mainwin->is_reloading() || mainwin->is_terminating())
    {
        /* put the game in single-step mode */
        mainwin->exec_step_into();

        /* let TADS shut down so we can start over again */
        return;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Debugger main frame window implementation
 */

/*
 *   Get the application-wide debugger main window object 
 */
CHtmlSys_dbgmain *CHtmlSys_dbgmain::get_main_win()
{
    return S_main_win;
}

/*
 *   instantiate 
 */
CHtmlSys_dbgmain::CHtmlSys_dbgmain(dbgcxdef *ctx, const char *argv_last)
{
    CHtmlSys_mainwin *main_win;
    char modname[OSFNMAX];
    char *rootname;
    int int_val;
    int startup_mode;
    size_t len;
    char buf[256];

    /* 
     *   fetch and remember the original TADSLIB variable we inherit from our
     *   parent environment 
     */
    len = GetEnvironmentVariable("TADSLIB", buf, sizeof(buf));
    if (len != 0)
    {
        char *bufp;

        /* 
         *   if the value was too long for our trial-size buffer, allocate a
         *   bigger buffer 
         */
        bufp = buf;
        if (len >= sizeof(buf))
        {
            /* allocate the space */
            bufp = (char *)th_malloc(len);

            /* get the full value */
            len = GetEnvironmentVariable("TADSLIB", bufp, len);
        }

        /* remember it */
        orig_tadslib_var_.set(bufp);

        /* free the buffer if we allocated one */
        if (bufp != buf)
            th_free(bufp);
    }
    else
        orig_tadslib_var_.set("");

    /* 
     *   Build the name of the default configuration file - this is a file
     *   with a name given by w32_tdb_global_prefs_file, in the same
     *   directory as our executable 
     */
    GetModuleFileName(0, modname, sizeof(modname));
    rootname = os_get_root_name(modname);
    strcpy(rootname, w32_tdb_global_prefs_file);
    global_config_filename_.set(modname);

    /* create and load the global configuration object */
    global_config_ = new CHtmlDebugConfig();
    vsn_load_config_file(global_config_, global_config_filename_.get());

    /* read and cache the auto-flush preference setting */
    if (global_config_->getval("gamewin auto flush", 0, &int_val))
        int_val = TRUE;
    auto_flush_gamewin_ = int_val;

    /* no MDI docking port yet */
    mdi_dock_port_ = 0 ;

    /* no toolbar yet */
    toolbar_ = 0;

    /* no statusline yet */
    statusline_ = 0;

    /* get the statusline visibility from the configuration */
    if (global_config_->getval("statusline", "visible", &int_val))
        int_val = TRUE;
    show_statusline_ = int_val;

    /* remember the context */
    dbgctx_ = ctx;

    /* create a helper object (it'll implicitly add a reference to itself) */
    helper_ = new CHtmlDebugHelper(vsn_need_srcfiles());

    /* we don't have a local configuration object yet */
    local_config_ = 0;

    /* no accelerators yet */
    accel_ = 0;

    /* no watch/local/project windows yet */
    watchwin_ = 0;
    localwin_ = 0;
    projwin_ = 0;

    /* not loading a window configuration yet */
    loading_win_config_ = FALSE;
    loading_win_defer_list_ = 0;

    /* no find dialog yet */
    find_dlg_ = 0;

    /* load my accelerators */
    accel_ = LoadAccelerators(CTadsApp::get_app()->get_instance(),
                              MAKEINTRESOURCE(IDR_ACCEL_DEBUGMAIN));

    /* 
     *   Establish our accelerator.  Since the game window is always
     *   created prior to starting the debugger, we can be confident at
     *   this point that our accelerator will take precedence over the
     *   game window's (by virtue of having been established later).  This
     *   is what we want, because the debugger window is initially the
     *   active window.  
     */
    CTadsApp::get_app()->set_accel(accel_, get_handle());

    /* we have no workspace yet */
    workspace_open_ = FALSE;

    /* no game is loaded */
    game_loaded_ = FALSE;

    /* not building yet */
    build_running_ = FALSE;
    capture_build_output_ = FALSE;
    filter_build_output_ = FALSE;
    run_after_build_ = FALSE;

    /* not waiting for a build yet */
    build_wait_dlg_ = 0;

    /* no build commands in the list yet */
    build_cmd_head_ = build_cmd_tail_ = 0;

    /* clear various status flags */
    in_debugger_ = FALSE;
    quitting_ = FALSE;
    restarting_ = FALSE;
    reloading_ = FALSE;
    reloading_new_ = FALSE;
    reloading_go_ = FALSE;
    reloading_exec_ = FALSE;
    terminating_ = FALSE;
    term_in_progress_ = FALSE;
    aborting_cmd_ = FALSE;
    refreshing_sources_ = FALSE;
    profiling_ = FALSE;

    /* use the main window's preferences object */
    main_win = CHtmlSys_mainwin::get_main_win();
    prefs_ = (main_win != 0 ? main_win->get_prefs() : 0);

    /* make the "default" profile the active one */
    if (prefs_ != 0)
    {
        /* make the "default" profile active */
        prefs_->set_active_profile_name("Default");

        /* load it */
        prefs_->restore(FALSE);
    }

    /* no active window yet */
    active_dbg_win_ = 0;
    last_active_dbg_win_ = 0;
    post_menu_active_win_ = 0;

    /* create the find dialog */
    if (find_dlg_ == 0)
        find_dlg_ = new CTadsDialogFind();

    /* no "Find" text yet (so we can't "find next") */
    find_text_[0] = '\0';

    /* 
     *   Get the startup mode from the configuration file.  If no mode has
     *   been selected, the default is mode 1, "show welcome dialog."  
     */
    if (global_config_->getval("startup", "action", &startup_mode))
        startup_mode = 1;

    /* 
     *   load the configuration for the game, if a game was specified on the
     *   command line 
     */
    if (argv_last != 0)
    {
        char fname[OSFNMAX];
        char *root;

        /* get the full path name to the file */
        GetFullPathName(argv_last, sizeof(fname), fname, &root);

        /* adjust to the proper extension */
        vsn_adjust_argv_ext(fname);

        /* 
         *   set the initial Open File dialog directory to the game's
         *   directory 
         */
        CTadsApp::get_app()->set_openfile_dir(fname);

        /* load the game's configuration */
        if (load_config(fname, ctx != 0) == 0)
        {
            /* we've successfully loaded the configuration, so we're done */
            return;
        }
        else
        {
            char msg[256 + OSFNMAX];
            
            /* couldn't load the configuration - mention this */
            sprintf(msg, "Unable to open project file \"%s\".", fname);
            MessageBox(main_win->get_handle(), msg, "Error", MB_OK);

            /* proceed with startup mode 3, "start with no project" */
            startup_mode = 3;
        }
    }

    /* 
     *   We either didn't get a command-line argument telling us the game to
     *   load, or we were unsuccessful loading the specified game.  In either
     *   case, proceed according to the startup mode we've selected.
     */
    switch(startup_mode)
    {
    case 1:
        /* show the "welcome" dialog after loading the default config */
        load_config(0, FALSE);
        CHtmlDbgWiz::run_start_dlg(this);
        break;
        
    case 2:
        /* load the most recent project */
        if (!load_recent_game(ID_GAME_RECENT_1))
        {
            /* failed to load - start with the default config */
            load_config(0, FALSE);
        }
        break;
        
    case 3:
        /* start with no project - simply load the default config */
        load_config(0, FALSE);
        break;
    }
}

/*
 *   Get an integer variable value from the configuration file 
 */
int CHtmlSys_dbgmain::get_global_config_int(const char *varname,
                                            const char *subvar,
                                            int default_val) const
{
    int val;
    
    /* if there's no configuration, return the default */
    if (global_config_ == 0)
        return default_val;

    /* read the variable; if it doesn't exist, return the default */
    if (global_config_->getval(varname, subvar, &val))
        return default_val;

    /* return the value we read */
    return val;
}

void CHtmlSys_dbgmain::add_recent_game(const char *new_game)
{
    int i;
    int j;
    char order[10];
    char first;
    CHtmlPreferences *prefs;

    /* 
     *   get the main window's preferences object (we store the recent
     *   game list here, rather than in the debug config, because this
     *   information is global and so doesn't belong with a single game) 
     */
    if (CHtmlSys_mainwin::get_main_win() == 0)
        return;
    prefs = CHtmlSys_mainwin::get_main_win()->get_prefs();

    /* get the order string */
    strncpy(order, prefs->get_recent_dbg_game_order(), 4);
    order[4] = '\0';

    /* 
     *   First, search the current list to see if the game is already
     *   there.  If it is, we simply shuffle the list to move this game to
     *   the front of the list.  
     */
    for (i = 0 ; i < 4 ; ++i)
    {
        const textchar_t *fname;
        
        /* get this item */
        fname = prefs->get_recent_dbg_game(order[i]);
        if (fname == 0 || fname[0] == '\0')
            break;

        /* if this is the same game, we can simply shuffle the list */
        if (get_stricmp(new_game, fname) == 0)
        {
            /* 
             *   It's already in the list - simply reorder the list to put
             *   this item at the front of the list.  To do this, we
             *   simply move all the items down one until we get to its
             *   old position.  First, note the new first item.  
             */
            first = order[i];

            /* move everything down one slot */
            for (j = i ; j > 0 ; --j)
                order[j] = order[j - 1];

            /* fill in the first item */
            order[0] = first;

            /* we're done */
            goto done;
        }
    }

    /* 
     *   Okay, it's not already in the list, so we need to add it to the
     *   list.  To do this, drop the last item off the list, and move
     *   everything else down one slot.  First, note the current last slot
     *   - we're going to drop the file that's currently there and make it
     *   the new first slot.  
     */
    first = order[3];

    /* move everything down one position */
    for (j = 3 ; j > 0 ; --j)
        order[j] = order[j - 1];

    /* re-use the old last slot as the new first slot */
    order[0] = first;

    /* set the new first filename */
    prefs->set_recent_dbg_game(order[0], new_game);

done:
    /* set the new ordering */
    prefs->set_recent_dbg_game_order(order);

    /* 
     *   save the updated settings immediately, to ensure they're safely on
     *   disk in case we should eventually crash 
     */
    prefs->save();
    
    /* rebuild the game list */
    update_recent_game_menu();
}

/*
 *   Update the menu showing recent games 
 */
void CHtmlSys_dbgmain::update_recent_game_menu()
{
    HMENU file_menu;
    int i;
    int found;
    int id;
    const textchar_t *order;
    const textchar_t *fname;
    MENUITEMINFO info;
    const textchar_t *working_dir;
    HMENU recent_menu;
    CHtmlPreferences *prefs;

    /* 
     *   get the main window's preferences object (we store the recent
     *   game list here, rather than in the debug config, because this
     *   information is global and so doesn't belong with a single game) 
     */
    if (CHtmlSys_mainwin::get_main_win() == 0)
        return;
    prefs = CHtmlSys_mainwin::get_main_win()->get_prefs();

    /* get the File menu - it's the first submenu */
    file_menu = GetSubMenu(GetMenu(handle_), 0);

    /*
     *   Find the recent game submenu in the File menu.  This is the item
     *   that contains subitems; all of the subitems will have ID's in the
     *   range ID_GAME_RECENT_1 to ID_GAME_RECENT_4, or
     *   ID_GAME_RECENT_NONE. 
     */
    for (i = 0, found = FALSE ; i < GetMenuItemCount(file_menu) ; ++i)
    {
        /* 
         *   get this item's submenu - if it doesn't have one, it's not
         *   what we're looking for 
         */
        recent_menu = GetSubMenu(file_menu, i);
        if (recent_menu == 0)
            continue;

        /* get the ID of the first item in the submenu */
        id = GetMenuItemID(recent_menu, 0);

        /* if this is one of the one's we're looking for, we're done */
        if ((id >= ID_GAME_RECENT_1 && id <= ID_GAME_RECENT_4)
            || id == ID_GAME_RECENT_NONE)
        {
            /* note that we found it */
            found = TRUE;

            /* stop searching */
            break;
        }
    }

    /* if we found a recent game item, delete all of them */
    if (found)
    {
        /* delete all of the recent game items */
        while (GetMenuItemCount(recent_menu) != 0)
            DeleteMenu(recent_menu, 0, MF_BYPOSITION);
    }

    /* get the game ordering string */
    order = prefs->get_recent_dbg_game_order();

    /* set up the basic MENUITEMINFO entries */
    info.cbSize = menuiteminfo_size_;
    info.fMask = MIIM_TYPE | MIIM_DATA | MIIM_ID;
    info.fType = MFT_STRING;

    /* 
     *   check to see if we have any recent games in the prefs - if the
     *   first entry is blank, there aren't any games 
     */
    fname = prefs->get_recent_dbg_game(order[0]);
    if (fname == 0 || fname[0] == '\0')
    {
        /* re-insert the "no recent games" placeholder */
        info.wID = ID_GAME_RECENT_NONE;
        info.dwTypeData = "No Recent Games";
        info.cch = strlen((char *)info.dwTypeData);
        InsertMenuItem(recent_menu, 0, TRUE, &info);

        /* we're done */
        return;
    }

    /* 
     *   Get the open-file directory - we'll show all of the filenames in
     *   the recent file list relative to this directory.  Use the current
     *   open-file dialog start directory.  
     */
    working_dir = CTadsApp::get_app()->get_openfile_dir();

    /* insert as many items as we have defined */
    for (i = 0 ; i < 4 && i < (int)get_strlen(order) ; ++i)
    {
        char buf[OSFNMAX + 10];

        /* get this filename */
        fname = prefs->get_recent_dbg_game(order[i]);
        if (fname == 0 || fname[0] == '\0')
            break;

        /* build the menu string prefix */
        sprintf(buf, "&%d ", i + 1);

        /* get the filename relative to the open-file-dialog directory */
        if (working_dir != 0)
        {
            /* get the path relative to the working directory */
            oss_get_rel_path(buf + strlen(buf), sizeof(buf) - strlen(buf),
                             fname, working_dir);
        }
        else
        {
            /* we have no working directory - use the absolute path */
            strncpy(buf + strlen(buf), fname, sizeof(buf) - strlen(buf));
            buf[sizeof(buf) - 1] = '\0';
        }

        /* add this item */
        info.wID = ID_GAME_RECENT_1 + i;
        info.dwTypeData = buf;
        info.cch = strlen(buf);
        InsertMenuItem(recent_menu, i, TRUE, &info);
    }
}

/*
 *   Load the configuration for a game file.  Returns zero on success,
 *   non-zero if we fail to load the game.  
 */
int CHtmlSys_dbgmain::load_config(const char *load_filename, int game_loaded)
{
    RECT rc;
    RECT deskrc;
    CHtmlRect winpos;
    int winmax;
    textchar_t game_filename_buf[OSFNMAX];
    textchar_t *game_filename;
    textchar_t config_filename[OSFNMAX];
    int vis;
    int default_pos;
    int had_old_config;
    int new_config_exists;
    int i;

    /* get the filenames for the game and configuration files */
    if (!vsn_get_load_profile(&game_filename, game_filename_buf,
                              config_filename, load_filename, game_loaded))
        return 1;

    /* note whether or not the new configuration file exists */
    new_config_exists = (load_filename != 0 && !osfacc(config_filename));

    /* note whether we had an old configuration */
    had_old_config = (local_config_ != 0);

    /* if we have a configuration, save it, since we're about to replace it */
    if (local_config_ != 0 && local_config_filename_.get() != 0)
        vsn_save_config_file(local_config_, local_config_filename_.get());

    /* set the new config file name */
    if (load_filename != 0)
    {
        char full_path[OSFNMAX];
        char *root_name;
        
        /* set the new configuration filename */
        local_config_filename_.set(config_filename);

        /* we now have a workspace open */
        workspace_open_ = TRUE;

        /* 
         *   Set the working directory to the directory containing the
         *   config file.  First, get the absolute path to the config
         *   file, then set the working directory to the path prefix part.
         */
        if (GetFullPathName(config_filename, sizeof(full_path), full_path,
                            &root_name) != 0)
        {
            /* 
             *   remember the full path of the file, so that we have no
             *   dependency on the working directory 
             */
            local_config_filename_.set(full_path);

            /* clear out everything from the final separator on */
            if (root_name != 0)
            {
                /* if it ends with a '\' or '/', remove the trailing slash */
                if (root_name != full_path
                    && (*(root_name - 1) == '\\' || *(root_name - 1) == '/'))
                    *--root_name = '\0';

                /* 
                 *   if it's now empty, or it ends with a ':', add a '\' -
                 *   unlike any other directory path, the root directory
                 *   of a drive ends with a backslash 
                 */
                if (root_name == full_path || *(root_name - 1) == ':')
                    strcpy(root_name, "\\");

                /* set the working directory */
                SetCurrentDirectory(full_path);
            }
        }
    }


    /* delete our old source directory list */
    delete_source_dir_list();

    /* close all source windows */
    helper_->enum_source_windows(&enum_win_close_cb, this);

    /* remember the new game filename */
    if (game_filename != 0)
        game_filename_.set(game_filename);

    /*
     *   If we had a previous configuration, and the new configuration
     *   doesn't exist, don't do anything - just clear out some stuff from
     *   the old configuration, but keep the rest as the starting point of
     *   the new configuration.  
     */
    if (had_old_config && !new_config_exists)
    {
        /* clear out the parts that don't stay around in a default config */
        clear_non_default_config();
        
        /* clear any breakpoints that are hanging around */
        helper_->delete_internal_bps();

        /* clear any line sources that were loaded */
        helper_->delete_internal_sources();

        /* set the debugger defaults for the new configuration */
        set_dbg_defaults(this);

        /* set the build defaults for the new configuration */
        set_dbg_build_defaults(this, get_gamefile_name());

        /* 
         *   skip the configuration loading, since there's no prior
         *   configuration to load; just skip ahead to where we finish up
         *   with the initialization 
         */
        goto finish_load;
    }

    /* 
     *   if we already have a configuration object, delete it, so that we
     *   can start fresh with a new configuration 
     */
    if (local_config_ != 0)
    {
        /* delete the old configuration */
        delete local_config_;
        local_config_ = 0;
    }

    /*
     *   Close the docking windows, so that we can open them in their new
     *   state.
     */

    /* if we previously had a watch or local window, close them */
    if (watchwin_ != 0)
        SendMessage(GetParent(watchwin_->get_handle()), DOCKM_DESTROY, 0, 0);
    if (localwin_ != 0)
        SendMessage(GetParent(localwin_->get_handle()), DOCKM_DESTROY, 0, 0);

    /* close any extra windows that this debugger version uses */
    vsn_load_config_close_windows();
    
    /* likewise, close the stack, debug, and call trace windows */
    if (helper_->get_stack_win() != 0)
        ((CHtmlSysWin_win32 *)helper_->get_stack_win())->close_window(TRUE);
    if (helper_->get_hist_win() != 0)
        ((CHtmlSysWin_win32 *)helper_->get_hist_win())->close_window(TRUE);
    if (helper_->get_debuglog_win() != 0)
        SendMessage(GetParent(GetParent(((CHtmlSysWin_win32 *)helper_->
                                         get_debuglog_win())->get_handle())),
                    DOCKM_DESTROY, 0, 0);

    /*
     *   Next, set up for the new game 
     */

    /* create a new local configuration object */
    local_config_ = new CHtmlDebugConfig();

    /* 
     *   read the configuration file - if the new configuration file
     *   exists, read it; otherwise, load the default configuration file 
     */
    if (new_config_exists)
    {
        /* load the configuration file */
        vsn_load_config_file(local_config_, local_config_filename_.get());
    }
    else if (global_config_filename_.get() != 0)
    {
        /* no game configuration file - read the default configuration */
        vsn_load_config_file(local_config_, global_config_filename_.get());
    }

    /* 
     *   now that we've loaded the configuration, get the name of the game
     *   file; for t3 workbench, we can't know the game filename until here 
     */
    if (new_config_exists
        && vsn_get_load_profile_gamefile(game_filename_buf))
        game_filename_.set(game_filename_buf);

    /* mark that configuration loading is in progress */
    loading_win_config_ = TRUE;

    /* load the source directory list */
    rebuild_srcdir_list();

    /* set initial configuration options */
    on_update_srcwin_options();

    /* read my stored position from the preferences */
    if (!local_config_->getval("main win pos", 0, &winpos)
        && !local_config_->getval("main win max", 0, &winmax))
    {
        /* move the window */
        SetRect(&rc, winpos.left, winpos.top,
                winpos.right - winpos.left, winpos.bottom - winpos.top);
        default_pos = FALSE;

        /* if the window is off the screen, use the default sizes */
        GetClientRect(GetDesktopWindow(), &deskrc);
        if (!winmax
            && (rc.left < deskrc.left || rc.left + rc.right > deskrc.right
                || rc.top < deskrc.top || rc.top + rc.bottom > deskrc.bottom
                || rc.right < 20 || rc.bottom < 20))
        {
            /* the stored position looks invalid - use a default position */
            w32tdb_set_default_frame_rc(&rc);
            default_pos = TRUE;
        }
    }
    else
    {
        /* no configuration - use the default position */
        w32tdb_set_default_frame_rc(&rc);
        winmax = FALSE;
        default_pos = TRUE;
    }

    /* 
     *   open my window if we haven't already; otherwise, move the window
     *   to the new position if we have a saved position 
     */
    if (handle_ == 0)
    {
        HICON ico;
            
        /* create the window in the default location */
        create_system_window(0, !winmax, "TADS Workbench", &rc,
                             w32tdb_new_frame_syswin(this, -2));

        /* set it up with our special icons - large... */
        ico = (HICON)LoadImage(CTadsApp::get_app()->get_instance(),
                               MAKEINTRESOURCE(IDI_DBGWIN),
                               IMAGE_ICON, 32, 32, 0);
        SendMessage(handle_, WM_SETICON, ICON_BIG, (LPARAM)ico);

        /* ... and small */
        ico = (HICON)LoadImage(CTadsApp::get_app()->get_instance(),
                               MAKEINTRESOURCE(IDI_DBGWIN),
                               IMAGE_ICON, 16, 16, 0);
        SendMessage(handle_, WM_SETICON, ICON_SMALL, (LPARAM)ico);

        /* create and register the MDI client docking port */
        mdi_dock_port_ = w32tdb_create_dockport(this,
                                                get_parent_of_children());

        /* maximize it if appropriate */
        if (winmax)
            ShowWindow(handle_, SW_SHOWMAXIMIZED);
    }
    else if (!default_pos)
    {
        /* move the existing window to the saved position */
        if (winmax)
            ShowWindow(handle_, SW_SHOWMAXIMIZED);
        else
            MoveWindow(handle_, rc.left, rc.top,
                       rc.right, rc.bottom, TRUE);
    }

    /* 
     *   load the docking configuration - note that we need to finish
     *   creating the main window before we do this, because we can't load
     *   the docking configuration until we have a dockport, and we don't
     *   have a dockport until we have a main window 
     */
    load_dockwin_config(local_config_);

    /* 
     *   Get the desktop window area, so that we can set up default
     *   positions for our windows at the edges of the desktop area.  This
     *   will retrieve the area of the entire Windows desktop in SDI mode,
     *   or of the MDI client area in MDI mode.  
     */
    w32tdb_get_desktop_rc(this, &deskrc);

    /* create the local variables window */
    localwin_ = new CHtmlDbgWatchWinLocals();
    localwin_->init_panels(this, helper_);
    SetRect(&rc, deskrc.left + 2, deskrc.bottom - 2 - 150, 300, 150);
    localwin_->load_win_config(local_config_, "locals win", &rc, &vis);
    load_dockwin_config_win(local_config_, "locals win", localwin_,
                            "Locals", vis, TADS_DOCK_ALIGN_BOTTOM,
                            270, 170, &rc, TRUE);

    /* create the watch window */
    watchwin_ = new CHtmlDbgWatchWinExpr();
    watchwin_->init_panels(this, helper_);
    SetRect(&rc, deskrc.left+2+300+2, deskrc.bottom-2-150, 300, 150);
    watchwin_->load_win_config(local_config_, "watch win", &rc, &vis);
    load_dockwin_config_win(local_config_, "watch win", watchwin_,
                            "Watch Expressions", vis, TADS_DOCK_ALIGN_BOTTOM,
                            270, 170, &rc, TRUE);
    watchwin_->load_panel_config(local_config_, "watch win");

    /* create the stack window if appropriate */
    if (local_config_->getval("stack win vis", 0, &vis) || vis)
        helper_->create_stack_win(this);

    /* create the history window if appropriate */
    if (!local_config_->getval("hist win vis", 0, &vis) && vis)
        helper_->create_hist_win(this);

    /* create the debug log window */
    helper_->create_debuglog_win(this);

    /* create version-specific windows */
    vsn_load_config_open_windows(&deskrc);

    /* load search configuration */
    if (local_config_->getval("search", "exact case", &search_exact_case_))
        search_exact_case_ = FALSE;
    if (local_config_->getval("search", "wrap", &search_wrap_))
        search_wrap_ = TRUE;
    if (local_config_->getval("search", "dir", &search_dir_))
        search_dir_ = 1;
    if (local_config_->getval("search", "start at top", &search_from_top_))
        search_from_top_ = FALSE;
    local_config_->getval("search", "text", 0, find_text_, sizeof(find_text_));
    load_find_config(local_config_, "search dlg");

    /* 
     *   If there's no game loaded, load the source file list from the
     *   configuration.  Don't do this if there's a game loaded, because
     *   we'll want to use the up-to-date source list in the compiled game
     *   file instead in this case. 
     */
    if (!game_loaded)
        helper_->init_after_load(0, this, local_config_, FALSE);

    /*
     *   Load the source windows that were open at the end of the last
     *   session, if any 
     */
    i = local_config_->get_strlist_cnt("src win list", 0);
    while (i > 0)
    {
        textchar_t path[OSFNMAX];
        textchar_t fname[OSFNMAX];

        /* get the next window's name */
        --i;
        if (!local_config_->getval("src win list", 0, i, path, sizeof(path))
            && !local_config_->getval("src win title", 0, i,
                                      fname, sizeof(fname)))
        {
            /* open the file */
            helper_->open_text_file(this, fname, path);
        }
    }

    /* set the debugger defaults */
    set_dbg_defaults(this);

    /* set the build defaults */
    set_dbg_build_defaults(this, get_gamefile_name());

finish_load:
    /* 
     *   load the file menu with the list of sources we just loaded, if we
     *   are not yet running the game 
     */
    if (!game_loaded)
        load_file_menu();

    /* not yet quitting, restarting, reloading, or aborting */
    quitting_ = FALSE;
    restarting_ = FALSE;
    reloading_ = FALSE;
    reloading_new_ = FALSE;
    reloading_go_ = FALSE;
    reloading_exec_ = FALSE;
    terminating_ = FALSE;
    term_in_progress_ = FALSE;
    aborting_cmd_ = FALSE;

    /* not yet refreshing sources */
    refreshing_sources_ = FALSE;

    /* done loading */
    loading_win_config_ = FALSE;

    /* 
     *   if there are any deferred-visibility windows, post a message so
     *   that we'll show them - we must post a message rather than showing
     *   them now because we must wait until after we've figured the final
     *   docking window layout, which is caused by messages that we posted
     *   while loading the docking windows 
     */
    if (loading_win_defer_list_ != 0)
        PostMessage(handle_, HTMLM_LOAD_CONFIG_DONE, 0, 0);

    /* add this game to the list of recently-opened games */
    if (load_filename != 0)
        add_recent_game(local_config_filename_.get());

    /* update the window title */
    vsn_set_win_title();

    /* success */
    return 0;
}

/*
 *   Save configuration information 
 */
int CHtmlSys_dbgmain::save_find_config(CHtmlDebugConfig *config,
                                       const textchar_t *varname)
{
    int i;

    /* forget any old list of search strings in the configuration */
    config->clear_strlist(varname, "old searches");

    /* save the new ones */
    for (i = 0 ; i < find_dlg_->get_max_old_search_strings() ; ++i)
    {
        const char *str;
        
        /* get this string */
        str = find_dlg_->get_old_search_string(i);

        /* if it's null, we've reached the end of the list */
        if (str == 0)
            break;

        /* save the string */
        config->setval(varname, "old searches", i, str);
    }

    /* success */
    return 0;
}

/*
 *   Load configuration information for the 'find' dialog
 */
int CHtmlSys_dbgmain::load_find_config(CHtmlDebugConfig *config,
                                       const textchar_t *varname)
{
    int i;
    int cnt;

    /* forget any old strings */
    find_dlg_->forget_old_search_strings();

    /* get the number of strings to load */
    cnt = config->get_strlist_cnt(varname, "old searches");

    /* load them */
    for (i = 0 ; i < cnt ; ++i)
    {
        textchar_t buf[512];
        size_t len;

        /* get this string - return an error if we couldn't load it */
        if (config->getval(varname, "old searches", i,
                           buf, sizeof(buf), &len))
            return 1;

        /* save it */
        find_dlg_->set_old_search_string(i, buf, len);
    }

    /* success */
    return 0;
}

/*
 *   Load the source directory list from the configuration object 
 */
void CHtmlSys_dbgmain::rebuild_srcdir_list()
{
    int i;
    int cnt;

    /* delete our old source directory list */
    delete_source_dir_list();

    /* get the number of entries in the configuration list */
    cnt = local_config_->get_strlist_cnt("srcdir", 0);

    /* load each entry */
    for (i = 0 ; i < cnt ; ++i)
    {
        textchar_t buf[OSFNMAX];
        CDbg_win32_srcdir *srcdir;

        /* load the next source directory */
        if (!local_config_->getval("srcdir", 0, i, buf, sizeof(buf)))
        {
            /* create the new entry */
            srcdir = new CDbg_win32_srcdir;
            srcdir->dir_.set(buf);

            /* add it to the list */
            srcdir->next_ = srcdirs_;
            srcdirs_ = srcdir;
        }
    }
}

/*
 *   Save the source directory list to the configuration object 
 */
void CHtmlSys_dbgmain::srcdir_list_to_config()
{
    CDbg_win32_srcdir *srcdir;
    int i;
    
    /* clear the old list */
    local_config_->clear_strlist("srcdir", 0);
    
    /* traverse our list and store each entry in the config object */
    for (i = 0, srcdir = srcdirs_ ; srcdir != 0 ; srcdir = srcdir->next_, ++i)
        local_config_->setval("srcdir", 0, i, srcdir->dir_.get());
}

/*
 *   window enumeration callback - close source windows 
 */
void CHtmlSys_dbgmain::enum_win_close_cb(void *ctx0, int /*idx*/,
                                         CHtmlSysWin *win,
                                         int /*line_source_id*/,
                                         HtmlDbg_wintype_t win_type)
{
    /* if this is a source file, close it */
    if (win_type == HTMLDBG_WINTYPE_SRCFILE)
    {
        /* close the window */
        ((CHtmlSysWin_win32 *)win)->close_window(TRUE);
    }
}


/*
 *   additional initialization after the .GAM file has been loaded 
 */
void CHtmlSys_dbgmain::post_load_init(dbgcxdef *ctx)
{
    /* 
     *   Initialize the helper.  Keep the breakpoint configuration if and
     *   only if we're reloading the same game.
     */
    switch(helper_->init_after_load(ctx, this, local_config_,
                                    reloading_ && !reloading_new_))
    {
    case HTMLDBG_LBP_OK:
        /* okay - nothing to report */
        break;

    case HTMLDBG_LBP_NOT_SET:
        /* breakpoints not set */
        if (get_global_config_int("bpmove dlg", 0, TRUE))
            MessageBox(handle_,
                       "One or more breakpoints from the saved configuration "
                       "could not be set.  This usually is because a source "
                       "file has been renamed or removed from the game.",
                       "TADS Workbench", MB_OK);
        break;

    case HTMLDBG_LBP_MOVED:
        /* breakpoints moved */
        if (get_global_config_int("bpmove dlg", 0, TRUE))
            MessageBox(handle_,
                       "One or more breakpoints from the saved configuration "
                       "are not on valid lines.  Each of these breakpoints "
                       "has been moved to the next executable line.",
                       "TADS Workbench", MB_OK);
        break;
    }

    /* if we're reloading, start in the correct mode */
    if (reloading_)
    {
        /* 
         *   set the appropriate run mode: if we're to start running, set
         *   the state to "go", otherwise to "step" 
         */
        if (reloading_go_)
            exec_go();
        else
            exec_step_into();
    }

    /* not yet quitting, restarting, reloading, etc. */
    quitting_ = FALSE;
    restarting_ = FALSE;
    reloading_ = FALSE;
    reloading_new_ = FALSE;
    reloading_go_ = FALSE;
    reloading_exec_ = FALSE;
    terminating_ = FALSE;
    term_in_progress_ = FALSE;
    aborting_cmd_ = FALSE;

    /* the game is now loaded */
    game_loaded_ = TRUE;

    /* we're not in the debugger yet */
    in_debugger_ = FALSE;

    /* 
     *   load the source file menu, now that we know the source files
     *   (references are embedded in the .GAM file, which we've now
     *   loaded) 
     */
    load_file_menu();

    /* update the statusline */
    if (statusline_ != 0)
        statusline_->update();
}


/* 
 *   callback context structure for source window enumeration for saving
 *   source windows 
 */
struct enum_win_save_ctx_t
{
    int count_;
    CHtmlDebugConfig *config_;
};

CHtmlSys_dbgmain::~CHtmlSys_dbgmain()
{
    /* close out the local configuration object, if we have one */
    if (local_config_ != 0)
    {
        /* save the local configuration */
        if (local_config_filename_.get() != 0)
            vsn_save_config_file(local_config_, local_config_filename_.get());

        /* done with the configuration object */
        delete local_config_;
        local_config_ = 0;
    }

    /* close out the global configuration as well */
    if (global_config_ != 0)
    {
        /* save the global configuration */
        if (global_config_filename_.get() != 0)
            vsn_save_config_file(global_config_,
                                 global_config_filename_.get());

        /* done with the global configuration */
        delete global_config_;
        global_config_ = 0;
    }

    /* release my helper reference */
    helper_->remove_ref();

    /* delete the "Find" dialog */
    delete find_dlg_;

    /* delete my MDI docking port */
    delete mdi_dock_port_;

    /* delete the build command list */
    clear_build_cmds();
}

/*
 *   Clear configuration information that isn't suitable for a default
 *   configuration. 
 */
void CHtmlSys_dbgmain::clear_non_default_config()
{
    /* forget the source window list for the default config */
    local_config_->clear_strlist("src win list", 0);
    local_config_->clear_strlist("src win title", 0);
    local_config_->clear_strlist("srcfiles", "fname");
    local_config_->clear_strlist("srcfiles", "path");

    /* forget the source directory list as well */
    local_config_->clear_strlist("srcdir", 0);

    /* forget all breakpoints */
    helper_->clear_bp_config(local_config_);

    /* clear the internal line source list */
    helper_->clear_internal_source_config(local_config_);

    /* clear the internal line status list */
    helper_->delete_line_status(TRUE);

    /* clear all build config info */
    clear_dbg_build_info(local_config_);

    /* clear version-specific info */
    vsn_clear_non_default_config();
}

/*
 *   window destruction 
 */
void CHtmlSys_dbgmain::do_destroy()
{
    /* remove the toolbar timer */
    if (toolbar_ != 0)
        rem_toolbar_proc(toolbar_);

    /* remove our statusline */
    if (statusline_ != 0)
    {
        /* unregister as a status source */
        statusline_->unregister_source(this);

        /* delete the statusline object */
        delete statusline_;
    }
    
    /* inherit default handling */
    CHtmlSys_framewin::do_destroy();
}

/*
 *   callback for enumerating open source windows and saving them 
 */
void CHtmlSys_dbgmain::enum_win_save_cb(void *ctx0, int /*idx*/,
                                        CHtmlSysWin *win,
                                        int /*line_source_id*/,
                                        HtmlDbg_wintype_t win_type)
{
    enum_win_save_ctx_t *ctx = (enum_win_save_ctx_t *)ctx0;
    CHtmlSysWin_win32_dbgsrc *srcwin;
    textchar_t title[OSFNMAX];

    /* 
     *   don't bother saving anything for special windows, such as the
     *   stack and the call history window -- save only regular text file
     *   windows 
     */
    if (win_type != HTMLDBG_WINTYPE_SRCFILE)
        return;

    /* it's a source window - cast it */
    srcwin = (CHtmlSysWin_win32_dbgsrc *)win;

    /* get the window's title */
    GetWindowText(srcwin->get_frame_handle(), title, sizeof(title));

    /* save this window's source path in the configuration */
    ctx->config_->setval("src win list", 0, ctx->count_,
                         srcwin->get_path());
    ctx->config_->setval("src win title", 0, ctx->count_, title);

    /* count it */
    ctx->count_++;
}

/*
 *   window creation
 */
void CHtmlSys_dbgmain::do_create()
{
    HMENU mnu;
    TBBUTTON buttons[30];
    int i;

    /* inherit default */
    CHtmlSys_framewin::do_create();

    /* load my menu */
    mnu = LoadMenu(CTadsApp::get_app()->get_instance(),
                   MAKEINTRESOURCE(IDR_DEBUGMAIN_MENU));
    set_win_menu(mnu, -2);

    /* remember the Windows menu handle for our own purposes */
    win_menu_hdl_ = GetSubMenu(mnu, GetMenuItemCount(mnu) - 2);

    /* 
     *   initialize the button descriptors 
     */
    i = 0;

    /* GO button */
    buttons[i].iBitmap = 0;
    buttons[i].idCommand = ID_DEBUG_GO;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* STEP INTO button */
    buttons[i].iBitmap = 1;
    buttons[i].idCommand = ID_DEBUG_STEPINTO;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* STEP OVER button */
    buttons[i].iBitmap = 2;
    buttons[i].idCommand = ID_DEBUG_STEPOVER;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* STEP OUT button */
    buttons[i].iBitmap = 14;
    buttons[i].idCommand = ID_DEBUG_STEPOUT;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* run to cursor */
    buttons[i].iBitmap = 5;
    buttons[i].idCommand = ID_DEBUG_RUNTOCURSOR;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* set next statement */
    buttons[i].iBitmap = 15;
    buttons[i].idCommand = ID_DEBUG_SETNEXTSTATEMENT;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* break execution */
    buttons[i].iBitmap = 11;
    buttons[i].idCommand = ID_DEBUG_BREAK;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* terminate program */
    buttons[i].iBitmap = 18;
    buttons[i].idCommand = ID_FILE_TERMINATEGAME;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* separator */
    buttons[i].iBitmap = 0;
    buttons[i].idCommand = 0;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_SEP;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* set/clear breakpoint */
    buttons[i].iBitmap = 3;
    buttons[i].idCommand = ID_DEBUG_SETCLEARBREAKPOINT;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* enable/disable breakpoint */
    buttons[i].iBitmap = 6;
    buttons[i].idCommand = ID_DEBUG_DISABLEBREAKPOINT;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* separator */
    buttons[i].iBitmap = 0;
    buttons[i].idCommand = 0;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_SEP;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* evaluate expression */
    buttons[i].iBitmap = 12;
    buttons[i].idCommand = ID_DEBUG_EVALUATE;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* separator */
    buttons[i].iBitmap = 0;
    buttons[i].idCommand = 0;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_SEP;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* show next statement */
    buttons[i].iBitmap = 13;
    buttons[i].idCommand = ID_DEBUG_SHOWNEXTSTATEMENT;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* show game window */
    buttons[i].iBitmap = 7;
    buttons[i].idCommand = ID_VIEW_GAMEWIN;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* show locals window */
    buttons[i].iBitmap = 8;
    buttons[i].idCommand = ID_VIEW_LOCALS;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* show watch window */
    buttons[i].iBitmap = 9;
    buttons[i].idCommand = ID_VIEW_WATCH;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* show stack window */
    buttons[i].iBitmap = 10;
    buttons[i].idCommand = ID_VIEW_STACK;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* separator */
    buttons[i].iBitmap = 0;
    buttons[i].idCommand = 0;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_SEP;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* compile for debugging */
    buttons[i].iBitmap = 16;
    buttons[i].idCommand = ID_BUILD_COMPDBG;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* compile and run */
    buttons[i].iBitmap = 17;
    buttons[i].idCommand = ID_BUILD_COMP_AND_RUN;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* stop build */
    buttons[i].iBitmap = 19;
    buttons[i].idCommand = ID_BUILD_STOP;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* set up the toolbar */
    toolbar_ = CreateToolbarEx(handle_,
                               WS_CHILD | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT,
                               0, 1, CTadsApp::get_app()->get_instance(),
                               IDB_DEBUG_TOOLBAR, buttons, i,
                               16, 15, 16, 15, sizeof(buttons[0]));
    if (toolbar_ != 0)
    {
        /* show the toolbar window */
        ShowWindow(toolbar_, SW_SHOW);

        /* set up the toolbar to do idle status updating */
        add_toolbar_proc(toolbar_);
    }

    /* create the statusline control */
    statusline_ = new CTadsStatusline(this, TRUE, 1999);

    /* register as a source of status information */
    statusline_->register_source(this);

    /* hide the statusline window if the prefs don't want a statusline */
    if (!show_statusline_)
        ShowWindow(statusline_->get_handle(), SW_HIDE);

    /* initialize the recent-games menu */
    update_recent_game_menu();
}

/*
 *   Initialize the statusline 
 */
void CHtmlSys_dbgmain::adjust_statusline_layout()
{
    RECT cli_rc;
    HDC dc;
    HGDIOBJ oldfont;
    SIZE txtsiz;
    int widths[32];
    int part_cnt;
    int i;

    /* if there's no statusline object, ignore it */
    if (statusline_ == 0)
        return;

    /* start at the second part (the first is always the fixed main part) */
    part_cnt = 1;

    /* get the size of the statusbar */
    GetClientRect(statusline_->get_handle(), &cli_rc);

    /*
     *   Current line/column number panel 
     */

    /* figure out how much space we need for our display */
    dc = GetDC(statusline_->get_handle());
    oldfont = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    ht_GetTextExtentPoint32(dc, " Ln 123456  Col 12345 ", 23, &txtsiz);

    /* give this item a fixed area at the right edge */
    widths[part_cnt++] = txtsiz.cx;

    /* restore the drawing context */
    SelectObject(dc, oldfont);
    ReleaseDC(statusline_->get_handle(), dc);

    /*
     *   Finish up 
     */

    /* add space for the sizebar into the rightmost item */
    widths[part_cnt - 1] += GetSystemMetrics(SM_CXVSCROLL);

    /* 
     *   give the leftmost item however much space is left over after summing
     *   up the sizes needed for the other panels 
     */
    for (widths[0] = cli_rc.right, i = 1 ; i < part_cnt ; ++i)
        widths[0] -= widths[i];

    /* set the rightmost item to use all remaining space to the right */
    widths[part_cnt - 1] = -1;

    /* set up the panels */
    SendMessage(statusline_->get_handle(), SB_SETPARTS,
                part_cnt, (LPARAM)widths);
}

/*
 *   Process window activation 
 */
int CHtmlSys_dbgmain::do_activate(int flag, int, HWND)
{
    /* if we're activating, establish our accelerator */
    if (flag != WA_INACTIVE)
    {
        static int enumerating;

        /* establish our accelerator */
        CTadsApp::get_app()->set_accel(accel_, handle_);

        /*
         *   If we're not already refreshing outdated files, post a
         *   message to myself so that we do this after we've finished
         *   activation.  (We post a separate message, because activation
         *   is too delicate to do it in-line.  We test for recursion,
         *   because we may pop up message boxes during the refresh
         *   process, which will result in re-activations when the message
         *   boxes are dismissed.) 
         */
        if (!refreshing_sources_)
            PostMessage(handle_, HTMLM_REFRESH_SOURCES, 0, 0);

        /*
         *   If we're not in the debugger, clear the stack window.  This
         *   will ensure that, if the user activates the debugger window
         *   while the game is running, we won't give a misleading
         *   impression of the current execution state by showing them
         *   what was in effect the last time we were in the debugger.  
         */
        if (!in_debugger_)
        {
            helper_->update_stack_window(0, TRUE);
            helper_->forget_current_line();
        }
    }
    
    /* proceed with default activation */
    return FALSE;
}

/*
 *   Get our statusline status 
 */
textchar_t *CHtmlSys_dbgmain::get_status_message(int *caller_deletes)
{
    /* presume we'll return a static string that won't need to be deleted */
    *caller_deletes = FALSE;

    /* get our status message */
    if (quitting_)
        return "Terminating";
    if (!workspace_open_)
        return "No Project Loaded";
    if (compiler_hproc_ != 0)
    {
        if (compile_status_.get() != 0 && compile_status_.get()[0] != 0)
            return compile_status_.get();
        else
            return "Build in Progress";
    }
    if (!game_loaded_)
        return "Ready";
    if (in_debugger_)
        return "Program Paused";

    /* 
     *   we have a game loaded and we're not in the debugger, so the game
     *   must be running 
     */
    return "Program Running";
}

/*
 *   Schedule a full library reload.  If there's already a full reload
 *   scheduled, we'll ignore the redundant request.  
 */
void CHtmlSys_dbgmain::schedule_lib_reload()
{
    MSG msg;

    /* 
     *   Remove any pending library reload request.  Remove any existing one
     *   (rather than simply not schedling a new one if there's already one
     *   there) to ensure that we upgrade the request to an unconditional
     *   reload, overriding any conditional (modified-files-only) reload that
     *   might have already been scheduled.  (An unconditional request can
     *   simply supersede a modified-only request, since the unconditional
     *   request will load any modified files along with everything else.)  
     */
    PeekMessage(&msg, handle_, HTMLM_REFRESH_SOURCES, HTMLM_REFRESH_SOURCES,
                PM_REMOVE);

    /* 
     *   Schedule the new message.  Pass TRUE for the wparam to indicate
     *   that the reload is unconditional. 
     */
    PostMessage(handle_, HTMLM_REFRESH_SOURCES, TRUE, 0);
}

/*
 *   Process a user message 
 */
int CHtmlSys_dbgmain::do_user_message(int msg, WPARAM wpar, LPARAM lpar)
{
    int maxed;
    
    switch(msg)
    {
    case HTMLM_REFRESH_SOURCES:
        /*
         *   Refresh libraries.  If 'wpar' is true it means that we're to
         *   refresh unconditionally; otherwise, it means that we're merely
         *   to refresh any libraries that have changed on disk.
         *   
         *   Note that we're in the process of refreshing our windows, so
         *   that we don't try to do recursive refreshing in response to
         *   dismissal of message boxes that we may display during this
         *   process.  
         */
        refreshing_sources_ = TRUE;

        /* 
         *   go through our open source windows and reload any that have been
         *   changed since we last loaded them 
         */
        helper_->enum_source_windows(&enum_win_activate_cb, this);

        /* if we have a project window, refresh it as well */
        if (projwin_ != 0)
        {
            /* 
             *   refresh modified libraries or unconditionally, according to
             *   the 'wpar' 
             */
            if (wpar)
                projwin_->refresh_all_libs();
            else
                projwin_->refresh_for_activation();
        }

        /* we're done refreshing the sources */
        refreshing_sources_ = FALSE;

        /* handled */
        return TRUE;

    case HTMLM_BUILD_DONE:
        /* 
         *   "Build done" message - this is posted to us by the background
         *   thread doing the build when it finishes.  wpar is the exit
         *   code of the build process. 
         */
        
        /* the build is finished - clear the build flag */
        build_running_ = FALSE;

        /* 
         *   wait for the build thread to exit (which should be
         *   immediately, since posting us is the last thing it does),
         *   then close its handle 
         */
        WaitForSingleObject(build_thread_, INFINITE);
        CloseHandle(build_thread_);
        
        /* if a build dialog is running, dismiss it */
        if (build_wait_dlg_ != 0)
        {
            /* 
             *   post our special command message to the dialog to tell it
             *   that the build is finished - when the dialog receives
             *   this, it will dismiss itself, allowing us to proceed with
             *   whatever we were waiting for while the build was running 
             */
            PostMessage(build_wait_dlg_->get_handle(), WM_COMMAND,
                        IDC_BUILD_DONE, 0);
        }

        /* 
         *   If we're meant to run the program when the compilation is
         *   finished, and the build was successful, run the program by
         *   posting a "go" command to ourselves.
         */
        if (wpar == 0 && run_after_build_)
            PostMessage(handle_, WM_COMMAND, ID_DEBUG_GO, 0);

        /* turn auto-vscroll back on in the debug log window */
        if (helper_->get_debuglog_win() != 0)
            ((CHtmlSysWin_win32 *)helper_->get_debuglog_win())
                ->set_auto_vscroll(TRUE);

        /* handled */
        return TRUE;

    case HTMLM_LOAD_CONFIG_DONE:
        /* 
         *   we're done loading the configuration - show any
         *   deferred-visibility windows 
         */
        while (loading_win_defer_list_ != 0)
        {
            html_dbg_win_list *entry;
            
            /* get this entry */
            entry = loading_win_defer_list_;
            
            /* unlink it from the lis t*/
            loading_win_defer_list_ =  loading_win_defer_list_->nxt;
            
            /* show this window */
            ShowWindow(entry->hwnd, SW_SHOW);
            
            /* we're finished with this list entry - delete it */
            delete entry;
        }

        /* 
         *   get the MDI child-window-maximized status; if it's not stored,
         *   default to not-maximized 
         */
        if (local_config_->getval("main win mdi max", 0, &maxed)
            && global_config_->getval("main win mdi max", 0, &maxed))
            maxed = FALSE;

        /*   
         *   If we left things in the saved configuration with the MDI child
         *   window maximized, re-maximize the window now; if not, restore
         *   now in case we inherited a maximized state from the prior
         *   configuration.  
         */
        SendMessage(get_parent_of_children(),
                    maxed ? WM_MDIMAXIMIZE : WM_MDIRESTORE,
                    (WPARAM)(HWND)SendMessage(get_parent_of_children(),
                                              WM_MDIGETACTIVE, 0, 0), 0);

        /* handled */
        return TRUE;

        
    default:
        /* it's not one of ours; inherit default behavior */
        return CHtmlSys_framewin::do_user_message(msg, wpar, lpar);
    }
}


/*
 *   Callback for enumerating source windows during debugger activation.
 *   For each window that corresponds to an external file, we'll check the
 *   external file's modification date to see if it's been modified since
 *   we originally loaded it, and reload the file if so.  
 */
void CHtmlSys_dbgmain::enum_win_activate_cb(void *ctx0, int /*idx*/,
                                            CHtmlSysWin *win0,
                                            int /*line_source_id*/,
                                            HtmlDbg_wintype_t win_type)
{
    CHtmlSys_dbgmain *ctx = (CHtmlSys_dbgmain *)ctx0;
    CHtmlSysWin_win32_dbgsrc *win;
    
    /* we don't need to worry about anything but source files */
    if (win_type != HTMLDBG_WINTYPE_SRCFILE)
        return;

    /* appropriately cast the window */
    win = (CHtmlSysWin_win32_dbgsrc *)win0;
    
    /* ask the window to compare its dates */
    if (win->has_file_changed())
    {
        char msg[MAX_PATH + 100];
        int update;
        int optval;

        /* presume we won't update the file */
        update = FALSE;
        
        /* 
         *   The file has changed - ask the user what to do; if the file
         *   was explicitly opened for editing by the user, though, don't
         *   ask, since they told us they were going to change the file
         *   and thus almost certainly want it reloaded after it's been
         *   updated.
         *   
         *   Suppress the query if the appropriate option setting so
         *   indicates.  
         */
        if (!win->was_file_opened_for_editing()
            && (ctx->get_global_config()->getval("ask reload src", 0, &optval)
                || optval))
        {
            sprintf(msg, "The file \"%s\" has been modified by another "
                    "program since the TADS Workbench loaded it.  Do you "
                    "wish to re-load the file?", win->get_path());
            switch(MessageBox(ctx->get_handle(), msg, "TADS Workbench",
                              MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1))
            {
            case IDYES:
            case 0:
                /* reload the file */
                update = TRUE;
                break;

            default:
                /* ignore this file */
                break;
            }
        }
        else
        {
            /* 
             *   it was explicitly opened for editing - reload it without
             *   asking 
             */
            update = TRUE;
        }

        /* if we do indeed want to update the file, reload it */
        if (update)
            ctx->helper_->reload_source_window(ctx, win);
    }

    /* 
     *   bring the window's internal timestamp up to date, so that we
     *   don't ask again until the file changes again 
     */
    win->update_file_time();
}


/*
 *   Process a notification message.  We receive notifications from the
 *   toolbar to get tooltip text.  
 */
int CHtmlSys_dbgmain::do_notify(int /*control_id*/, int notify_code,
                                LPNMHDR nmhdr)
{
    switch(notify_code)
    {
    case TTN_NEEDTEXT:
        /* retreive tooltip text for a button */
        {
            TOOLTIPTEXT *ttt = (TOOLTIPTEXT *)nmhdr;

            /* get the strings from my application instance */
            ttt->hinst = CTadsApp::get_app()->get_instance();

            /* determine which button was pressed */
            switch(ttt->hdr.idFrom)
            {
            case ID_DEBUG_GO:
                ttt->lpszText = MAKEINTRESOURCE(IDS_DEBUG_GO);
                break;
                
            case ID_DEBUG_STEPINTO:
                ttt->lpszText = MAKEINTRESOURCE(IDS_DEBUG_STEPINTO);
                break;
                
            case ID_DEBUG_STEPOVER:
                ttt->lpszText = MAKEINTRESOURCE(IDS_DEBUG_STEPOVER);
                break;
                
            case ID_DEBUG_RUNTOCURSOR:
                ttt->lpszText = MAKEINTRESOURCE(IDS_DEBUG_RUNTOCURSOR);
                break;
                
            case ID_DEBUG_BREAK:
                ttt->lpszText = MAKEINTRESOURCE(IDS_DEBUG_BREAK);
                break;

            case ID_FILE_TERMINATEGAME:
                ttt->lpszText = MAKEINTRESOURCE(IDS_FILE_TERMINATEGAME);
                break;
                
            case ID_DEBUG_SETCLEARBREAKPOINT:
                ttt->lpszText = MAKEINTRESOURCE(IDS_DEBUG_SETCLEARBREAKPOINT);
                break;
                
            case ID_DEBUG_DISABLEBREAKPOINT:
                ttt->lpszText = MAKEINTRESOURCE(IDS_DEBUG_DISABLEBREAKPOINT);
                break;
                
            case ID_DEBUG_EVALUATE:
                ttt->lpszText = MAKEINTRESOURCE(IDS_DEBUG_EVALUATE);
                break;
                
            case ID_VIEW_GAMEWIN:
                ttt->lpszText = MAKEINTRESOURCE(IDS_VIEW_GAMEWIN);
                break;
                
            case ID_VIEW_WATCH:
                ttt->lpszText = MAKEINTRESOURCE(IDS_VIEW_WATCH);
                break;
                
            case ID_VIEW_LOCALS:
                ttt->lpszText = MAKEINTRESOURCE(IDS_VIEW_LOCALS);
                break;
                
            case ID_VIEW_STACK:
                ttt->lpszText = MAKEINTRESOURCE(IDS_VIEW_STACK);
                break;

            case ID_DEBUG_SHOWNEXTSTATEMENT:
                ttt->lpszText = MAKEINTRESOURCE(IDS_DEBUG_SHOWNEXTSTATEMENT);
                break;

            case ID_DEBUG_SETNEXTSTATEMENT:
                ttt->lpszText = MAKEINTRESOURCE(IDS_DEBUG_SETNEXTSTATEMENT);
                break;

            case ID_DEBUG_STEPOUT:
                ttt->lpszText = MAKEINTRESOURCE(IDS_DEBUG_STEPOUT);
                break;

            case ID_BUILD_COMPDBG:
                ttt->lpszText = MAKEINTRESOURCE(IDS_BUILD_COMPDBG);
                break;

            case ID_BUILD_STOP:
                ttt->lpszText = MAKEINTRESOURCE(IDS_BUILD_STOP);
                break;

            case ID_BUILD_COMP_AND_RUN:
                ttt->lpszText = MAKEINTRESOURCE(IDS_BUILD_COMP_AND_RUN);
                break;
                
            default:
                /* not handled */
                return FALSE;
            }
        }
        
        /* handled */
        return TRUE;

    default:
        /* ignore other notifications */
        return FALSE;
    }
}

/*
 *   close the main window 
 */
int CHtmlSys_dbgmain::do_close()
{
    /* 
     *   this will stop the debugging session -- ask the user for
     *   confirmation 
     */
    ask_quit();

    /* 
     *   Even if we're quitting, don't actually close the window for now;
     *   the quitting flag will make us shut down, so we don't need to
     *   close the window for real right now 
     */
    return FALSE;
}

/* 
 *   resize the window 
 */
void CHtmlSys_dbgmain::do_resize(int mode, int x, int y)
{
    RECT cli_rc;
    RECT rc;
    
    /* inherit default */
    CHtmlSys_framewin::do_resize(mode, x, y);

    /* 
     *   Send the resize to the toolbar window, so that it can reposition
     *   itself correctly.  It doesn't matter what the parameters are (it
     *   looks at the parent window to figure out what size to use), but
     *   it's important that the window receives the WM_SIZE whenever the
     *   parent window's size changes.  
     */
    if (toolbar_ != 0)
        SendMessage(toolbar_, TB_AUTOSIZE, 0, 0);

    /* notify the statusline */
    if (statusline_ != 0)
    {
        /* notify it, to move it to the right spot */
        statusline_->notify_parent_resize();

        /* adjust the internal status panel layout */
        adjust_statusline_layout();
    }

    /*
     *   If we're not minimizing the window, resize the MDI client area
     *   and its adornments.  
     */
    if (mode == SIZE_MAXIMIZED || mode == SIZE_RESTORED)
    {
        POINT orig;

        /* get the main client rectangle */
        GetClientRect(handle_, &cli_rc);

        /* get my client rectangle's area in absolute screen coordinates */
        orig.x = orig.y = 0;
        ClientToScreen(handle_, &orig);

        /* subtract the space occupied by the toolbar */
        GetWindowRect(toolbar_, &rc);
        OffsetRect(&rc, -orig.x, -orig.y);
        SubtractRect(&cli_rc, &cli_rc, &rc);

        /* subtract the space occupied by the statusline */
        if (statusline_ != 0 && show_statusline_)
        {
            /* get the statusline area, relative to my area */
            GetWindowRect(statusline_->get_handle(), &rc);
            OffsetRect(&rc, -orig.x, -orig.y);

            /* deduct the statusline area from my client area */
            SubtractRect(&cli_rc, &cli_rc, &rc);
        }

        /* fix up the positions of the docked windows */
        if (mdi_dock_port_ != 0)
            mdi_dock_port_->dockport_mdi_resize(&cli_rc);
        
        /* 
         *   tell the system window the new client size - don't let the
         *   MDI system window handler do the work itself, because its
         *   default layout calculator doesn't have enough information to
         *   get it right all the time 
         */
        MoveWindow(get_parent_of_children(),
                   cli_rc.left, cli_rc.top,
                   cli_rc.right - cli_rc.left,
                   cli_rc.bottom - cli_rc.top, TRUE);
    }
}

/*
 *   paint the background 
 */
int CHtmlSys_dbgmain::do_erase_bkg(HDC hdc)
{
    RECT rc;
    
    /* fill in the background with button face color */
    GetClientRect(handle_, &rc);
    FillRect(hdc, &rc, GetSysColorBrush(COLOR_3DFACE));
    return TRUE;
}

/*
 *   Ask the user if they want to quit.  Return true if so, false if not.  
 */
int CHtmlSys_dbgmain::ask_quit()
{
    /* 
     *   put up a message box asking about quitting (unless the game is
     *   over, in which case we can quit without asking) 
     */
    if (game_loaded_ && get_global_config_int("ask exit dlg", 0, TRUE))
    {
        switch(MessageBox(0,
                          "This will terminate your debugging session.  "
                          "Do you really want to quit?", "TADS Workbench",
                          MB_YESNO | MB_ICONQUESTION
                          | MB_DEFBUTTON2 | MB_TASKMODAL))
        {
        case IDYES:
        case 0:
            /* go ahead and quit */
            break;
            
        default:
            /* don't quit */
            return FALSE;
        }
    }
        
    /* set the flag to indicate that we should quit the program */
    quitting_ = TRUE;

    /* tell the main window not to pause for exit */
    if (CHtmlSys_mainwin::get_main_win() != 0)
        CHtmlSys_mainwin::get_main_win()->enable_pause_for_exit(FALSE);
    
    /* tell the event loop to relinquish control */
    go_ = TRUE;

    /* abort any command entry that the game is doing */
    abort_game_command_entry();

    /* set the end-of-file flag in the main game window */
    if (CHtmlSys_mainwin::get_main_win() != 0)
        CHtmlSys_mainwin::get_main_win()->end_current_game();

    /* indicate that we are quitting */
    return TRUE;
}

/*
 *   Abort command entry in the game and force entry into the debugger 
 */
void CHtmlSys_dbgmain::abort_game_command_entry()
{
    /*
     *   If we're not in the debugger, set stepping mode, and tell the
     *   main window to abort the current command.  This will force
     *   debugger entry.
     */
    if (!in_debugger_)
    {
        /* 
         *   make sure we're in stepping mode, so that we regain control
         *   when the current command ends 
         */
        helper_->set_exec_state_step_into(dbgctx_);

        /* tell the main window to abort the current command */
        if (CHtmlSys_mainwin::get_main_win() != 0)
            CHtmlSys_mainwin::get_main_win()->abort_command();
    }
}

/*
 *   store my window position preferences 
 */
void CHtmlSys_dbgmain::set_winpos_prefs(const CHtmlRect *winpos)
{
    /* save the position in the debugger configuration */
    local_config_->setval("main win pos", 0, winpos);
    local_config_->setval("main win max", 0, IsZoomed(handle_));

    /* save it in the global configuration as well */
    global_config_->setval("main win pos", 0, winpos);
    global_config_->setval("main win max", 0, IsZoomed(handle_));
}


/* 
 *   context structure for source file enumerator callback 
 */
struct HtmlDbg_srcenum_t
{
    /* number of items inserted into the menu so far */
    int count_;

    /* handle of the menu into which we're inserting */
    HMENU menu_;
};

/*
 *   Set up the file menu.  We'll invoke this the first time we enter the
 *   debugger (after the game is loaded) to load the source files into the
 *   quick-open file submenu.  Note that we can't do this during debugger
 *   initialization, because that happens before the game file is loaded,
 *   so we don't know any of the source files at that time.  
 */
void CHtmlSys_dbgmain::load_file_menu()
{
    HMENU mnu;
    HMENU filemnu;
    HMENU submnu;
    HtmlDbg_srcenum_t cbctx;
    int i;
    MENUITEMINFO info;
    static const char no_files[] = "No Files";

    /* get the File menu (first menu) */
    mnu = GetMenu(handle_);
    filemnu = GetSubMenu(mnu, 0);

    /* 
     *   Find the first item in the file menu with its own submenu whose
     *   first menu item has a command ID in the range we expect (we
     *   search here to insulate against changes in the menu layout; as
     *   long as we don't add another item with a submenu, we'll be safe) 
     */
    for (submnu = 0, i = 0 ; i < GetMenuItemCount(filemnu) ; ++i)
    {
        /* get the next item's submenu */
        submnu = GetSubMenu(filemnu, i);

        /* if this item has a submenu, consider it further */
        if (submnu != 0)
        {
            int id;
            
            /* get the command for the first submenu item */
            id = GetMenuItemID(submnu, 0);

            /* 
             *   if this item has an ID we'd expect in this submenu, this
             *   is the one we're looking for 
             */
            if (id == ID_FILE_OPEN_NONE
                || (id >= ID_FILE_OPENLINESRC_FIRST
                    && id <= ID_FILE_OPENLINESRC_LAST))
            {
                /* this is our menu - stop searching */
                break;
            }
        }
    }

    /* if we couldn't find a submenu, we can't build a file menu */
    if (submnu == 0)
        return;

    /* delete everything from the menu */
    while (GetMenuItemCount(submnu) != 0)
        DeleteMenu(submnu, 0, MF_BYPOSITION);

    /* insert the placeholder item */
    info.cbSize = menuiteminfo_size_;
    info.fMask = MIIM_TYPE | MIIM_DATA | MIIM_ID;
    info.fType = MFT_STRING;
    info.wID = ID_FILE_OPEN_NONE;
    info.dwTypeData = (char *)no_files;
    info.cch = sizeof(no_files) - 1;
    InsertMenuItem(submnu, 0, TRUE, &info);

    /* load the source files into the menu */
    cbctx.count_ = 0;
    cbctx.menu_ = submnu;
    helper_->enum_source_files(load_menu_cb, &cbctx);

    /* 
     *   if we added any items, remove the "no files" placeholder item
     *   that's in the menu resource -- it will still be the first item,
     *   so simply remove the first item from the menu 
     */
    if (cbctx.count_ > 0)
    {
        /* delete the placeholder */
        DeleteMenu(submnu, 0, MF_BYPOSITION);
    }
}

/* 
 *   Callback for source file enumerator - loads the current source file
 *   into the menu.
 */
void CHtmlSys_dbgmain::load_menu_cb(void *ctx0, const textchar_t *fname,
                                    int line_source_id)
{
    MENUITEMINFO info;
    HtmlDbg_srcenum_t *ctx = (HtmlDbg_srcenum_t *)ctx0;
    char caption[260];

    /* show the root name portion only */
    fname = os_get_root_name((char *)fname);

    /*
     *   Generate the menu item caption.  Prepend a keyboard shortcut key
     *   to the name using the current item number. 
     */
    ctx->count_++;
    caption[0] = '&';
    caption[1] = ctx->count_ + (ctx->count_ < 10 ? '0' : 'a' - 10);
    caption[2] = ' ';
    do_strcpy(caption + 3, fname);
    
    /* 
     *   Set up the menu item information structure.  Generate the command
     *   ID by adding the line source ID to the base file command ID.  
     */
    info.cbSize = menuiteminfo_size_;
    info.fMask = MIIM_TYPE | MIIM_DATA | MIIM_ID;
    info.fType = MFT_STRING;
    info.wID = ID_FILE_OPENLINESRC_FIRST + line_source_id;
    info.dwTypeData = caption;
    info.cch = get_strlen(caption);

    /* insert the menu at the end of the menu */
    InsertMenuItem(ctx->menu_, ctx->count_, TRUE, &info);
}

/*
 *   save our configuration 
 */
void CHtmlSys_dbgmain::save_config()
{
    enum_win_save_ctx_t cbctx;
    HWND hwnd;
    CHtmlRect hrc;
    int maxed;

    /* if there's no local configuration, there's nothing to save */
    if (local_config_ == 0)
        return;

    /*
     *   If there's no main window, don't bother saving the configuration,
     *   since the window layout is part of the configuration.
     *   
     *   We can try to save the configuration after destroying the main
     *   window if we terminate the debugger with the program still running:
     *   we'll save the config, destroy the main window, and then wait for
     *   the program to exit.  When the program exits, it will try to save
     *   the configuration; this is no longer possible because the main
     *   window has been destroyed, and no longer necessary because we will
     *   already have saved the configuration anyway.  
     */
    if (S_main_win == 0)
        return;

    /* remember whether or not the active MDI child is maximized */
    SendMessage(get_parent_of_children(), WM_MDIGETACTIVE, 0, (LPARAM)&maxed);
    local_config_->setval("main win mdi max", 0, maxed);
    global_config_->setval("main win mdi max", 0, maxed);

    /* save the docking configuration locally and globally */
    save_dockwin_config(local_config_);
    save_dockwin_config(global_config_);

    /* save the list of open windows in the local configuration */
    local_config_->clear_strlist("src win list", 0);
    local_config_->clear_strlist("src win title", 0);
    cbctx.count_ = 0;
    cbctx.config_ = local_config_;
    helper_->enum_source_windows(&enum_win_save_cb, &cbctx);

    /* save the watch and local configurations locally and globally */
    if (watchwin_ != 0)
    {
        watchwin_->save_config(local_config_, "watch win");
        watchwin_->save_config(global_config_, "watch win");
    }
    if (localwin_ != 0)
    {
        localwin_->save_config(local_config_, "locals win");
        localwin_->save_config(global_config_, "locals win");
    }

    /* save the visibility and position of the debug log window */
    if (helper_->get_debuglog_win() != 0)
    {
        RECT rc;
        CHtmlRect hrc;

        /* save its visibility */
        hwnd = ((CHtmlSysWin_win32 *)helper_->get_debuglog_win())
               ->get_frame_handle();
        local_config_->setval("log win vis", 0, IsWindowVisible(hwnd));
        global_config_->setval("log win vis", 0, IsWindowVisible(hwnd));

        /* get its position, adjusted to frame-relative coordinates */
        GetWindowRect(hwnd, &rc);
        w32tdb_adjust_client_pos(this, &rc);

        /* save the position */
        hrc.set(rc.left, rc.top, rc.right, rc.bottom);
        local_config_->setval("log win pos", 0, &hrc);
        global_config_->setval("log win pos", 0, &hrc);
    }
    else
    {
        local_config_->setval("log win vis", 0, FALSE);
        global_config_->setval("log win vis", 0, FALSE);
    }

    /* save the visibility and position of the stack window */
    if (helper_->get_stack_win() != 0)
    {
        hwnd = ((CHtmlSysWin_win32 *)helper_->get_stack_win())
               ->get_frame_handle();
        local_config_->setval("stack win vis", 0, IsWindowVisible(hwnd));
        global_config_->setval("stack win vis", 0, IsWindowVisible(hwnd));
    }
    else
    {
        local_config_->setval("stack win vis", 0, FALSE);
        global_config_->setval("stack win vis", 0, FALSE);
    }

    /* save visibility and position of the call history window */
    if (helper_->get_hist_win() != 0)
    {
        hwnd = ((CHtmlSysWin_win32 *)helper_->get_hist_win())
               ->get_frame_handle();
        local_config_->setval("hist win vis", 0, IsWindowVisible(hwnd));
        global_config_->setval("hist win vis", 0, IsWindowVisible(hwnd));
    }
    else
    {
        local_config_->setval("hist win vis", 0, FALSE);
        global_config_->setval("hist win vis", 0, FALSE);
    }

    /* save breakpoint list */
    helper_->save_bp_config(local_config_);

    /* save the search configuration locally */
    local_config_->setval("search", "exact case", search_exact_case_);
    local_config_->setval("search", "wrap", search_wrap_);
    local_config_->setval("search", "dir", search_dir_);
    local_config_->setval("search", "start at top", search_from_top_);
    local_config_->setval("search", "text", 0, find_text_);
    save_find_config(local_config_, "search dlg");

    /* save the source search directory list */
    srcdir_list_to_config();

    /* save the source file configuration */
    helper_->save_internal_source_config(local_config_);

    /* save version-specific configuration information */
    vsn_save_config();
}

/*
 *   callback for saving the docking configuration 
 */
static void save_dockwin_cb(void *ctx, int id, const char *info)
{
    CHtmlDebugConfig *config = (CHtmlDebugConfig *)ctx;

    /* write the string to the "docking config" string */
    config->setval("docking config", 0, id, info);
}

/*
 *   Save the docking window configuration 
 */
void CHtmlSys_dbgmain::save_dockwin_config(CHtmlDebugConfig *config)
{
    /* if we don't have a docking port, do nothing */
    if (mdi_dock_port_ == 0)
        return;

    /* clear out the old docking window configuration list */
    config->clear_strlist("docking config", 0);

    /* assign ID's to the model elements */
    mdi_dock_port_->get_model_frame()->serialize(save_dockwin_cb, config);
}

/*
 *   callback for docking configuration loader 
 */
static int load_dockwin_cb(void *ctx, int id, char *buf, size_t buflen)
{
    CHtmlDebugConfig *config = (CHtmlDebugConfig *)ctx;

    /* load the desired string */
    return config->getval("docking config", 0, id, buf, buflen);
}

/*
 *   Load the docking window configuration 
 */
void CHtmlSys_dbgmain::load_dockwin_config(CHtmlDebugConfig *config)
{
    int cnt;
    
    /* if we don't have a docking port, do nothing */
    if (mdi_dock_port_ == 0)
        return;

    /* 
     *   determine how many configuration strings we have for the docking
     *   configuration 
     */
    cnt = config->get_strlist_cnt("docking config", 0);

    /* 
     *   if there are no docking configuration strings, don't bother
     *   loading a new configuration - just keep the existing model as it
     *   is currently set up 
     */
    if (cnt == 0)
        return;

    /* ask the frame model to load the information */
    mdi_dock_port_->get_model_frame()->load(load_dockwin_cb,
                                            config, cnt);

    /* 
     *   In case we loaded a file saved under the old scheme, which used
     *   docking model serial numbers to associate GUID's with windows,
     *   assign GUID's to all of the model windows.  
     */
    assign_dockwin_guid("locals win");
    assign_dockwin_guid("watch win");
    assign_dockwin_guid("stack win");
    assign_dockwin_guid("hist win");
    assign_dockwin_guid("log win");
}

/*
 *   Assign a docking window GUID to a window based on its serial number.
 *   This is used to convert a file that uses the old serial numbering scheme
 *   to the new GUID scheme.  
 */
void CHtmlSys_dbgmain::assign_dockwin_guid(const char *guid)
{
    int id;

    /* 
     *   If we have configuration data for this "dock id" serial number, it's
     *   the mapping of the GUID to the docking window serial number.  
     */
    if (!local_config_->getval(guid, "dock id", &id))
    {
        /* 
         *   We have the old information, so we need to assign the GUID to
         *   the window identified by this serial number.  
         */
        mdi_dock_port_->get_model_frame()->assign_guid(id, guid);

        /* 
         *   We've now resolved the serial number to the window and assigned
         *   the GUID, so the serial numbering information is obsolete.
         *   Drop it from the configuration. 
         */
        local_config_->delete_val(guid, "dock id");
    }
}

/*
 *   load the docking window configuration for a given window 
 */
void CHtmlSys_dbgmain::
   load_dockwin_config_win(CHtmlDebugConfig *config, const char *guid,
                           CTadsWin *subwin, const char *win_title,
                           int visible, tads_dock_align default_dock_align,
                           int default_docked_width,
                           int default_docked_height,
                           const RECT *default_undocked_pos,
                           int default_is_docked)
{
    CTadsWinDock *dock_win;
    RECT rc;
    int need_default;

    /* presume we'll need a default docking container */
    need_default = TRUE;

    /* find the window in our docking model */
    dock_win = mdi_dock_port_->get_model_frame()
               ->find_window_by_guid(this, guid);

    /* 
     *   If we found a window, and it doesn't already have a subwindow, use
     *   it.  (If the window already has a subwindow, something must be wrong
     *   with the configuration file, since another window must have been
     *   saved with the same serial ID.  Work around this by forcing creation
     *   of a new parent window.)  
     */
    if (dock_win != 0 && dock_win->get_subwin() == 0)
    {
        /* set the docking window's subwindow */
        dock_win->set_subwin(subwin);
        
        /* set the docking window's title */
        SetWindowText(dock_win->get_handle(), win_title);
        
        /* we don't need a default window now */
        need_default = FALSE;
    }

    /*
     *   If we didn't manage to find any stored configuration information,
     *   create a default docking parent for the window.  
     */
    if (need_default)
    {
        /* create the window using the default parameters */
        dock_win = new CTadsWinDock(subwin, default_docked_width,
                                    default_docked_height,
                                    default_dock_align, guid);

        /* create its system window, initially hidden */
        dock_win->create_system_window(this, FALSE, win_title,
                                       default_undocked_pos);

        /* 
         *   if the window is to be docked by default, mark it for docking
         *   on showing 
         */
        if (default_is_docked)
            dock_win->set_dock_on_unhide();
    }

    /* 
     *   create the subwindow's system window inside the docking parent
     *   window 
     */
    GetClientRect(dock_win->get_handle(), &rc);
    subwin->create_system_window(dock_win, TRUE, "", &rc);
    
    /* 
     *   If the window should be visible, show the docking window.  If
     *   this window is set to convert to MDI when it becomes visible, and
     *   we're in the process of loading a configuration, defer showing
     *   the window until after we've finished loading the configuration -
     *   this will ensure that all of the frame adornments are created and
     *   thus that the window's final position relative to the MDI client
     *   area can be properly calculated. 
     */
    if (visible && loading_win_config_ && dock_win->is_mdi_on_unhide())
    {
        html_dbg_win_list *entry;
        
        /* 
         *   defer showing the window until loading is complete - simply
         *   create a new entry for the deferred visibility list and link
         *   it into the list 
         */
        entry = new html_dbg_win_list(dock_win->get_handle());
        entry->nxt = loading_win_defer_list_;
        loading_win_defer_list_ = entry;
    }
    else
    {
        /* show the window now if it's to be visible */
        ShowWindow(dock_win->get_handle(), visible ? SW_SHOW : SW_HIDE);
    }
}



/*
 *   Terminate the debugger 
 */
void CHtmlSys_dbgmain::terminate_debugger()
{
    /*
     *   Before we do anything else, save the configuration.  We need to
     *   do this before destroying the main debugger window, because all
     *   of the other debugger windows are owned by the main window, hence
     *   destroying the main window will destroy all of them first. 
     */
    save_config();

    /* delete the watch window and the local variables window */
    if (watchwin_ != 0)
    {
        SendMessage(GetParent(watchwin_->get_handle()), DOCKM_DESTROY, 0, 0);
        watchwin_ = 0;
    }
    if (localwin_ != 0)
    {
        SendMessage(GetParent(localwin_->get_handle()), DOCKM_DESTROY, 0, 0);
        localwin_ = 0;
    }

    /* close the project window if we have one */
    if (projwin_ != 0)
    {
        SendMessage(GetParent(projwin_->get_handle()), DOCKM_DESTROY, 0, 0);
        projwin_ = 0;
    }

    /* if a build is still running, run the "waiting for build" dialog */
    if (build_running_)
        run_wait_for_build_dlg();

    /* delete the main debugger window */
    DestroyWindow(handle_);

    /* forget the main debugger window */
    S_main_win = 0;
}

/*
 *   Run the wait-for-build dialog.  This dialog simply shows that we're
 *   waiting for the build to complete, and keeps running until the build
 *   is finished.  
 */
void CHtmlSys_dbgmain::run_wait_for_build_dlg()
{
    /* create the dialog */
    build_wait_dlg_ = new CTadsDlg_buildwait(compiler_hproc_);

    /* display the dialog */
    build_wait_dlg_->run_modal(DLG_BUILD_WAIT, handle_,
                               CTadsApp::get_app()->get_instance());

    /* done with the dialog */
    delete build_wait_dlg_;
    build_wait_dlg_ = 0;
}

/*
 *   Receive notification of TADS shutdown.  This will be invoked when
 *   TADS is closing down, but the debugger will remain active.  
 */
void CHtmlSys_dbgmain::notify_tads_shutdown()
{
    /* save the current configuration */
    save_config();

    /* forget our TADS debugger context */
    dbgctx_ = 0;

    /* the game is no longer loaded */
    set_game_loaded(FALSE);

    /* clear the stack window */
    helper_->update_stack_window(0, TRUE);

    /* remove any current line displays */
    helper_->forget_current_line();
}

/*
 *   Debugger main event loop 
 */
int CHtmlSys_dbgmain::dbg_event_loop(dbgcxdef *ctx, int bphit, int err,
                                     int *restart, int *reload,
                                     int *terminate, int *abort,
                                     unsigned int *exec_ofs)
{
    int ret;
    int old_in_dbg;
    CHtmlSysWin_win32 *win;
    CHtmlSys_mainwin *mainwin;

    /* get the main window */
    mainwin = CHtmlSys_mainwin::get_main_win();
        
    /* remember the current debugger context */
    dbgctx_ = ctx;

    /* remember the pointer to the debugger's execution offset */
    exec_ofs_ptr_ = exec_ofs;

    /* presume we won't want to reload or terminate */
    *reload = FALSE;
    *terminate = FALSE;

    /* 
     *   if the 'quitting' flag is already set, it means they closed the
     *   debugger while the game was running, and we're just now getting
     *   control - immediately return and indicate that we should
     *   terminate 
     */
    if (quitting_)
        return FALSE;

    /* 
     *   add a self-reference to keep our event loop variable in memory
     *   (it's a part of 'this', hence we need to keep 'this' around) 
     */
    AddRef();

    /* 
     *   if we're terminating the game, set the caller's terminate flag,
     *   but tell the caller to keep the debugger running
     */
    if (terminating_)
        goto do_terminate;

    /*
     *   If the 'restarting' flag is already set, it means they restarted
     *   the game while the debugger was inactive, and we're just now
     *   getting control - immediately return and indicate that we should
     *   restart 
     */
    if (restarting_)
        goto do_restart;

    /* remember whether we were in the debugger when we entered */
    old_in_dbg = in_debugger_;

    /* we're in the debugger */
    in_debugger_ = TRUE;

    /* flush the game window if appropriate */
    if (auto_flush_gamewin_)
        CHtmlDebugHelper::flush_main_output(ctx);

    /* 
     *   If the game window is in front, move it behind all of the
     *   debugger windows, since we probably are entering the debugger
     *   after having started reading a command (which brings the game
     *   window to the front). 
     */
    w32tdb_bring_group_to_top(get_handle());
    
    /* clear any temporary breakpoint that we had set */
    helper_->clear_temp_bp(ctx);

    /* update the status line */
    if (statusline_ != 0)
        statusline_->update();

    /* tell the main window that the game is paused */
    if (mainwin != 0)
        mainwin->set_game_paused(TRUE);

    /* 
     *   Display the current source file.  If we can't find one, bring the
     *   main debugger window (i.e., myself) to the front. 
     */
    win = (CHtmlSysWin_win32 *)helper_->update_for_entry(ctx, this);
    if (win != 0)
        win->bring_to_front();
    else
        bring_to_front();

    /* update the expressions window */
    update_expr_windows();

    /* check why we stopped, and display a message about it if appropriate */
    if (err != 0)
    {
        char msgbuf[1024];

        /* 
         *   we stopped due to a run-time error -- display a message box
         *   with the error text 
         */
        helper_->format_error(ctx, err, msgbuf, sizeof(msgbuf));
        MessageBox(0, msgbuf, "TADS Runtime Error",
                   MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
    }
    else
    {
        /*
         *   Check the breakpoint - if it's a global breakpoint, pop up a
         *   message box telling the user what happened, since it's not
         *   immediately obvious just looking at the code why we entered
         *   the debugger. 
         */
        if (helper_->is_bp_global(bphit))
        {
            textchar_t cond[HTMLDBG_MAXCOND + 100];
            int stop_on_change;
            size_t len;
            
            /* 
             *   it's a global breakpoint -- get the text of the
             *   condition, and display it so the user can see why we
             *   stopped 
             */
            strcpy(cond, "Stopped at global breakpoint: ");
            len = get_strlen(cond);
            if (!helper_->get_bp_cond(bphit, cond + len, sizeof(cond) - len,
                                      &stop_on_change))
            {
                /* 
                 *   if it's a stop-on-true breakpoint, the VM will have
                 *   automatically disabled it; note this 
                 */
                if (!stop_on_change)
                    strcat(cond, "\r\n\r\n(Note: This breakpoint "
                           "is now disabled.)");
                       
                /* show the message */
                MessageBox(0, cond, "TADS Workbench",
                           MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);
            }
        }
    }

    /* process events until we start stepping */
    go_ = FALSE;
    ret = CTadsApp::get_app()->event_loop(&go_);

    /* pop debugger status flag */
    in_debugger_ = old_in_dbg;

    /* update the statusline */
    if (statusline_ != 0)
        statusline_->update();

    /* 
     *   if we're quitting, force the return value to FALSE so that we
     *   terminate the game 
     */
    if (quitting_)
        ret = FALSE;

    /*
     *   if we're restarting, tell the caller to restart 
     */
    if (restarting_)
    {
    do_restart:
        /* start a new page in the main output window */
        if (mainwin != 0)
        {
            CHtmlDebugHelper::flush_main_output(ctx);
            mainwin->clear_all_pages();
        }

        /* tell the caller to do the restart */
        *restart = TRUE;

        /* if we're reloading, tell the caller about it */
        *reload = reloading_;

        /* only restart once per request */
        restarting_ = FALSE;

        /* make sure we re-enter the debugger as soon as the game restarts */
        helper_->set_exec_state_step_into(ctx);
    }

    /*
     *   If the 'aborting' flag is set, tell the caller to abort the
     *   current command 
     */
    if (aborting_cmd_)
    {
        /* only abort once per request */
        aborting_cmd_ = FALSE;

        /* make sure we re-enter the debugger on the next command */
        helper_->set_exec_state_step_into(ctx);

        /* tell the caller to do the abort */
        *abort = TRUE;
    }

    /* 
     *   if we're terminating the game, set the caller's terminate flag,
     *   but tell the caller to keep the debugger running 
     */
    if (terminating_)
    {
    do_terminate:
        /* tell the caller we're terminating */
        *terminate = TRUE;

        /* clear our flag now that we've told the caller about it */
        terminating_ = FALSE;

        /* release our self-reference */
        Release();

        /* tell the caller to keep the debugger loaded */
        return TRUE;
    }

    /* tell the main window that the game is running again, if it is */
    if (mainwin != 0)
        mainwin->set_game_paused(in_debugger_);

    /* release our self-reference */
    Release();

    /* return the result */
    return ret;
}

/*
 *   Update expression windows 
 */
void CHtmlSys_dbgmain::update_expr_windows()
{
    /* update the watch window */
    if (watchwin_ != 0)
        watchwin_->update_all();

    /* update the local variables */
    if (localwin_ != 0)
        localwin_->update_all();
}

/* 
 *   Add a new watch expression.  This will add the new expression at the
 *   end of the currently selected panel of the watch window.  
 */
void CHtmlSys_dbgmain::add_watch_expr(const textchar_t *expr)
{
    if (watchwin_ != 0)
    {
        /* add the expression */
        watchwin_->add_expr(expr, TRUE, TRUE);

        /* make sure the watch window is visible */
        ShowWindow(GetParent(watchwin_->get_handle()), SW_SHOW);
    }
}

/*
 *   Get the text for a Find command 
 */
const textchar_t *CHtmlSys_dbgmain::
   get_find_text(int command_id, int *exact_case, int *start_at_top,
                 int *wrap, int *dir)
{
    /*
     *   If this is a Find command, or if we don't have any text from the
     *   last "Find" command, run the dialog. 
     */
    if (command_id == ID_EDIT_FIND || find_text_[0] == '\0')
    {
        int new_exact_case = search_exact_case_;
        int new_search_from_top = search_from_top_;
        int new_wrap = search_wrap_;
        int new_dir = search_dir_;
        
        /* run the dialog - if they cancel, return null */
        if (!find_dlg_->run_find_dlg(get_handle(), find_text_,
                                     sizeof(find_text_),
                                     &new_exact_case, &new_search_from_top,
                                     &new_wrap, &new_dir))
            return 0;

        /* they accepted the dialog -- remember the new settings */
        search_exact_case_ = new_exact_case;
        search_from_top_ = new_search_from_top;
        search_wrap_ = new_wrap;
        search_dir_ = new_dir;

        /* use the search-from-top setting from the dialog */
        *start_at_top = search_from_top_;
    }
    else
    {
        /* 
         *   continuing an old search, so don't start over at the top,
         *   regardless of the last dialog settings 
         */
        *start_at_top = FALSE;
    }

    /* 
     *   fill in the flags from the last dialog settings, whether this is
     *   a new search or a continued search 
     */
    *exact_case = search_exact_case_;
    *wrap = search_wrap_;
    *dir = search_dir_;

    /* return the text to find */
    return find_text_;
}


/*
 *   Check command status 
 */
TadsCmdStat_t CHtmlSys_dbgmain::check_command(int command_id)
{
    CHtmlDbgSubWin *active_win;
    TadsCmdStat_t result;

    /* if it's a source file command, always enable it */
    if (command_id >= ID_FILE_OPENLINESRC_FIRST
        && command_id <= ID_FILE_OPENLINESRC_LAST)
        return TADSCMD_ENABLED;

    /* 
     *   if it's a window command, leave the status unchanged - the window
     *   menu builder will set these up properly 
     */
    if (command_id >= ID_WINDOW_FIRST
        && command_id <= ID_WINDOW_LAST)
        return TADSCMD_DO_NOT_CHANGE;

    /* check the command */
    switch(command_id)
    {
    case ID_DEBUG_SHOWHIDDENOUTPUT:
        return (!game_loaded_
                ? TADSCMD_DISABLED
                : helper_->get_dbg_hid() ? TADSCMD_CHECKED : TADSCMD_ENABLED);
        break;

    case ID_FILE_OPEN_NONE:
    case ID_WINDOW_NONE:
        /* dummy entry for when there are no source files; always disabled */
        return TADSCMD_DISABLED;

    case ID_DEBUG_EDITBREAKPOINTS:
    case ID_BUILD_SETTINGS:
    case ID_DEBUG_PROGARGS:
        /* enabled as long as a workspace is open */
        return workspace_open_ ? TADSCMD_ENABLED : TADSCMD_DISABLED;

    case ID_BUILD_STOP:
        /* we can try to halt a build if a build is running */
        return build_running_ ? TADSCMD_ENABLED : TADSCMD_DISABLED;

    case ID_BUILD_COMPDBG:
    case ID_BUILD_COMPDBG_AND_SCAN:
    case ID_BUILD_COMPDBG_FULL:
    case ID_BUILD_COMPRLS:
    case ID_BUILD_COMPEXE:
    case ID_BUILD_COMPINST:
    case ID_BUILD_COMP_AND_RUN:
    case ID_BUILD_CLEAN:
    case ID_PROJ_SCAN_INCLUDES:
        /* 
         *   these are enabled as long as a workspace is open and we're
         *   not already building 
         */
        return !build_running_ && workspace_open_
            ? TADSCMD_ENABLED : TADSCMD_DISABLED;

    case ID_FILE_RELOADGAME:
    case ID_FILE_RESTARTGAME:
    case ID_FILE_TERMINATEGAME:
        /* these are active as long as a game is loaded */
        return !game_loaded_ ? TADSCMD_DISABLED : TADSCMD_ENABLED;

    case ID_FILE_QUIT:
    case ID_FILE_OPEN:
    case ID_DEBUG_OPTIONS:
        /* always enabled */
        return TADSCMD_ENABLED;

    case ID_GAME_RECENT_NONE:
        /* this is a placeholder only - it's always disabled */
        return TADSCMD_DISABLED;

    case ID_FILE_CREATEGAME:
    case ID_FILE_LOADGAME:
    case ID_GAME_RECENT_1:
    case ID_GAME_RECENT_2:
    case ID_GAME_RECENT_3:
    case ID_GAME_RECENT_4:
        /* enabled as long as a build isn't running */
        return build_running_ ? TADSCMD_DISABLED : TADSCMD_ENABLED;

    case ID_DEBUG_GO:
        /* 
         *   "Go" works while a workspace is open, and we're not building.
         *   We can use "go" even if the game is already running, in which
         *   case we simply activate the game window.  
         */
        return (workspace_open_ && !build_running_
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);
        
    case ID_DEBUG_STEPINTO:
    case ID_DEBUG_STEPOVER:
        /* 
         *   these only work while the debugger is active, a workspace is
         *   open, and we're not building (we can go, step, etc., even if
         *   a game isn't currently loaded - this will simply reload the
         *   current game) 
         */
        return (in_debugger_ && workspace_open_ && !build_running_
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_DEBUG_STEPOUT:
        /* 
         *   step out only works when a game is loaded and the debugger
         *   has control 
         */
        return (in_debugger_ && game_loaded_
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_DEBUG_EVALUATE:
    case ID_DEBUG_ABORTCMD:
    case ID_DEBUG_SHOWNEXTSTATEMENT:
        /* 
         *   these only work when the game is running and the debugger is
         *   active 
         */
        return (in_debugger_ && game_loaded_
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_DEBUG_SETNEXTSTATEMENT:
    case ID_DEBUG_RUNTOCURSOR:
        /* 
         *   These work only while the debugger is active and the game is
         *   loaded and running, but may not always work when the debugger
         *   is active - disallow them here if the debugger is not active;
         *   otherwise allow them for now, but proceed with additional
         *   checks.  
         */
        if (!in_debugger_ || !game_loaded_)
            return TADSCMD_DISABLED;
        break;

    case ID_DEBUG_CLEARHISTORYLOG:
        /* 
         *   this doesn't work when the debugger isn't active; when the
         *   debugger is active, only allow it when there's a non-empty
         *   call history log 
         */
        return (in_debugger_
                && helper_->get_call_trace_len(dbgctx_) != 0
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_DEBUG_HISTORYLOG:
        /* 
         *   this works only while the debugger is active, but check it or
         *   not depending on whether tracing is on 
         */
        if (!game_loaded_)
            return TADSCMD_DISABLED;
        else if (helper_->is_call_trace_active(dbgctx_))
            return in_debugger_ ? TADSCMD_CHECKED
                                : TADSCMD_DISABLED_CHECKED;
        else
            return in_debugger_ ? TADSCMD_ENABLED
                                : TADSCMD_DISABLED;
        
    case ID_DEBUG_BREAK:
        /* this only works while the game is active */
        return !game_loaded_ || in_debugger_
            ? TADSCMD_DISABLED : TADSCMD_ENABLED;

    case ID_VIEW_STACK:
    case ID_VIEW_WATCH:
    case ID_VIEW_TRACE:
    case ID_VIEW_LOCALS:
    case ID_VIEW_PROJECT:
    case ID_SHOW_DEBUG_WIN:
    case ID_VIEW_GAMEWIN:
    case ID_HELP_ABOUT:
    case ID_HELP_TOPICS:
    case ID_HELP_TADSMAN:
    case ID_HELP_TADSPRSMAN:
    case ID_HELP_T3LIB:
        return TADSCMD_ENABLED;

    case ID_PROFILER_START:
        /* 
         *   enable it if a game is loaded, and the profiler isn't already
         *   running 
         */
        return (game_loaded_ && !profiling_
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_PROFILER_STOP:
        /* enable it only if the profiler is running */
        return (profiling_ ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_VIEW_STATUSLINE:
        return (show_statusline_ ? TADSCMD_CHECKED : TADSCMD_ENABLED);

    case ID_FLUSH_GAMEWIN:
        /* this only works when the debugger has control */
        return game_loaded_ && in_debugger_
            ? TADSCMD_ENABLED : TADSCMD_DISABLED;

    case ID_FLUSH_GAMEWIN_AUTO:
        /* check or uncheck according to the current setting */
        return auto_flush_gamewin_ ? TADSCMD_CHECKED : TADSCMD_ENABLED;

    case ID_WINDOW_CLOSE:
    case ID_WINDOW_CLOSE_ALL:
    case ID_WINDOW_NEXT:
    case ID_WINDOW_PREV:
    case ID_WINDOW_CASCADE:
    case ID_WINDOW_TILE_HORIZ:
    case ID_WINDOW_TILE_VERT:
        /* 
         *   these all work as long as we have one or more children in the
         *   MDI client window 
         */
        return (SendMessage(get_parent_of_children(),
                            WM_MDIGETACTIVE, 0, 0) == 0
                ? TADSCMD_DISABLED : TADSCMD_ENABLED);

    default:
        break;
    }

    /* 
     *   we didn't handle it -- see if the active debugger window wants to
     *   handle it 
     */
    active_win = get_active_dbg_win();
    if (active_win != 0)
    {
        /* run it past the active debugger window */
        result = active_win->active_check_command(command_id);

        /* if the active window handled the command, return the result */
        if (result != TADSCMD_UNKNOWN)
            return result;
    }

    /* try version-specific handling */
    if ((result = vsn_check_command(command_id)) != TADSCMD_UNKNOWN)
        return result;

    /* inherit default behavior */
    return CHtmlSys_framewin::check_command(command_id);
}

/* callback context for enumerating source windows */
struct winmenu_win_enum_cb_ctx_t
{
    /* number of windows inserted into the menu so far */
    int count_;

    /* the debugger main window object */
    CHtmlSys_dbgmain *dbgmain_;

    /* handle to the Window menu */
    HMENU menuhdl_;

    /* handle of active MDI child window */
    HWND active_;
};

/*
 *   Run the main debugger window's menu, activating it with the given
 *   system keydown message parameters.  This can be used when a window
 *   wants to let the keyboard menu interface in its window run the main
 *   debugger window's menu.  
 */
void CHtmlSys_dbgmain::run_menu_kb_ifc(int vkey, long keydata, HWND srcwin)
{
    /* 
     *   remember the current active window, so that we can activate it
     *   again when we're done 
     */
    post_menu_active_win_ = srcwin;

    /*
     *   Bring the debugger main window to the top.  We need to do this
     *   before we can open its menu, because Windows will always open the
     *   active window's menu when the keyboard interface runs, regardless
     *   of which window is actually the target of the message. 
     */
    BringWindowToTop(handle_);

    /* 
     *   send myself the WM_SYSKEYDOWN message that the source window
     *   delegated to us 
     */
    PostMessage(handle_, WM_SYSKEYDOWN, vkey, keydata);
}

/*
 *   Receive notification that we've closed a menu 
 */
int CHtmlSys_dbgmain::menu_close(unsigned int /*item*/)
{
    /* 
     *   if we opened the menu from the keyboard while another window was
     *   active, make the original window active again 
     */
    if (post_menu_active_win_ != 0)
    {
        BringWindowToTop(post_menu_active_win_);
        post_menu_active_win_ = 0;
    }
    
    /* return false to proceed with system default handling */
    return FALSE;
}

/*
 *   Initialize a popup menu that's about to be displayed 
 */
void CHtmlSys_dbgmain::init_menu_popup(HMENU menuhdl, unsigned int pos,
                                       int sysmenu)
{
    winmenu_win_enum_cb_ctx_t cbctx;
    int i;
    
    /* if this is the Window menu, initialize it */
    if (!sysmenu && menuhdl == win_menu_hdl_)
    {
        /* erase all of the window entries in the menu */
        for (i = GetMenuItemCount(menuhdl) ; i != 0 ; )
        {
            int id;
            
            /* move to the previous element */
            --i;
            
            /* get the item's command ID */
            id = GetMenuItemID(menuhdl, i);
            
            /* if it refers to a child window, delete it */
            if ((id >= ID_WINDOW_FIRST && id <= ID_WINDOW_LAST)
                || id == ID_WINDOW_NONE)
                DeleteMenu(menuhdl, i, MF_BYPOSITION);
        }
        
        /* insert all of the open source windows into the menu */
        cbctx.count_ = 0;
        cbctx.dbgmain_ = this;
        cbctx.menuhdl_ = menuhdl;
        cbctx.active_ = (HWND)SendMessage(get_parent_of_children(),
                                          WM_MDIGETACTIVE, 0, 0);
        helper_->enum_source_windows(&winmenu_win_enum_cb, &cbctx);
        
        /* if we didn't add any window item at all, add a placeholder item */
        if (cbctx.count_ == 0)
        {
            MENUITEMINFO info;
            static const textchar_t *itemname = "No Windows";
            
            /* set up the new menu item descriptor */
            info.cbSize = menuiteminfo_size_;
            info.fMask = MIIM_TYPE | MIIM_DATA | MIIM_ID;
            info.fType = MFT_STRING;
            info.wID = ID_WINDOW_NONE;
            info.dwTypeData = (char *)itemname;
            info.cch = get_strlen(itemname);
            
            /* insert the new menu item */
            InsertMenuItem(menuhdl, GetMenuItemCount(menuhdl), TRUE, &info);
        }
    }

    /* inherit the default handling now that we've built the menu */
    CHtmlSys_framewin::init_menu_popup(menuhdl, pos, sysmenu);
}

void CHtmlSys_dbgmain::winmenu_win_enum_cb(void *ctx0, int idx,
                                           CHtmlSysWin *win,
                                           int /*line_source_id*/,
                                           HtmlDbg_wintype_t win_type)
{
    winmenu_win_enum_cb_ctx_t *ctx = (winmenu_win_enum_cb_ctx_t *)ctx0;
    MENUITEMINFO info;
    textchar_t win_title[260];
    char *p;
    int menu_idx;
    CHtmlSysWin_win32 *syswin = (CHtmlSysWin_win32 *)win;

    /* 
     *   ignore everything but regular file windows (the special windows,
     *   such as the stack window and the history window, are already in
     *   the "view" menu) 
     */
    if (win_type != HTMLDBG_WINTYPE_SRCFILE)
        return;

    /* if we're on number 1 through 9, include a numeric shortcut */
    if (ctx->count_ + 1 <= 9)
    {
        sprintf(win_title, "&%d ", ctx->count_ + 1);
        p = win_title + get_strlen(win_title);
    }
    else
    {
        /* 
         *   don't include a shortcut prefix - put the title at the start
         *   of the buffer 
         */
        p = win_title;
    }

    /* get the window's title - use this as the name in the menu */
    GetWindowText(((CHtmlSysWin_win32 *)win)->get_frame_handle(),
                  p, sizeof(win_title) - (p - win_title));

    /* set up the new menu item descriptor for the window */
    info.cbSize = menuiteminfo_size_;
    info.fMask = MIIM_TYPE | MIIM_DATA | MIIM_ID;
    info.fType = MFT_STRING;
    info.wID = ID_WINDOW_FIRST + idx;
    info.dwTypeData = win_title;
    info.cch = get_strlen(win_title);

    /* insert the new menu item */
    menu_idx = GetMenuItemCount(ctx->menuhdl_),
    InsertMenuItem(ctx->menuhdl_, menu_idx, TRUE, &info);

    /* always enable these items */
    EnableMenuItem(ctx->menuhdl_, menu_idx, MF_BYPOSITION | MF_ENABLED);
    
    /* 
     *   If this is the active window, check it.  Note that the active MDI
     *   window will be a generic container window containing the actual
     *   source window, so we must check to see if the source window
     *   handle is a child of the active MDI window. 
     */
    if (IsChild(ctx->active_, syswin->get_handle()))
        CheckMenuItem(ctx->menuhdl_, menu_idx, MF_BYPOSITION | MF_CHECKED);

    /* count it */
    ctx->count_++;
}

/* ------------------------------------------------------------------------ */
/*
 *   Callback context for profile data enumerator 
 */
struct write_prof_ctx
{
    /* file to which we're writing the data */
    FILE *fp;
};

/*
 *   Write profiling data - enumeration callback 
 */
void CHtmlSys_dbgmain::write_prof_data(void *ctx0, const char *func_name,
                                       unsigned long time_direct,
                                       unsigned long time_in_children,
                                       unsigned long call_cnt)
{
    write_prof_ctx *ctx = (write_prof_ctx *)ctx0;

    /* write the data, tab-delimited */
    fprintf(ctx->fp, "%s\t%lu\t%lu\t%lu\n",
            func_name, time_direct, time_in_children, call_cnt);
}

/* ------------------------------------------------------------------------ */
/*
 *   Workbench "Credits" dialog.  This is a simple subclass of the standard
 *   HTML TADS credits dialog with some superficial customizations. 
 */
class CHtmlSys_wbcreditswin: public CHtmlSys_creditswin
{
public:
    CHtmlSys_wbcreditswin(class CHtmlSys_mainwin *mainwin)
        : CHtmlSys_creditswin(mainwin) { }

protected:
    /* get our window title */
    virtual const char *get_dlg_title() const
        { return "About TADS Workbench"; }

    /* get the program title for the dialog's display text */
    virtual const char *get_prog_title() const
    {
        return "<font size=5><b>TADS Workbench</b></font><br><br>"
            "for Windows 95/98/Me/NT/2000/XP";
    }

};

/*
 *   "About Workbench" dialog.  This is a simple subclass of the standard
 *   HTML TADS "About" dialog with some superficial customizations. 
 */
class CHtmlSys_aboutwbwin: public CHtmlSys_abouttadswin
{
public:
    CHtmlSys_aboutwbwin(class CHtmlSys_mainwin *mainwin)
        : CHtmlSys_abouttadswin(mainwin) { }

protected:
    /* get the dialog title */
    virtual const char *get_dlg_title() const
        { return "About TADS Workbench"; }

    /* get the name of the background picture */
    virtual const char *get_bkg_name() const { return "aboutwb.jpg"; }

    /* show the "Credits" dialog */
    virtual void show_credits_dlg()
    {
        /* create and run our Credits dialog subclass */
        (new CHtmlSys_wbcreditswin(mainwin_))->run_dlg(this);
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   Process a command
 */
int CHtmlSys_dbgmain::do_command(int notify_code, int command_id, HWND ctl)
{
    CHtmlDbgSubWin *active_win;

    /* get the active debugger window */
    active_win = get_active_dbg_win();

    /* 
     *   most commands require that the debugger be active; if it's not,
     *   ignore such commands 
     */
    switch(command_id)
    {
    case ID_HELP_TOPICS:
    case ID_HELP_TADSMAN:
    case ID_HELP_TADSPRSMAN:
    case ID_HELP_T3LIB:
        {
            char buf[256];
            char *rootname;

            /* get the executable's filename so we can get its directory */
            GetModuleFileName(CTadsApp::get_app()->get_instance(),
                              buf, sizeof(buf));

            /* find the directory part */
            rootname = os_get_root_name(buf);

            /* see what we have */
            switch(command_id)
            {
            case ID_HELP_TOPICS:
                /* look for the help file in the executable's directory */
                strcpy(rootname, "wbcont.htm");

                /* if it's not there, try the doc/wb subdirectory */
                if (GetFileAttributes(buf) == 0xffffffff)
                    strcpy(rootname, "doc\\wb\\wbcont.htm");
                break;

            case ID_HELP_T3LIB:
                strcpy(rootname, "doc\\libref\\index.html");
                break;

            case ID_HELP_TADSMAN:
                strcpy(rootname, w32_tadsman_fname);
                goto look_for_tadsman;
                
            case ID_HELP_TADSPRSMAN:
                strcpy(rootname, "doc\\parser.htm");
                goto look_for_tadsman;

            look_for_tadsman:
                /* try opening the manual file to see if it exists */
                if (GetFileAttributes(buf) == 0xffffffff)
                {
                    /* 
                     *   it doesn't exist - show the file explaining what
                     *   to do instead 
                     */
                    strcpy(rootname, "getmanuals.htm");
                    if (GetFileAttributes(buf) == 0xffffffff)
                        strcpy(rootname, "doc\\wb\\getmanuals.htm");
                }
                break;
            }

            /* open the file; if we fail, show an error */
            if ((unsigned long)ShellExecute(0, 0, buf, 0, 0, 0) <= 32)
                MessageBox(0, "The requested help file cannot be opened. "
                           "You must have a web browser or other HTML "
                           "viewer installed to view this help file.",
                           "TADS Workbench",
                           MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
        }
        return TRUE;

    case ID_HELP_ABOUT:
        (new CHtmlSys_aboutwbwin(CHtmlSys_mainwin::get_main_win()))
            ->run_dlg(this);
        return TRUE;
        
    case ID_DEBUG_GO:
        /* if a build is running, ignore it */
        if (build_running_)
        {
            Beep(1000, 100);
            return FALSE;
        }
        break;
        
    case ID_DEBUG_STEPINTO:
    case ID_DEBUG_STEPOVER:
    case ID_DEBUG_STEPOUT:
    case ID_DEBUG_RUNTOCURSOR:
    case ID_DEBUG_CLEARHISTORYLOG:
    case ID_DEBUG_HISTORYLOG:
        /* these commands all require the debugger to be active */
        if (!in_debugger_ || build_running_)
        {
            /* provide some minimal feedback in the form of an alert sound */
            Beep(1000, 100);

            /* ignore it */
            return FALSE;
        }
        break;

    case ID_DEBUG_ABORTCMD:
    case ID_DEBUG_EVALUATE:
    case ID_DEBUG_SHOWNEXTSTATEMENT:
    case ID_DEBUG_SETNEXTSTATEMENT:
        /* 
         *   these commands require the debugger to be active and the game
         *   to be running 
         */
        if (!in_debugger_ || !game_loaded_)
        {
            /* provide some minimal feedback in the form of an alert sound */
            Beep(1000, 100);

            /* ignore it */
            return FALSE;
        }
        break;
    }

    /*
     *   Check for a source-file open command 
     */
    if (command_id >= ID_FILE_OPENLINESRC_FIRST
        && command_id <= ID_FILE_OPENLINESRC_LAST)
    {
        CHtmlSysWin_win32 *win;
        
        /* open the line source and bring it to the front */
        win = (CHtmlSysWin_win32 *)helper_
              ->open_line_source(this,
                                 command_id - ID_FILE_OPENLINESRC_FIRST);
        if (win != 0)
            win->bring_to_front();
        
        /* handled */
        return TRUE;
    }

    /*
     *   Check for Window commands 
     */
    if (command_id >= ID_WINDOW_FIRST
        && command_id <= ID_WINDOW_LAST)
    {
        CHtmlSysWin_win32 *win;
        
        /* get the correct window */
        win = (CHtmlSysWin_win32 *)helper_
              ->get_source_window_by_index(command_id - ID_WINDOW_FIRST);

        /* bring it to the front */
        if (win != 0)
            win->bring_to_front();

        /* handled */
        return TRUE;
    }
    
    /* check the command */
    switch(command_id)
    {
    case ID_DEBUG_EVALUATE:
    case ID_DEBUG_ADDTOWATCH:
        /* 
         *   if the active window wants to handle it, let it; otherwise,
         *   provide a default implementation here 
         */
        if (active_win != 0
            && active_win->active_do_command(notify_code, command_id, ctl))
        {
            /* it handled it */
            return TRUE;
        }
        else
        {
            CHtmlFormatter *formatter;
            unsigned long sel_start, sel_end;
            CStringBuf buf;

            /* get the active window's selection range */
            if (active_win != 0
                && active_win->get_sel_range(&formatter, &sel_start,
                                             &sel_end))
            {
                unsigned long len;

                /* if the selection range is nil, get the word limits */
                if (sel_start == sel_end)
                    formatter->get_word_limits(&sel_start, &sel_end,
                                               sel_start);
                
                /* 
                 *   see how long the selection is - if it's too long,
                 *   don't bother 
                 */
                len = formatter->get_chars_in_ofs_range(sel_start, sel_end);
                if (len < 256)
                {
                    textchar_t *p;
                    int all_spaces;

                    /* extract the text */
                    formatter->extract_text(&buf, sel_start, sel_end);
                    
                    /* 
                     *   scan through the text, converting control
                     *   characters to spaces 
                     */
                    for (p = buf.get(), all_spaces = TRUE ;
                         p != 0 && *p != '\0' ; ++p)
                    {
                        /* if it's not printable, convert it to a space */
                        if (!isprint(*p))
                            *p = ' ';

                        /* if it's not a space, so note */
                        if (!isspace(*p))
                            all_spaces = FALSE;
                    }

                    /* 
                     *   if the buffer is nothing but spaces, consider it
                     *   to be blank 
                     */
                    if (all_spaces)
                        buf.set("");
                }
            }

            switch(command_id)
            {
            case ID_DEBUG_EVALUATE:
                /* run the dialog */
                CHtmlDbgWatchWinDlg::run_as_dialog(this, helper_, buf.get());
                break;

            case ID_DEBUG_ADDTOWATCH:
                add_watch_expr(buf.get());
                break;
            }
        }
        return TRUE;

    case ID_DEBUG_BREAK:
        /* force debugger entry */
        abort_game_command_entry();
        return TRUE;

    case ID_FILE_TERMINATEGAME:
        /* terminate the game */
        terminate_current_game();

        /* handled */
        return TRUE;

    case ID_GAME_RECENT_NONE:
        /* this is a placeholder only - ignore it */
        return TRUE;

    case ID_FILE_CREATEGAME:
    case ID_FILE_LOADGAME:
    case ID_FILE_RELOADGAME:
    case ID_GAME_RECENT_1:
    case ID_GAME_RECENT_2:
    case ID_GAME_RECENT_3:
    case ID_GAME_RECENT_4:
        /* can't do these if a compile is running */
        if (build_running_)
            return TRUE;

        /* if a game is already active, warn about it */
        if (get_game_loaded()
            && get_global_config_int("ask load dlg", 0, TRUE))
        {
            switch(MessageBox(handle_, "This will end the current debugging "
                              "session.  Are you sure you want to proceed?",
                              "TADS Workbench",
                              MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2))
            {
            case IDYES:
            case 0:
                /* proceed */
                break;

            default:
                /* ignore the request */
                return TRUE;
            }
        }

        /* if a game is currently running, terminate it */
        if (game_loaded_)
        {
            /* terminate the game */
            terminate_current_game();
            
            /* 
             *   re-post the command so that we get it again when we
             *   re-enter the debugger event loop after the game has
             *   terminated 
             */
            PostMessage(handle_, WM_COMMAND, command_id, 0);
            
            /* 
             *   done - we'll run the command again when we pick up the
             *   message we just posted 
             */
            return TRUE;
        }
            
        /* ask for the new game file if appropriate */
        if (command_id == ID_FILE_LOADGAME)
        {
            OPENFILENAME info;
            char fname[OSFNMAX];

            /* ask for a file to open */
            info.lStructSize = sizeof(info);
            info.hwndOwner = handle_;
            info.hInstance = CTadsApp::get_app()->get_instance();
            info.lpstrFilter = w32_debug_opendlg_filter;
            info.lpstrCustomFilter = 0;
            info.nFilterIndex = 0;
            info.lpstrFile = fname;
            fname[0] = '\0';
            info.nMaxFile = sizeof(fname);
            info.lpstrFileTitle = 0;
            info.nMaxFileTitle = 0;
            info.lpstrInitialDir = CTadsApp::get_app()->get_openfile_dir();
            info.lpstrTitle = "Select a game to load";
            info.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST
                         | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
            info.nFileOffset = 0;
            info.nFileExtension = 0;
            info.lpstrDefExt = 0;
            info.lCustData = 0;
            info.lpfnHook = 0;
            info.lpTemplateName = 0;
            CTadsDialog::set_filedlg_center_hook(&info);
            if (!GetOpenFileName(&info))
                return TRUE;

            /* validate the type of the file we're opening */
            if (vsn_validate_project_file(fname))
            {
                /* handled (or failed) - we're done */
                return TRUE;
            }

            /* 
             *   save the new directory as the initial directory for the
             *   next file open operation -- we do this in lieu of
             *   allowing the dialog to change the current directory,
             *   since we need to stay in the initial game directory at
             *   all times (since the game incorporates relative filename
             *   paths for source files) 
             */
            CTadsApp::get_app()->set_openfile_dir(fname);

            /* set up to load the new game */
            load_game(fname);
        }
        else if (command_id == ID_FILE_CREATEGAME)
        {
            /* run the new game wizard */
            run_new_game_wiz(handle_, this);

            /* handled */
            return TRUE;
        }
        else if (command_id >= ID_GAME_RECENT_1
                 && command_id <= ID_GAME_RECENT_4)
        {
            /* load the given recent game */
            load_recent_game(command_id);
        }
        else
        {
            /*
             *   They want to reload the current game - set up the reload
             *   name with the name of the current configuration file.  
             */
            load_game(local_config_filename_.get());
        }

        /* handled */
        return TRUE;

    case ID_FILE_OPEN:
        {
            OPENFILENAME info;
            char fname[256];
            CHtmlSysWin_win32 *win;

            /* ask for a file to open */
            info.lStructSize = sizeof(info);
            info.hwndOwner = handle_;
            info.hInstance = CTadsApp::get_app()->get_instance();
            info.lpstrFilter = "TADS Source Files\0*.t\0"
                               "TADS Libary Files\0*.tl\0"
                               "Text Files\0*.txt\0"
                               "All Files\0*.*\0\0";
            info.lpstrCustomFilter = 0;
            info.nFilterIndex = 0;
            info.lpstrFile = fname;
            fname[0] = '\0';
            info.nMaxFile = sizeof(fname);
            info.lpstrFileTitle = 0;
            info.nMaxFileTitle = 0;
            info.lpstrInitialDir = CTadsApp::get_app()->get_openfile_dir();
            info.lpstrTitle = "Select a text file";
            info.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST
                         | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
            info.nFileOffset = 0;
            info.nFileExtension = 0;
            info.lpstrDefExt = 0;
            info.lCustData = 0;
            info.lpfnHook = 0;
            info.lpTemplateName = 0;
            CTadsDialog::set_filedlg_center_hook(&info);
            if (!GetOpenFileName(&info))
                return TRUE;

            /* 
             *   save the new directory as the initial directory for the
             *   next file open operation -- we do this in lieu of
             *   allowing the dialog to change the current directory,
             *   since we need to stay in the initial game directory at
             *   all times (since the game incorporates relative filename
             *   paths for source files) 
             */
            CTadsApp::get_app()->set_openfile_dir(fname);

            /* open the file */
            win = (CHtmlSysWin_win32 *)
                  helper_->open_text_file(this, fname);
            if (win != 0)
                win->bring_to_front();
        }
        return TRUE;
        
    case ID_DEBUG_SHOWNEXTSTATEMENT:
        if (get_game_loaded())
        {
            CHtmlSysWin_win32 *win;

            /* tell the helper to activate the top item on the stack */
            win = (CHtmlSysWin_win32 *)helper_->activate_stack_win_line(
                0, dbgctx_, this);

            /* update expressions */
            update_expr_windows();

            /* bring the window to the front */
            if (win != 0)
                win->bring_to_front();
        }
        return TRUE;

    case ID_DEBUG_SHOWHIDDENOUTPUT:
        helper_->toggle_dbg_hid();
        return TRUE;

    case ID_FILE_QUIT:
        /* make sure they really want to quit */
        ask_quit();
        return TRUE;

    case ID_FILE_RESTARTGAME:
        /* set the flag to indicate that we should restart the game */
        restarting_ = TRUE;
        
        /* tell the event loop to relinquish control */
        go_ = TRUE;
        
        /* force debugger entry if we're reading a command line */
        abort_game_command_entry();

        /* handled */
        return TRUE;

    case ID_DEBUG_OPTIONS:
        /* run the options dialog */
        run_dbg_prefs_dlg(handle_, this);
        return TRUE;

    case ID_BUILD_SETTINGS:
        /* run the build settings dialog if there's a workspace open */
        if (workspace_open_)
            run_dbg_build_dlg(handle_, this, 0, 0);
        return TRUE;

    case ID_DEBUG_PROGARGS:
        /* run the arguments dialog if there's a workspace open */
        if (workspace_open_)
            vsn_run_dbg_args_dlg();
        return TRUE;

    case ID_BUILD_STOP:
        /* if a build is running, terminate it */
        if (build_running_)
            TerminateProcess(compiler_hproc_, (UINT)-1);

        /* handled */
        return TRUE;

    case ID_BUILD_COMPDBG:
    case ID_BUILD_COMPDBG_AND_SCAN:
    case ID_BUILD_COMPDBG_FULL:
    case ID_BUILD_COMPRLS:
    case ID_BUILD_COMPEXE:
    case ID_BUILD_COMPINST:
    case ID_BUILD_COMP_AND_RUN:
    case ID_BUILD_CLEAN:
    case ID_PROJ_SCAN_INCLUDES:
        /* can't do these if a build is running */
        if (build_running_)
            return TRUE;

        /* bring the log window to the front */
        SendMessage(handle_, WM_COMMAND, ID_SHOW_DEBUG_WIN, 0);

        /* 
         *   make sure we have minimally set the source file - if not,
         *   tell the user they need to configure the build 
         */
        if (workspace_open_)
        {
            textchar_t buf[OSFNMAX];
            int clrflag;

            /* 
             *   if a game is currently running, and we're going to
             *   rebuild the debug version, we need to terminate the game 
             */
            if (get_game_loaded()
                && (command_id == ID_BUILD_COMPDBG
                    || command_id == ID_BUILD_COMPDBG_AND_SCAN
                    || command_id == ID_BUILD_COMPDBG_FULL
                    || command_id == ID_BUILD_COMP_AND_RUN
                    || command_id == ID_BUILD_CLEAN))
            {
                int btn;

                /* ask them about it only if we're configured to do so */
                if (get_global_config_int("ask load dlg", 0, TRUE))
                {
                    /* ask them about it */
                    btn = MessageBox(handle_,
                                     "Compiling requires that the game be "
                                     "terminated.  Do you wish to terminate "
                                     "the game and proceed with the "
                                     "compilation?",
                                     "TADS Workbench",
                                     MB_YESNO | MB_ICONQUESTION
                                     | MB_DEFBUTTON2);

                    /* if they didn't say yes, abort */
                    if (btn != IDYES && btn != 0)
                    {
                        /* 
                         *   they don't want to stop the game - ignore the
                         *   compilation command 
                         */
                        return TRUE;
                    }
                }

                /* terminate the game */
                terminate_current_game();

                /* 
                 *   re-post the compile command, so that we try again
                 *   after the game finishes 
                 */
                PostMessage(handle_, WM_COMMAND, command_id, 0);

                /* 
                 *   done - we need to return from the event loop so that
                 *   we terminate the game; we'll try compiling again when
                 *   we get around to processing the re-posted command 
                 */
                return TRUE;
            }

            /* make version-specific checks to see if we can compile */
            if (!vsn_ok_to_compile())
                return TRUE;

            /*
             *   If they're building the release game, and they haven't
             *   set the release filename, or the release filename is the
             *   same as the game filename, they need to configure before
             *   they can build 
             */
            if (command_id == ID_BUILD_COMPRLS)
            {
                const char *msg;

                msg = 0;
                if (local_config_
                    ->getval("build", "rlsgam", 0, buf, sizeof(buf))
                    || strlen(buf) == 0)
                {
                    msg = "Before you can build the release game, you "
                          "must select the name of the release game file.  "
                          "Please enter the release game name, then use "
                          "the Compile command again.";
                }
                else if (stricmp(get_gamefile_name(), buf) == 0)
                {
                    msg = "The name of the release game is the same as the "
                          "debugging game.  You must use a different name "
                          "for the release version of the game so that it "
                          "doesn't overwrite the debug version.  Please "
                          "change the name, then use the Compile command "
                          "again.";
                }

                /* if we found a reason to complain, do so */
                if (msg != 0)
                {
                    /* show the error */
                    MessageBox(handle_, msg, "TADS Workbench",
                               MB_OK | MB_ICONEXCLAMATION);

                    /* run the configuration dialog */
                    run_dbg_build_dlg(handle_, this,
                                      DLG_BUILD_OUTPUT, IDC_FLD_RLSGAM);

                    /* we're done for now */
                    return TRUE;
                }
            }

            /* 
             *   if they're building an executable or an installer, and
             *   they haven't set the executable filename, they need to
             *   configure before they can run this 
             */
            if ((command_id == ID_BUILD_COMPEXE
                 || command_id == ID_BUILD_COMPINST)
                && (local_config_
                    ->getval("build", "exe", 0, buf, sizeof(buf))
                    || strlen(buf) == 0))
            {
                /* tell them what's wrong */
                MessageBox(handle_, "Before you can build an executable, "
                           "you must select the name of the executable "
                           "file in the build configuration.  Please enter "
                           "an executable name, then use the Compile "
                           "Application command again when you're done.",
                           "TADS Workbench", MB_OK | MB_ICONEXCLAMATION);

                /* show the dialog */
                run_dbg_build_dlg(handle_, this,
                                  DLG_BUILD_OUTPUT, IDC_FLD_EXE);

                /* 
                 *   don't proceed with compilation right now - wait for
                 *   the user to retry explicitly 
                 */
                return TRUE;
            }

            /*
             *   if they're building an installer, they must specify the
             *   name of the installation file
             */
            if (command_id == ID_BUILD_COMPINST
                && (local_config_->getval("build", "installer prog", 0, buf,
                                          sizeof(buf)) || strlen(buf) == 0))
            {
                /* tell them what's wrong */
                MessageBox(handle_, "Before you can build an installer, "
                           "you must select the name of the setup program "
                           "to create.  Please enter the information, "
                           "then use the Compile Installer command again "
                           "when you're done.", "TADS Workbench",
                           MB_OK | MB_ICONEXCLAMATION);

                /* show the dialog */
                run_dbg_build_dlg(handle_, this,
                                  DLG_BUILD_INSTALL, IDC_FLD_INSTALL_EXE);

                /* 
                 *   don't proceed with compilation right now - wait for
                 *   the user to retry explicitly 
                 */
                return TRUE;
            }

            /* clear the debug log window if desired */
            if (!global_config_->getval("dbglog", "clear on build", &clrflag)
                && clrflag)
            {
                /* clear the window */
                helper_->clear_debuglog_win();

                /* 
                 *   turn off auto-vscroll, since we're starting at the top -
                 *   the first error messages are usually the most relevant 
                 */
                if (helper_->get_debuglog_win() != 0)
                    ((CHtmlSysWin_win32 *)helper_->get_debuglog_win())
                        ->set_auto_vscroll(FALSE);
            }

            /* do the compilation */
            run_compiler(command_id);
        }
        
        /* handled */
        return TRUE;

    case ID_DEBUG_ABORTCMD:
        /* set the "abort" flag */
        aborting_cmd_ = TRUE;

        /* exit the debugger event loop, so that we can signal the abort */
        go_ = TRUE;
        return TRUE;

    case ID_DEBUG_GO:
        /* ignore if a build is running */
        if (build_running_)
            return TRUE;

        /* if the program is already running, just show the game window */
        if (!in_debugger_)
        {
            /* bring the game window to the front */
            if (CHtmlSys_mainwin::get_main_win() != 0)
                CHtmlSys_mainwin::get_main_win()->bring_owner_to_front();

            /* handled */
            return TRUE;
        }
        
        /* set the new execution state */
        helper_->set_exec_state_go(dbgctx_);

    continue_execution:
        /* 
         *   If we the game isn't currently loaded, we need to load the
         *   game in order to begin execution.
         */
        if (!game_loaded_)
        {
            /* 
             *   Save the project file configuration - do this just in case
             *   Workbench should do something unexpected (like crash) while
             *   we're running the game.  First, save the current settings to
             *   our in-memory configuration object, then save the
             *   configuration object to the disk file.  
             */
            save_config();
            vsn_save_config_file(local_config_, local_config_filename_.get());
            
            /* 
             *   set the RESTART and RELOAD flags to indicate that we need
             *   to load the game and start execution 
             */
            restarting_ = TRUE;
            reloading_ = TRUE;

            /* we're loading the same game whose workspace is already open */
            reloading_new_ = FALSE;

            /* 
             *   if the command was "go", start running as soon as we
             *   reload the game; otherwise, stop at the first instruction 
             */
            reloading_go_ = (command_id == ID_DEBUG_GO);

            /* begin execution */
            reloading_exec_ = TRUE;

            /* set the game to reload */
            if (CHtmlSys_mainwin::get_main_win() != 0)
            {
                /* set the pending game */
                CHtmlSys_mainwin::get_main_win()
                    ->set_pending_new_game(game_filename_.get());

                /* 
                 *   show the main window (do this immediately, so that the
                 *   window is visible while we're setting it up initially -
                 *   this is sometimes important to get correct sizing for
                 *   the toolbars and so on) 
                 */
                CHtmlSys_mainwin::get_main_win()->bring_owner_to_front();
            }
        }
        
        /* tell the event loop to relinquish control */
        go_ = TRUE;

        /* handled */
        return TRUE;

    case ID_DEBUG_STEPINTO:
        /* ignore if a build is running */
        if (build_running_)
            return TRUE;

        /* set new execution state */
        helper_->set_exec_state_step_into(dbgctx_);

        /* resume execution of the game */
        goto continue_execution;

    case ID_DEBUG_STEPOVER:
        /* ignore if a build is running */
        if (build_running_)
            return TRUE;

        /* set the new execution state */
        helper_->set_exec_state_step_over(dbgctx_);

        /* resume execution of the game */
        goto continue_execution;

    case ID_DEBUG_STEPOUT:
        /* set the new execution state */
        helper_->set_exec_state_step_out(dbgctx_);

        /* done with debugging for now */
        go_ = TRUE;
        return TRUE;

    case ID_VIEW_STACK:
        /* show the stack window */
        {
            CHtmlSysWin_win32 *win;

            /* create and/or show the stack window */
            win = (CHtmlSysWin_win32 *)helper_->create_stack_win(this);

            /* make sure it's up to date */
            helper_->update_stack_window(dbgctx_, FALSE);

            /* bring it to the front */
            if (win != 0)
                win->bring_to_front();
        }
        return TRUE;

    case ID_VIEW_TRACE:
    show_trace_window:
        /* show the history trace window */
        {
            CHtmlSysWin_win32 *win;

            /* create and/or show the history trace window */
            win = (CHtmlSysWin_win32 *)helper_->create_hist_win(this);

            /* make sure it's up to date */
            helper_->update_hist_window(dbgctx_);

            /* bring it to the front */
            if (win != 0)
                win->bring_to_front();
        }
        return TRUE;

    case ID_PROFILER_START:
        /* start the profiler */
        if (!profiling_)
        {
            /* start the profiler */
            helper_->start_profiling(dbgctx_);
            
            /* remember that it's running */
            profiling_ = TRUE;
        }
        return TRUE;

    case ID_PROFILER_STOP:
        if (profiling_)
        {
            OPENFILENAME info;
            char fname[OSFNMAX];
            
            /* stop the profiler */
            helper_->end_profiling(dbgctx_);

            /* remember that it's no longer running */
            profiling_ = FALSE;

            /* ask for a filename for dumping the profiler data */
            info.lStructSize = sizeof(info);
            info.hwndOwner = handle_;
            info.hInstance = CTadsApp::get_app()->get_instance();
            info.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
            info.lpstrCustomFilter = 0;
            info.nFilterIndex = 0;
            info.lpstrFile = fname;
            info.nMaxFile = sizeof(fname);
            info.lpstrFileTitle = 0;
            info.nMaxFileTitle = 0;
            info.lpstrTitle = "Select a file for saving profiling data";
            info.Flags = OFN_NOCHANGEDIR
                         | OFN_PATHMUSTEXIST
                         | OFN_OVERWRITEPROMPT;
            info.nFileOffset = 0;
            info.nFileExtension = 0;
            info.lpstrDefExt = "txt";
            info.lCustData = 0;
            info.lpfnHook = 0;
            info.lpTemplateName = 0;
            info.lpstrInitialDir = CTadsApp::get_app()->get_openfile_dir();
            CTadsDialog::set_filedlg_center_hook(&info);
            strcpy(fname, "profile.txt");
            if (GetSaveFileName(&info))
            {
                FILE *fp;
                
                /* open the file */
                fp = fopen(fname, "w");

                /* if we got a file, write the data */
                if (fp != 0)
                {
                    write_prof_ctx ctx;

                    /* set up our context */
                    ctx.fp = fp;

                    /* write the data */
                    helper_->get_profiling_data(dbgctx_, &write_prof_data,
                                                &ctx);
                    
                    /* close the file */
                    fclose(fp);
                }
                else
                {
                    MessageBox(handle_, "Unable to open file - profiling "
                               "data not written.", "TADS Workbench",
                               MB_OK | MB_ICONEXCLAMATION);
                }
            }
        }
        return TRUE;

    case ID_VIEW_WATCH:
        /* show the watch window and bring it to the front */
        if (watchwin_ != 0)
        {
            ShowWindow(GetParent(watchwin_->get_handle()), SW_SHOW);
            BringWindowToTop(GetParent(watchwin_->get_handle()));
        }
        return TRUE;

    case ID_VIEW_LOCALS:
        /* show the local variables window and bring it to the front */
        if (localwin_ != 0)
        {
            ShowWindow(GetParent(localwin_->get_handle()), SW_SHOW);
            BringWindowToTop(GetParent(localwin_->get_handle()));
        }
        return TRUE;

    case ID_VIEW_STATUSLINE:
        /* toggle the statusline */
        show_statusline_ = !show_statusline_;

        /* remember the new setting in the configuration */
        global_config_->setval("statusline", "visible", show_statusline_);

        /* show or hide the control window */
        if (statusline_ != 0)
            ShowWindow(statusline_->get_handle(),
                       show_statusline_ ? SW_SHOW : SW_HIDE);

        /* act as though we've been resized, to regenerate the layout */
        synth_resize();

        /* handled */
        return TRUE;

    case ID_VIEW_PROJECT:
        /* show the project window and bring it to the front */
        if (projwin_ != 0)
        {
            ShowWindow(GetParent(projwin_->get_handle()), SW_SHOW);
            BringWindowToTop(GetParent(projwin_->get_handle()));
        }
        return TRUE;
            
    case ID_SHOW_DEBUG_WIN:
        /* show the debug window and bring it to the front */
        if (helper_->get_debuglog_win() != 0)
        {
            CHtmlSysWin_win32 *win = (CHtmlSysWin_win32 *)
                                     helper_->get_debuglog_win();
            ShowWindow(GetParent(GetParent(win->get_handle())), SW_SHOW);
            BringWindowToTop(GetParent(GetParent(win->get_handle())));
        }
        return TRUE;

    case ID_VIEW_GAMEWIN:
        /* bring the game window to the front */
        if (CHtmlSys_mainwin::get_main_win() != 0)
            CHtmlSys_mainwin::get_main_win()->bring_owner_to_front();
        return TRUE;

    case ID_FLUSH_GAMEWIN:
        /* manually flush the game window if the debugger has control */
        if (game_loaded_ && in_debugger_)
            CHtmlDebugHelper::flush_main_output(dbgctx_);
        return TRUE;

    case ID_FLUSH_GAMEWIN_AUTO:
        /* reverse the current setting */
        auto_flush_gamewin_ = !auto_flush_gamewin_;
        
        /* save the new setting in the global preferences */
        global_config_->setval("gamewin auto flush", 0, auto_flush_gamewin_);

        /* handled */
        return TRUE;

    case ID_DEBUG_EDITBREAKPOINTS:
        /* run the breakpoints dialog */
        CTadsDialogBreakpoints::run_bp_dlg(this);
        return TRUE;

    case ID_DEBUG_HISTORYLOG:
        /* reverse history logging state */
        helper_->set_call_trace_active
            (dbgctx_, !helper_->is_call_trace_active(dbgctx_));

        /* 
         *   if we just turned on call tracing, show the trace window and
         *   bring it to the front 
         */
        if (helper_->is_call_trace_active(dbgctx_))
            goto show_trace_window;

        /* handled */
        return TRUE;

    case ID_DEBUG_CLEARHISTORYLOG:
        /* clear the history log */
        helper_->clear_call_trace_log(dbgctx_);
        return TRUE;

    case ID_WINDOW_CLOSE:
        {
            HWND hwnd;
            
            /* close the active MDI child */
            if ((hwnd = get_active_mdi_child(0)) != 0)
                SendMessage(hwnd, WM_CLOSE, 0, 0);
        }
        return TRUE;
        
    case ID_WINDOW_CLOSE_ALL:
        helper_->enum_source_windows(&enum_win_close_cb, this);
        return TRUE;
        
    case ID_WINDOW_NEXT:
        /* tell the client to activate the next window after the active one */
        SendMessage(get_parent_of_children(), WM_MDINEXT, 0, 0);
        return TRUE;
        
    case ID_WINDOW_PREV:
        /* tell the client to activate the previous window */
        SendMessage(get_parent_of_children(), WM_MDINEXT, 0, 1);
        return TRUE;

    case ID_WINDOW_CASCADE:
        /* tell the client to cascade windows */
        SendMessage(get_parent_of_children(), WM_MDICASCADE, 0, 0);
        return TRUE;
        
    case ID_WINDOW_TILE_HORIZ:
        /* tell the client to tile windows */
        SendMessage(get_parent_of_children(), WM_MDITILE,
                    MDITILE_HORIZONTAL, 0);
        return TRUE;

    case ID_WINDOW_TILE_VERT:
        /* tell the client to tile windows */
        SendMessage(get_parent_of_children(), WM_MDITILE,
                    MDITILE_VERTICAL, 0);
        return TRUE;

    default:
        break;
    }

    /* 
     *   we didn't handle it -- run the command by the active debugger
     *   window, and see if it can handle the command 
     */
    if (active_win != 0
        && active_win->active_do_command(notify_code, command_id, ctl))
        return TRUE;

    /* try version-specific handling */
    if (vsn_do_command(notify_code, command_id, ctl))
        return TRUE;

    /* inherit default behavior */
    return CHtmlSys_framewin::do_command(notify_code, command_id, ctl);
}

/*
 *   Open a source or text file 
 */
void CHtmlSys_dbgmain::open_text_file(const char *fname)
{
    CHtmlSysWin_win32 *win;

    /* ask the helper to open the file */
    win = (CHtmlSysWin_win32 *)helper_->open_text_file(this, fname);

    /* bring the window to the front if we successfully opened the file */
    if (win != 0)
    {
        /* bring the window to the top */
        win->bring_to_front();

        /* move focus to the window */
        SetFocus(win->get_handle());
    }
}

/*
 *   Load a recent game, given the ID_GAME_RECENT_n command ID of the game to
 *   load.  Returns true if we loaded a game successfully, false if there is
 *   no such recent game in the MRU list.  
 */
int CHtmlSys_dbgmain::load_recent_game(int command_id)
{
    const textchar_t *order;
    const textchar_t *fname;
    CHtmlPreferences *prefs;
    int idx;
    
    /* 
     *   get the main window preferences object, where our recent game list
     *   is stored 
     */
    if (CHtmlSys_mainwin::get_main_win() == 0)
        return FALSE;
    prefs = CHtmlSys_mainwin::get_main_win()->get_prefs();
    
    /* get the order string - if there isn't one, return failure */
    order = prefs->get_recent_dbg_game_order();
    if (order == 0)
        return FALSE;
    
    /* get the appropriate game, based on the order string */
    idx = command_id - ID_GAME_RECENT_1;
    fname = prefs->get_recent_dbg_game(order[idx]);

    /* if there's no such entry, return failure */
    if (fname == 0 || fname[0] == '\0')
        return FALSE;
    
    /* set the open-file directory to this game's path */
    CTadsApp::get_app()->set_openfile_dir(fname);
    
    /* if the entry isn't a valid project file, indicate failure */
    if (!vsn_is_valid_proj_file(fname))
        return FALSE;

    /* load the file */
    load_game(fname);
    
    /* success */
    return TRUE;
}

/*
 *   Load a game 
 */
void CHtmlSys_dbgmain::load_game(const textchar_t *fname)
{
    /*
     *   Note whether we're opening a new game.  If the filename is
     *   different than our original filename, assume we're loading a new
     *   game.  
     */
    reloading_new_ = !dbgsys_fnames_eq(fname, game_filename_.get());

    /* 
     *   set the new game as the pending game to load in the main window 
     */
    if (CHtmlSys_mainwin::get_main_win() != 0)
        CHtmlSys_mainwin::get_main_win()->set_pending_new_game(fname);

    /* save the outgoing configuration */
    save_config();

    /* set the RESTART and RELOAD flags */
    restarting_ = TRUE;
    reloading_ = TRUE;

    /* do not start execution - simply reload */
    reloading_go_ = FALSE;
    reloading_exec_ = FALSE;

    /* tell the event loop to relinquish control */
    go_ = TRUE;

    /* if we're entering a command, abort that */
    abort_game_command_entry();
}

/*
 *   Thread static entrypoint 
 */
DWORD WINAPI CHtmlSys_dbgmain::run_compiler_entry(void *ctx)
{
    CHtmlSys_dbgmain *mainwin = (CHtmlSys_dbgmain *)ctx;

    /* invoke the main compiler entrypoint */
    mainwin->run_compiler_th();

    /* the thread is finished */
    return 0;
}

/* 
 *   member function entrypoint for compiler thread 
 */
void CHtmlSys_dbgmain::run_compiler_th()
{
    time_t timeval;
    struct tm *tm;
    CHtmlDbgBuildCmd *cmd;
    CHtmlSys_mainwin *mainwin;
    int err;
    CHtmlFontDesc border_font;

    /* get the main window */
    mainwin = CHtmlSys_mainwin::get_main_win();

    /* get the font for the epilogue and prologue lines */
    dbgsys_set_srcfont(&border_font);
    border_font.weight = 700;
    border_font.italic = TRUE;

    /* show the build prologue */
    if (!capture_build_output_)
    {
        time(&timeval);
        tm = localtime(&timeval);
        mainwin->dbg_printf(&border_font, TRUE,
                            "----- begin build: %.24s -----\n",
                            asctime(tm));
    }

    /* 
     *   Go through the command list.  The main thread won't touch the
     *   command list as long as we're running, so we don't need to worry
     *   about synchronizing with the main thread when reading this list:
     *   the main thread synchronizes with us instead.  
     */
    for (err = FALSE, cmd = build_cmd_head_ ; cmd != 0 ; cmd = cmd->next_)
    {
        /* 
         *   if we've already encountered an error, and this command isn't
         *   marked as required, don't bother running it 
         */
        if (err && !cmd->required_)
            continue;
        
        /* execute this command */
        if (run_command(cmd->exe_name_.get(), cmd->exe_.get(),
                        cmd->args_.get(), cmd->dir_.get()))
        {
            /* 
             *   this command failed - note that we have an error, so that
             *   we skip any subsequent commands that aren't marked as
             *   required 
             */
            err = TRUE;
        }
    }

    /* add completion information to the debug log, if not capturing */
    if (!capture_build_output_)
    {
        /* show the overall result */
        mainwin->dbg_printf(FALSE, "\nBuild %s.\n",
                            err == 0 ? "successfully completed" : "failed");

        /* show the build epilogue */
        time(&timeval);
        tm = localtime(&timeval);
        mainwin->dbg_printf(&border_font, FALSE,
                            "----- end build: %.24s -----\n\n",
                            asctime(tm));
    }

    /* post the main window to tell it we're done with the build */
    PostMessage(handle_, HTMLM_BUILD_DONE, (WPARAM)err, 0);
}

/*
 *   Run an external command.  Displays a message if the command fails. 
 */
int CHtmlSys_dbgmain::run_command(const textchar_t *exe_name,
                                  const textchar_t *exe,
                                  const textchar_t *cmdline,
                                  const textchar_t *cmd_dir)
{
    int err;
    CHtmlSys_mainwin *mainwin;
    DWORD exit_code;

    /* get the main window */
    mainwin = CHtmlSys_mainwin::get_main_win();
    
    /* run the command */
    err = run_command_sub(exe_name, exe, cmdline, cmd_dir, &exit_code);

    /* if the command failed, show the error */
    if (err != 0)
    {
        char msg[256];

        /* 
         *   we couldn't start the command - check for the simplest case,
         *   which is that the executable we were trying to launch doesn't
         *   exist; in this case, provide a somewhat friendlier error than
         *   the system would normally do 
         */
        if (osfacc(exe))
        {
            /* the executable doesn't exist - explain this */
            mainwin->dbg_printf(FALSE, "error: executable file \"%s\" does "
                                "not exist or could not be opened - you "
                                "might need to re-install TADS.\n", exe);
        }
        else
        {
            DWORD err;

            /* get the last error code */
            err = GetLastError();

            /* 
             *   If the error is zero, it's hard to say for sure what the
             *   problem is.  One possibility that seems to arise in this
             *   case is that the working directory in which we tried to
             *   execute the command is invalid; check for this
             *   explicitly.  In other cases, show a generic failure
             *   message. (Don't show the actual Windows messages, because
             *   it will be "the operation completed successfully", which
             *   it obviously did not.) 
             */
            if (err == 0)
            {
                /* check for an invalid working directory */
                if (GetFileAttributes(cmd_dir) == 0xffffffff)
                {
                    /* the working directory doesn't exist - say so */
                    sprintf(msg, "source directory \"%s\" is invalid",
                            cmd_dir);
                }
                else
                {
                    /* 
                     *   somehow, we had a failure, but the Windows
                     *   internal error code didn't get set to anything
                     *   meaningful; show a generic error message 
                     */
                    strcpy(msg, "unable to execute program");
                }
            }
            else
            {
                /* 
                 *   we had a system error starting the process - get the
                 *   system error message and display it 
                 */
                FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0,
                              GetLastError(), 0, msg, sizeof(msg), 0);
            }

            /* show the message */
            mainwin->dbg_printf(FALSE, "%s: error: %s\n", exe_name, msg);
        }
    }
    else if (exit_code != 0)
    {
        /* we got an error from the subprogram itself */
        mainwin->dbg_printf(FALSE, "%s: error code %ld\n",
                            exe_name, exit_code);

        /* flag it as an error to our caller */
        err = 1;
    }

    /* return the result */
    return err;
}

/*
 *   Run an external command.  Returns the return value of the external
 *   command.  
 */
int CHtmlSys_dbgmain::run_command_sub(const textchar_t *exe_name,
                                      const textchar_t *exe,
                                      const textchar_t *cmdline,
                                      const textchar_t *cmd_dir,
                                      DWORD *exit_code)
{
    SECURITY_ATTRIBUTES sattr;
    HANDLE old_in;
    HANDLE old_out;
    HANDLE old_err;
    HANDLE chi_out_r, chi_out_r2;
    HANDLE chi_out_w;
    HANDLE chi_in_r;
    HANDLE chi_in_w;
    int result;
    PROCESS_INFORMATION pinfo;
    STARTUPINFO sinfo;
    CHtmlSys_mainwin *mainwin;
    const textchar_t *p;
    textchar_t endch;

    /* get the main window */
    mainwin = CHtmlSys_mainwin::get_main_win();

    /* clear out our command status on the statusline */
    compile_status_.set("");
    if (statusline_ != 0)
        statusline_->update();

    /* 
     *   find the argument portion of the command line - skip the first
     *   quoted token, which is the full path to the program we're
     *   invoking 
     */
    p = cmdline;
    if (*p == '"')
    {
        /* it starts with a quote, so find the closing quote */
        endch = '"';

        /* skip the open quote */
        ++p;
    }
    else
    {
        /* it doesn't start with a quote - just find the first space */
        endch = ' ';
    }

    /* seek the end of the first token */
    for ( ; *p != '\0' && *p != endch ; ++p) ;

    /* if we stopped at a quote, skip the quote */
    if (*p == '"')
        ++p;

    /* skip spaces */
    for ( ; isspace(*p) ; ++p) ;

    /* presume the child process will indicate success */
    *exit_code = 0;

    /* check for our special internal commands */
    if (stricmp(exe, "del") == 0)
    {
        /* 
         *   display the special command syntax - the command line is just
         *   the filename to be deleted 
         */
        mainwin->dbg_printf(FALSE, ">del %s\n", cmdline);
        
        /* try deleting the file - return an error if it fails */
        if (!DeleteFile(cmdline))
            *exit_code = GetLastError();
        
        /* 
         *   we successfully ran the command (even if the command failed, we
         *   successfully *executed* it; a failure will be reflected in the
         *   child process exit code) 
         */
        return 0;
    }
    else if (stricmp(exe, "rmdir/s") == 0)
    {
        /* it's a remove-directory command - display special syntax */
        mainwin->dbg_printf(FALSE, ">rmdir /s %s\n", cmdline);

        /* 
         *   delete the directory and its contents; the result is the "child
         *   process" exit code, since we're running the operation in-line 
         */
        *exit_code = del_temp_dir(cmdline);

        /* 
         *   we successfully "ran the child process" (the rmdir/s could have
         *   failed, but if it did, that's a failure in the "child process"
         *   rather than in executing the command) 
         */
        return 0;
    }

    /* 
     *   write the command line we're executing to the debug log; don't echo
     *   the command if we're capturing build output for our own purposes 
     */
    if (!capture_build_output_)
        mainwin->dbg_printf(FALSE, ">%s %s\n", exe_name, p);

    /* no compiler status yet */
    compile_status_.set("");

    /* set up the security attributes to make an inheritable pipe */
    sattr.nLength = sizeof(sattr);
    sattr.bInheritHandle = TRUE;
    sattr.lpSecurityDescriptor = 0;

    /* save the original stdout and stderr */
    old_out = GetStdHandle(STD_OUTPUT_HANDLE);
    old_err = GetStdHandle(STD_ERROR_HANDLE);

    /* create the output pipe for the child */
    if (!CreatePipe(&chi_out_r, &chi_out_w, &sattr, 0))
        return -1;

    /* 
     *   set standard out to the child write handle, so that the child
     *   inherits the pipe as its stdout 
     */
    if (!SetStdHandle(STD_OUTPUT_HANDLE, chi_out_w)
        || !SetStdHandle(STD_ERROR_HANDLE, chi_out_w))
        return -1;

    /* make the read handle noninheritable */
    if (!DuplicateHandle(GetCurrentProcess(), chi_out_r,
                         GetCurrentProcess(), &chi_out_r2, 0,
                         FALSE, DUPLICATE_SAME_ACCESS))
        return -1;

    /* close the original (inheritable) child output read handle */
    CloseHandle(chi_out_r);

    /* save our old stdin */
    old_in = GetStdHandle(STD_INPUT_HANDLE);

    /* create a pipe for the child to use as stdin */
    if (!CreatePipe(&chi_in_r, &chi_in_w, &sattr, 0))
        return -1;

    /*
     *   set the standard in to the child read handle, so that the child
     *   inherits the pipe as its stdin 
     */
    if (!SetStdHandle(STD_INPUT_HANDLE, chi_in_r))
        return -1;

    /* 
     *   close our end of the child's standard input handle, since we
     *   can't provide any input to the child - the child will just get an
     *   eof if it tries to read from its stdin 
     */
    CloseHandle(chi_in_w);

    /* set up our STARTUPINFO */
    memset(&sinfo, 0, sizeof(sinfo));
    sinfo.cb = sizeof(sinfo);
    sinfo.dwFlags = STARTF_USESTDHANDLES;
    sinfo.hStdInput = chi_in_r;
    sinfo.hStdOutput = chi_out_w;
    sinfo.hStdError = chi_out_w;

    /* start the child program */
    result = CreateProcess(exe, (char *)cmdline, 0, 0, TRUE,
                           DETACHED_PROCESS, 0, cmd_dir, &sinfo, &pinfo);

    /* 
     *   remember the compiler's process handle, in case we need to forcibly
     *   terminate the process 
     */
    compiler_hproc_ = pinfo.hProcess;
    compiler_pid_ = pinfo.dwProcessId;

    /* update the statusline to indicate the compiler is running */
    if (statusline_ != 0)
        statusline_->update();

    /* 
     *   we don't need the thread handle for anything - close it if we
     *   successfully started the process 
     */
    if (result != 0)
        CloseHandle(pinfo.hThread);

    /* restore our original stdin and stdout */
    SetStdHandle(STD_INPUT_HANDLE, old_in);
    SetStdHandle(STD_OUTPUT_HANDLE, old_out);
    SetStdHandle(STD_ERROR_HANDLE, old_err);

    /* close our copies of the child's ends of the handles */
    CloseHandle(chi_in_r);
    CloseHandle(chi_out_w);

    /* 
     *   if the process started successfully, read its standard output and
     *   wait for it to complete 
     */
    if (result)
    {
        CStringBuf line_so_far;

        /* keep going until we get an eof on the pipe */
        for (;;)
        {
            char buf[256];
            DWORD bytes_read;
            char *src;
            char *dst;

            /* read from the pipe */
            if (!ReadFile(chi_out_r2, buf, sizeof(buf), &bytes_read, FALSE))
                break;

            /* 
             *   scan for newlines - remove any '\r' characters (console
             *   apps tend to send us a "\r\n" sequence for each newline;
             *   we only need a C-style "\n" character, so eliminate all
             *   of the "\r" characters)
             */
            for (src = dst = buf ; src < buf + bytes_read ; ++src)
            {
                /* if this isn't a '\r' character, copy it to the output */
                if (*src != '\r')
                    *dst++ = *src;
            }

            /* set the new length, with the '\r's eliminated */
            bytes_read = dst - buf;

            /* if that leaves anything, process it */
            if (bytes_read != 0)
            {
                /* 
                 *   write the text to the console, or process it if we're
                 *   capturing the output 
                 */
                if (capture_build_output_ || filter_build_output_)
                {
                    const char *p;
                    const char *start;
                    size_t rem;

                    /* 
                     *   We're capturing the build output.  Scan the text
                     *   for each newline.  
                     */
                    for (start = p = buf, rem = bytes_read ; rem != 0 ;
                         ++p, --rem)
                    {
                        /* if we're at a newline, process this chunk */
                        if (*p == '\n')
                        {
                            /* 
                             *   append this chunk to any partial line left
                             *   over at the end of the previous chunk 
                             */
                            line_so_far.append_inc(start, p - start, 512);

                            /* process this line */
                            if (vsn_process_build_output(&line_so_far)
                                && !capture_build_output_)
                            {
                                /* they want to display the text */
                                mainwin->dbg_printf(FALSE, "%s\n",
                                    line_so_far.get());
                            }

                            /* flush the line buffer */
                            line_so_far.set("");

                            /* 
                             *   move the starting point for the next line
                             *   to the character following the newline 
                             */
                            start = p + 1;
                        }
                    }

                    /* 
                     *   if we have anything left over after the last
                     *   newline, copy it to the partial line buffer so that
                     *   we'll process it next time 
                     */
                    if (p != start)
                        line_so_far.append_inc(start, p - start, 512);
                }
                else
                {
                    /* send the output to the debug log window */
                    mainwin->dbg_printf(FALSE, "%.*s", (int)bytes_read, buf);
                }
            }
        }

        /* finish the output */
        if (capture_build_output_ || filter_build_output_)
        {
            /* process the last chunk */
            if (line_so_far.get() != 0
                && line_so_far.get()[0] != '\0' 
                && vsn_process_build_output(&line_so_far))
            {
                /* show the line */
                mainwin->dbg_printf(FALSE, "%s\n", line_so_far.get());
            }

            /* add an extra blank line if not capturing everything */
            if (!capture_build_output_)
                mainwin->dbg_printf(FALSE, "\n");
        }
        else
        {
            /* add an extra blank line to the debug log output */
            mainwin->dbg_printf(FALSE, "\n");
        }

        /* clear out our command status on the statusline */
        compile_status_.set("");
        if (statusline_ != 0)
            statusline_->update();

        /* wait for the process to terminate */
        WaitForSingleObject(pinfo.hProcess, INFINITE);

        /* get the child result code */
        GetExitCodeProcess(pinfo.hProcess, exit_code);

        /* close the process handle */
        CloseHandle(pinfo.hProcess);

        /* success */
        result = 0;
    }
    else
    {
        /* we didn't start the process - return an error */
        result = -1;
    }

    /* forget the compiler process handle */
    compiler_hproc_ = 0;

    /* update the statusline */
    if (statusline_ != 0)
        statusline_->update();

    /* close our end of the stdout pipe */
    CloseHandle(chi_out_r2);

    /* return the result from the child process */
    return result;
}

/*
 *   Delete a temporary directory.  Returns zero on success, non-zero on
 *   failure.  
 */
int CHtmlSys_dbgmain::del_temp_dir(const char *dir)
{
    char pat[MAX_PATH];
    HANDLE shnd;
    WIN32_FIND_DATA info;

    /* if the directory name is empty, ignore this */
    if (dir == 0 || dir[0] == '\0')
        return 0;

    /* set up to find all files in this directory */
    os_build_full_path(pat, sizeof(pat), dir, "*");

    /* start the search */
    shnd = FindFirstFile(pat, &info);
    if (shnd != INVALID_HANDLE_VALUE)
    {
        int err;

        /* keep going until we run out of files */
        for (;;)
        {
            /* delete all files other than "." and ".." */
            if (strcmp(info.cFileName, ".") != 0
                && strcmp(info.cFileName, "..") != 0)
            {
                char fullname[MAX_PATH];

                /* generate the full path to this file */
                os_build_full_path(fullname, sizeof(fullname),
                                   dir, info.cFileName);

                /* if it's a directory, delete it recursively */
                if ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                {
                    /* delete the directory recursively */
                    if ((err = del_temp_dir(fullname)) != 0)
                        return err;
                }
                else
                {
                    /* delete this file */
                    if (!DeleteFile(fullname))
                        return GetLastError();
                }
            }

            /* move on to the next file */
            if (!FindNextFile(shnd, &info))
                break;
        }

        /* note the error that caused us to stop looping */
        err = GetLastError();

        /* close the search handle */
        FindClose(shnd);

        /* make sure we stopped because we were out of files */
        if (err != ERROR_NO_MORE_FILES)
            return err;
    }

    /* the directory should now be completely empty, so delete it */
    if (!RemoveDirectory(dir))
        return GetLastError();

    /* success */
    return 0;
}


/*
 *   Clear the list of build commands 
 */
void CHtmlSys_dbgmain::clear_build_cmds()
{
    CHtmlDbgBuildCmd *cur;
    CHtmlDbgBuildCmd *nxt;

    /* delete each item in the list */
    for (cur = build_cmd_head_ ; cur != 0 ; cur = nxt)
    {
        /* remember the next one */
        nxt = cur->next_;

        /* delete this one */
        delete cur;
    }

    /* clear the head and tail pointers */
    build_cmd_head_ = build_cmd_tail_ = 0;
}


/*
 *   Add a build command at the end of the build command list 
 */
void CHtmlSys_dbgmain::add_build_cmd(const textchar_t *exe_name,
                                     const textchar_t *exe,
                                     const textchar_t *cmdline,
                                     const textchar_t *cmd_dir,
                                     int required)
{
    CHtmlDbgBuildCmd *cmd;

    /* create a build command object */
    cmd = new CHtmlDbgBuildCmd(exe_name, exe, cmdline, cmd_dir, required);

    /* link it in at the end of the list */
    cmd->next_ = 0;
    if (build_cmd_tail_ == 0)
        build_cmd_head_ = cmd;
    else
        build_cmd_tail_->next_ = cmd;
    build_cmd_tail_ = cmd;
}


/*
 *   Receive notification that source window preferences have changed.
 *   We'll update the source windows accordingly.  
 */
void CHtmlSys_dbgmain::on_update_srcwin_options()
{
    int new_tab_size;

    /* update each window's color settings */
    helper_->enum_source_windows(&options_enum_srcwin_cb, this);

    /* update tab size if necessary */
    if (!local_config_->getval("src win options", "tab size", &new_tab_size)
        && helper_->get_tab_size() != new_tab_size)
    {
        /* 
         *   Tell the helper to update the tab size - this will
         *   necessitate reloading all source files, since tabs are
         *   translated at load time.  The helper will reformat all of the
         *   windows as a result, so we'll pick up any other changes to
         *   the source options.  
         */
        helper_->set_tab_size(this, new_tab_size);
    }
    else
    {
        /* reformat the source windows to account for the changes */
        helper_->reformat_source_windows(dbgctx_, this);
    }
}

/*
 *   Resume execution 
 */
void CHtmlSys_dbgmain::exec_go()
{
    /* set the new execution state */
    helper_->set_exec_state_go(dbgctx_);

    /* tell the event loop to relinquish control */
    go_ = TRUE;
}

/*
 *   Resume execution for a single step 
 */
void CHtmlSys_dbgmain::exec_step_into()
{
    /* set the new execution state */
    helper_->set_exec_state_step_into(dbgctx_);

    /* tell the event loop to return control to the run-time */
    go_ = TRUE;
}

/* 
 *   enumeration callback for applying options changes to source windows 
 */
void CHtmlSys_dbgmain::options_enum_srcwin_cb(void *ctx, int /*idx*/,
                                              CHtmlSysWin *win,
                                              int /*line_source_id*/,
                                              HtmlDbg_wintype_t win_type)
{
    /* if it's a source window, notify the window of the option changes */
    switch(win_type)
    {
    case HTMLDBG_WINTYPE_SRCFILE:
    case HTMLDBG_WINTYPE_STACK:
    case HTMLDBG_WINTYPE_HISTORY:
    case HTMLDBG_WINTYPE_DEBUGLOG:
        /* 
         *   these are all derived from the basic source window type, so
         *   these all need to be notified of formatting changes 
         */
        ((CHtmlSys_dbgmain *)ctx)
            ->note_formatting_changes((CHtmlSysWin_win32 *)win);
        break;

    default:
        /* other types are not affected by formatting changes */
        break;
    }
}

/*
 *   Receive notification of global formatting option changes, updating the
 *   given window accordingly.  
 */
void CHtmlSys_dbgmain::note_formatting_changes(CHtmlSysWin_win32 *win)
{
    HTML_color_t bkg_color, text_color;
    int use_win_colors;

    /* get the color preferences */
    if (global_config_->getval("src win options", "use win colors",
                               &use_win_colors))
        use_win_colors = TRUE;
    if (global_config_->getval_color("src win options", "bkg color",
                                     &bkg_color))
        bkg_color = HTML_make_color(0xff, 0xff, 0xff);
    if (global_config_->getval_color("src win options", "text color",
                                     &text_color))
        text_color = HTML_make_color(0, 0, 0);

    /* apply the current color settings to the window */
    win->note_debug_format_changes(bkg_color, text_color, use_win_colors);
}

/*
 *   Get the last active window for command routing.  If there's an active
 *   debugger window, we'll return that window.  If the debugger main
 *   window is itself the active window, and the previous active debugger
 *   window is the second window in Z order, we'll return that window,
 *   since we left it explicitly to come to the debugger main window,
 *   presumably for a toolbar or menu operation. 
 */
CHtmlDbgSubWin *CHtmlSys_dbgmain::get_active_dbg_win()
{
    HWND top_owned;
    
    /* if we have an active debugger window, use it */
    if (active_dbg_win_ != 0)
        return active_dbg_win_;

#ifdef W32TDB_MDI

    /* ask the MDI client for its active window */
    top_owned = (HWND)SendMessage(get_parent_of_children(),
                                  WM_MDIGETACTIVE, 0, 0);

#else /* W32TDB_MDI */

    /* find the top window that I own */
    for (top_owned = GetTopWindow(0) ;
         top_owned != 0 && GetWindow(top_owned, GW_OWNER) != handle_ ;
         top_owned = GetWindow(top_owned, GW_HWNDNEXT)) ;

#endif /* W32TDB_MDI */

    /*
     *   If a debug window was previously active, and the main debugger
     *   window itself is the system's idea of the active window, and the
     *   previously active debug window is our main debug window's top
     *   "owned" window or a child of our top owned window, then consider
     *   that previous window to be still active.  In this case, route the
     *   command to that previous window. 
     */
    if (last_active_dbg_win_ != 0
        && GetActiveWindow() == handle_
        && top_owned != 0
        && (top_owned == last_active_dbg_win_->active_get_handle()
            || IsChild(top_owned, last_active_dbg_win_->active_get_handle())))
        return last_active_dbg_win_;

    /* there isn't an appropriate window */
    return 0;
}

/*
 *   Bring the window to the front 
 */
void CHtmlSys_dbgmain::bring_to_front()
{
    BringWindowToTop(handle_);
}

/*
 *   Set the active debugger window.  Source, stack, and other debugger
 *   windows should set themselves active when they're activated, and
 *   should clear the active window when they're deactivated.  We keep
 *   track of the active debugger window for command routine purposes. 
 */
void CHtmlSys_dbgmain::set_active_dbg_win(CHtmlDbgSubWin *win)
{
    /* remember the active window */
    active_dbg_win_ = win;

    /* 
     *   in case the new window isn't a source window, clear the source
     *   line/column number panel - if it does turn out to be a source
     *   window, it'll update the panel itself 
     */
    if (statusline_ != 0)
        SendMessage(statusline_->get_handle(), SB_SETTEXT,
                    STATPANEL_LINECOL, (LPARAM)"");
}

/*
 *   Clear the active debugger window.  Windows that register themselves
 *   upon activation with set_active_dbg_win() should call this routine
 *   when they're deactivated.  
 */
void CHtmlSys_dbgmain::clear_active_dbg_win()
{
    /* 
     *   Remember this as the previous active debugger window.  When the
     *   debugger main window is the active window, and the previously
     *   active window is the next window in window order, we'll route
     *   commands that belong to debugger windows to the old active
     *   window. 
     */
    last_active_dbg_win_ = active_dbg_win_;

    /* forget the window */
    active_dbg_win_ = 0;
}

/*
 *   Receive notification that we're closing a debugger window.  Any
 *   window that registers itself with set_active_dbg_win() on activation
 *   must call this routine when the window is destroyed, to ensure that
 *   we don't try to route any commands to the window after it's gone. 
 */
void CHtmlSys_dbgmain::on_close_dbg_win(CHtmlDbgSubWin *win)
{
    /* 
     *   if this is the old active window or the current active window,
     *   forget about it 
     */
    if (active_dbg_win_ == win)
        active_dbg_win_ = 0;
    if (last_active_dbg_win_ == win)
        last_active_dbg_win_ = 0;
}

/*
 *   Open a file for editing in the external text editor.  Returns true on
 *   success, false on error.  
 */
int CHtmlSys_dbgmain::open_in_text_editor(const textchar_t *fname,
                                          long linenum, long colnum,
                                          int local_if_no_linenum)
{
    char prog[1024];
    char args[1024];
    char *p;
    char *seg;
    CStringBuf expanded_args;
    int success;
    int found_pctf;
    int found_pctn;
    char numbuf[20];
    char path[OSFNMAX];

    /* 
     *   if the filename isn't absolute, try to find it in the usual
     *   places 
     */
    if (os_is_file_absolute(fname))
    {
        /* the name is already an absolute path - use it as-is */
        strcpy(path, fname);
    }
    else
    {
        /* look for the file - fail if we can't */
        if (!html_dbgui_find_src(fname, strlen(fname),
                                 path, sizeof(path), TRUE))
            return FALSE;
    }

    /* get the editor program - use NOTEPAD as the default */
    if (global_config_->getval("editor", "program", 0, prog, sizeof(prog))
        || strlen(prog) == 0)
        strcpy(prog, "Notepad.EXE");

    /* get the editor command line - use "%f" as the default */
    if (global_config_->getval("editor", "cmdline", 0, args, sizeof(args))
        || strlen(args) == 0)
        strcpy(args, "%f");

    /* presume we won't find a %f or %n substitution parameters */
    found_pctf = FALSE;
    found_pctn = FALSE;

    /* start the command line empty */
    expanded_args.set("");
    
    /* substitute the "%" parameters */
    for (seg = p = args ; *p != '\0' ; ++p)
    {
        /* if this is a percent sign, substitute the parameter */
        if (*p == '%')
        {
            long adjust_amt = 0;

            /* append the part up to this point */
            if (p > seg)
                expanded_args.append(seg, p - seg);
            
            /* skip the '%' */
            ++p;
            
        apply_subst_code:
            /* see what we have */
            switch(*p)
            {
            case 'f':
                /* substitute the filename */
                expanded_args.append(path);
                
                /* note that we found a filename */
                found_pctf = TRUE;
                break;

            case '0':
                /* 
                 *   zero-base adjustment - if we have another character
                 *   following, note the -1 adjustment (since we want to pass
                 *   a zero-based value to the program, but we were passed
                 *   1-based values), and go back to apply the actual code
                 *   from the next character 
                 */
                if (*(p+1) != '\0')
                {
                    /* adjust for the zero base */
                    adjust_amt = -1;

                    /* skip the '0' qualifier */
                    ++p;

                    /* go apply the real substitution code */
                    goto apply_subst_code;
                }
                break;
                
            case 'n':
                /* substitute the line number */
                sprintf(numbuf, "%lu", linenum + adjust_amt);
                expanded_args.append(numbuf);

                /* note that we found a line number specifier */
                found_pctn = TRUE;
                break;

            case 'c':
                /* substitute the column number */
                sprintf(numbuf, "%lu", colnum + adjust_amt);
                expanded_args.append(numbuf);
                break;

            case '%':
                /* substitute a single % */
                expanded_args.append("%");
                break;
                
            default:
                /* leave anything else intact */
                expanded_args.append("%");
                --p;
                break;
            }
            
            /* the next segment starts at the next character */
            seg = p + 1;
        }
    }
    
    /* append anything remaining after the last parameter */
    if (p > seg)
        expanded_args.append(seg, p - seg);
    
    /* if we didn't find a "%f", append the filename */
    if (!found_pctf)
        expanded_args.append(path);

    /*
     *   If we didn't find a line number specifier in the command string,
     *   we can't go to the selected line in the external editor.  If the
     *   caller requested that we open the file locally in such a case,
     *   open the file locally. 
     */
    if (!found_pctn && local_if_no_linenum)
    {
        CHtmlSysWin_win32 *win;
        
        /* open the source file in a viewer window */
        win = (CHtmlSysWin_win32 *)helper_
              ->open_text_file_at_line(this, fname, path, linenum);

        /* bring the window to the front */
        if (win != 0)
            win->bring_to_front();

        /* done */
        return TRUE;
    }
    
    /* presume failure */
    success = FALSE;

    /* 
     *   if it's a DDE command, execute it through DDE; otherwise, it's
     *   just a program to invoke as an application 
     */
    if (strlen(prog) > 4 && memicmp(prog, "DDE:", 4) == 0)
    {
        dde_info_t dde_info;
        char *server;
        char *topic;
        char *exename;
        char *p;
        int tries;
        
        /* 
         *   parse the string to find the server and topic names - the
         *   server is the part from the character after the "DDE:" prefix
         *   to the comma, and the topic is the part after the comma 
         */
        server = prog + 4;
        for (p = server ; *p != '\0' && *p != ',' ; ++p) ;
        if (*p == ',')
            *p++ = '\0';
        topic = p;
        
        /* 
         *   look for another comma, which if present introduces the program
         *   to launch if the DDE server isn't already running 
         */
        for ( ; *p != '\0' && *p != ',' ; ++p) ;
        if (*p == ',')
            *p++ = '\0';
        exename = p;

        /* 
         *   try connecting via DDE twice - once before launching the
         *   program, then again if we fail the first time and can launch
         *   the program explicitly 
         */
        for (tries = 0 ; tries < 2 && !success ; ++tries)
        {
            /* initialize a DDE conversation */
            if (dde_open_conv(&dde_info, server, topic) == 0)
            {
                /* send the command string */
                if (dde_exec(&dde_info, expanded_args.get()) == 0)
                    success = TRUE;
            }
            
            /* close the conversation */
            dde_close_conv(&dde_info);
        
            /*
             *   If we failed, and the DDE string contained a third field
             *   with an executable name, launch the executable and try the
             *   conversation again.  
             */
            if (!success && *exename != '\0')
            {
                SHELLEXECUTEINFO exinfo;

                /* try launching the program */
                exinfo.cbSize = sizeof(exinfo);
                exinfo.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS;
                exinfo.hwnd = 0;
                exinfo.lpVerb = "open";
                exinfo.lpFile = exename;
                exinfo.lpParameters = "";
                exinfo.lpDirectory = 0;
                exinfo.nShow = SW_SHOW;
                if (!ShellExecuteEx(&exinfo)
                    || (unsigned long)exinfo.hInstApp <= 32
                    || exinfo.hProcess == 0)
                {
                    /* 
                     *   we couldn't even do that - don't bother retrying
                     *   the DDE command 
                     */
                    break;
                }
                else
                {
                    DWORD wait_stat;

                    /* 
                     *   wait until the new program finishes initializing
                     *   before attempting to contact it again 
                     */
                    wait_stat = WaitForInputIdle(exinfo.hProcess, 5000);

                    /* we don't need the process handle any more */
                    CloseHandle(exinfo.hProcess);

                    /* if the wait failed, don't bother trying DDE again */
                    if (wait_stat != 0)
                        break;
                }
            }
        }

        /* if we didn't succeed, show an error */
        if (!success)
        {
            MessageBox(0, "Error sending DDE command to text editor "
                       "application.  Check the Advanced settings "
                       "in the Editor page in the Options dialog.",
                       "TADS Workbench",
                       MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
        }
    }
    else
    {
        char filedir[OSFNMAX];
        
        /* get the directory containing our file */
        os_get_path_name(filedir, sizeof(filedir), path);
        
        /* invoke the editor */
        if ((unsigned long)ShellExecute(0, "open", prog,
                                        expanded_args.get(), filedir,
                                        SW_SHOW) <= 32)
        {
            /* show the error */
            MessageBox(0, "Error executing text editor application.  "
                       "Check the Editor settings in the Options "
                       "dialog.", "TADS Workbench",
                       MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
        }
        else
        {
            /* note the success */
            success = TRUE;
        }
    }

    /*
     *   If we were successful, find the window for this file, and notify
     *   it that the file is opened for editing now 
     */
    if (success)
    {
        CHtmlSysWin_win32 *win;

        /* find the window */
        win = (CHtmlSysWin_win32 *)helper_->find_text_file(this, fname, path);

        /* if we found it, notify it that the file is open */
        if (win != 0)
            SendMessage(win->get_handle(), WM_COMMAND,
                        ID_FILE_EDITING_TEXT, 0);
    }

    /* return success indication */
    return success;
}


/* ------------------------------------------------------------------------ */
/*
 *   CHtmlDebugSysIfc_win implementation 
 */

/*
 *   create a source window 
 */
CHtmlSysWin *CHtmlSys_dbgmain::dbgsys_create_win(
    CHtmlParser *parser,  CHtmlFormatter *formatter,
    const textchar_t *win_title, const textchar_t *full_path,
    HtmlDbg_wintype_t win_type)
{
    CHtmlSys_dbgwin *win;
    RECT pos;
    CHtmlRect hrc;
    int vis;
    int is_docking;
    tads_dock_align dock_align;
    int dock_width;
    int dock_height;
    const char *dockwin_guid;

    /* presume this won't be a docking window */
    is_docking = FALSE;

    /* create the new frame window */
    switch(win_type)
    {
    case HTMLDBG_WINTYPE_SRCFILE:
        /* if we don't have a full path, use the window title as the path */
        if (full_path == 0 || full_path[0] == '\0')
            full_path = win_title;

        /* create the window */
        win = new CHtmlSys_dbgsrcwin(parser, prefs_, helper_, this,
                                     full_path);

        /* load the window's configuration, if saved */
        if (!local_config_->getval("src win pos", win_title, &hrc))
            SetRect(&pos, hrc.left, hrc.top,
                    hrc.right - hrc.left, hrc.bottom - hrc.top);
        else
            SetRect(&pos, CW_USEDEFAULT, CW_USEDEFAULT,
                    CW_USEDEFAULT, CW_USEDEFAULT);

        /* start off showing the window */
        vis = TRUE;
        break;

    case HTMLDBG_WINTYPE_STACK:
        /* create the window */
        win = new CHtmlSys_dbgstkwin(parser, prefs_, helper_, this);

        /* get the position setting */
        if (!local_config_->getval("stack win pos", 0, &hrc))
            SetRect(&pos, hrc.left, hrc.top,
                    hrc.right - hrc.left, hrc.bottom - hrc.top);
        else
            w32tdb_set_default_stk_rc(&pos);

        /* note that we need to create a docking window */
        is_docking = TRUE;
        dock_align = TADS_DOCK_ALIGN_RIGHT;
        dock_width = 170;
        dock_height = 170;
        dockwin_guid = "stack win";
        break;

    case HTMLDBG_WINTYPE_HISTORY:
        /* create the window */
        win = new CHtmlSys_dbghistwin(parser, prefs_, helper_, this);
        
        /* get the position setting */
        if (!local_config_->getval("hist win pos", 0, &hrc))
            SetRect(&pos, hrc.left, hrc.top,
                    hrc.right - hrc.left, hrc.bottom - hrc.top);
        else
            SetRect(&pos, 50, 50, 350, 500);

        /* set up to create it as a docking window */
        is_docking = TRUE;
        dock_align = TADS_DOCK_ALIGN_RIGHT;
        dock_width = 170;
        dock_height = 170;
        dockwin_guid = "hist win";

        /* note that we need to create a docking window */
        is_docking = TRUE;
        break;

    case HTMLDBG_WINTYPE_DEBUGLOG:
        /* create the window */
        win = new CHtmlSys_dbglogwin(parser, prefs_, this);

        /* get the position setting */
        if (!local_config_->getval("log win pos", 0, &hrc))
            SetRect(&pos, hrc.left, hrc.top,
                    hrc.right - hrc.left, hrc.bottom - hrc.top);
        else
            SetRect(&pos, CW_USEDEFAULT, CW_USEDEFAULT,
                    CW_USEDEFAULT, CW_USEDEFAULT);

        /* set up to create it as a docking window */
        is_docking = TRUE;
        dock_align = TADS_DOCK_ALIGN_RIGHT;
        dock_width = 170;
        dock_height = 170;
        dockwin_guid = "log win";
        break;
    }

    /* initialize the HTML panel in the window */
    win->init_html_panel(formatter);

    /* this will show only preformatted text, so don't reformat on resize */
    win->get_html_panel()->set_format_on_resize(FALSE);

    /* give the panel a sizebox */
    win->get_html_panel()->set_has_sizebox(TRUE);

    /* open the system window */
    if (is_docking)
    {
        /* this is a docking window - create it as a docking window */
        load_dockwin_config_win(local_config_, dockwin_guid,
                                win, win_title,
                                TRUE, dock_align, dock_width, dock_height,
                                &pos, TRUE);
    }
    else
    {
        /* create an MDI child window for the new window */
        win->create_system_window(this, vis, win_title, &pos,
                                  w32tdb_new_child_syswin(win));
    }

    /* finish up, depending on the window type */
    switch(win_type)
    {
    case HTMLDBG_WINTYPE_SRCFILE:
    case HTMLDBG_WINTYPE_STACK:
    case HTMLDBG_WINTYPE_HISTORY:
        /* 
         *   it's a source window (or a subclass) - synthesize an options
         *   change notification, so that we load the current options 
         */
        note_formatting_changes(win->get_html_panel());
        break;

    case HTMLDBG_WINTYPE_DEBUGLOG:
        /* note the formatting changes */
        note_formatting_changes(win->get_html_panel());

        /* 
         *   Tell the main game window about the log window, so that it can
         *   send debug messages to the window.  Note that we do not want to
         *   add a menu for the debug log window to the game window, since
         *   the window is conceptually part of the debugger rather than of
         *   the run-time.  
         */
        if (CHtmlSys_mainwin::get_main_win() != 0)
            CHtmlSys_mainwin::get_main_win()->
                set_debug_win((CHtmlSys_dbglogwin *)win, FALSE);
        break;
    }

    /* return the system window object for the HTML panel in the window */
    return win->get_html_panel();
}

/*
 *   adjust a font for use in source windows 
 */
void CHtmlSys_dbgmain::dbgsys_set_srcfont(CHtmlFontDesc *font_desc)
{
    textchar_t buf[HTMLFONTDESC_FACE_MAX];
    int fontsize;
    
    /* 
     *   use a smaller font size; the base size is too big for monospaced
     *   fonts 
     */
    font_desc->htmlsize = 2;

    /* if there's a configuration setting, use its font */
    if (!global_config_->getval("src win options", "font name",
                                0, buf, sizeof(buf)))
        strcpy(font_desc->face, buf);
    else
        font_desc->face[0] = '\0';

    /* if there's a specific size setting, use it */
    if (!global_config_->getval("src win options", "font size", &fontsize))
    {
        /* use the given point size, rather than an abstract HTML size */
        font_desc->htmlsize = 0;
        font_desc->pointsize = fontsize;
    }
}

/*
 *   Test a filename against a full filename path to see if the filename
 *   matches the path with its prefix.  For example, if the path is
 *   "c:\foo\bar\dir\file.c" and the filename is "dir\file.c", we would
 *   have a match, because "dir\file.c" is a trailing suffix of the path
 *   after a path separator character.  
 */
static int path_prefix_matches(const char *path, const char *fname)
{
    size_t path_len;
    size_t fname_len;

    /* get the lengths */
    path_len = strlen(path);
    fname_len = strlen(fname);

    /* 
     *   if the path name isn't any longer than the filename, there's
     *   nothing to check 
     */
    if (path_len <= fname_len)
        return FALSE;

    /* 
     *   if the path doesn't have a separator character just before where
     *   the suffix would go, it's not a match 
     */
    if (path[path_len - fname_len - 1] != '\\'
        && path[path_len - fname_len - 1] != '/'
        && path[path_len - fname_len - 1] != ':')
        return FALSE;

    /* check to see if the suffix matches */
    return (strcmp(path + (path_len - fname_len), fname) == 0);
}

/*
 *   compare filenames 
 */
int CHtmlSys_dbgmain::dbgsys_fnames_eq(const char *fname1,
                                       const char *fname2)
{
    /* if one or the other is null, we can't have a match */
    if (fname1 == 0 || fname2 == 0)
        return FALSE;

    /* if the root names match, consider it a match */
    return (stricmp(os_get_root_name((char *)fname1),
                    os_get_root_name((char *)fname2)) == 0);

#if 0
    char buf1[256], buf2[256];
    char *fpart1, *fpart2;

    /* if one or the other is null, we can't have a match */
    if (fname1 == 0 || fname2 == 0)
        return FALSE;

    /* clear the full path buffers */
    buf1[0] = '\0';
    buf2[0] = '\0';

    /* 
     *   Get the full names and paths of the files.  This will expand any
     *   short (8.3) names into full names, and will resolve relative
     *   paths to provide the fully-qualified name with drive and absolute
     *   path.  
     */
    GetFullPathName(fname1, sizeof(buf1), buf1, &fpart1);
    GetFullPathName(fname2, sizeof(buf2), buf2, &fpart2);

    /* see if they match, ignoring case - if so, indicate a match */
    if (stricmp(buf1, buf2) == 0)
        return TRUE;

    /*
     *   Test to see if one of the given filenames matches the other as a
     *   relative path.  If so, consider it a match as well.
     */
    if (path_prefix_matches(buf1, fname2)
        || path_prefix_matches(buf2, fname1))
        return TRUE;

    /* they don't appear to match */
    return FALSE;
#endif //0
}

/*
 *   compare a filename to a path 
 */
int CHtmlSys_dbgmain::dbgsys_fname_eq_path(const char *path,
                                           const char *fname)
{
    char buf[256];
    char *fpart;
    size_t pathlen, fnamelen;
    int tries;

    /* if one or the other is null, we can't have a match */
    if (path == 0 || fname == 0)
        return FALSE;

    /* find the root name of the filename */
    fname = os_get_root_name((char *)fname);

    /* try it with the given path, then with the full path */
    for (tries = 0 ; ; ++tries)
    {
        /*
         *   Check to see if the filename is a suffix of the path.  If so,
         *   and the character immediately preceding the suffix of the
         *   path is a path separator, then we have a match.  
         */
        pathlen = strlen(path);
        fnamelen = strlen(fname);
        if (fnamelen < pathlen
            && memicmp(path + pathlen - fnamelen, fname, fnamelen) == 0
            && strchr("/\\:", path[pathlen - fnamelen - 1]) != 0)
        {
            /* it's a match */
            return TRUE;
        }

        /* if we've already tried with the full name, we've failed */
        if (tries > 0)
            return FALSE;

        /*
         *   Get the full name of the path and try again.  It's possible
         *   that the path given is relative, or contains 8.3 short names,
         *   so it's worth trying again with the full path name.  
         */
        GetFullPathName(path, sizeof(buf), buf, &fpart);

        /* try again with the expanded form of the path */
        path = buf;
    }
}

/*
 *   Find a source file 
 */
int CHtmlSys_dbgmain::dbgsys_find_src(const char *origname, int origlen,
                                      char *fullname, size_t full_len,
                                      int must_find_file)
{
    /* use the same implementation as html_dbgui_find_src */
    return html_dbgui_find_src(origname, origlen, fullname, full_len,
                               must_find_file);
}


/*
 *   Receive notification that a debug log window is being destroyed 
 */
void CHtmlSys_dbgmain::dbghostifc_on_destroy(CHtmlSys_dbglogwin *win,
                                             CHtmlSysWin_win32 *subwin)
{
    /* 
     *   tell the helper about it - notify the helper using the HTML
     *   panel, since this is the window we told the helper about in the
     *   first place 
     */
    helper_->on_close_srcwin(subwin);
}



/* ------------------------------------------------------------------------ */
/*
 *   Copy a source file, replacing $xxx$ fields with values for the new game.
 */
int CHtmlSys_dbgmain::copy_insert_fields(const char *srcfile,
                                         const char *dstfile,
                                         const CHtmlBiblio *biblio)
{
    FILE *srcfp = 0;
    FILE *dstfp = 0;
    int ret = FALSE;
    char buf[1024];
    size_t len;

    /* open the two files */
    if ((srcfp = fopen(srcfile, "r")) == 0
        || (dstfp = fopen(dstfile, "w")) == 0)
        goto done;

    /* copy the files one line at a time */
    for (;;)
    {
        char *p;
        char *start;

        /* read the next line */
        if (fgets(buf, sizeof(buf), srcfp) == 0)
            break;

        /* search for $xxx$ fields to replace */
        for (p = start = buf ; *p != '\0' ; ++p)
        {
            /* check for a field */
            if (*p == '$')
            {
                const char *key = 0;
                const char *val = 0;
                const CHtmlBiblioPair *pair;
                char ifid[64];

                /* check for IFID, and the various bibliographic keys */
                if (memcmp(p, "$IFID$", 6) == 0)
                {
                    /* it's the IFID */
                    key = "$IFID$";

                    /* generate the IFID value */
                    tads_generate_ifid(ifid, sizeof(ifid), dstfile);
                    val = ifid;
                }
                else if (biblio != 0 && (pair = biblio->find_key(p)) != 0)
                {
                    /* use this key/value pair */
                    key = pair->key_.get();
                    val = pair->val_.get();
                }

                /* if we found something, make the replacement */
                if (key != 0)
                {
                    const char *q1, *q2;
                    
                    /* write the part up to here */
                    if ((len = (p - start)) != 0
                        && fwrite(start, 1, len, dstfp) != len)
                        goto done;

                    /* write the replacement text, escaping single quotes */
                    for (q1 = q2 = val ; ; ++q2)
                    {
                        /* if we're at a quote or backslash, escape it */
                        if (*q2 == '\'' || *q2 == '\\' || *q2 == '\0')
                        {
                            /* write the part up to the special character */
                            if ((len = q2 - q1) != 0
                                && fwrite(q1, 1, len, dstfp) != len)
                                goto done;

                            /* if at end of string, stop */
                            if (*q2 == '\0')
                                break;

                            /* write the escaping backslash */
                            if (fputs("\\", dstfp) == EOF)
                                goto done;

                            /* continue from the quote */
                            q1 = q2;
                        }
                    }

                    /* move to the last character of the $xxx$ string */
                    p += strlen(key) - 1;

                    /* the next segment starts just after this point */
                    start = p + 1;
                }
            }
        }

        /* write out the last part of the line, if any */
        if ((len = p - start) != 0
            && fwrite(start, 1, len, dstfp) != len)
            goto done;
    }

    /* successfully copied */
    ret = TRUE;

done:
    /* close the files, if we opened them */
    if (srcfp != 0)
        fclose(srcfp);
    if (dstfp != 0 && fclose(dstfp) == EOF)
        ret = FALSE;

    /* return the result */
    return ret;
}

/* ------------------------------------------------------------------------ */
/*
 *   CHtmlSys_srcwin implementation 
 */

CHtmlSys_dbgsrcwin::CHtmlSys_dbgsrcwin(CHtmlParser *parser,
                                       CHtmlPreferences *prefs,
                                       CHtmlDebugHelper *helper,
                                       CHtmlSys_dbgmain *dbgmain,
                                       const textchar_t *path)
    : CHtmlSys_dbgwin(parser, prefs)
{
    /* remember my helper object, and add a reference to it */
    helper_ = helper;
    helper_->add_ref();

    /* remember the main debugger window */
    dbgmain_ = dbgmain;
    dbgmain_->AddRef();

    /* remember the path to the file */
    path_.set(path);
}

CHtmlSys_dbgsrcwin::~CHtmlSys_dbgsrcwin()
{
    /* release our reference on the main debugger window */
    dbgmain_->Release();
}

/*
 *   initialize 
 */
void CHtmlSys_dbgsrcwin::init_html_panel(CHtmlFormatter *formatter)
{
    CHtmlRect margins;

    /* create the main HTML panel as a debugger source HTML panel */
    create_html_panel(formatter);

    /* don't inset from the left margin at all */
    margins = formatter->get_phys_margins();
    margins.left = 0;
    formatter->set_phys_margins(&margins);
}

/*
 *   create the HTML panel 
 */
void CHtmlSys_dbgsrcwin::create_html_panel(CHtmlFormatter *formatter)
{
    /* create the debugger source window panel */
    html_panel_ = new CHtmlSysWin_win32_dbgsrc(formatter, this, TRUE, TRUE,
                                               prefs_, helper_, dbgmain_,
                                               path_.get());
}

/*
 *   receive notification that the window is being destroyed 
 */
void CHtmlSys_dbgsrcwin::do_destroy()
{
    CHtmlDebugHelper *helper;
    CHtmlSysWin *win;

    /* 
     *   tell the main window that I'm gone, in case it's keeping track of
     *   me as a background window for routing commands 
     */
    dbgmain_->on_close_dbg_win(this);

    /* 
     *   note my helper and HTML panel objects for later -- since 'this'
     *   will be destroyed when we inherit our superclass do_destroy(), we
     *   won't be able to access my members after the inherited
     *   do_destroy() returns 
     */
    helper = helper_;
    win = get_html_panel();

    /* 
     *   inherit default handling - this will delete 'this', so we can't
     *   access any of my member variables after this call 
     */
    CHtmlSys_dbgwin::do_destroy();

    /* 
     *   Now that the window has been deleted and thus disentangled from
     *   the parser and formatter, tell the helper to delete its objects
     *   associated with the window. 
     */
    helper->on_close_srcwin(win);

    /* release our reference on the helper, now that we're going away */
    helper->remove_ref();
}

/*
 *   handle a user message 
 */
int CHtmlSys_dbgsrcwin::do_user_message(int msg, WPARAM wpar, LPARAM lpar)
{
    /* see what we have */
    switch (msg)
    {
    case HTMLM_SAVE_WIN_POS:
        /* 
         *   It's our special deferred save-window-position message.
         *   Whatever event triggered the size/move has finished, so it
         *   should now be safe to save our new preference setting.  
         */
        set_winpos_prefs_deferred();

        /* handled */
        return TRUE;
    }

    /* if we didn't handle it, inherit the default handling */
    return CHtmlSys_dbgwin::do_user_message(msg, wpar, lpar);
}

/* 
 *   get the text for a 'find' command 
 */
const textchar_t *CHtmlSys_dbgsrcwin::
   get_find_text(int command_id, int *exact_case,
                 int *start_at_top, int *wrap, int *dir)
{
    /* get the text from the main debugger window */
    return dbgmain_->get_find_text(command_id, exact_case,
                                   start_at_top, wrap, dir);
}

/*
 *   Adjust screen coordinates to frame-relative coordinates, if this
 *   window is contained inside another window.  For MDI mode, this will
 *   adjust the given screen coordinates so they're relative to the MDI
 *   client window that contains this window; for SDI mode, this won't
 *   change the coordinates, since the window is not contained inside any
 *   other window. 
 */
void CHtmlSys_dbgsrcwin::adjust_coords_to_frame(CHtmlRect *newpos,
                                                const CHtmlRect *pos)
{
    RECT rc;

    /* set up the RECT structure with the screen coordinates */
    SetRect(&rc, pos->left, pos->top, pos->right, pos->bottom);

    /* adjust the coordinates */
    w32tdb_adjust_client_pos(dbgmain_, &rc);

    /* store the result */
    newpos->set(rc.left, rc.top, rc.right, rc.bottom);
}

/*
 *   Store my position in the preferences.  This is invoked from our
 *   self-posted follow-up message, which we post during a size/move event
 *   for deferred processing after the triggering event has finished.  
 */
void CHtmlSys_dbgsrcwin::set_winpos_prefs_deferred()
{
    RECT rc;
    CHtmlRect pos;

    /* 
     *   If we're the current maximized MDI child, do not save the settings.
     *   The maximized child's size is entirely dependent on the MDI frame
     *   size, so it obviously doesn't have any business being stored; what's
     *   more, we do want to store the 'restored' size, so if we saved the
     *   maximized size, we'd overwrite that information, which we don't want
     *   to do.  
     */
    if (is_win_maximized())
        return;

    /* get my window position */
    GetWindowRect(handle_, &rc);

    /* adjust to frame-relative coordinates */
    w32tdb_adjust_client_pos(dbgmain_, &rc);

    /* convert to our internal rectangle structure */
    pos.set(rc.left, rc.top, rc.right, rc.bottom);

    /* store the position */
    store_winpos_prefs(&pos);
}

/*
 *   store my position in the preferences 
 */
void CHtmlSys_dbgsrcwin::store_winpos_prefs(const CHtmlRect *pos)
{
    char win_title[256];

    /* get my window title */
    GetWindowText(handle_, win_title, sizeof(win_title));

    /* store the position in the debugger configuration */
    if (dbgmain_->get_local_config() != 0)
        dbgmain_->get_local_config()->setval("src win pos", win_title, pos);
}

/*
 *   bring this owner window to the front 
 */
void CHtmlSys_dbgsrcwin::bring_owner_to_front()
{
    /* inherit default behavior */
    CHtmlSys_dbgwin::bring_owner_to_front();

    /* 
     *   update the caret position, since we may have changed it since we
     *   last set it 
     */
    html_panel_->update_caret_pos(TRUE, FALSE);
}

/* 
 *   process a notification message 
 */
int CHtmlSys_dbgsrcwin::do_notify(int control_id, int notify_code, LPNMHDR nm)
{
    /* check the message */
    switch(notify_code)
    {
    case NM_SETFOCUS:
    case NM_KILLFOCUS:
        /* notify myself of an artificial activation */
        do_parent_activate(notify_code == NM_SETFOCUS);

        /* inherit the default handling as well */
        break;

    default:
        /* no special handling */
        break;
    }

    /* inherit default handling */
    return CHtmlSys_dbgwin::do_notify(control_id, notify_code, nm);
}

/*
 *   Non-client Activate/deactivate.  Notify the debugger main window of
 *   the coming or going of this debugger window as the active debugger
 *   window, which will allow us to receive certain command messages from
 *   the main window.
 *   
 *   We handle this in the non-client activation, since MDI doesn't send
 *   regular activation messages to child windows when the MDI frame
 *   itself is becoming active or inactive.  
 */
int CHtmlSys_dbgsrcwin::do_ncactivate(int flag)
{
    /* set active/inactive with the main window */
    dbgmain_->notify_active_dbg_win(this, flag);

    /* if we're becoming active, update the statusline line/column */
    if (flag)
        update_statusline_linecol();

    /* inherit default handling */
    return CHtmlSys_dbgwin::do_ncactivate(flag);
}

/*
 *   Receive notification of activation from parent 
 */
void CHtmlSys_dbgsrcwin::do_parent_activate(int flag)
{
    /* set active/inactive with the main window */
    dbgmain_->notify_active_dbg_win(this, flag);

    /* if we're becoming active, update the statusline line/column */
    if (flag)
        update_statusline_linecol();

    /* inherit default handling */
    CHtmlSys_dbgwin::do_parent_activate(flag);
}

/*
 *   Update the statusline line/column number information 
 */
void CHtmlSys_dbgsrcwin::update_statusline_linecol()
{
    char buf[128];

    /* if there's no statusline, ignore it */
    if (dbgmain_->get_statusline() == 0)
        return;

    /* format the line/column information */
    format_linecol(buf, sizeof(buf));

    /* display it */
    SendMessage(dbgmain_->get_statusline()->get_handle(), SB_SETTEXT,
                STATPANEL_LINECOL, (LPARAM)buf);
}

/*
 *   Format our cursor line/column position for display in the statusline 
 */
void CHtmlSys_dbgsrcwin::format_linecol(char *buf, size_t)
{
    CHtmlSysWin_win32_dbgsrc *panel;
    long lin;
    long col;

    /* get my main panel, cast to our special subtype */
    panel = (CHtmlSysWin_win32_dbgsrc *)get_html_panel();

    /* format the position as reported by the HTML panel */
    panel->get_caret_linecol(&lin, &col);
    sprintf(buf, " Ln %ld  Col %ld ", lin, col);
}

/*
 *   Check a command status.  Pass the command to the main window for
 *   routine.  
 */
TadsCmdStat_t CHtmlSys_dbgsrcwin::check_command(int command_id)
{
    return dbgmain_->check_command(command_id);
}

/*
 *   Process a command.  Pass the command to the main window for routing.  
 */
int CHtmlSys_dbgsrcwin::do_command(int notify_code, int command_id, HWND ctl)
{
    return dbgmain_->do_command(notify_code, command_id, ctl);
}

/*
 *   Check a command status, for routing from the main window.  Pass the
 *   command to our HTML panel for processing.  
 */
TadsCmdStat_t CHtmlSys_dbgsrcwin::active_check_command(int command_id)
{
    return ((CHtmlSysWin_win32_dbgsrc *)html_panel_)
        ->active_check_command(command_id);
}

/*
 *   Process a command, for routing from the main window.  Pass the
 *   command to our HTML panel.  
 */
int CHtmlSys_dbgsrcwin::active_do_command(int notify_code, int command_id,
                                          HWND ctl)
{
    return ((CHtmlSysWin_win32_dbgsrc *)html_panel_)
        ->active_do_command(notify_code, command_id, ctl);
}

/*
 *   receive notification of a new caret position 
 */
void CHtmlSys_dbgsrcwin::owner_update_caret_pos()
{
    /* update the status line */
    update_statusline_linecol();
}

/* ------------------------------------------------------------------------ */
/*
 *   Go To Line dialog 
 */
class CTadsDlgGoToLine: public CTadsDialog
{
public:
    CTadsDlgGoToLine(long init_linenum)
    {
        /* no line number yet */
        linenum_ = init_linenum;
    }

    void init()
    {
        char buf[20];
        
        /* inherit default initialization */
        CTadsDialog::init();

        /* 
         *   if we have a valid line number, put it in the edit field;
         *   otherwise, disable the OK button, since we don't have a valid
         *   number in the field yet 
         */
        if (linenum_ != 0)
        {
            /* we have a line number - initialize the edit field */
            sprintf(buf, "%ld", linenum_);
            SetWindowText(GetDlgItem(handle_, IDC_EDIT_LINENUM), buf);
        }
        else
        {
            /* no line number in the field yet - disable the OK button */
            EnableWindow(GetDlgItem(handle_, IDOK), FALSE);
        }
    }
    
    int do_command(WPARAM id, HWND ctl)
    {
        char buf[256];
        
        switch(LOWORD(id))
        {
        case IDC_EDIT_LINENUM:
            /* check for edit field notificaitons */
            if (HIWORD(id) == EN_CHANGE)
            {
                /* 
                 *   it's a change notification - enable or disable the OK
                 *   button according to whether or not we have a valid
                 *   number in the field 
                 */
                GetWindowText(GetDlgItem(handle_, IDC_EDIT_LINENUM),
                              buf, sizeof(buf));
                EnableWindow(GetDlgItem(handle_, IDOK), atol(buf) != 0);
            }

            /* proceed with normal processing */
            break;
            
        case IDOK:
            /* read the line number from the control */
            GetWindowText(GetDlgItem(handle_, IDC_EDIT_LINENUM),
                          buf, sizeof(buf));

            /* convert it to an integer */
            linenum_ = atol(buf);

            /* proceed with normal processing */
            break;

        case IDCANCEL:
            /* cancelling - forget the line number */
            linenum_ = 0;

            /* proceed with normal processing */
            break;

        default:
            /* nothing special to us - use normal processing */
            break;
        }

        /* inherit default processing */
        return CTadsDialog::do_command(id, ctl);
    }

    /* 
     *   get the line number - returns zero if no valid line number was
     *   entered or the dialog was cancelled 
     */
    long get_linenum() const { return linenum_; }

private:
    /* the entered line number, or zero if no line number was entered */
    long linenum_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Debugger source HTML panel window implementation
 */

CHtmlSysWin_win32_dbgsrc::CHtmlSysWin_win32_dbgsrc(
    CHtmlFormatter *formatter, CHtmlSysWin_win32_owner *owner,
    int has_vscroll, int has_hscroll, CHtmlPreferences *prefs,
    CHtmlDebugHelper *helper, CHtmlSys_dbgmain *dbgmain,
    const textchar_t *path)
    : CHtmlSysWin_win32(formatter, owner, has_vscroll, has_hscroll, prefs)
{
    /* remember the debugger helper object, and add our reference to it */
    helper_ = helper;
    helper_->add_ref();

    /* remember the debugger main window, and add our reference */
    dbgmain_ = dbgmain;
    dbgmain_->AddRef();

    /* remember my file path */
    path_.set(path);

    /* load the normal size (16x16) debugger icons */
    ico_dbg_curline_ = (HICON)LoadImage(CTadsApp::get_app()->get_instance(),
                                        MAKEINTRESOURCE(IDI_DBG_CURLINE),
                                        IMAGE_ICON, 16, 16, 0);
    ico_dbg_ctxline_ = (HICON)LoadImage(CTadsApp::get_app()->get_instance(),
                                        MAKEINTRESOURCE(IDI_DBG_CTXLINE),
                                        IMAGE_ICON, 16, 16, 0);
    ico_dbg_bp_ = (HICON)LoadImage(CTadsApp::get_app()->get_instance(),
                                   MAKEINTRESOURCE(IDI_DBG_BP),
                                   IMAGE_ICON, 16, 16, 0);
    ico_dbg_bpdis_ = (HICON)LoadImage(CTadsApp::get_app()->get_instance(),
                                      MAKEINTRESOURCE(IDI_DBG_BPDIS),
                                      IMAGE_ICON, 16, 16, 0);
    ico_dbg_curbp_ = (HICON)LoadImage(CTadsApp::get_app()->get_instance(),
                                      MAKEINTRESOURCE(IDI_DBG_CURBP),
                                      IMAGE_ICON, 16, 16, 0);
    ico_dbg_curbpdis_ = (HICON)LoadImage(CTadsApp::get_app()->get_instance(),
                                         MAKEINTRESOURCE(IDI_DBG_CURBPDIS),
                                         IMAGE_ICON, 16, 16, 0);
    ico_dbg_ctxbp_ = (HICON)LoadImage(CTadsApp::get_app()->get_instance(),
                                      MAKEINTRESOURCE(IDI_DBG_CTXBP),
                                      IMAGE_ICON, 16, 16, 0);
    ico_dbg_ctxbpdis_ = (HICON)LoadImage(CTadsApp::get_app()->get_instance(),
                                         MAKEINTRESOURCE(IDI_DBG_CTXBPDIS),
                                         IMAGE_ICON, 16, 16, 0);

    /* load the small (8x8) debugger icons */
    smico_dbg_curline_ = (HICON)LoadImage(
        CTadsApp::get_app()->get_instance(),
        MAKEINTRESOURCE(IDI_DBG_CURLINE), IMAGE_ICON, 8, 8, 0);
    smico_dbg_ctxline_ = (HICON)LoadImage(
        CTadsApp::get_app()->get_instance(),
        MAKEINTRESOURCE(IDI_DBG_CTXLINE), IMAGE_ICON, 8, 8, 0);
    smico_dbg_bp_ = (HICON)LoadImage(
        CTadsApp::get_app()->get_instance(),
        MAKEINTRESOURCE(IDI_DBG_BP), IMAGE_ICON, 8, 8, 0);
    smico_dbg_bpdis_ = (HICON)LoadImage(
        CTadsApp::get_app()->get_instance(),
        MAKEINTRESOURCE(IDI_DBG_BPDIS), IMAGE_ICON, 8, 8, 0);
    smico_dbg_curbp_ = (HICON)LoadImage(
        CTadsApp::get_app()->get_instance(),
        MAKEINTRESOURCE(IDI_DBG_CURBP), IMAGE_ICON, 8, 8, 0);
    smico_dbg_curbpdis_ = (HICON)LoadImage(
        CTadsApp::get_app()->get_instance(),
        MAKEINTRESOURCE(IDI_DBG_CURBPDIS), IMAGE_ICON, 8, 8, 0);
    smico_dbg_ctxbp_ = (HICON)LoadImage(
        CTadsApp::get_app()->get_instance(),
        MAKEINTRESOURCE(IDI_DBG_CTXBP), IMAGE_ICON, 8, 8, 0);
    smico_dbg_ctxbpdis_ = (HICON)LoadImage(
        CTadsApp::get_app()->get_instance(),
        MAKEINTRESOURCE(IDI_DBG_CTXBPDIS), IMAGE_ICON, 8, 8, 0);

    /* enable the caret */
    caret_enabled_ = TRUE;

    /* get rid of the default popup menu and load a new one instead */
    load_context_popup(IDR_DEBUG_POPUP_MENU);

    /* no target column for up/down arrow yet */
    target_col_valid_ = FALSE;

    /* store the file's timestamp as it is now */
    update_file_time();

    /* mark the file as not yet opened for editing */
    opened_for_editing_ = FALSE;

    /* the icon size hasn't been measured yet */
    icon_size_ = W32TDB_ICON_UNKNOWN;
}

CHtmlSysWin_win32_dbgsrc::~CHtmlSysWin_win32_dbgsrc()
{
    /* done with the debugger source window icons */
    DeleteObject(ico_dbg_curline_);
    DeleteObject(ico_dbg_ctxline_);
    DeleteObject(ico_dbg_bp_);
    DeleteObject(ico_dbg_bpdis_);
    DeleteObject(ico_dbg_curbp_);
    DeleteObject(ico_dbg_curbpdis_);
    DeleteObject(ico_dbg_ctxbp_);
    DeleteObject(ico_dbg_ctxbpdis_);
    DeleteObject(smico_dbg_curline_);
    DeleteObject(smico_dbg_ctxline_);
    DeleteObject(smico_dbg_bp_);
    DeleteObject(smico_dbg_bpdis_);
    DeleteObject(smico_dbg_curbp_);
    DeleteObject(smico_dbg_curbpdis_);
    DeleteObject(smico_dbg_ctxbp_);
    DeleteObject(smico_dbg_ctxbpdis_);

    /* release our helper object reference */
    helper_->remove_ref();

    /* release the debugger main window */
    dbgmain_->Release();
}

/*
 *   create the window 
 */
void CHtmlSysWin_win32_dbgsrc::do_create()
{
    /* inherit default handling */
    CHtmlSysWin_win32::do_create();

    /* 
     *   set focus on myself (this seems to be necessary on NT for some
     *   reason - after creating an MDI child window, it doesn't receive
     *   focus by default, as it does on 95/98; this is not necessary on
     *   95/98 but seems harmless enough) 
     */
    SetFocus(handle_);
}

/*
 *   remember the file's current modification timestamp for future
 *   reference 
 */
void CHtmlSysWin_win32_dbgsrc::update_file_time()
{
    /* presume we won't get valid date information for the file */
    memset(&file_time_, 0, sizeof(file_time_));

    /* if the path is provided, remember it and get the original timestamp */
    if (path_.get() != 0)
    {
        WIN32_FIND_DATA ff;
        HANDLE hfind;

        /* 
         *   store the modification time for the file so that we can check
         *   it against the file's modification time later on 
         */
        hfind = FindFirstFile(path_.get(), &ff);
        if (hfind != INVALID_HANDLE_VALUE)
        {
            /* remember the modification time on the file */
            file_time_ = ff.ftLastWriteTime;

            /* we're done with the find handle */
            FindClose(hfind);
        }
    }
}

/*
 *   Check the file's current modification timestamp against the value
 *   when we last loaded the file.  
 */
int CHtmlSysWin_win32_dbgsrc::has_file_changed() const
{
    WIN32_FIND_DATA ff;
    HANDLE hfind;
    int ret;

    /* if there's no file, there's no timestamp to have changed */
    if (path_.get() == 0)
        return FALSE;

    /* 
     *   presume we won't find a change; if we don't find the file at all, we
     *   won't find a change 
     */
    ret = FALSE;

    /* look up the file's current timestamp information */
    hfind = FindFirstFile(path_.get(), &ff);
    if (hfind != INVALID_HANDLE_VALUE)
    {
        /* return true if the timestamps are different */
        ret = (CompareFileTime(&ff.ftLastWriteTime, &file_time_) != 0);

        /* we're done with the find handle */
        FindClose(hfind);
    }

    /* return the result */
    return ret;
}

/*
 *   receive notifications
 */
int CHtmlSysWin_win32_dbgsrc::do_notify(int control_id, int notify_code,
                                        LPNMHDR nmhdr)
{
    switch(notify_code)
    {
    case TTN_NEEDTEXT:
        {
            TOOLTIPTEXT *ttt = (TOOLTIPTEXT *)nmhdr;
            unsigned long sel_start, sel_end;
            unsigned long ofs;
            POINT pt;
            CHtmlPoint hpt;
            CStringBuf buf;
            textchar_t valbuf[512];
            static textchar_t tipbuf[512];
            textchar_t *p;
            int all_spaces;
            int expr_good;

            /* get the mouse position */
            GetCursorPos(&pt);
            ScreenToClient(handle_, &pt);

            /* get the position of the mouse */
            hpt = screen_to_doc(CHtmlPoint(pt.x, pt.y));
            ofs = formatter_->find_textofs_by_pos(hpt);

            /* get the selection range */
            formatter_->get_sel_range(&sel_start, &sel_end);

            /* 
             *   if the mouse is over the selection, use the whole
             *   selection range; otherwise, just use the current word 
             */
            if (ofs < sel_start || ofs >= sel_end)
                formatter_->get_word_limits(&sel_start, &sel_end, ofs);

            /* extract the text from the range */
             formatter_->extract_text(&buf, sel_start, sel_end);

            /* 
             *   Scan through the text, converting control characters to
             *   spaces.  Also look to see if there are any non-space
             *   characters; if not, we won't bother popping anything up
             *   at this position.  
             */
            for (p = buf.get(), all_spaces = TRUE ;
                 p != 0 && *p != '\0' ; ++p)
            {
                /* if it's not printable, convert it to a space */
                if (!isprint(*p))
                    *p = ' ';

                /* if it's not a space, so note */
                if (!isspace(*p))
                    all_spaces = FALSE;
            }

            /* 
             *   presume this won't work - there are more ways to fail
             *   than to succeed 
             */
            expr_good = FALSE;

            /* if it's not all spaces, try evaluating the expression */
            if (!all_spaces)
            {
                /* 
                 *   try evaluating it - use speculative mode, so that we
                 *   don't accidentally change anything just because the
                 *   user was moving the mouse around 
                 */
                if (!helper_->eval_expr(dbgmain_->get_dbg_ctx(),
                                        valbuf, sizeof(valbuf),
                                        buf.get(), 0, 0, 0, 0, TRUE))
                {
                    /*
                     *   Suppress tautologies -- if the expression and the
                     *   result are identical, don't bother showing it.
                     *   If they're not identical, show the result.  
                     */
                    if (get_strcmp(buf.get(), valbuf) != 0)
                    {
                        size_t copy_len;
                        
                        /* 
                         *   the expression is good -- add the result to
                         *   the expression buffer, and show the result as
                         *   the tip text 
                         */
                        buf.append_inc(" = ", 50);
                        buf.append(valbuf);
                        
                        /* use the message */
                        expr_good = TRUE;

                        /* store the result in the tip buffer */
                        copy_len = get_strlen(buf.get());
                        if (copy_len > sizeof(tipbuf) - 1)
                            copy_len = sizeof(tipbuf) - 1;
                        memcpy(tipbuf, buf.get(), copy_len);
                        tipbuf[copy_len] = '\0';
                        ttt->lpszText = tipbuf;
                    }
                }
            }

            /*
             *   If we didn't get a valid expression, don't show anything.
             *   To avoid popping up the tip, deactivate the tip, then
             *   immediately reactivate it.  This will suppress this
             *   message display, but will leave the tooltip active for
             *   another try later.  
             */
            if (!expr_good)
            {
                SendMessage(tooltip_, TTM_ACTIVATE, FALSE, 0);
                SendMessage(tooltip_, TTM_ACTIVATE, TRUE, 0);
            }
            return TRUE;
        }

    case TTN_SHOW:
        {
            TOOLINFO ti;
            POINT pt;

            /* get the mouse position */
            GetCursorPos(&pt);
            ScreenToClient(handle_, &pt);

            /*
             *   While it's showing, replace the bounding box for the tool
             *   with the bounding box for the screen area containing the
             *   mouse.  If the user moves out of the box, we'll want to
             *   act as though they've moved on to a new tip.  
             */
            ti.cbSize = sizeof(ti);
            SetRect(&ti.rect, pt.x < 10 ? 0 : pt.x - 10,
                    pt.y < 10 ? 0 : pt.y - 10, pt.x + 10, pt.y + 10);
            ti.hwnd = handle_;
            ti.uId = 1;
            SendMessage(tooltip_, TTM_NEWTOOLRECT, 0, (LPARAM)&ti);

            /* tell the tooltip control to use the default tooltip position */
            return FALSE;
        }

    default:
        /* inherit default */
        return CHtmlSysWin_win32::do_notify(control_id, notify_code, nmhdr);
    }
}

/*
 *   process a click with the left button 
 */
int CHtmlSysWin_win32_dbgsrc::do_leftbtn_down(int keys, int x, int y,
                                              int clicks)
{
    /*
     *   When the tooltip control sees a mouse click, it turns itself off
     *   until the mouse moves out of the tool.  We don't want this to
     *   happen, because we're taking over the whole window for the tool.
     *   To get the tooltip to keep popping up after a click, deactivate
     *   it and then immediately reactivate it.  
     */
    SendMessage(tooltip_, TTM_ACTIVATE, FALSE, 0);
    SendMessage(tooltip_, TTM_ACTIVATE, TRUE, 0);

    /* 
     *   If the click is in my breakpoint area, toggle a breakpoint at the
     *   clicked-on line.  Only allow this if we'd allow a breakpoint
     *   command in this window (some subclasses, such as the stack
     *   subclass, might not allow this sort of thing) 
     */
    if (x < measure_dbgsrc_icon().x
        && check_command(ID_DEBUG_SETCLEARBREAKPOINT) != TADSCMD_DISABLED)
    {
        unsigned long old_start, old_end;
        CHtmlPoint pos;
        class CHtmlDisp *disp;
        
        /* remember the old selection */
        formatter_->get_sel_range(&old_start, &old_end);

        /* put the caret at the start of the line they clicked on */
        pos = screen_to_doc(CHtmlPoint(x, y));
        disp = formatter_->find_by_pos(pos, FALSE);
        if (disp != 0)
            formatter_->set_sel_range(disp->get_text_ofs(),
                                      disp->get_text_ofs());

        /* toggle the breakpoint at the currently selected line */
        helper_->toggle_breakpoint(dbgmain_->get_dbg_ctx(), this);

        /* restore the original selection */
        formatter_->set_sel_range(old_start, old_end);

        /* handled */
        return TRUE;
    }
    else
    {
        /* process a click normally */
        return common_btn_down(keys, x, y, clicks);
    }
}

/*
 *   process a click with the right button 
 */
int CHtmlSysWin_win32_dbgsrc::do_rightbtn_down(int keys, int x, int y,
                                               int clicks)
{
    /* process a basic click */
    return common_btn_down(keys, x, y, clicks);
}

/*
 *   Process a left or right mouse click 
 */
int CHtmlSysWin_win32_dbgsrc::common_btn_down(int keys, int x, int y,
                                              int clicks)
{
    /* 
     *   it's a click in the source area - forget any target column we
     *   had, since this click sets an explicit active column 
     */
    target_col_valid_ = FALSE;
    
    /* inherit default left-button code */
    return CHtmlSysWin_win32::do_leftbtn_down(keys, x, y, clicks);
}

/*
 *   Handle system (ALT) keystrokes.  Send the keystroke to the main
 *   debugger window, so that the user can access the main debugger
 *   window's menu bar from the keyboard while a source window has focus. 
 */
int CHtmlSysWin_win32_dbgsrc::do_syskeydown(int vkey, long keydata)
{
    dbgmain_->run_menu_kb_ifc(vkey, keydata, GetParent(handle_));
    return TRUE;
}


/*
 *   Handle keystrokes 
 */
int CHtmlSysWin_win32_dbgsrc::do_keydown(int vkey, long keydata)
{
    /* check the key */
    switch(vkey)
    {
    case VK_UP:
    case VK_DOWN:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_END:
    case VK_HOME:
    case VK_NEXT:
    case VK_PRIOR:
        /* these keystrokes all move the caret */
        {
            unsigned long sel_start, sel_end;
            unsigned long caret_end, other_end;
            
            /* get current selection range */
            formatter_->get_sel_range(&sel_start, &sel_end);
            
            /* 
             *   if the shift key is down, note the caret end of the range
             *   and the other end of the range; otherwise, treat them as
             *   equivalent 
             */
            if (get_shift_key())
            {
                /* shift key is down - note both ends of the range */
                if (caret_at_right_)
                {
                    caret_end = sel_end;
                    other_end = sel_start;
                }
                else
                {
                    caret_end = sel_start;
                    other_end = sel_end;
                }
            }
            else
            {
                /* no shift key - we only care about the caret position */
                caret_end = other_end =
                    (caret_at_right_ ? sel_end : sel_start);
            }

            switch(vkey)
            {
            case VK_NEXT:
            case VK_PRIOR:
                {
                    RECT rc;
                    int lines;
                    
                    /* 
                     *   if the control key is down, scroll without moving
                     *   the cursor - simply treat this the same way the
                     *   base class does 
                     */
                    if (get_ctl_key())
                        return CHtmlSysWin_win32::do_keydown(vkey, keydata);

                    /* get the window's height */
                    GetClientRect(handle_, &rc);

                    /* 
                     *   figure out how many lines that will hold
                     *   (assuming that all of the lines are of constant
                     *   height, which is the case for our type of window) 
                     */
                    if (formatter_->get_first_disp() != 0)
                    {
                        CHtmlRect lrc;

                        /* get the height of a line */
                        lrc = formatter_->get_first_disp()->get_pos();

                        /* figure out how many fit in the window */
                        lines = (rc.bottom - rc.top) / (lrc.bottom - lrc.top);

                        /* 
                         *   for context, move one line less than the
                         *   window will hold 
                         */
                        if (lines > 1)
                            --lines;
                    }
                    else
                    {
                        /* 
                         *   no lines - use a default window size (it
                         *   doesn't even matter, since if there are no
                         *   lines we can't do any real navigation) 
                         */
                        lines = 25;
                    }
                    
                    /* move the caret up or down by one screenfull */
                    caret_end =
                        do_keydown_arrow(vkey == VK_PRIOR ? VK_UP : VK_DOWN,
                                         caret_end, lines);
                }
                break;
                
            case VK_UP:
            case VK_DOWN:
                caret_end = do_keydown_arrow(vkey, caret_end, 1);
                break;

            case VK_LEFT:
                /* move the caret left a character */
                caret_end = formatter_->dec_text_ofs(caret_end);
                break;

            case VK_RIGHT:
                /* move the caret right a character */
                caret_end = formatter_->inc_text_ofs(caret_end);
                break;

            case VK_END:
                if (get_ctl_key())
                {
                    /* move to the end of the file */
                    caret_end = formatter_->get_text_ofs_max();
                }
                else
                {
                    unsigned long lstart, lend;

                    /* move to the end of the current line */
                    formatter_->get_line_limits(&lstart, &lend, caret_end);
                    caret_end = formatter_->dec_text_ofs(lend);
                }
                break;

            case VK_HOME:
                if (get_ctl_key())
                {
                    /* move to the start of the file */
                    caret_end = 0;
                }
                else
                {
                    unsigned long lstart, lend;

                    /* move to the start of the current line */
                    formatter_->get_line_limits(&lstart, &lend, caret_end);
                    caret_end = lstart;
                }
                break;
            }

            /* 
             *   if the keystroke wasn't an up/down arrow, forget any
             *   target column we had 
             */
            if (vkey != VK_UP && vkey != VK_DOWN
                && vkey != VK_NEXT && vkey != VK_PRIOR)
                target_col_valid_ = FALSE;

            /*
             *   If we reached the very end, because of the way the final
             *   newlines are laid out, we may find ourselves past the end
             *   of the last item.  If this is the case, decrement our
             *   position so that we're at the start of the last item. 
             */
            if (caret_end == formatter_->get_text_ofs_max()
                && formatter_->find_by_txtofs(caret_end, FALSE, FALSE) == 0)
            {
                CHtmlDisp *disp;

                /* get the previous item */
                disp = formatter_->find_by_txtofs(caret_end, TRUE, FALSE);

                /* move to the start of that item */
                if (disp != 0)
                    caret_end = disp->get_text_ofs();
            }

            /* figure out the new selection range */
            if (get_shift_key())
            {
                /* extended range - figure out what direction to go */
                if (caret_end < other_end)
                {
                    /* the caret is at the low (left) end of the range */
                    sel_start = caret_end;
                    sel_end = other_end;
                    caret_at_right_ = FALSE;
                }
                else
                {
                    /* the caret is at the high (right) end */
                    sel_start = other_end;
                    sel_end = caret_end;
                    caret_at_right_ = TRUE;
                }
            }
            else
            {
                /* no shift key - set zero-length range at caret */
                sel_start = sel_end = caret_end;
            }

            /* set the new selection range */
            formatter_->set_sel_range(sel_start, sel_end);
            
            /* make sure the selection is on the screen */
            update_caret_pos(TRUE, FALSE);
        }

        /* handled */
        return TRUE;
        
    default:
        /* inherit default for other keys */
        return CHtmlSysWin_win32::do_keydown(vkey, keydata);
    }
}

/*
 *   process an arrow key; returns new text offset after movement
 */
unsigned long CHtmlSysWin_win32_dbgsrc::
   do_keydown_arrow(int vkey, unsigned long caret_pos, int cnt)
{
    /* keep going for the requested number of lines */
    for ( ; cnt > 0 ; --cnt)
    {
        unsigned long lstart, lend;
        unsigned long ofs;
        CHtmlDisp *nxt;

        /* 
         *   find our current column by examining the limits of the
         *   current line 
         */
        formatter_->get_line_limits(&lstart, &lend, caret_pos);
        
        /* 
         *   if we don't have an existing valid target column, use the
         *   current column as the new target 
         */
        if (!target_col_valid_)
        {
            target_col_ = caret_pos - lstart;
            target_col_valid_ = TRUE;
        }
        
        /* get the next/previous line's display item */
        if (vkey == VK_UP)
        {
            ofs = formatter_->dec_text_ofs(lstart);
            nxt = formatter_->find_by_txtofs(ofs, TRUE, FALSE);
        }
        else
        {
            ofs = lend;
            nxt = formatter_->find_by_txtofs(ofs, FALSE, TRUE);
        }

        /* make sure we got a target item */
        if (nxt != 0)
        {
            CHtmlDispTextDebugsrc *disp;
            unsigned int cur_target;
            
            /* cast it to my correct type */
            disp = (CHtmlDispTextDebugsrc *)nxt;
            
            /* see if it contains the target column */
            if (disp->get_text_len() > target_col_)
            {
                /* it's long enough - go to the taret */
                cur_target = target_col_;
            }
            else
            {
                /* it's too short - go to its end */
                cur_target = disp->get_text_len();
            }
            
            /* move to the correct spot */
            caret_pos = disp->get_text_ofs() + cur_target;
        }
    }

    /* return the new caret position */
    return caret_pos;
}

/*
 *   set the colors 
 */
void CHtmlSysWin_win32_dbgsrc::note_debug_format_changes(
    HTML_color_t bkg_color, HTML_color_t text_color, int use_windows_colors)
{
    /* inherit default handling to update the colors */
    CHtmlSysWin_win32::note_debug_format_changes(
        bkg_color, text_color, use_windows_colors);

    /* 
     *   forget the icon size, so we remeasure the next time we need it (wait
     *   until we actually need it, since that way we'll be sure the new font
     *   has been selected) 
     */
    icon_size_ = W32TDB_ICON_UNKNOWN;
}

/*
 *   Measure the debugger source icon 
 */
CHtmlPoint CHtmlSysWin_win32_dbgsrc::measure_dbgsrc_icon()
{
    CHtmlPoint retval;

    /* if we haven't decided what size icon to use, figure it out */
    if (icon_size_ == W32TDB_ICON_UNKNOWN)
        calc_icon_size();

    switch(icon_size_)
    {
    case W32TDB_ICON_SMALL:
        /* the small icons are 8x8 */
        retval.x = 10;
        retval.y = 8;
        return retval;

    case W32TDB_ICON_NORMAL:
    default:
        /* 
         *   the normal icons are 16x16, but put in a couple of extra pixels
         *   horizontally to space things out 
         */
        retval.x = 20;
        retval.y = 16;
        return retval;
    }
}

/*
 *   Calculate the icon size, if we haven't already 
 */
void CHtmlSysWin_win32_dbgsrc::calc_icon_size()
{
    HDC dc;
    TEXTMETRIC tm;

    /* if we already know the icon size, save the trouble */
    if (icon_size_ != W32TDB_ICON_UNKNOWN)
        return;

    /* get my device context */
    dc = GetDC(handle_);

    /* 
     *   get my current text metrics - use small or normal icons, depending
     *   on the text size 
     */
    GetTextMetrics(dc, &tm);

    /* done with our device context */
    ReleaseDC(handle_, dc);

    /* use small icons or large icons, depending on line spacing */
    icon_size_ = (tm.tmHeight + tm.tmExternalLeading < 14
                  ? W32TDB_ICON_SMALL
                  : W32TDB_ICON_NORMAL);

    /* recalculate the caret height while we're at it */
    set_caret_size(formatter_->get_font());
}

/*
 *   Draw a debugger source line icon 
 */
void CHtmlSysWin_win32_dbgsrc::draw_dbgsrc_icon(const CHtmlRect *pos,
                                                unsigned int stat)
{
    CHtmlRect scrpos;
    RECT rc;
    HICON ico;
    int siz;

    /* if we haven't decided what size icon to use, figure it out */
    if (icon_size_ == W32TDB_ICON_UNKNOWN)
        calc_icon_size();

    /* set up a windows RECT structure for the screen coordinates */
    scrpos = doc_to_screen(*pos);
    scrpos.right -= 2;
    SetRect(&rc, scrpos.left, scrpos.top, scrpos.right, scrpos.bottom);

    /* draw the margin background */
    draw_margin_bkg(&rc);

    /* if there's no breakpoint, lose any related status */
    if ((stat & HTMLTDB_STAT_BP) == 0)
        stat &= ~(HTMLTDB_STAT_BP_DIS | HTMLTDB_STAT_BP_COND);

    /* select the correct icon based on the status */
    switch(stat & (HTMLTDB_STAT_CURLINE | HTMLTDB_STAT_CTXLINE
                   | HTMLTDB_STAT_BP | HTMLTDB_STAT_BP_DIS))
    {
    case HTMLTDB_STAT_CURLINE:
    case HTMLTDB_STAT_CURLINE | HTMLTDB_STAT_CTXLINE:
        ico = (icon_size_ == W32TDB_ICON_NORMAL
               ? ico_dbg_curline_ : smico_dbg_curline_);
        break;

    case HTMLTDB_STAT_CTXLINE:
        ico = (icon_size_ == W32TDB_ICON_NORMAL
               ? ico_dbg_ctxline_ : smico_dbg_ctxline_);
        break;

    case HTMLTDB_STAT_BP:
        ico = (icon_size_ == W32TDB_ICON_NORMAL
               ? ico_dbg_bp_ : smico_dbg_bp_);
        break;

    case HTMLTDB_STAT_BP | HTMLTDB_STAT_BP_DIS:
        ico = (icon_size_ == W32TDB_ICON_NORMAL
               ? ico_dbg_bpdis_ : smico_dbg_bpdis_);
        break;

    case HTMLTDB_STAT_CURLINE | HTMLTDB_STAT_BP:
        ico = (icon_size_ == W32TDB_ICON_NORMAL
               ? ico_dbg_curbp_ : smico_dbg_curbp_);
        break;

    case HTMLTDB_STAT_CURLINE | HTMLTDB_STAT_BP | HTMLTDB_STAT_BP_DIS:
        ico = (icon_size_ == W32TDB_ICON_NORMAL
               ? ico_dbg_curbpdis_ : smico_dbg_curbpdis_);
        break;

    case HTMLTDB_STAT_CTXLINE | HTMLTDB_STAT_BP:
        ico = (icon_size_ == W32TDB_ICON_NORMAL
               ? ico_dbg_ctxbp_ : smico_dbg_ctxbp_);
        break;

    case HTMLTDB_STAT_CTXLINE | HTMLTDB_STAT_BP | HTMLTDB_STAT_BP_DIS:
        ico = (icon_size_ == W32TDB_ICON_NORMAL
               ? ico_dbg_ctxbpdis_ : smico_dbg_ctxbpdis_);
        break;

    default:
        ico = 0;
        break;
    }

    /* note the size */
    switch (icon_size_)
    {
    case W32TDB_ICON_NORMAL:
        /* offset the area slightly to leave blank space in the margin */
        rc.left += 2;

        /* normal icons are 16x16 pixels */
        siz = 16;
        break;

    case W32TDB_ICON_SMALL:
        /* small icons are 8x8 */
        siz = 8;
        break;
    }

    /* vertically center */
    rc.top += (rc.bottom - rc.top - siz) / 2;

    /* draw the icon in the given area */
    if (ico != 0)
        DrawIconEx(hdc_, rc.left, rc.top, ico, siz, siz, 0, 0, DI_NORMAL);
}

/*
 *   draw the margin background 
 */
void CHtmlSysWin_win32_dbgsrc::draw_margin_bkg(const RECT *rc)
{
    /* fill in the area with a light gray background */
    FillRect(hdc_, rc, GetSysColorBrush(COLOR_3DFACE));
}

/*
 *   Set the background cursor.  We use the i-beam cursor when the mouse
 *   isn't over a display item.  
 */
int CHtmlSysWin_win32_dbgsrc::do_setcursor_bkg()
{
    /* use the I-beam cursor */
    SetCursor(ibeam_csr_);

    /* indicate that we set a cursor */
    return TRUE;
}

/*
 *   Get my caret line/column position 
 */
void CHtmlSysWin_win32_dbgsrc::get_caret_linecol(long *lin, long *col)
{
    unsigned long start, end;
    unsigned long pos;
    CHtmlDisp *disp;

    /* presume we don't have a valid position */
    *lin = 0;
    *col = 0;

    /* get the current selection range */
    formatter_->get_sel_range(&start, &end);

    /* get the end of the range where the caret is */
    pos = (caret_at_right_ ? end : start);

    /* get the display item at our current position */
    disp = formatter_->find_by_txtofs(pos, TRUE, FALSE);
    if (disp != 0)
    {
        /* get the line number */
        *lin = ((CHtmlDispTextDebugsrc *)disp)->get_debugsrcpos();

        /* 
         *   get the column number - this is the text offset of the caret
         *   minus the text offset of the start of the line (plus one, since
         *   we number the columns starting at 1) 
         */
        *col = pos - disp->get_text_ofs() + 1;
    }
}

/*
 *   run the "go to line" dialog 
 */
void CHtmlSysWin_win32_dbgsrc::do_goto_line()
{
    CTadsDlgGoToLine *dlg;
    long linenum;
    long colnum;

    /* get my line/column information */
    get_caret_linecol(&linenum, &colnum);

    /* create the dialog */
    dlg = new CTadsDlgGoToLine(linenum);

    /* display it */
    dlg->run_modal(DLG_GOTOLINE, handle_,
                   CTadsApp::get_app()->get_instance());

    /* if we got a valid line number, go there */
    if (dlg->get_linenum() != 0)
    {
        CHtmlDisp *disp;

        /* 
         *   find the display item for the line number (adjusting for the
         *   zero base we use for our internal numbering) 
         */
        disp = formatter_->get_disp_by_linenum(dlg->get_linenum() - 1);

        /* show this display item */
        if (disp != 0)
        {
            /* go to the start of the line */
            formatter_->set_sel_range(disp->get_text_ofs(),
                                      disp->get_text_ofs());

            /* update the caret position to reflect the new selection */
            update_caret_pos(TRUE, FALSE);
        }
    }

    /* done with it */
    delete dlg;
}

/*
 *   Check a command - route to the main window for processing 
 */
TadsCmdStat_t CHtmlSysWin_win32_dbgsrc::check_command(int command_id)
{
    /* route it to the main window for processing */
    return dbgmain_->check_command(command_id);
}

/*
 *   Process a command - route it to the main window for processing 
 */
int CHtmlSysWin_win32_dbgsrc::do_command(int notify_code,
                                         int command_id, HWND ctl)
{
    /* check for special commands that we get sent directly */
    switch(command_id)
    {
    case ID_FILE_EDITING_TEXT:
        /* 
         *   This is simply to notify us that the main window has opened
         *   the file for editing - make a note of it.  
         */
        opened_for_editing_ = TRUE;
        return TRUE;

    default:
        /* use default handling for others */
        break;
    }
    
    /* route it to the main window for processing */
    return dbgmain_->do_command(notify_code, command_id, ctl);
}

/*
 *   Determine command status, routed from the main window
 */
TadsCmdStat_t CHtmlSysWin_win32_dbgsrc::active_check_command(int command_id)
{
    switch(command_id)
    {
    case ID_DEBUG_SETCLEARBREAKPOINT:
    case ID_DEBUG_DISABLEBREAKPOINT:
    case ID_FILE_EDIT_TEXT:
        /* these always work in a source window */
        return TADSCMD_ENABLED;
        
    case ID_DEBUG_SETNEXTSTATEMENT:
    case ID_DEBUG_RUNTOCURSOR:
        /* these work only when the game is loaded */
        return !dbgmain_->get_game_loaded()
            ? TADSCMD_DISABLED : TADSCMD_ENABLED;

    case ID_EDIT_FIND:
    case ID_EDIT_FINDNEXT:
    case ID_DEBUG_ADDTOWATCH:
    case ID_EDIT_GOTOLINE:
        /* these are always allowed in a source window */
        return TADSCMD_ENABLED;

    default:
        /* inherit base class behavior for other commands */
        return CHtmlSysWin_win32::check_command(command_id);
    }
}


/*
 *   Handle a command routed from the main window
 */
int CHtmlSysWin_win32_dbgsrc::active_do_command(int notify_code,
                                                int command_id, HWND ctl)
{
    switch (command_id)
    {
    case ID_EDIT_FIND:
    case ID_EDIT_FINDNEXT:
        do_find(command_id);
        return TRUE;

    case ID_EDIT_GOTOLINE:
        do_goto_line();
        return TRUE;
        
    case ID_DEBUG_SETCLEARBREAKPOINT:
        /* set/clear a breakpoint at the current selection */
        helper_->toggle_breakpoint(dbgmain_->get_dbg_ctx(), this);
        return TRUE;

    case ID_DEBUG_DISABLEBREAKPOINT:
        /* enable/disable breakpoint at the current selection */
        helper_->toggle_bp_disable(dbgmain_->get_dbg_ctx(), this);
        return TRUE;

    case ID_DEBUG_RUNTOCURSOR:
        /* try to set a temporary breakpoint here */
        if (helper_->set_temp_bp(dbgmain_->get_dbg_ctx(), this))
        {
            /* got it - tell the main window to resume execution */
            dbgmain_->exec_go();
        }
        else
        {
            /* 
             *   couldn't set a temporary breakpoint - it's not worth a
             *   whole error dialog, but beep to indicate that it failed 
             */
            Beep(1000, 100);
        }
        return TRUE;

    case ID_FILE_EDIT_TEXT:
        {
            long linenum;
            long colnum;

            /* get the current caret position */
            get_caret_linecol(&linenum, &colnum);

            /* open the file in the text editor */
            if (dbgmain_->open_in_text_editor(path_.get(), linenum, colnum,
                                              FALSE))
            {
                /* 
                 *   we were successful, so mark the file as opened for
                 *   editing - this will suppress prompts when we find
                 *   that the file has been modified externally; we'll
                 *   simply reload the file if the user explicitly told us
                 *   they wanted to edit it 
                 */
                opened_for_editing_ = TRUE;
            }
        }

        /* handled */
        return TRUE;

    case ID_DEBUG_SETNEXTSTATEMENT:
        /* 
         *   make sure the game is running, and the new address is within
         *   the current function 
         */
        if (!dbgmain_->get_game_loaded())
        {
            Beep(1000, 100);
            return TRUE;
        }
        else if (!helper_->is_in_same_fn(dbgmain_->get_dbg_ctx(), this))
        {
            MessageBox(0, "This is not a valid location - execution "
                       "cannot be transferred outside of the current "
                       "function or method.", "TADS Workbench",
                       MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
            return TRUE;
        }

        /* change the execution position */
        {
            int need_single_step;

            /* set the next statement here */
            if (helper_->set_next_statement(dbgmain_->get_dbg_ctx(),
                                            dbgmain_->get_exec_ofs_ptr(),
                                            this, &need_single_step))
            {
                /* something went wrong - beep an error */
                Beep(1000, 100);
            }
            else
            {
                /* 
                 *   success - if the engine requires it, execute for a
                 *   single step, so that we enter into the current line 
                 */
                if (need_single_step)
                    dbgmain_->exec_step_into();
            }
        }

        /* handled */
        return TRUE;
        
    default:
        /* inherit base class behavior */
        return CHtmlSysWin_win32::do_command(notify_code, command_id, ctl);
    }
}

/*
 *   show the text-not-found message for a 'find' command 
 */
void CHtmlSysWin_win32_dbgsrc::find_not_found()
{
    /* display a message box telling the user that we failed */
    MessageBox(0, "No more matches found.", "TADS Workbench",
               MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);
}

/* ------------------------------------------------------------------------ */
/*
 *   Debugger stack window implementation.  This is a simple
 *   specialization of the debugger source window which provides a
 *   slightly different drawing style. 
 */

/*
 *   create the HTML panel 
 */
void CHtmlSys_dbgstkwin::create_html_panel(CHtmlFormatter *formatter)
{
    /* create the debugger stack window panel */
    html_panel_ = new CHtmlSysWin_win32_dbgstk(formatter, this, TRUE, TRUE,
                                               prefs_, helper_, dbgmain_);
}

/*
 *   close the owner window 
 */
int CHtmlSys_dbgstkwin::close_owner_window(int force)
{
    /* we're inside a docking or MDI parent, so close the parent */
    if (force)
    {
        /* 
         *   force the window closed with the special DOCK_DESTROY message
         *   (don't destroy it directly, since the window could be an MDI
         *   child 
         */
        SendMessage(GetParent(handle_), DOCKM_DESTROY, 0, 0);
        return TRUE;
    }
    else
    {
        /* ask the parent window to close itself */
        return !SendMessage(GetParent(handle_), WM_CLOSE, 0, 0);
    }
}

/*
 *   store my position in the preferences 
 */
void CHtmlSys_dbgstkwin::store_winpos_prefs(const CHtmlRect *pos)
{
    /* store the position in the debugger configuration */
    if (dbgmain_->get_local_config() != 0)
        dbgmain_->get_local_config()->setval("stack win pos", 0, pos);

    /* store it in the global configuration (for the default) as well */
    if (dbgmain_->get_global_config() != 0)
        dbgmain_->get_global_config()->setval("stack win pos", 0, pos);
}

/*
 *   process a mouse click 
 */
int CHtmlSysWin_win32_dbgstk::do_leftbtn_down(int keys, int x, int y,
                                              int clicks)
{
    /* if we have a double-click, do some special processing */
    if (clicks == 2)
    {
        CHtmlPoint pos;
        CHtmlDisp *disp;

        /* ignore it if we're not in the debugger */
        if (!dbgmain_->get_in_debugger())
        {
            Beep(1000, 100);
            return TRUE;
        }

        /* 
         *   Get the document coordinates.  Ignore the x position of the
         *   click; act as though they're clicking on the left edge of the
         *   text area.  
         */
        pos = screen_to_doc(CHtmlPoint(x, y));
        pos.x = 0;

        /* find the item we're clicking on */
        disp = formatter_->find_by_pos(pos, TRUE);
        if (disp != 0)
        {
            CHtmlSysWin_win32 *win;
            
            /* tell the helper to select this item */
            win = (CHtmlSysWin_win32 *)helper_->activate_stack_win_item(
                disp, dbgmain_->get_dbg_ctx(), dbgmain_);

            /* 
             *   tell the debugger to refresh its expression windows for
             *   the new context 
             */
            dbgmain_->update_expr_windows();

            /* bring the source window to the front */
            if (win != 0)
                win->bring_to_front();
            
            /* don't track the mouse any further */
            end_mouse_tracking(HTML_TRACK_LEFT);
            
            /* handled */
            return TRUE;
        }
    }
    
    /* inherit the default */
    return CHtmlSysWin_win32_dbgsrc::do_leftbtn_down(keys, x, y, clicks);
}

/* 
 *   check the status of a command 
 */
TadsCmdStat_t CHtmlSysWin_win32_dbgstk::active_check_command(int command_id)
{
    switch(command_id)
    {
    case ID_DEBUG_SETCLEARBREAKPOINT:
    case ID_DEBUG_DISABLEBREAKPOINT:
    case ID_DEBUG_RUNTOCURSOR:
    case ID_DEBUG_SETNEXTSTATEMENT:
    case ID_DEBUG_ADDTOWATCH:
    case ID_FILE_EDIT_TEXT:
        /* can't do these in a stack window */
        return TADSCMD_DISABLED;

    default:
        /* inherit default */
        return CHtmlSysWin_win32_dbgsrc::active_check_command(command_id);
    }
}

/* 
 *   execute a command 
 */
int CHtmlSysWin_win32_dbgstk::active_do_command(int notify_code,
                                                int command_id, HWND ctl)
{
    switch(command_id)
    {
    case ID_DEBUG_SETCLEARBREAKPOINT:
    case ID_DEBUG_DISABLEBREAKPOINT:
    case ID_DEBUG_RUNTOCURSOR:
    case ID_DEBUG_SETNEXTSTATEMENT:
        /* can't do these in a stack window */
        return FALSE;

    default:
        /* inherit default */
        return CHtmlSysWin_win32_dbgsrc::
            active_do_command(notify_code, command_id, ctl);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   History window implementation 
 */

/*
 *   create the HTML panel 
 */
void CHtmlSys_dbghistwin::create_html_panel(CHtmlFormatter *formatter)
{
    /* create the debugger stack window panel */
    html_panel_ = new CHtmlSysWin_win32_dbghist(formatter, this, TRUE, TRUE,
                                                prefs_, helper_, dbgmain_);
}

/*
 *   close the owner window 
 */
int CHtmlSys_dbghistwin::close_owner_window(int force)
{
    /* we're inside a docking or MDI parent, so close the parent */
    if (force)
    {
        /* 
         *   force the window closed with the special DOCK_DESTROY message
         *   (don't destroy it directly, since the window could be an MDI
         *   child 
         */
        SendMessage(GetParent(handle_), DOCKM_DESTROY, 0, 0);
        return TRUE;
    }
    else
    {
        /* ask the parent window to close itself */
        return !SendMessage(GetParent(handle_), WM_CLOSE, 0, 0);
    }
}

/*
 *   store my position in the preferences 
 */
void CHtmlSys_dbghistwin::store_winpos_prefs(const CHtmlRect *pos)
{
    /* store the position in the debugger configuration */
    if (dbgmain_->get_local_config() != 0)
        dbgmain_->get_local_config()->setval("hist win pos", 0, pos);

    /* store it in the global configuration (for the default) as well */
    if (dbgmain_->get_global_config() != 0)
        dbgmain_->get_global_config()->setval("hist win pos", 0, pos);
}

/* 
 *   check the status of a command 
 */
TadsCmdStat_t CHtmlSysWin_win32_dbghist::active_check_command(int command_id)
{
    switch(command_id)
    {
    case ID_DEBUG_SETCLEARBREAKPOINT:
    case ID_DEBUG_DISABLEBREAKPOINT:
    case ID_DEBUG_RUNTOCURSOR:
    case ID_DEBUG_SETNEXTSTATEMENT:
        /* can't do these in a history window */
        return TADSCMD_DISABLED;

    default:
        /* inherit default */
        return CHtmlSysWin_win32_dbgsrc::active_check_command(command_id);
    }
}

/* 
 *   execute a command 
 */
int CHtmlSysWin_win32_dbghist::active_do_command(
    int notify_code, int command_id, HWND ctl)
{
    switch(command_id)
    {
    case ID_DEBUG_SETCLEARBREAKPOINT:
    case ID_DEBUG_DISABLEBREAKPOINT:
    case ID_DEBUG_RUNTOCURSOR:
    case ID_DEBUG_SETNEXTSTATEMENT:
        /* can't do these in a history window */
        return FALSE;

    default:
        /* inherit default */
        return CHtmlSysWin_win32_dbgsrc::
            active_do_command(notify_code, command_id, ctl);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Build Wait dialog 
 */

/*
 *   process a command 
 */
int CTadsDlg_buildwait::do_command(WPARAM id, HWND ctl)
{
    switch(id)
    {
    case IDOK:
        /* do not dismiss the dialog on regular keystrokes */
        return TRUE;

    case IDCANCEL:
        /* on cancel/escape, ask about killing the compiler */
        if (MessageBox(0, "Unless the build appears to be stuck, "
                       "it is recommended that you wait for it to finish "
                       "normally, because forcibly ending a process can "
                       "destabilize Windows.  Do you really want to "
                       "force the build to terminate?", "Warning",
                       MB_YESNO | MB_ICONEXCLAMATION | MB_TASKMODAL)
            == IDYES)
        {
            /* try killing the compiler process */
            TerminateProcess(comp_hproc_, (UINT)-1);
        }

        /* 
         *   handled - don't dismiss, since we still want to wait until the
         *   compiler process ends 
         */
        return TRUE;

    case IDC_BUILD_DONE:
        /* 
         *   special message sent from main window when it hears that the
         *   build is done - dismiss the dialog
         */
        EndDialog(handle_, id);
        return TRUE;

    default:
        /* inherit default processing for anything else */
        return CTadsDialog::do_command(id, ctl);
    }
}
