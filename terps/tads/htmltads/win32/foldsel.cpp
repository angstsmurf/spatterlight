#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/foldsel.cpp,v 1.1 1999/07/11 00:46:46 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  foldsel.cpp - folder selection dialog
Function
  
Notes
  
Modified
  05/04/98 MJRoberts  - Creation
*/


#include <Windows.h>
#include <CommCtrl.h>
#include <shlobj.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "foldsel.h"
#include "foldselr.h"

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif
#ifndef HTMLHASH_H
#include "htmlhash.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Hash table entry for icon object - stores the filename and icon index
 *   of the icon.  
 */
class CHtmlHashEntryIcon: public CHtmlHashEntryCI
{
public:
    CHtmlHashEntryIcon(const textchar_t *str, size_t len, int copy,
                       int our_index)
        : CHtmlHashEntryCI(str, len, copy) { our_index_ = our_index; }

    /* index in our image list of the icon */
    int our_index_;

    /*
     *   Build the key for this object from the filename and icon index 
     */
    static void build_key(char *keybuf, const char *filename, int file_index)
    {
        sprintf(keybuf, "%s:%x", filename, file_index);
    }
};


/* ------------------------------------------------------------------------ */
/*
 *   Folder selector dialog implementation 
 */

CTadsDialogFolderSel::CTadsDialogFolderSel(const textchar_t *prompt,
                                           const textchar_t *caption,
                                           const textchar_t *init_folder)
{
    /* we don't have the desktop interfaces yet */
    dt_folder_ = 0;
    sh_imalloc_ = 0;
    my_computer_ = 0;
    imagelist_ = 0;
    
    /* load our cursors */
    arrow_cursor_ = LoadCursor(0, IDC_ARROW);
    busy_cursor_ = LoadCursor(0, IDC_WAIT);
    
    /* create our hash table */
    image_hashtab_ = new CHtmlHashTable(256, new CHtmlHashFuncCI());
    
    /* remember our initial folder */
    if (init_folder != 0)
    {
        /* save their initial directory */
        init_folder_.set(init_folder);
    }
    else
    {
        char buf[MAX_PATH];
        
        /* start off in the current system directory */
        GetCurrentDirectory(sizeof(buf), buf);
        init_folder_.set(buf);
    }
    
    /* we don't have an initial selection yet */
    post_init_sel_ = 0;

    /* remember the caption and prompt if given */
    if (caption != 0)
        caption_.set(caption);
    if (prompt != 0)
        prompt_.set(prompt);
}

CTadsDialogFolderSel::~CTadsDialogFolderSel()
{
    /* delete the "My Computer" item ID list */
    if (my_computer_ != 0)
        sh_imalloc_->Free(my_computer_);

    /* release the desktop folder interface, if we got it */
    if (dt_folder_ != 0)
        dt_folder_->Release();

    /* done with the task allocator */
    if (sh_imalloc_ != 0)
        sh_imalloc_->Release();

    /* delete our image list */
    if (imagelist_ != 0)
        ImageList_Destroy(imagelist_);

    /* destroy our cursors */
    if (arrow_cursor_ != 0)
        DestroyCursor(arrow_cursor_);
    if (busy_cursor_ != 0)
        DestroyCursor(busy_cursor_);

    /* delete our hash table */
    delete image_hashtab_;
}


/*
 *   Get the ANSI string for a given STRRET.  Returns a pointer to the
 *   string.  If we have to convert from Unicode, we'll translate the
 *   string into the cStr buffer.  
 */
char *CTadsDialogFolderSel::cvt_ansi(ITEMIDLIST *item, STRRET *nm)
{
    int len;

    switch(nm->uType)
    {
    case STRRET_CSTR:
        /* it's already in cStr */
        return nm->cStr;

    case STRRET_OFFSET:
        /* it's already in ANSI format, but in the ID list */
        return ((char *)item) + nm->uOffset;

    case STRRET_WSTR:
        /* convert it, using the cStr buffer to hold the result */
        len = WideCharToMultiByte(CP_ACP, 0, nm->pOleStr, -1,
                                  nm->cStr, sizeof(nm->cStr), 0, 0);
        nm->cStr[len] = '\0';

        /* return the buffer */
        return nm->cStr;

    default:
        return 0;
    }
}

