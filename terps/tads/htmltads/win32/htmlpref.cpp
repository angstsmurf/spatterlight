#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/htmlpref.cpp,v 1.4 1999/07/11 00:46:46 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlpref.cpp - preferences dialog
Function
  
Notes
  
Modified
  10/26/97 MJRoberts  - Creation
*/

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif
#ifndef HTMLRES_H
#include "htmlres.h"
#endif
#ifndef HTMLPREF_H
#include "htmlpref.h"
#endif
#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif
#ifndef HTMLW32_H
#include "htmlw32.h"
#endif
#ifndef TADSREG_H
#include "tadsreg.h"
#endif
#ifndef TADSCBTN_H
#include "tadscbtn.h"
#endif
#ifndef FOLDSEL_H
#include "foldsel.h"
#endif
#ifndef W32MAIN_H
#include "w32main.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Font point sizes offered in the preferences dialog.  Note that the last
 *   element is always zero to indicate the end of the list.  
 */
const int CHtmlPreferences::font_pt_sizes[] =
{
    8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28, 36, 48, 72, 0
};


/* ------------------------------------------------------------------------ */
/*
 *   Generic font-popup dialog - this class has some simple routines for
 *   handling property page dialogs with popups. 
 */
class CHtmlDialogFontPp: public CTadsDialogPropPage
{
public:
    CHtmlDialogFontPp(CHtmlPreferences *prefs) { prefs_ = prefs; }
    
protected:
    /* 
     *   Read the value of one of the font popups; returns true if this
     *   changes the font setting.  If 'store' is false, just checks the
     *   value without storing it.  
     */
    int read_font_popup(int id, HTML_pref_id_t prefid, int store,
                        const char *blank_val);

    /* read a font size popup and check for a change */
    int read_fontsz_popup(int id, HTML_pref_id_t prefid, int store);

    /* the main preferences object */
    CHtmlPreferences *prefs_;
};


/*
 *   read the contents of one of the font popups into a CStringBuf 
 */
int CHtmlDialogFontPp::read_font_popup(int id, HTML_pref_id_t prefid,
                                       int store, const char *blank_val)
{
    HWND ctl;
    char buf[128];
    size_t len;
    int idx;
    int changed;
    const textchar_t *oldval;

    /* get the control handle */
    ctl = GetDlgItem(handle_, id);

    /* get the selection number */
    idx = SendMessage(ctl, CB_GETCURSEL, 0, 0);

    /* if it's not a valid selection, don't make any changes */
    if (idx == CB_ERR)
        return FALSE;

    /* get the selected string */
    len = SendMessage(ctl, CB_GETLBTEXT, (WPARAM)idx, (LPARAM)buf);

    /* get the old value */
    oldval = prefs_->get_val_str(prefid);

    /* if the old value is blank, substitute the blank_val value */
    if (oldval == 0 || strlen(oldval) == 0)
        oldval = blank_val;

    /* note whether or not this is a change */
    changed = (oldval == 0 || len != get_strlen(oldval)
               || memcmp(buf, oldval, len) != 0);

    /* store it in the specified preference property */
    if (store)
    {
        /* if the new value matches the blank_val value, make it blank */
        if (blank_val != 0 && stricmp(buf, blank_val) == 0)
        {
            buf[0] = '\0';
            len = 0;
        }

        /* store the new value in the preferences */
        prefs_->set_val_str(prefid, buf, len);
    }

    /* return change indication */
    return changed;
}

/*
 *   read the contents of one of the font popups into a CStringBuf 
 */
int CHtmlDialogFontPp::read_fontsz_popup(int id, HTML_pref_id_t prefid,
                                         int store)
{
    HWND ctl;
    int idx;
    int newptsiz;
    int oldptsiz;
    int changed;

    /* get the control handle */
    ctl = GetDlgItem(handle_, id);

    /* get the selection number */
    idx = SendMessage(ctl, CB_GETCURSEL, 0, 0);

    /* if it's not a valid selection, don't make any changes */
    if (idx == CB_ERR)
        return FALSE;

    /* get the point size (it's the extra item data for the list entry) */
    newptsiz = (int)SendMessage(ctl, CB_GETITEMDATA, (WPARAM)idx, 0);

    /* note whether this is a change */
    oldptsiz = prefs_->get_val_longint(prefid);
    changed = (newptsiz != oldptsiz);

    /* store it in the specified preference property */
    if (store && changed)
        prefs_->set_val_longint(prefid, newptsiz);

    /* return change indication */
    return changed;
}

/* ------------------------------------------------------------------------ */
/*
 *   Font dialog 
 */


class CHtmlDialogFonts: public CHtmlDialogFontPp
{
public:
    CHtmlDialogFonts(CHtmlPreferences *prefs, unsigned int charset_id,
                     int standalone)
        : CHtmlDialogFontPp(prefs)
    {
        /* remember the desired character set ID */
        charset_id_ = charset_id;

        /* remember whether or not we're a stand-alone dialog */
        standalone_ = standalone;
    }
    void init();
    int do_notify(NMHDR *nm, int ctl);
    int do_command(WPARAM cmd, HWND ctl);

private:
    /* callbacks to select specific parameterized font types */
    static int font_select_serif(void *, ENUMLOGFONTEX *, NEWTEXTMETRIC *);
    static int font_select_sans(void *, ENUMLOGFONTEX *, NEWTEXTMETRIC *);
    static int font_select_script(void *, ENUMLOGFONTEX *, NEWTEXTMETRIC *);
    static int font_select_typewriter(void *, ENUMLOGFONTEX *,
                                      NEWTEXTMETRIC *);

    /* initialize a point-size popup list */
    void init_ptsiz_popup(int id, int ptsiz);

    /* check for changes */
    int has_changes(int save);

    /* input text color */
    HTML_color_t input_text_color_;

    /* special popup entry for "Main text font" */
    static const char *main_font_entry;

    /* the Windows character set ID for the desired character set */
    unsigned int charset_id_;

    /* 
     *   flag: our property sheet is running as a stand-alone dialog, rather
     *   than as a sub-dialog of the main options dialog 
     */
    int standalone_;
};

/* main font entry text (for special input font name selection) */
const char *CHtmlDialogFonts::main_font_entry = "(Main Game Font)";

/*
 *   initialize 
 */
void CHtmlDialogFonts::init()
{
    COLORREF *cust_colors = prefs_->get_cust_colors();
    const char *cur_inp_font;
    HWND pop;

    /* 
     *   if we're running as a stand-alone dialog (rather than as a
     *   sub-dialog of the main options dialog), center the property sheet
     *   (which is our parent window) on the screen 
     */
    if (standalone_)
        center_dlg_win(GetParent(handle_));

    /* turn off the context-help style in the parent window */
    SetWindowLong(GetParent(handle_), GWL_EXSTYLE,
                  GetWindowLong(GetParent(handle_), GWL_EXSTYLE)
                  & ~WS_EX_CONTEXTHELP);

    /* set up the font combos with lists of available fonts */
    init_font_popup(IDC_MAINFONTPOPUP, TRUE, TRUE,
                    prefs_->get_prop_font(), TRUE, charset_id_);
    init_font_popup(IDC_MONOFONTPOPUP, FALSE, TRUE,
                    prefs_->get_mono_font(), TRUE, charset_id_);
    init_font_popup(IDC_FONTSERIF, prefs_->get_font_serif(),
                    &font_select_serif, 0, TRUE, charset_id_);
    init_font_popup(IDC_FONTSANS, prefs_->get_font_sans(),
                    &font_select_sans, 0, TRUE, charset_id_);
    init_font_popup(IDC_FONTSCRIPT, prefs_->get_font_script(),
                    &font_select_script, 0, TRUE, charset_id_);
    init_font_popup(IDC_FONTTYPEWRITER, prefs_->get_font_typewriter(),
                    &font_select_typewriter, 0, TRUE, charset_id_);

    /* load the point-size combo boxes */
    init_ptsiz_popup(IDC_POP_MAINFONTSIZE, prefs_->get_prop_fontsz());
    init_ptsiz_popup(IDC_POP_MONOFONTSIZE, prefs_->get_mono_fontsz());
    init_ptsiz_popup(IDC_POP_SERIFFONTSIZE, prefs_->get_serif_fontsz());
    init_ptsiz_popup(IDC_POP_SANSFONTSIZE, prefs_->get_sans_fontsz());
    init_ptsiz_popup(IDC_POP_SCRIPTFONTSIZE, prefs_->get_script_fontsz());
    init_ptsiz_popup(IDC_POP_TYPEWRITERFONTSIZE,
                     prefs_->get_typewriter_fontsz());
    init_ptsiz_popup(IDC_POP_INPUTFONTSIZE, prefs_->get_inpfont_size());

    /* get the current input font from the preferences */
    cur_inp_font = prefs_->get_inpfont_name();

    /* if the name is empty, use the "Main Text Font" entry by default */
    if (cur_inp_font == 0 || cur_inp_font[0] == '\0')
        cur_inp_font = main_font_entry;

    /* get the font selector control */
    pop = GetDlgItem(handle_, IDC_POP_INPUTFONT);

    /* clear out any existing list contents */
    SendMessage(pop, CB_RESETCONTENT, 0, 0);

    /* add an element to the font popup for "Main text font" */
    SendMessage(pop, CB_ADDSTRING, (WPARAM)0, (LPARAM)main_font_entry);

    /* set up the font combo with a list of the available fonts */
    init_font_popup(IDC_POP_INPUTFONT, TRUE, TRUE, cur_inp_font, FALSE,
                    charset_id_);

    /* add the color button */
    input_text_color_ = prefs_->get_inpfont_color();
    controls_.add(new CColorBtnPropPage(handle_, IDC_BTN_INPUTCOLOR,
                                        &input_text_color_,
                                        cust_colors, this));

    /* initialize the input font bold/italic style settings */
    CheckDlgButton(handle_, IDC_CK_INPUTBOLD,
                   (prefs_->get_inpfont_bold()
                    ? BST_CHECKED : BST_UNCHECKED));
    CheckDlgButton(handle_, IDC_CK_INPUTITALIC,
                   (prefs_->get_inpfont_italic()
                    ? BST_CHECKED : BST_UNCHECKED));
}

/*
 *   initialize a font-size popup 
 */
void CHtmlDialogFonts::init_ptsiz_popup(int id, int ptsiz)
{
    HWND pop;
    const int *p;
    
    /* get the popup control */
    pop = GetDlgItem(handle_, id);

    /* clear out any existing list contents */
    SendMessage(pop, CB_RESETCONTENT, 0, 0);

    /* add the point size strings */
    for (p = CHtmlPreferences::font_pt_sizes ; *p != 0 ; ++p)
    {
        char buf[20];
        int idx;

        /* build the name for this item */
        sprintf(buf, "%d pt", *p);

        /* add this item to the combo */
        idx = SendMessage(pop, CB_ADDSTRING, (WPARAM)0, (LPARAM)buf);

        /* set this item's lparam to the point size */
        SendMessage(pop, CB_SETITEMDATA, (WPARAM)idx, (LPARAM)*p);

        /* if this is the current point size, make it the selection */
        if (*p == ptsiz)
            SendMessage(pop, CB_SETCURSEL, (WPARAM)idx, 0);
    }
}

/*
 *   select serifed fonts 
 */
int CHtmlDialogFonts::font_select_serif(void *, ENUMLOGFONTEX *elf,
                                       NEWTEXTMETRIC *tm)
{
    /* return ROMAN-style fonts */
    return (tm->tmCharSet != SYMBOL_CHARSET
            && (elf->elfLogFont.lfPitchAndFamily & 0xf0) == FF_ROMAN);
}

/*
 *   select sans-serif fonts 
 */
int CHtmlDialogFonts::font_select_sans(void *, ENUMLOGFONTEX *elf,
                                       NEWTEXTMETRIC *tm)
{
    /* return SWISS-style fonts */
    return (tm->tmCharSet != SYMBOL_CHARSET
            && (elf->elfLogFont.lfPitchAndFamily & 0xf0) == FF_SWISS);
}

/*
 *   select script fonts 
 */
int CHtmlDialogFonts::font_select_script(void *, ENUMLOGFONTEX *elf,
                                         NEWTEXTMETRIC *tm)
{
    /* 
     *   return SCRIPT and ROMAN styles - a lot of script-type fonts seem
     *   to claim they're ROMAN fonts 
     */
    return (tm->tmCharSet != SYMBOL_CHARSET
            && ((elf->elfLogFont.lfPitchAndFamily & 0xf0) == FF_SCRIPT
                || (elf->elfLogFont.lfPitchAndFamily & 0xf0) == FF_ROMAN));
}

/*
 *   select typewriter fonts 
 */
int CHtmlDialogFonts::font_select_typewriter(void *, ENUMLOGFONTEX *elf,
                                             NEWTEXTMETRIC *tm)
{
    /* return MODERN-style fonts */
    return (tm->tmCharSet != SYMBOL_CHARSET
            && (elf->elfLogFont.lfPitchAndFamily & 0xf0) == FF_MODERN);
}

/*
 *   receive a notification message 
 */
int CHtmlDialogFonts::do_notify(NMHDR *nm, int)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* check for and save changes */
        if (has_changes(TRUE))
        {
            /* there are changes, so reformat the main window */
            prefs_->schedule_reformat(FALSE);
        }
        
        /* handled */
        return TRUE;
    }

    return FALSE;
}

/*
 *   determine if I have any changes to reflect 
 */
