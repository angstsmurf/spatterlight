/* $Header: d:/cvsroot/tads/html/htmljpeg.h,v 1.2 1999/05/17 02:52:21 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmljpeg.h - HTML JPEG implementation
Function
  This code is based in part on Chris Losinger's JpegFile package,
  and uses the JPEG implementation in the Independent JPEG Group's
  (IJG) V.6a code.
Notes
  
Modified
  10/24/97 MJRoberts  - Creation
*/

#ifndef HTMLJPEG_H
#define HTMLJPEG_H

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTML_OS_H
#include "html_os.h"
#endif

/* ------------------------------------------------------------------------ */
/*
 *   Global options 
 */

/* 
 *   always use 24-bit RGB for the in-memory representation, even if the
 *   video mode is paletted 
 */
#define HTMLJPEG_OPT_RGB24  0x0001

/* ------------------------------------------------------------------------ */
/*
 *   portable JPEG image handler
 */
class CHtmlJpeg
{
public:
    CHtmlJpeg();
    ~CHtmlJpeg();
    
    /* set global options */
    static void set_options(unsigned int options) { options_ = options; }

    /* 
     *   Load a JPEG from a file.  Returns zero on success, non-zero on
     *   error.  If an image was previously stored in this object, the old
     *   image is discarded.  If convert_256_color is set, we convert the
     *   image to a palette-index format and reduce the color map to 256
     *   colors.  
     */
    int read_jpeg_file(const textchar_t *filename, unsigned long seekpos,
                       unsigned long filesize, int convert_256_color);

    /* get the dimensions of the image */
    unsigned long get_width() const { return width_; }
    unsigned long get_height() const { return height_; }

    /* get a pointer to the image buffer */
    OS_HUGEPTR(unsigned char) get_image_buf() const { return image_buf_; }

    /* get the palette - returns null for RGB images */
    HTML_color_t *get_palette() const { return palette_; }
    unsigned int get_palette_size() const { return palette_size_; }

private:
    /* delete the image, if we have one */
    void delete_image();
    
    /* pointer to the image buffer */
    OS_HUGEPTR(unsigned char) image_buf_;

    /* palette, if the image has been converted to a palette-index format */
    HTML_color_t *palette_;
    unsigned int palette_size_;

    /* dimensions */
    unsigned long width_, height_;

    /* global options */
    static unsigned int options_;
};

#endif /* HTMLJPEG_H */

