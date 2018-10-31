#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadsimg.cpp,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsimg.cpp - TADS image base class
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
#ifndef TADSIMG_H
#include "tadsimg.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif

/* some versions of the win sdk don't have this defined yet */
#ifndef AC_SRC_ALPHA
#define AC_SRC_ALPHA 1                                     /* from wingdi.h */
#endif


/*
 *   statics 
 */
BOOL (WINAPI *CTadsImage::alphablend_proc_)
    (HDC, int, int, int, int, HDC, int, int, int, int, BLENDFUNCTION) = 0;
int CTadsImage::linked_alphablend_proc_ = FALSE;

/*
 *   create 
 */
CTadsImage::CTadsImage()
{
    /* no pixel buffer yet */
    pix_ = 0;

    /* no mask buffer yet */
    mask_ = 0;

    /* no DIB section yet */
    dibsect_ = 0;

    /* no dimensions yet */
    width_ = 0;
    height_ = 0;

    /* presume we'll use the current display's bits per pixel */
    bpp_ = 0;

    /* presume we won't have alpha information */
    has_alpha_ = FALSE;
}

/*
 *   destroy 
 */
CTadsImage::~CTadsImage()
{
    /* delete any existing image */
    delete_image();
}

/*
 *   delete the image 
 */
void CTadsImage::delete_image()
{
    /* if we have a DIB section, delete it */
    if (dibsect_ != 0)
        DeleteObject(dibsect_);

    /* if we have a mask buffer, delete it */
    if (mask_ != 0)
        os_free_huge(mask_);

    /* clear members */
    dibsect_ = 0;
    pix_ = 0;
    width_ = 0;
    height_ = 0;
}

/*
 *   Draw the image 
 */
void CTadsImage::draw(CTadsWin *win, CHtmlRect *pos,
                      htmlimg_draw_mode_t mode)
{
    HDC hdc;
    long ht, wid;
    int blt_mode;
    HDC src_dc = 0;
    BLENDFUNCTION bf;
    BOOL (WINAPI *alphaproc)
        (HDC, int, int, int, int, HDC, int, int, int, int, BLENDFUNCTION);

    /* don't do anything if not drawing */
    if ((hdc = win->gethdc()) == 0)
        return;

    /* 
     *   get the AlphaBlend address in case we need it; don't bother with it
     *   if we have a mask, since we'll just render with the mask instead 
     */
    alphaproc = (mask_ == 0 ? get_alphablend_proc() : 0);

    /* create a source DC, and select the DIB section into it */
    src_dc = CreateCompatibleDC(hdc);
    SelectObject(src_dc, dibsect_);

    /* set up the BLENDFUNCTION if we're using alpha blending */
    if (alphaproc != 0 && has_alpha_)
    {
        /* 
         *   set up to copy from the source image; we don't use constant
         *   alpha, so set the constant alpha to max (0xff) 
         */
        bf.BlendOp = AC_SRC_OVER;
        bf.BlendFlags = 0;
        bf.SourceConstantAlpha = 0xff;
        bf.AlphaFormat = AC_SRC_ALPHA;
    }

    /* presume we'll draw in 'source copy' mode */
    blt_mode = SRCCOPY;

    /* if we have a mask, draw it first */
    if (mask_ != 0)
    {
        /* 
         *   AND the mask to set background pixels to black wherever we're
         *   drawing opaque image pixels 
         */
        draw_mask(hdc, win, pos, mode);

        /* set the drawing mode to OR for the image */
        blt_mode = SRCPAINT;
    }

    /* figure out how we're going to draw the image into the given area */
    switch(mode)
    {
    case HTMLIMG_DRAW_CLIP:
        /* 
         *   draw at exact size, aligning at upper left of destination
         *   area, and clipping if it's too big 
         */
        wid = width_ < (unsigned long)(pos->right - pos->left)
              ? width_ : pos->right - pos->left;
        ht = height_ < (unsigned long)(pos->bottom - pos->top)
             ? height_ : pos->bottom - pos->top;

        /* display the bitmap in the appropriate mode */
        if (has_alpha_ && alphaproc != 0)
            (*alphaproc)(hdc, pos->left, pos->top, wid, ht,
                         src_dc, 0, 0, wid, ht, bf);
        else
            StretchBlt(hdc, pos->left, pos->top, wid, ht,
                       src_dc, 0, 0, wid, ht, blt_mode);
        break;

    case HTMLIMG_DRAW_STRETCH:
        /*
         *   stretch the image to fill the given area 
         */
        if (has_alpha_ && alphaproc != 0)
            (*alphaproc)(hdc, pos->left, pos->top,
                         pos->right - pos->left, pos->bottom - pos->top,
                         src_dc, 0, 0, width_, height_, bf);
        else
            StretchBlt(hdc, pos->left, pos->top,
                       pos->right - pos->left, pos->bottom - pos->top,
                       src_dc, 0, 0, width_, height_, blt_mode);
        break;

    case HTMLIMG_DRAW_TILE:
        /*
         *   draw at the exact size as many times as necessary to tile the
         *   area 
         */
        {
            long curx, cury;

            for (curx = pos->left ; curx < pos->right ; curx += width_)
            {
                for (cury = pos->top ; cury < pos->bottom ; cury += height_)
                {
                    /* 
                     *   draw the image at the current tile position,
                     *   clipping if necessary 
                     */
                    wid = width_ < (unsigned long)(pos->right - curx)
                          ? width_ : pos->right - curx;
                    ht = height_ < (unsigned long)(pos->bottom - cury)
                         ? height_ : pos->bottom - cury;

                    /* display the bitmap */
                    if (has_alpha_ && alphaproc != 0)
                        (*alphaproc)(hdc, curx, cury, wid, ht,
                                     src_dc, 0, 0, wid, ht, bf);
                    else
                        StretchBlt(hdc, curx, cury, wid, ht,
                                   src_dc, 0, 0, wid, ht, blt_mode);
                }
            }
        }
        break;
    }

    /* delete our source DC */
    DeleteDC(src_dc);
}

