#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 2000 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32prj.cpp - TADS 3 debugger for Win32 - project window
Function
  
Notes
  
Modified
  01/14/00 MJRoberts  - Creation
*/

#include <Windows.h>
#include <shlobj.h>
#include <Ole2.h>
#include <string.h>

#include "os.h"

#include "t3std.h"
#include "tclibprs.h"

#ifndef W32PRJ_H
#include "w32prj.h"
#endif
#ifndef HTMLRES_H
#include "htmlres.h"
#endif
#ifndef W32EXPR_H
#include "w32expr.h"
#endif
#ifndef W32TDB_H
#include "w32tdb.h"
#endif
#ifndef TADSFONT_H
#include "tadsfont.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif
#ifndef HTMLDBG_H
#include "htmldbg.h"
#endif
#ifndef HTMLDCFG_H
#include "htmldcfg.h"
#endif
#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif
#ifndef TADSTAB_H
#include "tadstab.h"
#endif
#ifndef TADSOLE_H
#include "tadsole.h"
#endif
#ifndef FOLDSEL_H
#include "foldsel.h"
#endif

/* ------------------------------------------------------------------------ */
/*
 *   Add a directory to the library search list 
 */
static void add_search_path(CHtmlSys_dbgmain *dbgmain, const char *path)
{
    CHtmlDebugConfig *gconfig;
    int cnt;

    /* get the debugger's global configuration object */
    gconfig = dbgmain->get_global_config();

    /* get the current number of search path entries */
    cnt = gconfig->get_strlist_cnt("srcdir", 0);

    /* add the new entry at the end of the current list */
    gconfig->setval("srcdir", 0, cnt, path);
}

/* ------------------------------------------------------------------------ */
/*
 *   "Absolute Path" dialog.  
 */
class CTadsDlgAbsPath: public CTadsDialog, public CTadsFolderControl
{
public:
    CTadsDlgAbsPath(CHtmlSys_dbgmain *dbgmain, const char *fname)
    {
        /* save the debugger object and filename */
        dbgmain_ = dbgmain;
        fname_ = fname;
    }

    int do_command(WPARAM id, HWND ctl)
    {
        int idx;
        HWND pop;

        /* see what we have */
        switch(LOWORD(id))
        {
        case IDOK:
            /* get the item selected in the parent popup */
            pop = GetDlgItem(handle_, IDC_POP_PARENT);
            idx = SendMessage(pop, CB_GETCURSEL, 0, 0);
            if (idx != CB_ERR)
            {
                lb_folder_t *info;

                /* get our descriptor associated with the item */
                info = (lb_folder_t *)
                       SendMessage(pop, CB_GETITEMDATA, idx, 0);

                /* add the item's path to the seach list */
                add_search_path(dbgmain_, info->fullpath_.get());
            }

            /* continue with default handling */
            break;
        }

        /* inherit default handling */
        return CTadsDialog::do_command(id, ctl);
    }

    /* initialize */
    void init()
    {
        char path[OSFNMAX];

        /* inherit default handling */
        CTadsDialog::init();

        /* show the filename in the dialog */
        SetDlgItemText(handle_, IDC_TXT_FILE, fname_);

        /* 
         *   populate the popup with the folder list - include only the file
         *   path, not the file itself 
         */
        os_get_path_name(path, sizeof(path), fname_);
        populate_folder_tree(GetDlgItem(handle_, IDC_POP_PARENT),
                             FALSE, path);
    }

    /* destroy */
    void destroy()
    {
        /* 
         *   manually clear out the parent popup so that our owner-drawn
         *   items are deleted properly - NT doesn't do this automatically on
         *   destroying the controls, so we must do it manually 
         */
        SendMessage(GetDlgItem(handle_, IDC_POP_PARENT),
                    CB_RESETCONTENT, 0, 0);

        /* inherit default handling */
        CTadsDialog::destroy();
    }

    /* draw an owner-drawn item */
    int draw_item(int ctl_id, DRAWITEMSTRUCT *draw_info)
    {
        switch(ctl_id)
        {
        case IDC_POP_PARENT:
            /* draw the popup item */
            draw_folder_item(draw_info);

            /* handled */
            return TRUE;

        default:
            /* not handled */
            return FALSE;
        }
    }

    /* delete an owner-allocated control item  */
    int delete_ctl_item(int ctl_id, DELETEITEMSTRUCT *delete_info)
    {
        switch (ctl_id)
        {
        case IDC_POP_PARENT:
            /* delet ethe folder information */
            delete (lb_folder_t *)delete_info->itemData;
            return TRUE;

        default:
            /* not one of ours */
            return FALSE;
        }
    }

protected:
    /* debugger main window */
    CHtmlSys_dbgmain *dbgmain_;

    /* the name of the file with the absolute path */
    const char *fname_;
};

/* ------------------------------------------------------------------------ */
/*
 *   "Missing Source File" dialog.  
 */
class CTadsDlgMissingSrc: public CTadsDialog
{
public:
    CTadsDlgMissingSrc(CHtmlSys_dbgmain *dbgmain, const char *fname)
    {
        /* save the debugger object and filename */
        dbgmain_ = dbgmain;
        fname_ = fname;
    }

    /* initialize */
    void init()
    {
        /* inherit default handling */
        CTadsDialog::init();

        /* show the filename in the dialog */
        SetDlgItemText(handle_, IDC_TXT_FILE, fname_);
    }

    /* handle a command */
    int do_command(WPARAM id, HWND ctl)
    {
        OPENFILENAME info;
        char buf[OSFNMAX];
        char dir[OSFNMAX];
        char title[OSFNMAX + 50];

        /* see what we have */
        switch(LOWORD(id))
        {
        case IDC_BTN_FIND:
            /* "Find" button - locate the file */
            info.lStructSize = sizeof(info);
            info.hwndOwner = handle_;
            info.hInstance = CTadsApp::get_app()->get_instance();
            info.lpstrFilter = "TADS Source Files\0*.t;*.tl\0";
            info.lpstrCustomFilter = 0;
            info.nFilterIndex = 0;
            info.lpstrFile = buf;
            strcpy(buf, os_get_root_name((char *)fname_));
            info.nMaxFile = sizeof(buf);
            info.lpstrFileTitle = 0;
            info.nMaxFileTitle = 0;
            info.lpstrInitialDir = CTadsApp::get_app()->get_openfile_dir();
            sprintf(title, "%s - Please Locate File", fname_);
            info.lpstrTitle = title;
            info.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST
                         | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
            info.nFileOffset = 0;
            info.nFileExtension = 0;
            info.lpstrDefExt = 0;
            info.lCustData = 0;
            info.lpfnHook = 0;
            info.lpTemplateName = 0;
            CTadsDialog::set_filedlg_center_hook(&info);
            if (!GetOpenFileName(&info))
                return TRUE;

            /* 
             *   Add the directory path of the file they selected to the
             *   search list (but just the directory path - leave off the
             *   filename portion).  Note that we don't check that the file
             *   they selected is the same file we asked for; all we care
             *   about is the path.  
             */
            os_get_path_name(dir, sizeof(dir), buf);
            add_search_path(dbgmain_, dir);

            /* post an "OK" to myself so we dismiss and proceed */
            PostMessage(handle_, WM_COMMAND, IDOK, 0);

            /* handled */
            return TRUE;
        }

        /* return default handling */
        return CTadsDialog::do_command(id, ctl);
    }

protected:
    /* debugger main window */
    CHtmlSys_dbgmain *dbgmain_;

    /* the name of the file with the absolute path */
    const char *fname_;
};


/* ------------------------------------------------------------------------ */
/*
 *   User-defined messages specific to this window 
 */

/* open the selected item in the project tree */
#define W32PRJ_MSG_OPEN_TREE_SEL  (WM_USER + 1)

/* ------------------------------------------------------------------------ */
/* 
 *   template configuration group name for external resource files - we'll
 *   change the '0000' suffix to a serial number based on each external
 *   resource file's position in the project list 
 */
#define EXTRES_CONFIG_TEMPLATE "extres_files_0000"


/* ------------------------------------------------------------------------ */
/* 
 *   child "open" actions 
 */
enum proj_item_open_t
{
    /* direct children cannot be opened */
    PROJ_ITEM_OPEN_NONE,
    
    /* open as a source file */
    PROJ_ITEM_OPEN_SRC,

    /* open an an external file */
    PROJ_ITEM_OPEN_EXTERN
};

/* ------------------------------------------------------------------------ */
/*
 *   Item extra information structure.  We allocate one of these
 *   structures for each item in the list so that we can track extra data
 *   for the item.  
 */
struct proj_item_info
{
    proj_item_info(const char *fname, proj_item_type_t item_type,
                   const char *config_group,
                   const char *child_path, int child_icon,
                   int can_delete, int can_add_children,
                   int openable, proj_item_open_t child_open_action,
                   proj_item_type_t child_type, int is_sys_file)
    {
        /* remember the filename */
        if (fname == 0 || fname[0] == '\0')
            fname_ = 0;
        else
        {
            fname_ = (char *)th_malloc(strlen(fname) + 1);
            strcpy(fname_, fname);
        }

        /* remember the configuration file group name */
        if (config_group == 0 || config_group[0] == '\0')
            config_group_ = 0;
        else
        {
            config_group_ = (char *)th_malloc(strlen(config_group) + 1);
            strcpy(config_group_, config_group);
        }

        /* remember my type */
        type_ = item_type;

        /* remember the child path */
        if (child_path == 0 || child_path[0] == '\0')
            child_path_ = 0;
        else
        {
            child_path_ = (char *)th_malloc(strlen(child_path) + 1);
            strcpy(child_path_, child_path);
        }
        
        /* remember the child icon */
        child_icon_ = child_icon;

        /* note whether we can delete the item */
        can_delete_ = can_delete;

        /* note whether we can add children */
        can_add_children_ = can_add_children;

        /* note whether this can be opened in the user interface */
        openable_ = openable;

        /* remember the child open action */
        child_open_action_ = child_open_action;

        /* remember the child type */
        child_type_ = child_type;

        /* presume it's included in the installer */
        in_install_ = TRUE;

        /* remember whether or not it's a system file */
        is_sys_file_ = is_sys_file;

        /* we don't keep timestamp information by default */
        memset(&file_time_, 0, sizeof(file_time_));

        /* no URL-style name yet */
        url_ = 0;

        /* no child system-file path yet */
        child_sys_path_ = 0;

        /* presume it's not excluded from its library */
        exclude_from_lib_ = FALSE;

        /* no parent-library or user-library path yet */
        lib_path_ = 0;
        in_user_lib_ = FALSE;

        /* make it recursive by default (if it's a resource directory) */
        recursive_ = TRUE;
    }

    ~proj_item_info()
    {
        /* delete the filename */
        if (fname_ != 0)
            th_free(fname_);

        /* delete the configuration group name */
        if (config_group_ != 0)
            th_free(config_group_);

        /* delete the child path */
        if (child_path_ != 0)
            th_free(child_path_);

        /* delete the URL-style name if present */
        if (url_ != 0)
            th_free(url_);

        /* delete the child system-file path if present */
        if (child_sys_path_ != 0)
            th_free(child_sys_path_);

        /* if we have a library path, free it */
        if (lib_path_ != 0)
            th_free(lib_path_);
    }

    /* set the item's URL-style name */
    void set_url(const char *url)
    {
        /* if we already have a url-style name, delete it */
        if (url_ != 0)
            th_free(url_);

        /* allocate space and copy the URL string */
        if (url != 0 && url[0] != '\0')
        {
            url_ = (char *)th_malloc(strlen(url) + 1);
            strcpy(url_, url);
        }
        else
            url_ = 0;
    }

    /* set the item's child system-file path */
    void set_child_sys_path(const char *path)
    {
        /* free any old path */
        if (child_sys_path_ != 0)
            th_free(child_sys_path_);

        /* allocate space and copy the string */
        if (path != 0 && path[0] != '\0')
        {
            child_sys_path_ = (char *)th_malloc(strlen(path) + 1);
            strcpy(child_sys_path_, path);
        }
        else
            child_sys_path_ = 0;
    }

    /* set the item's library path */
    void set_lib_path(const char *path)
    {
        /* free any old path */
        if (lib_path_ != 0)
            th_free(lib_path_);

        /* allocate space and copy the string */
        if (path != 0 && path[0] != '\0')
        {
            lib_path_ = (char *)th_malloc(strlen(path) + 1);
            strcpy(lib_path_, path);
        }
        else
            lib_path_ = 0;
    }

    /* set the item's user library path */
    void set_user_path(const char *path)
    {
        /* save the library path */
        set_lib_path(path);

        /* remember that we're in a user library */
        in_user_lib_ = TRUE;
    }

    /* 
     *   Filename string - if this item represents a file, this contains the
     *   full filename; this entry is null for artificial grouping items
     *   (such as the "Sources" folder).
     *   
     *   For most items, the filename stored here is relative to the
     *   containing item's full path.  However, there are exceptions:
     *   
     *   - If is_sys_file_ is true, this filename is relative to the
     *   containing item's system directory path.
     *   
     *   - If lib_path_ is non-null, then this filename is relative to the
     *   path given by lib_path_.
     */
    char *fname_;

    /*
     *   Parent library path.  If this is non-null, then the filename in
     *   fname_ is relative to this path rather than to the container path.
     *   
     *   This path is either the path to some library file that is now or
     *   has in the past been part of the project, or is a directory in the
     *   global configuration's user library search path.  
     */
    char *lib_path_;

    /* 
     *   Flag: the lib_path_ value is from the user library search path.  If
     *   this is true, then lib_path_ is a path taken from the user library
     *   search list; otherwise, lib_path_ is the path to a directory
     *   containing a .tl file.  
     */
    int in_user_lib_;

    /* 
     *   the URL-style filename - we retain this for library members,
     *   because it allows us to build an exclusion (-x) option for this
     *   item for the compiler command line 
     */
    char *url_;

    /* 
     *   configuration group name - this is the name of the string list in
     *   the configuration file that stores children of this object;
     *   applies only to containers 
     */
    char *config_group_;

    /* my item type */
    proj_item_type_t type_;

    /* child object path - this is relative to any containers */
    char *child_path_;

    /* child object path for system files */
    char *child_sys_path_;

    /* icon to use for children of this item (index in image list) */
    int child_icon_;

    /* flag indicating whether we can delete this item */
    unsigned int can_delete_ : 1;

    /* flag indicating whether we can add children */
    unsigned int can_add_children_ : 1;

    /* flag indicating whether we can open this object in the UI */
    unsigned int openable_ : 1;

    /* for external resources: is it included in the install? */
    unsigned int in_install_ : 1;

    /* for resource directory entries: is this recursive? */
    unsigned int recursive_ : 1;

    /* flag indicating whether it's a system file or a user file */
    unsigned int is_sys_file_ : 1;

    /* flag indicating whether or not it's excluded from its library */
    unsigned int exclude_from_lib_ : 1;

    /* action to take to open a child item */
    proj_item_open_t child_open_action_;

    /* type of child items */
    proj_item_type_t child_type_;

    /* 
     *   file timestamp - we keep this information for certain types of
     *   files (libraries) so that we can tell if the file on disk has
     *   changed since we last loaded the file 
     */
    FILETIME file_time_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Library parser for the project window - this is a library parser
 *   specialized to parse a library file as we add it to the project window.
 */
class CTcLibParserProj: public CTcLibParser
{
public:
    /*
     *   Parse a library and add the files in the library to the command
     *   line under construction.  
     */
    static void add_lib_to_proj(const char *lib_name,
                                CHtmlDbgProjWin *proj_win, HWND tree,
                                HTREEITEM lib_tree_item,
                                proj_item_info *lib_item_info)
    {
        /* set up our library parser object */
        CTcLibParserProj parser(lib_name, proj_win, tree,
                                lib_tree_item, lib_item_info);

        /* scan the library and add its files to the command line */
        parser.parse_lib();
    }

protected:
    /* instantiate */
    CTcLibParserProj(const char *lib_name, CHtmlDbgProjWin *proj_win,
                     HWND tree, HTREEITEM lib_tree_item,
                     proj_item_info *lib_item_info)
        : CTcLibParser(lib_name)
    {
        /* remember our project window information */
        proj_win_ = proj_win;
        tree_ = tree;

        /* remember the parent library item information */
        lib_tree_item_ = lib_tree_item;
        lib_item_info_ = lib_item_info;
    }

    /* 
     *   process the "name" variable - we'll set the displayed name of the
     *   library to this name 
     */
    void scan_name(const char *name)
    {
        char buf[OSFNMAX + 1024];
        TV_ITEM item;

        /* 
         *   build the full display name by using the library's descriptive
         *   name plus the filename in parentheses 
         */
        sprintf(buf, "%s (%s)", name, lib_item_info_->fname_);

        /* set the new name */
        item.mask = TVIF_HANDLE | TVIF_TEXT;
        item.hItem = lib_tree_item_;
        item.pszText = buf;
        TreeView_SetItem(tree_, &item);
    }

    /* 
     *   Process a "source" entry in the library.  Note that we scan the raw
     *   source file name, not the full filename with the library source
     *   path, because we do our own figuring on including the library
     *   source path.  
     */
    void scan_source(const char *fname)
    {
        /* add this with a default "t" extension and known type SOURCE */
        scan_add_to_project(fname, "t", PROJ_ITEM_TYPE_SOURCE);
    }


    /*
     *   Process a "library" entry.  As with scan_file(), we scan the raw
     *   filename, because we handle path considerations ourselves.  
     */
    void scan_library(const char *fname)
    {
        /* add this with a default "tl" extension and known type LIBRARY */
        scan_add_to_project(fname, "tl", PROJ_ITEM_TYPE_LIB);
    }

    /*
     *   Common handler for scan_source() and scan_library().  Adds the file
     *   to the project with the given default extension and known item
     *   type.  
     */
    void scan_add_to_project(const char *url, const char *ext,
                             proj_item_type_t known_type)
    {
        char rel_path[OSFNMAX];
        char parent_path[OSFNMAX];
        char full_path[OSFNMAX];
        HTREEITEM item;
        proj_item_info *info;

        /* convert the filename from URL notation to Windows notation */
        os_cvt_url_dir(rel_path, sizeof(rel_path), url, FALSE);

        /* add the default extension if necessary */
        os_defext(rel_path, ext);

        /* get the parent directory */
        proj_win_->get_item_child_dir(parent_path, sizeof(parent_path),
                                      lib_tree_item_);

        /* build the full path */
        os_build_full_path(full_path, sizeof(full_path),
                           parent_path, rel_path);

        /* add this item to the project */
        item = proj_win_->add_file_to_project(lib_tree_item_, TVI_LAST,
                                              full_path, known_type);

        /* 
         *   save the original URL-style name, since this is the name we use
         *   to identify the file for exclusion options 
         */
        if (item != 0 && (info = proj_win_->get_item_info(item)) != 0)
            info->set_url(url);
    }

