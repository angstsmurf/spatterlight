#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/htmlpng.cpp,v 1.2 1999/05/17 02:52:21 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlpng.cpp - PNG image object implementation
Function
  
Notes
  
Modified
  11/08/97 MJRoberts  - Creation
*/

/* include main PNG header */
#include <png.h>

/* include our extended PNG library header */
#include "pngext.h"

/* include TADS OS headers - we need to use TADS OS-layer I/O routines */
#include <os.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTML_OS_H
#include "html_os.h"
#endif
#ifndef HTMLPNG_H
#include "htmlpng.h"
#endif

/* ------------------------------------------------------------------------ */
/*
 *   statics 
 */

/* options flags - set all flags to zero by default */
unsigned int CHtmlPng::options_ = 0;


/* ------------------------------------------------------------------------ */
/*
 *   constructor
 */
CHtmlPng::CHtmlPng()
{
    /* no image allocated yet */
    rows_ = 0;
    alpha_ = 0;
    height_ = 0;
    width_ = 0;
    palette_ = 0;
    palette_size_ = 0;
    is_simple_trans_ = FALSE;
}

CHtmlPng::~CHtmlPng()
{
    /* delete the image */
    delete_image();

    /* delete any alpha information we have */
    delete_alpha();
}

/*
 *   Delete the existing image 
 */
void CHtmlPng::delete_image()
{
    /* if we have an image, delete it */
    if (rows_ != 0)
    {
        unsigned long i;
        png_bytep *rowp;
        
        /* delete each row */
        for (i = 0, rowp = rows_ ; i < height_ ; ++i, ++rowp)
            th_free(*rowp);

        /* delete the row pointer array */
        th_free(rows_);

        /* no more image */
        rows_ = 0;
    }

    /* if we have a palette, free it */
    if (palette_ != 0)
    {
        delete [] palette_;
        palette_ = 0;
    }
}

/*
 *   Delete any existing alpha channel information 
 */
void CHtmlPng::delete_alpha()
{
    /* if we have an alpha channel, delete it */
    if (alpha_ != 0)
    {
        unsigned long i;
        png_bytep *rowp;
        
        /* delete each row */
        for (i = 0, rowp = alpha_ ; i < height_ ; ++i, ++rowp)
            th_free(*rowp);

        /* delete the pointer array */
        th_free(alpha_);

        /* no more alpha */
        alpha_ = 0;
    }
}

/*
 *   Load a PNG file 
 */
