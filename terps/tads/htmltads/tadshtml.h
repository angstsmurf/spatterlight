/* $Header: d:/cvsroot/tads/html/tadshtml.h,v 1.3 1999/07/11 00:46:41 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadshtml.h - definitions for TADS HTML renderer
Function
  
Notes
  
Modified
  08/23/97 MJRoberts  - Creation
*/

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>


#ifndef TADSHTML_H
#define TADSHTML_H

/* include the TADS OS header, to allow for global OS-specific definitions */
#include "os.h"


/*
 *   Debug support 
 */
//#define TADSHTML_DEBUG
#ifdef TADSHTML_DEBUG
#define HTML_IF_DEBUG(x) x
#else
#define HTML_IF_DEBUG(x)
#endif

/* ------------------------------------------------------------------------ */
/*
 *   Define some basic types 
 */

/*
 *   Text character type.  We define this as an explicit type to allow for
 *   eventual migration to Unicode or another more readily
 *   internationalizable (than plain old 'char') character representation.
 */
typedef char textchar_t;


/* ------------------------------------------------------------------------ */
/*
 *   Include the HTML TADS OS supplement.  (We make a point of including this
 *   here, rather than earlier, because it might depend on the basic types we
 *   defined above.)  
 */
#include "html_os.h"

/* ------------------------------------------------------------------------ */
/*
 *   Debugging covers for standard memory allocation routines.  These
 *   covers allow us to track allocations and deallocations so that we can
 *   ensure that we don't leak any memory.  
 */

/* 
 *   Debug console print routine.  These print messages to a suitable
 *   console window.  (These functions are implemented in the portable
 *   code, and call routines provided by the CHtmlSysFrame interface
 *   object.)  
 */
void oshtml_dbg_printf(const char *fmt, ...);
void oshtml_dbg_vprintf(const char *fmt, va_list argptr);

#ifdef TADSHTML_DEBUG

#include <stdarg.h>

void *th_malloc(size_t siz);
void *th_realloc(void *oldptr, size_t siz);
void  th_free(void *ptr);

void *operator new(size_t siz);
void *operator new[](size_t siz);
void operator delete(void *ptr);
void operator delete[](void *ptr);

/*
 *   List all allocated memory blocks - displays heap information on
 *   stdout.  This can be called at program termination to detect unfreed
 *   memory blocks, the existence of which could indicate a memory leak.  
 */
void th_list_memory_blocks();

/*
 *   List all allocated memory blocks in the external subsystem.  This must
 *   be redirected in the tads 2 or tads 3 implementation to use the memory
 *   lister for that subsystem.  
 */
void th_list_subsys_memory_blocks();

#else /* TADSHTML_DEBUG */

/* non-debug version - use standard library routines */

#define th_malloc(siz) malloc(siz)
#define th_realloc(ptr, siz) realloc(ptr, siz)
#define th_free(ptr) free(ptr)

#endif /* TADSHTML_DEBUG */

/* ------------------------------------------------------------------------ */
/*
 *   cast a textchar_t to an int - this can be used to get a "wide"
 *   representation of a character. 
 */
inline int textchar_to_int(textchar_t c)
{
    /* 
     *   cast it by way of unsigned char, to ensure that we don't
     *   sign-extend 8-bit values 
     */
    return (int)(unsigned char)c;
}

/*
 *   Character classification functions.  We'll use ctype.h for now, but
 *   we will want to replace these in the Unicode future.  
 */
inline int is_alpha(textchar_t c)
    { return ((unsigned char)c < 127 && isalpha(c)); }
inline int is_space(textchar_t c)
    { return ((unsigned char)c < 127 && isspace(c)); }
inline int is_newline(textchar_t c) { return (c == '\n' || c == '\r'); }
inline int is_digit(textchar_t c)
    { return ((unsigned char)c < 127 && isdigit(c)); }
inline int is_hex_digit(textchar_t c)
    { return ((unsigned char)c < 127 && isxdigit(c)); }
inline int is_upper(textchar_t c)
    { return ((unsigned char)c < 127 && isupper(c)); }
inline int is_lower(textchar_t c)
    { return ((unsigned char)c < 127 && islower(c)); }
inline textchar_t to_upper(textchar_t c)
    { return (unsigned char)c >= 127 ? c : (textchar_t)toupper(c); }
inline textchar_t to_lower(textchar_t c)
    { return (unsigned char)c >= 127 ? c : (textchar_t)tolower(c); }

