#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/htmltxar.cpp,v 1.2 1999/05/17 02:52:22 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmltxar.cpp - text array
Function
  
Notes
  
Modified
  09/23/97 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <assert.h>

#ifndef HTMLTXAR_H
#include "htmltxar.h"
#endif

CHtmlTextArray::CHtmlTextArray()
{
    /* allocate the initial page pointer array */
    page_entries_ = 128;
    pages_ = (CHtmlTextArrayEntry *)th_malloc(page_entries_ *
                                              sizeof(CHtmlTextArrayEntry));
    assert(pages_ != 0);

    /* allocate the first page */
    alloc_first_page();

    /* nothing consumed yet */
    max_addr_ = 0;
    mem_in_use_ = 0;
}

CHtmlTextArray::~CHtmlTextArray()
{
    size_t i;
    CHtmlTextArrayEntry *p;

    /* delete each page we've allocated */
    for (i = 0, p = pages_ ; i < pages_alloced_ ; ++i, ++p)
    {
        /* free this page if it was allocated */
        if (p->text_ != 0)
            th_free(p->text_);
    }

    /* delete the page array itself */
    th_free(pages_);
}

/*
 *   clear the text array - deletes all pages, but doesn't delete the page
 *   list itself 
 */
void CHtmlTextArray::clear()
{
    size_t i;
    CHtmlTextArrayEntry *p;

    /* delete each page we've allocated */
    for (i = 0, p = pages_ ; i < pages_alloced_ ; ++i, ++p)
    {
        /* free this page if it was allocated */
        if (p->text_ != 0)
        {
            /* free the page */
            th_free(p->text_);

            /* forget about it */
            p->text_ = 0;
        }
    }

    /* reset everything */
    pages_alloced_ = 0;
    pages_in_use_ = 0;
    max_addr_ = 0;
    mem_in_use_ = 0;

    /* allocate the first page */
    alloc_first_page();
}

/*
 *   Remove a reference to a block of text previously allocated.  If this
 *   leaves the page containing the text unreferenced, we'll delete the
 *   entire page. 
 */
void CHtmlTextArray::delete_text(unsigned long addr, size_t len)
{
    size_t pg;

    /* get the containing page */
    pg = get_page(addr);
    
    /* remove a reference to the containing page */
    --(pages_[pg].refs_);

    /* subtract the size from the space-in-use marker on the page */
    pages_[pg].space_in_use_ -= len;

    /* deduct this space from the total memory in use */
    mem_in_use_ -= len;

    /* 
     *   If that leaves the page unreferenced, delete it.  Never delete
     *   the active (i.e., last) page, even if it's otherwise
     *   unreferenced, since we could create a new reference in it at any
     *   time.  
     */
    if (pages_[pg].refs_ == 0 && pg != pages_alloced_ - 1)
    {
        /* deduct anything remaining on this page from the memory in use */
        mem_in_use_ -= pages_[pg].space_in_use_;

        /* delete the page's memory */
        th_free(pages_[pg].text_);

        /* forget the pointer, since it's gone now */
        pages_[pg].text_ = 0;

        /* that's one less page in use */
        --pages_in_use_;
    }
}

/*
 *   allocate the first page 
 */
void CHtmlTextArray::alloc_first_page()
{
    pages_[0].text_ = (textchar_t *)th_malloc(HTML_TEXTARRAY_PAGESIZE);
    pages_[0].used_ = 0;
    pages_[0].refs_ = 0;
    pages_[0].space_in_use_ = 0;
    pages_[0].alloced_ = 0;
    pages_alloced_ = 1;
    pages_in_use_ = 1;
    assert(pages_[0].text_ != 0);
}


/*
 *   Add the text to the array, counting the reference
 */
unsigned long CHtmlTextArray::append_text(const textchar_t *txt, size_t len)
{
    unsigned long addr;
    size_t pg;
    
    /* add the text normally */
    addr = store_text(txt, len);

    /* count the reference to the page containing the text */
    pg = get_page(addr);
    ++(pages_[pg].refs_);

    /* return the address */
    return addr;
}

/*
 *   Temporarily store text without committing the space 
 */