int CHtmlPng::read_png_file(const textchar_t *filename,
                            unsigned long seekpos, unsigned long /*filesize*/,
                            int convert_256_color)
{
    png_structp png_ptr;
    png_infop info_ptr;
    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type;
    osfildef *infile;
    double screen_gamma, image_gamma;
    png_color_16 image_background;
    png_color_16 *image_background_ptr;
    int trans_to_alpha;
    int pass_number;
    int sep_alpha;

    /* open the file; return error if it fails */
    if ((infile = osfoprb(filename, OSFTBIN)) == 0)
        return 1;

    /* 
     *   if we always want to use a 24-bit RGB in-memory representation,
     *   don't convert to 8-bit, even if the caller indicated we should 
     */
    if ((options_ & HTMLPNG_OPT_RGB24) != 0)
        convert_256_color = FALSE;

    /* delete any existing image and alpha information */
    delete_image();
    delete_alpha();

    /* 
     *   note that this is the first pass, in case we have to come back
     *   for a second 
     */
    pass_number = 1;

    /* presume we won't need to separate any alpha information */
    sep_alpha = FALSE;

read_file:
    /* seek to the start of the image in the file */
    osfseek(infile, seekpos, OSFSK_SET);

    /* create and initialize the png_struct */
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                     (void *)0, (png_error_ptr)0,
                                     (png_error_ptr)0);

    /* if that failed, return an error */
    if (png_ptr == 0)
    {
        osfcls(infile);
        return 1;
    }

    /* allocate and initialize image header memory */
    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == 0)
    {
        osfcls(infile);
        png_destroy_read_struct(&png_ptr, (png_infopp)0, (png_infopp)0);
        return 1;
    }

    /* set up error handling */
    if (setjmp(png_ptr->jmpbuf))
    {
        /* an error occurred - clean up and return failure */
        osfcls(infile);
        png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
        return 1;
    }

    /* use our own file reader */
    png_set_read_fn(png_ptr, (void *)infile, &fread_cb);

    /* read the header */
    png_read_info(png_ptr, info_ptr);
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth,
                 &color_type, &interlace_type, (int *)0, (int *)0);

    /* reduce color precision to 8 bits per color if it's higher */
    if (info_ptr->bit_depth == 16)
        png_set_strip_16(png_ptr);

    /* set the screen gamma value */
    screen_gamma = 2.2;
    if (png_get_gAMA(png_ptr, info_ptr, &image_gamma))
        png_set_gamma(png_ptr, screen_gamma, image_gamma);
    else
        png_set_gamma(png_ptr, screen_gamma, 0.45);

    /* presume we won't want to convert transparency to alpha */
    trans_to_alpha = FALSE;

    /* check to see if we need to convert transparency to an alpha channel */
    if ((options_ & HTMLPNG_OPT_TRANS_TO_ALPHA) != 0)
    {
        int has_trans;
        
        /* get the transparency information from the file */
        has_trans = png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS);

        /* if there's transparency, convert it to alpha channel data */
        if (has_trans)
        {
            int num_trans;
            png_bytep trans;
            png_color_16p trans_values;

            /* convert the transparency information to an alpha channel */
            trans_to_alpha = TRUE;

            /* tell libpng to convert the transparency info to alpha */
            png_set_tRNS_to_alpha(png_ptr);

            /*
             *   Check for simple transparency.  If we have only full on and
             *   full off values in our transparency ranges, the OS code
             *   might want to render using masking rather than full alpha
             *   blending, since masking is sometimes faster and has exactly
             *   the same effect as alpha blending for an image with only
             *   full-on and full-off transparency values. 
             */
            if (png_get_tRNS(png_ptr, info_ptr, &trans, &num_trans,
                             &trans_values))
            {
                /* check for a single value or palette list */
                if (trans != 0)
                {
                    int i;
                    
                    /* 
                     *   we have a palette-ordered list of transparency
                     *   values; check each, and if we find any that are not
                     *   either fully on (255) or fully off (0), we need full
                     *   alpha; otherwise, we can make do with a simple mask 
                     */
                    for (is_simple_trans_ = TRUE, i = 0 ; i < num_trans ; ++i)
                    {
                        /* if this isn't fully on or off, we need alpha */
                        if (trans[i] != 0 && trans[i] != 255)
                        {
                            /* note that we can't have simple masking */
                            is_simple_trans_ = FALSE;

                            /* no need to look any further */
                            break;
                        }
                    }
                }
                else
                {
                    /* 
                     *   we have just a single transparency color, which is
                     *   fully transparent, and everything else is fully
                     *   opaque - this means we can use simple masking if
                     *   desired
                     */
                    is_simple_trans_ = TRUE;
                }
            }
        }
    }

    /* 
     *   Reduce to an 8-bit palette if desired.
     *   
     *   If we have transparency-to-alpha conversion, translate to RGBA
     *   format on the first pass so that we can extract the alpha channel,
     *   if desired; once we've pulled out the alpha channel, we'll run the
     *   second pass to convert to paletted as originally desired.  
     */
    if (convert_256_color && !(trans_to_alpha && pass_number == 1))
    {
        int num_palette;
        png_colorp palette;

        /* check to see if the image includes a palette */
        if (png_get_PLTE(png_ptr, info_ptr, &palette, &num_palette))
        {
            png_uint_16p histogram;

            /* it has a palette - dither it to the palette histogram */
            png_get_hIST(png_ptr, info_ptr, &histogram);
            png_set_dither(png_ptr, palette, num_palette,
                           256, histogram, FALSE);
        }
        else
        {
            /* dither to a rainbow color cube */
            png_set_rainbow_dither(png_ptr);
        }

        /* if it's grayscale with bit depth less than 8, expand to 8 bits */
        if (info_ptr->color_type == PNG_COLOR_TYPE_GRAY
            && info_ptr->bit_depth < 8)
            png_set_gray_1_2_4_to_8(png_ptr);
    }
    else
    {
        /* ask for full RGB expansion */
        png_set_expand(png_ptr);

        /* convert gray to RGB */
        if (info_ptr->color_type == PNG_COLOR_TYPE_GRAY
            || info_ptr->color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
            png_set_gray_to_rgb(png_ptr);

        /* 
         *   if this is the first pass, and we're doing transparency-to-alpha
         *   conversion, and we ultimately want a paletted image, note that
         *   we want to separate the alpha channel on this pass 
         */
        if (trans_to_alpha && pass_number == 1 && convert_256_color)
            sep_alpha = TRUE;
    }

    /* 
     *   Set the background color if the options are set to drop alpha
     *   information from the final image, AND we're not doing
     *   transparency-to-alpha conversion.
     *   
     *   Don't set a background color if we want to keep alpha or we are
     *   converting transparency to alpha, because in either case we will do
     *   our own compositing onto the actual background during rendering and
     *   thus don't want to force a default background here.  
     */
    if ((options_ & HTMLPNG_OPT_NO_ALPHA) != 0 && !trans_to_alpha)
    {
        /*  
         *   Check the file for a background color, and use it if
         *   specified; otherwise, use a default of white. 
         */
        if (png_get_bKGD(png_ptr, info_ptr, &image_background_ptr))
        {
            /* there's a background color specified in the file -- use it */
            png_set_background(png_ptr, image_background_ptr,
                               PNG_BACKGROUND_GAMMA_FILE, 1, 1.0);
        }
        else
        {
            unsigned int level;

            /* 
             *   There's no background color specified in the file -- use a
             *   light gray (almost white) background by default.  Note that
             *   we must set the background level to an 8-bit or 16-bit RGB
             *   value, depending on the input format.
             *   
             *   Note also that we take care not to set the value to true
             *   gray (R==G==B) value, because the PNG library seems to have
             *   a problem decoding 8-bit grayscale images with alpha
             *   channels and no backgrounds when we do.  This appears to be
             *   a bug in pnglib's handling of the order of transformations
             *   during reading; it seems to do the gray-to-RGB transform too
             *   late in the process in this particular case due to an
             *   apparent optimization.  We can suppress the errant
             *   optimization simply by using a background color that isn't a
             *   true gray value - we need only set one component off by one
             *   level to accomplish this.  
             */
            level = (info_ptr->bit_depth == 8 ? 0xE0 : 0xE000);
            image_background.red = (png_uint_16)level;
            image_background.green = (png_uint_16)level;
            image_background.blue = (png_uint_16)level + 1;
            png_set_background(png_ptr, &image_background,
                               PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
        }
    }

    /* read pixels as BGR */
    png_set_bgr(png_ptr);

    /* swap bytes of 16 bit files to least significant byte first */
    png_set_swap(png_ptr);

    /* update the palette */
    png_read_update_info(png_ptr, info_ptr);

    /* remember the width in pixels and bytes */
    width_ = info_ptr->width;
    row_bytes_ = png_get_rowbytes(png_ptr, info_ptr);

    /* allocate memory */
    allocate_rows(png_ptr, info_ptr, height, width, sep_alpha);

    /* 
     *   calculate the bits per pixel by dividing the row width in bytes
     *   by the row width in pixels, and multiplying by 8 
     */
    bpp_ = (row_bytes_ / width_) * 8;

    /* read the image */
    png_read_image(png_ptr, rows_);

    /* done reading */
    png_read_end(png_ptr, info_ptr);

    /*
     *   If we want a color palette, and the image has one (it should),
     *   save the color palette for later use 
     */
    if (convert_256_color && png_ptr->palette != 0)
    {
        png_colorp src;
        HTML_color_t *dst;
        unsigned int i;
        
        /* create the palette */
        palette_size_ = png_ptr->num_palette;
        if (palette_size_ > 256) palette_size_ = 256;
        palette_ = new HTML_color_t[palette_size_];

        /* store it */
        for (dst = palette_, src = png_ptr->palette, i = 0 ;
             i < palette_size_ ; ++i, ++src, ++dst)
            *dst = HTML_make_color(src->red, src->green, src->blue);
    }

    /* done - clean up */
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);

    /* 
     *   If we wanted a paletted version of the image, and we read
     *   transparency information, we will have created an RGBA version on
     *   the first pass rather than the paletted version requested.  If
     *   this is the case, we must take the 'A' part out of the image to
     *   create a separate alpha channel, then go back and re-read the
     *   image, this time reading it as (or converting it to) the paletted
     *   format.  
     */
    if (sep_alpha)
    {
        unsigned long y;
        png_bytep *srcrow;
        png_bytep *dstrow;
            
        /* copy the alpha information into the separate alpha stream */
        for (y = 0, srcrow = rows_, dstrow = alpha_ ; y < height_ ;
             ++y, ++srcrow, ++dstrow)
        {
            png_byte *src;
            png_byte *dst;
            unsigned long x;
            
            /* 
             *   copy this row's alpha bytes - we read the rows in BGRA
             *   format, so every fourth byte is an alpha byte 
             */
            for (x = 0, src = *srcrow, dst = *dstrow ; x < width_ ;
                 ++x, src += 4, ++dst)
            {
                /* copy this byte of alpha */
                *dst = *(src + 3);
            }
        }

        /* 
         *   delete the image we just loaded, since it's not the format we
         *   really want 
         */
        delete_image();

        /* 
         *   go back for a second pass - note that we're on the second
         *   pass, and also note that we do not want a separate alpha
         *   channel this time through, since we've already stored it on
         *   this pass 
         */
        pass_number = 2;
        sep_alpha = FALSE;
        goto read_file;
    }

    /* close the input file */
    osfcls(infile);

    /* success */
    return 0;
}

