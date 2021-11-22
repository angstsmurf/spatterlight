#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadsdlg.cpp,v 1.3 1999/07/11 00:46:47 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsdlg.cpp - dialogs
Function
  
Notes
  
Modified
  10/27/97 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <malloc.h>
#include <windows.h>

#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif
#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif

/* ------------------------------------------------------------------------ */
/*
 *   Dialog implementation 
 */

/* list of open dialogs */
CTadsDialog **CTadsDialog::dialogs_ = 0;
size_t CTadsDialog::dialogs_size_ = 0;
size_t CTadsDialog::dialog_cnt_ = 0;


CTadsDialog::CTadsDialog()
{
}

CTadsDialog::~CTadsDialog()
{
    /* make sure I'm not still in the dialog list */
    remove_from_active();
}

/*
 *   Run a modal dialog 
 */
int CTadsDialog::run_modal(unsigned int res_id, HWND owner,
                           HINSTANCE app_inst)
{
    void *ctx;
    int result;

    /* disable windows while the dialog is running */
    if (owner != 0)
        ctx = modal_dlg_pre(owner, FALSE);

    /* run the dialog */
    result = DialogBoxParam(app_inst, MAKEINTRESOURCE(res_id), owner,
                            (DLGPROC)&dialog_proc, (LPARAM)this);

    /* re-enable the windows */
    if (owner != 0)
        modal_dlg_post(ctx);

    /* return the result */
    return result;
}

/*
 *   Open a modeless dialog 
 */
HWND CTadsDialog::open_modeless(unsigned int res_id, HWND owner,
                                HINSTANCE app_inst)
{
    /* create the dialog and return its handle */
    return CreateDialogParam(app_inst, MAKEINTRESOURCE(res_id), owner,
                             (DLGPROC)&dialog_proc, (LPARAM)this);
}

/*
 *   Context structure for modal dialog pre/post routines.  The pre
 *   routine allocates the context, fills it up with information to pass
 *   to the post routine, and the post routine uses the information and
 *   deletes the memory.  
 */
struct modal_dlg_ctx_t
{
    /* number of window handles allocated */
    int count_;

    /* window handle of owner, if we disabled it */
    HWND owner_;

    /* window handles */
    HWND disabled_[1];
};

/*
 *   Disable windows prior to running a modal dialog.  The returned
 *   context must be passed to modal_dlg_post() after the dialog is
 *   dismissed.  
 */
void *CTadsDialog::modal_dlg_pre(HWND owner, int disable_owner)
{
    int i;
    HWND cur;
    modal_dlg_ctx_t *ctx;

    /*
     *   Determine how many windows we'll need to disable, so that we can
     *   allocate memory for a list of those windows.
     */
    for (cur = GetTopWindow(0), i = 0 ; cur != 0 ;
         cur = GetWindow(cur, GW_HWNDNEXT))
    {
        /* if we'll need to disable this window, count it */
        if (GetWindow(cur, GW_OWNER) == owner && IsWindowEnabled(cur))
            ++i;
    }

    /* allocate space to keep track of the ones we disable */
    ctx = (modal_dlg_ctx_t *)th_malloc(sizeof(modal_dlg_ctx_t)
                                       + (i-1)*sizeof(HWND));
    ctx->count_ = i;

    /* 
     *   disable all windows owned by the owner that aren't already
     *   disabled; keep track of the ones we disable so that we can
     *   re-enable them (and only them) when we're done 
     */
    for (cur = GetTopWindow(0), i = 0 ; cur != 0 ;
         cur = GetWindow(cur, GW_HWNDNEXT))
    {
        if (GetWindow(cur, GW_OWNER) == owner && IsWindowEnabled(cur))
        {
            /* remember the window so we can re-enable it later */
            ctx->disabled_[i] = cur;
            ++i;

            /* disable it */
            EnableWindow(cur, FALSE);
        }
    }

    /* disable the owner as well, if appropriate */
    if (disable_owner && IsWindowEnabled(owner))
    {
        ctx->owner_ = owner;
        EnableWindow(owner, FALSE);
    }
    else
        ctx->owner_ = 0;

    /* 
     *   return the list of disabled windows - modal_dlg_post() will use
     *   it to re-enable the windows 
     */
    return ctx;
}

