#include <sys/select.h>

#import "GlkController_Private.h"
#import "GlkController+BorderColor.h"
#import "GlkController+FullScreen.h"
#import "GlkController+GlkRequests.h"
#import "GlkController+InterpreterGlue.h"
#import "GlkController+Speech.h"

#import "BufferTextView.h"
#import "GridTextView.h"
#import "CommandScriptHandler.h"
#import "GlkTextBufferWindow.h"
#import "GlkTextGridWindow.h"
#import "GlkGraphicsWindow.h"
#import "Theme.h"
#import "Fetches.h"
#import "Game.h"
#import "Metadata.h"
#import "Preferences.h"
#import "SoundHandler.h"
#import "ImageHandler.h"
#import "GlkEvent.h"
#import "FolderAccess.h"
#import "NotificationBezel.h"
#import "GlkStyle.h"
#import "CoverImageHandler.h"
#import "JourneyMenuHandler.h"
#import "TableViewController.h"
#import "ZMenu.h"
#import "BureaucracyForm.h"

#import "NSString+Categories.h"
#import "NSColor+integer.h"

#import "DebugLog.h"
#import "NSAlert+Spatterlight.h"
#import "Constants.h"

NSString * const kAutosaveTerpFile = @"autosave.plist";
NSString * const kAutosaveTerpBakFile = @"autosave-bak.plist";
NSString * const kAutosaveTerpTmpFile = @"autosave-tmp.plist";
NSString * const kAutosaveGlkSaveFile = @"autosave.glksave";
NSString * const kAutosaveGlkSaveBakFile = @"autosave-bak.glksave";
NSString * const kAutosaveGlkSaveTmpFile = @"autosave-tmp.glksave";
NSString * const kAutosaveGUIFile = @"autosave-GUI.plist";
NSString * const kAutosaveGUILateFile = @"autosave-GUI-late.plist";

// TempLibrary is a lightweight shim used solely to peek at the autosaveTag
// stored in interpreter autosave files, without needing to fully deserialize
// the interpreter library state. This lets us compare tags between the GUI
// and interpreter autosave files to verify they are in sync.
@interface TempLibrary : NSObject
@property glui32 autosaveTag;
@end

@implementation TempLibrary

- (instancetype) initWithCoder:(NSCoder *)decoder {
    self = [super init];
    if (self) {
    int version = [decoder decodeIntForKey:@"version"];
    if (version <= 0 || version != AUTOSAVE_SERIAL_VERSION)
    {
        NSLog(@"Interpreter autosave file:Wrong serial version!");
        return nil;
    }

    _autosaveTag = (glui32)[decoder decodeInt32ForKey:@"autosaveTag"];
    }
    return self;
}
@end

@implementation GlkController (Autorestore)

// Depth-first search of theView and its subviews for a grid window.
- (GlkTextGridWindow *)findGridWindowIn:(NSView *)theView {
    if ([theView isKindOfClass:[GlkTextGridWindow class]])
        return (GlkTextGridWindow *)theView;
    for (NSView *subview in theView.subviews) {
        GlkTextGridWindow *found = [self findGridWindowIn:subview];
        if (found)
            return found;
    }
    return nil;
}

- (Theme *)findThemeByName:(NSString *)name {
    return [Fetches findTheme:name inContext:libcontroller.managedObjectContext];
}

#pragma mark Autorestore at launch

// Attempt to restore a previously autosaved game session. This involves:
// 1. Deserializing the GUI state from autosave-GUI.plist (window layout, text, scroll positions)
// 2. Optionally deserializing a "late" GUI state captured after the interpreter finished processing
// 3. Verifying that the GUI and interpreter autosave files are in sync via autosaveTag comparison
// 4. Handling edge cases like version mismatches, missing files, and dead (finished) games
- (void)runTerpWithAutorestore {
    autorestoring = YES;
    [self loadRestoredControllers];

    if (!restoredController) {
        // If there are no useful UI autosave files,
        // delete any leftovers and run game without autorestoring
        [self deleteAutosaveFiles];
        self.game.autosaved = NO;
        [self runTerpNormal];
        return;
    }

    // Restore fullscreen state and the window frame from before fullscreen,
    // which we'll need if the user exits fullscreen later.
    self.inFullscreen = restoredControllerLate.inFullscreen;
    self.windowPreFullscreenFrame = restoredController.windowPreFullscreenFrame;
    self.shouldStoreScrollOffset = NO;

    // If the game had ended (interpreter process was dead) when autosaved,
    // we either show the finished game window (system restoration) or
    // start a fresh game (user-initiated launch).
    if (!restoredController.isAlive) {
        if (windowRestoredBySystem) {
            [self restoreWindowWhenDead];
        } else {
            // Otherwise we delete any autorestore files and
            // restart the game
            [self deleteAutosaveFiles];
            self.game.autosaved = NO;
            // If we die in fullscreen and close the game,
            // the game should not open in fullscreen the next time
            self.inFullscreen = NO;
            [self runTerpNormal];
        }
        return;
    }

    if (![self checkTerpAutosave]) {
        self.game.autosaved = NO;
        [self runTerpNormal];
        return;
    }

    // If we're in UI-only restore mode but the late autosave wasn't from the
    // first turn, the UI state won't match the fresh game state. Fall back to
    // a clean start.
    if (restoreUIOnly && restoredControllerLate.hasAutoSaved) {
        NSLog(@"restoredControllerLate was not saved at the first turn!");
        NSLog(@"Delete autosave files and start normally without restoring UI");
        restoreUIOnly = NO;
        [self deleteAutosaveFiles];
        self.game.autosaved = NO;
        [self runTerpNormal];
        return;
    }

    [self adoptRestoredTheme];
    [self adoptRestoredWindowFrame];

    [self adjustContentView];
    shouldRestoreUI = YES;
    [self forkInterpreterTask];

    // The interpreter must process several events (including arrange events)
    // before the Glk windows exist and are ready to receive restored UI state.
    // The actual UI restoration happens later in the GlkRequests category
    // when the interpreter reaches the appropriate NEXTEVENT.
}