unsigned long CHtmlTextArray::store_text_temp(const textchar_t *txt,
                                              size_t len)
{
    unsigned long addr;

    /*
     *   Make sure the text will fit in the page in a single chunk by
     *   reserving space for it 
     */
    addr = reserve_space(len);

    /*
     *   Note the maximum address assigned, regardless of whether the
     *   space is committed yet or not 
     */
    max_addr_ = addr + len;

    /* note the amount reserved on the page */
    pages_[last_page()].alloced_ += len;

    /* copy the text into the page at the current offset */
    if (len != 0)
        memcpy(pages_[last_page()].text_ + last_page_ofs(), txt, len);

    /* return the address */
    return addr;
}

/*
 *   Store text, committing the space 
 */
unsigned long CHtmlTextArray::store_text(const textchar_t *txt, size_t len)
{
    unsigned long addr;
    size_t pg;
    
    /* store the text */
    addr = store_text_temp(txt, len);

    /* count the space in use on the page */
    pg = get_page(addr);
    pages_[pg].space_in_use_ += len;

    /* count this space in the total memory we're using */
    mem_in_use_ += len;

    /*
     *   commit the storage by moving the page's free space pointer past
     *   the text 
     */
    inc_last_page_ofs(len);

    /* return the address */
    return addr;
}

/*
 *   Reserve space for a chunk of text, ensuring that the chunk will be
 *   stored contiguously on a single page 
 */
unsigned long CHtmlTextArray::reserve_space(size_t len)
{
    /* it must fit on a page */
    assert(len <= HTML_TEXTARRAY_PAGESIZE);

    /*
     *   We're required to keep each text block contiguous, so make sure
     *   we have room for this entire block in the current page; if not,
     *   we need to move on to the next page.  
     */
    if (len > HTML_TEXTARRAY_PAGESIZE - last_page_ofs())
    {
        /* make sure we have room in the top-level page array */
        if (pages_alloced_ == page_entries_)
        {
            /* allocate more entries */
            page_entries_ += 128;
            pages_ = (CHtmlTextArrayEntry *)th_realloc(pages_,
                page_entries_ * sizeof(CHtmlTextArrayEntry));
            assert(pages_ != 0);
        }

        /* allocate a new page */
        pages_[pages_alloced_].text_ = (textchar_t *)
                                       th_malloc(HTML_TEXTARRAY_PAGESIZE);
        pages_[pages_alloced_].used_ = 0;
        pages_[pages_alloced_].refs_ = 0;
        pages_[pages_alloced_].alloced_ = 0;
        pages_[pages_alloced_].space_in_use_ = 0;
        ++pages_alloced_;
        ++pages_in_use_;
    }

    /*
     *   Calculate the address.  Treating the pages as though they
     *   constituted a single linear array by pretending each array were
     *   allocated directly after the previous one in memory, we'd simply
     *   use the page location times the page size plus the page offset.
     *   Since the pages are of a fixed size, we can later decode that
     *   formulation to find the actual location in memory.  
     */
    return make_addr(last_page(), last_page_ofs());
}

/*
 *   Increment an offset so that it points to another valid offset 
 */
unsigned long CHtmlTextArray::inc_ofs(unsigned long addr) const
{
    size_t pg;
    size_t ofs;

    /* get the page and page offset of this address */
    pg = get_page(addr);
    ofs = get_page_ofs(addr);

    /*
     *   if the address is at the end of its page, we need to move to the
     *   next page, otherwise just go to the next offset within this page
     */
    if (ofs >= pages_[pg].alloced_)
    {
        /*
         *   if we're at the end of the whole array, return the same
         *   address 
         */
        if (pg + 1 >= pages_alloced_)
            return addr;

        /* return the first address in the next page */
        return make_addr(pg + 1, 0);
    }
    else
        return addr + 1;
}

/*
 *   Decrement an offset so that it points to another valid offset 
 */
unsigned long CHtmlTextArray::dec_ofs(unsigned long addr) const
{
    size_t pg;
    size_t ofs;

    /* get the page and page offset of this address */
    pg = get_page(addr);
    ofs = get_page_ofs(addr);

    /*
     *   if we're at the start of this page, we need to move to the
     *   previous page, otherwise just move down a character on this page
     */
    if (ofs == 0)
    {
        /* if we're at the very beginning, return the same value */
        if (addr == 0)
            return addr;
        
        /* return the last offset in the previous page */
        return make_addr(pg - 1, pages_[pg - 1].used_ - 1);
    }
    else
        return addr - 1;
}

