//
//  glulxestart.m
//  glulxestart
//
//  Created by Petter Sjölund on 2019-03-08.
//
//

/* spatterstart.c: Spatterlight-specific code for Glulxe.
 Based on unixstrt.c by Andrew Plotkin <erkyrath@eblong.com>
 http://eblong.com/zarf/glulx/index.html
 */

#include <stdlib.h>
#include <string.h>
#include "glk.h"
#include "gi_blorb.h"
#include "glulxe.h"
#include "glkstart.h" /* This comes with the Glk library. */

#if VM_DEBUGGER
/* This header file may come with the Glk library. If it doesn't, comment
 out VM_DEBUGGER in glulxe.h -- you won't be able to use debugging. */
#include "gi_debug.h"
#endif /* VM_DEBUGGER */

#import "spatterstart.h"

static void *accel_func_array; /* used by the archive/unarchive hooks */

//static void spatterglk_game_start(void);
static void spatterglk_game_autorestore(void);
static void spatterglk_game_select(glui32 eventaddr);
static void spatterglk_library_archive(TempLibrary *library, NSCoder *encoder);
static void spatterglk_library_unarchive(TempLibrary *library, NSCoder *decoder);
static void recover_library_state(LibraryState *library_state);

char autosavename[1024] = "";

/* The only command-line argument is the filename. And the profiling switch,
 if that's compiled in. The only *two* command-line arguments are...
 */
glkunix_argumentlist_t glkunix_arguments[] = {

#if VM_PROFILING
    { "--profile", glkunix_arg_ValueFollows, "Generate profiling information to a file." },
    { "--profcalls", glkunix_arg_NoValue, "Include what-called-what details in profiling. (Slow!)" },
#endif /* VM_PROFILING */
#if VM_DEBUGGER
    { "--gameinfo", glkunix_arg_ValueFollows, "Read debug information from a file." },
    { "--cpu", glkunix_arg_NoValue, "Display CPU usage of each command (debug)." },
    { "--starttrap", glkunix_arg_NoValue, "Enter debug mode at startup time (debug)." },
    { "--quittrap", glkunix_arg_NoValue, "Enter debug mode at quit time (debug)." },
    { "--crashtrap", glkunix_arg_NoValue, "Enter debug mode on any fatal error (debug)." },
#endif /* VM_DEBUGGER */

    { "", glkunix_arg_ValueFollows, "filename: The game file to load." },

    { NULL, glkunix_arg_End, NULL }
};