int CHtmlDialogFonts::has_changes(int save)
{
    int ch;
    int new_bold, new_ital;

    /* presume no changes */
    ch = FALSE;

    /* check our popup controls for changes */
    ch |=  read_font_popup(IDC_MAINFONTPOPUP, HTML_PREF_PROP_FONT, save, 0);
    ch |=  read_font_popup(IDC_MONOFONTPOPUP, HTML_PREF_MONO_FONT, save, 0);
    ch |= read_font_popup(IDC_FONTSERIF, HTML_PREF_FONT_SERIF, save, 0);
    ch |= read_font_popup(IDC_FONTSANS, HTML_PREF_FONT_SANS, save, 0);
    ch |= read_font_popup(IDC_FONTSCRIPT, HTML_PREF_FONT_SCRIPT, save, 0);
    ch |= read_font_popup(IDC_FONTTYPEWRITER,
                          HTML_PREF_FONT_TYPEWRITER, save, 0);
    ch |= read_font_popup(IDC_POP_INPUTFONT, HTML_PREF_INPFONT_NAME, save,
                          main_font_entry);
    ch |= read_fontsz_popup(IDC_POP_MAINFONTSIZE,
                            HTML_PREF_PROP_FONTSZ, save);
    ch |= read_fontsz_popup(IDC_POP_MONOFONTSIZE,
                            HTML_PREF_MONO_FONTSZ, save);
    ch |= read_fontsz_popup(IDC_POP_SERIFFONTSIZE,
                            HTML_PREF_SERIF_FONTSZ, save);
    ch |= read_fontsz_popup(IDC_POP_SANSFONTSIZE,
                            HTML_PREF_SANS_FONTSZ, save);
    ch |= read_fontsz_popup(IDC_POP_SCRIPTFONTSIZE,
                            HTML_PREF_SCRIPT_FONTSZ, save);
    ch |= read_fontsz_popup(IDC_POP_TYPEWRITERFONTSIZE,
                            HTML_PREF_TYPEWRITER_FONTSZ, save);
    ch |= read_fontsz_popup(IDC_POP_INPUTFONTSIZE,
                            HTML_PREF_INPUT_FONTSZ, save);

    /* check for changes in the color setting */
    ch |= (prefs_->get_inpfont_color() != input_text_color_);

    /* note the new style settings */
    new_bold = (IsDlgButtonChecked(handle_, IDC_CK_INPUTBOLD)
                == BST_CHECKED);
    new_ital = (IsDlgButtonChecked(handle_, IDC_CK_INPUTITALIC)
                == BST_CHECKED);

    /* check for style changes */
    ch |= ((new_bold != prefs_->get_inpfont_bold())
           || (new_ital != prefs_->get_inpfont_italic()));

    /* if saving, store the new input text color and style settings */
    if (save && ch)
    {
        prefs_->set_inpfont_bold(new_bold);
        prefs_->set_inpfont_italic(new_ital);
        prefs_->set_inpfont_color(input_text_color_);
    }

    /* return the change indication */
    return ch;
}

/*
 *   process a command 
 */
int CHtmlDialogFonts::do_command(WPARAM cmd, HWND ctl)
{
    /* see if it's a notification that one of the popups changed selection */
    if (HIWORD(cmd) == CBN_SELCHANGE)
    {
        /* see if anything changed (but don't save changes at this point) */
        set_changed(has_changes(FALSE));

        /* handled */
        return TRUE;
    }

    /* check for other commands */
    switch(cmd)
    {
    case IDC_CK_INPUTBOLD:
    case IDC_CK_INPUTITALIC:
        /* check for changes */
        set_changed(has_changes(FALSE));

        /* handled */
        return TRUE;

    case IDC_BTN_INPUTCOLOR:
        /* inherit default handling to activate the button */
        CHtmlDialogFontPp::do_command(cmd, ctl);

        /* check for changes now that we've run the color selector */
        set_changed(has_changes(FALSE));

        /* handled */
        return TRUE;

    default:
        /* inherit the default behavior */
        return CHtmlDialogFontPp::do_command(cmd, ctl);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Color dialog
 */

class CHtmlDialogColor: public CTadsDialogPropPage
{
public:
    CHtmlDialogColor(CHtmlPreferences *prefs)
    {
        /* remember the preferences object */
        prefs_ = prefs;

        /* we haven't warned about a link status change yet */
        warned_link_change_ = FALSE;
    }
    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
    /* check for changes */
    int has_changes(int save);

    /* add a resource string to a popup control's list */
    void add_popup_string(HWND pop, int str_id);

    /* adjust enabling status of the link-related controls */
    void adjust_link_controls();

    /* our preferences object */
    CHtmlPreferences *prefs_;

    /* current color settings */
    HTML_color_t bg_color_;
    HTML_color_t text_color_;
    HTML_color_t link_color_;
    HTML_color_t vlink_color_;
    HTML_color_t hlink_color_;
    HTML_color_t alink_color_;
    HTML_color_t stat_text_color_;
    HTML_color_t stat_bg_color_;

    /* 
     *   flag: we've warned once about a change in the link status, so don't
     *   warn any more while the dialog is open 
     */
    int warned_link_change_;
};


/* 
 *   initialize 
 */
void CHtmlDialogColor::init()
{
    int use_win_colors;
    int override_colors;
    int hover_hilite;
    COLORREF *cust_colors = prefs_->get_cust_colors();
    HWND link_pop;
    int idx;
    
    /* get initial colors from preferences */
    bg_color_ = prefs_->get_bg_color();
    text_color_ = prefs_->get_text_color();
    link_color_ = prefs_->get_link_color();
    vlink_color_ = prefs_->get_vlink_color();
    hlink_color_ = prefs_->get_hlink_color();
    alink_color_ = prefs_->get_alink_color();
    stat_text_color_ = prefs_->get_color_status_text();
    stat_bg_color_ = prefs_->get_color_status_bg();

    /* set up the color buttons */
    controls_.add(new CColorBtnPropPage(handle_, IDC_BKCOLOR,
                                        &bg_color_, cust_colors, this));
    controls_.add(new CColorBtnPropPage(handle_, IDC_TXTCOLOR,
                                        &text_color_, cust_colors, this));
    controls_.add(new CColorBtnPropPage(handle_, IDC_LINKCOLOR,
                                        &link_color_, cust_colors, this));
    controls_.add(new CColorBtnPropPage(handle_, IDC_HLINKCOLOR,
                                        &hlink_color_, cust_colors, this));
    controls_.add(new CColorBtnPropPage(handle_, IDC_ALINKCOLOR,
                                        &alink_color_, cust_colors, this));
    controls_.add(new CColorBtnPropPage(handle_, IDC_SBKCOLOR,
                                        &stat_bg_color_, cust_colors, this));
    controls_.add(new CColorBtnPropPage(handle_, IDC_STXTCOLOR,
                                        &stat_text_color_,
                                        cust_colors, this));

    /* initialize enabling state of text/background color buttons */
    use_win_colors = prefs_->get_use_win_colors();
    override_colors = prefs_->get_override_colors();
    CheckDlgButton(handle_, IDC_CK_USEWINCLR,
                   (use_win_colors ? BST_CHECKED : BST_UNCHECKED));
    CheckDlgButton(handle_, IDC_CK_OVERRIDECLR,
                   (override_colors ? BST_CHECKED : BST_UNCHECKED));

    /* enable or disable the text and background buttons accordingly */
    EnableWindow(GetDlgItem(handle_, IDC_TXTCOLOR), !use_win_colors);
    EnableWindow(GetDlgItem(handle_, IDC_BKCOLOR), !use_win_colors);

    /* enable or disable the statusline color buttons */
    EnableWindow(GetDlgItem(handle_, IDC_STXTCOLOR), !override_colors);
    EnableWindow(GetDlgItem(handle_, IDC_SBKCOLOR), !override_colors);

    /* initialize underline checkbox */
    CheckDlgButton(handle_, IDC_CK_UNDERLINE,
                   (prefs_->get_underline_links()
                    ? BST_CHECKED : BST_UNCHECKED));

    /* initialize hover hilite checkbox */
    hover_hilite = prefs_->get_hover_hilite();
    CheckDlgButton(handle_, IDC_CK_HOVER,
                   (hover_hilite ? BST_CHECKED : BST_UNCHECKED));

    /* enable or disable the hover color accordingly */
    EnableWindow(GetDlgItem(handle_, IDC_HLINKCOLOR), hover_hilite);

    /* add the element names to the "show links" popup */
    link_pop = GetDlgItem(handle_, IDC_POP_SHOW_LINKS);
    add_popup_string(link_pop, IDS_LINKS_ALWAYS);
    add_popup_string(link_pop, IDS_LINKS_CTRL);
    add_popup_string(link_pop, IDS_LINKS_NEVER);

    /* select the appropriate item in the popup for the current settings */
    idx = (prefs_->get_links_on() ? 0 : prefs_->get_links_ctrl() ? 1 : 2);
    SendMessage(link_pop, CB_SETCURSEL, idx, 0);

    /* adjust the link-related controls for the initial state */
    adjust_link_controls();
}

/*
 *   Add a resource string to a popup's list 
 */
void CHtmlDialogColor::add_popup_string(HWND pop, int str_id)
{
    char buf[256];

    /* load the string */
    LoadString(CTadsApp::get_app()->get_instance(), str_id, buf, sizeof(buf));

    /* add it to the popup list */
    SendMessage(pop, CB_ADDSTRING, 0, (LPARAM)buf);
}

/*
 *   process notifications 
 */
int CHtmlDialogColor::do_notify(NMHDR *nm, int)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* check for and save changes */
        if (has_changes(TRUE))
        {
            /* we have changes, so reformat the main window */
            prefs_->schedule_reformat(FALSE);
        }

        /* handled */
        return TRUE;
    }
    return FALSE;
}

/*
 *   check for changes 
 */
int CHtmlDialogColor::has_changes(int save)
{
    int new_use_win_clr;
    int new_override_clr;
    int new_underline;
    int new_hover_hilite;
    int ch;
    int idx;
    int new_links_on;
    int new_links_ctrl;
    int link_ch;

    /* note new checkbox settings */
    new_use_win_clr = (IsDlgButtonChecked(handle_, IDC_CK_USEWINCLR)
                       == BST_CHECKED);
    new_override_clr = (IsDlgButtonChecked(handle_, IDC_CK_OVERRIDECLR)
                        == BST_CHECKED);
    new_underline = (IsDlgButtonChecked(handle_, IDC_CK_UNDERLINE)
                     == BST_CHECKED);
    new_hover_hilite = (IsDlgButtonChecked(handle_, IDC_CK_HOVER)
                        == BST_CHECKED);

    /* note the 'show links' popup selection */
    idx = SendMessage(GetDlgItem(handle_, IDC_POP_SHOW_LINKS),
                      CB_GETCURSEL, 0, 0);
    new_links_on = (idx == 0);
    new_links_ctrl = (idx == 1);

    /* note a change in link status */
    link_ch = (prefs_->get_links_on() != new_links_on
               || prefs_->get_links_ctrl() != new_links_ctrl);

    /* note changes in any of our colors */
    ch = (prefs_->get_use_win_colors() != new_use_win_clr
          || prefs_->get_override_colors() != new_override_clr
          || prefs_->get_bg_color() != bg_color_
          || prefs_->get_text_color() != text_color_
          || prefs_->get_link_color() != link_color_
          || prefs_->get_vlink_color() != vlink_color_
          || prefs_->get_hlink_color() != hlink_color_
          || prefs_->get_alink_color() != alink_color_
          || prefs_->get_underline_links() != new_underline
          || prefs_->get_hover_hilite() != new_hover_hilite
          || prefs_->get_color_status_text() != stat_text_color_
          || prefs_->get_color_status_bg() != stat_bg_color_
          || link_ch);
    
    /* save the changes, if desired */
    if (save && ch)
    {
        /* save the current settings */
        prefs_->set_use_win_colors(new_use_win_clr);
        prefs_->set_override_colors(new_override_clr);
        prefs_->set_bg_color(bg_color_);
        prefs_->set_text_color(text_color_);
        prefs_->set_link_color(link_color_);
        prefs_->set_vlink_color(vlink_color_);
        prefs_->set_hlink_color(hlink_color_);
        prefs_->set_alink_color(alink_color_);
        prefs_->set_underline_links(new_underline);
        prefs_->set_hover_hilite(new_hover_hilite);
        prefs_->set_color_status_text(stat_text_color_);
        prefs_->set_color_status_bg(stat_bg_color_);
        prefs_->set_links_on(new_links_on);
        prefs_->set_links_ctrl(new_links_ctrl);

        /* 
         *   if the link status is changing, notify the main window - it
         *   might want to warn about this, since in some states, disabling
         *   links will not have any immediate effect 
         */
        if (link_ch && !warned_link_change_)
        {
            /* notify the main window about it */
            prefs_->notify_link_pref_change();

            /* only notify once per dialog session */
            warned_link_change_ = TRUE;
        }
    }

    /* return the change indication */
    return ch;
}

/*
 *   process a command 
 */
int CHtmlDialogColor::do_command(WPARAM id, HWND ctl)
{
    int checked;

    switch(LOWORD(id))
    {
    case IDC_CK_USEWINCLR:
        /* get the new state of the button */
        checked = (IsDlgButtonChecked(handle_, (int)id) == BST_CHECKED);
            
        /* enable or disable the text and background buttons accordingly */
        EnableWindow(GetDlgItem(handle_, IDC_TXTCOLOR), !checked);
        EnableWindow(GetDlgItem(handle_, IDC_BKCOLOR), !checked);

        /* note the state change */
        set_changed(has_changes(FALSE));
        return TRUE;

    case IDC_CK_HOVER:
    case IDC_POP_SHOW_LINKS:
        /* note the state change */
        set_changed(has_changes(FALSE));

        /* adjust control enabling for the current mode */
        adjust_link_controls();

        /* handled */
        return TRUE;
        
    case IDC_CK_OVERRIDECLR:
        /* get the new state of the button */
        checked = (IsDlgButtonChecked(handle_, (int)id) == BST_CHECKED);

        /* enable or disable the statusline color buttons accordingly */
        EnableWindow(GetDlgItem(handle_, IDC_STXTCOLOR), !checked);
        EnableWindow(GetDlgItem(handle_, IDC_SBKCOLOR), !checked);

        /* note the state change */
        set_changed(has_changes(FALSE));
        return TRUE;

    case IDC_CK_UNDERLINE:
        /* note the state change */
        set_changed(has_changes(FALSE));
        return TRUE;

    case IDC_BKCOLOR:
    case IDC_TXTCOLOR:
    case IDC_LINKCOLOR:
    case IDC_HLINKCOLOR:
    case IDC_ALINKCOLOR:
    case IDC_SBKCOLOR:
    case IDC_STXTCOLOR:
        /* color button - inherit default handling to activate the button */
        CTadsDialogPropPage::do_command(id, ctl);

        /* check for changes now that we've run the color selector */
        set_changed(has_changes(FALSE));

        /* handled */
        return TRUE;
        
    default:
        /* pass to superclass for processing */
        return CTadsDialog::do_command(id, ctl);
    }
}

/*
 *   Adjust the link controls for the current link enabling states 
 */
