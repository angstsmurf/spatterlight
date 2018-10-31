#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/w32bpdlg.cpp,v 1.3 1999/07/11 00:46:48 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32bpdlg.cpp - breakpoints dialog for win32 debugger
Function
  
Notes
  
Modified
  03/09/98 MJRoberts  - Creation
*/

#include <Windows.h>

#ifndef HTMLRES_H
#include "htmlres.h"
#endif
#ifndef W32BPDLG_H
#include "w32bpdlg.h"
#endif
#ifndef W32TDB_H
#include "w32tdb.h"
#endif
#ifndef HTMLDBG_H
#include "htmldbg.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif


CTadsDialogBreakpoints::CTadsDialogBreakpoints(CHtmlSys_dbgmain  *mainwin)
{
    /* remember the main window */
    mainwin_ = mainwin;
}

CTadsDialogBreakpoints::~CTadsDialogBreakpoints()
{
}

/*
 *   Run the dialog 
 */
void CTadsDialogBreakpoints::run_bp_dlg(CHtmlSys_dbgmain *mainwin)
{
    CTadsDialogBreakpoints *dlg;
    
    /* create the dialog */
    dlg = new CTadsDialogBreakpoints(mainwin);

    /* run it */
    dlg->run_modal(DLG_BREAKPOINTS, mainwin->get_handle(),
                   CTadsApp::get_app()->get_instance());

    /* we're done with the dialog - delete it */
    delete dlg;
}

/* callback context structure for enumerating breakpoints during init */
struct init_enum_cb_ctx_t
{
    /* the dialog pointer */
    CTadsDialogBreakpoints *dlg_;

    /* number of breakpoints added to the list so far */
    int count_;
};

/*
 *   initialize 
 */
void CTadsDialogBreakpoints::init()
{
    init_enum_cb_ctx_t cbctx;
    
    /* center the dialog on the screen */
    center_dlg_win(handle_);

    /* build the list of breakpoints */
    cbctx.dlg_ = this;
    cbctx.count_ = 0;
    mainwin_->get_helper()->enum_breakpoints(mainwin_->get_dbg_ctx(),
                                             &init_enum_bp_cb, &cbctx);

    /* set up the buttons correctly for the initial conditions */
    update_buttons();
}

/*
 *   Enumerate breakpoints 
 */
void CTadsDialogBreakpoints::init_enum_bp_cb(void *ctx0, int bpnum,
                                             const textchar_t *desc,
                                             int /*disabled*/)
{
    init_enum_cb_ctx_t *ctx = (init_enum_cb_ctx_t *)ctx0;
    HWND lbhdl;
    int idx;

    /* get the listbox's handle */
    lbhdl = GetDlgItem(ctx->dlg_->handle_, IDC_LB_BP);

    /* add the breakpoint to the list box in the dialog */
    idx = SendMessage(lbhdl, LB_ADDSTRING, 0, (LPARAM)desc);

    /* set the item's extra data to the breakpoint ID */
    SendMessage(lbhdl, LB_SETITEMDATA, (WPARAM)idx, (LPARAM)bpnum);
}

/*
 *   update the buttons for the current list box selection 
 */
