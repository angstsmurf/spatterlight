#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/w32dbwiz.cpp,v 1.1 1999/07/11 00:46:50 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32dbwiz.cpp - debugger wizards
Function
  
Notes
  
Modified
  06/08/99 MJRoberts  - Creation
*/

#include <Windows.h>

#include "tadsapp.h"
#include "tadsdlg.h"
#include "w32dbwiz.h"
#include "w32tdb.h"
#include "htmlres.h"
#include "htmldcfg.h"


/* ------------------------------------------------------------------------ */
/*
 *   Debugger Startup Dialog 
 */

/*
 *   run the dialog 
 */
int CHtmlDbgWiz::run_start_dlg(CHtmlSys_dbgmain *dbgmain)
{
    CHtmlDbgWiz *dlg;
    int ret;
    
    /* create the dialog */
    dlg = new CHtmlDbgWiz(dbgmain);

    /* run it */
    ret = dlg->run_modal(DLG_DBGWIZ_START, dbgmain->get_handle(),
                         CTadsApp::get_app()->get_instance());

    /* delete the dialog now that we're done with it */
    delete dlg;

    /* determine what to do based on the result */
    switch(ret)
    {
    case IDOK:
        /* done - return */
        return ret;

    case IDC_BTN_CREATE:
        /* post a "create new game" command */
        PostMessage(dbgmain->get_handle(), WM_COMMAND,
                    (WPARAM)ID_FILE_CREATEGAME, (LPARAM)0);
        break;

    case IDC_BTN_OPEN:
        /* post a "load new game" command to the main window */
        PostMessage(dbgmain->get_handle(), WM_COMMAND,
                    (WPARAM)ID_FILE_LOADGAME, (LPARAM)0);
        break;
    }

    /* return the dialog result */
    return ret;
}

/*
 *   create 
 */
CHtmlDbgWiz::CHtmlDbgWiz(CHtmlSys_dbgmain *dbgmain)
{
    /* remember the debugger main window and global configuration object */
    dbgmain_ = dbgmain;
    global_config_ = dbgmain->get_global_config();
    
    /* load our bitmaps */
    bmp_new_ = LoadBitmap(CTadsApp::get_app()->get_instance(),
                          MAKEINTRESOURCE(IDB_BMP_START_NEW));
    bmp_open_ = LoadBitmap(CTadsApp::get_app()->get_instance(),
                           MAKEINTRESOURCE(IDB_BMP_START_OPEN));
}

/*
 *   destroy 
 */
CHtmlDbgWiz::~CHtmlDbgWiz()
{
    /* delete our button bitmaps */
    DeleteObject(bmp_new_);
    DeleteObject(bmp_open_);
}

/*
 *   initialize 
 */
void CHtmlDbgWiz::init()
{
    int val;
    CTadsFont *font;

    /* center the dialog */
    center_dlg_win(handle_);

    /* set the button bitmaps */
    SendMessage(GetDlgItem(handle_, IDC_BTN_CREATE), BM_SETIMAGE,
                (WPARAM)IMAGE_BITMAP, (LPARAM)bmp_new_);
    SendMessage(GetDlgItem(handle_, IDC_BTN_OPEN), BM_SETIMAGE,
                (WPARAM)IMAGE_BITMAP, (LPARAM)bmp_open_);

    /* set up a font for a bold version of the dialog font */
    font = CTadsApp::get_app()->get_font("MS Sans Serif", 8, TRUE, FALSE,
                                         GetSysColor(COLOR_BTNTEXT),
                                         FF_SWISS | VARIABLE_PITCH);

    /* embolden some of the text items */
    SendMessage(GetDlgItem(handle_, IDC_ST_WELCOME), WM_SETFONT,
                (WPARAM)font->get_handle(), MAKELPARAM(TRUE, 0));
    SendMessage(GetDlgItem(handle_, IDC_ST_CREATE), WM_SETFONT,
                (WPARAM)font->get_handle(), MAKELPARAM(TRUE, 0));
    SendMessage(GetDlgItem(handle_, IDC_ST_OPEN), WM_SETFONT,
                (WPARAM)font->get_handle(), MAKELPARAM(TRUE, 0));

    /* 
     *   Check the checkbox if "startup:action" is set to 1 (for the "show
     *   welcome dialog" mode) or is undefined.  (This config variable
     *   indicates the startup action; it defaults to "show welcome dialog,"
     *   which has value 1.)  
     */
    if (global_config_->getval("startup", "action", &val))
        val = 1;
    CheckDlgButton(handle_, IDC_CK_SHOWAGAIN,
                   val == 1 ? BST_CHECKED : BST_UNCHECKED);
}

/*
 *   process a command 
 */
int CHtmlDbgWiz::do_command(WPARAM id, HWND ctl)
{
    switch(id)
    {
    case IDC_CK_SHOWAGAIN:
        /* 
         *   If they've checked the button, set the startup action to "show
         *   welcome dialog" (code 1); otherwise, set it to "do nothing"
         *   (code 3).
         *   
         *   Note that we don't have to worry about other possible states; if
         *   we're showing the dialog at all, then the checkbox will have
         *   been initially checked.  We can thus arbitrarily choose the mode
         *   selected by unchecking the checkbox.  
         */
        global_config_->setval("startup", "action",
                               (IsDlgButtonChecked(handle_, IDC_CK_SHOWAGAIN)
                                == BST_CHECKED) ? 1 : 3);
        return TRUE;
        
    case IDC_BTN_CREATE:
    case IDC_BTN_OPEN:
        /* dismiss the dialog and return this result code */
        EndDialog(handle_, id);
        return TRUE;
        
    default:
        /* use default processing */
        break;
    }

    /* inherit default handling */
    return CTadsDialog::do_command(id, ctl);
}