void CHtmlDialogColor::adjust_link_controls()
{
    int hover;
    int links_on;

    /* check to see if 'highlight when hovering' is checked */
    hover = (IsDlgButtonChecked(handle_, (int)IDC_CK_HOVER) == BST_CHECKED);

    /* 
     *   check to see if links are on - 'always' or 'ctrl key down' both
     *   count as on 
     */
    links_on = (SendMessage(GetDlgItem(handle_, IDC_POP_SHOW_LINKS),
                            CB_GETCURSEL, 0, 0) != 2);

    /* 
     *   Enable or disable the color buttons, underline checkbox, and hover
     *   checkbox: enable them if links are on, disable if not.  In addition,
     *   disable the hover color button if hovering isn't enabled.  
     */
    EnableWindow(GetDlgItem(handle_, IDC_CK_UNDERLINE), links_on);
    EnableWindow(GetDlgItem(handle_, IDC_CK_HOVER), links_on);
    EnableWindow(GetDlgItem(handle_, IDC_LINKCOLOR), links_on);
    EnableWindow(GetDlgItem(handle_, IDC_ALINKCOLOR), links_on);
    EnableWindow(GetDlgItem(handle_, IDC_HLINKCOLOR), links_on && hover);
}


/* ------------------------------------------------------------------------ */
/*
 *   MORE dialog 
 */

class CHtmlDialogMore: public CTadsDialogPropPage
{
public:
    CHtmlDialogMore(CHtmlPreferences *prefs) { prefs_ = prefs; }
    void init();
    int do_notify(NMHDR *nm, int ctl);
    int do_command(WPARAM cmd, HWND ctl);

private:
    /* check for changes */
    int has_changes(int save);

    /* get the state of the Alt More Style button */
    int get_alt_more_button();

    CHtmlPreferences *prefs_;
};

/*
 *   initialize 
 */
void CHtmlDialogMore::init()
{
    /* initialize controls with current preference settings */
    CheckRadioButton(handle_, IDC_RB_MORE_NORMAL, IDC_RB_MORE_ALT,
                     prefs_->get_alt_more_style() ?
                     IDC_RB_MORE_ALT : IDC_RB_MORE_NORMAL);
}

/*
 *   process notifications 
 */
int CHtmlDialogMore::do_notify(NMHDR *nm, int)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* check for and save changes */
        if (has_changes(TRUE))
        {
            /* the style has changed, so reformat the window */
            prefs_->schedule_reformat(TRUE);
        }

        /* handled */
        return TRUE;
    }
    return FALSE;
}

/* 
 *   get the alt-more-mode button state 
 */
int CHtmlDialogMore::get_alt_more_button()
{
    /* check the checkbox state */
    return (IsDlgButtonChecked(handle_, IDC_RB_MORE_ALT) == BST_CHECKED);
}

/*
 *   process commands 
 */
int CHtmlDialogMore::do_command(WPARAM, HWND)
{
    /* see if anything has changed */
    set_changed(has_changes(FALSE));

    /* handled */
    return TRUE;
}

/*
 *   check for changes 
 */
int CHtmlDialogMore::has_changes(int save)
{
    int ch;

    /* check for changes */
    ch = ((prefs_->get_alt_more_style() != 0)
          != (get_alt_more_button() != 0));

    /* save changes if desired */
    if (ch && save)
    {
        /* save the current selection state in the preferences */
        prefs_->set_alt_more_style(get_alt_more_button());
    }

    /* return change indication */
    return ch;
}

/* ------------------------------------------------------------------------ */
/*
 *   Media dialog 
 */

class CHtmlDialogMedia: public CTadsDialogPropPage
{
public:
    CHtmlDialogMedia(CHtmlPreferences *prefs) { prefs_ = prefs; }
    void init();
    int do_notify(NMHDR *nm, int ctl);
    int do_command(WPARAM cmd, HWND ctl);

private:
    /* check for changes */
    int has_changes(int save);

    CHtmlPreferences *prefs_;
};

/*
 *   initialize 
 */
