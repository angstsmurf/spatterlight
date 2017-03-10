#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/w32dbgpr.cpp,v 1.3 1999/07/11 00:46:48 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32dbgpr.cpp - win32 debugger preferences dialog
Function
  
Notes
  
Modified
  03/14/98 MJRoberts  - Creation
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <Windows.h>

#include <os.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef W32DBGPR_H
#include "w32dbgpr.h"
#endif
#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif
#ifndef TADSCBTN_H
#include "tadscbtn.h"
#endif
#ifndef HTMLRES_H
#include "htmlres.h"
#endif
#ifndef HTMLDCFG_H
#include "htmldcfg.h"
#endif
#ifndef W32TDB_H
#include "w32tdb.h"
#endif
#ifndef FOLDSEL_H
#include "foldsel.h"
#endif
#ifndef W32MAIN_H
#include "w32main.h"
#endif
#ifndef TADSREG_H
#include "tadsreg.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Browse for a file to load or save.  Fills in the string buffer with the
 *   new filename and returns true if successful; returns false if the user
 *   cancels the dialog without selecting anything.  If 'load' is true,
 *   we'll show an "open file" dialog; otherwise, we'll show a "save file"
 *   dialog.  
 */
static int browse_file(HWND handle, CTadsDialogPropPage *pg,
                       const char *filter, const char *prompt,
                       const char *defext, int fld_id, DWORD flags,
                       int load, const char *rel_base_path)
{
    OPENFILENAME info;
    char fname[256];
    char dir[256];
    int result;
    
    /* set up the dialog definition structure */
    info.lStructSize = sizeof(info);
    info.hwndOwner = handle;
    info.hInstance = CTadsApp::get_app()->get_instance();
    info.lpstrFilter = filter;
    info.lpstrCustomFilter = 0;
    info.nFilterIndex = 0;
    info.lpstrFile = fname;
    info.nMaxFile = sizeof(fname);
    info.lpstrFileTitle = 0;
    info.nMaxFileTitle = 0;
    info.lpstrTitle = prompt;
    info.Flags = flags | OFN_NOCHANGEDIR;
    info.nFileOffset = 0;
    info.nFileExtension = 0;
    info.lpstrDefExt = defext;
    info.lCustData = 0;
    info.lpfnHook = 0;
    info.lpTemplateName = 0;
    CTadsDialog::set_filedlg_center_hook(&info);
    
    /* get the original filename from the field */
    GetWindowText(GetDlgItem(handle, fld_id), fname, sizeof(fname));
    if (fname[0] != '\0')
    {
        char *p;
        
        /* start off with the current filename */
        strcpy(dir, fname);
        
        /* get the root of the name */
        p = os_get_root_name(dir);
        
        /* copy the filename part */
        strcpy(fname, p);
        
        /* remove the file portion */
        if (p > dir && *(p-1) == '\\')
            *(p-1) = '\0';
        info.lpstrInitialDir = dir;
    }
    else
    {
        /* there's no filename yet */
        fname[0] = '\0';
        
        /* start off in the default open-file directory */
        info.lpstrInitialDir = CTadsApp::get_app()->get_openfile_dir();
    }
    
    /* ask for the file */
    if (load)
        result = GetOpenFileName(&info);
    else
        result = GetSaveFileName(&info);
    
    /* if it succeeded, store the result */
    if (result != 0)
    {
        /* if they want to make the filename relative, do so */
        if (rel_base_path != 0)
        {
            char buf[OSFNMAX];

            /* make a safe copy of the original filename */
            strcpy(buf, fname);

            /* get the path relative to the base path */
            oss_get_rel_path(fname, sizeof(fname), buf, rel_base_path);
        }

        /* copy the new filename back into the text field */
        SetWindowText(GetDlgItem(handle, fld_id), fname);
        
        /* update the open-file directory */
        CTadsApp::get_app()->set_openfile_dir(fname);
        
        /* set the change flag */
        if (pg != 0)
            pg->set_changed(TRUE);
        
        /* success */
        return TRUE;
    }
    else
    {
        /* they cancelled */
        return FALSE;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Basic debugger preferences property page 
 */
class CHtmlDialogDebugPref: public CTadsDialogPropPage
{
public:
    CHtmlDialogDebugPref(CHtmlSys_dbgmain *mainwin,
                         CHtmlDebugConfig *config,
                         int *inited)
    {
        mainwin_ = mainwin;
        config_ = config;
        inited_ = inited;
    }

protected:
    /* initialize */
    void init()
    {
        /* if we haven't yet done so, center the dialog */
        if (!*inited_)
        {
            /* center the parent dialog */
            center_dlg_win(GetParent(handle_));

            /* turn off the context-help style in the parent window */
            SetWindowLong(GetParent(handle_), GWL_EXSTYLE,
                          GetWindowLong(GetParent(handle_), GWL_EXSTYLE)
                          & ~WS_EX_CONTEXTHELP);

            /* note that we've done this so that other pages don't */
            *inited_ = TRUE;
        }

        /* inherit default */
        CTadsDialog::init();
    }

    /* load a text field from a configuration string */
    void load_field(const textchar_t *var, const textchar_t *subvar,
                    int field_id)
    {
        textchar_t buf[1024];

        /* get the configuration value */
        if (!config_->getval(var, subvar, 0, buf, sizeof(buf)))
        {
            /* store the value in the field */
            SetWindowText(GetDlgItem(handle_, field_id), buf);
        }
        else
        {
            /* the config value isn't set - clear the field */
            SetWindowText(GetDlgItem(handle_, field_id), "");
        }
    }

    /* save a field's value into a configuration string */
    void save_field(const textchar_t *var, const textchar_t *subvar,
                    int field_id, int is_filename)
    {
        textchar_t buf[1024];
        
        /* get the field value */
        GetWindowText(GetDlgItem(handle_, field_id), buf, sizeof(buf));

        /* if this is a filename, make sure we obey DOS-style naming rules */
        if (is_filename)
        {
            char *p;

            /* scan the filename */
            for (p = buf ; *p != '\0'; ++p)
            {
                /* convert forward slashes to backslashes */
                if (*p == '/')
                    *p = '\\';
            }
        }

        /* store the value in the configuration */
        config_->setval(var, subvar, 0, buf);
    }

    /* load a multi-line field from a list of configuration strings */
    void load_multiline_fld(const textchar_t *var, const textchar_t *subvar,
                            int field_id);

    /* save a multi-line field into a list of configuration strings */
    void save_multiline_fld(const textchar_t *var, const textchar_t *subvar,
                            int field_id, int is_filename);

    /* check for changes to a multiline field */
    int multiline_fld_changed(const textchar_t *var, const textchar_t *subvar,
                              int field_id);

    /* load a checkbox */
    void load_ckbox(const textchar_t *var, const textchar_t *subvar,
                    int ctl_id, int default_val)
    {
        int val;

        /* get the current value */
        if (config_->getval(var, subvar, &val))
            val = default_val;

        /* set the checkbox */
        CheckDlgButton(handle_, ctl_id, val ? BST_CHECKED : BST_UNCHECKED);
    }

    /* save a checkbox */
    void save_ckbox(const textchar_t *var, const textchar_t *subvar,
                    int ctl_id)
    {
        int val;
        
        /* get the checkbox value */
        val = (IsDlgButtonChecked(handle_, ctl_id) == BST_CHECKED);

        /* save it */
        config_->setval(var, subvar, val);
    }

    /* browse for a file */
    int browse_file(const char *filter, const char *prompt,
                    const char *defext, int fld_id, DWORD flags, int load,
                    int keep_rel)
    {
        char rel_base[OSFNMAX];

        /* if we want a relative path, get the project directory */
        if (keep_rel)
            os_get_path_name(rel_base, sizeof(rel_base),
                             mainwin_->get_local_config_filename());

        /* do the normal file browsing */
        return ::browse_file(handle_, this, filter, prompt, defext,
                             fld_id, flags, load, keep_rel ? rel_base : 0);
    }

    /* configuration object */
    CHtmlDebugConfig *config_;

    /* main window */
    CHtmlSys_dbgmain *mainwin_;

    /* 
     *   flag: we've initialized the parent dialog's position; we keep a
     *   pointer to a single flag common to all pages, so that, whichever
     *   page comes up first, we only initialize the parent dialog a
     *   single time 
     */
    int *inited_;
};

/*
 *   load a multi-line field from a list of configuration strings 
 */
void CHtmlDialogDebugPref::load_multiline_fld(const textchar_t *var,
                                              const textchar_t *subvar,
                                              int field_id)
{
    HWND fld;
    int i;
    int cnt;

    /* get the field handle */
    fld = GetDlgItem(handle_, field_id);

    /* get the number of strings to load */
    cnt = config_->get_strlist_cnt(var, subvar);

    /* read the strings from the configuration */
    for (i = 0 ; i < cnt ; ++i)
    {
        textchar_t buf[OSFNMAX];

        /* get the next string */
        if (!config_->getval(var, subvar, i, buf, sizeof(buf)))
        {
            /* append this string to the field */
            SendMessage(fld, EM_SETSEL, 32767, 32767);
            SendMessage(fld, EM_REPLACESEL, FALSE, (LPARAM)buf);

            /* append a newline */
            SendMessage(fld, EM_SETSEL, 32767, 32767);
            SendMessage(fld, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");
        }
    }
}

/*
 *   save a multi-line field into a list of configuration strings 
 */
void CHtmlDialogDebugPref::save_multiline_fld(const textchar_t *var,
                                              const textchar_t *subvar,
                                              int field_id, int is_filename)
{
    int i;
    int idx;
    int cnt;
    HWND fld;

    /* clear the old resource list */
    config_->clear_strlist(var, subvar);

    /* get the field handle */
    fld = GetDlgItem(handle_, field_id);

    /* get the number of items in the field */
    cnt = SendMessage(fld, EM_GETLINECOUNT, 0, 0);

    /* save the items in the field */
    for (i = 0, idx = 0 ; i < cnt ; ++i)
    {
        size_t len;
        textchar_t buf[OSFNMAX];

        /* get this line */
        *(WORD *)buf = (WORD)sizeof(buf) - 1;
        len = (size_t)SendMessage(fld, EM_GETLINE, (WPARAM)i, (LPARAM)buf);

        /* null-terminate the buffer */
        buf[len] = '\0';

        /* if this is a filename, convert to DOS naming rules */
        if (is_filename)
        {
            char *p;

            /* scan the filename */
            for (p = buf ; *p != '\0' ; ++p)
            {
                /* convert forward slashes to backslashes */
                if (*p == '/')
                    *p = '\\';
            }
        }

        /* if there's anything there, save it */
        if (len > 0)
            config_->setval(var, subvar, idx++, buf);
    }
}

/*
 *   check for changes (from the saved configuration) to a multi-line field
 *   into a list of configuration strings 
 */
int CHtmlDialogDebugPref::multiline_fld_changed(const textchar_t *var,
                                                const textchar_t *subvar,
                                                int field_id)
{
    int i;
    int cfg_cnt;
    int fld_cnt;
    HWND fld;
    char *matched;
    int mismatch;

    /* get the number of strings saved in the configuration */
    cfg_cnt = config_->get_strlist_cnt(var, subvar);

    /* get the field handle */
    fld = GetDlgItem(handle_, field_id);

    /* get the number of items in the field */
    fld_cnt = SendMessage(fld, EM_GETLINECOUNT, 0, 0);

    /* allocate an array of integers: one match per config item */
    matched = (char *)th_malloc(cfg_cnt);

    /* initialize each config item match flag to "not matched" */
    memset(matched, 0, cfg_cnt);

    /* 
     *   scan the lines in the field: check to see if we can match up each
     *   line of the field to a value in the configuration 
     */
    for (mismatch = FALSE, i = 0 ; i < fld_cnt ; ++i)
    {
        size_t len;
        textchar_t fld_buf[OSFNMAX];
        int j;
        int fld_matched;

        /* get this line from the field */
        *(WORD *)fld_buf = (WORD)sizeof(fld_buf) - 1;
        len = (size_t)SendMessage(fld, EM_GETLINE, (WPARAM)i,
                                  (LPARAM)fld_buf);

        /* null-terminate the buffer */
        fld_buf[len] = '\0';

        /* if there's nothing there, skip it */
        if (len <= 0)
            continue;

        /* scan the config items we haven't matched so far for a match */
        for (fld_matched = FALSE, j = 0 ; j < cfg_cnt && !fld_matched ; ++j)
        {
            textchar_t cfg_buf[OSFNMAX];

            /* 
             *   if this config item has been previously matched to a
             *   different field item, skip it - we only want to match each
             *   item once 
             */
            if (matched[j])
                continue;

            /* get this config item */
            if (!config_->getval(var, subvar, i, cfg_buf, sizeof(cfg_buf)))
            {
                /* if this one matches, so note */
                if (strcmp(cfg_buf, fld_buf) == 0)
                {
                    /* note that we found a field match */
                    fld_matched = TRUE;

                    /* note that this config item matched */
                    matched[j] = TRUE;

                    /* no need to keep scanning config items */
                    break;
                }
            }
        }

        /* if we didn't match the field, we have a mismatch */
        if (!fld_matched)
        {
            /* note the mismatch */
            mismatch = TRUE;

            /* no need to keep scanning field items */
            break;
        }
    }

    /* 
     *   if we didn't already find a mismatch among the field items, check
     *   for any unmatched config items 
     */
    if (!mismatch)
    {
        /* scan each config item for a mismatch */
        for (i = 0 ; i < cfg_cnt ; ++i)
        {
            /* check for a mismatch */
            if (!matched[i])
            {
                /* note that we have a mismatch */
                mismatch = TRUE;

                /* no need to keep looking */
                break;
            }
        }
    }

    /* we're done with the match array */
    th_free(matched);

    /* return the change indication */
    return mismatch;
}

/* ------------------------------------------------------------------------ */
/*
 *   Debugger preferences page dialog for start-up options 
 */
class CHtmlDialogDebugStart: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogDebugStart(CHtmlSys_dbgmain *mainwin,
                          CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
    /* 
     *   Note: We store the current start-up mode in the configuration
     *   variable "startup:action" as an integer value, as follows:
     *   
     *   1 = show welcome dialog
     *.  2 = open most recent project
     *.  3 = start with no project
     */
};

/*
 *   initialize 
 */
void CHtmlDialogDebugStart::init()
{
    int mode;

    /* inherit default processing */
    CHtmlDialogDebugPref::init();

    /* 
     *   get the current mode from the configuration, defaulting to "show
     *   welcome dialog" if there's no setting in the configuration 
     */
    if (config_->getval("startup", "action", &mode))
        mode = 1;

    /* set the appropriate radio button */
    CheckRadioButton(handle_, IDC_RB_START_WELCOME, IDC_RB_START_NONE,
                     IDC_RB_START_WELCOME + mode - 1);
}

/*
 *   process a command 
 */
int CHtmlDialogDebugStart::do_command(WPARAM cmd, HWND ctl)
{
    /* check which control is involved */
    switch(LOWORD(cmd))
    {
    case IDC_RB_START_WELCOME:
    case IDC_RB_START_RECENT:
    case IDC_RB_START_NONE:
        /* note the change */
        set_changed(TRUE);
        return TRUE;
    }

    /* we didn't handle it - inherit the default handling */
    return CHtmlDialogDebugPref::do_command(cmd, ctl);
}

int CHtmlDialogDebugStart::do_notify(NMHDR *nm, int ctl)
{
    int mode;

    /* check what we're doing */
    switch(nm->code)
    {
    case PSN_APPLY:
        /* figure out which mode is selected in the radio buttons */
        if (IsDlgButtonChecked(handle_, IDC_RB_START_WELCOME))
            mode = 1;
        else if (IsDlgButtonChecked(handle_, IDC_RB_START_RECENT))
            mode = 2;
        else
            mode = 3;

        /* save the mode in the configuration data */
        config_->setval("startup", "action", mode);

        /* done */
        return TRUE;
    }

    /* inherit the default handling */
    return CHtmlDialogDebugPref::do_notify(nm, ctl);
}


/* ------------------------------------------------------------------------ */
/*
 *   Debugger preferences page dialog for source window options 
 */
class CHtmlDialogDebugSrcwin: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogDebugSrcwin(CHtmlSys_dbgmain *mainwin,
                           CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }
    
    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
    /* read a popup's current value into a configuration variable */
    int read_popup(int id, const textchar_t *element);
    
    /* color settings */
    COLORREF cust_colors_[16];
    HTML_color_t text_color_;
    HTML_color_t bkg_color_;
    int use_win_colors_;

    /* font settings */
    CStringBuf font_;
    int font_size_;

    /* tab spacing */
    int tab_size_;
};

/*
 *   Read a popup 
 */
/*
 *   read the contents of one of the font popups into a CStringBuf 
 */
int CHtmlDialogDebugSrcwin::read_popup(int id, const textchar_t *element)
{
    HWND ctl;
    char buf[128];
    textchar_t oldval[128];
    size_t len;
    int idx;
    int changed;

    /* get the control handle */
    ctl = GetDlgItem(handle_, id);

    /* get the selection number */
    idx = SendMessage(ctl, CB_GETCURSEL, 0, 0);

    /* get the selected string */
    len = SendMessage(ctl, CB_GETLBTEXT, (WPARAM)idx, (LPARAM)buf);

    /* get the old value from the configuration */
    if (config_->getval("src win options", element,
                        0, oldval, sizeof(oldval)))
        oldval[0] = '\0';
    
    /* note whether this is a change */
    changed = (len != get_strlen(oldval)
               || memcmp(buf, oldval, len) != 0);

    /* store it in the configuration */
    config_->setval("src win options", element, 0, buf, len);

    /* return change indication */
    return changed;
}


/*
 *   Initialize 
 */
void CHtmlDialogDebugSrcwin::init()
{
    textchar_t buf[256];
    static const textchar_t *fontsizes[] =
    {
        "4", "5", "6", "8", "9", "10", "11", "12", "14", "16", "18",
        "20", "24", "28", "36", 0
    };
    static const textchar_t *tabsizes[] =
    {
        "1", "2", "3", "4", "5", "6", "7", "8", "10", "16", 0
    };
    const textchar_t **p;
    int idx;

    /* get the initial color settings */
    if (config_->getval_color("src win options", "text color", &text_color_))
        text_color_ = HTML_make_color(0, 0, 0);
    if (config_->getval_color("src win options", "bkg color", &bkg_color_))
        bkg_color_ = HTML_make_color(0xff, 0xff, 0xff);

    /* get the initial use-windows-colors setting */
    if (config_->getval("src win options", "use win colors",
                        &use_win_colors_))
        use_win_colors_ = TRUE;

    /* get the custom colors from the preferences */
    if (config_->getval_bytes("src win options", "custom colors",
                              cust_colors_, sizeof(cust_colors_)))
        memset(cust_colors_, 0, sizeof(cust_colors_));

    /* get the initial font from the preferences */
    if (config_->getval("src win options", "font name", 0, buf, sizeof(buf)))
        font_.set("Courier New");
    else
        font_.set(buf);
    
    /* get the initial font size from the preferences */
    if (config_->getval("src win options", "font size", &font_size_))
        font_size_ = 10;

    /* set up the color buttons */
    controls_.add(new CColorBtnPropPage(handle_, IDC_BTN_DBGTXTCLR,
                                        &text_color_, cust_colors_, this));
    controls_.add(new CColorBtnPropPage(handle_, IDC_BTN_DBGBKGCLR,
                                        &bkg_color_, cust_colors_, this));

    /* get the tab spacing */
    if (config_->getval("src win options", "tab size", &tab_size_))
        tab_size_ = 8;

    /* set the use-window-colors checkbox */
    CheckDlgButton(handle_, IDC_CK_DBGWINCLR,
                   (use_win_colors_ ? BST_CHECKED : BST_UNCHECKED));
    
    /* 
     *   enable or disable the text and background buttons according to
     *   whether the "use windows colors" checkbox is checked 
     */
    EnableWindow(GetDlgItem(handle_, IDC_BTN_DBGTXTCLR), !use_win_colors_);
    EnableWindow(GetDlgItem(handle_, IDC_BTN_DBGBKGCLR), !use_win_colors_);

    /* set up the font combo with a list of the available fonts */
    init_font_popup(IDC_CBO_DBGFONT, FALSE, TRUE, font_.get(), TRUE,
                    DEFAULT_CHARSET);

    /* set the initial font size field */
    sprintf(buf, "%d", font_size_);
    SetWindowText(GetDlgItem(handle_, IDC_CBO_DBGFONTSIZE), buf);

    /* set the initial tab size field */
    sprintf(buf, "%d", tab_size_);
    SetWindowText(GetDlgItem(handle_, IDC_CBO_DBGTABSIZE), buf);

    /* load up the suggested font sizes list */
    for (idx = 0, p = fontsizes ; *p != 0 ; ++p, ++idx)
        SendMessage(GetDlgItem(handle_, IDC_CBO_DBGFONTSIZE),
                    CB_INSERTSTRING, idx, (LPARAM)*p);

    /* load up the suggested tab sizes list */
    for (idx = 0, p = tabsizes ; *p != 0 ; ++p, ++idx)
        SendMessage(GetDlgItem(handle_, IDC_CBO_DBGTABSIZE),
                    CB_INSERTSTRING, idx, (LPARAM)*p);

    /* inherit default */
    CHtmlDialogDebugPref::init();
}

int CHtmlDialogDebugSrcwin::do_command(WPARAM cmd, HWND ctl)
{
    switch(LOWORD(cmd))
    {
    case IDC_CK_DBGWINCLR:
        {
            int checked;

            /* get the state of the button */
            checked = (IsDlgButtonChecked(handle_, IDC_CK_DBGWINCLR)
                       == BST_CHECKED);

            /* 
             *   enable or disable the text and background buttons
             *   accordingly 
             */
            EnableWindow(GetDlgItem(handle_, IDC_BTN_DBGTXTCLR), !checked);
            EnableWindow(GetDlgItem(handle_, IDC_BTN_DBGBKGCLR), !checked);

            /* note the state change */
            set_changed(TRUE);
        }
        return TRUE;

    case IDC_CBO_DBGFONT:
    case IDC_CBO_DBGFONTSIZE:
    case IDC_CBO_DBGTABSIZE:
        switch(HIWORD(cmd))
        {
        case CBN_EDITCHANGE:
        case CBN_SELCHANGE:
            /* note the change to the value */
            set_changed(TRUE);
            break;

        default:
            /* ignore other notifications */
            break;
        }
        return TRUE;

    default:
        /* inherit default behavior */
        return CHtmlDialogDebugPref::do_command(cmd, ctl);
    }
}

int CHtmlDialogDebugSrcwin::do_notify(NMHDR *nm, int ctl)
{
    char buf[256];
    
    switch(nm->code)
    {
    case PSN_APPLY:
        /* save the color settings */
        config_->setval_color("src win options", "text color", text_color_);
        config_->setval_color("src win options", "bkg color", bkg_color_);
        config_->setval_bytes("src win options", "custom colors",
                              cust_colors_, sizeof(cust_colors_));

        /* save the use-windows-colors setting */
        use_win_colors_ = (IsDlgButtonChecked(handle_, IDC_CK_DBGWINCLR)
                           == BST_CHECKED);
        config_->setval("src win options", "use win colors",
                        use_win_colors_);

        /* save the font */
        read_popup(IDC_CBO_DBGFONT, "font name");

        /* save the font size */
        GetWindowText(GetDlgItem(handle_, IDC_CBO_DBGFONTSIZE),
                      buf, sizeof(buf));
        config_->setval("src win options", "font size", (int)get_atol(buf));

        /* save the tab size */
        GetWindowText(GetDlgItem(handle_, IDC_CBO_DBGTABSIZE),
                      buf, sizeof(buf));
        config_->setval("src win options", "tab size", (int)get_atol(buf));

        /* tell the main window that we've updated the preferences */
        mainwin_->on_update_srcwin_options();

        /* done */
        return TRUE;
            
    default:
        /* inherit default behavior */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Debugger preferences page dialog for prompts 
 */
class CHtmlDialogDebugPrompt: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogDebugPrompt(CHtmlSys_dbgmain *mainwin,
                           CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
    /* read a config file setting */
    void read_config(const char *varname, const char *varname2,
                     int ckbox_id, int *internal_var, int default_val);

    /* get a checkbox into our internal state */
    void get_ck_state(int id, int *var);

    /* prompt settings */
    int welcome_;
    int gameover_;
    int bpmove_;
    int ask_exit_;
    int ask_load_;
    int ask_rlsrc_;
    int clr_dbglog_;
};

/*
 *   read a stored config setting and set our checkbox and internal state
 *   accordingly 
 */
void CHtmlDialogDebugPrompt::read_config(const char *varname,
                                         const char *varname2,
                                         int ckbox_id, int *internal_var,
                                         int default_val)
{
    /* get the config setting; if it's not defined, use the default value */
    if (config_->getval(varname, varname2, internal_var))
        *internal_var = default_val;

    /* set the checkbox */
    CheckDlgButton(handle_, ckbox_id,
                   *internal_var ? BST_CHECKED : BST_UNCHECKED);
}

/*
 *   initialize 
 */
void CHtmlDialogDebugPrompt::init()
{
    /* get the initial settings */
    read_config("gameover dlg", 0, IDC_CK_SHOW_GAMEOVER, &gameover_, TRUE);
    read_config("bpmove dlg", 0, IDC_CK_SHOW_BPMOVE, &bpmove_, TRUE);
    read_config("ask exit dlg", 0, IDC_CK_ASK_EXIT, &ask_exit_, TRUE);
    read_config("ask load dlg", 0, IDC_CK_ASK_LOAD, &ask_load_, TRUE);
    read_config("ask reload src", 0, IDC_CK_ASK_RLSRC, &ask_rlsrc_, TRUE);
    read_config("dbglog", "clear on build", IDC_CK_CLR_DBGLOG,
                &clr_dbglog_, TRUE);

    /* inherit default */
    CHtmlDialogDebugPref::init();
}

/*
 *   get a checkbutton's state into our internal variable 
 */
void CHtmlDialogDebugPrompt::get_ck_state(int id, int *var)
{
    /* get the button state and set the variable accordingly */
    *var = (IsDlgButtonChecked(handle_, id) == BST_CHECKED);
}

/*
 *   process a command 
 */
int CHtmlDialogDebugPrompt::do_command(WPARAM cmd, HWND ctl)
{
    switch(cmd)
    {
    case IDC_CK_SHOW_GAMEOVER:
    case IDC_CK_SHOW_BPMOVE:
    case IDC_CK_ASK_EXIT:
    case IDC_CK_ASK_LOAD:
    case IDC_CK_ASK_RLSRC:
    case IDC_CK_CLR_DBGLOG:
        /* update our internal state */
        get_ck_state(IDC_CK_SHOW_GAMEOVER, &gameover_);
        get_ck_state(IDC_CK_SHOW_BPMOVE, &bpmove_);
        get_ck_state(IDC_CK_ASK_EXIT, &ask_exit_);
        get_ck_state(IDC_CK_ASK_LOAD, &ask_load_);
        get_ck_state(IDC_CK_ASK_RLSRC, &ask_rlsrc_);
        get_ck_state(IDC_CK_CLR_DBGLOG, &clr_dbglog_);
        
        /* note the change */
        set_changed(TRUE);

        /* handled */
        return TRUE;
        
    default:
        /* inherit default */
        return CHtmlDialogDebugPref::do_command(cmd, ctl);
    }
}

/*
 *   process a notification 
 */
int CHtmlDialogDebugPrompt::do_notify(NMHDR *nm, int ctl)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* save the new settings */
        config_->setval("gameover dlg", 0, gameover_);
        config_->setval("bpmove dlg", 0, bpmove_);
        config_->setval("ask exit dlg", 0, ask_exit_);
        config_->setval("ask load dlg", 0, ask_load_);
        config_->setval("ask reload src", 0, ask_rlsrc_);
        config_->setval("dbglog", "clear on build", clr_dbglog_);

        /* done */
        return TRUE;

    default:
        /* inherit default behavior */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Debugger preferences page dialog for source file paths
 */
class CHtmlDialogDebugSrcpath: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogDebugSrcpath(CHtmlSys_dbgmain *mainwin,
                            CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
    /* read a config file setting */
    void read_config(const char *varname, int ckbox_id,
                     int *internal_var, int default_val);
};

/*
 *   initialize 
 */
void CHtmlDialogDebugSrcpath::init()
{
    /* 
     *   as the debugger to make sure the config is up to date with its
     *   internal source file list 
     */
    mainwin_->srcdir_list_to_config();

    /* load the source path */
    load_multiline_fld("srcdir", 0, IDC_FLD_SRCPATH);

    /* 
     *   For tads 2, if there's no workspace is open, disable the field,
     *   because a source path has no meaning outside of an active workspace.
     *   In tads 3, the field is always enabled because this is global
     *   configuration data.  
     */
    if (!mainwin_->get_workspace_open() && w32_system_major_vsn < 3)
        EnableWindow(GetDlgItem(handle_, IDC_FLD_SRCPATH), FALSE);
    
    /* inherit default */
    CHtmlDialogDebugPref::init();
}


/*
 *   process a command 
 */
int CHtmlDialogDebugSrcpath::do_command(WPARAM cmd, HWND ctl)
{
    char fname[OSFNMAX];

    /* check the control that was activated */
    switch(LOWORD(cmd))
    {
    case IDCANCEL:
        /* pass cancel to our parent */
        SendMessage(GetParent(handle_), WM_COMMAND, IDCANCEL, (LPARAM)ctl);
        return TRUE;

    case IDC_BTN_ADDDIR:
        /* 
         *   for tads 2, if no workspace is open, simply tell them why they
         *   can't add a directory at this time 
         */
        if (!mainwin_->get_workspace_open() && w32_system_major_vsn < 3)
        {
            /* show the message */
            MessageBox(GetParent(handle_),
                       "You must load a game before you can set the source "
                       "file search path, because the path needs to be "
                       "associated with a game.", "TADS Workbench",
                       MB_OK | MB_ICONEXCLAMATION);
            
            /* handled */
            return TRUE;
        }
        
        /* start in the current open-file folder */
        if (CTadsApp::get_app()->get_openfile_dir() != 0
            && CTadsApp::get_app()->get_openfile_dir()[0] != '\0')
            strcpy(fname, CTadsApp::get_app()->get_openfile_dir());
        else
            GetCurrentDirectory(sizeof(fname), fname);

        /* run the folder selector dialog */
        if (CTadsDialogFolderSel2::
            run_select_folder(GetParent(handle_),
                              CTadsApp::get_app()->get_instance(),
                              "Folder to add to source &path:",
                              "Add Folder to Source Search Path",
                              fname, sizeof(fname),
                              fname, 0, 0))
        {
            HWND fld;

            /* add the folder to our list */
            fld = GetDlgItem(handle_, IDC_FLD_SRCPATH);
            SendMessage(fld, EM_SETSEL, 32767, 32767);
            SendMessage(fld, EM_REPLACESEL, FALSE, (LPARAM)fname);

            /* add a newline */
            SendMessage(fld, EM_SETSEL, 32767, 32767);
            SendMessage(fld, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");

            /* use this as the next open-file directory */
            CTadsApp::get_app()->set_openfile_path(fname);
        }

        /* handled */
        return TRUE;

    case IDC_FLD_SRCPATH:
        if (HIWORD(cmd) == EN_CHANGE)
        {
            /* mark the data as updated */
            set_changed(TRUE);

            /* handled */
            return TRUE;
        }
        break;

    default:
        /* no special handling */
        break;
    }
    
    /* inherit default */
    return CHtmlDialogDebugPref::do_command(cmd, ctl);
}

/*
 *   process a notification 
 */
int CHtmlDialogDebugSrcpath::do_notify(NMHDR *nm, int ctl)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* save the new source path */
        save_multiline_fld("srcdir", 0, IDC_FLD_SRCPATH, TRUE);

        /* tell the debugger to rebuild its internal source path list */
        mainwin_->rebuild_srcdir_list();

        /* done */
        return TRUE;

    default:
        /* inherit default behavior */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Editor Auto-Configuration sub-dialog 
 */

/* editor information structure */
struct auto_config_info
{
    auto_config_info(const char *disp_name, const char *prog,
                     const char *cmdline)
    {
        /* allocate space for all three strings */
        this->disp_name = (char *)th_malloc(strlen(disp_name) + 1
                                            + strlen(prog) + 1
                                            + strlen(cmdline) + 1);
        
        /* copy the display name */
        strcpy(this->disp_name, disp_name);

        /* copy the program name after that */
        this->prog = this->disp_name + strlen(this->disp_name) + 1;
        strcpy(this->prog, prog);

        /* copy the command line after that */
        this->cmdline = this->prog + strlen(this->prog) + 1;
        strcpy(this->cmdline, cmdline);
    }

    ~auto_config_info()
    {
        /* delete our string buffer */
        th_free(disp_name);
    }

    /* display name */
    char *disp_name;

    /* the program executable name, or the DDE string, as applicable */
    char *prog;

    /* command line */
    char *cmdline;
};

class CTadsDialogEditAuto: public CTadsDialog
{
public:
    /* run the dialog */
    static void run_dlg(CHtmlDialogDebugPref *editor_dlg)
    {
        CTadsDialogEditAuto *dlg;

        /* create the dialog */
        dlg = new CTadsDialogEditAuto(editor_dlg);

        /* run the dialog */
        dlg->run_modal(DLG_DBG_EDITOR_AUTOCONFIG,
                       GetParent(editor_dlg->get_handle()),
                       CTadsApp::get_app()->get_instance());

        /* we're done with the dialog; delete it */
        delete dlg;
    }

    /* initialize */
    void init()
    {
        size_t i;
        HWND lb;
        HKEY hkey;
        DWORD disposition;

        /* allocate space for our editor list */
        editor_cnt_ = 10;
        editors_ = (auto_config_info **)
                   th_malloc(editor_cnt_ * sizeof(editors_[0]));

        /* clear the list */
        for (i = 0 ; i < editor_cnt_ ; ++i)
            editors_[i] = 0;

        /* we don't have anything in the list yet */
        i = 0;

        /* add Notepad */
        editors_[i++] = new auto_config_info("Notepad",
                                             "NOTEPAD.EXE", "%f");

        /* add Epsilon - read the registry to get its install directory */
        hkey = CTadsRegistry::open_key(HKEY_CURRENT_USER,
                                       "Software\\Lugaru\\Epsilon",
                                       &disposition, FALSE);
        if (hkey != 0)
        {
            DWORD idx;
            
            /* scan for a value with a name like "BinPathNN" */
            for (idx = 0 ; ; ++idx)
            {
                char namebuf[128];
                DWORD namesiz = sizeof(namebuf);
                BYTE databuf[OSFNMAX + 1];
                DWORD datasiz = sizeof(databuf);
                DWORD valtype;

                /* get the new value - give up on failure */
                if (RegEnumValue(hkey, idx, namebuf, &namesiz, 0,
                                 &valtype, databuf, &datasiz)
                    != ERROR_SUCCESS)
                    break;

                /* if this value starts with "BinPath", it's the one */
                if (valtype == REG_SZ
                    && strlen(namebuf) >= 7
                    && memicmp(namebuf, "BinPath", 7) == 0)
                {
                    char path[OSFNMAX];
                    
                    /* 
                     *   the value is the path to the Epsilon binaries
                     *   directory - build the full path to the SendEps
                     *   filename 
                     */
                    os_build_full_path(path, sizeof(path),
                                       (char *)databuf, "SendEps.exe");

                    /* add the configuration item */
                    editors_[i++] = new auto_config_info(
                        "Epsilon", path, "\"+%n:%0c\" \"%f\"");

                    /* we're done searching for keys */
                    break;
                }
            }

            /* done with the key */
            CTadsRegistry::close_key(hkey);
        }

        /* check to see if Imaginate is installed, and add it if so */
        hkey = CTadsRegistry::open_key(HKEY_LOCAL_MACHINE,
                                       "Software\\Imaginate",
                                       &disposition, FALSE);
        if (hkey != 0)
        {
            char dir[OSFNMAX];
            
            /* read the "Location" value */
            if (CTadsRegistry::query_key_str(hkey, "Location",
                                             dir, sizeof(dir)) != 0)
            {
                char path[OSFNMAX];
                
                /* that's the directory - build the full path */
                os_build_full_path(path, sizeof(path), dir, "Imaginate.exe");

                /* add the configuration item */
                editors_[i++] = new auto_config_info(
                    "Imaginate", path, "-f\"%f\"");
            }
            
            /* done with the key */
            CTadsRegistry::close_key(hkey);
        }

        /* populate the editors with file type associations */
        add_auto_config(".c");
        add_auto_config(".h");
        add_auto_config(".cpp");
        add_auto_config(".txt");
        
        /* get the listbox's handle */
        lb = GetDlgItem(handle_, IDC_LB_EDITORS);
        
        /* populate our list box */
        for (i = 0 ; editors_[i] != 0 ; ++i)
        {
            int idx;
            
            /* add this editor's name to the list box */
            idx = SendMessage(lb, LB_ADDSTRING,
                              0, (LPARAM)editors_[i]->disp_name);

            /* set the item data to our editor info structure */
            SendMessage(lb, LB_SETITEMDATA, idx, (LPARAM)editors_[i]);
        }

        /* we have no initial selection, so disable the OK button */
        EnableWindow(GetDlgItem(handle_, IDOK), FALSE);
    }

    /*
     *   Add an auto-configuration item for a file association
     */
    void add_auto_config(const char *suffix)
    {
        HKEY key;
        HKEY shell_key;
        DWORD disposition;
        char filetype[256];
        DWORD idx;

        /* find the registry key for the file suffix in CLASSES_ROOT */
        if (!get_key_val_str(HKEY_CLASSES_ROOT, suffix, 0,
                             filetype, sizeof(filetype)))
            return;
        
        /* 
         *   the value of the suffix key's default value is the type name
         *   for the suffix; the type name is in turn another key, under
         *   which we can find the launch information for the file type 
         */
        key = CTadsRegistry::open_key(HKEY_CLASSES_ROOT, filetype,
                                      &disposition, FALSE);
        if (key == 0)
            return;
        
        /* 
         *   we're interested in the "Shell" subkey of this key - under this
         *   key we'll find the various shell commands for the file type 
         */
        shell_key = CTadsRegistry::open_key(key, "Shell", &disposition, FALSE);
        
        /* we're done with the file type key */
        CTadsRegistry::close_key(key);
        
        /* enumerate the subkeys */
        for (idx = 0 ; ; ++idx)
        {
            LONG stat;
            char namebuf[256];
            char namebuf2[256];
            DWORD namelen;
            FILETIME wtime;
            HKEY item_key;
            char *src;
            char *dst;
            
            /* get the next key */
            namelen = sizeof(namebuf);
            stat = RegEnumKeyEx(shell_key, idx, namebuf, &namelen,
                                0, 0, 0, &wtime);

            /* if that was the last key, we're done */
            if (stat == ERROR_NO_MORE_ITEMS)
                break;

            /* remove any '&' we find in the buffer */
            for (src = namebuf, dst = namebuf2 ; *src != '\0' ; ++src)
            {
                /* copy anything buf '&' to the output */
                if (*src != '&')
                    *dst++ = *src;
            }
            
            /* null-terminate the output */
            *dst = '\0';

            /* if the name string doesn't start with "Open", skip it */
            if (strlen(namebuf2) < 4 || memicmp(namebuf2, "open", 4) != 0)
                continue;

            /* open the subkey */
            item_key = CTadsRegistry::open_key(shell_key, namebuf,
                                               &disposition, FALSE);
            if (item_key != 0)
            {
                HKEY dde_key;
                char dispname[256];
                char cmdbuf[256];
                char appbuf[256];
                char topicbuf[256];
                char argbuf[256];
                char *cmdroot;
                char *cmdext;
                size_t i;
                char dde_string[1024];
                char *p;

                /* get the "command" subkey's contents */
                if (!get_key_val_str(item_key, "command", 0,
                                     cmdbuf, sizeof(cmdbuf)))
                    cmdbuf[0] = '\0';

                /* get the arguments, which are with the "ddeexec" key */
                if (!get_key_val_str(item_key, "ddeexec", 0,
                                     argbuf, sizeof(argbuf)))
                    argbuf[0] = '\0';

                /* get the "ddeexec" subkey */
                dde_key = CTadsRegistry::open_key(item_key, "ddeexec",
                    &disposition, FALSE);
                if (dde_key != 0)
                {
                    /* get the "application" subkey */
                    if (!get_key_val_str(dde_key, "application", 0,
                                         appbuf, sizeof(appbuf)))
                        appbuf[0] = '\0';
                    
                    /* get the "topic" subkey */
                    if (!get_key_val_str(dde_key, "topic", 0,
                                         topicbuf, sizeof(topicbuf)))
                        topicbuf[0] = '\0';
                    
                    /* done with the DDE key */
                    CTadsRegistry::close_key(dde_key);
                }
                
                /* done with the item key - close it */
                CTadsRegistry::close_key(item_key);

                /* if we didn't find one or more of the values, skip it */
                if (cmdbuf[0] == '\0' || argbuf[0] == '\0'
                    || appbuf[0] == '\0' || topicbuf[0] == '\0')
                    continue;

                /* 
                 *   Pull out the program name from the command buffer.  If
                 *   the command starts with a double quote, pull out the
                 *   part up to the close quote.  Otherwise, pull out the
                 *   part before the first space. 
                 */
                if (cmdbuf[0] == '"')
                {
                    /* 
                     *   move everything down a byte, until we find the
                     *   close quote 
                     */
                    for (dst = cmdbuf, src = cmdbuf + 1 ; *src != '\0' ;
                         ++src)
                    {
                        /* 
                         *   If we're at a close quote, we're done.  If the
                         *   quote is stuttered, it's intended to indicate a
                         *   single double-quote - just copy one of them in
                         *   this case, and keep going, since it's not the
                         *   closing quote. 
                         */
                        if (*src == '"')
                        {
                            /* check for stuttering */
                            if (*(src+1) == '"')
                            {
                                /* it's stuttered - just copy one */
                                *dst++ = '"';
                                ++src;
                            }
                            else
                            {
                                /* that's the end of the command token */
                                break;
                            }
                        }
                        else
                        {
                            /* copy the character as it is */
                            *dst++ = *src;
                        }
                    }

                    /* null-terminate the result */
                    *dst = '\0';
                }
                else
                {
                    /* find the first space, and terminate there */
                    p = strchr(cmdbuf, ' ');
                    if (p != 0)
                        *p = '\0';
                }

                /* get the root of the program name */
                cmdroot = os_get_root_name(cmdbuf);

                /* find the extension */
                cmdext = strrchr(cmdroot, '.');
                if (cmdext == 0)
                    cmdext = cmdroot + strlen(cmdroot);

                /* 
                 *   pull out the command root name, sans path or extension
                 *   - this is the display name 
                 */
                memcpy(dispname, cmdroot, cmdext - cmdroot);
                dispname[cmdext - cmdroot] = '\0';

                /* set up the DDE command for this item */
                sprintf(dde_string, "DDE:%s,%s,%s", appbuf, topicbuf, cmdbuf);

                /* 
                 *   update the argument buffer - if we find a "%1", change
                 *   it to "%f", since that's our notation for the filename 
                 */
                for (p = argbuf ; *p != '\0' ; ++p)
                {
                    /* if it's "%1", change it to "%f" */
                    if (*p == '%' && *(p+1) == '1')
                        *(p+1) = 'f';
                }

                /* scan for another entry with the same program name */
                for (i = 0 ; i < editor_cnt_ && editors_[i] != 0 ; ++i)
                {
                    /* 
                     *   if the display names match (ignoring case), we
                     *   already have this command in our list
                     */
                    if (stricmp(dispname, editors_[i]->disp_name) == 0)
                    {
                        /*
                         *   This one matches.  Use the newer version in
                         *   preference to the older one, since the older
                         *   one could have been statically added and thus
                         *   mioght not have included the full program name.
                         *   If we find the same program name in the
                         *   registry that we added statically, the registry
                         *   version is better because it'll have the full
                         *   path. 
                         */
                        delete editors_[i];
                        editors_[i] = 0;

                        /* no need to look any further */
                        break;
                    }
                }

                /* set up the new editor at the index we landed on */
                editors_[i] =
                    new auto_config_info(dispname, dde_string, argbuf);
            }
        }

        /* we're done with the shell key */
        CTadsRegistry::close_key(shell_key);
    }

    /*
     *   Get the string value of a key.  Returns true on success, false if
     *   there is no such key or no such value.  
     */
    static int get_key_val_str(HKEY parent_key, const char *key_name,
                               const char *val_name, char *buf, size_t buflen)
    {
        HKEY key;
        DWORD disposition;
        int ret;
        
        /* presume failure */
        ret = FALSE;
        
        /* open the key */
        key = CTadsRegistry::open_key(parent_key, key_name,
                                      &disposition, FALSE);
        
        /* if there's no key, indicate failure */
        if (key == 0)
            return FALSE;

        /* if the value exists, retrieve it */
        if (CTadsRegistry::value_exists(key, val_name))
        {
            /* get the value */
            CTadsRegistry::query_key_str(key, val_name, buf, buflen);
            
            /* success */
            ret = TRUE;
        }
        
        /* we're done with the key - close it */
        CTadsRegistry::close_key(key);
        
        /* return our results */
        return ret;
    }

    /* destroy */
    void destroy()
    {
        size_t i;
        
        /* delete each editor configuration we allocated */
        for (i = 0 ; i < editor_cnt_ ; ++i)
        {
            /* if we allocated it, delete it */
            if (editors_[i] != 0)
                delete editors_[i];
        }

        /* delete the editor list itself */
        th_free(editors_);
    }

    /* process a command */
    int do_command(WPARAM id, HWND ctl)
    {
        int sel;
        HWND lb;

        /* see what command we're processing */
        switch(LOWORD(id))
        {
        case IDC_LB_EDITORS:
            /* it's a listbox notification - get the listbox handle */
            lb = GetDlgItem(handle_, IDC_LB_EDITORS);

            /* see what kind of notification we have */
            switch(HIWORD(id))
            {
            case LBN_SELCHANGE:
                /* the selection is changing - get the new selection */
                sel = SendMessage(lb, LB_GETCURSEL, 0, 0);

                /* 
                 *   enable the "OK" button if we have a valid selection,
                 *   disable it if not 
                 */
                EnableWindow(GetDlgItem(handle_, IDOK), (sel != LB_ERR));
                break;

            case LBN_DBLCLK:
                /* 
                 *   they double-clicked on a selection - if we have a valid
                 *   selection, treat this like an "OK" click 
                 */
                sel = SendMessage(lb, LB_GETCURSEL, 0, 0);
                if (sel != LB_ERR)
                {
                    /* change the event to IDOK */
                    id = IDOK;
                    
                    /* go process it like an "OK" button event */
                    goto do_ok_btn;
                }
            }

            /* proceed to inherited handling */
            break;
            
        case IDOK:
        do_ok_btn:
            /* get the listbox handle */
            lb = GetDlgItem(handle_, IDC_LB_EDITORS);

            /* get the currently selected item from the listbox */
            sel = SendMessage(lb, LB_GETCURSEL, 0, 0);

            /* 
             *   if an item is selected, fill in its information in the
             *   parent dialog 
             */
            if (sel != LB_ERR)
            {
                auto_config_info *info;
                
                /* 
                 *   get the pointer to our information structure for the
                 *   item 
                 */
                info = (auto_config_info *)
                       SendMessage(lb, LB_GETITEMDATA, sel, 0);

                /* set the program information in the parent dialog */
                SetWindowText(GetDlgItem(editor_dlg_->get_handle(),
                                         IDC_FLD_EDITOR), info->prog);

                /* set the command line information in the parent dialog */
                SetWindowText(GetDlgItem(editor_dlg_->get_handle(),
                                         IDC_FLD_EDITOR_ARGS), info->cmdline);

                /* set the modified flag in the parent dialog */
                editor_dlg_->set_changed(TRUE);
            }
            
            /* proceed to inherit the default processing */
            break;

        default:
            break;
        }

        /* inherit the default processing */
        return CTadsDialog::do_command(id, ctl);
    }

protected:
    CTadsDialogEditAuto(CHtmlDialogDebugPref *editor_dlg)
    {
        /* remember the editor dialog */
        editor_dlg_ = editor_dlg;
    }

    /* the list of pre-configured editors */
    struct auto_config_info **editors_;
    size_t editor_cnt_;

    /* parent editor dialog */
    CHtmlDialogDebugPref *editor_dlg_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Debugger editor preferences page 
 */
class CHtmlDialogDebugEditor: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogDebugEditor(CHtmlSys_dbgmain *mainwin,
                           CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
    /* editor settings */
    CStringBuf editor_;
    CStringBuf args_;
};

/*
 *   Advanced editor settings dialog 
 */
class CTadsDialogEditAdv: public CTadsDialog
{
public:
    /* run the dialog */
    static void run_dlg(CTadsDialogPropPage *editor_dlg)
    {
        CTadsDialogEditAdv *dlg;
        
        /* create the dialog */
        dlg = new CTadsDialogEditAdv(editor_dlg);

        /* run the dialog */
        dlg->run_modal(DLG_DBG_EDIT_ADV, GetParent(editor_dlg->get_handle()),
                       CTadsApp::get_app()->get_instance());

        /* we're done with the dialog; delete it */
        delete dlg;
    }

    /* initialize */
    void init()
    {
        char prog[1024];
        
        /* get the current "program" setting from the parent dialog */
        GetWindowText(GetDlgItem(editor_hwnd_, IDC_FLD_EDITOR),
                      prog, sizeof(prog));

        /* 
         *   if it starts with "DDE:", it has the form "DDE:server,topic";
         *   otherwise, it's not a DDE string 
         */
        if (strlen(prog) > 4 && memicmp(prog, "DDE:", 4) == 0)
        {
            char *p;
            char *server;
            char *topic;
            char *exename;
            
            /* check the DDE checkbox */
            CheckDlgButton(handle_, IDC_CK_USE_DDE, BST_CHECKED);

            /* get the server name string - it's everything up to the comma */
            for (p = server = prog + 4 ; *p != '\0' && *p != ',' ; ++p) ;
            if (*p == ',')
                *p++ = '\0';

            /* get the topic name string - it's up to the next comma */
            for (topic = p ; *p != '\0' && *p != ',' ; ++p) ;
            if (*p == ',')
                *p++ = '\0';

            /* everything else is the executable name */
            exename = p;

            /* set the server name string in the dialog */
            SetWindowText(GetDlgItem(handle_, IDC_FLD_DDESERVER), server);

            /* set the topic name */
            SetWindowText(GetDlgItem(handle_, IDC_FLD_DDETOPIC), topic);

            /* set the executable name */
            SetWindowText(GetDlgItem(handle_, IDC_FLD_DDEPROG), exename);

            /* note that we originally had DDE selected */
            dde_was_on_ = TRUE;
        }
        else
        {
            /* uncheck the DDE checkbox */
            CheckDlgButton(handle_, IDC_CK_USE_DDE, BST_UNCHECKED);

            /* note that DDE was not originally selected */
            dde_was_on_ = FALSE;
        }
    }

    /* process a command */
    int do_command(WPARAM id, HWND ctl)
    {
        switch(id)
        {
        case IDOK:
            /* 
             *   they're accepting the settings - if "use dde" is checked,
             *   copy the DDE settings back to the main dialog; otherwise,
             *   restore the original program name setting 
             */
            if (IsDlgButtonChecked(handle_, IDC_CK_USE_DDE) == BST_CHECKED)
            {
                char ddestr[1024];
                size_t len;
                
                /* get the server part */
                strcpy(ddestr, "DDE:");
                GetWindowText(GetDlgItem(handle_, IDC_FLD_DDESERVER),
                              ddestr + 4, sizeof(ddestr) - 4);

                /* add the comma */
                len = strlen(ddestr);
                ddestr[len++] = ',';

                /* add the topic part */
                GetWindowText(GetDlgItem(handle_, IDC_FLD_DDETOPIC),
                              ddestr + len, sizeof(ddestr) - len);

                /* add another comma */
                len = strlen(ddestr);
                ddestr[len++] = ',';

                /* add the program name */
                GetWindowText(GetDlgItem(handle_, IDC_FLD_DDEPROG),
                              ddestr + len, sizeof(ddestr) - len);

                /* set the program string to the DDE string */
                SetWindowText(GetDlgItem(editor_hwnd_,
                                         IDC_FLD_EDITOR), ddestr);

                /* set the 'changed' flag in the parent */
                editor_dlg_->set_changed(TRUE);
            }
            else
            {
                /* if DDE was originally selected, clear the program string */
                if (dde_was_on_)
                    SetWindowText(GetDlgItem(editor_hwnd_,
                                             IDC_FLD_EDITOR), "");

                /* set the 'changed' flag in the parent */
                editor_dlg_->set_changed(TRUE);
            }

            /* proceed to inherit the default processing */
            break;
            
        case IDC_BTN_BROWSE:
            /* browse for the program file */
            browse_file(handle_, 0, "Applications\0*.exe\0All Files\0*.*\0\0",
                        "Select a Text Editor Application", 0,
                        IDC_FLD_DDEPROG, OFN_HIDEREADONLY | OFN_FILEMUSTEXIST
                        | OFN_PATHMUSTEXIST, TRUE, FALSE);

            /* handled */
            return TRUE;

        default:
            break;
        }

        /* inherit the default processing */
        return CTadsDialog::do_command(id, ctl);
    }

protected:
    CTadsDialogEditAdv(CTadsDialogPropPage *editor_dlg)
    {
        /* remember the editor dialog */
        editor_dlg_ = editor_dlg;
        editor_hwnd_ = editor_dlg->get_handle();
    }

    /* parent editor dialog */
    CTadsDialogPropPage *editor_dlg_;

    /* parent editor dialog window */
    HWND editor_hwnd_;

    /* flag: DDE was initially selected when the dialog started */
    int dde_was_on_ : 1;
};

/*
 *   initialize 
 */
void CHtmlDialogDebugEditor::init()
{
    char buf[1024];
    
    /* 
     *   Get the initial editor setting.  If there's no editor, use
     *   NOTEPAD by default.  
     */
    if (config_->getval("editor", "program", 0, buf, sizeof(buf))
        || strlen(buf) == 0)
        strcpy(buf, "Notepad.EXE");
    SetWindowText(GetDlgItem(handle_, IDC_FLD_EDITOR), buf);

    /* get the command line; use "%f" by default */
    if (config_->getval("editor", "cmdline", 0, buf, sizeof(buf))
        || strlen(buf) == 0)
        strcpy(buf, "%f");
    SetWindowText(GetDlgItem(handle_, IDC_FLD_EDITOR_ARGS), buf);

    /* inherit default */
    CHtmlDialogDebugPref::init();
}

/*
 *   process a command 
 */
int CHtmlDialogDebugEditor::do_command(WPARAM cmd, HWND ctl)
{
    switch(LOWORD(cmd))
    {
    case IDC_FLD_EDITOR:
    case IDC_FLD_EDITOR_ARGS:
        /* if it's an EN_CHANGE notification, note the change */
        if (HIWORD(cmd) == EN_CHANGE)
        {
            set_changed(TRUE);
            return TRUE;
        }

        /* nothing special; inherit default */
        break;

    case IDC_BTN_ADVANCED:
        /* run the "advanced settings" dialog */
        CTadsDialogEditAdv::run_dlg(this);
        return TRUE;

    case IDC_BTN_AUTOCONFIG:
        /* run the "editor auto configuration" dialog */
        CTadsDialogEditAuto::run_dlg(this);
        return TRUE;
        
    case IDC_BTN_BROWSE:
        /* browse for the application file */
        browse_file("Applications\0*.exe\0All Files\0*.*\0\0",
                    "Select your Text Editor Application", 0, IDC_FLD_EDITOR,
                    OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY,
                    TRUE, FALSE);
        
        /* handled */
        return TRUE;

    default:
        /* nothing special; inherit default */
        break;
    }
    
    /* inherit default */
    return CHtmlDialogDebugPref::do_command(cmd, ctl);
}

/*
 *   process a notification 
 */
int CHtmlDialogDebugEditor::do_notify(NMHDR *nm, int ctl)
{
    char buf[1024];
    
    switch(nm->code)
    {
    case PSN_APPLY:
        /* save the new editor setting */
        GetWindowText(GetDlgItem(handle_, IDC_FLD_EDITOR), buf, sizeof(buf));
        config_->setval("editor", "program", 0, buf);

        /* save the new command line setting */
        GetWindowText(GetDlgItem(handle_, IDC_FLD_EDITOR_ARGS),
                      buf, sizeof(buf));
        config_->setval("editor", "cmdline", 0, buf);

        /* done */
        return TRUE;

    default:
        /* inherit default behavior */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }
}



/* ------------------------------------------------------------------------ */
/*
 *   Run the debugger preferences dialog 
 */
void run_dbg_prefs_dlg(HWND owner, CHtmlSys_dbgmain *mainwin)
{
    PROPSHEETPAGE psp[10];
    int i;
    PROPSHEETHEADER psh;
    CHtmlDialogDebugStart *start_dlg;
    CHtmlDialogDebugSrcwin *srcwin_dlg;
    CHtmlDialogDebugPrompt *prompt_dlg;
    CHtmlDialogDebugEditor *editor_dlg;
    CHtmlDialogDebugSrcpath *srcpath_dlg;
    void *ctx;
    int inited = FALSE;

    /* create our pages */
    start_dlg = new CHtmlDialogDebugStart(mainwin,
                                          mainwin->get_global_config(),
                                          &inited);
    srcwin_dlg = new CHtmlDialogDebugSrcwin(mainwin,
                                            mainwin->get_global_config(),
                                            &inited);
    prompt_dlg = new CHtmlDialogDebugPrompt(mainwin,
                                            mainwin->get_global_config(),
                                            &inited);
    editor_dlg = new CHtmlDialogDebugEditor(mainwin,
                                            mainwin->get_global_config(),
                                            &inited);

    /* 
     *   Create the library path dialog.  Note that the source path is part
     *   of the local configuration for tads 2 workbench, but in T3 it
     *   becomes the library path and moves to the global configuration.  
     */
    srcpath_dlg = new CHtmlDialogDebugSrcpath(mainwin,
                                              w32_system_major_vsn >= 3
                                              ? mainwin->get_global_config()
                                              : mainwin->get_local_config(),
                                              &inited);

    /* set up the main dialog descriptor */
    psh.dwSize = PROPSHEETHEADER_V1_SIZE;
    psh.dwFlags = PSH_USEICONID | PSH_PROPSHEETPAGE;
    psh.hwndParent = owner;
    psh.hInstance = CTadsApp::get_app()->get_instance();
    psh.pszIcon = 0;
    psh.pszCaption = (LPSTR)"Debugger Options";
    psh.nStartPage = 0;
    psh.ppsp = (LPCPROPSHEETPAGE)&psp;
    psh.pfnCallback = 0;

    /* set up the property page descriptor for the start-up options page */
    i = 0;
    start_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                            &psp[i++], DLG_DEBUG_STARTOPT,
                            IDS_DEBUG_STARTOPT, 0, 0, &psh);

    /* add the source-file formatting page */
    srcwin_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                             &psp[i++], DLG_DEBUG_SRCWIN, IDS_DEBUG_SRCWIN, 
                             0, 0, &psh);

    /* set up the "messages" page */
    prompt_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                             &psp[i++], DLG_DEBUG_PROMPTS, IDS_DEBUG_PROMPTS,
                             0, 0, &psh);

    /* set up the "editor" page */
    editor_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                             &psp[i++], DLG_DEBUG_EDITOR, IDS_DEBUG_EDITOR,
                             0, 0, &psh);

    /* set up the "library path" page */
    srcpath_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                              &psp[i++], DLG_DEBUG_SRCPATH, IDS_DEBUG_SRCPATH,
                              0, 0, &psh);

    /* set the page count in the header */
    psh.nPages = i;

    /* disable windows owned by the main window before showing the dialog */
    ctx = CTadsDialog::modal_dlg_pre(mainwin->get_handle(), FALSE);

    /* run the property sheet */
    PropertySheet(&psh);

    /* re-enable windows */
    CTadsDialog::modal_dlg_post(ctx);

    /* delete the dialogs */
    delete start_dlg;
    delete srcwin_dlg;
    delete prompt_dlg;
    delete editor_dlg;
    delete srcpath_dlg;
}

/* ------------------------------------------------------------------------ */
/*
 *   Build Options - Source page
 */
class CHtmlDialogBuildSrc: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogBuildSrc(CHtmlSys_dbgmain *mainwin,
                        CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
        /* start at the first file filter */
        selector_filter_index_ = 1;
    }

    ~CHtmlDialogBuildSrc()
    {
        /* restore the tree control's original window procedure */
        SetWindowLong(GetDlgItem(handle_, IDC_FLD_RSC), GWL_WNDPROC,
                      (LONG)tree_defproc_);

        /* remove our property from the tree control */
        RemoveProp(GetDlgItem(handle_, IDC_FLD_RSC),
                   "CHtmlDialogBuildSrc.parent.this");
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
    /* insert an item into our tree view control after the given item */
    HTREEITEM insert_tree(HTREEITEM parent, HTREEITEM ins_after,
                          const textchar_t *txt, DWORD state)
    {
        TV_INSERTSTRUCT tvi;
        HTREEITEM item;
        HWND tree;

        /* get the tree control */
        tree = GetDlgItem(handle_, IDC_FLD_RSC);

        /* build the insertion record */
        tvi.hParent = parent;
        tvi.hInsertAfter = ins_after;
        tvi.item.mask = TVIF_TEXT | TVIF_STATE;
        tvi.item.state = state;
        tvi.item.stateMask = 0xffffffff;
        tvi.item.pszText = (char *)txt;

        /* insert it into the tree */
        item = TreeView_InsertItem(tree, &tvi);

        /* 
         *   make sure the parent is expanded (but don't expand the root,
         *   since this is not legal with some version of the tree
         *   control) 
         */
        if (parent != TVI_ROOT)
            TreeView_Expand(tree, parent, TVE_EXPAND);

        /* select the newly-inserted item */
        TreeView_SelectItem(tree, item);

        /* return the new item handle */
        return item;
    }

    /* insert an item in our tree view control at the end of a list */
    HTREEITEM append_tree(HTREEITEM parent, const textchar_t *txt,
                          DWORD state)
        { return insert_tree(parent, TVI_LAST, txt, state); }

    /* get the selected top-level resource file (a .RSn or the .GAM file) */
    HTREEITEM get_selected_rsfile() const
    {
        HWND tree;
        HTREEITEM parent;
        HTREEITEM cur;

        /* get the tree */
        tree = GetDlgItem(handle_, IDC_FLD_RSC);

        /* get the current selected item */
        parent = TreeView_GetSelection(tree);

        /* if there's nothing selected, use the first item */
        if (parent == 0)
            return TreeView_GetRoot(tree);

        /* find the top-level control containing the selection */
        for (cur = parent ; cur != 0 ;
             parent = cur, cur = TreeView_GetParent(tree, cur)) ;

        /* return the result */
        return parent;
    }

    /* 
     *   renumber the .RSn files in our list - every time we insert or
     *   delete a .RSn file, we renumber them to keep the numbering
     *   contiguous 
     */
    void renum_rsn_files()
    {
        HWND tree = GetDlgItem(handle_, IDC_FLD_RSC);
        HTREEITEM cur;
        int i;
        
        /* 
         *   get the root item - this is the .gam file, so it doesn't need
         *   a number 
         */
        cur = TreeView_GetRoot(tree);

        /* start at number 0 */
        i = 0;

        /* renumber each item */
        for (cur = TreeView_GetNextSibling(tree, cur) ; cur != 0 ;
             cur = TreeView_GetNextSibling(tree, cur), ++i)
        {
            char buf[40];
            TV_ITEM tvi;
            
            /* build the new name for this item */
            sprintf(buf, "External Resource File (.rs%d)", i);

            /* relabel the item */
            tvi.hItem = cur;
            tvi.mask = TVIF_TEXT;
            tvi.pszText = buf;
            TreeView_SetItem(tree, &tvi);
        }
    }

    /* hook procedure for subclassed tree control */
    static LRESULT CALLBACK tree_proc(HWND hwnd, UINT msg, WPARAM wpar,
                                      LPARAM lpar)
    {
        CHtmlDialogBuildSrc *win;

        /* get the 'this' pointer from the window properties */
        win = (CHtmlDialogBuildSrc *)
              GetProp(hwnd, "CHtmlDialogBuildSrc.parent.this");

        /* there's nothing we can do if we couldn't get the window */
        if (win == 0)
            return 0;
        
        /* invoke the non-static version */
        return win->do_tree_proc(hwnd, msg, wpar, lpar);
    }

    /* hook procedure - member variable version */
    LRESULT do_tree_proc(HWND hwnd, UINT msg, WPARAM wpar, LPARAM lpar)
    {
        /* check the message */
        switch(msg)
        {
        case WM_DROPFILES:
            /* do our drop file procedure */
            do_tree_dropfiles((HDROP)wpar);

            /* handled */
            return 0;

        default:
            /* no special handling is required */
            break;
        }
        
        /* call the original handler */
        return CallWindowProc((WNDPROC)tree_defproc_, hwnd, msg, wpar, lpar);
    }

    /* process a drop-files event in the tree control */
    void do_tree_dropfiles(HDROP hdrop)
    {
        HTREEITEM cur;
        HTREEITEM parent;
        HWND tree;
        TV_HITTESTINFO ht;
        int cnt;
        int i;

        /* get the tree */
        tree = GetDlgItem(handle_, IDC_FLD_RSC);

        /* find the tree view list item nearest the drop point */
        DragQueryPoint(hdrop, &ht.pt);
        parent = TreeView_HitTest(tree, &ht);

        /* if they're below the last item, use the last item */
        if (parent == 0)
        {
            /* find the last item at the root level in the list */
            for (parent = cur = TreeView_GetRoot(tree) ; cur != 0 ;
                 parent = cur, cur = TreeView_GetNextSibling(tree, cur)) ;
        }
        else
        {
            /* find the topmost parent of the selected item */
            for (cur = parent ; cur != 0 ;
                 parent = cur, cur = TreeView_GetParent(tree, cur)) ;
        }

        /* insert the files */
        cnt = DragQueryFile(hdrop, 0xFFFFFFFF, 0, 0);
        for (i = 0 ; i < cnt ; ++i)
        {
            char fname[OSFNMAX];
            
            /* get this file's name */
            DragQueryFile(hdrop, i, fname, sizeof(fname));

            /* add it to the list */
            append_tree(parent, fname, 0);
        }

        /* end the drag operation */
        DragFinish(hdrop);

        /* note that we've changed the list */
        set_changed(TRUE);
    }

    /* delete the selected item */
    void delete_tree_selection()
    {
        HWND tree;
        HTREEITEM sel;
        int deleting_rsn;

        /* get the tree */
        tree = GetDlgItem(handle_, IDC_FLD_RSC);

        /* get the selected item */
        sel = TreeView_GetSelection(tree);
        
        /* if there's no selection, there's nothing to do */
        if (sel == 0)
            return;

        /* if this is the first item, we can't delete it */
        if (sel == TreeView_GetRoot(tree))
        {
            /* ignore it */
            return;
        }

        /* note whether we're deleting an .RSn file */
        deleting_rsn = (TreeView_GetParent(tree, sel) == 0);

        /* 
         *   if this is a top-level item, and it has any children, confirm
         *   the deletion 
         */
        if (deleting_rsn && TreeView_GetChild(tree, sel) != 0)
        {
            int btn;

            /* ask if they really want to do this */
            btn = MessageBox(GetParent(handle_),
                             "Are you sure you want to remove this entire "
                             "external resource file and all of its "
                             "resources?", "Confirm Deletion",
                             MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);

            /* if they didn't say yes, ignore it */
            if (btn != IDYES && btn != 0)
                return;
        }

        /* delete the item */
        TreeView_DeleteItem(tree, sel);

        /* if we deleted an .RSn file, renumber the remaining .RSn files */
        if (deleting_rsn)
            renum_rsn_files();

        /* mark the change */
        set_changed(TRUE);
    }

    /* original tree procedure, before we hooked it */
    WNDPROC tree_defproc_;

    /* index of file filter in selector dialog */
    int selector_filter_index_;
};

/*
 *   initialize 
 */
void CHtmlDialogBuildSrc::init()
{
    HTREEITEM parent;
    int cnt;
    int i;
    int rs_file_cnt;
    int next_rs_idx;
    int cur_rs_file;
    textchar_t buf[20];
    HWND tree;
        
    /* initialize the source file field */
    load_field("build", "source", IDC_FLD_SOURCE);

    /* insert the top-level resource list item for the game */
    parent = append_tree(TVI_ROOT, "Compiled game file (.gam)",
                         TVIS_BOLD | TVIS_EXPANDED);

    /* get the number of .RSn files */
    rs_file_cnt = config_->get_strlist_cnt("build", "rs file cnt");

    /* start at the zeroeth resource file, which is the .gam file */
    cur_rs_file = 0;

    /* 
     *   get the number of resources in the main game file - this is
     *   always the first resource file 
     */
    if (rs_file_cnt == 0
        || config_->getval("build", "rs file cnt", 0, buf, sizeof(buf)))
    {
        /* 
         *   there's no limit; use a negative number so that we never get
         *   there 
         */
        next_rs_idx = -1;
    }
    else
    {
        /* use the count from the configuration */
        next_rs_idx = atoi(buf);
    }

    /* get the resource list tree control */
    tree = GetDlgItem(handle_, IDC_FLD_RSC);

    /* insert the game resources */
    cnt = config_->get_strlist_cnt("build", "resources");
    for (i = 0 ; i < cnt ; ++i)
    {
        char buf[OSFNMAX];

        /* 
         *   If we've reached the start of the next .RSn file, insert the
         *   new parent item.  Note that we must make this check
         *   repeatedly in case we encounter a .RSn file with no entries.  
         */
        while (i == next_rs_idx)
        {
            char buf[50];
            
            /* 
             *   if the previous file wasn't the game file, show it
             *   initially collapsed 
             */
            if (cur_rs_file != 0)
                TreeView_Expand(tree, parent, TVE_COLLAPSE);

            /* append the next item */
            sprintf(buf, "External Resource File (.rs%d)", cur_rs_file);
            parent = append_tree(TVI_ROOT, buf, TVIS_BOLD);

            /* on to the next file */
            ++cur_rs_file;

            /* read the number of resources in the next block */
            if (cur_rs_file >= rs_file_cnt
                || config_->getval("build", "rs file cnt", cur_rs_file,
                                   buf, sizeof(buf)))
            {
                /* 
                 *   this is the last one, so all remaining resources are
                 *   in this file 
                 */
                next_rs_idx = -1;
            }
            else
            {
                /* 
                 *   calculate the index of the first resource in the next
                 *   file - it's the starting index of this file plus the
                 *   number of items in this file 
                 */
                next_rs_idx += atoi(buf);
            }
        }
        
        /* get this item */
        if (!config_->getval("build", "resources", i, buf, sizeof(buf)))
        {
            /* add it to the tree */
            append_tree(parent, buf, 0);
        }
    }

    /* initially collapse the last .RSn file (unless it's the .gam file) */
    if (cur_rs_file != 0)
        TreeView_Expand(tree, parent, TVE_COLLAPSE);

    /* add any additional empty files at the end of the list */
    while (cur_rs_file + 1 < rs_file_cnt)
    {
        char buf[50];
        
        /* append the next item */
        sprintf(buf, "External Resource File (.rs%d)", cur_rs_file);
        parent = append_tree(TVI_ROOT, buf, TVIS_BOLD);

        /* up the count */
        ++cur_rs_file;
    }

    /* initially select the first item in the list */
    TreeView_SelectItem(tree, TreeView_GetRoot(tree));

    /* 
     *   hook the window procedure for the list control so that we can
     *   intercept WM_DROPFILE messages 
     */
    tree_defproc_ = (WNDPROC)SetWindowLong(tree, GWL_WNDPROC,
                                           (LONG)&tree_proc);

    /* 
     *   set a property with my 'this' pointer, so we can find it in the
     *   hook procedure 
     */
    SetProp(tree, "CHtmlDialogBuildSrc.parent.this", (HANDLE)this);

    /* inherit default */
    CHtmlDialogDebugPref::init();
}

/*
 *   process a notification 
 */
int CHtmlDialogBuildSrc::do_notify(NMHDR *nm, int ctl)
{
    HWND tree;
    HTREEITEM parent;
    int resnum;
    int resnum_base;
    char label[OSFNMAX];
    int cur_rs_file;
    
    switch(nm->code)
    {
    case TVN_KEYDOWN:
        if (ctl == IDC_FLD_RSC)
        {
            TV_KEYDOWN *tvk = (TV_KEYDOWN *)nm;

            /* check for special keys that we recognize */
            switch(tvk->wVKey)
            {
            case VK_DELETE:
            case VK_BACK:
                /* delete/backspace - delete the selected item */
                delete_tree_selection();

                /* done */
                break;

            default:
                /* no special handling is required */
                break;
            }
            
            /* handled */
            return TRUE;
        }

        /* other cases aren't handled */
        break;

    case TVN_SELCHANGED:
        if (ctl == IDC_FLD_RSC)
        {
            NM_TREEVIEW *tvn = (NM_TREEVIEW *)nm;
            HWND tree = GetDlgItem(handle_, ctl);

            /* 
             *   turn on the "remove" button if anything other than the
             *   root item (the .gam file) is selected; turn it off if the
             *   game file is selected or there's no selection 
             */
            EnableWindow(GetDlgItem(handle_, IDC_BTN_REMOVE),
                         tvn->itemNew.hItem != 0
                         && tvn->itemNew.hItem != TreeView_GetRoot(tree));
            
            /* handled */
            return TRUE;
        }

        /* other cases aren't handled */
        break;

    case PSN_APPLY:
        /* save the new source file setting */
        save_field("build", "source", IDC_FLD_SOURCE, TRUE);

        /* get the resource list tree */
        tree = GetDlgItem(handle_, IDC_FLD_RSC);

        /* clear the old resource list */
        config_->clear_strlist("build", "resources");
        config_->clear_strlist("build", "rs file cnt");

        /* start at resource zero */
        resnum = 0;
        resnum_base = 0;

        /* start at the first resource file, which is the .gam file */
        cur_rs_file = 0;

        /* go through the top-level tree items */
        for (parent = TreeView_GetRoot(tree) ; parent != 0 ;
             parent = TreeView_GetNextSibling(tree, parent))
        {
            HTREEITEM cur;
            char buf[20];

            /* scan the children of this item */
            for (cur = TreeView_GetChild(tree, parent) ; cur != 0 ;
                 cur = TreeView_GetNextSibling(tree, cur))
            {
                TV_ITEM tvi;
                
                /* get the information on this item */
                tvi.mask = TVIF_TEXT;
                tvi.pszText = label;
                tvi.cchTextMax = sizeof(label);
                tvi.hItem = cur;
                TreeView_GetItem(tree, &tvi);
                
                /* save this item */
                config_->setval("build", "resources", resnum, label);

                /* on to the next resource */
                ++resnum;
            }

            /* set the resource count for this file */
            sprintf(buf, "%u", resnum - resnum_base);
            config_->setval("build", "rs file cnt", cur_rs_file, buf);

            /* on to the next file */
            ++cur_rs_file;

            /* note the index of the first resource in this file */
            resnum_base = resnum;
        }

        /* done */
        return TRUE;

    default:
        /* not handled */
        break;
    }
    
    /* inherit default behavior */
    return CHtmlDialogDebugPref::do_notify(nm, ctl);
}

/*
 *   process a command 
 */
int CHtmlDialogBuildSrc::do_command(WPARAM cmd, HWND ctl)
{
    OPENFILENAME info;
    char fname[1024];
    HTREEITEM sel;

    /* check the command */
    switch(LOWORD(cmd))
    {
    case IDC_FLD_SOURCE:
        /* if it's an EN_CHANGE notification, note the change */
        if (HIWORD(cmd) == EN_CHANGE)
        {
            set_changed(TRUE);
            return TRUE;
        }

        /* nothing special; inherit default */
        break;
        
    case IDC_BTN_BROWSE:
        /* browse for the main source file */
        browse_file("TADS Source Files\0*.t\0"
                    "TADS Library Files\0*.tl\0"
                    "All Files\0*.*\0\0",
                    "Select your game's main source file", 0, IDC_FLD_SOURCE,
                    OFN_HIDEREADONLY, TRUE, TRUE);

        /* handled */
        return TRUE;

    case IDC_BTN_ADDRS:
        /* get the current selection */
        sel = get_selected_rsfile();
        
        /* add a new .RSn file after the selected .RSn file */
        insert_tree(TVI_ROOT, sel, "External Resource File",
                    TVIS_BOLD | TVIS_EXPANDED);

        /* renumber the .RSn files */
        renum_rsn_files();

        /* set focus on the tree */
        SetFocus(GetDlgItem(handle_, IDC_FLD_RSC));

        /* handled */
        return TRUE;

    case IDC_BTN_REMOVE:
        /* delete the selected item */
        delete_tree_selection();

        /* set focus on the tree */
        SetFocus(GetDlgItem(handle_, IDC_FLD_RSC));

        /* handled */
        return TRUE;

    case IDC_BTN_ADDFILE:
        /* ask for a file to open */
        info.lStructSize = sizeof(info);
        info.hwndOwner = handle_;
        info.hInstance = CTadsApp::get_app()->get_instance();
        info.lpstrCustomFilter = 0;
        info.nFilterIndex = selector_filter_index_;
        info.lpstrFile = fname;
        fname[0] = '\0';
        info.nMaxFile = sizeof(fname);
        info.lpstrFileTitle = 0;
        info.nMaxFileTitle = 0;
        info.lpstrInitialDir = CTadsApp::get_app()->get_openfile_dir();
        info.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
        info.nFileOffset = 0;
        info.nFileExtension = 0;
        info.lpstrDefExt = 0;
        info.lCustData = 0;
        info.lpfnHook = 0;
        info.lpTemplateName = 0;
        info.lpstrTitle = "Select resource file or files to add";
        info.lpstrFilter = "JPEG Images\0*.jpg;*.jpeg;*.jpe\0"
                           "PNG Images\0*.png\0"
                           "Wave Sounds\0*.wav\0"
                           "MPEG Sounds\0*.mp3\0"
                           "MIDI Music\0*.mid;*.midi\0"
                           "All Files\0*.*\0\0";
        CTadsDialog::set_filedlg_center_hook(&info);
        
        /* allow multiple files to be selected */
        info.Flags |= OFN_ALLOWMULTISELECT | OFN_EXPLORER;

        /* ask for the file(s) */
        if (GetOpenFileName(&info))
        {
            char *p;
            HWND tree = GetDlgItem(handle_, IDC_FLD_RSC);
            HTREEITEM parent;

            /* remember the updated filter the user selected */
            selector_filter_index_ = info.nFilterIndex;

            /* find the selected .RSn file in the tree */
            parent = get_selected_rsfile();

            /* 
             *   if this is a lone filename, the dialog won't have
             *   inserted a null byte between the file and the path, so
             *   insert one of our own - this makes the two cases the same
             *   so that we won't need special handling in the loop 
             */
            if (info.nFileOffset != 0)
                fname[info.nFileOffset - 1] = '\0';
            
            /* 
             *   append each file (there may be many) to the resource
             *   list, each on a separate line 
             */
            for (p = fname + info.nFileOffset ; *p != '\0' ;
                 p += strlen(p) + 1)
            {
                char fullname[OSFNMAX];

                /* build the full name */
                strcpy(fullname, fname);
                
                /* 
                 *   add a slash if the path doesn't already end in a
                 *   slash 
                 */
                if (info.nFileOffset < 2
                    || fname[info.nFileOffset - 2] != '\\')
                    strcat(fullname, "\\");
                
                /* append the filename */
                strcat(fullname, p);

                /* add it to the list */
                append_tree(parent, fullname, 0);
            }
            
            /* note that we've changed the list */
            set_changed(TRUE);

            /* set the path for future open-file dialog calls */
            CTadsApp::get_app()->set_openfile_path(fname);
        }

        /* set focus on the tree */
        SetFocus(GetDlgItem(handle_, IDC_FLD_RSC));

        /* handled */
        return TRUE;

    case IDC_BTN_ADDDIR:
        /* start in the current open-file folder */
        if (CTadsApp::get_app()->get_openfile_dir() != 0
            && CTadsApp::get_app()->get_openfile_dir()[0] != '\0')
            strcpy(fname, CTadsApp::get_app()->get_openfile_dir());
        else
            GetCurrentDirectory(sizeof(fname), fname);
        
        /* run the folder selector dialog */
        if (CTadsDialogFolderSel2::
            run_select_folder(GetParent(handle_),
                              CTadsApp::get_app()->get_instance(),
                              "&Resource Folder:",
                              "Add Resource Folder", fname, sizeof(fname),
                              fname, 0, 0))
        {
            /* add the folder to the tree under the selected resource file */
            append_tree(get_selected_rsfile(), fname, 0);

            /* note that we've changed the list */
            set_changed(TRUE);

            /* use this as the next open-file directory */
            CTadsApp::get_app()->set_openfile_path(fname);
        }

        /* set focus on the tree */
        SetFocus(GetDlgItem(handle_, IDC_FLD_RSC));

        /* handled */
        return TRUE;

    case IDCANCEL:
        /* pass cancel to our parent */
        SendMessage(GetParent(handle_), WM_COMMAND, IDCANCEL, (LPARAM)ctl);
        return TRUE;

    default:
        /* not handled */
        break;
    }

    /* inherit default handling */
    return CHtmlDialogDebugPref::do_command(cmd, ctl);
}


/* ------------------------------------------------------------------------ */
/*
 *   Build Options - Includes page 
 */
class CHtmlDialogBuildInc: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogBuildInc(CHtmlSys_dbgmain *mainwin,
                        CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
};

/*
 *   initialize 
 */
void CHtmlDialogBuildInc::init()
{
    /* initialize the include path list */
    load_multiline_fld("build", "includes", IDC_FLD_INC);

    /* inherit default */
    CHtmlDialogDebugPref::init();
}

/*
 *   process a notification 
 */
int CHtmlDialogBuildInc::do_notify(NMHDR *nm, int ctl)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* save the new include path setting */
        save_multiline_fld("build", "includes", IDC_FLD_INC, TRUE);

        /* done */
        return TRUE;

    default:
        /* inherit default behavior */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }
}

