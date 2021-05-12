
/* spatterlight-autosave.m
 *
 * This file is part of Spatterlight.
 *
 * Copyright (c) 2021 Petter Sjölund, adapted from code by Andrew Plotkin
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

#import <Foundation/Foundation.h>

#include "glk.h"
#include "fileref.h"

#include "stack.h"
#include "zterp.h"
#include "io.h"
#include "process.h"
#include "screen.h"

#import "TempLibrary.h"

#import "spatterlight-autosave.h"

struct zterp_io
{
  enum type
  {
    IO_STDIO,
    IO_GLK,
  } type;

  union
  {
    FILE *stdio;
    strid_t glk;
  } file;
  enum zterp_io_mode mode;
  enum zterp_io_purpose purpose;
};

static void spatterlight_library_archive(TempLibrary *library, NSCoder *encoder);
static void spatterlight_library_unarchive(TempLibrary *library, NSCoder *decoder);
static library_state_data library_state; /* used by the archive/unarchive hooks */

char autosavename[1024] = "";

/* Do an auto-save of the game state, to an macOS-appropriate location. This also saves the Glk library state.
 
	The game goes into ~Library/Application Support/Spatterlight/Bocfel Files/Autosaves/(FILE HASH)/autosave.glksave; the library state into autosave.plist. However, we do this as atomically as possible -- we write to temp files and then rename.
 
	This is called in the VM thread, just before setting up line input and calling glk_select(). (So no window will actually be requesting line input at this time.)
 */
int spatterlight_do_autosave() {

    //spatterlight_clear_autosave();

    if (!gli_enable_autosave)
        return 0;

    if ((int)lasteventtype == -1 || lasteventtype == evtype_Arrange || (lasteventtype == evtype_Timer && !gli_enable_autosave_on_timer))
    {
		return 0;
    }

    getautosavedir((char *)game_file);

    @autoreleasepool {
        TempLibrary *library = [[TempLibrary alloc] init];
        unsigned long orig_pc;

        NSString *dirname = [NSString stringWithUTF8String:autosavedir];

        if (!dirname) {
            return 0;
        }
        NSString *tmpgamepath = [dirname stringByAppendingPathComponent:@"autosave-tmp.glksave"];

        strncpy(autosavename, [tmpgamepath UTF8String], sizeof autosavename);
        autosavename[sizeof autosavename-1] = 0;
        zterp_io *save_file = zterp_io_open(autosavename, ZTERP_IO_WRONLY, ZTERP_IO_SAVE);

        if(save_file == NULL)
        {
          win_showerror("Could not open autosave file.");
          warning("unable to open save file");
          return 0;
        }

        orig_pc = pc;
        pc = current_instruction;

        bool res;

        res = save_quetzal(save_file, false, false);

        zterp_io_close(save_file);
        /* save_file is now closed */

        pc = orig_pc;

        if (!res) {
            win_showerror("Failed to autosave.");
            NSLog(@"save_game_to_stream failed!");
            return 0;
        }

        stash_library_state(&library_state);
        /* The spatterlight_library_archive hook will write out the contents of library_state. */

        NSString *tmplibpath = [dirname stringByAppendingPathComponent:@"autosave-tmp.plist"];
        [TempLibrary setExtraArchiveHook:spatterlight_library_archive];
        res = [NSKeyedArchiver archiveRootObject:library toFile:tmplibpath];
        [TempLibrary setExtraArchiveHook:nil];

        if (!res) {
            win_showerror("Could not autosave Glk objects.");
            NSLog(@"library serialize failed!");
            return 0;
        }

        NSString *finallibpath = [dirname stringByAppendingPathComponent:@"autosave.plist"];
        NSString *finalgamepath = [dirname stringByAppendingPathComponent:@"autosave.glksave"];

        NSString *oldlibpath = [dirname stringByAppendingPathComponent:@"autosave-bak.plist"];
        NSString *oldgamepath = [dirname stringByAppendingPathComponent:@"autosave-bak.glksave"];

        NSError *error = nil;
        /* This is not really atomic, but we're already past the serious failure modes. */
        [[NSFileManager defaultManager] removeItemAtPath:oldlibpath error:&error];
        [[NSFileManager defaultManager] removeItemAtPath:oldgamepath error:&error];
        [[NSFileManager defaultManager] moveItemAtPath:finallibpath toPath:oldlibpath error:&error];
        [[NSFileManager defaultManager] moveItemAtPath:finalgamepath toPath:oldgamepath error:&error];

        error = nil;
        res = [[NSFileManager defaultManager] moveItemAtPath:tmpgamepath toPath:finalgamepath error:&error];
        if (error) NSLog(@"%@", error);
        if (!res) {
            win_showerror("Could not move autosave file to final position.");
            NSLog(@"could not move game autosave to final position!");
            return 0;
        }
        res = [[NSFileManager defaultManager] moveItemAtPath:tmplibpath toPath:finallibpath error:nil];
        if (!res) {
            /* We don't abort out in this case; we leave the game autosave in place by itself, which is not ideal but better than data loss. */
            win_showerror("Could not move autosave plist to final position.");
            NSLog(@"could not move library autosave to final position (continuing)");
        }
//        NSLog(@"Bocfel created an autosave with tag %u", library.autosaveTag);
        win_autosave(library.autosaveTag); // Call window server to do its own autosave
    }
	return 0;
}

