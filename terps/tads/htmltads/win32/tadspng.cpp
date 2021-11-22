#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadspng.cpp,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadspng.cpp - Win32 PNG object implementation
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
#ifndef HTMLPNG_H
#include "htmlpng.h"
#endif
#ifndef TADSPNG_H
#include "tadspng.h"
#endif


CTadsPng::CTadsPng()
{
}

CTadsPng::~CTadsPng()
{
}

/*
 *   Load the iamge from a PNG object 
 */
int CTadsPng::load_from_png(CHtmlPng *png)
{
    int pix_bytes_per_pixel;
    
    /* drop any previously loaded image */
    delete_image();

    /* create a DWORD-aligned pixel buffer from the PNG rows */
    if (create_pix_dword_aligned(png->get_rows(),
                                 png->get_width(),
                                 png->get_row_bytes(),
                                 png->get_height(),
                                 &pix_bytes_per_pixel))
        return 1;

    /*
     *   If there's alpha information, and alpha blending isn't supported on
     *   this version of Windows, create the mask.  We don't need to create a
     *   mask if we have alpha blending available, because we'll use the full
     *   alpha information to render the transparency instead.  However, even
     *   if we have alpha rendering available, still use a simple mask if the
     *   transparency is simple.
     *   
     *   So: check to see if we have a 32bpp image or we have a separate
     *   alpha mask; in either case, we have alpha information.  If we have
     *   alpha, then create a mask if we lack alpha blending support in the
     *   OS, or we don't need full alpha because the transparency is simple
     *   on/off only.  
     */
    if ((png->get_bits_per_pixel() == 32 || png->get_alpha() != 0)
        && (get_alphablend_proc() == 0 || png->is_simple_trans()))
    {
        /* create the DWORD-aligned pixel buffer for the mask */
        if (create_mask(png, pix_bytes_per_pixel))
            return 1;
    }

    /* success */
    return 0;
}

/*
 *   Create the mask, if we have transparency information 
 */
int CTadsPng::create_mask(CHtmlPng *png, int pix_bytes_per_pixel)
{
    long out_width_bytes;
    long pix_width_bits;
    long pix_width_bytes;
    long width_bits;
    long new_size;
    unsigned long row;
    OS_HUGEPTR(unsigned char) dst;
    OS_HUGEPTR(unsigned char) pix;
    const png_byte *const *src;
    size_t srclen;
    int black_idx;

    /* 
     *   if this is a paletted (8-bit) image, find the 'black' color in
     *   the palette 
     */
    black_idx = 0;
    if (pix_bytes_per_pixel == 1 && png->get_alpha() == 0)
    {
        size_t i;
        HTML_color_t *pal;
        
        /* search the palette for black */
        for (i = 0, pal = png->get_palette() ;
             i < png->get_palette_size() ; ++i, ++pal)
        {
            if (*pal == HTML_make_color(0, 0, 0))
            {
                /* this is the one */
                black_idx = i;
                break;
            }
        }
    }

    /* 
     *   calculate the smallest DWORD-aligned buffer we can use - we only
     *   need to store a monochrome bitmap, so we need one byte per 8
     *   pixels 
     */
    width_bits = width_;
    out_width_bytes = ((width_bits + 31) / 32) * 4;

    /* calculate the width of each row of the pix_ array */
    pix_width_bits = pix_bytes_per_pixel * 8 * width_;
    pix_width_bytes = ((pix_width_bits + 31) / 32) * 4;

    /* calculate the size we'll need for the DWORD-aligned buffer */
    new_size = (unsigned long)out_width_bytes * (unsigned long)height_;

    /* allocate our buffer */
    mask_ = (OS_HUGEPTR(unsigned char))os_alloc_huge(new_size);
    if (mask_ == 0)
        return 1;

    /* read either from the separate alpha channel or the mixed-in alpha */
    if (png->get_alpha() != 0)
    {
        /* read from the separate one-byte-per-pixel alpha channel */
        src = png->get_alpha();
        srclen = 1;
    }
    else
    {
        /* read from the alpha channel mixed into the RGBA data */
        src = png->get_rows();
        srclen = 4;
    }

    /* 
     *   initialize the entire mask to 1 bits - we want to leave the mask
     *   as 1 wherever the alpha channel indicates transparency, since we
     *   AND the mask with the background to leave only the parts of the
     *   background where the image is transparent 
     */
    for (row = 0, dst = mask_ ; row < height_ ;
         ++row, dst = os_add_huge(dst, out_width_bytes))
    {
        /* set this row to all 1's */
        memset(dst, 0xff, out_width_bytes);
    }

    /* 
     *   set up a pointer to the image, so we can clear the transparent
     *   pixels to black 
     */
    pix = os_add_huge(pix_, pix_width_bytes * (height_ - 1));

    /* copy the rows */
    dst = os_add_huge(mask_, out_width_bytes * (height_ - 1));
    for (row = 0 ; row < height_ ; ++row, ++src)
    {
        unsigned long i;
        const png_byte *srcp;
        unsigned char *pixp;
        unsigned char *dstp;
        int bitpos;

        /* 
         *   start out at the first alpha byte in this row - if we're
         *   reading from RGBA data, note that the first A byte is at
         *   offset 3 in the row (after the R, G, and B bytes) 
         */
        srcp = *src + (srclen - 1);
        
        /* copy this row */
        for (i = 0, dstp = dst, pixp = pix, bitpos = 0 ; i < width_ ;
             ++i, srcp += srclen, pixp += pix_bytes_per_pixel)
        {
            /* 
             *   If this pixel is not transparent, clear this bit of the mask
             *   - this will ensure that we clear the background for each
             *   non-transparent pixel of the image.
             *   
             *   Since we're not doing alpha blending but simply creating a
             *   simple transparency mask, consider anything less than fully
             *   opaque to be transparent.  
             */
            if (*srcp == 0xff)
            {
                /* fully opaque - clear the mask here */
                *dstp &= ~(1 << (7 - bitpos));
            }
            else
            {
                /* non-opaque == transparent - clear the image here */
                memset(pixp, black_idx, pix_bytes_per_pixel);
            }

            /* advance to the next bit position */
            ++bitpos;

            /* if bitpos has reached 8, move to the next byte */
            dstp += ((bitpos & 0x08) >> 3);

            /* when bitpos reaches 8, wrap to 0 */
            bitpos &= 0x07;
        }
        
        /* move to next row of the mask */
        dst = os_add_huge(dst, -out_width_bytes);

        /* move to the next row of the picture */
        pix = os_add_huge(pix, -pix_width_bytes);
    }

    /* success */
    return 0;
}

/*
 *   Initialize alpha channel support 
 */
void CTadsPng::init_alpha_support()
{
    /* 
     *   check for availability of the AlphaBlend function - if it's
     *   available, we can support alpha blending, otherwise we cannot 
     */
    if (get_alphablend_proc() == 0)
    {
        /* 
         *   Alpha blending isn't available on this version of Windows.  Tell
         *   the PNG loader to discard alpha channel information by blending
         *   the alpha explicitly during loading into a default background.  
         */
        CHtmlPng::set_options(CHtmlPng::get_options() | HTMLPNG_OPT_NO_ALPHA);
    }
}

