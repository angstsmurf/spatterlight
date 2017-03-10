/* 
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsmng.h - Win32 MNG object
Function
  
Notes
  
Modified
  05/04/02 MJRoberts  - Creation
*/

#ifndef TADSMNG_H
#define TADSMNG_H

#include <stdlib.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTML_OS_H
#include "html_os.h"
#endif
#ifndef TADSIMG_H
#include "tadsimg.h"
#endif
#ifndef HTMLMNG_H
#include "htmlmng.h"
#endif


/*
 *   Win32 MNG display object.  We use the generic CHtmlMng helper object, so
 *   we need to provide the CHtmlMng client interface.  
 */
class CTadsMng: public CTadsImage, public CHtmlMngClient
{
public:
    CTadsMng();
    ~CTadsMng();

    /* start reading from an MNG helper object */
    int start_reading_mng(const textchar_t *filename,
                          unsigned long seekpos, unsigned long filesize);

    /* 
     *   initialize alpha support - check to see if this version of Windows
     *   supports alpha blending, and set MNG read options accordingly 
     */
    static void init_alpha_support();


    /* -------------------------------------------------------------------- */
    /*
     *   Partial CHtmlMngClient implementation.  Subclasses must finish
     *   implementing this interface to provide the conduit back to the
     *   display site.  We don't want to make any assumptions about the
     *   nature of the display site, so we leave this interface partially
     *   unimplemented.  
     */

    /* initialize the canvas */
    int init_mng_canvas(mng_handle handle, int width, int height);

    /* get a pointer to a line of canvas pixels */
    char *get_mng_canvas_row(int rownum);

    /* receive notification of a canvas update */
    virtual void notify_mng_update(int x, int y, int wid, int ht) = 0;

    /* set a timer */
    virtual void set_mng_timer(long msecs) = 0;

    /* cancel the timer */
    virtual void cancel_mng_timer() = 0;

protected:
    /* 
     *   our MNG loader - we hang onto this throughout our existence, since
     *   we need to continue loading incrementally as the animation proceeds 
     */
    class CHtmlMng *mng_;
};


#endif /* TADSMNG_H */

