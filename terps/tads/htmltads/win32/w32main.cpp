#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/w32main.cpp,v 1.4 1999/07/11 00:46:51 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32main.cpp - tads html win32 - main entrypoint
Function
  
Notes
  
Modified
  01/31/98 MJRoberts  - Creation
*/

#include <stdio.h>
#include <ctype.h>

#include <Windows.h>
#include <CommCtrl.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLW32_H
#include "htmlw32.h"
#endif
#ifndef HTMLPRS_H
#include "htmlprs.h"
#endif
#ifndef HTMLFMT_H
#include "htmlfmt.h"
#endif
#ifndef HTML_OS_H
#include "html_os.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif
#ifndef HTMLPREF_H
#include "htmlpref.h"
#endif
#ifndef HTMLRF_H
#include "htmlrf.h"
#endif
#ifndef W32MAIN_H
#include "w32main.h"
#endif
#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif
#ifndef TADSREG_H
#include "tadsreg.h"
#endif
#ifndef TADSIMG_H
#include "tadsimg.h"
#endif
#ifndef HTMLPNG_H
#include "htmlpng.h"
#endif
#ifndef HTMLMNG_H
#include "htmlmng.h"
#endif
#ifndef HTMLJPEG_H
#include "htmljpeg.h"
#endif
#ifndef HTMLRES_H
#include "htmlres.h"
#endif


/* TADS runtime definitions */
#ifndef TRD_H
#include "trd.h"
#endif

/* TADS OS header */
#include "os.h"


/* ------------------------------------------------------------------------ */
/*
 *   Application instance handle global variable - oswin.c requires that
 *   we (the definer of WinMain) define and initialize this variable. 
 */
extern "C" { HINSTANCE oss_G_hinstance; }


/* ------------------------------------------------------------------------ */
/*
 *   Flag: get-game-name callback has been invoked 
 */
static int S_get_game_cb_invoked;

/*
 *   Callback for getting the name of the game file.  The run-time will
 *   call this if it can't find the name of the game to play through any
 *   other means (such as via the command line).
 */
static int get_game_name_cb(void *ctx, char *buf, size_t buflen)
{
    OPENFILENAME info;
    CHtmlSys_mainwin *win = (CHtmlSys_mainwin *)ctx;
    int ret;
    char prompt[256];

    /* note that this callback has been invoked */
    S_get_game_cb_invoked = TRUE;

    /*
     *   If we have a pending game name, it means that the player has
     *   already selected a new game to play.  Simply return it now. 
     */
    if (win != 0 && win->get_pending_new_game() != 0
        && strlen(win->get_pending_new_game()) + 1 <= buflen)
    {
        /* copy the name */
        strcpy(buf, win->get_pending_new_game());

        /* 
         *   the pending game name has now been consumed - tell the window
         *   to forget about it 
         */
        win->clear_pending_new_game();

        /* return success */
        return TRUE;
    }

    /* fill in the control structure for an "open file" dialog */
    info.lStructSize = sizeof(info);
    info.hwndOwner = win->get_handle();
    info.hInstance = CTadsApp::get_app()->get_instance();
    info.lpstrFilter = w32_opendlg_filter;
    info.lpstrCustomFilter = 0;
    info.nFilterIndex = 0;
    info.lpstrFile = buf;
    buf[0] = '\0';
    info.nMaxFile = buflen;
    info.lpstrFileTitle = 0;
    info.nMaxFileTitle = 0;
    info.lpstrInitialDir = CTadsApp::get_app()->get_openfile_dir();
    LoadString(CTadsApp::get_app()->get_instance(),
               IDS_CHOOSE_GAME, prompt, sizeof(prompt));
    info.lpstrTitle = prompt;
    info.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    info.nFileOffset = 0;
    info.nFileExtension = 0;
    info.lpstrDefExt = 0;
    info.lCustData = 0;
    info.lpfnHook = 0;
    info.lpTemplateName = 0;

    /* set up to use the CTadsDialog centering hook */
    CTadsDialog::set_filedlg_center_hook(&info);

    /* 
     *   run the open file dialog - if it returns zero, it means the user
     *   cancelled, so we should return false to indicate that we can't
     *   offer a game file name; if it returns non-zero, the user entered
     *   a valid file, so we should return true to indicate that we
     *   provided a file 
     */
    ret = GetOpenFileName(&info);

    /* save the open file directory if that succeeded */
    if (ret != 0)
        CTadsApp::get_app()->set_openfile_dir(buf);

    /* return the result */
    return (ret != 0);
}