// Read the GUI autosave and the late GUI autosave into restoredController
// and restoredControllerLate, and settle on a consistent pair: either may
// stand in for the other when it is missing, older or tagged differently.
// restoredController is left nil if neither is usable.
- (void)loadRestoredControllers {
    restoredController = [self unarchiveAutosaveFile:self.autosaveFileGUI];

    NSDate *terpAutosaveDate = [self creationDateOfFile:self.autosaveFileTerp];
    NSDate *GUIAutosaveDate = [self creationDateOfFile:self.autosaveFileGUI];
    if (terpAutosaveDate && GUIAutosaveDate && [terpAutosaveDate compare:GUIAutosaveDate] == NSOrderedDescending) {
        NSLog(@"GUI autosave file is created before terp autosave file!");
    }

    // Check if restored GUI is correct version
    if (restoredController && restoredController.autosaveVersion != AUTOSAVE_SERIAL_VERSION) {
        NSLog(@"GUI autosave file is wrong version! Wanted %d, got %ld. Deleting!", AUTOSAVE_SERIAL_VERSION, restoredController.autosaveVersion );
        restoredController = nil;
    }

    // The "late" autosave is a second GUI snapshot taken after the interpreter
    // has finished processing, capturing any additional state changes (like
    // text printed after the autosave point). If the main GUI autosave fails,
    // the late one can serve as a fallback.
    restoredControllerLate = nil;

    NSDate *GUILateAutosaveDate = nil;
    NSString *autosaveLatePath = [self appSupportFile:kAutosaveGUILateFile];

    if ([[NSFileManager defaultManager] fileExistsAtPath:autosaveLatePath]) {
        GUILateAutosaveDate = [self creationDateOfFile:autosaveLatePath];
        restoredControllerLate = [self unarchiveAutosaveFile:autosaveLatePath];
        if (!restoredController) {
            restoredController = restoredControllerLate;
            GUIAutosaveDate = GUILateAutosaveDate;
        }
    }

    // With no late autosave at all (the window was closed before the first
    // one was written, say), the ordinary one has to stand in for it.
    // Everything below reads the late controller without checking it, and
    // from a nil one gets an empty window frame, no sound handler, no style
    // hints and a hasAutoSaved of NO, which then passes for a first-turn save.
    if (!restoredControllerLate)
        restoredControllerLate = restoredController;

    if (GUIAutosaveDate && GUILateAutosaveDate && [GUIAutosaveDate compare:GUILateAutosaveDate] == NSOrderedDescending) {
        NSLog(@"GUI late autosave file is created before GUI autosave file!");
        NSLog(@"Do not use it.");
        restoredControllerLate = restoredController;
    }

    if (restoredController.autosaveTag != restoredControllerLate.autosaveTag) {
        NSLog(@"GUI late autosave tag does not match GUI autosave file tag!");
        NSLog(@"restoredController.autosaveTag %ld restoredControllerLate.autosaveTag: %ld", restoredController.autosaveTag, restoredControllerLate.autosaveTag);
        if (restoredController && restoredControllerLate.autosaveTag == 0)
            restoredControllerLate = restoredController;
        else
            restoredController = restoredControllerLate;
    }
}

// If the interpreter's autosave file exists, verify its tag matches the
// GUI autosave tag to ensure they represent the same game state. A mismatch
// means one was saved at a different point than the other, so we try falling
// back to a backup terp save or limiting restoration to UI-only.
//
// Sets restoreUIOnly. Returns NO if the game should start normally instead
// (autosave off, or the user chose never to autorestore).
- (BOOL)checkTerpAutosave {
    if (![[NSFileManager defaultManager] fileExistsAtPath:self.autosaveFileTerp]) {
        NSLog(@"No interpreter autorestore file exists");
        NSLog(@"Only restore UI state at first turn");
        restoreUIOnly = YES;
        return YES;
    }

    restoreUIOnly = NO;

    // A truncated or malformed archive (a plist half-written when the
    // machine lost power) makes the unarchiver raise, so read it
    // defensively like the GUI archives above (unarchiveAutosaveFile:
    // catches that). Without this the exception is uncaught at launch
    // and, since nothing removes the bad file, every relaunch retries the
    // same restoration: a crash loop.
    //
    // An unreadable library must then be rejected on the nil, NOT on the
    // tag: an archive that fails to decode reports tag 0, and a restored
    // GUI controller can legitimately carry tag 0 as well (observed), so
    // comparing tags alone would call the two "in sync" and feed a corrupt
    // library into the restore. Falling into the mismatch path below is
    // the right failure -- it tries the backup save, or deletes both files
    // and restores UI only.
    TempLibrary *tempLib = [self unarchiveAutosaveFile:self.autosaveFileTerp];
    if (!tempLib || tempLib.autosaveTag != restoredController.autosaveTag) {
        NSLog(@"The terp autosave is unreadable, or has a tag not matching the GUI autosave.");
        NSLog(@"The terp autosave tag: %u GUI autosave tag: %ld", tempLib.autosaveTag, restoredController.autosaveTag);
        NSLog(@"Trying to use previous terp save");

        if ([self usePreviousTerpAutosave]) {
            NSLog(@"Successfully used previous terp save");
        } else {
            NSLog(@"Only restore UI state at first turn");
            [self deleteFiles:@[ [NSURL fileURLWithPath:self.autosaveFileGUI isDirectory:NO],
                                 [NSURL fileURLWithPath:self.autosaveFileTerp isDirectory:NO] ]];
            restoreUIOnly = YES;
        }
        return YES;
    }

    // Only show the alert about autorestoring if this is not a system
    // window restoration, and the user has not suppressed it.
    if (windowRestoredBySystem)
        return YES;
    if (self.theme.autosave == NO)
        return NO;
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if ([defaults boolForKey:kAutorestoreAlertSuppressionKey]) {
        // Autorestore alert suppressed
        if (![defaults boolForKey:@"AlwaysAutorestore"]) {
            // The user has checked "Remember this choice" when
            // choosing to not autorestore
            [self deleteAutosaveFiles];
            return NO;
        }
    } else {
        self.shouldShowAutorestoreAlert = YES;
    }
    return YES;
}

// The interpreter autosave does not match the GUI one. If the backup
// interpreter autosave (-bak) does, move the backup pair into place.
// Returns YES if it did.
- (BOOL)usePreviousTerpAutosave {
    NSString *oldAutosaveFileTerp = [self appSupportFile:kAutosaveTerpBakFile];
    TempLibrary *tempLib = [self unarchiveAutosaveFile:oldAutosaveFileTerp];
    if (!tempLib || tempLib.autosaveTag != restoredController.autosaveTag)
        return NO;

    // Put the previous pair back, game state first. The library
    // carries the tag, so as long as the backup library is still
    // waiting under its -bak name, a crash in here leaves a
    // mismatch that the next launch finds and finishes. Library
    // first would instead leave the previous library beside the
    // current game state, with matching tags: two turns mixed
    // and nothing to tell.
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSError *error = nil;
    NSString *glkSaveTerp = [self appSupportFile:kAutosaveGlkSaveFile];
    NSString *oldGlkSaveTerp = [self appSupportFile:kAutosaveGlkSaveBakFile];

    // No backup game state means an earlier launch already put
    // it back and was interrupted before the library followed.
    BOOL gameStateBack = YES;
    if ([fileManager fileExistsAtPath:oldGlkSaveTerp]) {
        gameStateBack = [fileManager replaceItemAtURL:[NSURL fileURLWithPath:glkSaveTerp isDirectory:NO]
                                        withItemAtURL:[NSURL fileURLWithPath:oldGlkSaveTerp isDirectory:NO]
                                       backupItemName:nil
                                              options:0
                                     resultingItemURL:nil
                                                error:&error];
    }
    BOOL usedPrevious = NO;
    if (gameStateBack) {
        usedPrevious = [fileManager replaceItemAtURL:[NSURL fileURLWithPath:self.autosaveFileTerp isDirectory:NO]
                                       withItemAtURL:[NSURL fileURLWithPath:oldAutosaveFileTerp isDirectory:NO]
                                      backupItemName:nil
                                             options:0
                                    resultingItemURL:nil
                                               error:&error];
    }
    if (!usedPrevious)
        NSLog(@"Could not move previous terp save into place: %@", error);
    return usedPrevious;
}

