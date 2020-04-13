
/* spatterlight-autosave.m
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

#include "fizmo.h"
#include "savegame.h"
#include "zpu.h"
#include "filesys_interface.h"
#include "glk_interface.h"
#include "glk_screen_if.h"
#include "glk_filesys_if.h"
#include "filesys.h"

#import "spatterlight-autosave.h"

static void spatterlight_library_archive(TempLibrary *library, NSCoder *encoder);
static void spatterlight_library_unarchive(TempLibrary *library, NSCoder *decoder);
static library_state_data library_state; /* used by the archive/unarchive hooks */

char autosavename[1024] = "";
char normal_start_save[1024] = ""; /* Not actually used */

/* Do an auto-save of the game state, to an iOS-appropriate location. This also saves the Glk library state.
 
	The game goes into $DOCS/autosave.glksave; the library state into $DOCS/autosave.plist. However, we do this as atomically as possible -- we write to temp files and then rename.
 
	Returns 0 to indicate that the interpreter should not exit after saving. (If Fizmo is invoked from a CGI script, it can exit after every command cycle. But we're not doing that.)
 
	This is called in the VM thread, just before setting up line input and calling glk_select(). (So no window will actually be requesting line input at this time.)
 */
int spatterlight_do_autosave() {

    //spatterlight_clear_autosave();

    if (lasteventtype == -1 || lasteventtype == evtype_Arrange)
    {
		return 0;
    }

    @autoreleasepool {
        TempLibrary *library = [[TempLibrary alloc] init];
        uint8_t *orig_pc;

        NSString *dirname = [NSString stringWithUTF8String:autosavedir];
        if (!dirname)
            return 0;
        NSString *tmpgamepath = [dirname stringByAppendingPathComponent:@"autosave-tmp.glksave"];

        strncpy(autosavename, [tmpgamepath UTF8String], sizeof autosavename);
        autosavename[sizeof autosavename-1] = 0;
        z_file *save_file = fsi->openfile(autosavename, FILETYPE_DATA, FILEACCESS_WRITE);
        if (!save_file) {
            NSLog(@"unable to create z_file!");
            return 0;
        }

        orig_pc = pc;
        pc = current_instruction_location;

        int res = save_game_to_stream(0, (active_z_story->dynamic_memory_end - z_mem + 1), save_file, false);
        /* save_file is now closed */

        pc = orig_pc;

        if (!res) {
            NSLog(@"save_game_to_stream failed!");
            return 0;
        }

        glkint_stash_library_state(&library_state);
        /* The spatterlight_library_archive hook will write out the contents of library_state. */

        NSString *tmplibpath = [dirname stringByAppendingPathComponent:@"autosave-tmp.plist"];
        [TempLibrary setExtraArchiveHook:spatterlight_library_archive];
        res = [NSKeyedArchiver archiveRootObject:library toFile:tmplibpath];
        [TempLibrary setExtraArchiveHook:nil];

        if (!res) {
            NSLog(@"library serialize failed!");
            return 0;
        }

        NSString *finalgamepath = [dirname stringByAppendingPathComponent:@"autosave.glksave"];
        NSString *finallibpath = [dirname stringByAppendingPathComponent:@"autosave.plist"];

        /* This is not really atomic, but we're already past the serious failure modes. */
        [[NSFileManager defaultManager] removeItemAtPath:finallibpath error:nil];
        [[NSFileManager defaultManager] removeItemAtPath:finalgamepath error:nil];

        res = [[NSFileManager defaultManager] moveItemAtPath:tmpgamepath toPath:finalgamepath error:nil];
        if (!res) {
            NSLog(@"could not move game autosave to final position!");
            return 0;
        }
        res = [[NSFileManager defaultManager] moveItemAtPath:tmplibpath toPath:finallibpath error:nil];
        if (!res) {
            /* We don't abort out in this case; we leave the game autosave in place by itself, which is not ideal but better than data loss. */
            NSLog(@"could not move library autosave to final position (continuing)");
        }
    }

    win_autosave(AUTOSAVE_SERIAL_VERSION); // Call window server to do its own autosave

	return 0;
}

/* The argument is actually an NSString. We retain it for the next iosglk_find_autosave() call.
 */
void spatterlight_queue_autosave(NSString *pathnameval) {
    strncpy(normal_start_save, [pathnameval UTF8String], sizeof normal_start_save);
    normal_start_save[sizeof normal_start_save-1] = 0;
}


z_file *spatterlight_find_autosave() {

	if (strcmp(normal_start_save, "")) {
        frefid_t fref = gli_new_fileref(normal_start_save, fileusage_SavedGame, 1);
		strid_t savefile = glk_stream_open_file(fref, filemode_Write, 1);
		
		if (savefile) {
			z_file *zsavefile = zfile_from_glk_strid(savefile, "Restore", FILETYPE_SAVEGAME, FILEACCESS_READ);
			if (zsavefile)
				return zsavefile;
		}
	}

    @autoreleasepool {

        NSString *dirname = [NSString stringWithUTF8String:autosavedir];
        if ((!dirname) || [dirname isEqualToString:@""])
            return nil;
        NSString *finalgamepath = [dirname stringByAppendingPathComponent:@"autosave.glksave"];

        strncpy(autosavename, [finalgamepath cStringUsingEncoding:NSUTF8StringEncoding], sizeof autosavename);
        autosavename[sizeof autosavename-1] = 0;
    }

    z_file *save_file = fsi->openfile(autosavename, FILETYPE_DATA, FILEACCESS_READ);
    return save_file;
}

/* Delete an autosaved game, if one exists.
 */