/* ------------------------------------------------------------------------ */
/*
 *   simple hex-to-string converter for the exception handler, so that the
 *   exception handler can avoid calling any heavyweight library routines
 *   such as sprintf 
 */
static void hex_to_str(char *buf, DWORD hexval)
{
    int i;

    /* build the string in reverse order */
    for (i = 0 ; i < 8 ; ++i)
    {
        char cur;

        /* get the current character value */
        cur = (char)(hexval & 0xf);
        *(buf + 7 - i) = (cur < 10 ? cur + '0' : cur + 'a' - 10);

        /* shift the value to remove this character */
        hexval >>= 4;
    }

    /* null-terminate the string */
    *(buf + 8) = '\0';
}

/*
 *   Top-level exception handler.  We'll use this to catch fatal errors
 *   before the operating system kills our process entirely.  We'll print
 *   out some extra diagnostic information so that users can send us crash
 *   details for us to analyze in cases where the cause of the crash is
 *   not clear from the test case (which is the case when the problem is
 *   configuration-dependent).
 *   
 *   (This symbol is non-static so that it shows up in the map file, which
 *   is important because it provides us with the adjustment bias so that
 *   we can figure out what all of the other offsets mean.)  
 */
LONG WINAPI exc_handler(EXCEPTION_POINTERS *info)
{
    CONTEXT *ctx = info->ContextRecord;
    DWORD *ebp;
    int i;
    HANDLE hfile;
    char buf[128];
    DWORD actual;

    /* 
     *   Open an error dump file.  We'll hope the system is stable enough
     *   that we can do this. 
     */
    hfile = CreateFile("tadscrsh.txt", GENERIC_WRITE, 0, 0, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, 0);
    if (hfile == 0)
        return EXCEPTION_CONTINUE_SEARCH;

    /* put in our own address for calculating the relative base address */
    strcpy(buf, "exc_handler = ");
    hex_to_str(buf + 14, (DWORD)exc_handler);
    strcpy(buf + 22, "\r\n");
    WriteFile(hfile, buf, 24, &actual, 0);

    /* show where we are now */
    strcpy(buf, "CS:EIP = ");
    hex_to_str(buf + 9, ctx->SegCs);
    *(buf + 17) = ':';
    hex_to_str(buf + 18, ctx->Eip);
    strcpy(buf + 26, "\r\n");
    WriteFile(hfile, buf, 28, &actual, 0);

    /* trace back the stack */
    ebp = (DWORD *)ctx->Ebp;
    for (i = 0 ; i < 50 ; ++i)
    {
        DWORD retaddr;

        /* 
         *   if the enclosing stack frame doesn't look valid, don't
         *   proceed -- each enclosing stack frame should have a higher
         *   stack address than inner ones, because the stack grows
         *   downwards 
         */
        if ((DWORD *)*ebp <= ebp)
            break;

        /* display the return address */
        retaddr = *(ebp + 1);
        hex_to_str(buf, retaddr);
        strcpy(buf + 8, "\r\n");
        WriteFile(hfile, buf, 10, &actual, 0);

        /* move on to the enclosing frame */
        ebp = (DWORD *)*ebp;
    }

    /* close the file */
    CloseHandle(hfile);

    /* use the default exception handling, which will end the process */
    return EXCEPTION_CONTINUE_SEARCH;
}


/* ------------------------------------------------------------------------ */
/*
 *   Set_game_name host-application callback.  The TADS engine calls this
 *   routine upon loading a game to tell the host application about the
 *   game file.  We use this information to set up the resource loader,
 *   set the window title, and add the game to the list of recently-loaded
 *   games.  
 */

/* callback context structure */
struct set_game_ctx_t
{
    /* hooked callback - next in chain */
    void (*set_game_name)(void *appctxdat, const char *fname);
    void *set_game_name_ctx;
};

/* set_game_name callback */
static void set_game_name(void *ctx0, const char *fname)
{
    set_game_ctx_t *ctx = (set_game_ctx_t *)ctx0;

    /* if we have a main window, notify it of the new game */
    if (CHtmlSys_mainwin::get_main_win() != 0)
        CHtmlSys_mainwin::get_main_win()->notify_load_game(fname);

    /* 
     *   call the previous callback in the chain (this is the callback
     *   that was in effect before we hooked the callback pointer) 
     */
    (*ctx->set_game_name)(ctx->set_game_name_ctx, fname);
}


