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
  w32tdbt2.cpp - HTML TADS Debugger - TADS 2 engine configuration
Function
  
Notes
  
Modified
  11/24/99 MJRoberts  - Creation
*/


#include <Windows.h>

#include "trd.h"
#include "w32main.h"
#include "w32tdb.h"
#include "htmldcfg.h"
#include "w32dbgpr.h"
#include "htmldbg.h"
#include "tadsapp.h"
#include "w32prj.h"
#include "htmlver.h"
#include "w32ver.h"


/* ------------------------------------------------------------------------ */
/*
 *   Define startup configuration variables for the TADS debugger 
 */
int (*w32_tadsmain)(int, char **, struct appctxdef *, char *) = tddmain;
char *w32_beforeopts = "i";
char *w32_configfile = "config.tdh";
int w32_allow_debugwin = FALSE;
int w32_always_pause_on_exit = TRUE;
char *w32_setup_reg_val_name = "Debugger Setup Done";
char *w32_usage_app_name = "htmltdb";
char *w32_titlebar_name = "HTML TADS";
int w32_in_debugger = TRUE;
char *w32_opendlg_filter = "TADS Games\0*.gam\0All Files\0*.*\0\0";
char *w32_config_opendlg_filter =
    "TADS Games and Configurations\0*.gam;*.tdc\0"
    "All Files\0*.*\0\0";
char *w32_debug_opendlg_filter =
    "TADS Games, Configurations, and Sources\0"
    "*.gam;*.tdc;*.t\0"
    "All Files\0*.*\0\0";
char *w32_game_ext = "gam";
const char *w32_version_string =
    HTMLTADS_VERSION
    " (Build Win" HTMLTADS_WIN32_BUILD
    "; TADS " TADS_RUNTIME_VERSION ")";
const char w32_pref_key_name[] =
    "Software\\TADS\\HTML TADS\\1.0\\Settings";
const char w32_tdb_global_prefs_file[] = "HtmlTDB.tdc";
const char w32_tdb_config_ext[] = "tdc";
const char w32_tadsman_fname[] = "doc\\index.html";
int w32_system_major_vsn = 2;

/* ------------------------------------------------------------------------ */
/*
 *   Debugger version factoring routines for TADS 2
 */

/* 
 *   we need the helper to maintain the "srcfiles" list for the TADS 2
 *   Workbench, because we don't have any UI to maintain the project file
 *   list directly 
 */
int CHtmlSys_dbgmain::vsn_need_srcfiles() const
{
    return TRUE;
}

/*
 *   Load a configuration file 
 */
int CHtmlSys_dbgmain::vsn_load_config_file(CHtmlDebugConfig *config,
                                           const textchar_t *fname)
{
    /* load the file with the standard binary file loader */
    return config->load_file(fname);
}

/*
 *   Save a configuration file 
 */
int CHtmlSys_dbgmain::vsn_save_config_file(CHtmlDebugConfig *config,
                                           const textchar_t *fname)
{
    /* save the file with the standard binary file writer */
    return config->save_file(fname);
}

/*
 *   Close windows in preparation for loading a new configuration 
 */
void CHtmlSys_dbgmain::vsn_load_config_close_windows()
{
    /* nothing extra for TADS 2 */
}

/*
 *   Open windows for a newly-loaded configuration 
 */
void CHtmlSys_dbgmain::vsn_load_config_open_windows(const RECT *)
{
    /* nothing extra for TADS 2 */
}

/*
 *   Save additional version-specific configuration information 
 */
void CHtmlSys_dbgmain::vsn_save_config()
{
    /* nothing extra for TADS 2 */
}

/*
 *   check command status 
 */
TadsCmdStat_t CHtmlSys_dbgmain::vsn_check_command(int command_id)
{
    return TADSCMD_UNKNOWN;
}

/* 
 *   execute command 
 */