void CHtmlDialogMedia::init()
{
    /* initialize controls with current preference settings */
    CheckDlgButton(handle_, IDC_CK_GRAPHICS,
                   prefs_->get_graphics_on() ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(handle_, IDC_CK_SOUND_FX,
                   prefs_->get_sounds_on() ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(handle_, IDC_CK_MUSIC,
                   prefs_->get_music_on() ? BST_CHECKED : BST_UNCHECKED);
}

/*
 *   process notifications 
 */
int CHtmlDialogMedia::do_notify(NMHDR *nm, int)
{
    int old_gr;

    switch(nm->code)
    {
    case PSN_APPLY:
        /* remember whether or not graphics are currently on */
        old_gr = prefs_->get_graphics_on();

        /* check for and save changes */
        if (has_changes(TRUE))
        {
            /* notify the game window that it might need to cancel sounds */
            prefs_->notify_sound_pref_change();

            /* if the graphics enabling changed, reformat the game window */
            if (old_gr != prefs_->get_graphics_on())
                prefs_->schedule_reformat(FALSE);
        }

        /* handled */
        return TRUE;
    }

    /* not handled */
    return FALSE;
}

/*
 *   process commands 
 */
int CHtmlDialogMedia::do_command(WPARAM id, HWND ctl)
{
    switch (LOWORD(id))
    {
    case IDC_CK_GRAPHICS:
    case IDC_CK_SOUND_FX:
    case IDC_CK_MUSIC:
        /* see if anything has changed */
        set_changed(has_changes(FALSE));

        /* handled */
        return TRUE;
    }

    /* not handled */
    return CTadsDialogPropPage::do_command(id, ctl);
}

/*
 *   check for changes 
 */
int CHtmlDialogMedia::has_changes(int save)
{
    int graphics_on;
    int sound_on;
    int music_on;
    int ch;

    /* get the current button settings */
    graphics_on = (IsDlgButtonChecked(handle_, IDC_CK_GRAPHICS)
                   == BST_CHECKED);
    sound_on = (IsDlgButtonChecked(handle_, IDC_CK_SOUND_FX)
                == BST_CHECKED);
    music_on = (IsDlgButtonChecked(handle_, IDC_CK_MUSIC)
                == BST_CHECKED);

    /* check for changed */
    ch = (prefs_->get_graphics_on() != graphics_on
          || prefs_->get_sounds_on() != sound_on
          || prefs_->get_music_on() != music_on);

    /* save the changes if desired */
    if (ch && save)
    {
        prefs_->set_graphics_on(graphics_on);
        prefs_->set_sounds_on(sound_on);
        prefs_->set_music_on(music_on);
    }

    /* return the change indication */
    return ch;
}

/* ------------------------------------------------------------------------ */
/*
 *   Appearance dialog.  
 */

/*
 *   Run the preferences dialog 
 */
void CHtmlPreferences::run_appearance_dlg(HWND hwndOwner,
                                          CHtmlSysWin_win32 *win,
                                          int standalone)
{
    PROPSHEETPAGE psp[20];
    PROPSHEETHEADER psh;
    CHtmlDialogFonts *fonts_dlg;
    CHtmlDialogColor *color_dlg;
    CHtmlDialogMore *more_dlg;
    CHtmlDialogMedia *media_dlg;
    int i;
    unsigned int charset_id;
    char title[128 + 50];

    /* remember the owning window */
    win_ = win;

    /* get the character set ID of the owner window */
    charset_id = win->get_owner()->owner_get_default_charset().charset;

    /* create our dialog objects */
    fonts_dlg = new CHtmlDialogFonts(this, charset_id, standalone);
    color_dlg = new CHtmlDialogColor(this);
    more_dlg = new CHtmlDialogMore(this);
    media_dlg = new CHtmlDialogMedia(this);

    /* 
     *   set up the page descriptors 
     */

    /* no pages yet */
    i = 0;

    /* set up the Fonts page */
    psp[i].dwSize = sizeof(PROPSHEETPAGE);
    psp[i].dwFlags = PSP_USETITLE;
    psp[i].hInstance = CTadsApp::get_app()->get_instance();
    psp[i].pszTemplate = MAKEINTRESOURCE(DLG_FONT);
    psp[i].pszIcon = 0;
    psp[i].pszTitle = MAKEINTRESOURCE(IDS_FONT);
    fonts_dlg->prepare_prop_page(&psp[i]);
    ++i;

    /* set up the Colors page */
    psp[i].dwSize = sizeof(PROPSHEETPAGE);
    psp[i].dwFlags = PSP_USETITLE;
    psp[i].hInstance = CTadsApp::get_app()->get_instance();
    psp[i].pszTemplate = MAKEINTRESOURCE(DLG_COLORS);
    psp[i].pszIcon = 0;
    psp[i].pszTitle = MAKEINTRESOURCE(IDS_COLORS);
    color_dlg->prepare_prop_page(&psp[i]);
    ++i;

    /* set up the "More" Style page */
    psp[i].dwSize = sizeof(PROPSHEETPAGE);
    psp[i].dwFlags = PSP_USETITLE;
    psp[i].hInstance = CTadsApp::get_app()->get_instance();
    psp[i].pszTemplate = MAKEINTRESOURCE(DLG_MORE);
    psp[i].pszIcon = 0;
    psp[i].pszTitle = MAKEINTRESOURCE(IDS_MORE);
    more_dlg->prepare_prop_page(&psp[i]);
    ++i;

    /* set up the Media page */
    psp[i].dwSize = sizeof(PROPSHEETPAGE);
    psp[i].dwFlags = PSP_USETITLE;
    psp[i].hInstance = CTadsApp::get_app()->get_instance();
    psp[i].pszTemplate = MAKEINTRESOURCE(DLG_MEDIA);
    psp[i].pszIcon = 0;
    psp[i].pszTitle = MAKEINTRESOURCE(IDS_MEDIA);
    media_dlg->prepare_prop_page(&psp[i]);
    ++i;

    /* set up the main dialog descriptor */
    psh.dwSize = PROPSHEETHEADER_V1_SIZE;
    psh.dwFlags = PSH_PROPSHEETPAGE;
    psh.hwndParent = hwndOwner;
    psh.hInstance = CTadsApp::get_app()->get_instance();
    psh.pszIcon = 0;
    sprintf(title, "Customize \"%s\" Theme", get_active_profile_name());
    psh.pszCaption = (LPSTR)title;
    psh.nPages = i;
    psh.nStartPage = 0;
    psh.ppsp = (LPCPROPSHEETPAGE)&psp;
    psh.pfnCallback = 0;

    /* run the property sheet */
    PropertySheet(&psh);

    /* delete the dialogs */
    delete fonts_dlg;
    delete color_dlg;
    delete more_dlg;
    delete media_dlg;
}


/* ------------------------------------------------------------------------ */
/*
 *   New Profile dialog 
 */
class CTadsDialogNewProfile: public CTadsDialog
{
public:
    CTadsDialogNewProfile(char *buf, size_t buflen, HWND pop)
    {
        /* remember our profile name buffer */
        buf_ = buf;
        buflen_ = buflen;

        /* remember the listbox containing the existing names */
        pop_ = pop;
    }

    /* run the dialog modally */
    static int run_dialog(HWND parent, char *buf, size_t buflen, HWND pop)
    {
        CTadsDialogNewProfile *dlg;
        int id;

        /* create the dialog */
        dlg = new CTadsDialogNewProfile(buf, buflen, pop);

        /* run the dialog modally */
        id = dlg->run_modal(DLG_NEW_PROFILE, parent,
                            CTadsApp::get_app()->get_instance());

        /* delete the dialog */
        delete dlg;

        /* return true if we were dismissed with OK */
        return (id == IDOK);
    }

    /* process a command */
    virtual int do_command(WPARAM id, HWND ctl)
    {
        int i;
        int cnt;

        switch (LOWORD(id))
        {
        case IDOK:
            /* make sure the name isn't too long */
            if (strlen(buf_) > 128)
            {
                /* it's too long */
                MessageBox(handle_, "This profile name is too long.  Please "
                           "choose a shorter name.", "TADS",
                           MB_OK | MB_ICONEXCLAMATION);

                /* handled - do not dismiss */
                return TRUE;
            }

            /* 
             *   make sure the name doesn't contain a backslash (we use the
             *   name as a registry key, and registry keys can't contain
             *   backslash characters) 
             */
            if (strchr(buf_, '\\') != 0)
            {
                /* explain the problem */
                MessageBox(handle_, "The character '\\' is not allowed "
                           "in a profile name.", "TADS",
                           MB_OK | MB_ICONEXCLAMATION);

                /* handled - do not dismiss */
                return TRUE;
            }

            /* scan the name list to make sure this name doesn't collide */
            cnt = SendMessage(pop_, CB_GETCOUNT, 0, 0);
            for (i = 0 ; i < cnt ; ++i)
            {
                char buf[128];

                /* get this item's name */
                SendMessage(pop_, CB_GETLBTEXT, i, (LPARAM)buf);

                /* make sure this new name is different */
                if (stricmp(buf, buf_) == 0)
                {
                    /* tell them about it */
                    MessageBox(handle_, "A profile with this name already "
                               "exists.  You must give each profile a unique "
                               "name.", "TADS", MB_OK | MB_ICONEXCLAMATION);

                    /* handled - do not dismiss the dialog */
                    return TRUE;
                }
            }

            /* inherit default handling as well, to dismiss the dialog */
            break;

        case IDC_FLD_PROFILE:
            /* see what kind of notification we're receiving */
            switch(HIWORD(id))
            {
            case EN_CHANGE:
                /* get the new field value */
                GetDlgItemText(handle_, IDC_FLD_PROFILE, buf_, buflen_);

                /* enable the "OK" button only if the field is non-empty */
                EnableWindow(GetDlgItem(handle_, IDOK),
                             (buf_[0] != '\0'));

                /* handled */
                return TRUE;
            }

            /* not handled */
            break;
        }

        /* inherit default handling */
        return CTadsDialog::do_command(id, ctl);
    }

private:
    /* 
     *   profile name buffer - we'll fill this in with the name supplied by
     *   the user if successful, and we'll use the initial contents to
     *   initialize the text field 
     */
    char *buf_;
    size_t buflen_;

    /* handle to popup containing the list of existing profiles */
    HWND pop_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Appearance dialog 
 */
class CHtmlDialogAppearance: public CTadsDialogPropPage
{
public:
    CHtmlDialogAppearance(CHtmlPreferences *prefs)
    {
        /* remember the preferences object */
        prefs_ = prefs;
    }
    void init();
    int do_notify(NMHDR *nm, int ctl);
    int do_command(WPARAM cmd, HWND ctl);

private:
    /* update for a profile change */
    void on_profile_change();

    /* save any changes to the description field */
    void save_desc();

    /* our preferences object */
    CHtmlPreferences *prefs_;
};

/*
 *   initialize 
 */
void CHtmlDialogAppearance::init()
{
    HWND pop;
    HKEY key;
    DWORD disposition;
    const textchar_t *active;
    char base_key[256];
    int key_idx;
    int pop_idx;

    /* inherit default initialization */
    CTadsDialogPropPage::init();

    /* center the parent dialog on the screen */
    center_dlg_win(GetParent(handle_));

    /* get the active profile */
    active = prefs_->get_active_profile_name();

    /* get the popup handle */
    pop = GetDlgItem(handle_, IDC_POP_THEME);

    /* add each profile to the popup */
    sprintf(base_key, "%s\\Profiles", w32_pref_key_name);
    key = CTadsRegistry::open_key(HKEY_CURRENT_USER, base_key,
                                  &disposition, TRUE);
    for (key_idx = 0 ; ; ++key_idx)
    {
        char subkey[128];
        DWORD len;
        FILETIME ft;
        
        /* get the next profile name from the registry */
        len = sizeof(subkey);
        if (RegEnumKeyEx(key, key_idx, subkey, &len, 0, 0, 0, &ft)
            != ERROR_SUCCESS)
            break;
        
        /* add the profile name to the popup */
        pop_idx = SendMessage(pop, CB_ADDSTRING, 0, (LPARAM)subkey);

        /* select it if it's currently active */
        if (stricmp(active, subkey) == 0)
            SendMessage(pop, CB_SETCURSEL, pop_idx, 0);
    }

    /* done with the key */
    CTadsRegistry::close_key(key);

    /* initialize controls for the current profile */
    on_profile_change();
}

/*
 *   save the profile description text
 */
void CHtmlDialogAppearance::save_desc()
{
    char buf[128];

    /* get the descriptive text from the field */
    GetWindowText(GetDlgItem(handle_, IDC_FLD_DESC), buf, sizeof(buf));

    /* save the description in the preferences */
    prefs_->set_profile_desc(buf);
}

/*
 *   update the UI for a change in the active profile
 */
void CHtmlDialogAppearance::on_profile_change()
{
    const char *active;
    int is_std;

    /* note the active profile */
    active = prefs_->get_active_profile_name();

    /* note if it's one of the standard (pre-defined) profiles */
    is_std = CHtmlPreferences::is_standard_profile(active);
    
    /* disable the 'delete' button for standard profiles */
    EnableWindow(GetDlgItem(handle_, IDC_BTN_DELETE), !is_std);

    /* enable the 'restore to defaults' button only for standard profiles */
    EnableWindow(GetDlgItem(handle_, IDC_BTN_DEFAULT), is_std);

    /* if it's a standard profile, use the standard description for it */
    if (is_std)
        prefs_->set_std_profile_desc(active);

    /* set the profile desciption text */
    SetWindowText(GetDlgItem(handle_, IDC_FLD_DESC),
                  prefs_->get_profile_desc());

    /* make the profile description read-only for standard profiles */
    SendMessage(GetDlgItem(handle_, IDC_FLD_DESC), EM_SETREADONLY, is_std, 0);
}

/*
 *   process notifications 
 */
int CHtmlDialogAppearance::do_notify(NMHDR *nm, int)
{
    switch (nm->code)
    {
    case PSN_KILLACTIVE:
        /* make sure we've saved any changes to the profile's description */
        save_desc();
        return TRUE;
    }

    /* not handled */
    return FALSE;
}

/*
 *   process commands 
 */
int CHtmlDialogAppearance::do_command(WPARAM id, HWND ctl)
{
    HWND pop;
    int sel;
    char buf[128];

    /* note the theme popup handle, as we need it for most operations */
    pop = GetDlgItem(handle_, IDC_POP_THEME);

    /* check which control the command came from */
    switch(LOWORD(id))
    {
    case IDC_POP_THEME:
        /* it's from the theme selector popup - check what happened */
        switch(HIWORD(id))
        {
        case CBN_SELCHANGE:
            /* selection change - get the new selection */
            sel = SendMessage(pop, CB_GETCURSEL, 0, 0);
            
            /* get the name of the profile selected */
            SendMessage(pop, CB_GETLBTEXT, sel, (LPARAM)buf);

            /* save any changes to the description */
            save_desc();

            /* save the outgoing profile */
            prefs_->save();

            /* activate this profile */
            CHtmlSys_mainwin::get_main_win()->set_game_specific_profile(buf);

            /* update for the profile change */
            on_profile_change();

            /* handled */
            return TRUE;
        }

        /* not handled */
        break;

    case IDC_BTN_NEW:
        /* run the New Profile dialog */
        buf[0] = '\0';
        if (CTadsDialogNewProfile::run_dialog(
            GetParent(handle_), buf, sizeof(buf),
            GetDlgItem(handle_, IDC_POP_THEME)))
        {
            /* insert the profile name into the popup and select it */
            sel = SendMessage(pop, CB_ADDSTRING, 0, (LPARAM)buf);
            SendMessage(pop, CB_SETCURSEL, sel, 0);

            /* save changes to the description */
            save_desc();

            /* save our current settings */
            prefs_->save();

            /* 
             *   save a copy of the current settings under the new profile
             *   name, to create the new profile, then switch to the new
             *   profile 
             */
            prefs_->save_as(buf);
            CHtmlSys_mainwin::get_main_win()->set_game_specific_profile(buf);

            /* the new profile initially has no descriptive text */
            prefs_->set_profile_desc("");
            SetWindowText(GetDlgItem(handle_, IDC_FLD_DESC), "");

            /* update the buttons for the selection change */
            on_profile_change();
        }

        /* handled */
        return TRUE;

    case IDC_BTN_DELETE:
        /* make sure they mean it */
        if (MessageBox(handle_, "Are you sure you want to delete this "
                       "theme and discard all of its settings?",
                       "TADS", MB_YESNO | MB_ICONQUESTION) == IDYES)
        {
            const char *active;
            int cnt;
            
            /* get the current selection from the popup */
            sel = SendMessage(pop, CB_GETCURSEL, 0, 0);

            /* delete the profile from the popup list */
            SendMessage(pop, CB_DELETESTRING, sel, 0);

            /* get the registry key name for the current profile */
            active = prefs_->get_active_profile_name();
            prefs_->get_settings_key_for(buf, sizeof(buf), active);

            /* delete the key */
            RegDeleteKey(HKEY_CURRENT_USER, buf);

            /* 
             *   select the next listbox item, or the previous if that was
             *   the last one 
             */
            cnt = SendMessage(pop, CB_GETCOUNT, 0, 0);
            if (sel >= cnt)
                sel = cnt - 1;
            SendMessage(pop, CB_SETCURSEL, sel, 0);

            /* get the new selected profile */
            sel = SendMessage(pop, CB_GETCURSEL, 0, 0);
            SendMessage(pop, CB_GETLBTEXT, sel, (LPARAM)buf);

            /* activate this profile */
            CHtmlSys_mainwin::get_main_win()->set_game_specific_profile(buf);

            /* update the buttons for the selection change */
            on_profile_change();
        }

        /* handled */
        return TRUE;

    case IDC_BTN_CUSTOMIZE:
        /* run the visual settings dialog */
        prefs_->run_appearance_dlg(GetParent(handle_),
                                   prefs_->get_game_win(), FALSE);

        /* handled */
        return TRUE;

    case IDC_BTN_DEFAULT:
        /* make sure they really want to proceed */
        if (MessageBox(GetParent(handle_),
                       "Resetting will discard any customizations you've "
                       "made to this theme's fonts, colors, and other "
                       "visual settings.  Are you sure you want to "
                       "discard your changes and reset the theme to its "
                       "default settings?", "TADS",
                       MB_YESNO | MB_ICONQUESTION) == IDYES)
        {
            /* reset the theme */
            prefs_->set_theme_defaults(prefs_->get_active_profile_name());

            /* save the changes */
            prefs_->save();

            /* notify the game window that everything has changed */
            prefs_->schedule_reformat(FALSE);
            prefs_->notify_sound_pref_change();
        }

        /* handled */
        return TRUE;
    }

    /* no special handling - inherit default handling */
    return CTadsDialogPropPage::do_command(id, ctl);
}

/* ------------------------------------------------------------------------ */
/*
 *   Keys dialog 
 */

class CHtmlDialogKeys: public CTadsDialogPropPage
{
public:
    CHtmlDialogKeys(CHtmlPreferences *prefs) { prefs_ = prefs; }
    void init();
    int do_notify(NMHDR *nm, int ctl);
    int do_command(WPARAM cmd, HWND ctl);
    
private:
    /* check for changes */
    int has_changes(int save);

    /* get the button states */
    void get_button_state(int *emacs_ctrl_v, int *arrow_scroll,
                          int *emacs_alt_v);

    CHtmlPreferences *prefs_;
};

/*
 *   initialize 
 */
void CHtmlDialogKeys::init()
{
    /* initialize controls with current preference settings */
    CheckRadioButton(handle_, IDC_RB_CTLV_EMACS, IDC_RB_CTLV_PASTE,
                     prefs_->get_emacs_ctrl_v()
                     ? IDC_RB_CTLV_EMACS : IDC_RB_CTLV_PASTE);
    CheckRadioButton(handle_, IDC_RB_ARROW_SCROLL, IDC_RB_ARROW_HIST,
                     prefs_->get_arrow_keys_always_scroll()
                     ? IDC_RB_ARROW_SCROLL : IDC_RB_ARROW_HIST);
    CheckRadioButton(handle_, IDC_RB_ALTV_EMACS, IDC_RB_ALTV_WIN,
                     prefs_->get_emacs_alt_v()
                     ? IDC_RB_ALTV_EMACS : IDC_RB_ALTV_WIN);
}

/*
 *   process notifications 
 */
int CHtmlDialogKeys::do_notify(NMHDR *nm, int)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* check for and save changes */
        has_changes(TRUE);

        /* handled */
        return TRUE;
    }
    return FALSE;
}

/*
 *   get the current state 
 */
void CHtmlDialogKeys::get_button_state(int *emacs_ctrl_v, int *arrow_scroll,
                                       int *emacs_alt_v)
{
    *emacs_ctrl_v = (IsDlgButtonChecked(handle_, IDC_RB_CTLV_EMACS)
                     == BST_CHECKED);
    *arrow_scroll = (IsDlgButtonChecked(handle_, IDC_RB_ARROW_SCROLL)
                     == BST_CHECKED);
    *emacs_alt_v = (IsDlgButtonChecked(handle_, IDC_RB_ALTV_EMACS)
                    == BST_CHECKED);
}

/*
 *   process commands 
 */
int CHtmlDialogKeys::do_command(WPARAM, HWND)
{
    /* check for changes */
    set_changed(has_changes(FALSE));

    /* handled */
    return TRUE;
}

/*
 *   check for changes 
 */
int CHtmlDialogKeys::has_changes(int save)
{
    int new_emacs_ctrl_v, new_arrow_scroll, new_emacs_alt_v;
    int ch;

    /* get the current button states */
    get_button_state(&new_emacs_ctrl_v, &new_arrow_scroll,
                     &new_emacs_alt_v);

    /* check for changes */
    ch = ((new_emacs_ctrl_v != 0) != (prefs_->get_emacs_ctrl_v() != 0)
          || ((new_arrow_scroll != 0)
              != (prefs_->get_arrow_keys_always_scroll() != 0))
          || ((new_emacs_alt_v != 0) != (prefs_->get_emacs_alt_v() != 0)));


    /* save changes if desired */
    if (ch && save)
    {
        /* save the current selection state in the preferences */
        prefs_->set_emacs_ctrl_v(new_emacs_ctrl_v);
        prefs_->set_arrow_keys_always_scroll(new_arrow_scroll);
        prefs_->set_emacs_alt_v(new_emacs_alt_v);
    }
    
    /* return the change indication */
    return ch;
}

/* ------------------------------------------------------------------------ */
/*
 *   File safety level dialog 
 */

class CHtmlDialogSafety: public CTadsDialogPropPage
{
public:
    CHtmlDialogSafety(CHtmlPreferences *prefs)
    {
        /* save the preferences object */
        prefs_ = prefs;
    }
    void init();
    int do_notify(NMHDR *nm, int ctl);
    int do_command(WPARAM cmd, HWND ctl);

private:
    /* check for changes */
    int has_changes(int save);

    /* level selected on initialization or last "apply" */
    int saved_level_;

    /* 
     *   array of our button identifiers; the array index is the
     *   corresponding safety level setting 
     */
    static int buttons_[];

    /* preferences object */
    CHtmlPreferences *prefs_;
};

int CHtmlDialogSafety::buttons_[] =
{
    IDC_RB_SAFETY0,
    IDC_RB_SAFETY1,
    IDC_RB_SAFETY2,
    IDC_RB_SAFETY3,
    IDC_RB_SAFETY4
};

/*
 *   initialize 
 */
void CHtmlDialogSafety::init()
{
    /* note the initial button setting */
    saved_level_ = prefs_->get_file_safety_level();

    /* initialize the appropriate radio button for the current setting */
    CheckRadioButton(handle_, IDC_RB_SAFETY0, IDC_RB_SAFETY4,
                     buttons_[saved_level_]);
}

/*
 *   handle a command 
 */
int CHtmlDialogSafety::do_command(WPARAM id, HWND ctl)
{
    /* check the control */
    switch(id)
    {
    case IDC_RB_SAFETY0:
    case IDC_RB_SAFETY1:
    case IDC_RB_SAFETY2:
    case IDC_RB_SAFETY3:
    case IDC_RB_SAFETY4:
        /* note the change */
        set_changed(has_changes(FALSE));

        /* handled */
        return TRUE;

    default:
        /* inherit default handling */
        return CTadsDialogPropPage::do_command(id, ctl);
    }
}

/*
 *   process notifications 
 */
int CHtmlDialogSafety::do_notify(NMHDR *nm, int)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* check for and save changes */
        has_changes(TRUE);

        /* note the button as of this last save */
        saved_level_ = prefs_->get_file_safety_level();

        /* handled */
        return TRUE;
    }

    /* not handled */
    return FALSE;
}

/*
 *   check for changes 
 */
int CHtmlDialogSafety::has_changes(int save)
{
    int level;
    int ch;

    /* get the current button setting */
    for (level = 0 ; level < sizeof(buttons_)/sizeof(buttons_[0]) ; ++level)
    {
        /* if this button is checked, it's the new level */
        if (IsDlgButtonChecked(handle_, buttons_[level]) == BST_CHECKED)
        {
            /* no need to look further */
            break;
        }
    }

    /* if this is different from the button last selected, so note */
    ch = (level != saved_level_);

    /* if saving, update the preferences */
    if (save && ch)
        prefs_->set_file_safety_level(level);

    /* indicate if there was a change */
    return ch;
}


/* ------------------------------------------------------------------------ */
/*
 *   Memory dialog 
 */