/*
 *   Re-enable windows disabled during a modal dialog 
 */
void CTadsDialog::modal_dlg_post(void *ctx0)
{
    modal_dlg_ctx_t *ctx = (modal_dlg_ctx_t *)ctx0;
    int i;

    /* re-enable all windows we changed */
    for (i = ctx->count_ ; i != 0 ; )
    {
        /* move on to the next window */
        --i;

        /* re-enable it */
        EnableWindow(ctx->disabled_[i], TRUE);
    }

    /* re-enable the owner window if appropriate */
    if (ctx->owner_ != 0)
        EnableWindow(ctx->owner_, TRUE);

    /* free the context structure */
    th_free(ctx);
}

/*
 *   Dialog procedure 
 */
BOOL CALLBACK CTadsDialog::dialog_proc(HWND dlg_hwnd, UINT message,
                                       WPARAM wpar, LPARAM lpar)
{
    CTadsDialog *dlg;

    /* 
     *   if this is the initialization message, get the CTadsDialog object
     *   from the LPARAM, and register the dialog 
     */
    if (message == WM_INITDIALOG)
    {
        dlg = (CTadsDialog *)lpar;
        if (dlg != 0)
            dlg->prepare_to_open(dlg_hwnd);
    }
    else
    {
        /* find the dialog object; abort if not found */
        if ((dlg = find_dialog(dlg_hwnd)) == 0)
            return FALSE;
    }

    /* process the message */
    return dlg->do_dialog_msg(dlg_hwnd, message, wpar, lpar);
}

/*
 *   Initialize the dialog 
 */
void CTadsDialog::init()
{
    RECT drc;
    RECT rc;
    int wid, ht;

    /* get my size */
    GetWindowRect(handle_, &rc);
    wid = rc.right - rc.left;
    ht = rc.bottom - rc.top;
    
    /* center the dialog on the screen */
    GetWindowRect(GetDesktopWindow(), &drc);
    MoveWindow(handle_, drc.left + (drc.right - drc.left - wid)/2,
               drc.top + (drc.bottom - drc.top - ht)/2, wid, ht, TRUE);
}

/*
 *   Process a dialog event 
 */
BOOL CTadsDialog::do_dialog_msg(HWND /*dlg_hwnd*/, UINT message,
                                WPARAM wpar, LPARAM lpar)
{
    switch(message)
    {
    case WM_INITDIALOG:
        /* initialize the dialog */
        init();

        /* initialize focus, and return the result */
        return init_focus((int)wpar);

    case WM_DESTROY:
        /* notify the dialog that the window is being destroyed */
        destroy();
        return TRUE;

    case WM_DRAWITEM:
        /* draw an owner-drawn control */
        return draw_item((int)wpar, (DRAWITEMSTRUCT *)lpar);

    case WM_DELETEITEM:
        /* delete an item in an owner-drawn control */
        return delete_ctl_item((int)wpar, (DELETEITEMSTRUCT *)lpar);

    case WM_COMMAND:
        /* process a command */
        return do_command(wpar, (HWND)lpar);

    case WM_COMPAREITEM:
        /* return the sorting information */
        return compare_ctl_item((int)wpar, (COMPAREITEMSTRUCT *)lpar);

    case WM_NOTIFY:
        /* process a notification message */
        return do_notify((NMHDR *)lpar, (int)wpar);

    default:
        /* didn't handle it */
        return FALSE;
    }
}

/*
 *   receive notification that the Windows dialog is being destroyed
 */
void CTadsDialog::destroy()
{
    /* remove myself from the active dialog list */
    remove_from_active();
}