/*
 *   process a command 
 */
int CHtmlDialogBuildInc::do_command(WPARAM cmd, HWND ctl)
{
    char fname[OSFNMAX];

    /* check the command */
    switch(LOWORD(cmd))
    {
    case IDC_FLD_INC:
        /* if it's an EN_CHANGE notification, note the change */
        if (HIWORD(cmd) == EN_CHANGE)
        {
            set_changed(TRUE);
            return TRUE;
        }
        
        /* nothing special; inherit default */
        break;

    case IDC_BTN_ADDDIR:
        /* start in the current open-file folder */
        if (CTadsApp::get_app()->get_openfile_dir() != 0
            && CTadsApp::get_app()->get_openfile_dir()[0] != '\0')
            strcpy(fname, CTadsApp::get_app()->get_openfile_dir());
        else
            GetCurrentDirectory(sizeof(fname), fname);

        /* run the folder selector dialog */
        if (CTadsDialogFolderSel2::
            run_select_folder(GetParent(handle_),
                              CTadsApp::get_app()->get_instance(),
                              "Folder to add to #include &path:",
                              "Add Folder to #include Search Path",
                              fname, sizeof(fname),
                              fname, 0, 0))
        {
            HWND fld;

            /* add the folder to our list */
            fld = GetDlgItem(handle_, IDC_FLD_INC);
            SendMessage(fld, EM_SETSEL, 32767, 32767);
            SendMessage(fld, EM_REPLACESEL, FALSE, (LPARAM)fname);

            /* add a newline */
            SendMessage(fld, EM_SETSEL, 32767, 32767);
            SendMessage(fld, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");

            /* use this as the next open-file directory */
            CTadsApp::get_app()->set_openfile_path(fname);
        }

        /* handled */
        return TRUE;

    case IDCANCEL:
        /* pass cancel to our parent */
        SendMessage(GetParent(handle_), WM_COMMAND, IDCANCEL, (LPARAM)ctl);
        return TRUE;

    default:
        /* it's not one of ours */
        break;
    }

    /* inherit default */
    return CHtmlDialogDebugPref::do_command(cmd, ctl);
}

/* ------------------------------------------------------------------------ */
/*
 *   Build Options - Outputs page 
 */
class CHtmlDialogBuildOut: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogBuildOut(CHtmlSys_dbgmain *mainwin,
                        CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
};

/*
 *   initialize 
 */
void CHtmlDialogBuildOut::init()
{
    /* 
     *   store the current game name - this field cannot be modified, since
     *   it's based on the name of the loaded project (.tdc/.t3c) file 
     */
    SetWindowText(GetDlgItem(handle_, IDC_FLD_GAM),
                  mainwin_->get_gamefile_name());

    /* load the release .gam and .exe filenames */
    load_field("build", "rlsgam", IDC_FLD_RLSGAM);
    load_field("build", "exe", IDC_FLD_EXE);

    /* inherit default */
    CHtmlDialogDebugPref::init();
}

/*
 *   process a notification 
 */
int CHtmlDialogBuildOut::do_notify(NMHDR *nm, int ctl)
{
    textchar_t buf[OSFNMAX];
    textchar_t buf2[OSFNMAX];
    
    switch(nm->code)
    {
    case PSN_APPLY:
        /* 
         *   make sure the release game and debug game files aren't the
         *   same file 
         */
        GetWindowText(GetDlgItem(handle_, IDC_FLD_RLSGAM), buf, sizeof(buf));
        GetWindowText(GetDlgItem(handle_, IDC_FLD_GAM), buf2, sizeof(buf2));
        if (stricmp(buf, buf2) == 0)
        {
            /* display an error message */
            MessageBox(GetParent(handle_),
                       "The release game and debug game file names "
                       "are identical; this is not allowed because the "
                       "files would overwrite each other.  "
                       "Please use different filenames.",
                       "TADS Debugger", MB_ICONEXCLAMATION | MB_OK);

            /* indicate that we cannot proceed */
            SetWindowLong(handle_, DWL_MSGRESULT,
                          PSNRET_INVALID_NOCHANGEPAGE);
            return TRUE;
        }
        
        /* save the release .GAM and .EXE file settings */
        mainwin_->vsn_change_pref_gamefile_name(buf2);
        save_field("build", "rlsgam", IDC_FLD_RLSGAM, TRUE);
        save_field("build", "exe", IDC_FLD_EXE, TRUE);

        /* done */
        return TRUE;

    default:
        /* inherit default behavior */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }
}

/*
 *   process a command 
 */
int CHtmlDialogBuildOut::do_command(WPARAM cmd, HWND ctl)
{
    /* check the command */
    switch(LOWORD(cmd))
    {
    case IDC_FLD_GAM:
    case IDC_FLD_EXE:
    case IDC_FLD_RLSGAM:
        /* if it's an EN_CHANGE notification, note the change */
        if (HIWORD(cmd) == EN_CHANGE)
        {
            set_changed(TRUE);
            return TRUE;
        }

        /* nothing special; inherit default */
        break;

    case IDC_BTN_BROWSE:
        /* browse for the release game file */
        browse_file(w32_opendlg_filter,
                    "Select the release compiled game file", w32_game_ext,
                    IDC_FLD_RLSGAM, OFN_HIDEREADONLY, FALSE, TRUE);
        return TRUE;
        
    case IDC_BTN_BROWSE2:
        /* browse for the .EXE file */
        browse_file("Applications\0*.exe\0All Files\0*.*\0\0",
                    "Select the application file name", "exe",
                    IDC_FLD_EXE, OFN_HIDEREADONLY, FALSE, TRUE);
        return TRUE;

    case IDC_BTN_BROWSE3:
        /* 
         *   browse for debug game file - supported only in t3 workbench, but
         *   we don't have to worry about disabling the code for tads 2 as
         *   we'll simply never get this request from the dialog 
         */
        browse_file(w32_opendlg_filter,
                    "Select the debug compiled game file", w32_game_ext,
                    IDC_FLD_GAM, OFN_HIDEREADONLY, FALSE, TRUE);
        return TRUE;

    default:
        /* it's not one of ours */
        break;
    }

    /* inherit default */
    return CHtmlDialogDebugPref::do_command(cmd, ctl);
}

/* ------------------------------------------------------------------------ */
/*
 *   Build Options - Defines page 
 */
class CHtmlDialogBuildDef: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogBuildDef(CHtmlSys_dbgmain *mainwin,
                        CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
};

/*
 *   "Add #define symbol" dialog
 */
class CTadsDialogBuildDefAdd: public CTadsDialog
{
public:
    /* run the dialog */
    static void run_dlg(HWND parent_hwnd)
    {
        CTadsDialogBuildDefAdd *dlg;

        /* create the dialog */
        dlg = new CTadsDialogBuildDefAdd(parent_hwnd);

        /* run the dialog */
        dlg->run_modal(DLG_ADD_MACRO, GetParent(parent_hwnd),
                       CTadsApp::get_app()->get_instance());

        /* we're done with the dialog; delete it */
        delete dlg;
    }

    /* initialize */
    void init()
    {
        /* 
         *   initially disable the "OK" button - it's not enabled until
         *   the macro name field contains some text 
         */
        EnableWindow(GetDlgItem(handle_, IDOK), FALSE);
    }

    /* process a command */
    int do_command(WPARAM id, HWND ctl)
    {
        switch(LOWORD(id))
        {
        case IDC_FLD_MACRO:
            if (HIWORD(id) == EN_CHANGE)
            {
                char buf[50];
                char *src;
                char *dst;
                int changed;
                
                /* 
                 *   the macro field is being changed - turn the "OK"
                 *   button on if the field is not empty, off if it is 
                 */
                GetWindowText(GetDlgItem(handle_, IDC_FLD_MACRO),
                              buf, sizeof(buf));

                /* remove any invalid characters */
                for (changed = FALSE, src = dst = buf ; *src != '\0' ; ++src)
                {
                    /* copy this character if it's valid in a macro name */
                    if (isalpha(*src) || *src == '_'
                        || (src > buf && isdigit(*src)))
                    {
                        /* it's valid - copy it */
                        *dst++ = *src;
                    }
                    else
                    {
                        /* note the change */
                        changed = TRUE;
                    }
                }

                /* null-terminate the result */
                *dst = '\0';

                /* copy it back into the control if we changed it */
                if (changed)
                    SetWindowText(GetDlgItem(handle_, IDC_FLD_MACRO), buf);

                /* enable the window if the name is not empty */
                EnableWindow(GetDlgItem(handle_, IDOK), dst > buf);

                /* handled */
                return TRUE;
            }

            /* not handled; pick up the default handling */
            break;
            
        case IDOK:
            /* 
             *   they're accepting the settings - add the macro definition
             *   to the parent dialog's macro list 
             */
            {
                HWND fld;
                char buf[256];

                /* get the parent field */
                fld = GetDlgItem(parent_hwnd_, IDC_FLD_DEFINES);

                /* add the macro name */
                GetWindowText(GetDlgItem(handle_, IDC_FLD_MACRO),
                              buf, sizeof(buf));
                SendMessage(fld, EM_SETSEL, 32767, 32767);
                SendMessage(fld, EM_REPLACESEL, FALSE, (LPARAM)buf);

                /* add the '=' */
                SendMessage(fld, EM_SETSEL, 32767, 32767);
                SendMessage(fld, EM_REPLACESEL, FALSE, (LPARAM)"=");

                /* add the expansion text */
                GetWindowText(GetDlgItem(handle_, IDC_FLD_MACRO_EXP),
                              buf, sizeof(buf));
                SendMessage(fld, EM_SETSEL, 32767, 32767);
                SendMessage(fld, EM_REPLACESEL, FALSE, (LPARAM)buf);

                /* add the newline */
                SendMessage(fld, EM_SETSEL, 32767, 32767);
                SendMessage(fld, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");
            }

            /* proceed to inherit the default processing */
            break;

        default:
            break;
        }

        /* inherit the default processing */
        return CTadsDialog::do_command(id, ctl);
    }

protected:
    CTadsDialogBuildDefAdd(HWND parent_hwnd)
    {
        /* remember the parent dialog window */
        parent_hwnd_ = parent_hwnd;
    }

    /* parent dialog window */
    HWND parent_hwnd_;
};


/*
 *   initialize 
 */
void CHtmlDialogBuildDef::init()
{
    /* load the "defines" list */
    load_multiline_fld("build", "defines", IDC_FLD_DEFINES);

    /* load the "undefines" list */
    load_multiline_fld("build", "undefs", IDC_FLD_UNDEF);

    /* inherit default */
    CHtmlDialogDebugPref::init();
}

/*
 *   process a notification 
 */
int CHtmlDialogBuildDef::do_notify(NMHDR *nm, int ctl)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* 
         *   If the list of defines is changing, reload the libraries.  This
         *   is necessary because libraries can refer to macro values; when
         *   macro values change, we need to reload the libraries to ensure
         *   that the loaded libraries reflect any changes that affect them.
         */
        if (multiline_fld_changed("build", "defines",
                                  IDC_FLD_DEFINES))
        {
            /* schedule the libraries for reloading when convenient */
            mainwin_->schedule_lib_reload();
        }

        /* save the "defines" and "undefs" lists */
        save_multiline_fld("build", "defines", IDC_FLD_DEFINES, FALSE);
        save_multiline_fld("build", "undefs", IDC_FLD_UNDEF, FALSE);

        /* done */
        return TRUE;

    default:
        /* inherit default behavior */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }
}

