#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/tadshtml.cpp,v 1.3 1999/07/11 00:46:41 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadshtml.cpp - utility functions for TADS HTML engine
Function
  
Notes
  
Modified
  08/26/97 MJRoberts  - Creation
*/

#include <stdio.h>
#include <memory.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif
#ifndef HTML_OS_H
#include "html_os.h"
#endif

/* ------------------------------------------------------------------------ */
/*
 *   Text stream buffer.  This buffer accumulates output for eventual
 *   submission to the parser.  
 */

/*
 *   Append text to the end of the buffer 
 */
void CHtmlTextBuffer::append(const textchar_t *txt, size_t len)
{
    /* make sure we have enough space allocated */
    if (buf_ == 0)
    {
        /* nothing allocated yet - allocate the initial buffer space */
        bufalo_ = init_alloc_unit;
        buf_ = (textchar_t *)th_malloc(bufalo_ * sizeof(textchar_t));

        /* nothing in the buffer yet - the end is the beginning */
        bufend_ = buf_;
    }
    else if (getlen() + len > bufalo_)
    {
        size_t curlen;

        /* remember the current length, so we can reposition bufend_ later */
        curlen = getlen();

        /* allocate the additional space */
        bufalo_ += extra_alloc_unit;
        buf_ = (textchar_t *)th_realloc(buf_, bufalo_ * sizeof(textchar_t));

        /* update the end pointer, in case the buffer moved */
        bufend_ = buf_ + curlen;
    }

    /* copy the text into the buffer */
    memcpy(bufend_, txt, len * sizeof(*txt));

    /* remember the new end pointer */
    bufend_ += len;
}

/* ------------------------------------------------------------------------ */
/*
 *   Counted-length string pointer 
 */

textchar_t CCntlenStrPtr::nextchar() const
{
    /*
     *   if we have another character after the current character, return
     *   it, otherwise return a null character 
     */
    return (textchar_t)(len_ > 1 ? *(ptr_ + 1) : 0);
}


/* ------------------------------------------------------------------------ */
/*
 *   Debugging routines for memory management
 */

#ifdef TADSHTML_DEBUG

#include <stdlib.h>
#include <malloc.h>

/*
 *   memory block prefix - each block we allocate has this prefix
 *   attached just before the pointer that we return to the program 
 */
struct mem_prefix_t
{
    long id;
    size_t siz;
    mem_prefix_t *nxt;
    mem_prefix_t *prv;
};

/* head and tail of memory allocation linked list */
static mem_prefix_t *mem_head = 0;
static mem_prefix_t *mem_tail = 0;

/*
 *   Check the integrity of the heap: traverse the entire list, and make
 *   sure the forward and backward pointers match up. 
 */
static void th_check_heap()
{
    mem_prefix_t *p;

    /* scan from the front */
    for (p = mem_head ; p != 0 ; p = p->nxt)
    {
        /* 
         *   If there's a backwards pointer, make sure it matches up.  If
         *   there's no backwards pointer, make sure we're at the head of
         *   the list.  If this is the end of the list, make sure it
         *   matches the tail pointer.  
         */
        if ((p->prv != 0 && p->prv->nxt != p)
            || (p->prv == 0 && p != mem_head)
            || (p->nxt == 0 && p != mem_tail))
            oshtml_dbg_printf("\n--- heap corrupted ---\n");
    }
}

/*
 *   Allocate a block, storing it in a doubly-linked list of blocks and
 *   giving the block a unique ID. 
 */
void *th_malloc(size_t siz)
{
    static long id;
    static int check = 0;

    mem_prefix_t *mem = (mem_prefix_t *)malloc(siz + sizeof(mem_prefix_t));
    mem->id = id++;
    mem->siz = siz;
    mem->prv = mem_tail;
    mem->nxt = 0;
    if (mem_tail != 0)
        mem_tail->nxt = mem;
    else
        mem_head = mem;
    mem_tail = mem;

    if (check)
        th_check_heap();

    return (void *)(mem + 1);
}

