/* OS-layer functions and macros.
 *
 * This file does not introduce any curses (or other screen-API)
 * dependencies; it can be used for both the interpreter as well as the
 * compiler.
 */
#ifndef OSCOCOTADS_H
#define OSCOCOTADS_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif
    
#include "../osfrobtads.h"

/* Let the OS do the paging .. the worst named #define ever? */
#define MAC_OS

/* We don't support the Atari 2600. */
#include "osbigmem.h"

/* Provide some non-standard functions (memicmp(), etc). */
#define strnicmp strncasecmp
#define stricmp strcasecmp
int memicmp( const char* s1, const char* s2, size_t len );
size_t wcslen( const wchar_t* s );
wchar_t* wcscpy( wchar_t* dest, const wchar_t* src );

/* Our Tads OEM identifier. */
#define TADS_OEM_NAME "Tor Andersson"

/* We assume that the C-compiler is mostly ANSI compatible. */
#define OSANSI

/* Special function qualifier needed for certain types of callback
 * functions.  This is for old 16-bit systems; we don't need it and
 * define it to nothing. */
#define OS_LOADDS

/* Unices don't suffer the near/far pointers brain damage (thank God) so
 * we make this a do-nothing macro. */
#define osfar_t

/* This is used to explicitly discard computed values (some compilers
 * would otherwise give a warning like "computed value not used" in some
 * cases).  Casting to void should work on every ANSI-Compiler. */
#define DISCARD (void)

/* Copies a struct into another.  ANSI C allows the assignment operator
 * to be used with structs. */
#define OSCPYSTRUCT(x,y) ((x)=(y))

/* Link error messages into the application. */
#define ERR_LINK_MESSAGES

/* Program Exit Codes. */
#define OSEXSUCC 0 /* Successful completion. */
#define OSEXFAIL 1 /* Failure. */

/* Here we configure the osgen layer; refer to tads2/osgen3.c for more
 * information about the meaning of these macros. */
#define USE_DOSEXT
#define USE_NULLINIT
#define USE_NULLPAUSE
#define USE_NULLSTYPE
#define USE_NULL_SET_TITLE

/* System identifier and system descriptive name.  We also state
 * "Windows" since we compile and run just fine under MS Windows.  We
 * ommit "curses" in the compilers, as only the interpreter uses
 * curses. */
#define OS_SYSTEM_NAME "Coco"
#define OS_SYSTEM_LDESC "Coco (Unix/Darwin)"

/* Theoretical maximum osmalloc() size.
 * Unix systems have at least a 32-bit memory space.  Even on 64-bit
 * systems, 2^32 is a good value, so we don't bother trying to find out
 * an exact value. */
#define OSMALMAX 0xffffffffL

/* Maximum length of a filename. */
#define OSFNMAX 255

#ifndef OSPATHALT
/* Other path separator characters. */
#define OSPATHALT ""
#endif

#ifndef OSPATHURL
/* Path separator characters for URL conversions. */
#define OSPATHURL "/"
#endif

#ifndef OSPATHCHAR
/* Normal path separator character. */
#define OSPATHCHAR '/'
#endif

/* Directory separator for PATH-style environment variables. */
#define OSPATHSEP ':'

#define OS_NEWLINE_SEQ "\n"

#define OSPATHPWD "."

/* File handle structure for osfxxx functions. */
typedef FILE osfildef;

/* Directory handle for searches via os_open_dir() et al. */
typedef DIR* osdirhdl_t;
    
/* The maximum width of a line of text.
 *
 * We ignore this, but the base code needs it defined.  If the
 * interpreter is run inside a console or terminal with more columns
 * than the value defined here, weird things will happen, so we go safe
 * and use a large value. */
#define OS_MAXWIDTH 255

/* Disable the Tads swap file; computers have plenty of RAM these days.
 */
#define OS_DEFAULT_SWAP_ENABLED 0

/* TADS 2 macro/function configuration.  Modern configurations always
 * use the no-macro versions, so these definitions should always be set
 * as shown below. */
#define OS_MCM_NO_MACRO
#define ERR_NO_MACRO

/* Not really needed; just a dummy. */
#define OS_TR_USAGE "usage: tadsr [options] file"

/* TADS 2 compiler usage message.  We should actually use the name the
 * user used to invoke the compiler, but the base code doesn't provide
 * for that. */