/*
 *   Draw the mask
 */
void CTadsImage::draw_mask(HDC hdc, CTadsWin *win, CHtmlRect *pos,
                           htmlimg_draw_mode_t mode)
{
    struct
    {
        BITMAPINFO bmi;
        RGBQUAD clr;
    }
    bmi;
    long ht, wid;

    /* fill in the bitmap info header */
    bmi.bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmi.bmiHeader.biWidth = width_;
    bmi.bmi.bmiHeader.biHeight = height_;
    bmi.bmi.bmiHeader.biPlanes = 1;
    bmi.bmi.bmiHeader.biCompression = 0;
    bmi.bmi.bmiHeader.biSizeImage = 0;
    bmi.bmi.bmiHeader.biXPelsPerMeter = 0;
    bmi.bmi.bmiHeader.biYPelsPerMeter = 0;
    bmi.bmi.bmiHeader.biClrUsed = 2;
    bmi.bmi.bmiHeader.biClrImportant = 2;
    bmi.bmi.bmiHeader.biBitCount = 1;

    /* fill in the two-color palette - just black and white */
    bmi.bmi.bmiColors[0].rgbBlue = 0x00;
    bmi.bmi.bmiColors[0].rgbGreen = 0x00;
    bmi.bmi.bmiColors[0].rgbRed = 0x00;

    bmi.bmi.bmiColors[1].rgbBlue = 0xff;
    bmi.bmi.bmiColors[1].rgbGreen = 0xff;
    bmi.bmi.bmiColors[1].rgbRed = 0xff;

    /* figure out how we're going to draw the image into the given area */
    switch(mode)
    {
    case HTMLIMG_DRAW_CLIP:
        /* 
         *   draw at exact size. aligning at upper left of destination
         *   area, and clipping if it's too big 
         */
        wid = width_ < (unsigned long)(pos->right - pos->left)
              ? width_ : pos->right - pos->left;
        ht = height_ < (unsigned long)(pos->bottom - pos->top)
             ? height_ : pos->bottom - pos->top;

        /* display the bitmap */
        StretchDIBits(hdc,
                      pos->left, pos->top, wid, ht,
                      0, 0, wid, ht,
                      mask_,
                      (LPBITMAPINFO)&bmi.bmi.bmiHeader,
                      DIB_RGB_COLORS,
                      SRCAND);
        break;

    case HTMLIMG_DRAW_STRETCH:
        /*
         *   stretch the image to fill the given area 
         */
        StretchDIBits(hdc, pos->left, pos->top,
                      pos->right - pos->left, pos->bottom - pos->top,
                      0, 0, width_, height_,
                      mask_,
                      (LPBITMAPINFO)&bmi.bmi.bmiHeader,
                      DIB_RGB_COLORS,
                      SRCAND);
        break;

    case HTMLIMG_DRAW_TILE:
        /*
         *   draw at the exact size as many times as necessary to tile the
         *   area 
         */
        {
            long curx, cury;

            for (curx = pos->left ; curx < pos->right ; curx += width_)
            {
                for (cury = pos->top ; cury < pos->bottom ; cury += height_)
                {
                    /* 
                     *   draw the image at the current tile position,
                     *   clipping if necessary 
                     */
                    wid = width_ < (unsigned long)(pos->right - curx)
                          ? width_ : pos->right - curx;
                    ht = height_ < (unsigned long)(pos->bottom - cury)
                         ? height_ : pos->bottom - cury;

                    /* display the bitmap */
                    StretchDIBits(hdc,
                                  curx, cury, wid, ht,
                                  0, 0, wid, ht,
                                  mask_,
                                  (LPBITMAPINFO)&bmi.bmi.bmiHeader,
                                  DIB_RGB_COLORS,
                                  SRCAND);
                }
            }
        }
        break;
    }
}

