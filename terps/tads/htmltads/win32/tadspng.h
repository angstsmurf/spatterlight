/* $Header: d:/cvsroot/tads/html/win32/tadspng.h,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadspng.h - Win32 PNG object
Function
  
Notes
  
Modified
  11/08/97 MJRoberts  - Creation
*/

#ifndef TADSPNG_H
#define TADSPNG_H

#include <stdlib.h>
#include <png.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTML_OS_H
#include "html_os.h"
#endif
#ifndef TADSIMG_H
#include "tadsimg.h"
#endif


class CTadsPng: public CTadsImage
{
public:
    CTadsPng();
    ~CTadsPng();
    
    /* load from a PNG object */
    int load_from_png(class CHtmlPng *png);

    /* 
     *   initialize alpha support - check to see if this version of Windows
     *   supports alpha blending, and set PNG read options accordingly 
     */
    static void init_alpha_support();

private:
    /* create the mask, if we have transparency information */
    int create_mask(class CHtmlPng *png, int pix_bytes_per_pixel);
};


#endif /* TADSPNG_H */