/* ------------------------------------------------------------------------ */
/*
 *   Runtime startup 
 */

static void run_game(char *cmdline,
                     int (*tadsmain)(int, char **, appctxdef *, char *),
                     char *before_opts, char *config_file)
{
    CHtmlParser *parser;
    CHtmlFormatterInput *formatter;
    CHtmlSys_mainwin *win;
    RECT pos;
    const CHtmlRect *winpos;
    const int MAX_ARGS = 50;
    char *argv[MAX_ARGS];
    appctxdef appctx;
    int argc;
    char exefile[256];
    char *p;
    int debugwin_opt = FALSE;
    CHtmlParser *dbgprs = 0;
    CHtmlFormatter *dbgfmt = 0;
    CHtmlSys_dbglogwin *dbgwin = 0;
    int done;
    int respath_arg;
    char respathbuf[OSFNMAX];
    int iter;
    RECT deskrc;
    set_game_ctx_t set_game_ctx;

    /* 
     *   clear out the application context - if any fields are added in
     *   the future that we don't deal with, this will ensure they're
     *   cleared out 
     */
    memset(&appctx, 0, sizeof(appctx));

    /* set the name of the application for usage messages */
    appctx.usage_app_name = w32_usage_app_name;

#ifndef TADSHTML_DEBUG
    /* set the top-level exception handler */
    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)exc_handler);
