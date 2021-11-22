#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/foldsel2.cpp,v 1.3 1999/07/11 00:46:46 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  foldsel2.cpp - alternative folder selector dialog
Function
  
Notes
  
Modified
  05/09/98 MJRoberts  - Creation
*/

#include <Windows.h>
#include <CommCtrl.h>
#include <shlobj.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <direct.h>

#include "foldsel.h"

/* 
 *   include resources - explicitly force a path search (by using
 *   brackets) so that we can compile this in different directories to get
 *   different resource headers (the installer, for example, uses this
 *   file with its own set of resource ID's) 
 */
#include <foldselr.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif


/*
 *   structure for tracking a drive letter in the drive letter popup box 
 */
struct pop_drive_t
{
    pop_drive_t(char drive_letter, HICON hicon)
    {
        drive_letter_ = drive_letter;
        hicon_ = hicon;
    }
    
    /* drive name */
    char drive_letter_;

    /* icon */
    HICON hicon_;

    /* current directory */
    CStringBuf curdir_;
};

/*
 *   create the folder selector 
 */
CTadsDialogFolderSel2::
   CTadsDialogFolderSel2(const textchar_t *prompt, const textchar_t *caption,
                         const textchar_t *init_folder,
                         int (*testcb)(void *, const char *, int, HWND),
                         void *cbctx)
{
    /* remember the initial folder, if we got one */
    if (init_folder != 0)
    {
        /* use the caller's current folder */
        cur_folder_.set(init_folder);
    }
    else
    {
        char dir[MAX_PATH];

        /* start in the current directory */
        GetCurrentDirectory(sizeof(dir), dir);
        cur_folder_.set(dir);
    }

    /* presume we will not be successful with the starting folder */
    init_success_ = FALSE;

    /* we don't have an index in the popup for this drive yet */
    cur_folder_drive_idx_ = -1;

    /* no desktop interfaces yet */
    dt_folder_ = 0;
    sh_imalloc_ = 0;

    /* no cursors loaded yet */
    busy_cursor_ = arrow_cursor_ = 0;

    /* remember the caption and prompt if given */
    if (caption != 0)
        caption_.set(caption);
    if (prompt != 0)
        prompt_.set(prompt);

    /* remember the callback */
    testcb_ = testcb;
    cbctx_ = cbctx;

    /* 
     *   if there's a callback, invoke it on the initial directory; if it
     *   returns successful, set the initial success flag to indicate
     *   this, which will allow the caller to skip the entire dialog if
     *   they want 
     */
    if (testcb != 0
        && (*testcb)(cbctx, cur_folder_.get(), FALSE, 0))
        init_success_ = TRUE;
}

CTadsDialogFolderSel2::~CTadsDialogFolderSel2()
{
    /* release the desktop interfaces */
    if (dt_folder_ != 0)
        dt_folder_->Release();
    if (sh_imalloc_ != 0)
        sh_imalloc_->Release();

    /* unload our cursors */
    if (arrow_cursor_ != 0)
        DestroyCursor(arrow_cursor_);
    if (busy_cursor_ != 0)
        DestroyCursor(busy_cursor_);
}


/*
 *   initialize the dialog 
 */
void CTadsDialogFolderSel2::init()
{
    /* inherit default behavior */
    CTadsDialog::init();
    
    /* set the caption and prompt, if they were provided */
    if (prompt_.get() != 0)
        SetDlgItemText(handle_, IDC_TXT_PROMPT, prompt_.get());
    if (caption_.get() != 0)
        SendMessage(handle_, WM_SETTEXT, 0, (LPARAM)caption_.get());

    /* remember the small icon size */
    smicon_x_ = GetSystemMetrics(SM_CXSMICON);
    smicon_y_ = GetSystemMetrics(SM_CYSMICON);

    /* load our cursors */
    arrow_cursor_ = LoadCursor(0, IDC_ARROW);
    busy_cursor_ = LoadCursor(0, IDC_WAIT);

    /* 
     *   Calculate the DWORD parameter for IExtractIcon::Extract.  This is
     *   a weird encoding where the low 16 bits give the large icon size,
     *   and the high 16 bits give the small icon size.  We'll use the
     *   system settings for these and code up the DWORD value.  
     */
    icon_extract_size_ = MAKELONG(GetSystemMetrics(SM_CXICON),
                                  GetSystemMetrics(SM_CXSMICON));

    /* get the desktop folder and IMalloc */
    SHGetDesktopFolder(&dt_folder_);
    SHGetMalloc(&sh_imalloc_);

    /* initialize the drive letter popup */
    init_drives();

    /* build the initial folder list */
    build_folder_lb();
}

