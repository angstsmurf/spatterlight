/* $Header: d:/cvsroot/tads/html/win32/tadsimg.h,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsimg.h - TADS image base class
Function
  This class serves as a base class for images stored as bitmaps.
  Subclasses are responsible for loading a specific type of image
  into our internal bitmap.
Notes
  
Modified
  11/08/97 MJRoberts  - Creation
*/

#ifndef TADSIMG_H
#define TADSIMG_H

#include <windows.h>

#ifndef HTML_OS_H
#include "html_os.h"
#endif
#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif

/*
 *   base image class 
 */
class CTadsImage
{
public:
    CTadsImage();
    virtual ~CTadsImage();

    /* draw the image into a window */
    void draw(class CTadsWin *win, class CHtmlRect *pos,
              htmlimg_draw_mode_t mode);

    /* 
     *   Map the image's color palette into the system palette.  Returns
     *   zero if there was no change, non-zero if any colors changed.  
     *   
     *   We never use palettes in the image objects in the Windows
     *   implementation, because we do all of our drawing work in 24-bit RGB
     *   mode.  
     */
    int map_palette(class CTadsWin *win, int foreground) { return FALSE; }

    /* do we have alpha support? */
    static int is_alpha_supported()
    {
        /* alpha is supported if the AlphaBlend API is avialable */
        return get_alphablend_proc() != 0;
    }

    /*
     *   Disable alpha support.  This can be used to prevent alpha blending
     *   from being enabled even if the platform supports it.  This can be
     *   useful if alpha blending is too slow for the local hardware, or
     *   (more likely) if the local implementation is buggy, as it appears to
     *   be in WineX as of April 2003. 
     */
    static void disable_alpha_support()
    {
        /* forget any AlphaBlend API we've found */
        alphablend_proc_ = 0;

        /* do not attempt to link to the AlphaBlend API */
        linked_alphablend_proc_ = TRUE;
    }

protected:
    /* create the DWORD-aligned version of the image data */
    int create_pix_dword_aligned(const unsigned char *const *src_rows,
                                 unsigned long width_pix,
                                 unsigned long width_bytes,
                                 unsigned long height,
                                 int *pix_bytes_per_pixel);

    /* allocate the DIB section object */
    int alloc_dib();

    /* get the AlphaBlend function, if available */
    static BOOL (WINAPI *get_alphablend_proc())
        (HDC, int, int, int, int, HDC, int, int, int, int, BLENDFUNCTION);

    /* AND the mask onto the screen */
    void draw_mask(HDC hdc, class CTadsWin *win, class CHtmlRect *pos,
                   htmlimg_draw_mode_t mode);
    
    /* delete any existing image */
    void delete_image();

    /* pixel buffer */
    OS_HUGEPTR(unsigned char) pix_;

    /* pixel buffer for the transparency mask, if present */
    OS_HUGEPTR(unsigned char) mask_;

    /* DIB section containing the source image */
    HBITMAP dibsect_;

    /* flag: we have alpha information in the bitmap */
    int has_alpha_;

    /* size of the image */
    unsigned long width_, height_;

    /* 
     *   width of each line of the image in bytes - since Windows bitmaps
     *   must be DWORD-aligned, this may differ from the pixel width 
     */
    unsigned long width_bytes_;

    /* 
     *   bits per pixel - if this is zero, we assume that the stored image
     *   has been converted to the same bit depth as the active display 
     */
    int bpp_;

    /*
     *   Address of dynamically-linked win32 API function AlphaBlend().  We
     *   link to this function at run-time so that we can run on Windows
     *   versions (95, NT4) that don't provide the function - on those
     *   platforms, we'll fail to find the function and fall back to simple
     *   transparency without alpha blending.  
     */
    static BOOL (WINAPI *alphablend_proc_)
        (HDC, int, int, int, int, HDC, int, int, int, int, BLENDFUNCTION);

    /* flag: we've attempted to link to AlphaBlend() */
    static int linked_alphablend_proc_;
};

#endif /* TADSIMG_H */