static void load_resources(void)
{
    strid_t gamefile = NULL;
    strid_t blorbfile = NULL;

    strid_t stream;

    for (stream = glk_stream_iterate(NULL, NULL); stream; stream = glk_stream_iterate(stream, NULL))
    {
        if (stream->filename != NULL && strcmp(stream->filename, game_file) == 0)
        {
            gamefile = stream;
            break;
        }
    }
    if(gamefile != NULL)
    {
        if(giblorb_set_resource_map(gamefile) == giblorb_err_None)
            return;
        gamefile = NULL;
    }
    /* 7 for the worst case of needing to add .blorb to the end plus the
     * null character.
     */
    char *filename = malloc(strlen(game_file) + 7);
    if(filename != NULL)
    {
        const char *exts[] = { ".blb", ".blorb" };

        strcpy(filename, game_file);
        for(size_t i = 0; blorbfile == NULL && i < (sizeof exts) / (sizeof *exts); i++)
        {
            char *p = strrchr(filename, '.');
            if(p != NULL) *p = 0;
            strcat(filename, exts[i]);

            for (stream = glk_stream_iterate(NULL, NULL); stream; stream = glk_stream_iterate(stream, NULL))
            {
                if (stream->filename != NULL && strcmp(stream->filename, filename) == 0)
                {
                    blorbfile = stream;
                    break;
                }
            }
        }
        free(filename);
    }

    if (blorbfile != NULL)
        giblorb_set_resource_map(blorbfile);
}

/* Restore an autosaved game, if one exists.
	Returns 1 if a game was restored successfully, 0 if not.
 */