void spatterlight_clear_autosave() {

    getautosavedir(gamefilestream->filename);

    @autoreleasepool {
        NSString *dirname = [NSString stringWithUTF8String:autosavedir];
        if ((!dirname) || [dirname isEqualToString:@""])
            return;

        NSString *finalgamepath = [dirname stringByAppendingPathComponent:@"autosave.glksave"];
        NSString *finallibpath = [dirname stringByAppendingPathComponent:@"autosave.plist"];
        NSString *tmpgamepath = [dirname stringByAppendingPathComponent:@"autosave-tmp.glksave"];
        NSString *tmplibpath = [dirname stringByAppendingPathComponent:@"autosave-tmp.plist"];

        [[NSFileManager defaultManager] removeItemAtPath:tmpgamepath error:nil];
        [[NSFileManager defaultManager] removeItemAtPath:tmplibpath error:nil];
        [[NSFileManager defaultManager] removeItemAtPath:finallibpath error:nil];
        [[NSFileManager defaultManager] removeItemAtPath:finalgamepath error:nil];
    }
}

/* Restore an autosaved game, if one exists. The file argument is closed in the process.
 
	Returns 1 if a game was restored successfully, 0 if not.
 
	This is called in the VM thread, from inside fizmo_start(). glkint_open_interface() has already happened, so we're going to have to replace the initial library state with the autosaved state.
 */
int spatterlight_restore_autosave(z_file *save_file) {
	//NSLog(@"restore_autosave of file (%s)", (save_file->implementation==0) ? "glk" : "stdio");
    @autoreleasepool {
        TempLibrary *newlib = nil;
        /* A normal file must be restored with evaluate_result. An autosave file must not be. Fortunately, we've arranged things so that normal files are opened with zfile_from_glk_strid(), and autosave files with fsi->openfile(). */
        bool is_normal_file = (save_file->implementation==FILE_IMPLEMENTATION_GLK);

        int res = restore_game_from_stream(0,
                                           (uint16_t)(active_z_story->dynamic_memory_end - z_mem + 1),
                                           save_file, is_normal_file);
        /* save_file is now closed */

        if (!res) {
            NSLog(@"unable to restore autosave file.");
            return 0;
        }

        NSString *dirname = [NSString stringWithUTF8String:autosavedir];
        if (!dirname)
            return 0;
        NSString *glksavepath = [dirname stringByAppendingPathComponent:@"autosave.glksave"];
        NSString *libsavepath = [dirname stringByAppendingPathComponent:@"autosave.plist"];

        if (![[NSFileManager defaultManager] fileExistsAtPath:glksavepath])
            return 0;
        if (![[NSFileManager defaultManager] fileExistsAtPath:libsavepath]) {

            // If there is a glksave but no plist, we delete the glksave
            // to make sure it does not cause trouble later.
            NSError *error;

            if ([[NSFileManager defaultManager] isDeletableFileAtPath:glksavepath]) {
                BOOL success = [[NSFileManager defaultManager] removeItemAtPath:glksavepath error:&error];
                if (!success) {
                    NSLog(@"Error removing Glk autosave: %@", error);
                }
            }
            return 0;
        }

		[TempLibrary setExtraUnarchiveHook:spatterlight_library_unarchive];

		@try {
			newlib = [NSKeyedUnarchiver unarchiveObjectWithFile:libsavepath];
		}
		@catch (NSException *ex) {
			// leave newlib as nil
			NSLog(@"Unable to restore autosave library: %@", ex);
		}

		[TempLibrary setExtraUnarchiveHook:nil];

        if (newlib) {
            [newlib updateFromLibrary];
            glkint_recover_library_state(&library_state);
            [newlib updateFromLibraryLate];
        } else { win_reset(); exit(0); }

    }
	return 1;
}
                                  
static void spatterlight_library_archive(TempLibrary *library, NSCoder *encoder) {
	if (library_state.active) {
		//NSLog(@"### archive hook: seenheight %d, maxheight %d, curheight %d", library_state.statusseenheight, library_state.statusmaxheight, library_state.statuscurheight);
		[encoder encodeBool:YES forKey:@"fizmo_library_state"];
		[encoder encodeInt32:library_state.statusseenheight forKey:@"fizmo_statusseenheight"];
		[encoder encodeInt32:library_state.statusmaxheight forKey:@"fizmo_statusmaxheight"];
		[encoder encodeInt32:library_state.statuscurheight forKey:@"fizmo_statuscurheight"];
//        [encoder encodeInt32:library_state.activewindow forKey:@"fizmo_activewindow"];
//        [encoder encodeBool:library_state.instatuswin forKey:@"fizmo_instatuswin"];


	}
}

static void spatterlight_library_unarchive(TempLibrary *library, NSCoder *decoder) {
	if ([decoder decodeBoolForKey:@"fizmo_library_state"]) {
		library_state.active = true;
		library_state.statusseenheight = [decoder decodeInt32ForKey:@"fizmo_statusseenheight"];
		library_state.statusmaxheight = [decoder decodeInt32ForKey:@"fizmo_statusmaxheight"];
		library_state.statuscurheight = [decoder decodeInt32ForKey:@"fizmo_statuscurheight"];
//        library_state.activewindow = [decoder decodeInt32ForKey:@"fizmo_activewindow"];
//        library_state.instatuswin = [decoder decodeBoolForKey:@"fizmo_instatuswin"];

//        NSLog(@"### unarchive hook: seenheight %d, maxheight %d, curheight %d", library_state.statusseenheight, library_state.statusmaxheight, library_state.statuscurheight);
	}
}
