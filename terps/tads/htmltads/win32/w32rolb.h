/* $Header$ */

/* Copyright (c) 2000 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  w32rolb.h - win32 rollover link-style button control
Function
  Implements a button control that acts like an HTML link
Notes
  
Modified
  01/21/00 MJRoberts  - Creation
*/

#ifndef W32ROLB_H
#define W32ROLB_H

#include "tadswin.h"

/* drawing states */
enum tadsrollbtn_state_t
{
    TADSROLLBTN_NORMAL,
    TADSROLLBTN_HOVER,
    TADSROLLBTN_CLICKED
};

class CTadsWinRollBtn: public CTadsWin
{
public:
    CTadsWinRollBtn(const char *txt, int cmd);
    ~CTadsWinRollBtn();

    /* 
     *   initialize a RECT with the size of a label - sets rc->right to
     *   rc->left plus the width of the label, and rc->bottom to rc->top
     *   plus the height of the label 
     */
    void set_size(RECT *rc);

    /* paint the window */
    void do_paint_content(HDC hdc, const RECT *area);

    /* process a keystroke */
    int do_keydown(int vkey, long keydata);

    /* process focus changes */
    void do_setfocus(HWND prv);
    void do_killfocus(HWND nxt);

    /* window style - we're a transparent child window */
    DWORD get_winstyle() { return WS_CHILD; }
    DWORD get_winstyle_ex() { return WS_EX_TRANSPARENT; }

    /* process a click */
    void process_click(int from_kb);

    /* mouse events */
    int do_leftbtn_down(int keys, int x, int y, int clicks);
    int do_leftbtn_up(int keys, int x, int y);
    int do_mousemove(int keys, int x, int y);
    int do_capture_changed(HWND new_capture_win);

    /* cursor change event */
    int do_setcursor(HWND hwnd, int hittest, int mousemsg);

    /* erase the background - let the paint routine do the work */
    int do_erase_bkg(HDC) { return 1; }

protected:
    /* invalidate my area */
    void inval();
    
    /* 
     *   command ID (we send this to the parent in a WM_COMMAND message
     *   when clicked) 
     */
    int cmd_;
    
    /* text of the button */
    char *txt_;

    /* current drawing state */
    tadsrollbtn_state_t state_;

    /* font */
    HFONT font_;

    /* hand cursor */
    HCURSOR hand_csr_;

    /* tracking mouse hovering (no button click) */
    unsigned int hovering_ : 1;

    /* tracking mouse motion with the button down */
    unsigned int tracking_ : 1;
    
};

#endif /* W32ROLB_H */