/*
 *   process a command 
 */
int CHtmlDialogBuildDef::do_command(WPARAM cmd, HWND ctl)
{
    /* check the command */
    switch(LOWORD(cmd))
    {
    case IDC_FLD_DEFINES:
    case IDC_FLD_UNDEF:
        /* if it's an EN_CHANGE notification, note the change */
        if (HIWORD(cmd) == EN_CHANGE)
        {
            set_changed(TRUE);
            return TRUE;
        }

        /* nothing special; inherit default */
        break;

    case IDC_BTN_ADD:
        /* run the #define dialog */
        CTadsDialogBuildDefAdd::run_dlg(handle_);
        return TRUE;

    case IDCANCEL:
        /* pass cancel to our parent */
        SendMessage(GetParent(handle_), WM_COMMAND, IDCANCEL, (LPARAM)ctl);
        return TRUE;

    default:
        /* it's not one of ours */
        break;
    }

    /* inherit default */
    return CHtmlDialogDebugPref::do_command(cmd, ctl);
}

/* ------------------------------------------------------------------------ */
/*
 *   Build Options - Diagnostics (errors) page 
 */
class CHtmlDialogBuildErr: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogBuildErr(CHtmlSys_dbgmain *mainwin,
                        CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
};

/*
 *   initialize 
 */