int glkunix_startup_code(glkunix_startup_t *data)
{
    /* It turns out to be more convenient if we return TRUE from here, even
     when an error occurs, and display an error in glk_main(). */
    int ix;
    char *filename = NULL;
    //char *gameinfofilename = NULL;
    //int gameinfoloaded = FALSE;
    unsigned char buf[12];
    int res;

    garglk_set_program_name("Glulxe 0.5.4");
    garglk_set_program_info("Glulxe 0.5.4 by Andrew Plotkin");

    /* Parse out the arguments. They've already been checked for validity,
     and the library-specific ones stripped out.
     As usual for Unix, the zeroth argument is the executable name. */
    for (ix=1; ix<data->argc; ix++) {

#if VM_PROFILING
        if (!strcmp(data->argv[ix], "--profile")) {
            ix++;
            if (ix<data->argc) {
                strid_t profstr = glkunix_stream_open_pathname_gen(data->argv[ix], TRUE, FALSE, 1);
                if (!profstr) {
                    init_err = "Unable to open profile output file.";
                    init_err2 = data->argv[ix];
                    return TRUE;
                }
                setup_profile(profstr, NULL);
            }
            continue;
        }
        if (!strcmp(data->argv[ix], "--profcalls")) {
            profile_set_call_counts(TRUE);
            continue;
        }
#endif /* VM_PROFILING */

#if VM_DEBUGGER
        if (!strcmp(data->argv[ix], "--gameinfo")) {
            ix++;
            if (ix<data->argc) {
                gameinfofilename = data->argv[ix];
            }
            continue;
        }
        if (!strcmp(data->argv[ix], "--cpu")) {
            debugger_track_cpu(TRUE);
            continue;
        }
        if (!strcmp(data->argv[ix], "--starttrap")) {
            debugger_set_start_trap(TRUE);
            continue;
        }
        if (!strcmp(data->argv[ix], "--quittrap")) {
            debugger_set_quit_trap(TRUE);
            continue;
        }
        if (!strcmp(data->argv[ix], "--crashtrap")) {
            debugger_set_crash_trap(TRUE);
            continue;
        }
#endif /* VM_DEBUGGER */

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

#if VM_DEBUGGER
    if (gameinfofilename) {
        strid_t debugstr = glkunix_stream_open_pathname_gen(gameinfofilename, FALSE, FALSE, 1);
        if (!debugstr) {
            nonfatal_warning("Unable to open gameinfo file for debug data.");
        }
        else {
            int bres = debugger_load_info_stream(debugstr);
            glk_stream_close(debugstr, NULL);
            if (!bres)
                nonfatal_warning("Unable to parse game info.");
            else
                gameinfoloaded = TRUE;
        }
    }

    /* Report debugging available, whether a game info file is loaded or not. */
    gidebug_debugging_available(debugger_cmd_handler, debugger_cycle_handler);
#endif /* VM_DEBUGGER */

    /* Now we have to check to see if it's a Blorb file. */

    glk_stream_set_position(gamefile, 0, seekmode_Start);
    res = glk_get_buffer_stream(gamefile, (char *)buf, 12);
    if (!res) {
        init_err = "The data in this stand-alone game is too short to read.";
        return TRUE;
    }

    if (buf[0] == 'G' && buf[1] == 'l' && buf[2] == 'u' && buf[3] == 'l') {
        /* Load game directly from file. */
        locate_gamefile(FALSE);

    }
    else if (buf[0] == 'F' && buf[1] == 'O' && buf[2] == 'R' && buf[3] == 'M'
             && buf[8] == 'I' && buf[9] == 'F' && buf[10] == 'R' && buf[11] == 'S') {
        /* Load game from a chunk in the Blorb file. */
        locate_gamefile(TRUE);

#if VM_DEBUGGER
        /* Load the debug info from the Blorb, if it wasn't loaded from a file. */
        if (!gameinfoloaded) {
            glui32 giblorb_ID_Dbug = giblorb_make_id('D', 'b', 'u', 'g');
            giblorb_err_t err;
            giblorb_result_t blorbres;
            err = giblorb_load_chunk_by_type(giblorb_get_resource_map(),
                                             giblorb_method_FilePos,
                                             &blorbres, giblorb_ID_Dbug, 0);
            if (!err) {
                int bres = debugger_load_info_chunk(gamefile, blorbres.data.startpos, blorbres.length);
                if (!bres)
                    nonfatal_warning("Unable to parse game info.");
                else
                    gameinfoloaded = TRUE;
            }
        }
#endif /* VM_DEBUGGER */

    }
    else {
        init_err = "This is neither a Glulx game file nor a Blorb file "
        "which contains one.";

        return TRUE;
    }

    set_library_autorestore_hook(&spatterglk_game_autorestore);
    set_library_select_hook(&spatterglk_game_select);
    max_undo_level = 32; // allow 32 undo steps

    return TRUE;
}

/* This is the library_autorestore_hook, which will be called from glk_main() between VM setup and the beginning of the execution loop. (VM thread)
 */
static void spatterglk_game_autorestore()
{
    @autoreleasepool {

        TempLibrary *newlib = nil;
        getautosavedir(gamefile->filename);
        NSString *dirname = [NSString stringWithUTF8String:autosavedir];
        if (!dirname)
            return;
        NSString *glksavepath = [dirname stringByAppendingPathComponent:@"autosave.glksave"];
        NSString *libsavepath = [dirname stringByAppendingPathComponent:@"autosave.plist"];

        if (![[NSFileManager defaultManager] fileExistsAtPath:glksavepath])
            return;
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
            return;
        }


        [TempLibrary setExtraUnarchiveHook:spatterglk_library_unarchive];
        @try {
            newlib = [NSKeyedUnarchiver unarchiveObjectWithFile:libsavepath];
        }
        @catch (NSException *ex) {
            // leave newlib as nil
            NSLog(@"Unable to restore autosave library: %@", ex);
        }
        [TempLibrary setExtraUnarchiveHook:nil];

        if (!newlib ||!((LibraryState *)newlib.extraData).active) {
            /* Without a Glk state, there's no point in even trying the VM state. We reset the game */
            NSLog(@"library autorestore failed!");
            win_reset();
            return;
        }

        int res;

        strncpy(autosavename, [glksavepath UTF8String], sizeof autosavename);
        autosavename[sizeof autosavename-1] = 0;
        frefid_t fref = gli_new_fileref(autosavename, fileusage_SavedGame, 1);
        strid_t savefile = glk_stream_open_file(fref, filemode_Read, 1);
        res = perform_restore(savefile, TRUE);
        glk_stream_close(savefile, nil);
        gli_delete_fileref(fref);
        savefile = nil;

        if (res) {
            NSLog(@"VM autorestore failed!");
            win_reset();
            return;
        }

        /* Pop the callstub, restoring the PC to the @glk opcode (prevpc). */
        pop_callstub(0);

        /* Annoyingly, the updateFromLibrary we're about to do will close the currently-open gamefile. We'll recover it immediately, in recover_library_state(). */
        gamefile = nil;

        [newlib updateFromLibrary];
        recover_library_state((LibraryState *)newlib.extraData);

        giblorb_err_t err;
        err = giblorb_set_resource_map(gamefile);
        if (err) {
             NSLog(@"Could not set resource map from gamefile.");
        }
        
        [newlib updateFromLibraryLate];
        
        NSLog(@"autorestore succeeded.");
        newlib.extraData = nil;
    }
}

