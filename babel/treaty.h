/*  treaty.h    Header file for Treaty of Babel compliant format modules
 *  By L. Ross Raszewski
 *  Version 3b
 *
 *  This file is public domain, but please note that derived versions
 *  may not not be compliant with the Treaty of Babel.
 *
 *  It would be wise for derived works to change the value of
 *  TREATY_COMPLIANCE to reflect deviations
 */

#ifndef TREATY_H

#define TREATY_H

#define TREATY_COMPLIANCE "Treaty of Babel revision 7"
#define TREATY_VERSION "r7"

/* return codes */
#define NO_REPLY_RV                    0
#define INVALID_STORY_FILE_RV          -1
#define UNAVAILABLE_RV                 -2
#define INVALID_USAGE_RV               -3
#define INCOMPLETE_REPLY_RV            -4
#define VALID_STORY_FILE_RV            1

#define PNG_COVER_FORMAT                1
#define JPEG_COVER_FORMAT               2

/* Treaty bitmasks.  These are not required by the treaty, but are here
   as a convenience.
*/
#define TREATY_SELECTOR_INPUT           0x100
#define TREATY_SELECTOR_OUTPUT          0x200
#define TREATY_SELECTOR_NUMBER          0xFF

#define TREATY_CONTAINER_SELECTOR       0x400

/* Treaty selectors */
#define GET_HOME_PAGE_SEL                       0x201
#define GET_FORMAT_NAME_SEL                     0x202
#define GET_FILE_EXTENSIONS_SEL                 0x203
#define CLAIM_STORY_FILE_SEL                    0x104
#define GET_STORY_FILE_METADATA_EXTENT_SEL      0x105
#define GET_STORY_FILE_COVER_EXTENT_SEL         0x106
#define GET_STORY_FILE_COVER_FORMAT_SEL         0x107
#define GET_STORY_FILE_IFID_SEL                 0x308
#define GET_STORY_FILE_METADATA_SEL             0x309
#define GET_STORY_FILE_COVER_SEL                0x30A
#define GET_STORY_FILE_EXTENSION_SEL            0x30B

/* Container selectors */
#define CONTAINER_GET_STORY_FORMAT_SEL                0x710
#define CONTAINER_GET_STORY_EXTENT_SEL          0x511
#define CONTAINER_GET_STORY_FILE_SEL            0x711




/* Other magic size limits */
#define TREATY_MINIMUM_EXTENT           512


#include <limits.h>

/* 32-bit integer types */
#ifndef VAX
#if   SCHAR_MAX >= 0x7FFFFFFFL && SCHAR_MIN <= -0x7FFFFFFFL
      typedef signed char       int32;
#elif SHRT_MAX >= 0x7FFFFFFFL  && SHRT_MIN <= -0x7FFFFFFFL
      typedef signed short int  int32;
#elif INT_MAX >= 0x7FFFFFFFL   && INT_MIN <= -0x7FFFFFFFL
      typedef signed int        int32;
#elif LONG_MAX >= 0x7FFFFFFFL  && LONG_MIN <= -0x7FFFFFFFL
      typedef signed long int   int32;
#else
#error No type large enough to support 32-bit integers.
#endif
#else
      /*  VAX C does not provide these limit constants, contrary to ANSI  */
      typedef int int32;
#endif



/* Pointer to treaty function.  Treaty functions must follow this prototype */

typedef int32 (*TREATY)(int32 selector, void *, int32, void *, int32);

#endif