/*
 *   Extract an icon for an object and store it in our image list.
 *   Returns the index of the entry in our image list. 
 */
int CTadsDialogFolderSel::extract_icon(IShellFolder *folder,
                                       ITEMIDLIST *item, int open)
{
    IExtractIcon *iextract;
    int idx;

    /* 
     *   presume we won't find it - use the first image in the list by
     *   default 
     */
    idx = 0;

    /* get the icon extractor interface */
    if (folder->GetUIObjectOf(0, 1, (LPCITEMIDLIST *)&item,
                              IID_IExtractIcon, 0,
                              (void **)&iextract) == NOERROR)
    {
        UINT flags;
        char iconfile[MAX_PATH];
        char keybuf[MAX_PATH + 10];
        HICON smicon, lgicon;
        int icon_id;

        /* ask the extractor for the icon */
        if (iextract->GetIconLocation(GIL_FORSHELL
                                      | (open ? GIL_OPENICON : 0),
                                      iconfile, sizeof(iconfile),
                                      &icon_id, &flags) == NOERROR)
        {
            CHtmlHashEntryIcon *entry;
            
            /* check to see if we already have this icon in our list */
            CHtmlHashEntryIcon::build_key(keybuf, iconfile, icon_id);
            entry = (CHtmlHashEntryIcon *)
                    image_hashtab_->find(keybuf, strlen(keybuf));
            if (entry != 0)
            {
                /*
                 *   We already have it in our table - use the existing
                 *   entry 
                 */
                idx = entry->our_index_;
            }
            else
            {
                /* 
                 *   we don't already have the icon in our image list --
                 *   extract the icon from the file 
                 */
                iextract->Extract(iconfile, icon_id,
                                  &lgicon, &smicon, extract_size_);
            
                /* add it to our image list */
                idx = ImageList_AddIcon(imagelist_, smicon);

                /* 
                 *   add it to our hash table, so we find the same one
                 *   again next time 
                 */
                image_hashtab_->
                    add(new CHtmlHashEntryIcon(keybuf, strlen(keybuf),
                                               TRUE, idx));
            }
        }

        /* done with the extractor interface */
        iextract->Release();
    }

    /* return the image list index */
    return idx;
}

/*
 *   Add the contents of a given shell folder interface to the tree under
 *   the given parent.  Returns the number of children added.  
 */