/* This is only needed for autorestore. */
extern gidispatch_rock_t glulxe_classtable_register_existing(void *obj, glui32 objclass, glui32 dispid);

/* Backtrack through the current opcode (at prevpc), and figure out whether its input arguments are on the stack or not. This will be important when setting up the saved VM state for restarting its opcode.

 The opmodes argument must be an array int[3]. Returns YES on success.
 */
static int parse_partial_operand(int *opmodes)
{
	glui32 addr = prevpc;

    /* Fetch the opcode number. */
    glui32 opcode = Mem1(addr);
    addr++;
    if (opcode & 0x80) {
		/* More than one-byte opcode. */
		if (opcode & 0x40) {
			/* Four-byte opcode */
			opcode &= 0x3F;
			opcode = (opcode << 8) | Mem1(addr);
			addr++;
			opcode = (opcode << 8) | Mem1(addr);
			addr++;
			opcode = (opcode << 8) | Mem1(addr);
			addr++;
		}
		else {
			/* Two-byte opcode */
			opcode &= 0x7F;
			opcode = (opcode << 8) | Mem1(addr);
			addr++;
		}
    }

	if (opcode != 0x130) { /* op_glk */
		NSLog(@"spatterglk_startup_code: parsed wrong opcode: %d", opcode);
		return NO;
	}

	/* @glk has operands LLS. */
	opmodes[0] = Mem1(addr) & 0x0F;
	opmodes[1] = (Mem1(addr) >> 4) & 0x0F;
	opmodes[2] = Mem1(addr+1) & 0x0F;

	return YES;
}



/* This is the library_select_hook, which will be called every time glk_select() is invoked. (VM thread)
 */
static void spatterglk_game_select(glui32 eventaddr)
{
//    NSLog(@"### game called select, last event was %d", lasteventtype);

	/* Do not autosave if we've just started up, or if the last event was a rearrange event. (We get rearranges in clusters, and they don't change anything interesting anyhow.) */
	if (lasteventtype == -1 || lasteventtype == evtype_Arrange)
    {
		return;
    }

	spatterglk_do_autosave(eventaddr);
}

