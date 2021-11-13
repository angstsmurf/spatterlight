/* $Header: d:/cvsroot/tads/html/win32/w32main.h,v 1.4 1999/07/11 00:46:51 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32main.h - tads html win32 main entrypoint
Function
  
Notes
  
Modified
  01/31/98 MJRoberts  - Creation
*/

#ifndef W32MAIN_H
#define W32MAIN_H

#include <setjmp.h>


/*
 *   TADS entrypoint selector.  These variables can be set up so that we
 *   call either the normal TADS runtime or the debugger.  By defining
 *   these as externals, we can configure the executable we build with the
 *   linker, by supplying an appropriate set of initializations for these
 *   variables. 
 */

/* TADS entrypoint - usually trdmain() or tddmain() */
extern int (*w32_tadsmain)(int, char **, struct appctxdef *, char *);

/* "before options" - options to parse before config file options */
extern char *w32_beforeopts;

/* configuration file name - usually "config.trh" or "config.tdh" */
extern char *w32_configfile;

/* 
 *   Flag indicating whether the debug log window is allowed.  If this is
 *   true, we'll show a debug window if "-debugwin" is specified on the
 *   command line; if it's false, we won't show a debug log window.
 *   Normally, the run-time will allow the debug log window, whereas the
 *   debugger will not (because it provides its own debug log window).  
 */
extern int w32_allow_debugwin;

/*
 *   flag indicating that we always pause on exit 
 */
extern int w32_always_pause_on_exit;

/*
 *   Name of the registry value (attached to our main registry key) that
 *   we use to detect whether we've completed (or permanently skipped)
 *   setup for this program.  The normal run-time and the debugger use
 *   different keys, because it's possible that a user will install the
 *   run-time, then later install the debugger; when only the run-time is
 *   present, we won't set up the debugger, so we need to be able to
 *   detect the need to run setup again when the debugger is first
 *   started.  
 */
extern char *w32_setup_reg_val_name;

/*
 *   Name of the application to display in "usage" messages 
 */
extern char *w32_usage_app_name;

/*
 *   Name of the application to display in the main window title bar 
 */
extern char *w32_titlebar_name;

/*
 *   File selector filter.  This is the filter used for the open-file
 *   dialog that we present if there is no game specified on the command
 *   line.  This should be a string following the normal filter
 *   convention; for example, for TADS 2 .gam files, it might read "TADS
 *   Games\0*.gam\0All Files\0*.*\0\0".  
 */
extern char *w32_opendlg_filter;

/*
 *   File selector filter for the debugger.  This is the filter used for
 *   opening a new program; it should include program files as well as
 *   configuration files and source files. 
 */
extern char *w32_debug_opendlg_filter;

/*
 *   File selector for the debugger for opening a game configuration file.
 *   This filter is used when loading a program; it should include program
 *   files and configuration files.  
 */
extern char *w32_config_opendlg_filter;

/*
 *   Game file default extension.  The debugger uses this extension to
 *   build the name of a program file given the corresponding
 *   configuration file, by substituting this extension for the
 *   configuration file's extension.  
 */
extern char *w32_game_ext;

/*
 *   Debugger flag: true means that we're running under the debugger,
 *   false means we're running as a normal interpreter. 
 */
extern int w32_in_debugger;

/* underlying VM engine version ID string */
extern const char *w32_version_string;

/* registry key name for our preference settings */
extern const char w32_pref_key_name[];

/* name of debugger global configuration file */
extern const char w32_tdb_global_prefs_file[];

/* filename extension for configuration/project files (.tdc, .t3c) */
extern const char w32_tdb_config_ext[];

/* name of main Author's Manual index html file */
extern const char w32_tadsman_fname[];

/* major version number of the underlying system (2 for TADS 2, 3 for T3) */
extern int w32_system_major_vsn;

/*
 *   Pre-startup routine.  An appropriate implementation must be provided
 *   for each executable.
 *   
 *   'iter' is the iteration count through the TADS invocation loop;
 *   before the first invocation of the TADS engine, iter == 0, and iter
 *   is incremented each time we invoke the engine again (in other words,
 *   each time we load and run a new game).
 *   
 *   'argc' and 'argv' are the usual argument vector.  argc == 1 indicates
 *   that there are no arguments (argv[0] is always the executable name).
 *   This routine can change entries in the argument vector if it wants.
 *   
 *   Return true to tell the caller to proceed, false to terminate the
 *   executable.  
 */
int w32_pre_start(int iter, int argc, char **argv);

/*
 *   Post-quit processing routine.  An appropriate implementation of this
 *   routine must be provided for each executable; the stand-alone
 *   interpreter has a different implementation from the debugger, for
 *   example, because the two programs must handle quitting differently.
 *   
 *   main_ret_code is the result of the main TADS invocation: OSEXSUCC for
 *   success, OSEXFAIL for failure.
 *   
 *   Returns true if the caller should proceed to invoke the main TADS
 *   entrypoint again (normally to load a new game), false if the caller
 *   should terminate the program.  
 */
int w32_post_quit(int main_ret_code);

/*
 *   Version-specific routine - run the interpreter "about" box 
 */
void vsn_run_terp_about_dlg(HWND owner);

#endif /* W32MAIN_H */

