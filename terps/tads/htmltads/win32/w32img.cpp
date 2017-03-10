#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/w32img.cpp,v 1.2 1999/05/17 02:52:26 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32img.cpp - tads html win32 image implementation
Function
  
Notes
  
Modified
  01/31/98 MJRoberts  - Creation
*/

#include <Windows.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLURL_H
#include "htmlurl.h"
#endif
#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif
#ifndef HTMLRC_H
#include "htmlrc.h"
#endif
#ifndef HTMLJPEG_H
#include "htmljpeg.h"
#endif
#ifndef HTMLPNG_H
#include "htmlpng.h"
#endif
#ifndef HTMLMNG_H
#include "htmlmng.h"
#endif
#ifndef TADSJPEG_H
#include "tadsjpeg.h"
#endif
#ifndef TADSPNG_H
#include "tadspng.h"
#endif
#ifndef TADSMNG_H
#include "tadsmng.h"
#endif
#ifndef W32IMG_H
#include "w32img.h"
#endif
#ifndef HTMLW32_H
#include "htmlw32.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   JPEG Image implementation 
 */

/* create a new system-specific JPEG display object */
CHtmlSysResource *CHtmlSysImageJpeg::
   create_jpeg(const CHtmlUrl *url, const textchar_t *filename,
               unsigned long seekpos, unsigned long filesize,
               CHtmlSysWin *win)
{
    CHtmlJpeg *jpeg;
    CHtmlSysImageJpeg_win32 *img = 0;
    int err;

    /* create the helper object to load the data */
    jpeg = new CHtmlJpeg();

    /* load the data from the file */
    err = jpeg->read_jpeg_file(filename, seekpos, filesize,
                               win->get_use_palette());
    if (err != 0)
    {
        oshtml_dbg_printf("jpeg: error %d loading image %s\n",
                          err, url->get_url());
    }
    else
    {
        /* 
         *   successfully loaded - create the jpeg display object using
         *   the data loaded into the helper
         */
        img = new CHtmlSysImageJpeg_win32();
        if ((err = img->load_from_jpeg(jpeg)) != 0)
        {
            /* failure - delete it and log the error */
            delete img;
            img = 0;
            oshtml_dbg_printf("jpeg: error %d preparing image %s\n",
                              err, url->get_url());
        }
    }

    /* we're done with the helper object - delete it */
    delete jpeg;

    /* return the result */
    return img;
}

/* map the palette */
int CHtmlSysImageJpeg_win32::map_palette(CHtmlSysWin *win, int foreground)
{
    return CTadsJpeg::map_palette((CHtmlSysWin_win32 *)win, foreground);
}

/*
 *   Draw the image 
 */
void CHtmlSysImageJpeg_win32::draw_image(CHtmlSysWin *syswin,
                                         CHtmlRect *docpos,
                                         htmlimg_draw_mode_t mode)
{
    CHtmlSysWin_win32 *win = (CHtmlSysWin_win32 *)syswin;
    CHtmlRect winpos = win->doc_to_screen(*docpos);

    /* draw through our CTadsJpeg base class */
    CTadsJpeg::draw(win, &winpos, mode);
}


/* ------------------------------------------------------------------------ */
/*
 *   PNG Image implementation 
 */

/* create a new system-specific PNG display object */
CHtmlSysResource *CHtmlSysImagePng::
   create_png(const CHtmlUrl *url, const textchar_t *filename,
              unsigned long seekpos, unsigned long filesize,
              CHtmlSysWin *win)
{
    CHtmlPng *png;
    CHtmlSysImagePng_win32 *img = 0;
    int err;

    /* load the PNG from the file, using the helper class */
    png = new CHtmlPng();
    err = png->read_png_file(filename, seekpos, filesize,
                             win->get_use_palette());
    if (err != 0)
    {
        oshtml_dbg_printf("png: error %d loading image %s\n",
                          err, url->get_url());
    }
    else
    {
        /* 
         *   successfully loaded - create a PNG display object using the
         *   data we've loaded into the helper object 
         */
        img = new CHtmlSysImagePng_win32();
        if ((err = img->load_from_png(png)) != 0)
        {
            /* failure - delete it and log the error */
            delete img;
            img = 0;
            oshtml_dbg_printf("png: error %d preparing image %s\n",
                              err, url->get_url());
        }
    }

    /* done with the png loader helper object - delete it */
    delete png;

    /* return the image */
    return img;
}

/* map the palette */
int CHtmlSysImagePng_win32::map_palette(CHtmlSysWin *win, int foreground)
{
    return CTadsPng::map_palette((CHtmlSysWin_win32 *)win, foreground);
}

/*
 *   Draw the image 
 */
void CHtmlSysImagePng_win32::draw_image(CHtmlSysWin *syswin,
                                        CHtmlRect *docpos,
                                        htmlimg_draw_mode_t mode)
{
    CHtmlSysWin_win32 *win = (CHtmlSysWin_win32 *)syswin;
    CHtmlRect winpos = win->doc_to_screen(*docpos);

    /* draw through our CTadsPng base class */
    CTadsPng::draw(win, &winpos, mode);
}


/* ------------------------------------------------------------------------ */
/*
 *   MNG Image implementation 
 */