int CTadsDialogFolderSel::add_contents(HWND tree, HTREEITEM parent,
                                       IShellFolder *folder, int top_level)
{
    IEnumIDList *penum;
    TV_INSERTSTRUCT insitem;
    int item_cnt;

    /* we haven't added anything yet */
    item_cnt = 0;

    /* enumerate the contents */
    if (folder->EnumObjects(0, SHCONTF_FOLDERS, &penum) == NOERROR)
    {
        /* scan the contents of the enumerator */
        for (;;)
        {
            HRESULT result;
            ITEMIDLIST *item;
            ULONG cnt;
            STRRET dispname;
            char *pdisp;
            ULONG attr;
            int is_folder, is_filecont, is_filesys;

            /* get the next item, and stop if we're done */
            if (penum->Next(1, &item, &cnt) != NOERROR || cnt != 1)
                break;

            /* get the display name */
            dispname.uType = STRRET_CSTR;
            folder->GetDisplayNameOf(item, SHGDN_NORMAL, &dispname);
            pdisp = cvt_ansi(item, &dispname);

            /* get the attributes */
            attr = SFGAO_FILESYSTEM | SFGAO_FILESYSANCESTOR | SFGAO_FOLDER;
            result = folder->
                     GetAttributesOf(1, (LPCITEMIDLIST *)&item, &attr);

            /* 
             *   Note whether we're a folder, a file system object, and a
             *   file system ancestor (container) object (i.e., an object
             *   that can contain file system objects).  We only display
             *   items that are file system objects or that contain file
             *   system objects, and we only allow selecting file system
             *   objects.  
             */
            is_folder = ((attr & SFGAO_FOLDER) != 0);
            is_filesys = ((attr & SFGAO_FILESYSTEM) != 0);
            is_filecont = ((attr & SFGAO_FILESYSANCESTOR) != 0);

            /* 
             *   display only if it's a file system object or can contain
             *   file system objects; in any case, only display folders 
             */
            if ((is_filesys || is_filecont) && is_folder)
            {
                HTREEITEM hitem;
                SHFILEINFO shinfo;
                int icon_id, open_icon_id;
                DWORD parattr;
                IShellFolder *grandparfolder;

                /* extract icons into our image list */
                icon_id = extract_icon(folder, item, FALSE);
                open_icon_id = extract_icon(folder, item, TRUE);

                /* get the folder containing the parent item */
                grandparfolder = get_parent_folder(tree, parent);
                if (grandparfolder != 0)
                {
                    TV_ITEM info;
                    
                    /* get the parent item ID list */
                    info.hItem = parent;
                    info.mask = TVIF_PARAM;
                    SendMessage(tree, TVM_GETITEM, 0, (LPARAM)&info);
                    
                    /* get the parent item's attributes */
                    parattr = SFGAO_FILESYSTEM | SFGAO_FOLDER;
                    grandparfolder->
                        GetAttributesOf(1, (LPCITEMIDLIST *)&info.lParam,
                                        &parattr);

                    /* done with the grandparent folder */
                    grandparfolder->Release();
                }
                else
                {
                    /* 
                     *   the parent item does not itself have a parent
                     *   folder - the parent item is not a folder or file
                     *   system object 
                     */
                    parattr = 0;
                }

                /*
                 *   If the parent item is a file system folder, sort its
                 *   children, otherwise insert in the same order we get
                 *   the items from the shell 
                 */
                insitem.hInsertAfter = (((parattr & SFGAO_FILESYSTEM) != 0
                                         && (parattr & SFGAO_FOLDER) != 0)
                                        ? TVI_SORT : TVI_LAST);

                /* add it to the list */
                insitem.hParent = parent;
                insitem.item.mask = TVIF_PARAM | TVIF_STATE
                                    | TVIF_IMAGE | TVIF_SELECTEDIMAGE
                                    | TVIF_TEXT | TVIF_CHILDREN;
                insitem.item.state = 0;
                insitem.item.stateMask = TVIS_EXPANDED;
                insitem.item.pszText = pdisp;
                insitem.item.lParam = (LPARAM)item;
                insitem.item.cChildren = 1;
                insitem.item.iImage = icon_id;
                insitem.item.iSelectedImage = open_icon_id;
                hitem = (HTREEITEM)SendMessage(tree, TVM_INSERTITEM, 0,
                                               (LPARAM)&insitem);

                /* count it */
                ++item_cnt;

                /*
                 *   As a special case, if we're at the top level,
                 *   automatically open the "My Computer" item
                 */
                if (top_level
                    && folder->CompareIDs(0, item, my_computer_) == 0)
                {
                    /* expand this item */
                    SendMessage(tree, TVM_EXPAND, (WPARAM)TVE_EXPAND,
                                (LPARAM)hitem);

                    /* select it */
                    SendMessage(tree, TVM_SELECTITEM, (WPARAM)TVGN_CARET,
                                (LPARAM)hitem);
                }

                /*
                 *   If this is a file system item, and its path is a
                 *   prefix to our initial folder, open this item 
                 */
                if (is_filesys && is_folder && init_folder_.get() != 0)
                {
                    STRRET pathname;
                    char *ppath;
                    
                    /* get the path name */
                    pathname.uType = STRRET_CSTR;
                    folder->GetDisplayNameOf(item, SHGDN_FORPARSING,
                                             &pathname);
                    ppath = cvt_ansi(item, &pathname);

                    /* compare it to our initial folder */
                    if (strlen(ppath) != 0
                        && strlen(ppath) <= strlen(init_folder_.get())
                        && memicmp(ppath, init_folder_.get(),
                                   strlen(ppath)) == 0)
                    {
                        /* 
                         *   note this item as the item to select after
                         *   initialization (if we find a more deeply
                         *   nested match later, we'll replace it at that
                         *   point) 
                         */
                        post_init_sel_ = hitem;

                        /* expand it */
                        SendMessage(tree, TVM_EXPAND, (WPARAM)TVE_EXPAND,
                                    (LPARAM)hitem);
                    }
                }
            }
        }
        
        /* done with the enumerator */
        penum->Release();
    }

    /* return the number of items we added */
    return item_cnt;
}