// Temporarily switch to the theme that was active when the autosave was
// created, so the restored UI renders correctly. The original theme is
// stashed and will be restored after the autorestore process completes.
- (void)adoptRestoredTheme {
    Theme *theme = self.theme;
    self.stashedTheme = theme;
    self.restoredStylesStale = NO;
    Theme *restoredTheme = [self findThemeByName:restoredControllerLate.oldThemeName];
    if (restoredTheme && restoredTheme != theme) {
        NSLog(@"GlkController runtTerpWithAutorestore: Temporarily set theme to %@", restoredTheme.name);
        self.theme = restoredTheme;
        NSLog(@"GlkController runtTerpWithAutorestore: BufNormal font name to %@", restoredTheme.bufferNormal.font.displayName);
    } else {
        self.stashedTheme = nil;
        // If the autosaved theme name could not be resolved to an existing
        // theme, there was no temporary theme to render with, so the archived
        // window styles may belong to a different theme than the current one.
        // Flag this so the style rebuild in notePreferencesChanged is not
        // skipped during autorestore, which would otherwise leave the restored
        // windows showing stale fonts/colors (e.g. the default serif font when
        // the game is set to the DOSBox theme).
        self.restoredStylesStale = (restoredTheme == nil);
    }
}

// If this is not a window restoration done by the system, we re-enter
// fullscreen manually if the game was closed in fullscreen mode. Otherwise
// put the window back where it was.
- (void)adoptRestoredWindowFrame {
    if (!windowRestoredBySystem && self.inFullscreen
        && !self.isInSystemFullscreen) {
        self.startingInFullscreen = YES;
        [self startInFullscreen];
        return;
    }
    self.gameView.autoresizingMask = kResizeCenteredAtTop;
    [self.window setFrame:restoredControllerLate.storedWindowFrame display:YES];
    // A game restored in fullscreen keeps its width and stays centered,
    // as windowDidEnterFullScreen: would have it. The system may still
    // resize the window, when it applies its own saved state to it, and a
    // game view that follows the window's width while keeping its
    // fullscreen margins would be squeezed down to nothing.
    if (self.inFullscreen)
        self.gameView.autoresizingMask = kResizeCenteredFullHeight;
    else
        self.gameView.autoresizingMask = kResizeFill;
}

// Take over a quote box from the restored controller, attached to bufwin.
- (void)adoptQuoteBox:(GlkTextGridWindow *)quotebox
      forBufferWindow:(GlkTextBufferWindow *)bufwin
           addedOnPAC:(NSInteger)pac {
    if (self.quoteBoxes == nil)
        self.quoteBoxes = [[NSMutableArray alloc] init];
    [quotebox removeFromSuperview];
    [self.quoteBoxes addObject:quotebox];
    quotebox.glkctl = self;
    quotebox.quoteboxAddedOnPAC = pac;
    quotebox.quoteboxParent = bufwin.textview.enclosingScrollView;
    bufwin.quoteBox = quotebox;
}

- (void)restoreUI {
    // We try to restore the UI here, in order to catch things
    // like entered text and scrolling, that has changed the UI
    // but not sent any events to the interpreter process.
    // This method is called in handleRequest on NEXTEVENT.

    [self chooseRestoredControllerForCommandScript];

    shouldRestoreUI = NO;
    self.shouldStoreScrollOffset = NO;

    [self adoptRestoredSoundHandler];

    // Copy values from autorestored GlkController object
    self.firstResponderView = restoredControllerLate.firstResponderView;
    self.windowPreFullscreenFrame = [self frameWithSanitycheckedSize:restoredControllerLate.windowPreFullscreenFrame];
    self.autosaveTag = restoredController.autosaveTag;

    // Not when only the UI is restored: the interpreter is then starting the
    // game from scratch and sets its own style hints as it goes. This can run
    // in the middle of that (scarier gets here from its temporary loading
    // window, before it has opened the real ones), and swapping in the
    // archived hints would throw away the ones set so far.
    if (!restoreUIOnly) {
        // Before the windows are adopted: postRestoreAdjustments: stamps
        // their last new text with it.
        self.turns = restoredControllerLate.turns;
        if (restoredController.bufferStyleHints)
            self.bufferStyleHints = restoredController.bufferStyleHints;
        if (restoredController.gridStyleHints)
            self.gridStyleHints = restoredController.gridStyleHints;
    }

    // Restore frame size
    self.gameView.frame = restoredControllerLate.storedGameViewFrame;

    [self adoptRestoredWindows];

    // This makes autorestoring in fullscreen a little less flickery
    [self adjustContentView];

    if (restoredControllerLate.bgcolor)
        [self setBorderColor:restoredControllerLate.bgcolor];

    if (restoredController.commandScriptRunning) {
        [self.commandScriptHandler copyPropertiesFrom:restoredController.commandScriptHandler];
    }

    [self adoptRestoredJourneyMenus];
    [self finishRestoredWindows];

    if (!restoreUIOnly)
        self.hasAutoSaved = YES;

    restoredController = nil;
    restoredControllerLate = nil;
    restoreUIOnly = NO;
    windowsClosedBeforeRestore = nil;

    // We create a forced arrange event in order to force the interpreter process
    // to re-send us window sizes. The player may have changed settings that
    // affect window size since the autosave was created.
    //
    // This used to wait 0.2 seconds, to let the system finish resizing a
    // window it was restoring. The system no longer gets the window until
    // postRestoreArrange: hands it over, so the wait only held up the launch.
    // What the system does to the window afterwards is dealt with in
    // window:didDecodeRestorableState:.

    [self performSelector:@selector(postRestoreArrange:) withObject:nil afterDelay:0];
}

