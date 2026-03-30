#import <QuartzCore/QuartzCore.h>
#import <CoreImage/CoreImage.h>

#import "AppDelegate.h"
#import "BufferTextView.h"
#import "BureaucracyForm.h"
#import "CommandScriptHandler.h"
#import "Constants.h"
#import "CoverImageHandler.h"
#import "CoverImageView.h"
#import "FolderAccess.h"
#import "GlkController.h"
#import "GlkGraphicsWindow.h"
#import "GlkEvent.h"
#import "GlkSoundChannel.h"
#import "GlkStyle.h"
#import "GlkTextBufferWindow.h"
#import "GlkTextGridWindow.h"
#import "GridTextView.h"
#import "InfoController.h"
#import "InputTextField.h"
#import "ImageHandler.h"
#import "LibController.h"
#import "Metadata.h"
#import "NotificationBezel.h"
#import "Preferences.h"
#import "RotorHandler.h"
#import "SoundHandler.h"
#import "TableViewController.h"
#import "ZMenu.h"
#import "JourneyMenuHandler.h"
#import "InfocomV6MenuHandler.h"

#import "GlkController+Autorestore.h"
#import "GlkController+BorderColor.h"
#import "GlkController+FullScreen.h"
#import "GlkController+InterpreterGlue.h"
#import "GlkController+GlkRequests.h"
#import "GlkController+Speech.h"

#import "Game.h"
#import "Theme.h"
#import "Image.h"
#import "Fetches.h"

#import "NSColor+integer.h"
#import "NSData+Categories.h"
#import "NSString+Categories.h"

#include "glkimp.h"
#include "protocol.h"


// In release builds, suppress NSLog entirely to avoid unnecessary logging overhead.
#ifndef DEBUG
#define NSLog(...)
#endif

//static const char *msgnames[] = {
//    "NOREPLY",         "OKAY",             "ERROR",       "HELLO",
//    "PROMPTOPEN",      "PROMPTSAVE",       "NEWWIN",      "DELWIN",
//    "SIZWIN",          "CLRWIN",           "MOVETO",      "PRINT",
//    "UNPRINT",         "MAKETRANSPARENT",  "STYLEHINT",   "CLEARHINT",
//    "STYLEMEASURE",    "SETBGND",          "REFRESH",     "SETTITLE",
//    "AUTOSAVE",        "RESET",            "BANNERCOLS",  "BANNERLINES",
//    "TIMER",           "INITCHAR",         "CANCELCHAR",
//    "INITLINE",        "CANCELLINE",       "SETECHO",     "TERMINATORS",
//    "INITMOUSE",       "CANCELMOUSE",      "FILLRECT",    "FINDIMAGE",
//    "LOADIMAGE",       "SIZEIMAGE",        "DRAWIMAGE",   "FLOWBREAK",
//    "NEWCHAN",         "DELCHAN",          "FINDSOUND",   "LOADSOUND",
//    "SETVOLUME",       "PLAYSOUND",        "STOPSOUND",   "PAUSE",
//    "UNPAUSE",         "BEEP",
//    "SETLINK",         "INITLINK",         "CANCELLINK",  "SETZCOLOR",
//    "SETREVERSE",      "QUOTEBOX",         "SHOWERROR",   "CANPRINT",
//    "PURGEIMG",        "MENU",
//
//    "NEXTEVENT",       "EVTARRANGE",       "EVTREDRAW",   "EVTLINE",
//    "EVTKEY",          "EVTMOUSE",         "EVTTIMER",    "EVTHYPER",
//    "EVTSOUND",        "EVTVOLUME",        "EVTPREFS",    "EVTQUIT" };

//static const char *wintypenames[] = {"wintype_AllTypes", "wintype_Pair",
//    "wintype_Blank",    "wintype_TextBuffer",
//    "wintype_TextGrid", "wintype_Graphics"};
//

// static const char *stylehintnames[] =
//{
//    "stylehint_Indentation", "stylehint_ParaIndentation",
//    "stylehint_Justification", "stylehint_Size",
//    "stylehint_Weight","stylehint_Oblique", "stylehint_Proportional",
//    "stylehint_TextColor", "stylehint_BackColor", "stylehint_ReverseColor",
//    "stylehint_NUMHINTS"
//};

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


// GlkHelperView is the content area that hosts all Glk windows (text buffers,
// text grids, and graphics windows). It uses a flipped coordinate system
// (origin at top-left) to match the Glk layout model, and forwards resize
// events to GlkController so the interpreter can be notified of arrange events.
@implementation GlkHelperView

// Use top-left origin to match Glk's coordinate system.
- (BOOL)isFlipped {
    return YES;
}

// Notify the controller when the frame changes programmatically (not during
// a live resize drag, which is handled separately to avoid excessive events).
- (void)setFrame:(NSRect)frame {
    super.frame = frame;
    GlkController *glkctl = _glkctrl;

    if (glkctl.alive && !self.inLiveResize && !glkctl.ignoreResizes) {
        [glkctl contentDidResize:frame];
    }
}

// Capture scroll offsets before a user-initiated live resize begins,
// so they can be restored afterward to prevent text from jumping around.
- (void)viewWillStartLiveResize {
    GlkController *glkctl = _glkctrl;
    if ((glkctl.window.styleMask & NSWindowStyleMaskFullScreen) !=
        NSWindowStyleMaskFullScreen && !glkctl.ignoreResizes) {
        [glkctl storeScrollOffsets];
    }
}

// After the user finishes dragging to resize, send the final content size
// to the controller and restore the previously saved scroll offsets.
- (void)viewDidEndLiveResize {
    GlkController *glkctl = _glkctrl;
    // We use a custom fullscreen width, so don't resize to full screen width
    // when viewDidEndLiveResize is called because we just entered fullscreen
    if ((glkctl.window.styleMask & NSWindowStyleMaskFullScreen) !=
        NSWindowStyleMaskFullScreen && !glkctl.ignoreResizes) {
        [glkctl contentDidResize:self.frame];
        [glkctl restoreScrollOffsets];
    }
}

@end

#import "GlkController_Private.h"

@implementation GlkController

@synthesize autosaveFileGUI = _autosaveFileGUI;
@synthesize autosaveFileTerp = _autosaveFileTerp;

// Lazily builds and caches the per-game application support directory path.
// Each game gets its own subdirectory for autosave files, preferences, etc.
- (NSString *)appSupportDir {
    if (!_appSupportDir) {
        _appSupportDir = [self buildAppSupportDir];
    }
    return _appSupportDir;
}

+ (BOOL) supportsSecureCoding {
    return YES;
}

/*
 * This is the real initializer.
 */

#pragma mark Initialization

// shouldReset means that we have killed the interpreter process and want to
// start the game anew, deleting any existing autosave files. This reuses the
// game window and should not resize it. A reset may be initiated by the user
// from the file menu or the autorestore alert at game start, or may occur
// automatically when the game has reached its end, crashed or when an
// autorestore attempt failed.

// windowRestoredBySystem means that the game was running when
// the application was last closed, and that its window was restored
// by the AppDelegate restoreWindowWithIdentifier method. The main
// difference from the manual autorestore that occurs when the user
// clicks on a game in the library window or similar, is that
// fullscreen is handled automatically

- (void)runTerp:(NSString *)terpname_
       withGame:(Game *)game_
          reset:(BOOL)shouldReset