    /* scan a "nodef" flag in the library */
    void scan_nodef()
    {
        /* 
         *   We don't need to do anything special here.  Note, in particular,
         *   that we do NOT need to set the "-nodef" flag for our own Build
         *   Settings configuration, because the presence of this "nodef"
         *   flag in the library is enough; adding our own -nodef flag would
         *   be redundant.  So we can simply ignore this.  
         */
    }

    /* look up a preprocessor symbol */
    int get_pp_symbol(char *dst, size_t dst_len,
                      const char *sym_name, size_t sym_len)
    {
        CHtmlDebugConfig *config;
        size_t cnt;
        size_t i;

        /* get the configuration object (which has the build settings) */
        config = proj_win_->dbgmain_->get_local_config();

        /* get the number of -D symbols */
        cnt = config->get_strlist_cnt("build", "defines");

        /* scan the -D symbols for the one they're asking about */
        for (i = 0 ; i < cnt ; ++i)
        {
            char buf[1024];
            char *val;
            size_t var_len;

            /* get this variable from the build settings */
            config->getval("build", "defines", i, buf, sizeof(buf));

            /* 
             *   scan for an equals sign - the build configuration stores the
             *   variables as NAME=VALUE strings 
             */
            for (val = buf ; *val != '\0' && *val != '=' ; ++val) ;

            /* 
             *   if we found an equals sign, the variable name is the part up
             *   to the '=', and the rest is the value; if we didn't find an
             *   equals sign, the whole thing is the name and the value is
             *   the implied value "1" 
             */
            var_len = val - buf;
            val = (*val == '=' ? val + 1 : "1");

            /* if it matches, return the value */
            if (sym_len == var_len && memcmp(buf, sym_name, var_len) == 0)
            {
                size_t val_len;

                /* if there's room, copy the expansion */
                val_len = strlen(val);
                if (val_len < dst_len)
                    memcpy(dst, val, val_len);

                /* return the result length */
                return val_len;
            }
        }

        /* didn't find it - so indicate by returning -1 */
        return -1;
    }

    /* the project window */
    CHtmlDbgProjWin *proj_win_;

    /* the treeview control for the project window */
    HWND tree_;

    /* 
     *   the treeview item for the library file, which is the parent of the
     *   files we add to the tree; and our extra information object for that
     *   item 
     */
    HTREEITEM lib_tree_item_;
    proj_item_info *lib_item_info_;
};


/* ------------------------------------------------------------------------ */
/*
 *   control ID for the tree view 
 */
const int TREEVIEW_ID = 1;

/* statics */
HCURSOR CHtmlDbgProjWin::no_csr_ = 0;
HCURSOR CHtmlDbgProjWin::move_csr_ = 0;


/* ------------------------------------------------------------------------ */
/*
 *   Creation 
 */
CHtmlDbgProjWin::CHtmlDbgProjWin(CHtmlSys_dbgmain *dbgmain)
{
    /* remember the debugger main window object, and add our reference */
    dbgmain_ = dbgmain;
    dbgmain_->AddRef();

    /* 
     *   note whether or not we have a project open - if there's no game
     *   filename in the main debugger object, no project is open, so we
     *   show only a minimal display indicating this 
     */
    proj_open_ = (dbgmain_->get_gamefile_name() != 0
                  && dbgmain_->get_gamefile_name()[0] != '\0');

    /* not tracking a drag yet */
    tracking_drag_ = FALSE;
    last_drop_target_ = 0;

    /* load our static cursors if we haven't already */
    if (no_csr_ == 0)
        no_csr_ = LoadCursor(0, IDC_NO);
    if (move_csr_ == 0)
        move_csr_ = LoadCursor(CTadsApp::get_app()->get_instance(),
                               MAKEINTRESOURCE(IDC_DRAG_MOVE_CSR));
}

/*
 *   Deletion 
 */
CHtmlDbgProjWin::~CHtmlDbgProjWin()
{
}

/*
 *   Find the full path to a file.  
 */
int CHtmlDbgProjWin::find_file_path(char *fullname, size_t fullname_len,
                                    const char *name)
{
    /* recursively scan the tree starting at the root level */
    return find_file_path_in_tree(TVI_ROOT, fullname, fullname_len,
                                  os_get_root_name((char *)name));
}

/*
 *   Find the full path to a file among the children of the given parent
 *   item in the treeview control, recursively searching children of the the
 *   children of parent.  
 */
int CHtmlDbgProjWin::find_file_path_in_tree(
    HTREEITEM parent, char *fullname, size_t fullname_len,
    const char *rootname)
{
    HTREEITEM cur;

    /* scan each child of this parent */
    for (cur = TreeView_GetChild(tree_, parent) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur))
    {
        proj_item_info *info;

        /* get this item's information */
        info = get_item_info(cur);
        if (info != 0 && info->fname_ != 0)
        {
            /* 
             *   if this item's root name matches the name we're looking
             *   for, return this item's full path 
             */
            if (stricmp(os_get_root_name(info->fname_), rootname) == 0)
            {
                /* get the full path to this item */
                get_item_path(fullname, fullname_len, cur);

                /* this is the one we're looking for */
                return TRUE;
            }
        }

        /* search this item's children */
        if (find_file_path_in_tree(cur, fullname, fullname_len, rootname))
            return TRUE;
    }

    /* we didn't find it; so indicate */
    return FALSE;
}

/*
 *   Refresh after application activation.  
 */
void CHtmlDbgProjWin::refresh_for_activation()
{
    /* 
     *   Recursively refresh everything in the tree starting at the root.
     *   Only refresh libraries whose files have been updated externally.  
     */
    refresh_lib_tree(TVI_ROOT, FALSE);
}

/*
 *   Unconditionally reload all libraries.  This should be called whenever a
 *   macro variable definition changes, to ensure that any library inclusions
 *   that depend on macro variables are reloaded to reflect the new variable
 *   values.  
 */
void CHtmlDbgProjWin::refresh_all_libs()
{
    /* unconditionally refresh all libraries in the tree */
    refresh_lib_tree(TVI_ROOT, TRUE);
}

/*
 *   Refresh children of the given parent for application activation, and
 *   recursively refresh their children.  If 'always_reload' is true, we'll
 *   unconditionally reload all libraries, regardless of file timestamps;
 *   this can be used to ensure that all libraries are reloaded due to a
 *   change in macro variable values, for example.  
 */
void CHtmlDbgProjWin::refresh_lib_tree(HTREEITEM parent, int always_reload)
{
    HTREEITEM cur;

    /* visit each child of the given parent */
    for (cur = TreeView_GetChild(tree_, parent) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur))
    {
        proj_item_info *info;
        int reload_cur;

        /* get this item's information */
        info = get_item_info(cur);

        /* assume we won't need to reload this file */
        reload_cur = FALSE;

        /* if this is a library, check to see if we need to reload it */
        if (info != 0 && info->type_ == PROJ_ITEM_TYPE_LIB)
        {
            /* determine if we need to reload */
            if (always_reload)
            {
                /* we are reloading all libraries */
                reload_cur = TRUE;
            }
            else
            {
                FILETIME ft;

                /* 
                 *   we're only reloading libraries that have changed on
                 *   disk, so check this library's timestamp: get the current
                 *   timestamp for the item 
                 */
                get_item_timestamp(cur, &ft);

                /* 
                 *   if the timestamp is different than it was when we first
                 *   loaded the file, reload the file 
                 */
                reload_cur = (CompareFileTime(&ft, &info->file_time_) != 0);
            }

            /* if we need to reload the library, do so */
            if (reload_cur)
            {
                /* reload the library */
                refresh_library(cur);

                /* the new time is now the loaded time */
                get_item_timestamp(cur, &info->file_time_);
            }
        }

        /* 
         *   If we didn't reload this item, recursively visit this child's
         *   children.  We don't need to traverse into an item that we just
         *   explicitly reloaded, as all of their children will necessarily
         *   be reloaded when we reload the parent.  
         */
        if (!reload_cur)
            refresh_lib_tree(cur, always_reload);
    }
}

/*
 *   Reload a library into the tree.  This is used when we detect that a
 *   library has changed since it was original loaded.  
 */
void CHtmlDbgProjWin::refresh_library(HTREEITEM lib_item)
{
    HTREEITEM cur;
    char path[OSFNMAX];
    TVITEM tvi;
    int was_expanded;

    /* 
     *   delete all of the item's immediate children (this will
     *   automatically delete the children of the children, and so on) 
     */
    for (cur = TreeView_GetChild(tree_, lib_item) ; cur != 0 ;
         cur = TreeView_GetChild(tree_, lib_item))
    {
        /* delete the current child */
        TreeView_DeleteItem(tree_, cur);
    }

    /* get the library's filename */
    get_item_path(path, sizeof(path), lib_item);

    /* 
     *   remember the item's current expanded state, so we can restore it
     *   after loading (the loading process will open it even it's closed to
     *   start with) 
     */
    tvi.mask = TVIF_HANDLE | TVIF_STATE;
    tvi.stateMask = 0xff;
    tvi.hItem = lib_item;
    TreeView_GetItem(tree_, &tvi);
    was_expanded = (tvi.state & TVIS_EXPANDED) != 0;

    /* reload the library */
    CTcLibParserProj::add_lib_to_proj(path, this, tree_, lib_item,
                                      get_item_info(lib_item));

    /* restore the original expanded state of the item */
    TreeView_Expand(tree_, lib_item,
                    was_expanded ? TVE_EXPAND : TVE_COLLAPSE);
}

/*
 *   Process window creation 
 */
void CHtmlDbgProjWin::do_create()
{
    /* do default creation work */
    CTadsWin::do_create();

    /* create the tree control */
    create_tree();
}

/*
 *   create the tree control 
 */
void CHtmlDbgProjWin::create_tree()
{
    RECT rc;
    HTREEITEM proj_item;
    HBITMAP bmp;
    const char *proj_file_name;
    const char *proj_name;
    DWORD ex_style;
    DWORD style;
    char proj_file_dir[OSFNMAX];
    CHtmlDebugConfig *config;

    /* get the local configuration object */
    config = dbgmain_->get_local_config();

    /* get our client area - the tree will fill the whole thing */
    GetClientRect(handle_, &rc);

    /* set the base styles for the tree control */
    style = WS_VISIBLE | WS_CHILD;
    ex_style = 0;

    /* add some style information if a project is open */
    if (proj_open_)
    {
        style |= TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT;
        ex_style |= WS_EX_ACCEPTFILES;
    }

    /* create a tree control filling our whole client area */
    tree_ = CreateWindowEx(ex_style, WC_TREEVIEW, "project tree", style,
                           0, 0, rc.right, rc.bottom, handle_,
                           (HMENU)TREEVIEW_ID,
                           CTadsApp::get_app()->get_instance(), 0);

    /* if a project is open, set up to accept dropped files */
    if (proj_open_)
    {
        /* 
         *   hook the window procedure for the tree control so we can
         *   intercept WM_DROPFILE messages 
         */
        tree_defproc_ = (WNDPROC)SetWindowLong(tree_, GWL_WNDPROC,
                                               (LONG)&tree_proc);

        /* 
         *   set a property with my 'this' pointer, so we can find 'this'
         *   in the hook procedure 
         */
        SetProp(tree_, "CHtmlDbgProjWin.parent.this", (HANDLE)this);
    }

    /* 
     *   if there's a project, show the project's image file name as the
     *   root level object name; otherwise, simply show "No Project" 
     */
    if (proj_open_)
    {
        /* there's a project - use the image file name */
        proj_file_name = dbgmain_->get_local_config_filename();

        /* the displayed project name is the root part of the name */
        proj_name = os_get_root_name((char *)proj_file_name);

        /* get the directory portion of the project path */
        os_get_path_name(proj_file_dir, sizeof(proj_file_dir),
                         proj_file_name);

        /*
         *   Build the NORMAL image list 
         */

        /* create the image list */
        img_list_ = ImageList_Create(16, 16, FALSE, 6, 0);

        /* add the root bitmap */
        bmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDB_PROJ_ROOT));
        img_idx_root_ = ImageList_Add(img_list_, bmp, 0);
        DeleteObject(bmp);

        /* add the folder bitmap */
        bmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDB_PROJ_FOLDER));
        img_idx_folder_ = ImageList_Add(img_list_, bmp, 0);
        DeleteObject(bmp);
        
        /* add the source file bitmap */
        bmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDB_PROJ_SOURCE));
        img_idx_source_ = ImageList_Add(img_list_, bmp, 0);
        DeleteObject(bmp);

        /* add the resource file bitmap */
        bmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDB_PROJ_RESOURCE));
        img_idx_resource_ = ImageList_Add(img_list_, bmp, 0);
        DeleteObject(bmp);

        /* add the resource folder bitmap */
        bmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDB_PROJ_RES_FOLDER));
        img_idx_res_folder_ = ImageList_Add(img_list_, bmp, 0);
        DeleteObject(bmp);

        /* add the external resource file bitmap */
        bmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDB_PROJ_EXTRESFILE));
        img_idx_extres_ = ImageList_Add(img_list_, bmp, 0);
        DeleteObject(bmp);

        /* add the library file bitmap */
        bmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDB_PROJ_LIB));
        img_idx_lib_ = ImageList_Add(img_list_, bmp, 0);

        /* set the image list in the tree control */
        TreeView_SetImageList(tree_, img_list_, TVSIL_NORMAL);

        /*
         *   Build the STATE image list 
         */

        /* create the state image list */
        state_img_list_ = ImageList_Create(16, 16, FALSE, 2, 0);

        /* 
         *   add the in-install state bitmap - note that we must add this
         *   item twice, since the zeroeth item in a state list is never
         *   used (this is an artifact of the windows treeview API - image
         *   zero means no image) 
         */
        bmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDB_PROJ_IN_INSTALL));
        ImageList_Add(state_img_list_, bmp, 0);
        img_idx_in_install_ = ImageList_Add(state_img_list_, bmp, 0);
        DeleteObject(bmp);

        /* add the not-in-install state bitmap */
        bmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDB_PROJ_NOT_IN_INSTALL));
        img_idx_not_in_install_ = ImageList_Add(state_img_list_, bmp, 0);
        DeleteObject(bmp);

        /* add the checkbox-unchecked state bitmap */
        bmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDB_PROJ_NOCHECK));
        img_idx_nocheck_ = ImageList_Add(state_img_list_, bmp, 0);

        /* add the checkbox-checked state bitmap */
        bmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDB_PROJ_CHECK));
        img_idx_check_ = ImageList_Add(state_img_list_, bmp, 0);

        /* set the state image list in the tree control */
        TreeView_SetImageList(tree_, state_img_list_, TVSIL_STATE);
    }
    else
    {
        /* no project - display an indication to this effect */
        proj_name = "(No Project)";

        /* there's no project filename */
        proj_file_name = 0;
        proj_file_dir[0] = '\0';

        /* there's no image list */
        img_idx_root_ = 0;
        img_idx_folder_ = 0;
        img_idx_source_ = 0;
        img_idx_resource_ = 0;
        img_idx_res_folder_ = 0;
        img_idx_extres_ = 0;
    }

    /* fill in the top-level "project" item */
    proj_item = insert_tree(TVI_ROOT, TVI_LAST, proj_name,
                            TVIS_BOLD | TVIS_EXPANDED, img_idx_root_,
                            new proj_item_info(proj_file_name,
                                               PROJ_ITEM_TYPE_PROJECT, 0,
                                               proj_file_dir,
                                               0, FALSE, FALSE, FALSE,
                                               PROJ_ITEM_OPEN_NONE,
                                               PROJ_ITEM_TYPE_FOLDER, FALSE));
    
    /* 
     *   add the project-level containers if a project is open - don't
     *   bother with this if there's no project, since we can't list any
     *   files unless we have a project 
     */
    if (proj_open_)
    {
        HTREEITEM cur;
        int extres_cnt;
        int i;
        proj_item_info *info;
        char path[OSFNMAX];

        /* fill in the "sources" item */
        info = new proj_item_info(0, PROJ_ITEM_TYPE_FOLDER, "source_files",
                                  0, img_idx_source_, FALSE, TRUE, FALSE,
                                  PROJ_ITEM_OPEN_SRC, PROJ_ITEM_TYPE_SOURCE,
                                  FALSE);
        cur = insert_tree(proj_item, TVI_LAST, "Source Files",
                          TVIS_BOLD | TVIS_EXPANDED, img_idx_folder_, info);

        /* remember the "sources" group */
        src_item_ = cur;

        /* set the system path for library source files */
        os_get_special_path(path, sizeof(path), _pgmptr, OS_GSP_T3_LIB);
        info->set_child_sys_path(path);

        /* load the source files from the configuration */
        load_config_files(config, cur);

        /* fill in the "includes" item */
        info = new proj_item_info(0, PROJ_ITEM_TYPE_FOLDER, "include_files",
                                  0, img_idx_source_, FALSE, TRUE, FALSE,
                                  PROJ_ITEM_OPEN_SRC, PROJ_ITEM_TYPE_HEADER,
                                  FALSE);
        cur = insert_tree(proj_item, TVI_LAST, "Include Files",
                          TVIS_BOLD | TVIS_EXPANDED, img_idx_folder_, info);

        /* set the system path for library header files */
        os_get_special_path(path, sizeof(path), _pgmptr, OS_GSP_T3_INC);
        info->set_child_sys_path(path);

        /* remember the "includes" group */
        inc_item_ = cur;

        /* load the include files from the configuration */
        load_config_files(config, cur);

        /* 
         *   fill in the "resources" item (don't expand this one
         *   initially, since it could be large) 
         */
        cur = insert_tree(proj_item, TVI_LAST, "Resource Files",
                          TVIS_BOLD, img_idx_folder_,
                          new proj_item_info(0, PROJ_ITEM_TYPE_FOLDER,
                                             "image_resources",
                                             0, img_idx_resource_,
                                             FALSE, TRUE, FALSE,
                                             PROJ_ITEM_OPEN_EXTERN,
                                             PROJ_ITEM_TYPE_RESOURCE, FALSE));

        /* load the resource files from the configuration */
        load_config_files(config, cur);

        /* explicitly collapse this list */
        TreeView_Expand(tree_, cur, TVE_COLLAPSE);

        /* get the number of external resource files */
        if (config->getval("build", "ext resfile cnt", &extres_cnt))
            extres_cnt = 0;

        /* load the external resource file configuration */
        for (i = 0 ; i < extres_cnt ; ++i)
        {
            char dispname[50];
            char groupname[50];
            char in_inst[10];
            proj_item_info *info;

            /* build the names */
            gen_extres_names(dispname, groupname, i);

            /* create the information structure */
            info = new proj_item_info(0, PROJ_ITEM_TYPE_EXTRES, groupname,
                                      proj_file_dir, img_idx_resource_,
                                      TRUE, TRUE, FALSE,
                                      PROJ_ITEM_OPEN_EXTERN,
                                      PROJ_ITEM_TYPE_RESOURCE, FALSE);
            
            /* add this resource item */
            cur = insert_tree(TVI_ROOT, TVI_LAST, dispname, TVIS_BOLD,
                              img_idx_extres_, info);

            /* get the in-intsaller status (put it in by default) */
            if (config->getval("build", "ext_resfile_in_install", i,
                               in_inst, sizeof(in_inst)))
                in_inst[0] = 'Y';

            /* set the status */
            info->in_install_ = (in_inst[0] == 'Y');
            set_extres_in_install(cur, info->in_install_);

            /* load the contents of this external resource file */
            load_config_files(config, cur);

            /* explicitly collapse this list */
            TreeView_Expand(tree_, cur, TVE_COLLAPSE);
        }

        /* select the root item */
        TreeView_Select(tree_, proj_item, TVGN_CARET);
    }

    /* load the context menu */
    ctx_menu_container_ = LoadMenu(CTadsApp::get_app()->get_instance(),
                                   MAKEINTRESOURCE(IDR_PROJ_POPUP_MENU));

    /* extract the context submenu from the menu container */
    ctx_menu_ = GetSubMenu(ctx_menu_container_, 0);
}