// Pick which archived controller restoreUI copies from. A UI-only restore
// always uses the late one. Otherwise, a command script that was running
// either had its last command handled (drop it) or continues from the late
// controller's state.
- (void)chooseRestoredControllerForCommandScript {
    if (restoreUIOnly) {
        restoredController = restoredControllerLate;
        self.shouldShowAutorestoreAlert = NO;
        return;
    }

    self.windowsToBeAdded = [[NSMutableArray alloc] init];

    if (restoredController.commandScriptRunning) {
        CommandScriptHandler *handler = restoredControllerLate.commandScriptHandler;
        if (handler.commandIndex >= handler.commandArray.count - 1) {
            restoredController.commandScriptHandler = nil;
            restoredController.commandScriptRunning = NO;
        } else {
            skipNextScriptCommand = YES;
            restoredController = restoredControllerLate;
        }
    }
}

- (void)adoptRestoredSoundHandler {
    // Stop whatever the pre-restore handler started before replacing it
    // wholesale. Sound requests can arrive between the fork and restoreUI, and
    // GlkSoundChannel.handler is weak, so in the common case the outgoing
    // handler simply deallocates and takes its players with it. A channel
    // mid-fade is the exception: init_fade schedules a repeating NSTimer with
    // target:self, so the run loop retains that channel after its handler is
    // gone. It would keep playing and ticking forever with a nil handler
    // (notifications silently dropped), and nothing could ever reach it --
    // stopAllAndCleanUp iterates only the live handler's channels. [stop]
    // runs [cleanup], which invalidates the timer and releases the channel.
    [self.soundHandler stopAllAndCleanUp];

    self.soundHandler = restoredControllerLate.soundHandler;
    self.soundHandler.glkctl = self;
    [self.soundHandler restartAll];
}

// Copy all views and GlkWindow objects from the restored controller (or, in
// a UI-only restore, keep our own windows and only take their frames).
- (void)adoptRestoredWindows {
    // Re-add the subviews in ascending window-name order, which is the order
    // the windows were originally created in (names are sequential creation
    // tags). addSubview: stacks later views on top, so this reproduces the
    // live creation z-order (flushDisplay adds _windowsToBeAdded in creation
    // order). Iterating allKeys directly would use an undefined dictionary
    // order, which can leave overlapping V6 windows mis-stacked -- e.g. the
    // Zork Zero encyclopedia caption (windows[3], created after the foreground
    // graphics window) ending up hidden behind graphics_fg_glk.
    NSArray *orderedKeys = [restoredController.gwindows.allKeys
                            sortedArrayUsingSelector:@selector(compare:)];
    for (NSNumber *key in orderedKeys) {
        GlkWindow *win;
        if (!restoreUIOnly) {
            if ([windowsClosedBeforeRestore containsObject:key]) {
                [(restoredController.gwindows)[key] removeFromSuperview];
                continue;
            }
            win = [self adoptRestoredWindow:key];
        } else {
            win = self.gwindows[key];
            // Ugly hack to make sure quote boxes shown at game start still appear when restoring UI only
            if (self.quoteBoxes.count == 0 && self.restorationHandler && [win isKindOfClass:[GlkTextBufferWindow class]]) {
                GlkWindow *restored = restoredController.gwindows[@(win.name)];
                GlkTextGridWindow *quotebox = ((GlkTextBufferWindow *)restored).quoteBox;
                if (quotebox)
                    [self adoptQuoteBox:quotebox
                        forBufferWindow:(GlkTextBufferWindow *)win
                             addedOnPAC:self.numberOfPrintsAndClears];
            }
        }
        GlkWindow *laterWin = (restoredControllerLate.gwindows)[key];
        win.frame = laterWin.frame;
    }
}

// Replace our window named key (if any) with the restored one.
- (GlkWindow *)adoptRestoredWindow:(NSNumber *)key {
    [self.gwindows[key] removeFromSuperview];

    GlkWindow *win = (restoredController.gwindows)[key];
    self.gwindows[@(win.name)] = win;

    if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
        NSScrollView *scrollview = ((GlkTextBufferWindow *)win).textview.enclosingScrollView;

        GlkTextGridWindow *aView = [self findGridWindowIn:scrollview];
        while (aView) {
            [aView removeFromSuperview];
            aView = [self findGridWindowIn:scrollview];
        }

        GlkTextGridWindow *quotebox = ((GlkTextBufferWindow *)win).quoteBox;
        if (quotebox && (restoredController.numberOfPrintsAndClears == quotebox.quoteboxAddedOnPAC) && (restoredController.printsAndClearsThisTurn == 0 || quotebox.quoteboxAddedOnPAC == 0)) {
            // Our own count of prints and clears starts over (and
            // already includes the scrollback the interpreter replayed
            // while restoring), so a count stamped on the box by the
            // previous session means nothing here. Zero makes the next
            // NEXTEVENT stamp it afresh; otherwise the box would fade
            // at once or outstay its turn, depending on which count
            // happened to be higher.
            [self adoptQuoteBox:quotebox
                forBufferWindow:(GlkTextBufferWindow *)win
                     addedOnPAC:0];
        }
    }

    [win removeFromSuperview];
    [self.gameView addSubview:win];

    win.glkctl = self;
    win.theme = self.theme;
    return win;
}

- (void)adoptRestoredJourneyMenus {
    if (self.gameID != kGameIsJourney || !restoredController.journeyMenuHandler)
        return;
    self.journeyMenuHandler = restoredController.journeyMenuHandler;
    restoredController.journeyMenuHandler = nil;
    self.journeyMenuHandler.glkctl = self;
    self.journeyMenuHandler.textGridWindow = (GlkTextGridWindow *)self.gwindows[@(self.journeyMenuHandler.gridTextWinName)];
    self.journeyMenuHandler.textBufferWindow = (GlkTextBufferWindow *)self.gwindows[@(self.journeyMenuHandler.bufferTextWinName)];

    [self.journeyMenuHandler showMenus];
}

// Restore scroll positions etc., focus the window that had it, and resend
// the command a running command script was in the middle of.
- (void)finishRestoredWindows {
    GlkWindow *winToGrabFocus = nil;

    for (GlkWindow *win in self.gwindows.allValues) {
        if (![win isKindOfClass:[GlkGraphicsWindow class]] && !self.windowsToRestore.count) {
            [win postRestoreAdjustments:(restoredControllerLate.gwindows)[@(win.name)]];
        }
        if (win.name == self.firstResponderView) {
            winToGrabFocus = win;
        }
        if (self.commandScriptRunning && win.name == self.commandScriptHandler.lastCommandWindow) {
            if (self.commandScriptHandler.lastCommandType == kCommandTypeLine) {
                [self.commandScriptHandler sendCommandLineToWindow:win];
            } else {
                [self.commandScriptHandler sendCommandKeyPressToWindow:win];
            }
        }
    }

    if (winToGrabFocus)
        [winToGrabFocus grabFocus];
}