void CTadsDialogBreakpoints::update_buttons()
{
    int idx;
    HWND lb;

    /* get the selected item from the listbox */
    lb = GetDlgItem(handle_, IDC_LB_BP);
    idx = SendMessage(lb, LB_GETCURSEL, 0, 0);

    /* 
     *   if there's no item, disable all of the item-specific buttons;
     *   otherwise, enable them as appropriate 
     */
    if (idx == LB_ERR)
    {
        EnableWindow(GetDlgItem(handle_, IDC_BTN_GOTOCODE), FALSE);
        EnableWindow(GetDlgItem(handle_, IDC_BTN_CONDITION), FALSE);
        EnableWindow(GetDlgItem(handle_, IDC_BTN_REMOVE), FALSE);
        EnableWindow(GetDlgItem(handle_, IDC_BTN_DISABLEBP), FALSE);
    }
    else
    {
        int bpnum;
        int has_code;
        int enabled;

        /* get the breakpoint number */
        bpnum = (int)SendMessage(lb, LB_GETITEMDATA, (WPARAM)idx, 0);

        /* determine if the breakpoint has associated code */
        has_code = mainwin_->get_helper()->
                   is_bp_at_line(bpnum);

        /* determine whether the breakpoint is currently enabled */
        enabled = mainwin_->get_helper()->is_bp_enabled(bpnum);

        /* 
         *   update the buttons: turn on the "Edit Condition" and "Remove"
         *   buttons for any valid breakpoint; turn on the "Go to code"
         *   button only if the breakpoint is associated with code 
         */
        EnableWindow(GetDlgItem(handle_, IDC_BTN_GOTOCODE), has_code);
        EnableWindow(GetDlgItem(handle_, IDC_BTN_CONDITION), TRUE);
        EnableWindow(GetDlgItem(handle_, IDC_BTN_REMOVE), TRUE);

        /*
         *   turn on the enable/disable button, and set its text according
         *   to whether the breakpoint is currently enabled or disabled 
         */
        EnableWindow(GetDlgItem(handle_, IDC_BTN_DISABLEBP), TRUE);
        SetWindowText(GetDlgItem(handle_, IDC_BTN_DISABLEBP),
                      enabled ? "&Disable" : "&Enable");
    }

    /* 
     *   turn on the "Remove All" button only if we have one or more
     *   breakpoints in the list 
     */
    EnableWindow(GetDlgItem(handle_, IDC_BTN_REMOVEALL),
                 SendMessage(lb, LB_GETCOUNT, 0, 0) != 0);
}

/*
 *   handle a command 
 */
