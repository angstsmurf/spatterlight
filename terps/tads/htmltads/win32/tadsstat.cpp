#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadsstat.cpp,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsstat.cpp - status line implementation
Function
  
Notes
  
Modified
  10/26/97 MJRoberts  - Creation
*/

#include <windows.h>
#include <commctrl.h>

#ifndef TADSSTAT_H
#include "tadsstat.h"
#endif
#ifndef TADSWIN_H
#include "tadswin.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Status line 
 */

CTadsStatusline::CTadsStatusline(CTadsWin *parent, int sizebox, int id)
{
    /* make sure common controls are initialized */
    InitCommonControls();
    
    /* create and show the status line control */
    handle_ = CreateStatusWindow(WS_CHILD | (sizebox ? SBARS_SIZEGRIP : 0),
                                 "", parent->get_handle(), id);
    ShowWindow(handle_, SW_SHOW);

    /* no sources yet */
    sources_ = 0;
}

CTadsStatusline::~CTadsStatusline()
{
    CTadsStatusSourceListitem *nxt;
    
    /* delete all of our list items */
    for ( ; sources_ != 0 ; sources_ = nxt)
    {
        /* remember the next souce */
        nxt = sources_->nxt_;

        /* delete the current source */
        delete sources_;
    }
}

/*
 *   register a new status source 
 */
void CTadsStatusline::register_source(CTadsStatusSource *source)
{
    CTadsStatusSourceListitem *item;
    
    /* create a new list item */
    item = new CTadsStatusSourceListitem(source);

    /* put it at the head of the list */
    item->nxt_ = sources_;
    sources_ = item;
}

/*
 *   unregister a source 
 */
void CTadsStatusline::unregister_source(CTadsStatusSource *source)
{
    CTadsStatusSourceListitem *item;

    /* find this item and unlink it from the list */
    item = unlink(source);

    /* 
     *   if we found the list container, delete it, since we no longer
     *   have any use for it 
     */
    if (item != 0)
        delete item;
}

/*
 *   Update the status line.  Run through the list of sources until we
 *   find a message, then display that message. 
 */
void CTadsStatusline::update()
{
    CTadsStatusSourceListitem *cur;
    textchar_t *msg = 0;
    int caller_deletes = FALSE;

    /* go through each source until we find a message */
    for (cur = sources_ ; cur != 0 ; cur = cur->nxt_)
    {
        /* ask this item for a message; if we found one, stop looking */
        msg = cur->item_->get_status_message(&caller_deletes);
        if (msg != 0)
            break;
    }

    /* if we didn't find a message, display an empty message */
    if (msg == 0)
        msg = "";

    /* display the message */
    SendMessage(handle_, SB_SETTEXT, 0, (LPARAM)msg);

    /* 
     *   update the status line immediately, in case we're in the middle
     *   of a long-running operation and we won't check for events for a
     *   while 
     */
    UpdateWindow(handle_);

    /* we're done with the message - if we're to delete it, do so */
    if (caller_deletes)
        delete [] msg;
}

/*
 *   Bring a source to the front of the source list, and update the
 *   message 
 */
void CTadsStatusline::source_to_front(CTadsStatusSource *source)
{
    CTadsStatusSourceListitem *cur;
    
    /* find this source in the list and unlink it from the list */
    cur = unlink(source);

    /* if we found it, link it back in at the start of the list */
    if (cur != 0)
    {
        cur->nxt_ = sources_;
        sources_ = cur;
    }

    /* update the status line message to account for the change */
    update();
}

/*
 *   Notify the status line that the parent window has been resized 
 */
void CTadsStatusline::notify_parent_resize()
{
    /* 
     *   tell the status line control to resize itself; we don't need to
     *   tell it anything more (it would ignore us if we did), since the
     *   control is able to figure out where it should go based on the
     *   parent window size 
     */
    MoveWindow(handle_, 0, 0, 0, 0, TRUE);

    /* 
     *   immediately redraw the status line, in case we're in the middle
     *   of an operation that will suspend event handling for a while 
     */
    UpdateWindow(handle_);
}

/*
 *   Unlink an item from the list 
 */
CTadsStatusSourceListitem *CTadsStatusline::unlink(CTadsStatusSource *source)
{
    CTadsStatusSourceListitem *cur;
    
    /* 
     *   if it's at the head of the list, unlink the head item; otherwise,
     *   we'll need to find the previous item in the list 
     */
    if (sources_ != 0 && sources_->item_ == source)
    {
        /* it's at the head */
        cur = sources_;

        /* advance the head to the next item in the list */
        sources_ = sources_->nxt_;

        /* unlink this item and return it */
        cur->nxt_ = 0;
        return cur;
    }
    else
    {
        CTadsStatusSourceListitem *prv;

        /* find the previous item in the list */
        for (prv = sources_ ;
             prv != 0 && prv->nxt_ != 0 && prv->nxt_->item_ != source ;
             prv = prv->nxt_);

        /* if we found it, remove it, and put it back in at the front */
        if (prv != 0)
        {
            /* remember the container item that we're removing */
            cur = prv->nxt_;

            /* unlink this item */
            prv->nxt_ = cur->nxt_;
            cur->nxt_ = 0;

            /* return the item */
            return cur;
        }

        /* we didn't find it in the list */
        return 0;
    }
}