- (void)postRestoreArrange:(id)sender {
    if (!self.startingInFullscreen) {
        [self performSelector:@selector(showAutorestoreAlert:) withObject:nil afterDelay:0.1];
    }

    Theme *stashedTheme = self.stashedTheme;
    if (stashedTheme && stashedTheme != self.theme) {
        self.theme = stashedTheme;
        self.stashedTheme = nil;
    }
    NSNotification *notification = [NSNotification notificationWithName:kPreferencesChangedNotification object:self.theme];
    [self notePreferencesChanged:notification];
    self.restoredStylesStale = NO;
    self.shouldStoreScrollOffset = YES;

    // Now we can actually show the window
    if (![self runWindowsRestorationHandler]) {
        [self showWindow:nil];
        [self.window makeKeyAndOrderFront:nil];
        [self.window makeFirstResponder:nil];
    }
    if (self.startingInFullscreen) {
        [self performSelector:@selector(deferredEnterFullscreen:) withObject:nil afterDelay:0.1];
    } else {
        autorestoring = NO;
    }
}


// The per-game signature used to name the autosave directory. Falls back to
// the hash stored in the library when the game file itself cannot be read,
// e.g. an evicted iCloud file. This value is also passed to the interpreter
// process (which has no library to fall back on) so that both processes
// always agree on the directory.
- (NSString *)gameSignature {
    NSString *signature = self.gamefile.signatureFromFile;
    if (signature.length == 0)
        signature = self.game.hashTag;
    return signature;
}

- (NSString *)buildAppSupportDir {
    static NSDictionary *gFolderMap;
    static NSDictionary *gFolderMapExt;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        gFolderMap = @{@"adrift" : @"SCARE",
                       @"advsys" : @"AdvSys",
                       @"agt"    : @"AGiliTy",
                       @"archetype" : @"Archetype",
                       @"comprehend" : @"Comprehend",
                       @"glulx"  : @"Glulxe",
                       @"hugo"   : @"Hugo",
                       @"level9" : @"Level 9",
                       @"magscrolls" : @"Magnetic",
                       @"quest4" : @"Quest",
                       // Both Quest versions share one folder: the Question terp runs
                       // both and cannot distinguish them when naming its own
                       // work/autosave dir (glkimp fileref.m keys off the program
                       // name's first word, "quest" for either). Per-game signature
                       // subfolders keep individual games separate inside it.
                       @"quest5" : @"Quest",
                       @"quill"  : @"UnQuill",
                       @"tads2"  : @"TADS",
                       @"tads3"  : @"TADS",
                       @"zcode"  : @"Bocfel",
                       @"sagaplus" : @"Plus",
                       @"scott"  : @"ScottFree",
                       @"jacl"   : @"JACL",
                       @"taylor" : @"TaylorMade"
        };

        // Formats not in gFolderMap are looked up by file extension
        gFolderMapExt = @{@"acd" : @"Alan 2",
                          @"a3c" : @"Alan 3",
                          @"d$$" : @"AGiliTy"};
    });

    NSError *error;
    NSURL *appSupportURL = [[NSFileManager defaultManager]
                            URLForDirectory:NSApplicationSupportDirectory
                            inDomain:NSUserDomainMask
                            appropriateForURL:nil
                            create:NO
                            error:&error];

    if (error)
        NSLog(@"Could not find Application Support folder. Error: %@",
              error);

    if (appSupportURL == nil)
        return nil;

    Game *game = self.game;
    NSString *detectedFormat = game.detectedFormat;

    if (!detectedFormat) {
        NSLog(@"GlkController appSupportDir: Game %@ has no specified format!", game.metadata.title);
        return nil;
    }

    NSString *signature = self.gameSignature;

    if (signature.length == 0) {
        NSLog(@"GlkController appSupportDir: Could not create signature from game file \"%@\"!", self.gamefile);
        return nil;
    }

    NSString *terpName = gFolderMap[detectedFormat];
    if (!terpName)
        terpName = gFolderMapExt[self.gamefile.pathExtension.lowercaseString];

    if (!terpName) {
        NSLog(@"GlkController appSupportDir: Could not map game format %@ to a folder name!", detectedFormat);
        return nil;
    }

    NSString *terpFolder = [terpName stringByAppendingString:@" Files"];

    NSString *dirstr =
    [@"Spatterlight" stringByAppendingPathComponent:terpFolder];
    dirstr = [dirstr stringByAppendingPathComponent:@"Autosaves"];
    dirstr = [dirstr
              stringByAppendingPathComponent:signature];
    dirstr = [dirstr stringByAddingPercentEncodingWithAllowedCharacters:[NSCharacterSet URLPathAllowedCharacterSet]];

    appSupportURL = [NSURL URLWithString:dirstr
                           relativeToURL:appSupportURL];

    if (self.supportsAutorestore && self.theme.autosave) {
        error = nil;
        BOOL succeed  = [[NSFileManager defaultManager] createDirectoryAtURL:appSupportURL
                                 withIntermediateDirectories:YES
                                                  attributes:nil
                                                       error:&error];

        if (!succeed || error != nil) {
            NSLog(@"Error when creating appSupportDir: %@", error);
        }

        NSString *dummyfilename = [game.metadata.title
                                   stringByAppendingPathExtension:@"txt"];

        NSString *dummytext = [NSString
                               stringWithFormat:
                                   @"This file, %@, was placed here by Spatterlight in order to make "
                               @"it easier for humans to guess what game these autosave files belong "
                               @"to. Any files in this folder are for the game %@, or possibly "
                               @"a game with a different name but identical contents.",
                               dummyfilename, game.metadata.title];

        NSString *dummyfilepath =
        [appSupportURL.path stringByAppendingPathComponent:dummyfilename];

        error = nil;
        succeed =
        [dummytext writeToURL:[NSURL fileURLWithPath:dummyfilepath isDirectory:NO]
                   atomically:YES
                     encoding:NSUTF8StringEncoding
                        error:&error];
        if (!succeed || error) {
            NSLog(@"Failed to write dummy file to autosave directory. Error:%@",
                  error);
        }
    }
    return appSupportURL.path;
}

// If there exists an autosave dir using the old hashing method,
// copy any files from it to the new one, unless there are newer
// equivalents in the new-style autosave directory. Then delete the
// old files and the old directory.