/*
 *   reallocate a block - to simplify, we'll allocate a new block, copy
 *   the old block up to the smaller of the two block sizes, and delete
 *   the old block 
 */
void *th_realloc(void *oldptr, size_t newsiz)
{
    void *newptr;
    size_t oldsiz;
    
    /* allocate a new block */
    newptr = th_malloc(newsiz);

    /* copy the old block into the new block */
    oldsiz = (((mem_prefix_t *)oldptr) - 1)->siz;
    memcpy(newptr, oldptr, (oldsiz <= newsiz ? oldsiz : newsiz));

    /* free the old block */
    th_free(oldptr);

    /* return the new block */
    return newptr;
}


/* free a block, removing it from the allocation block list */
void th_free(void *ptr)
{
    static int check = 0;
    static int double_check = 0;
    static int check_heap = 0;
    mem_prefix_t *mem = ((mem_prefix_t *)ptr) - 1;
    static long ckblk[] = { 0xD9D9D9D9, 0xD9D9D9D9, 0xD9D9D9D9 };

    /* check the integrity of the entire heap if desired */
    if (check_heap)
        th_check_heap();

    /* check for a pre-freed block */
    if (memcmp(mem, ckblk, sizeof(ckblk)) == 0)
    {
        oshtml_dbg_printf("\n--- memory block freed twice ---\n");
        return;
    }

    /* if desired, check to make sure the block is in our list */
    if (check)
    {
        mem_prefix_t *p;
        for (p = mem_head ; p != 0 ; p = p->nxt)
        {
            if (p == mem)
                break;
        }
        if (p == 0)
            oshtml_dbg_printf("\n--- memory block not found in th_free ---\n");
    }

    /* unlink the block from the list */
    if (mem->prv != 0)
        mem->prv->nxt = mem->nxt;
    else
        mem_head = mem->nxt;

    if (mem->nxt != 0)
        mem->nxt->prv = mem->prv;
    else
        mem_tail = mem->prv;

    /* 
     *   if we're being really cautious, check to make sure the block is
     *   no longer in the list 
     */
    if (double_check)
    {
        mem_prefix_t *p;
        for (p = mem_head ; p != 0 ; p = p->nxt)
        {
            if (p == mem)
                break;
        }
        if (p != 0)
            oshtml_dbg_printf("\n--- memory block still in list after "
                              "th_free ---\n");
    }

    /* make it obvious that the memory is invalid */
    memset(mem, 0xD9, mem->siz + sizeof(mem_prefix_t));

    /* free the memory with the system allocator */
    free((void *)mem);
}

void *operator new(size_t siz)
{
    return th_malloc(siz);
}

void operator delete(void *ptr)
{
    th_free(ptr);
}

void *operator new[](size_t siz)
{
    return th_malloc(siz);
}

void operator delete[](void *ptr)
{
    th_free(ptr);
}

/*
 *   Diagnostic routine to display the current state of the heap.  This
 *   can be called just before program exit to display any memory blocks
 *   that haven't been deleted yet; any block that is still in use just
 *   before program exit is a leaked block, so this function can be useful
 *   to help identify and remove memory leaks. 
 */
void th_list_memory_blocks()
{
    mem_prefix_t *mem;
    int cnt;
    char buf[128];

    /* display introductory message */
    os_dbg_sys_msg("\n(HTMLTADS) Memory blocks still in use:\n");

    /* display the list of undeleted memory blocks */
    for (mem = mem_head, cnt = 0 ; mem ; mem = mem->nxt, ++cnt)
    {
        sprintf(buf, "  id = %ld, siz = %d\n", mem->id, mem->siz);
        os_dbg_sys_msg(buf);
    }

    /* display totals */
    sprintf(buf, "\nTotal blocks in use: %d\n", cnt);
    os_dbg_sys_msg(buf);
}

#endif /* TADSHTML_DEBUG */