restorationHandler:(nullable void (^)(NSWindow *, NSError *))completionHandler {

    if (!game_) {
        NSLog(@"GlkController runTerp called with nil game!");
        return;
    }
    autorestoring = NO;

    // Initialize the handlers that manage sound playback and image caching
    // for the game session. These are created fresh on each run.
    _soundHandler = [SoundHandler new];
    _soundHandler.glkctl = self;
    _imageHandler = [ImageHandler new];
    _infocomV6MenuHandler = nil;

    // We could use separate versioning for GUI and interpreter autosaves,
    // but it is probably simpler this way
    _autosaveVersion = AUTOSAVE_SERIAL_VERSION;

    _ignoreResizes = YES;

    skipNextScriptCommand = NO;

    _game = game_;
    Game *game = _game;

    [Preferences changeCurrentGlkController:self];
    [self noteColorModeChanged:nil];

    libcontroller = ((AppDelegate *)NSApp.delegate).tableViewController;

    [self.window registerForDraggedTypes:@[ NSPasteboardTypeURL, NSPasteboardTypeString]];

    if (!_theme.name) {
        NSLog(@"GlkController runTerp called with theme without name!");
        if (Preferences.currentTheme)
            game.theme = [game.managedObjectContext objectWithID:Preferences.currentTheme.objectID];
        _theme = game.theme;
    }

    // Identify the specific game being played so we can apply game-specific
    // workarounds and special behaviors (e.g., Narcolepsy window mask,
    // Beyond Zork font handling, Journey menu system). The "nohacks" theme
    // option disables all game-specific behavior.
    if (_theme.nohacks) {
        [self resetGameIdentification];
    } else {
        [self identifyGame:game.ifid];
    }

    _gamefile = _gameFileURL.path;

    // This may happen if the game file is deleted (or the drive it is on
    // is disconnected) after a game has started, and the game is then reset,
    // or Spatterlight tries to autorestore it at startup.
    if (![[NSFileManager defaultManager] isReadableFileAtPath:_gamefile]) {
        _gameFileURL = [game urlForBookmark];
        _gamefile = _gameFileURL.path;
        if (![[NSFileManager defaultManager] isReadableFileAtPath:_gamefile]) {
            game.found = NO;
            if (windowRestoredBySystem) {
                _restorationHandler(nil, nil);
                _restorationHandler = nil;
            } else {
                [self showGameFileGoneAlert];
            }
            return;
        }
    }

    [_imageHandler cacheImagesFromBlorbURL:_gameFileURL withData:_gameData];

    _terpname = terpname_;

    if ([_terpname isEqualToString:@"bocfel"])
        _usesFont3 = YES;

    // Initialize style hint arrays for both text grid and text buffer windows.
    // Glk style hints allow games to override default text appearance (font
    // weight, color, indentation, etc.) per style. Each window type gets its
    // own style_NUMSTYLES x stylehint_NUMHINTS matrix, initially filled with
    // NSNull to indicate "no override" for each hint slot.
    NSMutableArray *nullarray = [NSMutableArray arrayWithCapacity:stylehint_NUMHINTS];

    NSInteger i;

    for (i = 0 ; i < stylehint_NUMHINTS ; i ++)
        [nullarray addObject:[NSNull null]];
    _gridStyleHints = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];
    _bufferStyleHints = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];

    for (i = 0 ; i < style_NUMSTYLES ; i ++) {
        [_gridStyleHints addObject:[nullarray mutableCopy]];
        [_bufferStyleHints addObject:[nullarray mutableCopy]];
    }

    // Initialize VoiceOver / speech state. We start quiet (mustBeQuiet = YES)
    // to prevent speaking during initialization and autorestore. Speech is
    // enabled later once the game is fully running and the window becomes key.
    _speechTimeStamp = [NSDate distantPast];
    _shouldSpeakNewText = NO;
    _mustBeQuiet = YES;

    _supportsAutorestore = self.window.restorable;
    if (_theme.autosave == NO)
        self.window.restorable = NO;
    game.autosaved = (_supportsAutorestore && _theme.autosave);
    windowRestoredBySystem = (completionHandler != nil);
    _restorationHandler = completionHandler;

    _shouldShowAutorestoreAlert = NO;
    shouldRestoreUI = NO;
    _eventcount = 0;
    _turns = 0;
    _hasAutoSaved = NO;

    // Track the last arrange event values sent to the interpreter. This allows
    // us to avoid sending duplicate arrange events when nothing has changed.
    lastArrangeValues = @{
        @"width" : @(0),
        @"height" : @(0),
        @"bufferMargin" : @(0),
        @"gridMargin" : @(0),
        @"charWidth" : @(0),
        @"lineHeight" : @(0),
        @"leading" : @(0)
    };

    // Glk window management. _gwindows maps Glk window IDs to GlkWindow objects.
    // Windows are staged in _windowsToBeAdded/_windowsToBeRemoved and committed
    // during flushDisplay to avoid modifying the view hierarchy mid-update.
    _gwindows = [[NSMutableDictionary alloc] init];
    _windowsToBeAdded = [[NSMutableArray alloc] init];
    _windowsToBeRemoved = [[NSMutableArray alloc] init];
    bufferedData = nil;

    if (game.metadata.title.length)
        self.window.title = game.metadata.title;

    // Protocol state: the interpreter subprocess communicates via a pipe-based
    // protocol. waitforevent/waitforfilename track what the interpreter is
    // currently blocked on. dead is YES until the subprocess is launched.
    waitforevent = NO;
    waitforfilename = NO;
    dead = YES;

    _gameView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    windowdirty = NO;

    _ignoreResizes = NO;
    _shouldScrollOnCharEvent = NO;

    _quoteBoxes = nil;
    _stashedTheme = nil;

    // If we are resetting, there is a bunch of stuff that we have already done
    // and we can skip
    if (shouldReset) {
        if (!_inFullscreen) {
            _windowPreFullscreenFrame = self.window.frame;
        }
        [self forkInterpreterTask];
        return;
    }

    // When preferences change, we may change window size
    // if the theme has changed, so we set this internal var to
    // let that code know the theme has not changed
    lastTheme = _theme;
    _windowPreFullscreenFrame = [self frameWithSanitycheckedSize:NSZeroRect];

    _voiceOverActive = [NSWorkspace sharedWorkspace].voiceOverEnabled;

    [[NSWorkspace sharedWorkspace] addObserver:self forKeyPath:@"voiceOverEnabled" options:(NSKeyValueObservingOptionNew | NSKeyValueObservingOptionOld) context:nil];

    NSNotificationCenter *notifications = [NSNotificationCenter defaultCenter];

    [notifications
     addObserver:self
     selector:@selector(notePreferencesChanged:)
     name:@"PreferencesChanged"
     object:nil];

    [notifications
     addObserver:self
     selector:@selector(noteDefaultSizeChanged:)
     name:@"DefaultSizeChanged"
     object:nil];

    [notifications
     addObserver:self
     selector:@selector(noteBorderChanged:)
     name:@"BorderChanged"
     object:nil];

    [notifications
     addObserver:self
     selector:@selector(noteManagedObjectContextDidChange:)
     name:NSManagedObjectContextObjectsDidChangeNotification
     object:game.managedObjectContext];

    [notifications
     addObserver:self
     selector:@selector(noteColorModeChanged:)
     name:@"ColorModeChanged"
     object:nil];

    lastContentResize = NSZeroRect;

    restoredController = nil;
    inFullScreenResize = NO;

    self.window.representedFilename = _gamefile;

    // Enable layer-backing for the border view so we can animate its
    // background color. The border color can be set manually by the user
    // (kUserOverride) or automatically derived from the buffer background.
    _borderView.wantsLayer = YES;
    _borderView.layerContentsRedrawPolicy = NSViewLayerContentsRedrawOnSetNeedsDisplay;
    _lastAutoBGColor = _theme.bufferBackground;
    if (_theme.borderBehavior == kUserOverride)
        [self setBorderColor:_theme.borderColor];
    else
        [self setBorderColor:_theme.bufferBackground];

    NSString *autosaveLatePath = [[self appSupportDir]
                                  stringByAppendingPathComponent:@"autosave-GUI-late.plist"];


    lastScriptKeyTimestamp = [NSDate distantPast];
    lastKeyTimestamp = [NSDate distantPast];

    // Narcolepsy uses a special image-based mask on the game view to create
    // a shaped window effect. Only applied when graphics and styles are on.
    if (self.gameID == kGameIsNarcolepsy && _theme.doGraphics && _theme.doStyles) {
        [self adjustMaskLayer:nil];
    }

    // Migrate any old-format autosave directories before checking for files.
    if (_supportsAutorestore && _theme.autosave) {
        [self dealWithOldFormatAutosaveDir];
    }

    // Decide whether to autorestore or start fresh. If GUI autosave files
    // exist, attempt autorestore; otherwise delete stale files and start normally.
    if (_supportsAutorestore && _theme.autosave &&
        ([[NSFileManager defaultManager] fileExistsAtPath:self.autosaveFileGUI] || [[NSFileManager defaultManager] fileExistsAtPath:autosaveLatePath])) {
        [self runTerpWithAutorestore];
    } else {
        [self deleteAutosaveFiles];
        [self runTerpNormal];
    }
}