/*
 *   Create a DWORD-aligned pixel buffer from an array of rows.  Each row is
 *   width_bytes bytes wide and width_pix pixels wide, which implies the
 *   number of bytes per pixel.  src_rows points to an array of pointers,
 *   where each pointer points to an array of bytes for a row.  
 */
int CTadsImage::create_pix_dword_aligned(const unsigned char *const *src_rows,
                                         unsigned long width_pix,
                                         unsigned long width_bytes,
                                         unsigned long height,
                                         int *pix_bytes_per_pixel)
{
    long out_width_bytes;
    long in_width_bytes;
    long width_bits;
    long new_size;
    unsigned long row;
    OS_HUGEPTR(unsigned char) dst;
    const unsigned char *const *src;
    int in_bytes_per_pixel;
    int out_bytes_per_pixel;

    /* store the width and height */
    width_ = width_pix;
    height_ = height;

    /* if there are no rows, we can't do anything */
    if (src_rows == 0)
        return 1;

    /* the width in bytes of the input is the same as the in-memory image */
    in_width_bytes = width_bytes;

    /* figure the number of bytes per pixel of the input */
    in_bytes_per_pixel = in_width_bytes / width_pix;

    /* 
     *   keep the alpha channel in the output if (a) we have alpha in the
     *   input (indicated by 32 bits == 4 bytes per pixel), and (b) we have
     *   support on this version of Windows for the AlphaBlend API 
     */
    has_alpha_ = (in_bytes_per_pixel >= 4 && get_alphablend_proc() != 0);

    /* 
     *   Figure the bytes per pixel of the output:
     *   
     *   - if we have an alpha channel and 4 or more bytes per input pixel,
     *   use 4 bytes per output pixel (RGBA format)
     *   
     *   - if we have no alpha channel, use the same number of bytes per
     *   pixel as the input, but no more than 3 (for RGB format) 
     */
    out_bytes_per_pixel = in_bytes_per_pixel;
    if (has_alpha_ && out_bytes_per_pixel > 4)
        out_bytes_per_pixel = 4;
    else if (!has_alpha_ && out_bytes_per_pixel > 3)
        out_bytes_per_pixel = 3;

    /* tell the caller the bytes per pixel in the output image */
    *pix_bytes_per_pixel = out_bytes_per_pixel;

    /* remember the bit depth of the bitmap data we're storing */
    bpp_ = out_bytes_per_pixel * 8;

    /* calculate the smallest DWORD-aligned buffer we can use */
    width_bits = width_pix * out_bytes_per_pixel * 8;
    out_width_bytes = ((width_bits + 31) / 32) * 4;

    /* calculate the size we'll need for the DWORD-aligned buffer */
    new_size = (unsigned long)out_width_bytes * (unsigned long)height;

    /* allocate our DIB section, where we'll stash our pixel data */
    if (alloc_dib())
        return 1;

    /* copy the rows */
    dst = os_add_huge(pix_, out_width_bytes * (height - 1));
    if (in_bytes_per_pixel == out_bytes_per_pixel)
    {
        /* check for 32-bit-per-pixel with alpha */
        if (has_alpha_ && in_bytes_per_pixel == 4)
        {
            /* 
             *   We have a 32bpp image on both input and output, but we have
             *   an alpha channel that we're going to use.  For Windows
             *   rendering, we have to pre-multiply the source pixels by the
             *   alpha channel value.  
             */
            for (src = src_rows, row = 0 ; row < height ; ++row, ++src)
            {
                unsigned long i;
                const unsigned char *srcp;
                unsigned char *dstp;

                /* copy each 32-bit pixel */
                for (i = 0, srcp = (const unsigned char *)*src, dstp = dst ;
                     i < width_pix ; ++i)
                {
                    /* get this pixel's source alpha value (0-255 range) */
                    unsigned int alpha = (unsigned char)*(srcp + 3);

                    /* scale each component by the source alpha */
                    *dstp++ = (unsigned char)(((int)(*srcp++) * alpha)/255);
                    *dstp++ = (unsigned char)(((int)(*srcp++) * alpha)/255);
                    *dstp++ = (unsigned char)(((int)(*srcp++) * alpha)/255);

                    /* copy the alpha */
                    *dstp++ = *srcp++;
                }

                /* move to next row of output */
                dst = os_add_huge(dst, -out_width_bytes);
            }
        }
        else
        {
            /* 
             *   the pixels are the same size, and we have no alpha, so we
             *   can simply copy a row at a time 
             */
            for (src = src_rows, row = 0 ; row < height ; ++row, ++src)
            {
                /* copy this row */
                memcpy(dst, *src, in_bytes_per_pixel * width_pix);

                /* move to next row of output */
                dst = os_add_huge(dst, -out_width_bytes);
            }
        }
    }
    else
    {
        /* 
         *   we need to adjust the size of each pixel, so copy only one pixel
         *   at a time 
         */
        for (src = src_rows, row = 0 ; row < height ; ++row, ++src)
        {
            unsigned long i;
            const unsigned char *srcp;
            unsigned char *dstp;

            /* copy this row */
            for (i = 0, srcp = (const unsigned char *)*src, dstp = dst ;
                 i < width_pix ;
                 ++i, srcp += in_bytes_per_pixel, dstp += out_bytes_per_pixel)
            {
                /* copy this pixel */
                memcpy(dstp, srcp, out_bytes_per_pixel);
            }
            
            /* move to next row */
            dst = os_add_huge(dst, -out_width_bytes);
        }
    }

    /* remember the byte width */
    width_bytes_ = out_width_bytes;

    /* success */
    return 0;
}

