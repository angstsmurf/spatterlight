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
  w32trt2.cpp - win32 main startup file for HTML TADS Interpreter
Function
  
Notes
  
Modified
  11/24/99 MJRoberts  - Creation
*/

#include <Windows.h>

#ifndef W32MAIN_H
#include "w32main.h"
#endif
#ifndef TRD_H
#include "trd.h"
#endif
#include "htmlver.h"
#include "w32ver.h"


/* ------------------------------------------------------------------------ */
/*
 *   Define startup configuration variables for the TADS 2 engine
 */
int (*w32_tadsmain)(int, char **, struct appctxdef *, char *) = trdmain;
char *w32_beforeopts = "";
char *w32_configfile = "config.trh";
int w32_allow_debugwin = TRUE;
int w32_always_pause_on_exit = FALSE;
char *w32_setup_reg_val_name = "Setup Done";
char *w32_usage_app_name = "htmltads";
char *w32_titlebar_name = "HTML TADS";
int w32_in_debugger = FALSE;
char *w32_opendlg_filter = "TADS Games\0*.gam\0All Files\0*.*\0\0";
const char *w32_version_string =
    HTMLTADS_VERSION
    " (Build Win" HTMLTADS_WIN32_BUILD
    "; TADS " TADS_RUNTIME_VERSION ")";
const char w32_pref_key_name[] =
    "Software\\TADS\\HTML TADS\\1.0\\Settings";