/*
 *   Reset the project window - delete all sources, includes, etc 
 */
void CHtmlDbgProjWin::reset_proj_win()
{
    HTREEITEM par;
    HTREEITEM cur;
    HTREEITEM nxt;
    const char *new_proj_name;
    
    /* delete all of the external resource files */
    for (cur = TreeView_GetNextSibling(tree_, TreeView_GetRoot(tree_)) ;
         cur != 0 ; cur = nxt)
    {
        /* remember the next item */
        nxt = TreeView_GetNextSibling(tree_, cur);

        /* delete this item */
        TreeView_DeleteItem(tree_, cur);
    }

    /* delete all children from the main project subgroups */
    for (par = TreeView_GetChild(tree_, TreeView_GetRoot(tree_)) ;
         par != 0 ; par = TreeView_GetNextSibling(tree_, par))
    {
        /* delete all children of this group */
        for (cur = TreeView_GetChild(tree_, par) ; cur != 0 ; cur = nxt)
        {
            /* remember the next item */
            nxt = TreeView_GetNextSibling(tree_, cur);

            /* delete this item */
            TreeView_DeleteItem(tree_, cur);
        }
    }

    /* rename the root item */
    if (dbgmain_->get_gamefile_name() != 0
        && dbgmain_->get_gamefile_name()[0] != '\0')
    {
        /* update the root item's name to show the new game file name */
        new_proj_name =
            os_get_root_name((char *)dbgmain_->get_gamefile_name());

        /* 
         *   if we didn't have a project before (in which case the root
         *   item won't have any children), set up the default project
         *   items 
         */
        if (TreeView_GetChild(tree_, TreeView_GetRoot(tree_)) == 0)
        {
            /* a project is now open */
            proj_open_ = TRUE;

            /* delete the old tree control */
            DestroyWindow(tree_);
            tree_ = 0;

            /* re-create the tree control */
            create_tree();
        }
    }
    else
    {
        /* no project is now open */
        proj_open_ = FALSE;

        /* forget the special folders - we're going to delete them */
        inc_item_ = 0;
        src_item_ = 0;

        /* delete the old tree control */
        DestroyWindow(tree_);
        tree_ = 0;

        /* re-create the tree */
        create_tree();
    }
}

/*
 *   Load a group of files from the configuration 
 */
void CHtmlDbgProjWin::load_config_files(CHtmlDebugConfig *config,
                                        HTREEITEM parent)
{
    int cnt;
    int i;
    HTREEITEM after;
    proj_item_info *parent_info;
    char sys_flag_var[128];

    /* get information on the parent item */
    parent_info = get_item_info(parent);

    /* create the configuration variable name for the sys/user flag */
    sprintf(sys_flag_var, "%s-sys", parent_info->config_group_);

    /* get the number of files */
    cnt = config->get_strlist_cnt("build", parent_info->config_group_);

    /* start adding at the start of the parent list */
    after = TVI_FIRST;

    /* load the files */
    for (i = 0 ; i < cnt ; ++i)
    {
        char fname[OSFNMAX];
        char sys_flag[40];
        char type_flag[40];
        HTREEITEM new_item;
        proj_item_info *new_item_info;
        proj_item_type_t new_type;
        
        /* we don't have any idea of the item type yet */
        new_type = PROJ_ITEM_TYPE_NONE;

        /* get this file */
        config->getval("build", parent_info->config_group_,
                       i, fname, sizeof(fname));

        /* read the system/user flag for the file */
        config->getval("build", sys_flag_var, i, sys_flag, sizeof(sys_flag));

        /* if this is the source file list, read the type */
        if (parent == src_item_
            && !config->getval("build", "source_file_types", i,
                               type_flag, sizeof(type_flag)))
        {
            /* we have an explicit type, so note it */
            new_type = (type_flag[0] == 'l'
                        ? PROJ_ITEM_TYPE_LIB
                        : PROJ_ITEM_TYPE_SOURCE);
        }

        /* 
         *   if it's a system file, expand the name, which is stored in the
         *   file relative to the system directory, into the full absolute
         *   path 
         */
        if (sys_flag[0] == 's' && parent_info->child_sys_path_ != 0)
        {
            char buf[OSFNMAX];

            /* build the full path to the system folder */
            os_build_full_path(buf, sizeof(buf),
                               parent_info->child_sys_path_, fname);

            /* use the fully-expanded version of the filename */
            strcpy(fname, buf);
        }
        else if (sys_flag[0] == 'l')
        {
            /*
             *   It's an include file that's found in a library's directory.
             *   Scan the libraries, looking for a library that has a file
             *   with this name in its directory.  
             */
            add_lib_header_path(fname, sizeof(fname));
        }

        /* add it to the list */
        new_item = add_file_to_project(parent, after, fname, new_type);

        /* 
         *   if the new item is a library, load the exclusion list for the
         *   library 
         */
        new_item_info = get_item_info(new_item);
        if (new_item_info != 0 && new_item_info->type_ == PROJ_ITEM_TYPE_LIB)
            load_config_lib_exclude(config, new_item, new_item_info);

        /* if we succeeded, add the next item after this one */
        if (new_item != 0)
            after = new_item;

        /* read the recursion setting, if applicable */
        if (strcmp(parent_info->config_group_, "image_resources") == 0)
        {
            char rbuf[5];

            /* read the recursion status (the default is 'recursive') */
            if (config->getval("build", "image_resources_recurse",
                               i, rbuf, sizeof(rbuf)))
                rbuf[0] = 'Y';

            /* note the status in the new item */
            new_item_info->recursive_ = (rbuf[0] == 'Y');
        }
    }
}

/*
 *   Scan the libraries, looking for a library that has a file with the given
 *   name in its directory.  If we find a library path containing this
 *   header, we'll expand the header file name to include the full library
 *   directory path prefix. 
 */
void CHtmlDbgProjWin::add_lib_header_path(char *fname, size_t fname_size)
{
    /* 
     *   scan the project source tree recursively, starting with the children
     *   of the root source item 
     */
    add_lib_header_path_scan(src_item_, fname, fname_size);
}

/*
 *   Service routine for add_lib_header_path: scan recursively starting with
 *   the children of the given tree item.  Returns true if we find an item,
 *   so recursive callers will know that there's no need to look any further.
 */
int CHtmlDbgProjWin::add_lib_header_path_scan(HTREEITEM parent_item,
                                              char *fname, size_t fname_size)
{
    HTREEITEM cur;

    /* scan the children of the given item */
    for (cur = TreeView_GetChild(tree_, parent_item) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur))
    {
        proj_item_info *cur_info;
        char cur_fname[OSFNMAX];
        char *root;

        /* get this item's associated information */
        cur_info = get_item_info(cur);

        /* if this isn't a library, skip it */
        if (cur_info == 0 || cur_info->type_ != PROJ_ITEM_TYPE_LIB)
            continue;
        
        /* get the item's path */
        get_item_path(cur_fname, sizeof(cur_fname), cur);
        
        /* get the root filename */
        root = os_get_root_name(cur_fname);
        
        /* replace the library name with the new item's filename */
        strcpy(root, fname);
        
        /* if the file exists, take this as its path */
        if (!osfacc(cur_fname))
        {
            /* use the fully-expanded path for the filename */
            strcpy(fname, cur_fname);
            
            /* indicate success */
            return TRUE;
        }

        /* 
         *   we didn't find the file here, but a library can contain other
         *   libraries, so search the children of this item for nested
         *   libraries 
         */
        if (add_lib_header_path_scan(cur, fname, fname_size))
            return TRUE;
    }

    /* didn't find a match - indicate failure */
    return FALSE;
}

/*
 *   Load configuration information determining if an item is excluded from
 *   its library.  Sets the exclude_from_lib_ field in the given object
 *   based on the configuration information.  
 */
void CHtmlDbgProjWin::load_config_lib_exclude(CHtmlDebugConfig *config,
                                              HTREEITEM item,
                                              proj_item_info *info)
{
    char var_name[OSFNMAX + 50];
    int cnt;
    int i;

    /* build the exclusion variable name for this library */
    sprintf(var_name, "lib_exclude:%s", info->fname_);

    /* get the number of excluded files */
    cnt = config->get_strlist_cnt("build", var_name);

    /* scan the excluded files and set the checkmarks accordingly */
    for (i = 0 ; i < cnt ; ++i)
    {
        char url[1024];

        /* get this item */
        config->getval("build", var_name, i, url, sizeof(url));

        /* 
         *   scan children of the library for the item with this URL, and
         *   set its checkbox state to 'excluded' 
         */
        load_config_lib_exclude_url(item, url);
    }
}

/*
 *   Scan children of the given item, and their children, for the given URL,
 *   and mark the given item as excluded by setting its checkbox state. 
 */
int CHtmlDbgProjWin::load_config_lib_exclude_url(
    HTREEITEM parent, const char *url)
{
    HTREEITEM cur;
    
    /* scan the item's children */
    for (cur = TreeView_GetChild(tree_, parent) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur))
    {
        char cur_url[1024];
        
        /* get this item's URL */
        get_item_lib_url(cur_url, sizeof(cur_url), cur);

        /* check it against the one we're looking for */
        if (stricmp(url, cur_url) == 0)
        {
            /* it matches - mark it as excluded */
            set_item_checkmark(cur, FALSE);

            /* remember that it's excluded */
            get_item_info(cur)->exclude_from_lib_ = TRUE;

            /* no need to look any further */
            return TRUE;
        }

        /* check this item's children */
        if (load_config_lib_exclude_url(cur, url))
            return TRUE;
    }

    /* didn't find it */
    return FALSE;
}

/*
 *   Save a group of files to the configuration 
 */
void CHtmlDbgProjWin::save_config_files(CHtmlDebugConfig *config,
                                        HTREEITEM parent)
{
    HTREEITEM cur;
    proj_item_info *parent_info;
    int i;
    char sys_flag_var[128];

    /* get information on the parent item */
    parent_info = get_item_info(parent);

    /* clear any existing string list for this configuration group */
    config->clear_strlist("build", parent_info->config_group_);

    /* create the configuration variable name for the sys/user flag */
    sprintf(sys_flag_var, "%s-sys", parent_info->config_group_);

    /* if it's "image_resources", clear the recursive flag as well */
    if (strcmp(parent_info->config_group_, "image_resources") == 0)
        config->clear_strlist("build", "image_resources_recurse");

    /* load the files */
    for (i = 0, cur = TreeView_GetChild(tree_, parent) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur), ++i)
    {
        proj_item_info *info;
        
        /* get this item's information */
        info = get_item_info(cur);
        
        /* save this item's name */
        config->setval("build", parent_info->config_group_,
                       i, info->fname_);

        /* 
         *   if this is the "source_files" group, add the file type to
         *   indicate whether it's a library or a source file 
         */
        if (parent == src_item_)
            config->setval("build", "source_file_types", i,
                           info->type_ == PROJ_ITEM_TYPE_LIB
                           ? "library" : "source");

        /* if it's a resource file, save its recursion status */
        if (strcmp(parent_info->config_group_, "image_resources") == 0)
            config->setval("build", "image_resources_recurse", i,
                           info->recursive_ ? "Y" : "N");

        /* save the item's system/user status */
        config->setval("build", sys_flag_var, i,
                       info->is_sys_file_
                       ? "system"
                       : info->lib_path_ != 0 && !info->in_user_lib_
                       ? "lib"
                       : "user");

        /* if this is a library, save its exclusion information */
        if (info->type_ == PROJ_ITEM_TYPE_LIB)
            save_config_lib_excl(config, cur);
    }
}

/*
 *   Update the saved configuration information after making a change to a
 *   library exclusion setting.  We'll run through the exclusion list for
 *   the top-level library including the given item and update the
 *   configuration information for the excluded items.  
 */
void CHtmlDbgProjWin::save_config_lib_excl(CHtmlDebugConfig *config,
                                           HTREEITEM lib)
{
    char var_name[OSFNMAX + 50];
    proj_item_info *lib_info;
    int idx;

    /* get the item's information */
    lib_info = get_item_info(lib);

    /* build the exclusion variable name for this library */
    sprintf(var_name, "lib_exclude:%s", lib_info->fname_);

    /* clear the old exclusion list for this top-level library */
    config->clear_strlist("build", var_name);

    /* 
     *   recursively visit the children of this object and add an entry for
     *   each excluded item 
     */
    idx = 0;
    save_config_lib_excl_children(config, lib, var_name, &idx);
}

/*
 *   Save library exclusion configuration information for the children of
 *   the given library.  
 */
void CHtmlDbgProjWin::save_config_lib_excl_children(
    CHtmlDebugConfig *config, HTREEITEM lib, const char *var_name, int *idx)
{
    HTREEITEM cur;

    /* run through this item's children */
    for (cur = TreeView_GetChild(tree_, lib) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur))
    {
        proj_item_info *info;

        /* get this item's information; skip it if there's none */
        info = get_item_info(cur);
        if (info == 0)
            continue;

        /* 
         *   If the item is a library, visit it recursively; otherwise, if
         *   this item is marked as excluded, add an item for it.  Note that
         *   sublibraries can never be excluded from their containing
         *   libraries, so these tests are mutually exclusive.  
         */
        if (info->type_ == PROJ_ITEM_TYPE_LIB)
        {
            /* scan the children of this library */
            save_config_lib_excl_children(config, cur, var_name, idx);
        }
        else if (info->exclude_from_lib_)
        {
            char url[1024];

            /* get this item's URL */
            get_item_lib_url(url, sizeof(url), cur);

            /* add it to the exclusion list for the library */
            config->setval("build", var_name, *idx, url);

            /* advance the variable array counter */
            ++(*idx);
        }
    }
}

/*
 *   Save the project configuration 
 */
void CHtmlDbgProjWin::save_config(CHtmlDebugConfig *config)
{
    HTREEITEM cur;
    int extres_cnt;
    
    /* 
     *   save the configuration information for each child of the first
     *   top-level item - the first item is the main project item, and its
     *   immediate children are the file groups (source, include,
     *   resource) within the main project 
     */
    cur = TreeView_GetRoot(tree_);
    for (cur = TreeView_GetChild(tree_, cur) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur))
    {
        /* save this item's file list */
        save_config_files(config, cur);
    }

    /*
     *   now save each external resource file list - the external resource
     *   files are all at root level 
     */
    for (extres_cnt = 0, cur = TreeView_GetRoot(tree_) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur))
    {
        proj_item_info *info;

        /* get the info on this item */
        info = get_item_info(cur);
        
        /* if this isn't an external resource file, skip it */
        if (info->type_ != PROJ_ITEM_TYPE_EXTRES)
            continue;

        /* save it */
        save_config_files(config, cur);

        /* save the in-installer status */
        config->setval("build", "ext_resfile_in_install", extres_cnt,
                       info->in_install_ ? "Y" : "N");

        /* count it */
        ++extres_cnt;
    }

    /* save the external resource file count */
    config->setval("build", "ext resfile cnt", extres_cnt);
}

/* 
 *   hook procedure for subclassed tree control 
 */
LRESULT CALLBACK CHtmlDbgProjWin::tree_proc(HWND hwnd, UINT msg,
                                            WPARAM wpar, LPARAM lpar)
{
    CHtmlDbgProjWin *win;

    /* get the 'this' pointer from the window properties */
    win = (CHtmlDbgProjWin *)GetProp(hwnd, "CHtmlDbgProjWin.parent.this");

    /* there's nothing we can do if we couldn't get the window */
    if (win == 0)
        return 0;

    /* invoke the non-static version */
    return win->do_tree_proc(hwnd, msg, wpar, lpar);
}

/*
 *   non-static version of tree control hook procedure 
 */
LRESULT CHtmlDbgProjWin::do_tree_proc(HWND hwnd, UINT msg,
                                      WPARAM wpar, LPARAM lpar)
{
    switch(msg)
    {
    case WM_DROPFILES:
        /* do our drop-file procedure */
        do_tree_dropfiles((HDROP)wpar);

        /* handled */
        return 0;

    default:
        /* no special handling required */
        break;
    }

    /* call the original tree-view procedure */
    return CallWindowProc((WNDPROC)tree_defproc_, hwnd, msg, wpar, lpar);
}

/*
 *   Process window destruction 
 */