// Migration shim: autosaves used to live in a directory named after the
// game's old file signature. Move anything found there into the current
// directory (unless a newer file of the same name is already there) and
// remove the old directory.
- (void)dealWithOldFormatAutosaveDir {
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSError *error = nil;

    NSURL *newDirURL = [NSURL fileURLWithPath:self.appSupportDir isDirectory:YES];

    // Create the URL an old-format autosave directory would have
    NSString *oldDirPath = [self.appSupportDir stringByDeletingLastPathComponent];
    oldDirPath = [oldDirPath stringByAppendingPathComponent:self.gamefile.oldSignatureFromFile];
    NSURL *oldDirURL = [NSURL fileURLWithPath:oldDirPath isDirectory:YES];

    NSArray<NSURL *> *files = [fileManager contentsOfDirectoryAtURL:oldDirURL includingPropertiesForKeys:nil options:0 error:&error];
    if (!files.count)
        return;

    for (NSURL *url in files)
        [self migrateOldFormatAutosaveFile:url toDirectory:newDirURL];

    // Delete the old directory
    error = nil;
    if (![fileManager removeItemAtURL:oldDirURL error:&error]) {
        NSLog(@"GlkController: Failed to delete old autosave directory at %@. Error:%@", oldDirURL.path, error);
    }

    // Re-write the late UI autosave, to give it a creation date later than autosave-GUI.plist
    // (Our autorestore code will compare the creation dates of autosave-GUI.plist and
    // autosave-GUI-late.plist.)
    NSURL *lateUIURL = [newDirURL URLByAppendingPathComponent:kAutosaveGUILateFile];
    NSData *lateUISave = [NSData dataWithContentsOfURL:lateUIURL options:NSDataReadingMappedAlways error:&error];
    if (lateUISave) {
        [lateUISave writeToURL:lateUIURL options:NSDataWritingAtomic error:&error];
    }
}

// Copy one file from the old autosave directory into newDirURL, unless a file
// of the same name created later is already there, then delete the original.
// The original is kept if the copy fails.
- (void)migrateOldFormatAutosaveFile:(NSURL *)url toDirectory:(NSURL *)newDirURL {
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSError *error = nil;
    NSString *filename = url.lastPathComponent;

    NSData *data = [NSData dataWithContentsOfURL:url options:NSDataReadingMappedAlways error:&error];
    if (!data) {
        NSLog(@"GlkController: Could not read file in old autosave directory. Error: %@", error);
        return;
    }

    NSDate *dateInOld = nil;
    error = nil;
    NSDictionary *attrs = [fileManager attributesOfItemAtPath:url.path error:&error];
    if (attrs) {
        dateInOld = (NSDate *)attrs[NSFileCreationDate];
    } else {
        NSLog(@"GlkController: Failed to get creation date of file %@ in old directory. Error:%@", url.path, error);
    }

    NSURL *newfileURL = [newDirURL URLByAppendingPathComponent:filename];
    BOOL overwrite = YES;

    error = nil;
    attrs = [fileManager attributesOfItemAtPath:newfileURL.path error:&error];
    if (attrs) {
        NSDate *dateInNew = (NSDate *)attrs[NSFileCreationDate];
        if (dateInNew && dateInOld && [dateInNew compare:dateInOld] == NSOrderedDescending) {
            NSLog(@"%@: Newer file exists in new directory. Don't overwrite! dateInNew:%@ dateInOld:%@", filename, dateInNew, dateInOld);
            overwrite = NO;
        }
    } else {
        NSLog(@"GlkController: Failed to get creation date of file %@ in new directory. Error:%@", url.path, error);
    }

    if (overwrite && ![data writeToURL:newfileURL options:NSDataWritingAtomic error:&error]) {
        NSLog(@"GlkController: Failed to copy file %@ to new autosave directory. Error:%@", filename, error);
        return;
    }

    if (![fileManager removeItemAtURL:url error:&error]) {
        NSLog(@"GlkController: Failed to delete file %@ in old autosave directory. Error:%@", filename, error);
    }
}

- (NSString *)appSupportFile:(NSString *)name {
    return [self.appSupportDir stringByAppendingPathComponent:name];
}

- (NSString *)autosaveFileGUI {
    if (!_autosaveFileGUI)
        _autosaveFileGUI = [self appSupportFile:kAutosaveGUIFile];
    return _autosaveFileGUI;
}

- (NSString *)autosaveFileTerp {
    if (!_autosaveFileTerp)
        _autosaveFileTerp = [self appSupportFile:kAutosaveTerpFile];
    return _autosaveFileTerp;
}

- (NSDate *)creationDateOfFile:(NSString *)path {
    NSError *error = nil;
    NSDictionary *attrs = [[NSFileManager defaultManager] attributesOfItemAtPath:path error:&error];
    if (!attrs)
        NSLog(@"Could not get creation date of %@: %@", path.lastPathComponent, error);
    return attrs[NSFileCreationDate];
}

// Unarchive an autosave plist. A truncated or malformed archive (one
// half-written when the machine lost power) makes the unarchiver raise, so
// catch that and return nil.
- (id)unarchiveAutosaveFile:(NSString *)path {
    NSError *error = nil;
    NSData *data = [NSData dataWithContentsOfFile:path options:0 error:&error];
    if (!data)
        return nil;
    id result = nil;
    @try {
        NSKeyedUnarchiver *unarchiver = [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:&error];
        unarchiver.requiresSecureCoding = NO;
        result = [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey];
        [unarchiver finishDecoding];
    } @catch (NSException *ex) {
        NSLog(@"Unable to read autosave file %@: %@", path.lastPathComponent, ex);
        result = nil;
    }
    if (!result && error)
        NSLog(@"Unable to read autosave file %@: %@", path.lastPathComponent, error);
    return result;
}

// TableViewController calls this to reset non-running games
- (void)deleteAutosaveFilesForGame:(Game *)aGame {
    self.gamefile = aGame.urlForBookmark.path;
    aGame.autosaved = NO;
    if (!self.gamefile)
        return;
    self.game = aGame;
    [self deleteAutosaveFiles];
}

- (void)deleteAutosaveFiles {
    [self deleteAutosaveFilesKeepingLateGUI:NO];
}

// When the game has ended we keep the late GUI archive, so that the finished
// game window can be shown again, but nothing that would restore the game.
- (void)deleteAutosaveFilesKeepingLateGUI:(BOOL)keepLate {
    if (self.appSupportDir.length == 0)
        return;
    NSMutableArray<NSString *> *names =
    @[ kAutosaveTerpFile, kAutosaveTerpBakFile, kAutosaveTerpTmpFile,
       kAutosaveGlkSaveFile, kAutosaveGlkSaveBakFile, kAutosaveGlkSaveTmpFile,
       kAutosaveGUIFile ].mutableCopy;
    if (!keepLate)
        [names addObject:kAutosaveGUILateFile];

    NSMutableArray<NSURL *> *urls = [[NSMutableArray alloc] initWithCapacity:names.count];
    for (NSString *name in names)
        [urls addObject:[NSURL fileURLWithPath:[self appSupportFile:name] isDirectory:NO]];
    [self deleteFiles:urls];
}

