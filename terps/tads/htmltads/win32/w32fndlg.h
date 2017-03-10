/* $Header: d:/cvsroot/tads/html/win32/w32fndlg.h,v 1.2 1999/05/17 02:52:26 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32fndlg.h - TADS win32 debugger "Find" dialog
Function
  
Notes
  
Modified
  03/11/98 MJRoberts  - Creation
*/

#ifndef W32FNDLG_H
#define W32FNDLG_H

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif

class CTadsDialogFind: public CTadsDialog
{
public:
    CTadsDialogFind();
    ~CTadsDialogFind();

    /*
     *   Run the dialog and get the string to find.  Puts the string into
     *   the caller's buffer and returns true if we got a string, returns
     *   false if the user cancelled the dialog. 
     */
    int run_find_dlg(HWND parent_hdl,
                     textchar_t *buf, size_t buflen, int *exact_case,
                     int *start_at_top, int *wrap, int *direction);

    /* initialize */
    void init();

    /* handle a command */
    int do_command(WPARAM cmd, HWND ctl);

    /* forget all old search strings */
    void forget_old_search_strings();

    /* set an old search string */
    void set_old_search_string(int idx, const char *str, size_t len);

    /* get an old search string - returns null if the string isn't set */
    const char *get_old_search_string(int idx) const
    {
        return (idx >= get_max_old_search_strings() ? 0 : past_strings_[idx]);
    }

    /* get the maximum number of old search strings */
    int get_max_old_search_strings() const
        { return sizeof(past_strings_)/sizeof(past_strings_[0]); }

private:
    /*
     *   Caller's buffer for the result string.  We'll copy the result
     *   here if the user dismisses the dialog with "Find" (rather than
     *   "Cancel").  
     */
    textchar_t *buf_;
    size_t buflen_;

    /* 
     *   caller's exact case flag - we'll set this flag to true if exact
     *   case is desired, false otherwise 
     */
    int *exact_case_;

    /*
     *   caller's start-at-top flag - we'll set this flag to true if the
     *   user wants to start the search at the top of the file, false
     *   otherwise 
     */
    int *start_at_top_;

    /*
     *   caller's wrap-around flag - we'll set this flag to true if the
     *   user wants the search to wrap around at the end of the file,
     *   false otherwise 
     */
    int *wrap_around_;

    /*
     *   caller's direction-to-search flag - we'll set this to 1 for down
     *   (forwards), -1 for up (backwards) 
     */
    int *direction_;
    
    
    /* 
     *   Stored strings from past invocations.  Each time the user enters
     *   a string and accepts it, we add the string to our list; we keep
     *   only the ten most recent strings if more than that have been
     *   entered. 
     */
    textchar_t *past_strings_[10];
};

#endif /* W32FNDLG_H */

