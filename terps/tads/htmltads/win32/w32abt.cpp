#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* Copyright (c) 2000, 2002 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  w32abt.cpp - win32 about box for tads 3
Function
  
Notes
  
Modified
  01/20/00 MJRoberts  - Creation
*/

#include <Windows.h>
#include <RichEdit.h>
#include <limits.h>

#include "tadshtml.h"
#include "tadsapp.h"
#include "tadswin.h"
#include "tadsfont.h"
#include "w32abt.h"
#include "tadsdlg.h"
#include "w32main.h"
#include "w32rolb.h"
#include "htmlres.h"

/* ------------------------------------------------------------------------ */
/*
 *   Basic dialog window 
 */

/*
 *   create 
 */
void CTadsWinAboutDlg::do_create()
{
    HWND owner;
    
    /* inherit default handling */
    CTadsWin::do_create();

    /* disable the owner window */
    owner = GetWindow(handle_, GW_OWNER);
    disable_list_ = CTadsDialog::modal_dlg_pre(owner, TRUE);

    /* 
     *   re-enable myself - the dialog set-up routine will disable
     *   everything owned by our owner, including us 
     */
    EnableWindow(handle_, TRUE);
}

/*
 *   delete 
 */
void CTadsWinAboutDlg::do_destroy()
{
    /* re-enable the original windows */
    if (disable_list_ != 0)
    {
        CTadsDialog::modal_dlg_post(disable_list_);
        disable_list_ = 0;
    }

    /* inherit default */
    CTadsWin::do_destroy();
}

/* 
 *   close 
 */
int CTadsWinAboutDlg::do_close()
{
    /* re-enable the window list */
    if (disable_list_ != 0)
    {
        /* re-enable the modal windows */
        CTadsDialog::modal_dlg_post(disable_list_);
        disable_list_ = 0;
    }

    /* re-activate our parent (owner) window */
    BringWindowToTop(GetWindow(handle_, GW_OWNER));

    /* inherit the default processing */
    return CTadsWin::do_close();
}

/*
 *   gain focus 
 */
void CTadsWinAboutDlg::do_setfocus(HWND)
{
    /* set focus on my first child control */
    SetFocus(GetWindow(handle_, GW_CHILD));
}

/*
 *   process a keystroke 
 */
