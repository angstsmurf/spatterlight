/* $Header: d:/cvsroot/tads/html/htmltxar.h,v 1.2 1999/05/17 02:52:22 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmltxar.h - text array class
Function
  This class provides a mechanism for storing a long run of text
  in a linear address space.  Since we'd like to be able to tell
  whether a particular piece of text comes before or after another
  piece of text in the text stream, it's useful for each piece of
  text to have an address in a linear address space.

  Since the text stream for a document can become quite large, we
  don't actually store the text in a single large chunk of memory.
  Instead, we break the text up into pages.  To allow for linear
  addresses within these pages, we give each piece of text a
  "virtual" address, which we can then map into a pointer to the
  actual memory containing the text.

  To simplify code that accesses the text, we ensure that each
  chunk of text added to the array is stored contiguously in
  memory.  So, once the caller has obtained a pointer to the
  memory containing a chunk of text, the caller can treat the
  pointer as a simple C++ character array pointer.

  Note that the addresses that we return are not necessarily
  contiguous; that is, if you store a block of 10 characters,
  and that block is assigned address 25, the next block of
  characters will not necessarily be stored at location 35.
  Therefore, only the addresses actually returned by append_text
  can be used to retrieve text.  Therefore, "address arithmetic"
  is not possible with the values returned by append_text()
  outside of a single chunk.  However, because ordering is
  guaranteed, comparisons are legal and reliably determine
  the relative storage order of two chunks.
Notes
  
Modified
  09/23/97 MJRoberts  - Creation
*/

#ifndef HTMLTXAR_H
#define HTMLTXAR_H

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif

const size_t HTML_TEXTARRAY_PAGESIZE = 32*1024;

/* page entry */
class CHtmlTextArrayEntry
{
public:
    textchar_t *text_;                                  /* text of the page */
    size_t used_;                      /* amount of space used on this page */
    size_t alloced_;                /* amount of space reserved on the page */
    size_t refs_;                      /* number of references to this page */
    size_t space_in_use_;     /* space actually in use (counting deletions) */
};

class CHtmlTextArray
{
public:
    CHtmlTextArray();
    ~CHtmlTextArray();

    /* clear the text array -- deletes all of the text in the array */
    void clear();

    /*
     *   Determine how much memory, in bytes, the text in the array is
     *   consuming.  This measures the memory allocated to the text, hence
     *   the granularity is the size of a page.  
     */
    unsigned long get_mem_in_use() const { return mem_in_use_; }

    /*
     *   Add text to the array.  We return the linear address of the text
     *   in the buffer, which is guaranteed to be higher than that of any
     *   previously appended text.  We also guarantee that the text will
     *   be stored in a contiguous block of memory, so that subsequent
     *   uses of the text can treat it as a simple character array.  If a
     *   text item of length zero is appended, we won't actually store
     *   anything, but we will return an address that is higher than that
     *   of any text in the array.  
     */
    unsigned long append_text(const textchar_t *txt, size_t len);

    /*
     *   Add text to the array without creating a reference to the text.
     *   This should be used whenever the caller doesn't need to keep
     *   track of the text, such as when the text is added purely to
     *   signal a word or line break. 
     */
    void append_text_noref(const textchar_t *txt, size_t len)
        { store_text(txt, len); }

    /*
     *   Temporarily store text in the array without actually consuming
     *   space.  Returns the address used to store the text, which may
     *   change from the address previouly used for the same temporary
     *   storage. 
     */
    unsigned long store_text_temp(const textchar_t *txt, size_t len);

    /*
     *   Store text and commit the space. 
     */
    unsigned long store_text(const textchar_t *txt, size_t len);

    /*
     *   Delete a reference to a block of text previously allocated.
     */
    void delete_text(unsigned long addr, size_t len);

    /*
     *   Reserve space for a chunk of text, ensuring that the chunk will
     *   be stored contiguously on a single page.  (It's not necessary to
     *   call this prior to append_text, since that will ensure that the
     *   text stored is in a single chunk.  However, it is necessary to
     *   use this if you want to call append_text several times and have
     *   the whole group of text end up in a single page -- for this case,
     *   call reserve_space with the sum of the sizes of the pieces of
     *   text, then make the append_text calls.)  Returns the text offset
     *   for the start of the reserved chunk.
     */
    unsigned long reserve_space(size_t len);

    /*
     *   Get the address of a chunk of text previously allocated.  Only
     *   values returned by append_text() can be used reliably with this
     *   call; no "pointer arithmetic" is possible on the values returned
     *   by append_text() outside of a single chunk, other than comparison
     *   of addresses from any chunks to determine storage order.  
     */
    textchar_t *get_text(unsigned long linear_address) const
    {
        /* make sure it's within range */
        if (linear_address > max_addr_)
            linear_address = max_addr_;

        /* get the text at the given offset */
        return (pages_[get_page(linear_address)].text_
                + get_page_ofs(linear_address));
    }

