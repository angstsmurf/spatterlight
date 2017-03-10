/* $Header: d:/cvsroot/tads/html/htmlurl.h,v 1.2 1999/05/17 02:52:22 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlurl.h - URL class
Function
  
Notes
  
Modified
  09/07/97 MJRoberts  - Creation
*/

#ifndef HTMLURL_H
#define HTMLURL_H

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif


class CHtmlUrl
{
public:
    CHtmlUrl() { }
    CHtmlUrl(const textchar_t *url) { urltext.set(url); }
    CHtmlUrl(const textchar_t *url, size_t len) { urltext.set(url, len); }

    void set_url(const textchar_t *url) { urltext.set(url); }
    void set_url(const textchar_t *url, size_t len) { urltext.set(url, len); }
    void set_url(const CHtmlUrl *url) { urltext.set(url->get_url()); }
    const textchar_t *get_url() const { return urltext.get(); }

private:
    CStringBuf urltext;
};


#endif /* HTMLURL_H */