int CTadsWinAboutDlg::do_keydown(int vkey, long keydata)
{
    switch(vkey)
    {
    case VK_ESCAPE:
        /* dismiss the dialog */
        SendMessage(handle_, WM_CLOSE, 0, 0);
        return TRUE;

    case VK_RETURN:
        /* 
         *   send the return key to the default button, if we have one; if
         *   not, just dismiss the dialog 
         */
        if (default_btn_ != 0)
            SendMessage(default_btn_->get_handle(), WM_KEYDOWN,
                        (WPARAM)VK_RETURN, (LPARAM)keydata);
        else
            SendMessage(handle_, WM_CLOSE, 0, 0);
        return TRUE;

    case VK_TAB:
        {
            HWND cur;

            /* get the current focus window */
            cur = GetFocus();

            /* if I have focus, go to my first child */
            if (cur == handle_)
                cur = 0;
            
            /* get the next child in z order (== tab order) */
            if (cur != 0)
                cur = GetWindow(cur, GW_HWNDNEXT);

            /* if that was the last, loop to the first */
            if (cur == 0)
                cur = GetWindow(handle_, GW_CHILD);

            /* set focus on this window */
            if (cur != 0)
                SetFocus(cur);
        }
        return TRUE;

    default:
        return CTadsWin::do_keydown(vkey, keydata);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   "About" dialog window
 */

/*
 *   command codes 
 */
const int CMD_ABOUT_CLOSE = 1001;
const int CMD_ABOUT_CREDITS = 1002;

/* width of hilite area */
const int HILITE_WIDTH = 75;

/*
 *   instantiate 
 */
CTadsWinAboutTads3::CTadsWinAboutTads3(int bkg_bmp_id,
                                       const char *credits_txt)
{
    BITMAPINFO bmp_info;
    HDC dc;

    /* remember the credits string */
    credits_txt_ = credits_txt;
    
    /* load my bitmap */
    bmp_ = (HBITMAP)LoadImage(CTadsApp::get_app()->get_instance(),
                              MAKEINTRESOURCE(bkg_bmp_id), IMAGE_BITMAP,
                              0, 0, LR_CREATEDIBSECTION);

    /* load the hilite bitmap */
    hibmp_ = (HBITMAP)LoadImage(CTadsApp::get_app()->get_instance(),
                                MAKEINTRESOURCE(IDB_ABOUT_T3_HILITE),
                                IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);

    /* get information on the hilite bitmap so we can calculate its size */
    dc = GetDC(0);
    bmp_info.bmiHeader.biSize = sizeof(bmp_info.bmiHeader);
    bmp_info.bmiHeader.biBitCount = 0;
    GetDIBits(dc, hibmp_, 0, 0, 0, &bmp_info, DIB_RGB_COLORS);
    ReleaseDC(0, dc);

    /* remember the size of the bitmap */
    hilite_x_ = bmp_info.bmiHeader.biWidth;
    hilite_y_ = bmp_info.bmiHeader.biHeight;

    /* no hilite timer yet */
    hi_timer_ = 0;

    /* start the hilite after a full initial delay */
    hilite_ = hilite_x_ + 1;

    /* set up a rainbow palette */
    {
        static const PALETTEENTRY rainbow[] =
        {
            /* a uniform color cube */
            { 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x33 },
            { 0x00, 0x00, 0x66 },
            { 0x00, 0x00, 0x99 },
            { 0x00, 0x00, 0xcc },
            { 0x00, 0x00, 0xff },
            { 0x00, 0x33, 0x00 },
            { 0x00, 0x33, 0x33 },
            { 0x00, 0x33, 0x66 },
            { 0x00, 0x33, 0x99 },
            { 0x00, 0x33, 0xcc },
            { 0x00, 0x33, 0xff },
            { 0x00, 0x66, 0x00 },
            { 0x00, 0x66, 0x33 },
            { 0x00, 0x66, 0x66 },
            { 0x00, 0x66, 0x99 },
            { 0x00, 0x66, 0xcc },
            { 0x00, 0x66, 0xff },
            { 0x00, 0x99, 0x00 },
            { 0x00, 0x99, 0x33 },
            { 0x00, 0x99, 0x66 },
            { 0x00, 0x99, 0x99 },
            { 0x00, 0x99, 0xcc },
            { 0x00, 0x99, 0xff },
            { 0x00, 0xcc, 0x00 },
            { 0x00, 0xcc, 0x33 },
            { 0x00, 0xcc, 0x66 },
            { 0x00, 0xcc, 0x99 },
            { 0x00, 0xcc, 0xcc },
            { 0x00, 0xcc, 0xff },
            { 0x00, 0xff, 0x00 },
            { 0x00, 0xff, 0x33 },
            { 0x00, 0xff, 0x66 },
            { 0x00, 0xff, 0x99 },
            { 0x00, 0xff, 0xcc },
            { 0x00, 0xff, 0xff },
            { 0x33, 0x00, 0x00 },
            { 0x33, 0x00, 0x33 },
            { 0x33, 0x00, 0x66 },
            { 0x33, 0x00, 0x99 },
            { 0x33, 0x00, 0xcc },
            { 0x33, 0x00, 0xff },
            { 0x33, 0x33, 0x00 },
            { 0x33, 0x33, 0x33 },
            { 0x33, 0x33, 0x66 },
            { 0x33, 0x33, 0x99 },
            { 0x33, 0x33, 0xcc },
            { 0x33, 0x33, 0xff },
            { 0x33, 0x66, 0x00 },
            { 0x33, 0x66, 0x33 },
            { 0x33, 0x66, 0x66 },
            { 0x33, 0x66, 0x99 },
            { 0x33, 0x66, 0xcc },
            { 0x33, 0x66, 0xff },
            { 0x33, 0x99, 0x00 },
            { 0x33, 0x99, 0x33 },
            { 0x33, 0x99, 0x66 },
            { 0x33, 0x99, 0x99 },
            { 0x33, 0x99, 0xcc },
            { 0x33, 0x99, 0xff },
            { 0x33, 0xcc, 0x00 },
            { 0x33, 0xcc, 0x33 },
            { 0x33, 0xcc, 0x66 },
            { 0x33, 0xcc, 0x99 },
            { 0x33, 0xcc, 0xcc },
            { 0x33, 0xcc, 0xff },
            { 0x33, 0xff, 0x00 },
            { 0x33, 0xff, 0x33 },
            { 0x33, 0xff, 0x66 },
            { 0x33, 0xff, 0x99 },
            { 0x33, 0xff, 0xcc },
            { 0x33, 0xff, 0xff },
            { 0x66, 0x00, 0x00 },
            { 0x66, 0x00, 0x33 },
            { 0x66, 0x00, 0x66 },
            { 0x66, 0x00, 0x99 },
            { 0x66, 0x00, 0xcc },
            { 0x66, 0x00, 0xff },
            { 0x66, 0x33, 0x00 },
            { 0x66, 0x33, 0x33 },
            { 0x66, 0x33, 0x66 },
            { 0x66, 0x33, 0x99 },
            { 0x66, 0x33, 0xcc },
            { 0x66, 0x33, 0xff },
            { 0x66, 0x66, 0x00 },
            { 0x66, 0x66, 0x33 },
            { 0x66, 0x66, 0x66 },
            { 0x66, 0x66, 0x99 },
            { 0x66, 0x66, 0xcc },
            { 0x66, 0x66, 0xff },
            { 0x66, 0x99, 0x00 },
            { 0x66, 0x99, 0x33 },
            { 0x66, 0x99, 0x66 },
            { 0x66, 0x99, 0x99 },
            { 0x66, 0x99, 0xcc },
            { 0x66, 0x99, 0xff },
            { 0x66, 0xcc, 0x00 },
            { 0x66, 0xcc, 0x33 },
            { 0x66, 0xcc, 0x66 },
            { 0x66, 0xcc, 0x99 },
            { 0x66, 0xcc, 0xcc },
            { 0x66, 0xcc, 0xff },
            { 0x66, 0xff, 0x00 },
            { 0x66, 0xff, 0x33 },
            { 0x66, 0xff, 0x66 },
            { 0x66, 0xff, 0x99 },
            { 0x66, 0xff, 0xcc },
            { 0x66, 0xff, 0xff },
            { 0x99, 0x00, 0x00 },
            { 0x99, 0x00, 0x33 },
            { 0x99, 0x00, 0x66 },
            { 0x99, 0x00, 0x99 },
            { 0x99, 0x00, 0xcc },
            { 0x99, 0x00, 0xff },
            { 0x99, 0x33, 0x00 },
            { 0x99, 0x33, 0x33 },
            { 0x99, 0x33, 0x66 },
            { 0x99, 0x33, 0x99 },
            { 0x99, 0x33, 0xcc },
            { 0x99, 0x33, 0xff },
            { 0x99, 0x66, 0x00 },
            { 0x99, 0x66, 0x33 },
            { 0x99, 0x66, 0x66 },
            { 0x99, 0x66, 0x99 },
            { 0x99, 0x66, 0xcc },
            { 0x99, 0x66, 0xff },
            { 0x99, 0x99, 0x00 },
            { 0x99, 0x99, 0x33 },
            { 0x99, 0x99, 0x66 },
            { 0x99, 0x99, 0x99 },
            { 0x99, 0x99, 0xcc },
            { 0x99, 0x99, 0xff },
            { 0x99, 0xcc, 0x00 },
            { 0x99, 0xcc, 0x33 },
            { 0x99, 0xcc, 0x66 },
            { 0x99, 0xcc, 0x99 },
            { 0x99, 0xcc, 0xcc },
            { 0x99, 0xcc, 0xff },
            { 0x99, 0xff, 0x00 },
            { 0x99, 0xff, 0x33 },
            { 0x99, 0xff, 0x66 },
            { 0x99, 0xff, 0x99 },
            { 0x99, 0xff, 0xcc },
            { 0x99, 0xff, 0xff },
            { 0xcc, 0x00, 0x00 },
            { 0xcc, 0x00, 0x33 },
            { 0xcc, 0x00, 0x66 },
            { 0xcc, 0x00, 0x99 },
            { 0xcc, 0x00, 0xcc },
            { 0xcc, 0x00, 0xff },
            { 0xcc, 0x33, 0x00 },
            { 0xcc, 0x33, 0x33 },
            { 0xcc, 0x33, 0x66 },
            { 0xcc, 0x33, 0x99 },
            { 0xcc, 0x33, 0xcc },
            { 0xcc, 0x33, 0xff },
            { 0xcc, 0x66, 0x00 },
            { 0xcc, 0x66, 0x33 },
            { 0xcc, 0x66, 0x66 },
            { 0xcc, 0x66, 0x99 },
            { 0xcc, 0x66, 0xcc },
            { 0xcc, 0x66, 0xff },
            { 0xcc, 0x99, 0x00 },
            { 0xcc, 0x99, 0x33 },
            { 0xcc, 0x99, 0x66 },
            { 0xcc, 0x99, 0x99 },
            { 0xcc, 0x99, 0xcc },
            { 0xcc, 0x99, 0xff },
            { 0xcc, 0xcc, 0x00 },
            { 0xcc, 0xcc, 0x33 },
            { 0xcc, 0xcc, 0x66 },
            { 0xcc, 0xcc, 0x99 },
            { 0xcc, 0xcc, 0xcc },
            { 0xcc, 0xcc, 0xff },
            { 0xcc, 0xff, 0x00 },
            { 0xcc, 0xff, 0x33 },
            { 0xcc, 0xff, 0x66 },
            { 0xcc, 0xff, 0x99 },
            { 0xcc, 0xff, 0xcc },
            { 0xcc, 0xff, 0xff },
            { 0xff, 0x00, 0x00 },
            { 0xff, 0x00, 0x33 },
            { 0xff, 0x00, 0x66 },
            { 0xff, 0x00, 0x99 },
            { 0xff, 0x00, 0xcc },
            { 0xff, 0x00, 0xff },
            { 0xff, 0x33, 0x00 },
            { 0xff, 0x33, 0x33 },
            { 0xff, 0x33, 0x66 },
            { 0xff, 0x33, 0x99 },
            { 0xff, 0x33, 0xcc },
            { 0xff, 0x33, 0xff },
            { 0xff, 0x66, 0x00 },
            { 0xff, 0x66, 0x33 },
            { 0xff, 0x66, 0x66 },
            { 0xff, 0x66, 0x99 },
            { 0xff, 0x66, 0xcc },
            { 0xff, 0x66, 0xff },
            { 0xff, 0x99, 0x00 },
            { 0xff, 0x99, 0x33 },
            { 0xff, 0x99, 0x66 },
            { 0xff, 0x99, 0x99 },
            { 0xff, 0x99, 0xcc },
            { 0xff, 0x99, 0xff },
            { 0xff, 0xcc, 0x00 },
            { 0xff, 0xcc, 0x33 },
            { 0xff, 0xcc, 0x66 },
            { 0xff, 0xcc, 0x99 },
            { 0xff, 0xcc, 0xcc },
            { 0xff, 0xcc, 0xff },
            { 0xff, 0xff, 0x00 },
            { 0xff, 0xff, 0x33 },
            { 0xff, 0xff, 0x66 },
            { 0xff, 0xff, 0x99 },
            { 0xff, 0xff, 0xcc },
            { 0xff, 0xff, 0xff },

            /* MS Windows standard system colors not in the above cube */
            { 0x80, 0x00, 0x00 },
            { 0x00, 0x80, 0x00 },
            { 0x80, 0x80, 0x00 },
            { 0x00, 0x00, 0x80 },
            { 0x80, 0x00, 0x80 },
            { 0x00, 0x80, 0x80 },
            { 0xc0, 0xc0, 0xc0 },
            { 0xc0, 0xdc, 0xc0 },
            { 0xa6, 0xca, 0xf0 },
            { 0xff, 0xfb, 0xf0 },
            { 0xa0, 0xa0, 0xa4 },
            { 0x80, 0x80, 0x80 },

            /* 
             *   We have 216 colors in the cube plus 12 above, which
             *   leaves us with 28 colors.  Allocate the remaining colors
             *   to ramps of red, green, blue, and gray, each with 7
             *   levels, filling in holes in the color cube for those
             *   colors.
             *   
             *   However, note that in the Windows colors above, we
             *   already have a mid-level entry for each color (0x80,
             *   0x00, 0x00 for red, likewise for blue, green, and gray),
             *   so we can omit this from our ramp.  
             */
            { 0x11, 0x00, 0x00 },
            { 0x22, 0x00, 0x00 },
            { 0x44, 0x00, 0x00 },
            { 0x55, 0x00, 0x00 },
            { 0xb2, 0x00, 0x00 },
            { 0xdd, 0x00, 0x00 },
            { 0xee, 0x00, 0x00 },

            { 0x00, 0x11, 0x00 },
            { 0x00, 0x22, 0x00 },
            { 0x00, 0x44, 0x00 },
            { 0x00, 0x55, 0x00 },
            { 0x00, 0xb2, 0x00 },
            { 0x00, 0xdd, 0x00 },
            { 0x00, 0xee, 0x00 },

            { 0x00, 0x00, 0x11 },
            { 0x00, 0x00, 0x22 },
            { 0x00, 0x00, 0x44 },
            { 0x00, 0x00, 0x55 },
            { 0x00, 0x00, 0xb2 },
            { 0x00, 0x00, 0xdd },
            { 0x00, 0x00, 0xee },

            { 0x11, 0x11, 0x11 },
            { 0x22, 0x22, 0x22 },
            { 0x44, 0x44, 0x44 },
            { 0x55, 0x55, 0x55 },
            { 0xb2, 0xb2, 0xb2 },
            { 0xdd, 0xdd, 0xdd },
            { 0xee, 0xee, 0xee }
        };
        struct
        {
            LOGPALETTE logpal;
            PALETTEENTRY entry[255];
        } pal_info;
        int i;
        
        /* copy the rainbow palette description */
        for (i = 0 ; i < sizeof(rainbow)/sizeof(rainbow[0]) ; ++i)
        {
            pal_info.logpal.palPalEntry[i] = rainbow[i];
            pal_info.logpal.palPalEntry[i].peFlags = 0;
        }

        /* create the rainbow palette */
        pal_info.logpal.palVersion = 0x0300;
        pal_info.logpal.palNumEntries = sizeof(rainbow)/sizeof(rainbow[0]);
        pal_ = CreatePalette(&pal_info.logpal);
    }
}

/*
 *   delete 
 */
CTadsWinAboutTads3::~CTadsWinAboutTads3()
{
    /* unload the bitmap */
    if (bmp_ != 0)
        DeleteObject(bmp_);

    /* unload the hilite bitmap */
    if (hibmp_ != 0)
        DeleteObject(hibmp_);

    /* delete the palette */
    if (pal_ != 0)
        DeleteObject(pal_);
}

/*
 *   create 
 */
void CTadsWinAboutTads3::do_create()
{
    RECT clirc;
    RECT rc;
    
    /* inherit default */
    CTadsWinAboutDlg::do_create();

    /* get my client area for positioning buttons */
    GetClientRect(handle_, &clirc);

    /* 
     *   set up a default initial position for the buttons - we'll move
     *   them later when we figure out how much space they take up 
     */
    SetRect(&rc, 0, 0, 20, 20);

    /* create our "credits" button  */
    credits_btn_ = new CTadsWinRollBtn("Credits...", CMD_ABOUT_CREDITS);
    credits_btn_->create_system_window(this, FALSE, "", &rc);

    /* create our "close" button (create last so it's first in tab order) */
    close_btn_ = new CTadsWinRollBtn("Close", CMD_ABOUT_CLOSE);
    close_btn_->create_system_window(this, FALSE, "", &rc);

    /* move the "close" button to the bottom right corner */
    SetRect(&rc, 0, 0, 0, 0);
    close_btn_->set_size(&rc);
    OffsetRect(&rc, clirc.right - rc.right - 16,
               clirc.bottom - rc.bottom - 6);
    MoveWindow(close_btn_->get_handle(), rc.left, rc.top,
               rc.right - rc.left, rc.bottom - rc.top, FALSE);

    /* move the "credits" button to the left of the "close" button */
    rc.left -= 20;
    credits_btn_->set_size(&rc);
    OffsetRect(&rc, -(rc.right - rc.left), 0);
    MoveWindow(credits_btn_->get_handle(), rc.left, rc.top,
               rc.right - rc.left, rc.bottom - rc.top, FALSE);

    /* show the buttons */
    ShowWindow(close_btn_->get_handle(), SW_SHOW);
    ShowWindow(credits_btn_->get_handle(), SW_SHOW);

    /* set focus in our "close" button */
    SetFocus(close_btn_->get_handle());

    /* make the close button the default */
    set_default_btn(close_btn_);

    /* create my hilite timer */
    hi_timer_ = alloc_timer_id();
    if (hi_timer_ != 0)
        SetTimer(handle_, hi_timer_, 30, (TIMERPROC)0);
}

/*
 *   destroy 
 */
void CTadsWinAboutTads3::do_destroy()
{
    /* stop my timer */
    if (hi_timer_ != 0)
    {
        KillTimer(handle_, hi_timer_);
        free_timer_id(hi_timer_);
    }
    
    /* inherit default */
    CTadsWinAboutDlg::do_destroy();
}


/* 
 *   create the system window
 */
int CTadsWinAboutTads3::
   create_system_window(CTadsWin *parent, HWND parent_hwnd,
                        int show, const char *title,
                        const RECT *, CTadsSyswin *sysifc)
{
    BITMAP info;
    RECT pos;
    RECT deskrc;
    int wid, ht;
    int desk_wid, desk_ht;
    int err;

    /* get the desktop window area, so we can center our window */
    GetClientRect(GetDesktopWindow(), &deskrc);
    desk_wid = deskrc.right - deskrc.left;
    desk_ht = deskrc.bottom - deskrc.top;

    /* get the area of our bitmap */
    if (bmp_ != 0)
    {
        /* get the bitmap information */
        GetObject(bmp_, sizeof(info), &info);
        wid = info.bmWidth;
        ht = info.bmHeight;
    }
    else
    {
        /* no bitmap - use a default size */
        wid = 400;
        ht = 250;
    }

    /* set up the window with the bitmap size */
    SetRect(&pos, 0, 0, wid, ht);

    /* adjust the size for frame adornments */
    AdjustWindowRectEx(&pos, get_winstyle(), FALSE, get_winstyle_ex());

    /* center the final window size on the desktop */
    wid = pos.right - pos.left;
    ht = pos.bottom - pos.top;
    SetRect(&pos, (desk_wid - wid)/2, (desk_ht - ht)/2, wid, ht);

    /* inherit default handling now that we've calculated the position */
    err = CTadsWinAboutDlg::create_system_window(parent, parent_hwnd,
        show, title, &pos, sysifc);

    /* return the result from the inherited creation routine */
    return err;
}

/*
 *   paint 
 */
void CTadsWinAboutTads3::do_paint_content(HDC hdc, const RECT *)
{
    RECT rc;
    int oldbkmode;
    COLORREF oldtextcolor;
    int text_x, text_y;
    char vsn[256];
    HPALETTE oldpal;

    /* select the system color palette into the memory DC */
    oldpal = SelectPalette(hdc, pal_, TRUE);
    RealizePalette(hdc);

    /* the bitmap covers the entire client area */
    GetClientRect(handle_, &rc);

    /* paint the background bitmap */
    if (bmp_ != 0)
        draw_hbitmap(bmp_, &rc, &rc);

    /* paint the hilite area */
    if (hibmp_ != 0 && hilite_ < hilite_x_)
    {
        RECT src_rc;
        RECT dst_rc;

        /* calculate the hilite bitmap's location in the upper right corner */
        GetClientRect(handle_, &dst_rc);
        dst_rc.left = dst_rc.right - hilite_x_;
        dst_rc.bottom = dst_rc.top + hilite_y_;

        /* 
         *   calculate the hilite area - hilite_ is the x offset where
         *   we're currently drawing 
         */
        dst_rc.left += hilite_;
        dst_rc.right = dst_rc.left + HILITE_WIDTH;

        /* set the source area */
        SetRect(&src_rc, hilite_, 0, hilite_ + HILITE_WIDTH, hilite_y_);

        /* draw the hilite area */
        draw_hbitmap(hibmp_, &dst_rc, &src_rc);
    }

    /* build the version string */
    sprintf(vsn, "Version %s", w32_version_string);

    /* set up to draw the version string text */
    oldbkmode = SetBkMode(hdc, TRANSPARENT);
    text_x = 10;
    text_y = 170;

    /* draw first in black, to provide a background matting for the text */
    oldtextcolor = SetTextColor(hdc, RGB(0, 0, 0));
    ExtTextOut(hdc, text_x + 2, text_y + 1, 0, 0, vsn, strlen(vsn), 0);

    /* now draw in white, slightly offset */
    SetTextColor(hdc, RGB(0xff, 0xff, 0xff));
    ExtTextOut(hdc, text_x, text_y, 0, 0, vsn, strlen(vsn), 0);

    /* restore dc settings */
    SetTextColor(hdc, oldtextcolor);
    SetBkMode(hdc, oldbkmode);

    /* restore the window palette */
    SelectPalette(hdc, oldpal, TRUE);
    RealizePalette(hdc);
}

/*
 *   Process a palette change request 
 */
int CTadsWinAboutTads3::do_querynewpalette()
{
    /* redraw if the palette changed */
    if (recalc_palette(TRUE))
    {
        /* we changed the palette - redraw the window */
        InvalidateRect(handle_, 0, TRUE);
        return TRUE;
    }
    else
    {
        /* no change, but realize in the background */
        SendMessage(handle_, WM_PALETTECHANGED, (WPARAM)handle_, 0);
    }

    /* we didn't change anything */
    return FALSE;
}

/*
 *   Update for a palette change in another window 
 */
void CTadsWinAboutTads3::do_palettechanged(HWND srcwin)
{
    /* if I'm the source of the change, there's no need to do anything */
    if (srcwin == get_handle())
    {
        /* I initiated the change - nothing to do */
    }
    else
    {
        /* recalculate our palette */
        recalc_palette(FALSE);

        /* redraw with the new palette */
        InvalidateRect(get_handle(), 0, TRUE);
    }
}

/*
 *   Recalculate my palette 
 */
int CTadsWinAboutTads3::recalc_palette(int foreground)
{
    HPALETTE oldpal;
    HDC dc;
    int changed;

    /* get my window dc */
    dc = GetDC(handle_);

    /* set our palette in the foreground or background as appropriate */
    oldpal = SelectPalette(dc, pal_, !foreground);
    changed = RealizePalette(dc);

    /* restore the old palette in the background */
    SelectPalette(dc, oldpal, TRUE);
    RealizePalette(dc);

    /* release the window DC */
    ReleaseDC(handle_, dc);

    /* return the change indication */
    return changed;
}


/*
 *   handle timer events 
 */
int CTadsWinAboutTads3::do_timer(int timer_id)
{
    /* check to see if it's ours */
    if (timer_id == hi_timer_)
    {
        RECT rc;

        /* advance hilite state */
        hilite_ += (HILITE_WIDTH * 2) / 3;
        if (hilite_ > HILITE_WIDTH * 50)
            hilite_ = 0;

        /* 
         *   invalidate the hilite area - it's always at the upper right
         *   corner of our window, so calculate the area based on the size
         *   of the hilite bitmap placed in the upper left corner 
         */
        GetClientRect(handle_, &rc);
        rc.left = rc.right - hilite_x_;
        rc.bottom = rc.top + hilite_y_;
        InvalidateRect(handle_, &rc, TRUE);

        /* handled */
        return TRUE;
    }
    else
    {
        /* not ours - inherit default handling */
        return CTadsWinAboutDlg::do_timer(timer_id);
    }
}

/*
 *   Process a command 
 */
int CTadsWinAboutTads3::do_command(int notify_code, int cmd, HWND ctl)
{
    switch(cmd)
    {
    case CMD_ABOUT_CLOSE:
        /* send myself a 'close' message */
        SendMessage(handle_, WM_CLOSE, 0, 0);
        return TRUE;

    case CMD_ABOUT_CREDITS:
        /* run the credits dialog */
        show_credits();
        return TRUE;
        
    default:
        /* ignore other commands */
        return FALSE;
    }
}

/*
 *   show the credits dialog 
 */
void CTadsWinAboutTads3::show_credits()
{
    RECT deskrc;
    RECT rc;
    CTadsWinCreditsTads3 *credits_win;
    int wid, ht;
    int desk_wid, desk_ht;

    /* get my area, so we can base the credits window size on my size */
    GetClientRect(handle_, &rc);
    wid = rc.right - rc.left - 50;
    ht = rc.bottom - rc.top + 50;

    /* center the credits window on the desktop */
    GetClientRect(GetDesktopWindow(), &deskrc);
    desk_wid = deskrc.right - deskrc.left;
    desk_ht = deskrc.bottom - deskrc.top;
    SetRect(&rc, (desk_wid - wid)/2, (desk_ht - ht)/2, wid, ht);

    /* create the credits window */
    credits_win = new CTadsWinCreditsTads3(credits_txt_);
    credits_win->create_system_window(this, TRUE, "Credits", &rc);
}


/* ------------------------------------------------------------------------ */
/*
 *   Credits dialog 
 */

/*
 *   EDITSTREAM callback information structure 
 */
struct es_callback_info
{
    /* current pointer */
    const char *p;
};

/*
 *   EDITSTREAM callback - for loading a rich text control with text 
 */
static DWORD CALLBACK es_callback(DWORD cookie, BYTE *buf, LONG cb,
                                  LONG FAR *pcb)
{
    es_callback_info *info;
    LONG cnt;

    /* cast the cookie to our information structure */
    info = (es_callback_info *)cookie;

    /* if there's nothing left, tell the caller we're done */
    if (*info->p == '\0')
        return 1;

    /* copy as many bytes as we can */
    for (cnt = 0 ; cb != 0 && *info->p != '\0' ; ++cnt, ++(info->p), ++buf)
    {
        /* copy this byte */
        *buf = *info->p;
    }

    /* tell the caller how many bytes we transferred */
    *pcb = cnt;

    /* tell the caller to proceed */
    return 0;
}

/*
 *   window creation 
 */
void CTadsWinCreditsTads3::do_create()
{
    CTadsWinRollBtn *close_btn;
    RECT rc;
    RECT clirc;
    EDITSTREAM es;
    es_callback_info info;

    /* inherit default creation handling */
    CTadsWinAboutDlg::do_create();

    /* get my client area size, for positioning controls */
    GetClientRect(handle_, &clirc);

    /* create our "close" button */
    close_btn = new CTadsWinRollBtn("Close", CMD_ABOUT_CLOSE);
    close_btn->create_system_window(this, FALSE, "", &rc);

    /* move the "close" button to the bottom center */
    SetRect(&rc, 0, 0, 0, 0);
    close_btn->set_size(&rc);
    OffsetRect(&rc, (clirc.right - rc.right)/2, clirc.bottom - rc.bottom - 2);
    MoveWindow(close_btn->get_handle(), rc.left, rc.top,
               rc.right - rc.left, rc.bottom - rc.top, FALSE);

    /* show the button */
    ShowWindow(close_btn->get_handle(), SW_SHOW);

    /* create a font for the text box */
    tx_font_ = CreateFont(0, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                          CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                          VARIABLE_PITCH | FF_ROMAN, "");

    /* create a multi-line text field for the rest of the window */
    fld_ = CreateWindowEx(0, "RichEdit", "",
                          WS_VISIBLE | WS_CHILD | ES_CENTER
                          | ES_MULTILINE | WS_VSCROLL | ES_READONLY
                          | ES_AUTOVSCROLL,
                          0, 0, clirc.right, rc.top - 2, handle_, 0,
                          CTadsApp::get_app()->get_instance(), 0);

    /* subclass the field so we can hook certain messages */
    fld_defproc_ = (WNDPROC)SetWindowLong(fld_, GWL_WNDPROC, (LONG)&fld_proc);
    SetProp(fld_, "CTadsWinCreditsTads3.parent.this", (HANDLE)this);

    /* set the background color */
    SendMessage(fld_, EM_SETBKGNDCOLOR, FALSE,
                (LPARAM)GetSysColor(COLOR_BTNFACE));

    /* set the text field's font */
    SendMessage(fld_, WM_SETFONT, (WPARAM)tx_font_, MAKELPARAM(FALSE, 0));

    /* stream our RTF text into the control */
    info.p = credits_txt_;
    es.dwCookie = (DWORD)&info;
    es.dwError = 0;
    es.pfnCallback = (EDITSTREAMCALLBACK)es_callback;
    SendMessage(fld_, EM_STREAMIN, (WPARAM)SF_RTF, (LPARAM)&es);

    /* move focus to the close button */
    SetFocus(close_btn->get_handle());

    /* make the close button the default */
    set_default_btn(close_btn);
}

/*
 *   subclassed window procedure - static version; we'll figure out our
 *   'this' pointer and call the non-static member function 
 */
LRESULT CALLBACK CTadsWinCreditsTads3::
   fld_proc(HWND hwnd, UINT msg, WPARAM wpar, LPARAM lpar)
{
    CTadsWinCreditsTads3 *win;

    /* get the 'this' pointer from the window properties */
    win = (CTadsWinCreditsTads3 *)
          GetProp(hwnd, "CTadsWinCreditsTads3.parent.this");

    /* there's nothing we can do if we couldn't get the window */
    if (win == 0)
        return 0;

    /* invoke the non-static version */
    return win->do_fld_proc(hwnd, msg, wpar, lpar);
}

/*
 *   subclassed window procedure for edit field 
 */
LRESULT CTadsWinCreditsTads3::do_fld_proc(HWND hwnd, UINT msg,
                                          WPARAM wpar, LPARAM lpar)
{
    /* check the message */
    switch(msg)
    {
    case WM_KEYDOWN:
        /* it's a key press - see which key it is */
        switch(wpar)
        {
        case VK_RETURN:
        case VK_TAB:
        case VK_ESCAPE:
            /* send these keys to the parent window (i.e., me) */
            SendMessage(handle_, msg, wpar, lpar);

            /* handled */
            return 0;

        default:
            /* let the original handler take care of other messages */
            break;
        }
        break;

    default:
        /* let the original handler have everything else */
        break;
    }

    /* send the message to the original handler */
    return CallWindowProc((WNDPROC)fld_defproc_, hwnd, msg, wpar, lpar);
}


/*
 *   destroy 
 */
void CTadsWinCreditsTads3::do_destroy()
{
    /* remove our font from the text box (so we can delete it) */
    SendMessage(fld_, WM_SETFONT, 0, MAKELPARAM(FALSE, 0));

    /* remove our subclassed window procedure for the text field */
    SetWindowLong(fld_, GWL_WNDPROC, (LONG)fld_defproc_);
    RemoveProp(fld_, "CTadsWinCreditsTads3.parent.this");

    /* delete our font */
    DeleteObject(tx_font_);
    
    /* inherit default */
    CTadsWinAboutDlg::do_destroy();
}

/*
 *   process a command 
 */
int CTadsWinCreditsTads3::do_command(int, int cmd, HWND)
{
    switch(cmd)
    {
    case CMD_ABOUT_CLOSE:
        /* send myself a close message */
        SendMessage(handle_, WM_CLOSE, 0, 0);
        return TRUE;

    default:
        /* ignore other commands */
        return FALSE;
    }
}

/*
 *   paint 
 */
void CTadsWinCreditsTads3::do_paint_content(HDC hdc, const RECT *area)
{
    /* paint in the same color as the text field */
    FillRect(hdc, area, GetSysColorBrush(COLOR_BTNFACE));
}