/*
 *   Initialize the dialog.  Populate the tree control with the root
 *   items, which show the available drive letters listed under the "My
 *   Computer" item.  
 */
void CTadsDialogFolderSel::init()
{
    HWND tree;
    TV_INSERTSTRUCT item;
    HTREEITEM root_item;
    char drives[128];
    ITEMIDLIST *dtitem;
    HIMAGELIST sysimagelist;
    SHFILEINFO shinfo;
    int icon_id, open_icon_id;
    STRRET dispname;
    char *pdisp;
    int i;
    HICON hIcon, hOpenIcon;

    /* inherit default behavior */
    CTadsDialog::init();

    /* set the caption and prompt, if they were provided */
    if (prompt_.get() != 0)
        SetDlgItemText(handle_, IDC_TXT_PROMPT, prompt_.get());
    if (caption_.get() != 0)
        SendMessage(handle_, WM_SETTEXT, 0, (LPARAM)caption_.get());

    /* get the desktop folder and task allocator */
    if (SHGetDesktopFolder(&dt_folder_) != NOERROR
        || SHGetMalloc(&sh_imalloc_) != NOERROR)
        return;

    /* get the "My Computer" item ID list, for later comparisons */
    SHGetSpecialFolderLocation(0, CSIDL_DRIVES, &my_computer_);

    /* get the tree control */
    tree = GetDlgItem(handle_, IDC_TREE_FOLDER_TREE);

    /* get the root "Desktop" item's information */
    SHGetSpecialFolderLocation(0, CSIDL_DESKTOP, &dtitem);

    /* 
     *   get its icon information, and while we're at it we can get the
     *   system image list, which we'll use as the tree control's image
     *   list 
     */
    sysimagelist = (HIMAGELIST)
                   SHGetFileInfo((LPCTSTR)dtitem, 0, &shinfo, sizeof(shinfo),
                                 SHGFI_PIDL | SHGFI_ICON| SHGFI_SMALLICON
                                 | SHGFI_SYSICONINDEX);
    hIcon = shinfo.hIcon;

    /* get the open icon, in case it's different */
    SHGetFileInfo((LPCTSTR)dtitem, 0, &shinfo, sizeof(shinfo),
                  SHGFI_PIDL | SHGFI_ICON| SHGFI_SMALLICON
                  | SHGFI_SYSICONINDEX | SHGFI_OPENICON);
    hOpenIcon = shinfo.hIcon;

    /* create our own copy of the system image list */
    imagelist_ = ImageList_Create(GetSystemMetrics(SM_CXSMICON),
                                  GetSystemMetrics(SM_CYSMICON),
                                  ILC_COLOR | ILC_MASK, 10, 10);

    /* 
     *   Calculate the DWORD parameter for IExtractIcon::Extract.  This is
     *   a weird encoding where the low 16 bits give the large icon size,
     *   and the high 16 bits give the small icon size.  We'll use the
     *   system settings for these and code up the DWORD value. 
     */
    extract_size_ = MAKELONG(GetSystemMetrics(SM_CXICON),
                             GetSystemMetrics(SM_CXSMICON));

    /* add the desktop icons to our new image list */
    icon_id = ImageList_AddIcon(imagelist_, hIcon);
    open_icon_id = ImageList_AddIcon(imagelist_, hOpenIcon);

    /* set the tree control's image list */
    TreeView_SetImageList(tree, imagelist_, TVSIL_NORMAL);

    /* get the item's display name */
    dt_folder_->GetDisplayNameOf(dtitem, SHGDN_NORMAL, &dispname);
    pdisp = cvt_ansi(dtitem, &dispname);

    /* 
     *   Add the "Desktop" item at the top.  Use a null lParam value for
     *   this item (all other items use their shell ITEMIDLIST objects for
     *   the lParam).  
     */
    item.hParent = 0;
    item.hInsertAfter = TVI_LAST;
    item.item.mask = TVIF_PARAM | TVIF_STATE | TVIF_TEXT | TVIF_CHILDREN
                     | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    item.item.state = TVIS_EXPANDED | TVIS_EXPANDEDONCE;
    item.item.stateMask = TVIS_EXPANDED | TVIS_EXPANDEDONCE;
    item.item.pszText = pdisp;
    item.item.cChildren = 1;
    item.item.lParam = (LPARAM)dtitem;
    item.item.iImage = icon_id;
    item.item.iSelectedImage = open_icon_id;
    root_item = (HTREEITEM)
                SendMessage(tree, TVM_INSERTITEM, 0, (LPARAM)&item);

    /* open the root item immediately */
    add_contents(tree, root_item, dt_folder_, TRUE);

    /* if we found an item to initially select, select it now */
    if (post_init_sel_ != 0)
        SendMessage(tree, TVM_SELECTITEM, (WPARAM)TVGN_CARET,
                    (LPARAM)post_init_sel_);
}