/*
 *   Determine how many characters are between two text offsets.  Since
 *   text offsets are not assigned continuously, it is possible that the
 *   difference of the two offsets overstates the number of characters
 *   between the two offset.  
 */
unsigned long CHtmlTextArray::get_char_count(unsigned long startaddr,
                                             unsigned long endaddr) const
{
    size_t startpg, endpg, curpg;
    size_t startofs, endofs, curofs;
    unsigned long siz;
    
    /* get the page and offset of the start and end addresses */
    startpg = get_page(startaddr);
    startofs = get_page_ofs(startaddr);
    endpg = get_page(endaddr);
    endofs = get_page_ofs(endaddr);

    /* scan pages for the size */
    for (siz = 0, curpg = startpg, curofs = startofs ;; )
    {
        /*
         *   if we're on the last page, get the distance from the ending
         *   offset to the current offset, and we're done 
         */
        if (curpg >= endpg || curpg >= pages_alloced_)
        {
            siz += endofs - curofs;
            break;
        }

        /* add in the amount remaining on the current page */
        siz += pages_[curpg].alloced_;

        /* move on to the beginning of the next page */
        ++curpg;
        curofs = 0;
    }

    /* return the size we calculated */
    return siz;
}


/*
 *   Get a pointer to a chunk of characters starting at the given offset.
 *   Returns a pointer to the characters, and sets *len to the number of
 *   characters (up to a maximum of maxlen) in the chunk.  This allows a
 *   caller to traverse the possibly discontinuous array of characters.  
 */
const textchar_t *CHtmlTextArray::get_text_chunk(unsigned long *addr,
    size_t *len_in_chunk, unsigned long maxlen) const
{
    size_t pg;
    size_t ofs;

    /* get the page and offset of the given address */
    pg = get_page(*addr);
    ofs = get_page_ofs(*addr);

    /* if we're past the end of this page, move on to the next */
    if (ofs > pages_[pg].alloced_)
    {
        /* move to the start of the next page */
        ++pg;
        ofs = 0;
    }

    /* for the size, use everything on the current page, up to the limit */
    *len_in_chunk = pages_[pg].alloced_ - ofs;
    if (*len_in_chunk > maxlen)
    {
        /* limit the size */
        *len_in_chunk = maxlen;

        /*
         *   we'll still have text left in this page after returning this
         *   chunk -- the next address will still be in this page 
         */
        *addr = make_addr(pg, ofs + maxlen);
    }
    else
    {
        /*
         *   we exhausted this page with this chunk -- the next address
         *   will be at the start of the next page 
         */
        *addr = make_addr(pg + 1, 0);
    }

    /* return the pointer to the page at the appropriate offset */
    return pages_[pg].text_ + ofs;
}

/*
 *   Search for text 
 */