void spatterglk_do_autosave(glui32 eventaddr)
{
    @autoreleasepool {
        TempLibrary *library = [[TempLibrary alloc] init];
        // NSLog(@"### attempting autosave (pc = %x, eventaddr = %x, stack = %d before stub)", prevpc, eventaddr, stackptr);

        /* When the save file is autorestored, the VM will restart the @glk opcode. That means that the Glk argument (the event structure address) must be waiting on the stack. Possibly also the @glk opcode's operands -- these might or might not have come off the stack. */

        glui32 oldmode, oldrock;

        int res;
        int opmodes[3];
        res = parse_partial_operand(opmodes);
        if (!res)
            return;

        getautosavedir(gamefile->filename);
        NSString *dirname = [NSString stringWithUTF8String:autosavedir];
        if (!dirname)
            return;
        NSString *tmpgamepath = [dirname stringByAppendingPathComponent:@"autosave-tmp.glksave"];

        strncpy(autosavename, [tmpgamepath UTF8String], sizeof autosavename);
        autosavename[sizeof autosavename-1] = 0;

        frefid_t fref = gli_new_fileref(autosavename, fileusage_SavedGame, 1);
        strid_t savefile = glk_stream_open_file(fref, filemode_Write, 1);

        /* Push all the necessary arguments for the @glk opcode. */
        glui32 origstackptr = stackptr;
        int stackvals = 0;
        /* The event structure address: */
        stackvals++;
        if (stackptr+4 > stacksize)
            fatal_error("Stack overflow in autosave callstub.");
        StkW4(stackptr, eventaddr);
        stackptr += 4;
        if (opmodes[1] == 8) {
            /* The number of Glk arguments (1): */
            stackvals++;
            if (stackptr+4 > stacksize)
                fatal_error("Stack overflow in autosave callstub.");
            StkW4(stackptr, 1);
            stackptr += 4;
        }
        if (opmodes[0] == 8) {
            /* The Glk call selector (0x00C0): */
            stackvals++;
            if (stackptr+4 > stacksize)
                fatal_error("Stack overflow in autosave callstub.");
            StkW4(stackptr, 0x00C0); /* glk_select */
            stackptr += 4;
        }

        /* Push a temporary callstub which contains the *last* PC -- the address of the @glk(select) invocation. */
        if (stackptr+16 > stacksize)
            fatal_error("Stack overflow in autosave callstub.");
        StkW4(stackptr+0, 0);
        StkW4(stackptr+4, 0);
        StkW4(stackptr+8, prevpc);
        StkW4(stackptr+12, frameptr);
        stackptr += 16;

        /* temporarily set I/O system to GLK, which perform_save() needs */

        stream_get_iosys(&oldmode, &oldrock);
        stream_set_iosys(2, 0); 

        res = perform_save(savefile);

        stream_set_iosys(oldmode, oldrock);

        stackptr -= 16; // discard the temporary callstub
        stackptr -= 4 * stackvals; // discard the temporary arguments
        if (origstackptr != stackptr)
            fatal_error("Stack pointer mismatch in autosave");

        glk_stream_close(savefile, nil);
        gli_delete_fileref(fref);

        savefile = nil;

        if (res) {
            NSLog(@"VM autosave failed!");
            return;
        }

        //stash_library_state();
        /* The spatterglk_library_archive hook will write out the contents of library_state. */

        NSString *tmplibpath = [dirname stringByAppendingPathComponent:@"autosave-tmp.plist"];
        [TempLibrary setExtraArchiveHook:spatterglk_library_archive];
        res = [NSKeyedArchiver archiveRootObject:library toFile:tmplibpath];
        [TempLibrary setExtraArchiveHook:nil];

        if (!res) {
            NSLog(@"library serialize failed!");
            return;
        }

        NSString *finalgamepath = [dirname stringByAppendingPathComponent:@"autosave.glksave"];
        NSString *finallibpath = [dirname stringByAppendingPathComponent:@"autosave.plist"];

        /* This is not really atomic, but we're already past the serious failure modes. */
        [[NSFileManager defaultManager] removeItemAtPath:finallibpath error:nil];
        [[NSFileManager defaultManager] removeItemAtPath:finalgamepath error:nil];

        res = [[NSFileManager defaultManager] moveItemAtPath:tmpgamepath toPath:finalgamepath error:nil];
        if (!res) {
            NSLog(@"could not move game autosave to final position!");
            return;
        }
        res = [[NSFileManager defaultManager] moveItemAtPath:tmplibpath toPath:finallibpath error:nil];
        if (!res) {
            NSLog(@"could not move library autosave to final position");
            return;
        }
        win_autosave(AUTOSAVE_SERIAL_VERSION); // Call window server to do its own autosave
    }
}

