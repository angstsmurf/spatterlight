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
 *   oshtml.c to translate
 */
#define USE_HTML

/*
 *   ... unless the player asked for text only (-nohtml, t23run.cpp): then
 *   the TADS 2 and T3 formatters handle the markup themselves, as in a
 *   text-only build
 */
#define os_html_target() osglk_html_target()
int osglk_html_target(void);
void osglk_set_html(int on);

#ifndef OS_MAXWIDTH
#define OS_MAXWIDTH 2048
#endif

#define OS_ATTR_HILITE  OS_ATTR_BOLD
#define OS_ATTR_EM      OS_ATTR_ITALIC
#define OS_ATTR_STRONG  OS_ATTR_BOLD

#define OS_DECLARATIVE_TLS
#define OS_DECL_TLS(t, v) t v

int os_vasprintf(char **bufptr, const char *fmt, va_list ap);

/*
 *   The clock, which under the determinism testing theme is a virtual one
 *   (osportable.cc): the time of day, local time (UTC under that theme), and the
 *   millisecond timer
 */
#define os_time_t        time_t
#define os_gmtime(t)     gmtime(t)
#define os_localtime(t)  osglk_localtime(t)
#define os_time(t)       osglk_time(t)

time_t osglk_time(time_t *t);
struct tm *osglk_localtime(const time_t *t);
int osglk_vclock(void);
long osglk_vclock_ms(void);
void osglk_vclock_advance(unsigned long ms);
void osglk_vclock_time_ns(time_t *seconds, long *nanoseconds);

/* The game read input or wrote text: it isn't stuck (os_break, osmisc.c) */
void osglk_not_idle(void);

/* os_sleep_ms on a Glk timer, so what was drawn is shown (osglk.c) */
void osglk_sleep_ms(unsigned long ms);

#ifdef __cplusplus
}
#endif

#endif /* OSSPATTER_H */
