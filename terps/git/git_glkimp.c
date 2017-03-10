// git_glkimp.c -- based on git_unix.c

// unixstrt.c: Unix-specific code for Glulxe.
// Designed by Andrew Plotkin <erkyrath@eblong.com>
// http://www.eblong.com/zarf/glulx/index.html

#include "git.h"
#include <glk.h>
#include <glkstart.h> // This comes with the Glk library.

// The only command-line argument is the filename.
glkunix_argumentlist_t glkunix_arguments[] =
{
    { "", glkunix_arg_ValueFollows, "filename: The game file to load." },
    { NULL, glkunix_arg_End, NULL }
};

#define CACHE_SIZE (256 * 1024L)
#define UNDO_SIZE (512 * 1024L)

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

/* fatal_error_handler():
Display an error in the error window, and then exit.
*/
void fatalError(const char *str)
{
    winid_t win = get_error_win();
    fprintf (stderr, "*** fatal error: %s ***\n", str);
    if (win) {
	glk_set_window(win);
	glk_put_string("Fatal error: ");
	glk_put_string(str);
	glk_put_string("\n");
    }
    glk_exit();
}

// Generic loader that should work anywhere.

strid_t gStream = 0;

int glkunix_startup_code(glkunix_startup_t *data)
{
    if (data->argc <= 1)
    {
        printf ("usage: git gamefile.ulx\n");
        return 0;
    }
    gStream = glkunix_stream_open_pathname ((char*) data->argv[1], 0, 0);
    return 1;
}

void glk_main ()
{
    if (gStream == NULL)
        fatalError ("could not open game file");

    gitWithStream (gStream, CACHE_SIZE, UNDO_SIZE);
}

