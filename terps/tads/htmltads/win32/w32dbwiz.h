/* $Header: d:/cvsroot/tads/html/win32/w32dbwiz.h,v 1.1 1999/07/11 00:46:50 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1999 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32dbwiz.h - TADS debugger wizards
Function
  
Notes
  
Modified
  06/08/99 MJRoberts  - Creation
*/

#ifndef W32DBWIZ_H
#define W32DBWIZ_H

#include <Windows.h>

#include "tadsdlg.h"

/* ------------------------------------------------------------------------ */
/*
 *   Startup Dialog 
 */

class CHtmlDbgWiz: public CTadsDialog
{
public:
    CHtmlDbgWiz(class CHtmlSys_dbgmain *dbgmain);
    ~CHtmlDbgWiz();
    
    /* run the startup dialog */
    static int run_start_dlg(class CHtmlSys_dbgmain *dbgmain);

    /* initialize */
    void init();

    /* process a command */
    int do_command(WPARAM id, HWND ctl);
    
protected:
    /* button bitmaps */
    HBITMAP bmp_new_;
    HBITMAP bmp_open_;

    /* debugger main window object */
    class CHtmlSys_dbgmain *dbgmain_;

    /* debugger global configuration object */
    class CHtmlDebugConfig *global_config_;
};

#endif /* W32DBWIZ_H */

