
/* glk_interface.c
 *
 * This file is part of fizmo.
 *
 * Copyright (c) 2011-2017 Andrew Plotkin and Christoph Ender.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <strings.h>

#include "glk.h"
#include "glkstart.h" /* This comes with the Glk library. */

#include "glk_filesys_if.h"
#include "glk_screen_if.h"

#include <interpreter/fizmo.h>
#include <interpreter/text.h>
#include <interpreter/streams.h>
#include <tools/unused.h>
#include <tools/types.h>
#include <tools/i18n.h>
#include <tools/filesys.h>
#include <tools/tracelog.h>

static void stream_hexnum(glsi32 val);


/* get_error_win():
   Return a window in which to display errors. The first time this is called,
   it creates a new window; after that it returns the window it first
   created.
   */
static winid_t get_error_win()
{
  static winid_t errorwin = NULL;

  if (!errorwin) {
    winid_t rootwin = glk_window_get_root();
    if (!rootwin) {
      errorwin = glk_window_open(0, 0, 0, wintype_TextBuffer, 1);
    }
    else {
      errorwin = glk_window_open(rootwin, winmethod_Below | winmethod_Fixed, 
          3, wintype_TextBuffer, 0);
    }
    if (!errorwin)
      errorwin = rootwin;
  }

  return errorwin;
}


/* stream_hexnum():
   Write a signed integer to the current Glk output stream.
   */
static void stream_hexnum(glsi32 val)
{
  char buf[16];
  glui32 ival;
  int ix;

  if (val == 0) {
    glk_put_char('0');
    return;
  }

  if (val < 0) {
    glk_put_char('-');
    ival = -val;
  }
  else {
    ival = val;
  }

  ix = 0;
  while (ival != 0) {
    buf[ix] = (ival % 16) + '0';
    if (buf[ix] > '9')
      buf[ix] += ('A' - ('9' + 1));
    ix++;
    ival /= 16;
  }

  while (ix) {
    ix--;
    glk_put_char(buf[ix]);
  }
}


/* Write a message to the error window, and exit. */
void glkint_fatal_error_handler(char *str, glui32 *ustr, char *arg,
    int useval, glsi32 val)
{
  winid_t win = get_error_win();
  if (win) {
    glk_set_window(win);
    glk_put_string("Fizmo fatal error: ");
    if (str)
      glk_put_string(str);
    if (ustr)
      glk_put_string_uni(ustr);
    if (arg || useval) {
      glk_put_string(" (");
      if (arg)
        glk_put_string(arg);
      if (arg && useval)
        glk_put_string(" ");
      if (useval)
        stream_hexnum(val);
      glk_put_string(")");
    }
    glk_put_string("\n");
  }
  glk_exit();
}


/* Create a Fizmo file for a given Glk stream. */
z_file *zfile_from_glk_strid(strid_t str, char *filename, int filetype,
    int fileaccess)
{
  z_file *result;

  if ((result = malloc(sizeof(z_file))) == NULL)
    return NULL;

  result->file_object = str;
  result->filename = filename != NULL ? strdup(filename) : NULL;
  result->filetype = filetype;
  result->fileaccess = fileaccess;
  result->implementation = FILE_IMPLEMENTATION_GLK;

  return result;
}


/* Replace the Glk stream in an existing Fizmo file. This awkward
   maneuver is only necessary because of the way we autosave-restore
   on iOS. It's not used otherwise. */
void zfile_replace_glk_strid(z_file *result, strid_t str)
{
  result->file_object = str;
}

