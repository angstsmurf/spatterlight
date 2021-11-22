#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/w32fndlg.cpp,v 1.3 1999/07/11 00:46:51 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32fnddlg.cpp - TADS win32 debugger "Find" dialog
Function
  
Notes
  
Modified
  03/11/98 MJRoberts  - Creation
*/

#include <Windows.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif
#ifndef W32FNDLG_H
#include "w32fndlg.h"
#endif
#ifndef HTMLRES_H
#include "htmlres.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif


CTadsDialogFind::CTadsDialogFind()
{
    int i;
    
    /* we have no old strings yet - clear out our array */
    for (i = 0 ; i < sizeof(past_strings_)/sizeof(past_strings_[0]) ; ++i)
        past_strings_[i] = 0;
}

CTadsDialogFind::~CTadsDialogFind()
{
    int i;
    
    /* delete any old strings we've allocated */
    for (i = 0 ; i < sizeof(past_strings_)/sizeof(past_strings_[0]) ; ++i)
    {
        /* if we've allocated this one, delete it now */
        if (past_strings_[i] != 0)
            th_free(past_strings_[i]);
    }
}

/*
 *   Run the dialog 
 */
int CTadsDialogFind::run_find_dlg(HWND parent_hdl,
                                  textchar_t *buf, size_t buflen,
                                  int *exact_case, int *start_at_top,
                                  int *wrap, int *direction)
{
    /* 
     *   set up our pointer to the caller's buffer, so that the dialog can
     *   copy the string into the buffer when the user hits "find" 
     */
    buf_ = buf;
    buflen_ = buflen;

    /* remember the flag pointers */
    exact_case_ = exact_case;
    start_at_top_ = start_at_top;
    wrap_around_ = wrap;
    direction_ = direction;
    
    /* run the dialog, returning true only if they hit "find" */
    return (run_modal(DLG_FIND, parent_hdl,
                      CTadsApp::get_app()->get_instance()) == IDOK);
}


/*
 *   Initialize 
 */
void CTadsDialogFind::init()
{
    int i;
    HWND combo;
    
    /* center the dialog on the screen */
    center_dlg_win(handle_);

    /* load up the combo box with the old strings */
    combo = GetDlgItem(handle_, IDC_CBO_FINDWHAT);
    for (i = 0 ; i < sizeof(past_strings_)/sizeof(past_strings_[0]) ; ++i)
    {
        /* if we have a string in this slot, add it */
        if (past_strings_[i] != 0)
            SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)past_strings_[i]);
    }

    /* 
     *   set the initial enabling of the combo box according to whether we
     *   have any text for it 
     */
    EnableWindow(GetDlgItem(handle_, IDOK), get_strlen(buf_) != 0);

    /* put the initial text into the combo box */
    SetWindowText(GetDlgItem(handle_, IDC_CBO_FINDWHAT), buf_);

    /* set the initial checkbox and radio states */
    CheckDlgButton(handle_, IDC_CK_MATCHCASE,
                   (*exact_case_ ? BST_CHECKED : BST_UNCHECKED));
    CheckDlgButton(handle_, IDC_CK_WRAP,
                   (*wrap_around_ ? BST_CHECKED : BST_UNCHECKED));
    CheckDlgButton(handle_, IDC_CK_STARTATTOP,
                   (*start_at_top_ ? BST_CHECKED : BST_UNCHECKED));
    CheckDlgButton(handle_, IDC_RD_UP,
                   (*direction_ == -1 ? BST_CHECKED : BST_UNCHECKED));
    CheckDlgButton(handle_, IDC_RD_DOWN,
                   (*direction_ == 1 ? BST_CHECKED : BST_UNCHECKED));
}

/*
 *   process a command 
 */
