/* 
 *   Copyright (c) 2001 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadscar.h - TADS caret for win32
Function
  Provides a caret (flashing insertion point), implemented without the
  weird restrictions of the win32 native caret.
Notes
  The win32 native caret has a design element that makes it extremely
  difficult to use in some situations: there is only one caret for each
  message queue.  Usually, an app is designed with just one message queue,
  so it gets just one caret for the whole application.

  The problem this creates is that caret handling can't be readily
  encapsulated in a single window or single operation.  Certain operations
  must hide the caret while they're taking place, but restore it after
  they're done - a mouse drag operation, for example.  In some of these
  cases, the caret must be shown in the same or another window during the
  operation - a drag-and-drop operation where the user moves the mouse
  cursor, with dragged data "in hand," as it were, over a text window, for
  example, displays the caret as the mouse moves over the text area to show
  the insertion point if the user were to release the mouse button to drop
  the data at any given time.

  This caret implementation doesn't have the one-caret-per-queue
  restriction of the win32 native caret, so an app can use this class to
  manipulate several carets at once.

  Note that this class is designed specifically to overcome the
  programming model problems of the win32 native caret, not to create a user
  interface where multiple carets are showing at once.  Even though this
  class could be used to show several carets at once, it should not be used
  this way; apps using this code should follow win32 GUI guidelines and
  actually make visible only one caret at a time.
Modified
  04/02/01 MJRoberts  - Creation
*/

#ifndef TADSCAR_H
#define TADSCAR_H

#include <Windows.h>


class CTadsCaret
{
public:
    /* 
     *   Create the caret - a caret is always associated with a particular
     *   window.  Initially, the caret is hidden; show() must be used to
     *   make the caret visible.  
     */
    CTadsCaret(class CTadsWin *win);

    /* destroy the caret - removes the caret visually, if it's showing */
    ~CTadsCaret();

    /* 
     *   set the caret's position - this sets the position of the upper left
     *   corner of the caret's bounding rectangle 
     */
    void set_pos(int x, int y);

    /* set the caret's size */
    void set_size(int width, int height);

    /* 
     *   Hide the caret.  Removes the caret immediately from the display, if
     *   it's showing, and prevents it from showing again.  This call is
     *   cumulative: an equal number of show() calls must be made before the
     *   caret becomes visible again.  
     */
    void hide();

    /*
     *   Show the caret.  Reverses the effect of hide().
     */
    void show();
    
protected:
    /* draw - make the caret visible if it isn't already */
    void draw()
    {
        /* draw the caret only if it's currently invisible */
        if (!flash_state_ && show_level_ > 0)
            invert();
    }

    /* 
     *   undraw - make the caret invisible if it isn't already, and if the
     *   caret isn't marked as hidden 
     */
    void undraw()
    {
        /* remove the drawing only if it's visible */
        if (flash_state_ && show_level_ > 0)
            invert();
    }

    /* 
     *   invert our display area, update the flash state flag, and update
     *   the flash timer 
     */
    void invert();

    /* are we currently showing on the screen? */
    int is_showing() const { return show_level_ > 0 && flash_state_; }

    /* timer callback function */
    static void CALLBACK timer_cb(HWND hwnd, UINT msg, UINT id,
                                 DWORD time);

    /* timer callback for an individual caret */
    void flash_timer();

    /* unlink me from the active list, if I'm in it */
    void unlink_from_active();

    /* my window */
    class CTadsWin *win_;
    
    /* current position in my window */
    int x_;
    int y_;

    /* width and height of the caret's rectangle */
    int width_;
    int height_;

    /* 
     *   Current 'show' level - the caret is visible if this is at least 1.
     *   show() increments this and hide() decrements this. 
     */
    int show_level_;

    /* 
     *   current flash state - true means the caret is flashed on (i.e.,
     *   showing), false means it's flashed off (i.e., nothing is visible) 
     */
    int flash_state_;

    /*
     *   Time (in system ticks) at which the next flash state change is to
     *   occur.  
     */
    DWORD flash_time_;

    /* next/previous entries in master list of active and visible carets */
    CTadsCaret *next_active_;
    CTadsCaret *prev_active_;

    /* timer ID */
    static UINT timer_id_;

    /* number of instances in existence */
    static int instances_;

    /* head of list of active and visible carets */
    static CTadsCaret *first_active_;
};

#endif /* TADSCAR_H */