#endif

    /* get the executable's filename and use it as argv[0] */
    GetModuleFileName(CTadsApp::get_app()->get_instance(),
                      exefile, sizeof(exefile));
    argv[0] = exefile;

    /* 
     *   Create the parser, and start it off in literal mode.  We'll stay
     *   in literal mode until the game tells us that it wants HTML
     *   interpreted, so that the initial startup information (such as the
     *   TADS runtime banner) will be displayed properly, and so that
     *   games that don't use HTML will look okay. 
     */
    parser = new CHtmlParser(TRUE);

    /* set up the formatter and a window for display */
    formatter = new CHtmlFormatterInput(parser);
    win = new CHtmlSys_mainwin(formatter, parser, w32_in_debugger);
    winpos = win->get_prefs()->get_win_pos();
    SetRect(&pos,
            winpos->left < 0 ? CW_USEDEFAULT : winpos->left,
            winpos->top < 0 ? CW_USEDEFAULT : winpos->top,
            (winpos->right < 0 || winpos->left < 0
             ? CW_USEDEFAULT : winpos->right - winpos->left),
            (winpos->bottom < 0 || winpos->top < 0
             ? CW_USEDEFAULT : winpos->bottom - winpos->top));

    /* if the window is off the screen, use the default sizes */
    GetWindowRect(GetDesktopWindow(), &deskrc);
    if (pos.left < deskrc.left || pos.left + pos.right > deskrc.right
        || pos.top < deskrc.top || pos.top + pos.bottom > deskrc.bottom
        || pos.right < 20 || pos.bottom < 20)
    {
        /* the saved values look invalid; use a default position */
        SetRect(&pos, CW_USEDEFAULT, CW_USEDEFAULT,
                CW_USEDEFAULT, CW_USEDEFAULT);
    }

    /* create the window */
    win->create_system_window(0, TRUE, w32_titlebar_name, &pos);

    /* try loading the .exe resources */
    win->load_exe_resources(exefile);

    /* 
     *   initialize the TADS container application context with
     *   information relevant to the formatter's resource finder -- this
     *   will let the TADS loader inform the resource finder of any
     *   HTMLRES resources embedded in the .GAM file 
     */
    formatter->get_res_finder()->init_appctx(&appctx);

    /* set up our own callback for getting the game name */
    appctx.get_game_name = get_game_name_cb;
    appctx.get_game_name_ctx = win;

    /* no resource path yet */
    appctx.ext_res_path = 0;

    /* set up the preferences callbacks for file safety level */
    appctx.set_io_safety_level = &CHtmlPreferences::set_io_safety_level_cb;
    appctx.get_io_safety_level = &CHtmlPreferences::get_io_safety_level_cb;
    appctx.io_safety_level_ctx = win->get_prefs();

    /* hook into the call chain for the set_game_name callback */
    set_game_ctx.set_game_name = appctx.set_game_name;
    set_game_ctx.set_game_name_ctx = appctx.set_game_name_ctx;
    appctx.set_game_name = &set_game_name;
    appctx.set_game_name_ctx = &set_game_ctx;

    /*
     *   Parse the command line 
     */
    for (argc = 1, p = cmdline, done = FALSE, respath_arg = FALSE ;
         !done && p != 0 && *p != '\0' && argc + 1 < MAX_ARGS ; ++p)
    {
        char inquote;
        
        /* skip leading spaces */
        while (*p != '\0' && isspace(*p))
            ++p;

        /* if we're done, quit */
        if (*p == '\0')
            break;

        /* start a new argument */
        argv[argc] = p;

        /* scan the argument */
        for (inquote = '\0' ; *p != '\0' ; ++p)
        {
            /* if this is a space and we're not in a quote, we're done */
            if (isspace(*p) && inquote == '\0')
                break;

            /* if it's a quote, enter/exit quotes */
            if ((inquote == '\0' && (*p == '"' || *p == '\''))
                || (inquote != '\0' && *p == inquote))
            {
                /* start/close the quote */
                inquote = (inquote ? '\0' : *p);

                /* remove the quote from the argument text */
                memmove(p, p + 1, strlen(p));

                /* back off one to adjust for the movement */
                --p;
            }
            else if (inquote != '\0' && *p == '\\' && *(p+1) == inquote)
            {
                /* 
                 *   it's a quoted quote within a quoted section - remove
                 *   the backslash, and proceed, ignoring the quote 
                 */
                memmove(p, p + 1, strlen(p));
            }
        }

        /* note if we're at the end of the string */
        done = (*p == '\0');

        /* end the argument */
        *p = '\0';

        /*
         *   Check for html-specific arguments.  If we see an argument
         *   that's special to the HTML run-time, note it, then remove it
         *   from the list, since we don't want to pass it along to the
         *   normal run-time.  
         */
        if (respath_arg)
        {
            size_t len;
            
            /* 
             *   The previous argument was "-respath", so this is the
             *   resource path value.  Copy it to our buffer, and make
             *   sure it ends in a path separator.  
             */
            strcpy(respathbuf, argv[argc]);
            if ((len = strlen(respathbuf)) != 0
                && respathbuf[len - 1] != '\\'
                && respathbuf[len - 1] != '/'
                && respathbuf[len - 1] != ':')
            {
                respathbuf[len++] = '\\';
                respathbuf[len] = '\0';
            }

            /* set the application context with the resource path */
            appctx.ext_res_path = respathbuf;

            /* we've now fetched the resource path argument */
            respath_arg = FALSE;
        }
        else if (w32_allow_debugwin && !stricmp(argv[argc], "-debugwin"))
        {
            /* turn on the debug window */
            debugwin_opt = TRUE;
        }
        else if (!stricmp(argv[argc], "-respath"))
        {
            /* the next argument is the resource path */
            respath_arg = TRUE;
        }
        else if (!stricmp(argv[argc], "-noalphablend"))
        {
            /* turn off alpha blending in the image subsystem */
            CTadsImage::disable_alpha_support();
        }
        else
        {
            /* 
             *   this isn't an HTML argument, so pass it to the normal
             *   run-time by including it in the argument vector 
             */
            ++argc;
        }
    }

    /*
     *   If they asked for a debug log window, open one
     */
    if (debugwin_opt)
    {
        /* set up the HTML parser and formatter for the debug window */
        dbgprs = new CHtmlParser(TRUE);
        dbgfmt = new CHtmlFormatterStandalone(dbgprs);

        /* create the debug window itself */
        dbgwin = new CHtmlSys_dbglogwin(dbgprs, win->get_prefs(), 0);
        dbgwin->init_html_panel(dbgfmt);

        /* figure out where it goes and open it */
        winpos = win->get_prefs()->get_dbgwin_pos();
        SetRect(&pos,
                winpos->left < 0 ? CW_USEDEFAULT : winpos->left,
                winpos->top < 0 ? CW_USEDEFAULT : winpos->top,
                (winpos->right < 0 || winpos->left < 0
                 ? CW_USEDEFAULT : winpos->right - winpos->left),
                (winpos->bottom < 0 || winpos->top < 0
                 ? CW_USEDEFAULT : winpos->bottom - winpos->top));
        dbgwin->create_system_window(0, TRUE, "HTML Debug Log", &pos);

        /* put the debug window behind the main window */
        SetWindowPos(dbgwin->get_handle(), win->get_handle(), 0, 0, 0, 0,
                     SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);

        /*  
         *   Tell the main window about the debug window, so that it knows
         *   where to put the debug messages, and add a menu for it.  
         */
        win->set_debug_win(dbgwin, TRUE);
    }

    /* the get-game-name callback has not yet been invoked */
    S_get_game_cb_invoked = FALSE;

    /* keep going until we quit */
    for (iter = 0 ;; ++iter)
    {
        int ret;

        /* run the pre-startup routine */
        if (!w32_pre_start(iter, argc, argv))
            break;

        /* 
         *   if there's *already* a pending new game name, it means that
         *   the pre-start routine provided its own game to run - throw
         *   away our arguments so that we pick up the pending new game
         *   instead 
         */
        if (CHtmlSys_mainwin::get_main_win() != 0
            && CHtmlSys_mainwin::get_main_win()->get_pending_new_game() != 0)
            argc = 1;

        /* tell the main panel we're starting a new game */
        if (CHtmlSys_mainwin::get_main_win() != 0)
            CHtmlSys_mainwin::get_main_win()->start_new_game();

        /* delete all memory previously allocated by the TADS run-time */
        oss_win_free_all();

        /* 
         *   if this isn't the first time through, re-initialize the
         *   resource finder, to clear out resources from the last run
         */
        if (iter != 0)
            formatter->get_res_finder()->reset();

        /* run TADS */
        ret = os0main2(argc, argv, tadsmain,
                       before_opts, config_file, &appctx);

        /* notify the game window that the game has ended */
        if (CHtmlSys_mainwin::get_main_win() != 0)
            CHtmlSys_mainwin::get_main_win()->end_current_game();

        /* perform appropriate post-quit processing */
        if (!w32_post_quit(ret))
            break;

        /*
         *   If the get-game-name callback function was never invoked,
         *   forget our last argument.  This is a bit tricky: if the
         *   get-game-name callback was not invoked, it means that our
         *   last argument is the name of a game to be played.  Because we
         *   now want to replace that name with the new game the user has
         *   selected, we want to forget this last argument.  If the
         *   get-game-name callback was invoked, though, it means that the
         *   argument list did not have a game name specified, hence
         *   there's nothing to forget.  
         */
        if (argc > 1 && !S_get_game_cb_invoked)
            --argc;
    }

    /* close the debug window, if we opened one */
    if (dbgwin != 0)
        DestroyWindow(dbgwin->get_handle());
    
    /*
     *   Make sure we've closed the main window.  If it's still around,
     *   the user must have quit via a command to the game, which doesn't
     *   get rid of the window - get rid of it now. 
     */
    if (CHtmlSys_mainwin::get_main_win() != 0)
    {
        /* destroy the main window */
        DestroyWindow(CHtmlSys_mainwin::get_main_win()->get_handle());
    }
    else
    {
        /* make sure we get the termination signal */
        PostQuitMessage(0);
    }

    /* handle events until we've terminated */
    CTadsApp::get_app()->event_loop(0);

    /* tell the formatter to forget about the parser */
    formatter->release_parser();

    /* done with the parser - delete it */
    delete parser;

    /* 
     *   Done with the formatter - delete it.  Note that the parser
     *   depends on the formatter, so we must delete the parser before we
     *   delete the formatter. 
     */
    delete formatter;

    /* if we created a debug window, delete its parser and formatter */
    if (dbgfmt != 0)
    {
        dbgfmt->release_parser();
        delete dbgprs;
        delete dbgfmt;
    }
}