inline int ascii_digit_to_int(textchar_t c) { return c - '0'; }
inline int ascii_hex_to_int(textchar_t c)
{
    return (c >= 'a' && c <= 'f' ? c - 'a' + 10
                                 : (c >= 'A' && c <= 'F' ? c - 'A' + 10
        : ascii_digit_to_int(c)));
}

inline textchar_t int_to_ascii_digit(int d) { return (textchar_t)(d + '0'); }
inline textchar_t int_to_hex_digit(int d, int caps)
{
    return (textchar_t)(d < 10 ? 'd' + '0' : 'd' + (caps ? 'A' : 'a'));
}

/*
 *   Copy a null-terminated string.  It's just strlen for now, but we'll
 *   want to replace this with an appropriate Unicode function in the
 *   future. 
 */
inline void do_strcpy(textchar_t *dst, const textchar_t *src)
    { strcpy(dst, src); }

/*
 *   Get length of a null-terminated string.  It's just strlen for now,
 *   but we will want to replace this in the Unicode future.  
 */
inline size_t get_strlen(const textchar_t *str) { return strlen(str); }

/*
 *   Compare two strings 
 */
inline size_t get_strcmp(const textchar_t *a, const textchar_t *b)
    { return strcmp(a, b); }

/*
 *   case insensitive string comparison 
 */
inline size_t get_stricmp(const textchar_t *a, const textchar_t *b)
    { return stricmp(a, b); }

/*
 *   Convert a string to a number.  It's just atol for now, but we will
 *   want to replace it for a future Unicode version.  
 */
inline long get_atol(const textchar_t *str) { return atol(str); }


/*
 *   TRUE and FALSE 
 */
#ifndef FALSE
# define FALSE 0
#endif
#ifndef TRUE
# define TRUE 1
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Flat color type -- stores an RGB value in a long.
 */
typedef long HTML_color_t;

/* get the red/green/blue elements out of a color_t */
#define HTML_color_red(c)   ((unsigned char)(((c) >> 16) & 0xff))
#define HTML_color_green(c) ((unsigned char)(((c) >> 8) & 0xff))
#define HTML_color_blue(c)  ((unsigned char)((c) & 0xff))

/* make an HTML_color_t out of a red, green, and blue triplet */
#define HTML_make_color(r, g, b) \
    ((((unsigned long)((r) & 0xff)) << 16) \
     | (((unsigned long)((g) & 0xff)) << 8) \
     | ((unsigned long)((b) & 0xff)))

/* ------------------------------------------------------------------------ */
/* 
 *   resource types 
 */
enum HTML_res_type_t
{
    HTML_res_type_unknown,                         /* invalid resource type */
    HTML_res_type_JPEG,                                       /* JPEG image */
    HTML_res_type_PNG,                                         /* PNG image */
    HTML_res_type_WAV,                                         /* WAV audio */
    HTML_res_type_MP123,                /* MPEG 2.0 audio layer 1, 2, and 3 */
    HTML_res_type_MIDI,                                       /* MIDI audio */
    HTML_res_type_OGG,                                  /* Ogg Vorbis audio */
    HTML_res_type_MNG                                 /* MNG animated image */
};


/* ------------------------------------------------------------------------ */
/*
 *   Resource loader function type.  This is the interface to the loader
 *   function for a resource type; this function loads the resource from a
 *   file and creates a type-specific and system-dependent C++ object that
 *   handles the resource.  The C++ object is a descendant of
 *   CHtmlSysResource (see htmlsys.h).  
 *   
 *   Note that 'filesize' is the size of the data starting at 'seekpos'
 *   that the resource loader considers to be part of this resource; the
 *   actual file may be larger than this value, since other data may
 *   follow the resource in the file.
 *   
 *   'url' is provided for diagnostic purposes; it should be used in any
 *   error message that this routine generates (with oshtml_dbg_printf).
 *   It can otherwise be ignored.
 *   
 *   The 'win' argument is provided so that the loader can check the
 *   current environment for any information necessary for loading the
 *   resource.  For example, an image loader may want to call
 *   win->get_use_palette() to determine if the image's color map must be
 *   mapped to a palette.  
 */
typedef class CHtmlSysResource
   *(*htmlres_loader_func_t)(const class CHtmlUrl *, const textchar_t *,
                             unsigned long, unsigned long,
                             class CHtmlSysWin *);


/* ------------------------------------------------------------------------ */
/*
 *   Resource type sensing.  We determine the type of an resource based on
 *   the file extension in its URL.  The file extension is the part of the
 *   URL after the last period after the last slash (forward or backslash)
 *   or colon.  
 */

