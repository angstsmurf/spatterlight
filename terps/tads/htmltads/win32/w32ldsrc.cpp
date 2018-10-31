/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32ldsrc.cpp - win32 workbench - Load Source Wizard
Function
  Provides a "wizard" interface for creating a project configuration from
  a game's source file.
Notes

Modified
  03/01/03 MJRoberts  - creation (split off from w32dbgpr.cpp; we split this
                        up into a separate file because we don't need this
                        functionality in the TADS 3 Workbench, so we want
                        this stuff separate so we can link the rest of the
                        preferences dialogs, which are common to T2 and T3,
                        without having to include this code)
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
#ifndef W32MAIN_H
#include "w32main.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Load Source Wizard Dialog - game file 
 */

class CHtmlLdSrcWizGame: public CHtmlNewWizGame
{
public:
    CHtmlLdSrcWizGame(CHtmlNewWiz *wiz) : CHtmlNewWizGame(wiz) { }

    int do_command(WPARAM cmd, HWND ctl)
    {
        switch(cmd)
        {
        case IDC_BTN_BROWSE:
            /* browse for a game or configuration file */
            browse_file(w32_config_opendlg_filter,
                        &wiz_->gamefile_,
                        "Choose a name for your new compiled game file",
                        w32_game_ext, IDC_FLD_NEWGAMEFILE,
                        OFN_PATHMUSTEXIST | OFN_HIDEREADONLY);
            return TRUE;

        default:
            /* inherit default */
            return CHtmlNewWizGame::do_command(cmd, ctl);
        }
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   Load Source Game Wizard Dialog - success 
 */

class CHtmlLdSrcWizSuccess: public CHtmlNewWizSuccess
{
public:
    CHtmlLdSrcWizSuccess(CHtmlNewWiz *wiz) : CHtmlNewWizSuccess(wiz) { }

    int do_notify(NMHDR *nm, int ctl)
    {
        textchar_t fname[OSFNMAX];

        switch(nm->code)
        {
        case PSN_SETACTIVE:
            /* 
             *   Check for the existence of the project (.tdc/.t3c) file.  If
             *   a project file exists, show the message indicating that
             *   we'll simply be loading the existing configuration;
             *   otherwise, show the message that we'll be creating a new
             *   configuration.  
             */
            strcpy(fname, wiz_->gamefile_.get());
            os_remext(fname);
            os_addext(fname, w32_tdb_config_ext);
            if (osfacc(fname))
            {
                /* the project (.tdc/.t3c) doesn't exist - we'll create it */
                ShowWindow(GetDlgItem(handle_, IDC_TXT_CONFIG_NEW), SW_SHOW);
                ShowWindow(GetDlgItem(handle_, IDC_TXT_CONFIG_OLD), SW_HIDE);
                wiz_->tdc_exists_ = FALSE;
            }
            else
            {
                /* the project (.tdc/.t3c) already exists - we'll load it */
                ShowWindow(GetDlgItem(handle_, IDC_TXT_CONFIG_NEW), SW_HIDE);
                ShowWindow(GetDlgItem(handle_, IDC_TXT_CONFIG_OLD), SW_SHOW);
                wiz_->tdc_exists_ = TRUE;
            }

            /* inherit the default as well */
            break;

        default:
            /* go inherit the default */
            break;
        }

        /* inherit default handling */
        return CHtmlNewWizSuccess::do_notify(nm, ctl);
    }
};


/* ------------------------------------------------------------------------ */
/*
 *   Run the load-game-from-source wizard 
 */
void run_load_src_wiz(HWND owner, class CHtmlSys_dbgmain *mainwin,
                      const textchar_t *srcfile)
{
    PROPSHEETPAGE psp[10];
    PROPSHEETHEADER psh;
    CHtmlNewWizWelcome *welcome;
    CHtmlLdSrcWizGame *game;
    CHtmlLdSrcWizSuccess *success;
    int i;
    int inited = FALSE;
    void *ctx;
    CHtmlNewWiz wiz;
    textchar_t fname[OSFNMAX];

    /* create our pages */
    welcome = new CHtmlNewWizWelcome(&wiz);
    game = new CHtmlLdSrcWizGame(&wiz);
    success = new CHtmlLdSrcWizSuccess(&wiz);

    /* set the source file in the wizard */
    wiz.srcfile_.set(srcfile);

    /* build the default game file name */
    strcpy(fname, srcfile);
    os_remext(fname);
    os_addext(fname, w32_game_ext);
    wiz.gamefile_.set(fname);

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
                          &psp[i++], DLG_LDSRCWIZ_WELCOME,
                          IDS_LDSRCWIZ_WELCOME, 0, 0, &psh);
    game->init_ps_page(CTadsApp::get_app()->get_instance(),
                       &psp[i++], DLG_LDSRCWIZ_GAMEFILE,
                       IDS_LDSRCWIZ_GAMEFILE, 0, 0, &psh);
    success->init_ps_page(CTadsApp::get_app()->get_instance(),
                          &psp[i++], DLG_LDSRCWIZ_SUCCESS,
                          IDS_LDSRCWIZ_SUCCESS, 0, 0, &psh);

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
        /* 
         *   if the project (.tdc/.t3c) file for the game already exists,
         *   simply load it; otherwise, create the new game 
         */
        if (wiz.tdc_exists_)
        {
            /* simply load the new project (.tdc/.t3c) file */
            mainwin->load_game(wiz.gamefile_.get());
        }
        else
        {
            /* ask the main window to do the work for us */
            mainwin->create_new_game(wiz.srcfile_.get(), wiz.gamefile_.get(),
                                     FALSE, W32TDB_TPL_INTRO, 0, 0);
        }
    }

    /* delete the dialogs */
    delete welcome;
    delete game;
    delete success;
}