// Attempt to restore a previously autosaved game session. This involves:
// 1. Deserializing the GUI state from autosave-GUI.plist (window layout, text, scroll positions)
// 2. Optionally deserializing a "late" GUI state captured after the interpreter finished processing
// 3. Verifying that the GUI and interpreter autosave files are in sync via autosaveTag comparison
// 4. Handling edge cases like version mismatches, missing files, and dead (finished) games
- (void)runTerpWithAutorestore {
    autorestoring = YES;
    @try {
        restoredController =
        [NSKeyedUnarchiver unarchiveObjectWithFile:self.autosaveFileGUI];
    } @catch (NSException *ex) {
        // leave restoredController as nil
        NSLog(@"Unable to restore GUI autosave: %@", ex);
    }

    NSDate *terpAutosaveDate = nil;
    NSDate *GUIAutosaveDate = nil;
    NSDate *GUILateAutosaveDate = nil;

    NSError *error;
    NSFileManager *fileManager = [NSFileManager defaultManager];

    // Check creation date of interpreter autosave file
    NSDictionary* attrs = [fileManager attributesOfItemAtPath:self.autosaveFileTerp error:&error];
    if (attrs) {
        terpAutosaveDate = (NSDate*)attrs[NSFileCreationDate];
    } else {
        NSLog(@"Error: %@", error);
    }

    // Check creation date of GUI autosave file
    attrs = [fileManager attributesOfItemAtPath:self.autosaveFileGUI error:&error];
    if (attrs) {
        GUIAutosaveDate = (NSDate*)attrs[NSFileCreationDate];
        if (terpAutosaveDate && GUIAutosaveDate && [terpAutosaveDate compare:GUIAutosaveDate] == NSOrderedDescending) {
            NSLog(@"GUI autosave file is created before terp autosave file!");
        }
    } else {
        NSLog(@"Error: %@", error);
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

    NSString *autosaveLatePath = [self.appSupportDir
                                  stringByAppendingPathComponent:@"autosave-GUI-late.plist"];

    if ([fileManager fileExistsAtPath:autosaveLatePath]) {
        // Get creation date of GUI late autosave
        attrs = [fileManager attributesOfItemAtPath:autosaveLatePath error:&error];
        if (attrs) {
            GUILateAutosaveDate = (NSDate*)attrs[NSFileCreationDate];
        } else {
            NSLog(@"Error: %@", error);
        }
        @try {
            restoredControllerLate =
            [NSKeyedUnarchiver unarchiveObjectWithFile:autosaveLatePath];
            if (!restoredController) {
                restoredController = restoredControllerLate;
                GUIAutosaveDate = GUILateAutosaveDate;
            }
        } @catch (NSException *ex) {
            NSLog(@"Unable to restore late GUI autosave: %@", ex);
            restoredControllerLate = restoredController;
        }
    }

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

    Game *game = _game;
    Theme *theme = _theme;

    if (!restoredController) {
        // If there are no useful UI autosave files,
        // delete any leftovers and run game without autorestoring
        [self deleteAutosaveFiles];
        game.autosaved = NO;
        [self runTerpNormal];
        return;
    }

    // Restore fullscreen state and the window frame from before fullscreen,
    // which we'll need if the user exits fullscreen later.
    _inFullscreen = restoredControllerLate.inFullscreen;
    _windowPreFullscreenFrame = restoredController.windowPreFullscreenFrame;
    _shouldStoreScrollOffset = NO;

    // If the game had ended (interpreter process was dead) when autosaved,
    // we either show the finished game window (system restoration) or
    // start a fresh game (user-initiated launch).
    if (!restoredController.isAlive) {
        if (windowRestoredBySystem) {
            [self restoreWindowWhenDead];
            return;
        } else {
            // Otherwise we delete any autorestore files and
            // restart the game
            [self deleteAutosaveFiles];
            game.autosaved = NO;
            // If we die in fullscreen and close the game,
            // the game should not open in fullscreen the next time
            _inFullscreen = NO;
            [self runTerpNormal];
            return;
        }
    }

    // If the interpreter's autosave file exists, verify its tag matches the
    // GUI autosave tag to ensure they represent the same game state. A mismatch
    // means one was saved at a different point than the other, so we try falling
    // back to a backup terp save or limiting restoration to UI-only.
    if ([fileManager fileExistsAtPath:self.autosaveFileTerp]) {
        restoreUIOnly = NO;

        TempLibrary *tempLib =
        [NSKeyedUnarchiver unarchiveObjectWithFile:self.autosaveFileTerp];
        if (tempLib.autosaveTag != restoredController.autosaveTag) {
            NSLog(@"The terp autosave and the GUI autosave have non-matching tags.");
            NSLog(@"The terp autosave tag: %u GUI autosave tag: %ld", tempLib.autosaveTag, restoredController.autosaveTag);
            NSLog(@"Trying to use previous terp save");

            NSString *oldAutosaveFileTerp =
            [self.appSupportDir stringByAppendingPathComponent:@"autosave-bak.plist"];
            tempLib = [NSKeyedUnarchiver unarchiveObjectWithFile:oldAutosaveFileTerp];
            if (tempLib.autosaveTag == restoredController.autosaveTag) {
                [fileManager removeItemAtPath:self.autosaveFileTerp error:nil];
                [fileManager moveItemAtPath:oldAutosaveFileTerp toPath:self.autosaveFileTerp error:nil];
                NSString *glkSaveTerp = [self.appSupportDir stringByAppendingPathComponent:@"autosave.glksave"];

                NSString *oldGlkSaveTerp = [self.appSupportDir stringByAppendingPathComponent:@"autosave-bak.glksave"];
                [fileManager removeItemAtPath:glkSaveTerp error:nil];
                [fileManager moveItemAtPath:oldGlkSaveTerp toPath:glkSaveTerp error:nil];
                NSLog(@"Successfully used previous terp save");
            } else {
                NSLog(@"Only restore UI state at first turn");
                [self deleteFiles:@[ [NSURL fileURLWithPath:self.autosaveFileGUI isDirectory:NO],
                                     [NSURL fileURLWithPath:self.autosaveFileTerp isDirectory:NO] ]];
                restoreUIOnly = YES;
            }
        } else {
            // Only show the alert about autorestoring if this is not a system
            // window restoration, and the user has not suppressed it.
            if (!windowRestoredBySystem) {
                if (theme.autosave == NO) {
                    game.autosaved = NO;
                    [self runTerpNormal];
                    return;
                }
                NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
                if ([defaults boolForKey:@"AutorestoreAlertSuppression"]) {
                    // Autorestore alert suppressed
                    if (![defaults boolForKey:@"AlwaysAutorestore"]) {
                        // The user has checked "Remember this choice" when
                        // choosing to not autorestore
                        [self deleteAutosaveFiles];
                        game.autosaved = NO;
                        [self runTerpNormal];
                        return;
                    }
                } else {
                    _shouldShowAutorestoreAlert = YES;
                }
            }
        }
    } else {
        NSLog(@"No interpreter autorestore file exists");
        NSLog(@"Only restore UI state at first turn");
        restoreUIOnly = YES;
    }

    // If we're in UI-only restore mode but the late autosave wasn't from the
    // first turn, the UI state won't match the fresh game state. Fall back to
    // a clean start.
    if (restoreUIOnly && restoredControllerLate.hasAutoSaved) {
        NSLog(@"restoredControllerLate was not saved at the first turn!");
        NSLog(@"Delete autosave files and start normally without restoring UI");
        restoreUIOnly = NO;
        [self deleteAutosaveFiles];
        game.autosaved = NO;
        [self runTerpNormal];
        return;
    }

    // Temporarily switch to the theme that was active when the autosave was
    // created, so the restored UI renders correctly. The original theme is
    // stashed and will be restored after the autorestore process completes.
    _stashedTheme = theme;
    Theme *restoredTheme = [self findThemeByName:restoredControllerLate.oldThemeName];
    if (restoredTheme && restoredTheme != theme) {
        _theme = restoredTheme;
    } else _stashedTheme = nil;

    // If this is not a window restoration done by the system,
    // we now re-enter fullscreen manually if the game was
    // closed in fullscreen mode.
    if (!windowRestoredBySystem && _inFullscreen
        && (self.window.styleMask & NSWindowStyleMaskFullScreen) != NSWindowStyleMaskFullScreen) {
        _startingInFullscreen = YES;
        [self startInFullscreen];
    } else {
        _gameView.autoresizingMask =
        NSViewMinXMargin | NSViewMaxXMargin | NSViewMinYMargin;
        [self.window setFrame:restoredControllerLate.storedWindowFrame display:YES];
        _gameView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    }

    [self adjustContentView];
    shouldRestoreUI = YES;
    [self forkInterpreterTask];

    // The interpreter must process several events (including arrange events)
    // before the Glk windows exist and are ready to receive restored UI state.
    // The actual UI restoration happens later in the GlkRequests category
    // when the interpreter reaches the appropriate NEXTEVENT.
}

// Start the interpreter without autorestore. Sizes and positions the window
// using the theme's default dimensions, cascading to avoid overlap with other
// open game windows. If the game has cover art configured, shows that first.
- (void)runTerpNormal {
    autorestoring = NO;
    NSRect newContentFrame = (self.window.contentView).frame;
    if (!(windowRestoredBySystem || _showingCoverImage)) {
        newContentFrame.size = [self defaultContentSize];
        NSRect newWindowFrame = [self.window frameRectForContentRect:newContentFrame];
        NSRect screenFrame = self.window.screen.visibleFrame;
        // Make sure that the window is shorter than the screen
        if (NSHeight(newWindowFrame) > NSHeight(screenFrame))
            newWindowFrame.size.height = NSHeight(screenFrame);

        newWindowFrame.origin.x = round((NSWidth(screenFrame) - NSWidth(newWindowFrame)) / 2);
        // Place the window just above center by default
        newWindowFrame.origin.y = round(screenFrame.origin.y + (NSHeight(screenFrame) - NSHeight(newWindowFrame)) / 2) + 40;

        // Cascade windows so multiple game windows don't stack directly on
        // top of each other. Offsets each new window 20px right and 20px down
        // from any overlapping window.
        if (libcontroller.gameSessions.count > 1) {
            NSPoint thisPoint, thatPoint;
            NSInteger repeats = 0;
            BOOL overlapping;
            thisPoint = newWindowFrame.origin;
            thisPoint.y += NSHeight(newWindowFrame);
            do {
                overlapping = NO;
                for (GlkController *ctrl in libcontroller.gameSessions.allValues) {
                    if (ctrl != self && ctrl.window) {
                        thatPoint = ctrl.window.frame.origin;
                        thatPoint.y += NSHeight(ctrl.window.frame);
                        if (fabs(thisPoint.x - thatPoint.x) < 3 || fabs(thisPoint.y - thatPoint.y) < 3) {
                            thisPoint.x = thatPoint.x + 20;
                            thisPoint.y = thatPoint.y - 20;
                            overlapping = YES;
                            repeats++;
                        }
                    }
                }
            } while (overlapping == YES && repeats < 100);
            if (repeats >= 100)
                NSLog(@"Got caught in a cascading windows infinite loop");
            newWindowFrame.origin = thisPoint;
            newWindowFrame.origin.y -= NSHeight(newWindowFrame);
        }

        [self.window setFrame:newWindowFrame display:NO];
        [self adjustContentView];
    }
    lastSizeInChars = [self contentSizeToCharCells:_gameView.frame.size];
    if (![self runWindowsRestorationHandler])
        [self showWindow:nil];
    if (_theme.coverArtStyle != kDontShow && _game.metadata.cover.data) {
        [self deleteAutosaveFiles];
        _gameView.autoresizingMask =
        NSViewMinXMargin | NSViewMaxXMargin | NSViewHeightSizable;
        restoredController = nil;
        _coverController = [[CoverImageHandler alloc] initWithController:self];
        [_coverController showLogoWindow];
    } else {
        [self forkInterpreterTask];
    }
}

// Invoke and clear the system window restoration completion handler, if one
// was provided. Returns YES if a handler was called, NO if none was pending.
// This is called during both normal startup and dead-game restoration to
// complete the NSWindowRestoration protocol.
- (BOOL)runWindowsRestorationHandler {
    if (_restorationHandler == nil) {
        return NO;
    }

    _restorationHandler(self.window, nil);
    _restorationHandler = nil;
    return YES;
}

// Restore a game window for a game that had already finished when it was
// autosaved. This is only used during system window restoration at app launch.
// Shows the final game state with a "(finished)" title and a "Game Over" bezel.
- (void)restoreWindowWhenDead {
    // If the game was showing its cover image when it was saved, restart the
    // cover image display flow instead of showing finished state.
    if (restoredController.showingCoverImage) {
        dead = NO;
        _showingCoverImage = YES;
        [self runTerpNormal];
        return;
    }

    dead = YES;

    [self runWindowsRestorationHandler];
    [self restoreUI];
    self.window.title = [self.window.title stringByAppendingString:@" (finished)"];

    GlkController * __weak weakSelf = self;
    NSScreen *screen = self.window.screen;
    NSString *title = [NSString stringWithFormat:@"%@ has finished.", _game.metadata.title];
    double delayInSeconds = 1.5;
    dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
    dispatch_after(popTime, dispatch_get_main_queue(), ^(void) {
        NotificationBezel *bezel = [[NotificationBezel alloc] initWithScreen:screen];
        [bezel showGameOver];
        [weakSelf speakStringNow:title];
    });

    restoredController = nil;
}

// Match the game's IFID against known game identifiers to enable game-specific
// workarounds and features. Each recognized game gets a kGameIdentity enum
// value used throughout the codebase to conditionally apply special behavior
// (e.g., Journey's menu system, Narcolepsy's window mask, Adrian Mole's
// scrolling fix, Bureaucracy's form handling, Beyond Zork's font 3 support).
- (void)identifyGame:(NSString *)ifid {
    NSString *l9Substring = nil;
    if (ifid.length >= 10)
        l9Substring = [ifid substringToIndex:10];
    if ([l9Substring isEqualToString:@"LEVEL9-001"] || // The Secret Diary of Adrian Mole
        [l9Substring isEqualToString:@"LEVEL9-002"] || // The Growing Pains of Adrian Mole
        [l9Substring isEqualToString:@"LEVEL9-019"]) { // The Archers
        _gameID = kGameIsAdrianMole;
    } else if ([ifid isEqualToString:@"ZCODE-5-990206-6B48"]) {
        _gameID = kGameIsAnchorheadOriginal;
    } else if ([ifid isEqualToString:@"ZCODE-47-870915"] ||
               [ifid isEqualToString:@"ZCODE-49-870917"] ||
               [ifid isEqualToString:@"ZCODE-51-870923"] ||
               [ifid isEqualToString:@"ZCODE-57-871221"] ||
               [ifid isEqualToString:@"ZCODE-60-880610"]) {
        _gameID = kGameIsBeyondZork;
    } else if ([ifid isEqualToString:@"ZCODE-86-870212"] ||
               [ifid isEqualToString:@"ZCODE-116-870602"] ||
               [ifid isEqualToString:@"ZCODE-160-880521"]) {
        _gameID = kGameIsBureaucracy;
    } else if ([ifid isEqualToString:@"BFDE398E-C724-4B9B-99EB-18EE4F26932E"]) {
        _gameID = kGameIsAColderLight;
    } else if ([ifid isEqualToString:@"ZCODE-7-930428-0000"] ||
               [ifid isEqualToString:@"ZCODE-8-930603-0000"] ||
               [ifid isEqualToString:@"ZCODE-10-940120-BD9E"] ||
               [ifid isEqualToString:@"ZCODE-12-940604-6035"] ||
               [ifid isEqualToString:@"ZCODE-16-951024-4DE6"]) {
        _gameID = kGameIsCurses;
    } else if ([ifid isEqualToString:@"303E9BDC-6D86-4389-86C5-B8DCF01B8F2A"]) {
        _gameID = kGameIsDeadCities;
    } else if ([ifid isEqualToString:@"AC0DAF65-F40F-4A41-A4E4-50414F836E14"]) {
        _gameID = kGameIsKerkerkruip;
    } else if ([ifid isEqualToString:@"GLULX-1-040108-D8D78266"]) {
        _gameID = kGameIsNarcolepsy;
    } else if ([ifid isEqualToString:@"afb163f4-4d7b-0dd9-1870-030f2231e19f"]) {
        _gameID = kGameIsThaumistry;
    } else if ([ifid isEqualToString:@"ZCODE-1-851202"] ||
               [ifid isEqualToString:@"ZCODE-1-860221"] ||
               [ifid isEqualToString:@"ZCODE-14-860313"] ||
               [ifid isEqualToString:@"ZCODE-11-860509"] ||
               [ifid isEqualToString:@"ZCODE-12-860926"] ||
               [ifid isEqualToString:@"ZCODE-15-870628"]) {
        _gameID = kGameIsTrinity;
    } else if ([ifid isEqualToString:@"ZCODE-1-050929-F8AB"] ||
               [ifid isEqualToString:@"ZCODE-1-051128-B5AA"]) {
        _gameID = kGameIsVespers;
    } else if ([ifid isEqualToString:@"CF619423-EEC7-4E83-8C66-AE7182D55C89"]) {
        _gameID = kGameIsJuniorArithmancer;
    } else if ([ifid isEqualToString:@"ZCODE-0-870831"] ||
               [ifid isEqualToString:@"ZCODE-1-871030"] ||
               [ifid isEqualToString:@"ZCODE-74-880114"] ||
               [ifid isEqualToString:@"ZCODE-96-880224"] ||
               [ifid isEqualToString:@"ZCODE-153-880510"] ||
               [ifid isEqualToString:@"ZCODE-242-880830"] ||
               [ifid isEqualToString:@"ZCODE-242-880901"] ||
               [ifid isEqualToString:@"ZCODE-296-881019"] ||
               [ifid isEqualToString:@"ZCODE-66-890111"] ||
               [ifid isEqualToString:@"ZCODE-343-890217"] ||
               [ifid isEqualToString:@"ZCODE-366-890323"] ||
               [ifid isEqualToString:@"ZCODE-383-890602"] ||
               [ifid isEqualToString:@"ZCODE-387-890612"] ||
               [ifid isEqualToString:@"ZCODE-392-890714"] ||
               [ifid isEqualToString:@"ZCODE-393-890714"]) {
        _gameID = kGameIsZorkZero;
    } else if ([ifid isEqualToString:@"ZCODE-0-870831"] ||
               [ifid isEqualToString:@"ZCODE-40-890502"] ||
               [ifid isEqualToString:@"ZCODE-41-890504"] ||
               [ifid isEqualToString:@"ZCODE-54-890606"] ||
               [ifid isEqualToString:@"ZCODE-63-890622"] ||
               [ifid isEqualToString:@"ZCODE-74-890714"] ) {
        _gameID = kGameIsArthur;
    } else if ([ifid isEqualToString:@"ZCODE-278-890209"] ||
               [ifid isEqualToString:@"ZCODE-278-890211"] ||
               [ifid isEqualToString:@"ZCODE-279-890217"] ||
               [ifid isEqualToString:@"ZCODE-280-890217"] ||
               [ifid isEqualToString:@"ZCODE-281-890222"] ||
               [ifid isEqualToString:@"ZCODE-282-890224"] ||
               [ifid isEqualToString:@"ZCODE-283-890228"] ||
               [ifid isEqualToString:@"ZCODE-284-890302"] ||
               [ifid isEqualToString:@"ZCODE-286-890306"] ||
               [ifid isEqualToString:@"ZCODE-288-890308"] ||
               [ifid isEqualToString:@"ZCODE-289-890309"] ||
               [ifid isEqualToString:@"ZCODE-290-890311"] ||
               [ifid isEqualToString:@"ZCODE-291-890313"] ||
               [ifid isEqualToString:@"ZCODE-292-890314"] ||
               [ifid isEqualToString:@"ZCODE-295-890321"] ||
               [ifid isEqualToString:@"ZCODE-311-890510"] ||
               [ifid isEqualToString:@"ZCODE-320-890627"] ||
               [ifid isEqualToString:@"ZCODE-321-890629"] ||
               [ifid isEqualToString:@"ZCODE-322-890706"]) {
        _gameID = kGameIsShogun;
    } else if ([ifid isEqualToString:@"ZCODE-142-890205"] ||
               [ifid isEqualToString:@"ZCODE-2-890303"] ||
               [ifid isEqualToString:@"ZCODE-11-890304"] ||
               [ifid isEqualToString:@"ZCODE-3-890310"] ||
               [ifid isEqualToString:@"ZCODE-5-890310"] ||
               [ifid isEqualToString:@"ZCODE-10-890313"] ||
               [ifid isEqualToString:@"ZCODE-26-890316"] ||
               [ifid isEqualToString:@"ZCODE-30-890322"] ||
               [ifid isEqualToString:@"ZCODE-51-890522"] ||
               [ifid isEqualToString:@"ZCODE-54-890526"] ||
               [ifid isEqualToString:@"ZCODE-76-890615"] ||
               [ifid isEqualToString:@"ZCODE-77-890616"] ||
               [ifid isEqualToString:@"ZCODE-79-890627"] ||
               [ifid isEqualToString:@"ZCODE-83-890706"]) {
        _gameID = kGameIsJourney;
    } else {
        _gameID = kGameIsGeneric;
    }
}

// Clear any game-specific identification, treating the game as generic.
// Called when the theme's "nohacks" option is enabled.
- (void)resetGameIdentification {
    _gameID = kGameIsGeneric;
}

// Z-machine version 6 games (Arthur, Journey, Shogun, Zork Zero) use a
// graphical windowing model that differs from standard text games.
- (BOOL)zVersion6 {
    return (_gameID == kGameIsArthur || _gameID == kGameIsJourney || _gameID == kGameIsShogun ||  _gameID == kGameIsZorkZero);
}

// Launch the interpreter as a child process. The interpreter communicates
// with Spatterlight over stdin/stdout pipes using a binary protocol defined
// in protocol.h. The interpreter sends display commands (print text, create
// windows, draw images, play sounds) and Spatterlight sends back events
// (key presses, line input, arrange events, timer events).
- (void)forkInterpreterTask {
    Theme *theme = _theme;

    NSString *terppath;
    NSPipe *readpipe;
    NSPipe *sendpipe;

    // The interpreter executable is bundled as an auxiliary executable
    // inside the app bundle (e.g., bocfel, glulxe, hugo, tads, etc.)
    terppath = [[NSBundle mainBundle] pathForAuxiliaryExecutable:_terpname];
    readpipe = [NSPipe pipe];
    sendpipe = [NSPipe pipe];
    readfh = readpipe.fileHandleForReading;
    sendfh = sendpipe.fileHandleForWriting;

    task = [[NSTask alloc] init];
    task.currentDirectoryPath = NSHomeDirectory();
    task.standardOutput = readpipe;
    task.standardInput = sendpipe;

#ifdef TEE_TERP_OUTPUT
    [task setLaunchPath:@"/bin/bash"];

    NSString *cmdline = @" ";
    cmdline = [cmdline stringByAppendingString:terppath];
    cmdline = [cmdline stringByAppendingString:@" \""];
    cmdline = [cmdline stringByAppendingString:_gamefile];

    cmdline = [cmdline
               stringByAppendingString:@"\" | tee -a ~/Desktop/Spatterlight\\ "];

    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    [formatter setDateFormat:@"yyyy-MM-dd HH.mm"];
    NSString *stringFromDate = [formatter stringFromDate:[NSDate date]];

    stringFromDate =
    [stringFromDate stringByReplacingOccurrencesOfString:@" "
                                              withString:@"\\ "];
    cmdline = [cmdline stringByAppendingString:stringFromDate];
    cmdline = [cmdline stringByAppendingString:@".txt"];

    [task setArguments:@[ @"-c", cmdline ]];
#else

    task.launchPath = terppath;
    task.arguments = @[ _gamefile ];

    // Bocfel (Z-machine interpreter) accepts extra options:
    // -n: set the interpreter number reported to the game
    // -N: set the interpreter letter
    // -z: fix the random seed for deterministic/reproducible playback
    // -p: disable game-specific patches (the "nohacks" option)
    if ([_terpname isEqualToString:@"bocfel"]) {
        // Due to a bug in earlier versions, theme.zMachineTerp might be 0.
        if (theme.zMachineTerp < 1 || theme.zMachineTerp > 11) {
            theme.zMachineTerp = 6; // Interpreter 6 means IBM PC.
        }
        NSArray *extraBocfelOptions =
        @[@"-n", [NSString stringWithFormat:@"%d", theme.zMachineTerp],
          @"-N", theme.zMachineLetter];
        if (theme.determinism)
            extraBocfelOptions = [extraBocfelOptions arrayByAddingObjectsFromArray:@[@"-z", @"1234"]];
        if (theme.nohacks)
            extraBocfelOptions = [extraBocfelOptions arrayByAddingObject:@"-p"];

        task.arguments = [extraBocfelOptions arrayByAddingObjectsFromArray:task.arguments];
    }

    // TADS 3 interpreter: -norandomize disables random number generation
    // for deterministic/reproducible testing.
    if ([_game.detectedFormat isEqualToString:@"tads3"]) {
        if (theme.determinism) {
            NSArray *extraTadsOptions = @[@"-norandomize"];
            task.arguments = [extraTadsOptions arrayByAddingObjectsFromArray:task.arguments];
        }
    }

#endif // TEE_TERP_OUTPUT

    GlkController * __weak weakSelf = self;

    // Handle interpreter process termination (crash, game over, or explicit kill).
    // Dispatches to the main queue since UI updates must happen on the main thread.
    task.terminationHandler = ^(NSTask *aTask) {
        [aTask.standardOutput fileHandleForReading].readabilityHandler = nil;
        GlkController *strongSelf = weakSelf;
        if(strongSelf) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [strongSelf noteTaskDidTerminate:aTask];
            });
        }
    };

    // The event queue holds GlkEvent objects to be sent to the interpreter
    // when it next requests an event (NEXTEVENT).
    _queue = [[NSMutableArray alloc] init];

    // Listen for data from the interpreter's stdout pipe. When data arrives,
    // noteDataAvailable: parses the protocol messages and dispatches commands.
    [[NSNotificationCenter defaultCenter]
     addObserver: self
     selector: @selector(noteDataAvailable:)
     name: NSFileHandleDataAvailableNotification
     object: readfh];

    dead = NO;

    // Ensure we have sandbox access to the game file before launching.
    if (_secureBookmark == nil) {
        _secureBookmark = [FolderAccess grantAccessToFile:_gameFileURL];
    }

    [task launch];

    // Send initial preferences and window dimensions to the interpreter so it
    // knows the available display area before it starts creating windows.
    GlkEvent *gevent;

    gevent = [[GlkEvent alloc] initPrefsEventForTheme:theme];
    [self queueEvent:gevent];

    [self sendArrangeEventWithFrame:_gameView.frame force:NO];

    restartingAlready = NO;
    [readfh waitForDataInBackgroundAndNotify];
}