class CHtmlResType
{
public:
    /* determine the type of a resource given its URL */
    static HTML_res_type_t get_res_type(const textchar_t *url);

    /* get the full mapping information for a resource */
    static HTML_res_type_t
        get_res_mapping(const textchar_t *url,
                        htmlres_loader_func_t *loader_func);

    /* add a resource mapping */
    static void add_res_type(const textchar_t *suffix,
                             HTML_res_type_t res_type,
                             htmlres_loader_func_t create_res_obj);

    /* 
     *   Add the basic resource types - this adds mappings to the system
     *   resource table for all of the built-in resource types (JPEG, PNG,
     *   etc).  This must be called once during initialization.  
     */
    static void add_basic_types();

    /* 
     *   delete the resource table - this can be called during program
     *   termination to ensure that the memory associated with the global
     *   resource table is freed before the program exits 
     */
    static void delete_res_table();

private:
    /* resource type mapping table */
    static struct ext_map_t *ext_table_;

    /* number of entries in the mapping table */
    static size_t ext_table_cnt_;

    /* total number of slots (not necessarily all in use) in the table */
    static size_t ext_table_size_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Standard cursor types.  
 */

enum Html_csrtype_t
{
    HTML_CSRTYPE_ARROW,                            /* standard arrow cursor */
    HTML_CSRTYPE_IBEAM,                               /* text I-beam cursor */
    HTML_CSRTYPE_HAND        /* hand with pointy finger ("click to select") */
};


/* ------------------------------------------------------------------------ */
/*
 *   Geometry convenience types 
 */

/* an x,y location */
struct CHtmlPoint
{
    CHtmlPoint() : x(0), y(0) { }
    CHtmlPoint(long x, long y) : x(x), y(y) { }
    void set(long x, long y) { this->x = x; this->y = y; }
    long x;
    long y;
};

/* bounds of a rectangle */
class CHtmlRect
{
public:
    CHtmlRect() : left(0), top(0), right(0), bottom(0) { }
    CHtmlRect(long left, long top, long right, long bottom)
        : left(left), top(top), right(right), bottom(bottom) { }
    void set(long left, long top, long right, long bottom)
        { this->left = left; this->top = top;
          this->right = right; this->bottom = bottom; }
    void set(const CHtmlRect *rect)
        { set(rect->left, rect->top, rect->right, rect->bottom); }
    void offset(long x, long y)
        { left += x; right += x; top += y; bottom += y; }

    /* see if the rectangle contains a given point */
    int contains(CHtmlPoint pt) const
    {
        return (pt.x >= left && pt.x < right
                && pt.y >= top && pt.y < bottom);
    }

    long left;
    long top;
    long right;
    long bottom;
};


/* ------------------------------------------------------------------------ */
/*
 *   Types for <AREA> tag COORD attributes
 */

/* maximum number of coordinate scalars in a coordinate list */
const int CHtmlTagAREA_max_coords = 20;

/* coordinates structure */
struct CHtmlTagAREA_coords_t
{
    /* value of this scalar in the list */
    long val_;

    /* flag indicating it's a percentage */
    int pct_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Hyperlink styles.
 */

/*
 *   Plain link style: show the link as though it's ordinary text, rather
 *   than using a distinctive link appearance. 
 */
#define LINK_STYLE_PLAIN   0

/*
 *   Normal link style: show the link using the standard appearance, but
 *   show it as plain text if links are globally disabled. 
 */
#define LINK_STYLE_NORMAL  1

/*
 *   Forced link style: show the link using the distinctive hyperlink
 *   appearance, even if link visibility is currently disabled globally.
 *   This can be used for special links that are to be visible and active
 *   even when ordinary links are not being shown.  
 */
#define LINK_STYLE_FORCED  2

/*
 *   "Hidden" link style: show the link as ordinary text except when the
 *   mouse is hovering over the link, in which case we'll show it as a link.
 */
#define LINK_STYLE_HIDDEN  3


/* ------------------------------------------------------------------------ */
/*
 *   Text stream buffer.  This buffer accumulates output for eventual
 *   submission to the parser.  
 */

/* initial allocation size - the buffer starts at this size */
const int init_alloc_unit = 16*1024;

/*
 *   incremental allocation size - we'll extend the buffer by this amount
 *   each time we run out of space 
 */
const int extra_alloc_unit = 16*1024;

class CHtmlTextBuffer
{
public:
    CHtmlTextBuffer()
    {
        buf_ = 0;
        bufend_ = 0;
        bufalo_ = 0;
    }

