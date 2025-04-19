
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

extern "C" {
#include "glk.h"
#include "glkimp.h"
#include "fileref.h"
}

#include "zterp.h"
#include "stack.h"
#include "process.h"
#include "spatterlight-autosave.h"

#import "TempLibrary.h"

static void spatterlight_library_archive(TempLibrary *library, NSCoder *encoder);
static void spatterlight_library_unarchive(TempLibrary *library, NSCoder *decoder);
static library_state_data library_state; /* used by the archive/unarchive hooks */

/* Do an auto-save of the game state, to an macOS-appropriate location. This also saves the Glk library state.
 
 The game goes into ~Library/Application Support/Spatterlight/Bocfel Files/Autosaves/(FILE HASH)/autosave.glksave; the library state into autosave.plist. However, we do this as atomically as possible -- we write to temp files and then rename.
 */
void spatterlight_do_autosave(enum SaveOpcode saveopcode) {
    
    if (!gli_enable_autosave)
        return;
    
    if ((int)lasteventtype == -1 || lasteventtype == evtype_Arrange || lasteventtype == evtype_Redraw || (lasteventtype == evtype_Timer && !gli_enable_autosave_on_timer))
    {
        return;
    }

    if (autosavedir == NULL) {
        int len = game_file.size();
        char *c = new char[len + 1];
        std::copy(game_file.begin(), game_file.end(), c);
        c[len] = '\0';
        getautosavedir(c);
        delete[] c;
    }

    if (autosavedir == NULL) {
        win_showerror(@"Could not create autosave directory name.".UTF8String);
        return;
    }

    @autoreleasepool {
        TempLibrary *library = [[TempLibrary alloc] init];
        NSFileManager *fileManager = [NSFileManager defaultManager];
        
        NSString *dirname = [fileManager stringWithFileSystemRepresentation:autosavedir length:strlen(autosavedir)];
        if (!dirname) {
            return;
        }

        NSError *error = nil;

        // Rename any existing autosave file
        NSString *finalgamepath = [dirname stringByAppendingPathComponent:@"autosave.glksave"];
        NSString *oldgamepath = [dirname stringByAppendingPathComponent:@"autosave-bak.glksave"];
        
        [fileManager removeItemAtPath:oldgamepath error:&error];
        [fileManager moveItemAtPath:finalgamepath toPath:oldgamepath error:&error];

        unsigned long stored_pc = pc;
        if (saveopcode == SaveOpcode::None) {
            saveopcode = SaveOpcode::ReadChar;
            pc--;
        }
        bool res;
        res = do_save(SaveType::Autosave, saveopcode);
        pc = stored_pc;
        
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
            win_showerror(@"Could not move autosave plist to final position.".UTF8String);
            NSLog(@"Could not move library autosave to final position (continuing)");
        }
        win_autosave(library.autosaveTag); // Call window server to do its own autosave
    }
    return;
}