void CHtmlDialogBuildErr::init()
{
    int level;

    /* load the "verbose" checkbox */
    load_ckbox("build", "verbose errors", IDC_CK_VERBOSE, FALSE);

    /* get the warning level setting */
    if (config_->getval("build", "warning level", &level))
        level = 1;

    /* check the appropriate warning level button */
    CheckRadioButton(handle_, IDC_RB_WARN_NONE, IDC_RB_WARN_PEDANTIC,
                     IDC_RB_WARN_NONE + level);

    /* inherit default */
    CHtmlDialogDebugPref::init();
}

/*
 *   process a notification 
 */
int CHtmlDialogBuildErr::do_notify(NMHDR *nm, int ctl)
{
    int level;

    switch(nm->code)
    {
    case PSN_APPLY:
        /* save the "verbose" setting */
        save_ckbox("build", "verbose errors", IDC_CK_VERBOSE);

        /* save the warning level setting */
        if (IsDlgButtonChecked(handle_, IDC_RB_WARN_NONE))
            level = 0;
        else if (IsDlgButtonChecked(handle_, IDC_RB_WARN_STANDARD))
            level = 1;
        else
            level = 2;
        config_->setval("build", "warning level", level);

        /* done */
        return TRUE;

    default:
        /* inherit default behavior */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }
}

