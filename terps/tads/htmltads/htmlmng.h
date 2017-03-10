/* 
 *   Copyright (c) 1997, 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlmng.h - MNG (animated) image object
Function

Notes

Modified
  05/03/02 MJRoberts  - creation
*/

#ifndef HTMLMNG_H
#define HTMLMNG_H

/* include MNG library header */
#include <libmng_types.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif

/* ------------------------------------------------------------------------ */
/*
 *   Options flags 
 */

/* 
 *   Always discard alpha information.  If this is set, if an image has an
 *   alpha channel, we'll explicitly create a default background for the
 *   image and blend the image into the default background.  
 */
#define HTMLMNG_OPT_NO_ALPHA        0x0001


/* ------------------------------------------------------------------------ */
/*
 *   Playback states. 
 */
enum htmlmng_state_t
{
    /* 
     *   Initializing.  In this state, we haven't yet finished reading the
     *   image header. 
     */
    HTMLMNG_STATE_INIT,

    /*
     *   Playing, waiting to draw first frame.  In this state, we've loaded
     *   the image, and we have a timer wait pending, but we have yet to
     *   display the first frame of the animation.  At this point, the
     *   playback clock is frozen until we draw the first frame.  We don't
     *   start the clock on playback until we've displayed the first image,
     *   in case we're doing a lot of loading work all at once (loading other
     *   images, for example) and thus might be too busy for the start of the
     *   animation to be displayed smoothly.  We assume that once we get
     *   around to showing the first frame, we're done with any batch of work
     *   that might have been underway and are ready to start servicing
     *   display updates for the animation.  
     */
    HTMLMNG_STATE_WAIT_FIRST,

    /*
     *   Playing.  In this state, we're attempting to show frames from the
     *   MNG in the time sequence specified in the file. 
     */
    HTMLMNG_STATE_PLAYING,

    /*
     *   Done.  In this state, we have either reached the end of the
     *   animation, or we've permanently frozen playback.  No further timed
     *   events will be processed.  
     */
    HTMLMNG_STATE_DONE,
};


/* ------------------------------------------------------------------------ */
/*
 *   MNG helper client interface.  This is an interface that must be
 *   implemented by the client code using the MNG helper.  In most cases,
 *   this can be implemented by the system-specific MNG display object.  
 */
class CHtmlMngClient
{
public:
    /*
     *   Select the canvas format and allocate the canvas.  The canvas is the
     *   memory into which the MNG decoder writes the pixels.  We let the
     *   client code handle the allocation of the canvas so that the client
     *   can display directly from the canvas rather than having to go
     *   through an intermediate copy.
     *   
     *   The client code should call mng_set_canvasstyle() on the MNG handle,
     *   and should set the background color if an alpha channel isn't
     *   desired and the image source has alpha information.
     *   
     *   Returns zero on success, non-zero on error.  
     */
    virtual int init_mng_canvas(mng_handle handle,
                                int width, int height) = 0;

    /* get a pointer to a row of pixels from the canvas */
    virtual char *get_mng_canvas_row(int rownum) = 0;

    /* 
     *   Receive notification of a change in the image.  If the client is
     *   double-buffering the image, the client must copy from the canvas to
     *   its other buffer.  If this client displays directly from the canvas,
     *   the client need do nothing here, because the MNG helper will have
     *   updated the canvas  directly.
     */
    virtual void notify_mng_update(int x, int y, int wid, int ht) = 0;

    /*
     *   Set a timer.  The client must call our notify_timer() method after
     *   the given number of milliseconds elapse. 
     */
    virtual void set_mng_timer(long msecs) = 0;

    /* cancel the timer previously set with set_mng_timer() */
    virtual void cancel_mng_timer() = 0;
};


/* ------------------------------------------------------------------------ */
/*
 *   MNG helper object.  This object is meant to encapsulate the MNG library,
 *   so that minimal platform-specific code is necessary.  This class is
 *   designed to be instantiated as a helper object by the platform-specific
 *   implementation (of CHtmlSysImageMng_xxx).  
 */
class CHtmlMng
{
public:
    CHtmlMng(class CHtmlMngClient *client);
    ~CHtmlMng();

    /* load an image from a file */
    int start_mng_read(const textchar_t *filename,
                       unsigned long seekpos, unsigned long filesize);

    /* halt playback */
    void halt_playback()
    {
        /* 
         *   set our state to 'done' - we'll ignore any further timer events
         *   once we're in this state 
         */
        state_ = HTMLMNG_STATE_DONE;

        /* note that we have timer event pending */
        event_is_pending_ = FALSE;
    }

    /* pause playback */
    void pause_playback();

    /* resume from pause */
    void resume_playback();

    /* get the dimensions */
    unsigned long get_height() const { return height_; }
    unsigned long get_width() const { return width_; }

    /* 
     *   Notify that we've just drawn.  The system-specific client should
     *   call this whenever it draws the image, so that we can take care of
     *   any timer-related processing when drawing occurs. 
     */
    void notify_draw();