class CHtmlDialogMem: public CTadsDialogPropPage
{
public:
    CHtmlDialogMem(CHtmlPreferences *prefs)
    {
        /* save the preferences object */
        prefs_ = prefs;
    }
    void init();
    int do_notify(NMHDR *nm, int ctl);
    int do_command(WPARAM cmd, HWND ctl);

private:
    /* check for changes */
    int has_changes(int save);
    
    /* preferences object */
    CHtmlPreferences *prefs_;
};

/* 
 *   initialize 
 */
void CHtmlDialogMem::init()
{
    HWND pop;
    int i;
    static const char *settings[] =
    {
        "No Limit",
        "32 KBytes",
        "64 KBytes",
        "96 KBytes",
        "128 KBytes",
    };
    int settings_count;
    int idx;

    /* calculate the number of items in our list */
    settings_count  = sizeof(settings)/sizeof(settings[0]);
    
    /* add the possible settings to the popup */
    pop = GetDlgItem(handle_, IDC_POP_MEM);
    SendMessage(pop, CB_RESETCONTENT, 0, 0);
    for (i = 0 ; i < settings_count ; ++i)
        SendMessage(pop, CB_ADDSTRING, (WPARAM)0, (LPARAM)settings[i]);

    /* 
     *   Select the appropriate list item.  Get the current setting, and
     *   divide it by 32K to get the nearest appropriate index in our
     *   list.  If the current memory limit is zero, it means that we have
     *   no limit; our first element is conveniently "No Limit", hence a
     *   limit of zero is also an index of zero. 
     */
    idx = (int)(prefs_->get_mem_text_limit() / (unsigned long)32768);
    if (idx >= settings_count)
        idx = settings_count;

    /* select the list item */
    SendMessage(pop, CB_SETCURSEL, (WPARAM)idx, 0);
}

/*
 *   check for changes 
 */
int CHtmlDialogMem::has_changes(int save)
{
    int dlg_idx;
    int pref_idx;
    
    /* get the current dialog and preference settings */
    dlg_idx = SendMessage(GetDlgItem(handle_, IDC_POP_MEM),
                          CB_GETCURSEL, 0, 0);
    pref_idx = (int)(prefs_->get_mem_text_limit() / (unsigned long)32768);

    /* save the change if desired */
    if (save && dlg_idx >= 0 && dlg_idx != pref_idx)
    {
        /* 
         *   Set the new memory size in bytes.  Each element of the list is a
         *   32K increment, so multiply the index by 32768 to get the byte
         *   size.  Note that the first list item (at index 0) is "No Limit",
         *   hence our calculation will give us zero in this case; this is
         *   just what we want, since a value of zero bytes indicates
         *   unlimited memory.  
         */
        prefs_->set_mem_text_limit((unsigned long)dlg_idx
                                   * (unsigned long)32768);
    }

    /* return the change indication */
    return (dlg_idx != pref_idx);
}

/*
 *   handle a command 
 */
int CHtmlDialogMem::do_command(WPARAM id, HWND ctl)
{
    if (HIWORD(id) == CBN_SELCHANGE)
    {
        /* note the change */
        set_changed(has_changes(FALSE));

        /* handled */
        return TRUE;
    }

    /* not handled */
    return FALSE;
}

/*
 *   process notifications 
 */
int CHtmlDialogMem::do_notify(NMHDR *nm, int)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* check for and save changes */
        has_changes(TRUE);

        /* handled */
        return TRUE;
    }

    /* not handled */
    return FALSE;
}


/* ------------------------------------------------------------------------ */
/*
 *   Quitting dialog 
 */

class CHtmlDialogQuit: public CTadsDialogPropPage
{
public:
    CHtmlDialogQuit(CHtmlPreferences *prefs)
    {
        /* save the preferences object */
        prefs_ = prefs;
    }
    void init();
    int do_notify(NMHDR *nm, int ctl);
    int do_command(WPARAM cmd, HWND ctl);

private:
    /* check for changes */
    int has_changes(int save);

    /* preferences object */
    CHtmlPreferences *prefs_;
};

/* 
 *   initialize 
 */
void CHtmlDialogQuit::init()
{
    int btn;
    
    /* figure out which "close action" button should be selected initially */
    switch(prefs_->get_close_action())
    {
    case HTML_PREF_CLOSE_CMD:
        btn = IDC_RB_CLOSE_QUITCMD;
        break;

    case HTML_PREF_CLOSE_PROMPT:
        btn = IDC_RB_CLOSE_PROMPT;
        break;

    case HTML_PREF_CLOSE_NOW:
        btn = IDC_RB_CLOSE_IMMEDIATE;
        break;
    }

    /* initialize controls with current preference settings */
    CheckRadioButton(handle_, IDC_RB_CLOSE_QUITCMD, IDC_RB_CLOSE_IMMEDIATE,
                     btn);

    /* figure out which "post-quit action" button to select initially */
    switch(prefs_->get_postquit_action())
    {
    case HTML_PREF_POSTQUIT_EXIT:
        btn = IDC_RB_POSTQUIT_EXIT;
        break;

    case HTML_PREF_POSTQUIT_KEEP:
        btn = IDC_RB_POSTQUIT_KEEP;
        break;
    }

    /* initialize controls with current preference settings */
    CheckRadioButton(handle_, IDC_RB_POSTQUIT_EXIT, IDC_RB_POSTQUIT_KEEP,
                     btn);
}

/*
 *   handle a command 
 */
int CHtmlDialogQuit::do_command(WPARAM id, HWND ctl)
{
    switch(id)
    {
    case IDC_RB_CLOSE_QUITCMD:
    case IDC_RB_CLOSE_PROMPT:
    case IDC_RB_CLOSE_IMMEDIATE:
    case IDC_RB_POSTQUIT_EXIT:
    case IDC_RB_POSTQUIT_KEEP:
        /* check for changes */
        set_changed(has_changes(FALSE));

        /* handled */
        return TRUE;
        
    default:
        /* inherit default handling */
        return CTadsDialogPropPage::do_command(id, ctl);
    }
}

/*
 *   process notifications 
 */
int CHtmlDialogQuit::do_notify(NMHDR *nm, int)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* check for and save changes */
        has_changes(TRUE);

        /* handled */
        return TRUE;
    }
        
    /* not handled */
    return FALSE;
}

/*
 *   check for changes 
 */
int CHtmlDialogQuit::has_changes(int save)
{
    int ch;
    int quit_stat;
    int close_stat;

    /* determine the new "close" setting */
    if (IsDlgButtonChecked(handle_, IDC_RB_CLOSE_QUITCMD))
        close_stat = HTML_PREF_CLOSE_CMD;
    else if (IsDlgButtonChecked(handle_, IDC_RB_CLOSE_PROMPT))
        close_stat = HTML_PREF_CLOSE_PROMPT;
    else
        close_stat = HTML_PREF_CLOSE_NOW;

    /* note changes */
    ch = (close_stat != prefs_->get_close_action());

    /* determine the new "post quit" setting */
    if (IsDlgButtonChecked(handle_, IDC_RB_POSTQUIT_EXIT))
        quit_stat = HTML_PREF_POSTQUIT_EXIT;
    else
        quit_stat = HTML_PREF_POSTQUIT_KEEP;

    /* note changes */
    ch |= (quit_stat != prefs_->get_postquit_action());

    /* save the new settings if desired */
    if (ch && save)
    {
        prefs_->set_postquit_action(quit_stat);
        prefs_->set_close_action(close_stat);
    }

    /* return the change indication */
    return ch;
}


/* ------------------------------------------------------------------------ */
/*
 *   Starting dialog 
 */

class CHtmlDialogStart: public CTadsDialogPropPage
{
public:
    CHtmlDialogStart(CHtmlPreferences *prefs)
    {
        /* save the preferences object */
        prefs_ = prefs;
    }
    void init();
    int do_notify(NMHDR *nm, int ctl);
    int do_command(WPARAM cmd, HWND ctl);

private:
    /* check for changes */
    int has_changes(int save);

    /* preferences object */
    CHtmlPreferences *prefs_;
};

/* 
 *   initialize 
 */
void CHtmlDialogStart::init()
{
    /* set the initial "ask for game" button state */
    CheckDlgButton(handle_, IDC_CK_ASKGAME,
                   prefs_->get_init_ask_game()
                   ? BST_CHECKED : BST_UNCHECKED);

    /* set the initial game folder field value */
    SetWindowText(GetDlgItem(handle_, IDC_FLD_INITFOLDER),
                  prefs_->get_init_open_folder());
}

/*
 *   handle a command 
 */
int CHtmlDialogStart::do_command(WPARAM id, HWND ctl)
{
    textchar_t fname[OSFNMAX];

    switch(LOWORD(id))
    {
    case IDC_FLD_INITFOLDER:
        /* if it's an EN_CHANGE notification, note the change */
        if (HIWORD(id) == EN_CHANGE)
        {
            /* check for changes */
            set_changed(has_changes(FALSE));

            /* handled */
            return TRUE;
        }

        /* nothing special; inherit default handling */
        break;
        
    case IDC_BTN_BROWSE:
        /* get the initial setting from the field */
        GetWindowText(GetDlgItem(handle_, IDC_FLD_INITFOLDER),
                      fname, sizeof(fname));

        /* if that's empty, use the current directory by default */
        if (fname[0] == '\0')
            GetCurrentDirectory(sizeof(fname), fname);
        
        /* run the folder selector */
        if (CTadsDialogFolderSel2::
            run_select_folder(GetParent(handle_),
                              CTadsApp::get_app()->get_instance(),
                              "&Initial \"Open\" Folder:",
                              "Select Initial Folder", fname, sizeof(fname),
                              fname, 0, 0))
        {
            /* set the field to the new value */
            SetWindowText(GetDlgItem(handle_, IDC_FLD_INITFOLDER), fname);

            /* note the change */
            set_changed(has_changes(FALSE));
        }

        /* handled */
        return TRUE;

    case IDC_CK_ASKGAME:
        /* check for changes */
        set_changed(has_changes(FALSE));

        /* handled */
        return TRUE;

    default:
        /* no special handling - go use default */
        break;
    }

    /* inherit default handling */
    return CTadsDialogPropPage::do_command(id, ctl);
}

/*
 *   process notifications 
 */
int CHtmlDialogStart::do_notify(NMHDR *nm, int)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* check for and save changes */
        if (has_changes(TRUE))
        {
            /* 
             *   make the new starting folder active for the next "open"
             *   dialog, since most people would be confused if this change
             *   didn't show up in the next dialog 
             */
            CTadsApp::get_app()->set_openfile_path(
                prefs_->get_init_open_folder());
        }

        /* handled */
        return TRUE;
    }

    /* not handled */
    return FALSE;
}

/* 
 *   check for changes 
 */
int CHtmlDialogStart::has_changes(int save)
{
    char init_folder[OSFNMAX];
    int ch;
    int ask_stat;
    
    /* check the initial folder value */
    GetWindowText(GetDlgItem(handle_, IDC_FLD_INITFOLDER),
                  init_folder, sizeof(init_folder));

    /* check for a change */
    ch = (stricmp(init_folder, prefs_->get_init_open_folder()) != 0);

    /* check the "ask for game" checkbox status */
    ask_stat = (IsDlgButtonChecked(handle_, IDC_CK_ASKGAME) == BST_CHECKED);
    ch |= ((ask_stat != 0) != (prefs_->get_init_ask_game() != 0));

    /* save changes if desired */
    if (ch && save)
    {
        prefs_->set_init_open_folder(init_folder);
        prefs_->set_init_ask_game(ask_stat);
    }

    /* return the change indication */
    return ch;
}


/* ------------------------------------------------------------------------ */
/*
 *   Game Chest preferences page 
 */

class CHtmlDialogGameChest: public CTadsDialogPropPage
{
public:
    CHtmlDialogGameChest(CHtmlPreferences *prefs)
    {
        /* save the preferences object */
        prefs_ = prefs;
    }
    void init();
    int do_notify(NMHDR *nm, int ctl);
    int do_command(WPARAM cmd, HWND ctl);

private:
    /* check for changes */
    int has_changes(int save);

    /* browse for a file */
    void browse_for_file(int ctl_id, const char *filter,
                         const char *prompt, DWORD flags)
    {
        OPENFILENAME info;
        char fname[OSFNMAX];
        char dir[OSFNMAX];

        /* get the initial setting from the field */
        GetWindowText(GetDlgItem(handle_, ctl_id), fname, sizeof(fname));

        /* if that's empty, use the current directory by default */
        if (fname[0] == '\0')
        {
            /* use the current directory */
            GetCurrentDirectory(sizeof(dir), dir);
        }
        else
        {
            textchar_t *root;

            /* use the directory from the selected file */
            os_get_path_name(dir, sizeof(dir), fname);

            /* extract just the filename portion */
            root = os_get_root_name(fname);
            memmove(fname, root, strlen(root) + 1);
        }

        /* set up to browse for a file */
        info.lStructSize = sizeof(info);
        info.hwndOwner = GetParent(handle_);
        info.hInstance = CTadsApp::get_app()->get_instance();
        info.lpstrFilter = filter;
        info.lpstrCustomFilter = 0;
        info.nFilterIndex = 0;
        info.lpstrFile = fname;
        info.nMaxFile = sizeof(fname);
        info.lpstrFileTitle = 0;
        info.nMaxFileTitle = 0;
        info.lpstrTitle = prompt;
        info.Flags = flags
                     | OFN_PATHMUSTEXIST
                     | OFN_HIDEREADONLY
                     | OFN_NOCHANGEDIR;
        info.nFileOffset = 0;
        info.nFileExtension = 0;
        info.lpstrDefExt = 0;
        info.lCustData = 0;
        info.lpfnHook = 0;
        info.lpTemplateName = 0;
        info.lpstrInitialDir = dir;
        CTadsDialog::set_filedlg_center_hook(&info);

        /* ask for the filename */
        if (GetOpenFileName(&info))
        {
            /* store the new file in the field */
            SetWindowText(GetDlgItem(handle_, ctl_id), fname);

            /* note the change */
            set_changed(has_changes(FALSE));
        }
    }

    /* check for a changed filename */
    int fname_changed(int ctl_id, const textchar_t *oldval,
                      char *buf, size_t buflen)
    {
        char newval[OSFNMAX];

        /* if the caller didn't pass a buffer, use our default */
        if (buf == 0)
        {
            buf = newval;
            buflen = sizeof(newval);
        }

        /* read the new value */
        GetWindowText(GetDlgItem(handle_, ctl_id), buf, buflen);

        /* compare it to the old value */
        return (stricmp(buf, oldval != 0 ? oldval : "") != 0);
    }