void CHtmlDbgProjWin::do_destroy()
{
    /* destroy our context menu */
    if (ctx_menu_container_ != 0)
        DestroyMenu(ctx_menu_container_);
    
    /* 
     *   delete each item in the tree explicitly, so that we free the
     *   associated memory 
     */
    while (TreeView_GetRoot(tree_) != 0)
        TreeView_DeleteItem(tree_, TreeView_GetRoot(tree_));

    /* un-subclass the tree control if we subclassed it */
    if (proj_open_)
    {
        /* restore the original window procedure */
        SetWindowLong(tree_, GWL_WNDPROC, (LONG)tree_defproc_);
        
        /* remove our property */
        RemoveProp(tree_, "CHtmlDbgProjWin.parent.this");
    }

    /* tell the debugger to clear my window if I'm the active one */
    dbgmain_->on_close_dbg_win(this);
    
    /* release our reference on the main window */
    dbgmain_->Release();

    /* delete our image lists */
    TreeView_SetImageList(tree_, 0, TVSIL_NORMAL);
    TreeView_SetImageList(tree_, 0, TVSIL_STATE);
    ImageList_Destroy(img_list_);
    ImageList_Destroy(state_img_list_);
    
    /* inherit default */
    CTadsWin::do_destroy();
}

/*
 *   Load my window configuration 
 */
void CHtmlDbgProjWin::load_win_config(CHtmlDebugConfig *config,
                                      const textchar_t *varname,
                                      RECT *pos, int *vis)
{
    CHtmlRect hrc;

    /* get my position from the saved configuration */
    if (!config->getval(varname, "pos", &hrc))
        SetRect(pos, hrc.left, hrc.top,
                hrc.right - hrc.left, hrc.bottom - hrc.top);

    /* get my visibility from the saved configuration */
    if (config->getval(varname, "vis", vis))
        *vis = TRUE;
}

/*
 *   Save my configuration 
 */
void CHtmlDbgProjWin::save_win_config(CHtmlDebugConfig *config,
                                      const textchar_t *varname)
{
    RECT rc;
    CHtmlRect hrc;

    /* save my visibility */
    config->setval(varname, "vis", IsWindowVisible(handle_));

    /* save my position */
    GetWindowRect(handle_, &rc);
    w32tdb_adjust_client_pos(dbgmain_, &rc);
    hrc.set(rc.left, rc.top, rc.right, rc.bottom);
    config->setval(varname, "pos", &hrc);
}

/* 
 *   process a drop-files event in the tree control 
 */
void CHtmlDbgProjWin::do_tree_dropfiles(HDROP hdrop)
{
    HTREEITEM cur, prv;
    HTREEITEM target;
    HTREEITEM parent;
    TV_HITTESTINFO ht;
    HTREEITEM root;
    int cnt;
    int i;

    /* get the root item for easy reference */
    root = TreeView_GetRoot(tree_);

    /* find the tree view list item nearest the drop point */
    DragQueryPoint(hdrop, &ht.pt);
    target = TreeView_HitTest(tree_, &ht);
    
    /* if they're below the last item, insert into the last container */
    if (target == 0)
    {
        /* 
         *   no target - first find the last visible item and pretend they
         *   were pointing to that 
         */
        for (cur = root ; cur != 0 ; 
             prv = cur,
             cur = TreeView_GetNextItem(tree_, cur, TVGN_NEXTVISIBLE)) ;

        /* use the last item we found as the target */
        target = prv;
    }

    /*
     *   Find the nearest enclosing container of the target item that
     *   accepts children 
     */
    for (parent = 0, cur = target ; cur != 0 ;
         cur = TreeView_GetParent(tree_, cur))
    {
        proj_item_info *info;
        
        /* if this item accepts children, it's the parent */
        if ((info = get_item_info(cur)) != 0 && info->can_add_children_)
        {
            /* this is the one - note it and stop looking */
            parent = cur;
            break;
        }
    }

    /* if we found a valid parent, insert the file */
    if (parent != 0)
    {
        /* iterate over each dropped file */
        cnt = DragQueryFile(hdrop, 0xFFFFFFFF, 0, 0);
        for (i = 0 ; i < cnt ; ++i)
        {
            char fname[OSFNMAX];
            HTREEITEM new_item;
            
            /* get this file's name */
            DragQueryFile(hdrop, i, fname, sizeof(fname));

            /* add the file */
            new_item = add_file_to_project(parent, target, fname,
                                           PROJ_ITEM_TYPE_NONE);

            /* 
             *   if we successfully added the item, add the next file (if
             *   any) after this item 
             */
            if (new_item != 0)
                target = new_item;
        }

        /* end the drag operation */
        DragFinish(hdrop);

        /* save the configuration changes */
        save_config(dbgmain_->get_local_config());
    }
    else
    {
        /* end the drag operation */
        DragFinish(hdrop);

        /* 
         *   bring the debugger to the foreground (the drag source window
         *   is probably the foreground window at the moment)
         */
        dbgmain_->bring_to_front();

        /* show a message explaining the problem */
        MessageBox(dbgmain_->get_handle(),
                   "You can't drop files into this part of the project "
                   "window.  If you want to add a source "
                   "file, drop it onto the \"Source Files\" section.  If "
                   "you want to add an image or sound file, drop it onto the "
                   "\"Resource Files\" section.",
                   "TADS Workbench", MB_OK | MB_ICONEXCLAMATION);
    }
}

/*
 *   Insert an item into the tree control
 */
HTREEITEM CHtmlDbgProjWin::insert_tree(HTREEITEM parent, HTREEITEM ins_after,
                                       const textchar_t *txt, DWORD state,
                                       int icon_index, proj_item_info *info)
{
    TV_INSERTSTRUCT tvi;
    HTREEITEM item;
    proj_item_info *parent_info;

    /* build the insertion record */
    tvi.hParent = parent;
    tvi.hInsertAfter = ins_after;
    tvi.item.mask = TVIF_TEXT | TVIF_STATE | TVIF_PARAM
                    | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    tvi.item.state = state;
    tvi.item.stateMask = 0xffffffff;
    tvi.item.pszText = (char *)txt;
    tvi.item.iImage = tvi.item.iSelectedImage = icon_index;
    tvi.item.lParam = (LPARAM)info;

    /* insert it into the tree */
    item = TreeView_InsertItem(tree_, &tvi);

    /* get the parent information  */
    parent_info = get_item_info(parent);

    /* 
     *   make sure the parent is expanded, unless the parent is a library
     *   (we leave libraries closed initially, because many users wish to
     *   treat them as opaque and thus don't wish to see their contents) 
     */
    if (parent != TVI_ROOT
        && (parent_info == 0 || parent_info->type_ != PROJ_ITEM_TYPE_LIB))
    {
        /* expand the parent */
        TreeView_Expand(tree_, parent, TVE_EXPAND);
    }

    /* select the newly-inserted item */
    TreeView_SelectItem(tree_, item);

    /* return the new item handle */
    return item;
}

/*
 *   delete the current tree selection 
 */
void CHtmlDbgProjWin::delete_tree_selection()
{
    HTREEITEM sel;
    proj_item_info *info;
    int is_extres;
    
    /* get the selected item in the tree control */
    sel = TreeView_GetSelection(tree_);

    /* get the associated information */
    info = get_item_info(sel);

    /* if the item is marked as non-deletable, ignore the request */
    if (info == 0 || !info->can_delete_)
        return;

    /* note if it's an external resource file */
    is_extres = (info->type_ == PROJ_ITEM_TYPE_EXTRES);

    /* 
     *   if this is an external resource item with children, confirm the
     *   deletion 
     */
    if (is_extres && TreeView_GetChild(tree_, sel) != 0)
    {
        int resp;
        
        /* ask for confirmation */
        resp = MessageBox(dbgmain_->get_handle(),
                          "Are you sure you want to remove this external "
                          "file and all of the resources it contains "
                          "from the project?", "TADS Workbench",
                          MB_YESNO | MB_ICONQUESTION);

        /* if they didn't say yes, abort */
        if (resp != IDYES)
            return;
    }

    /* delete it */
    TreeView_DeleteItem(tree_, sel);

    /* 
     *   if we just deleted an external resource file, renumber the
     *   remaining external resources 
     */
    if (is_extres)
        fix_up_extres_numbers();

    /* save the configuration */
    save_config(dbgmain_->get_local_config());
}


/*
 *   gain focus 
 */
void CHtmlDbgProjWin::do_setfocus(HWND /*prev_win*/)
{
    /* give focus to our tree control */
    SetFocus(tree_);
}

/*
 *   resize the window 
 */
void CHtmlDbgProjWin::do_resize(int mode, int x, int y)
{
    RECT rc;
    
    switch(mode)
    {
    case SIZE_MAXHIDE:
    case SIZE_MAXSHOW:
    case SIZE_MINIMIZED:
        /* don't bother resizing the subwindows on these changes */
        break;

    case SIZE_MAXIMIZED:
    case SIZE_RESTORED:
    default:
        /* resize the tree control to fill our client area */
        GetClientRect(handle_, &rc);
        MoveWindow(tree_, 0, 0, rc.right, rc.bottom, TRUE);
        break;
    }

    /* inherit default handling */
    CTadsWin::do_resize(mode, x, y);
}

/*
 *   Activate/deactivate.  Notify the debugger main window of the coming
 *   or going of this debugger window as the active debugger window, which
 *   will allow us to receive certain command messages from the main
 *   window.  
 */
int CHtmlDbgProjWin::do_ncactivate(int flag)
{
    /* register or deregister with the main window */
    if (dbgmain_ != 0)
    {
        if (flag)
            dbgmain_->set_active_dbg_win(this);
        else
            dbgmain_->clear_active_dbg_win();
    }

    /* inherit default handling */
    return CTadsWin::do_ncactivate(flag);
}

/*
 *   Receive notification of activation from parent 
 */
void CHtmlDbgProjWin::do_parent_activate(int flag)
{
    /* register or deregister with the main window */
    if (dbgmain_ != 0)
    {
        if (flag)
            dbgmain_->set_active_dbg_win(this);
        else
            dbgmain_->clear_active_dbg_win();
    }

    /* inherit default handling */
    CTadsWin::do_parent_activate(flag);
}

/*
 *   check a command's status 
 */
TadsCmdStat_t CHtmlDbgProjWin::check_command(int command_id)
{
    TadsCmdStat_t stat;

    /* try handling it using the active-window mechanism */
    if ((stat = active_check_command(command_id)) != TADSCMD_UNKNOWN)
        return stat;

    /* let the debugger main window handle it */
    return dbgmain_->check_command(command_id);
}

/*
 *   process a command 
 */
int CHtmlDbgProjWin::do_command(int notify_code, int command_id, HWND ctl)
{
    /* try handling it using the active-window mechanism */
    if (active_do_command(notify_code, command_id, ctl))
        return TRUE;

    /* let the debugger main window handle it */
    return dbgmain_->do_command(notify_code, command_id, ctl);
}

/*
 *   Check the status of a command routed from the main window 
 */