int CTadsDialogBreakpoints::do_command(WPARAM cmd, HWND ctl)
{
    int idx;
    HWND lb;
    int bpnum;
    CHtmlDebugHelper *helper = mainwin_->get_helper();
    struct dbgcxdef *dbgctx = mainwin_->get_dbg_ctx();

    /* get the selected item from the listbox */
    lb = GetDlgItem(handle_, IDC_LB_BP);
    idx = SendMessage(lb, LB_GETCURSEL, 0, 0);

    /* get the breakpoint number for the selected item */
    if (idx != LB_ERR)
    {
        /* the breakpoint number is the extra data in the selected item */
        bpnum = (int)SendMessage(lb, LB_GETITEMDATA, (WPARAM)idx, 0);
    }

    switch(LOWORD(cmd))
    {
    case IDOK:
    case IDCANCEL:
        /* dismiss the dialog */
        EndDialog(handle_, 0);
        return TRUE;

    case IDC_LB_BP:
        /* check the notification message */
        switch(HIWORD(cmd))
        {
            case LBN_SELCHANGE:
            case LBN_SELCANCEL:
                /* update the buttons */
                update_buttons();
                break;

            default:
                return FALSE;
        }

        /* handled */
        return TRUE;

    case IDC_BTN_NEWGLOBAL:
        /* add a new global breakpoint */
        {
            textchar_t cond[HTMLDBG_MAXCOND];
            textchar_t buf[HTMLDBG_MAXBPDESC];
            textchar_t errbuf[256];
            int err;
            int stop_on_change;

            /* start out with an empty condition */
            cond[0] = '\0';

            /* 
             *   presume they'll want a stop-on-change breakpoint (if that's
             *   supported by the VM; the TADS 2 VM doesn't support such a
             *   thing, but T3 does) 
             */
            stop_on_change = TRUE;

            /* run the condition dialog */
        run_dlg_newglobal:
            switch(CTadsDialogBpcond::
                   run_bpcond_dlg(cond, sizeof(cond),
                                  &stop_on_change, handle_))
            {
            case IDOK:
                /* try setting the new breakpoint */
                err = helper->set_global_breakpoint(
                    dbgctx, cond, stop_on_change,
                    &bpnum, errbuf, sizeof(errbuf));
                if (err != 0)
                {
                    /* 
                     *   the condition failed - tell the user about it and
                     *   give up 
                     */
                    MessageBox(0, errbuf, "TADS Debugger",
                               MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);

                    /* 
                     *   go back and run the dialog again, without
                     *   changing the text, so that they can correct the
                     *   error or cancel the operation 
                     */
                    goto run_dlg_newglobal;
                }

                /* get the new breakpoint information */
                helper->get_bp_desc(bpnum, buf, sizeof(buf));

                /* add the new breakpoint to the list of breakpoints */
                idx = SendMessage(lb, LB_ADDSTRING, 0, (LPARAM)buf);

                /* set the item's extra data to the breakpoint ID */
                SendMessage(lb, LB_SETITEMDATA, (WPARAM)idx, (LPARAM)bpnum);

                /* select the new item */
                SendMessage(lb, LB_SETCURSEL, (WPARAM)idx, 0);
                update_buttons();

                /* done */
                break;

            default:
                /* don't set a new breakpoint if they cancelled */
                break;
            }
        }
        return TRUE;

    case IDC_BTN_GOTOCODE:
        /* go to the code with the selected breakpoint */
        {
            CHtmlSysWin_win32 *win;

            /* show the code */
            win = (CHtmlSysWin_win32 *)mainwin_->get_helper()->
                  show_bp_code(bpnum, mainwin_);

            /* if we didn't find the window, we can't show the code */
            if (win == 0)
            {
                Beep(1000, 100);
                return TRUE;
            }

            /* dismiss the dialog */
            EndDialog(handle_, 0);

            /* bring the window to the front */
            win->bring_to_front();
        }
        return TRUE;

    case IDC_BTN_CONDITION:
        /* edit the condition for this breakpoint */
        if (idx != LB_ERR)
        {
            textchar_t cond[HTMLDBG_MAXCOND];
            textchar_t buf[HTMLDBG_MAXBPDESC];
            textchar_t errbuf[256];
            int err;
            int stop_on_change;
            
            /* get the condition for this breakpoint */
            if (helper->get_bp_cond(bpnum, cond, sizeof(cond),
                                    &stop_on_change))
                return TRUE;
            
            /* run the dialog */
        run_dlg_cond:
            switch (CTadsDialogBpcond::
                    run_bpcond_dlg(cond, sizeof(cond),
                                   &stop_on_change, handle_))
            {
            case IDOK:
                /* update the condition */
                if ((err = helper->change_bp_cond(dbgctx, bpnum, cond,
                    stop_on_change, errbuf, sizeof(errbuf))) != 0)
                {
                    /* 
                     *   the condition failed - tell the user about it and
                     *   give up 
                     */
                    MessageBox(0, errbuf, "TADS Debugger",
                               MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);

                    /* 
                     *   go back and run the dialog again, without
                     *   changing the text, so that they can correct the
                     *   error or cancel the operation 
                     */
                    goto run_dlg_cond;
                }

                /* get the new breakpoint information */
                helper->get_bp_desc(bpnum, buf, sizeof(buf));

                /* update the list display */
                SendMessage(lb, LB_DELETESTRING, idx, 0);
                SendMessage(lb, LB_INSERTSTRING, idx, (LPARAM)buf);
                SendMessage(lb, LB_SETITEMDATA, (WPARAM)idx, (LPARAM)bpnum);

                /* select the same item */
                SendMessage(lb, LB_SETCURSEL, (WPARAM)idx, 0);
                update_buttons();

                /* done */
                break;

            default:
                /* don't update the condition if they cancelled */
                break;
            }
        }
        return TRUE;

    case IDC_BTN_DISABLEBP:
        /* toggle enabled/disabled status of the breakpoint */
        if (idx != LB_ERR)
        {
            int enabled;
            textchar_t buf[HTMLDBG_MAXBPDESC];

            /* get the current status */
            enabled = helper->is_bp_enabled(bpnum);
            
            /* reverse the status */
            if (helper->enable_breakpoint(dbgctx, bpnum, !enabled))
                return TRUE;

            /* get the new description */
            helper->get_bp_desc(bpnum, buf, sizeof(buf));

            /* update the list display */
            SendMessage(lb, LB_DELETESTRING, idx, 0);
            SendMessage(lb, LB_INSERTSTRING, idx, (LPARAM)buf);
            SendMessage(lb, LB_SETITEMDATA, (WPARAM)idx, (LPARAM)bpnum);

            /* select the same item */
            SendMessage(lb, LB_SETCURSEL, (WPARAM)idx, 0);
            update_buttons();
        }
        return TRUE;

    case IDC_BTN_REMOVE:
        /* remove the selected breakpoint */
        if (idx == LB_ERR || helper->delete_breakpoint(dbgctx, bpnum))
        {
            /* failed - beep in protest */
            Beep(1000, 100);
        }
        else
        {
            /* 
             *   breakpoint successfully deleted - remove this item from
             *   the list 
             */
            SendMessage(lb, LB_DELETESTRING, idx, 0);

            /* update the buttons for the change */
            update_buttons();
        }
        return TRUE;

    case IDC_BTN_REMOVEALL:
        /* if there aren't any breakpoints, there's nothing to do */
        if (SendMessage(lb, LB_GETCOUNT, 0, 0) == 0)
            return TRUE;
        
        /* remove all breakpoints */
        switch(MessageBox(0, "Do you really want to delete all breakpoints?",
                          "TADS Debugger",
                          MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2
                          | MB_TASKMODAL))
        {
        case IDYES:
        case 0:
            /* delete all of the breakpoints */
            helper->delete_all_bps(dbgctx);

            /* clear everything out of the list */
            while (SendMessage(lb, LB_GETCOUNT, 0, 0) != 0)
                SendMessage(lb, LB_DELETESTRING, 0, 0);

            /* update the buttons for the change */
            update_buttons();
            break;

        default:
            break;
        }
        return TRUE;

    default:
        /* inherit default handling */
        return CTadsDialog::do_command(cmd, ctl);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Condition dialog 
 */

/*
 *   initialize 
 */
void CTadsDialogBpcond::init()
{
    /* set the initial condition text */
    SendMessage(GetDlgItem(handle_, IDC_EDIT_COND), WM_SETTEXT,
                0, (LPARAM)condbuf_);

    /* select the mode button */
    CheckRadioButton(handle_, IDC_RB_TRUE, IDC_RB_CHANGED,
                     *stop_on_change_ ? IDC_RB_CHANGED : IDC_RB_TRUE);
}

/*
 *   run the dialog 
 */
int CTadsDialogBpcond::run_bpcond_dlg(textchar_t *cond, size_t condbuflen,
                                      int *stop_on_change, HWND owner)
{
    CTadsDialogBpcond *dlg;
    int ret;

    /* create the dialog */
    dlg = new CTadsDialogBpcond(cond, condbuflen, stop_on_change);

    /* run it */
    ret = dlg->run_modal(DLG_BPCOND, owner,
                         CTadsApp::get_app()->get_instance());

    /* we're done with the dialog - delete it */
    delete dlg;

    /* return the result */
    return ret;
}

/*
 *   process a command 
 */
int CTadsDialogBpcond::do_command(WPARAM cmd, HWND ctl)
{
    switch(cmd)
    {
    case IDOK:
    case IDCANCEL:
        /* copy the field's contents to our buffer */
        SendMessage(GetDlgItem(handle_, IDC_EDIT_COND), WM_GETTEXT,
                    (WPARAM)condbuflen_, (LPARAM)condbuf_);

        /* get the selected stop mode radio button, and tell the caller */
        *stop_on_change_ = (IsDlgButtonChecked(handle_, IDC_RB_CHANGED)
                            == BST_CHECKED);
        
        /* dismiss the dialog */
        EndDialog(handle_, cmd);
        return TRUE;

    default:
        /* inherit default */
        return CTadsDialog::do_command(cmd, ctl);
    }
}