/*
 *   Process a command 
 */
int CTadsDialogFolderSel::do_command(WPARAM id, HWND hwnd)
{
    switch(LOWORD(id))
    {
    case IDC_FLD_FOLDER_PATH:
        switch(HIWORD(id))
        {
        case EN_CHANGE:
            {
                HWND okbtn;
                HWND fld;
                char buf[MAX_PATH];
                
                /* get the control handles */
                okbtn = GetDlgItem(handle_, IDOK);
                fld = GetDlgItem(handle_, IDC_FLD_FOLDER_PATH);
                
                /* enable the "OK" button only if the field is non-empty */
                EnableWindow(okbtn, GetWindowTextLength(fld) != 0);
                
                /* save the current setting */
                GetDlgItemText(handle_, IDC_FLD_FOLDER_PATH,
                               buf, sizeof(buf));
                cur_folder_.set(buf);
                
                /* the return value is ignored */
                return 0;
            }

        default:
            break;
        }
        break;

    default:
        break;
    }

    /* if we didn't return already, inherit the default handling */
    return CTadsDialog::do_command(id, hwnd);
}

/*
 *   Process a notification message 
 */
int CTadsDialogFolderSel::do_notify(NMHDR *nm, int ctl)
{
    switch(nm->code)
    {
    case TVN_ITEMEXPANDING:
        /* 
         *   the item is expanding for the first time - fill in its child
         *   list 
         */
        {
            NM_TREEVIEW *tv = (NM_TREEVIEW *)nm;
            TV_INSERTSTRUCT item;

            /* 
             *   if the item is being expanded for the first time, fill in
             *   its children 
             */
            if (!(tv->itemNew.state & TVIS_EXPANDEDONCE))
            {
                ITEMIDLIST *item;
                HWND tree = nm->hwndFrom;

                /* set the busy cursor while we're working */
                SetCursor(busy_cursor_);

                /* 
                 *   get the shell ITEMIDLIST - it's the lParam of the
                 *   tree item 
                 */
                item = (ITEMIDLIST *)tv->itemNew.lParam;

                /* 
                 *   If the item is null, ignore it - this is the desktop
                 *   root item, which should already have been expanded.
                 *   If it's not null, expand it. 
                 */
                if (item != 0)
                {
                    IShellFolder *subfolder;
                    HRESULT result;
                    TV_ITEM info;

                    /* get the folder interface for this object */
                    subfolder = get_item_folder(tree, tv->itemNew.hItem);
                    if (subfolder != 0)
                    {
                        /* set up for TVM_SETITEM */
                        info.hItem = tv->itemNew.hItem;
                        info.mask = TVIF_CHILDREN | TVIF_HANDLE;

                        /* list the contents */
                        if (add_contents(tree, tv->itemNew.hItem,
                                         subfolder, FALSE) == 0)
                        {
                            /* 
                             *   this item didn't turn out to have any
                             *   children, so remove its open/close box 
                             */
                            info.mask |= TVIF_STATE;
                            info.stateMask = TVIS_EXPANDED;
                            info.state = 0;
                            info.cChildren = 0;
                        }
                        else
                        {
                            /* 
                             *   this item does have children; we can
                             *   remove the callback notification now,
                             *   since we have expanded the list 
                             */
                            info.cChildren = 1;
                        }

                        /* adjust the item's state now that it's open */
                        SendMessage(tree, TVM_SETITEM, 0, (LPARAM)&info);
                            
                        /* done with the folder */
                        subfolder->Release();
                    }
                }

                /* done working */
                SetCursor(arrow_cursor_);
            }
        }

        /* allow the list to expand */
        return FALSE;

    case TVN_SELCHANGED:
        /* selection changed */
        {
            NM_TREEVIEW *tv = (NM_TREEVIEW *)nm;
            char buf[MAX_PATH];
            IShellFolder *parent_folder;
            ITEMIDLIST *item;
            TV_ITEM info;
            DWORD attr;

            /* get the parent folder */
            parent_folder = get_parent_folder(nm->hwndFrom,
                                              tv->itemNew.hItem);

            /* 
             *   if the parent folder is null, this is the top-level
             *   (Desktop) item, which isn't a file system object 
             */
            if (parent_folder == 0)
                return 0;

            /* get the ID list from the tree item's lparam */
            info.hItem = tv->itemNew.hItem;
            info.mask = TVIF_PARAM;
            SendMessage(nm->hwndFrom, TVM_GETITEM, 0, (LPARAM)&info);
            item = (ITEMIDLIST *)info.lParam;

            /* 
             *   get the attributes - in particular, we want to know if
             *   this is a file system object and/or a folder object,
             *   since we only want to use it if it's both 
             */
            attr = SFGAO_FILESYSTEM | SFGAO_FOLDER;
            parent_folder->GetAttributesOf(1, (LPCITEMIDLIST *)&item, &attr);

            /* we're done with the parent folder - release it */
            parent_folder->Release();

            /* 
             *   if the newly selected item is a file system object,
             *   update the path text field to reflect the new item's path 
             */
            if ((attr & SFGAO_FILESYSTEM) != 0 && (attr & SFGAO_FOLDER) != 0)
            {
                /* get the path for this item */
                get_item_path(buf, tv->itemNew.hItem);

                /* set the text field to reflect the newly selected item */
                SetDlgItemText(handle_, IDC_FLD_FOLDER_PATH, buf);
                cur_folder_.set(buf);
            }
        }

        /* return value ignored */
        return 0;

    case TVN_DELETEITEM:
        /* the item is being deleted - delete the ITEMIDLIST object */
        {
            NM_TREEVIEW *nmtv = (NM_TREEVIEW *)nm;
            ITEMIDLIST *item;

            /* get the item - it's the lParam in the itemOld entry */
            item = (ITEMIDLIST *)nmtv->itemOld.lParam;

            /* delete its memory */
            sh_imalloc_->Free(item);
        }

        /* return value is ignored */
        return 0;
        
    default:
        /* inherit default handling */
        return CTadsDialog::do_notify(nm, ctl);
    }
}