    ~CHtmlTextBuffer()
    {
        if (buf_)
        {
            th_free(buf_);
            buf_ = 0;
        }
    }

    /* clear the buffer of all text */
    void clear() { bufend_ = buf_; }

    /* add text to end of the buffer */
    void append(const textchar_t *txt, size_t len);

    /* get the buffer pointer */
    const textchar_t *getbuf() const { return buf_; }

    /* get the number of characters in the buffer */
    size_t getlen() const { return bufend_ - buf_; }

    /* change the length of the buffer */
    void setlen(size_t newlen) { bufend_ = buf_ + newlen; }

private:
    /* pointer to the buffer */
    textchar_t *buf_;

    /* allocated size of the buffer */
    size_t bufalo_;

    /* pointer to the next available free character position in the buffer */
    textchar_t *bufend_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Simple string buffer class, to simplify string storage management 
 */
class CStringBuf
{
public:
    CStringBuf() { buf_ = 0; len_ = 0; }
    CStringBuf(const textchar_t *str) { init(str, get_strlen(str)); }
    CStringBuf(const textchar_t *str, size_t len) { init(str, len); }
    CStringBuf(size_t len) { alloc(len); buf_[0] = '\0'; }

    ~CStringBuf()
    {
        if (buf_) delete [] buf_;
    }

    void set(const CStringBuf *str) { set(str->get()); }
    void set(const textchar_t *str)
        { set(str, str == 0 ? 0 : get_strlen(str)); }
    void set(const textchar_t *str, size_t len)
    {
        if (len_ < len + 1)
        {
            clear();
            init(str, len);
        }
        else
            store(str, len, 0);
    }

    textchar_t *get() const { return buf_; }

    void append(CStringBuf *str) { append(str->get()); }
    void append(const textchar_t *str) { append(str, get_strlen(str)); }
    void append(const textchar_t *str, size_t len)
    {
        size_t newlen;
        size_t oldlen = (buf_ == 0 ? 0 : get_strlen(buf_));

        newlen = oldlen + len + 1;
        if (len_ < newlen)
        {
            /* reallocate the buffer to be big enough */
            realloc(newlen);
        }

        /* add the new string at the end of the old one */
        store(str, len, oldlen);
    }

    /* 
     *   append, increasing the buffer size by the given increment (rather
     *   than just enough to add the string) if more space is needed 
     */
    void append_inc(const textchar_t *str, size_t alloc_len)
        { append_inc(str, str == 0 ? 0 : get_strlen(str), alloc_len); }

    void append_inc(const textchar_t *str, size_t len, size_t alloc_inc)
    {
        size_t newlen;
        size_t oldlen = (buf_ == 0 ? 0 : get_strlen(buf_));

        /* determine the amount of space we need to add the new string */
        newlen = oldlen + len + 1;
        if (len_ < newlen)
        {
            /* 
             *   increase the space by the increment, if the increment is
             *   larger than the string we're adding 
             */
            if (len_ + alloc_inc > newlen)
                newlen = len_ + alloc_inc;

            /* reallocate the buffer to be big enough */
            realloc(newlen);
        }

        /* add the new string at the end of the old one */
        store(str, len, oldlen);
    }

    void clear()
    {
        len_ = 0;
        if (buf_) delete [] buf_;
        buf_ = 0;
    }

    /* ensure that our buffer is at least as large as requested */
    void ensure_size(size_t min_len)
    {
        /* if we're smaller than the requested size, reallocate */
        if (len_ < min_len + 1)
            realloc(min_len + 1);
    }

private:
    void init(const textchar_t *str, size_t len)
    {
        alloc(len + 1);
        store(str, len, 0);
    }

    void alloc(size_t len)
    {
        len_ = len;
        buf_ = new textchar_t[len_];
        buf_[0] = 0;
    }

    void realloc(size_t newlen)
    {
        textchar_t *newbuf;

        /* allocate a new buffer */
        newbuf = new textchar_t[newlen];

        /* if we already had a bufer, copy its contents to the new buffer */
        if (buf_ != 0)
        {
            /* copy the old buffer contents into the new buffer */
            memcpy(newbuf, buf_, len_ * sizeof(textchar_t));

            /* drop the old buffer */
            delete [] buf_;
        }

        /* remember the new buffer and size */
        buf_ = newbuf;
        len_ = newlen;
    }

