/* $Header: d:/cvsroot/tads/html/htmlpng.h,v 1.2 1999/05/17 02:52:21 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlpng.h - PNG image object
Function
  
Notes
  
Modified
  11/08/97 MJRoberts  - Creation
*/

#ifndef HTMLPNG_H
#define HTMLPNG_H

/* include PNG library header */
#include <png.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif

/*
 *   Options flags 
 */

/* 
 *   Convert transparency to alpha information.  If this flag is set, we'll
 *   generate alpha information (either by expanding to 32 bit-per-pixel RGBA
 *   format, or by keeping a separate alpha mask for paletted images) for any
 *   image with a transparency mask.  If this flag isn't set, we'll create a
 *   background for any image with transparency.  
 */
#define HTMLPNG_OPT_TRANS_TO_ALPHA  0x0001

/* 
 *   always use 24-bit RGB for the in-memory representation, regardless of
 *   video mode 
 */
#define HTMLPNG_OPT_RGB24           0x0002

/* 
 *   Always discard alpha information.  If this is set, if an image has an
 *   alpha channel, we'll explicitly create a default background for the
 *   image and blend the image into the default background. 
 */
#define HTMLPNG_OPT_NO_ALPHA        0x0004


class CHtmlPng
{
public:
    CHtmlPng();
    ~CHtmlPng();

    /* load an image from a file */
    int read_png_file(const textchar_t *filename,
                      unsigned long seekpos, unsigned long filesize,
                      int convert_256_color);

    /* get the palette - returns null for RGB images */
    HTML_color_t *get_palette() const { return palette_; }
    unsigned int get_palette_size() const { return palette_size_; }

    /* get the dimensions */
    unsigned long get_height() const { return height_; }
    unsigned long get_width() const { return width_; }

    /* get the number of bytes per row */
    unsigned long get_row_bytes() const { return row_bytes_; }

    /* get the row pointers */
    const png_byte *const *get_rows() const { return rows_; }

    /* 
     *   Get the alpha row pointers - this returns an array of pointers to
     *   rows of bytes, with one byte of alpha per pixel.  This will be null
     *   unless the image was read in convert_256_color mode (so the image in
     *   memory is paletted), AND the HTMLPNG_OPT_TRANS_TO_ALPHA flag is set.
     *   
     *   If the image in memory is full color and has transparency
     *   information, it will be stored with 32 bits per pixel in BGRA
     *   format, so the alpha channel must be retrieved directly from the
     *   get_rows() for this in-memory format.  
     */
    const png_byte *const *get_alpha() const { return alpha_; }

    /*
     *   Determine whether we need simple on/off transparency or full alpha.
     *   Returns true if we have only simple transparency.  This can be used
     *   as an optimization to render an image that uses only simple on/off
     *   transparency using a mask, instead of using full alpha blending; on
     *   some systems, masking is much faster than alpha blending.  
     */
    int is_simple_trans() const { return is_simple_trans_; }

    /* color depth in bits per pixel */
    int get_bits_per_pixel() const { return bpp_; }

    /* set global options (HTMLPNG_OPT_xxx flags) */
    static void set_options(unsigned int options)
    {
        /* remember the options */
        options_ = options;
    }

    /* get options */
    static unsigned int get_options() { return options_; }

private:
    /* allocate memory for the image */
    void allocate_rows(png_struct *png_ptr, png_info *info_ptr,
                       unsigned long height, unsigned long width,
                       int sep_alpha);
    
    /* delete the existing image, if any */
    void delete_image();

    /* delete the existing alpha channel information, if any */
    void delete_alpha();

    /* PNG callback function to read from the file */
    static void fread_cb(png_structp, png_bytep, png_size_t);

    /* palette, if the image has been converted to a palette-index format */
    HTML_color_t *palette_;
    unsigned int palette_size_;
    
    /* pointers to the rows containing the image bytes */
    png_bytep *rows_;

    /* pointers to the rows containing the alpha bytes */
    png_bytep *alpha_;

    /* number of rows allocated */
    unsigned long height_;

    /* width of each row */
    unsigned long width_;

    /* bytes in each row */
    unsigned long row_bytes_;

    /* number of bits per pixel */
    int bpp_;

    /* global options */
    static unsigned int options_;

    /* simple on/off transparency only - full alpha blending not required */
    int is_simple_trans_;
};

#endif /* HTMLPNG_H */