/* Delete an autosaved game, if one exists.
 */
void spatterglk_clear_autosave()
{
	getautosavedir(gamefile->filename);
	NSString *dirname = [NSString stringWithUTF8String:autosavedir];
	if (!dirname)
		return;

	NSString *finalgamepath = [dirname stringByAppendingPathComponent:@"autosave.glksave"];
	NSString *finallibpath = [dirname stringByAppendingPathComponent:@"autosave.plist"];

	[[NSFileManager defaultManager] removeItemAtPath:finallibpath error:nil];
	[[NSFileManager defaultManager] removeItemAtPath:finalgamepath error:nil];
}

/* Utility function used by stash_library_state. Assumes that the global accel_func_array points to a valid NSMutableArray. */
static void stash_one_accel_func(glui32 index, glui32 addr)
{
    @autoreleasepool {
        NSMutableArray *arr = (__bridge NSMutableArray *)accel_func_array;
        GlulxAccelEntry *ent = [[GlulxAccelEntry alloc] initWithIndex:index addr:addr];
        [arr addObject:ent];
    }
}

static void spatterglk_library_archive(TempLibrary *library, NSCoder *encoder)
{
    @autoreleasepool {
        LibraryState *library_state = [[LibraryState alloc] initWithLibrary:library];

        //if (library_state.active) {
        [encoder encodeBool:YES forKey:@"glulx_library_state"];
        [encoder encodeInt32:library_state.protectstart forKey:@"glulx_protectstart"];
        [encoder encodeInt32:library_state.protectend forKey:@"glulx_protectend"];
        [encoder encodeInt32:library_state.iosys_mode forKey:@"glulx_iosys_mode"];
        [encoder encodeInt32:library_state.iosys_rock forKey:@"glulx_iosys_rock"];
        [encoder encodeInt32:library_state.stringtable forKey:@"glulx_stringtable"];
        if (library_state.accel_params)
            [encoder encodeObject:library_state.accel_params forKey:@"glulx_accel_params"];
        else NSLog(@"spatterglk_library_archive: no accelerated parameters?");
        if (library_state.accel_funcs)
            [encoder encodeObject:library_state.accel_funcs forKey:@"glulx_accel_funcs"];
        else NSLog(@"spatterglk_library_archive: no accelerated functions?");
        [encoder encodeInt32:library_state.gamefiletag forKey:@"glulx_gamefiletag"];
        if (library_state.id_map_list)
            [encoder encodeObject:library_state.id_map_list forKey:@"glulx_id_map_list"];
        //}
        // else NSLog(@"spatterglk_library_archive: library_state not active?");
    }
}

static void spatterglk_library_unarchive(TempLibrary *library, NSCoder *decoder)
{
    @autoreleasepool {

        LibraryState *library_state = [[LibraryState alloc] init];

        if ([decoder decodeBoolForKey:@"glulx_library_state"]) {
            library_state.active = true;
            library_state.protectstart = [decoder decodeInt32ForKey:@"glulx_protectstart"];
            library_state.protectend = [decoder decodeInt32ForKey:@"glulx_protectend"];
            library_state.iosys_mode = [decoder decodeInt32ForKey:@"glulx_iosys_mode"];
            library_state.iosys_rock = [decoder decodeInt32ForKey:@"glulx_iosys_rock"];
            library_state.stringtable = [decoder decodeInt32ForKey:@"glulx_stringtable"];
            library_state.accel_params = [decoder decodeObjectForKey:@"glulx_accel_params"];
            library_state.accel_funcs = [decoder decodeObjectForKey:@"glulx_accel_funcs"];
            library_state.gamefiletag = [decoder decodeInt32ForKey:@"glulx_gamefiletag"];
            library_state.id_map_list = [decoder decodeObjectForKey:@"glulx_id_map_list"];
            
            library.extraData = library_state;
        }
        else library.extraData = nil;
    }
}