// Request sandbox access to a file's directory. If access is already granted
// via a stored security-scoped bookmark, the block runs immediately. Otherwise,
// if dialogFlag is YES, presents an NSOpenPanel asking the user to authorize
// access. If the user declines (or dialogFlag is NO), the game window closes.
- (void)askForAccessToURL:(NSURL *)url showDialog:(BOOL)dialogFlag andThenRunBlock:(void (^)(void))block {

    _showingDialog = NO;

    NSURL *bookmarkURL = [FolderAccess suitableDirectoryForURL:url];
    if (bookmarkURL) {
        if (![FolderAccess needsPermissionForURL:bookmarkURL]) {
            [FolderAccess storeBookmark:bookmarkURL];
            [FolderAccess saveBookmarks];
        } else {
            [FolderAccess restoreURL:bookmarkURL];
            if ([FolderAccess needsPermissionForURL:bookmarkURL]) {

                if (!dialogFlag) {
                    double delayInSeconds = 0.5;
                    dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
                    dispatch_after(popTime, dispatch_get_main_queue(), ^(void) {
                        // Closing game window as we have no security scoped bookmark
                        // and we don't want to show a file dialog during window
                        // restoration.
                        [self close];
                    });

                    return;
                }

                NSOpenPanel *openPanel = [NSOpenPanel openPanel];
                openPanel.message = NSLocalizedString(@"Spatterlight would like to access files in this folder", nil);
                openPanel.prompt = NSLocalizedString(@"Authorize", nil);
                openPanel.canChooseFiles = NO;
                openPanel.canChooseDirectories = YES;
                openPanel.canCreateDirectories = NO;
                openPanel.directoryURL = bookmarkURL;

                _showingDialog = YES;

                GlkController __weak *weakSelf = self;

                [openPanel beginWithCompletionHandler:^(NSInteger result) {
                    if (result == NSModalResponseOK) {
                        NSURL *blockURL = openPanel.URL;
                        [FolderAccess storeBookmark:blockURL];
                        [FolderAccess saveBookmarks];
                        block();
                    } else {
                        NSLog(@"GlkController askForAccessToURL: closing window because user would not grant access");
                        [weakSelf close];
                    }
                    weakSelf.showingDialog = NO;
                }];
                return;
            } else {
                _secureBookmark = bookmarkURL;
            }
        }
    }

    block();
}

