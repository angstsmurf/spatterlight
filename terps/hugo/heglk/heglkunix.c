/* glkstart.c: Unix-specific startup code -- sample file.
    Designed by Andrew Plotkin <erkyrath@netcom.com>
    http://www.eblong.com/zarf/glk/index.html

    This is Unix startup code for the simplest possible kind of Glk
    program -- no command-line arguments; no startup files; no nothing.

    Remember, this is a sample file. You should copy it into the Glk
    program you are compiling, and modify it to your needs. This should
    *not* be compiled into the Glk library itself.
*/

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "glk.h"
#include "glkstart.h"
#include "heheader.h"

static winid_t errorwin = NULL;

static void glkunix_startup_error (const char *fmt, ...)
  __attribute__ ((format (printf, 1, 2)));

glkunix_argumentlist_t glkunix_arguments[] = {
  { "", glkunix_arg_ValueFollows, "filename: The game file to load." },
  { NULL, glkunix_arg_End, NULL }
};

static void
glkunix_startup_error (const char *fmt, ...)
{
  va_list ap;
  char buf[1024];

  if (errorwin == NULL)
    {
      errorwin = glk_window_open (0, 0, 0, wintype_TextBuffer, 0);
      if (errorwin == NULL)
	return;
    }

  va_start (ap, fmt);
  vsnprintf (buf, sizeof buf, fmt, ap);
  va_end (ap);

  glk_set_window (errorwin);
  glk_put_string (buf);
}

int
glkunix_startup_code (glkunix_startup_t *data)
{
  char *base_name;

  garglk_set_program_name("Hugo 3.1");
  garglk_set_program_info(
	"Hugo 3.1 by Kent Tessman\n"
	"Graphics support by Simon Baldwin\n"
	"Sound support by Tor Andersson\n");

  if (data->argc != 2)
    {
      base_name = strrchr (data->argv[0], '/');
      base_name = (base_name != NULL) ? base_name + 1 : data->argv[0];
      glkunix_startup_error ("Usage: %s game-file\n", base_name);
      return FALSE;
    }

  base_name = strrchr (data->argv[1], '/');
  if (!base_name)
    base_name = strrchr (data->argv[1], '\\');
  garglk_set_story_name (base_name ? base_name + 1 : data->argv[1]);

  glkunix_set_base_file(data->argv[1]);
  game = glkunix_stream_open_pathname (data->argv[1], 0, 0);
  if (game == NULL)
    {
      glkunix_startup_error ("Error: cannot open game file `%s'\n",
			     data->argv[1]);
      return FALSE;
    }

  /* This should not happen, but handle it anyway. */
  if (errorwin != NULL)
    glk_window_close (errorwin, NULL);

  return TRUE;
}