/* ------------------------------------------------------------------------ */
/*
 *   Debug code - displays messages to a console window 
 */

#ifdef TADSHTML_DEBUG

void init_debug_console()
{
    AllocConsole();
}

void close_debug_console()
{
    INPUT_RECORD inrec;
    DWORD cnt;

    /*
     *   Before exiting, wait for a keystroke, so that the user can see
     *   the contents of the console buffer 
     */
    oshtml_dbg_printf("\nPress any key to exit...");

    /* clear out any keyboard events in the console buffer already */
    for (;;)
    {
        if (!PeekConsoleInput(GetStdHandle(STD_INPUT_HANDLE),
                              &inrec, 1, &cnt)
            || cnt == 0)
            break;
        ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inrec, 1, &cnt);
    }

    /* wait for a key from the console input buffer */
    for (;;)
    {
        if (!ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE),
                              &inrec, 1, &cnt))
            break;
        if (inrec.EventType == KEY_EVENT)
            break;
    }
}

/*
 *   Display a debug message to the system console 
 */
void os_dbg_sys_msg(const textchar_t *msg)
{
    DWORD cnt;

    /* display the message on the debug console */
    WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), msg,
                 get_strlen(msg), &cnt, 0);
}

#else /* TADSHTML_DEBUG */

/*
 *   Debugging not enabled - provide dummy versions of the debug functions 
 */