- (void)showGameFileGoneAlert {
    NSAlert *anAlert = [[NSAlert alloc] init];
    anAlert.messageText = [NSString stringWithFormat:NSLocalizedString(@"The game file \"%@\" is no longer accessible. This game will now close.", nil), self.gamefile.lastPathComponent];

    [anAlert beginSheetModalForWindow:self.window completionHandler:^(NSInteger result) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            [self.window performClose:nil];
        });
    }];
}

#pragma mark Cocoa glue

// Inverse of the "dead" ivar: the game is alive when the interpreter process
// is running and has not yet terminated.
- (BOOL)isAlive {
    return !dead;
}

// Called when this game window becomes the key (focused) window.
// Re-enables VoiceOver speech, sets focus to the appropriate Glk window,
// and updates any game-specific menu state (e.g., Journey's party menu).
- (void)windowDidBecomeKey:(NSNotification *)notification {
    [Preferences changeCurrentGlkController:self];

    if (!dead) {
        [self guessFocus];
        // Allow VoiceOver to speak once the game has processed at least one
        // event and we're past any pending autorestore alert.
        if (_eventcount > 1 && !_shouldShowAutorestoreAlert) {
            _mustBeQuiet = NO;
        }

        if (_journeyMenuHandler && [_journeyMenuHandler updateOnBecameKey:!_shouldShowAutorestoreAlert || _turns > 1]) {
            return;
        }

        [self speakOnBecomingKey];
    } else if (_theme.vODelayOn) {
        // If the game has ended, still speak the final text for VoiceOver users.
        [self speakMostRecentAfterDelay];
    }
}

// Silence VoiceOver when the window loses focus to prevent this game's
// speech from interrupting whatever the user switched to.
- (void)windowDidResignKey:(NSNotification *)notification {
    _mustBeQuiet = YES;
    [_journeyMenuHandler captureMembersMenu];
    [_journeyMenuHandler hideJourneyMenus];
}