int CHtmlTextArray::search(const textchar_t *txt, size_t txtlen,
                           int exact_case, int wrap, int dir,
                           unsigned long startofs,
                           unsigned long *match_start,
                           unsigned long *match_end)
{
    const textchar_t *cur_p;
    const textchar_t *cur_start;
    size_t cur_rem;
    size_t cur_pg;
    size_t cur_ofs;
    size_t start_pg;
    size_t start_ofs;

    /* get the page and offset of the starting address */
    start_pg = cur_pg = get_page(startofs);
    start_ofs = cur_ofs = get_page_ofs(startofs);
    cur_rem = pages_[cur_pg].used_ - cur_ofs;
    cur_p = cur_start = pages_[cur_pg].text_ + cur_ofs;

    /* 
     *   if this page has been deleted, set the remaining amount on the page
     *   to zero so that we know we have no text to check here 
     */
    if (pages_[cur_pg].text_ == 0)
        cur_rem = cur_ofs = 0;

    /* keep going until we run out of text */
    for (;;)
    {
        size_t src_rem;
        const textchar_t *src_p;
        const textchar_t *arr_p;
        size_t arr_rem;
        size_t arr_pg;
        size_t arr_ofs;

        /* if we're going backwards, decrement first */
        if (dir == -1)
        {
            /*
             *   Move back to the previous character on this page.  If
             *   we're already at the start of the page, move to the
             *   previous page.  
             */
            if (cur_ofs == 0)
            {
                /* go back to the last character of the previous page */
                if (cur_pg == 0)
                {
                    if (wrap)
                    {
                        /* go back to the end of the buffer */
                        cur_pg = pages_alloced_;
                    }
                    else
                    {
                        /* 
                         *   we've reached the start of the buffer without
                         *   finding the target, and wrapping is not
                         *   allowed, so the search has failed 
                         */
                        return FALSE;
                    }
                }

                /* set up at the end of the previous page */
                --cur_pg;
                cur_p = pages_[cur_pg].text_ + pages_[cur_pg].used_ - 1;
                cur_rem = 1;
                cur_ofs = pages_[cur_pg].used_ - 1;

                /* if the page has been deleted, there's no text to check */
                if (pages_[cur_pg].text_ == 0)
                    cur_rem = cur_ofs = 0;
            }
            else
            {
                /* back up a character on the current page */
                --cur_ofs;
                --cur_p;
                ++cur_rem;
            }
        }

        /* set up with the current array page */
        arr_p = cur_p;
        arr_rem = cur_rem;
        arr_pg = cur_pg;
        arr_ofs = cur_ofs;

        /*
         *   We may need to do the comparison in chunks, because the
         *   current page may only hold a portion of the length of the
         *   target string.  Keep scanning on subsequent pages as
         *   necessary. 
         */
        for (src_rem = txtlen, src_p = txt ;; )
        {
            size_t cur_comp_len;
            
            /* determine how much we can compare on the current page */
            cur_comp_len = src_rem;
            if (cur_comp_len > arr_rem)
                cur_comp_len = arr_rem;

            /* see if this one matches - if not, stop looking */
            if ((exact_case
                 ? memcmp(src_p, arr_p, cur_comp_len)
                 : memicmp(src_p, arr_p, cur_comp_len)) != 0)
                break;

            /* advance our remainder pointers */
            src_rem -= cur_comp_len;
            src_p += cur_comp_len;

            /* if we're out of source, we have a match */
            if (src_rem == 0)
            {
                *match_start = make_addr(cur_pg, cur_ofs);
                *match_end = make_addr(arr_pg, arr_ofs + cur_comp_len);
                return TRUE;
            }

            /* 
             *   Advance to the next page.  If there isn't another page,
             *   we can stop searching, because we don't have enough text
             *   left to contain the target.  
             */
            ++arr_pg;
            if (arr_pg >= pages_alloced_)
                break;

            /* set up at the start of the next page */
            arr_p = pages_[arr_pg].text_;
            arr_rem = pages_[arr_pg].used_;
            arr_ofs = 0;

            /* if the page has been deleted, there's no text to check */
            if (pages_[cur_pg].text_ == 0)
                arr_rem = 0;
        }

        /* if we're going forwards, increment after an unsuccessful match */
        if (dir == 1)
        {
            /* move ahead to the next character on this page, if possible */
            if (cur_rem != 0)
            {
                ++cur_ofs;
                ++cur_p;
                --cur_rem;
            }

            /*   
             *   If we've run out of text on the current page, move on to
             *   the next page.  
             */
            if (cur_rem == 0)
            {
                /* move to the start of the next page */
                ++cur_pg;
                
                /* if there isn't another page, we can't go any further */
                if (cur_pg >= pages_alloced_)
                {
                    /* 
                     *   if they want us to wrap, go back to the start of
                     *   the buffer 
                     */
                    if (wrap)
                    {
                        /* go back to the start of the buffer */
                        cur_pg = 0;
                    }
                    else
                    {
                        /* 
                         *   we've reached the end of the buffer without
                         *   finding the target, and wrapping is not
                         *   allowed, so the search has failed 
                         */
                        return FALSE;
                    }
                }
                
                /* set up at the start of this page */
                cur_p = pages_[cur_pg].text_;
                cur_rem = pages_[cur_pg].used_;
                cur_ofs = 0;

                /* if the page has been deleted, there's no text to check */
                if (pages_[cur_pg].text_ == 0)
                    cur_rem = cur_ofs = 0;
            }
        }

        /*
         *   If we're back where we started, we must have wrapped around
         *   without finding anything, in which case the search has failed 
         */
        if (cur_pg == start_pg && cur_ofs == start_ofs)
            return FALSE;
    }
}