/*
 *   process a command 
 */
int CHtmlDialogBuildErr::do_command(WPARAM cmd, HWND ctl)
{
    /* check the command */
    switch(LOWORD(cmd))
    {
    case IDC_CK_VERBOSE:
    case IDC_RB_WARN_NONE:
    case IDC_RB_WARN_STANDARD:
    case IDC_RB_WARN_PEDANTIC:
        /* note the change */
        set_changed(TRUE);
        return TRUE;

    default:
        /* it's not one of ours */
        break;
    }

    /* inherit default */
    return CHtmlDialogDebugPref::do_command(cmd, ctl);
}



/* ------------------------------------------------------------------------ */
/*
 *   Build Options - Intermediate Files page
 */
class CHtmlDialogBuildInter: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogBuildInter(CHtmlSys_dbgmain *mainwin,
                          CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
};

/*
 *   initialize 
 */
void CHtmlDialogBuildInter::init()
{
    /* load the symbol and object file directory fields */
    load_field("build", "symfile path", IDC_FLD_SYMDIR);
    load_field("build", "objfile path", IDC_FLD_OBJDIR);

    /* inherit default */
    CHtmlDialogDebugPref::init();
}

/*
 *   process a notification 
 */
int CHtmlDialogBuildInter::do_notify(NMHDR *nm, int ctl)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* save the directory path fields */
        save_field("build", "symfile path", IDC_FLD_SYMDIR, TRUE);
        save_field("build", "objfile path", IDC_FLD_OBJDIR, TRUE);

        /* done */
        return TRUE;

    default:
        /* inherit default behavior */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }
}

