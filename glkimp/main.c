#include "glkimp.h"


int gli_screenwidth = 80;
int gli_screenheight = 24;

#if GIDEBUG_LIBRARY_SUPPORT
int gli_debugger = FALSE;
#endif /* GIDEBUG_LIBRARY_SUPPORT */

static int inittime = FALSE;

int main(int argc, char **argv)
{
    glkunix_startup_t startdata;
    
    /* Test for compile-time errors. If one of these spouts off, you
	   must edit glk.h and recompile. */
    if (sizeof(glui32) != 4)
    {
	fprintf(stderr, "Compile-time error: glui32 is not a 32-bit value. Please fix glk.h.\n");
	return 1;
    }
    if ((glui32)(-1) < 0)
    {
	fprintf(stderr, "Compile-time error: glui32 is not unsigned. Please fix glk.h.\n");
	return 1;
    }
    
    // freopen("/tmp/glkimp.log", "w", stderr);
    // setvbuf(stderr, 0, _IOLBF, 0);
    
    /* Now some argument-parsing. This is probably going to hurt. */
    /* This hurt, so I killed it. */
    startdata.argc = argc;
    startdata.argv = malloc(argc * sizeof(char*));
    memcpy(startdata.argv, argv, argc * sizeof(char*));
    
    win_hello();
    
    gli_initialize_sound();
    
    inittime = TRUE;
    if (!glkunix_startup_code(&startdata))
	return 1;
    inittime = FALSE;
    
    /* sleep to give us time to attach a gdb process */
    if (getenv("CUGELWAIT"))
	sleep(atoi(getenv("CUGELWAIT")));
    
    glk_main();
    glk_exit();
    return 0;
}

/* This opens a file for reading or writing. (You cannot open a file
 for appending using this call.)
 
 This should be used only by glkunix_startup_code().
 */
strid_t glkunix_stream_open_pathname_gen(char *pathname, glui32 writemode,
                                         glui32 textmode, glui32 rock)
{
    if (!inittime)
        return 0;
    return gli_stream_open_pathname(pathname, (writemode != 0), (textmode != 0), rock);
}

/* This opens a file for reading. It is a less-general form of
 glkunix_stream_open_pathname_gen(), preserved for backwards
 compatibility.
 
 This should be used only by glkunix_startup_code().
 */
strid_t glkunix_stream_open_pathname(char *pathname, glui32 textmode,
                                     glui32 rock)
{
    if (!inittime)
        //return 0;
        gli_strict_warning("glkunix_stream_open_pathname called outside glkunix_startup_code");
    return gli_stream_open_pathname(pathname, FALSE, (textmode != 0), rock);
}