/*
 *   Allocate our DIB section.  This creates the DIB section object and
 *   allocates the memory for our pixels (storing a pointer to the pixel
 *   memory in pix_).  
 */
int CTadsImage::alloc_dib()
{
    BITMAPINFO bmi;
    HDC deskdc;

    /* fill in the bitmap info header */
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width_;
    bmi.bmiHeader.biHeight = height_;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage = 0;
    bmi.bmiHeader.biXPelsPerMeter = 0;
    bmi.bmiHeader.biYPelsPerMeter = 0;
    bmi.bmiHeader.biClrUsed = 0;
    bmi.bmiHeader.biClrImportant = 0;

    /* 
     *   set the bits per pixel to the setting stored in the image - if bpp_
     *   is zero, though, assume we're using 24-bit RGB 
     */
    bmi.bmiHeader.biBitCount = (WORD)(bpp_ != 0 ? bpp_ : 24);

    /* get the desktop DC */
    deskdc = GetDC(GetDesktopWindow());

    /* create the DIB section */
    dibsect_ = CreateDIBSection(deskdc, &bmi, DIB_RGB_COLORS,
                                (void **)&pix_, 0, 0);

    /* done with the desktop DC */
    ReleaseDC(GetDesktopWindow(), deskdc);

    /* indicate failure if the DIB handle is null */
    return (dibsect_ == 0);
}

/*
 *   Get the address of the Win32 API function AlphaBlend(), if it's
 *   available.  If we haven't dynamically linked to the routine yet, we'll
 *   do so now.  Returns null if the procedure isn't available.  
 */
BOOL (WINAPI *CTadsImage::get_alphablend_proc())
    (HDC, int, int, int, int, HDC, int, int, int, int, BLENDFUNCTION)
{
    HMODULE dll;

    /* 
     *   if we've already linked to the routine (or tried), return the
     *   address we found previously 
     */
    if (linked_alphablend_proc_)
        return alphablend_proc_;

    /* note that we've now linked to the routine (or at least tried) */
    linked_alphablend_proc_ = TRUE;

    /*
     *   Check to see if this is Windows 98.  If this is Win98, the
     *   AlphaBlend function will be available, but will be unusable because
     *   it's too buggy.  (In particular, the Win98 AlphaBlend routine is
     *   unable to accept negative destination coordinates, and completely
     *   ignores source coordinates.  This makes it impossible to perform
     *   necessary translations during drawing; for example, it makes it
     *   impossible to draw an image whose top is slightly above the top of
     *   the window due to scrolling of the page.)
     *   
     *   Since we can't use AlphaBlend on Win98, note the OS version, and if
     *   it is indeed Win98, don't even bother looking to see if AlphaBlend
     *   is present, and simply indicate that alpha blending is not
     *   available.  
     */
    if (CTadsApp::get_app()->is_win98())
    {
        /* we're on Win98 - do not use AlphaBlend */
        alphablend_proc_ = 0;
        return 0;
    }

    /* load the DLL containing AlphaBlend */
    dll = LoadLibrary("Msimg32.dll");

    /* if we found the library, try getting the AlphaBlend address */
    if (dll != 0)
        alphablend_proc_ = (BOOL (WINAPI *)
                            (HDC, int, int, int, int,
                             HDC, int, int, int, int, BLENDFUNCTION))
                           GetProcAddress(dll, "AlphaBlend");

    /* return the AlphaBlend address, if we got it */
    return alphablend_proc_;
}