int spatterlight_restore_autosave()
{
    if (!gli_enable_autosave)
        return 0;
    @autoreleasepool {
        getautosavedir((char *)game_file);

        NSString *dirname = [NSString stringWithUTF8String:autosavedir];
        if (!dirname.length) {
            win_showerror("Could not create autosave folder name.");
            return 0;
        }
        NSString *finalgamepath = [dirname stringByAppendingPathComponent:@"autosave.glksave"];
        strncpy(autosavename, [finalgamepath cStringUsingEncoding:NSUTF8StringEncoding], sizeof autosavename);
        autosavename[sizeof autosavename-1] = 0;

        NSString *libsavepath = [dirname stringByAppendingPathComponent:@"autosave.plist"];

        if (![[NSFileManager defaultManager] fileExistsAtPath:finalgamepath]) {
            // No autosave.glksave exists. Do nothing.
            return 0;
        }
        if (![[NSFileManager defaultManager] fileExistsAtPath:libsavepath]) {
            // If there is a glksave but no plist, we delete the glksave
            // to make sure it does not cause trouble later.
            NSError *error;

            if ([[NSFileManager defaultManager] isDeletableFileAtPath:finalgamepath]) {
                BOOL success = [[NSFileManager defaultManager] removeItemAtPath:finalgamepath error:&error];
                if (!success) {
                    NSLog(@"Error deleting glksave: %@", error);
                }
            }
            return 0;
        }
        
        zterp_io *save_file = zterp_io_open(autosavename, ZTERP_IO_RDONLY, ZTERP_IO_DATA);

        if (save_file == NULL) {
            win_showerror("Autosave file could not be read.");
            NSLog(@"spatterlight_restore_autosave: failed to open autosave file");
            return 0;
        }

        TempLibrary *newlib = nil;

        bool is_bfms;

        int res = restore_quetzal(save_file, false, &is_bfms);
        /* save_file is now closed */

        if (!res) {
            win_showerror("Failed to restore from autosave file.");
            NSLog(@"unable to restore autosave file.");
            return 0;
        }

		[TempLibrary setExtraUnarchiveHook:spatterlight_library_unarchive];

		@try {
			newlib = [NSKeyedUnarchiver unarchiveObjectWithFile:libsavepath];
		}
		@catch (NSException *ex) {
			// leave newlib as nil
            win_showerror("Failed to restore autosaved Glk objects.");
			NSLog(@"Unable to restore autosave library: %@", ex);
		}

		[TempLibrary setExtraUnarchiveHook:nil];

        if (newlib) {
            [newlib updateFromLibrary];
            recover_library_state(&library_state);

            load_resources();

            [newlib updateFromLibraryLate];
        } else { win_reset(); exit(0); }

    }
	return 1;
}
                                  
static void spatterlight_library_archive(TempLibrary *library, NSCoder *encoder) {
	if (library_state.active) {
		[encoder encodeBool:YES forKey:@"bocfel_library_state"];
		[encoder encodeBool:library_state.headerfixedfont forKey:@"bocfel_headerfixedfont"];

        [encoder encodeInt32:library_state.wintag0 forKey:@"bocfel_win0"];
        [encoder encodeInt32:library_state.wintag1 forKey:@"bocfel_win1"];
        [encoder encodeInt32:library_state.wintag2 forKey:@"bocfel_win2"];
        [encoder encodeInt32:library_state.wintag3 forKey:@"bocfel_win3"];
        [encoder encodeInt32:library_state.wintag4 forKey:@"bocfel_win4"];
        [encoder encodeInt32:library_state.wintag5 forKey:@"bocfel_win5"];
        [encoder encodeInt32:library_state.wintag6 forKey:@"bocfel_win6"];
        [encoder encodeInt32:library_state.wintag7 forKey:@"bocfel_win7"];

        [encoder encodeInt32:library_state.curwintag forKey:@"bocfel_curwintag"];
        [encoder encodeInt32:library_state.mainwintag forKey:@"bocfel_mainwintag"];
        [encoder encodeInt32:library_state.statuswintag forKey:@"bocfel_statuswintag"];
        [encoder encodeInt32:library_state.upperwintag forKey:@"bocfel_upperwintag"];
        [encoder encodeInt32:library_state.errorwintag forKey:@"bocfel_errorwintag"];
        [encoder encodeInt32:(int32_t)library_state.upperwinheight forKey:@"bocfel_upperwinheight"];
        [encoder encodeInt32:(int32_t)library_state.upperwinwidth forKey:@"bocfel_upperwinwidth"];
        [encoder encodeInt32:(int32_t)library_state.upperwinx forKey:@"bocfel_upperwinx"];
        [encoder encodeInt32:(int32_t)library_state.upperwiny forKey:@"bocfel_upperwiny"];
        [encoder encodeInt32:(int32_t)library_state.fgcolor forKey:@"bocfel_fgcolor"];
        [encoder encodeInt32:(int32_t)library_state.bgcolor forKey:@"bocfel_bgcolor"];
        [encoder encodeInt32:(int32_t)library_state.fgmode forKey:@"bocfel_fgmode"];
        [encoder encodeInt32:(int32_t)library_state.bgmode forKey:@"bocfel_bgmode"];
        [encoder encodeInt32:(int32_t)library_state.style forKey:@"bocfel_style"];
        [encoder encodeInt32:(int32_t)library_state.random_calls_count forKey:@"bocfel_random_calls_count"];
        [encoder encodeInt32:(int32_t)library_state.routine forKey:@"bocfel_routine"];
        [encoder encodeInt32:(int32_t)library_state.queued_sound forKey:@"bocfel_next_sample"];
        [encoder encodeInt32:(int32_t)library_state.sound_channel_tag forKey:@"bocfel_sound_channel_tag"];
	}
}

