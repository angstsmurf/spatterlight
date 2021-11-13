/* $Header$ */

/* Copyright (c) 2000 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  w32abt.h - about box window class for TADS 3 interpreter/workbench
Function
  
Notes
  
Modified
  01/20/00 MJRoberts  - Creation
*/

#ifndef W32ABT_H
#define W32ABT_H

#include "tadswin.h"

/* ------------------------------------------------------------------------ */
/*
 *   dialog window 
 */
class CTadsWinAboutDlg: public CTadsWin
{
public:
    CTadsWinAboutDlg()
    {
        /* no modal dialog disable context yet */
        disable_list_ = 0;

        /* no default button yet */
        default_btn_ = 0;
    }

    /* window creation */
    void do_create();
    
    /* window destruction */
    void do_destroy();

    /* prepare to close */
    int do_close();

    /* process a keystroke */
    int do_keydown(int vkey, long keydata);

    /* process focus changes */
    void do_setfocus(HWND prv);

    /* set the default button */
    void set_default_btn(class CTadsWinRollBtn *btn) { default_btn_ = btn; }

protected:
    /* modal_dlg_pre/post context */
    void *disable_list_;

    /* default button control, if any */
    class CTadsWinRollBtn *default_btn_;
};

/* ------------------------------------------------------------------------ */
/*
 *   about-box window 
 */
class CTadsWinAboutTads3: public CTadsWinAboutDlg
{
public:
    CTadsWinAboutTads3(int bkg_bmp_id, const char *credits_txt);
    ~CTadsWinAboutTads3();

    /* window creation */
    void do_create();

    /* window destruction */
    void do_destroy();

    /* process a timer event */
    int do_timer(int timer_id);

    /* process a command */
    int do_command(int notify_code, int command_id, HWND ctl);

    /* paint the window */
    void do_paint_content(HDC hdc, const RECT *area);

    /* palette change notification */
    int do_querynewpalette();
    void do_palettechanged(HWND srcwin);

    /* create the system window */
    int create_system_window(CTadsWin *parent, HWND parent_hwnd,
                             int show, const char *title,
                             const RECT *pos, CTadsSyswin *sysifc);

    /* style */
    DWORD get_winstyle()
    {
        /* do not include min/max/size controls */
        return (WS_OVERLAPPED | WS_BORDER
                | WS_CAPTION | WS_SYSMENU | WS_CLIPSIBLINGS);
    }

    /* erase the background - let the paint routine do the work */
    int do_erase_bkg(HDC) { return 1; }

protected:
    /* recalculate my palette */
    int recalc_palette(int foreground);

    /* show the credits dialog */
    void show_credits();

    /* bitmap handle */
    HBITMAP bmp_;

    /* palette */
    HPALETTE pal_;

    /* "close" and "credits" buttons */
    class CTadsWinRollBtn *close_btn_;
    class CTadsWinRollBtn *credits_btn_;

    /* hilite timer */
    int hi_timer_;

    /* hilite bitmap */
    HBITMAP hibmp_;

    /* width and height of hilite bitmap */
    int hilite_x_;
    int hilite_y_;

    /* current hilite state (x offset of hilite area) */
    int hilite_;

    /* credits string (passed in from the creator) */
    const char *credits_txt_;
};

/* ------------------------------------------------------------------------ */
/*
 *   credits dialog 
 */
class CTadsWinCreditsTads3: public CTadsWinAboutDlg
{
public:
    CTadsWinCreditsTads3(const char *credits_txt)
    {
        /* remember the caller's credits string */
        credits_txt_ = credits_txt;
    }
    
    /* window creation */
    void do_create();

    /* window destruction */
    void do_destroy();

    /* process a command */
    int do_command(int notify_code, int command_id, HWND ctl);

    /* paint the window */
    void do_paint_content(HDC hdc, const RECT *area);

    /* style */
    DWORD get_winstyle()
    {
        /* do not include min/max/size controls */
        return (WS_OVERLAPPED | WS_BORDER
                | WS_CAPTION | WS_SYSMENU
                | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
    }

protected:
    /* subclassed window procedure for text field - static version */
    static LRESULT CALLBACK fld_proc(HWND hwnd, UINT msg,
                                     WPARAM wpar, LPARAM lpar);

    /* subclassed window procedure for text field - method version */
    LRESULT do_fld_proc(HWND hwnd, UINT msg, WPARAM wpar, LPARAM lpar);

    /* text field */
    HWND fld_;

    /* original field window procedure */
    WNDPROC fld_defproc_;

    /* text field's font */
    HFONT tx_font_;

    /* credits string (obtained from the caller) */
    const char *credits_txt_;
};

#endif /* W32ABT_H */

