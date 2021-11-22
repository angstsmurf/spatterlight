/* 
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlmng.cpp - MNG (animated) image object
Function
  
Notes
  
Modified
  05/03/02 MJRoberts  - Creation
*/

#include <libmng.h>

#include <os.h>
#include "tadshtml.h"
#include "htmlmng.h"
#include "htmlsys.h"


/*
 *   create 
 */
CHtmlMng::CHtmlMng(CHtmlMngClient *client)
{
    /* remember the client */
    client_ = client;

    /* clear our variables */
    handle_ = 0;
    height_ = 0;
    width_ = 0;
    fp_ = 0;
    file_name_ = 0;
    file_seek_start_ = 0;
    file_size_ = 0;
    file_rem_ = 0;
    resume_time_ = 0;
    state_ = HTMLMNG_STATE_INIT;
    paused_ = FALSE;
    pause_event_is_pending_ = FALSE;
    pause_event_delta_ = 0;
    sys_image_ = 0;
    event_is_pending_ = FALSE;
    pending_event_time_ = 0;
}

/*
 *   delete 
 */
CHtmlMng::~CHtmlMng()
{
    /* close our MNG handle */
    if (handle_ != 0)
        mng_cleanup(&handle_);

    /* if we haven't yet closed our input stream, do so now */
    if (fp_ != 0)
        osfcls(fp_);
}

/* 
 *   Start loading an image from an MNG file.  This will initialize reading
 *   and load the first frame.  
 */
int CHtmlMng::start_mng_read(const textchar_t *filename,
                             unsigned long seekpos, unsigned long filesize)
{
    mng_retcode ret;
    
    /* remember the file information */
    file_name_ = filename;
    file_seek_start_ = seekpos;
    file_size_ = filesize;

    /* create our MNG handle */
    handle_ = mng_initialize(this, &alloc_cb, &free_cb, 0);

    /* set up the MNG callbacks */
    mng_setcb_openstream(handle_, open_cb);
    mng_setcb_closestream(handle_, close_cb);
    mng_setcb_readdata(handle_, read_cb);
    mng_setcb_processheader(handle_, process_header_cb);
    mng_setcb_getcanvasline(handle_, getcanvasline_cb);
    mng_setcb_refresh(handle_, refresh_cb);
    mng_setcb_gettickcount(handle_, gettickcount_cb);
    mng_setcb_settimer(handle_, settimer_cb);

    /* start the read-and-display process */
    ret = mng_readdisplay(handle_);

    /* check our return code */
    switch(ret)
    {
    case MNG_NEEDTIMERWAIT:
        /* 
         *   We need to resume after a given delay.  The MNG decoder will
         *   have invoked our settimer callback to tell us the amount of time
         *   to wait, and we will have noted it.  Set our state to 'waiting
         *   to draw first frame', so that we can actually start playback
         *   when we draw the first frame.  
         */
        state_ = HTMLMNG_STATE_WAIT_FIRST;

        /* tell the caller we were successful */
        return 0;

    case MNG_NOERROR:
        /*
         *   Success.  Since we don't need a timer wait, we're done.  Set our
         *   state to 'done' and return success.  
         */
        state_ = HTMLMNG_STATE_DONE;
        return 0;

    default:
        /* anything else is an error */
        return 1;
    }
}

/* MNG callback: allocate and clear memory */
mng_ptr CHtmlMng::alloc_cb(mng_size_t len)
{
    mng_ptr mem;

    /* allocate the memory */
    mem = (mng_ptr)th_malloc(len);

    /* clear it to zeroes */
    memset(mem, 0, len);

    /* return the memory pointer */
    return mem;
}

/* MNG callback: free memory allocated with alloc_cb */
void CHtmlMng::free_cb(mng_ptr ptr, mng_size_t)
{
    /* free the memory */
    th_free(ptr);
}

