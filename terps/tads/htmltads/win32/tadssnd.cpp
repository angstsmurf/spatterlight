#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadssnd.cpp,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadssnd.cpp - TADS sound object
Function
  
Notes
  
Modified
  01/10/98 MJRoberts  - Creation
*/

#include <Windows.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSSND_H
#include "tadssnd.h"
#endif
#ifndef HTMLSND_H
#include "htmlsnd.h"
#endif

CTadsSound::~CTadsSound()
{
}

/*
 *   load data from a sound loader file 
 */
int CTadsSound::load_from_res(CHtmlSound *res)
{
    /* get the file identification information from the resource */
    fname_.set(res->get_fname());
    seek_pos_ = res->get_seekpos();
    data_size_ = res->get_filesize();

    /* success */
    return 0;
}