int CTadsDialogFind::do_command(WPARAM cmd, HWND ctl)
{
    switch(LOWORD(cmd))
    {
    case IDC_CBO_FINDWHAT:
        switch(HIWORD(cmd))
        {
        case CBN_EDITCHANGE:
            /* 
             *   The text in the combo has change.  Enable the "Find"
             *   button if the combo is non-empty, and disable the button
             *   if the combo is empty.  
             */
            EnableWindow(GetDlgItem(handle_, IDOK),
                         (GetWindowTextLength(ctl) != 0));
            return TRUE;

        case CBN_SELCHANGE:
            /* they selected something, so there must be some text now */
            EnableWindow(GetDlgItem(handle_, IDOK), TRUE);
            return TRUE;

        default:
            break;
        }
        return FALSE;
        
    case IDC_CK_MATCHCASE:
        *exact_case_ = (IsDlgButtonChecked(handle_, IDC_CK_MATCHCASE)
                        == BST_CHECKED);
        return TRUE;

    case IDC_CK_WRAP:
        *wrap_around_ = (IsDlgButtonChecked(handle_, IDC_CK_WRAP)
                         == BST_CHECKED);
        return TRUE;

    case IDC_CK_STARTATTOP:
        *start_at_top_ = (IsDlgButtonChecked(handle_, IDC_CK_STARTATTOP)
                          == BST_CHECKED);
        return TRUE;

    case IDC_RD_UP:
    case IDC_RD_DOWN:
        *direction_ = ((IsDlgButtonChecked(handle_, IDC_RD_UP)
                       == BST_CHECKED) ? -1 : 1);
        return TRUE;

    case IDCANCEL:
        /* cancel the dialog */
        EndDialog(handle_, IDCANCEL);
        return TRUE;

    case IDOK:
        /* copy the result into the caller's buffer */
        GetWindowText(GetDlgItem(handle_, IDC_CBO_FINDWHAT), buf_, buflen_);

        /* 
         *   add this item as the first string in our past strings list --
         *   to do this, push down the old list, and add this item in the
         *   first slot 
         */
        {
            size_t cnt;
            size_t i;

            /* get the maximum number of strings to save */
            cnt = sizeof(past_strings_)/sizeof(past_strings_[0]);

            /*
             *   Scan for an existing copy of the current string.  If it's
             *   already in the list, move it to the head of the list
             *   (i.e., remove it from its current position and reinsert
             *   it at the first position). 
             */
            for (i = 0 ; i < cnt && past_strings_[i] != 0 ; ++i)
            {
                /* if this one matches, stop looking */
                if (get_stricmp(past_strings_[i], buf_) == 0)
                    break;
            }

            /* if we found a match, simply move it to the first position */
            if (i != cnt && past_strings_[i] != 0)
            {
                int j;
                textchar_t *str;

                /* remember the string we're moving for a moment */
                str = past_strings_[i];
                
                /* 
                 *   close the gap by moving all of the strings from the
                 *   first position to the one before the one we're
                 *   removing forward one position in the list 
                 */
                for (j = i ; j != 0 ; --j)
                    past_strings_[j] = past_strings_[j-1];

                /* put this item back in as the first string */
                past_strings_[0] = str;
            }
            else
            {
                /* 
                 *   this search string is new, so we'll insert it; if
                 *   we're pushing one off the end, free its memory 
                 */
                if (past_strings_[cnt-1] != 0)
                    th_free(past_strings_[cnt-1]);
                
                /* move all of the strings down a slot */
                for (i = cnt - 1 ; i != 0 ; --i)
                    past_strings_[i] = past_strings_[i-1];
                
                /* allocate space for the new string */
                past_strings_[0] = (textchar_t *)
                                   th_malloc((get_strlen(buf_) + 1)
                                             * sizeof(textchar_t));
                
                /* copy it */
                do_strcpy(past_strings_[0], buf_);
            }
        }

        /* end the dialog */
        EndDialog(handle_, IDOK);
        return TRUE;

    default:
        /* inherit default handling */
        return CTadsDialog::do_command(cmd, ctl);
    }
}

/*
 *   forget the old search strings 
 */
void CTadsDialogFind::forget_old_search_strings()
{
    int i;

    /* loop through our strings and delete each one */
    for (i = 0 ; i < sizeof(past_strings_)/sizeof(past_strings_[0]) ; ++i)
    {
        /* if this one was allocated, free it and clear the slot */
        if (past_strings_[i] != 0)
        {
            th_free(past_strings_[i]);
            past_strings_[i] = 0;
        }
    }
}

/*
 *   set an old search string 
 */
void CTadsDialogFind::set_old_search_string(int idx, const char *str,
                                            size_t len)
{
    /* if it's already allocated, free the old one */
    if (past_strings_[idx] != 0)
        th_free(past_strings_[idx]);

    /* allocate space to save a copy of the string */
    past_strings_[idx] = (textchar_t *)
                         th_malloc((len + 1) * sizeof(textchar_t));

    /* make a copy of the string in our allocated buffer */
    memcpy(past_strings_[idx], str, len*sizeof(textchar_t));
    past_strings_[idx][len] = '\0';
}