/* Copy chunks of VM state out of the (static) library_state object.
 */
static void recover_library_state(LibraryState *library_state)
{
    @autoreleasepool {

        if (library_state.active) {
            protectstart = library_state.protectstart;
            protectend = library_state.protectend;
            stream_set_iosys(library_state.iosys_mode, library_state.iosys_rock);
            stream_set_table(library_state.stringtable);

            if (library_state.accel_params) {
                for (int ix=0; ix<library_state.accel_params.count; ix++) {
                    NSNumber *num = [library_state.accel_params objectAtIndex:ix];
                    glui32 param = num.unsignedIntValue;
                    accel_set_param(ix, param);
                }
            }
            else NSLog(@"spatterglk_library_unarchive: No accel_params!");

            if (library_state.accel_funcs) {
                for (GlulxAccelEntry *entry in library_state.accel_funcs) {
                    accel_set_func(entry.index, entry.addr);
                }
            }
            else NSLog(@"spatterglk_library_unarchive: No accel_funcs!");

            if (library_state.id_map_list) {
                for (GlkObjIdEntry *ent in library_state.id_map_list) {
                    switch (ent.objclass) {
                        case gidisp_Class_Window: {
                            window_t *win = gli_window_for_tag(ent.tag);
                            if (!win) {
                                NSLog(@"### Could not find window for tag %d", ent.tag);
                                continue;
                            }
                            win->disprock = glulxe_classtable_register_existing(win, ent.objclass, ent.dispid);
                        }
                            break;
                        case gidisp_Class_Stream: {
                            stream_t *str = gli_stream_for_tag(ent.tag);
                            if (!str) {
                                NSLog(@"### Could not find stream for tag %d", ent.tag);
                                continue;
                            }
                            str->disprock = glulxe_classtable_register_existing(str, ent.objclass, ent.dispid);
                        }
                            break;
                        case gidisp_Class_Fileref: {
                            fileref_t *fref = gli_fileref_for_tag(ent.tag);
                            if (!fref) {
                                NSLog(@"### Could not find fileref for tag %d", ent.tag);
                                continue;
                            }
                            fref->disprock = glulxe_classtable_register_existing(fref, ent.objclass, ent.dispid);
                        }
                            break;

                        case gidisp_Class_Schannel: {
                            channel_t *chan = gli_schan_for_tag(ent.tag);
                            if (!chan) {
                                NSLog(@"### Could not find sound channel for tag %d", ent.tag);
                                continue;
                            }
                            chan->disprock = glulxe_classtable_register_existing(chan, ent.objclass, ent.dispid);
                        }
                            break;
                    }
                }
            }
            else NSLog(@"spatterglk_library_unarchive: no id_map_list!");

            if (library_state.gamefiletag)
            {
                gamefile = gli_stream_for_tag(library_state.gamefiletag);
                if (!gamefile)
                    NSLog(@"### Could not find game file stream, tag %d",library_state.gamefiletag);
            }
            else NSLog(@"spatterglk_library_unarchive: no gamefiletag!");

        }
    }
}

int spatterglk_can_restart_cleanly()
{
 	return vm_exited_cleanly;
}

void spatterglk_shut_down_process()
{
	/* Yes, we really do want to exit the app here. A fatal error has occurred at the interpreter level, so we can't restart it cleanly. The user has either hit a "goodbye" dialog button or the Home button; either way, it's time for suicide. */
	NSLog(@"spatterglk_shut_down_process: goodbye!");
	exit(1);
}

@implementation LibraryState