/*
 *   draw an owner-drawn control 
 */
int CTadsDialog::draw_item(int, DRAWITEMSTRUCT *dis)
{
    /* find the item in the control list and draw it */
    return controls_.draw_item(dis);
}

/*
 *   process a command 
 */
int CTadsDialog::do_command(WPARAM id, HWND)
{
    /* find the item in the control list and send it the command */
    if (controls_.do_command(handle_, id))
        return TRUE;

    /* if we didn't find a control, see if we can handle it ourselves */
    switch(id)
    {
    case IDOK:
    case IDCANCEL:
        /* dismiss the dialog */
        EndDialog(handle_, id);
        return TRUE;

    default:
        /* we couldn't handle it */
        return FALSE;
    }
}

/*
 *   add me to the open dialog list 
 */
void CTadsDialog::add_to_active()
{
    /* if the list isn't big enough, expand it */
    if (dialogs_ == 0)
    {
        dialogs_size_ = 5;
        dialogs_ = (CTadsDialog **)
                   th_malloc(dialogs_size_ * sizeof(*dialogs_));
    }
    else if (dialog_cnt_ >= dialogs_size_)
    {
        dialogs_size_ += 5;
        dialogs_ = (CTadsDialog **)
                   th_realloc(dialogs_, dialogs_size_ * sizeof(*dialogs_));
    }

    /* add myself to the list */
    dialogs_[dialog_cnt_++] = this;
}

/*
 *   remove a dialog from the list 
 */
void CTadsDialog::remove_from_active()
{
    size_t i;
    CTadsDialog **cur;
    
    /* scan the list for me */
    for (i = 0, cur = dialogs_ ; i < dialog_cnt_ ; ++i, ++cur)
    {
        /* see if it's me */
        if (*cur == this)
        {
            /* move everything else in the list down one slot */
            for ( ; i + 1 < dialog_cnt_ ; ++i, ++cur)
                *cur = *(cur + 1);

            /* decrease the count */
            --dialog_cnt_;

            /* 
             *   if the count has dropped to zero, release the memory used
             *   by the list 
             */
            if (dialog_cnt_ == 0)
            {
                th_free(dialogs_);
                dialogs_ = 0;
                dialogs_size_ = 0;
            }

            /* no need to look any further */
            break;
        }
    }
}

/*
 *   find a dialog in the active dialog list, given a window handle 
 */
CTadsDialog *CTadsDialog::find_dialog(HWND hwnd)
{
    CTadsDialog **cur;
    size_t i;

    /* scan the list for a dialog with the given window handle */
    for (i = 0, cur = dialogs_ ; i < dialog_cnt_ ; ++i, ++cur)
    {
        /* if this is the one, return it */
        if ((*cur)->handle_ == hwnd)
            return *cur;
    }

    /* didn't find it */
    return 0;
}

/* our private callback paramaters structure for the font enumerator */
struct ifp_cb_info
{
    HWND ctl;
    int include_proportional;
    int include_fixed;
    int (*selector_func)(void *ctx, ENUMLOGFONTEX *elf, NEWTEXTMETRIC *tm);
    void *selector_func_ctx;
    unsigned int charset_id;
};

/*
 *   font enumeration callback - add the font to the popup if it matches
 *   the font type we're looking for 
 */