// Intercept the window close request. If the game is still running and
// autosave is disabled, prompt the user to confirm they want to abandon
// the game. Games with autosave enabled close silently since progress is preserved.
- (BOOL)windowShouldClose:(id)sender {
    NSAlert *alert;

    // Allow immediate close if the game has ended or autosave will preserve state.
    if (dead || (_supportsAutorestore && _theme.autosave)) {
        return YES;
    }

    if ([[NSUserDefaults standardUserDefaults]
         boolForKey:@"CloseAlertSuppression"]) {
        NSLog(@"Window close alert suppressed");
        return YES;
    }
    alert = [[NSAlert alloc] init];
    alert.messageText = NSLocalizedString(@"Do you want to abandon the game?", nil);
    alert.informativeText = NSLocalizedString(@"Any unsaved progress will be lost.", nil);
    alert.showsSuppressionButton = YES; // Uses default checkbox title

    [alert addButtonWithTitle:NSLocalizedString(@"Close", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

    _mustBeQuiet = YES;

    [alert beginSheetModalForWindow:self.window
                  completionHandler:^(NSInteger result) {
        if (result == NSAlertFirstButtonReturn) {
            if (alert.suppressionButton.state == NSOnState) {
                // Suppress this alert from now on
                [[NSUserDefaults standardUserDefaults]
                 setBool:YES
                 forKey:@"CloseAlertSuppression"];
            }
            [self close];
        }
        self.mustBeQuiet = NO;
    }];
    return NO;
}

// Forcefully terminate the interpreter subprocess. Clears the termination
// handler first to prevent the normal noteTaskDidTerminate: flow from firing,
// since this is an intentional kill (e.g., during reset or window close).
- (void)terminateTask {
    if (task) {
        [task setTerminationHandler:nil];
        [task.standardOutput fileHandleForReading].readabilityHandler = nil;
        readfh = nil;
        [task terminate];
    }
}


// Clean up all resources when the game window closes. This includes saving
// autosave state, stopping sounds, releasing sandbox bookmarks, invalidating
// timers, sending a quit event to the interpreter, and transferring the
// "current game" status to another open game session if one exists.
- (void)windowWillClose:(id)sender {
    if (windowClosedAlready) {
        NSLog(@"windowWillClose called twice! Returning");
        return;
    } else windowClosedAlready = YES;

    // Prevent the interpreter from processing new events during teardown
    waitforfilename = YES;

    if (_showingCoverImage) {
        [[NSNotificationCenter defaultCenter]
         removeObserver:_coverController];
        _coverController = nil;
    }
    [self autoSaveOnExit];
    [_soundHandler stopAllAndCleanUp];

    if (_journeyMenuHandler) {
        [_journeyMenuHandler captureMembersMenu];
        [self.journeyMenuHandler hideJourneyMenus];
    }

    if (_theme.autosave == NO) {
        _game.autosaved = NO;
        [self deleteAutosaveFiles];
    }

    @try {
        [[NSWorkspace sharedWorkspace] removeObserver:self forKeyPath:@"voiceOverEnabled"];
    } @catch (NSException *ex) {
        NSLog(@"%@", ex);
    }

    if (_game && [Preferences instance].currentGame == _game) {
        GlkController *remainingGameSession = nil;
        if (libcontroller) {
            for (GlkController *session in
                 libcontroller.gameSessions.allValues)
                if (session != self) {
                    remainingGameSession = session;
                    break;
                }
        }
        [Preferences changeCurrentGlkController:remainingGameSession];
        if (remainingGameSession) {
            [remainingGameSession.window makeKeyAndOrderFront:nil];
        }
    }

    if (timer) {
        // Stop the timer
        [timer invalidate];
        timer = nil;
    }

    GlkEvent *evt = [[GlkEvent alloc] initQuitEvent];
    [evt writeEvent:sendfh.fileDescriptor];

    if (libcontroller) {
        [libcontroller releaseGlkControllerSoon:self];
    }

    if (_secureBookmark)
        [FolderAccess releaseBookmark:_secureBookmark];

    libcontroller = nil;
}

// Commit all pending display changes. Called after the interpreter finishes
// processing a batch of commands and before waiting for the next event.
// This is the main "render frame" method that:
// 1. Adds newly created Glk windows to the view hierarchy
// 2. Updates the border color based on the largest window's background
// 3. Applies the Narcolepsy window mask if needed
// 4. Flushes each individual Glk window's buffered text/graphics
// 5. Removes closed windows from the view hierarchy
// 6. Triggers VoiceOver speech for new game text
- (void)flushDisplay {
    [Preferences instance].inMagnification = NO;

    // Add any windows that were created during this update cycle
    for (GlkWindow *win in _windowsToBeAdded) {
        [_gameView addSubview:win];
    }

    if (windowdirty && !changedBorderThisTurn) {
        GlkWindow *largest = [self largestWindow];
        if (largest) {
            [largest recalcBackground];
        }
        windowdirty = NO;
    }
    changedBorderThisTurn = NO;

    if (self.gameID == kGameIsNarcolepsy && _theme.doGraphics && _theme.doStyles) {
        [self adjustMaskLayer:nil];
    }

    _windowsToBeAdded = [NSMutableArray new];

    // Flush each window's buffered content (text, images, style changes).
    // For text windows, mark the current content position as "last move"
    // so we can track what's new on the next update (unless VoiceOver needs
    // the full text to remain unmarked for speech purposes).
    for (GlkWindow *win in _gwindows.allValues) {
        [win flushDisplay];
        if (![win isKindOfClass:[GlkGraphicsWindow class]] &&
            (!_voiceOverActive || _mustBeQuiet)) {
            [win setLastMove];
        }
    }

    // Remove windows that were closed during this update cycle
    for (GlkWindow *win in _windowsToBeRemoved) {
        [win removeFromSuperview];
        win.glkctl = nil;
    }

    // Speak new game text via VoiceOver if enabled and appropriate
    if (_shouldSpeakNewText) {
        if (_voiceOverActive && !_mustBeQuiet) {
            [self checkZMenuAndSpeak:YES];
            if (!_zmenu && !_form) {
                CGFloat delay = 0.2;
                // We need a longer delay if we just closed a dialog
                if (_journeyMenuHandler && [_journeyMenuHandler.journeyDialogClosedTimestamp timeIntervalSinceNow] > -1)
                    delay = 1;
                dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                    [self forceSpeech];
                    [self speakNewText];
                });
            }
            _shouldSpeakNewText = NO;
        }
    }

    if (_journeyMenuHandler)
        [_journeyMenuHandler flushDisplay];

    _windowsToBeRemoved = [[NSMutableArray alloc] init];
}


// Determine which Glk window should receive keyboard focus. Walks up the
// responder chain to check if focus is already in a Glk window that wants it.
// If not, searches all windows for one requesting focus (typically whichever
// window has an active line or character input request).
- (void)guessFocus {
    // Journey with VoiceOver always focuses the text buffer for accessibility.
    if (_gameID == kGameIsJourney && _voiceOverActive) {
        for (GlkWindow *win in _gwindows.allValues) {
            if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
                [win grabFocus];
                return;
            }
        }
    }

    // Walk the responder chain upward to find if a GlkWindow already has focus
    id focuswin = self.window.firstResponder;
    while (focuswin) {
        if ([focuswin isKindOfClass:[NSView class]]) {
            if ([focuswin isKindOfClass:[GlkWindow class]])
                break;
            else
                focuswin = [focuswin superview];
        } else
            focuswin = nil;
    }

    // If focus is already in a window that wants it, leave it alone
    if (focuswin && [focuswin wantsFocus])
        return;

    // Otherwise, find any window requesting focus and give it to them
    for (GlkWindow *win in _gwindows.allValues) {
        if (win.wantsFocus) {
            [win grabFocus];
            return;
        }
    }
}

// Mark the current scroll position in all text buffer windows as "last seen"
// by the user. Used to determine the "more" prompt position - text after this
// point is considered new and may trigger a scroll pause.
- (void)markLastSeen {
    for (GlkWindow *win in _gwindows.allValues) {
        if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
            [win markLastSeen];
        }
    }
}

// Scroll all text buffer windows to show new content. Skipped during
// autorestore to prevent premature scrolling before the UI is fully rebuilt.
- (void)performScroll {
    if (autorestoring)
        return;
    for (GlkWindow *win in _gwindows.allValues)
        if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
            [win performScroll];
        }
}

// Provide custom field editors for Glk text input fields. Each GlkWindow's
// InputTextField has its own field editor to support independent input handling,
// command history, and key event interception per window.
- (id)windowWillReturnFieldEditor:(NSWindow *)sender
                         toObject:(id)client {
    for (GlkWindow *win in _gwindows.allValues)
        if (win.input == client) {
            return win.input.fieldEditor;
        }
    return nil;
}

#pragma mark Narcolepsy window mask

// Create or update the Narcolepsy window mask. The game "Narcolepsy" uses a
// custom image-based mask to create an irregularly shaped visible area,
// producing a distinctive visual effect where parts of the window are transparent.
- (void)adjustMaskLayer:(id)sender {
    CALayer *maskLayer = nil;
    if (!_gameView.layer.mask) {
        _gameView.layer.mask = [self createMaskLayer];
        maskLayer = _gameView.layer.mask;
        maskLayer.layoutManager = [CAConstraintLayoutManager layoutManager];
        maskLayer.autoresizingMask = kCALayerHeightSizable | kCALayerWidthSizable;
        // Make the window non-opaque so the masked-out areas show through
        self.window.opaque = NO;
        self.window.backgroundColor = [NSColor clearColor];
    }

    if (maskLayer) {
        maskLayer.frame = _gameView.bounds;
    }
}

// Build the mask layer from Narcolepsy's image resource #3. The image is
// inverted and converted to an alpha mask so white areas become transparent
// and dark areas become opaque, creating the shaped window effect.
- (CALayer *)createMaskLayer {
    CALayer *layer = [CALayer layer];

    if (![_imageHandler handleFindImageNumber:3]) {
        NSLog(@"createMaskLayer: Failed to load Narcolepsy image 3!");
        return nil;
    } else {
        CIImage *inputImage = [CIImage imageWithData:_imageHandler.resources[@(3)].data];

        CIFilter *invert = [CIFilter filterWithName:@"CIColorInvert"];

        [invert setValue: inputImage forKey:kCIInputImageKey];

        CIImage *imageWithFirstFilter = [invert valueForKey:kCIOutputImageKey];

        CIFilter *mask = [CIFilter filterWithName:@"CIMaskToAlpha"];
        [mask setValue: imageWithFirstFilter forKey:kCIInputImageKey];

        CIImage *result = [mask valueForKey:kCIOutputImageKey];

        CGRect extent = result.extent;
        CGImageRef cgImage = [[CIContext contextWithOptions:nil] createCGImage:result fromRect:extent];

        layer.contents = CFBridgingRelease(cgImage);
    }
    return layer;
}

#pragma mark Window resizing

// Handle the "zoom" button (green traffic light). Returns a frame sized to
// the theme's default character cell dimensions rather than filling the screen.
- (NSRect)windowWillUseStandardFrame:(NSWindow *)window
                        defaultFrame:(NSRect)screenframe {
    NSSize windowSize = [self defaultContentSize];

    NSRect frame = [window frameRectForContentRect:NSMakeRect(0, 0, windowSize.width, windowSize.height)];

    if (frame.size.width > screenframe.size.width)
        frame.size.width = screenframe.size.width;
    if (frame.size.height > screenframe.size.height)
        frame.size.height = screenframe.size.height;

    frame.origin = NSMakePoint(self.window.frame.origin.x, self.window.frame.origin.y - (frame.size.height - self.window.frame.size.height));

    if (NSMaxY(frame) > NSMaxY(screenframe))
        frame.origin.y = NSMaxY(screenframe) - frame.size.height;

    return frame;
}

// Ensure the window frame meets minimum size requirements. If either
// dimension is below the minimum, reset to the theme's default size and
// center on screen. Used when restoring saved window frames that might
// have been saved with invalid dimensions.
- (NSRect)frameWithSanitycheckedSize:(NSRect)rect {
    CGFloat minimumWidth = (CGFloat)kMinimumWindowWidth;
    CGFloat minimumHeight = (CGFloat)kMinimumWindowHeight;

    if (rect.size.width < minimumWidth || rect.size.height < minimumHeight) {
        NSSize defaultSize = [self defaultContentSize];
        NSRect screenFrame = self.window.screen.visibleFrame;
        if (rect.size.width < defaultSize.width) {
            rect.size.width = defaultSize.width;
        }
        if (rect.size.height < defaultSize.height) {
            rect.size.height = defaultSize.height;
            rect.origin.y = round(screenFrame.origin.y + (NSHeight(screenFrame) - defaultSize.height) / 2) + 40;
        }
        rect.origin.x = round((NSWidth(screenFrame) - defaultSize.width) / 2);
    }
    if (rect.size.width < minimumWidth)
        rect.size.width = minimumWidth;
    if (rect.size.height < minimumHeight)
        rect.size.height = minimumHeight;
    return rect;
}

