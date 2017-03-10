#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* Copyright (c) 2000 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  w32rolb.cpp - rollover button control class
Function
  
Notes
  
Modified
  01/21/00 MJRoberts  - Creation
*/

#include <Windows.h>

#include "tadshtml.h"
#include "tadswin.h"
#include "tadsapp.h"
#include "w32rolb.h"


/*
 *   create 
 */
CTadsWinRollBtn::CTadsWinRollBtn(const char *txt, int cmd)
{
    /* make a copy of the text */
    txt_ = (char *)th_malloc(strlen(txt) + 1);
    strcpy(txt_, txt);

    /* remember my command ID */
    cmd_ = cmd;

    /* create my font - use the default dialog font, but underlined */
    font_ = CreateFont(CTadsFont::calc_lfHeight(8), 0,
                       0, 0, FW_BOLD, FALSE, TRUE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, "MS Sans Serif");

    /* load the mouse tracking cursor */
    hand_csr_ = LoadCursor(CTadsApp::get_app()->get_instance(),
                           "HAND_CURSOR");

    /* start in normal mode */
    state_ = TADSROLLBTN_NORMAL;

    /* no mouse tracking yet */
    hovering_ = FALSE;
    tracking_ = FALSE;
}

/*
 *   destroy 
 */
CTadsWinRollBtn::~CTadsWinRollBtn()
{
    /* free our text */
    th_free(txt_);

    /* delete my font */
    DeleteObject(font_);

    /* delete the hand cursor */
    DestroyCursor(hand_csr_);
}

/*
 *   initialize a RECT with the size of the label 
 */
void CTadsWinRollBtn::set_size(RECT *rc)
{
    HDC hdc;
    SIZE txtsiz;
    HGDIOBJ oldfont;

    /* get the dc in the parent window */
    hdc = GetDC(handle_);

    /* select my font */
    oldfont = SelectObject(hdc, font_);

    /* calculate the size of the label */
    GetTextExtentPoint32(hdc, txt_, strlen(txt_), &txtsiz);

    /* 
     *   set the right and bottom elements of the rectangle - add in two
     *   pixels of height and width to account for our shadow 
     */
    rc->right = rc->left + txtsiz.cx + 2;
    rc->bottom = rc->top + txtsiz.cy + 2;

    /* restore the old font */
    SelectObject(hdc, oldfont);

    /* release the parent dc */
    ReleaseDC(handle_, hdc);
}

/*
 *   paint 
 */
void CTadsWinRollBtn::do_paint_content(HDC hdc, const RECT *)
{
    int oldbkmode;
    COLORREF oldtxtclr;
    COLORREF txtcolor;
    HGDIOBJ oldfont;
    
    /* select my font */
    oldfont = SelectObject(hdc, font_);

    /* set transparent mode */
    oldbkmode = SetBkMode(hdc, TRANSPARENT);

    /* draw the shadow in black below and to the right */
    oldtxtclr = SetTextColor(hdc, RGB(0x00, 0x00, 0x00));
    ExtTextOut(hdc, 2, 1, 0, 0, txt_, strlen(txt_), 0);

    /* figure out what color to use based on the state */
    switch(state_)
    {
    case TADSROLLBTN_NORMAL:
        /* normal link color */
        txtcolor = RGB(0x00, 0xff, 0xff);
        break;

    case TADSROLLBTN_HOVER:
        /* hover color */
        txtcolor = RGB(0xff, 0x00, 0xff);
        break;

    case TADSROLLBTN_CLICKED:
        /* clicked color */
        txtcolor = RGB(0xff, 0x00, 0x00);
        break;
    }

    /* set the text color */
    SetTextColor(hdc, txtcolor);

    /* draw my text for the main label */
    ExtTextOut(hdc, 0, 0, 0, 0, txt_, strlen(txt_), 0);

    /* draw the focus rectangle if I have focus */
    if (GetFocus() == handle_)
    {
        RECT rc;

        GetClientRect(handle_, &rc);
        DrawFocusRect(hdc, &rc);
    }

    /* restore drawing state */
    SetTextColor(hdc, oldtxtclr);
    SetBkMode(hdc, oldbkmode);
    SelectObject(hdc, oldfont);
}

/*
 *   process a keystroke 
 */
int CTadsWinRollBtn::do_keydown(int vkey, long keydata)
{
    switch(vkey)
    {
    case VK_SPACE:
    case VK_RETURN:
        /* do my click action */
        process_click(TRUE);
        return TRUE;

    case VK_ESCAPE:
    case VK_TAB:
        /* send these to my parent */
        if (GetParent(handle_) != 0)
            SendMessage(GetParent(handle_), WM_KEYDOWN, (WPARAM)vkey,
                        (LPARAM)keydata);
        return TRUE;
        
    default:
        /* inherit default processing */
        return CTadsWin::do_keydown(vkey, keydata);
    }
}

/*
 *   process a click 
 */
void CTadsWinRollBtn::process_click(int /*from_kb*/)
{
    /* send my parent my command */
    SendMessage(GetParent(handle_), WM_COMMAND, cmd_, (LPARAM)handle_);
}