/*
 *   process a command 
 */
int CHtmlDialogBuildInter::do_command(WPARAM cmd, HWND ctl)
{
    char fname[OSFNMAX];
    int fld;
    const char *prompt;
    const char *title;

    /* check the command */
    switch(LOWORD(cmd))
    {
    case IDC_FLD_SYMDIR:
    case IDC_FLD_OBJDIR:
        /* if it's an EN_CHANGE notification, note the change */
        if (HIWORD(cmd) == EN_CHANGE)
        {
            set_changed(TRUE);
            return TRUE;
        }

        /* nothing special; inherit default */
        break;

    case IDC_BTN_BROWSE:
        /* browse for the symbol file directory */
        fld = IDC_FLD_SYMDIR;
        prompt = "Folder for symbol files:";
        title = "Select Symbol File Folder";
        goto browse_for_folder;

    case IDC_BTN_BROWSE2:
        /* browse for the object file directory */
        fld = IDC_FLD_OBJDIR;
        prompt = "Folder for object files:";
        title = "Select Object File Folder";

    browse_for_folder:
        /* 
         *   get the current setting; if there isn't a current setting, start
         *   browsing in the current open-file folder 
         */
        GetDlgItemText(handle_, fld, fname, sizeof(fname));
        if (fname[0] == '\0')
        {
            /* start in the current open-file folder */
            if (CTadsApp::get_app()->get_openfile_dir() != 0
                && CTadsApp::get_app()->get_openfile_dir()[0] != '\0')
                strcpy(fname, CTadsApp::get_app()->get_openfile_dir());
            else
                GetCurrentDirectory(sizeof(fname), fname);
        }

        /* browse for a symbol file directory */
        if (CTadsDialogFolderSel2::
            run_select_folder(GetParent(handle_),
                              CTadsApp::get_app()->get_instance(),
                              prompt, title,
                              fname, sizeof(fname), fname, 0, 0))
        {
            /* store the value */
            SetDlgItemText(handle_, fld, fname);
        }
            
        /* handled */
        return TRUE;

    case IDCANCEL:
        /* pass cancel to our parent */
        SendMessage(GetParent(handle_), WM_COMMAND, IDCANCEL, (LPARAM)ctl);
        return TRUE;

    default:
        /* it's not one of ours */
        break;
    }

    /* inherit default */
    return CHtmlDialogDebugPref::do_command(cmd, ctl);
}



/* ------------------------------------------------------------------------ */
/*
 *   Build Options - Advanced Options page 
 */
class CHtmlDialogBuildAdv: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogBuildAdv(CHtmlSys_dbgmain *mainwin,
                        CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
};

/*
 *   initialize 
 */
void CHtmlDialogBuildAdv::init()
{
    /* load the checkboxes */
    load_ckbox("build", "ignore case", IDC_CK_CASE, FALSE);
    load_ckbox("build", "c ops", IDC_CK_C_OPS, FALSE);
    load_ckbox("build", "use charmap", IDC_CK_CHARMAP, FALSE);
    load_ckbox("build", "run preinit", IDC_CK_PREINIT, TRUE);
    load_ckbox("build", "keep default modules", IDC_CK_DEFMOD, TRUE);

    /* load the character mapping file field */
    load_field("build", "charmap", IDC_FLD_CHARMAP);

    /* load the extra options field */
    load_multiline_fld("build", "extra options", IDC_FLD_OPTS);

    /* inherit default */
    CHtmlDialogDebugPref::init();
}

/*
 *   process a notification 
 */
int CHtmlDialogBuildAdv::do_notify(NMHDR *nm, int ctl)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* save the checkboxes */
        save_ckbox("build", "ignore case", IDC_CK_CASE);
        save_ckbox("build", "c ops", IDC_CK_C_OPS);
        save_ckbox("build", "use charmap", IDC_CK_CHARMAP);
        save_ckbox("build", "run preinit", IDC_CK_PREINIT);
        save_ckbox("build", "keep default modules", IDC_CK_DEFMOD);

        /* load the character mapping file field */
        save_field("build", "charmap", IDC_FLD_CHARMAP, TRUE);

        /* load the extra options field */
        save_multiline_fld("build", "extra options", IDC_FLD_OPTS, FALSE);

        /* done */
        return TRUE;

    default:
        /* inherit default behavior */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }
}

/*
 *   process a command 
 */
int CHtmlDialogBuildAdv::do_command(WPARAM cmd, HWND ctl)
{
    char buf[OSFNMAX];
    char *rootname;
    
    /* check the command */
    switch(LOWORD(cmd))
    {
    case IDC_FLD_CHARMAP:
    case IDC_FLD_OPTS:
        /* if it's an EN_CHANGE notification, note the change */
        if (HIWORD(cmd) == EN_CHANGE)
        {
            set_changed(TRUE);
            return TRUE;
        }

        /* nothing special; inherit default */
        break;

    case IDC_CK_CASE:
    case IDC_CK_C_OPS:
    case IDC_CK_CHARMAP:
    case IDC_CK_PREINIT:
    case IDC_CK_DEFMOD:
        /* note the change */
        set_changed(TRUE);
        return TRUE;

    case IDC_BTN_HELP:
        /* look for the help file in the executable directory */
        GetModuleFileName(0, buf, sizeof(buf));
        rootname = os_get_root_name(buf);
        strcpy(rootname, "helptc.htm");

        /* if it's not there, look in the doc/wb directory */
        if (GetFileAttributes(buf) == 0xffffffff)
            strcpy(rootname, "doc\\wb\\helptc.htm");
        
        /* display the help file for the compiler options */
        if ((unsigned long)ShellExecute(0, "open", buf, 0, 0, 0) <= 32)
            MessageBox(GetParent(handle_),
                       "Couldn't open help.  You must have a web "
                       "browser or HTML viewer installed to view help.",
                       "TADS Workbench", MB_OK | MB_ICONEXCLAMATION);

        /* handled */
        return TRUE;

    case IDC_BTN_BROWSE:
        /* browse for a character mapping file */
        browse_file("TADS Character Maps\0*.tcp\0All Files\0*.*\0\0",
                    "Select a TADS Character Mapping file", 0,
                    IDC_FLD_CHARMAP, OFN_HIDEREADONLY, TRUE, FALSE);

        /* handled */
        return TRUE;

    case IDCANCEL:
        /* pass cancel to our parent */
        SendMessage(GetParent(handle_), WM_COMMAND, IDCANCEL, (LPARAM)ctl);
        return TRUE;

    default:
        /* it's not one of ours */
        break;
    }

    /* inherit default */
    return CHtmlDialogDebugPref::do_command(cmd, ctl);
}

/* ------------------------------------------------------------------------ */
/*
 *   Build Options - Installer page
 */
class CHtmlDialogBuildIns: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogBuildIns(CHtmlSys_dbgmain *mainwin,
                        CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
};

/* structure associating a field with an installer option */
struct insopt_fldinfo_t
{
    /* installer option name for the field */
    const textchar_t *nm;

    /* field control ID in the dialog */
    int id;

    /* information string ID */
    int strid;
};

/* list of field associations */
static const insopt_fldinfo_t insopt_flds[] =
{
    { "name", IDC_FLD_DISPNAME, IDS_INFO_DISPNAME },
    { "savext", IDC_FLD_SAVE_EXT, IDS_INFO_SAVE_EXT },
    { "icon", IDC_FLD_ICON, IDS_INFO_ICON },
    { "license", IDC_FLD_LICENSE, IDS_INFO_LICENSE },
    { "progdir", IDC_FLD_PROGDIR, IDS_INFO_PROGDIR },
    { "startfolder", IDC_FLD_STARTFOLDER, IDS_INFO_STARTFOLDER },
    { "readme", IDC_FLD_README, IDS_INFO_README },
    { "readme_title", IDC_FLD_README_TITLE, IDS_INFO_README_TITLE },
    { "charlib", IDC_FLD_CHARLIB, IDS_INFO_CHARLIB },
    { 0, 0, IDS_INFO_NONE }
};

/*
 *   Installer Options dialog
 */
class CTadsDialogInsOpts: public CTadsDialog
{
public:
    /* run the dialog */
    static void run_dlg(HWND parent)
    {
        CTadsDialogInsOpts *dlg;

        /* create the dialog */
        dlg = new CTadsDialogInsOpts(parent);

        /* run the dialog */
        dlg->run_modal(DLG_INSTALL_OPTIONS, GetParent(parent),
                       CTadsApp::get_app()->get_instance());

        /* we're done with the dialog; delete it */
        delete dlg;
    }

    /* 
     *   Parse the free-form text in the parent list, and either load it
     *   into our structured fields or store our structured fields back
     *   into the free-form text list.  If 'loading' is true, we'll load
     *   the free-form list into our fields; otherwise we'll store our
     *   fields back into the free-form list.  
     */
    void parse_list(int loading)
    {
        HWND fld;
        const insopt_fldinfo_t *fldp;
        int i;
        int cnt;
        char field_found[sizeof(insopt_flds)/sizeof(insopt_flds[0]) - 1];

        /* get the parent field with our initial data */
        fld = GetDlgItem(parent_, IDC_FLD_INSTALL_OPTS);

        /* get the number of lines in the parent field */
        cnt = SendMessage(fld, EM_GETLINECOUNT, 0, 0);

        /* we haven't found any of the fields yet */
        memset(field_found, 0, sizeof(field_found));

        /* 
         *   Load the lines.  For each line, find the installer option
         *   name; if we find it, load the value into the corresponding
         *   field in this dialog.  
         */
        for (i = 0 ; i < cnt ; ++i)
        {
            char buf[512];
            size_t len;
            char *p;

            /* get this line */
            *(WORD *)buf = (WORD)sizeof(buf) - 1;
            len = (size_t)SendMessage(fld, EM_GETLINE,
                                      (WPARAM)i, (LPARAM)buf);

            /* null-terminate the buffer */
            buf[len] = '\0';

            /* find the '=', if any */
            for (p = buf ; *p != '=' && *p != '\0' ; ++p) ;
            len = p - buf;

            /* if we found the '=', skip it to get the value */
            if (*p == '=')
            {
                /* we got a variable - skip the delimiting '=' */
                ++p;
            }
            else
            {
                /* there's no '=' - ignore this line */
                continue;
            }

            /* find the variable name in our list */
            for (fldp = insopt_flds ; fldp->nm != 0 ; ++fldp)
            {
                /* check for a match */
                if (len == strlen(fldp->nm)
                    && memicmp(fldp->nm, buf, len) == 0)
                {
                    /* mark this field as found in our list */
                    field_found[fldp - insopt_flds] = TRUE;
                    
                    /* we found it - stop looking */
                    break;
                }
            }

            /*
             *   If we found this string in our field list, load it or
             *   store it, as appropriate 
             */
            if (fldp->nm != 0)
            {
                if (loading)
                {
                    /* set our field from the free-form list */
                    SetWindowText(GetDlgItem(handle_, fldp->id), p);
                }
                else
                {
                    int start;
                    int linelen;
                    char val[512];
                    
                    /* 
                     *   store our field value in the list - get the
                     *   limits of the current line 
                     */
                    start = (int)SendMessage(fld, EM_LINEINDEX, (WPARAM)i, 0);
                    linelen = (int)SendMessage(fld, EM_LINELENGTH,
                                               (WPARAM)start, 0);

                    /* get the new value from the field */
                    GetWindowText(GetDlgItem(handle_, fldp->id),
                                  val, sizeof(val));

                    /* 
                     *   if the field is empty, delete the line;
                     *   otherwise, replace the value 
                     */
                    if (strlen(val) == 0)
                    {
                        /* select the entire line, plus the CR-LF */
                        SendMessage(fld, EM_SETSEL, (WPARAM)start,
                                    (LPARAM)(start + linelen + 2));

                        /* back up a line in our counting */
                        --i;
                        --cnt;
                    }
                    else
                    {
                        /* select the part of the line after the '=' */
                        SendMessage(fld, EM_SETSEL, (WPARAM)(start + len + 1),
                                    (LPARAM)(start + linelen));
                    }

                    /* replace the value part with the field's new value */
                    SendMessage(fld, EM_REPLACESEL, FALSE, (LPARAM)val);
                }
            }
        }

        /*
         *   If we're saving, look through our fields for any that were
         *   not in the original list.  We need to add a new line for each
         *   field that has a value in our dialog but did not exist in the
         *   original free-form list. 
         */
        if (!loading)
        {
            /* scan the defined fields */
            for (i = 0, fldp = insopt_flds ;
                 i < sizeof(field_found)/sizeof(field_found[0]) ;
                 ++i, ++fldp)
            {
                /* 
                 *   if this field was not found, check for a value in the
                 *   text field 
                 */
                if (!field_found[i])
                {
                    char val[512];
                    size_t len;
                    
                    /* build the start of the "name=value" string */
                    len = strlen(fldp->nm);
                    memcpy(val, fldp->nm, len);
                    val[len++] = '=';

                    /* get the dialog field value */
                    GetWindowText(GetDlgItem(handle_, fldp->id),
                                  val + len, sizeof(val) - len);

                    /* if it's not empty, add a line for it */
                    if (strlen(val + len) != 0)
                    {
                        int has_newline;
                        int linecnt;

                        /* get the line count */
                        linecnt =
                            (int)SendMessage(fld, EM_GETLINECOUNT, 0, 0);

                        /* get the last line */
                        if (linecnt > 0)
                        {
                            size_t line_len;
                            int line_idx;

                            /* get the index of the last line */
                            line_idx = (int)SendMessage(fld, EM_LINEINDEX,
                                (WPARAM)(linecnt - 1), 0);

                            /* get the length of the last line */
                            line_len = (size_t)SendMessage(fld, EM_LINELENGTH,
                                (WPARAM)line_idx, 0);

                            /* 
                             *   If there's a trailing newline, the last
                             *   line will be empty.  Note whether this is
                             *   the case.  
                             */
                            has_newline = (line_len == 0);
                        }
                        else
                        {
                            /* there are no lines -> there's no newline */
                            has_newline = FALSE;
                        }
                        
                        /* go to the end of the free-form field */
                        SendMessage(fld, EM_SETSEL, 32767, 32767);

                        /* if necessary, add a newline */
                        if (!has_newline)
                        {
                            SendMessage(fld, EM_REPLACESEL, FALSE,
                                        (LPARAM)"\r\n");
                            SendMessage(fld, EM_SETSEL, 32767, 32767);
                        }

                        /* add our new line's text */
                        SendMessage(fld, EM_REPLACESEL, FALSE, (LPARAM)val);
                    }
                }
            }
        }
    }