    /* preferences object */
    CHtmlPreferences *prefs_;
};

/* 
 *   initialize 
 */
void CHtmlDialogGameChest::init()
{
    /* set the initial database file value */
    SetWindowText(GetDlgItem(handle_, IDC_FLD_GC_FILE),
                  prefs_->get_gc_database());

    /* set the initial background image */
    SetWindowText(GetDlgItem(handle_, IDC_FLD_GC_BKG),
                  prefs_->get_gc_bkg());
}

/*
 *   check for changes 
 */
int CHtmlDialogGameChest::has_changes(int save)
{
    int ch;
    int gcdb_ch;
    char gc_file[OSFNMAX];
    char bkg[OSFNMAX];

    /* check the game chest database file */
    gcdb_ch = ch = fname_changed(IDC_FLD_GC_FILE, prefs_->get_gc_database(),
                                 gc_file, sizeof(gc_file));

    /* check the game chest background image file */
    ch |= fname_changed(IDC_FLD_GC_BKG, prefs_->get_gc_bkg(),
                        bkg, sizeof(bkg));

    /* save changes if desired */
    if (ch && save)
    {
        /* 
         *   always save the outgoing game chest file before changing the
         *   preference value 
         */
        prefs_->save_game_chest_db_as(prefs_->get_gc_database());

        /* 
         *   if we're changing the game chest database file, and the new file
         *   doesn't exist, save the current game chest settings to the new
         *   file 
         */
        if (gcdb_ch && osfacc(gc_file))
            prefs_->save_game_chest_db_as(gc_file);

        /* 
         *   make sure the new game chest file exists - we should have
         *   created it if it didn't exist before, so if it doesn't exist
         *   now, then there must be some problem (bad path, disk full, etc)
         *   that makes the selected file unusable 
         */
        if (gcdb_ch && osfacc(gc_file))
            MessageBox(GetParent(handle_), "Warning: The new Game Chest "
                       "file cannot be created.  This could be because the "
                       "folder path is invalid or the disk is full.  You "
                       "should use a different file.", "TADS",
                       MB_OK | MB_ICONEXCLAMATION);

        /* set the new values */
        prefs_->set_gc_database(gc_file);
        prefs_->set_gc_bkg(bkg);
    }

    /* return change indication */
    return ch;
}

/*
 *   handle a command 
 */
int CHtmlDialogGameChest::do_command(WPARAM id, HWND ctl)
{
    textchar_t fname[OSFNMAX];

    switch(LOWORD(id))
    {
    case IDC_FLD_GC_FILE:
    case IDC_FLD_GC_BKG:
        /* if it's an EN_CHANGE notification, note the change */
        if (HIWORD(id) == EN_CHANGE)
        {
            set_changed(has_changes(FALSE));
            return TRUE;
        }

        /* nothing special; inherit default handling */
        break;

    case IDC_BTN_DEFAULT:
        /* restore the default game chest file path */
        if (CTadsApp::get_my_docs_path(fname, sizeof(fname)))
        {
            char dir[OSFNMAX];

            /* build the full filename */
            strcpy(dir, fname);
            os_build_full_path(fname, sizeof(fname), dir,
                               "TADS\\GameChest.txt");
        }
        else
        {
            /* can't get the My Documents path - use the current directory */
            strcpy(fname, "GameChest.txt");
        }

        /* update the field */
        SetWindowText(GetDlgItem(handle_, IDC_FLD_GC_FILE), fname);

        /* check for changes */
        set_changed(has_changes(FALSE));

        /* handled */
        return TRUE;

    case IDC_BTN_DEFAULT2:
        /* restore the default game chest background image */
        SetWindowText(GetDlgItem(handle_, IDC_FLD_GC_BKG),
                      "exe:gamechest/bkg.png");

        /* check for changes */
        set_changed(has_changes(FALSE));

        /* handled */
        return TRUE;

    case IDC_BTN_BROWSE:
        /* browse for a new database file */
        browse_for_file(IDC_FLD_GC_FILE,
                        "Game Chest Database\0GameChest.txt\0",
                        "Select the Game Chest database file", 0);

        /* handled */
        return TRUE;

    case IDC_BTN_BROWSE2:
        /* browse for a new image file */
        browse_for_file(IDC_FLD_GC_BKG,
                        "Images\0*.jpg;*.jpeg;*.jpe;*.png\0",
                        "Select an image to use as the Game Chest background",
                        OFN_FILEMUSTEXIST);

        /* handled */
        return TRUE;

    default:
        /* no special handling - go use default */
        break;
    }

    /* inherit default handling */
    return CTadsDialogPropPage::do_command(id, ctl);
}

/*
 *   process notifications 
 */
int CHtmlDialogGameChest::do_notify(NMHDR *nm, int)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* check for and save changes */
        if (has_changes(TRUE))
        {
            /* we have changes - reformat the main window */
            prefs_->schedule_reload_game_chest();
        }

        /* handled */
        return TRUE;
    }

    /* not handled */
    return FALSE;
}


/* ------------------------------------------------------------------------ */
/*
 *   Preferences object
 */


/* custom color value name */
const textchar_t CHtmlPreferences::custclr_val_name[] =
    "Custom Colors";


/*
 *   construct 
 */
CHtmlPreferences::CHtmlPreferences()
{
    /* set an initial reference */
    refcnt_ = 1;
    
    /* create the property list */
    proplist_ = new CHtmlPropList();

    /* initialize the property names (for serialization purposes) */
    proplist_->set_prop_name(HTML_PREF_PROP_FONT, "Proportional Font");
    proplist_->set_prop_name(HTML_PREF_MONO_FONT, "Monospaced Font");
    proplist_->set_prop_name(HTML_PREF_FONT_SERIF, "TADS-Serif Font");
    proplist_->set_prop_name(HTML_PREF_FONT_SANS, "TADS-Sans Font");
    proplist_->set_prop_name(HTML_PREF_FONT_SCRIPT, "TADS-Script Font");
    proplist_->set_prop_name(HTML_PREF_FONT_TYPEWRITER,
                             "TADS-Typewriter Font");
    proplist_->set_prop_name(HTML_PREF_FONT_SCALE, "Font Scale");
    proplist_->set_prop_name(HTML_PREF_USE_WIN_COLORS, "Use Windows Colors");
    proplist_->set_prop_name(HTML_PREF_OVERRIDE_COLORS, "Override Colors");
    proplist_->set_prop_name(HTML_PREF_TEXT_COLOR, "Text Color");
    proplist_->set_prop_name(HTML_PREF_BG_COLOR, "Background Color");
    proplist_->set_prop_name(HTML_PREF_LINK_COLOR, "Link Color");
    proplist_->set_prop_name(HTML_PREF_HLINK_COLOR, "Hovering Link Color");
    proplist_->set_prop_name(HTML_PREF_ALINK_COLOR, "Active Link Color");
    proplist_->set_prop_name(HTML_PREF_VLINK_COLOR, "Visited Link Color");
    proplist_->set_prop_name(HTML_PREF_UNDERLINE_LINKS, "Underline Links");
    proplist_->set_prop_name(HTML_PREF_EMACS_CTRL_V, "Emacs Ctrl-V");
    proplist_->set_prop_name(HTML_PREF_EMACS_ALT_V, "Emacs Alt-V");
    proplist_->set_prop_name(HTML_PREF_ARROW_KEYS_ALWAYS_SCROLL,
                            "Arrows Keys Always Scroll");
    proplist_->set_prop_name(HTML_PREF_ALT_MORE_STYLE, "Alt More Style");
    proplist_->set_prop_name(HTML_PREF_WINPOS, "Window Position");
    proplist_->set_prop_name(HTML_PREF_DBGWINPOS, "Debug Window Position");
    proplist_->set_prop_name(HTML_PREF_DBGMAINPOS,
                             "Debug Main Window Position");
    proplist_->set_prop_name(HTML_PREF_MUTE_SOUND, "Mute All Sounds");
    proplist_->set_prop_name(HTML_PREF_MUSIC_ON, "Music");
    proplist_->set_prop_name(HTML_PREF_SOUNDS_ON, "Sound Effects");
    proplist_->set_prop_name(HTML_PREF_GRAPHICS_ON, "Graphics");
    proplist_->set_prop_name(HTML_PREF_LINKS_ON, "Show Links");
    proplist_->set_prop_name(HTML_PREF_LINKS_CTRL, "Show Links on Ctrl");
    proplist_->set_prop_name(HTML_PREF_DIRECTX_HIDEWARN,
                             "Hide DirectX Startup Warning");
    proplist_->set_prop_name(HTML_PREF_DIRECTX_ERROR_CODE,
                             "DirectX Initialization Result");
    proplist_->set_prop_name(HTML_PREF_FILE_SAFETY_LEVEL,
                             "File Safety Level");
    proplist_->set_prop_name(HTML_PREF_COLOR_STATUS_BG,
                             "Status line Background Color");
    proplist_->set_prop_name(HTML_PREF_COLOR_STATUS_TEXT,
                             "Status line Text Color");
    proplist_->set_prop_name(HTML_PREF_INPFONT_DEFAULT,
                             "Input Font Use Default");
    proplist_->set_prop_name(HTML_PREF_INPFONT_NAME,
                             "Input Font Name");
    proplist_->set_prop_name(HTML_PREF_INPFONT_COLOR,
                             "Input Font Color");
    proplist_->set_prop_name(HTML_PREF_INPFONT_BOLD,
                             "Input Font Bold");
    proplist_->set_prop_name(HTML_PREF_INPFONT_ITALIC,
                             "Input Font Italic");
    proplist_->set_prop_name(HTML_PREF_MEM_TEXT_LIMIT,
                             "Text Memory Limit");
    proplist_->set_prop_name(HTML_PREF_CLOSE_ACTION,
                             "Window-Close Action");
    proplist_->set_prop_name(HTML_PREF_POSTQUIT_ACTION,
                             "Post-Quit Action");
    proplist_->set_prop_name(HTML_PREF_INIT_ASK_GAME,
                             "Ask for Game at Startup");
    proplist_->set_prop_name(HTML_PREF_INIT_OPEN_FOLDER,
                             "Initial Open Folder");
    proplist_->set_prop_name(HTML_PREF_RECENT_1, "Recent Game 1");
    proplist_->set_prop_name(HTML_PREF_RECENT_2, "Recent Game 2");
    proplist_->set_prop_name(HTML_PREF_RECENT_3, "Recent Game 3");
    proplist_->set_prop_name(HTML_PREF_RECENT_4, "Recent Game 4");
    proplist_->set_prop_name(HTML_PREF_RECENT_ORDER, "Recent Game Order");
    proplist_->set_prop_name(HTML_PREF_RECENT_DBG_1, "Recent Debug Game 1");
    proplist_->set_prop_name(HTML_PREF_RECENT_DBG_2, "Recent Debug Game 2");
    proplist_->set_prop_name(HTML_PREF_RECENT_DBG_3, "Recent Debug Game 3");
    proplist_->set_prop_name(HTML_PREF_RECENT_DBG_4, "Recent Debug Game 4");
    proplist_->set_prop_name(HTML_PREF_RECENT_DBG_ORDER,
                             "Recent Debug Game Order");
    proplist_->set_prop_name(HTML_PREF_TOOLBAR_VIS, "Show Toolbar");
    proplist_->set_prop_name(HTML_PREF_SHOW_TIMER, "Show Timer");
    proplist_->set_prop_name(HTML_PREF_SHOW_TIMER_SECONDS,
                             "Show Seconds in Timer");
    proplist_->set_prop_name(HTML_PREF_PROP_FONTSZ, "Proportional Font Size");
    proplist_->set_prop_name(HTML_PREF_MONO_FONTSZ, "Monospaced Font Size");
    proplist_->set_prop_name(HTML_PREF_SERIF_FONTSZ, "TADS-Serif Font Size");
    proplist_->set_prop_name(HTML_PREF_SANS_FONTSZ, "TADS-Sans Font Size");
    proplist_->set_prop_name(HTML_PREF_SCRIPT_FONTSZ,
                             "TADS-Script Font Size");
    proplist_->set_prop_name(HTML_PREF_TYPEWRITER_FONTSZ,
                             "TADS-Typewriter Font Size");
    proplist_->set_prop_name(HTML_PREF_INPUT_FONTSZ, "Input Font Size");
    proplist_->set_prop_name(HTML_PREF_GC_DATABASE, "Game Chest Database");
    proplist_->set_prop_name(HTML_PREF_GC_BKG, "Game Chest Background Image");
    proplist_->set_prop_name(HTML_PREF_PROFILE_DESC, "Profile Description");
    proplist_->set_prop_name(HTML_PREF_DEFAULT_PROFILE, "Default Profile");

    /* 
     *   Mark certain properties as global, so that they're the same in all
     *   profiles.  The settings that are local are the fonts, colors, MORE
     *   style, music/sound on/off, graphics on/off, and link visibility;
     *   everything else is global.  
     */
    proplist_->get_prop(HTML_PREF_FONT_SCALE)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_EMACS_CTRL_V)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_EMACS_ALT_V)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_ARROW_KEYS_ALWAYS_SCROLL)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_WINPOS)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_DBGWINPOS)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_DBGMAINPOS)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_MUTE_SOUND)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_DIRECTX_HIDEWARN)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_DIRECTX_ERROR_CODE)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_FILE_SAFETY_LEVEL)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_MEM_TEXT_LIMIT)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_CLOSE_ACTION)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_POSTQUIT_ACTION)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_INIT_ASK_GAME)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_INIT_OPEN_FOLDER)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_RECENT_1)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_RECENT_2)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_RECENT_3)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_RECENT_4)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_RECENT_ORDER)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_RECENT_DBG_1)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_RECENT_DBG_2)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_RECENT_DBG_3)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_RECENT_DBG_4)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_RECENT_DBG_ORDER)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_TOOLBAR_VIS)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_SHOW_TIMER)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_SHOW_TIMER_SECONDS)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_GC_DATABASE)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_GC_BKG)->set_global(TRUE);
    proplist_->get_prop(HTML_PREF_DEFAULT_PROFILE)->set_global(TRUE);

    /* 
     *   Mark certain properties as unsynchronized (i.e., last saved change
     *   wins).  These properties are not resynchronized on UI activation,
     *   but are merely stored when we save our changes, ignoring any changes
     *   made by other instances.  
     */
    proplist_->get_prop(HTML_PREF_FONT_SCALE)->set_synchronized(FALSE);
    proplist_->get_prop(HTML_PREF_MUTE_SOUND)->set_synchronized(FALSE);
    proplist_->get_prop(HTML_PREF_WINPOS)->set_synchronized(FALSE);
    proplist_->get_prop(HTML_PREF_DBGWINPOS)->set_synchronized(FALSE);
    proplist_->get_prop(HTML_PREF_DBGMAINPOS)->set_synchronized(FALSE);
    proplist_->get_prop(HTML_PREF_TOOLBAR_VIS)->set_synchronized(FALSE);
    proplist_->get_prop(HTML_PREF_SHOW_TIMER)->set_synchronized(FALSE);
    proplist_->get_prop(HTML_PREF_SHOW_TIMER_SECONDS)
        ->set_synchronized(FALSE);

    /* if the standard profiles don't exist, initialize them to defaults */
    init_standard_profiles();

    /* register or look up our global "preferences updated" message ID */
    prefs_updated_msg_ = RegisterWindowMessage("htmltads.prefs_updated_msg");
}

