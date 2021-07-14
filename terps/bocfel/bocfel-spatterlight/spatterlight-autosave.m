
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

#include "glk.h"
#include "fileref.h"

#include "zterp.h"
#include "spatterlight-autosave.h"

#import "TempLibrary.h"

static void spatterlight_library_archive(TempLibrary *library, NSCoder *encoder);
static void spatterlight_library_unarchive(TempLibrary *library, NSCoder *decoder);
static library_state_data library_state; /* used by the archive/unarchive hooks */

/* Do an auto-save of the game state, to an macOS-appropriate location. This also saves the Glk library state.
 
 The game goes into ~Library/Application Support/Spatterlight/Bocfel Files/Autosaves/(FILE HASH)/autosave.glksave; the library state into autosave.plist. However, we do this as atomically as possible -- we write to temp files and then rename.
 
 This is called in the VM thread, just before setting up line input and calling glk_select(). (So no window will actually be requesting line input at this time.)
 */
void spatterlight_do_autosave(enum SaveOpcode saveopcode) {
    
    if (!gli_enable_autosave)
        return;
    
    if ((int)lasteventtype == -1 || lasteventtype == evtype_Arrange || (lasteventtype == evtype_Timer && !gli_enable_autosave_on_timer))
    {
        return;
    }
    
    getautosavedir((char *)game_file);
    
    @autoreleasepool {
        TempLibrary *library = [[TempLibrary alloc] init];
        
        NSString *dirname = [NSString stringWithUTF8String:autosavedir];
        if (!dirname) {
            return;
        }

        NSFileManager *fileManager = [NSFileManager defaultManager];
        NSError *error = nil;

        // Rename any existing autosave file
        NSString *finalgamepath = [dirname stringByAppendingPathComponent:@"autosave.glksave"];
        NSString *oldgamepath = [dirname stringByAppendingPathComponent:@"autosave-bak.glksave"];
        
        [fileManager removeItemAtPath:oldgamepath error:&error];
        [fileManager moveItemAtPath:finalgamepath toPath:oldgamepath error:&error];

        bool res;
        res = do_save(SaveTypeAutosave, saveopcode);
        
        if (!res) {
            win_showerror("Failed to autosave.");
            NSLog(@"do_save() failed!");

            // Re-instate the old renamed save file back to where it was
            [fileManager removeItemAtPath:finalgamepath error:&error];
            [fileManager moveItemAtPath:oldgamepath toPath:finalgamepath error:&error];
            return;
        }
        
        stash_library_state(&library_state);
        /* The spatterlight_library_archive hook will write out the contents of library_state. */
        
        NSString *tmplibpath = [dirname stringByAppendingPathComponent:@"autosave-tmp.plist"];

        [TempLibrary setExtraArchiveHook:spatterlight_library_archive];
        res = [NSKeyedArchiver archiveRootObject:library toFile:tmplibpath];
        [TempLibrary setExtraArchiveHook:nil];
        
        if (!res) {
            NSLog(@"library serialize failed!");
            return;
        }
        
        NSString *finallibpath = [dirname stringByAppendingPathComponent:@"autosave.plist"];
        NSString *oldlibpath = [dirname stringByAppendingPathComponent:@"autosave-bak.plist"];


        /* This is not really atomic, but we're already past the serious failure modes. */
        [fileManager removeItemAtPath:oldlibpath error:&error];
        [fileManager moveItemAtPath:finallibpath toPath:oldlibpath error:&error];

        error = nil;
        res = [[NSFileManager defaultManager] moveItemAtPath:tmplibpath toPath:finallibpath error:nil];
        if (!res) {
            /* We don't abort out in this case; we leave the game autosave in place by itself, which is not ideal but better than data loss. */
            win_showerror("Could not move autosave plist to final position.");
            NSLog(@"Could not move library autosave to final position (continuing)");
        }
        win_autosave(library.autosaveTag); // Call window server to do its own autosave
    }
    return;
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

// Restore an autosaved game, if one exists.
// Returns true if the game was restored successfully, false if not.
bool spatterlight_restore_autosave(enum SaveOpcode *saveopcode)
{
    if (!gli_enable_autosave)
        return false;
    @autoreleasepool {
        getautosavedir((char *)game_file);
        
        NSString *dirname = [NSString stringWithUTF8String:autosavedir];
        if (!dirname.length) {
            win_showerror("Could not create autosave folder name.");
            return false;
        }
        NSString *finalgamepath = [dirname stringByAppendingPathComponent:@"autosave.glksave"];
        
        NSString *libsavepath = [dirname stringByAppendingPathComponent:@"autosave.plist"];
        
        if (![[NSFileManager defaultManager] fileExistsAtPath:finalgamepath])
            return false;
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
            return false;
        }
        
        TempLibrary *newlib = nil;
        
        int res = do_restore(SaveTypeAutosave, saveopcode);
        /* save_file is now closed */
        
        if (!res) {
            NSLog(@"Unable to restore autosave file.");
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
            recover_library_state(&library_state);
            
            load_resources();
            
            [newlib updateFromLibraryLate];
        } else { win_reset(); exit(0); }
        
    }
    return true;
}

static void spatterlight_library_archive(TempLibrary *library, NSCoder *encoder) {
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
    [encoder encodeInt32:(int32_t)library_state.routine forKey:@"bocfel_routine"];
    [encoder encodeInt32:(int32_t)library_state.queued_sound forKey:@"bocfel_next_sample"];
    [encoder encodeInt32:(int32_t)library_state.sound_channel_tag forKey:@"bocfel_sound_channel_tag"];
}

static void spatterlight_library_unarchive(TempLibrary *library, NSCoder *decoder) {
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
    library_state.routine = [decoder decodeInt32ForKey:@"bocfel_routine"];
    library_state.queued_sound = [decoder decodeInt32ForKey:@"bocfel_next_sample"];
    library_state.sound_channel_tag = [decoder decodeInt32ForKey:@"bocfel_sound_channel_tag"];
}
