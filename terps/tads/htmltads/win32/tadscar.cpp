/* 
 *   Copyright (c) 2001 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadscar.cpp - TADS caret for win32
Function
  
Notes
  
Modified
  04/02/01 MJRoberts  - Creation
*/


#include <Windows.h>

#include "tadscar.h"
#include "tadswin.h"

/* statics */
int CTadsCaret::instances_ = 0;
UINT CTadsCaret::timer_id_ = 0;
CTadsCaret *CTadsCaret::first_active_ = 0;

/*
 *   Create 
 */
CTadsCaret::CTadsCaret(class CTadsWin *win)
{
    /* remember the window */
    win_ = win;

    /* we haven't been shown yet */
    show_level_ = 0;

    /* 
     *   we're not showing, so the flash state and time are irrelevant, but
     *   initialize them to something anyway 
     */
    flash_state_ = FALSE;
    flash_time_ = 0;

    /* our position and size are not yet known */
    x_ = y_ = 0;
    width_ = height_ = 0;

    /* count the instance */
    ++instances_;

    /* 
     *   If this is the only instance, set a timer - fire it off at a
     *   fraction of the system caret blink time to ensure we are notified
     *   in a timely fashion.  Note that there's only one timer for all of
     *   the carets, no matter how many instances we have.  
     */
    if (instances_ == 1)
        timer_id_ = SetTimer(0, 0, GetCaretBlinkTime()/5,
                             (TIMERPROC)&timer_cb);

    /* not in the active list yet */
    next_active_ = prev_active_ = 0;
}

/*
 *   Destroy 
 */
CTadsCaret::~CTadsCaret()
{
    /* hide ourselves to ensure we don't show again */
    hide();

    /* take me out of the active list */
    unlink_from_active();

    /* count the loss of an instance */
    --instances_;

    /* if this was the last instance, kill our class timer */
    if (instances_ == 0)
    {
        /* kill the timer */
        KillTimer(0, timer_id_);

        /* forget its identifier */
        timer_id_ = 0;
    }
}

/*
 *   invert our area on screen 
 */
void CTadsCaret::invert()
{
    HDC dc;
    RECT rc;

    /* 
     *   If we're adding the caret, make sure the window is fully up to date
     *   before we do the drawing - since we're inverting the window
     *   directly, we need everything to be exactly as it is right after
     *   drawing for this to work properly.  If we're removing the caret
     *   leave things as they are, since we must trust the window not to do
     *   any painting as long as the caret is visible - it must always
     *   remove the caret before painting.  
     */
    if (!flash_state_)
        UpdateWindow(win_->get_handle());

    /* get the dc for our window */
    dc = GetDC(win_->get_handle());

    /* set up a rectangle for our drawing area */
    SetRect(&rc, x_, y_, x_ + width_, y_ + height_);
    
    /* invert the area under the rectangle */
    InvertRect(dc, &rc);

    /* release our window's dc */
    ReleaseDC(win_->get_handle(), dc);

    /* update the state flag */
    flash_state_ = !flash_state_;

    /* 
     *   update the flash timer, so that we show the caret for a full blink
     *   in the current state 
     */
    flash_time_ = GetTickCount() + GetCaretBlinkTime();
}

/*
 *   Set the position 
 */
void CTadsCaret::set_pos(int x, int y)
{
    /* if we're not changing the position, ignore it */
    if (x_ == x && y_ == y)
        return;

    /* remove drawing */
    undraw();

    /* note the new position */
    x_ = x;
    y_ = y;

    /* draw it again at the new position, if it's not hidden */
    draw();
}

/*
 *   Set the size 
 */
void CTadsCaret::set_size(int width, int height)
{
    /* if we're not changing the size, ignore it */
    if (width_ == width && height_ == height)
        return;

    /* remove drawing */
    undraw();

    /* note the new size */
    width_ = width;
    height_ = height;

    /* draw it again at the new size, if it's not hidden */
    draw();
}

/*
 *   show the caret 
 */
void CTadsCaret::show()
{
    /* count the showing */
    ++show_level_;

    /* if we're just becoming visible, add us to the list of active carets */
    if (show_level_ == 1)
    {
        /* we just went from inactive to active - link us into the list */
        next_active_ = first_active_;
        prev_active_ = 0;
        if (first_active_ != 0)
            first_active_->prev_active_ = this;
        first_active_ = this;
    }

    /* draw it if it's now showing */
    draw();
}

/*
 *   hide the caret 
 */
void CTadsCaret::hide()
{
    /* remove it from the screen if it's showing */
    undraw();

    /* count the hiding */
    --show_level_;

    /* if I'm just becoming invisible, remove me from the active list */
    if (show_level_ == 0)
        unlink_from_active();
}

/*
 *   Unlink me from the active list, if I'm in it 
 */
void CTadsCaret::unlink_from_active()
{
    /* set the next object's back pointer */
    if (next_active_ != 0)
        next_active_->prev_active_ = prev_active_;

    /* set the previous object's forward pointer, or the head pointer */
    if (prev_active_ != 0)
        prev_active_->next_active_ = next_active_;
    else
        first_active_ = next_active_;

    /* I'm no longer in the list */
    next_active_ = prev_active_ = 0;
}

/*
 *   Timer callback for the whole class - visits each active caret and tells
 *   each one to update its flash state 
 */
void CALLBACK CTadsCaret::timer_cb(HWND, UINT, UINT, DWORD)
{
    CTadsCaret *cur;
    
    /* go through the active list and flash each one whose time has come */
    for (cur = first_active_ ; cur != 0 ; cur = cur->next_active_)
        cur->flash_timer();
}

/*
 *   Individual caret flash timer callback 
 */
void CTadsCaret::flash_timer()
{
    /* 
     *   if we've reached the next flash time, and the caret is not hidden,
     *   invert the flash state 
     */
    if (GetTickCount() >= flash_time_ && show_level_ > 0)
        invert();
}
