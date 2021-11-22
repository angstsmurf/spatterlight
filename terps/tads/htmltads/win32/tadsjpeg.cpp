#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadsjpeg.cpp,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsjpeg.cpp - Win32 JPEG object
Function
  
Notes
  
Modified
  11/08/97 MJRoberts  - Creation
*/

#include <windows.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSWIN_H
#include "tadswin.h"
#endif
#ifndef HTMLJPEG_H
#include "htmljpeg.h"
#endif
#ifndef TADSJPEG_H
#include "tadsjpeg.h"
#endif


CTadsJpeg::CTadsJpeg()
{
}

CTadsJpeg::~CTadsJpeg()
{
}

/*
 *   load from a JPEG object 
 */
int CTadsJpeg::load_from_jpeg(CHtmlJpeg *jpeg)
{
    /* drop any previously loaded image */
    delete_image();
    
    /* create a DWORD-aligned version of the buffer */
    if (create_pix_dword_aligned(jpeg->get_image_buf(),
                                 jpeg->get_width(), jpeg->get_height(),
                                 TRUE, TRUE))
        return 1;

    /* success */
    return 0;
}


/*
 *   Create a DWORD-aligned pixel buffer given a JPEG image buffer 
 */
int CTadsJpeg::create_pix_dword_aligned(OS_HUGEPTR(unsigned char) src,
                                        long width_pix, long height,
                                        int flip_rgb, int flip_vert)
{
    long out_width_bytes;
    long in_width_bytes;
    long width_bits;
    long new_size;
    long row;
    OS_HUGEPTR(unsigned char) dst;
    long dst_inc;

    /* store the width and height */
    width_ = width_pix;
    height_ = height;

    /* if there's no source buffer, we can't do anything */
    if (src == 0)
        return 1;

    /* calculate the smallest DWORD-aligned buffer we can use */
    width_bits = width_pix * 24;
    out_width_bytes = ((width_bits + 31) / 32) * 4;

    /* calculate the size we'll need for the DWORD-aligned buffer */
    new_size = out_width_bytes * height;

    /* allocate our DIB section */
    if (alloc_dib())
        return 1;

    /* 
     *   Copy the rows.  If we need to vertically flip the rows, start at
     *   the end of our output buffer and work backwards, while working
     *   forwards in the source buffer.  
     */
    in_width_bytes = width_pix * 3;
    if (flip_vert)
    {
        dst = os_add_huge(pix_, (height - 1) * out_width_bytes);
        dst_inc = -out_width_bytes;
    }
    else
    {
        dst = pix_;
        dst_inc = out_width_bytes;
    }
    for (row = 0 ; row < height ; ++row)
    {
        /* copy this row, flipping RGB ordering if necessary */
        if (flip_rgb)
            flip_rgb_order(dst, src, (size_t)width_pix);
        else
            memcpy(dst, src, (size_t)in_width_bytes);

        /* move to the next row */
        src = os_add_huge(src, in_width_bytes);
        dst = os_add_huge(dst, dst_inc);
    }

    /* remember the byte width */
    width_bytes_ = out_width_bytes;

    /* success */
    return 0;
}

/*
 *   Copy a row of an image, swapping the order of the RGB bytes.  The
 *   image is assumed to be a 3-component image with one byte per
 *   component, packed into the buffer for the given width in pixels.  
 */
void CTadsJpeg::flip_rgb_order(OS_HUGEPTR(unsigned char) dst,
                               OS_HUGEPTR(unsigned char) src,
                               size_t width_pix)
{
    for ( ; width_pix > 0 ; --width_pix)
    {
        /* swap and copy the current set of bytes */
        *dst = *os_add_huge(src, 2);
        *os_add_huge(dst, 1) = *os_add_huge(src, 1);
        *os_add_huge(dst, 2) = *src;

        /* move on to the next set */
        src = os_add_huge(src, 3);
        dst = os_add_huge(dst, 3);
    }
}