// Calculate the default content view size (excluding the title bar) based on
// the theme's character cell dimensions and margins. This determines the
// initial window size when a game is first opened.
- (NSSize)defaultContentSize {
    NSSize size;
    Theme *theme = _theme;
    // Width = (cell width × columns) + margins on both sides (grid margin + border + 5px padding)
    size.width = round(theme.cellWidth * theme.defaultCols + (theme.gridMarginX + theme.border + 5.0) * 2.0);
    // Height = (cell height × rows) + margins on both sides (grid margin + border)
    size.height = round(theme.cellHeight * theme.defaultRows + (theme.gridMarginY + theme.border) * 2.0);
    return size;
}

// Handle content view resize events by queuing an arrange event to the
// interpreter. Deduplicates identical frames and filters out invalid
// dimensions that can occur during fullscreen transitions.
- (void)contentDidResize:(NSRect)frame {
    if (NSEqualRects(frame, lastContentResize)) {
        return;
    }

    lastContentResize = frame;
    lastSizeInChars = [self contentSizeToCharCells:_gameView.frame.size];

    // Negative dimensions can occur transiently during fullscreen animation
    if (frame.origin.x < 0 || frame.origin.y < 0 || frame.size.width < 0 || frame.size.height < 0) {
        return;
    }

    if (!inFullScreenResize && !dead) {
        GlkEvent *gevent = [[GlkEvent alloc] initArrangeWidth:(NSInteger)frame.size.width
                                                       height:(NSInteger)frame.size.height
                                                        theme:_theme
                                                        force:NO];
        [self queueEvent:gevent];
    }
}

// Resize the game content area to a specific pixel size. Handles both windowed
// and fullscreen modes differently: in windowed mode, resizes the window frame;
// in fullscreen, adjusts the game view within the fixed-size border view.
// Temporarily suppresses resize events to prevent recursive arrange events.
- (void)zoomContentToSize:(NSSize)newSize {
    _ignoreResizes = YES;
    NSRect oldframe = _gameView.frame;

    NSUInteger borders = (NSUInteger)_theme.border * 2;

    if ((self.window.styleMask & NSWindowStyleMaskFullScreen) !=
        NSWindowStyleMaskFullScreen) {

        newSize.width += (CGFloat)borders;
        newSize.height += (CGFloat)borders;

        NSRect screenframe = [NSScreen mainScreen].visibleFrame;

        NSRect contentRect = NSMakeRect(0, 0, newSize.width, newSize.height);

        NSRect winrect = [self.window frameRectForContentRect:contentRect];
        winrect.origin = self.window.frame.origin;

        // If the new size is too big to fit on screen, clip at screen size
        if (NSHeight(winrect) > NSHeight(screenframe))
            winrect.size.height = NSHeight(screenframe);
        if (NSWidth(winrect) > NSWidth(screenframe))
            winrect.size.width = NSWidth(screenframe);

        CGFloat offset = NSHeight(winrect) - NSHeight(self.window.frame);

        winrect.origin.y -= offset;

        [self.window setFrame:winrect display:YES];
        _gameView.frame = [self contentFrameForWindowed];
    } else {
        // We are in fullscreen
        NSRect newframe = NSMakeRect(oldframe.origin.x, oldframe.origin.y,
                                     newSize.width,
                                     NSHeight(_borderView.frame));

        if (NSWidth(newframe) > NSWidth(_borderView.frame) - (CGFloat)borders)
            newframe.size.width = NSWidth(_borderView.frame) - (CGFloat)borders;

        CGFloat widthDiff = NSWidth(oldframe) - NSWidth(newframe);

        newframe.origin.x += widthDiff / 2;

        _windowPreFullscreenFrame.origin.x += widthDiff / 2;
        _windowPreFullscreenFrame.size.width += widthDiff;

        CGFloat offset = NSHeight(newframe) - NSHeight(oldframe);
        newframe.origin.y -= offset;

        _gameView.frame = newframe;
    }
    _ignoreResizes = NO;
}


// Convert a size in character cells (columns × rows) to pixel dimensions
// for the content view. Does not include the border area.
- (NSSize)charCellsToContentSize:(NSSize)cells {
    NSSize size;
    Theme *theme = _theme;
    size.width = round(theme.cellWidth * cells.width + (theme.gridMarginX + 5.0) * 2.0);
    size.height = round(theme.cellHeight * cells.height + (theme.gridMarginY) * 2.0);
    if (isnan(size.height) || isinf(size.height)) {
        NSLog(@"ERROR: charCellsToContentSize: Height is NaN!");
        size.height = 10;
    }
    return size;
}

// Convert a pixel size to character cells (columns × rows). The inverse of
// charCellsToContentSize:. Falls back to hardcoded defaults if no theme is set.
- (NSSize)contentSizeToCharCells:(NSSize)points {
    NSSize size;
    CGFloat cellWidth;
    CGFloat cellHeight;
    CGFloat gridMarginX;
    CGFloat gridMarginY;
    if (_theme == nil)
        _theme = [Preferences currentTheme];
    if (_theme == nil) {
        cellWidth = 8.0;
        cellHeight = 18.0;
        gridMarginX = 5;
        gridMarginY = 0;
    } else {
        cellWidth = _theme.cellWidth;
        cellHeight = _theme.cellHeight;
        gridMarginX = _theme.gridMarginX + 5;
        gridMarginY = _theme.gridMarginY;
    }

    size.width = round((points.width - gridMarginX * 2) / cellWidth);
    size.height = round((points.height - gridMarginY * 2) / cellHeight);

    return size;
}

// Respond to border size changes from the preferences panel. Adjusts the window
// frame to accommodate the new border while keeping the content area stable.
// Only applies in windowed mode when "Adjust window size" is enabled.
- (void)noteBorderChanged:(NSNotification *)notify {
    if (notify.object != _theme || (self.window.styleMask & NSWindowStyleMaskFullScreen) ==
        NSWindowStyleMaskFullScreen || ![[NSUserDefaults standardUserDefaults] boolForKey:@"AdjustSize"])
        return;
    [Preferences instance].inMagnification = YES;
    _movingBorder = YES;
    NSInteger diff = ((NSNumber *)notify.userInfo[@"diff"]).integerValue;
    _gameView.autoresizingMask = NSViewMaxXMargin | NSViewMaxYMargin | NSViewMinXMargin | NSViewMinYMargin;
    NSRect frame = self.window.frame;
    frame.origin = NSMakePoint(frame.origin.x - diff, frame.origin.y - diff);
    frame.size = NSMakeSize(frame.size.width + (diff * 2), frame.size.height + (diff * 2));
    NSRect contentframe = _gameView.frame;
    contentframe.origin = NSMakePoint(contentframe.origin.x + diff, contentframe.origin.y + diff);
    [self.window setFrame:frame display:NO];
    _gameView.frame = contentframe;
    _movingBorder = NO;
    _gameView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    NSNotification *notification = [[NSNotification alloc] initWithName:@"PreferencesChanged" object:_theme userInfo:nil];
    [self notePreferencesChanged:notification];
}

// Queue an arrange event to tell the interpreter about the current window
// dimensions. When force is YES, the event is sent even if dimensions
// haven't changed (used after preference changes that affect layout).
- (void)sendArrangeEventWithFrame:(NSRect)frame force:(BOOL)force {
    GlkEvent *gevent = [[GlkEvent alloc] initArrangeWidth:(NSInteger)frame.size.width

                                                   height:(NSInteger)frame.size.height
                                                    theme:_theme
                                                    force:force];
    [self queueEvent:gevent];
}


/*
 *
 */

#pragma mark Preference and style hint glue