    void store(const textchar_t *str, size_t len, size_t offset)
    {
        memcpy(buf_ + offset, str, len * sizeof(textchar_t));
        buf_[offset + len] = 0;
    }

    textchar_t *buf_;
    size_t len_;
};


/* ------------------------------------------------------------------------ */
/*
 *   service class to save position in a counted-length string 
 */
class CCntlenStrPtrSaver
{
public:
    void save(const textchar_t *ptr, size_t len)
    {
        ptr_ = ptr;
        len_ = len;
    }
    void restore(const textchar_t **ptr, size_t *len)
    {
        *ptr = ptr_;
        *len = len_;
    }

private:
    const textchar_t *ptr_;
    size_t len_;
};

/*
 *   Counted-length string pointer.
 *   
 *   For the most part, this class operates in bytes; some methods
 *   specifically operate on multi-byte characters using the underlying
 *   OS-level support.  This design is compatible with any MBCS's that
 *   represent their ASCII subset as single-byte characters, which is the
 *   case for most widely-used MBCS's.   
 */
class CCntlenStrPtr
{
public:
    CCntlenStrPtr() { set(0, 0); }
    CCntlenStrPtr(const textchar_t *ptr, size_t len) { set(ptr, len); }
    CCntlenStrPtr(const CCntlenStrPtr *p) { set(p); }

    /* set up to point to a buffer */
    void set(const textchar_t *ptr, size_t len) { ptr_ = ptr; len_ = len; }
    void set(const CCntlenStrPtr *p) { set(p->ptr_, p->len_); }

    /* get the length and the character at the current position */
    textchar_t curchar() const { return *ptr_; }
    textchar_t nextchar() const;
    size_t getlen() const { return len_; }

    /* get a pointer to the text from this point forward */
    const textchar_t *gettext() const { return ptr_; }

    /* increment and decrement the buffer pointer */
    void inc() { ++ptr_; --len_; }
    void inc(size_t amt) { ptr_ += amt; len_ -= amt; }
    void dec() { --ptr_; ++len_; }
    void dec(size_t amt) { ptr_ -= amt; len_ += amt; }

    /*
     *   Increment the buffer pointer by a whole character.  This will
     *   properly handle any multi-byte characters using OS-level support.  
     */
    void inc_char(oshtml_charset_id_t charset)
    {
        /* 
         *   ask the OS to traverse the string to the next character (this
         *   will properly handle any multi-byte characters, according to the
         *   local OS conventions) 
         */
        const textchar_t *nxt = os_next_char(charset, ptr_);

        /* adjust the length for the pointer change */
        len_ -= (nxt - ptr_);

        /* set the pointer to the next character */
        ptr_ = nxt;
    }

    /* 
     *   get the number of bytes that the current character uses, taking into
     *   account multi-byte character sets 
     */
    size_t char_bytes(oshtml_charset_id_t charset)
    {
        /* 
         *   find the next character, and return the difference of the
         *   pointer to the next character and the pointer to the current
         *   character 
         */
        return os_next_char(charset, ptr_) - ptr_;
    }
    
    /* skip a newline sequence */
    void skip_newline()
    {
        /* remember the first character */
        textchar_t c = curchar();

        /* skip the first character */
        inc();

        /* if the second character is the other or (\n|\r), skip it, too */
        if (len_ != 0 && ((c == '\n' && curchar() == '\r')
                          || (c == '\r' && curchar() == '\n')))
            inc();
    }

    /* save/restore position */
    void save_position(CCntlenStrPtrSaver *saver)
    {
        saver->save(ptr_, len_);
    }
    void restore_position(CCntlenStrPtrSaver *saver)
    {
        saver->restore(&ptr_, &len_);
    }

private:
    const textchar_t *ptr_;
    size_t len_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Generic iterator callback object.  This is an abstract interface that
 *   can be implemented as needed to provide callback functions in contexts
 *   where we need to iterate over a set of elements.  
 */
class CIterCallback
{
public:
    virtual ~CIterCallback() {}

    /* invoke the callback with the given parameter */
    virtual void invoke(void *arg) = 0;
};

/* ------------------------------------------------------------------------ */
/*
 *   A simple array list type.  We keep an underlying array of void* object
 *   pointers, automatically expanding the underlying array as needed to
 *   accomodate new elements.  
 */
class CHArrayList
{
public:
    CHArrayList() { init(16, 16); }
    CHArrayList(size_t init_cnt, size_t inc_siz) { init(init_cnt, inc_siz); }

    ~CHArrayList()
    {
        /* delete our underlying array */
        th_free(arr_);
    }