    /* get the highest address currently in use */
    unsigned long get_max_addr() const { return max_addr_; }

    /* get the character at a given offset */
    textchar_t get_char(unsigned long addr) const { return *get_text(addr); }

    /* increment an offset so that it points to another valid offset */
    unsigned long inc_ofs(unsigned long ofs) const;

    /* decrement an offset so that it points to another valid offset */
    unsigned long dec_ofs(unsigned long ofs) const;

    /*
     *   Determine how many characters are between two text offsets.
     *   Since text offsets are not assigned continuously, it is possible
     *   that the difference of the two offsets overstates the number of
     *   characters between the two offset. 
     */
    unsigned long get_char_count(unsigned long startofs,
                                 unsigned long endofs) const;

    /*
     *   Get a pointer to a chunk of characters starting at the given
     *   offset.  Returns a pointer to the characters, sets *len_in_chunk
     *   to the number of characters (up to a maximum of maxlen) in the
     *   chunk, and advances *startofs to point to the next valid offset
     *   after the chunk returned.  This allows a caller to traverse the
     *   possibly discontinuous array of characters by calling this
     *   routine repeatedly to get chunks.  
     */
    const textchar_t *get_text_chunk(unsigned long *startofs,
                                     size_t *len_in_chunk,
                                     unsigned long maxlen) const;

    /*
     *   Find a text string.  Searches from the given starting offset to
     *   the end of the text array.  If we find the string, we'll set
     *   *match_start and *match_end to the starting and ending offsets of
     *   the match and return true; we'll return false if we can't find
     *   the string.  If exact_case is true, we'll match only if the case
     *   matches, otherwise we'll ignore case.
     *   
     *   If dir is 1, we'll search forwards; otherwise, we'll search
     *   backwards.  If wrap is true, we'll wrap around at the end (or
     *   start if going backwards) of the buffer to the opposite end of
     *   the buffer and continue the search from there; we'll only fail if
     *   we get back to the starting point and still haven't found the
     *   string.  
     */
    int search(const textchar_t *txt, size_t txtlen, int exact_case,
               int wrap, int dir, unsigned long startofs,
               unsigned long *match_start, unsigned long *match_end);

private:
    /* allocate the first page */
    void alloc_first_page();
    
    /* get the page containing an address */
    size_t get_page(unsigned long addr) const
        { return addr / HTML_TEXTARRAY_PAGESIZE; }

    /* get the offset within the page containing an address */
    size_t get_page_ofs(unsigned long addr) const
        { return addr % HTML_TEXTARRAY_PAGESIZE; }

    /* make an address out of a page number and offset */
    unsigned long make_addr(size_t pg, size_t ofs) const
        { return (((unsigned long)pg * HTML_TEXTARRAY_PAGESIZE)
                  + (unsigned long)ofs); }
    
    /* last page */
    size_t last_page() const { return pages_alloced_ - 1; }

    /* offset of next free byte on last page */
    size_t last_page_ofs() const
        { return pages_[last_page()].used_; }

    /* increase amount used on last page */
    void inc_last_page_ofs(size_t len)
        { pages_[last_page()].used_ += len; }
    
    /* array of page pointers; the pages contain the actual text */
    CHtmlTextArrayEntry *pages_;

    /* size of top-level page array (number of pointers allocated) */
    size_t page_entries_;

    /* 
     *   number of pages allocated (this is actually just a high-water
     *   mark for the number of pages *ever* allocated; pages can be
     *   deleted when they are unreferenced, hence this doesn't represent
     *   the number of pages actually present in memory, but just the
     *   number of the next slot to be filled) 
     */
    size_t pages_alloced_;

    /*
     *   Maximum address used.  This includes both storage actually
     *   committed and storage only temporarily used.
     */
    unsigned long max_addr_;

    /*
     *   Pages in use.  This represents the actual number of pages for
     *   which memory is currently allocated.  Whenever we allocate a new
     *   page, we increment this, and whenever we delete a page (because
     *   it becomes unreferenced) we decrement this. 
     */
    size_t pages_in_use_;

    /*
     *   Amount of memory currently in use.  This keeps track of
     *   allocations and deletions.  We might actually be using more
     *   system memory than this would indicate, because pages are not
     *   necessarily completely full; a partial page takes up more OS
     *   memory than this would indicate.  
     */
    unsigned long mem_in_use_;
};


#endif /* HTMLTXAR_H */

