#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/htmlstk.cpp,v 1.2 1999/05/17 02:52:22 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlstk.cpp - 
Function
  
Notes
  
Modified
  08/26/97 MJRoberts  - Creation
*/

#ifndef HTMLSTK_H
#include "htmlstk.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Stack implementation 
 */

void *CHtmlStack::get_item_n(int n) const
{
    CHtmlStackElement *ele;

    /* run through the stack until we get to the desired element */
    for (ele = top_ ; ele != 0 && n > 0 ; --n, ele = ele->nxt_) ;

    /* return it, if there was such an element */
    return ele ? ele->get_item() : 0;
}

void *CHtmlStack::pop()
{
    if (top_)
    {
        void *ret;
        CHtmlStackElement *old_top;

        /* remember the return value */
        ret = top_->get_item();

        /* remember the current top of stack */
        old_top = top_;

        /* point the top of the stack to the next item on the stack */
        top_ = top_->nxt_;

        /* delete the old top-of-stack element */
        delete old_top;

        /* decrease the depth counter */
        --depth_;

        /*
         *   return the item that was on the top of the stack (it didn't
         *   just get deleted - we deleted the stack element containing
         *   the reference to the item, not the item itself) 
         */
        return ret;
    }
    else
        return 0;
}