/* 
 *   delete 
 */
CHtmlPreferences::~CHtmlPreferences()
{
    /* delete the property list */
    delete proplist_;
}

/*
 *   Establish the theme settings for a standard profile.  
 */
void CHtmlPreferences::set_theme_defaults(const textchar_t *theme)
{
    /* set the standard description for the theme */
    set_std_profile_desc(theme);

    /* establish the base font settings */
    set_prop_font("Times New Roman");
    set_prop_fontsz(12);
    set_mono_font("Courier New");
    set_mono_fontsz(11);
    set_font_serif("Times New Roman");
    set_serif_fontsz(12);
    set_font_sans("Arial");
    set_sans_fontsz(11);
    set_font_script("Comic Sans MS");
    set_script_fontsz(11);
    set_font_typewriter("Courier New");
    set_typewriter_fontsz(11);

    /* use game font for input font by default, but make it bold */
    set_inpfont_name("");
    set_inpfont_size(get_prop_fontsz());
    set_inpfont_color(COLORREF_to_HTML_color(GetSysColor(COLOR_WINDOWTEXT)));
    set_inpfont_bold(TRUE);
    set_inpfont_italic(FALSE);

    /* use system colors and the standard web browser color scheme */
    set_use_win_colors(TRUE);
    set_override_colors(FALSE);
    set_text_color(COLORREF_to_HTML_color(GetSysColor(COLOR_WINDOWTEXT)));
    set_bg_color(COLORREF_to_HTML_color(GetSysColor(COLOR_WINDOW)));

    /* 
     *   use black text on light gray ("silver," in html parlance) background
     *   for default status line color scheme 
     */
    set_color_status_text(0x000000);
    set_color_status_bg(0xc0c0c0);

    /* use the "alternative" more style */
    set_alt_more_style(TRUE);

    /* turn on music, sound effects, and graphics */
    set_music_on(TRUE);
    set_sounds_on(TRUE);
    set_graphics_on(TRUE);

    /* enable hyperlinks, and use the standard web browser look */
    set_links_on(TRUE);
    set_links_ctrl(FALSE);
    set_link_color(HTML_make_color(0x00, 0x00, 0xff));
    set_vlink_color(HTML_make_color(0x00, 0x80, 0x80));
    set_alink_color(HTML_make_color(0xff, 0x00, 0xff));
    set_hlink_color(HTML_make_color(0xff, 0x00, 0x00));
    set_underline_links(TRUE);
    set_hover_hilite(FALSE);

    /* apply overrides for the Web Style theme */
    if (stricmp(theme, "Web Style") == 0)
    {
        /* make some changes to the base fonts for this theme */
        set_prop_font("Verdana");
        set_prop_fontsz(11);
        set_font_sans("Verdana");
        set_sans_fontsz(11);
        set_mono_fontsz(12);
        set_typewriter_fontsz(12);
    }

    /* apply overrides for the Plain Text theme */
    if (stricmp(theme, "Plain Text") == 0)
    {
        /* make some changes to the base fonts */
        set_prop_font(get_mono_font());
        set_prop_fontsz(get_mono_fontsz());
        set_inpfont_size(get_mono_fontsz());
        set_serif_fontsz(11);
        set_script_fontsz(11);
        set_sans_fontsz(10);
        set_inpfont_name("");
        set_inpfont_color(HTML_make_color(0xff, 0xff, 0xff));
        set_inpfont_bold(FALSE);
        set_inpfont_italic(FALSE);

        /* use a special white-on-navy color scheme */
        set_use_win_colors(FALSE);
        set_text_color(HTML_make_color(0xc0, 0xc0, 0xc0));
        set_bg_color(HTML_make_color(0x00, 0x00, 0x80)); 
        set_color_status_text(HTML_make_color(0x00, 0x00, 0x80));
        set_color_status_bg(HTML_make_color(0xc0, 0xc0, 0xc0));
        set_link_color(HTML_make_color(0xc0, 0xff, 0xff));
        set_vlink_color(HTML_make_color(0xc0, 0xff, 0xff));
        set_alink_color(HTML_make_color(0xff, 0x00, 0xff));
        set_hlink_color(HTML_make_color(0xff, 0x00, 0x80));
    }
}


/*
 *   Initialize a standard profile 
 */
void CHtmlPreferences::init_standard_profiles()
{
    CHtmlRect rc;
    char fname[OSFNMAX];

    /* set the basic global defaults */
    set_font_scale(2);
    set_emacs_ctrl_v(FALSE);
    set_emacs_alt_v(FALSE);
    set_arrow_keys_always_scroll(FALSE);
    set_mute_sound(FALSE);
    set_directx_hidewarn(FALSE);
    set_directx_error_code(HTMLW32_DIRECTX_OK);
    set_mem_text_limit(65536);
    set_close_action(HTML_PREF_CLOSE_PROMPT);
    set_postquit_action(HTML_PREF_POSTQUIT_KEEP);
    set_init_ask_game(FALSE);
    set_init_open_folder("");
    set_recent_game('0', "");
    set_recent_game('1', "");
    set_recent_game('2', "");
    set_recent_game('3', "");
    set_recent_game_order("0123");
    set_recent_dbg_game('0', "");
    set_recent_dbg_game('1', "");
    set_recent_dbg_game('2', "");
    set_recent_dbg_game('3', "");
    set_recent_dbg_game_order("0123");
    set_toolbar_vis(TRUE);
    set_show_timer(TRUE);
    set_show_timer_seconds(TRUE);
    set_default_profile("Multimedia");

    /* set up default initial window positions */
    rc.set(50, 50, 500, 600);
    set_win_pos(&rc);
    set_dbgwin_pos(&rc);
    rc.set(25, 25, 400, 50);
    set_dbgmain_pos(&rc);

    /* 
     *   By default, use the safety settings in the preferences (so don't use
     *   a temporary setting).  If it's not stored, default it to 2
     *   (read/write current directory only).  The run-time will set a
     *   temporary setting if the user specified a command line setting.  
     */
    set_file_safety_level(2);
    temp_file_safety_level_set_ = FALSE;

    /* by default, the Game Chest database file goes in My Documents\TADS */
    if (CTadsApp::get_my_docs_path(fname, sizeof(fname)))
    {
        char dir[OSFNMAX];

        /* build the full filename */
        strcpy(dir, fname);
        os_build_full_path(fname, sizeof(fname), dir, "TADS\\GameChest.txt");
    }
    else
    {
        /* can't get the My Documents path - use the current directory */
        strcpy(fname, "GameChest.txt");
    }
    set_gc_database(fname);

    /* set the Game Chest background image to the built-in default */
    set_gc_bkg("exe:gamechest/bkg.png");

    /* set the default settings for the "Multimedia" profile first */
    set_theme_defaults("Multimedia");

    /*
     *   Load any old-style, pre-profile preference settings.  This will
     *   ensure that if the user has just upgraded from a pre-profile
     *   version, we'll carry their settings forward into the new standard
     *   profiles.  Loading a null profile loads the pre-profile settings.
     *   Note that it's important that we do this *after* initializing the
     *   defaults: if there are any old settings, this will load them over
     *   the defaults, otherwise it'll just leave the defaults as they are.  
     */
    restore_as(0, FALSE);

    /* 
     *   The current preferences in memory are now set up the way we want
     *   them by default for the Multimedia profile: we've set all of the
     *   basic defaults, and we've loaded any existing pre-profile
     *   preferences over those, so we either have the Multimedia defaults or
     *   the user's old pre-profile settings.  Save these as the new
     *   Multimedia profile, if that profile doesn't already exist.  
     */
    if (!profile_exists("Multimedia"))
        save_as("Multimedia");

    /* if the Web Style profile doesn't exist, create it */
    if (!profile_exists("Web Style"))
    {
        /* re-initialize with the Web Style settings */
        set_theme_defaults("Web Style");

        /* save the settings */
        save_as("Web Style");
    }

    /* if the Plain Text theme doesn't exist, create it */
    if (!profile_exists("Plain Text"))
    {
        /* load the defaults for the Plain Text theme */
        set_theme_defaults("Plain Text");
        
        /* save the settings */
        save_as("Plain Text");
    }
}

/*
 *   Set the current profile description to the profile description of one of
 *   the standard pre-defined profiles.  
 */
void CHtmlPreferences::set_std_profile_desc(const textchar_t *profile)
{
    int id;
    char buf[256];

    /* get the resource string ID for the desired standard profile */
    if (stricmp(profile, "Multimedia") == 0)
        id = IDS_THEMEDESC_MULTIMEDIA;
    else if (stricmp(profile, "Plain Text") == 0)
        id = IDS_THEMEDESC_PLAIN_TEXT;
    else if (stricmp(profile, "Web Style") == 0)
        id = IDS_THEMEDESC_WEB_STYLE;
    else
        return;

    /* load the selected resource string */
    LoadString(CTadsApp::get_app()->get_instance(), id, buf, sizeof(buf));

    /* save it as the current profile description */
    set_profile_desc(buf);
}

/*
 *   Run the preferences dialog 
 */
void CHtmlPreferences::run_preferences_dlg(HWND hwndOwner,
                                           CHtmlSysWin_win32 *win)
{
    PROPSHEETPAGE psp[20];
    PROPSHEETHEADER psh;
    CHtmlDialogAppearance *appearance_dlg;
    CHtmlDialogKeys *keys_dlg;
    CHtmlDialogSafety *safety_dlg;
    CHtmlDialogMem *mem_dlg;
    CHtmlDialogStart *start_dlg;
    CHtmlDialogQuit *quit_dlg;
    CHtmlDialogGameChest *gc_dlg;
    int i;
    
    /* remember the owning window */
    win_ = win;

    /* create our dialog objects */
    appearance_dlg = new CHtmlDialogAppearance(this);
    keys_dlg = new CHtmlDialogKeys(this);
    safety_dlg = new CHtmlDialogSafety(this);
    mem_dlg = new CHtmlDialogMem(this);
    start_dlg = new CHtmlDialogStart(this);
    quit_dlg = new CHtmlDialogQuit(this);
    gc_dlg = new CHtmlDialogGameChest(this);

    /* 
     *   set up the page descriptors 
     */

    /* no pages yet */
    i = 0;

    /* set up the Appearance page */
    psp[i].dwSize = sizeof(PROPSHEETPAGE);
    psp[i].dwFlags = PSP_USETITLE;
    psp[i].hInstance = CTadsApp::get_app()->get_instance();
    psp[i].pszTemplate = MAKEINTRESOURCE(DLG_APPEARANCE);
    psp[i].pszIcon = 0;
    psp[i].pszTitle = MAKEINTRESOURCE(IDS_APPEARANCE);
    appearance_dlg->prepare_prop_page(&psp[i]);
    ++i;

    /* set up the Keyboard page */
    psp[i].dwSize = sizeof(PROPSHEETPAGE);
    psp[i].dwFlags = PSP_USETITLE;
    psp[i].hInstance = CTadsApp::get_app()->get_instance();
    psp[i].pszTemplate = MAKEINTRESOURCE(DLG_KEYS);
    psp[i].pszIcon = 0;
    psp[i].pszTitle = MAKEINTRESOURCE(IDS_KEYS);
    keys_dlg->prepare_prop_page(&psp[i]);
    ++i;

    /* set up the File Safety Level page */
    psp[i].dwSize = sizeof(PROPSHEETPAGE);
    psp[i].dwFlags = PSP_USETITLE;
    psp[i].hInstance = CTadsApp::get_app()->get_instance();
    psp[i].pszTemplate = MAKEINTRESOURCE(DLG_FILE_SAFETY);
    psp[i].pszIcon = 0;
    psp[i].pszTitle = MAKEINTRESOURCE(IDS_FILE_SAFETY);
    safety_dlg->prepare_prop_page(&psp[i]);
    ++i;

    /* set up the Memory page */
    psp[i].dwSize = sizeof(PROPSHEETPAGE);
    psp[i].dwFlags = PSP_USETITLE;
    psp[i].hInstance = CTadsApp::get_app()->get_instance();
    psp[i].pszTemplate = MAKEINTRESOURCE(DLG_MEM);
    psp[i].pszIcon = 0;
    psp[i].pszTitle = MAKEINTRESOURCE(IDS_MEM);
    mem_dlg->prepare_prop_page(&psp[i]);
    ++i;

    /* set up the Starting page */
    psp[i].dwSize = sizeof(PROPSHEETPAGE);
    psp[i].dwFlags = PSP_USETITLE;
    psp[i].hInstance = CTadsApp::get_app()->get_instance();
    psp[i].pszTemplate = MAKEINTRESOURCE(DLG_STARTING);
    psp[i].pszIcon = 0;
    psp[i].pszTitle = MAKEINTRESOURCE(IDS_STARTING);
    start_dlg->prepare_prop_page(&psp[i]);
    ++i;

    /* set up the Quitting page */
    psp[i].dwSize = sizeof(PROPSHEETPAGE);
    psp[i].dwFlags = PSP_USETITLE;
    psp[i].hInstance = CTadsApp::get_app()->get_instance();
    psp[i].pszTemplate = MAKEINTRESOURCE(DLG_QUITTING);
    psp[i].pszIcon = 0;
    psp[i].pszTitle = MAKEINTRESOURCE(IDS_QUITTING);
    quit_dlg->prepare_prop_page(&psp[i]);
    ++i;

    /* add the Game Chest page only if Game Chest is in the build */
    if (CHtmlSys_mainwin::is_game_chest_present())
    {
        psp[i].dwSize = sizeof(PROPSHEETPAGE);
        psp[i].dwFlags = PSP_USETITLE;
        psp[i].hInstance = CTadsApp::get_app()->get_instance();
        psp[i].pszTemplate = MAKEINTRESOURCE(DLG_GAMECHEST_PREFS);
        psp[i].pszIcon = 0;
        psp[i].pszTitle = MAKEINTRESOURCE(IDS_GAMECHEST_PREFS);
        gc_dlg->prepare_prop_page(&psp[i]);
        ++i;
    }

    /* set up the main dialog descriptor */
    psh.dwSize = PROPSHEETHEADER_V1_SIZE;
    psh.dwFlags = PSH_PROPSHEETPAGE;
    psh.hwndParent = hwndOwner;
    psh.hInstance = CTadsApp::get_app()->get_instance();
    psh.pszIcon = 0;
    psh.pszCaption = (LPSTR)"Options";
    psh.nPages = i;
    psh.nStartPage = 0;
    psh.ppsp = (LPCPROPSHEETPAGE)&psp;
    psh.pfnCallback = 0;

    /* run the property sheet */
    PropertySheet(&psh);

    /* delete the dialogs */
    delete appearance_dlg;
    delete keys_dlg;
    delete safety_dlg;
    delete mem_dlg;
    delete start_dlg;
    delete quit_dlg;
    delete gc_dlg;
}