- (instancetype) initWithLibrary: (TempLibrary *)library {

    self = [super init];

	if (self) {

        _active = YES;

        _protectstart = protectstart;
        _protectend = protectend;
        stream_get_iosys(&_iosys_mode, &_iosys_rock);
        _stringtable = stream_get_table();

        glui32 count = accel_get_param_count();
        _accel_params = [NSMutableArray arrayWithCapacity:count];
        for (int ix=0; ix<count; ix++) {
            glui32 param = accel_get_param(ix);
            [_accel_params addObject:[NSNumber numberWithUnsignedInt:param]];
        }

        NSMutableArray *accel_funcs = [NSMutableArray arrayWithCapacity:8];
        accel_func_array = (__bridge void *)(accel_funcs);
        accel_iterate_funcs(&stash_one_accel_func);
        _accel_funcs = [(__bridge NSMutableArray *)accel_func_array copy];
        accel_funcs = nil;

        if (gamefile)
            _gamefiletag = gamefile->tag;
        else { _gamefiletag = 0;
            NSLog(@"LibraryState initWithLibrary: No gamefile?");
        }

        _id_map_list = [NSMutableArray arrayWithCapacity:4];

        for (TempWindow *win in library.windows) {
            GlkObjIdEntry *ent = [[GlkObjIdEntry alloc] initWithClass:gidisp_Class_Window tag:win.tag id:find_id_for_window(gli_window_for_tag(win.tag))];
            //NSLog(@"LibraryState initWithLibrary: added window with tag %d",ent.tag);
            [_id_map_list addObject:ent];
        }
        for (TempStream *str in library.streams) {
            GlkObjIdEntry *ent = [[GlkObjIdEntry alloc] initWithClass:gidisp_Class_Stream tag:str.tag id:find_id_for_stream(gli_stream_for_tag(str.tag))];
            //NSLog(@"LibraryState initWithLibrary: added stream with tag %d",ent.tag);
            [_id_map_list addObject:ent];
        }
        for (TempFileRef *fref in library.filerefs) {
            GlkObjIdEntry *ent = [[GlkObjIdEntry alloc] initWithClass:gidisp_Class_Fileref tag:fref.tag id:find_id_for_fileref(gli_fileref_for_tag(fref.tag))];
            //NSLog(@"LibraryState initWithLibrary: added file reference with tag %d",ent.tag);
            [_id_map_list addObject:ent];
        }

        for (TempSChannel *chan in library.schannels) {
            GlkObjIdEntry *ent = [[GlkObjIdEntry alloc] initWithClass:gidisp_Class_Schannel tag:chan.tag id:find_id_for_schannel(gli_schan_for_tag(chan.tag))];
            //NSLog(@"LibraryState initWithLibrary: added sound channel with tag %d",ent.tag);
            [_id_map_list addObject:ent];
        }

    }

    return self;
}

@end

/* GlkObjIdEntry: A simple data class which stores the mapping of a TempLibrary object (window, stream, etc) to its Glulx-VM ID number. */

@implementation GlkObjIdEntry

- (id) initWithClass:(int)objclassval tag:(glui32)tagref id:(glui32)dispidval
{
	self = [super init];

	if (self) {
		_objclass = objclassval;
		_tag = tagref;
		_dispid = dispidval;
	}

	return self;
}

- (id) initWithCoder:(NSCoder *)decoder
{
	_objclass = [decoder decodeInt32ForKey:@"objclass"];
	_tag = [decoder decodeInt32ForKey:@"tag"];
	_dispid = [decoder decodeInt32ForKey:@"dispid"];

    return self;
}


- (void) encodeWithCoder:(NSCoder *)encoder
{
	[encoder encodeInt32:_objclass forKey:@"objclass"];
	[encoder encodeInt32:_tag forKey:@"tag"];
	[encoder encodeInt32:_dispid forKey:@"dispid"];
}

@end


/* GlulxAccelEntry: A simple data class which stores an accelerated-function table entry. */

@implementation GlulxAccelEntry

- (id) initWithIndex:(glui32)indexval addr:(glui32)addrval
{
	self = [super init];
    
	if (self) {
		index = indexval;
		addr = addrval;
	}
    
	return self;
}

- (id) initWithCoder:(NSCoder *)decoder
{
	index = [decoder decodeInt32ForKey:@"index"];
	addr = [decoder decodeInt32ForKey:@"addr"];
	return self;
}

- (void) encodeWithCoder:(NSCoder *)encoder
{
	[encoder encodeInt32:index forKey:@"index"];
	[encoder encodeInt32:addr forKey:@"addr"];
}

- (glui32) index { return index; }
- (glui32) addr { return addr; }

@end

