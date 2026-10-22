/*
 * heglkunix.c: Glk startup for the Spatterlight Hugo interpreter. Opens the
 * story file named on the command line and names the program and the story.
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "glk.h"
#include "glkstart.h"
#include "heheader.h"

#define HEGLK_STR2(x) #x
#define HEGLK_STR(x) HEGLK_STR2(x)
#define HEGLK_PROGRAM_NAME "Hugo " HEGLK_STR(HEVERSION) "." HEGLK_STR(HEREVISION)

static winid_t errorwin = NULL;

static void glkunix_startup_error(const char *fmt, ...)
    __attribute__ ((format (printf, 1, 2)));

glkunix_argumentlist_t glkunix_arguments[] = {
    { "", glkunix_arg_ValueFollows, "filename: The game file to load." },
    { NULL, glkunix_arg_End, NULL }
};

static void glkunix_startup_error(const char *fmt, ...)
{
    va_list ap;
    char buf[1024];

    if (errorwin == NULL)
    {
        errorwin = glk_window_open(0, 0, 0, wintype_TextBuffer, 0);
        if (errorwin == NULL)
            return;
    }

    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);

    glk_set_window(errorwin);
    glk_put_string(buf);
}

/* The last component of PATH, which may use / or \ as separator. */
static const char *basename_of(const char *path)
{
    const char *base = strrchr(path, '/');
    if (!base)
        base = strrchr(path, '\\');
    return base ? base + 1 : path;
}

int glkunix_startup_code(glkunix_startup_t *data)
{
    garglk_set_program_name(HEGLK_PROGRAM_NAME);
    garglk_set_program_info(
        HEGLK_PROGRAM_NAME " by Kent Tessman\n"
        "Graphics support by Simon Baldwin\n"
        "Sound support by Tor Andersson\n");

    if (data->argc != 2)
    {
        glkunix_startup_error("Usage: %s game-file\n", basename_of(data->argv[0]));
        return FALSE;
    }

    garglk_set_story_name(basename_of(data->argv[1]));

    glkunix_set_base_file(data->argv[1]);
    game = glkunix_stream_open_pathname(data->argv[1], 0, 0);
    if (game == NULL)
    {
        glkunix_startup_error("Error: cannot open game file `%s'\n", data->argv[1]);
        return FALSE;
    }

    /* This should not happen, but handle it anyway. */
    if (errorwin != NULL)
        glk_window_close(errorwin, NULL);

    return TRUE;
}