/*
 *   Run the profiles dialog.  This is just like the preferences dialog, but
 *   only shows the "Appearance" tab for managing profiles.  
 */
void CHtmlPreferences::run_profiles_dlg(HWND hwndOwner,
                                        CHtmlSysWin_win32 *win)
{
    PROPSHEETPAGE psp[5];
    PROPSHEETHEADER psh;
    CHtmlDialogAppearance *appearance_dlg;
    int i;

    /* remember the owning window */
    win_ = win;

    /* create our dialog objects */
    appearance_dlg = new CHtmlDialogAppearance(this);

    /* 
     *   set up the page descriptors 
     */

    /* no pages yet */
    i = 0;

    /* set up the Appearance page */
    psp[i].dwSize = sizeof(PROPSHEETPAGE);
    psp[i].dwFlags = PSP_USETITLE;
    psp[i].hInstance = CTadsApp::get_app()->get_instance();
    psp[i].pszTemplate = MAKEINTRESOURCE(DLG_APPEARANCE);
    psp[i].pszIcon = 0;
    psp[i].pszTitle = MAKEINTRESOURCE(IDS_APPEARANCE);
    appearance_dlg->prepare_prop_page(&psp[i]);
    ++i;

    /* set up the main dialog descriptor */
    psh.dwSize = PROPSHEETHEADER_V1_SIZE;
    psh.dwFlags = PSH_PROPSHEETPAGE;
    psh.hwndParent = hwndOwner;
    psh.hInstance = CTadsApp::get_app()->get_instance();
    psh.pszIcon = 0;
    psh.pszCaption = (LPSTR)"Manage Themes";
    psh.nPages = i;
    psh.nStartPage = 0;
    psh.ppsp = (LPCPROPSHEETPAGE)&psp;
    psh.pfnCallback = 0;

    /* run the property sheet */
    PropertySheet(&psh);

    /* delete the dialogs */
    delete appearance_dlg;
}


/*
 *   Schedule reformatting of the window 
 */
void CHtmlPreferences::schedule_reformat(int more_mode)
{
    /* 
     *   if we're only reformatting MORE mode, and the window isn't in
     *   MORE mode, we don't need to reformat 
     */
    if (more_mode && !win_->in_more_mode())
        return;
    
    /* tell the window to schedule reformatting */
    win_->schedule_reformat(TRUE, TRUE, FALSE);
}

/*
 *   Notify the game window of a change in the sound preferences.
 */
void CHtmlPreferences::notify_sound_pref_change()
{
    /* tell the window about it */
    win_->notify_sound_pref_change();
}

/*
 *   Notify the game window of a change in links-enabled preferences 
 */
void CHtmlPreferences::notify_link_pref_change()
{
    /* tell the window about it */
    win_->notify_link_pref_change();
}

/*
 *   Schedule reloading the game chest data 
 */
void CHtmlPreferences::schedule_reload_game_chest()
{
    /* tell the window to schedule the reload */
    win_->schedule_reload_game_chest();
}

/*
 *   Save the game chest database to the given file 
 */
void CHtmlPreferences::save_game_chest_db_as(const char *fname)
{
    /* tell the window to save the database contents */
    win_->save_game_chest_db_as(fname);
}


/*
 *   Save the preferences 
 */
void CHtmlPreferences::save()
{
    /* save the settings under the active profile name */
    save_as(get_active_profile_name());
}

/*
 *   Save the current preferences under the given profile name, creating the
 *   profile name if it doesn't already exist.  This does NOT change the
 *   active profile - it simply saves the settings under the given profile
 *   name.  
 */
void CHtmlPreferences::save_as(const char *profile)
{
    int id;
    HKEY key;
    HKEY global_key;
    DWORD disposition;
    char key_name[256];

    /* get the key containing the settings */
    get_settings_key_for(key_name, sizeof(key_name), profile);

    /* open the registry key for our preference settings */
    key = CTadsRegistry::open_key(HKEY_CURRENT_USER, key_name,
                                  &disposition, TRUE);

    /* open the global key as well */
    global_key = CTadsRegistry::open_key(HKEY_CURRENT_USER, w32_pref_key_name,
                                         &disposition, TRUE);

    /* save the properties */
    for (id = (HTML_pref_id_t)0 ; id < HTML_PREF_LAST ; ++id)
    {
        int glob;

        /* check to see if it's global */
        glob = proplist_->get_prop(id)->is_global();

        /* write it under the appropriate key */
        write_to_registry((HTML_pref_id_t)id, glob ? global_key : key);
    }

    /* save the custom colors */
    CTadsRegistry::set_key_binary(key, custclr_val_name,
                                  cust_colors_, sizeof(cust_colors_));

    /* done with the registry key */
    CTadsRegistry::close_key(global_key);
    CTadsRegistry::close_key(key);

    /* 
     *   Broadcast a notification to all top-level windows in the system to
     *   let them know about the update.  This will allow any other running
     *   instances of this same application to check for changes in the
     *   registry and reload their preference settings to reflect the
     *   updates.  
     */
    PostMessage(HWND_BROADCAST, get_prefs_updated_msg(), 0, 0);
}

/*
 *   Restore the preferences for the currently active profile
 */
void CHtmlPreferences::restore(int synced_only)
{
    /* restore the settings for the active profile */
    restore_as(get_active_profile_name(), synced_only);
}

/*
 *   Restore the preferences for a given profile.  
 */
void CHtmlPreferences::restore_as(const char *profile, int synced_only)
{
    int id;
    HKEY key;
    HKEY global_key;
    DWORD disposition;
    char key_name[256];

    /* get the key containing the settings */
    get_settings_key_for(key_name, sizeof(key_name), profile);

    /* open the registry key for our preference settings */
    key = CTadsRegistry::open_key(HKEY_CURRENT_USER, key_name,
                                  &disposition, TRUE);

    /* open the global key as well */
    global_key = CTadsRegistry::open_key(HKEY_CURRENT_USER, w32_pref_key_name,
                                         &disposition, TRUE);

    /* save the properties */
    for (id = 0 ; id < HTML_PREF_LAST ; ++id)
    {
        int glob;
        CHtmlProperty *prop = proplist_->get_prop(id);

        /* check to see if it's global */
        glob = prop->is_global();

        /* 
         *   If it's synchronized, or we're reading all properties, read it
         *   from the appropriate key.  (Don't restore unsynchronized
         *   properties when we're merely synchronizing.  During initial
         *   loading, though, restore everything.)  
         */
        if (!synced_only || prop->is_synchronized())
            read_from_registry((HTML_pref_id_t)id, glob ? global_key : key);
    }

    /* load the custom colors */
    CTadsRegistry::query_key_binary(key, custclr_val_name,
                                    cust_colors_, sizeof(cust_colors_));

    /* done with the registry key */
    CTadsRegistry::close_key(global_key);
    CTadsRegistry::close_key(key);
}

/*
 *   Compare the active profile to its saved version 
 */
int CHtmlPreferences::equals_saved(int synced_only)
{
    return equals_saved(get_active_profile_name(), synced_only);
}

/*
 *   Compare a profile in memory to the corresponding profile in the
 *   registry.  Returns true if the saved values in the registry match the
 *   values in memory, false if not.  
 */
int CHtmlPreferences::equals_saved(const char *profile, int synced_only)
{
    int id;
    HKEY key;
    HKEY global_key;
    DWORD disposition;
    char key_name[256];
    int eq;
    char cust_color_buf[sizeof(cust_colors_)];

    /* get the key containing the settings */
    get_settings_key_for(key_name, sizeof(key_name), profile);

    /* open the registry key for our preference settings */
    key = CTadsRegistry::open_key(HKEY_CURRENT_USER, key_name,
                                  &disposition, TRUE);

    /* open the global key as well */
    global_key = CTadsRegistry::open_key(HKEY_CURRENT_USER, w32_pref_key_name,
                                         &disposition, TRUE);

    /* scan the properties */
    for (eq = TRUE, id = 0 ; id < HTML_PREF_LAST ; ++id)
    {
        int glob;
        CHtmlProperty *prop = proplist_->get_prop(id);

        /* check to see if it's global */
        glob = prop->is_global();
        
        /* 
         *   Compare the value to the registry value; if it doesn't match,
         *   then the overall profile doesn't match, so we can stop scanning.
         *   If we're only checking synchronized properties, ignore the
         *   property if it's unsynchronized.  
         */
        if ((!synced_only || prop->is_synchronized())
            && !equals_registry_value((HTML_pref_id_t)id,
                                      glob ? global_key : key))
        {
            /* it doesn't match */
            eq = FALSE;

            /* no need to keep looking, as it can't match now */
            break;
        }
    }

    /* load the custom colors */
    CTadsRegistry::query_key_binary(key, custclr_val_name,
                                    cust_color_buf, sizeof(cust_color_buf));

    /* if these don't match, we don't have a match */
    if (memcmp(cust_color_buf, cust_colors_, sizeof(cust_colors_)) != 0)
        eq = FALSE;

    /* done with the registry key */
    CTadsRegistry::close_key(global_key);
    CTadsRegistry::close_key(key);

    /* return the overall comparison result */
    return eq;
}

/*
 *   Write a property to the registry
 */
void CHtmlPreferences::write_to_registry(HTML_pref_id_t id, HKEY key)
{
    char buf[256];
    size_t vallen;
    CHtmlProperty *prop;
    
    /* get the property value in string format */
    prop = proplist_->get_prop(id);
    vallen = prop->gen_str_rep(buf, sizeof(buf));

    /* write it to the registry */
    CTadsRegistry::set_key_str(key, prop->get_name(), buf, vallen);
}

/*
 *   Read a property from the registry 
 */
void CHtmlPreferences::read_from_registry(HTML_pref_id_t id, HKEY key)
{
    char buf[256];
    size_t vallen;
    CHtmlProperty *prop;

    /* get the property */
    prop = proplist_->get_prop(id);

    /* if the registry value isn't set, keep the default value */
    if (!CTadsRegistry::value_exists(key, prop->get_name()))
        return;
    
    /* read the registry value */
    vallen = CTadsRegistry::query_key_str(key, prop->get_name(),
                                          buf, sizeof(buf));

    /* set the preference property item */
    prop->parse_str_rep(buf, vallen);
}

/*
 *   Compare a key to the registry value.  Returns true if the key is equal
 *   to the registry value, false if not.  
 */
int CHtmlPreferences::equals_registry_value(HTML_pref_id_t id, HKEY key)
{
    char buf[256];
    size_t vallen;
    CHtmlProperty *prop;
    char regbuf[256];
    size_t regvallen;

    /* get the property value in string format */
    prop = proplist_->get_prop(id);
    vallen = prop->gen_str_rep(buf, sizeof(buf));

    /* read the registry value */
    regvallen = CTadsRegistry::query_key_str(key, prop->get_name(),
                                             regbuf, sizeof(regbuf));

    /* is the value in memory is the same as the value in the registry? */
    return (vallen == regvallen && memcmp(buf, regbuf, vallen) == 0);
}

/*
 *   Get the name of the registry key containing our settings.  The key
 *   depends on the active profile.  
 */
void CHtmlPreferences::get_settings_key(char *buf, size_t buflen)
{
    /* get the key for the active profile */
    get_settings_key_for(buf, buflen, get_active_profile_name());
}

/*
 *   Get the settings key for the given profile name 
 */
void CHtmlPreferences::get_settings_key_for(char *buf, size_t buflen,
                                            const char *profile)
{
    /* check to see if a profile name was given */
    if (profile == 0)
    {
        /*  
         *   The profile name is null, which means that we want to use the
         *   old-style non-profiled settings key.  These are stored directly
         *   under the root preferences key.  
         */
        strcpy(buf, w32_pref_key_name);
    }
    else
    {
        /* build the registry key for the given profile name */
        _snprintf(buf, buflen, "%s\\Profiles\\%s",
                  w32_pref_key_name, profile);
    }
}


/*
 *   Get the currently active profile name
 */
const textchar_t *CHtmlPreferences::get_active_profile_name() const
{
    /* if we don't have an active profile, use the default */
    if (active_profile_.get() == 0)
        return get_default_profile();

    /* if the named profile doesn't exist, use the default */
    if (!profile_exists(active_profile_.get()))
        return get_default_profile();

    /* return the active profile name string */
    return active_profile_.get();
}

/*
 *   Set the active profile name
 */
void CHtmlPreferences::set_active_profile_name(const char *profile)
{
    /* if it's "Default", use the current default */
    if (stricmp(profile, "Default") == 0)
        profile = get_default_profile();

    /* set the name */
    active_profile_.set(profile);
}

/*
 *   Check to see if the selected profile is valid 
 */
int CHtmlPreferences::profile_exists(const char *profile)
{
    char keyname[256];
    HKEY key;
    DWORD disposition;
    int exists;

    /* the empty profile name is invalid */
    if (profile != 0 && profile[0] == '\0')
        return FALSE;

    /* 
     *   try opening the base key for the profile; don't create it if it
     *   doesn't already exist, since we want to test for its existence 
     */
    get_settings_key_for(keyname, sizeof(keyname), profile);
    key = CTadsRegistry::open_key(HKEY_CURRENT_USER, keyname,
                                  &disposition, FALSE);

    /* if the key exists, the profile is valid */
    exists = (key != 0);

    /* if we successfully opened the key, close it */
    if (exists)
        CTadsRegistry::close_key(key);

    /* return the result */
    return exists;
}