/* MNG callback: open the input stream */
mng_bool CHtmlMng::open_cbm()
{
    /* open the file - if we can't, return failure */
    fp_ = osfoprb(file_name_, OSFTBIN);
    if (fp_ == 0)
        return MNG_FALSE;

    /* seek to the start of our stream */
    if (file_seek_start_ != 0)
        osfseek(fp_, file_seek_start_, OSFSK_SET);

    /* we have the entire stream left to read */
    file_rem_ = file_size_;

    /* success */
    return MNG_TRUE;
}

/* MNG callback: close the input stream */
mng_bool CHtmlMng::close_cbm()
{
    /* close the stream if we opened it */
    if (fp_ != 0)
        osfcls(fp_);

    /* forget the stream */
    fp_ = 0;

    /* success */
    return MNG_TRUE;
}

/* MNG callback: read from the input stream */
mng_bool CHtmlMng::read_cbm(mng_ptr buf, mng_uint32 len, mng_uint32p actualp)
{
    /* 
     *   adjust our request size downwards if we don't have enough data left
     *   in the stream to satisfy the request 
     */
    if (len > file_rem_)
        len = file_rem_;

    /* deduct the amount requested from the remaining stream length */
    file_rem_ -= len;

    /* read the data, returning the amount */
    *actualp = osfrbc(fp_, buf, len);

    /* indicate success */
    return MNG_TRUE;
}

/*
 *   MNG callback: process the header 
 */
mng_bool CHtmlMng::process_header_cbm(mng_uint32 wid, mng_uint32 ht)
{
    /* ask the client to initialize the canvas */
    if (client_->init_mng_canvas(handle_, wid, ht))
    {
        /* error initializing the canvas - indicate failure */
        return MNG_FALSE;
    }

    /* success */
    return MNG_TRUE;
}

/* MNG callback: get a canvas line */
mng_ptr CHtmlMng::getcanvasline_cbm(mng_uint32 rownum)
{
    /* ask the client for it */
    return client_->get_mng_canvas_row(rownum);
}

/* MNG callback: refresh the display */
mng_bool CHtmlMng::refresh_cbm(mng_uint32 x, mng_uint32 y,
                               mng_uint32 wid, mng_uint32 ht)
{
    /* let the client know about the update */
    client_->notify_mng_update(x, y, wid, ht);

    /* success */
    return MNG_TRUE;
}

/* MNG callback: set timer */
mng_bool CHtmlMng::settimer_cbm(mng_uint32 msecs)
{
    /* check our playback state */
    switch(state_)
    {
    case HTMLMNG_STATE_INIT:
        /* 
         *   We're initializing, so simply remember the timer interval.  When
         *   we draw the first frame, we'll set up an actual timer. 
         */
        resume_time_ = msecs;
        break;

    case HTMLMNG_STATE_PLAYING:
        /*
         *   We're already in playback mode, so ask the client to set an
         *   actual timer for requested delay.  
         */
        client_->set_mng_timer(msecs);

        /* 
         *   In any case, note that we have an event pending and note the
         *   actual system time at which the event is meant to occur.  Even
         *   if we don't have a display site now, we can use this information
         *   to schedule a timer event when we do get a display site.  We
         *   also need this information if the display site changes while
         *   we're still waiting for the timer to expire.  
         */
        event_is_pending_ = TRUE;
        pending_event_time_ = os_get_sys_clock_ms() + msecs;

        /* done */
        break;
        
    case HTMLMNG_STATE_WAIT_FIRST:
        /* we shouldn't get a timer request in this state */
        break;

    case HTMLMNG_STATE_DONE:
        /* playback has stopped, so ignore the request */
        break;
    }

    /* success */
    return MNG_TRUE;
}

/*
 *   Get the 'this' pointer from an MNG handle. 
 */
CHtmlMng *CHtmlMng::get_this(mng_handle handle)
{
    return (CHtmlMng *)mng_get_userdata(handle);
}

/*
 *   Receive notification of drawing from the system-specific client. 
 */
