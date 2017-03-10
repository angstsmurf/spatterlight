/* $Header: d:/cvsroot/tads/html/win32/hos_w32.h,v 1.2 1999/05/17 02:52:24 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  hos_w32.h - HTML OS definitions for 32-bit Windows
Function
  
Notes
  This file should be included ONLY by html_os.h, which serves as the
  switchboard for OS header inclusion.  Do not include this file directly
  from any other file.
Modified
  01/24/98 MJRoberts  - Creation
*/

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif


#ifndef HOS_W32_H
#define HOS_W32_H

/* ------------------------------------------------------------------------ */
/*
 *   System debugging console messages
 */

/*
 *   Write a debug message to the system console.  This is only used when
 *   TADSHTML_DEBUG is defined, and even then is only used to display
 *   internal debugging messages.  The system code can provide an empty
 *   implementation, if desired, and should not even need to include a
 *   definition when TADSHTML_DEBUG isn't defined when the system is
 *   compiled.  
 */
void os_dbg_sys_msg(const textchar_t *msg);


/* ------------------------------------------------------------------------ */
/*
 *   Character set identifier type.  The font descriptor uses this type to
 *   contain a description of the character set to use in selecting a font.
 *   On systems that use code pages or a similar mechanism, this should
 *   contain an identifier for the desired code page.  This identifier is
 *   system-specific and completely opaque to the generic code; the generic
 *   code merely stores this type of ID and passes it back to system
 *   routines.
 *   
 *   On Windows, we use a structure containing the xxx_CHARSET identifier
 *   along with the corresponding code page to select a font's character
 *   set.  Other systems may use a font ID or font family ID, code page
 *   number, script ID, or other system-specific type; the correct type to
 *   use depends on how the system chooses the character set to display when
 *   drawing text.  
 */
struct oshtml_charset_id_t
{
    unsigned int charset;                 /* Windows xxx_CHARSET identifier */
    unsigned int codepage;                              /* code page number */

    /* initialize, given the xxx_CHARSET ID and the code page number */
    oshtml_charset_id_t(unsigned int cs, unsigned int cp);

    /* initialize to default settings */
    oshtml_charset_id_t();

    /* same character sets? */
    int equals(oshtml_charset_id_t other)
    {
        /* if the character set ID matches, consider it a match */
        return other.charset == charset;
    }
};


/* ------------------------------------------------------------------------ */
/*
 *   Advance a character string pointer to the next character in the string.
 *   The character string is in the character set identified by 'id'.
 *   
 *   For systems that use plain ASCII or other single-byte character sets,
 *   this can simply always return (p+1).
 *   
 *   For systems that use or can use multi-byte character sets, this must
 *   advance 'p' by the number of bytes taken up by the character to which
 *   'p' points.  On some systems, single-byte and double-byte characters
 *   can be mixed within the same character set; in such cases, this
 *   function must look at the actual character that 'p' points to and
 *   figure out how big that specific character is.
 *   
 *   Note that this function takes a 'const' pointer and returns a non-const
 *   pointer to the same string, so the pointer passed is stripped of its
 *   const-ness.  This isn't great, but (short of templates) is the only
 *   convenient way to allow both const and non-const uses of this function.
 *   Callers should take care not to misuse the const removal.  (It's not
 *   like this is a security breach or anything; the caller can always use
 *   an ordinary type-cast if they really want to remove the const-ness.)  
 */
textchar_t *os_next_char(oshtml_charset_id_t id, const textchar_t *p);


/* ------------------------------------------------------------------------ */
/*
 *   System timer.  The system timer is used to measure short time
 *   differences.  The timer should provide resolution as high as
 *   possible; in practice, we'll use it to measure intervals of about a
 *   second, so a precision of about a tenth of a second is adequate.
 *   Since we only use the timer to measure intervals, the timer's zero
 *   point is unimportant (i.e., it doesn't need to relate to any
 *   particular time zone).  
 */

/* system timer datatype */
typedef unsigned long os_timer_t;

/* get the current system time value */
os_timer_t os_get_time();

/* 
 *   Get the number of timer units per second.  For Windows, the timer
 *   indicates milliseconds, so there are 1000 units per second.  
 */
#define OS_TIMER_UNITS_PER_SECOND   1000


/* ------------------------------------------------------------------------ */
/*
 *   Huge memory block manipulation - for use with blocks of memory over
 *   64k on 16-bit machines.  For 32-bit (and larger) architectures, huge
 *   blocks are the same as normal blocks, so these macros have trivial
 *   implementations.  
 */

/*
 *   Huge pointer type.  For a given datatype, this should construct an
 *   appropriate type declaration for a huge pointer to that datatype.  On
 *   win32, this just returns a plain pointer to the type.  
 */
#define OS_HUGEPTR(type) type *

/* 
 *   Allocate a huge (>64k) buffer.  On win32, we can use the normal
 *   memory allocator; on 16-bit systems, this would have to make a
 *   special OS call to do the allocation.
 *   
 *   Note that we use th_malloc(), not the plain malloc().  th_malloc() is
 *   defined in the portable HTML TADS code; it provides a debug version
 *   of malloc that we can use to track memory allocations to ensure there
 *   are no memory leaks.  All 32-bit (and larger) platforms should use
 *   this same definition for os_alloc_huge().  
 */
#define os_alloc_huge(siz) th_malloc(siz)

/*
 *   Increment a "huge" pointer by a given amount.  On win32, ordinary
 *   pointers are huge, so we just do the normal arithmetic.  On 16-bit
 *   systems, this may have to do some extra work.  
 */
#define os_add_huge(ptr, amt) ((ptr) + (amt))

/*
 *   Free a huge pointer.  On win32, we use the standard memory allocator.
 *   
 *   Note that we use th_free(), not the plain free(); see the explanation
 *   of the use of th_malloc() above.  
 */
#define os_free_huge(ptr) th_free(ptr)

/*
 *   Memory alignment: align a size to the worst-case alignment
 *   requirement for this system.  For most systems, 4-byte alignment is
 *   adequate, but some systems (such as 64-bit hardware) may have a
 *   larger worst-case alignment requirement.  
 */
#define os_align_size(siz) (((siz) + 3) & ~3)


/* ------------------------------------------------------------------------ */
/*
 *   Definitions below this point are for internal use in the Windows
 *   implementation only.  These are NOT part of the generic API, so these
 *   do NOT need to be implemented on other systems.
 *   
 *   Generic code is never allowed to call any of these routines.  
 */

/* 
 *   Get the current status of an OS_CMD_xxx command.  'typ' specifies which
 *   type we're interested in - specify this as an OSCS_xxx_ON bit.  
 */
int oss_is_cmd_event_enabled(int id, unsigned int typ);


#endif /* HOS_W32_H */
