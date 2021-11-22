#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 1999 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32tdbt3.cpp - HTML TADS Debugger - TADS 3 engine configuration
Function
  
Notes
  
Modified
  11/24/99 MJRoberts  - Creation
*/

#include <Windows.h>

#include <string.h>
#include <ctype.h>

#include "os.h"

#include "vmvsn.h"

#include "t3main.h"
#include "t3std.h"
#include "w32main.h"
#include "w32tdb.h"
#include "w32prj.h"
#include "tadsdock.h"
#include "htmldcfg.h"
#include "w32dbgpr.h"
#include "tadsdlg.h"
#include "tadsapp.h"
#include "htmldcfg.h"
#include "vmprofty.h"
#include "htmlver.h"
#include "w32ver.h"


/* ------------------------------------------------------------------------ */
/*
 *   Service routine: is a file in the build configuration a library (".tl")
 *   file?  It is if it's explicitly marked as such in the build config, or
 *   if the filename ends in ".tl".  'idx' is the index in the build config
 *   of the source file in question.  
 */
static int is_source_file_a_lib(CHtmlDebugConfig *config, int idx)
{
    char typebuf[64];

    /* check for an explicit type indicator */
    if (config->getval("build", "source_file_types", idx,
                       typebuf, sizeof(typebuf)))
        typebuf[0] = '\0';
    
    /* 
     *   Note the type.  If we have an explicit type specifier, use the type
     *   from the specifier; otherwise, it's a library if it ends in ".tl", a
     *   source file otherwise.  
     */
    if (typebuf[0] != '\0')
    {
        /* 
         *   we have an explicit type specifier, so this is easy - if it's
         *   type 'l', it's a library; otherwise it isn't 
         */
        return (typebuf[0] == 'l');
    }
    else
    {
        char buf[OSFNMAX];
        size_t len;
        
        /* no type specifier, so we need to check the filename - fetch it */
        config->getval("build", "source_files", idx, buf, sizeof(buf));

        /* it's a library if the filename ends in ".tl" */
        return ((len = strlen(buf)) > 3
                && stricmp(buf + len - 3, ".tl") == 0);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Debugger cover function for beginning program execution.  This
 *   routine re-builds the argument vector from the arguments stored in
 *   the configuration settings.  
 */
static int t3_dbg_main(int argc, char **argv, struct appctxdef *appctx,
                       char *config_file)
{
    CHtmlSys_dbgmain *dbgmain = CHtmlSys_dbgmain::get_main_win();
    char **my_argv;
    int my_argc;
    int i;
    char *src;
    char *dst;
    char logfile[OSFNMAX];
    char cmdfile[OSFNMAX];
    char resdir[OSFNMAX];
    char args[1024];
    CHtmlDebugConfig *config;
    char quote;
    int ret;

    /* 
     *   if there's no main debugger window, we can't synthesize any
     *   arguments 
     */
    if (dbgmain == 0)
        return t3main(argc, argv, appctx, config_file);

    /* get the local configuration object */
    config = dbgmain->get_local_config();

    /*
     *   Ignore the command line passed in from the caller, and use our
     *   own command line instead.  First, retrieve the command line from
     *   the configuration settings.  
     */
    if (config->getval("prog_args", "args", 0, args, sizeof(args)))
        args[0] = '\0';

    /* skip any leading spaces in the argument string */
    for (src = args ; isspace(*src) ; ++src) ;

    /* 
     *   count the first argument, if it is present - we count the start
     *   of each new argument 
     */
    my_argc = 0;
    if (*src != '\0')
        ++my_argc;

    /* 
     *   Parse the arguments into individual strings, null-terminating
     *   each argument, but leaving them in the original buffer.  Note
     *   that the buffer can only shrink, as we remove quotes, but it can
     *   never grow; this allows us to process the buffer in-place,
     *   without making a separate copy.  
     */
    for (quote = '\0', dst = args ; *src != '\0' ; )
    {
        /* if process according to whether we're in a quoted string or not */
        if (quote == '\0')
        {
            /* 
             *   we're not in a quoted string - if this is a space, it
             *   starts the next argument; if it's a quote, note that
             *   we're in a quoted string, and remove the quote; otherwise
             *   just copy it out 
             */
            switch(*src)
            {
            case '"':
                /* it's a quote - note it, but don't copy it to the output */
                quote = *src;
                ++src;
                break;

            case ' ':
                /* 
                 *   it's the end of the current argument - null-terminate
                 *   the current argument string 
                 */
                *dst++ = '\0';

                /* skip the space we just scanned */
                ++src;

                /* skip to the next non-space character */
                while (isspace(*src))
                    ++src;

                /* 
                 *   if another argument follows, count the start of the
                 *   new argument 
                 */
                if (*src != '\0')
                    ++my_argc;
                break;

            default:
                /* simply copy anything else to the output */
                *dst++ = *src++;
                break;
            }
        }
        else
        {
            /* 
             *   if it's our quote, we're done with the quoted section -
             *   unless it's a stuttered quote, in which case we will
             *   simply copy a single quote to the output 
             */
            if (*src == quote)
            {
                /* check for stuttering */
                if (*(src + 1) == quote)
                {
                    /* it's stuttered - make a single copy of the quote */
                    *dst++ = quote;

                    /* skip both copies of the quote in the input */
                    src += 2;
                }
                else
                {
                    /* 
                     *   it's the end of our quoted section - skip it in
                     *   the input, but don't copy it to the output 
                     */
                    ++src;

                    /* note that we're no longer in a quoted section */
                    quote = '\0';
                }
            }
            else
            {
                /* 
                 *   it's just the next character of the quoted section -
                 *   copy it unchanged 
                 */
                *dst++ = *src++;
            }
        }
    }

    /* null-terminate the last argument string */
    *dst = '\0';

    /* get the log file, if one is specified */
    if (config->getval("prog_args", "logfile", 0, logfile, sizeof(logfile)))
        logfile[0] = '\0';

    /* get the command input file, if one is specified in the configuration */
    if (config->getval("prog_args", "cmdfile", 0, cmdfile, sizeof(cmdfile)))
        cmdfile[0] = '\0';

    /* add space for the executable name argument (argv[0]) */
    ++my_argc;

    /* add space for the image file name argument */
    ++my_argc;

    /* 
     *   add space for the log file, if there is one - we need two
     *   arguments for this (one for the "-l", and one for the filename) 
     */
    if (logfile[0] != '\0')
        my_argc += 2;

    /* add another two for the command file, if there is one ("-c file") */
    if (cmdfile[0] != '\0')
        my_argc += 2;

    /* add another two for the resource directory ("-R dir") */
    my_argc += 2;

    /* allocate space for the argument vector */
    my_argv = (char **)t3malloc(my_argc * sizeof(*my_argv));

    /* copy the original argv[0] */
    my_argv[0] = argv[0];

    /* start at slot 1 */
    i = 1;

    /* add the log file argument, if present */
    if (logfile[0] != '\0')
    {
        my_argv[i++] = "-l";
        my_argv[i++] = logfile;
    }

    /* add the command file argument, if present */
    if (cmdfile[0] != '\0')
    {
        my_argv[i++] = "-i";
        my_argv[i++] = cmdfile;
    }

    /* 
     *   Add the resource file path - this is the path to the *project* file,
     *   which might be different from the path to the game file.  As a
     *   matter of convention, we tell users to keep resources rooted in the
     *   project directory, so we want to look there regardless of where the
     *   game file goes.  (In particular, it's typical for the game file to
     *   be in a ./Debug subdirectory, since that's the way we configure new
     *   projects by default.)  
     */
    os_get_path_name(resdir, sizeof(resdir),
                     dbgmain->get_local_config_filename());
    my_argv[i++] = "-R";
    my_argv[i++] = resdir;

    /* add the program name, which we can learn from the debugger */
    my_argv[i++] = (char *)dbgmain->get_gamefile_name();

    /* add the user arguments */
    for (src = args ; i < my_argc ; ++i, src += strlen(src) + 1)
    {
        /* set this argument */
        my_argv[i] = src;
    }

    /*
     *   If we have a host application context, and it accepts resource file
     *   paths, add all of the top-level library directories to the resource
     *   search list.  This lets the program find resources that come from
     *   library directories.  
     */
    if (appctx != 0 && appctx->add_res_path != 0)
    {
        int i;
        int cnt;
        
        /* scan the source file list for libraries */
        cnt = config->get_strlist_cnt("build", "source_files");
        for (i = 0 ; i < cnt ; ++i)
        {
            /* if this is a library, add its path to the search list */
            if (is_source_file_a_lib(config, i))
            {
                char fname[OSFNMAX];
                char path[OSFNMAX];
                
                /* get the library's name */
                config->getval("build", "source_files", i,
                               fname, sizeof(fname));

                /* search the library directory list for this file */
                if (CLibSearchPathList::find_tl_file(
                    dbgmain->get_global_config(), fname, path, sizeof(path))
                    && path[0] != '\0')
                {
                    /* we found a path - add it to the search list */
                    appctx->add_res_path(appctx->add_res_path_ctx,
                                         path, strlen(path));
                }
            }
        }
    }
            
    /* run the program */
    ret = t3main(my_argc, my_argv, appctx, config_file);

    /* free the argument vector */
    t3free(my_argv);

    /* return the result */
    return ret;
}


/* ------------------------------------------------------------------------ */
/*
 *   Define startup configuration variables for the TADS debugger 
 */
int (*w32_tadsmain)(int, char **, struct appctxdef *, char *) = t3_dbg_main;
char *w32_beforeopts = "";
char *w32_configfile = 0;
int w32_allow_debugwin = FALSE;
int w32_always_pause_on_exit = FALSE;
char *w32_setup_reg_val_name = "Debugger Setup Done";
char *w32_usage_app_name = "htmltdb3";
char *w32_titlebar_name = "HTML TADS 3";
int w32_in_debugger = TRUE;
char *w32_opendlg_filter = "TADS 3 Applications\0*.t3\0All Files\0*.*\0\0";
char *w32_debug_opendlg_filter =
    "TADS 3 Projects\0*.t3m\0"
    "All Files\0*.*\0\0";
char *w32_config_opendlg_filter =
    "TADS 3 Applications and Projects\0*.t3;*.t3m\0"
    "All Files\0*.*\0\0";
char *w32_game_ext = "t3";
const char *w32_version_string =
    HTMLTADS_VERSION
    " (Build Win" HTMLTADS_WIN32_BUILD
    "; TADS " T3VM_VSN_STRING ")";
const char w32_pref_key_name[] =
    "Software\\TADS\\HTML TADS\\3.0\\Settings";
const char w32_tdb_global_prefs_file[] = "htmltdb3.t3c";
const char w32_tdb_config_ext[] = "t3m";
const char w32_tadsman_fname[] = "doc\\index.htm";
int w32_system_major_vsn = 3;

/* ------------------------------------------------------------------------ */
/*
 *   Debugger version factoring routines for TADS 3
 */

/*
 *   T3 Workbench has the "project" UI to maintain the source file list, so
 *   there's no need to maintain a separate source files list in the helper's
 *   project configuration.  
 */
int CHtmlSys_dbgmain::vsn_need_srcfiles() const
{
    return FALSE;
}

/*
 *   Load the configuration file 
 */
int CHtmlSys_dbgmain::vsn_load_config_file(CHtmlDebugConfig *config,
                                           const textchar_t *fname)
{
    size_t len;

    /* 
     *   if the filename ends in .t3m, load it with the .t3m loader;
     *   otherwise, load it with our private binary file format loader 
     */
    len = strlen(fname);
    if (len >= 4 && stricmp(fname + len - 4, ".t3m") == 0)
    {
        char sys_lib_path[OSFNMAX];
        
        /* get the system library path */
        os_get_special_path(sys_lib_path, sizeof(sys_lib_path),
                            _pgmptr, OS_GSP_T3_LIB);

        /* it's a .t3m file - use the .t3m loader */
        return config->load_t3m_file(fname, sys_lib_path);
    }
    else
    {
        /* use the private binary file format loader */
        return config->load_file(fname);
    }
}

/*
 *   Save the configuration file 
 */
int CHtmlSys_dbgmain::vsn_save_config_file(CHtmlDebugConfig *config,
                                           const textchar_t *fname)
{
    size_t len;

    /* 
     *   if the filename ends in .t3m, save it with the .t3m writer;
     *   otherwise, save it with our private binary file format writer.  
     */
    len = strlen(fname);
    if (len >= 4 && stricmp(fname + len - 4, ".t3m") == 0)
    {
        /* it's a .t3m file - use the .t3m writer */
        return config->save_t3m_file(fname);
    }
    else
    {
        /* use the private binary file format writer */
        return config->save_file(fname);
    }
}

/*
 *   Close windows in preparation for loading a new configuration 
 */
void CHtmlSys_dbgmain::vsn_load_config_close_windows()
{
    /* close the project window */
    if (projwin_ != 0)
        SendMessage(GetParent(projwin_->get_handle()), DOCKM_DESTROY, 0, 0);
}

/*
 *   Open windows for a newly-loaded configuration 
 */
void CHtmlSys_dbgmain::vsn_load_config_open_windows(const RECT *deskrc)
{
    RECT rc;
    int vis;
    
    /* create the project window */
    projwin_ = new CHtmlDbgProjWin(this);
    SetRect(&rc, deskrc->left + 2, deskrc->bottom - 2 - 150, 300, 150);
    projwin_->load_win_config(local_config_, "project win", &rc, &vis);
    load_dockwin_config_win(local_config_, "project win", projwin_,
                            "Project", vis, TADS_DOCK_ALIGN_LEFT,
                            170, 270, &rc, TRUE);
}

/*
 *   Save version-specific configuration information 
 */
void CHtmlSys_dbgmain::vsn_save_config()
{
    /* save the project window's layout information */
    if (projwin_ != 0)
    {
        /* save the project window's layout */
        projwin_->save_win_config(local_config_, "project win");
        projwin_->save_win_config(global_config_, "project win");

        /* save the project file information */
        projwin_->save_config(local_config_);
    }
}

/*
 *   check command status 
 */
TadsCmdStat_t CHtmlSys_dbgmain::vsn_check_command(int command_id)
{
    switch(command_id)
    {
    case ID_PROJ_ADDFILE:
    case ID_PROJ_ADDEXTRES:
    case ID_PROJ_ADDRESDIR:
        /* 
         *   always route these to the project window, even if it's not
         *   the active window 
         */
        return (projwin_ != 0
                ? projwin_->check_command(command_id)
                : TADSCMD_UNKNOWN);

    default:
        /* we don't handle other cases */
        return TADSCMD_UNKNOWN;
    }
}

/* 
 *   execute command 
 */
int CHtmlSys_dbgmain::vsn_do_command(int nfy, int cmd, HWND ctl)
{
    switch(cmd)
    {
    case ID_PROJ_ADDFILE:
    case ID_PROJ_ADDEXTRES:
    case ID_PROJ_ADDRESDIR:
        /* 
         *   always route these to the project window, even if it's not
         *   the active window 
         */
        return (projwin_ != 0 ? projwin_->do_command(nfy, cmd, ctl) : FALSE);
        
    default:
        /* we don't handle other cases */
        return FALSE;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Create a temporary directory.  Returns zero on success, non-zero on
 *   failure. 
 */
static int create_temp_dir(char *dirbuf, HWND owner_hwnd)
{
    char systmp[OSFNMAX];

    /* 
     *   seed the random number generator - we'll try random names until
     *   we find one that doesn't collide with another process's names 
     */
    srand((unsigned int)GetTickCount());

    /* 
     *   get the system temporary directory - we'll create our temporary
     *   directory as a subdirectory 
     */
    GetTempPath(sizeof(systmp), systmp);

    /* keep going until we find a unique name */
    for (;;)
    {
        UINT id;
        
        /* generate a temporary directory name */
        id = (UINT)rand();
        GetTempFileName(systmp, "tads", id, dirbuf);

        /* try creating the directory */
        if (CreateDirectory(dirbuf, 0))
        {
            /* success */
            return 0;
        }

        /* check the error - if it's not "already exists", give up */
        if (GetLastError() != ERROR_ALREADY_EXISTS)
        {
            /* tell the user what's wrong */
            MessageBox(owner_hwnd,
                       "An error occurred creating a temporary "
                       "directory.  Please free up some space on "
                       "your hard disk and try again.", "TADS Workbench",
                       MB_OK | MB_ICONEXCLAMATION);

            /* tell the caller we failed */
            return 1;
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Dialog for waiting for a #include scan to complete.  We show this
 *   dialog modally while running the #include scan, which ensures that the
 *   user interface is disabled until the scan completes.  
 */
class CTadsDlg_scan_includes: public CTadsDialog
{
public:
    /* handle a command */
    virtual int do_command(WPARAM id, HWND ctl)
    {
        switch(id)
        {
        case IDOK:
        case IDCANCEL:
            /* do not dismiss the dialog on regular keystrokes */
            return TRUE;

        case IDC_BUILD_DONE:
            /* 
             *   special message sent from main window when it hears that
             *   the build is done - dismiss the dialog 
             */
            EndDialog(handle_, id);
            return TRUE;

        default:
            /* inherit default processing for anything else */
            return CTadsDialog::do_command(id, ctl);
        }
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   Run the compiler.  We build a list of commands, then we start a
 *   thread to execute the list.  We build the list so that the background
 *   build thread doesn't need to allocate memory, access the
 *   configuration, or otherwise look at any data that aren't thread-safe.
 */
void CHtmlSys_dbgmain::run_compiler(int command_id)
{
    CStringBufCmd cmdline(2048);
    char extres_base_filename[OSFNMAX];
    char exe[OSFNMAX];
    char cmd_dir[OSFNMAX];
    char gamefile[OSFNMAX];
    char optfile[OSFNMAX];
    char buf[1024];
    int i;
    int cnt;
    int intval;
    DWORD thread_id;
    int extres_cnt;
    int cur_extres;
    int force_full = FALSE;
    char tmpdir[OSFNMAX];
    CStringBuf tadslib_var;
    CStringBuf env_var;
    int debug_build;

    /* 
     *   make one last check to make sure we don't already have a build in
     *   progress - we can only handle one at a time 
     */
    if (build_running_)
        return;

    /* determine if we're compiling for debugging */
    debug_build = (command_id == ID_BUILD_COMPDBG
                   || command_id == ID_BUILD_COMPDBG_AND_SCAN
                   || command_id == ID_BUILD_COMPDBG_FULL
                   || command_id == ID_BUILD_COMP_AND_RUN);

    /*
     *   Set up the TADSLIB environment variable for the compiler.  We set
     *   this to the list of library directories, with entries separated by
     *   semicolons.  
     */
    cnt = cnt = global_config_->get_strlist_cnt("srcdir", 0);
    for (i = 0 ; i < cnt ; ++i)
    {
        /* get this element */
        global_config_->getval("srcdir", 0, i, buf, sizeof(buf));

        /* 
         *   if this isn't the first element, add a semicolon to separate it
         *   from the previous element 
         */
        if (i != 0)
            tadslib_var.append_inc(";", 512);

        /* append this element to the path under construction */
        tadslib_var.append_inc(buf, 512);
    }

    /*
     *   If there's a version inherited from our environment, append that
     *   after the paths we've added from our own configuration.  
     */
    if (orig_tadslib_var_.get() != 0 && orig_tadslib_var_.get()[0] != '\0')
    {
        /* 
         *   if we added any paths of our own, add a semicolon to separate
         *   our paths from the TADSLIB paths 
         */
        if (cnt != 0)
            tadslib_var.append_inc(";", 512);

        /* append the original TADSLIB path */
        tadslib_var.append(orig_tadslib_var_.get());
    }

    /* 
     *   Now that we have the properly-formatted list of directory names, set
     *   TADSLIB in our environment, so the compiler child process will
     *   inherit it.  We have no use for the value ourselves, as we've
     *   stashed away a private copy in orig_tadslib_var_, so there's no
     *   reason not to just clobber whatever's in our own environment.  
     */
    SetEnvironmentVariable("TADSLIB", tadslib_var.get());

    /* presume we won't need a temporary directory */
    tmpdir[0] = '\0';

    /* clear the build command list */
    clear_build_cmds();

    /*
     *   Build the name of the game file.  If we're building a debug game,
     *   use the current game name.  If we're building the release game,
     *   use the release game name.  If we're building an executable,
     *   build to a temporary file. 
     */
    switch(command_id)
    {
    case ID_BUILD_COMPDBG:
    case ID_BUILD_COMPDBG_AND_SCAN:
    case ID_BUILD_COMPDBG_FULL:
    case ID_BUILD_COMP_AND_RUN:
    case ID_BUILD_CLEAN:
    case ID_PROJ_SCAN_INCLUDES:
        /* build to the loaded game file */
        strcpy(gamefile, get_gamefile_name());

        /* we're not going to build external resource files */
        extres_base_filename[0] = '\0';

        /* if this is a full build, add the 'force' flag */
        if (command_id == ID_BUILD_COMPDBG_FULL)
            force_full = TRUE;
        break;

    case ID_BUILD_COMPRLS:
        /* preparing a release - force a full build */
        force_full = TRUE;
        
        /* build to the release game file */
        local_config_->getval("build", "rlsgam", 0,
                              gamefile, sizeof(gamefile));

        /* create a temporary directory for our intermediate files */
        if (create_temp_dir(tmpdir, handle_))
            goto cleanup;

        /* base the external resource filenames on the release filename */
        strcpy(extres_base_filename, gamefile);
        break;

    case ID_BUILD_COMPEXE:
    case ID_BUILD_COMPINST:
        /* preparing a release - force a full build */
        force_full = TRUE;

        /* create a temporary directory for our intermediate files */
        if (create_temp_dir(tmpdir, handle_))
            goto cleanup;

        /* 
         *   we're compiling to an executable (this is also the case if
         *   we're building an installer), so we don't really care about
         *   the .gam file; build the game to a temp file 
         */
        GetTempPath(sizeof(buf), buf);
        GetTempFileName(buf, "tads", 0, gamefile);

        /* check to see if we're building an installer */
        if (command_id == ID_BUILD_COMPINST)
        {
            /* 
             *   get the name of the executable - we'll base the external
             *   resource files on this name 
             */
            local_config_->getval("build", "exe", 0, buf, sizeof(buf));

            /* 
             *   put the external resource files into the temp dir, using
             *   the executable's root filename 
             */
            os_build_full_path(extres_base_filename,
                               sizeof(extres_base_filename),
                               tmpdir, os_get_root_name(buf));
        }
        else
        {
            /* 
             *   base the name of the external resource files on the
             *   executable name 
             */
            local_config_->getval("build", "exe", 0, extres_base_filename,
                                  sizeof(extres_base_filename));
        }
        break;
    }

    /* -------------------------------------------------------------------- */
    /*
     *   Step one: run the compiler 
     */

    /* generate the name of the compiler executable */
    GetModuleFileName(0, exe, sizeof(exe));
    strcpy(os_get_root_name(exe), "t3make.exe");

    /* start the command with the executable name */
    cmdline.set("");
    cmdline.append_qu(exe, FALSE);

    /* if we're in "clean" mode, add the "-clean" option */
    if (command_id == ID_BUILD_CLEAN)
        cmdline.append_inc(" -clean", 512);

    /* 
     *   If we're scanning for #include files, add the -Pi option.  If we're
     *   doing a build-plus-scan, do the scan now, since we want to get that
     *   out of the way before we start the full build.  
     */
    if (command_id == ID_PROJ_SCAN_INCLUDES
        || command_id == ID_BUILD_COMPDBG_AND_SCAN)
        cmdline.append_inc(" -Pi", 512);

    /* write intermediate files to our temp dir if we have one */
    if (tmpdir[0] != '\0')
    {
        /* add the symbol file directory option */
        cmdline.append_opt("-Fy", tmpdir);

        /* add the object file directory option */
        cmdline.append_opt("-Fo", tmpdir);
    }
    else
    {
        /* add the symbol file path elements, if any */
        cnt = local_config_->get_strlist_cnt("build", "symfile path");
        for (i = 0 ; i < cnt ; ++i)
        {
            /* get this path */
            local_config_->getval("build", "symfile path",
                                  i, buf, sizeof(buf));

            /* add the -Fy option and the path */
            cmdline.append_opt("-Fy", buf);
        }

        /* add the object file path elements, if any */
        cnt = local_config_->get_strlist_cnt("build", "objfile path");
        for (i = 0 ; i < cnt ; ++i)
        {
            /* get this path */
            local_config_->getval("build", "objfile path",
                                  i, buf, sizeof(buf));

            /* add the -Fy option and the path */
            cmdline.append_opt("-Fo", buf);
        }
    }

    /* add the include list */
    cnt = local_config_->get_strlist_cnt("build", "includes");
    for (i = 0 ; i < cnt ; ++i)
    {
        /* get this include path */
        local_config_->getval("build", "includes", i, buf, sizeof(buf));

        /* add the -i option and the path */
        cmdline.append_opt("-I", buf);
    }

    /* 
     *   add the output image file name option - enclose the filename in
     *   quotes in case it contains spaces 
     */
    cmdline.append_opt("-o", gamefile);

    /* if we need to force a full build, add the -a option */
    if (force_full)
        cmdline.append_inc(" -a", 512);

    /* add the #defines, enclosing each string in quotes */
    cnt = local_config_->get_strlist_cnt("build", "defines");
    for (i = 0 ; i < cnt ; ++i)
    {
        /* get this symbol definition */
        local_config_->getval("build", "defines", i, buf, sizeof(buf));

        /* add the -D option */
        cmdline.append_opt("-D", buf);
    }

    /* 
     *   add the undefs; these don't need quotes, since they must be
     *   simple identifiers 
     */
    cnt = local_config_->get_strlist_cnt("build", "undefs");
    for (i = 0 ; i < cnt ; ++i)
    {
        /* get this symbol */
        local_config_->getval("build", "undefs", i, buf, sizeof(buf));

        /* add the -U option and the symbol to undefine */
        cmdline.append_opt("-U", buf);
    }

    /* add the character set option if required */
    if (!local_config_->getval("build", "use charmap", &intval) && intval
        && !local_config_->getval("build", "charmap", 0, buf, sizeof(buf)))
    {
        /* add the option */
        cmdline.append_opt("-cs", buf);
    }

    /* add the "verbose" option if required */
    if (!local_config_->getval("build", "verbose errors", &intval) && intval)
        cmdline.append_inc(" -v", 512);

    /* add the "-nopre" option if appropriate */
    if (!local_config_->getval("build", "run preinit", &intval) && !intval)
        cmdline.append_inc(" -nopre", 512);

    /* add the "-nodef" option if desired */
    if (!local_config_->getval("build", "keep default modules", &intval)
        && !intval)
        cmdline.append_inc(" -nodef", 512);

    /* 
     *   add the warning level option if required - note that warning level
     *   1 is the default, so we only need to add the warning level if it's
     *   something different 
     */
    if (!local_config_->getval("build", "warning level", &intval)
        && intval != 1)
    {
        char buf[20];

        /* build the option and add it to the command line */
        sprintf(buf, " -w%d", intval);
        cmdline.append_inc(buf, 512);
    }

    /* add the source file search path elements, if any */
    cnt = local_config_->get_strlist_cnt("build", "srcfile path");
    for (i = 0 ; i < cnt ; ++i)
    {
        /* get this source file path */
        local_config_->getval("build", "srcfile path", i, buf, sizeof(buf));

        /* add the -Fy option and the path */
        cmdline.append_opt("-Fs", buf);
    }

    /* add the extra options - just add them exactly as they're entered */
    cnt = local_config_->get_strlist_cnt("build", "extra options");
    for (i = 0 ; i < cnt ; ++i)
    {
        /* get this option */
        local_config_->getval("build", "extra options", i, buf, sizeof(buf));

        /* add a space, then this line of options */
        cmdline.append_inc(" ", 512);
        cmdline.append_inc(buf, 512);
    }

    /* if we're compiling for debugging, add the debug option */
    if (debug_build)
        cmdline.append_inc(" -d", 512);

    /* 
     *   add our special status-message prefix, so we can easily pick out the
     *   status messages from the output stream while we're compiling 
     */
    cmdline.append_inc(" -statprefix <@>", 512);

    /* add the list of source files */
    cnt = local_config_->get_strlist_cnt("build", "source_files");
    for (i = 0 ; i < cnt ; ++i)
    {
        int is_lib;
        
        /* get this source file */
        local_config_->getval("build", "source_files", i, buf, sizeof(buf));

        /* check to see if it's a library file */
        is_lib = is_source_file_a_lib(local_config_, i);

        /* 
         *   if the filename starts with a hyphen, we need to include a type
         *   specifier so that the compiler doesn't confuse the filename for
         *   an option 
         */
        if (buf[0] == '-')
        {
            /* 
             *   add the appropriate specifier, depending on whether it's a
             *   source file or a library file 
             */
            cmdline.append_inc(is_lib ? " -lib" : " -source", 512);
        }

        /* add the filename it to the command line in quotes */
        cmdline.append_qu(buf, TRUE);

        /* if it's a library, add "-x" options as needed */
        if (is_lib)
        {
            char var_name[1024];
            int xcnt;
            int xi;

            /* build the exclusion variable name */
            sprintf(var_name, "lib_exclude:%s", buf);

            /* run through the exclusion list */
            xcnt = local_config_->get_strlist_cnt("build", var_name);
            for (xi = 0 ; xi < xcnt ; ++xi)
            {
                char xbuf[1024];
                
                /* get the item */
                local_config_->getval("build", var_name, xi,
                                      xbuf, sizeof(xbuf));

                /* add a "-x" option for this item */
                cmdline.append_inc(" -x", 512);
                cmdline.append_qu(xbuf, TRUE);
            }
        }
    }

    /* add the list of resource files, if we're not compiling for debugging */
    if (!debug_build)
    {
        int recurse = TRUE;

        /* add the "-res" option */
        cmdline.append_inc(" -res", 512);

        /* scan the resource file list */
        cnt = local_config_->get_strlist_cnt("build", "image_resources");
        for (i = 0 ; i < cnt ; ++i)
        {
            char rbuf[5];
            int new_recurse;

            /* get this resource */
            local_config_->getval("build", "image_resources",
                                  i, buf, sizeof(buf));

            /* note its recursive status */
            local_config_->getval("build", "image_resources_recurse",
                                  i, rbuf, sizeof(rbuf));
            new_recurse = (rbuf[0] == 'Y');

            /* add a -recurse/-norecurse option if we're changing modes */
            if (recurse != new_recurse)
            {
                /* add the option */
                cmdline.append_inc(new_recurse ? " -recurse" : " -norecurse",
                                   512);

                /* note the new mode */
                recurse = new_recurse;
            }

            /* add this resource file */
            cmdline.append_qu(buf, TRUE);
        }
    }

    /* run the compiler in the project directory */
    GetFullPathName(local_config_filename_.get(), sizeof(buf), buf, 0);
    os_get_path_name(cmd_dir, sizeof(cmd_dir), buf);

    /* add the command to the list */
    add_build_cmd("t3make", exe, cmdline.get(), cmd_dir, FALSE);


    /* -------------------------------------------------------------------- */
    /*
     *   Step two: run the resource bundler.  Bundle resources only for
     *   release/exe builds, not for debugging builds.  
     */
    if (command_id == ID_BUILD_COMPDBG
        || command_id == ID_BUILD_COMPDBG_AND_SCAN
        || command_id == ID_BUILD_COMPDBG_FULL
        || command_id == ID_BUILD_COMP_AND_RUN
        || command_id == ID_BUILD_CLEAN
        || command_id == ID_PROJ_SCAN_INCLUDES)
    {
        /* this is a debug build, so we're done */
        goto done;
    }

#if 0 // we don't need t3res for image resources any more
    /* build the name of the bundler */
    strcpy(os_get_root_name(exe), "t3res.exe");

    /* start the command with the executable name */
    cmdline.set("");
    cmdline.append_qu(exe, FALSE);

    /* add the image file name (preceded by a space, and in quotes) */
    cmdline.append_qu(gamefile, TRUE);

    /* set up the options to add multimedia resources */
    cmdline.append_inc(" -add ", 512);

    /* get the count of resource files to go into the image file */
    cnt = local_config_->get_strlist_cnt("build", "image_resources");

    /* add the image file resources */
    for (i = 0 ; i < cnt ; ++i)
    {
        /* get this source file */
        local_config_->getval("build", "image_resources",
                              i, buf, sizeof(buf));

        /* add it to the command line in quotes */
        cmdline.append_qu(buf, TRUE);
    }

    /* add the build command for the image resources */
    add_build_cmd("t3res", exe, cmdline.get(), cmd_dir, FALSE);
#endif

    /* get the number of external resource files */
    if (local_config_->getval("build", "ext resfile cnt", &extres_cnt))
        extres_cnt = 0;

    /* build the external resource files */
    for (cur_extres = 0 ; cur_extres < extres_cnt ; ++cur_extres)
    {
        char config_name[50];

        /* 
         *   check to see if we should include this file, and get its name
         *   if so - if this returns false, it means the file shouldn't be
         *   included, so just skip it in this case 
         */
        if (!build_get_extres_file(buf, cur_extres, command_id,
                                   extres_base_filename))
            continue;

        /* build the name of the bundler */
        strcpy(os_get_root_name(exe), "t3res.exe");

        /* start the command line */
        cmdline.set("");
        cmdline.append_qu(exe, FALSE);

        /* add the resource filename with a -create option */
        cmdline.append_inc(" -create", 512);
        cmdline.append_qu(buf, TRUE);

        /* add the action specifiers */
        cmdline.append_inc(" -add", 512);

        /* generate the name of this external resource file */
        CHtmlDbgProjWin::gen_extres_names(0, config_name, cur_extres);

        /* get the count of resource files to go into the image file */
        cnt = local_config_->get_strlist_cnt("build", config_name);

        /* add the image file resources */
        for (i = 0 ; i < cnt ; ++i)
        {
            /* get this source file */
            local_config_->getval("build", config_name, i, buf, sizeof(buf));
            
            /* add it to the command line in quotes */
            cmdline.append_qu(buf, TRUE);
        }

        /* add the build command */
        add_build_cmd("t3res", exe, cmdline.get(), cmd_dir, FALSE);
    }

    /* -------------------------------------------------------------------- */
    /*
     *   If we're building an executable, run the executable bundler 
     */
    if (command_id == ID_BUILD_COMPINST)
    {
        /* building the installer - skip the executable build step */
        goto build_installer;
    }
    else if (command_id != ID_BUILD_COMPEXE)
    {
        /* we're not building an executable, so we're done */
        goto done;
    }

    /* build the name of the executable builder */
    strcpy(os_get_root_name(exe), "maketrx32.exe");

    /* start the command with the executable name */
    cmdline.set("");
    cmdline.append_qu(exe, FALSE);

    /* set the "-t3" option */
    cmdline.append_qu("-t3", TRUE);

    /* specify the source executable (in quotes) */
    GetModuleFileName(0, buf, sizeof(buf));
    strcpy(os_get_root_name(buf), "htmlt3.exe");
    cmdline.append_qu(buf, TRUE);

    /* add the game name */
    cmdline.append_qu(gamefile, TRUE);

    /* add the executable filename in quotes */
    local_config_->getval("build", "exe", 0, buf, sizeof(buf));
    cmdline.append_qu(buf, TRUE);

    /* run the command */
    add_build_cmd("maketrx32", exe, cmdline.get(), cmd_dir, FALSE);

    /* add a command to delete the temporary image file */
    add_build_cmd("del", "del", gamefile, "", TRUE);


    /* -------------------------------------------------------------------- */
    /*
     *   If we're building the installer, run the install maker
     */
build_installer:
    if (command_id != ID_BUILD_COMPINST)
    {
        /* not building the installer - we're done */
        goto done;
    }

    /* build the name of the installer maker */
    strcpy(os_get_root_name(exe), "mksetup.exe");

    /* start the command with the executable name */
    cmdline.set("");
    cmdline.append_qu(exe, FALSE);

    /* 
     *   add the -detached option - this causes the installer creator to
     *   run its own subprocesses as detached, so that we get the results
     *   piped back to us through the standard handles that we create 
     */
    cmdline.append_inc(" -detached", 512);

    /* create a temporary options file name */
    GetTempPath(sizeof(buf), buf);
    GetTempFileName(buf, "tads", 0, optfile);
    
    /* add the options file - this is a temp file */
    cmdline.append_qu(optfile, TRUE);

    /* add the name of the resulting installation program */
    local_config_->getval("build", "installer prog", 0, buf, sizeof(buf));
    cmdline.append_qu(buf, TRUE);

    /* add the command */
    add_build_cmd("mksetup", exe, cmdline.get(), cmd_dir, FALSE);

    /* add a build command to delete the temporary options file */
    add_build_cmd("del", "del", optfile, "", TRUE);

    /* add a build command to delete the temporary game file */
    add_build_cmd("del", "del", gamefile, "", TRUE);

    /* create the options file */
    {
        FILE *fp;
        int cnt;
        int i;
        int found_name;
        int found_savext;
        int found_gam;
        int found_exe;

        /* we haven't found our special values yet */
        found_name = FALSE;
        found_savext = FALSE;
        found_gam = FALSE;
        found_exe = FALSE;

        /* open the file */
        fp = fopen(optfile, "w");

        /* always include the "T3" option */
        fprintf(fp, "T3=1\n");

        /* write the options lines */
        cnt = local_config_->get_strlist_cnt("build", "installer options");
        for (i = 0 ; i < cnt ; ++i)
        {
            /* get this string */
            local_config_->getval("build", "installer options",
                            i, buf, sizeof(buf));

            /* if this is one of the required ones, note its presence */
            if (strlen(buf) > 5 && memicmp(buf, "name=", 5) == 0)
                found_name = TRUE;
            else if (strlen(buf) > 7 && memicmp(buf, "savext=", 7) == 0)
                found_savext = TRUE;
            else if (strlen(buf) > 4 && memicmp(buf, "gam=", 4) == 0)
                found_gam = TRUE;
            else if (strlen(buf) > 4 && memicmp(buf, "exe=", 4) == 0)
                found_exe = TRUE;

            /* write it out */
            fprintf(fp, "%s\n", buf);
        }

        /* write out the "GAM" line if we didn't find an explicit one */
        if (!found_gam)
            fprintf(fp, "GAM=%s\n", gamefile);

        /* write out the "EXE" line if we didn't find an explicit one */
        if (!found_exe)
        {
            /* use the normal build .EXE filename here, sans path */
            local_config_->getval("build", "exe", 0, buf, sizeof(buf));
            fprintf(fp, "EXE=%s\n", os_get_root_name(buf));
        }

        /* add the external resources we're including */
        for (cur_extres = 0 ; cur_extres < extres_cnt ; ++cur_extres)
        {
            /* 
             *   check to see if we should include this file, and get its
             *   name if so - if this returns false, it means the file
             *   shouldn't be included, so just skip it in this case 
             */
            if (!build_get_extres_file(buf, cur_extres, command_id,
                                       extres_base_filename))
                continue;

            /* add this file to the installer options */
            fprintf(fp, "FILE=%s\n", buf);
        }

        /* we've finished writing the options file */
        fclose(fp);

        /* if we didn't find the NAME or SAVEXT settings, it's an error */
        if (!found_name || !found_savext)
        {
            /* show the message */
            MessageBox(handle_, "Before you can build an installer, you "
                       "must fill in the Name and Save Extension options "
                       "in the installer configuration.  Please fill in "
                       "these values, then use Compile Installer again "
                       "when you're done.", "TADS Workbench",
                       MB_OK | MB_ICONEXCLAMATION);

            /* take them to the dialog */
            run_dbg_build_dlg(handle_, this, DLG_BUILD_INSTALL, 0);

            /* 
             *   delete the temporary files (for the options file and for
             *   the game), since we won't actually be carrying out the
             *   build commands, which would ordinarily clean up the temp
             *   files 
             */
            DeleteFile(optfile);
            DeleteFile(gamefile);

            /* delete our temporary directory */
            del_temp_dir(tmpdir);

            /* return without starting a build */
            goto cleanup;
        }
    }

done:
    /* -------------------------------------------------------------------- */
    /*
     *   We've built the list of commands, so we can now start the build
     *   thread to carry out the command list 
     */

    /* if we have a temporary directory, add a command to delete it */
    if (tmpdir[0] != '\0')
        add_build_cmd("rmdir/s", "rmdir/s", tmpdir, "", TRUE);
    
    /* note that we have a compile running */
    build_running_ = TRUE;

    /* remember the build mode */
    build_cmd_ = command_id;

    /* 
     *   run all build output through our special processing, but assume for
     *   now that we won't capture all of the output 
     */
    filter_build_output_ = TRUE;
    capture_build_output_ = FALSE;

    /* if we're scanning for include files, set up capturing */
    if (command_id == ID_PROJ_SCAN_INCLUDES
        || command_id == ID_BUILD_COMPDBG_AND_SCAN)
    {
        /* capture the output from the build */
        capture_build_output_ = TRUE;
    }

    /* if we're meant to run after the build, so note */
    run_after_build_ = (command_id == ID_BUILD_COMP_AND_RUN);

    /* if we're scanning for include files, clear out the existing files */
    if (command_id == ID_PROJ_SCAN_INCLUDES && projwin_ != 0)
        projwin_->clear_includes();

    /* start the compiler thread */
    build_thread_ = CreateThread(0, 0,
                                 (LPTHREAD_START_ROUTINE)&run_compiler_entry,
                                 this, 0, &thread_id);

    /* if we couldn't start the thread, show an error */
    if (build_thread_ == 0)
    {
        /* note the problem */
        oshtml_dbg_printf("Build failure: unable to start build thread\n");

        /* we don't have a build running after all */
        build_running_ = FALSE;
    }

    /* 
     *   if we started the build successfully, and we're doing a #include
     *   scan, show a modal dialog until the scan completes 
     */
    if (build_running_
        && (command_id == ID_PROJ_SCAN_INCLUDES
            || command_id == ID_BUILD_COMPDBG_AND_SCAN))
    {
        /* create the dialog */
        build_wait_dlg_ = new CTadsDlg_scan_includes();

        /* display the dialog */
        build_wait_dlg_->run_modal(DLG_SCAN_INCLUDES, handle_,
                                   CTadsApp::get_app()->get_instance());

        /* done with the dialog */
        delete build_wait_dlg_;
        build_wait_dlg_ = 0;

        /*
         *   If we were doing a build-plus-scan operation, start the build
         *   now that we're done with the include scan.  We do the include
         *   scan first because it's the modal operation; once we kick off
         *   the build, the user interface allows most operations while the
         *   build proceeds in the background.  
         */
        if (command_id == ID_BUILD_COMPDBG_AND_SCAN)
            PostMessage(handle_, WM_COMMAND, ID_BUILD_COMPDBG, 0);
    }

cleanup:
    ;
}

/* ------------------------------------------------------------------------ */
/*
 *   Check to determine if it's okay to compile 
 */
int CHtmlSys_dbgmain::vsn_ok_to_compile()
{
    /* make sure the project window has saved its configuration */
    if (projwin_ != 0)
        projwin_->save_config(local_config_);
    
    /* ensure we have at least one source file listed */
    if (local_config_->get_strlist_cnt("build", "source_files") == 0)
    {
        /* 
         *   make sure the project window is visible before showing the
         *   error message, since the message refers to the project 
         */
        SendMessage(get_handle(), WM_COMMAND, ID_VIEW_PROJECT, 0);

        /* tell them what's wrong */
        MessageBox(handle_, "This project has no source files.  Before "
                   "you can compile, you must add at least one "
                   "file to the project's \"Source Files\" group.",
                   "TADS Workbench", MB_OK | MB_ICONEXCLAMATION);

        /* 
         *   make sure the project window is the active window now that
         *   we've removed the error message dialog 
         */
        SendMessage(get_handle(), WM_COMMAND, ID_VIEW_PROJECT, 0);
        
        /* 
         *   don't proceed with compilation right now - wait for the user
         *   to retry explicitly 
         */
        return FALSE;
    }

    /* we can proceed with the compilation */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the name of an external resource file, and check to see if it
 *   should be included in the build 
 */
int CHtmlSys_dbgmain::build_get_extres_file(char *buf, int cur_extres,
                                            int build_command,
                                            const char *extres_base_name)  
{
    char ext[32];

    /* 
     *   if we're building an installer, check to see if this file is
     *   included in the installer, and skip it if not 
     */
    if (build_command == ID_BUILD_COMPINST)
    {
        char in_inst[10];
        
        /* get the in-install status - make it yes by default */
        if (local_config_->getval("build", "ext_resfile_in_install",
                                  cur_extres, in_inst, sizeof(in_inst)))
            in_inst[0] = 'Y';
        
        /* if this one isn't in the install, tell the caller to skip it */
        if (in_inst[0] != 'Y')
            return FALSE;
    }
    
    /* build the name of this external resource file */
    sprintf(ext, "3r%d", cur_extres);
    strcpy(buf, extres_base_name);
    os_remext(buf);
    os_addext(buf, ext);

    /* include the file */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Create and load a new game 
 */
void CHtmlSys_dbgmain::create_new_game(const textchar_t *srcfile,
                                       const textchar_t *gamefile,
                                       int create_source,
                                       w32tdb_tpl_t template_type,
                                       const textchar_t *custom_template,
                                       const CHtmlBiblio *biblio)
{
    textchar_t new_config[OSFNMAX];
    char msg[512];
    char fbuf[OSFNMAX];
    char proj_path[OSFNMAX];
    char srcfile_rel[OSFNMAX];
    int file_idx;
    int defs_idx;
    char obj_path[OSFNMAX];
    char dbg_path[OSFNMAX];

    /* first, save the current configuration */
    if (local_config_filename_.get() != 0)
    {
        /* save the current configuration into the configuration object */
        save_config();

        /* write the configuration object to its file */
        vsn_save_config_file(local_config_, local_config_filename_.get());
    }

    /* 
     *   build the name of the new configuration file - use a standard T3
     *   makefile (.t3m) to store the configuration, so that the file can be
     *   shared with the command-line compiler 
     */
    strcpy(new_config, gamefile);
    os_remext(new_config);
    os_addext(new_config, "t3m");

    /* note the path name to the project file */
    os_get_path_name(proj_path, sizeof(proj_path), new_config);

    /* 
     *   before we do anything we might regret, make sure we can save the
     *   configuration file 
     */
    if (vsn_save_config_file(local_config_, new_config))
    {
        /* tell them what went wrong */
        sprintf(msg, "An error occurred creating the new game's "
                "configuration file (%s).  Please make sure the folder "
                "is correct and the disk isn't full.", new_config);
        MessageBox(handle_, msg, "TADS Workbench",
                   MB_ICONEXCLAMATION | MB_OK);

        /* stop now with no change to the loaded configuration */
        return;
    }

    /* if desired, create the source file from a starter file */
    if (create_source)
    {
        /* 
         *   use the custom template if provided; otherwise, use one of
         *   the standard templates 
         */
        if (custom_template != 0)
        {
            /* use the specified custom template */
            strcpy(fbuf, custom_template);
        }
        else
        {
            const char *basename;

            /* 
             *   get the path to the appropriate starter file - it's in the
             *   "samples" subdirectory of the main install directory, but
             *   the actual file depends on which type of template the user
             *   has selected 
             */
            switch (template_type)
            {
            case W32TDB_TPL_INTRO:
            default:
                basename = "samples\\startI3.t";
                break;

            case W32TDB_TPL_ADVANCED:
                basename = "samples\\startA3.t";
                break;

            case W32TDB_TPL_NOLIB:
                basename = "samples\\startB3.t";
                break;
            }

            /* build the full path */
            GetModuleFileName(0, fbuf, sizeof(fbuf));
            strcpy(os_get_root_name(fbuf), basename);
        }

        /* copy the file */
        if (!copy_insert_fields(fbuf, srcfile, biblio))
        {
            sprintf(msg, "An error occurred creating the new game's "
                    "source file (%s).  Please make sure the folder "
                    "is correct and the disk isn't full.", srcfile);
            MessageBox(handle_, msg, "TADS Workbench",
                       MB_ICONEXCLAMATION | MB_OK);

            /* abort */
            return;
        }
    }

    /*
     *   Forget the old configuration file, since we're about to start
     *   modifying the configuration object for the new configuration.
     *   We've already saved the old file, so we don't need to know about
     *   it any more, and we want to be sure we don't try to save the new
     *   configuration to the old file.  For now, set an empty
     *   configuration; this will ensure that we don't save the
     *   configuration at any point (since it's empty), and that we load a
     *   new configuration (since the new config filename will be
     *   different when we load the game than the current config
     *   filename).  
     */
    local_config_filename_.set(0, 0);

    /* no files in our list yet */
    file_idx = 0;

    /* no -D defines in our list yet */
    defs_idx = 0;

    /* 
     *   Set up the new configuration, based on the existing
     *   configuration.  First, clear out all of the information that
     *   isn't suitable for a default configuration, so that we carry
     *   forward only the information from this configuration that we'd
     *   apply to the default config.  
     */
    clear_non_default_config();

    /* set up the default build configuration */
    set_dbg_build_defaults(this, gamefile);

    /* clear out our internal source file list */
    helper_->delete_internal_sources();

    /* 
     *   create the debug output directory - create it as a Debug
     *   subdirectory of the project directory
     */
    os_build_full_path(dbg_path, sizeof(dbg_path), proj_path, "debug");
    if (CreateDirectory(dbg_path, 0))
    {
        /* put the image file in this directory */
        os_build_full_path(fbuf, sizeof(fbuf), "debug",
                           os_get_root_name((textchar_t *)gamefile));
    }
    else
    {
        /* failed - use the release game name plus "_dbg" */
        strcpy(fbuf, os_get_root_name((textchar_t *)gamefile));

        /* remove the extension, add "_dbg", and put the extension back */
        os_remext(fbuf);
        strcat(fbuf, "_dbg");
        os_defext(fbuf, "t3");
    }

    /* set the debug image file according to what we just decided */
    local_config_->setval("build", "debug_image_file", 0, fbuf);

    /* 
     *   save the base game filename as the release path; just use the base
     *   filename, since we want to put it by default in the project
     *   directory 
     */
    local_config_->setval("build", "rlsgam", 0,
                          os_get_root_name((textchar_t *)gamefile));

    /* 
     *   create the object file directory - create it as a .\OBJ subdirectory
     *   of the project directory 
     */
    os_build_full_path(obj_path, sizeof(obj_path), proj_path, "obj");
    CreateDirectory(obj_path, 0);

    /* if the .\OBJ directory doesn't exist, use the base path */
    if (osfacc(obj_path))
        strcpy(obj_path, proj_path);

    /* set the symbol and object file output directories */
    local_config_->setval("build", "symfile path", 0, obj_path);
    local_config_->setval("build", "objfile path", 0, obj_path);

    /* 
     *   add the system libraries, if the user selected a template that
     *   includes them 
     */
    switch (template_type)
    {
    case W32TDB_TPL_INTRO:
    case W32TDB_TPL_ADVANCED:
    case W32TDB_TPL_CUSTOM:
        /* add system.tl */
        local_config_->setval("build", "source_files", file_idx, "system.tl");
        local_config_->setval(
            "build", "source_file_types", file_idx, "library");
        local_config_->setval(
            "build", "source_files-sys", file_idx, "system");
        helper_->add_internal_line_source(
            "system.tl", "system.tl", file_idx++);

        /* add adv3.tl */
        local_config_->setval(
            "build", "source_files", file_idx, "adv3\\adv3.tl");
        local_config_->setval(
            "build", "source_file_types", file_idx, "library");
        local_config_->setval(
            "build", "source_files-sys", file_idx, "system");
        helper_->add_internal_line_source("adv3.tl", "adv3.tl", file_idx++);

        /* add the GameInfo.txt resource */
        local_config_->setval("build", "image_resources", 0, "GameInfo.txt");

        /* add the language-related -D defines */
        local_config_->setval(
            "build", "defines", defs_idx++, "LANGUAGE=en_us");
        local_config_->setval(
            "build", "defines", defs_idx++, "MESSAGESTYLE=neu");

        /* done */
        break;

    case W32TDB_TPL_NOLIB:
        /* don't include any system libraries with this template */
        break;
    }
    
    /*
     *   add the main source file, using a path relative to the project
     *   file's directory 
     */
    oss_get_rel_path(srcfile_rel, sizeof(srcfile_rel), srcfile, proj_path);
    local_config_->setval("build", "source_files", file_idx, srcfile_rel);
    local_config_->setval("build", "source_file_types", file_idx, "source");
    local_config_->setval("build", "source_files-sys", file_idx, "user");
    helper_->add_internal_line_source(
        os_get_root_name((textchar_t *)srcfile), srcfile, file_idx++);

    /* ask the helper to save the new source list into our config object */
    helper_->save_internal_source_config(local_config_);

    /* set some additional options */
    local_config_->setval("build", "verbose errors", TRUE);

    /* save the new configuration */
    vsn_save_config_file(local_config_, new_config);

    /*
     *   Set up the event loop flags so that we terminate the current
     *   event loop and load the new game 
     */
    reloading_new_ = TRUE;
    restarting_ = TRUE;
    reloading_ = TRUE;
    reloading_go_ = FALSE;
    reloading_exec_ = FALSE;

    /* tell the main loop to give up control so that we load the new game */
    go_ = TRUE;

    /* 
     *   Set the new game to load.  Since we're only loading the project, not
     *   actually running the game yet, use the project filename as the
     *   pending file to load.  
     */
    if (CHtmlSys_mainwin::get_main_win() != 0)
        CHtmlSys_mainwin::get_main_win()->set_pending_new_game(new_config);

    /*
     *   If we created the source file, post a Compile-and-Scan-Includes
     *   command, so that we compile the game as soon as we finish loading
     *   it, and then populate the include file list with the files actually
     *   #include'd after we're done with the compilation.  (The game should
     *   be fully loaded by the time we get around to processing events
     *   again.)
     *   
     *   Don't compile if we didn't create the source file, since we're not
     *   sure it's in any condition to compile at the moment.  However, do
     *   scan for #include's.  
     */
    if (create_source)
        PostMessage(handle_, WM_COMMAND, ID_BUILD_COMPDBG_AND_SCAN, 0);
    else
        PostMessage(handle_, WM_COMMAND, ID_PROJ_SCAN_INCLUDES, 0);

    /*
     *   Post an Open File command to open the first source file, which is
     *   the main game source file. 
     */
    PostMessage(handle_, WM_COMMAND, ID_FILE_OPENLINESRC_FIRST, 0);
}

/* ------------------------------------------------------------------------ */
/*
 *   Clear non-default configuration data 
 */
void CHtmlSys_dbgmain::vsn_clear_non_default_config()
{
    /* clear all project information */
    CHtmlDbgProjWin::clear_proj_config(local_config_);

    /* reset the project window */
    if (projwin_ != 0)
        projwin_->reset_proj_win();
}

/*
 *   Set version-specific build defaults.  In T3 workbench, this does
 *   nothing, since we can never load an empty configuration.  
 */
void vsn_set_dbg_build_defaults(CHtmlDebugConfig *config,
                                CHtmlSys_dbgmain *dbgmain,
                                const textchar_t *gamefile)
{
}

/* ------------------------------------------------------------------------ */
/*
 *   Program arguments dialog 
 */

class CTadsDlg_args: public CTadsDialog
{
public:
    CTadsDlg_args(CHtmlSys_dbgmain *dbgmain)
    {
        /* remember the debugger object */
        dbgmain_ = dbgmain;
    }
    
    /* initialize */
    void init();

    /* execute a command */
    int do_command(WPARAM id, HWND ctl);

protected:
    /* save our configuration settings */
    void save_config();
    
    /* debugger main window */
    CHtmlSys_dbgmain *dbgmain_;
};

/*
 *   initialize 
 */
void CTadsDlg_args::init()
{
    CHtmlDebugConfig *config;
    char buf[1024];

    /* get the local configuration object */
    config = dbgmain_->get_local_config();

    /* retrieve our command file setting and initialize the field */
    if (!config->getval("prog_args", "cmdfile", 0, buf, sizeof(buf)))
        SetWindowText(GetDlgItem(handle_, IDC_FLD_CMDFILE), buf);
    
    /* retrieve our log file setting and initialize the field */
    if (!config->getval("prog_args", "logfile", 0, buf, sizeof(buf)))
        SetWindowText(GetDlgItem(handle_, IDC_FLD_LOGFILE), buf);

    /* retrieve our arguments setting and initialize the field */
    if (!config->getval("prog_args", "args", 0, buf, sizeof(buf)))
        SetWindowText(GetDlgItem(handle_, IDC_FLD_PROGARGS), buf);

    /* inherit default */
    CTadsDialog::init();
}

/*
 *   save our configuration settings 
 */
void CTadsDlg_args::save_config()
{
    CHtmlDebugConfig *config;
    char buf[1024];

    /* get the local configuration object */
    config = dbgmain_->get_local_config();

    /* save the command file setting */
    GetWindowText(GetDlgItem(handle_, IDC_FLD_CMDFILE), buf, sizeof(buf));
    config->setval("prog_args", "cmdfile", 0, buf);

    /* save the log file setting */
    GetWindowText(GetDlgItem(handle_, IDC_FLD_LOGFILE), buf, sizeof(buf));
    config->setval("prog_args", "logfile", 0, buf);

    /* save the program arguments setting */
    GetWindowText(GetDlgItem(handle_, IDC_FLD_PROGARGS), buf, sizeof(buf));
    config->setval("prog_args", "args", 0, buf);
}

/*
 *   process a command 
 */
int CTadsDlg_args::do_command(WPARAM id, HWND ctl)
{
    OPENFILENAME info;
    char buf[1024];
    char fname[MAX_PATH];
    int result;
    HWND file_fld;
    int file_read;
    char *title;
    char path[MAX_PATH];

    /* see what we have */
    switch(id)
    {
    case IDC_BTN_SEL_CMDFILE:
        file_fld = GetDlgItem(handle_, IDC_FLD_CMDFILE);
        file_read = TRUE;
        title = "Select a command input file to read";
        goto ask_for_file;
        
    case IDC_BTN_SEL_LOGFILE:
        file_fld = GetDlgItem(handle_, IDC_FLD_LOGFILE);
        file_read = FALSE;
        title = "Select a log file for capturing output";

    ask_for_file:
        /* set up the open-file structure */
        info.lStructSize = sizeof(info);
        info.hwndOwner = handle_;
        info.hInstance = CTadsApp::get_app()->get_instance();
        info.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
        info.lpstrCustomFilter = 0;
        info.nFilterIndex = 0;
        info.lpstrFile = buf;
        info.nMaxFile = sizeof(buf);
        info.lpstrFileTitle = 0;
        info.nMaxFileTitle = 0;
        info.lpstrTitle = title;
        info.Flags = OFN_NOCHANGEDIR;
        info.nFileOffset = 0;
        info.nFileExtension = 0;
        info.lpstrDefExt = 0;
        info.lCustData = 0;
        info.lpfnHook = 0;
        info.lpTemplateName = 0;
        CTadsDialog::set_filedlg_center_hook(&info);

        /* get the original filename from the field */
        GetWindowText(file_fld, buf, sizeof(buf));

        /* 
         *   get the path to the game file, so that we can make our
         *   filename relative to this path 
         */
        os_get_path_name(path, sizeof(path),
                         dbgmain_->get_gamefile_name());

        /* 
         *   if the filename stored here is relative, build the full path
         *   relative to the directory containing the current image file 
         */
        if (!os_is_file_absolute(buf))
        {
            /* build the full path to the file */
            os_build_full_path(fname, sizeof(fname), path, buf);

            /* copy the result back to the dialog's filename buffer */
            strcpy(buf, fname);
        }

        /* start off in the directory containing the file */
        info.lpstrInitialDir = 0;

        /* ask for the file using the appropriate dialog */
        if (file_read)
            result = GetOpenFileName(&info);
        else
            result = GetSaveFileName(&info);

        /* if it succeeded, store the result */
        if (result != 0)
        {
            
            /* 
             *   translate the filename back to relative form, relative to
             *   the directory containing the game file 
             */
            oss_get_rel_path(fname, sizeof(fname), buf, path);
            
            /* copy the new filename back into the text field */
            SetWindowText(file_fld, fname);

            /* update the open-file directory */
            CTadsApp::get_app()->set_openfile_dir(buf);
        }

        /* handled */
        return TRUE;

    case IDOK:
        /* save the settings */
        save_config();
        
        /* proceed to default processing */
        break;

    default:
        /* use default processing */
        break;
    }

    /* inherit default processing */
    return CTadsDialog::do_command(id, ctl);
}

/*
 *   run the program argument dialog 
 */
void CHtmlSys_dbgmain::vsn_run_dbg_args_dlg()
{
    CTadsDlg_args *dlg;

    /* create the dialog */
    dlg = new CTadsDlg_args(this);

    /* run the dialog */
    dlg->run_modal(DLG_PROGARGS, get_handle(),
                   CTadsApp::get_app()->get_instance());

    /* delete the dialog */
    delete dlg;
}

/* ------------------------------------------------------------------------ */
/*
 *   Process captured build output.
 */
int CHtmlSys_dbgmain::vsn_process_build_output(CStringBuf *txt)
{
    /* check the build mode */
    switch (build_cmd_)
    {
    case ID_PROJ_SCAN_INCLUDES:
    case ID_BUILD_COMPDBG_AND_SCAN:
        /*
         *   We're scanning include files - if this is a #include line, send
         *   it to the project window for processing.  (Ignore anything that
         *   doesn't start with #include, since the include file lines all
         *   start with this sequence.)  
         */
        if (strlen(txt->get()) > 9 && memcmp(txt->get(), "#include ", 9) == 0)
            projwin_->add_scanned_include(txt->get() + 9);

        /* suppress all output while scanning includes */
        return FALSE;

    default:
        /* check for status messages */
        if (strlen(txt->get()) > 3 && memcmp(txt->get(), "<@>", 3) == 0)
        {
            /* build a status line message based on the status */
            compile_status_.set("Build in Progress: ");
            compile_status_.append(txt->get() + 3);

            /* update the status line */
            if (statusline_ != 0)
                statusline_->update();

            /* 
             *   if we have a statusline, suppress the output from the main
             *   buffer; otherwise, show it 
             */
            if (show_statusline_)
            {
                /* we have a statusline - no need to show the output */
                return FALSE;
            }
            else
            {
                CStringBuf newtxt;

                /* 
                 *   there's no statusline - show the output in the log
                 *   window, with the "<@>" turned into a tab 
                 */
                newtxt.set("\t");
                newtxt.append(txt->get() + 3);
                txt->set(newtxt.get());
                return TRUE;
            }
        }

        /* show all other text */
        return TRUE;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   High-precision timer, for the profiler 
 */

/* get the current high-precision timer time */
void os_prof_curtime(vm_prof_time *t)
{
    LARGE_INTEGER i;

    /* get the current time on the Windows high-precision system clock */
    QueryPerformanceCounter(&i);

    /* set the return value */
    t->hi = i.HighPart;
    t->lo = i.LowPart;
}

/* convert a high-precision timer value to microseconds */
unsigned long os_prof_time_to_ms(const vm_prof_time *t)
{
    LARGE_INTEGER freq;

    /* get the system high-precision timer frequency, in ticks per second */
    if (QueryPerformanceFrequency(&freq))
    {
        LARGE_INTEGER tl;

        /* 
         *   We have the frequency in ticks per second.  We want the time in
         *   microseconds.  To start with, we note that since freq is the
         *   number of ticks per second, freq/1e6 is the number of ticks per
         *   microsecond, and that t is expressed in ticks; so, t divided by
         *   (freq/1e6) would give us ticks divided by ticks-per-microsecond,
         *   which gives us the number of microseconds.  This calculation is
         *   equivalent to t*1e6/freq - do the calculation in this order to
         *   retain the full precision.  (Note that even though t times a
         *   million could be quite large, it's not likely to overflow our
         *   64-bit type.)  
         */

        /* get the input value as a long-long for easy arithmetic */
        tl.LowPart = t->lo;
        tl.HighPart = t->hi;

        /* compute the quotient and return the result */
        return (unsigned long)(tl.QuadPart * (ULONGLONG)1000000
                               / freq.QuadPart);
    }
    else
    {
        /* no high-precision clock is available; return zero */
        return 0;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Validate a project file 
 */
int CHtmlSys_dbgmain::vsn_validate_project_file(const textchar_t *fname)
{
    const textchar_t *ext;

    /* scan for the filename extension - it starts at the last '.' */
    ext = strrchr(fname, '.');

    /* if the extension is ".t" or ".t3", reject it */
    if (ext != 0 && (stricmp(ext, ".t") == 0 || stricmp(ext, ".t3") == 0))
    {
        /* can't load source or game - must load project */
        MessageBox(handle_, "To open a new project, you must open the "
                   "project's makefile, which usually has a name ending "
                   "in \".t3m\".  If you don't have a makefile, use "
                   "the \"New Project\" command to create one, then "
                   "add your source files to the project.",
                   "TADS Workbench", MB_OK);

        /* fully handled */
        return TRUE;
    }

    /* we didn't handle it; let the main loader proceed */
    return FALSE;
}

/*
 *   Determine if a project file is valid for loading 
 */
int CHtmlSys_dbgmain::vsn_is_valid_proj_file(const textchar_t *fname)
{
    const textchar_t *ext;

    /* in tads 3 workbench, we can't load .t or .t3 files */
    ext = strrchr(fname, '.');
    return !(ext != 0
             && (stricmp(ext, ".t") == 0 || stricmp(ext, ".t3") == 0));
}

/*
 *   Get the "load profile" for the given Load Game filename.  This fills in
 *   *game_filename with a pointer to the proper buffer for the name of the
 *   game file to load, and fills in config_filename with the name of the
 *   configuration file to load.  Returns true to proceed, false to abort the
 *   load.  All buffers are OSFNMAX bytes in size.
 */
int CHtmlSys_dbgmain::vsn_get_load_profile(char **game_filename,
                                           char *game_filename_buf,
                                           char *config_filename,
                                           const char *load_filename,
                                           int game_loaded)
{
    /* 
     *   in t3 workbench, we always load a project before we can start
     *   running the game; so if 'game_loaded' is true, it means we're
     *   running a game from a configuration we've already loaded, so there's
     *   no need to load the configuration again 
     */
    if (game_loaded)
        return FALSE;

    /* 
     *   If it's not a .t3m file, don't load it - in t3 workbench, we only
     *   load configurations as t3 files.  Due to quirks in the old tads2
     *   workbench initialization and re-initialization event handling, we
     *   can sometimes try to load the current *game* before and after
     *   running it.  Simply ignore such cases, since we can safely assume
     *   that we're just transitioning in and out of an active debug session,
     *   but the loaded configuration won't be changing. 
     */
    if (load_filename != 0)
    {
        const char *ext = strrchr(load_filename, '.');
        if (stricmp(ext, ".t3m") != 0)
            return FALSE;
    }

    /* 
     *   in t3 workbench, we won't know the game filename until after loading
     *   the configuration, since it's stored there
     */
    *game_filename = 0;

    /* 
     *   if we already have the same project file loaded, there's no need to
     *   load the same game again - just tell the caller to do nothing 
     */
    if (local_config_ != 0
        && local_config_filename_.get() != 0
        && load_filename != 0
        && get_strcmp(local_config_filename_.get(), load_filename) == 0)
        return FALSE;

    /* 
     *   if the game hasn't been loaded yet, set the open-file dialog's
     *   default directory to the project directory, so that we start in the
     *   project directory when looking for files to open 
     */
    if (!game_loaded && load_filename != 0)
        CTadsApp::get_app()->set_openfile_dir(load_filename);

    /* the project file *is* the config file */
    if (load_filename != 0)
        strcpy(config_filename, load_filename);

    /* tell the caller to proceed with the loading */
    return TRUE;
}

/*
 *   Get the game filename from the load profile.  In T3 workbench, we obtain
 *   the game filename from the project configuration file.  
 */
int CHtmlSys_dbgmain::vsn_get_load_profile_gamefile(char *buf)
{
    /* try the configuration string "build:debug_image_file" first */
    if (!local_config_->getval("build", "debug_image_file",
                               0, buf, OSFNMAX))
        return TRUE;

    /* 
     *   didn't find it; use the project name, with "_dbg" added to the root
     *   filename and the extension changed to ".t3" 
     */
    strcpy(buf, local_config_filename_.get());
    os_remext(buf);
    strcat(buf, "_dbg");
    os_defext(buf, "t3");

    /* save the name */
    local_config_->setval("build", "debug_image_file", 0, buf);

    /* tell the caller we found a name */
    return TRUE;
}

/*
 *   Set the main window title.  Set the title to the loaded project file
 *   name.  
 */
void CHtmlSys_dbgmain::vsn_set_win_title()
{
    char title[OSFNMAX + 100];

    /* build the text */
    if (workspace_open_)
        sprintf(title, "%s - TADS Workbench",
                os_get_root_name((char *)local_config_filename_.get()));
    else
        strcpy(title, "TADS Workbench");

    /* set the label */
    SetWindowText(handle_, title);
}

/*
 *   Update the game file name to reflect a change in the preferences.  
 */
void CHtmlSys_dbgmain::vsn_change_pref_gamefile_name(textchar_t *fname)
{
    char pathbuf[OSFNMAX];
    char relbuf[OSFNMAX];

    /* store the new game file name in the UI */
    game_filename_.set(fname);

    /* in the preferences, store this path relative to the project path */
    os_get_path_name(pathbuf, sizeof(pathbuf), get_local_config_filename());
    oss_get_rel_path(relbuf, sizeof(relbuf), fname, pathbuf);

    /* save the change in the configuration data as well */
    local_config_->setval("build", "debug_image_file", 0, relbuf);
}

/*
 *   Adjust an argv argument (a program startup argument) to the proper
 *   filename extension for initial loading.
 *   
 *   For the tads 3 workbench, we always load from project (.t3m) files.
 *   However, the user can also load from Windows Explorer by right-clicking
 *   on a .t3 file and selecting "debug" from the context menu, or even by
 *   typing in a command line directly via the Start/Run dialog or a DOS
 *   prompt.  So, if we have a .t3 file as the argument, change it to use a
 *.  t3m extension, and hope for the best.  
 */
void CHtmlSys_dbgmain::vsn_adjust_argv_ext(textchar_t *fname)
{
    size_t len;
    size_t ext_len;

    /* get the length of the name and of the game file suffix */
    len = strlen(fname);
    ext_len = strlen(w32_game_ext);

    /* check to see if we have the config file suffix */
    if (len > ext_len + 1
        && fname[len - ext_len - 1] == '.'
        && memicmp(fname + len - ext_len, w32_game_ext, ext_len) == 0)
    {
        /* replace the extension with the standard project file suffix */
        os_remext(fname);
        os_addext(fname, w32_tdb_config_ext);
    }
}