// Central handler for theme and preference changes. Responsible for:
// - Resolving the active theme (which may differ from game.theme if a
//   light/dark mode override is active, or during autorestore)
// - Re-identifying game-specific behavior if the nohacks setting changed
// - Updating autosave settings
// - Resizing the window to maintain the same character cell count if
//   the "Adjust window size" preference is enabled
// - Sending updated prefs and arrange events to the interpreter
// - Propagating the new theme to all Glk windows and quote boxes
- (void)notePreferencesChanged:(NSNotification *)notify {

    // Skip if we're in the middle of a border adjustment (which triggers
    // its own PreferencesChanged notification after completing)
    if (_movingBorder) {
        return;
    }
    Theme *theme = _theme;

    // Determine the active theme, accounting for dark/light mode overrides.
    // During autorestore, _stashedTheme holds the original theme while we
    // temporarily use the autosaved theme.
    if (_game) {
        if (!_stashedTheme) {
            if ([Preferences instance].lightOverrideActive)
                _theme = [Preferences instance].lightTheme;
            else if ([Preferences instance].darkOverrideActive)
                _theme = [Preferences instance].darkTheme;
            else
                _theme = _game.theme;
            theme = _theme;
        }
    } else {
        return;
    }

    if (notify.object != theme && notify.object != nil) {
        //  PreferencesChanged called for a different theme
        return;
    }

    if (theme.nohacks) {
        [self resetGameIdentification];
    } else {
        [self identifyGame:_game.ifid];
    }

    if (theme.vOSpeakMenu == kVOMenuNone) { // "Check for menu" was switched off
        if (_zmenu) {
            [NSObject cancelPreviousPerformRequestsWithTarget:_zmenu];
            _zmenu = nil;
        }
        if (_form) {
            [NSObject cancelPreviousPerformRequestsWithTarget:_form];
            _form = nil;
        }
    } else if (lastVOSpeakMenu == kVOMenuNone && theme.vOSpeakMenu != kVOMenuNone) { // "Check for menu" was switched on
        [self checkZMenuAndSpeak:NO];
    }

    lastVOSpeakMenu = theme.vOSpeakMenu;

    if (_supportsAutorestore) {
        if (theme.autosave == NO) {
            if (_game.autosaved) {
                [self deleteAutosaveFiles];
            }
            _game.autosaved = NO;
            _hasAutoSaved = NO;
            self.window.restorable = NO;
        } else {
            self.window.restorable = YES;
            if (!autorestoring) { // Do not autosave until we have fully autorestored.
                _game.autosaved = YES;
                [self handleAutosave:self.autosaveTag];
            }
        }
    }

    NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;

    if (![defaults boolForKey:@"SaveInGameDirectory"]) {
        self.saveDir = nil;
    }

    _shouldStoreScrollOffset = NO;
    if ([defaults boolForKey:@"AdjustSize"] && !autorestoring) {
        if (lastTheme != theme && !NSEqualSizes(lastSizeInChars, NSZeroSize)) { // Theme changed
            NSSize newContentSize = [self charCellsToContentSize:lastSizeInChars];
            CGFloat borders = theme.border * 2;
            NSSize newSizeIncludingBorders = NSMakeSize(newContentSize.width + borders, newContentSize.height + borders);

            if (!NSEqualSizes(_borderView.bounds.size, newSizeIncludingBorders)
                || !NSEqualSizes(_gameView.bounds.size, newContentSize)) {
                [self zoomContentToSize:newContentSize];
                //                NSLog(@"Changed window size to keep size in char cells constant. Previous size in char cells: %@ Current size in char cells: %@", NSStringFromSize(lastSizeInChars), NSStringFromSize([self contentSizeToCharCells:_contentView.frame.size]));
            }
        }
    }

    lastTheme = theme;
    lastSizeInChars = [self contentSizeToCharCells:_gameView.frame.size];

    [self adjustContentView];

    if (!_gwindows.count) {
        // No _gwindows, nothing to do.
        return;
    }

    if (theme.borderBehavior == kUserOverride && ![_bgcolor isEqualToColor:theme.borderColor]) {
        [self setBorderColor:theme.borderColor];
    } else if (theme.borderBehavior == kAutomatic && ![_lastAutoBGColor isEqualToColor:_bgcolor]) {
        [self setBorderColor:_lastAutoBGColor];
    }

    GlkEvent *gevent;

    CGFloat width = _gameView.frame.size.width;
    CGFloat height = _gameView.frame.size.height;

    if (width < 0)
        width = 0;
    if (height < 0)
        height = 0;

    gevent = [[GlkEvent alloc] initPrefsEventForTheme:theme];
    [self queueEvent:gevent];
    gevent = [[GlkEvent alloc] initArrangeWidth:(NSInteger)width
                                         height:(NSInteger)height
                                          theme:theme
                                          force:YES];
    [self queueEvent:gevent];

    for (GlkWindow *win in _gwindows.allValues)
    {
        if (!autorestoring || win.theme != theme) {
            win.theme = theme;
            [win prefsDidChange];
        }
    }

    for (GlkTextGridWindow *quotebox in _quoteBoxes)
    {
        quotebox.theme = theme;
        quotebox.alphaValue = 0;
        [quotebox prefsDidChange];
        [quotebox performSelector:@selector(quoteboxAdjustSize:) withObject:nil afterDelay:0.2];
    }

    // Reset any Narcolepsy window mask
    _gameView.layer.mask = nil;

    if (self.gameID == kGameIsNarcolepsy && theme.doGraphics && theme.doStyles) {
        [self adjustMaskLayer:nil];
    }

    _shouldStoreScrollOffset = YES;
}

// Handle system appearance changes (light/dark mode toggle). Switches the
// active theme to the appropriate light or dark override theme if configured,
// then triggers a full preferences update to propagate the change.
- (void)noteColorModeChanged:(NSNotification *)notification {
    Theme *oldTheme = _theme;
    if ([Preferences instance].darkOverrideActive) {
        if ([Preferences instance].darkTheme) {
            if (_theme != [Preferences instance].darkTheme) {
                _theme = [Preferences instance].darkTheme;
            }
        } else {
            NSLog(@"ERROR: darkOverrideActive but no hard dark theme exists, use normal selected theme");
            _theme = _game.theme;
        }
    } else if ([Preferences instance].lightOverrideActive) {
        if ([Preferences instance].lightTheme) {
            if (_theme != [Preferences instance].lightTheme) {
                _theme = [Preferences instance].lightTheme;
            }
        } else {
            NSLog(@"ERROR: lightOverrideActive but no hard light theme exists, use normal selected theme");
            _theme = _game.theme;
        }
    } else {
        _theme = _game.theme;
    }

    if (_theme != oldTheme) {
        [self notePreferencesChanged:[NSNotification notificationWithName:@"PreferencesChanged" object:_theme]];
    }
}


#pragma mark Zoom

// Zoom actions (Cmd+Plus, Cmd+Minus, Cmd+0) scale the font size and
// optionally resize the window to maintain the same number of character cells.
// Disabled when showing cover art in fullscreen to prevent layout issues.
- (IBAction)zoomIn:(id)sender {
    if (_showingCoverImage && _inFullscreen)
        return;
    [Preferences instance].inMagnification = YES;
    [Preferences zoomIn];
    if (Preferences.instance)
        [Preferences.instance updatePanelAfterZoom];
}

- (IBAction)zoomOut:(id)sender {
    if (_showingCoverImage && _inFullscreen)
        return;
    [Preferences instance].inMagnification = YES;
    [Preferences zoomOut];
    if (Preferences.instance)
        [Preferences.instance updatePanelAfterZoom];
}

- (IBAction)zoomToActualSize:(id)sender {
    if (_showingCoverImage && _inFullscreen)
        return;
    [Preferences instance].inMagnification = YES;
    [Preferences zoomToActualSize];
    if (Preferences.instance)
        [Preferences.instance updatePanelAfterZoom];
}

// Respond to changes in the default character cell size (triggered by zoom or
// theme font changes). Resizes the window to match the new dimensions while
// preventing paradoxical size changes (shrinking when zooming in, etc.).
- (void)noteDefaultSizeChanged:(NSNotification *)notification {

    if (notification.object != _game.theme)
        return;

    NSSize sizeAfterZoom;
    if ([Preferences instance].inMagnification) {
        if ([[NSUserDefaults standardUserDefaults] boolForKey:@"AdjustSize"] == NO)
            return;
        if (lastSizeInChars.width == 0) {
            sizeAfterZoom = [self defaultContentSize];
        } else {
            sizeAfterZoom = [self charCellsToContentSize:lastSizeInChars];
            sizeAfterZoom.width += _theme.border * 2;
            sizeAfterZoom.height += _theme.border * 2;
        }
    } else {
        sizeAfterZoom = [self defaultContentSize];
    }
    NSRect oldframe = _gameView.frame;

    // Prevent the window from shrinking when zooming in or growing when
    // zooming out, which might otherwise happen at edge cases
    if ((sizeAfterZoom.width < oldframe.size.width && Preferences.zoomDirection == ZOOMIN) ||
        (sizeAfterZoom.width > oldframe.size.width && Preferences.zoomDirection == ZOOMOUT)) {
        NSLog(@"noteDefaultSizeChanged: This would change the size in the wrong direction, so skip");
        return;
    }

    if ((self.window.styleMask & NSWindowStyleMaskFullScreen) !=
        NSWindowStyleMaskFullScreen) {

        NSRect screenframe = [NSScreen mainScreen].visibleFrame;

        NSRect contentRect = NSMakeRect(0, 0, sizeAfterZoom.width, sizeAfterZoom.height);

        NSRect winrect = [self.window frameRectForContentRect:contentRect];
        winrect.origin = self.window.frame.origin;

        // If the new size is too big to fit on screen, clip at screen size
        if (NSHeight(winrect) > NSHeight(screenframe) - 1)
            winrect.size.height = NSHeight(screenframe) - 1;
        if (NSWidth(winrect) > NSWidth(screenframe))
            winrect.size.width = NSWidth(screenframe);

        CGFloat offset = NSHeight(winrect) - NSHeight(self.window.frame);

        winrect.origin.y -= offset;


        [self.window setFrame:winrect display:NO animate:NO];
    } else {
        CGFloat borders = _theme.border * 2;
        NSRect newframe = NSMakeRect(oldframe.origin.x, oldframe.origin.y,
                                     sizeAfterZoom.width - borders,
                                     NSHeight(_borderView.frame) - borders);

        if (NSWidth(newframe) > NSWidth(_borderView.frame) - borders)
            newframe.size.width = NSWidth(_borderView.frame) - borders;

        newframe.origin.x += (NSWidth(oldframe) - NSWidth(newframe)) / 2;

        CGFloat offset = NSHeight(newframe) - NSHeight(oldframe);
        newframe.origin.y -= offset;

        _gameView.frame = newframe;
    }
}


/*
 *
 */

#pragma mark Command script

// Command scripts allow replaying a sequence of text commands into the game,
// either from a file or from the clipboard. The handler is lazily created.
@synthesize commandScriptHandler = _commandScriptHandler;

- (CommandScriptHandler *)commandScriptHandler {
    if (_commandScriptHandler == nil) {
        _commandScriptHandler = [CommandScriptHandler new];
        _commandScriptHandler.glkctl = self;
    }
    return _commandScriptHandler;
}

- (void)setCommandScriptHandler:(CommandScriptHandler *)commandScriptHandler {
    _commandScriptHandler = commandScriptHandler;
}

// Cancel a running command script (Cmd+. / Escape)
- (IBAction)cancel:(id)sender
{
    _commandScriptRunning = NO;
    _commandScriptHandler = nil;
}

#pragma mark Dragging and dropping

// Drag and drop support: accepts text strings and URLs dropped onto the game
// window. Dropped content is treated as a command script and fed into the game
// as a sequence of text commands, enabling easy "paste and play" functionality.
- (NSDragOperation)draggingEntered:sender {
    NSPasteboard *pboard = [sender draggingPasteboard];

    if ( [pboard.types containsObject:NSPasteboardTypeString] ||
        [pboard.types containsObject:NSPasteboardTypeURL] ) {
        return NSDragOperationCopy;
    }

    return NSDragOperationNone;
}

- (BOOL)performDragOperation:sender {
    NSPasteboard *pboard = [sender draggingPasteboard];
    return [self.commandScriptHandler commandScriptInPasteboard:pboard fromWindow:nil];
}


@end
