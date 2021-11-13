/* $Header: d:/cvsroot/tads/html/win32/w32img.h,v 1.2 1999/05/17 02:52:26 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32img.h - tads html win32 image implementation
Function
  
Notes
  
Modified
  01/31/98 MJRoberts  - Creation
*/

#ifndef W32IMG_H
#define W32IMG_H

#ifndef TADSJPEG_H
#include "tadsjpeg.h"
#endif
#ifndef TADSPNG_H
#include "tadspng.h"
#endif
#ifndef TADSMNG_H
#include "tadsmng.h"
#endif
#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   JPEG Image implementation 
 */
class CHtmlSysImageJpeg_win32: public CTadsJpeg, public CHtmlSysImageJpeg
{
public:
    /*
     *   CHtmlSysImageJpeg implementation 
     */

    /* load a JPEG image */
    int load_from_jpeg(class CHtmlJpeg *jpeg)
    {
        return CTadsJpeg::load_from_jpeg(jpeg);
    }

    /* display the image */
    void draw_image(class CHtmlSysWin *win, class CHtmlRect *pos,
                    htmlimg_draw_mode_t mode);

    /* map the palette */
    int map_palette(class CHtmlSysWin *win, int foreground);

    /* get the dimensions */
    unsigned long get_width() const { return width_; }
    unsigned long get_height() const { return height_; }
};

/*
 *   PNG Image implementation 
 */
class CHtmlSysImagePng_win32: public CTadsPng, public CHtmlSysImagePng
{
public:
    /*
     *   CHtmlSysImagePng implementation 
     */

    /* load a PNG image */
    int load_from_png(class CHtmlPng *png)
    {
        return CTadsPng::load_from_png(png);
    }

    /* display the image */
    void draw_image(class CHtmlSysWin *win, class CHtmlRect *pos,
                    htmlimg_draw_mode_t mode);

    /* map the palette */
    int map_palette(class CHtmlSysWin *win, int foreground);

    /* get the dimensions */
    unsigned long get_width() const { return width_; }
    unsigned long get_height() const { return height_; }
};

/*
 *   MNG Image implementation 
 */
class CHtmlSysImageMng_win32: public CTadsMng, public CHtmlSysImageMng
{
public:
    /*
     *   CHtmlSysImageMng implementation 
     */
    CHtmlSysImageMng_win32();
    ~CHtmlSysImageMng_win32();

    /* cancel playback */
    virtual void cancel_playback();

    /* pause/resume playback */
    virtual void pause_playback();
    virtual void resume_playback();

    /* set the display site */
    virtual void set_display_site(CHtmlSysImageDisplaySite *site);

    /* receive notification of a timer event from our display site */
    virtual void notify_timer();

    /* receive notification of an image change from the helper */
    virtual void notify_image_change(int x, int y, int wid, int ht);

    /* display the image */
    void draw_image(class CHtmlSysWin *win, class CHtmlRect *pos,
                    htmlimg_draw_mode_t mode);

    /* start reading the MNG file */
    int start_reading_mng(const CHtmlUrl *url, const textchar_t *filename,
                          unsigned long seekpos, unsigned long filesize);

    /* map the palette */
    int map_palette(class CHtmlSysWin *, int foreground) { return FALSE; }

    /* get the dimensions */
    unsigned long get_width() const { return width_; }
    unsigned long get_height() const { return height_; }

    /* -------------------------------------------------------------------- */
    /*
     *   CHtmlMngClient interface completion 
     */

    /* receive notification of a canvas update */
    virtual void notify_mng_update(int x, int y, int wid, int ht);

    /* set a timer */
    virtual void set_mng_timer(long msecs);

    /* cancel the timer */
    virtual void cancel_mng_timer();

protected:
    /* our display site */
    class CHtmlSysImageDisplaySite *display_site_;
};

#endif /* W32IMG_H */
