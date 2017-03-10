/* $Header: d:/cvsroot/tads/html/win32/w32bpdlg.h,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32bpdlg.h - breakpoint dialog for TADS win32 debugger
Function
  
Notes
  
Modified
  03/09/98 MJRoberts  - Creation
*/

#ifndef W32BPDLG_H
#define W32BPDLG_H

#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif

/* ------------------------------------------------------------------------ */
/*
 *   Main breakpoints dialog 
 */
class CTadsDialogBreakpoints: public CTadsDialog
{
public:
    CTadsDialogBreakpoints(class CHtmlSys_dbgmain  *mainwin);
    ~CTadsDialogBreakpoints();

    /*
     *   Run the dialog.  Creates the dialog object, runs it as a modal
     *   dialog, and destroys the dialog.
     */
    static void run_bp_dlg(class CHtmlSys_dbgmain *mainwin);

    /* initialize */
    void init();

    /* handle a command */
    int do_command(WPARAM cmd, HWND ctl);

private:
    /* update buttons for the current listbox selection */
    void update_buttons();
    
    /* breakpoint enumerator callback for initialization */
    static void init_enum_bp_cb(void *ctx, int bpnum,
                                const textchar_t *desc, int disabled);
    
    class CHtmlSys_dbgmain *mainwin_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Breakpoint condition dialog 
 */
class CTadsDialogBpcond: public CTadsDialog
{
public:
    CTadsDialogBpcond(textchar_t *cond, size_t condbuflen,
                      int *stop_on_change)
    {
        /* remember the caller's data exchange variables */
        condbuf_ = cond;
        condbuflen_ = condbuflen;
        stop_on_change_ = stop_on_change;
    }
    
    /* initialize */
    void init();

    /* 
     *   Run the dialog.  Initializes the edit field with the given
     *   condition, and fills in the condition buffer with the new value
     *   in the edit field when the dialog is dismissed.  Returns the ID
     *   of the button that dismissed the dialog. 
     */
    static int run_bpcond_dlg(textchar_t *cond, size_t condbuflen,
                              int *stop_on_change, HWND owner);
    
    /* handle a command */
    int do_command(WPARAM cmd, HWND ctl);

private:
    /* buffer for exchanging the condition with the caller */
    textchar_t *condbuf_;
    size_t condbuflen_;

    /* caller's stop-on-change flag */
    int *stop_on_change_;
};

#endif /* W32BPDLG_H */