/*
 *   gain focus 
 */
void CTadsWinRollBtn::do_setfocus(HWND prv)
{
    /* redraw with focus appearance */
    inval();

    /* inherit default */
    CTadsWin::do_setfocus(prv);
}

/*
 *   lose focus 
 */
void CTadsWinRollBtn::do_killfocus(HWND nxt)
{
    /* redraw without focus appearance */
    inval();

    /* inherit default */
    CTadsWin::do_killfocus(nxt);
}

/*
 *   invalidate the control's area 
 */
void CTadsWinRollBtn::inval()
{
    POINT pt;
    RECT rc;

    /* invalidate my entire area */
    InvalidateRect(handle_, 0, FALSE);

    /* 
     *   invalidate my entire area in the parent window so that we redraw
     *   the underlying background - get my area
     */
    GetClientRect(handle_, &rc);

    /* get the offset to my area in the parent's coordinate system */
    pt.x = pt.y = 0;
    ClientToScreen(handle_, &pt);
    ScreenToClient(GetParent(handle_), &pt);

    /* 
     *   offset my area to the parent's coordinate system and invalidate
     *   the area in the parent window 
     */
    OffsetRect(&rc, pt.x, pt.y);
    InvalidateRect(GetParent(handle_), &rc, FALSE);
}

/*
 *   mouse movement 
 */
int CTadsWinRollBtn::do_mousemove(int keys, int x, int y)
{
    POINT pt;
    HWND win;

    /* determine what window the mouse is currently over */
    pt.x = x;
    pt.y = y;
    ClientToScreen(handle_, &pt);
    win = WindowFromPoint(pt);

    /* see if we're tracking */
    if (tracking_)
    {
        /* 
         *   check to see if the mouse is in our window and set our state
         *   accordingly 
         */
        if (win == handle_)
        {
            /* it's in our window - draw clicked if it's not already */
            if (state_ != TADSROLLBTN_CLICKED)
            {
                state_ = TADSROLLBTN_CLICKED;
                inval();
            }
        }
        else
        {
            /* it's not in our window - draw normal if it's not already */
            if (state_ != TADSROLLBTN_NORMAL)
            {
                state_ = TADSROLLBTN_NORMAL;
                inval();
            }
        }
    }
    else if (hovering_)
    {
        /* if the mouse is no longer in our window, stop hover tracking */
        if (win != handle_)
        {
            /* release our capture */
            ReleaseCapture();
        }
    }
    else
    {
        /* we're not tracking yet - capture the mouse */
        SetCapture(handle_);

        /* note that we're tracking hovering */
        hovering_ = TRUE;

        /* set the state */
        state_ = TADSROLLBTN_HOVER;

        /* redraw in the new state */
        inval();
    }

    /* handled */
    return TRUE;
}

/*
 *   process a capture change 
 */
int CTadsWinRollBtn::do_capture_changed(HWND)
{
    /* if we're hovering or tracking, switch back to normal mode */
    if (tracking_)
    {
        POINT pt;
        HWND win;

        /* determine what window the mouse is currently over */
        GetCursorPos(&pt);
        win = WindowFromPoint(pt);

        /* no longer tracking or hovering */
        tracking_ = FALSE;
        hovering_ = FALSE;

        /* set my sate back to normal and redraw */
        state_ = TADSROLLBTN_NORMAL;
        inval();

        /* 
         *   if it's still in our window, process a click; otherwise,
         *   they've moved off the button and released the mouse, so we
         *   won't click 
         */
        if (win == handle_)
        {
            /* process a click */
            process_click(FALSE);
        }
    }
    else if (hovering_)
    {
        /* no longer hovering */
        hovering_ = FALSE;
        
        /* switch back to normal state and redraw */
        state_ = TADSROLLBTN_NORMAL;
        inval();
    }

    /* handled */
    return TRUE;
}

/*
 *   left button click 
 */
int CTadsWinRollBtn::do_leftbtn_down(int, int, int, int)
{
    /* move focus here */
    SetFocus(handle_);
    
    /* capture the mouse until the button is released */
    SetCapture(handle_);

    /* switch to clicked state and redraw */
    state_ = TADSROLLBTN_CLICKED;
    inval();

    /* note that we're tracking */
    tracking_ = TRUE;
    
    /* handled */
    return TRUE;
}

/*
 *   left button release 
 */
int CTadsWinRollBtn::do_leftbtn_up(int keys, int, int)
{
    /* end capture */
    ReleaseCapture();

    /* handled */
    return TRUE;
}

/*
 *   cursor change event 
 */
int CTadsWinRollBtn::do_setcursor(HWND hwnd, int hittest, int mousemsg)
{
    /* if it's in our window, set the hand cursor */
    if (hwnd == handle_)
    {
        SetCursor(hand_csr_);
        return TRUE;
    }

    /* inherit default */
    return CTadsWin::do_setcursor(hwnd, hittest, mousemsg);
}