    /* initialize */
    void init()
    {
        /* load the free-form list into our fields */
        parse_list(TRUE);
    }

    /* process a command */
    int do_command(WPARAM id, HWND ctl)
    {
        char buf[512];
        int strid;
        const insopt_fldinfo_t *fldp;
        OPENFILENAME info;
        
        /* check for special notification commands */
        switch(HIWORD(id))
        {
        case EN_SETFOCUS:
            /* find this field in our list */
            for (fldp = insopt_flds ; fldp->nm != 0 ; ++fldp)
            {
                /* if this is our field, stop looking */
                if (fldp->id == LOWORD(id))
                    break;
            }

            /* get this item's info string ID */
            strid = fldp->strid;

        set_info_msg:
            /* load the string */
            LoadString(CTadsApp::get_app()->get_instance(), strid,
                       buf, sizeof(buf));

            /* display the string in the info field */
            SetWindowText(GetDlgItem(handle_, IDC_FLD_INFO), buf);

            /* message handled */
            return TRUE;

        case EN_KILLFOCUS:
            /* leaving a text field - restore default message */
            strid = IDS_INFO_NONE;
            goto set_info_msg;
        }

        /* check which control is involved */
        switch(id)
        {
        case IDOK:
            /* save the updated values back into the parent dialog */
            parse_list(FALSE);
            
            /* proceed to inherit the default processing */
            break;

        case IDC_BTN_BROWSE:
        case IDC_BTN_BROWSE2:
        case IDC_BTN_BROWSE3:
        case IDC_BTN_BROWSE4:
            /* browse for an icon, license text, or readme file */
            info.lStructSize = sizeof(info);
            info.hwndOwner = handle_;
            info.hInstance = CTadsApp::get_app()->get_instance();
            info.lpstrCustomFilter = 0;
            info.nFilterIndex = 0;
            info.lpstrFile = buf;
            buf[0] = '\0';
            info.nMaxFile = sizeof(buf);
            info.lpstrFileTitle = 0;
            info.nMaxFileTitle = 0;
            info.lpstrInitialDir = CTadsApp::get_app()->get_openfile_dir();
            info.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST
                         | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
            info.nFileOffset = 0;
            info.nFileExtension = 0;
            info.lpstrDefExt = 0;
            info.lCustData = 0;
            info.lpfnHook = 0;
            info.lpTemplateName = 0;
            CTadsDialog::set_filedlg_center_hook(&info);

            switch(id)
            {
            case IDC_BTN_BROWSE:
                /* browsing for an icon */
                info.lpstrTitle = "Select an Icon File";
                info.lpstrFilter = "Icons\0*.ICO\0"
                                   "All Files\0*.*\0\0";
                break;

            case IDC_BTN_BROWSE2:
                /* browse for a license text file */
                info.lpstrTitle = "Select a License Text File";
                info.lpstrFilter = "Text Files\0*.TXT\0"
                                   "All Files\0*.*\0\0";
                break;

            case IDC_BTN_BROWSE3:
                /* browse for a read-me file */
                info.lpstrTitle = "Select a ReadMe File";
                info.lpstrFilter = "HTML Files\0*.htm;*.html\0"
                                   "Text Files\0*.txt\0"
                                   "All Files\0*.*\0\0";
                break;

            case IDC_BTN_BROWSE4:
                /* browse for a character map library */
                info.lpstrTitle = "Select Character Map Library";
                info.lpstrFilter = "Character Map Libraries\0*.t3r\0"
                                   "All Files\0*.*\0\0";
                break;
            }

            /* ask for the file */
            if (GetOpenFileName(&info))
            {
                /* copy the new value into the appropriate field */
                switch(id)
                {
                case IDC_BTN_BROWSE:
                    /* set the icon field */
                    SetWindowText(GetDlgItem(handle_, IDC_FLD_ICON), buf);
                    break;

                case IDC_BTN_BROWSE2:
                    /* set the license field */
                    SetWindowText(GetDlgItem(handle_, IDC_FLD_LICENSE), buf);
                    break;

                case IDC_BTN_BROWSE3:
                    /* set the readme field */
                    SetWindowText(GetDlgItem(handle_, IDC_FLD_README), buf);
                    break;

                case IDC_BTN_BROWSE4:
                    /* set the character map library field */
                    SetWindowText(GetDlgItem(handle_, IDC_FLD_CHARLIB), buf);
                    break;
                }

                /* update the open-file directory */
                CTadsApp::get_app()->set_openfile_dir(buf);
            }

            /* handled */
            return TRUE;

        default:
            break;
        }

        /* inherit the default processing */
        return CTadsDialog::do_command(id, ctl);
    }

protected:
    CTadsDialogInsOpts(HWND parent)
    {
        /* remember the parent dialog */
        parent_ = parent;
    }

    /* parent editor dialog window */
    HWND parent_;
};


/*
 *   initialize 
 */
void CHtmlDialogBuildIns::init()
{
    /* initialize the installer program name */
    load_field("build", "installer prog", IDC_FLD_INSTALL_EXE);

    /* initialize the installer option list */
    load_multiline_fld("build", "installer options", IDC_FLD_INSTALL_OPTS);

    /* inherit default */
    CHtmlDialogDebugPref::init();
}

/*
 *   process a notification 
 */
int CHtmlDialogBuildIns::do_notify(NMHDR *nm, int ctl)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* save the new installer executable */
        save_field("build", "installer prog", IDC_FLD_INSTALL_EXE, TRUE);

        /* save the new installer option list */
        save_multiline_fld("build", "installer options",
                           IDC_FLD_INSTALL_OPTS, FALSE);

        /* done */
        return TRUE;

    default:
        /* inherit default behavior */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }
}

/*
 *   process a command 
 */
int CHtmlDialogBuildIns::do_command(WPARAM cmd, HWND ctl)
{
    /* check the command */
    switch(LOWORD(cmd))
    {
    case IDC_FLD_INSTALL_EXE:
    case IDC_FLD_INSTALL_OPTS:
        /* if it's an EN_CHANGE notification, note the change */
        if (HIWORD(cmd) == EN_CHANGE)
        {
            set_changed(TRUE);
            return TRUE;
        }

        /* nothing special; inherit default */
        break;

    case IDC_BTN_BROWSE:
        /* browse for the setup executable */
        browse_file("Applications\0*.exe\0All Files\0*.*\0",
                    "Select the name of the installer executable to create",
                    "exe", IDC_FLD_INSTALL_EXE, OFN_HIDEREADONLY,
                    FALSE, TRUE);

        /* handled */
        return TRUE;

    case IDC_BTN_EDIT:
        /* run the edit-installer-options dialog */
        CTadsDialogInsOpts::run_dlg(handle_);

        /* handled */
        return TRUE;

    case IDCANCEL:
        /* pass cancel to our parent */
        SendMessage(GetParent(handle_), WM_COMMAND, IDCANCEL, (LPARAM)ctl);
        return TRUE;

    default:
        /* it's not one of ours */
        break;
    }

    /* inherit default */
    return CHtmlDialogDebugPref::do_command(cmd, ctl);
}

/* ------------------------------------------------------------------------ */
/*
 *   Run the build options dialog
 */
void run_dbg_build_dlg(HWND owner, CHtmlSys_dbgmain *mainwin,
                       int init_page_id, int init_ctl_id)
{
    PROPSHEETPAGE psp[16];
    PROPSHEETHEADER psh;
    CHtmlDialogBuildSrc *src_dlg = 0;
    CHtmlDialogBuildInc *inc_dlg;
    CHtmlDialogBuildOut *out_dlg;
    CHtmlDialogBuildDef *def_dlg;
    CHtmlDialogBuildErr *err_dlg = 0;
    CHtmlDialogBuildAdv *adv_dlg;
    CHtmlDialogBuildInter *int_dlg = 0;
    CHtmlDialogBuildIns *ins_dlg;
    int i;
    void *ctx;
    int inited = FALSE;

    /* 
     *   Create our pages.  Note that there is no need for a source page
     *   if the debugger uses the "project" window, since the project
     *   window supersedes the source page. 
     */
    if (mainwin->get_proj_win() == 0)
        src_dlg = new CHtmlDialogBuildSrc(mainwin, mainwin->get_local_config(),
                                          &inited);
    inc_dlg = new CHtmlDialogBuildInc(mainwin, mainwin->get_local_config(),
                                      &inited);
    if (w32_system_major_vsn >= 3)
        int_dlg = new CHtmlDialogBuildInter(mainwin,
                                            mainwin->get_local_config(),
                                            &inited);
    out_dlg = new CHtmlDialogBuildOut(mainwin, mainwin->get_local_config(),
                                      &inited);
    def_dlg = new CHtmlDialogBuildDef(mainwin, mainwin->get_local_config(),
                                      &inited);

    if (w32_system_major_vsn >= 3)
        err_dlg = new CHtmlDialogBuildErr(mainwin, mainwin->get_local_config(),
                                          &inited);
    adv_dlg = new CHtmlDialogBuildAdv(mainwin, mainwin->get_local_config(),
                                      &inited);
    ins_dlg = new CHtmlDialogBuildIns(mainwin, mainwin->get_local_config(),
                                      &inited);

    /* set up the main dialog descriptor */
    psh.dwSize = PROPSHEETHEADER_V1_SIZE;
    psh.dwFlags = PSH_USEICONID | PSH_PROPSHEETPAGE;
    psh.hwndParent = owner;
    psh.hInstance = CTadsApp::get_app()->get_instance();
    psh.pszIcon = 0;
    psh.pszCaption = (LPSTR)"Build Settings";
    psh.nStartPage = 0;
    psh.ppsp = (LPCPROPSHEETPAGE)&psp;
    psh.pfnCallback = 0;

    /* set up the property page descriptors */
    i = 0;
    if (src_dlg != 0)
        src_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                              &psp[i++], DLG_BUILD_SRC, IDS_BUILD_SRC,
                              init_page_id, init_ctl_id, &psh);
    inc_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                          &psp[i++], DLG_BUILD_INC, IDS_BUILD_INC, 
                          init_page_id, init_ctl_id, &psh);
    if (int_dlg != 0)
        int_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                              &psp[i++], DLG_BUILD_INTERMEDIATE,
                              IDS_BUILD_INTERMEDIATE,
                              init_page_id, init_ctl_id, &psh);
    out_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                          &psp[i++], DLG_BUILD_OUTPUT, IDS_BUILD_OUTPUT,
                          init_page_id, init_ctl_id, &psh);
    def_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                          &psp[i++], DLG_BUILD_DEFINES, IDS_BUILD_DEFINES,
                          init_page_id, init_ctl_id, &psh);
    if (err_dlg != 0)
        err_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                              &psp[i++], DLG_BUILD_DIAGNOSTICS,
                              IDS_BUILD_DIAGNOSTICS,
                              init_page_id, init_ctl_id, &psh);
    adv_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                          &psp[i++], DLG_BUILD_ADVANCED, IDS_BUILD_ADVANCED,
                          init_page_id, init_ctl_id, &psh);
    ins_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                          &psp[i++], DLG_BUILD_INSTALL, IDS_BUILD_INSTALL,
                          init_page_id, init_ctl_id, &psh);

    /* set the number of pages */
    psh.nPages = i;

    /* disable windows owned by the main window before showing the dialog */
    ctx = CTadsDialog::modal_dlg_pre(mainwin->get_handle(), FALSE);

    /* run the property sheet */
    PropertySheet(&psh);

    /* re-enable windows */
    CTadsDialog::modal_dlg_post(ctx);

    /* delete the dialogs */
    if (src_dlg != 0)
        delete src_dlg;
    delete inc_dlg;
    if (err_dlg != 0)
        delete err_dlg;
    if (int_dlg != 0)
        delete int_dlg;
    delete out_dlg;
    delete def_dlg;
    delete adv_dlg;
    delete ins_dlg;
}


/* ------------------------------------------------------------------------ */
/*
 *   Set default debugger values for a configuration 
 */
void set_dbg_defaults(CHtmlSys_dbgmain *dbgmain)
{
    CHtmlDebugConfig *config = dbgmain->get_global_config();
    textchar_t buf[OSFNMAX];

    /* set the default editor to "notepad */
    if (config->getval("editor", "program", 0, buf, sizeof(buf))
        || strlen(buf) == 0)
        config->setval("editor", "program", 0, "Notepad.EXE");

    /* set the default command line to "%f" */
    if (config->getval("editor", "cmdline", 0, buf, sizeof(buf))
        || strlen(buf) == 0)
        config->setval("editor", "cmdline", 0, "%f");
}

/* ------------------------------------------------------------------------ */
/*
 *   Set default build values for a configuration 
 */
void set_dbg_build_defaults(CHtmlSys_dbgmain *dbgmain,
                            const textchar_t *gamefile)
{
    CHtmlDebugConfig *config = dbgmain->get_local_config();
    char buf[OSFNMAX + 50];
    int val;

    /* 
     *   check to see if we've already applied the defaults; if so, don't
     *   apply them again, because the user might have deleted or modified
     *   values we previously stored as defaults 
     */
    if (!config->getval("build", "defaults set", &val) && val)
        return;

    /* set version-specific defaults */
    vsn_set_dbg_build_defaults(config, dbgmain, gamefile);

    /*
     *   Set the default .EXE file to be the same as the game name, with
     *   the .GAM replaced by .EXE 
     */
    if (gamefile != 0)
    {
        strcpy(buf, gamefile);
        os_remext(buf);
        os_addext(buf, "exe");
    }
    else
        buf[0] = '\0';

    /* 
     *   set the name - use the root name, so that it ends up in the same
     *   directory as the project file 
     */
    config->setval("build", "exe", 0, os_get_root_name(buf));

    /* set the default release game to the name of debug game plus "_rls" */
    if (gamefile != 0)
    {
        strcpy(buf, gamefile);
        os_remext(buf);
        strcat(buf, "_rls");
        os_addext(buf, w32_game_ext);
    }
    else
        buf[0] = '\0';

    /* set the root name */
    config->setval("build", "rlsgam", 0, os_get_root_name(buf));

    /* add the default installer options */
    if (gamefile != 0)
    {
        /* set the name of the installer */
        strcpy(buf, gamefile);
        os_remext(buf);
        strcat(buf, "_Setup");
        os_addext(buf, "exe");
        config->setval("build", "installer prog", 0,
                       os_get_root_name(buf));

        /* set the game name to that of the game file, minus the extension */
        sprintf(buf, "name=%s", os_get_root_name((char *)gamefile));
        os_remext(buf + 5);
        config->setval("build", "installer options", 0, buf);

        /* 
         *   set the default saved game extension to the name of the game,
         *   minus its extension 
         */
        sprintf(buf, "savext=%s", os_get_root_name((char *)gamefile));
        os_remext(buf + 5);
        config->setval("build", "installer options", 1, buf);
    }
    
    /* 
     *   make a note that we've set the default configuration, so that we
     *   don't try to set it again in the future 
     */
    config->setval("build", "defaults set", TRUE);
}

/* ------------------------------------------------------------------------ */
/*
 *   Clear build information from a configuration 
 */
