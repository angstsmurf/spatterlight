/*----------------------------------------------------------------------*\

  glkio.c

  Glk output for Alan interpreter

\*----------------------------------------------------------------------*/

#include <stdarg.h>
#include <stdio.h>

#include "glk.h"
#include "glkio.h"
#include "glkmedia.h"

#define BUFSIZE 1024

void glkio_printf(char *fmt, ...)
{
  va_list argp;
  va_start(argp, fmt);
  if (NULL == glkMainWin)
    /* assume stdio is available in this case only */
    vprintf(fmt, argp);
  else
  {
    char buf[BUFSIZE];	/* FIXME: buf size should be foolproof */
    vsnprintf(buf, BUFSIZE, fmt, argp);
#ifdef SPATTERLIGHT
    /* The media layer gets first refusal, so that it can hold back a
       description the game repeats across a picture (see glkmedia.c) */
    if (!glkmedia_filter_output(buf))
#endif
      glk_put_string(buf);
  }
  va_end(argp);
}

