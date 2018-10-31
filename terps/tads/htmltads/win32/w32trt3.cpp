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
  w32trt3.cpp - HTML TADS interpreter - T3 connector file
Function
  Connects and configures the HTML TADS main entrypoint for the T3
  VM engine.
Notes
  
Modified
  10/08/99 MJRoberts  - created (from w32tr.cpp)
*/

#include <Windows.h>

/* T3 includes */
#include "vmvsn.h"

/* HTML TADS includes */
#ifndef T3MAIN_H
#include "t3main.h"
#endif
#ifndef W32MAIN_H
#include "w32main.h"
#endif
#include "htmlver.h"
#include "w32ver.h"


/* ------------------------------------------------------------------------ */
/*
 *   Define startup configuration variables for the T3 engine
 */
int (*w32_tadsmain)(int, char **, struct appctxdef *, char *) = t3main;
char *w32_beforeopts = "";
char *w32_configfile = 0;
int w32_allow_debugwin = TRUE;
int w32_always_pause_on_exit = FALSE;
char *w32_setup_reg_val_name = "Setup Done";
char *w32_usage_app_name = "htmlt3";
char *w32_titlebar_name = "HTML T3";
int w32_in_debugger = FALSE;
char *w32_opendlg_filter = "T3 Applications\0*.t3\0All Files\0*.*\0\0";
const char *w32_version_string =
    HTMLTADS_VERSION
    " (Build Win" HTMLTADS_WIN32_BUILD
    "; TADS " T3VM_VSN_STRING ")";
const char w32_pref_key_name[] =
    "Software\\TADS\\HTML TADS\\3.0\\Settings";