void CHtmlMng::notify_draw()
{
    /* check our playback state */
    switch(state_)
    {
    case HTMLMNG_STATE_WAIT_FIRST:
        /*
         *   We're waiting to draw the first frame, and we've just received
         *   notification that we've drawn it.  If we're not paused, set up a
         *   timer event so we'll be notified when the first event on the
         *   clock arrives.  
         */
        if (!paused_)
        {
            /* ask the client to set up a timer */
            client_->set_mng_timer(resume_time_);

            /* note the system time at which the pending event is to occur */
            event_is_pending_ = TRUE;
            pending_event_time_ = os_get_sys_clock_ms() + resume_time_;
            
            /* our state is now 'playing' */
            state_ = HTMLMNG_STATE_PLAYING;
        }
        break;

    default:
        /* in other states, drawing is nothing special */
        break;
    }
}

/*
 *   Notify of a timer event. 
 */
void CHtmlMng::notify_timer()
{
    /* 
     *   if we're not in playback mode, or we're paused, or we don't have an
     *   underlying MNG object, this is spurious - ignore it 
     */
    if (state_ != HTMLMNG_STATE_PLAYING || paused_ || handle_ == 0)
        return;

    /* ask the MNG decoder to resume decoding and displaying */
    switch(mng_display_resume(handle_))
    {
    case MNG_NEEDTIMERWAIT:
        /* 
         *   The MNG decoder wants a new timer delay.  We will already have
         *   set up the new timer (in the settimer callback from libmng), so
         *   we have nothing left to do now.  
         */
        break;

    default:
        /* 
         *   In any other case (success or error), we're done with playback,
         *   so cancel any pending timer with the client and set our state to
         *   'done', so we know we have no more decoding left to do.  
         */
        state_ = HTMLMNG_STATE_DONE;
        client_->cancel_mng_timer();

        /* note that we no longer have any events pending */
        event_is_pending_ = FALSE;

        /* we have no more need for the libmng object - clean it up */
        if (handle_ != 0)
        {
            /* clean up the handle */
            mng_cleanup(&handle_);

            /* we no longer have a libmng handle */
            handle_ = 0;
        }
        break;
    }
}

/*
 *   Pause playback 
 */
void CHtmlMng::pause_playback()
{
    /* 
     *   if we have no underlying MNG object, or we're already paused,
     *   there's nothing to do 
     */
    if (handle_ == 0 || paused_)
        return;

    /* check our state */
    switch(state_)
    {
    case HTMLMNG_STATE_WAIT_FIRST:
        /* note that we're now paused */
        paused_ = TRUE;

        /* done */
        break;

    case HTMLMNG_STATE_PLAYING:
        /* note that we're now paused */
        paused_ = TRUE;

        /* 
         *   we should have a timer event set up - remember the interval to
         *   the next scheduled event so that we can restore the timer for
         *   the same relative time when we resume 
         */
        pause_event_is_pending_ = event_is_pending_;
        if (event_is_pending_)
        {
            long tcur;
            
            /* cancel the event */
            client_->cancel_mng_timer();
            event_is_pending_ = FALSE;

            /* remember the interval to the event */
            tcur = os_get_sys_clock_ms();
            if (tcur < pending_event_time_)
                pause_event_delta_ = pending_event_time_ - tcur;
            else
                pause_event_delta_ = 0;
        }

        /* done */
        break;

    default:
        break;
    }
}

/*
 *   Resume playback
 */
void CHtmlMng::resume_playback()
{
    /* 
     *   if we have no underlying MNG object, or we're not paused, or we're
     *   not in a playing state, there's nothing to do 
     */
    if (handle_ == 0 || !paused_
        || (state_ != HTMLMNG_STATE_WAIT_FIRST
            && state_ != HTMLMNG_STATE_PLAYING))
        return;

    /* if we had an event pending, set it up again */
    if (pause_event_is_pending_)
    {
        /* set up a timer for the event time */
        client_->set_mng_timer(pause_event_delta_);

        /* remember the event */
        event_is_pending_ = TRUE;
        pending_event_time_ = os_get_sys_clock_ms() + pause_event_delta_;
    }

    /* no longer paused */
    paused_ = FALSE;
}

