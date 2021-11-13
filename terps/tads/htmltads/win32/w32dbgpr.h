/* $Header: d:/cvsroot/tads/html/win32/w32dbgpr.h,v 1.3 1999/07/11 00:46:50 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32dbgpr.h - debugger preferences dialog
Function
  
Notes
  
Modified
  03/14/98 MJRoberts  - Creation
*/

#ifndef W32DBGPR_H
#define W32DBGPR_H

#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif
#ifndef W32TDB_H
#include "w32tdb.h"
#endif


/*
 *   Run the debugger preferences dialog 
 */
void run_dbg_prefs_dlg(HWND owner, class CHtmlSys_dbgmain *mainwin);

/*
 *   Run the build configuration dialog 
 */
void run_dbg_build_dlg(HWND owner, class CHtmlSys_dbgmain *mainwin,
                       int init_page_id, int init_ctl_id);

/*
 *   Set default debugger values for a configuration 
 */
void set_dbg_defaults(class CHtmlSys_dbgmain *mainwin);

/*
 *   Set default build values for a configuration 
 */
void set_dbg_build_defaults(class CHtmlSys_dbgmain *mainwin,
                            const textchar_t *gamefile);

/* private - set version-specific build defaults */
void vsn_set_dbg_build_defaults(class CHtmlDebugConfig *config,
                                class CHtmlSys_dbgmain *mainwin,
                                const textchar_t *gamefile);

/*
 *   Clear all build information from a configuration 
 */
void clear_dbg_build_info(class CHtmlDebugConfig *config);

/*
 *   Run the new game wizard 
 */
void run_new_game_wiz(HWND owner, class CHtmlSys_dbgmain *mainwin);

/*
 *   Run the load-game-from-source wizard 
 */
void run_load_src_wiz(HWND owner, class CHtmlSys_dbgmain *mainwin,
                      const textchar_t *srcfile);


/* ------------------------------------------------------------------------ */
/*
 *   New Game Wizard - main wizard control object 
 */

class CHtmlNewWiz
{
public:
    CHtmlNewWiz()
    {
        /* clear the flags */
        inited_ = FALSE;
        finish_ = FALSE;
        tdc_exists_ = FALSE;

        /* presume we'll create an introductory (not advanced) source file */
        tpl_type_ = W32TDB_TPL_INTRO;
    }

    /* name of the source file */
    CStringBuf srcfile_;

    /* name of the game file */
    CStringBuf gamefile_;

    /* name of the custom template file, if specified */
    CStringBuf custom_;

    /* type of template file to create */
    w32tdb_tpl_t tpl_type_;

    /* flag: parent dialog position has been initialized */
    int inited_;

    /* flag: user clicked the "finish" button */
    int finish_;

    /* flag: the project (.tdc/.t3c) file exists */
    int tdc_exists_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Base class for new game wizard dialogs 
 */
class CHtmlNewWizPage: public CTadsDialogPropPage
{
public:
    CHtmlNewWizPage(class CHtmlNewWiz *wiz)
    {
        /* remember the main wizard object */
        wiz_ = wiz;
    }

    /* handle a notification */
    int do_notify(NMHDR *nm, int ctl);

    /* initialize */
    void init();

    /*
     *   Browse for a file to save.  Fills in the string buffer with the new
     *   filename and returns true if successful; returns false if the user
     *   cancels the dialog without selecting anything.  
     */
    int browse_file(const char *filter, CStringBuf *filename,
                    const char *prompt, const char *defext, int fld_id,
                    DWORD flags);

    /* main wizard object */
    CHtmlNewWiz *wiz_;
};

/* ------------------------------------------------------------------------ */
/*
 *   New Game Wizard dialog - game file 
 */
class CHtmlNewWizGame: public CHtmlNewWizPage
{
public:
    CHtmlNewWizGame(CHtmlNewWiz *wiz) : CHtmlNewWizPage(wiz) { }

    int do_notify(NMHDR *nm, int ctl);
    int do_command(WPARAM cmd, HWND ctl);
};


/* ------------------------------------------------------------------------ */
/*
 *   New Game Wizard Dialog - welcome 
 */

class CHtmlNewWizWelcome: public CHtmlNewWizPage
{
public:
    CHtmlNewWizWelcome(CHtmlNewWiz *wiz) : CHtmlNewWizPage(wiz) { }

    int do_notify(NMHDR *nm, int ctl);
};

/* ------------------------------------------------------------------------ */
/*
 *   New Game Wizard Dialog - success 
 */
class CHtmlNewWizSuccess: public CHtmlNewWizPage
{
public:
    CHtmlNewWizSuccess(CHtmlNewWiz *wiz) : CHtmlNewWizPage(wiz) { }

    int do_notify(NMHDR *nm, int ctl);
};

/* ------------------------------------------------------------------------ */
/*
 *   Bibliographic data object.  The biblio settings page uses this to send
 *   the bibliographic data to the source file creator. 
 */
class CHtmlBiblioPair
{
public:
    CHtmlBiblioPair(const char *key, const char *val)
        : key_(key), val_(val)
    {
        nxt_ = 0;
    }

    CStringBuf key_;
    CStringBuf val_;

    CHtmlBiblioPair *nxt_;
};

class CHtmlBiblio
{
public:
    CHtmlBiblio()
    {
        keys_ = 0;
    }

    ~CHtmlBiblio()
    {
        /* clear our list */
        clear();
    }

    /* clear the list */
    void clear()
    {
        /* delete the key list */
        while (keys_ != 0)
        {
            CHtmlBiblioPair *nxt = keys_->nxt_;
            delete keys_;
            keys_ = nxt;
        }
    }

    /* add a key */
    void add_key(const char *key, const char *val)
    {
        /* create a new key pair object */
        CHtmlBiblioPair *ele = new CHtmlBiblioPair(key, val);

        /* link it into our list */
        ele->nxt_ = keys_;
        keys_ = ele;
    }

    /* check to see if a text string starts with one of our keys */
    const CHtmlBiblioPair *find_key(const char *buf) const
    {
        const CHtmlBiblioPair *cur;
        
        /* scan our pair list for a match */
        for (cur = keys_ ; cur != 0 ; cur = cur->nxt_)
        {
            /* check for a match */
            if (memcmp(buf, cur->key_.get(), strlen(cur->key_.get())) == 0)
                return cur;
        }

        /* didn't find it */
        return 0;
    }

private:
    /* our linked list of key/value pairs */
    CHtmlBiblioPair *keys_;
};

/*
 *   New Game Wizard Dialog - bibliograhpic settings 
 */
class CHtmlNewWizBiblio: public CHtmlNewWizPage
{
public:
    CHtmlNewWizBiblio(CHtmlNewWiz *wiz) : CHtmlNewWizPage(wiz) { }

    /* initialize */
    void init();

    /* handle notifications */
    int do_notify(NMHDR *nm, int ctl);

    /* get my bibliographic data list object */
    const CHtmlBiblio *get_biblio() const { return &biblio_; }

private:
    /* our bibliographic data list object */
    CHtmlBiblio biblio_;
};


#endif /* W32DBGPR_H */