int CHtmlSys_dbgmain::vsn_do_command(int, int, HWND)
{
    return FALSE;
}

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
    char exe[OSFNMAX];
    char cmd_dir[OSFNMAX];
    size_t cmd_dir_len;
    char gamefile[OSFNMAX];
    char optfile[OSFNMAX];
    char buf[1024];
    int i;
    int cnt;
    int intval;
    DWORD thread_id;
    int next_rs_idx;
    int rsfile_cnt;
    int cur_rs_file;
    int cur_cnt;

    /* 
     *   make one last check to make sure we don't already have a build in
     *   progress - we can only handle one at a time 
     */
    if (build_running_)
        return;

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
    case ID_BUILD_COMP_AND_RUN:
        /* build to the loaded game file */
        strcpy(gamefile, get_gamefile_name());
        break;

    case ID_BUILD_COMPRLS:
        /* build to the release game file */
        local_config_
            ->getval("build", "rlsgam", 0, gamefile, sizeof(gamefile));
        break;

    case ID_BUILD_COMPEXE:
    case ID_BUILD_COMPINST:
        /* 
         *   we're compiling to an executable (this is also the case if
         *   we're building an installer), so we don't really care about
         *   the .gam file; build the game to a temp file 
         */
        GetTempPath(sizeof(buf), buf);
        GetTempFileName(buf, "tads", 0, gamefile);
        break;
    }

    /* -------------------------------------------------------------------- */
    /*
     *   Step one: run the compiler 
     */

    /* generate the name of the compiler executable */
    GetModuleFileName(0, exe, sizeof(exe));
    strcpy(os_get_root_name(exe), "TC32.EXE");

    /* start the command with the executable name */
    cmdline.set("");
    cmdline.append_qu(exe, FALSE);

    /* add the include list */
    cnt = local_config_->get_strlist_cnt("build", "includes");
    for (i = 0 ; i < cnt ; ++i)
    {
        /* get this include path */
        local_config_->getval("build", "includes", i, buf, sizeof(buf));

        /* add the -i option and the path */
        cmdline.append_opt("-i", buf);
    }

    /* 
     *   add the output filename option - enclose the filename in quotes
     *   in case it contains spaces 
     */
    cmdline.append_opt("-o", gamefile);

    /* add the #defines, enclosing each string in quotes */
    cnt = local_config_->get_strlist_cnt("build", "defines");
    for (i = 0 ; i < cnt ; ++i)
    {
        /* get this include path */
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
        /* get this include path */
        local_config_->getval("build", "undefs", i, buf, sizeof(buf));

        /* add the -U option and the symbol to undefine */
        cmdline.append_opt("-U", buf);
    }

    /* add the -case- option if required */
    if (!local_config_->getval("build", "ignore case", &intval) && intval)
        cmdline.append_inc(" -case-", 512);

    /* add the -C+ option if required */
    if (!local_config_->getval("build", "c ops", &intval) && intval)
        cmdline.append_inc(" -C+", 512);

    /* add the -ctab option if required */
    if (!local_config_->getval("build", "use charmap", &intval) && intval
        && !local_config_->getval("build", "charmap", 0, buf, sizeof(buf)))
    {
        /* add the option */
        cmdline.append_opt("-ctab", buf);
    }

    /* add the extra options - just add them exactly as they're entered */
    cnt = local_config_->get_strlist_cnt("build", "extra options");
    for (i = 0 ; i < cnt ; ++i)
    {
        /* get this include path */
        local_config_->getval("build", "extra options", i, buf, sizeof(buf));

        /* add a space, then this line of options */
        cmdline.append_inc(" ", 512);
        cmdline.append_inc(buf, 512);
    }

    /* if we're compiling for debugging, add the debug option */
    if (command_id == ID_BUILD_COMPDBG || command_id == ID_BUILD_COMP_AND_RUN)
        cmdline.append_inc(" -ds2", 512);

    /* add the source filename, in quotes */
    local_config_->getval("build", "source", 0, buf, sizeof(buf));
    cmdline.append_qu(buf, TRUE);

    /* run the compiler in the source file's directory */
    os_get_path_name(cmd_dir, sizeof(cmd_dir), local_config_filename_.get());

    /* add the command to the list */
    add_build_cmd("tc32", exe, cmdline.get(), cmd_dir, FALSE);

    /* -------------------------------------------------------------------- */
    /*
     *   Step two: run the resource bundler.  Bundle resources only for
     *   release/exe builds, not for debugging builds.  
     */
    if (command_id == ID_BUILD_COMPDBG || command_id == ID_BUILD_COMP_AND_RUN)
    {
        /* this is a debug build, so we're done */
        goto done;
    }

    /* build the name of the bundler */
    strcpy(os_get_root_name(exe), "tadsrsc32.exe");

    /* start the command with the executable name */
    cmdline.set("");
    cmdline.append_qu(exe, FALSE);

    /* 
     *   run the resource bundler in the project file directory,
     *   regardless of where the game we're actually building will end up 
     */
    GetFullPathName(game_filename_.get(), sizeof(buf), buf, 0);
    os_get_path_name(cmd_dir, sizeof(cmd_dir), buf);

    /* note the length of the command directory, for prefix comparisons */
    cmd_dir_len = strlen(cmd_dir);

    /* add the game name (preceded by a space, and in quotes) */
    cmdline.append_qu(gamefile, TRUE);

    /* set up the options to add HTML resources */
    cmdline.append_inc(" -type html -add ", 512);

    /* get the count of resource files */
    rsfile_cnt = local_config_->get_strlist_cnt("build", "rs file cnt");

    /* 
     *   Get the index of the first element of the next resource file -
     *   it's zero (the starting element) plus the number of elements in
     *   the first resource file (the game file).  If we don't have a
     *   record of the number of resources in the game file, assume all of
     *   the resources are in the game file. 
     */
    if (local_config_->getval("build", "rs file cnt", 0, buf, sizeof(buf)))
    {
        /* 
         *   we have no record of resource file counts - use an index of
         *   -1 so that we never reach this value, which will cause us to
         *   put everything in the first resource file (the game file) 
         */
        next_rs_idx = -1;
    }
    else
    {
        /* use the number from the file */
        next_rs_idx = atoi(buf);
    }

    /* start at the first rs file */
    cur_rs_file = 0;

    /* we have nothing in the current file yet */
    cur_cnt = 0;

    /* run through the list and add each resource */
    cnt = local_config_->get_strlist_cnt("build", "resources");
    for (i = 0 ; i < cnt ; ++i)
    {
        char *p;

        /* 
         *   if we've reached the end of this resource list, start the
         *   next one - we must check this repeatedly in case we have
         *   empty resource files in the midst of the list 
         */
        while (i == next_rs_idx)
        {
            char ext[20];
            
            /* if there's anything in this file, write out the command */
            if (cur_cnt != 0)
                add_build_cmd("tadsrsc32", exe, cmdline.get(),
                              cmd_dir, FALSE);

            /* we have no resources in the next file yet */
            cur_cnt = 0;

            /* build the name of the next resource file */
            sprintf(ext, "rs%d", cur_rs_file);
            strcpy(buf, gamefile);
            os_remext(buf);
            os_addext(buf, ext);

            /* start the command line */
            cmdline.set("");
            cmdline.append_qu(exe, FALSE);

            /* add the resource filename with a -create option */
            cmdline.append_inc(" -create", 512);
            cmdline.append_qu(buf, TRUE);

            /* add the action specifiers to add HTML resources */
            cmdline.append_inc(" -type HTML -add", 512);

            /* on to the next .RS file */
            ++cur_rs_file;

            /* 
             *   read the number of elements in this file, and compute the
             *   index of the first resource in the next file 
             */
            if (local_config_->getval("build", "rs file cnt", cur_rs_file,
                                      buf, sizeof(buf)))
            {
                /* put everything in the current file */
                next_rs_idx = -1;
            }
            else
            {
                /* bump the starting index by the number of elements here */
                next_rs_idx += atoi(buf);
            }
        }
        
        /* get the next resource */
        local_config_->getval("build", "resources", i, buf, sizeof(buf));

        /* 
         *   If this file is in a subdirectory of the debugging game file
         *   path, remove the path prefix and make the name relative to
         *   the game file path.  Note that we care only about the
         *   debugging game file path, even if we're actually building to
         *   a different path, because we want a stable resource path
         *   prefix for all forms of the build.
         *   
         *   The file is in the subdirectory if its absolute path begins
         *   with the subdirectory, and the next character after the path
         *   prefix is a path separator.  
         */
        if (strlen(buf) > cmd_dir_len
            && memicmp(buf, cmd_dir, cmd_dir_len) == 0
            && (buf[cmd_dir_len] == '\\' || buf[cmd_dir_len] == '/'))
        {
            /* keep only the relative path after the command prefix part */
            p = buf + cmd_dir_len + 1;
        }
        else
        {
            /* keep the entire absolute path */
            p = buf;
        }

        /* add this resource name in quotes */
        cmdline.append_qu(p, TRUE);

        /* increment the count of resources in this file */
        ++cur_cnt;
    }

    /*   
     *   add the last command to the list; only bother with this command
     *   if there actually are any resources in this file 
     */
    if (cur_cnt != 0)
        add_build_cmd("tadsrsc32", exe, cmdline.get(), cmd_dir, FALSE);

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

    /* specify the source executable (in quotes) */
    GetModuleFileName(0, buf, sizeof(buf));
    strcpy(os_get_root_name(buf), "htmlt2.exe");
    cmdline.append_qu(buf, TRUE);

    /* add the game name */
    cmdline.append_qu(gamefile, TRUE);

    /* add the executable filename in quotes */
    local_config_->getval("build", "exe", 0, buf, sizeof(buf));
    cmdline.append_qu(buf, TRUE);

    /* run the command */
    add_build_cmd("maketrx32", exe, cmdline.get(), cmd_dir, FALSE);

    /* add a command to delete the temporary game file */
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

            /* return without starting a build */
            return;
        }
    }

