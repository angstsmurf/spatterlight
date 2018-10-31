#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/htmljpeg.cpp,v 1.2 1999/05/17 02:52:21 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmljpeg.cpp - JPEG operations
Function
  
Notes
  This code is based in part on Chris Losinger's JpegFile package,
  and uses the JPEG implementation in the Independent JPEG Group's
  (IJG) V.6a code.
Modified
  10/24/97 MJRoberts  - Creation
*/

#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
#include <memory.h>

/* include TADS OS header - we use the TADS OS-layer file I/O routines */
#include <os.h>

/* include jpeglib header - note that this is a C (not C++) header */
extern "C" {
#include <jpeglib.h>
#include <jerror.h>
}

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTML_OS_H
#include "html_os.h"
#endif
#ifndef HTMLJPEG_H
#include "htmljpeg.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Data source object for JPEG implementation.  This object provides an
 *   implementation of the JPEG data source using the TADS OS-layer I/O
 *   routines.  
 */

struct CTadsDataSource : public jpeg_source_mgr
{
public:
    CTadsDataSource()
    {
        /* fill in our implementations of the callback functions */
        init_source = &init_source_cb;
        fill_input_buffer = &fill_input_buffer_cb;
        skip_input_data = &skip_input_data_cb;
        term_source = &term_source_cb;

        /* use the standard jpeglib implementation of this one */
        resync_to_restart = &jpeg_resync_to_restart;

        /* initialize the rest of the jpeglib part of the structure */
        bytes_in_buffer = 0;
        next_input_byte = 0;
    }

    /* set the input file */
    void set_file(osfildef *fp) { fp_ = fp; }

private:
    /* the callback routines */
    static void init_source_cb(j_decompress_ptr);
    static boolean fill_input_buffer_cb(j_decompress_ptr);
    static void skip_input_data_cb(j_decompress_ptr, long);
    static void term_source_cb(j_decompress_ptr);
    
    /* the file we're reading */
    osfildef *fp_;

    /* my input buffer */
    JOCTET buffer_[4096];

    /* flag indicating that we're at the beginning of the file */
    int start_of_file_;
};

/*
 *   Method implementations for TADS JPEG data source.  These are mostly
 *   copied from the jpeglib stdio data source implementation, with
 *   appropriate changes for the TADS OS-layer I/O routines.  
 */

void CTadsDataSource::init_source_cb(j_decompress_ptr cinfo)
{
    CTadsDataSource *src = (CTadsDataSource *)cinfo->src;

    /* note that we haven't started reading yet */
    src->start_of_file_ = TRUE;
}

boolean CTadsDataSource::fill_input_buffer_cb(j_decompress_ptr cinfo)
{
    CTadsDataSource *src = (CTadsDataSource *)cinfo->src;
    size_t nbytes;

    /* read the buffer */
    nbytes = osfrbc(src->fp_, src->buffer_, sizeof(src->buffer_));
    if (nbytes <= 0)
    {
        /* Treat empty input file as fatal error */
        if (src->start_of_file_)
            ERREXIT(cinfo, JERR_INPUT_EMPTY);

        /* warn that we hit EOF */
        WARNMS(cinfo, JWRN_JPEG_EOF);

        /* Insert a fake EOI marker */
        src->buffer_[0] = (JOCTET) 0xFF;
        src->buffer_[1] = (JOCTET) JPEG_EOI;
        nbytes = 2;
    }

    /* fill in statistics */
    src->next_input_byte = src->buffer_;
    src->bytes_in_buffer = nbytes;
    src->start_of_file_ = FALSE;

    /* success */
    return TRUE;
}

void CTadsDataSource::skip_input_data_cb(j_decompress_ptr cinfo, long nbytes)
{
    CTadsDataSource *src = (CTadsDataSource *)cinfo->src;

    /* 
     *   Do this the dumb easy way.  We could use fseek(), but it would be
     *   more trouble (and more error-prone) to figure out what the
     *   disposition of the buffer would be after the operation.  
     */
    if (nbytes > 0)
    {
        while (nbytes > (long)src->bytes_in_buffer)
        {
            nbytes -= (long)src->bytes_in_buffer;
            fill_input_buffer_cb(cinfo);

            /* 
             *   note we assume that fill_input_buffer will never return
             *   FALSE, so suspension need not be handled.  
             */
        }
        src->next_input_byte += (size_t)nbytes;
        src->bytes_in_buffer -= (size_t)nbytes;
    }
}