    /* clear the entire list */
    void clear() { cnt_ = 0; }

    /* truncate to the given number of elements */
    void trunc(size_t cnt)
    {
        /* reduce (but don't expand) to the given count */
        if (cnt < cnt_)
            cnt_ = cnt;
    }

    /* get the number of elements in the array */
    size_t get_count() const { return cnt_; }

    /* get the element at the given index (no error checking) */
    void *get_ele(size_t idx) const { return arr_[idx]; }

    /* find an element's index; returns -1 if not found */
    int find_ele(void *ele) const
    {
        size_t i;
        void **p;

        /* scan for the element */
        for (i = 0, p = arr_ ; i < cnt_ ; ++i, ++p)
        {
            /* if this is the element, return the index */
            if (*p == ele)
                return (int)i;
        }

        /* didn't find it */
        return -1;
    }

    /* add a new element */
    void add_ele(void *ele)
    {
        /* expand the array if necessary */
        if (cnt_ >= alloc_)
        {
            /* figure the new size */
            alloc_ += inc_siz_;

            /* allocate at the new size */
            arr_ = (void **)th_realloc(arr_, alloc_ * sizeof(arr_[0]));
        }

        /* add the new element */
        arr_[cnt_++] = ele;
    }

    /* remove one element by value; returns true if found, false if not */
    int remove_ele(void *ele)
    {
        size_t i;
        void **p;

        /* scan for the element */
        for (i = 0, p = arr_ ; i < cnt_ ; ++i, ++p)
        {
            /* if this is the element, remove it */
            if (*p == ele)
            {
                /* remove the element at this index */
                remove_ele(i);

                /* indicate that we found the element */
                return TRUE;
            }
        }

        /* we didn't find the element */
        return FALSE;
    }

    /* remove the element at the given index */
    void remove_ele(size_t idx)
    {
        void **p;

        /* move each following element down one slot */
        for (p = arr_ + idx, ++idx ; idx < cnt_ ; ++idx, ++p)
            *p = *(p + 1);

        /* reduce the in-use count */
        --cnt_;
    }

    /*
     *   Safely iterate over all elements of the list.  The callback can do
     *   anything it wants, including inserting and deleting elements in this
     *   list.  We make a snapshot of the list before starting, so we
     *   guarantee that the callback is invoked once for each object in the
     *   list when this is called, regardless of any edits to the list later
     *   on.  
     */
    void iterate(class CIterCallback *cb)
    {
        void **lst;
        void **p;
        size_t cnt;

        /* if there are no elements, there's nothing to do */
        if (cnt_ == 0)
            return;

        /* make a copy of the list */
        lst = (void **)th_malloc(cnt_ * sizeof(lst[0]));
        cnt = cnt_;

        /* copy the list */
        memcpy(lst, arr_, cnt_ * sizeof(lst[0]));

        /* iterate over our safe copy of the list */
        for (p = lst ; cnt != 0 ; --cnt, ++p)
        {
            /* invoke the callback on this element */
            cb->invoke(*p);
        }

        /* delete our copy of the list */
        th_free(lst);
    }

protected:
    /* initialize */
    void init(size_t init_cnt, size_t inc_siz)
    {
        /* allocate the array */
        arr_ = (void **)th_malloc(init_cnt * sizeof(arr_[0]));

        /* remember the current size */
        alloc_ = init_cnt;

        /* no elements are currently in use */
        cnt_ = 0;

        /* remember the increment size */
        inc_siz_ = inc_siz;
    }

    /* our array of elements */
    void **arr_;

    /* number of elements allocated */
    size_t alloc_;

    /* number of elements currently in use */
    size_t cnt_;

    /* increment size */
    size_t inc_siz_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Debugger support.  These definitions apply only to TDB
 *   implementations. 
 */

/*
 *   Source line status codes.  Each source line can have a combination of
 *   these status codes (for example, a line can be current and have a
 *   breakpoint).  
 */
const unsigned int HTMLTDB_STAT_CURLINE = 0x01;            /* current line  */
const unsigned int HTMLTDB_STAT_BP      = 0x02;    /* line has a breakpoint */
const unsigned int HTMLTDB_STAT_BP_DIS  = 0x04;   /* breakpoint is disabled */
const unsigned int HTMLTDB_STAT_BP_COND = 0x08;       /* bp has a condition */
const unsigned int HTMLTDB_STAT_CTXLINE = 0x10;     /* current context line */


#endif /* TADSHTML_H */