static void spatterlight_library_unarchive(TempLibrary *library, NSCoder *decoder) {
	if ([decoder decodeBoolForKey:@"bocfel_library_state"]) {
		library_state.active = true;
        library_state.headerfixedfont = [decoder decodeBoolForKey:@"bocfel_headerfixedfont"];

        library_state.wintag0 = [decoder decodeInt32ForKey:@"bocfel_win0"];
        library_state.wintag1 = [decoder decodeInt32ForKey:@"bocfel_win1"];
        library_state.wintag2 = [decoder decodeInt32ForKey:@"bocfel_win2"];
        library_state.wintag3 = [decoder decodeInt32ForKey:@"bocfel_win3"];
        library_state.wintag4 = [decoder decodeInt32ForKey:@"bocfel_win4"];
        library_state.wintag5 = [decoder decodeInt32ForKey:@"bocfel_win5"];
        library_state.wintag6 = [decoder decodeInt32ForKey:@"bocfel_win6"];
        library_state.wintag7 = [decoder decodeInt32ForKey:@"bocfel_win7"];

        library_state.curwintag = [decoder decodeInt32ForKey:@"bocfel_curwintag"];
        library_state.mainwintag = [decoder decodeInt32ForKey:@"bocfel_mainwintag"];
        library_state.statuswintag = [decoder decodeInt32ForKey:@"bocfel_statuswintag"];
        library_state.upperwintag = [decoder decodeInt32ForKey:@"bocfel_upperwintag"];
        library_state.errorwintag = [decoder decodeInt32ForKey:@"bocfel_errorwintag"];

		library_state.upperwinheight = [decoder decodeInt32ForKey:@"bocfel_upperwinheight"];
		library_state.upperwinwidth = [decoder decodeInt32ForKey:@"bocfel_upperwinwidth"];
        library_state.upperwinx = [decoder decodeInt32ForKey:@"bocfel_upperwinx"];
        library_state.upperwiny = [decoder decodeInt32ForKey:@"bocfel_upperwiny"];
        library_state.fgcolor = [decoder decodeInt32ForKey:@"bocfel_fgcolor"];
        library_state.bgcolor = [decoder decodeInt32ForKey:@"bocfel_bgcolor"];
        library_state.fgmode = [decoder decodeInt32ForKey:@"bocfel_fgmode"];
        library_state.bgmode = [decoder decodeInt32ForKey:@"bocfel_bgmode"];
        library_state.style = [decoder decodeInt32ForKey:@"bocfel_style"];

        library_state.routine = [decoder decodeInt32ForKey:@"bocfel_routine"];
        library_state.queued_sound = [decoder decodeInt32ForKey:@"bocfel_next_sample"];
        library_state.sound_channel_tag = [decoder decodeInt32ForKey:@"bocfel_sound_channel_tag"];
	}
}