void CTadsDataSource::term_source_cb(j_decompress_ptr)
{
    /* we don't need to do anything here */
}

/* ------------------------------------------------------------------------ */
/*
 *   JPEG object 
 */

/* statics */
unsigned int CHtmlJpeg::options_  = 0;

/*
 *   create 
 */
CHtmlJpeg::CHtmlJpeg()
{
    /* initialize members - no image yet */
    height_ = 0;
    width_ = 0;
    image_buf_ = 0;
    palette_ = 0;
    palette_size_ = 0;
}

/*
 *   destroy 
 */
CHtmlJpeg::~CHtmlJpeg()
{
    /* if we have an image, delete it */
    delete_image();
}

/*
 *   Delete the image currently stored in the object 
 */
void CHtmlJpeg::delete_image()
{
    /* if we have an image buffer, delete it */
    if (image_buf_ != 0)
        os_free_huge(image_buf_);

    /* delete the palette as well */
    if (palette_ != 0)
        delete [] palette_;

    /* forget all image parameters */
    width_ = 0;
    height_ = 0;
    image_buf_ = 0;
    palette_ = 0;
    palette_size_ = 0;
}


/*
 *   Store an RGB scan line 
 */
static void j_putRGBScanline(unsigned char *jpegline, int widthPix,
                             unsigned char *outBuf, int row)
{
    int offset = row * widthPix * 3;
    int count;
    for (count = 0 ; count < widthPix ; ++count)
    {
        unsigned char iRed, iBlu, iGrn;
        unsigned char *oRed, *oBlu, *oGrn;
        
        iRed = *(jpegline + count * 3 + 0);
        iGrn = *(jpegline + count * 3 + 1);
        iBlu = *(jpegline + count * 3 + 2);
        
        oRed = outBuf + offset + count * 3 + 0;
        oGrn = outBuf + offset + count * 3 + 1;
        oBlu = outBuf + offset + count * 3 + 2;
        
        *oRed = iRed;
        *oGrn = iGrn;
        *oBlu = iBlu;
    }
}

/*
 *   store a palette-indexed scan line 
 */
void j_putMapScanline(unsigned char *jpegline, int widthPix,
                      unsigned char *outBuf, int row)
{
    memcpy(outBuf + row * widthPix, jpegline, widthPix);
}


/*
 *   store a gray-scale scan line 
 */
void j_putGrayScanlineToRGB(unsigned char *jpegline, int widthPix,
                            unsigned char *outBuf, int row)
{
    int offset = row * widthPix * 3;
    int count;
    for (count=0;count<widthPix;count++)
    {
        unsigned char iGray;
        unsigned char *oRed, *oBlu, *oGrn;
        
        /* get our grayscale value */
        iGray = *(jpegline + count);
        
        oRed = outBuf + offset + count * 3;
        oGrn = outBuf + offset + count * 3 + 1;
        oBlu = outBuf + offset + count * 3 + 2;
        
        *oRed = iGray;
        *oGrn = iGray;
        *oBlu = iGray;
    }
}


/*
 *   Extended version of jpeg_error_mgr for use with our error handler 
 */
struct html_error_mgr
{
    struct jpeg_error_mgr pub;                           /* "public" fields */
    jmp_buf setjmp_buffer;                          /* for return to caller */
};

/*
 *   Error handler for jpeglib errors
 */
static void html_error_exit(j_common_ptr cinfo)
{
    /* cinfo->err really points to an html_error_mgr struct; coerce it */
    html_error_mgr *myerr = (html_error_mgr *)cinfo->err;

    /* we could generate the error message: */
    // char buffer[JMSG_LENGTH_MAX];
    // (*cinfo->err->format_message)(cinfo, buffer);

    /* return control to the setjmp point in the caller */
    longjmp(myerr->setjmp_buffer, 1);
}


