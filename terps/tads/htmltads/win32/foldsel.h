/* $Header: d:/cvsroot/tads/html/win32/foldsel.h,v 1.3 1999/07/11 00:46:46 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  foldsel.h - folder selector dialogs
Function
  Defines two folder selector dialogs.  The two dialogs provide the
  same basic functionality, but have different user interfaces.

  CTadsDialogFolderSel - this is an explorer-style dialog, resembling
  the system folder browser dialog except that it adds a text field
  that allows the user to type in a folder name.  This allows rapid
  navigation for power users, and also allows a user to type in a
  directory that doesn't currently exist.  The folder list is presented
  as a tree showing the current machine, rooted at "Desktop."

  CTadsDialogFolderSel2 - this dialog uses the user interface that is
  more typically used for selecting folders than the explorer-style
  interface.  As with TadsDialogFolderSel, this dialog provides a
  text field for the user to type in any directory name.  However,
  rather than displaying an explorer-style list of drives and folders,
  this dialog shows a list of folders that at any given time shows
  only the contents of the currently opened folder, plus each of the
  parents of the currently opened folder.  The dialog also shows a
  separate popup list of drive letters.

  The second dialog is probably more familiar to most users, since it's
  the type that is usually used in install programs.  The explorer-style
  dialog may be more intuitive to some users, though, because it provides
  the same type of view they are used to seeing in explorer itself.
Notes
  
Modified
  05/09/98 MJRoberts  - Creation
*/

#ifndef FOLDSEL_H
#define FOLDSEL_H

#include <shlobj.h>

#ifndef HTML_OS_H
#include "html_os.h"
#endif
#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   A handy set of utilities for an owner-drawn popup/combo/listbox showing
 *   items with folder icons.  
 */

/*
 *   Folder display manager.  This is a class that can be multiply inherited
 *   into a dialog for managing a popup, combo, or listbox control that
 *   displays owner-drawn items representing folders and drives.  We take
 *   care of sensing and showing the icon for an item given its folder path.
 */
class CTadsFolderControl
{
public:
    /* initialize */
    CTadsFolderControl()
    {
        /* remember the small icon size */
        smicon_x_ = GetSystemMetrics(SM_CXSMICON);
        smicon_y_ = GetSystemMetrics(SM_CYSMICON);
    }

    /* get the icon to display for a given folder */
    HICON get_item_icon(const char *fname, int open);

    /* populate a popup/combo/listbox with items for the given path */
    struct lb_folder_t *populate_folder_tree(
        HWND ctl, int is_lb, const char *path);

    /* owner-draw a folder item */
    void draw_folder_item(DRAWITEMSTRUCT *draw_info);

protected:
    /* small icon metrics */
    int smicon_x_;
    int smicon_y_;
};

/*
 *   Structure for a folder entry in the folder popup list.  This is the
 *   owner-managed context information we store in the control.  
 */
struct lb_folder_t
{
    lb_folder_t(const char *foldername, size_t foldernamelen,
                const char *fullpath, size_t fullpathlen, int level)
    {
        foldername_.set(foldername, foldernamelen);
        fullpath_.set(fullpath, fullpathlen);
        level_ = level;
        normal_icon_ = open_icon_ = 0;
        is_open_ = FALSE;
    }

    /* name of this folder */
    CStringBuf foldername_;

    /* full path for this folder */
    CStringBuf fullpath_;

    /* nesting level */
    int level_;

    /* flag indicating whether the folder is currently open */
    int is_open_;

    /* icons */
    HICON normal_icon_;
    HICON open_icon_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Folder selector dialog type 1.  This dialog uses an explorer-style
 *   folder display, showing the list of disks and folders in a single
 *   tree view rooted at "Desktop."  
 */

class CTadsDialogFolderSel: public CTadsDialog
{
public:
    /*
     *   Run the folder browser dialog modally.  Returns true if the user
     *   selected a folder, false if they cancelled the dialog.  If
     *   successful, fills in 'selected_folder' with the full path of the
     *   selected folder (including drive letter prefix).  If
     *   'start_folder' is not null, it must be a full path (including
     *   drive letter) to the initial folder; if it's null, we'll start
     *   off in the current system directory.
     *   
     *   'prompt' is a string to display above the text field. 'caption'
     *   is a string to use as the title for the dialog box.  
     */
    static int run_select_folder(HWND owner, HANDLE app_instance,
                                 const textchar_t *prompt,
                                 const textchar_t *caption,
                                 textchar_t *selected_folder,
                                 size_t selected_folder_buffer_len,
                                 const textchar_t *start_folder);

    /* get the selected folder */
    const textchar_t *get_folder_sel() const
        { return cur_folder_.get(); }

    CTadsDialogFolderSel(const textchar_t *prompt, const textchar_t *caption,
                         const textchar_t *init_folder);
    ~CTadsDialogFolderSel();

    void init();
    int do_notify(NMHDR *nm, int ctl);
    int do_command(WPARAM id, HWND hwnd);

protected:
    /* add the contents of a folder to the list under the given parent */
    int add_contents(HWND tree, HTREEITEM parent, IShellFolder *folder,
                     int top_level);

    /* get the full filename path to a given item in the tree view */
    void get_item_path(char *buf, HTREEITEM item);

    /* get the folder for a given tree item */
    IShellFolder *get_item_folder(HWND tv, HTREEITEM hitem);