int CALLBACK CTadsDialog::init_font_popup_cb(ENUMLOGFONTEX *elf,
                                             NEWTEXTMETRIC *tm,
                                             DWORD /*font_type*/,
                                             LPARAM lpar)
{
    ifp_cb_info *info = (ifp_cb_info *)lpar;
    HWND ctl = info->ctl;
    int proportional;
    int fixed;
    int include_font;

    /* presume we won't include the font */
    include_font = FALSE;

    /* if there's a selector callback function, use it to decide */
    if (info->selector_func != 0)
    {
        /* let the selector decide whether to use it */
        include_font = (*info->selector_func)(info->selector_func_ctx,
                                              elf, tm);
    }
    else
    {
        /* note whether this font is proportional */
        proportional = ((tm->tmPitchAndFamily & TMPF_FIXED_PITCH) != 0);
        fixed = !proportional;

        /* 
         *   add this font name to the popup's list if it matches our
         *   criteria, and it's not a symbol font 
         */
        if (((proportional != 0 && info->include_proportional != 0)
             || (fixed != 0 && info->include_fixed != 0))
            && tm->tmCharSet != SYMBOL_CHARSET
            && (elf->elfLogFont.lfPitchAndFamily & 0xf0) != FF_DECORATIVE)
            include_font = TRUE;
    }

    /* add the font to the control's list if we want to include it */
    if (include_font)
        SendMessage(ctl, CB_ADDSTRING, (WPARAM)0,
                    (LPARAM)elf->elfLogFont.lfFaceName);

    /* continue the enumeration */
    return TRUE;
}

/*
 *   initialize a font popup - load it up with fonts of the appropriate
 *   type 
 */
void CTadsDialog::init_font_popup(int id, ifp_cb_info *info,
                                  const textchar_t *init_setting, int clear)
{
    HWND ctl;
    HDC dc;
    int idx;
    LOGFONT lf;

    /* get the control */
    ctl = GetDlgItem(handle_, id);

    /* empty out the list if the caller so desires */
    if (clear)
        SendMessage(ctl, CB_RESETCONTENT, 0, 0);

    /* get the device context, so we can list its fonts */
    dc = GetDC(handle_);

    /* ask for all fonts in all styles for the requested character set */
    lf.lfFaceName[0] = '\0';
    lf.lfPitchAndFamily = 0;
    lf.lfCharSet = (BYTE)info->charset_id;

    /* enumerate fonts and fill up the popup with font names */
    info->ctl = ctl;
    EnumFontFamiliesEx(dc, &lf, (FONTENUMPROC)init_font_popup_cb, 
                       (LPARAM)info, 0);

    /* select the item with the current setting, if possible */
    idx = SendMessage(ctl, CB_FINDSTRING, (WPARAM)-1, (LPARAM)init_setting);
    if (idx >= 0)
        SendMessage(ctl, CB_SETCURSEL, (WPARAM)idx, 0);

    /* done with the desktop device context */
    ReleaseDC(handle_, dc);
}

/*
 *   initialize a font popup - load it up with fonts of the appropriate
 *   type 
 */
void CTadsDialog::init_font_popup(int id, int include_proportional,
                                  int include_fixed,
                                  const textchar_t *init_setting, int clear,
                                  unsigned int charset_id)
{
    ifp_cb_info info;

    /* enumerate fonts based on fixed/proportional attributes */
    info.include_proportional = include_proportional;
    info.include_fixed = include_fixed;
    info.selector_func = 0;
    info.charset_id = charset_id;
    init_font_popup(id, &info, init_setting, clear);
}

/*
 *   initialize a font popup - load it with fonts selected by a given
 *   callback function 
 */
void CTadsDialog::
   init_font_popup(int id, const textchar_t *init_setting,
                   int (*selector_func)(void *, ENUMLOGFONTEX *,
                                        NEWTEXTMETRIC *),
                   void *selector_func_ctx, int clear,
                   unsigned int charset_id)

{
    ifp_cb_info info;

    /* enumerate fonts based on fixed/proportional attributes */
    info.selector_func = selector_func;
    info.selector_func_ctx = selector_func_ctx;
    info.charset_id = charset_id;
    init_font_popup(id, &info, init_setting, clear);
}

/*
 *   Set up an OPENFILENAME structure to hook to our centering hook 
 */
void CTadsDialog::set_filedlg_center_hook(OPENFILENAME *info)
{
    /* add our flags and hook procedure pointer */
    info->Flags |= OFN_ENABLEHOOK | OFN_EXPLORER;
    info->lpfnHook = &filedlg_center_hook;
}