// Restore an autosaved game, if one exists.
// Returns true if the game was restored successfully, false if not.
bool spatterlight_restore_autosave(enum SaveOpcode *saveopcode)
{
    if (!gli_enable_autosave)
        return false;
    @autoreleasepool {
        if (autosavedir == NULL) {
            int len = game_file.size();
            char *c = new char[len + 1];
            std::copy(game_file.begin(), game_file.end(), c);
            c[len] = '\0';
            getautosavedir(c);
            delete[] c;
        }
        
        NSString *dirname = @(autosavedir);
        if (!dirname.length) {
            win_showerror(@"Could not create autosave directory name.".UTF8String);
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
        
        int res = do_restore(SaveType::Autosave, *saveopcode);
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
            bool blorb_stream_was_active = (active_blorb_file_stream != nullptr);
            char old_blorb_file_name[2048];
            size_t old_name_length;
            if (blorb_stream_was_active) {
                old_name_length = strnlen(active_blorb_file_stream->filename, 2047) + 1;
                strncpy(old_blorb_file_name, active_blorb_file_stream->filename, old_name_length + 1);
                giblorb_unset_resource_map();
                active_blorb_file_stream = nullptr;
            }
            [newlib updateFromLibrary];
            recover_library_state(&library_state);

            if (active_blorb_file_stream != nullptr && blorb_stream_was_active &&
                strncmp(active_blorb_file_stream->filename, old_blorb_file_name, 2048) != 0) {
                // If the autorestored stream file name doesn't match the one we opened before
                // the autorestore, we need to close it (and reopen the old one below.)
                glk_stream_close(active_blorb_file_stream, 0);
                active_blorb_file_stream = nullptr;
            }

            // If there was a working, active blorb file, but there isn't anymore
            // we just reopen the blorb file we used before the autorestore
            if (active_blorb_file_stream == nullptr && blorb_stream_was_active) {
                active_blorb_file_stream = glkunix_stream_open_pathname(old_blorb_file_name, 0, 0);
            }

            if (active_blorb_file_stream != nullptr) {
                giblorb_err_t err = giblorb_set_resource_map(active_blorb_file_stream);
                if (err != giblorb_err_None) {
                    glk_stream_close(active_blorb_file_stream, 0);
                    active_blorb_file_stream = nullptr;
                }
            }

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
    [encoder encodeInt32:library_state.graphicswintag forKey:@"bocfel_graphicswintag"];
    [encoder encodeInt32:library_state.blorbfiletag forKey:@"bocfel_blorbfiletag"];
    [encoder encodeInt32:(int32_t)library_state.routine forKey:@"bocfel_routine"];
    [encoder encodeInt32:(int32_t)library_state.queued_sound forKey:@"bocfel_next_sample"];
    [encoder encodeInt32:(int32_t)library_state.sound_channel_tag forKey:@"bocfel_sound_channel_tag"];
    [encoder encodeInt64:(int64_t)library_state.last_random_seed forKey:@"bocfel_last_random_seed"];
    [encoder encodeInt32:(int32_t)library_state.random_calls_count forKey:@"bocfel_random_calls_count"];

    [encoder encodeInt32:(int32_t)library_state.screenmode forKey:@"bocfel_screenmode"];
    [encoder encodeInt32:(int32_t)library_state.selected_journey_line forKey:@"bocfel_selected_journey_line"];
    [encoder encodeInt32:(int32_t)library_state.selected_journey_column forKey:@"bocfel_selected_journey_column"];
    [encoder encodeInt32:(int32_t)library_state.current_input_mode forKey:@"bocfel_current_input_mode"];
    [encoder encodeInt32:(int32_t)library_state.current_input_length forKey:@"bocfel_current_input_length"];

    [encoder encodeInt32:(int32_t)library_state.current_graphics_win_tag forKey:@"bocfel_current_graphics_win_tag"];
    [encoder encodeInt32:(int32_t)library_state.graphics_fg_tag forKey:@"bocfel_graphics_fg_tag"];
    [encoder encodeInt32:(int32_t)library_state.stored_lower_tag forKey:@"bocfel_stored_lower_tag"];
    [encoder encodeInt32:(int32_t)library_state.hints_depth forKey:@"bocfel_hints_depth"];
    [encoder encodeInt32:(int32_t)library_state.slideshow_pic forKey:@"bocfel_slideshow_pic"];
    [encoder encodeInt32:(int32_t)library_state.current_picture forKey:@"bocfel_current_picture"];

    [encoder encodeInt32:(int32_t)library_state.shogun_menu forKey:@"bocfel_shogun_menu"];
    [encoder encodeInt32:(int32_t)library_state.shogun_menu_selection forKey:@"bocfel_shogun_menu_selection"];
    [encoder encodeInt32:(int32_t)library_state.define_line forKey:@"bocfel_define_line"];

    [encoder encodeInt32:(int32_t)library_state.internal_read_char_hack forKey:@"internal_read_char_hack"];

    if (library_state.number_of_journey_words > 0) {
        NSMutableArray<NSArray *> *tempMutArray = [[NSMutableArray alloc] initWithCapacity:library_state.number_of_journey_words];

        for (int i = 0; i < library_state.number_of_journey_words; i++) {
            NSArray<NSNumber *> *tempArray = @[@(library_state.journey_words[i].str), @(library_state.journey_words[i].pcf), @(library_state.journey_words[i].pcm)];
            [tempMutArray addObject:tempArray];
        }

        [encoder encodeObject:tempMutArray forKey:@"bocfel_printed_journey_words"];
    }

    if (library_state.number_of_margin_images > 0) {
        NSMutableArray<NSNumber *> *tempMutArray2 = [[NSMutableArray alloc] initWithCapacity:library_state.number_of_margin_images];

        for (int i = 0; i < library_state.number_of_margin_images; i++) {
            [tempMutArray2 addObject:@(library_state.margin_images[i])];
        }

        [encoder encodeObject:tempMutArray2 forKey:@"bocfel_margin_images"];
    }

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
    library_state.graphicswintag = [decoder decodeInt32ForKey:@"bocfel_graphicswintag"];
    library_state.blorbfiletag = [decoder decodeInt32ForKey:@"bocfel_blorbfiletag"];
    library_state.routine = [decoder decodeInt32ForKey:@"bocfel_routine"];
    library_state.queued_sound = [decoder decodeInt32ForKey:@"bocfel_next_sample"];
    library_state.sound_channel_tag = [decoder decodeInt32ForKey:@"bocfel_sound_channel_tag"];
    library_state.last_random_seed = [decoder decodeInt64ForKey:@"bocfel_last_random_seed"];
    library_state.random_calls_count = [decoder decodeInt32ForKey:@"bocfel_random_calls_count"];

    library_state.screenmode = (V6ScreenMode)[decoder decodeInt32ForKey:@"bocfel_screenmode"];
    library_state.selected_journey_line = [decoder decodeInt32ForKey:@"bocfel_selected_journey_line"];
    library_state.selected_journey_column = [decoder decodeInt32ForKey:@"bocfel_selected_journey_column"];
    library_state.current_input_mode = (inputMode)[decoder decodeInt32ForKey:@"bocfel_current_input_mode"];
    library_state.current_input_length = [decoder decodeInt32ForKey:@"bocfel_current_input_length"];

    library_state.current_graphics_win_tag = [decoder decodeInt32ForKey:@"bocfel_current_graphics_win_tag"];
    library_state.graphics_fg_tag = [decoder decodeInt32ForKey:@"bocfel_graphics_fg_tag"];
    library_state.stored_lower_tag = [decoder decodeInt32ForKey:@"bocfel_stored_lower_tag"];
    library_state.hints_depth = [decoder decodeInt32ForKey:@"bocfel_hints_depth"];
    library_state.slideshow_pic = [decoder decodeInt32ForKey:@"bocfel_slideshow_pic"];
    library_state.current_picture = [decoder decodeInt32ForKey:@"bocfel_current_picture"];

    library_state.shogun_menu = [decoder decodeInt32ForKey:@"bocfel_shogun_menu"];
    library_state.shogun_menu_selection = [decoder decodeInt32ForKey:@"bocfel_shogun_menu_selection"];
    library_state.define_line = [decoder decodeInt32ForKey:@"bocfel_define_line"];

    library_state.internal_read_char_hack = [decoder decodeInt32ForKey:@"internal_read_char_hack"];

    NSArray<NSArray *> *tempArray = [decoder decodeObjectOfClass:[NSArray class] forKey:@"bocfel_printed_journey_words"];
    library_state.number_of_journey_words = tempArray.count;
    NSUInteger i = 0;
    for (NSArray *array in tempArray) {
        library_state.journey_words[i].str = ((NSNumber *)[array objectAtIndex:0]).intValue;
        library_state.journey_words[i].pcf = ((NSNumber *)[array objectAtIndex:1]).intValue;
        library_state.journey_words[i].pcm = ((NSNumber *)[array objectAtIndex:2]).intValue;
        i++;
    }

    NSArray<NSNumber *> *tempArray2 = [decoder decodeObjectOfClass:[NSArray class] forKey:@"bocfel_margin_images"];
    library_state.number_of_margin_images = tempArray2.count;
    i = 0;

    for (NSNumber *number in tempArray2) {
        library_state.margin_images[i] = number.intValue;
        i++;
    }
}