    /* get the parent folder of the given tree item */
    IShellFolder *get_parent_folder(HWND tv, HTREEITEM hitem);

    /* get an ANSI string for a STRRET */
    static char *cvt_ansi(ITEMIDLIST *item, STRRET *nm);

    /* 
     *   extract an icon for a given object, and add it to our image list;
     *   returns the index of the icon in our image list 
     */
    int extract_icon(IShellFolder *folder, ITEMIDLIST *item, int open);

    /* desktop folder interface */
    IShellFolder *dt_folder_;

    /* desktop task allocator */
    IMalloc *sh_imalloc_;

    /* the item ID list for "My Computer" */
    ITEMIDLIST *my_computer_;

    /* our image list */
    HIMAGELIST imagelist_;

    /* DWORD parameter for icon size for IExtractIcon::Extract */
    DWORD extract_size_;

    /* busy/arrow cursors */
    HCURSOR busy_cursor_;
    HCURSOR arrow_cursor_;

    /* hash table for icon image list entries */
    class CHtmlHashTable *image_hashtab_;

    /* initial folder */
    CStringBuf init_folder_;

    /* 
     *   item in the list to select after initialization - this is the
     *   item that reflects the most deeply nested match for the initial
     *   directory 
     */
    HTREEITEM post_init_sel_;

    /* current folder selection */
    CStringBuf cur_folder_;

    /* caption and prompt settings */
    CStringBuf caption_;
    CStringBuf prompt_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Folder selector dialog type 2 - traditional dialog with separate
 *   "folders" and "drives" lists.  The "folders" list shows only one
 *   folder at a time, along with its contents and parents.  
 */

class CTadsDialogFolderSel2: public CTadsDialog, public CTadsFolderControl
{
public:
    /*
     *   Run the folder browser dialog modally.  Returns true if the user
     *   selected a folder, false if they cancelled the dialog.  If
     *   successful, fills in 'selected_folder' with the full path of the
     *   selected folder (including drive letter prefix).  If
     *   'start_folder' is not null, it must be a full path (including
     *   drive letter) to the initial folder; if it's null, we'll start
     *   off in the current system directory.  
     *   
     *   'prompt' is a string to display above the text field. 'caption'
     *   is a string to use as the title for the dialog box.
     *   
     *   'testcb' is an optional callback function, and cbctx is its
     *   context.  If testcb is not null, whenever the user double-clicks
     *   a directory or clicks on the OK button, we'll call this callback.
     *   If it returns true, we'll dismiss the dialog, otherwise we'll
     *   keep it running.  The callback parameter 'path' is the currently
     *   selected path, and 'ok' is true if the OK button was clicked,
     *   false if the user just double-clicked a directory.  
     */
    static int run_select_folder(HWND owner, HINSTANCE app_instance,
                                 const textchar_t *prompt,
                                 const textchar_t *caption,
                                 textchar_t *selected_folder,
                                 size_t selected_folder_buffer_len,
                                 const textchar_t *start_folder,
                                 int (*testcb)(void *ctx, const char *path,
                                               int ok, HWND dlg),
                                 void *cbctx);

    /* get the current folder selection */
    const textchar_t *get_folder_sel() const
        { return cur_folder_.get(); }

    CTadsDialogFolderSel2(const textchar_t *prompt, const textchar_t *caption,
                          const textchar_t *init_folder,
                          int (*testcb)(void *, const char *, int, HWND),
                          void *cbctx);
    ~CTadsDialogFolderSel2();

    /* 
     *   determine if we were successful without even running - if the
     *   callback returns acceptance for our initial directory, we'll set
     *   this flag during construction 
     */
    int is_init_success() const { return init_success_; }

protected:
    /* initialize */
    void init();

    /* destroy */
    void destroy();

    /* process a command */
    int do_command(WPARAM id, HWND hwnd);

    /* draw an owner-drawn item */
    int draw_item(int ctl_id, DRAWITEMSTRUCT *draw_info);

    /* delete an owner-allocated control item */
    int delete_ctl_item(int ctl_id, DELETEITEMSTRUCT *delete_info);

    /* compare owner-drawn items */
    int compare_ctl_item(int ctlid, COMPAREITEMSTRUCT *cmp_info);

    /* 
     *   load the drives into the drive letter popup, replacing any drives
     *   currently in the popup list 
     */
    void init_drives();

    /* build the folder listbox for the current cur_folder_ setting */
    void build_folder_lb();

    /* set the current directory */
    void set_cur_dir(const char *path);

    /* busy/arrow cursors */
    HCURSOR busy_cursor_;
    HCURSOR arrow_cursor_;

    /* initial folder */
    CStringBuf cur_folder_;

    /* index in drive popup for this folder */
    int cur_folder_drive_idx_;

    /* small icon x,y size */
    int smicon_x_;
    int smicon_y_;

    /* desktop folder interface and allocator */
    IShellFolder *dt_folder_;
    IMalloc *sh_imalloc_;

    /* icon extraction size information */
    DWORD icon_extract_size_;

    /* caption and prompt settings */
    CStringBuf caption_;
    CStringBuf prompt_;

    /* client test callback */
    int (*testcb_)(void *ctx, const char *path, int okbtn, HWND dlg);
    void *cbctx_;

    /* 
     *   flag: the client test callback returned success for the initial
     *   directory 
     */
    int init_success_;
};



#endif /* FOLDSEL_H */