- (void)deleteFiles:(NSArray<NSURL *> *)urls {
    for (NSURL *url in urls)
        [[NSFileManager defaultManager] removeItemAtURL:url error:nil];
}

// Ask the interpreter for one last autosave and wait for it, so that the
// GUI archive autoSaveOnExit writes next matches an interpreter save taken
// now. Glulxe paces its timer-driven saves, so without this the interpreter
// side could be up to that interval behind the window at the moment it
// closes, and an autorestore would run that interval of the game again.
// The request can only go out while the interpreter is waiting in
// glk_select; if it is mid-turn we read its output until it asks for an
// event. The wait is bounded so a busy game cannot hold the window open.
- (void)requestFinalAutosave {
    if (!self.supportsAutorestore || !self.theme.autosave || dead || !task || !task.isRunning)
        return;
    if (waitforfilename)
        return; /* blocked in a file prompt, not in glk_select */

    // Whatever was queued for the game is moot now: the next event it gets
    // after the save is the quit, so the save stays its last state.
    [self.queue removeAllObjects];

    int readfd = readfh.fileDescriptor;
    NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:2.0];
    BOOL sent = NO;
    while (deadline.timeIntervalSinceNow > 0) {
        if (waitforevent) {
            if (sent)
                return; /* saved (or nothing to save) and waiting again */
            GlkEvent *evt = [[GlkEvent alloc] initAutosaveEvent];
            [evt writeEvent:sendfh.fileDescriptor];
            waitforevent = NO;
            sent = YES;
        }
        if (waitForData(readfd, 50))
            [self noteDataAvailable:nil];
        if (dead)
            return;
    }
    NSLog(@"requestFinalAutosave: interpreter did not answer in time (request %s)", sent ? "sent" : "not sent");
}

// Archive the GUI state to path. Returns NO if archiving or writing failed.
- (BOOL)writeGUIAutosaveToFile:(NSString *)path {
    NSError *error = nil;
    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:self requiringSecureCoding:NO error:&error];
    if (!data || ![data writeToFile:path options:NSDataWritingAtomic error:&error]) {
        NSLog(@"Could not write GUI autosave %@: %@", path.lastPathComponent, error.localizedDescription);
        return NO;
    }
    return YES;
}

- (void)autoSaveOnExit {
    if (self.supportsAutorestore && self.theme.autosave && self.appSupportDir.length) {
        if (![self writeGUIAutosaveToFile:[self appSupportFile:kAutosaveGUILateFile]])
            return;

        self.game.autosaved = !dead;
    } else {
        [self deleteAutosaveFiles];
        if (self.game && self.game.managedObjectContext)
            self.game.autosaved = NO;
    }
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    [NSKeyedUnarchiver setClass:[BufferTextView class] forClassName:@"MyTextView"];
    [NSKeyedUnarchiver setClass:[GridTextView class] forClassName:@"MyGridTextView"];

    self = [super initWithCoder:decoder];
    if (self) {
        dead = [decoder decodeBoolForKey:@"dead"];
        waitforevent = NO;
        waitforfilename = NO;

        /* the glk objects */

        self.autosaveVersion = [decoder decodeIntegerForKey:@"version"];
        self.hasAutoSaved = [decoder decodeBoolForKey:@"hasAutoSaved"];
        self.autosaveTag =  [decoder decodeIntegerForKey:@"autosaveTag"];

        self.gwindows = [decoder decodeObjectOfClass:[NSMutableDictionary class] forKey:@"gwindows"];

        self.soundHandler = [decoder decodeObjectOfClass:[SoundHandler class] forKey:@"soundHandler"];
        // No imageHandler: it is not archived (see encodeWithCoder). Older
        // archives still carry the key; leaving it undecoded is harmless.

        self.storedWindowFrame = [decoder decodeRectForKey:@"windowFrame"];
        self.windowPreFullscreenFrame =
        [decoder decodeRectForKey:@"windowPreFullscreenFrame"];

        self.storedGameViewFrame = [decoder decodeRectForKey:@"contentFrame"];

        self.bgcolor = [decoder decodeObjectOfClass:[NSColor class] forKey:@"backgroundColor"];

        self.bufferStyleHints = [decoder decodeObjectOfClass:[NSMutableArray class] forKey:@"bufferStyleHints"];
        self.gridStyleHints = [decoder decodeObjectOfClass:[NSMutableArray class] forKey:@"gridStyleHints"];

        self.firstResponderView = [decoder decodeIntegerForKey:@"firstResponder"];
        self.inFullscreen = [decoder decodeBoolForKey:@"fullscreen"];

        self.turns = [decoder decodeIntegerForKey:@"turns"];
        self.numberOfPrintsAndClears = [decoder decodeIntegerForKey:@"printsAndClears"];
        self.printsAndClearsThisTurn = [decoder decodeIntegerForKey:@"printsAndClearsThisTurn"];

        self.oldThemeName = [decoder decodeObjectOfClass:[NSString class] forKey:@"oldThemeName"];

        self.showingCoverImage = [decoder decodeBoolForKey:@"showingCoverImage"];

        self.commandScriptRunning = [decoder decodeBoolForKey:@"commandScriptRunning"];
        if (self.commandScriptRunning)
            self.commandScriptHandler = [decoder decodeObjectOfClass:[CommandScriptHandler class] forKey:@"commandScriptHandler"];

        self.journeyMenuHandler = [decoder decodeObjectOfClass:[JourneyMenuHandler class] forKey:@"journeyMenuHandler"];
        if (self.journeyMenuHandler)
            self.journeyMenuHandler.glkctl = self;
        restoredController = nil;
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];

    [encoder encodeInteger:AUTOSAVE_SERIAL_VERSION forKey:@"version"];
    [encoder encodeBool:self.hasAutoSaved forKey:@"hasAutoSaved"];
    [encoder encodeInteger:self.autosaveTag forKey:@"autosaveTag"];

    [encoder encodeBool:dead forKey:@"dead"];
    [encoder encodeRect:self.window.frame forKey:@"windowFrame"];
    [encoder encodeRect:self.gameView.frame forKey:@"contentFrame"];

    [encoder encodeObject:self.bgcolor forKey:@"backgroundColor"];

    [encoder encodeObject:self.bufferStyleHints forKey:@"bufferStyleHints"];
    [encoder encodeObject:self.gridStyleHints forKey:@"gridStyleHints"];

    [encoder encodeObject:self.gwindows forKey:@"gwindows"];
    [encoder encodeObject:self.soundHandler forKey:@"soundHandler"];
    // The imageHandler is deliberately NOT archived. It used to be encoded
    // here and decoded in initWithCoder, but restoreUI never transferred it to
    // the live controller, so the decoded copy was pure dead weight in every
    // autosave. Adopting it instead would be the riskier repair: bocfel's V6
    // images live in per-session temp files whose paths are meaningless to the
    // next process, and interpreters re-establish their images after a restore
    // anyway (verified with Zork Zero: the restored window is pixel-identical,
    // border art, compass and inline picture intact, with a freshly built
    // handler). Blorb resources are re-cached at launch from the game file.
    [encoder encodeObject:self.journeyMenuHandler forKey:@"journeyMenuHandler"];

    [encoder encodeRect:self.windowPreFullscreenFrame
                 forKey:@"windowPreFullscreenFrame"];

    NSInteger firstResponderView = -1;

    NSResponder *firstResponder = self.window.firstResponder;

    if ([firstResponder isKindOfClass:[GlkWindow class]]) {
        firstResponderView = ((GlkWindow *)firstResponder).name;
    } else {
        id delegate = nil;
        if ([firstResponder isKindOfClass:[NSTextView class]]) {
            delegate = ((NSTextView *)firstResponder).delegate;
            if (![delegate isKindOfClass:[GlkWindow class]]) {
                delegate = nil;
            }
        }
        if (delegate) {
            firstResponderView = ((GlkWindow *)delegate).name;
        }
    }
    [encoder encodeInteger:firstResponderView forKey:@"firstResponder"];
    [encoder encodeBool:self.isInSystemFullscreen forKey:@"fullscreen"];

    [encoder encodeInteger:self.turns forKey:@"turns"];
    [encoder encodeInteger:self.numberOfPrintsAndClears forKey:@"printsAndClears"];
    [encoder encodeInteger:self.printsAndClearsThisTurn forKey:@"printsAndClearsThisTurn"];

    [encoder encodeObject:self.theme.name forKey:@"oldThemeName"];

    [encoder encodeBool:self.showingCoverImage forKey:@"showingCoverImage"];

    [encoder encodeBool:self.commandScriptRunning forKey:@"commandScriptRunning"];
    if (self.commandScriptRunning)
        [encoder encodeObject:self.commandScriptHandler forKey:@"commandScriptHandler"];
}