/*
 *   Read a JPEG file into memory.  This routine roughly follows the
 *   cookbook in the IJG Jpeg code v6 sample file example.c.  
 */
int CHtmlJpeg::read_jpeg_file(const textchar_t *filename,
                              unsigned long seekpos,
                              unsigned long /*filesize*/,
                              int convert_256_color)
{
    struct jpeg_decompress_struct cinfo;
    osfildef *infile;
    JSAMPARRAY row_buffer;                             /* Output row buffer */
    int row_stride;                  /* physical row width in output buffer */
    unsigned long image_buf_size;
    html_error_mgr jerr;
    CTadsDataSource my_data_src;

    /* open the file; return error if it fails */
    if ((infile = osfoprb(filename, OSFTBIN)) == 0)
        return 1;

    /* 
     *   if the global options require 24-bit RGB as the in-memory format,
     *   ignore the caller's request for 8-bit palette conversion 
     */
    if ((options_ & HTMLJPEG_OPT_RGB24) != 0)
        convert_256_color = FALSE;

    /* seek to the starting position in the file */
    osfseek(infile, seekpos, OSFSK_SET);

    /* delete any existing image */
    delete_image();

    /* set up the normal JPEG error routines, then override error_exit. */
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = html_error_exit;

    /* 
     *   establish longjmp context - we'll come here if the JPEG code
     *   throws an error 
     */
    if (setjmp(jerr.setjmp_buffer))
    {
        /* 
         *   If we get here, the JPEG code has signaled an error.  We need
         *   to clean up the JPEG object, delete the buffer, close the
         *   input file, and return failure.  
         */
        jpeg_destroy_decompress(&cinfo);
        delete_image();
        if (infile != 0)
            osfcls(infile);

        /* indicate failure */
        return 1;
    }

    /* Step 1: allocate and initialize JPEG decompression object */
    jpeg_create_decompress(&cinfo);

    /* 
     *   Step 2: specify data source - in this case, our input file.  Note
     *   that we define our own data source, since we want to use the TADS
     *   OS-layer I/O routines.  
     */
    cinfo.src = &my_data_src;
    my_data_src.set_file(infile);

    /* Step 3: read parameters from the JPEG file header */
    jpeg_read_header(&cinfo, TRUE);

    /* 
     *   Step 4: set parameters for decompression.  If the caller
     *   specified that they want the picture converted to a 256-color
     *   palette, quantize the palette. 
     */
    if (convert_256_color)
    {
        size_t i;
        static const struct
        {
            unsigned char r;
            unsigned char g;
            unsigned char b;
        }
        rainbow[] =
        {
            /* a uniform color cube */
            { 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x33 },
            { 0x00, 0x00, 0x66 },
            { 0x00, 0x00, 0x99 },
            { 0x00, 0x00, 0xcc },
            { 0x00, 0x00, 0xff },
            { 0x00, 0x33, 0x00 },
            { 0x00, 0x33, 0x33 },
            { 0x00, 0x33, 0x66 },
            { 0x00, 0x33, 0x99 },
            { 0x00, 0x33, 0xcc },
            { 0x00, 0x33, 0xff },
            { 0x00, 0x66, 0x00 },
            { 0x00, 0x66, 0x33 },
            { 0x00, 0x66, 0x66 },
            { 0x00, 0x66, 0x99 },
            { 0x00, 0x66, 0xcc },
            { 0x00, 0x66, 0xff },
            { 0x00, 0x99, 0x00 },
            { 0x00, 0x99, 0x33 },
            { 0x00, 0x99, 0x66 },
            { 0x00, 0x99, 0x99 },
            { 0x00, 0x99, 0xcc },
            { 0x00, 0x99, 0xff },
            { 0x00, 0xcc, 0x00 },
            { 0x00, 0xcc, 0x33 },
            { 0x00, 0xcc, 0x66 },
            { 0x00, 0xcc, 0x99 },
            { 0x00, 0xcc, 0xcc },
            { 0x00, 0xcc, 0xff },
            { 0x00, 0xff, 0x00 },
            { 0x00, 0xff, 0x33 },
            { 0x00, 0xff, 0x66 },
            { 0x00, 0xff, 0x99 },
            { 0x00, 0xff, 0xcc },
            { 0x00, 0xff, 0xff },
            { 0x33, 0x00, 0x00 },
            { 0x33, 0x00, 0x33 },
            { 0x33, 0x00, 0x66 },
            { 0x33, 0x00, 0x99 },
            { 0x33, 0x00, 0xcc },
            { 0x33, 0x00, 0xff },
            { 0x33, 0x33, 0x00 },
            { 0x33, 0x33, 0x33 },
            { 0x33, 0x33, 0x66 },
            { 0x33, 0x33, 0x99 },
            { 0x33, 0x33, 0xcc },
            { 0x33, 0x33, 0xff },
            { 0x33, 0x66, 0x00 },
            { 0x33, 0x66, 0x33 },
            { 0x33, 0x66, 0x66 },
            { 0x33, 0x66, 0x99 },
            { 0x33, 0x66, 0xcc },
            { 0x33, 0x66, 0xff },
            { 0x33, 0x99, 0x00 },
            { 0x33, 0x99, 0x33 },
            { 0x33, 0x99, 0x66 },
            { 0x33, 0x99, 0x99 },
            { 0x33, 0x99, 0xcc },
            { 0x33, 0x99, 0xff },
            { 0x33, 0xcc, 0x00 },
            { 0x33, 0xcc, 0x33 },
            { 0x33, 0xcc, 0x66 },
            { 0x33, 0xcc, 0x99 },
            { 0x33, 0xcc, 0xcc },
            { 0x33, 0xcc, 0xff },
            { 0x33, 0xff, 0x00 },
            { 0x33, 0xff, 0x33 },
            { 0x33, 0xff, 0x66 },
            { 0x33, 0xff, 0x99 },
            { 0x33, 0xff, 0xcc },
            { 0x33, 0xff, 0xff },
            { 0x66, 0x00, 0x00 },
            { 0x66, 0x00, 0x33 },
            { 0x66, 0x00, 0x66 },
            { 0x66, 0x00, 0x99 },
            { 0x66, 0x00, 0xcc },
            { 0x66, 0x00, 0xff },
            { 0x66, 0x33, 0x00 },
            { 0x66, 0x33, 0x33 },
            { 0x66, 0x33, 0x66 },
            { 0x66, 0x33, 0x99 },
            { 0x66, 0x33, 0xcc },
            { 0x66, 0x33, 0xff },
            { 0x66, 0x66, 0x00 },
            { 0x66, 0x66, 0x33 },
            { 0x66, 0x66, 0x66 },
            { 0x66, 0x66, 0x99 },
            { 0x66, 0x66, 0xcc },
            { 0x66, 0x66, 0xff },
            { 0x66, 0x99, 0x00 },
            { 0x66, 0x99, 0x33 },
            { 0x66, 0x99, 0x66 },
            { 0x66, 0x99, 0x99 },
            { 0x66, 0x99, 0xcc },
            { 0x66, 0x99, 0xff },
            { 0x66, 0xcc, 0x00 },
            { 0x66, 0xcc, 0x33 },
            { 0x66, 0xcc, 0x66 },
            { 0x66, 0xcc, 0x99 },
            { 0x66, 0xcc, 0xcc },
            { 0x66, 0xcc, 0xff },
            { 0x66, 0xff, 0x00 },
            { 0x66, 0xff, 0x33 },
            { 0x66, 0xff, 0x66 },
            { 0x66, 0xff, 0x99 },
            { 0x66, 0xff, 0xcc },
            { 0x66, 0xff, 0xff },
            { 0x99, 0x00, 0x00 },
            { 0x99, 0x00, 0x33 },
            { 0x99, 0x00, 0x66 },
            { 0x99, 0x00, 0x99 },
            { 0x99, 0x00, 0xcc },
            { 0x99, 0x00, 0xff },
            { 0x99, 0x33, 0x00 },
            { 0x99, 0x33, 0x33 },
            { 0x99, 0x33, 0x66 },
            { 0x99, 0x33, 0x99 },
            { 0x99, 0x33, 0xcc },
            { 0x99, 0x33, 0xff },
            { 0x99, 0x66, 0x00 },
            { 0x99, 0x66, 0x33 },
            { 0x99, 0x66, 0x66 },
            { 0x99, 0x66, 0x99 },
            { 0x99, 0x66, 0xcc },
            { 0x99, 0x66, 0xff },
            { 0x99, 0x99, 0x00 },
            { 0x99, 0x99, 0x33 },
            { 0x99, 0x99, 0x66 },
            { 0x99, 0x99, 0x99 },
            { 0x99, 0x99, 0xcc },
            { 0x99, 0x99, 0xff },
            { 0x99, 0xcc, 0x00 },
            { 0x99, 0xcc, 0x33 },
            { 0x99, 0xcc, 0x66 },
            { 0x99, 0xcc, 0x99 },
            { 0x99, 0xcc, 0xcc },
            { 0x99, 0xcc, 0xff },
            { 0x99, 0xff, 0x00 },
            { 0x99, 0xff, 0x33 },
            { 0x99, 0xff, 0x66 },
            { 0x99, 0xff, 0x99 },
            { 0x99, 0xff, 0xcc },
            { 0x99, 0xff, 0xff },
            { 0xcc, 0x00, 0x00 },
            { 0xcc, 0x00, 0x33 },
            { 0xcc, 0x00, 0x66 },
            { 0xcc, 0x00, 0x99 },
            { 0xcc, 0x00, 0xcc },
            { 0xcc, 0x00, 0xff },
            { 0xcc, 0x33, 0x00 },
            { 0xcc, 0x33, 0x33 },
            { 0xcc, 0x33, 0x66 },
            { 0xcc, 0x33, 0x99 },
            { 0xcc, 0x33, 0xcc },
            { 0xcc, 0x33, 0xff },
            { 0xcc, 0x66, 0x00 },
            { 0xcc, 0x66, 0x33 },
            { 0xcc, 0x66, 0x66 },
            { 0xcc, 0x66, 0x99 },
            { 0xcc, 0x66, 0xcc },
            { 0xcc, 0x66, 0xff },
            { 0xcc, 0x99, 0x00 },
            { 0xcc, 0x99, 0x33 },
            { 0xcc, 0x99, 0x66 },
            { 0xcc, 0x99, 0x99 },
            { 0xcc, 0x99, 0xcc },
            { 0xcc, 0x99, 0xff },
            { 0xcc, 0xcc, 0x00 },
            { 0xcc, 0xcc, 0x33 },
            { 0xcc, 0xcc, 0x66 },
            { 0xcc, 0xcc, 0x99 },
            { 0xcc, 0xcc, 0xcc },
            { 0xcc, 0xcc, 0xff },
            { 0xcc, 0xff, 0x00 },
            { 0xcc, 0xff, 0x33 },
            { 0xcc, 0xff, 0x66 },
            { 0xcc, 0xff, 0x99 },
            { 0xcc, 0xff, 0xcc },
            { 0xcc, 0xff, 0xff },
            { 0xff, 0x00, 0x00 },
            { 0xff, 0x00, 0x33 },
            { 0xff, 0x00, 0x66 },
            { 0xff, 0x00, 0x99 },
            { 0xff, 0x00, 0xcc },
            { 0xff, 0x00, 0xff },
            { 0xff, 0x33, 0x00 },
            { 0xff, 0x33, 0x33 },
            { 0xff, 0x33, 0x66 },
            { 0xff, 0x33, 0x99 },
            { 0xff, 0x33, 0xcc },
            { 0xff, 0x33, 0xff },
            { 0xff, 0x66, 0x00 },
            { 0xff, 0x66, 0x33 },
            { 0xff, 0x66, 0x66 },
            { 0xff, 0x66, 0x99 },
            { 0xff, 0x66, 0xcc },
            { 0xff, 0x66, 0xff },
            { 0xff, 0x99, 0x00 },
            { 0xff, 0x99, 0x33 },
            { 0xff, 0x99, 0x66 },
            { 0xff, 0x99, 0x99 },
            { 0xff, 0x99, 0xcc },
            { 0xff, 0x99, 0xff },
            { 0xff, 0xcc, 0x00 },
            { 0xff, 0xcc, 0x33 },
            { 0xff, 0xcc, 0x66 },
            { 0xff, 0xcc, 0x99 },
            { 0xff, 0xcc, 0xcc },
            { 0xff, 0xcc, 0xff },
            { 0xff, 0xff, 0x00 },
            { 0xff, 0xff, 0x33 },
            { 0xff, 0xff, 0x66 },
            { 0xff, 0xff, 0x99 },
            { 0xff, 0xff, 0xcc },
            { 0xff, 0xff, 0xff },

            /* MS Windows standard system colors not in the above cube */
            { 0x80, 0x00, 0x00 },
            { 0x00, 0x80, 0x00 },
            { 0x80, 0x80, 0x00 },
            { 0x00, 0x00, 0x80 },
            { 0x80, 0x00, 0x80 },
            { 0x00, 0x80, 0x80 },
            { 0xc0, 0xc0, 0xc0 },
            { 0xc0, 0xdc, 0xc0 },
            { 0xa6, 0xca, 0xf0 },
            { 0xff, 0xfb, 0xf0 },
            { 0xa0, 0xa0, 0xa4 },
            { 0x80, 0x80, 0x80 },

            /* 
             *   We have 216 colors in the cube plus 12 above, which
             *   leaves us with 28 colors.  Allocate the remaining colors
             *   to ramps of red, green, blue, and gray, each with 7
             *   levels, filling in holes in the color cube for those
             *   colors.
             *   
             *   However, note that in the Windows colors above, we
             *   already have a mid-level entry for each color (0x80,
             *   0x00, 0x00 for red, likewise for blue, green, and gray),
             *   so we can omit this from our ramp.  
             */
            { 0x11, 0x00, 0x00 },
            { 0x22, 0x00, 0x00 },
            { 0x44, 0x00, 0x00 },
            { 0x55, 0x00, 0x00 },
            { 0xb2, 0x00, 0x00 },
            { 0xdd, 0x00, 0x00 },
            { 0xee, 0x00, 0x00 },

            { 0x00, 0x11, 0x00 },
            { 0x00, 0x22, 0x00 },
            { 0x00, 0x44, 0x00 },
            { 0x00, 0x55, 0x00 },
            { 0x00, 0xb2, 0x00 },
            { 0x00, 0xdd, 0x00 },
            { 0x00, 0xee, 0x00 },

            { 0x00, 0x00, 0x11 },
            { 0x00, 0x00, 0x22 },
            { 0x00, 0x00, 0x44 },
            { 0x00, 0x00, 0x55 },
            { 0x00, 0x00, 0xb2 },
            { 0x00, 0x00, 0xdd },
            { 0x00, 0x00, 0xee },

            { 0x11, 0x11, 0x11 },
            { 0x22, 0x22, 0x22 },
            { 0x44, 0x44, 0x44 },
            { 0x55, 0x55, 0x55 },
            { 0xb2, 0xb2, 0xb2 },
            { 0xdd, 0xdd, 0xdd },
            { 0xee, 0xee, 0xee }
        };
        const size_t rainbow_count = sizeof(rainbow)/sizeof(rainbow[0]);
        
        /* tell the decompressor to quantize the picture to 256 colors */
        cinfo.quantize_colors = TRUE;
        cinfo.desired_number_of_colors = 256;
        cinfo.two_pass_quantize = TRUE;
        cinfo.enable_2pass_quant = TRUE;
        cinfo.dither_mode = JDITHER_FS;

        /* set up our standard rainbow palette */
        cinfo.actual_number_of_colors = rainbow_count;
        cinfo.colormap = (*cinfo.mem->alloc_sarray)
                         ((j_common_ptr)&cinfo, JPOOL_IMAGE,
                          (JDIMENSION)rainbow_count, (JDIMENSION)3);

        for (i = 0 ; i < rainbow_count ; ++i)
        {
            cinfo.colormap[0][i] = rainbow[i].r;
            cinfo.colormap[1][i] = rainbow[i].g;
            cinfo.colormap[2][i] = rainbow[i].b;
        }
    }
    else
    {
        /* 
         *   we want a 24-bit format - ask for RGB output so that the jpeg
         *   decoder will convert other formats for us 
         */
        cinfo.out_color_space = JCS_RGB;
    }

    /* 
     *   Step 5: start decompression; since suspension isn't possible with
     *   a file data source, "starting" is sufficient to decompress the
     *   entire image.  
     */
    jpeg_start_decompress(&cinfo);

    /* 
     *   allocate a buffer big enough to hold the entire image - there are
     *   (height times width) pixels, and cinfo.output_components bytes
     *   per pixel 
     */
    image_buf_size = cinfo.output_width * cinfo.output_height
                     * cinfo.output_components;
    image_buf_ = (OS_HUGEPTR(unsigned char))os_alloc_huge(image_buf_size);
    if (image_buf_ == 0)
    {
        /* failed to allocate a buffer - clean up and return failure */
        jpeg_destroy_decompress(&cinfo);
        osfcls(infile);
        return 1;
    }

    /* we now know the dimensions - store them */
    width_ = cinfo.output_width;
    height_ = cinfo.output_height;

    /* JSAMPLEs per row in output buffer */
    row_stride = cinfo.output_width * cinfo.output_components;

    /* allocate a temporary one-row-high sample array */
    row_buffer = (*cinfo.mem->alloc_sarray)
                 ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

    /* Step 6: read scan lines until we run out of scan lines */
    while (cinfo.output_scanline < cinfo.output_height)
    {
        /* read the next scan line */
        jpeg_read_scanlines(&cinfo, row_buffer, 1);

        /* get the appropriate type of scan line */
        if (cinfo.out_color_components == 3 && cinfo.output_components == 3)
        {
            /* move an RGB scan line into the buffer */
            j_putRGBScanline(row_buffer[0], width_, image_buf_,
                             cinfo.output_scanline - 1);
        }
        else if ((cinfo.out_color_components == 3
                  && cinfo.output_components == 1)
                 || (cinfo.out_color_components == 1
                     && cinfo.output_components == 1))
        {
            /* move a palette-indexed scan line into the buffer */
            j_putMapScanline(row_buffer[0], width_, image_buf_,
                             cinfo.output_scanline - 1);
        }
        else if (cinfo.out_color_components == 1
                 && cinfo.out_color_components == 3)
        {
            /* move a grayscale scan line into the buffer */
            j_putGrayScanlineToRGB(row_buffer[0], width_,
                                   image_buf_, cinfo.output_scanline-1);
        }
    }

    /*
     *   If we're converting the image to a palette-based 256-color image,
     *   remember the palette.  
     */
    if (convert_256_color)
    {
        HTML_color_t *palptr;
        unsigned int indx;

        /* create the palette */
        palette_size_ = cinfo.actual_number_of_colors;
        if (palette_size_ > 256)
            palette_size_ = 256;
        palette_ = new HTML_color_t[palette_size_];

        /* store the palette */
        for (indx = 0, palptr = palette_ ; indx < palette_size_ ;
             ++indx, ++palptr)
        {
            // $$$ should change these array dereferences to use pointers
            *palptr = HTML_make_color(cinfo.colormap[0][indx],
                                      cinfo.colormap[1][indx],
                                      cinfo.colormap[2][indx]);
        }
    }

    /* Step 7: Finish decompression. */
    jpeg_finish_decompress(&cinfo);

    /* Step 8: Delete the JPEG decompression object */
    jpeg_destroy_decompress(&cinfo);

    /* 
     *   After finish_decompress, we can close the input file.  Here we
     *   postpone it until after no more JPEG errors are possible, so as
     *   to simplify the setjmp error logic above.  
     */
    osfcls(infile);

    /* success */
    return 0;
}