/*
 *   Get the full path to a tree item
 */
void CTadsDialogFolderSel::get_item_path(char *buf, HTREEITEM hitem)
{
    HWND tv;
    TV_ITEM info;
    IShellFolder *folder;
    STRRET pathname;
    ITEMIDLIST *item;

    /* get the treeview control */
    tv = GetDlgItem(handle_, IDC_TREE_FOLDER_TREE);

    /* get this item's lParam */
    info.hItem = hitem;
    info.mask = TVIF_PARAM;
    SendMessage(tv, TVM_GETITEM, 0, (LPARAM)&info);

    /* presume we won't find a valid path */
    buf[0] = '\0';

    /* if this is the root item, there is not a valid path */
    item = (ITEMIDLIST *)info.lParam;
    if (item == 0)
        return;

    /* get the folder containing this item */
    folder = get_parent_folder(tv, hitem);
    if (folder == 0)
        return;

    /* get the display name for parsing - this is the file path */
    pathname.uType = STRRET_CSTR;
    folder->GetDisplayNameOf(item, SHGDN_FORPARSING, &pathname);

    /* copy it to the result buffer */
    strcpy(buf, cvt_ansi(item, &pathname));

    /* done with the parent folder */
    folder->Release();
}

/*
 *   Get the IShellFolder that contains the given item
 */