done:
    /* -------------------------------------------------------------------- */
    /*
     *   We've built the list of commands, so we can now start the build
     *   thread to carry out the command list 
     */
    
    /* note that we have a compile running */
    build_running_ = TRUE;

    /* if we're meant to run after the build, so note */
    run_after_build_ = (command_id == ID_BUILD_COMP_AND_RUN);

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
}

/* ------------------------------------------------------------------------ */
/*
 *   Check to determine if it's okay to compile 
 */
int CHtmlSys_dbgmain::vsn_ok_to_compile()
{
    char buf[OSFNMAX];
    
    /* check for the source file setting */
    if (local_config_->getval("build", "source", 0, buf, sizeof(buf))
        || strlen(buf) == 0)
    {
        /* tell them what's wrong */
        MessageBox(handle_, "Before you compile this game, you must "
                   "configure the build options.  At the least, "
                   "you must specify the game's source file.  "
                   "Please select your build options, then use "
                   "the Compile command again when you're done.",
                   "TADS Workbench", MB_OK | MB_ICONEXCLAMATION);

        /* show the dialog */
        run_dbg_build_dlg(handle_, this,
                          DLG_BUILD_SRC, IDC_FLD_SOURCE);

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

    /* first, save the current configuration */
    if (local_config_filename_.get() != 0)
    {
        /* save the current configuration into the configuration object */
        save_config();

        /* write the configuration object to its file */
        local_config_->save_file(local_config_filename_.get());
    }

    /* build the name of the new configuration file */
    strcpy(new_config, gamefile);
    os_remext(new_config);
    os_addext(new_config, "tdc");

    /* 
     *   before we do anything we might regret, make sure we can save the
     *   configuration file 
     */
    if (local_config_->save_file(new_config))
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
            /* 
             *   get the path to the appropriate starter file - it's in
             *   the the TADS program directory, and is called "startA.t"
             *   for the advanced starter file, or "startI.t" for the
             *   introductory starter file 
             */
            GetModuleFileName(0, fbuf, sizeof(fbuf));
            strcpy(os_get_root_name(fbuf),
                   template_type == W32TDB_TPL_ADVANCED
                   ? "startA.t" : "startI.t");
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

    /* 
     *   Set up the new configuration, based on the existing
     *   configuration.  First, clear out all of the information that
     *   isn't suitable for a default configuration, so that we carry
     *   forward only the information from this configuration that we'd
     *   apply to the default config.  
     */
    clear_non_default_config();

    /* set up the initial build configuration for the new game */
    set_dbg_build_defaults(this, gamefile);
    local_config_->setval("build", "source", 0, srcfile);

    /*
     *   Build our source list: include the source file, plus the files it
     *   includes (adv.t and std.t).  First, clear out the source list.  
     */
    helper_->delete_internal_sources();

    /* add our source file */
    helper_->add_internal_line_source(os_get_root_name((textchar_t *)srcfile),
                                      srcfile, 0);

    /* add adv.t */
    GetModuleFileName(0, fbuf, sizeof(fbuf));
    strcpy(os_get_root_name(fbuf), "adv.t");
    helper_->add_internal_line_source("adv.t", fbuf, 1);

    /* add std.t */
    strcpy(os_get_root_name(fbuf), "std.t");
    helper_->add_internal_line_source("std.t", fbuf, 2);

    /* ask the helper to save the new source list into our config object */
    helper_->save_internal_source_config(local_config_);

    /* add the source directory to the directory list */
    os_get_path_name(fbuf, sizeof(fbuf), srcfile);
    local_config_->setval("srcdir", 0, 0, fbuf);

    /* add the adv.t/std.t directory to the directory list */
    GetModuleFileName(0, msg, sizeof(msg));
    os_get_path_name(fbuf, sizeof(fbuf), msg);
    local_config_->setval("srcdir", 0, 1, fbuf);

    /* add the GameInfo.txt generated resource file */
    local_config_->setval("build", "resources", 0, "GameInfo.txt");

    /* save the new configuration */
    local_config_->save_file(new_config);

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

    /* set the new game to load */
    if (CHtmlSys_mainwin::get_main_win() != 0)
        CHtmlSys_mainwin::get_main_win()->set_pending_new_game(gamefile);

    /*
     *   If we created the source file, post a Compile command, so that we
     *   compile the game as soon as we finish loading it.  (The game
     *   should be fully loaded by the time we get around to processing
     *   events again.)  Don't do this if we didn't create the source
     *   file, since we're not sure it's in any condition to compile at
     *   the moment.  
     */
    if (create_source)
        PostMessage(handle_, WM_COMMAND, ID_BUILD_COMPDBG, 0);

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
    /* nothing extra to do */
}


/*
 *   Set version-specific build defaults 
 */
void vsn_set_dbg_build_defaults(CHtmlDebugConfig *config,
                                CHtmlSys_dbgmain *dbgmain,
                                const textchar_t *gamefile)
{
    char *rootname;
    char buf[OSFNMAX];

    /* add the current directory to the include path */
    config->setval("build", "includes", 0, ".");

    /* 
     *   add the TADS directory to the include path 
     */

    /* get the current application filename */
    GetModuleFileName(0, buf, sizeof(buf));

    /* find the rootname */
    rootname = os_get_root_name(buf);

    /* remove the filename portion, keeping only the directory */
    if (rootname > buf && *(rootname - 1) == '\\')
        *(rootname - 1) = '\0';

    /* add the executable directory to the include path */
    config->setval("build", "includes", 1, buf);
}

/* ------------------------------------------------------------------------ */
/*
 *   program argument dialog - not used in tads 2 
 */
void CHtmlSys_dbgmain::vsn_run_dbg_args_dlg()
{
}

/* ------------------------------------------------------------------------ */
/*
 *   Provide some dummy definitions for the missing project window.  (These
 *   will never actually be called, since we never create a project window
 *   in the TADS 2 version of Workbench, but we need the entrypoints for
 *   linking.)  
 */
int CHtmlDbgProjWin::find_file_path(char *, size_t, const char *name)
{
    return FALSE;
}

void CHtmlDbgProjWin::refresh_for_activation()
{
}

void CHtmlDbgProjWin::refresh_all_libs()
{
}

/* ------------------------------------------------------------------------ */
/*
 *   Process captured build output.  The TADS 2 Workbench doesn't use build
 *   output capturing at all, so we have nothing to do here.  
 */
int CHtmlSys_dbgmain::vsn_process_build_output(CStringBuf *)
{
    /* don't show it */
    return FALSE;
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

    /* check to see if they specified a source file */
    if (ext != 0 && stricmp(ext, ".t") == 0)
    {
        /* run the load source file wizard */
        run_load_src_wiz(handle_, this, fname);
        
        /* handled */
        return TRUE;
    }

    /* we didn't handle it; let the main loader proceed */
    return FALSE;
}

/*
 *   Determine if a project file is valid for loading 
 */
int CHtmlSys_dbgmain::vsn_is_valid_proj_file(const textchar_t *)
{
    /* in tads 2 workbench, we can try loading any type of file */
    return TRUE;
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
     *   Generate the game filename - this is the filename we're loading with
     *   the extension forced to the game file suffix (.gam).  We can load
     *   based on game file or configuration file, but we do everything
     *   internally based on the game file, so make sure that's the name
     *   we're working with here.  (It's also still important to know the
     *   name of the actual file they're loading, because if it is a
     *   configuration file then we'll load the file they explicitly named.)
     */
    if (load_filename != 0)
    {
        /* use our game filename buffer to store the game filename */
        *game_filename = game_filename_buf;

        /* apply the game file extension */
        strcpy(game_filename_buf, load_filename);
        os_remext(game_filename_buf);
        os_addext(game_filename_buf, w32_game_ext);
    }
    else
    {
        /* no file is being loaded, so there's no game filename */
        *game_filename = 0;
    }

    /* 
     *   if we already have the same game's configuration loaded, there's no
     *   need to load the same game again - just tell the caller to do
     *   nothing 
     */
    if (local_config_ != 0
        && game_filename_.get() != 0
        && *game_filename != 0
        && get_strcmp(game_filename_.get(), *game_filename) == 0)
        return FALSE;

    /* 
     *   if the game hasn't been loaded yet, set the open-file dialog's
     *   default directory to the game's directory, so that we start in the
     *   game's directory when looking for files to open 
     */
    if (!game_loaded && *game_filename != 0)
        CTadsApp::get_app()->set_openfile_dir(*game_filename);

    /* 
     *   Build the default configuration file name -- use the game file name,
     *   with the extension replaced by the config file extension (.tdc).  
     */
    if (*game_filename != 0)
    {
        const char *ext;

        /* find the extension */
        ext = strrchr(load_filename, '.');
            
        /*   
         *   It's possible that the file they explicitly loaded is actually a
         *   configuration file.  To detect this, check to see if the game
         *   filename we've computed (by forcing the game file extension) is
         *   different from the explicitly loaded file - if so, assume the
         *   explicitly loaded file is actually a configuration file.  Also
         *   make sure that load filename ends in the proper extension.  
         */
        if (stricmp(*game_filename, load_filename) != 0
            && ext != 0
            && stricmp(ext, w32_tdb_config_ext) == 0)
        {
            /* 
             *   the filename specified isn't a game file, so it must be a
             *   project configuration file - use the original name
             *   explicitly selected 
             */
            strcpy(config_filename, load_filename);
        }
        else
        {
            textchar_t full_game_filename[OSFNMAX];

            /* get the full path to the file */
            GetFullPathName(*game_filename, OSFNMAX, full_game_filename, 0);

            /* 
             *   copy the full game filename to the config filename, then
             *   change the extension to the configuration file suffix 
             */
            strcpy(config_filename, full_game_filename);
            os_remext(config_filename);
            os_addext(config_filename, w32_tdb_config_ext);

            /* use the full version of the game filename from now on */
            strcpy(game_filename_buf, full_game_filename);
            *game_filename = game_filename_buf;
        }
    }

    /* tell the caller to proceed with the loading */
    return TRUE;
}

/*
 *   Get the game filename from the load profile.  This does nothing in tads
 *   2 workbench, since the game filename is inferred from the loaded
 *   configuration filename (and in some cases is simply the file directly
 *   loaded by the user).  
 */
int CHtmlSys_dbgmain::vsn_get_load_profile_gamefile(char *)
{
    /* tell the caller that we're not changing the name */
    return FALSE;
}

/*
 *   Set the main window title.  Set the title to the loaded game file name.
 */
void CHtmlSys_dbgmain::vsn_set_win_title()
{
    char title[OSFNMAX + 100];

    /* build the text */
    if (workspace_open_)
        sprintf(title, "%s - TADS Workbench",
                os_get_root_name((char *)game_filename_.get()));
    else
        strcpy(title, "TADS Workbench");

    /* set the label */
    SetWindowText(handle_, title);
}

/*
 *   Update the game file name to reflect a change in the preferences.  In
 *   tads 2 workbench, this does nothing, since the game file name cannot be
 *   changed in the preferences; it's always derived from the project file
 *   name.  
 */
void CHtmlSys_dbgmain::vsn_change_pref_gamefile_name(textchar_t *)
{
    /* does nothing in tads 2 workbench */
}

/*
 *   Adjust an argv argument (a program startup argument) to the proper
 *   filename extension for initial loading.
 *   
 *   For the tads 2 workbench, we always load from .GAM files.  However, the
 *   user can also load by double-clicking on the corresponding .TDC file.
 *   So, check for a .TDC extension, and if we have it, change it to .GAM,
 *   since it's the game file we actually want to pass to the loader.  
 */
void CHtmlSys_dbgmain::vsn_adjust_argv_ext(textchar_t *fname)
{
    size_t len;
    size_t ext_len;

    /* get the length of the name and of the standard config file suffix */
    len = strlen(fname);
    ext_len = strlen(w32_tdb_config_ext);
    
    /* check to see if we have the config file suffix */
    if (len > ext_len + 1
        && fname[len - ext_len - 1] == '.'
        && memicmp(fname + len - ext_len,
                   w32_tdb_config_ext, ext_len) == 0)
    {
        /* 
         *   remove the old extension, and replace it with the game file
         *   extension 
         */
        os_remext(fname);
        os_addext(fname, w32_game_ext);
    }
}