/*
 *   Hook procedure for GetOpenFileName/GetSaveFileName hook.  This hook
 *   procedure simply centers the dialog on the screen at initialization. 
 */
UINT APIENTRY CTadsDialog::filedlg_center_hook(HWND, UINT msg,
                                               WPARAM, LPARAM lpar)
{
    switch(msg)
    {
    case WM_NOTIFY:
        {
            NMHDR *nm = (NMHDR *)lpar;

            /* check which notification we're receiving */
            switch(nm->code)
            {
            case CDN_INITDONE:
                /* center the dialog on the screen */
                center_dlg_win(nm->hwndFrom);
                
                /* processed */
                return 1;

            default:
                /* ignore others */
                break;
            }
        }
        break;

    default:
        break;
    }

    /* we didn't process the message */
    return 0;
}

/*
 *   Center a dialog window on the screen 
 */
void CTadsDialog::center_dlg_win(HWND dialog_hwnd)
{
    RECT deskrc;
    RECT dlgrc;
    int deskwid, deskht;
    int dlgwid, dlght;

    /* get the desktop area */
    GetWindowRect(GetDesktopWindow(), &deskrc);
    deskwid = deskrc.right - deskrc.left;
    deskht = deskrc.bottom - deskrc.top;

    /* get the dialog box area */
    GetWindowRect(dialog_hwnd, &dlgrc);
    dlgwid = dlgrc.right - dlgrc.left;
    dlght = dlgrc.bottom - dlgrc.top;

    /* center the dialog on the screen */
    MoveWindow(dialog_hwnd,
               deskrc.left + (deskwid - dlgwid)/2,
               deskrc.top + (deskht - dlght)/2,
               dlgwid, dlght, TRUE);
}


/* ------------------------------------------------------------------------ */
/*
 *   Initialize a PROPSHEETPAGE structure for this page 
 */