- (void)showAutorestoreAlert:(id)userInfo {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *alertSuppressionKey = kAutorestoreAlertSuppressionKey;
    NSString *alwaysAutorestoreKey = @"AlwaysAutorestore";

    if ([defaults boolForKey:alertSuppressionKey] == YES)
        self.shouldShowAutorestoreAlert = NO;

    if (!self.shouldShowAutorestoreAlert) {
        self.mustBeQuiet = NO;
        [self.journeyMenuHandler recreateDialog];
        return;
    }

    if (dead)
        return;

    self.mustBeQuiet = YES;

    NSAlert *anAlert = [NSAlert alertWithMessage:NSLocalizedString(@"This game was automatically restored from a previous session.", nil)
                                            info:NSLocalizedString(@"Would you like to start over instead?", nil)
                                         buttons:@[NSLocalizedString(@"Continue", nil), NSLocalizedString(@"Restart", nil)]];
    [anAlert showSuppressionButtonWithTitle:NSLocalizedString(@"Remember this choice.", nil)];

    GlkController * __weak weakSelf = self;

    [anAlert beginSheetModalForWindow:self.window completionHandler:^(NSInteger result) {

        weakSelf.mustBeQuiet = NO;

        if (anAlert.suppressionButton.state == NSOnState) {
            // Suppress this alert from now on
            [defaults setBool:YES forKey:alertSuppressionKey];
        }

        weakSelf.shouldShowAutorestoreAlert = NO;

        if (result == NSAlertSecondButtonReturn) {
            [weakSelf reset:nil];
            if (anAlert.suppressionButton.state == NSOnState) {
                [defaults setBool:NO forKey:alwaysAutorestoreKey];
            }
            return;
        } else {
            if (anAlert.suppressionButton.state == NSOnState) {
                [defaults setBool:YES forKey:alwaysAutorestoreKey];
            }
        }

        if (weakSelf.gameID == kGameIsJourney)
            [weakSelf.journeyMenuHandler recreateDialog];

        [weakSelf forceSpeech];
    }];
}

- (IBAction)reset:(id)sender {
    if (self.lastResetTimestamp && self.lastResetTimestamp.timeIntervalSinceNow < -1) {
        restartingAlready = NO;
    }

    if (restartingAlready || self.showingCoverImage)
        return;

    restartingAlready = YES;
    self.lastResetTimestamp = [NSDate date];
    self.mustBeQuiet = YES;

    [[NSNotificationCenter defaultCenter]
     removeObserver:self name: NSFileHandleDataAvailableNotification
     object: readfh];

    self.commandScriptHandler = nil;
    VoiceOverHelperDetach(_journeyMenuHandler);
    _journeyMenuHandler = nil;

    // Stop the interpreter
    [self terminateTask];
    task = nil;

    [self performSelector:@selector(deferredRestart:) withObject:nil afterDelay:0.3];
}

- (void)deferredRestart:(id)sender {
    [self cleanup];
    [self runTerp:(NSString *)self.terpname
         withGame:(Game *)self.game
            reset:YES
       restorationHandler:nil];

    [self.window makeKeyAndOrderFront:nil];
    [self.window makeFirstResponder:nil];
    [self guessFocus];
    NSAccessibilityPostNotification(self.window.firstResponder, NSAccessibilityFocusedUIElementChangedNotification);
}

-(void)cleanup {
    [self.soundHandler stopAllAndCleanUp];
    [self deleteAutosaveFiles];
    [self handleSetTimer:0];

    for (GlkWindow *win in self.gwindows.allValues) {
        win.glkctl = nil;
        if ([win isKindOfClass:[GlkTextBufferWindow class]])
            ((GlkTextBufferWindow *)win).quoteBox = nil;
        [win removeFromSuperview];
    }

    for (GlkTextGridWindow *win in self.quoteBoxes) {
        win.glkctl = nil;
        win.quoteboxParent = nil;
        [win removeFromSuperview];
    }

    VoiceOverHelperDetach(self.form);
    self.form = nil;
    VoiceOverHelperDetach(self.zmenu);
    self.zmenu = nil;

    readfh = nil;
    sendfh = nil;
    task = nil;
}

- (void)handleAutosave:(NSInteger)hash {
    self.autosaveTag = hash;

    @autoreleasepool {
        if (![self writeGUIAutosaveToFile:self.autosaveFileGUI])
            return;
    }

    self.hasAutoSaved = YES;
}

@end
