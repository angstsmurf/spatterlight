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
  w32trt23.cpp - HTML TADS interpreter - combined TADS 2/3 connector file
Function
  Connects and configures the HTML TADS main entrypoint for the combined
  interpreter, which hosts both TADS 2 and 3 engines in a single interpreter
  executable.
Notes

Modified
  03/06/01 MJRoberts  - created (from w32trt3.cpp)
*/

#include <Windows.h>


/* HTML TADS includes */
#ifndef T3MAIN_H
#include "t3main.h"
#endif
#ifndef T23MAIN_H
#include "t23main.h"
#endif
#ifndef W32MAIN_H
#include "w32main.h"
#endif

#include "htmlver.h"
#include "w32ver.h"

/* include the TADS 2 VM version information */
#include "trd.h"

/* include the TADS 3 VM version information */
#include "vmvsn.h"

/* include some other needed T3 headers */
#include "vmerr.h"
#include "vmimage.h"


/* ------------------------------------------------------------------------ */
/*
 *   Define startup configuration variables for the combined TADS 2 and 3
 *   engines 
 */
int (*w32_tadsmain)(int, char **, struct appctxdef *, char *) = t23main;
char *w32_beforeopts = "";
char *w32_configfile = 0;
int w32_allow_debugwin = TRUE;
int w32_always_pause_on_exit = FALSE;
char *w32_setup_reg_val_name = "Setup Done";
char *w32_usage_app_name = "htmlt23";
char *w32_titlebar_name = "HTML TADS";
int w32_in_debugger = FALSE;
char *w32_opendlg_filter =
    "TADS Games\0*.gam;*.t3\0"
    "All Files\0*.*\0\0";
const char *w32_version_string =
    HTMLTADS_VERSION
    " (Build Win" HTMLTADS_WIN32_BUILD
    "; TADS " TADS_RUNTIME_VERSION "/" T3VM_VSN_STRING ")";
const char w32_pref_key_name[] =
    "Software\\TADS\\HTML TADS\\3.0\\Settings";

