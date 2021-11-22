/* $Header: d:/cvsroot/tads/html/win32/tadsjpeg.h,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsjpeg.h - JPEG implementation
Function
  
Notes
  
Modified
  11/08/97 MJRoberts  - Creation
*/

#ifndef TADSJPEG_H
#define TADSJPEG_H

#include <stdlib.h>

#ifndef HTML_OS_H
#include "html_os.h"
#endif
#ifndef TADSIMG_H
#include "tadsimg.h"
#endif


class CTadsJpeg: public CTadsImage
{
public:
    CTadsJpeg();
    virtual ~CTadsJpeg();

    /* load from a JPEG object; returns zero on success, nonzero on failure */
    int load_from_jpeg(class CHtmlJpeg *jpeg);
    
protected:
    /* 
     *   Create the pixel map, dword aligned.  We always use 3-byte RGB
     *   components.  If flip_rgb is set, we'll flip the byte ordering
     *   RGB->BGR.  Returns 0 on success, in which case pix_ will be set to
     *   the pixel buffer suitably aligned and ordered for use as a windows
     *   bitmap; returns non-zero on failure.  If flip_vert is true, we'll
     *   invert the image vertically.  
     */
    int create_pix_dword_aligned(OS_HUGEPTR(unsigned char) src,
                                 long width_pix, long height,
                                 int flip_rgb, int flip_vert);

    /* copy a row of an image, swapping RGB for BGR component ordering */
    void flip_rgb_order(OS_HUGEPTR(unsigned char) dst,
                        OS_HUGEPTR(unsigned char) src,
                        size_t width_pix);
};


#endif /* TADSJPEG_H */