TadsCmdStat_t CHtmlDbgProjWin::active_check_command(int command_id)
{
    HTREEITEM sel;
    HTREEITEM parent;
    proj_item_info *sel_info;
    proj_item_info *par_info;
    
    /* get the current selection and its associated information */
    sel = TreeView_GetSelection(tree_);
    sel_info = get_item_info(sel);

    /* get the parent of the selection as well */
    if (sel != 0)
    {
        parent = TreeView_GetParent(tree_, sel);
        par_info = get_item_info(parent);
    }
    else
    {
        parent = 0;
        par_info = 0;
    }

    /* check the command */
    switch(command_id)
    {
    case ID_PROJ_OPEN:
        /* enable it if the selection is openable */
        return (sel_info != 0 && sel_info->openable_
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_FILE_EDIT_TEXT:
        /* 
         *   enable it if the selection is openable, and its open action
         *   is "open as source" 
         */
        return (sel_info != 0 && sel_info->openable_
                && par_info->child_open_action_ == PROJ_ITEM_OPEN_SRC)
            ? TADSCMD_ENABLED : TADSCMD_DISABLED;

    case ID_PROJ_REMOVE:
        /* enable it if the item is marked as deletable */
        return (sel_info != 0 && sel_info->can_delete_
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_PROJ_ADDFILE:
    case ID_PROJ_ADDEXTRES:
    case ID_PROJ_ADDRESDIR:
        /* always enable these, as long as a project is open */
        return proj_open_ ? TADSCMD_ENABLED : TADSCMD_DISABLED;

    case ID_PROJ_INCLUDE_IN_INSTALL:
        /*
         *   if it's an external resource file, enable the command and set
         *   the appropriate checkmark state; otherwise disable the
         *   command 
         */
        if (sel_info != 0 && sel_info->type_ == PROJ_ITEM_TYPE_EXTRES)
        {
            /* check it if it's in the install, uncheck if not */
            return (sel_info->in_install_
                    ? TADSCMD_CHECKED : TADSCMD_ENABLED);
        }
        else
        {
            /* it's not an external resource file - disable it */
            return TADSCMD_DISABLED;
        }
    }
    
    /* we don't currently handle any commands */
    return TADSCMD_UNKNOWN;
}

/*
 *   execute a command routed from the main window 
 */
int CHtmlDbgProjWin::active_do_command(int notify_code, int command_id,
                                       HWND ctl)
{
    HTREEITEM sel;

    /* get the current selection */
    sel = TreeView_GetSelection(tree_);

    /* check the command */
    switch(command_id)
    {
    case ID_PROJ_OPEN:
        /* open the selection */
        open_item(sel);
        return TRUE;

    case ID_FILE_EDIT_TEXT:
        /* open the selection in a text editor */
        open_item_in_text_editor(sel);
        return TRUE;

    case ID_PROJ_REMOVE:
        /* remove the selected item */
        delete_tree_selection();
        return TRUE;

    case ID_PROJ_ADDFILE:
        /* if no project is open, ignore it */
        if (!proj_open_)
            return TRUE;

        /* go add a file */
        add_project_file();

        /* handled */
        return TRUE;

    case ID_PROJ_ADDRESDIR:
        /* if no project is open, ignore it */
        if (!proj_open_)
            return TRUE;

        /* go add a diretory */
        add_project_dir();

        /* handled */
        return TRUE;
        
    case ID_PROJ_ADDEXTRES:
        /* if no project is open, ignore it */
        if (!proj_open_)
            return TRUE;

        /* go add an external resource file */
        add_ext_resfile();

        /* handled */
        return TRUE;

    case ID_PROJ_INCLUDE_IN_INSTALL:
        /* make sure a project is open and they clicked on some item */
        if (proj_open_ && sel != 0)
        {
            proj_item_info *sel_info;
            
            /* get the information for this item */
            sel_info = get_item_info(sel);

            /* make sure it's an external resource */
            if (sel_info != 0 && sel_info->type_ == PROJ_ITEM_TYPE_EXTRES)
            {
                /* reverse the in-install state */
                sel_info->in_install_ = !sel_info->in_install_;

                /* update the visual state */
                set_extres_in_install(sel, sel_info->in_install_);

                /* save the configuration change */
                save_config(dbgmain_->get_local_config());
            }
        }

        /* handled */
        return TRUE;
    }

    /* not handled */
    return FALSE;
}

/*
 *   Add a project file 
 */
void CHtmlDbgProjWin::add_project_file()
{
    HTREEITEM cont;
    HTREEITEM sel;
    proj_item_info *cont_info;
    proj_item_info *sel_info;
    OPENFILENAME info;
    char fname[1024];
    char *p;
    
    /* make sure the project window is visible */
    SendMessage(dbgmain_->get_handle(), WM_COMMAND, ID_VIEW_PROJECT, 0);

    /* 
     *   First, determine which container is selected.  If a child is
     *   selected, insert into the child's container. 
     */
    for (cont = sel = TreeView_GetSelection(tree_) ; cont != 0 ;
         cont = TreeView_GetParent(tree_, cont))
    {
        /* get this item's information */
        cont_info = get_item_info(cont);

        /* 
         *   if this item allows children, or it's a library, it's the
         *   container 
         */
        if (cont_info != 0
            && (cont_info->can_add_children_
                || cont_info->type_ == PROJ_ITEM_TYPE_LIB))
            break;
    }

    /* if we didn't find a valid container, complain and give up */
    if (cont == 0)
    {
        /* explain the problem */
        MessageBox(dbgmain_->get_handle(),
                   "Before adding a new file, please select the part "
                   "of the project that will contain the file.  Select "
                   "\"Source Files\" to add a source file, or "
                   "\"Resource Files\" to add an image or sound file.",
                   "TADS Workbench", MB_OK | MB_ICONEXCLAMATION);

        /* 
         *   make sure the project window is the active window (we need to
         *   wait until after the message box is dismissed, to ensure we
         *   leave the project window selected if it's docked; we also did
         *   this before the message box to ensure the project window is
         *   visible while the message box is being displayed) 
         */
        SendMessage(dbgmain_->get_handle(), WM_COMMAND, ID_VIEW_PROJECT, 0);

        /* stop now - the user must select something and try again */
        return;
    }

    /* get information on the selection */
    sel_info = get_item_info(sel);

    /* if the ocntainer is a library, complain and give up */
    if ((cont_info != 0 && cont_info->type_ == PROJ_ITEM_TYPE_LIB)
        || (sel_info != 0 && sel_info->type_ == PROJ_ITEM_TYPE_LIB))
    {
        /* explain the problem */
        MessageBox(dbgmain_->get_handle(),
                   "You cannot change the contents of a library "
                   "in the Project window.  If you want to add a file "
                   "to this library, you must edit the library's definition "
                   "file using the \"Open File in Text Editor\" command "
                   "on the \"Edit\" menu.",
                   "TADS Workbench", MB_OK | MB_ICONEXCLAMATION);

        /* select the library in the project window */
        TreeView_SelectItem(tree_, cont);

        /* activate the project window */
        SendMessage(dbgmain_->get_handle(), WM_COMMAND, ID_VIEW_PROJECT, 0);

        /* done */
        return;
    }

    /* set the default file selector options */
    info.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST
                 | OFN_NOCHANGEDIR | OFN_HIDEREADONLY
                 | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

    /* set up the dialog to ask for the appropriate type of file */
    switch(cont_info->child_type_)
    {
    case PROJ_ITEM_TYPE_SOURCE:
        /* set up to ask for a source file */
        info.lpstrTitle = "Select source files to add";
        info.lpstrFilter = "Source files\0*.t\0"
                           "Library files\0*.tl\0"
                           "All Files\0*.*\0\0";
        info.lpstrDefExt = "t";
        break;

    case PROJ_ITEM_TYPE_HEADER:
        /* set up to ask for a header file */
        info.lpstrTitle = "Select #include files to add";
        info.lpstrFilter = "Include files\0*.t;*.h\0"
                           "All Files\0*.*\0\0";
        info.lpstrDefExt = "h";
        break;

    case PROJ_ITEM_TYPE_RESOURCE:
    default:
        /* set up to ask for a resource file */
        info.lpstrTitle = "Select resource files to add";
        info.lpstrFilter = "JPEG Images\0*.jpg;*.jpeg;*.jpe\0"
                           "PNG Images\0*.png\0"
                           "Wave Sounds\0*.wav\0"
                           "MPEG Sounds\0*.mp3\0"
                           "MIDI Music\0*.mid;*.midi\0"
                           "All Files\0*.*\0\0";

        /* there's no default filename extension for resource files */
        info.lpstrDefExt = 0;

        /* allow adding resource files that don't exist yet */
        info.Flags &= ~OFN_FILEMUSTEXIST;
        break;
    }

    /* ask for a file or files to add to the project */
    info.lStructSize = sizeof(info);
    info.hwndOwner = dbgmain_->get_handle();
    info.hInstance = CTadsApp::get_app()->get_instance();
    info.lpstrCustomFilter = 0;
    info.nFilterIndex = 0;
    info.lpstrFile = fname;
    fname[0] = '\0';
    info.nMaxFile = sizeof(fname);
    info.lpstrFileTitle = 0;
    info.nMaxFileTitle = 0;
    info.lpstrInitialDir = CTadsApp::get_app()->get_openfile_dir();
    info.nFileOffset = 0;
    info.nFileExtension = 0;
    info.lCustData = 0;
    info.lpfnHook = 0;
    info.lpTemplateName = 0;
    CTadsDialog::set_filedlg_center_hook(&info);
    if (!GetOpenFileName(&info))
        return;
    
    /* 
     *   check for a single filename, and regularize the filename if
     *   that's what they selected 
     */
    if (info.nFileOffset != 0)
        fname[info.nFileOffset - 1] = '\0';

    /* add each file in the list */
    for (p = fname + info.nFileOffset ; *p != '\0' ; p += strlen(p) + 1)
    {
        char fullname[OSFNMAX];
        HTREEITEM new_item;

        /* get the path part of the name */
        strcpy(fullname, fname);

        /* add a slash if we don't have one already */
        if (info.nFileOffset < 2 || fname[info.nFileOffset - 2] != '\\')
            strcat(fullname, "\\");

        /* append the filename */
        strcat(fullname, p);

        /* add this file */
        new_item = add_file_to_project(cont, sel, fullname,
                                       PROJ_ITEM_TYPE_NONE);

        /* 
         *   if we successfully added a file, add the next file (if any)
         *   after this new file, so that we keep them in order 
         */
        if (new_item != 0)
            sel = new_item;
    }

    /* set the path for future open-file dialog calls */
    CTadsApp::get_app()->set_openfile_path(fname);

    /* save the configuration */
    save_config(dbgmain_->get_local_config());
}

/*
 *   Add a folder to the project (for resources only)
 */
void CHtmlDbgProjWin::add_project_dir()
{
    HTREEITEM cont;
    HTREEITEM sel;
    proj_item_info *cont_info;
    char fname[OSFNMAX];

    /* make sure the project window is visible */
    SendMessage(dbgmain_->get_handle(), WM_COMMAND, ID_VIEW_PROJECT, 0);

    /* 
     *   First, determine which container is selected.  If a child is
     *   selected, insert into the child's container.  
     */
    for (cont = sel = TreeView_GetSelection(tree_) ; cont != 0 ;
         cont = TreeView_GetParent(tree_, cont))
    {
        /* get this item's information */
        cont_info = get_item_info(cont);

        /* 
         *   if this item allows children, and its children are resource
         *   files, it's the parent we're looking for 
         */
        if (cont_info != 0 && cont_info->can_add_children_
            && cont_info->child_type_ == PROJ_ITEM_TYPE_RESOURCE)
            break;
    }

    /* if we didn't find a valid container, complain and give up */
    if (cont == 0)
    {
        HTREEITEM cur;
        int found_extres;
        
        /*
         *   First, check to see if there are any external resource files.
         *   If there aren't, this request is unambiguous - simply add to
         *   the resources in the image file.  
         */
        for (found_extres = FALSE, cur = TreeView_GetRoot(tree_) ; cur != 0 ;
             cur = TreeView_GetNextSibling(tree_, cur))
        {
            proj_item_info *info;
            
            /* get this item's information */
            info = get_item_info(cur);

            /* if it's an external resource file, note it */
            if (info->type_ == PROJ_ITEM_TYPE_EXTRES)
            {
                found_extres = TRUE;
                break;
            }
        }

        /* 
         *   if we found an external resource file, the request is
         *   ambiguous, so we can't go on 
         */
        if (found_extres)
        {
            /* explain the problem */
            MessageBox(dbgmain_->get_handle(),
                       "Before adding a new directory, please select the "
                       "resource area to contain the folder.  Select "
                       "\"Resource Files\" to add to the image file, or "
                       "select an external resource file.",
                       "TADS Workbench", MB_OK | MB_ICONEXCLAMATION);

            /* 
             *   make sure the project window is the active window (we
             *   need to wait until after the message box is dismissed, to
             *   ensure we leave the project window selected if it's
             *   docked; we also did this before the message box to ensure
             *   the project window is visible while the message box is
             *   being displayed) 
             */
            SendMessage(dbgmain_->get_handle(), WM_COMMAND,
                        ID_VIEW_PROJECT, 0);
            
            /* stop now - the user must select something and try again */
            return;
        }

        /* 
         *   find the "resource files" section of the image file - it's
         *   one of the children of the root item 
         */
        for (cur = TreeView_GetChild(tree_, TreeView_GetRoot(tree_)) ;
             cur != 0 ; cur = TreeView_GetNextSibling(tree_, cur))
        {
            proj_item_info *info;

            /* get this item's information */
            info = get_item_info(cur);

            /* if it's the resource container, it's what we want */
            if (info->child_type_ == PROJ_ITEM_TYPE_RESOURCE)
            {
                /* this is our target container */
                cont = cur;
                break;
            }
        }
    }

    /* browse for a folder */
    if (!CTadsDialogFolderSel2::
        run_select_folder(dbgmain_->get_handle(),
                          CTadsApp::get_app()->get_instance(),
                          "Folder to add to the resources:",
                          "Select a Resource Folder",
                          fname, sizeof(fname),
                          CTadsApp::get_app()->get_openfile_dir(), 0, 0))
        return;

    /* add the folder */
    add_file_to_project(cont, sel, fname, PROJ_ITEM_TYPE_NONE);

    /* save the configuration */
    save_config(dbgmain_->get_local_config());
}


/* ------------------------------------------------------------------------ */
/*
 *   Clear out the list of include files 
 */
void CHtmlDbgProjWin::clear_includes()
{
    HTREEITEM cur;
    HTREEITEM nxt;

    /* scan the children of the "Include Files" section */
    for (cur = TreeView_GetChild(tree_, inc_item_) ; cur != 0 ; cur = nxt)
    {
        /* remember the next sibling */
        nxt = TreeView_GetNextSibling(tree_, cur);
        
        /* delete this item */
        TreeView_DeleteItem(tree_, cur);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Add a scanned include file to the project 
 */
void CHtmlDbgProjWin::add_scanned_include(const char *fname)
{
    char full_name[OSFNMAX];
    HTREEITEM cur;

    /* get the full path name of the file to be added */
    GetFullPathName(fname, sizeof(full_name), full_name, 0);

    /* scan the existing children of the "Include Files" section */
    for (cur = TreeView_GetChild(tree_, inc_item_) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur))
    {
        char cur_name[OSFNMAX];

        /* get this item's full path */
        get_item_path(cur_name, sizeof(cur_name), cur);

        /* 
         *   if this item's full path matches the full path of the item to
         *   be added, there's no need to add this item 
         */
        if (stricmp(cur_name, full_name) == 0)
            return;
    }

    /* 
     *   we didn't find this item among the current include files, so add it
     *   to the Include Files section 
     */
    add_file_to_project(inc_item_, TVI_LAST, fname, PROJ_ITEM_TYPE_NONE);
}


/* ------------------------------------------------------------------------ */
/*
 *   Service routine: determine if the given filename is hierarchically below
 *   the given directory.  Both files are given as absolute paths.  Returns
 *   true if the file is within the given directory, or within one of its
 *   subdirectories.  
 */
static int is_file_below_dir(const char *fname, const char *dir)
{
    char rel[OSFNMAX];
    char path[OSFNMAX];

    /* re-express fname relative to dir */
    oss_get_rel_path(rel, sizeof(rel), fname, dir);

    /* extract the path from the relative name */
    os_get_path_name(path, sizeof(path), rel);

    /* if the path is empty, the file is directly within the directory */
    if (path[0] == '\0')
        return TRUE;

    /* 
     *   if the relatively-expressed filename is absolute, then there is
     *   nothing in common between the two paths, so the file is definitely
     *   not within the directory 
     */
    if (path[0] == '\\' || path[1] == ':')
        return FALSE;

    /*
     *   The path seems to be expressed relatively, and it's non-empty.
     *   There are two possibilities: the file is within the directory, or
     *   is within a parent of the directory or a subdirectory of a parent.
     *   In the latter case, the relatively-expressed path must start with
     *   ".."; so, if the path starts with "..", the file is not within the
     *   directory, otherwise it is.  
     */
    if (memcmp(path, "..", 2) == 0)
    {
        /* 
         *   it's relative to a parent of the directory, so it's not in the
         *   directory 
         */
        return FALSE;
    }
    else
    {
        /* it's relative to the directory, so it's in the directory */
        return TRUE;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Library path searcher.  We'll look through the "srcdir" settings in the
 *   global configuration, then through the entries in the TADSLIB
 *   environment variable, if there is one.  
 */
CLibSearchPathList::CLibSearchPathList(CHtmlDebugConfig *config)
{
    size_t len;
    
    /* remember the config object */
    config_ = config;
    
    /* reset to the start of the search */
    reset();
    
    /* read TADSLIB from the environment */
    if ((len = GetEnvironmentVariable("TADSLIB", 0, 0)) != 0)
    {
        /* allocate space */
        tadslib_ = (char *)th_malloc(len);
        
        /* read the variable */
        GetEnvironmentVariable("TADSLIB", tadslib_, len);
        
        /* start at the beginning of the variable */
        tadslib_p_ = tadslib_;
    }
    else
    {
        /* we have no TADSLIB, so clear the pointers to it */
        tadslib_ = tadslib_p_ = 0;
    }
}

CLibSearchPathList::~CLibSearchPathList()
{
    /* if we have a TADSLIB value stored, free the memory */
    if (tadslib_ != 0)
        th_free(tadslib_);
}

/* is there anything left in the search? */
int CLibSearchPathList::has_more() const
{
    /* if there's another "srcdir" entry, we have more entries */
    if (srcdir_cur_ < srcdir_cnt_)
        return TRUE;
    
    /* if there's anything left in the TADSLIB string, we have more */
    if (tadslib_p_ != 0 && *tadslib_p_ != '\0')
        return TRUE;
    
    /* we don't have anything left */
    return FALSE;
}

/* get the next entry; returns true on success, false on failure */
int CLibSearchPathList::get_next(char *buf, size_t buflen)
{
    int ret;
    
    /* presume failure */
    ret = FALSE;
    
    /* if we haven't exhausted the "srcdir" paths, read the next one */
    if (srcdir_cur_ < srcdir_cnt_)
    {
        /* read the next variable */
        ret = !config_->getval("srcdir", 0, srcdir_cur_, buf, buflen);
        
        /* skip to the next one */
        ++srcdir_cur_;
        
        /* return the success result */
        return ret;
    }
    
    /* check for a TADSLIB entry */
    if (tadslib_p_ != 0 && *tadslib_p_ != '\0')
    {
        char *p;
        size_t len;
        
        /* scan to the next semicolon */
        for (p = tadslib_p_ ; *p != '\0' && *p != ';' ; ++p) ;
        
        /* if we have space in the buffer, copy the result */
        if ((len = p - tadslib_p_) < buflen)
        {
            /* copy the data */
            memcpy(buf, tadslib_p_, len);
            
            /* null-terminate the result */
            buf[len] = '\0';
            
            /* success */
            ret = TRUE;
        }
        
        /* skip to the next non-semicolon */
        for ( ; *p == ';' ; ++p) ;
        
        /* this is the start of the next element in the list */
        tadslib_p_ = p;
        
        /* return the result */
        return ret;
    }
    
    /* we didn't find anything - return failure */
    return FALSE;
}

/* reset to the start of the search */
void CLibSearchPathList::reset()
{
    /* get the number of entries in the user library list */
    srcdir_cnt_ = config_->get_strlist_cnt("srcdir", 0);
    
    /* start at the first entry */
    srcdir_cur_ = 0;
    
    /* start at the beginning of TADSLIB */
    tadslib_p_ = tadslib_;
}

/*
 *   Conduct a search. 
 */
int CLibSearchPathList::find_tl_file(CHtmlDebugConfig *config,
                                     const char *fname,
                                     char *path, size_t pathlen)
{
    /* 
     *   if we find the file in the working directory, or if the file has an
     *   absolute path and thus can be found without a path, there's no need
     *   for a path - just return 'true' with an empty path 
     */
    if (!osfacc(fname))
    {
        /* we can find the filename as given, without an extra path prefix */
        path[0] = '\0';
        return TRUE;
    }

    /* set up a search context */
    CLibSearchPathList lst(config);

    /* scan the directories */
    while (lst.has_more())
    {
        char full_name[OSFNMAX];
        
        /* get the next path */
        lst.get_next(path, pathlen);

        /* build the full filename */
        os_build_full_path(full_name, sizeof(full_name), path, fname);

        /* if this file exists, we found it */
        if (!osfacc(full_name))
            return TRUE;
    }

    /* return failure */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Add a file to the project under the given container 
 */
HTREEITEM CHtmlDbgProjWin::add_file_to_project(HTREEITEM cont,
                                               HTREEITEM after,
                                               const char *fname,
                                               proj_item_type_t known_type)
{
    char new_fname[OSFNMAX];
    char parent_path[OSFNMAX];
    proj_item_info *cont_info;
    proj_item_info *info;
    char rel_fname[OSFNMAX];
    size_t len;
    HTREEITEM cur;
    int is_dir;
    void *search_ctx;
    char buf[OSFNMAX];
    char buf2[OSFNMAX];
    int icon_idx;
    HTREEITEM new_item;
    CHtmlDebugConfig *config = dbgmain_->get_local_config();
    const char *sys_path;
    int is_sys_file;
    proj_item_type_t new_item_type;
    proj_item_type_t new_child_type;
    proj_item_open_t new_child_open;
    int new_child_icon;
    char new_child_path[OSFNMAX];
    int can_delete;
    int is_lib_inc;
    int is_from_user_lib;

    /* get the container's information */
    cont_info = get_item_info(cont);

    /* 
     *   check to see if the file is a directory - if it is, we'll use a
     *   separate icon for it 
     */
    search_ctx = os_find_first_file("", fname, buf, sizeof(buf),
                                    &is_dir, buf2, sizeof(buf2));
    if (search_ctx != 0)
    {
        /* we just needed to find the file - cancel the search */
        os_find_close(search_ctx);
    }
    else
    {
        /* we didn't find a match - assume it's a normal file */
        is_dir = FALSE;
    }

    /*
     *   If this file has an absolute path, and no such file exists, strip
     *   off the absolute path and keep just the root name, so that we can
     *   look for the root name in the usual places.  Chances are that the
     *   project was moved from another computer with a different directory
     *   layout; the easiest way to fix up the project for the new machine is
     *   to convert to a local path and use the usual search mechanism.  
     */
    if (os_is_file_absolute(fname) && osfacc(fname))
    {
        /* it's an absolute path and doesn't exist - drop the path */
        strcpy(new_fname, os_get_root_name((char *)fname));
        fname = new_fname;
    }

    /* presume it's not a system file */
    is_sys_file = FALSE;

    /* 
     *   If the container has a valid system path for its child files, check
     *   to see if this is a system file: it's a system file if it's in the
     *   system directory associated with this container.  (The system
     *   directory varies by container; for the "source" container, it's the
     *   system library directory, and for the "include" container, it's the
     *   system header directory.)  
     */
    sys_path = cont_info->child_sys_path_;
    if (sys_path != 0)
    {
        char full_fname[OSFNMAX];
        size_t sys_path_len;

        /* get the full path name of the file we're adding */
        GetFullPathName(fname, sizeof(full_fname), full_fname, 0);

        /* get the length of the parent's system path, if it has one */
        sys_path_len = strlen(sys_path);

        /* 
         *   if the parent has a system path, and this name is relative to
         *   the system directory, mark it as a system file and store its
         *   name relative to the system directory; otherwise, store its name
         *   relative to the parent item's child path 
         */
        if (strlen(full_fname) > sys_path_len
            && memicmp(full_fname, sys_path, sys_path_len) == 0
            && (full_fname[sys_path_len] == '\\'
                || full_fname[sys_path_len] == '/'))
        {
            /* note that it's a system file */
            is_sys_file = TRUE;
            
            /* the system path is the parent path */
            strcpy(parent_path, sys_path);
        }
    }

    /* 
     *   if it's not a system file, get the parent item's path to its child
     *   items - we'll store the new item's filename relative to this path 
     */
    if (!is_sys_file)
        get_item_child_dir(parent_path, sizeof(parent_path), cont);

    /* 
     *   if we're adding a directory to something other than a resource
     *   container, ignore the request 
     */
    if (is_dir && cont_info->child_type_ != PROJ_ITEM_TYPE_RESOURCE)
        return 0;

    /* 
     *   if the container is the target item, insert at the start of the
     *   container's list 
     */
    if (after == cont)
        after = TVI_FIRST;
    else if (after == 0)
        after = TVI_LAST;

    /* generate a relative version of the filename path */
    oss_get_rel_path(rel_fname, sizeof(rel_fname), fname, parent_path);

    /* presume it's not an include file from a library directory */
    is_lib_inc = FALSE;
    is_from_user_lib = FALSE;

    /*
     *   If it's a header file, and it's not a system file, check to see if
     *   it's relative to a library directory.  If so, mark it as a library
     *   include file.  
     */
    if (cont == inc_item_
        && !is_sys_file
        && !is_file_below_dir(fname, parent_path))
    {
        /* scan the source tree for a library containing this file */
        is_lib_inc = scan_for_lib_inc_path(fname,
                                           parent_path, sizeof(parent_path),
                                           rel_fname, sizeof(rel_fname));
    }

    /*
     *   If it's a source file, and it's not a system file, check to see if
     *   it comes from somewhere on the library path.  Make this check if the
     *   filename as given doesn't exist, or it's not within the main project
     *   directory.  
     */
    if (cont == src_item_
        && !is_sys_file
        && (!is_file_below_dir(fname, parent_path) || osfacc(fname)))
    {
        CHtmlDebugConfig *gconfig = dbgmain_->get_global_config();
        char buf[OSFNMAX];
        int found;
        int is_abs;
        CLibSearchPathList pathlist(gconfig);

        /* note if the filename is absolute */
        is_abs = os_is_file_absolute(fname);

    search_lib_path:
        /* reset to the start of the search path */
        pathlist.reset();

        /* scan the library directory list */
        for (found = FALSE ; pathlist.has_more() ; )
        {
            /* get this entry; if it's not valid, keep looping */
            if (!pathlist.get_next(buf, sizeof(buf)))
                continue;

            /*
             *   If the filename given was absolute, then we know exactly
             *   where the file is, so merely check to see if this absolute
             *   path happens to be the same as the current search item's
             *   directory - if so, then we can express the name relative to
             *   the search item, eliminating the need to refer to it by an
             *   absolute path in the project file.
             *   
             *   If the name given was relative, then check to see if we can
             *   find this file in this search item's directory.  If so, then
             *   we can note that this search item is the parent directory of
             *   this file.  
             */
            if (is_abs)
            {
                /* check to see if the file is in this search directory */
                if (is_file_below_dir(fname, buf))
                {
                    /* note that we found a match */
                    found = TRUE;

                    /* get the filename relative to the library path */
                    oss_get_rel_path(rel_fname, sizeof(rel_fname),
                                     fname, buf);
                }
            }
            else
            {
                /* relative - check for an existing file in this folder */
                os_build_full_path(buf2, sizeof(buf2), buf, fname);

                /* if the file exists, use this location */
                if (!osfacc(buf2))
                {
                    /* note that we found a match */
                    found = TRUE;

                    /* the original name is the relative name */
                    strcpy(rel_fname, fname);

                    /* build the full absolute path for internal use */
                    os_build_full_path(new_fname, sizeof(new_fname),
                                       buf, rel_fname);
                    fname = new_fname;
                }
            }

            /* if we found a match, apply it */
            if (found)
            {
                /* mark it as a user library file */
                is_from_user_lib = TRUE;

                /* the parent path is the library path */
                strcpy(parent_path, buf);

                /* stop now, since we want to take the first match we find */
                break;
            }
        }

        /*
         *   If we didn't find the file, run the appropriate dialog to tell
         *   the user about the problem and give them a chance to correct it.
         */
        if (!found)
        {
            int id;

            /* 
             *   If the file was absolute, run the library path adder dialog;
             *   otherwise, run the file locator dialog.  If the dialog
             *   indicates that it added to the library search list, go back
             *   for another try at finding the file, since we might have
             *   added the file's location to the search list.  
             */
            if (is_abs)
            {
                CTadsDlgAbsPath *dlg;

                /* 
                 *   The source has an absolute path, and the file exists on
                 *   the local system.  Run the dialog offering to add the
                 *   directory to the library search list and convert the
                 *   filename to relative notation.  
                 */
                dlg = new CTadsDlgAbsPath(dbgmain_, fname);
                id = dlg->run_modal(DLG_ABS_SOURCE, dbgmain_->get_handle(),
                                    CTadsApp::get_app()->get_instance());
                delete dlg;
            }
            else
            {
                CTadsDlgMissingSrc *dlg;

                /*
                 *   The file has a relative name, and we couldn't find the
                 *   file in any of our search locations.  We probably just
                 *   need to add a new location to the library search list,
                 *   so run the missing-source dialog.  
                 */
                dlg = new CTadsDlgMissingSrc(dbgmain_, fname);
                id = dlg->run_modal(DLG_MISSING_SOURCE,
                                    dbgmain_->get_handle(),
                                    CTadsApp::get_app()->get_instance());
                delete dlg;
            }

            /* if they updated the search list, go back for another try */
            if (id == IDOK)
                goto search_lib_path;
        }
    }

    /* 
     *   scan the parent's contents list and make sure this same file
     *   isn't already in the list 
     */
    for (cur = TreeView_GetChild(tree_, cont) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur))
    {
        proj_item_info *cur_info;
        
        /* get this item's information */
        cur_info = get_item_info(cur);

        /* if the filenames match, ignore this addition */
        if (cur_info != 0 && cur_info->fname_ != 0
            && stricmp(rel_fname, cur_info->fname_) == 0)
            return 0;
    }

    /* 
     *   assume the new item is of the default type for children of our
     *   container, and that the new item will not allow any children of its
     *   own 
     */
    new_item_type = cont_info->child_type_;
    new_child_open = PROJ_ITEM_OPEN_NONE;
    new_child_type = PROJ_ITEM_TYPE_NONE;
    new_child_path[0] = '\0';
    new_child_icon = 0;
    can_delete = TRUE;

    /*
     *   If the type of the new item was specified by the caller, use that
     *   type.  Otherwise, we might need to infer the type from the filename
     *   (more specifically, from the filename suffix).  
     */
    if (known_type != PROJ_ITEM_TYPE_NONE)
    {
        /* 
         *   the caller knows exactly what type it wants to use for this
         *   file - use the item type that the caller specified, rather than
         *   the default child type for the container 
         */
        new_item_type = known_type;
    }
    else
    {
        /*
         *   The caller did not specify the item type, so we must infer it.
         *   In most cases, the item type is fixed by the container, so the
         *   default child type we assumed above is probably the right type.
         *   However, in some cases, we'll use something other than the
         *   default child type.
         *   
         *   If we're adding the file to the source file list, the new item
         *   could be either a source file or a library.  If the caller
         *   didn't specify which it is, infer the type from the filename:
         *   if the new item's filename ends in ".tl", it's a source
         *   library, not an ordinary source file.  
         */
        if (new_item_type == PROJ_ITEM_TYPE_SOURCE
            && (len = strlen(rel_fname)) > 3
            && stricmp(rel_fname + len - 3, ".tl") == 0)
        {
            /* the new item is a library - note its type */
            new_item_type = PROJ_ITEM_TYPE_LIB;
        }
    }

    /* if we're adding a library, use appropriate settings for the item */
    if (new_item_type == PROJ_ITEM_TYPE_LIB)
    {
        /* children of a library are, by default, source files */
        new_child_open = PROJ_ITEM_OPEN_SRC;
        new_child_type = PROJ_ITEM_TYPE_SOURCE;
        new_child_icon = img_idx_source_;

        /* 
         *   our children's filenames are all relative to our path - extract
         *   the path portion from our relative filename (we only want the
         *   relative part of the path, because we build paths by combining
         *   all of the paths from each of our parents hierarchically) 
         */
        os_get_path_name(new_child_path, sizeof(new_child_path), rel_fname);
    }

    /* 
     *   if this is a library member (i.e., the parent item is a library),
     *   use appropriate settings for the new item 
     */
    if (cont_info->type_ == PROJ_ITEM_TYPE_LIB)
    {
        /* we can't delete library members */
        can_delete = FALSE;
    }

    /* set up the new item's descriptor */
    info = new proj_item_info(rel_fname, new_item_type, 0, new_child_path,
                              new_child_icon, can_delete, FALSE, TRUE,
                              new_child_open, new_child_type, is_sys_file);

    /* if it's a library include file, remember the library path */
    if (is_lib_inc)
        info->set_lib_path(parent_path);
    else if (is_from_user_lib)
        info->set_user_path(parent_path);

    /* 
     *   figure out which icon to use, depending on whether this is a file
     *   or a directory 
     */
    icon_idx = (is_dir
                ? img_idx_res_folder_
                : (new_item_type == PROJ_ITEM_TYPE_LIB
                   ? img_idx_lib_
                   : cont_info->child_icon_));

    /* add it to the container's list */
    new_item = insert_tree(cont, after, rel_fname, 0, icon_idx, info);

    /* 
     *   if it's inside a library, give it a checkmark icon, checked by
     *   default; if we're loading this library from configuration
     *   information, the configuration loader will adjust the checkmarks
     *   for us later 
     */
    if (new_item != 0 && cont_info->type_ == PROJ_ITEM_TYPE_LIB)
    {
        /* give the item a checkbox, checked by default */
        set_item_checkmark(new_item, TRUE);
    }

    /*
     *   If we successfully added the item, and it's a source library, read
     *   the library's contents and add each file mentioned in the library
     *   to the list.  
     */
    if (new_item != 0 && new_item_type == PROJ_ITEM_TYPE_LIB)
    {
        /* parse the library */
        CTcLibParserProj::add_lib_to_proj(fname, this, tree_,
                                          new_item, info);

        /* 
         *   explicitly collapse the library list initially - many users
         *   want to treat libraries as opaque, so they don't want to see
         *   all of the source files involved 
         */
        TreeView_Expand(tree_, new_item, TVE_COLLAPSE);

        /* 
         *   store the library file's modification timestamp, so that we can
         *   detect if the library is ever changed on disk while we have it
         *   loaded 
         */
        get_item_timestamp(new_item, &info->file_time_);
    }

    /* 
     *   If we successfully added the item, and the parent is the "includes"
     *   group, add the path to this file to the include path in the
     *   configuration.
     *   
     *   If this item is included from a library directory, don't include it
     *   in the explicit include path, since the compiler automatically adds
     *   library directories to the include path.  
     */
    if (new_item != 0 && cont == inc_item_ && !is_lib_inc)
    {
        char full_path[OSFNMAX];
        char abs_path[OSFNMAX];
        char sys_path[OSFNMAX];
        char rel_path[OSFNMAX];
        int cnt;
        int i;
        int found;
        
        /* get this item's fully-qualified directory path */
        os_get_path_name(full_path, sizeof(full_path), fname);

        /* 
         *   The include path entries are normally stored internally in
         *   absolute path format, so find the absolute path to this file.
         *   If it's already in absolute format, just use it as-is;
         *   otherwise, prepend the parent path. 
         */
        if (os_is_file_absolute(fname))
            strcpy(abs_path, full_path);
        else
            os_build_full_path(abs_path, sizeof(abs_path),
                               parent_path, full_path);

        /* get the item's relative directory path */
        os_get_path_name(rel_path, sizeof(rel_path), rel_fname);

        /* presume we won't find the path */
        found = FALSE;

        /* 
         *   if the relative path is empty, this file is in the same
         *   directory as the main source files; the compiler looks here to
         *   begin with, so we don't need to add it to the include path 
         */
        if (strlen(rel_path) == 0)
            found = TRUE;

        /* 
         *   if the path is the system include path, we don't need to add it
         *   to the explicit include path because the compiler automatically
         *   looks there 
         */
        os_get_special_path(sys_path, sizeof(sys_path),
                            _pgmptr, OS_GSP_T3_INC);
        if (stricmp(sys_path, rel_path) == 0)
            found = TRUE;

        /* 
         *   search the #include list in the configuration to see if this
         *   path appears, either in absolute or relative form 
         */
        cnt = config->get_strlist_cnt("build", "includes");
        for (i = 0 ; i < cnt ; ++i)
        {
            /* get this item */
            config->getval("build", "includes", i, buf, sizeof(buf));

            /* 
             *   if it matches the relative, full, or absolute path, this
             *   file's path is already listed, so we don't need to add it
             *   again 
             */
            if (stricmp(full_path, buf) == 0
                || stricmp(abs_path, buf) == 0
                || stricmp(rel_path, buf) == 0)
            {
                /* note that we found it, and stop searching */
                found = TRUE;
                break;
            }
        }

        /* 
         *   If we didn't find it, add it - use the relative form of the
         *   path to make the configuration more easily movable to another
         *   location in the file system (or to another computer).
         */
        if (!found)
            config->setval("build", "includes", cnt, rel_path);
    }

    /* return the new item */
    return new_item;
}

/*
 *   Scan the source tree for a library whose location matches the given
 *   header file's path prefix.
 *   
 *   If we find a library in the project source file list whose .tl file is
 *   in the same directory as the given .h file, we'll return true, and we'll
 *   fill in 'parent_path' with the path of the library, and 'rel_fname' with
 *   the given header file's name relative to the parent path.
 *   
 *   If we don't find a matching library, we'll return false, and we won't
 *   change parent_path or rel_fname.  
 */
int CHtmlDbgProjWin::scan_for_lib_inc_path(const char *fname,
                                           char *parent_path,
                                           size_t parent_path_size,
                                           char *rel_fname,
                                           size_t rel_fname_size)
{
    char path[OSFNMAX];

    /* get the full path to the file */
    os_get_path_name(path, sizeof(path), fname);

    /* scan the tree recursively, starting at the source item */
    return scan_for_lib_inc_path_in(src_item_, path, fname,
                                    parent_path, parent_path_size,
                                    rel_fname, rel_fname_size);
}

/*
 *   Service routine for scan_for_lib_inc_path: search the source list
 *   recursively, starting at the given parent item.  We'll search all
 *   children of the given item.  
 */
int CHtmlDbgProjWin::scan_for_lib_inc_path_in(
    HTREEITEM parent_tree_item, const char *path, const char *fname,
    char *parent_path, size_t parent_path_size,
    char *rel_fname, size_t rel_fname_size)
{
    HTREEITEM cur;
    
    /* scan children of the given item in the tree */
    for (cur = TreeView_GetChild(tree_, parent_tree_item) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur))
    {
        proj_item_info *cur_info;
        
        /* get this item's information */
        cur_info = get_item_info(cur);
        
        /* 
         *   if it's a library, check to see if the new file is in the same
         *   directory as the library 
         */
        if (cur_info != 0 && cur_info->type_ == PROJ_ITEM_TYPE_LIB)
        {
            char cur_fname[OSFNMAX];
            char cur_path[OSFNMAX];
            
            /* get the item's path */
            get_item_path(cur_fname, sizeof(cur_fname), cur);
            os_get_path_name(cur_path, sizeof(cur_path), cur_fname);
            
            /* check to see if it's in this directory */
            if (stricmp(path, cur_path) == 0)
            {
                /* it's a match - the parent path is the library path */
                strcpy(parent_path, path);

                /* re-figure the name relative to the new parent path */
                oss_get_rel_path(rel_fname, rel_fname_size,
                                 fname, parent_path);
                
                /* no need to look further - indicate we found a match */
                return TRUE;
            }

            /* 
             *   Libraries can contain other libraries, so scan children of
             *   this item.  If we find a match among the children, indicate
             *   success.  
             */
            if (scan_for_lib_inc_path_in(cur, path, fname,
                                         parent_path, parent_path_size,
                                         rel_fname, rel_fname_size))
                return TRUE;
        }
    }

    /* we didn't find a match */
    return FALSE;
}

/*
 *   Get the timestamp for an item in the tree.  
 */
void CHtmlDbgProjWin::get_item_timestamp(HTREEITEM item, FILETIME *ft)
{
    char path[OSFNMAX];
    WIN32_FIND_DATA fdata;
    HANDLE hfind;

    /* get the full path to the file */
    get_item_path(path, sizeof(path), item);

    /* begin a search for the file to get its directory data */
    hfind = FindFirstFile(path, &fdata);

    /* if we found the file, not its timestamp */
    if (hfind != INVALID_HANDLE_VALUE)
    {
        /* copy the timestamp information out of the find data */
        memcpy(ft, &fdata.ftLastWriteTime, sizeof(*ft));

        /* we're done with the search, so close the search handle */
        FindClose(hfind);
    }
    else
    {
        /* no timestamp information is available */
        memset(ft, 0, sizeof(*ft));
    }
}

/*
 *   Get the directory that will contain children of an item 
 */
void CHtmlDbgProjWin::get_item_child_dir(char *buf, size_t buflen,
                                         HTREEITEM item)
{
    proj_item_info *info;
    proj_item_info *parent_info;
    HTREEITEM parent;
    char *p;
    size_t rem;

    /* get the information on this object */
    info = get_item_info(item);

    /* 
     *   If this item is marked as a system file, then the parent is not
     *   relevant in determining our path, because our path is relative to
     *   the system directory rather than to a parent of ours.
     *   
     *   Likewise, if this item was found on the user library search list,
     *   then the child location depends only on our own location.
     *   
     *   Otherwise, if this isn't a top-level object, our path is relative
     *   to our parent's child path.  If we're a root object, though, our
     *   child path is absolute, since there's nothing to be relative to.  
     */
    parent = TreeView_GetParent(tree_, item);
    parent_info = (parent != 0 ? get_item_info(parent) : 0);
    if (info->is_sys_file_
        && parent_info != 0
        && parent_info->child_sys_path_ != 0)
    {
        /* we're a system file, so we're relative to the system path */
        strncpy(buf, parent_info->child_sys_path_, buflen);
        buf[buflen-1] = '\0';
    }
    else if (info->in_user_lib_)
    {
        /* 
         *   we're from the search list, so we're relative to the location
         *   where we found a match in the search list 
         */
        strncpy(buf, info->lib_path_, buflen);
        buf[buflen-1] = '\0';
    }
    else if (parent != 0)
    {
        /* we're relative to our parent, so get the parent path */
        get_item_child_dir(buf, buflen, parent);
    }
    else
    {
        /* there's no parent - we're the root */
        buf[0] = '\0';
    }

    /* if we don't have our own child path, there's nothing to add */
    if (info->child_path_ == 0 || info->child_path_[0] == '\0')
        return;

    /* set up to add our path portion after the parent prefix */
    p = buf + strlen(buf);
    rem = buflen - (p - buf);

    /* add a path separator if necessary */
    if (p != buf && rem > 1
        && *(p - 1) != '\\' && *(p - 1) != '/' && *(p - 1) != ':')
    {
        *p++ = '\\';
        --rem;
    }

    /* add our path portion if there's room */
    strncpy(p, info->child_path_, rem);

    /* make sure the result is null-terminated */
    if (rem > 0)
        *(p + rem - 1) = '\0';
}

/*
 *   Get the path to an item 
 */
void CHtmlDbgProjWin::get_item_path(char *buf, size_t buflen, HTREEITEM item)
{
    proj_item_info *info;
    proj_item_info *parent_info;
    HTREEITEM parent;

    /* get the information on this object */
    info = get_item_info(item);

    /* 
     *   If it's a system file, it's in the system directory.  Otherwise, if
     *   it's not a top-level item, the path is relative to the parent
     *   object's common child path.  If it's a top-level item, its path is
     *   absolute and doesn't depend on any parent paths.  
     */
    parent = TreeView_GetParent(tree_, item);
    parent_info = (parent != 0 ? get_item_info(parent) : 0);
    if (info->is_sys_file_
        && parent_info != 0
        && parent_info->child_sys_path_ != 0)
    {
        /* build the full path by adding our name to the system path */
        os_build_full_path(buf, buflen,
                           parent_info->child_sys_path_, info->fname_);
    }
    else if (info->lib_path_ != 0)
    {
        /* 
         *   this item is explicitly in a library directory - use the
         *   library path stored with the item 
         */
        os_build_full_path(buf, buflen, info->lib_path_, info->fname_);
    }
    else if (parent != 0)
    {
        char parent_path[OSFNMAX];

        /* get the parent path */
        get_item_child_dir(parent_path, sizeof(parent_path), parent);
        
        /* build the full path by adding our name to the parent path */
        os_build_full_path(buf, buflen, parent_path, info->fname_);
    }
    else
    {
        /* we're a root-level object - our name is absolute */
        strncpy(buf, info->fname_, buflen);

        /* ensure the result is null-terminated */
        if (buflen != 0)
            buf[buflen - 1] = '\0';
    }
}

/*
 *   Get the library URL-style name for an item.  For an item in a top-level
 *   library, this is simply the original name of the item as it appeared in
 *   the library file itself.  For an item in a sub-library, this is a path
 *   to the item formed by prefixing the name of the item as it appeared in
 *   its immediately enclosing library with the URL-style name for the
 *   enclosing library and a slash (this rule is applied recursively for
 *   each enclosing library).
 *   
 *   For example, suppose we have a library structure that shows up in the
 *   treeview like so:
 *   
 *.  mylib
 *.    abc
 *.    def
 *.    libdir/sublib
 *.      ghi
 *.      jkl
 *.      subdir/subsublib
 *.        files/mno
 *.        files/pqr
 *   
 *   The URL for the first child of 'mylib' is simply 'abc'.  The URL for
 *   the last item in the list ('files/pqr') is
 *   'libdir/sublib/subdir/subsublib/files/pqr'.
 *   
 *   Note that the top-level library makes no contribution to any URL path,
 *   because each library member's path is relative to its top-level library
 *   in the first place.  
 */
void CHtmlDbgProjWin::get_item_lib_url(char *url, size_t url_len,
                                       HTREEITEM item)
{
    HTREEITEM parent;
    proj_item_info *info;
    proj_item_info *par_info;
    size_t len;

    /* if our parent is a library, get its path first */
    if ((parent = TreeView_GetParent(tree_, item)) != 0
        && (par_info = get_item_info(parent)) != 0
        && par_info->type_ == PROJ_ITEM_TYPE_LIB)
    {
        /* get the parent's URL as the prefix to our URL */
        get_item_lib_url(url, url_len, parent);

        /* 
         *   If there's room, add a '/' after the parent URL.  Do not add a
         *   '/' if there's nothing in the buffer, since we don't want a
         *   slash at the root level; the root is implicitly our top-level
         *   library, and by convention we don't add a slash before the path
         *   to files within it.  
         */
        len = strlen(url);
        if (len != 0 && len + 1 < url_len)
        {
            url[len++] = '/';
            url[len] = '\0';
        }
    }
    else
    {
        /* 
         *   our parent is not a library, so it has no contribution to our
         *   URL - we have no prefix 
         */
        url[0] = '\0';
        len = 0;
    }

    /* get our own information object */
    info = get_item_info(item);

    /* add our own URL-style name, if we have one and it fits */
    if (info != 0
        && info->url_ != 0
        && strlen(info->url_) + 1 < url_len - len)
    {
        /* copy it */
        strcpy(url + len, info->url_);
    }
}

/*
 *   Add an external resource file 
 */
void CHtmlDbgProjWin::add_ext_resfile()
{
    HTREEITEM cur;
    HTREEITEM after;
    HTREEITEM new_item;
    proj_item_info *info;
    proj_item_info *root_info;

    /* make sure the project window is visible */
    SendMessage(dbgmain_->get_handle(), WM_COMMAND, ID_VIEW_PROJECT, 0);

    /* 
     *   work our way up to the selected external resource file container,
     *   if we can find one 
     */
    for (cur = TreeView_GetSelection(tree_) ; cur != 0 ;
         cur = TreeView_GetParent(tree_, cur))
    {
        /* get this item's information */
        info = get_item_info(cur);

        /* if this is an external resource file object, we've found it */
        if (info->type_ == PROJ_ITEM_TYPE_EXTRES)
            break;
    }

    /*
     *   If we found a selected external resource file, insert the new one
     *   immediately after the selected one.  If we didn't find one,
     *   insert the new one as the first external resource.
     */
    if (cur != 0)
    {
        /* insert after the selected one */
        after = cur;
    }
    else
    {
        /* 
         *   Find the top-level object immediately preceding the first
         *   external resource (or the last top-level object, if there are
         *   no external resource files yet).  Start with the first item
         *   in the list, which is the root "project" item, and go through
         *   root-level items until we find the first external resource
         *   file.  
         */
        for (cur = after = TreeView_GetRoot(tree_) ; cur != 0 ;
             cur = TreeView_GetNextSibling(tree_, cur))
        {
            /* get this item's information */
            info = get_item_info(cur);

            /* 
             *   if it's not an external resource item, presume it will be
             *   the item we're looking for (we might find another one
             *   later, but remember this one for now) 
             */
            if (info->type_ != PROJ_ITEM_TYPE_EXTRES)
                after = cur;
        }

        /* if we didn't find any items, insert at the end of the list */
        if (after == 0)
            after = TVI_LAST;
    }

    /* get the root object's information */
    root_info = get_item_info(TreeView_GetRoot(tree_));

    /* 
     *   Create and insert the new item.  Use the same child path that the
     *   project file itself uses.  For the config group name, just use
     *   the template - we'll overwrite it with the actual group when we
     *   fix up the groups below, so all that matters is that we get the
     *   length right, which we can do by using the template string.  
     */
    info = new proj_item_info(0, PROJ_ITEM_TYPE_EXTRES,
                              EXTRES_CONFIG_TEMPLATE,
                              root_info->child_path_, img_idx_resource_,
                              TRUE, TRUE, FALSE, PROJ_ITEM_OPEN_EXTERN,
                              PROJ_ITEM_TYPE_RESOURCE, FALSE);

    /* add it */
    new_item = insert_tree(TVI_ROOT, after, "", TVIS_BOLD | TVIS_EXPANDED,
                           img_idx_extres_, info);

    /* assume this file will be in the installer */
    set_extres_in_install(new_item, TRUE);

    /* fix up the numbering of all of the external resource items */
    fix_up_extres_numbers();

    /* save the configuration */
    save_config(dbgmain_->get_local_config());
}

/*
 *   Visually mark a tree view item as being included in the install 
 */
void CHtmlDbgProjWin::set_extres_in_install(HTREEITEM item, int in_install)
{
    TV_ITEM info;

    /* set up the information structure */
    info.mask = TVIF_STATE | TVIF_HANDLE;
    info.hItem = item;
    info.stateMask = TVIS_STATEIMAGEMASK;
    info.state = INDEXTOSTATEIMAGEMASK(in_install
                                       ? img_idx_in_install_
                                       : img_idx_not_in_install_);

    /* set the new state icon */
    TreeView_SetItem(tree_, &info);
}

/*
 *   Visually mark a tree view item with a checkbox in the checked or
 *   unchecked state 
 */
void CHtmlDbgProjWin::set_item_checkmark(HTREEITEM item, int checked)
{
    TV_ITEM info;

    /* set up the information structure */
    info.mask = TVIF_STATE | TVIF_HANDLE;
    info.hItem = item;
    info.stateMask = TVIS_STATEIMAGEMASK;
    info.state = INDEXTOSTATEIMAGEMASK(checked
                                       ? img_idx_check_
                                       : img_idx_nocheck_);

    /* set the new state icon */
    TreeView_SetItem(tree_, &info);
}

/*
 *   Fix up the external resource item numbering 
 */
void CHtmlDbgProjWin::fix_up_extres_numbers()
{
    HTREEITEM cur;
    proj_item_info *info;
    int fileno;

    /* go through the root items and fix each external resource file */
    for (fileno = 0, cur = TreeView_GetRoot(tree_) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur))
    {
        /* get this item's information */
        info = get_item_info(cur);

        /* if it's an external resource, renumber it */
        if (info->type_ == PROJ_ITEM_TYPE_EXTRES)
        {
            char namebuf[OSFNMAX];
            char groupbuf[50];
            TV_ITEM item;

            /* build the new display name and config group name */
            gen_extres_names(namebuf, groupbuf, fileno);

            /* set the new name */
            item.mask = TVIF_HANDLE | TVIF_TEXT;
            item.hItem = cur;
            item.pszText = namebuf;
            TreeView_SetItem(tree_, &item);

            /* delete any config information under the old group name */
            dbgmain_->get_local_config()
                ->clear_strlist("build", info->config_group_);
            
            /* 
             *   set the new configuration group name (they're always the
             *   same length, so just overwrite the old config group name) 
             */
            strcpy(info->config_group_, groupbuf);

            /* consume this file number */
            ++fileno;
        }
    }
}

/*
 *   Generate the display name and configuration group name for an
 *   external resource file, given the file's index in the list 
 */
void CHtmlDbgProjWin::gen_extres_names(char *display_name,
                                       char *config_group_name,
                                       int fileno)
{
    char *p;

    /* build the display name if desired */
    if (display_name != 0)
        sprintf(display_name, "External Resource File (.3r%d)", fileno);

    /* generate the configuration group name if desired */
    if (config_group_name != 0)
    {
        /* start with the template */
        strcpy(config_group_name, EXTRES_CONFIG_TEMPLATE);

        /* find the '0000' suffix */
        for (p = config_group_name ; *p != '\0' && *p != '0' ; ++p) ;

        /* add the serial number */
        sprintf(p, "%04x", fileno);
    }

}

/*
 *   Open an item 
 */
void CHtmlDbgProjWin::open_item(HTREEITEM item)
{
    proj_item_info *info;
    HTREEITEM parent;
    proj_item_info *par_info;
    char fname[OSFNMAX];

    /* get the information on this item */
    if (item == 0
        || ((info = get_item_info(item)) == 0 || !info->openable_))
        return;

    /* find the parent to determine how to open the file */
    parent = TreeView_GetParent(tree_, item);
    if (parent == 0 || (par_info = get_item_info(parent)) == 0)
        return;

    /* take the appropriate action */
    switch(par_info->child_open_action_)
    {
    case PROJ_ITEM_OPEN_NONE:
        /* can't open this item - ignore the request */
        break;

    case PROJ_ITEM_OPEN_SRC:
        /* get this item's full name */
        get_item_path(fname, sizeof(fname), item);

        /* if the file doesn't exist, show an error */
        if (osfacc(fname))
        {
            MessageBox(dbgmain_->get_handle(),
                       "This file does not exist or is not accessible.  "
                       "Check the file's name and directory path, and "
                       "ensure that the file isn't being used by another "
                       "application.", "TADS Workbench",
                       MB_OK | MB_ICONEXCLAMATION);
            break;
        }
        
        /* open it as a source file */
        dbgmain_->open_text_file(fname);
        break;

    case PROJ_ITEM_OPEN_EXTERN:
        /* get this item's full name */
        get_item_path(fname, sizeof(fname), item);

        /* if it's a resource file, check for and drop any alias */
        if (info->type_ == PROJ_ITEM_TYPE_RESOURCE)
        {
            char *p;

            /* scan for an '=' character, which introduces an alias */
            for (p = fname ; *p != '\0' && *p != '=' ; ++p) ;

            /* terminate the filename at the '=', if we found one */
            if (*p == '=')
                *p = '\0';
        }

        /* ask the OS to open the file */
        if ((unsigned long)ShellExecute(0, "open", fname,
                                        0, 0, SW_SHOW) <= 32)
        {
            /* that failed - show an error */
            MessageBox(dbgmain_->get_handle(),
                       "Unable to open file.  In order to open this "
                       "file, you must install an application that "
                       "can view or edit this file type.",
                       "TADS Workbench", MB_OK | MB_ICONEXCLAMATION);
        }

        /* done */
        break;
    }
}

/*
 *   Open an item in the text editor
 */
void CHtmlDbgProjWin::open_item_in_text_editor(HTREEITEM item)
{
    proj_item_info *info;
    HTREEITEM parent;
    proj_item_info *par_info;
    char fname[OSFNMAX];

    /* get the information on this item */
    if (item == 0
        || ((info = get_item_info(item)) == 0 || !info->openable_))
        return;

    /* find the parent to determine how to open the file */
    parent = TreeView_GetParent(tree_, item);
    if (parent == 0 || (par_info = get_item_info(parent)) == 0)
        return;

    /* take the appropriate action */
    switch(par_info->child_open_action_)
    {
    case PROJ_ITEM_OPEN_NONE:
    case PROJ_ITEM_OPEN_EXTERN:
        /* can't open this item in the text editor - ignore the request */
        break;

    case PROJ_ITEM_OPEN_SRC:
        /* get this item's full name */
        get_item_path(fname, sizeof(fname), item);

        /* if the file doesn't exist, show an error */
        if (osfacc(fname))
        {
            MessageBox(dbgmain_->get_handle(),
                       "This file does not exist or is not accessible.  "
                       "Check the file's name and directory path, and "
                       "ensure that the file isn't being used by another "
                       "application.", "TADS Workbench",
                       MB_OK | MB_ICONEXCLAMATION);
            break;
        }

        /* it's a source file - open it in the text editor */
        dbgmain_->open_in_text_editor(fname, 1, 1, FALSE);
        break;
    }
}

/*
 *   Process a user-defined message 
 */
int CHtmlDbgProjWin::do_user_message(int msg, WPARAM wpar, LPARAM lpar)
{
    /* see which user-defined message we have */
    switch (msg)
    {
    case W32PRJ_MSG_OPEN_TREE_SEL:
        /* open the item selected in the project tree */
        open_item(TreeView_GetSelection(tree_));
        break;

    default:
        /* not one of ours */
        break;
    }

    /* if we didn't handle the message, inherit the default implementation */
    return CTadsWin::do_user_message(msg, wpar, lpar);
}

/* 
 *   Process a notification message 
 */
int CHtmlDbgProjWin::do_notify(int control_id, int notify_code, NMHDR *nm)
{
    TV_KEYDOWN *tvk;
    NM_TREEVIEW *nmt;
    HTREEITEM target;
    TV_HITTESTINFO ht;
    
    /* if it's the tree control, check the message */
    if (control_id == TREEVIEW_ID)
    {
        switch(nm->code)
        {
        case NM_RETURN:
            /* 
             *   This has been fully handled already in the TVN_KEYDOWN
             *   handler; just tell the caller we've handled it.
             *   
             *   (If we don't handle this notification explicitly, the tree
             *   view control will use the default window procedure handling
             *   on the event, which will beep.  The beep is annoying.
             *   Explicitly handling the notification stops the beep.)  
             */
            return TRUE;
            
        case TVN_KEYDOWN:
            /* cast the notify structure to a keydown-specific notifier */
            tvk = (TV_KEYDOWN *)nm;

            /* see what key we have */
            switch(tvk->wVKey)
            {
            case VK_DELETE:
            case VK_BACK:
                /* delete/backspace - delete the selected item */
                delete_tree_selection();

                /* handled - don't pass through to the incremental search */
                return TRUE;

            case VK_RETURN:
                /* enter/return - open the selected item */
                open_item(TreeView_GetSelection(tree_));

                /* handled - don't pass through to the incremental search */
                return TRUE;
            }

            /* if we didn't return already, we didn't handle it */
            break;

        case TVN_DELETEITEM:
            /* cast the notify structure to a tre-view notifier */
            nmt = (NM_TREEVIEW *)nm;

            /* 
             *   if we have an extra information structure for this item,
             *   delete it 
             */
            if (get_item_info(nmt->itemOld.hItem) != 0)
                delete get_item_info(nmt->itemOld.hItem);

            /* handled */
            return TRUE;

        case TVN_BEGINDRAG:
            {
                proj_item_info *info;

                /* cast the notify structure to a tree-view notifier */
                nmt = (NM_TREEVIEW *)nm;

                /* 
                 *   don't allow dragging files around within a library,
                 *   since we treat libraries as read-only 
                 */
                info = get_item_info(
                    TreeView_GetParent(tree_, nmt->itemNew.hItem));
                if (info == 0 || info->type_ == PROJ_ITEM_TYPE_LIB)
                    return TRUE;

                /* 
                 *   get the item being dragged - if we can't find an item,
                 *   we can't do the drag 
                 */
                info = get_item_info(nmt->itemNew.hItem);
                if (info == 0)
                    return TRUE;

                /* we can only drag around certain types of files */
                switch(info->type_)
                {
                case PROJ_ITEM_TYPE_SOURCE:
                case PROJ_ITEM_TYPE_LIB:
                case PROJ_ITEM_TYPE_HEADER:
                case PROJ_ITEM_TYPE_RESOURCE:
                    /* we can drag these types, so proceed */
                    break;

                default:
                    /* 
                     *   we can't drag any other types - stop now, without
                     *   initiating any drag tracking 
                     */
                    return TRUE;
                }

                /* capture input in the tree's parent window */
                SetCapture(handle_);

                /* remember the item being dragged */
                drag_item_ = nmt->itemNew.hItem;
                drag_info_ = info;

                /* select the item while it's being dragged */
                TreeView_SelectDropTarget(tree_, drag_item_);

                /* note that we're tracking a drag operation */
                tracking_drag_ = TRUE;
            }
            
            /* handled */
            return TRUE;

        case NM_DBLCLK:
            /* 
             *   Double-click - open the target item.  Due to some quirk in
             *   the way the tree control processes double clicks, focus
             *   will be set to the tree after this message processor
             *   returns, so focus won't be in our newly-opened window if we
             *   open it here.  So, handle this event by posting ourselves
             *   our user message "open selected item"; that will open the
             *   item that should have been selected by tree control's own
             *   processing of the double-click, and by the time we get
             *   around to processing the enter-key notification, the tree
             *   should be done with its aggressive focus setting.  
             */
            PostMessage(handle_, W32PRJ_MSG_OPEN_TREE_SEL, 0, 0);

            /* handled */
            return TRUE;

        case NM_CLICK:
            /* get the mouse location in tree-relative coordinates */
            GetCursorPos(&ht.pt);
            ScreenToClient(tree_, &ht.pt);

            /* get the item they clicked on */
            target = TreeView_HitTest(tree_, &ht);

            /* check for a click on a state icon */
            if ((ht.flags & TVHT_ONITEMSTATEICON) != 0)
            {
                proj_item_info *info;
                
                /* 
                 *   get this item's extra information; give up if we can't
                 *   find the information object 
                 */
                info = get_item_info(target);
                if (info == 0)
                    return TRUE;

                /* 
                 *   if it's an external resource file, reverse the
                 *   installer state 
                 */
                if (info->type_ == PROJ_ITEM_TYPE_EXTRES)
                {
                    /* reverse the state */
                    info->in_install_ = !info->in_install_;

                    /* set the new state */
                    set_extres_in_install(target, info->in_install_);

                    /* save the configuration changes */
                    save_config(dbgmain_->get_local_config());

                    /* handled */
                    return TRUE;
                }

                /* 
                 *   if it's a source file or a nested library, change its
                 *   exclusion status 
                 */
                if (info->type_ == PROJ_ITEM_TYPE_SOURCE
                    || info->type_ == PROJ_ITEM_TYPE_LIB)
                {
                    /* invert the library exclusion status */
                    info->exclude_from_lib_ = !info->exclude_from_lib_;

                    /* 
                     *   invert the item's checkmarks (note that the
                     *   checkmark shows *inclusion*, so its value is the
                     *   opposite of our internal 'exclude' value) 
                     */
                    set_item_checkmark(target, !info->exclude_from_lib_);

                    /* if it's a library, exclude/include all children */
                    if (info->type_ == PROJ_ITEM_TYPE_LIB)
                        set_exclude_children(target, info->exclude_from_lib_);

                    /* 
                     *   if we're including the item, make sure all library
                     *   parents are included 
                     */
                    if (!info->exclude_from_lib_)
                        set_include_parents(target);

                    /* update the saved configuration */
                    save_config(dbgmain_->get_local_config());
                }
            }

            /* not handled */
            break;

        case NM_RCLICK:
            /* get the mouse location */
            GetCursorPos(&ht.pt);
            ScreenToClient(handle_, &ht.pt);

            /* determine what they clicked on, and select that item */
            target = TreeView_HitTest(tree_, &ht);
            TreeView_Select(tree_, target, TVGN_CARET);
            
            /* run the context menu */
            track_context_menu(ctx_menu_, ht.pt.x, ht.pt.y);

            /* handled */
            return TRUE;

        default:
            /* no special handling required */
            break;
        }
    }
    
    /* inherit default processing */
    return CTadsWin::do_notify(control_id, notify_code, nm);
}

/*
 *   Set the 'exclude' status for all parents of the given item to *include*
 *   the parents.  This should be used whenever we include an item, to make
 *   sure all of its parents are included as well.  
 */
void CHtmlDbgProjWin::set_include_parents(HTREEITEM item)
{
    HTREEITEM cur;
    HTREEITEM par;
    
    /* visit each parent */
    for (cur = TreeView_GetParent(tree_, item) ; cur != 0 ; cur = par)
    {
        proj_item_info *par_info;
        proj_item_info *cur_info;

        /* get this item's parent */
        par = TreeView_GetParent(tree_, cur);

        /* if there's no parent, we're done */
        if (par == 0)
            break;

        /* get our info and the parent item's info */
        cur_info = get_item_info(cur);
        par_info = get_item_info(par);

        /* 
         *   if the parent isn't a library, then we're not a library inside a
         *   library, so we have no 'exclude' status at all; we're done in
         *   this case 
         */
        if (par_info->type_ != PROJ_ITEM_TYPE_LIB)
            break;

        /* mark this item as included */
        cur_info->exclude_from_lib_ = FALSE;

        /* set the checkmark to 'checked' to show we're included */
        set_item_checkmark(cur, TRUE);
    }
}

/*
 *   Set the 'exclude' status for all children of the given item, recursively
 *   setting the same value for their children.  
 */
void CHtmlDbgProjWin::set_exclude_children(HTREEITEM item, int exclude)
{
    HTREEITEM cur;

    /* visit the item's children */
    for (cur = TreeView_GetChild(tree_, item) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur))
    {
        proj_item_info *info;

        /* get this item's information */
        info = get_item_info(cur);

        /* if it's a source or library file, mark it as excluded */
        if (info->type_ == PROJ_ITEM_TYPE_SOURCE
            || info->type_ == PROJ_ITEM_TYPE_LIB)
        {
            /* set its 'exclude' status */
            info->exclude_from_lib_ = exclude;

            /* 
             *   set its checkmark (the checkmark shows the 'include' status,
             *   which is the inverse of our 'exclude' status) 
             */
            set_item_checkmark(cur, !exclude);

            /* visit its children if needed */
            if (info->type_ == PROJ_ITEM_TYPE_LIB)
                set_exclude_children(cur, exclude);
        }
    }
}

/*
 *   process a mouse move 
 */
int CHtmlDbgProjWin::do_mousemove(int keys, int x, int y)
{
    /* if we're tracking a drag event, process it */
    if (tracking_drag_)
    {
        HTREEITEM target;
        
        /* select the item being dragged */
        TreeView_SelectDropTarget(tree_, drag_item_);

        /* determine which item we're pointing at */
        get_dragmove_target(x, y, &target);

        /* draw the drag-move caret */
        draw_dragmove_caret(target);

        /* show the appropriate cursor */
        SetCursor(target != 0 && is_dragmove_target_useful(target)
                  ? move_csr_ : no_csr_);
    }

    /* inherit default handling */
    return CTadsWin::do_mousemove(keys, x, y);
}

/*
 *   Draw a drag-move insertion caret.  
 */
void CHtmlDbgProjWin::draw_dragmove_caret(HTREEITEM target)
{
    RECT rc;

    /* if this is the same caret as last time, there's nothing to do */
    if (target == last_drop_target_)
        return;

    /* remove the old caret by invalidating its area on the tree control */
    if (last_drop_target_ != 0)
    {
        HTREEITEM vis_target;

        /* get the visual insertion point */
        vis_target = get_dragmove_visual_target(last_drop_target_);

        /* get the preceding item's location */
        TreeView_GetItemRect(tree_, vis_target, &rc, FALSE);

        /* adjust to the caret rectangle */
        SetRect(&rc, rc.left, rc.bottom - 2, rc.right, rc.bottom + 2);

        /* invalidate this area */
        InvalidateRect(tree_, &rc, TRUE);
    }

    /* remember the new target item */
    last_drop_target_ = target;

    /* add the new caret */
    if (target != 0)
    {
        HDC dc;
        RECT lrc;
        HTREEITEM vis_target;

        /* get the visual insertion point */
        vis_target = get_dragmove_visual_target(target);

        /* 
         *   make sure the window is fully updated before we do any
         *   overwrite drawing 
         */
        UpdateWindow(tree_);

        /* get the preceding item's location */
        TreeView_GetItemRect(tree_, vis_target, &rc, FALSE);

        /* adjust to the caret rectangle */
        SetRect(&rc, rc.left, rc.bottom - 2, rc.right, rc.bottom + 2);

        /* get the device context for the tree control */
        dc = GetDC(tree_);

        /* draw a 2-pixel line across the width of the box */
        SetRect(&lrc, rc.left, rc.top + 1, rc.right, rc.top + 3);
        FillRect(dc, &lrc, (HBRUSH)GetStockObject(BLACK_BRUSH));

        /* done with the device context */
        ReleaseDC(tree_, dc);
    }
}

/*
 *   Get the visual drag-move target.  This is the item visually preceding
 *   the drop point for a particular target, which is the item visually
 *   preceding the next sibling of the drop target.  
 */
HTREEITEM CHtmlDbgProjWin::get_dragmove_visual_target(HTREEITEM target) const
{
    HTREEITEM cur;
    HTREEITEM nxt;

    /* 
     *   if the target is the parent of the item being dragged, it's a
     *   special case: inserting after the parent is our way of indicating
     *   that we're inserting at the start of the sibling list 
     */
    if (target == TreeView_GetParent(tree_, drag_item_))
        return target;

    /* keep going until we stop finding children */
    for (;;)
    {
        /*
         *   Get the next visible item after the target.  If this item is a
         *   child of the actual target, we must find the last sibling of
         *   this item; if this item is not a child of the target, then the
         *   target isn't visually open, so it's the visual target.  
         */
        cur = TreeView_GetNextVisible(tree_, target);
        if (cur == 0
            || TreeView_GetParent(tree_, cur) != target)
        {
            /* 
             *   there's no visible child of this item - the actual target
             *   is the same as the visual target 
             */
            return target;
        }

        /* find our last child (the last sibling of our first child) */
        for (nxt = TreeView_GetNextSibling(tree_, cur) ; nxt != 0 ;
             cur = nxt, nxt = TreeView_GetNextSibling(tree_, cur)) ;

        /* 
         *   keep going, using our last child as the new target - if our
         *   last child has visible children of its own, we must skip them
         *   as well, and so on until we find the last child of the last
         *   child of etc.  
         */
        target = cur;
    }
}

/*
 *   Get the drag-move target.  This is the item immediately preceding the
 *   insertion point.   
 */
int CHtmlDbgProjWin::get_dragmove_target(int x, int y, HTREEITEM *item) const
{
    TVHITTESTINFO hit;

    /* determine which item we're pointing at */
    hit.pt.x = x;
    hit.pt.y = y;
    *item = TreeView_HitTest(tree_, &hit);

    /* 
     *   if we're above the vertical center of the item, insert before the
     *   item; otherwise, insert after the item 
     */
    if (*item != 0)
    {
        RECT rc;

        /* get the item's rectangle */
        TreeView_GetItemRect(tree_, *item, &rc, FALSE);

        /* 
         *   if the mouse y coordinate is less than the y coordinate of the
         *   vertical center of the item, we want to insert before this
         *   item; since we always state the result as the item preceding
         *   the insertion point, we must change our result in this case to
         *   indicate the visually previous item 
         */
        if (y < rc.top + (rc.bottom - rc.top)/2)
        {
            HTREEITEM prv;

            /* get the previous item */
            prv = TreeView_GetPrevSibling(tree_, *item);

            /* 
             *   if there is no previous item, the parent is the previous
             *   item visually, so use it as the insertion point 
             */
            if (prv == 0)
                prv = TreeView_GetParent(tree_, *item);

            /* use this item */
            *item = prv;
        }
    }

    /* 
     *   We can only drag an item to the same group - if we're pointing to a
     *   different group, don't allow dropping here.  Allow dropping on any
     *   item with the same parent, or on the parent of the item.  
     */
    if (*item != 0
        && *item != TreeView_GetParent(tree_, drag_item_)
        && (TreeView_GetParent(tree_, drag_item_) !=
            TreeView_GetParent(tree_, *item)))
    {
        /*
         *   If the apparent target is a child of a sibling of the item
         *   being dragged, then the real target is the sibling parent.
         *   Look up the parent tree for an item that is a sibling of the
         *   drag item.  
         */
        for (;;)
        {
            /* get the item's parent */
            *item = TreeView_GetParent(tree_, *item);

            /* if we're out of parents, there is no item */
            if (*item == 0)
                break;

            /* 
             *   if this parent is a sibling of the item being dragged, it's
             *   the actual target 
             */
            if (TreeView_GetParent(tree_, drag_item_)
                == TreeView_GetParent(tree_, *item))
                break;
        }
    }

    /* return true if we found a target */
    return (*item != 0);
}

/*
 *   Determine if the given drag-move target will actually result in any
 *   change to the item's position.  Inserting an item immediately before or
 *   after its current location obviously has no effect.  
 */
int CHtmlDbgProjWin::is_dragmove_target_useful(HTREEITEM target) const
{
    /*
     *   If we're attempting to insert the item where it started, don't
     *   allow the operation, since it would do nothing.  The item will end
     *   where it started if the insert-after item is the item immediately
     *   preceding the item, or it's the item itself.
     *   
     *   Note that if the item being dragged has no previous sibling, then
     *   the effective drop target to insert just before the item is the
     *   item's parent.  
     */
    if (target != 0
        && (target == drag_item_
            || target == TreeView_GetPrevSibling(tree_, drag_item_)
            || (TreeView_GetPrevSibling(tree_, drag_item_) == 0
                && target == TreeView_GetParent(tree_, drag_item_))))
    {
        /* it's not a meaningful target */
        return FALSE;
    }

    /* it's a valid target */
    return TRUE;
}

/*
 *   mouse up event 
 */
int CHtmlDbgProjWin::do_leftbtn_up(int keys, int x, int y)
{
    /* if we're tracking a drag event, process it */
    if (tracking_drag_)
    {
        HTREEITEM target;

        /* remove the insertion caret */
        draw_dragmove_caret(0);

        /* remove the drag item selection */
        TreeView_SelectDropTarget(tree_, 0);

        /* get the target of the drop */
        if (get_dragmove_target(x, y, &target)
            && is_dragmove_target_useful(target))
        {
            HTREEITEM parent;
            HTREEITEM ins_after;
            HTREEITEM new_item;

            /* make sure nothing is selected before the drop */
            TreeView_SelectItem(tree_, 0);

            /* 
             *   We have a valid target, so move the dragged item to its new
             *   location after the target.  Note that if the target we've
             *   calculated is the dragged item's parent, then we're moving
             *   the item to the start of its parent's child list.  
             */
            parent = TreeView_GetParent(tree_, drag_item_);
            ins_after = (target == parent ? TVI_FIRST : target);
            new_item = move_tree_item(drag_item_, parent, ins_after);

            /* select the newly inserted item */
            TreeView_SelectItem(tree_, new_item);

            /* 
             *   forget the drag item handle - it's no longer valid now that
             *   we've deleted the item 
             */
            drag_item_ = 0;
        }

        /* we're no longer tracking the drag event */
        tracking_drag_ = FALSE;
        drag_item_ = 0;
        drag_info_ = 0;

        /* release our capture */
        ReleaseCapture();
    }

    /* inherit default */
    return CTadsWin::do_leftbtn_up(keys, x, y);
}

/*
 *   Move an item in the tree to a new location.  Returns the new item's
 *   handle (moving the item requires deleting the item and inserting a new
 *   one, so its handle changes in the process).  We recursively move all of
 *   the item's children with the item.  
 */
HTREEITEM CHtmlDbgProjWin::move_tree_item(HTREEITEM item,
                                          HTREEITEM new_parent,
                                          HTREEITEM ins_after)
{
    TV_INSERTSTRUCT tvi;
    char txtbuf[_MAX_PATH + 512];
    HTREEITEM new_item;
    HTREEITEM chi;
    HTREEITEM last_new_chi;

    /* set up to insert a new item at the specified target position */
    tvi.hParent = new_parent;
    tvi.hInsertAfter = ins_after;

    /* 
     *   get the item's current information, so we can replicate the
     *   information in the new item we'll insert 
     */
    tvi.item.mask = TVIF_TEXT | TVIF_STATE | TVIF_PARAM
                    | TVIF_IMAGE | TVIF_SELECTEDIMAGE
                    | TVIF_CHILDREN | TVIF_HANDLE;
    tvi.item.hItem = item;
    tvi.item.stateMask = (UINT)~0;
    tvi.item.pszText = (char *)txtbuf;
    tvi.item.cchTextMax = sizeof(txtbuf);
    TreeView_GetItem(tree_, &tvi.item);

    /* insert a copy of the item at the new location */
    new_item = TreeView_InsertItem(tree_, &tvi);

    /* move each child of this item to the new parent's child list */
    for (chi = TreeView_GetChild(tree_, item), last_new_chi = TVI_FIRST ;
         chi != 0 ;
         chi = TreeView_GetChild(tree_, item))
    {
        /* move this child so it's a child of the new parent */
        last_new_chi = move_tree_item(chi, new_item, last_new_chi);
    }

    /* 
     *   unlink the old tree item from its info structure, so that the info
     *   structure isn't deleted when we delete the old item - we want to
     *   re-use the same info structure with the new item we're about to
     *   insert 
     */
    forget_item_info(item);

    /* delete the old item */
    TreeView_DeleteItem(tree_, item);

    /* return a handle to the new item */
    return new_item;
}

/*
 *   process a capture change 
 */
int CHtmlDbgProjWin::do_capture_changed(HWND new_capture_win)
{
    /* if we're tracking a drag event, process it */
    if (tracking_drag_)
    {
        /* 
         *   it must have been an involuntary capture change - cancel the
         *   drag-drop processing 
         */
        tracking_drag_ = FALSE;
        drag_item_ = 0;
        drag_info_ = 0;
    }
    
    /* inherit default */
    return CTadsWin::do_capture_changed(new_capture_win);
}


/*
 *   Clear project information from a configuration object 
 */
void CHtmlDbgProjWin::clear_proj_config(CHtmlDebugConfig *config)
{
    int cnt;
    int i;
    
    /* clear the project file lists */
    config->clear_strlist("build", "source_files");
    config->clear_strlist("build", "source_file_types");
    config->clear_strlist("build", "include_files");
    config->clear_strlist("build", "image_resources");
    config->clear_strlist("build", "image_resources_recurse");
    config->clear_strlist("build", "ext_resfile_in_install");

    /* clear each external resource file list */
    if (config->getval("build", "ext resfile cnt", &cnt))
        cnt = 0;
    for (i = 0 ; i < cnt ; ++i)
    {
        char groupname[50];
        
        /* build the config group name for this file */
        gen_extres_names(0, groupname, i);

        /* clear this list */
        config->clear_strlist("build", groupname);
    }

    /* clear the external resource counter */
    config->setval("build", "ext resfile cnt", 0);
}