/*
 *   destroy 
 */
void CTadsDialogFolderSel2::destroy()
{
    /* 
     *   clear out the combo and list boxes explicitly - NT doesn't seem to
     *   do this automatically on destroying the controls, so do it manually
     *   to make sure our allocated list data items are deleted 
     */
    SendMessage(GetDlgItem(handle_, IDC_CBO_DRIVES), CB_RESETCONTENT, 0, 0);
    SendMessage(GetDlgItem(handle_, IDC_LB_FOLDERS), LB_RESETCONTENT, 0, 0);

    /* inherit default */
    CTadsDialog::destroy();
}

/*
 *   initialize the drive-letter popup 
 */
void CTadsDialogFolderSel2::init_drives()
{
    char buf[512];
    char *p;
    HWND drives;

    /* get the drive letter selector popup */
    drives = GetDlgItem(handle_, IDC_CBO_DRIVES);

    /* clear the list */
    SendMessage(drives, CB_RESETCONTENT, 0, 0);

    /* get the list of drive letters */
    if (GetLogicalDriveStrings(sizeof(buf), buf))
    {
        char curdrv;

        /* get the currently selected drive */
        if (cur_folder_.get() != 0
            && get_strlen(cur_folder_.get()) > 1
            && cur_folder_.get()[1] == ':')
        {
            /* get the active drive letter */
            curdrv = cur_folder_.get()[0];
            if (islower(curdrv))
                curdrv = (char)toupper(curdrv);
        }
        else
        {
            /* no active drive - don't select anything initially */
            curdrv = '\0';
        }

        /* go through the drives and add each one to the list */
        for (p = buf ; *p != '\0' ; p += strlen(p) + 1)
        {
            pop_drive_t *info;
            HICON hicon;
            int idx;

            /* convert to the drive letter to caps if it's not already */
            if (islower(*p))
                *p = (char)toupper(*p);

            /* get the icon for this drive */
            hicon = get_item_icon(p, FALSE);
            
            /* allocate this drive letter structure */
            info = new pop_drive_t(*p, hicon);

            /* add it to the popup list */
            idx = SendMessage(drives, CB_ADDSTRING, 0, (LPARAM)info);

            /* 
             *   if this drive letter matches the current directory's
             *   drive letter, choose this item 
             */
            if (cur_folder_.get() != 0 && *p == curdrv)
            {
                /* select this drive */
                SendMessage(drives, CB_SETCURSEL, idx, 0);

                /* 
                 *   remember the current folder as the current directory
                 *   for this drive 
                 */
                info->curdir_.set(cur_folder_.get());

                /* remember this as the current drive index */
                cur_folder_drive_idx_ = idx;
            }
            else
            {
                char dir[MAX_PATH];
                
                /* get the working directory on this drive */
                _getdcwd(*p - 'A' + 1, dir, sizeof(dir));

                /* remember it for this drive */
                info->curdir_.set(dir);
            }
        }
    }
}


/*
 *   build the folder list for the current directory 
 */
