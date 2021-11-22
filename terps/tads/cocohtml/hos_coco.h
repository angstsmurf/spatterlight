/*
 * hos_coco.h - HTML OS definitions for Cugel/Cocoa/Mac OS X
 *
 * This file should be included ONLY by html_os.h, which serves as the
 * switchboard for OS header inclusion.  Do not include this file directly
 * from any other file.
 */

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif

#ifndef HOS_COCO
#define HOS_COCO

/*
 *   Write a debug message to the system console.  This is only used when
 *   TADSHTML_DEBUG is defined, and even then is only used to display
 *   internal debugging messages.  The system code can provide an empty
 *   implementation, if desired, and should not even need to include a
 *   definition when TADSHTML_DEBUG isn't defined when the system is
 *   compiled.  
 */
void os_dbg_sys_msg(const textchar_t *msg);

/*
 *   Character set identifier type.  The font descriptor uses this type to
 *   contain a description of the character set to use in selecting a font.
 *   On systems that use code pages or a similar mechanism, this should
 *   contain an identifier for the desired code page.  This identifier is
 *   system-specific and completely opaque to the generic code; the generic
 *   code merely stores this type of ID and passes it back to system
 *   routines.
 */

typedef enum { charset_UTF8 } oshtml_charset_id_t ;

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
 *   Callers should take care not to misuse the const removal.
 *   It's not like this is a security breach or anything; the caller can always use
 *   an ordinary type-cast if they really want to remove the const-ness.)  
 */
textchar_t *os_next_char(oshtml_charset_id_t id, const textchar_t *p);

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
 *   Get the number of timer units per second.
 *   Let's use milliseconds.
 */
#define OS_TIMER_UNITS_PER_SECOND   1000

/*
 *   Huge memory block manipulation - for use with blocks of memory over
 *   64k on 16-bit machines.  For 32-bit (and larger) architectures, huge
 *   blocks are the same as normal blocks, so these macros have trivial
 *   implementations.  
 */

#define OS_HUGEPTR(type) type *
#define os_alloc_huge(siz) th_malloc(siz)
#define os_add_huge(ptr, amt) ((ptr) + (amt))
#define os_free_huge(ptr) th_free(ptr)
#define os_align_size(siz) (((siz) + 3) & ~3)

#endif

