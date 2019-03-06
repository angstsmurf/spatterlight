
/* fizmo-spatterlight.m
 *
 * This file is part of fizmo.
 *
 * Copyright (c) 2012 Andrew Plotkin.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "glk.h"
#include "glk_interface.h"
#include "glk_screen_if.h"
#include "glk_blorb_if.h"
#include "glk_filesys_if.h"
#include "glkstart.h" /* This comes with the Glk library. */
#include "spatterlight-autosave.h"
#include "fileref.h"

#include <interpreter/fizmo.h>
#include <interpreter/config.h>
//#include <tools/tracelog.h>
#include <tools/unused.h>

static char *init_err = NULL; /*### use this */
static char *init_err2 = NULL; /*### use this */

strid_t gamefilestream = NULL;
char *gamefilename = NULL;

glkunix_argumentlist_t glkunix_arguments[] = {
    { "", glkunix_arg_ValueFollows, "filename: The game file to load." },
    { NULL, glkunix_arg_End, NULL }
};

/* Called in the VM thread before glk_main. This sets up variables which will be used to launch the Fizmo core.
 */
int glkunix_startup_code(glkunix_startup_t *data)
{
    /* It turns out to be more convenient if we return TRUE from here, even
     when an error occurs, and display an error in glk_main(). */
    int ix;
    char *filename = NULL;
    strid_t gamefile = NULL;
    fizmo_register_filesys_interface(&glkint_filesys_interface);

#ifdef ENABLE_TRACING
    turn_on_trace();
#endif // ENABLE_TRACING

    /* Parse out the arguments. They've already been checked for validity,
     and the library-specific ones stripped out.
     As usual for Unix, the zeroth argument is the executable name. */
    for (ix=1; ix<data->argc; ix++) {
        if (filename) {
            init_err = "You must supply exactly one game file.";
            return TRUE;
        }
        filename = data->argv[ix];
    }

    if (!filename) {
        init_err = "You must supply the name of a game file.";
        return TRUE;
    }

    gamefile = glkunix_stream_open_pathname(filename, FALSE, 1);
    if (!gamefile) {
        init_err = "The game file could not be opened.";
        init_err2 = filename;
        return TRUE;
    }
    
    gamefilestream = gamefile;
    gamefilename = filename;
    
    return TRUE;
}

/* This callback has the job of finding the Glk stream for the game file
 and embedding it into a z_file object. If you pass in a z_file object,
 it uses that rather than allocating a new one.

 This should only be called once, because once you close gamefilestream,
 you can't get it back. For fizmo-glktermw, it will only be called once.
 (For other strange platforms, the equivalent interface is more
 interesting. But that's not important right now.)
 */

static z_file *spatterlight_open_game_stream(z_file *current_stream)
{
	if (gamefilestream) {
		// This is the old stream object; it's just been closed. Discard it.
		gamefilestream = nil;
	}

	// Open a new stream object.
	frefid_t fref = gli_new_fileref(gamefilename, fileusage_Data, 1);
    gamefilestream = glk_stream_open_file(fref, filemode_Read, 1);
	if (!gamefilestream)
		return nil;

    giblorb_err_t err = giblorb_set_resource_map(gamefilestream);
    if (err) {
        NSLog(@"Could not restore Blorb resource map");
    }

	// Create a z_file for the stream, or stick it into the existing z_file.
	if (!current_stream)
		current_stream = zfile_from_glk_strid(gamefilestream, "Game", FILETYPE_DATA, FILEACCESS_READ);
	else
		zfile_replace_glk_strid(current_stream, gamefilestream);

    getautosavedir(gamefilestream->filename);

	return current_stream;
}


/* Called in the VM thread. This is the VM main function.
 */
void glk_main(void)
{
	z_file *story_stream;
	z_file *autosave_stream;

	spatterlight_set_can_restart_flag(NO);

	if (init_err) {
		glkint_fatal_error_handler(init_err, NULL, NULL, FALSE, 0);
		return;
	}

    garglk_set_program_name("Fizmo");

	/* Set up all the configuration for Fizmo that we care about. */
	
	set_configuration_value("savegame-path", NULL);
	//set_configuration_value("transcript-filename", "transcript");
	set_configuration_value("savegame-default-filename", "");
	set_configuration_value("disable-stream-2-wrap", "true");
	set_configuration_value("disable-stream-2-hyphenation", "true");
	set_configuration_value("stream-2-left-margin", "0");
	set_configuration_value("max-undo-steps", "32"); //### should be able to customize this per-game
	@autoreleasepool {
        NSString *approot = [NSBundle mainBundle].resourcePath;
        NSString *locales = [approot stringByAppendingPathComponent:@"FizmoLocales"];
        set_configuration_value("i18n-search-path", (char *)locales.UTF8String);
    }

	/* Add the Spatterlight-specific autosave functions to the screen interface. */
	glkint_screen_interface.do_autosave = spatterlight_do_autosave;
	glkint_screen_interface.restore_autosave = spatterlight_restore_autosave;
	
	/* Install the interface objects. */
	fizmo_register_filesys_interface(&glkint_filesys_interface);
	fizmo_register_screen_interface(&glkint_screen_interface);
	fizmo_register_blorb_interface(&glkint_blorb_interface);
	
	/* Begin. */
	story_stream = glkint_open_interface(&spatterlight_open_game_stream);
	if (!story_stream) {
		return;
	}
	autosave_stream = spatterlight_find_autosave();
	fizmo_start(story_stream, NULL, autosave_stream);
	
	/* Dispose of the story_stream, in a way that won't conflict with the upcoming close of all Glk streams. */
	zfile_replace_glk_strid(story_stream, NULL);
	glkint_closefile(story_stream);
	
	/* Since fizmo_start() exited nicely, we're allowed to re-enter it. */
	spatterlight_set_can_restart_flag(YES);
}