void CTadsDialogFolderSel2::build_folder_lb()
{
    HWND lb;
    int level;
    int idx;
    HANDLE hfind;
    char pat[MAX_PATH];
    WIN32_FIND_DATA findinfo;
    size_t len;
    lb_folder_t *item;

    /* get the listbox */
    lb = GetDlgItem(handle_, IDC_LB_FOLDERS);

    /* turn on the busy cursor while we're working */
    SetCursor(busy_cursor_);

    /* don't redraw the list while we're working */
    SendMessage(lb, WM_SETREDRAW, FALSE, 0);

    /* populate the listbox */
    item = populate_folder_tree(lb, TRUE, cur_folder_.get());

    /* update the text field to match the selected item */
    if (item != 0)
        SetDlgItemText(handle_, IDC_FLD_FOLDER_PATH, item->fullpath_.get());

    /* if there's no folder, there's nothing else to do */
    if (cur_folder_.get() == 0 || cur_folder_.get()[0] == '\0')
        goto done;

    /* set up a pattern to find everything in the current folder */
    len = get_strlen(cur_folder_.get());
    memcpy(pat, cur_folder_.get(), len);
    if (pat[len - 1] != '\\')
        pat[len++] = '\\';
    strcpy(pat + len, "*.*");
    
    /* search items are children of the last item added */
    level = (item != 0 ? item->level_ + 1 : 0);

    /* 
     *   search for all folders within the current folder, adding each
     *   folder within the current folder to the list 
     */
    hfind = FindFirstFile(pat, &findinfo);
    while (hfind != INVALID_HANDLE_VALUE)
    {
        /* 
         *   if this file is a directory, and it's not marked as hidden or
         *   system, and it's not "." or "..", include it in the list 
         */
        if ((findinfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0
            && (findinfo.dwFileAttributes
                & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) == 0
            && strcmp(findinfo.cFileName, ".") != 0
            && strcmp(findinfo.cFileName, "..") != 0)
        {
            char fullname[MAX_PATH];
            
            /* build the full name of this file */
            memcpy(fullname, pat, len);
            strcpy(fullname + len, findinfo.cFileName);

            /* set up a folder entry for it */
            item = new lb_folder_t(findinfo.cFileName,
                                   strlen(findinfo.cFileName),
                                   fullname, strlen(fullname), level);

            /* get its icons */
            item->normal_icon_ = get_item_icon(item->fullpath_.get(), FALSE);
            item->open_icon_ = get_item_icon(item->fullpath_.get(), TRUE);

            /* add it to the list */
            idx = SendMessage(lb, LB_ADDSTRING, 0, (LPARAM)item);
        }

        /* find the next object */
        if (!FindNextFile(hfind, &findinfo))
        {
            /* no more files - close the search */
            FindClose(hfind);
            hfind = INVALID_HANDLE_VALUE;
        }
    }

done:
    /* turn off the busy cursor now that we're done */
    SetCursor(busy_cursor_);

    /* restore drawing in the list now that we've finished building it */
    SendMessage(lb, WM_SETREDRAW, TRUE, 0);
}


/*
 *   Set the current directory 
 */
void CTadsDialogFolderSel2::set_cur_dir(const char *path)
{
    HWND cbo;
    int idx;
    
    /* remember the active current directory */
    cur_folder_.set(path);

    /* get the current drive combo box item */
    cbo = GetDlgItem(handle_, IDC_CBO_DRIVES);
    if (cbo != 0
        && (idx = SendMessage(cbo, CB_GETCURSEL, 0, 0)) != CB_ERR)
    {
        pop_drive_t *drive;

        /* get our data for this drive */
        drive = (pop_drive_t *)SendMessage(cbo, CB_GETITEMDATA, idx, 0);
        if (drive != 0)
            drive->curdir_.set(path);
    }
}

/*
 *   run a command 
 */
int CTadsDialogFolderSel2::do_command(WPARAM id, HWND hwnd)
{
    switch(LOWORD(id))
    {
    case IDOK:
        {
            char buf[MAX_PATH];
            
            /* 
             *   copy the current contents of the text field into the
             *   current folder -- if they typed something in manually, we
             *   want to return their typed value 
             */
            GetDlgItemText(handle_, IDC_FLD_FOLDER_PATH, buf, sizeof(buf));
            cur_folder_.set(buf);

            /*
             *   If there's a client test callback, call it now to
             *   determine if the caller will allow dismissal.  If not,
             *   keep running the dialog.  
             */
            if (testcb_ != 0 && !(*testcb_)(cbctx_, buf, TRUE, handle_))
                return TRUE;

            /* inherit the default dismissal handling */
            return CTadsDialog::do_command(id, hwnd);
        }
        
    case IDC_CBO_DRIVES:
        switch(HIWORD(id))
        {
        case CBN_SELCHANGE:
            {
                int idx;
                HWND drives;

                /* get the drive popup */
                drives = GetDlgItem(handle_, IDC_CBO_DRIVES);

                /* get the new drive */
                idx = SendMessage(drives, CB_GETCURSEL, 0, 0);
                if (idx != CB_ERR)
                {
                    pop_drive_t *dlinfo;
                    
                    /* get the selected item data */
                    dlinfo = (pop_drive_t *)
                             SendMessage(drives, CB_GETITEMDATA, idx, 0);

                    /* switch to the current directory for this drive */
                    set_cur_dir(dlinfo->curdir_.get());

                    /* update the folder list to reflect the new directory */
                    build_folder_lb();
                }
            }

            /* handled */
            return TRUE;

        default:
            /* inherit default */
            return CTadsDialog::do_command(id, hwnd);
        }

    case IDC_LB_FOLDERS:
        {
            HWND lb;
            int idx;

            /* get the listbox */
            lb = GetDlgItem(handle_, IDC_LB_FOLDERS);

            /* get the selected item */
            idx = SendMessage(lb, LB_GETCURSEL, 0, 0);
            if (idx != LB_ERR)
            {
                lb_folder_t *finfo;
                    
                /* get the selected item data */
                finfo = (lb_folder_t *)
                        SendMessage(lb, LB_GETITEMDATA, idx, 0);

                /* see what we're being notified of */
                switch(HIWORD(id))
                {
                case LBN_SELCHANGE:
                    /* 
                     *   the selection changed - select the new path into
                     *   the edit box 
                     */
                    SetDlgItemText(handle_, IDC_FLD_FOLDER_PATH,
                                   finfo->fullpath_.get());
                    return TRUE;

                case LBN_DBLCLK:
                    /* 
                     *   Double-clicked - open the selected item.  To do
                     *   this, select the item's path as the current
                     *   directory, and rebuild the list.  Note that if
                     *   the current item is already the selected item, we
                     *   can avoid all this work entirely.  
                     */
                    if (stricmp(cur_folder_.get(),
                                finfo->fullpath_.get()) != 0)
                    {
                        /* select the new current directory */
                        set_cur_dir(finfo->fullpath_.get());

                        /*
                         *   If the client registered a test callback,
                         *   invoke it now, and see if the client wants to
                         *   dismiss the dialog. 
                         */
                        if (testcb_ != 0
                            && (*testcb_)(cbctx_, cur_folder_.get(), FALSE,
                                          handle_))
                        {
                            /* dismiss the dialog */
                            EndDialog(handle_, IDOK);

                            /* we've handled the command */
                            return TRUE;
                        }

                        /* rebuild the list for this new current folder */
                        build_folder_lb();
                    }
                    return TRUE;

                default:
                    /* we don't do anything special with other events */
                    break;
                }
            }

            /* we didn't handle it - inherit default handling */
            return CTadsDialog::do_command(id, hwnd);
        }

    case IDC_BTN_NETWORK:
        /* 
         *   run the network dialog - if the user maps a new drive,
         *   rebuild the drive list 
         */
        if (WNetConnectionDialog(handle_, RESOURCETYPE_DISK) == NOERROR)
            init_drives();
        
        /* handled */
        return TRUE;

    default:
        /* inherit default */
        return CTadsDialog::do_command(id, hwnd);
    }
}


/* 
 *   draw an owner-drawn item 
 */
int CTadsDialogFolderSel2::draw_item(int ctl_id, DRAWITEMSTRUCT *draw_info)
{
    switch(ctl_id)
    {
    case IDC_CBO_DRIVES:
        {
            pop_drive_t *dlinfo;
            char buf[20];
            COLORREF newbg, newfg;
            COLORREF oldbg, oldfg;
            HBRUSH hbr;
            int icon_x;
            int txt_y;
            TEXTMETRIC tm;

            /* 
             *   get the text drawing point so that the text is centered
             *   vertically within the drawing area 
             */
            GetTextMetrics(draw_info->hDC, &tm);
            txt_y = draw_info->rcItem.top +
                    (draw_info->rcItem.bottom - draw_info->rcItem.top
                     - tm.tmHeight)/2;

            /* offset the icon slightly from the left edge */
            icon_x = draw_info->rcItem.left + 4;

            /* set the colors according to the selection state */
            newfg = GetSysColor(draw_info->itemState & ODS_SELECTED ? 
                                COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT);
            oldfg = SetTextColor(draw_info->hDC, newfg);

            newbg = GetSysColor(draw_info->itemState & ODS_SELECTED ? 
                                COLOR_HIGHLIGHT : COLOR_WINDOW);
            oldbg = SetBkColor(draw_info->hDC, newbg);

            /* erase the area in the background color */
            hbr = CreateSolidBrush(newbg);
            FillRect(draw_info->hDC, &draw_info->rcItem, hbr);
            DeleteObject(hbr);

            /* draw the item contents only if it's non-empty */
            if (draw_info->itemID != -1)
            {
                /* get our item data */
                dlinfo = (pop_drive_t *)draw_info->itemData;
            
                /* draw the icon */
                DrawIconEx(draw_info->hDC, icon_x,
                           draw_info->rcItem.top, dlinfo->hicon_,
                           smicon_x_, smicon_y_, 0, 0, DI_NORMAL);

                /* draw the name */
                sprintf(buf, " %c:", dlinfo->drive_letter_);
                TextOut(draw_info->hDC, icon_x + smicon_x_, txt_y,
                        buf, strlen(buf));
            }

            /* draw the focus rectangle if appropriate */
            if (draw_info->itemState & ODS_FOCUS)
                DrawFocusRect(draw_info->hDC, &draw_info->rcItem);

            /* restore the color scheme */
            SetTextColor(draw_info->hDC, oldfg);
            SetBkColor(draw_info->hDC, oldbg);

            /* handled */
            return TRUE;
        }

    case IDC_LB_FOLDERS:
        /* draw the item */
        draw_folder_item(draw_info);

        /* handled */
        return TRUE;

    default:
        /* not handled */
        return FALSE;
    }
}

/* 
 *   compare owner-drawn items in a list
 */
int CTadsDialogFolderSel2::
   compare_ctl_item(int ctl_id, COMPAREITEMSTRUCT *cmp_info)
{
    switch(ctl_id)
    {
    case IDC_LB_FOLDERS:
        {
            lb_folder_t *item1, *item2;

            /* get the two items */
            item1 = (lb_folder_t *)cmp_info->itemData1;
            item2 = (lb_folder_t *)cmp_info->itemData2;

            /*
             *   If the items are not at the same nesting level, the one
             *   at the lower level goes first 
             */
            if (item1->level_ != item2->level_)
                return item1->level_ - item2->level_;

            /* 
             *   The items are at the same nesting level, so compare the
             *   displayed folder names (not the full path name, just the
             *   part that's displayed) 
             */
            return stricmp(item1->foldername_.get(),
                           item2->foldername_.get());
        }
        
    default:
        /* return "equivalent" for any other items */
        return 0;
    }
}

/* 
 *   delete an owner-allocated control item 
 */
int CTadsDialogFolderSel2::
   delete_ctl_item(int ctl_id, DELETEITEMSTRUCT *delete_info)
{
    switch(ctl_id)
    {
    case IDC_CBO_DRIVES:
        /* delete the drive letter information */
        delete (pop_drive_t *)delete_info->itemData;
        return TRUE;

    case IDC_LB_FOLDERS:
        /* delete the folder information */
        delete (lb_folder_t *)delete_info->itemData;
        return TRUE;

    default:
        /* not handled */
        return FALSE;
    }
}


/*
 *   Run the dialog 
 */
int CTadsDialogFolderSel2::
   run_select_folder(HWND owner, HINSTANCE app_inst,
                     const textchar_t *prompt,  const textchar_t *caption,
                     textchar_t *selected_folder, size_t sel_buf_len,
                     const textchar_t *start_folder,
                     int (*testcb)(void *, const char *, int, HWND),
                     void *cbctx)
{
    CTadsDialogFolderSel2 *dlg;
    int id;
    int result;
    
    /* create the dialog */
    dlg = new CTadsDialogFolderSel2(prompt, caption, start_folder,
                                    testcb, cbctx);

    /* 
     *   if the callback accepted the initial directory during
     *   initialization, there's no need to show the dialog; otherwise,
     *   run the dialog 
     */
    if (dlg->is_init_success())
    {
        /* 
         *   we were successful with the initial directory - pretend we
         *   ran and they hit OK 
         */
        id = IDOK;
    }
    else
    {
        /* run the dialog */
        id = dlg->run_modal(IDD_FOLDER_SELECTOR2, owner, app_inst);
    }

    /* if they didn't cancel, retrieve the selected folder */
    if (id == IDOK)
    {
        size_t copylen;
        
        /* if the buffer is too small, truncate the result */
        copylen = get_strlen(dlg->get_folder_sel());
        if (copylen > sel_buf_len - sizeof(textchar_t))
            copylen = sel_buf_len - sizeof(textchar_t);

        /* copy the result and null-terminate it */
        memcpy(selected_folder, dlg->get_folder_sel(), copylen);
        selected_folder[copylen] = '\0';
        
        /* indicate that a selection was successfully made */
        result = TRUE;
    }
    else
    {
        /* they cancelled - there's no folder selection to retrieve */
        result = FALSE;
    }

    /* delete the dialog and return the result */
    delete dlg;
    return result;
}


/* ------------------------------------------------------------------------ */
/*
 *   Folder display manager.  
 */

/*
 *   Get the icon for a given file object 
 */
HICON CTadsFolderControl::get_item_icon(const char *fname, int open)
{
    SHFILEINFO shinfo;

    /* get the information from the shell */
    SHGetFileInfo(fname, 0, &shinfo, sizeof(shinfo),
                  SHGFI_ICON | SHGFI_SMALLICON | (open ? SHGFI_OPENICON : 0));
    return shinfo.hIcon;
}

/*
 *   Populate a listbox, popup, or combo with a hierarchical display for the
 *   given folder.  'is_lb' indicates the type of control: true means it's a
 *   listbox, false means it's a combo/popup.
 *   
 *   Returns the last item added, which we also set as the selected item.
 *   This item represents the last directory in the path.  
 */
lb_folder_t *CTadsFolderControl::populate_folder_tree(
    HWND ctl, int is_lb, const char *path)
{
    int extra;
    const char *curtok;
    const char *sep;
    int level;
    int idx;
    lb_folder_t *item;

    /* clear the list */
    SendMessage(ctl, is_lb ? LB_RESETCONTENT : CB_RESETCONTENT, 0, 0);

    /* if there's no current folder, there's nothing to do */
    if (path == 0 || path[0] == '\0')
        return 0;

    /* 
     *   for the first token, include one extra character beyond what we'd
     *   usually include, since we need to include the backslash in the root
     *   folder's name 
     */
    extra = 1;

    /* we haven't added any items yet */
    item = 0;

    /*
     *   Parse the current folder.  For each token, create a listbox entry
     *   for that parent folder.  
     */
    for (curtok = path, level = 0 ; *curtok != '\0' ; ++level)
    {
        /* find the next separator */
        for (sep = curtok ; *sep != '\0' && *sep != '\\' ; ++sep) ;

        /* set up a folder entry for this parent folder */
        item = new lb_folder_t(curtok, sep - curtok + extra,
                               path, sep - path + extra,
                               level);

        /* get this item's icon */
        item->normal_icon_ = get_item_icon(item->fullpath_.get(), FALSE);
        item->open_icon_ = get_item_icon(item->fullpath_.get(), TRUE);

        /* mark this item as open */
        item->is_open_ = TRUE;

        /* add it to the listbox */
        idx = SendMessage(ctl, is_lb ? LB_ADDSTRING : CB_ADDSTRING,
                          0, (LPARAM)item);

        /* move on to the next folder */
        curtok = sep;
        if (*curtok == '\\')
            ++curtok;

        /* don't include any extra characters on the next token */
        extra = 0;
    }

    /* select the last item we added */
    if (item != 0)
    {
        /* select the item in the list */
        SendMessage(ctl, is_lb ? LB_SETCURSEL : CB_SETCURSEL, idx, 0);
    }

    /* return the selected item */
    return item;
}

/*
 *   Draw an item in a listbox/combo/popup showing folder items.  
 */
void CTadsFolderControl::draw_folder_item(DRAWITEMSTRUCT *draw_info)
{
    COLORREF newbg, newfg;
    COLORREF oldbg, oldfg;
    HBRUSH hbr;
    int icon_x;
    int txt_y;
    TEXTMETRIC tm;

    /* 
     *   get the text drawing point so that the text is centered vertically
     *   within the drawing area 
     */
    GetTextMetrics(draw_info->hDC, &tm);
    txt_y = draw_info->rcItem.top +
            (draw_info->rcItem.bottom - draw_info->rcItem.top
             - tm.tmHeight)/2;

    /* offset the icon slightly from the left edge */
    icon_x = draw_info->rcItem.left + 4;

    /* set the colors according to the selection state */
    newfg = GetSysColor(draw_info->itemState & ODS_SELECTED ? 
                        COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT);
    oldfg = SetTextColor(draw_info->hDC, newfg);

    newbg = GetSysColor(draw_info->itemState & ODS_SELECTED ? 
                        COLOR_HIGHLIGHT : COLOR_WINDOW);
    oldbg = SetBkColor(draw_info->hDC, newbg);

    /* erase the area in the background color */
    hbr = CreateSolidBrush(newbg);
    FillRect(draw_info->hDC, &draw_info->rcItem, hbr);
    DeleteObject(hbr);

    /* draw the item contents only if it's non-empty */
    if (draw_info->itemID != -1)
    {
        lb_folder_t *finfo;

        /* get our item data */
        finfo = (lb_folder_t *)draw_info->itemData;

        /* 
         *   Indent the item by its nesting level.
         *   
         *   If this is a combo box, and we're drawing in the combo box
         *   portion, don't indent the item - for a combo box, only indent
         *   items in the drop-down list.  
         */
        if ((draw_info->itemState & ODS_COMBOBOXEDIT) == 0)
            icon_x += finfo->level_ * 4;

        /* draw the icon */
        DrawIconEx(draw_info->hDC, icon_x,
                   draw_info->rcItem.top,
                   (finfo->is_open_ ?
                    finfo->open_icon_ : finfo->normal_icon_),
                   smicon_x_, smicon_y_, 0, 0, DI_NORMAL);

        /* draw the name */
        TextOut(draw_info->hDC, icon_x + smicon_x_, txt_y,
                finfo->foldername_.get(),
                get_strlen(finfo->foldername_.get()));
    }

    /* draw the focus rectangle if appropriate */
    if (draw_info->itemState & ODS_FOCUS)
        DrawFocusRect(draw_info->hDC, &draw_info->rcItem);
    
    /* restore the color scheme */
    SetTextColor(draw_info->hDC, oldfg);
    SetBkColor(draw_info->hDC, oldbg);
}


/* ------------------------------------------------------------------------ */
/*
 *   test section
 */


#ifdef BUILD_TEST_PROG

/* ------------------------------------------------------------------------ */
/*
 *   A couple of dummy entrypoints for library routines.  We don't need to
 *   display any debugging information, so these don't do anything here. 
 */
void os_dbg_sys_msg(const textchar_t *) { }
void oshtml_dbg_printf(const char *, ...) { }

/* ------------------------------------------------------------------------ */
/*
 *   Main routine 
 */
int PASCAL WinMain(HANDLE inst, HANDLE pinst, LPSTR cmdline, int cmdshow)
{
    textchar_t buf[MAX_PATH];
    
    /* initialize the common controls (for the tree control) */
    InitCommonControls();

    /* run the dialog */
    if (CTadsDialogFolderSel2::
        run_select_folder(0, inst, "Installation &Path:",
                          "Select Installation Folder",
                          buf, sizeof(buf), 0, 0, 0))
        MessageBox(0, buf, "Success!", MB_OK | MB_ICONINFORMATION);
    else
        MessageBox(0, "Cancelled.", "Status", MB_OK | MB_ICONINFORMATION);

    /* success */
    return 0;
}

#endif