/*
 *   allocate memory for the image 
 */
void CHtmlPng::allocate_rows(png_struct *png_ptr, png_info *info_ptr,
                             unsigned long height, unsigned long width,
                             int sep_alpha)
{
    png_bytep *rowp;
    png_uint_32 row;
    png_uint_32 rowbytes;

    /* remember the number of rows */
    height_ = height;

    /* allocate the row pointer */
    rows_ = (png_bytep *)th_malloc(height * sizeof(*rows_));

    /* allocate the rows */
    rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    for (row = 0, rowp = rows_ ; row < height ; ++row, ++rowp)
        *rowp = (png_bytep)th_malloc(rowbytes);

    /* if we need separate alpha information, allocate it as well */
    if (sep_alpha)
    {
        /* allocate the alpha row pointer */
        alpha_ = (png_bytep *)th_malloc(height * sizeof(*alpha_));

        /* allocate the alpha rows - one byte per pixel */
        for (row = 0, rowp = alpha_ ; row < height ; ++row, ++rowp)
            *rowp = (png_bytep)th_malloc(width);
    }
}

/*
 *   fread() replacement - we don't use stdio, but instead use the TADS
 *   OS-layer file I/O routines 
 */
void CHtmlPng::fread_cb(png_structp png_ptr, png_bytep buf, png_size_t siz)
{
    osfildef *fp;

    /* get the I/O pointer from png - it's the osfildef pointer */
    fp = (osfildef *)png_get_io_ptr(png_ptr);

    /* read the buffer */
    if (osfrb(fp, buf, siz))
        png_error(png_ptr, "error reading file");
}

