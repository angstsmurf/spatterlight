/* OS-layer functions and macros.
 *
 * This file does not introduce any curses (or other screen-API)
 * dependencies; it can be used for both the interpreter as well as the
 * compiler.
 */

#ifndef OSSPATTER_H
#define OSSPATTER_H

#define USE_OS_LINEWRAP	/* tell tads not to let os do paging */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <stdarg.h>

#define TRUE 1
#define FALSE 0

#ifdef __cplusplus
extern "C" {
#endif

#include "osfrobtads.h"

#define TADS_OEM_NAME   "Mr Oizo"

/* Replace stricmp with strcasecmp */
#define strnicmp strncasecmp
#define stricmp strcasecmp

#define memicmp os_memicmp

int os_memicmp(const char *a, const char *b, int n);

#define OS_SYSTEM_NAME "Spatterlight"

#define OSFNMAX 1024

#define OSPATHCHAR '/'
#define OSPATHALT ""
#define OSPATHURL "/"
#define OSPATHSEP ':'
#define OS_NEWLINE_SEQ "\n"

#define OSPATHPWD "."

void os_put_buffer (unsigned char *buf, size_t len);
void os_get_buffer (unsigned char *buf, size_t len, size_t init);
unsigned char *os_fill_buffer (unsigned char *buf, size_t len);

/*
 *   The TADS 2 formatter passes HTML markup through to os_print, for
 *   oshtml.c to translate.  Games are still told this is a text
 *   interpreter: define OS_REPORT_HTML to tell them otherwise.
 */
#define USE_HTML

#ifndef OS_MAXWIDTH
#define OS_MAXWIDTH 2048
#endif

#define OS_ATTR_HILITE  OS_ATTR_BOLD
#define OS_ATTR_EM      OS_ATTR_ITALIC
#define OS_ATTR_STRONG  OS_ATTR_BOLD

#define OS_DECLARATIVE_TLS
#define OS_DECL_TLS(t, v) t v

int os_vasprintf(char **bufptr, const char *fmt, va_list ap);

/* The game read input or wrote text: it isn't stuck (os_break, osmisc.c) */
void osglk_not_idle(void);

/* os_sleep_ms on a Glk timer, so what was drawn is shown (osglk.c) */
void osglk_sleep_ms(unsigned long ms);

#ifdef __cplusplus
}
#endif

#endif /* OSSPATTER_H */