#define OS_TC_USAGE "Usage: tadsc [options] file"

/* File timestamp type.
 *
 * Needed by the TADS 3 compiler. */
struct os_file_time_t {
	time_t t;
};

/* These values are used for the "mode" parameter of osfseek() to
 * indicate where to seek in the file. */
#define OSFSK_SET SEEK_SET /* Set position relative to the start of the file. */
#define OSFSK_CUR SEEK_CUR /* Set position relative to the current file position. */
#define OSFSK_END SEEK_END /* Set position relative to the end of the file. */


/* ============= Functions follow ================ */

/* Allocate a block of memory of the given size in bytes. */
#define osmalloc malloc

/* Free memory previously allocated with osmalloc(). */
#define osfree free

/* Reallocate memory previously allocated with osmalloc() or osrealloc(),
 * changing the block's size to the given number of bytes. */
#define osrealloc realloc

/* Open text file for reading. */
#define osfoprt(fname,typ) (fopen((fname),"r"))

/* Open text file for writing. */
#define osfopwt(fname,typ) (fopen((fname),"w"))

/* Open text file for reading and writing, keeping the file's existing
 * contents if the file already exists or creating a new file if no
 * such file exists. */
osfildef*
osfoprwt( const char* fname, os_filetype_t typ );

/* Open text file for reading/writing.  If the file already exists,
 * truncate the existing contents.  Create a new file if it doesn't
 * already exist. */
#define osfoprwtt(fname,typ) (fopen((fname),"w+"))

/* Open binary file for writing. */
#define osfopwb(fname,typ) (fopen((fname),"wb"))

/* Open source file for reading - use the appropriate text or binary
 * mode. */
#define osfoprs osfoprt

/* Open binary file for reading. */
#define osfoprb(fname,typ) (fopen((fname),"rb"))

/* Open binary file for reading/writing.  If the file already exists,
 * keep the existing contents.  Create a new file if it doesn't already
 * exist. */
osfildef*
osfoprwb( const char* fname, os_filetype_t typ );

/* Open binary file for reading/writing.  If the file already exists,
 * truncate the existing contents.  Create a new file if it doesn't
 * already exist. */
#define osfoprwtb(fname,typ) (fopen((fname),"w+b"))

/* Get a line of text from a text file. */
#define osfgets fgets

/* Write a line of text to a text file. */
#define osfputs fputs

/* Write bytes to file. */
#define osfwb(fp,buf,bufl) (fwrite((buf),(bufl),1,(fp))!=1)

/* Read bytes from file. */
#define osfrb(fp,buf,bufl) (fread((buf),(bufl),1,(fp))!=1)

/* Read bytes from file and return the number of bytes read. */
#define osfrbc(fp,buf,bufl) (fread((buf),1,(bufl),(fp)))

/* Get the current seek location in the file. */
#define osfpos ftell

/* Seek to a location in the file. */
#define osfseek fseek

/* Close a file. */
#define osfcls fclose

/* Delete a file. */
#define osfdel remove

/* Access a file - determine if the file exists.
 *
 * We map this to the access() function.  It should be available in
 * virtually every system out there, as it appears in many standards
 * (SVID, AT&T, POSIX, X/OPEN, BSD 4.3, DOS, MS Windows, maybe more). */
#define osfacc(fname) (access((fname), F_OK))

/* Get a character from a file. */
#define osfgetc fgetc

/* Set busy cursor.
 *
 * We don't have a mouse cursor so there's no need to implement this. */
#define os_csr_busy(a)

/* Update progress display.
 *
 * We don't provide any kind of "compilation progress display", so we
 * just define this as an empty macro.
 */
#define os_progress(fname,linenum)

/* Initialize the time zone.
 *
 * We don't need this (I think). */
#define os_tzset()

#define OS_ATTR_HILITE  OS_ATTR_BOLD
#define OS_ATTR_EM      OS_ATTR_ITALIC
#define OS_ATTR_STRONG  OS_ATTR_BOLD

#define OS_DECLARATIVE_TLS
#define OS_DECL_TLS(t, v) t v
    
#define OS_DEFAULT_SWAP_ENABLED   0

#ifdef __cplusplus
}
#endif

#endif /* OSCOCOTADS_H */