void CTadsDialogPropPage::init_ps_page(HINSTANCE app_inst,
                                       PROPSHEETPAGE *psp, int dlg_res_id,
                                       int caption_str_id,
                                       int init_page_id, int init_ctl_id,
                                       PROPSHEETHEADER *psh)
{
    /* set up the structure */
    psp->dwSize = sizeof(PROPSHEETPAGE);
    psp->dwFlags = PSP_USETITLE;
    psp->hInstance = app_inst;
    psp->pszTemplate = MAKEINTRESOURCE(dlg_res_id);
    psp->pszIcon = 0;
    psp->pszTitle = MAKEINTRESOURCE(caption_str_id);

    /* hook ourselves up to the PROPSHEETPAGE */
    prepare_prop_page(psp);

    /* if this is the initial page, set its initial control */
    if (init_page_id == dlg_res_id)
    {
        /* set the initial control in the dialog */
        if (init_ctl_id != 0)
            set_init_ctl(init_ctl_id);

        /* set this as the initial page in the header */
        psh->pStartPage = MAKEINTRESOURCE(caption_str_id);
        psh->dwFlags |= PSH_USEPSTARTPAGE;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Property page dialog subclass
 */
BOOL CALLBACK CTadsDialogPropPage::dialog_proc(HWND dlg_hwnd, UINT message,
                                               WPARAM wpar, LPARAM lpar)
{
    /* if this is a WM_INITDIALOG message, set up the property page */
    if (message == WM_INITDIALOG)
    {
        PROPSHEETPAGE *pg;
        CTadsDialogPropPage *dlg;
        
        /* the LPARAM is the propsheetpage structure */
        pg = (PROPSHEETPAGE *)lpar;

        /* 
         *   the lParam in the propsheetpage structure is the
         *   CTadsDialogPropPage object 
         */
        dlg = (CTadsDialogPropPage *)pg->lParam;

        /* initialize the dialog */
        dlg->prepare_to_open(dlg_hwnd);

        /* process the message */
        return dlg->do_dialog_msg(dlg_hwnd, message, wpar, lpar);
    }

    /* 
     *   if this is a notification with PSN_SETACTIVE, get the property
     *   sheet main dialog handle from the NMHDR structure 
     */
    if (message == WM_NOTIFY && ((NMHDR *)lpar)->code == PSN_SETACTIVE)
    {
        CTadsDialogPropPage *dlg;

        /* find the dialog */
        if ((dlg = (CTadsDialogPropPage *)find_dialog(dlg_hwnd)) != 0)
        {
            /* set its sheet handle */
            dlg->sheet_handle_ = ((NMHDR *)lpar)->hwndFrom;
        }
    }

    /* do the normal processing */
    return CTadsDialog::dialog_proc(dlg_hwnd, message, wpar, lpar);
}

/*
 *   Set the changed flag 
 */
void CTadsDialogPropPage::set_changed(int changed)
{
    /* send the appropriate message */
    SendMessage(sheet_handle_, changed ? PSM_CHANGED : PSM_UNCHANGED,
                (WPARAM)handle_, 0);
}


/*
 *   Process a dialog event 
 */
BOOL CTadsDialogPropPage::do_dialog_msg(HWND dlg_hwnd, UINT message,
                                        WPARAM wpar, LPARAM lpar)
{
    switch(message)
    {
    case PSM_QUERYSIBLINGS:
        /* 
         *   Process the query-siblings request reflected from the parent.
         *   Note that we must return the value through our window long value
         *   at DWL_MSGRESULT.  
         */
        SetWindowLong(handle_, DWL_MSGRESULT, query_siblings(wpar, lpar));

        /* indicate that we handled the message */
        return TRUE;

    default:
        /* proceed to default handling */
        break;
    }
        
    /* inherit the default handling */
    return CTadsDialog::do_dialog_msg(dlg_hwnd, message, wpar, lpar);
}

/* ------------------------------------------------------------------------ */
/*
 *   Owner-drawn button implementation 
 */

/*
 *   Draw the button.  This draws the frame structure of the button.
 *   Normally, a subclass's draw method should inherit the default to draw
 *   the frame, then fill in the contents of the face. 
 */
void CTadsOwnerDrawnBtn::draw(DRAWITEMSTRUCT *dis)
{
    HPEN shadow_pen =
        CreatePen(PS_SOLID, 2, GetSysColor(COLOR_3DSHADOW));
    HPEN oldpen;
    int ofs;

    FillRect(dis->hDC, &dis->rcItem, GetSysColorBrush(COLOR_3DFACE));

    /* 
     *   if focused, draw a white edge on the left and top inside the
     *   black edge; if clicked, draw the edge in shadow color instead 
     */
    if (dis->itemState & ODS_FOCUS)
    {
        SelectObject(dis->hDC, (dis->itemState & ODS_SELECTED) == 0 ?
                     GetStockObject(WHITE_PEN) : shadow_pen);
        MoveToEx(dis->hDC, dis->rcItem.left + 1,
                 dis->rcItem.bottom - 1, 0);
        LineTo(dis->hDC, dis->rcItem.left + 1, dis->rcItem.top + 1);
        LineTo(dis->hDC, dis->rcItem.right - 1, dis->rcItem.top + 1);
    }

    /* 
     *   draw the left and top outside edges - white if not focused, black
     *   if focused 
     */
    oldpen = (HPEN)SelectObject(dis->hDC, GetStockObject(
        (dis->itemState & ODS_FOCUS) == 0 ? WHITE_PEN : BLACK_PEN));
    MoveToEx(dis->hDC, dis->rcItem.left, dis->rcItem.bottom - 1, 0);
    LineTo(dis->hDC, dis->rcItem.left, dis->rcItem.top);
    LineTo(dis->hDC, dis->rcItem.right - 1, dis->rcItem.top);

    /* 
     *   draw the right and bottom inside shadows - move these down and
     *   right slightly if the button is clicked 
     */
    ofs = (dis->itemState & ODS_SELECTED)
          ? 0
          : (dis->itemState & ODS_FOCUS) ? -1 : 0;
    SelectObject(dis->hDC, shadow_pen);
    MoveToEx(dis->hDC, dis->rcItem.right - 1 + ofs,
             dis->rcItem.top + 2, 0);
    LineTo(dis->hDC, dis->rcItem.right - 1 + ofs,
           dis->rcItem.bottom - 1 + ofs);
    LineTo(dis->hDC, dis->rcItem.left + 2,
           dis->rcItem.bottom - 1 + ofs);

    /* 
     *   if we have focus and we're not selected, draw another black line
     *   inside the right and bottom edges 
     */
    if (!(dis->itemState & ODS_SELECTED)
        && (dis->itemState & ODS_FOCUS))
    {
        SelectObject(dis->hDC, GetStockObject(BLACK_PEN));
        MoveToEx(dis->hDC, dis->rcItem.right - 2,
                 dis->rcItem.top, 0);
        LineTo(dis->hDC, dis->rcItem.right - 2,
               dis->rcItem.bottom - 2);
        LineTo(dis->hDC, dis->rcItem.left,
               dis->rcItem.bottom - 2);
    }

    /* draw a black edge on the right and bottom */
    SelectObject(dis->hDC, GetStockObject(BLACK_PEN));
    MoveToEx(dis->hDC, dis->rcItem.right - 1, dis->rcItem.top, 0);
    LineTo(dis->hDC, dis->rcItem.right - 1, dis->rcItem.bottom - 1);
    LineTo(dis->hDC, dis->rcItem.left, dis->rcItem.bottom - 1);
    
    SelectObject(dis->hDC, oldpen);
    DeleteObject(shadow_pen);
}

/* ------------------------------------------------------------------------ */
/*
 *   Dialog control list 
 */


CTadsDialogCtlList::~CTadsDialogCtlList()
{
    int i;
    CTadsDialogCtl **ctl;

    /* delete each control */
    for (i = 0, ctl = arr_ ; i < cnt_ ; ++i, ++ctl)
        delete *ctl;

    /* delete the array */
    th_free(arr_);
}

void CTadsDialogCtlList::add(class CTadsDialogCtl *ctl)
{
    /* allocate more space if needed */
    if (arr_size_ <= cnt_)
    {
        arr_size_ += 10;
        arr_ = (class CTadsDialogCtl **)
               th_realloc(arr_, arr_size_ * sizeof(*arr_));
    }

    /* add the new item */
    arr_[cnt_++] = ctl;
}

class CTadsDialogCtl *CTadsDialogCtlList::find(HWND hdl)
{
    int i;
    CTadsDialogCtl **p;

    /* scan our array for a matching handle */
    for (i = 0, p = arr_ ; i < cnt_ ; ++i, ++p)
    {
        /* if this one matches, return the control */
        if ((*p)->gethdl() == hdl)
            return *p;
    }

    /* didn't find it */
    return 0;
}

/*
 *   draw a control 
 */
int CTadsDialogCtlList::draw_item(DRAWITEMSTRUCT *dis)
{
    CTadsDialogCtl *ctl;

    /* if we can find the item, draw it */
    if ((ctl = find(dis->hwndItem)) != 0)
    {
        ctl->draw(dis);
        return TRUE;
    }

    /* didn't find it */
    return FALSE;
}

/*
 *   invoke the command on a given control 
 */
int CTadsDialogCtlList::do_command(HWND ctlhdl)
{
    CTadsDialogCtl *ctl;

    /* if we can find the item, invoke its do_command method */
    if ((ctl = find(ctlhdl)) != 0)
    {
        ctl->do_command();
        return TRUE;
    }

    /* didn't find it */
    return FALSE;
}