    /*
     *   Notify of a timer event.  The system-specific client should simply
     *   pass along a notify_timer() call from the display site to us here. 
     */
    void notify_timer();

    /*
     *   Get the timer event status.  This indicates if we have a timer event
     *   pending, and if so, the system time (as given by
     *   os_get_sys_clock_ms()) at which the timer is scheduled to expire. 
     */
    int is_timer_pending() const { return event_is_pending_; }
    long get_timer_time() const { return pending_event_time_; }

private:
    /* allocate memory for the image */
    void allocate_rows(unsigned long height, unsigned long width,
                       int bits_per_pixel);

    /* MNG callback: allocate memory, cleared to all zeroes */
    static mng_ptr alloc_cb(mng_size_t len);

    /* MNG callback: free memory allocated with alloc_cb() */
    static void free_cb(mng_ptr ptr, mng_size_t);

    /* MNG callback: open stream */
    static mng_bool open_cb(mng_handle handle)
        { return get_this(handle)->open_cbm(); }
    mng_bool open_cbm();

    /* MNG callback: close stream */
    static mng_bool close_cb(mng_handle handle)
        { return get_this(handle)->close_cbm(); }
    mng_bool close_cbm();

    /* MNG callback: read from stream */
    static mng_bool read_cb(mng_handle handle, mng_ptr buf,
                            mng_uint32 len, mng_uint32p actualp)
        { return get_this(handle)->read_cbm(buf, len, actualp); }
    mng_bool read_cbm(mng_ptr buf, mng_uint32 len, mng_uint32p actualp);

    /* MNG callback: process header */
    static mng_bool process_header_cb(mng_handle handle,
                                      mng_uint32 wid, mng_uint32 ht)
        { return get_this(handle)->process_header_cbm(wid, ht); }
    mng_bool process_header_cbm(mng_uint32 wid, mng_uint32 ht);

    /* MNG callback: get a canvas line */
    static mng_ptr getcanvasline_cb(mng_handle handle,
                                    mng_uint32 rownum)
        { return get_this(handle)->getcanvasline_cbm(rownum); }
    mng_ptr getcanvasline_cbm(mng_uint32 rownum);
    
    /* MNG callback: refresh the display */
    static mng_bool refresh_cb(mng_handle handle,
                               mng_uint32 x, mng_uint32 y,
                               mng_uint32 wid, mng_uint32 ht)
        { return get_this(handle)->refresh_cbm(x, y, wid, ht); }
    mng_bool refresh_cbm(mng_uint32 x, mng_uint32 y,
                         mng_uint32 wid, mng_uint32 ht);

    /* MNG callback: get tick count; simply use osifc */
    static mng_uint32 gettickcount_cb(mng_handle handle)
        { return os_get_sys_clock_ms(); }

    /* MNG callback: set a timer */
    static mng_bool settimer_cb(mng_handle handle, mng_uint32 msecs)
        { return get_this(handle)->settimer_cbm(msecs); }
    mng_bool settimer_cbm(mng_uint32 msecs);
    
    /* 
     *   callback service routine: given an MNG handle, get 'this'; we
     *   stored our 'this' pointer in the user data in the MNG handle on
     *   creation, so simply get the user data from the MNG handle and cast
     *   it appropriately 
     */
    static CHtmlMng *get_this(mng_handle handle);

    /* client interface */
    class CHtmlMngClient *client_;

    /* our client system image resource object */
    class CHtmlSysImageAnimated *sys_image_;

    /* input file name, starting seek position, and stream size */
    const textchar_t *file_name_;
    unsigned long file_seek_start_;
    unsigned long file_size_;

    /* amount of data left to read from the input stream */
    unsigned long file_rem_;

    /* file handle */
    osfildef *fp_;

    /* our MNG handle */
    mng_handle handle_;

    /* number of rows allocated */
    unsigned long height_;

    /* width of each row */
    unsigned long width_;

    /*
     *   Resume timer.
     *   
     *   When we're in the 'initializing' state, this indicates the delay in
     *   milliseconds until the next event after the first frame.  When we
     *   draw the first frame, we'll officially start playback.
     *   
     *   When we're in the 'playing' state, this indicates the actual system
     *   time (as given by os_get_sys_clock_ms()) of the next resume event.  
     */
    long resume_time_;

    /*
     *   Next event time.  This keeps track of the next timer event we
     *   expect.  When our display site changes, and we have a pending event,
     *   we'll use this information to reset our event timer with the new
     *   site.  This time is an actual system time as given by
     *   os_get_sys_clock_ms(). 
     */
    long pending_event_time_;
    int event_is_pending_;

    /* current playback state */
    htmlmng_state_t state_;

    /* flag: playback is paused */
    int paused_;

    /* status to restore when we resume from a pause */
    int pause_event_is_pending_;
    long pause_event_delta_;
};

#endif /* HTMLMNG_H */