/* create a new system-specific MNG display object */
CHtmlSysResource *CHtmlSysImageMng::
   create_mng(const CHtmlUrl *url, const textchar_t *filename,
              unsigned long seekpos, unsigned long filesize,
              CHtmlSysWin *win)
{
    CHtmlSysImageMng_win32 *img = 0;
    int err;

    /* create our windows MNG display object */
    img = new CHtmlSysImageMng_win32();

    /* ask the display object to get the loading process underway */
    err = img->start_reading_mng(url, filename, seekpos, filesize);
    if (err != 0)
    {
        /* show the problem */
        oshtml_dbg_printf("mng: error %d loading image %s\n",
                          err, url->get_url());

        /* delete and forget the display object we created */
        delete img;
        img = 0;
    }

    /* return the image */
    return img;
}

/*
 *   create 
 */
CHtmlSysImageMng_win32::CHtmlSysImageMng_win32()
{
    /* no display site yet */
    display_site_ = 0;
}

/*
 *   delete 
 */
CHtmlSysImageMng_win32::~CHtmlSysImageMng_win32()
{
}

/*
 *   Receive notification of a timer event 
 */
void CHtmlSysImageMng_win32::notify_timer()
{
    /* notify the helper object of the timer event */
    mng_->notify_timer();
}

/*
 *   Receive notification of an image change from the helper 
 */
void CHtmlSysImageMng_win32::
   notify_image_change(int x, int y, int wid, int ht)
{
    /* there's nothing we need to do here */
}

/*
 *   Start reading the image file 
 */
int CHtmlSysImageMng_win32::start_reading_mng(const CHtmlUrl *url,
                                              const textchar_t *filename,
                                              unsigned long seekpos,
                                              unsigned long filesize)
{
    int err;

    /* try reading the file */
    err = CTadsMng::start_reading_mng(filename, seekpos, filesize);
    if (err != 0)
        oshtml_dbg_printf("mng: error 1-%d reading image %s\n",
                          err, url->get_url());

    /* return the result code */
    return err;
}

/*
 *   Draw the image 
 */
void CHtmlSysImageMng_win32::draw_image(CHtmlSysWin *syswin,
                                        CHtmlRect *docpos,
                                        htmlimg_draw_mode_t mode)
{
    CHtmlSysWin_win32 *win = (CHtmlSysWin_win32 *)syswin;
    CHtmlRect winpos = win->doc_to_screen(*docpos);

    /* draw through our CTadsMng base class */
    CTadsMng::draw(win, &winpos, mode);

    /* 
     *   tell the helper object we've just drawn the object - it might need
     *   to set up an event timer based on this 
     */
    mng_->notify_draw();
}

/*
 *   Set the display site 
 */
void CHtmlSysImageMng_win32::set_display_site(CHtmlSysImageDisplaySite *site)
{
    /* if we have an existing site, cancel any pending timer event */
    if (display_site_ != 0)
        display_site_->dispsite_cancel_timer(this);

    /* remember the new site */
    display_site_ = site;

    /* 
     *   If we are setting a valid new site (as opposed to disengaging from
     *   an existing site), and we have a pending timer event, ask the new
     *   site for notification at our event time.  Now that we've lost our
     *   old site, our new site will never deliver the event to us.  
     */
    if (site != 0 && mng_->is_timer_pending())
    {
        long tcur;

        /* 
         *   if we're already past the scheduled event time, ask for
         *   immediate notification; otherwise, calculate the amount of time
         *   still remaining before the event was originally scheduled 
         */
        tcur = os_get_sys_clock_ms();
        if (tcur >= mng_->get_timer_time())
            site->dispsite_set_timer(0, this);
        else
            site->dispsite_set_timer(mng_->get_timer_time() - tcur, this);
    }
}

/*
 *   Receive notification of a canvas update.  Notify the display site of the
 *   update.  
 */
void CHtmlSysImageMng_win32::notify_mng_update(int x, int y, int wid, int ht)
{
    /* invalidate the display site */
    if (display_site_ != 0)
        display_site_->dispsite_inval(x, y, wid, ht);
}

/*
 *   Set a timer 
 */
void CHtmlSysImageMng_win32::set_mng_timer(long msecs)
{
    /* if we have a display site, ask it to set the timer */
    if (display_site_ != 0)
        display_site_->dispsite_set_timer(msecs, this);
}

/*
 *   Cancel the timer 
 */
void CHtmlSysImageMng_win32::cancel_mng_timer()
{
    /* if we have a display site, ask it to cancel the timer */
    if (display_site_ != 0)
        display_site_->dispsite_cancel_timer(this);
}

/*
 *   Cancel playback 
 */
void CHtmlSysImageMng_win32::cancel_playback()
{
    /* tell the underlying MNG to stop playback */
    mng_->halt_playback();
}

/*
 *   Pause playback 
 */
void CHtmlSysImageMng_win32::pause_playback()
{
    /* tell the underlying MNG to pause playback */
    mng_->pause_playback();
}

/*
 *   Resume from pause 
 */
void CHtmlSysImageMng_win32::resume_playback()
{
    /* tell the underlying MNG to resume playback */
    mng_->resume_playback();
}