void clear_dbg_build_info(CHtmlDebugConfig *config)
{
    config->clear_strlist("build", "source");
    config->clear_strlist("build", "resources");
    config->clear_strlist("build", "includes");
    config->clear_strlist("build", "exe");
    config->clear_strlist("build", "rlsgam");
    config->clear_strlist("build", "defines");
    config->clear_strlist("build", "undefs");
    config->setval("build", "ignore case", FALSE);
    config->setval("build", "c ops", FALSE);
    config->setval("build", "use charmap", FALSE);
    config->clear_strlist("build", "charmap");
    config->clear_strlist("build", "extra options");
    config->clear_strlist("build", "installer options");
    config->clear_strlist("build", "installer prog");
    config->setval("build", "defaults set", FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   Base class for new game wizard dialogs - implementation
 */

int CHtmlNewWizPage::do_notify(NMHDR *nm, int ctl)
{
    switch(nm->code)
    {
    case PSN_SETACTIVE:
        /* by default, turn on the 'back' and 'next' buttons */
        SendMessage(GetParent(handle_), PSM_SETWIZBUTTONS, 0,
                    PSWIZB_BACK | PSWIZB_NEXT);
        return TRUE;
        
    default:
        return CTadsDialogPropPage::do_notify(nm, ctl);
    }
}

/* initialize */
void CHtmlNewWizPage::init()
{
    /* if we haven't yet done so, center the dialog */
    if (!wiz_->inited_)
    {
        /* center the parent dialog */
        center_dlg_win(GetParent(handle_));
        
        /* note that we've done this so that other pages don't */
        wiz_->inited_ = TRUE;
    }
    
    /* inherit default */
    CTadsDialog::init();
}

/*
 *   Browse for a file to save.  Fills in the string buffer with the new
 *   filename and returns true if successful; returns false if the user
 *   cancels the dialog without selecting anything.  
 */
int CHtmlNewWizPage::browse_file(const char *filter, CStringBuf *filename,
                                 const char *prompt, const char *defext,
                                 int fld_id, DWORD flags)
{
    OPENFILENAME info;
    char fname[256];
    char dir[256];
    
    /* ask for a file to save */
    info.lStructSize = sizeof(info);
    info.hwndOwner = handle_;
    info.hInstance = CTadsApp::get_app()->get_instance();
    info.lpstrFilter = filter;
    info.lpstrCustomFilter = 0;
    info.nFilterIndex = 0;
    info.lpstrFile = fname;
    if (filename->get() != 0 && filename->get()[0] != '\0')
    {
        char *p;
        
        /* start off with the current filename */
        strcpy(dir, filename->get());
        
        /* get the root of the name */
        p = os_get_root_name(dir);

        /* copy the filename part */
        strcpy(fname, p);
        
        /* remove the file portion */
        if (p > dir && *(p-1) == '\\')
            *(p-1) = '\0';
        info.lpstrInitialDir = dir;
    }
    else
    {
        /* there's no filename yet */
        fname[0] = '\0';
        
        /* start off in the default open-file directory */
        info.lpstrInitialDir = CTadsApp::get_app()->get_openfile_dir();
    }
    info.nMaxFile = sizeof(fname);
    info.lpstrFileTitle = 0;
    info.nMaxFileTitle = 0;
    info.lpstrTitle = prompt;
    info.Flags = flags | OFN_NOCHANGEDIR;
    info.nFileOffset = 0;
    info.nFileExtension = 0;
    info.lpstrDefExt = defext;
    info.lCustData = 0;
    info.lpfnHook = 0;
    info.lpTemplateName = 0;
    CTadsDialog::set_filedlg_center_hook(&info);
    if (GetSaveFileName(&info))
    {
        /* save the new filename */
        filename->set(fname);
        
        /* copy the new filename into the editor field, if desired */
        if (fld_id != 0)
            SetWindowText(GetDlgItem(handle_, fld_id), fname);
        
        /* update the open-file directory */
        CTadsApp::get_app()->set_openfile_dir(fname);
        
        /* enable the 'next' button now that we have a file */
        SendMessage(GetParent(handle_), PSM_SETWIZBUTTONS, 0,
                    PSWIZB_BACK | PSWIZB_NEXT);
        
        /* success */
        return TRUE;
    }
    else
    {
        /* they cancelled */
        return FALSE;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   New Game Wizard Dialog - welcome
 */

int CHtmlNewWizWelcome::do_notify(NMHDR *nm, int ctl)
{
    switch(nm->code)
    {
    case PSN_SETACTIVE:
        /* this is the first page - turn off the 'back' button */
        SendMessage(GetParent(handle_), PSM_SETWIZBUTTONS, 0,
                    PSWIZB_NEXT);
        return TRUE;
        
    default:
        return CHtmlNewWizPage::do_notify(nm, ctl);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   New Game Wizard Dialog - source file
 */

class CHtmlNewWizSource: public CHtmlNewWizPage
{
public:
    CHtmlNewWizSource(CHtmlNewWiz *wiz) : CHtmlNewWizPage(wiz) { }

    int do_notify(NMHDR *nm, int ctl)
    {
        switch(nm->code)
        {
        case PSN_SETACTIVE:
            /* 
             *   turn on the 'back' button, and turn on the 'next' button
             *   only if a source file has been selected 
             */
            SendMessage(GetParent(handle_), PSM_SETWIZBUTTONS, 0,
                        PSWIZB_BACK
                        | (wiz_->srcfile_.get() != 0
                           && wiz_->srcfile_.get()[0] != 0
                           ? PSWIZB_NEXT : 0));
            return TRUE;

        default:
            return CHtmlNewWizPage::do_notify(nm, ctl);
        }
    }

    int do_command(WPARAM cmd, HWND ctl)
    {
        CStringBuf new_fname;
        
        switch(cmd)
        {
        case IDC_BTN_BROWSE:
        browse_again:
            /* browse for a source file */
            if (browse_file("TADS Source Files\0*.t\0"
                            "All Files\0*.*\0\0", &new_fname,
                            "Choose a name for your new source file", "t",
                            0, OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT
                            | OFN_HIDEREADONLY))
            {
                char fname[OSFNMAX];
                char *ext;
                int ret;

                /* check the suffix */
                ext = strrchr(new_fname.get(), '.');
                if (ext != 0
                    && (stricmp(ext, ".t3m") == 0
                        || stricmp(ext, ".tdc") == 0
                        || stricmp(ext, ".gam") == 0
                        || stricmp(ext, ".t3") == 0
                        || stricmp(ext, ".tl") == 0
                        || stricmp(ext, ".t3o") == 0
                        || stricmp(ext, ".t3s") == 0))
                {
                    /* point out the problem */
                    MessageBox(GetParent(handle_),
                               "The filename you chose ends in a suffix that "
                               "is reserved for other purposes.  You can't "
                               "use this suffix for a source file.  Please "
                               "change the name - a suffix of '.t' is "
                               "recommended.", "TADS Workbench",
                               MB_ICONEXCLAMATION | MB_OK);

                    /* do not accept this filename */
                    ret = IDNO;
                }
                else if (ext != 0 && stricmp(ext, ".t") != 0)
                {
                    /* ask if they really want to use this extension */
                    ret = MessageBox(GetParent(handle_),
                                     "The filename you chose ends in a "
                                     "suffix that isn't usually used for "
                                     "TADS source files - a suffix of '.t' "
                                     "is recommended.  Are you sure you "
                                     "want to use this non-standard "
                                     "filename pattern?", "TADS Workbench",
                                     MB_ICONQUESTION | MB_YESNO);
                }
                else
                {
                    /* proceed */
                    ret = IDYES;
                }

                /* if they wanted to change the name, try again */
                if (ret != IDYES)
                    goto browse_again;

                /* accept the new filename */
                wiz_->srcfile_.set(new_fname.get());
                SetWindowText(GetDlgItem(handle_, IDC_FLD_NEWSOURCE),
                              new_fname.get());
                
                /* 
                 *   We might want to update the game file name to match
                 *   the new source file name.  If the game file hasn't
                 *   been set yet, change it without asking; otherwise,
                 *   ask the user what they want to do.  
                 */
                strcpy(fname, wiz_->srcfile_.get());
                os_remext(fname);
                os_addext(fname, w32_game_ext);
                if (wiz_->gamefile_.get() == 0
                    || wiz_->gamefile_.get()[0] == '\0')
                {
                    /* there's no name defined; set it without asking */
                    wiz_->gamefile_.set(fname);
                }
                else
                {
                    int ret;
                    
                    /* ask */
                    ret = MessageBox(GetParent(handle_),
                                     "Do you want to change the compiled "
                                     "game file name to match this new "
                                     "source file name?",
                                     "TADS Workbench",
                                     MB_ICONQUESTION | MB_YESNO);
                    if (ret == IDYES || ret == 0)
                        wiz_->gamefile_.set(fname);
                }
            }

            /* handled */
            return TRUE;

        default:
            /* inherit default */
            return CHtmlNewWizPage::do_command(cmd, ctl);
        }
    }
};


/* ------------------------------------------------------------------------ */
/*
 *   New Game Wizard Dialog - game file 
 */

int CHtmlNewWizGame::do_notify(NMHDR *nm, int ctl)
{
    switch(nm->code)
    {
    case PSN_SETACTIVE:
        /* 
         *   the game file might have changed externally, so load it into our
         *   text field 
         */
        if (wiz_->gamefile_.get() != 0
            && wiz_->gamefile_.get()[0] != '\0')
            SetWindowText(GetDlgItem(handle_, IDC_FLD_NEWGAMEFILE),
                          wiz_->gamefile_.get());
        
        /* 
         *   turn on the 'back' button, and turn on the 'next' button only if
         *   a game file has been selected 
         */
        SendMessage(GetParent(handle_), PSM_SETWIZBUTTONS, 0,
                    PSWIZB_BACK
                    | (wiz_->gamefile_.get() != 0
                       && wiz_->gamefile_.get()[0] != 0
                       ? PSWIZB_NEXT : 0));
        return TRUE;
        
    default:
        return CHtmlNewWizPage::do_notify(nm, ctl);
    }
}

int CHtmlNewWizGame::do_command(WPARAM cmd, HWND ctl)
{
    switch(cmd)
    {
    case IDC_BTN_BROWSE:
        /* browse for a game file */
        browse_file(w32_opendlg_filter,
                    &wiz_->gamefile_,
                    "Choose a name for your new compiled game file",
                    w32_game_ext, IDC_FLD_NEWGAMEFILE,
                    OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT
                    | OFN_HIDEREADONLY);
        return TRUE;
        
    default:
        /* inherit default */
        return CHtmlNewWizPage::do_command(cmd, ctl);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   New Game Wizard Dialog - source type
 */

class CHtmlNewWizType: public CHtmlNewWizPage
{
public:
    CHtmlNewWizType(CHtmlNewWiz *wiz) : CHtmlNewWizPage(wiz) { }

    void init()
    {
        /* set the "introductory" button by default */
        CheckRadioButton(handle_, IDC_RB_INTRO, IDC_RB_CUSTOM, IDC_RB_INTRO);
        
        /* inherit default*/
        CHtmlNewWizPage::init();
    }

    int do_command(WPARAM cmd, HWND ctl)
    {
        textchar_t buf[OSFNMAX + 20];
        
        switch(cmd)
        {
        case IDC_RB_INTRO:
            /* set "intro" mode */
            wiz_->tpl_type_ = W32TDB_TPL_INTRO;

        enable_next:
            /* we can go on now */
            SendMessage(GetParent(handle_), PSM_SETWIZBUTTONS,
                        0, PSWIZB_BACK | PSWIZB_NEXT);

            /* handled */
            return TRUE;

        case IDC_RB_ADV:
            /* turn on "advanced" mode */
            wiz_->tpl_type_ = W32TDB_TPL_ADVANCED;
            goto enable_next;

        case IDC_RB_PLAIN:
            /* turn on "plain" mode */
            wiz_->tpl_type_ = W32TDB_TPL_NOLIB;
            goto enable_next;

        case IDC_RB_CUSTOM:
            /* turn on "custom" mode */
            wiz_->tpl_type_ = W32TDB_TPL_CUSTOM;

            /* 
             *   if they haven't selected a custom template file yet, run
             *   the "browse" dialog to do so 
             */
            if (wiz_->custom_.get() == 0 || wiz_->custom_.get()[0] == '\0')
            {
                /* 
                 *   turn off the "next" button, since we can't go on
                 *   until a file is selected - we'll turn it back on if
                 *   they select a file or switch to one of the standard
                 *   template options 
                 */
                SendMessage(GetParent(handle_), PSM_SETWIZBUTTONS,
                            0, PSWIZB_BACK);
                
                /* run the "browse" dialog */
                PostMessage(handle_, WM_COMMAND, IDC_BTN_BROWSE, 0);
            }

            /* handled */
            return TRUE;

        case IDC_BTN_BROWSE:
            /* browse for a custom template file */
            if (browse_file("TADS Source Files\0*.t\0"
                            "TADS Library Files\0*.tl\0"
                            "All Files\0*.*\0\0",
                            &wiz_->custom_,
                            "Choose a custom template file",
                            0, 0,
                            OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST
                            | OFN_HIDEREADONLY))
            {
                /* update the radio button string */
                sprintf(buf, "&Custom: %s", wiz_->custom_.get());
                SetWindowText(GetDlgItem(handle_, IDC_RB_CUSTOM), buf);

                /* 
                 *   if we haven't already selected the "custom" radio
                 *   button, select it 
                 */
                if (wiz_->tpl_type_ != W32TDB_TPL_CUSTOM)
                    CheckRadioButton(handle_, IDC_RB_INTRO, IDC_RB_CUSTOM,
                                     IDC_RB_CUSTOM);
            }

            /* handled */
            return TRUE;

        default:
            /* inherit default */
            return CHtmlNewWizPage::do_command(cmd, ctl);
        }
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   New Game Wizard Dialog - bibliographic data
 */

/* initialize */
void CHtmlNewWizBiblio::init()
{
    /* set up the initial values for the fields */
    SetDlgItemText(handle_, IDC_BIB_TITLE, "Your New Game Title");
    SetDlgItemText(handle_, IDC_BIB_AUTHOR, "Your Name");
    SetDlgItemText(handle_, IDC_BIB_EMAIL, "your-email@host.com");
    SetDlgItemText(handle_, IDC_BIB_DESC,
                   "Put a brief \"blurb\" about your game here.");

    /* inherit default */
    CHtmlNewWizPage::init();
}

/* notifications */
int CHtmlNewWizBiblio::do_notify(NMHDR *nm, int ctl)
{
    char buf[512];
    char buf2[512];
    char *src, *dst;
    
    switch(nm->code)
    {
    case PSN_WIZNEXT:
        /* clear our old settings */
        biblio_.clear();

        /* store our updated settings */
        GetDlgItemText(handle_, IDC_BIB_TITLE, buf, sizeof(buf));
        biblio_.add_key("$TITLE$", buf);

        GetDlgItemText(handle_, IDC_BIB_AUTHOR, buf, sizeof(buf));
        biblio_.add_key("$AUTHOR$", buf);

        GetDlgItemText(handle_, IDC_BIB_EMAIL, buf, sizeof(buf));
        biblio_.add_key("$EMAIL$", buf);

        GetDlgItemText(handle_, IDC_BIB_DESC, buf, sizeof(buf));
        biblio_.add_key("$DESC$", buf);

        /* htmlify it, and add it as $HTMLDESC$ */
        for (src = buf, dst = buf2 ;
             *src != '\0' && dst < buf2 + sizeof(buf2) - 1 ; )
        {
            const char *markup;
            char c = *src++;
            
            /* translate markup-significant characters */
            switch (c)
            {
            case '<':
                markup = "&lt;";
                goto add_markup;
                
            case '>':
                markup = "&gt;";
                goto add_markup;
                
            case '&':
                markup = "&amp;";

            add_markup:
                if (dst < buf2 + sizeof(buf2) - strlen(markup) - 1)
                {
                    strcpy(dst, markup);
                    dst += strlen(markup);
                }
                break;
                
            default:
                /* copy it unchanged */
                *dst++ = c;
                break;
            }
        }

        /* null-terminate it */
        *dst = 0;

        /* add it */
        biblio_.add_key("$HTMLDESC$", buf2);
    }

    /* inherit the default handling */
    return CHtmlNewWizPage::do_notify(nm, ctl);
}


/* ------------------------------------------------------------------------ */
/*
 *   New Game Wizard Dialog - success
 */

int CHtmlNewWizSuccess::do_notify(NMHDR *nm, int ctl)
{
    switch(nm->code)
    {
    case PSN_SETACTIVE:
        /* 
         *   turn on the 'finish' button, and turn off the 'next' button,
         *   since there's nowhere to go from here 
         */
        SendMessage(GetParent(handle_), PSM_SETWIZBUTTONS, 0,
                    PSWIZB_FINISH | PSWIZB_BACK);
        return TRUE;
        
    case PSN_WIZFINISH:
        /* 
         *   the user has selected the "finish" button - set the successful
         *   completion flag 
         */
        wiz_->finish_ = TRUE;
        
        /* handled */
        return TRUE;
        
    default:
        return CHtmlNewWizPage::do_notify(nm, ctl);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Run the New Game Wizard 
 */
void run_new_game_wiz(HWND owner, CHtmlSys_dbgmain *mainwin)
{
    PROPSHEETPAGE psp[10];
    PROPSHEETHEADER psh;
    CHtmlNewWizWelcome *welcome;
    CHtmlNewWizSource *source;
    CHtmlNewWizGame *game;
    CHtmlNewWizType *typ;
    CHtmlNewWizBiblio *biblio;
    CHtmlNewWizSuccess *success;
    int i;
    int inited = FALSE;
    void *ctx;
    CHtmlNewWiz wiz;

    /* create our pages */
    welcome = new CHtmlNewWizWelcome(&wiz);
    source = new CHtmlNewWizSource(&wiz);
    game = new CHtmlNewWizGame(&wiz);
    typ = new CHtmlNewWizType(&wiz);
    biblio = new CHtmlNewWizBiblio(&wiz);
    success = new CHtmlNewWizSuccess(&wiz);

    /* set up the main dialog descriptor */
    psh.dwSize = PROPSHEETHEADER_V1_SIZE;
    psh.dwFlags = PSH_USEICONID | PSH_PROPSHEETPAGE | PSH_WIZARD;
    psh.hwndParent = owner;
    psh.hInstance = CTadsApp::get_app()->get_instance();
    psh.pszIcon = 0;
    psh.pszCaption = (LPSTR)"Build Settings";
    psh.nStartPage = 0;
    psh.ppsp = (LPCPROPSHEETPAGE)&psp;
    psh.pfnCallback = 0;

    /* set up the property page descriptors */
    i = 0;
    welcome->init_ps_page(CTadsApp::get_app()->get_instance(),
                          &psp[i++], DLG_NEWWIZ_WELCOME, IDS_NEWWIZ_WELCOME,
                          0, 0, &psh);
    source->init_ps_page(CTadsApp::get_app()->get_instance(),
                         &psp[i++], DLG_NEWWIZ_SOURCE, IDS_NEWWIZ_SOURCE,
                         0, 0, &psh);
    game->init_ps_page(CTadsApp::get_app()->get_instance(),
                       &psp[i++], DLG_NEWWIZ_GAMEFILE, IDS_NEWWIZ_GAMEFILE,
                       0, 0, &psh);
    typ->init_ps_page(CTadsApp::get_app()->get_instance(),
                      &psp[i++], DLG_NEWWIZ_TYPE, IDS_NEWWIZ_TYPE,
                      0, 0, &psh);
    biblio->init_ps_page(CTadsApp::get_app()->get_instance(),
                         &psp[i++], DLG_NEWWIZ_BIBLIO, IDS_NEWWIZ_BIBLIO,
                         0, 0, &psh);
    success->init_ps_page(CTadsApp::get_app()->get_instance(),
                          &psp[i++], DLG_NEWWIZ_SUCCESS, IDS_NEWWIZ_SUCCESS,
                          0, 0, &psh);

    /* set the number of pages */
    psh.nPages = i;

    /* disable windows owned by the main window before showing the dialog */
    ctx = CTadsDialog::modal_dlg_pre(mainwin->get_handle(), FALSE);

    /* run the property sheet */
    PropertySheet(&psh);

    /* re-enable windows */
    CTadsDialog::modal_dlg_post(ctx);

    /* if they clicked "Finish", create the new game */
    if (wiz.finish_)
    {
        /* ask the main window to do the work for us */
        mainwin->create_new_game(wiz.srcfile_.get(), wiz.gamefile_.get(),
                                 TRUE, wiz.tpl_type_,
                                 wiz.tpl_type_ == W32TDB_TPL_CUSTOM
                                 ? wiz.custom_.get() : 0,
                                 biblio->get_biblio());
    }

    /* delete the dialogs */
    delete welcome;
    delete source;
    delete game;
    delete typ;
    delete biblio;
    delete success;
}