IShellFolder *CTadsDialogFolderSel::
   get_parent_folder(HWND tv, HTREEITEM hitem)
{
    HTREEITEM hparent;

    /* get the parent item */
    hparent = TreeView_GetParent(tv, hitem);

    /* if the parent object is null, there is no parent folder */
    if (hparent == 0)
        return 0;

    /* return the parent item's folder */
    return get_item_folder(tv, hparent);
}

/*
 *   Get the IShellFolder for a given item
 */
IShellFolder *CTadsDialogFolderSel::
   get_item_folder(HWND tv, HTREEITEM hitem)
{
    TV_ITEM info;
    IShellFolder *parent_folder;
    ITEMIDLIST *item;
    HTREEITEM hparent;
    IShellFolder *folder;

    /* get this item's lParam so that we can get its ITEMIDLIST */
    info.hItem = hitem;
    info.mask = TVIF_PARAM;
    SendMessage(tv, TVM_GETITEM, 0, (LPARAM)&info);
    item = (ITEMIDLIST *)info.lParam;

    /* get this item's parent item */
    hparent = TreeView_GetParent(tv, hitem);
    if (hparent == 0)
    {
        /* 
         *   we're at the root - return the desktop folder, adding a
         *   reference on behalf of the caller 
         */
        dt_folder_->AddRef();
        return dt_folder_;
    }

    /* get the parent item's folder */
    parent_folder = get_item_folder(tv, hparent);
    if (parent_folder == 0)
        return 0;

    /* ask the parent folder for this item's folder */
    if (parent_folder->BindToObject(item, 0, IID_IShellFolder,
                                    (void **)&folder) != NOERROR)
        folder = 0;

    /* release the parent folder - we're done with it now */
    parent_folder->Release();

    /* return my folder */
    return folder;
}

/*
 *   Run the dialog 
 */
int CTadsDialogFolderSel::run_select_folder(HWND owner, HANDLE app_inst,
                                            const textchar_t *prompt,
                                            const textchar_t *caption,
                                            textchar_t *selected_folder,
                                            size_t sel_buf_len,
                                            const textchar_t *start_folder)
{
    CTadsDialogFolderSel *dlg;
    int id;
    int result;

    /* create the dialog */
    dlg = new CTadsDialogFolderSel(prompt, caption, start_folder);

    /* run the dialog */
    id = dlg->run_modal(IDD_FOLDER_SELECTOR, owner, app_inst);

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


#ifdef BUILD_TEST_PROG
/* ------------------------------------------------------------------------ */
/*
 *   Test section 
 */


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
    if (CTadsDialogFolderSel::
        run_select_folder(0, inst, "Installation &Path:",
                          "Select Installation Folder",
                          buf, sizeof(buf), 0))
        MessageBox(0, buf, "Success!", MB_OK | MB_ICONINFORMATION);
    else
        MessageBox(0, "Cancelled.", "Status", MB_OK | MB_ICONINFORMATION);

    /* success */
    return 0;
}

#endif