void init_debug_console() { }
void close_debug_console() { }

#endif /* TADSHTML_DEBUG */


/* ------------------------------------------------------------------------ */
/*
 *   Check the common controls DLL version.  Returns true if everything is
 *   okay or the user explicitly tells us to continue running despite a
 *   version problem, false if we should terminate due to a version
 *   mismatch.  
 */
static int check_common_ctl_vsn(HINSTANCE inst)
{
    void *vsnbuf;
    DWORD vsnsiz;
    DWORD hdl;
    VS_FIXEDFILEINFO *info;
    UINT infosiz;
    int ret;

    /* presume we'll allow execution to continue */
    ret = TRUE;

    /* get the file version sized information */
    vsnsiz = GetFileVersionInfoSize("comctl32.dll", &hdl);
    if (vsnsiz == 0)
        return TRUE;

    /* allocate a buffer, and load the version information */
    vsnbuf = th_malloc(vsnsiz);
    if (!GetFileVersionInfo("comctl32.dll", 0, vsnsiz, vsnbuf))
        goto done;

    /* get the VS_FIXEDFILEINFO structure */
    if (!VerQueryValue(vsnbuf, "\\", (void **)&info, &infosiz))
        goto done;

    /* check the information - we require 4.70 or later */
    if (info->dwFileVersionMS < 0x00040046)
    {
        char msg[512];

        LoadString(inst, IDS_COMCTL32_WARNING, msg, sizeof(msg));
        switch(MessageBox(0, msg, "TADS",
                          MB_OKCANCEL | MB_ICONEXCLAMATION | MB_TASKMODAL))
        {
        case IDCANCEL:
            /* they want to cancel - good for them */
            ret = FALSE;
            break;

        default:
            /* allow continued running */
            break;
        }
    }

done:
    /* free the version buffer */
    th_free(vsnbuf);

    /* return the result */
    return ret;
}

/* ------------------------------------------------------------------------ */
/*
 *   Main entrypoint 
 */
int PASCAL WinMain(HINSTANCE inst, HINSTANCE pinst,
                   LPSTR cmdline, int cmdshow)
{
    HINSTANCE rich_ed_hdl;

    /* make sure common controls are loaded */
    InitCommonControls();

    /* load the Rich Edit control in case we need it */
    rich_ed_hdl = LoadLibrary("RICHED32.DLL");

    /*
     *   Ensure that we have the right version of the common controls.  If
     *   we have an older version, we'll run into problems, so at least
     *   warn the user about it.  
     */
    if (!check_common_ctl_vsn(inst))
        return 0;

    /* set the application instance in the TADS os layer */
    oss_G_hinstance = inst;

    /* initialize the debug console */
    init_debug_console();

    /* initialize the global resource table */
    CHtmlResType::add_basic_types();

    /* create the main application object */
    CTadsApp::create_app(inst, pinst, cmdline, cmdshow);

    /* 
     *   Tell the PNG reader to keep transparency information.  Also, tell
     *   the reader to use the 24-bit RGB format for the in-memory
     *   representation, regardless of the current video mode - we do all of
     *   our drawing internally on an RGB DIB section, so we don't need to
     *   worry about palettes for our in-memory graphics objects.  
     */
    CHtmlPng::set_options(HTMLPNG_OPT_TRANS_TO_ALPHA | HTMLPNG_OPT_RGB24);

    /* ask the windows PNG and MNG code to check for alpha support */
    CTadsPng::init_alpha_support();

    /* 
     *   tell the JPEG reader to use 24-bit RGB format regardless of video
     *   mode (for the same reason we do this with PNG's) 
     */
    CHtmlJpeg::set_options(HTMLJPEG_OPT_RGB24);

    /* go run the game */
    run_game(cmdline, w32_tadsmain, w32_beforeopts, w32_configfile);

    /* done with the main application object */
    CTadsApp::delete_app();

    /* delete the global resource table */
    CHtmlResType::delete_res_table();

    /* check memory */
    HTML_IF_DEBUG(th_list_memory_blocks());
    HTML_IF_DEBUG(th_list_subsys_memory_blocks());

    /* close the debug console, making sure the user acknowledges it */
    close_debug_console();

    /* unload the Rich Edit control library */
    FreeLibrary(rich_ed_hdl);

    /* success */
    return 0;
}

