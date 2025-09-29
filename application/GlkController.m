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

#import "Game.h"
#import "Theme.h"
#import "Image.h"
#import "Fetches.h"

#import "NSColor+integer.h"
#import "NSData+Categories.h"
#import "NSString+Categories.h"

#include "glkimp.h"
#include "protocol.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
fprintf(stderr, "%s\n",                                                    \
[[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String])
#else
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

////static const char *wintypenames[] = {"wintype_AllTypes", "wintype_Pair",
////    "wintype_Blank",    "wintype_TextBuffer",
////    "wintype_TextGrid", "wintype_Graphics"};
//

// static const char *stylehintnames[] =
//{
//    "stylehint_Indentation", "stylehint_ParaIndentation",
//    "stylehint_Justification", "stylehint_Size",
//    "stylehint_Weight","stylehint_Oblique", "stylehint_Proportional",
//    "stylehint_TextColor", "stylehint_BackColor", "stylehint_ReverseColor",
//    "stylehint_NUMHINTS"
//};

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


@implementation GlkHelperView

- (BOOL)isFlipped {
    return YES;
}

- (void)setFrame:(NSRect)frame {
    super.frame = frame;
    GlkController *glkctl = _glkctrl;

    if (glkctl.alive && !self.inLiveResize && !glkctl.ignoreResizes) {
        [glkctl contentDidResize:frame];
    }
}

- (void)viewWillStartLiveResize {
    GlkController *glkctl = _glkctrl;
    if ((glkctl.window.styleMask & NSWindowStyleMaskFullScreen) !=
        NSWindowStyleMaskFullScreen && !glkctl.ignoreResizes)
        [glkctl storeScrollOffsets];
}

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

@interface GlkController () {
    /* for talking to the interpreter */
    NSTask *task;
    NSFileHandle *readfh;
    NSFileHandle *sendfh;

    /* current state of the protocol */
    NSTimer *timer;
    BOOL waitforevent;    /* terp wants an event */
    BOOL waitforfilename; /* terp wants a filename from a file dialog */
    BOOL dead;            /* le roi est mort! vive le roi! */
    NSDictionary *lastArrangeValues;
    NSRect lastContentResize;

    BOOL inFullScreenResize;

    BOOL windowRestoredBySystem;
    BOOL shouldRestoreUI;

    // When a game supports autosave, but is still on its first turn,
    // so that no interpreter autosave file exists, we still restore the UI
    // to catch entered text and scroll position
    BOOL restoreUIOnly;

    // Used for fullscreen animation
    NSWindowController *snapshotController;

    BOOL windowClosedAlready;
    BOOL restartingAlready;

    /* the glk objects */
    BOOL windowdirty; /* the contentView needs to repaint */

    GlkController *restoredController;
    GlkController *restoredControllerLate;
    NSMutableData *bufferedData;

    TableViewController *libcontroller;

    NSSize lastSizeInChars;
    Theme *lastTheme;

    // To fix scrolling in the Adrian Mole games
    NSInteger lastRequest;

    // Command script handling
    BOOL skipNextScriptCommand;
    NSDate *lastScriptKeyTimestamp;
    NSDate *lastKeyTimestamp;

    kVOMenuPrefsType lastVOSpeakMenu;
    BOOL shouldAddTitlePrefixToSpeech;
    BOOL changedBorderThisTurn;
    BOOL autorestoring;
}

@property (nonatomic) JourneyMenuHandler *journeyMenuHandler;

@property NSURL *saveDir;

@end

@implementation GlkController

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
        game.theme = [Preferences currentTheme];
        _theme = game.theme;
    }

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

    /* Setup blank style hints for both kinds of windows */

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

    /* Setup our own stuff */

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

    lastArrangeValues = @{
        @"width" : @(0),
        @"height" : @(0),
        @"bufferMargin" : @(0),
        @"gridMargin" : @(0),
        @"charWidth" : @(0),
        @"lineHeight" : @(0),
        @"leading" : @(0)
    };

    _gwindows = [[NSMutableDictionary alloc] init];
    _windowsToBeAdded = [[NSMutableArray alloc] init];
    _windowsToBeRemoved = [[NSMutableArray alloc] init];
    bufferedData = nil;

    if (game.metadata.title.length)
        self.window.title = game.metadata.title;

    waitforevent = NO;
    waitforfilename = NO;
    dead = YES; // This should be YES until the interpreter process is running

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

    _borderView.wantsLayer = YES;
    _borderView.layerContentsRedrawPolicy = NSViewLayerContentsRedrawOnSetNeedsDisplay;
    _lastAutoBGColor = _theme.bufferBackground;
    if (_theme.borderBehavior == kUserOverride)
        [self setBorderColor:_theme.borderColor];
    else
        [self setBorderColor:_theme.bufferBackground];

    NSString *autosaveLatePath = [self.appSupportDir
                                  stringByAppendingPathComponent:@"autosave-GUI-late.plist"];


    lastScriptKeyTimestamp = [NSDate distantPast];
    lastKeyTimestamp = [NSDate distantPast];

    if (self.gameID == kGameIsNarcolepsy && _theme.doGraphics && _theme.doStyles) {
        [self adjustMaskLayer:nil];
    }

    if (_supportsAutorestore && _theme.autosave) {
        [self dealWithOldFormatAutosaveDir];
    }
    if (_supportsAutorestore && _theme.autosave &&
        ([[NSFileManager defaultManager] fileExistsAtPath:self.autosaveFileGUI] || [[NSFileManager defaultManager] fileExistsAtPath:autosaveLatePath])) {
        [self runTerpWithAutorestore];
    } else {
        [self deleteAutosaveFiles];
        [self runTerpNormal];
    }
}

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

    // Check if GUI late autosave file exists
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

    _inFullscreen = restoredControllerLate.inFullscreen;
    _windowPreFullscreenFrame = restoredController.windowPreFullscreenFrame;
    _shouldStoreScrollOffset = NO;

    // If the process is dead, restore the dead window if this
    // is a system window restoration at application start
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

    if (restoreUIOnly && restoredControllerLate.hasAutoSaved) {
        NSLog(@"restoredControllerLate was not saved at the first turn!");
        NSLog(@"Delete autosave files and start normally without restoring UI");
        restoreUIOnly = NO;
        [self deleteAutosaveFiles];
        game.autosaved = NO;
        [self runTerpNormal];
        return;
    }

    // Temporarily switch to the theme used in the autosaved GUI
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

    // The game has to run to its third(?) NEXTEVENT
    // before we can restore the UI properly, so we don't
    // have to do anything else here for now.
}

- (void)runTerpNormal {
    autorestoring = NO;
    // Just start the game with no autorestore or fullscreen or resetting
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

        // Very lazy cascading
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

- (BOOL)runWindowsRestorationHandler {
    if (_restorationHandler == nil) {
        return NO;
    }

    _restorationHandler(self.window, nil);
    _restorationHandler = nil;
    return YES;
}

- (void)restoreWindowWhenDead {
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

- (void)resetGameIdentification {
    _gameID = kGameIsGeneric;
}

- (BOOL)zVersion6 {
    return (_gameID == kGameIsArthur || _gameID == kGameIsJourney || _gameID == kGameIsShogun ||  _gameID == kGameIsZorkZero);
}

- (void)forkInterpreterTask {
    /* Fork the interpreter process */

    Theme *theme = _theme;

    NSString *terppath;
    NSPipe *readpipe;
    NSPipe *sendpipe;

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

    if ([_game.detectedFormat isEqualToString:@"tads3"]) {
        if (theme.determinism) {
            NSArray *extraTadsOptions = @[@"-norandomize"];
            task.arguments = [extraTadsOptions arrayByAddingObjectsFromArray:task.arguments];
        }
    }

#endif // TEE_TERP_OUTPUT

    GlkController * __weak weakSelf = self;

    task.terminationHandler = ^(NSTask *aTask) {
        [aTask.standardOutput fileHandleForReading].readabilityHandler = nil;
        GlkController *strongSelf = weakSelf;
        if(strongSelf) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [strongSelf noteTaskDidTerminate:aTask];
            });
        }
    };

    _queue = [[NSMutableArray alloc] init];

    [[NSNotificationCenter defaultCenter]
     addObserver: self
     selector: @selector(noteDataAvailable:)
     name: NSFileHandleDataAvailableNotification
     object: readfh];

    dead = NO;

    if (_secureBookmark == nil) {
        _secureBookmark = [FolderAccess grantAccessToFile:_gameFileURL];
    }

    [task launch];

    /* Send a prefs and an arrange event first thing */
    GlkEvent *gevent;

    gevent = [[GlkEvent alloc] initPrefsEventForTheme:theme];
    [self queueEvent:gevent];

    [self sendArrangeEventWithFrame:_gameView.frame force:NO];

    restartingAlready = NO;
    [readfh waitForDataInBackgroundAndNotify];
}

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

#pragma mark Autorestore

- (GlkTextGridWindow *)findGridWindowIn:(NSView *)theView
{
    // Search the subviews for a view of class GlkTextGridWindow
    GlkTextGridWindow __block __weak *(^weak_findGridWindow)(NSView *);

    GlkTextGridWindow * (^findGridWindow)(NSView *);

    weak_findGridWindow = findGridWindow = ^(NSView *view) {
        if ([view isKindOfClass:[GlkTextGridWindow class]])
            return (GlkTextGridWindow *)view;
        GlkTextGridWindow __block *foundView = nil;
        [view.subviews enumerateObjectsUsingBlock:^(NSView *subview, NSUInteger idx, BOOL *stop) {
            foundView = weak_findGridWindow(subview);
            if (foundView)
                *stop = YES;
        }];
        return foundView;
    };

    return findGridWindow(theView);
}

- (Theme *)findThemeByName:(NSString *)name {
    if (!name || name.length == 0)
        return nil;
    NSFetchRequest *fetchRequest = [Theme fetchRequest];
    NSManagedObjectContext *context = libcontroller.managedObjectContext;
    Theme *foundTheme = nil;
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"name like[c] %@", name];
    NSError *error = nil;
    NSArray *fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];

    if (fetchedObjects && fetchedObjects.count) {
        foundTheme = fetchedObjects[0];
    } else {
        if (error != nil)
            NSLog(@"GlkController findThemeByName: %@", error);
    }
    return foundTheme;
}

- (void)restoreUI {
    // We try to restore the UI here, in order to catch things
    // like entered text and scrolling, that has changed the UI
    // but not sent any events to the interpreter process.
    // This method is called in handleRequest on NEXTEVENT.

    if (restoreUIOnly) {
        restoredController = restoredControllerLate;
        _shouldShowAutorestoreAlert = NO;
    } else {
        _windowsToBeAdded = [[NSMutableArray alloc] init];
    }

    shouldRestoreUI = NO;
    _shouldStoreScrollOffset = NO;

    _soundHandler = restoredControllerLate.soundHandler;
    _soundHandler.glkctl = self;
    [_soundHandler restartAll];

    GlkWindow *win;

    // Copy values from autorestored GlkController object
    _firstResponderView = restoredControllerLate.firstResponderView;
    _windowPreFullscreenFrame = [self frameWithSanitycheckedSize:restoredControllerLate.windowPreFullscreenFrame];
    _autosaveTag = restoredController.autosaveTag;

    _bufferStyleHints = restoredController.bufferStyleHints;
    _gridStyleHints = restoredController.gridStyleHints;

    // Restore frame size
    _gameView.frame = restoredControllerLate.storedGameViewFrame;

    // Copy all views and GlkWindow objects from restored Controller
    for (id key in restoredController.gwindows.allKeys) {

        win = _gwindows[key];

        if (!restoreUIOnly) {
            if (win) {
                [win removeFromSuperview];
            }
            win = (restoredController.gwindows)[key];

            _gwindows[@(win.name)] = win;

            if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
                NSScrollView *scrollview = ((GlkTextBufferWindow *)win).textview.enclosingScrollView;

                GlkTextGridWindow *aView = [self findGridWindowIn:scrollview];
                while (aView) {
                    [aView removeFromSuperview];
                    aView = [self findGridWindowIn:scrollview];
                }

                GlkTextGridWindow *quotebox = ((GlkTextBufferWindow *)win).quoteBox;
                if (quotebox && (restoredController.numberOfPrintsAndClears == quotebox.quoteboxAddedOnPAC) && (restoredController.printsAndClearsThisTurn == 0 || quotebox.quoteboxAddedOnPAC == 0)) {
                    if (_quoteBoxes == nil)
                        _quoteBoxes = [[NSMutableArray alloc] init];
                    [quotebox removeFromSuperview];
                    [_quoteBoxes addObject:quotebox];
                    quotebox.glkctl = self;
                    quotebox.quoteboxParent = ((GlkTextBufferWindow *)win).textview.enclosingScrollView;
                }
            }

            [win removeFromSuperview];
            [_gameView addSubview:win];

            win.glkctl = self;
            win.theme = _theme;

        // Ugly hack to make sure quote boxes shown at game start still appear when restoring UI only
        } else if (_quoteBoxes.count == 0 && _restorationHandler && [win isKindOfClass:[GlkTextBufferWindow class]]) {
            GlkWindow *restored = restoredController.gwindows[@(win.name)];
            GlkTextGridWindow *quotebox = ((GlkTextBufferWindow *)restored).quoteBox;
            if (quotebox) {
                _quoteBoxes = [[NSMutableArray alloc] init];
                [quotebox removeFromSuperview];
                [_quoteBoxes addObject:quotebox];
                quotebox.glkctl = self;
                quotebox.quoteboxAddedOnPAC = _numberOfPrintsAndClears;
                quotebox.quoteboxParent = ((GlkTextBufferWindow *)win).textview.enclosingScrollView;
                ((GlkTextBufferWindow *)win).quoteBox = quotebox;
            }
        }
        GlkWindow *laterWin = (restoredControllerLate.gwindows)[key];
        win.frame = laterWin.frame;
    }

    // This makes autorestoring in fullscreen a little less flickery
    [self adjustContentView];

    if (restoredControllerLate.bgcolor)
        [self setBorderColor:restoredControllerLate.bgcolor];

    GlkWindow *winToGrabFocus = nil;

    if (restoredController.commandScriptRunning) {
        [self.commandScriptHandler copyPropertiesFrom:restoredController.commandScriptHandler];
    }

    if (_gameID == kGameIsJourney && restoredController.journeyMenuHandler) {
        self.journeyMenuHandler = restoredController.journeyMenuHandler;
        restoredController.journeyMenuHandler = nil;
        _journeyMenuHandler.delegate = self;
        _journeyMenuHandler.textGridWindow = (GlkTextGridWindow *)_gwindows[@(_journeyMenuHandler.gridTextWinName)];
        _journeyMenuHandler.textBufferWindow = (GlkTextBufferWindow *)_gwindows[@(_journeyMenuHandler.bufferTextWinName)];

        [_journeyMenuHandler showJourneyMenus];
    }

    // Restore scroll position etc
    for (win in _gwindows.allValues) {
        if (![win isKindOfClass:[GlkGraphicsWindow class]] && !_windowsToRestore.count) {
            [win postRestoreAdjustments:(restoredControllerLate.gwindows)[@(win.name)]];
        }
        if (win.name == _firstResponderView) {
            winToGrabFocus = win;
        }
        if (_commandScriptRunning && win.name == _commandScriptHandler.lastCommandWindow) {
            if (self.commandScriptHandler.lastCommandType == kCommandTypeLine) {
                [self.commandScriptHandler sendCommandLineToWindow:win];
            } else {
                [self.commandScriptHandler sendCommandKeyPressToWindow:win];
            }
        }
    }

    if (winToGrabFocus)
        [winToGrabFocus grabFocus];

    if (!restoreUIOnly)
        _hasAutoSaved = YES;

    restoredController = nil;
    restoredControllerLate = nil;
    restoreUIOnly = NO;

    // We create a forced arrange event in order to force the interpreter process
    // to re-send us window sizes. The player may have changed settings that
    // affect window size since the autosave was created.

    [self performSelector:@selector(postRestoreArrange:) withObject:nil afterDelay:0.2];
}


- (void)postRestoreArrange:(id)sender {
    if (!_startingInFullscreen) {
        [self performSelector:@selector(showAutorestoreAlert:) withObject:nil afterDelay:0.1];
    }

    Theme *stashedTheme = _stashedTheme;
    if (stashedTheme && stashedTheme != _theme) {
        _theme = stashedTheme;
        _stashedTheme = nil;
    }
    NSNotification *notification = [NSNotification notificationWithName:@"PreferencesChanged" object:_theme];
    [self notePreferencesChanged:notification];
    _shouldStoreScrollOffset = YES;

    // Now we can actually show the window
    if (![self runWindowsRestorationHandler]) {
        [self showWindow:nil];
        [self.window makeKeyAndOrderFront:nil];
        [self.window makeFirstResponder:nil];
    }
    if (_startingInFullscreen) {
        [self performSelector:@selector(deferredEnterFullscreen:) withObject:nil afterDelay:0.1];
    } else {
        autorestoring = NO;
    }
}


- (NSString *)appSupportDir {
    if (!_appSupportDir) {
        NSDictionary *gFolderMap = @{@"adrift" : @"SCARE",
                                     @"advsys" : @"AdvSys",
                                     @"agt"    : @"AGiliTy",
                                     @"glulx"  : @"Glulxe",
                                     @"hugo"   : @"Hugo",
                                     @"level9" : @"Level 9",
                                     @"magscrolls" : @"Magnetic",
                                     @"quest4" : @"Quest",
                                     @"quill"  : @"UnQuill",
                                     @"tads2"  : @"TADS",
                                     @"tads3"  : @"TADS",
                                     @"zcode"  : @"Bocfel",
                                     @"sagaplus" : @"Plus",
                                     @"scott"  : @"ScottFree",
                                     @"jacl"   : @"JACL",
                                     @"taylor" : @"TaylorMade"
        };

        NSDictionary *gFolderMapExt = @{@"acd" : @"Alan 2",
                                        @"a3c" : @"Alan 3",
                                        @"d$$" : @"AGiliTy"};

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

        Game *game = _game;
        NSString *detectedFormat = game.detectedFormat;

        if (!detectedFormat) {
            NSLog(@"GlkController appSupportDir: Game %@ has no specified format!", game.metadata.title);
            return nil;
        }

        NSString *signature = _gamefile.signatureFromFile;

        if (signature.length == 0) {
            signature = game.hashTag;
            if (signature.length == 0) {
                NSLog(@"GlkController appSupportDir: Could not create signature from game file \"%@\"!", _gamefile);
                return nil;
            }
        }

        NSString *terpFolder =
        [gFolderMap[detectedFormat]
         stringByAppendingString:@" Files"];

        if (!terpFolder) {
            terpFolder = [gFolderMapExt[_gamefile.pathExtension.lowercaseString]
                          stringByAppendingString:@" Files"];
        }

        if (!terpFolder) {
            NSLog(@"GlkController appSupportDir: Could not map game format %@ to a folder name!", detectedFormat);
            return nil;
        }

        NSString *dirstr =
        [@"Spatterlight" stringByAppendingPathComponent:terpFolder];
        dirstr = [dirstr stringByAppendingPathComponent:@"Autosaves"];
        dirstr = [dirstr
                  stringByAppendingPathComponent:signature];
        dirstr = [dirstr stringByAddingPercentEncodingWithAllowedCharacters:[NSCharacterSet URLPathAllowedCharacterSet]];

        appSupportURL = [NSURL URLWithString:dirstr
                               relativeToURL:appSupportURL];

        if (_supportsAutorestore && _theme.autosave) {
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
        _appSupportDir = appSupportURL.path;
    }
    return _appSupportDir;
}

// If there exists an autosave dir using the old hashing method,
// copy any files from it to the new one, unless there are newer
// equivalents in the new-style autosave directory. Then delete the
// old files and the old directory.

-(void)dealWithOldFormatAutosaveDir {
    NSError *error = nil;

    NSURL *newDirURL = [NSURL fileURLWithPath:self.appSupportDir isDirectory:YES];

    // Create the URL an old-format autosave directory would have
    NSString *oldDirPath = [self.appSupportDir stringByDeletingLastPathComponent];
    oldDirPath = [oldDirPath stringByAppendingPathComponent:_gamefile.oldSignatureFromFile];
    NSURL *oldDirURL = [NSURL fileURLWithPath:oldDirPath isDirectory:YES];

    // Get a list of files in the old directory and iterate through them
    NSArray<NSURL *> *files = [[NSFileManager defaultManager] contentsOfDirectoryAtURL:oldDirURL includingPropertiesForKeys:nil options:0 error:&error];
    if (files.count) {
        for (NSURL *url in files) {
            error = nil;
            NSData *data = [NSData dataWithContentsOfURL:url options:NSDataReadingMappedAlways error:&error];
            if (!data) {
                NSLog(@"GlkController: Could not read file in old autosave directory. Error: %@", error);
                continue;
            } else {
                NSString *filename = [url lastPathComponent];
                NSDate *dateInOld = nil, *dateInNew = nil;

                // Get the creation date of the file
                error = nil;
                NSDictionary *attrs = [[NSFileManager defaultManager] attributesOfItemAtPath:url.path error:&error];
                if (attrs) {
                    dateInOld = (NSDate*)attrs[NSFileCreationDate];
                } else {
                    NSLog(@"GlkController: Failed to get creation date of file %@ in old directory. Error:%@", url.path, error);
                }
                error = nil;
                NSURL *newfileURL = [newDirURL URLByAppendingPathComponent:filename];
                BOOL overwrite = YES;

                // Get the creation date of the file with the same name in the new directory,
                // if there is one.
                error = nil;
                attrs = [[NSFileManager defaultManager] attributesOfItemAtPath:newfileURL.path error:&error];
                if (attrs) {
                    dateInNew = (NSDate*)attrs[NSFileCreationDate];
                    if (dateInNew && dateInOld && [dateInNew compare:dateInOld] == NSOrderedDescending) {
                        NSLog(@"%@: Newer file exists in new directory. Don't overwrite! dateInNew:%@ dateInOld:%@", filename, dateInNew, dateInOld);
                        overwrite = NO;
                    }
                } else {
                    NSLog(@"GlkController: Failed to get creation date of file %@ in new directory. Error:%@", url.path, error);
                }

                // Copy the file from the old directory to the new, overwriting any older files there
                BOOL result = NO;
                if (overwrite) {
                    result = [data writeToURL:newfileURL options:NSDataWritingAtomic error:&error];
                    if (!result) {
                        NSLog(@"GlkController: Failed to copy file %@ to new autosave directory. Error:%@", filename, error);
                    }
                }

                // Delete the original file
                result = [[NSFileManager defaultManager] removeItemAtURL:url error:&error];
                if (!result) {
                    NSLog(@"GlkController: Failed to delete file %@ in old autosave directory. Error:%@", filename, error);
                }
            }
        }
        // Delete the old directory
        BOOL result = [[NSFileManager defaultManager] removeItemAtURL:oldDirURL error:&error];
        if (!result) {
            NSLog(@"GlkController: Failed to delete old autosave directory at %@. Error:%@", oldDirURL.path, error);
        }
        // Re-write the late UI autosave, to give it a creation date later than autosave-GUI.plist
        // (Our autorestore code will compare the creation dates of autosave-GUI.plist and
        // autosave-GUI-late.plist.)
        NSURL *lateUIURL = [newDirURL URLByAppendingPathComponent:@"autosave-GUI-late.plist"];
        NSData *lateUISave = [NSData dataWithContentsOfURL:lateUIURL options:NSDataReadingMappedAlways error:&error];
        if (lateUISave) {
            [lateUISave writeToURL:lateUIURL options:NSDataWritingAtomic error:&error];
        }
    }
}

- (NSString *)autosaveFileGUI {
    if (!_autosaveFileGUI)
        _autosaveFileGUI = [self.appSupportDir
                            stringByAppendingPathComponent:@"autosave-GUI.plist"];
    return _autosaveFileGUI;
}

- (NSString *)autosaveFileTerp {
    if (!_autosaveFileTerp)
        _autosaveFileTerp =
        [self.appSupportDir stringByAppendingPathComponent:@"autosave.plist"];
    return _autosaveFileTerp;
}

// TableViewController calls this to reset non-running games
- (void)deleteAutosaveFilesForGame:(Game *)aGame {
    _gamefile = aGame.urlForBookmark.path;
    aGame.autosaved = NO;
    if (!_gamefile)
        return;
    _game = aGame;
    [self deleteAutosaveFiles];
}

- (void)deleteAutosaveFiles {
    if (self.autosaveFileGUI == nil || self.autosaveFileTerp == nil)
        return;
    [self deleteFiles:@[ [NSURL fileURLWithPath:self.autosaveFileGUI isDirectory:NO],
                         [NSURL fileURLWithPath:self.autosaveFileTerp isDirectory:NO],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave.glksave"] isDirectory:NO],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-bak.glksave"]  isDirectory:NO],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-bak.plist"] isDirectory:NO],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-tmp.glksave"]  isDirectory:NO],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-GUI.plist"] isDirectory:NO],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-GUI-late.plist"]  isDirectory:NO],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-tmp.plist"]  isDirectory:NO]]];
}

- (void)deleteFiles:(NSArray<NSURL *> *)urls {
    NSError *error;
    for (NSURL *url in urls) {
        error = nil;
        [[NSFileManager defaultManager] removeItemAtURL:url error:&error];
        //        if (error)
        //            NSLog(@"Error: %@", error);
    }
}

- (void)autoSaveOnExit {
    if (_supportsAutorestore && _theme.autosave && self.appSupportDir.length) {
        NSString *autosaveLate = [self.appSupportDir
                                  stringByAppendingPathComponent:@"autosave-GUI-late.plist"];

        NSError *error = nil;

        NSData *data = [NSKeyedArchiver archivedDataWithRootObject:self requiringSecureCoding:NO error:&error];
        [data writeToFile:autosaveLate options:NSDataWritingAtomic error:&error];

        if (error) {
            NSLog(@"autoSaveOnExit: Write returned error: %@", [error localizedDescription]);
            return;
        }

        _game.autosaved = !dead;
    } else {
        [self deleteAutosaveFiles];
        _game.autosaved = NO;
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

        _autosaveVersion = [decoder decodeIntegerForKey:@"version"];
        _hasAutoSaved = [decoder decodeBoolForKey:@"hasAutoSaved"];
        _autosaveTag =  [decoder decodeIntegerForKey:@"autosaveTag"];

        _gwindows = [decoder decodeObjectOfClass:[NSMutableDictionary class] forKey:@"gwindows"];

        _soundHandler = [decoder decodeObjectOfClass:[SoundHandler class] forKey:@"soundHandler"];
        _imageHandler = [decoder decodeObjectOfClass:[ImageHandler class] forKey:@"imageHandler"];

        _storedWindowFrame = [decoder decodeRectForKey:@"windowFrame"];
        _windowPreFullscreenFrame =
        [decoder decodeRectForKey:@"windowPreFullscreenFrame"];

        _storedGameViewFrame = [decoder decodeRectForKey:@"contentFrame"];

        _bgcolor = [decoder decodeObjectOfClass:[NSColor class] forKey:@"backgroundColor"];

        _bufferStyleHints = [decoder decodeObjectOfClass:[NSMutableArray class] forKey:@"bufferStyleHints"];
        _gridStyleHints = [decoder decodeObjectOfClass:[NSMutableArray class] forKey:@"gridStyleHints"];

        _queue = [decoder decodeObjectOfClass:[NSMutableArray class] forKey:@"queue"];

        _firstResponderView = [decoder decodeIntegerForKey:@"firstResponder"];
        _inFullscreen = [decoder decodeBoolForKey:@"fullscreen"];

        _turns = [decoder decodeIntegerForKey:@"turns"];
        _numberOfPrintsAndClears = [decoder decodeIntegerForKey:@"printsAndClears"];
        _printsAndClearsThisTurn = [decoder decodeIntegerForKey:@"printsAndClearsThisTurn"];

        _oldThemeName = [decoder decodeObjectOfClass:[NSString class] forKey:@"oldThemeName"];

        _showingCoverImage = [decoder decodeBoolForKey:@"showingCoverImage"];

        _commandScriptRunning = [decoder decodeBoolForKey:@"commandScriptRunning"];
        if (_commandScriptRunning)
            _commandScriptHandler = [decoder decodeObjectOfClass:[CommandScriptHandler class] forKey:@"commandScriptHandler"];

        _journeyMenuHandler = [decoder decodeObjectOfClass:[JourneyMenuHandler class] forKey:@"journeyMenuHandler"];
        if (_journeyMenuHandler)
            _journeyMenuHandler.delegate = self;
        restoredController = nil;
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];

    [encoder encodeInteger:AUTOSAVE_SERIAL_VERSION forKey:@"version"];
    [encoder encodeBool:_hasAutoSaved forKey:@"hasAutoSaved"];
    [encoder encodeInteger:_autosaveTag forKey:@"autosaveTag"];

    [encoder encodeBool:dead forKey:@"dead"];
    [encoder encodeRect:self.window.frame forKey:@"windowFrame"];
    [encoder encodeRect:_gameView.frame forKey:@"contentFrame"];

    [encoder encodeObject:_bgcolor forKey:@"backgroundColor"];

    [encoder encodeObject:_bufferStyleHints forKey:@"bufferStyleHints"];
    [encoder encodeObject:_gridStyleHints forKey:@"gridStyleHints"];

    [encoder encodeObject:_gwindows forKey:@"gwindows"];
    [encoder encodeObject:_soundHandler forKey:@"soundHandler"];
    [encoder encodeObject:_imageHandler forKey:@"imageHandler"];
    [encoder encodeObject:_journeyMenuHandler forKey:@"journeyMenuHandler"];

    [encoder encodeRect:_windowPreFullscreenFrame
                 forKey:@"windowPreFullscreenFrame"];

    [encoder encodeObject:_queue forKey:@"queue"];
    _firstResponderView = -1;

    NSResponder *firstResponder = self.window.firstResponder;

    if ([firstResponder isKindOfClass:[GlkWindow class]]) {
        _firstResponderView = ((GlkWindow *)firstResponder).name;
    } else {
        id delegate = nil;
        if ([firstResponder isKindOfClass:[NSTextView class]]) {
            delegate = ((NSTextView *)firstResponder).delegate;
            if (![delegate isKindOfClass:[GlkWindow class]]) {
                delegate = nil;
            }
        }
        if (delegate) {
            _firstResponderView = ((GlkWindow *)delegate).name;
        }
    }
    [encoder encodeInteger:_firstResponderView forKey:@"firstResponder"];
    [encoder encodeBool:((self.window.styleMask & NSWindowStyleMaskFullScreen) ==
                         NSWindowStyleMaskFullScreen)
                 forKey:@"fullscreen"];

    [encoder encodeInteger:_turns forKey:@"turns"];
    [encoder encodeInteger:_numberOfPrintsAndClears forKey:@"printsAndClears"];
    [encoder encodeInteger:_printsAndClearsThisTurn forKey:@"printsAndClearsThisTurn"];

    [encoder encodeObject:_theme.name forKey:@"oldThemeName"];

    [encoder encodeBool:_showingCoverImage forKey:@"showingCoverImage"];

    [encoder encodeBool:_commandScriptRunning forKey:@"commandScriptRunning"];
    if (_commandScriptRunning)
        [encoder encodeObject:_commandScriptHandler forKey:@"commandScriptHandler"];
}

- (void)showAutorestoreAlert:(id)userInfo {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *alertSuppressionKey = @"AutorestoreAlertSuppression";
    NSString *alwaysAutorestoreKey = @"AlwaysAutorestore";

    if ([defaults boolForKey:alertSuppressionKey] == YES)
        _shouldShowAutorestoreAlert = NO;

    if (!_shouldShowAutorestoreAlert) {
        _mustBeQuiet = NO;
        [_journeyMenuHandler recreateDialog];
        return;
    }

    if (dead)
        return;

    _mustBeQuiet = YES;

    NSAlert *anAlert = [[NSAlert alloc] init];
    anAlert.messageText =
    NSLocalizedString(@"This game was automatically restored from a previous session.", nil);
    anAlert.informativeText = NSLocalizedString(@"Would you like to start over instead?", nil);
    anAlert.showsSuppressionButton = YES;
    anAlert.suppressionButton.title = NSLocalizedString(@"Remember this choice.", nil);
    [anAlert addButtonWithTitle:NSLocalizedString(@"Continue", nil)];
    [anAlert addButtonWithTitle:NSLocalizedString(@"Restart", nil)];

    GlkController * __weak weakSelf = self;

    [anAlert beginSheetModalForWindow:self.window completionHandler:^(NSInteger result) {

        weakSelf.mustBeQuiet = NO;

        if (anAlert.suppressionButton.state == NSOnState) {
            // Suppress this alert from now on
            [defaults setBool:YES forKey:alertSuppressionKey];
        }

        weakSelf.shouldShowAutorestoreAlert = NO;

        if (result == NSAlertSecondButtonReturn) {
            [self reset:nil];
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

        [self forceSpeech];
    }];
}

- (IBAction)reset:(id)sender {
    if (_lastResetTimestamp && _lastResetTimestamp.timeIntervalSinceNow < -1) {
        restartingAlready = NO;
    }

    if (restartingAlready || _showingCoverImage)
        return;

    restartingAlready = YES;
    _lastResetTimestamp = [NSDate date];
    _mustBeQuiet = YES;

    [[NSNotificationCenter defaultCenter]
     removeObserver:self name: NSFileHandleDataAvailableNotification
     object: readfh];

    _commandScriptHandler = nil;
    _journeyMenuHandler = nil;

    if (task) {
        // Stop the interpreter
        task.terminationHandler = nil;
        [task.standardOutput fileHandleForReading].readabilityHandler = nil;
        readfh = nil;
        [task terminate];
        task = nil;
    }

    [self performSelector:@selector(deferredRestart:) withObject:nil afterDelay:0.3];
}

- (void)deferredRestart:(id)sender {
    [self cleanup];
    [self runTerp:(NSString *)_terpname
         withGame:(Game *)_game
            reset:YES
       restorationHandler:nil];

    [self.window makeKeyAndOrderFront:nil];
    [self.window makeFirstResponder:nil];
    [self guessFocus];
    NSAccessibilityPostNotification(self.window.firstResponder, NSAccessibilityFocusedUIElementChangedNotification);
}

-(void)cleanup {
    [_soundHandler stopAllAndCleanUp];
    [self deleteAutosaveFiles];
    [self handleSetTimer:0];

    for (GlkWindow *win in _gwindows.allValues) {
        win.glkctl = nil;
        if ([win isKindOfClass:[GlkTextBufferWindow class]])
            ((GlkTextBufferWindow *)win).quoteBox = nil;
        [win removeFromSuperview];
    }

    for (GlkTextGridWindow *win in _quoteBoxes) {
        win.glkctl = nil;
        win.quoteboxParent = nil;
        [win removeFromSuperview];
    }

    if (_form) {
        _form.glkctl = nil;
        _form = nil;
    }

    if (_zmenu) {
        _zmenu.glkctl = nil;
        _zmenu = nil;
    }

    readfh = nil;
    sendfh = nil;
    task = nil;
}

- (void)handleAutosave:(NSInteger)hash {
    _autosaveTag = hash;

    @autoreleasepool {
        NSError *error = nil;
        NSData *data = [NSKeyedArchiver archivedDataWithRootObject:self requiringSecureCoding:NO error:&error];
        [data writeToFile:self.autosaveFileGUI options:NSDataWritingAtomic error:&error];
        if (error) {
            NSLog(@"handleAutosave: Write returned error: %@", [error localizedDescription]);
            return;
        }
    }

    _hasAutoSaved = YES;
}

/*
 *
 */

#pragma mark Cocoa glue

- (BOOL)isAlive {
    return !dead;
}

- (void)windowDidBecomeKey:(NSNotification *)notification {
    [Preferences changeCurrentGlkController:self];

    // dead is YES before the interpreter process has started
    if (!dead) {
        [self guessFocus];
        if (_eventcount > 1 && !_shouldShowAutorestoreAlert) {
            _mustBeQuiet = NO;
        }

        if (_journeyMenuHandler && [_journeyMenuHandler updateOnBecameKey:!_shouldShowAutorestoreAlert || _turns > 1]) {
            return;
        }

        [self speakOnBecomingKey];
    } else if (_theme.vODelayOn) { // If the game has ended
        [self speakMostRecentAfterDelay];
    }
}

- (void)windowDidResignKey:(NSNotification *)notification {
    _mustBeQuiet = YES;
    [_journeyMenuHandler captureMembersMenu];
    [_journeyMenuHandler hideJourneyMenus];
}

- (BOOL)windowShouldClose:(id)sender {
    NSAlert *alert;

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

- (void)terminateTask {
    if (task) {
        // stop the interpreter
        [task setTerminationHandler:nil];
        [task.standardOutput fileHandleForReading].readabilityHandler = nil;
        readfh = nil;
        [task terminate];
    }
}


- (void)windowWillClose:(id)sender {
    if (windowClosedAlready) {
        NSLog(@"windowWillClose called twice! Returning");
        return;
    } else windowClosedAlready = YES;

    // Make sure any new Glk events are queued
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

- (void)flushDisplay {
    [Preferences instance].inMagnification = NO;

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

    for (GlkWindow *win in _gwindows.allValues) {
        [win flushDisplay];
        if (![win isKindOfClass:[GlkGraphicsWindow class]] &&
            (!_voiceOverActive || _mustBeQuiet)) {
            [win setLastMove];
        }
    }

    for (GlkWindow *win in _windowsToBeRemoved) {
        [win removeFromSuperview];
        win.glkctl = nil;
    }

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


- (void)guessFocus {
    if (_gameID == kGameIsJourney && _voiceOverActive) {
        for (GlkWindow *win in _gwindows.allValues) {
            if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
                [win grabFocus];
                return;
            }
        }
    }

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

    if (focuswin && [focuswin wantsFocus])
        return;

    // Guessing new window to focus on
    for (GlkWindow *win in _gwindows.allValues) {
        if (win.wantsFocus) {
            [win grabFocus];
            return;
        }
    }
}

- (void)markLastSeen {
    for (GlkWindow *win in _gwindows.allValues) {
        if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
            [win markLastSeen];
        }
    }
}

- (void)performScroll {
    for (GlkWindow *win in _gwindows.allValues)
        if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
            [win performScroll];
        }
}

- (id)windowWillReturnFieldEditor:(NSWindow *)sender
                         toObject:(id)client {
    for (GlkWindow *win in _gwindows.allValues)
        if (win.input == client) {
            return win.input.fieldEditor;
        }
    return nil;
}

#pragma mark Narcolepsy window mask

- (void)adjustMaskLayer:(id)sender {
    CALayer *maskLayer = nil;
    if (!_gameView.layer.mask) {
        _gameView.layer.mask = [self createMaskLayer];
        maskLayer = _gameView.layer.mask;
        maskLayer.layoutManager = [CAConstraintLayoutManager layoutManager];
        maskLayer.autoresizingMask = kCALayerHeightSizable | kCALayerWidthSizable;
        self.window.opaque = NO;
        self.window.backgroundColor = [NSColor clearColor];
    }

    if (maskLayer) {
        maskLayer.frame = _gameView.bounds;
    }
}

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

- (NSRect)frameWithSanitycheckedSize:(NSRect)rect {
    if (rect.size.width < kMinimumWindowWidth || rect.size.height < kMinimumWindowHeight) {
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
    if (rect.size.width < kMinimumWindowWidth)
        rect.size.width = kMinimumWindowWidth;
    if (rect.size.height < kMinimumWindowHeight)
        rect.size.height = kMinimumWindowHeight;
    return rect;
}

- (NSSize)defaultContentSize {
    // Actually the size of the content view, not including window title bar
    NSSize size;
    Theme *theme = _theme;
    size.width = round(theme.cellWidth * theme.defaultCols + (theme.gridMarginX + theme.border + 5.0) * 2.0);
    size.height = round(theme.cellHeight * theme.defaultRows + (theme.gridMarginY + theme.border) * 2.0);
    return size;
}

- (void)contentDidResize:(NSRect)frame {
    if (NSEqualRects(frame, lastContentResize)) {
        // contentDidResize called with same frame as last time. Skipping.
        return;
    }

    lastContentResize = frame;
    lastSizeInChars = [self contentSizeToCharCells:_gameView.frame.size];

    if (frame.origin.x < 0 || frame.origin.y < 0 || frame.size.width < 0 || frame.size.height < 0) {
        // Negative height happens during the fullscreen animation. We just ignore it
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

- (void)zoomContentToSize:(NSSize)newSize {
    _ignoreResizes = YES;
    NSRect oldframe = _gameView.frame;

    NSUInteger borders = (NSUInteger)_theme.border * 2;

    if ((self.window.styleMask & NSWindowStyleMaskFullScreen) !=
        NSWindowStyleMaskFullScreen) { // We are not in fullscreen

        newSize.width += borders;
        newSize.height += borders;

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

        if (NSWidth(newframe) > NSWidth(_borderView.frame) - borders)
            newframe.size.width = NSWidth(_borderView.frame) - borders;

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


- (NSSize)charCellsToContentSize:(NSSize)cells {
    // Only _contentView, does not take border into account
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

- (NSSize)contentSizeToCharCells:(NSSize)points {
    // Only _contentView, does not take border into account
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

- (void)notePreferencesChanged:(NSNotification *)notify {

    if (_movingBorder) {
        return;
    }
    Theme *theme = _theme;

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
        // No game
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
            NSUInteger borders = (NSUInteger)theme.border * 2;
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
        win.theme = theme;
        [win prefsDidChange];
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
        NSLog(@"noteDefaultSizeChanged: New default size: %@", NSStringFromSize(sizeAfterZoom));
    }
    NSRect oldframe = _gameView.frame;

    // Prevent the window from shrinking when zooming in or growing when
    // zooming out, which might otherwise happen at edge cases
    if ((sizeAfterZoom.width < oldframe.size.width && Preferences.zoomDirection == ZOOMIN) ||
        (sizeAfterZoom.width > oldframe.size.width && Preferences.zoomDirection == ZOOMOUT)) {
        NSLog(@"noteDefaultSizeChanged: This would change the size in the wrong direction, so skip");
        return;
    }

    NSLog(@"noteDefaultSizeChanged: Old contentView size: %@", NSStringFromSize(_gameView.frame.size));

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
        NSLog(@"noteDefaultSizeChanged: New contentView size: %@", NSStringFromSize(_gameView.frame.size));

    } else {
        NSUInteger borders = (NSUInteger)_theme.border * 2;
        NSRect newframe = NSMakeRect(oldframe.origin.x, oldframe.origin.y,
                                     sizeAfterZoom.width - borders,
                                     NSHeight(_borderView.frame) - borders);

        if (NSWidth(newframe) > NSWidth(_borderView.frame) - borders)
            newframe.size.width = NSWidth(_borderView.frame) - borders;

        newframe.origin.x += (NSWidth(oldframe) - NSWidth(newframe)) / 2;

        CGFloat offset = NSHeight(newframe) - NSHeight(oldframe);
        newframe.origin.y -= offset;

        _gameView.frame = newframe;
        NSLog(@"noteDefaultSizeChanged: New contentView size: %@", NSStringFromSize(_gameView.frame.size));
    }
}


/*
 *
 */

#pragma mark Command script

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

- (IBAction)cancel:(id)sender
{
    _commandScriptRunning = NO;
    _commandScriptHandler = nil;
}

#pragma mark Dragging and dropping

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


#pragma mark Glk requests

- (void)handleOpenPrompt:(int)fileusage {
    if (_pendingSaveFilePath) {
        [self feedSaveFileToPrompt];
        _pendingSaveFilePath = nil;
        return;
    }

    _commandScriptHandler = nil;
    NSURL *directory =
    [NSURL fileURLWithPath:[[NSUserDefaults standardUserDefaults]
                            objectForKey:@"SaveDirectory"]
               isDirectory:YES];

    NSInteger sendfd = sendfh.fileDescriptor;

    // Create and configure the panel.
    NSOpenPanel *panel = [NSOpenPanel openPanel];

    _mustBeQuiet = YES;

    waitforfilename = YES; /* don't interrupt */

    if (fileusage == fileusage_SavedGame)
        panel.prompt = NSLocalizedString(@"Restore", nil);
    panel.directoryURL = directory;

    // Display the panel attached to the document's window.
    [panel beginSheetModalForWindow:self.window
                  completionHandler:^(NSInteger result) {
        const char *s;
        struct message reply;

        if (result == NSModalResponseOK) {
            NSURL *theDoc = (panel.URLs)[0];

            [[NSUserDefaults standardUserDefaults]
             setObject:theDoc.path
                .stringByDeletingLastPathComponent
             forKey:@"SaveDirectory"];
            s = (theDoc.path).fileSystemRepresentation;
        } else
            s = "";

        reply.cmd = OKAY;
        reply.len = strlen(s);

        write((int)sendfd, &reply, sizeof(struct message));
        if (reply.len)
            write((int)sendfd, s, reply.len);
        self.mustBeQuiet = NO;
        // Needed to prevent Journey dialog from popping up after closing file dialog
        if (self.gameID == kGameIsJourney)
            self.journeyMenuHandler.skipNextDialog = YES;
    }];

    waitforfilename = NO; /* we're all done, resume normal processing */

    [readfh waitForDataInBackgroundAndNotify];
}

- (void)feedSaveFileToPrompt {
    NSInteger sendfd = sendfh.fileDescriptor;
    [[NSUserDefaults standardUserDefaults]
     setObject:_pendingSaveFilePath
        .stringByDeletingLastPathComponent
     forKey:@"SaveDirectory"];
    const char *s = _pendingSaveFilePath.fileSystemRepresentation;
    struct message reply;
    reply.cmd = OKAY;
    reply.len = strlen(s);
    write((int)sendfd, &reply, sizeof(struct message));
    if (reply.len)
        write((int)sendfd,s, reply.len);
    [readfh waitForDataInBackgroundAndNotify];
}

- (void)handleSavePrompt:(int)fileusage {
    _commandScriptRunning = NO;
    _commandScriptHandler = nil;
    NSURL *directory;
    NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;
    if ([defaults boolForKey:@"SaveInGameDirectory"]) {
        if (!_saveDir) {
            directory = [[_game urlForBookmark] URLByDeletingLastPathComponent];
        } else {
            directory = _saveDir;
        }
    } else {
        directory = [NSURL fileURLWithPath:
                     [defaults objectForKey:@"SaveDirectory"]
                               isDirectory:YES];
    }

    NSSavePanel *panel = [NSSavePanel savePanel];
    NSString *prompt;
    NSString *ext;
    NSString *filename;
    NSString *date;

    waitforfilename = YES; /* don't interrupt */

    switch (fileusage) {
        case fileusage_Data:
            prompt = @"Save data file: ";
            ext = @"glkdata";
            filename = @"Data";
            break;
        case fileusage_SavedGame:
            prompt = @"Save game: ";
            ext = @"glksave";
            break;
        case fileusage_Transcript:
            prompt = @"Save transcript: ";
            ext = @"txt";
            filename = @"Transcript of ";
            break;
        case fileusage_InputRecord:
            prompt = @"Save recording: ";
            ext = @"rec";
            filename = @"Recording of ";
            break;
        default:
            prompt = @"Save: ";
            ext = nil;
            break;
    }

    panel.nameFieldLabel = NSLocalizedString(prompt, nil);
    if (ext)
        panel.allowedFileTypes = @[ ext ];
    panel.directoryURL = directory;

    panel.extensionHidden = NO;
    panel.canCreateDirectories = YES;

    if (fileusage == fileusage_Transcript || fileusage == fileusage_InputRecord)
        filename =
        [filename stringByAppendingString:_game.metadata.title];

    if (fileusage == fileusage_SavedGame) {
        NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
        formatter.dateFormat = @" yyyy-MM-dd HH.mm";
        date = [formatter stringFromDate:[NSDate date]];

        filename =
        [_game.metadata.title stringByAppendingString:date];
    }

    if (ext)
        filename = [filename stringByAppendingPathExtension:ext];

    if (filename)
        panel.nameFieldStringValue = filename;

    NSInteger sendfd = sendfh.fileDescriptor;

    _mustBeQuiet = YES;

    [panel beginSheetModalForWindow:self.window
                  completionHandler:^(NSInteger result) {
        struct message reply;
        const char *s;

        if (result == NSModalResponseOK) {
            NSURL *theFile = panel.URL;
            if ([defaults boolForKey:@"SaveInGameDirectory"]) {
                self.saveDir = theFile.URLByDeletingLastPathComponent;
            }
            [defaults
             setObject:theFile.path
                .stringByDeletingLastPathComponent
             forKey:@"SaveDirectory"];
            s = (theFile.path).fileSystemRepresentation;
            reply.len = strlen(s);
        } else {
            s = nil;
            reply.len = 0;
        }

        reply.cmd = OKAY;

        write((int)sendfd, &reply, sizeof(struct message));
        if (reply.len)
            write((int)sendfd, s, reply.len);
        self.mustBeQuiet = NO;
    }];

    waitforfilename = NO; /* we're all done, resume normal processing */

    [readfh waitForDataInBackgroundAndNotify];
}

- (NSInteger)handleNewWindowOfType:(NSInteger)wintype andName:(NSInteger)name {
    NSInteger i;

    if (_theme == nil) {
        NSLog(@"Theme nil?");
        _theme = [Preferences currentTheme];
    }

    for (i = 0; i < MAXWIN; i++)
        if (_gwindows[@(i)] == nil)
            break;

    if (i == MAXWIN)
        return -1;

    // If we are autorestoring, the window the interpreter process is asking us
    // to create may already exist. The we just return without doing anything.
    if (i != name) {
        NSLog(@"GlkController handleNewWindowOfType: Was asked to create a new "
              @"window with id %ld, but that already exists. First unused id "
              @"is %lu.",
              (long)name, (unsigned long)i);
        return name;
    }

    GlkWindow *win;

    switch (wintype) {
        case wintype_TextGrid:
            win = [[GlkTextGridWindow alloc] initWithGlkController:self name:i];
            _gwindows[@(i)] = win;
            [_windowsToBeAdded addObject:win];
            return i;

        case wintype_TextBuffer:
            win = [[GlkTextBufferWindow alloc] initWithGlkController:self name:i];

            _gwindows[@(i)] = win;
            [_windowsToBeAdded addObject:win];
            return i;

        case wintype_Graphics:
            win = [[GlkGraphicsWindow alloc] initWithGlkController:self name:i];
            _gwindows[@(i)] = win;
            [_windowsToBeAdded addObject:win];
            return i;
    }

    return -1;
}

- (void)handleSetTimer:(NSUInteger)millisecs {
    if (timer) {
        [timer invalidate];
        timer = nil;
    }

    NSUInteger minTimer = (NSUInteger)_theme.minTimer;

    if (millisecs > 0) {
        if (millisecs < minTimer) {
            //            NSLog(@"glkctl: too small timer interval (%ld); increasing to %lu",
            //                  (unsigned long)millisecs, (unsigned long)minTimer);
            millisecs = minTimer;
        }
        if (_gameID == kGameIsKerkerkruip && millisecs == 10) {
            [timer invalidate];
            NSLog(@"Kerkerkruip tried to start a 10 millisecond timer.");
            return;
        }

        timer =
        [NSTimer scheduledTimerWithTimeInterval:millisecs / 1000.0
                                         target:self
                                       selector:@selector(noteTimerTick:)
                                       userInfo:0
                                        repeats:YES];
    }
}

- (void)noteTimerTick:(id)sender {
    if (waitforevent) {
        GlkEvent *gevent = [[GlkEvent alloc] initTimerEvent];
        [self queueEvent:gevent];
    }
}

- (void)restartTimer:(id)sender {
    [self handleSetTimer:(NSUInteger)(_storedTimerInterval * 1000)];
}

- (void)handleStyleHintOnWindowType:(int)wintype
                              style:(NSUInteger)style
                               hint:(NSUInteger)hint
                              value:(int)value {
    if (style >= style_NUMSTYLES)
        return;

    if (hint >= stylehint_NUMHINTS)
        return;

    NSMutableArray *bufferHintsForStyle = _bufferStyleHints[style];
    NSMutableArray *gridHintsForStyle = _gridStyleHints[style];

    switch (wintype) {
        case wintype_AllTypes:
            gridHintsForStyle[hint] = @(value);
            bufferHintsForStyle[hint] = @(value);
            break;
        case wintype_TextGrid:
            gridHintsForStyle[hint] = @(value);
            break;
        case wintype_TextBuffer:
            bufferHintsForStyle[hint] = @(value);
            break;
        default:
            NSLog(@"styleHintOnWindowType for unhandled wintype!");
            break;
    }
}

- (BOOL)handleStyleMeasureOnWin:(GlkWindow *)gwindow
                          style:(NSUInteger)style
                           hint:(NSUInteger)hint
                         result:(NSInteger *)result {

    if (hint == stylehint_TextColor || hint == stylehint_BackColor) {
        NSMutableDictionary *attributes = [gwindow getCurrentAttributesForStyle:style];
        NSColor *color = nil;
        if (hint == stylehint_TextColor) {
            color = attributes[NSForegroundColorAttributeName];
        }
        if (hint == stylehint_BackColor) {
            color = attributes[NSBackgroundColorAttributeName];
            if (!color) {
                color = [gwindow isKindOfClass:[GlkTextBufferWindow class]] ? _theme.bufferBackground : _theme.gridBackground;
            }
        }

        if (color) {
            *result = color.integerColor;
            return YES;
        }
    }

    if ([gwindow getStyleVal:style hint:hint value:result])
        return YES;

    return NO;
}

- (void)handleClearHintOnWindowType:(int)wintype
                              style:(NSUInteger)style
                               hint:(NSUInteger)hint {

    if (style >= style_NUMSTYLES)
        return;

    NSMutableArray *gridHintsForStyle = _gridStyleHints[style];
    NSMutableArray *bufferHintsForStyle = _bufferStyleHints[style];

    switch (wintype) {
        case wintype_AllTypes:
            gridHintsForStyle[hint] = [NSNull null];
            bufferHintsForStyle[hint] = [NSNull null];
            break;
        case wintype_TextGrid:
            gridHintsForStyle[hint] = [NSNull null];
            break;
        case wintype_TextBuffer:
            bufferHintsForStyle[hint] = [NSNull null];
            break;
        default:
            NSLog(@"clearHintOnWindowType for unhandled wintype!");
            break;
    }

}

- (void)handlePrintOnWindow:(GlkWindow *)gwindow
                      style:(NSUInteger)style
                     buffer:(char *)rawbuf
                     length:(size_t)len {
    NSString *str;
    size_t bytesize = sizeof(unichar) * len;
    unichar *buf = malloc(bytesize);
    if (buf == NULL) {
        NSLog(@"Out of memory!");
        return;
    }

    memcpy(buf, rawbuf, bytesize);
    NSNumber *styleHintProportional = _bufferStyleHints[style][stylehint_Proportional];
    BOOL proportional = ([gwindow isKindOfClass:[GlkTextBufferWindow class]] &&
                         (style & 0xff) != style_Preformatted &&
                         style != style_BlockQuote &&
                         ([styleHintProportional isEqualTo:[NSNull null]] ||
                          styleHintProportional.integerValue == 1));

    if (proportional) {
        GlkTextBufferWindow *textwin = (GlkTextBufferWindow *)gwindow;
        BOOL smartquotes = _theme.smartQuotes;
        NSUInteger spaceformat = (NSUInteger)_theme.spaceFormat;
        NSInteger lastchar = textwin.lastchar;
        NSInteger spaced = 0;
        NSUInteger i;

        for (i = 0; i < len; i++) {
            /* turn (punct sp sp) into (punct sp) */
            if (spaceformat) {
                if (buf[i] == '.' || buf[i] == '!' || buf[i] == '?')
                    spaced = 1;
                else if (buf[i] == ' ' && spaced == 1)
                    spaced = 2;
                else if (buf[i] == ' ' && spaced == 2) {
                    memmove(buf + i, buf + i + 1,
                            (len - (i + 1)) * sizeof(unichar));
                    len--;
                    i--;
                    spaced = 0;
                } else {
                    spaced = 0;
                }
            }

            if (smartquotes && buf[i] == '`')
                buf[i] = 0x2018;

            else if (smartquotes && buf[i] == '\'') {
                if (lastchar == ' ' || lastchar == '\n')
                    buf[i] = 0x2018;
                else
                    buf[i] = 0x2019;
            }

            else if (smartquotes && buf[i] == '"') {
                if (lastchar == ' ' || lastchar == '\n')
                    buf[i] = 0x201c;
                else
                    buf[i] = 0x201d;
            }

            else if (smartquotes && i > 1 && buf[i - 1] == '-' &&
                     buf[i] == '-') {
                memmove(buf + i, buf + i + 1,
                        (len - (i + 1)) * sizeof(unichar));
                len--;
                i--;
                buf[i] = 0x2013;
            }

            else if (smartquotes && i > 1 && buf[i - 1] == 0x2013 &&
                     buf[i] == '-') {
                memmove(buf + i, buf + i + 1,
                        (len - (i + 1)) * sizeof(unichar));
                len--;
                i--;
                buf[i] = 0x2014;
            }

            lastchar = buf[i];
        }

        len = (size_t)i;
    }

    str = [NSString stringWithCharacters:buf length:len];
    [gwindow putString:str style:style];
    free(buf);
}

- (void)handleSetTerminatorsOnWindow:(GlkWindow *)gwindow
                              buffer:(glui32 *)buf
                              length:(glui32)len {
    NSMutableDictionary *myDict = gwindow.pendingTerminators;
    NSNumber *key;
    NSArray *keys = myDict.allKeys;

    for (key in keys) {
        myDict[key] = @(NO);
    }

    for (NSInteger i = 0; i < len; i++) {
        key = @(buf[i]);

        // Convert input terminator keys for Beyond Zork arrow keys hack
        if ((_gameID == kGameIsBeyondZork || [self zVersion6]) && _theme.bZTerminator == kBZArrowsSwapped && [gwindow isKindOfClass:[GlkTextBufferWindow class]]) {
            if (buf[i] == keycode_Up) {
                key = @(keycode_Home);
            } else if (buf[i] == keycode_Down) {
                key = @(keycode_End);
            }
        }

        id terminator_setting = myDict[key];
        if (terminator_setting) {
            myDict[key] = @(YES);
        } else {
            // Convert input terminator keys for Beyond Zork arrow keys hack
            if (_gameID == kGameIsBeyondZork || [self zVersion6]) {
                if (_theme.bZTerminator != kBZArrowsOriginal) {
                    if (buf[i] == keycode_Left) {
                        myDict[@"storedLeft"] = @(YES);
                    } else if (buf[i] == keycode_Right) {
                        myDict[@"storedRight"] = @(YES);
                    } else {
                        NSLog(@"Illegal line terminator request: %x", buf[i]);
                    }
                } else {
                    NSLog(@"Illegal line terminator request: %x", buf[i]);
                }
            } else {
                NSLog(@"Illegal line terminator request: %x", buf[i]);
            }
        }
    }
    gwindow.terminatorsPending = YES;
}

- (void)handleChangeTitle:(char *)buf length:(size_t)len {
    buf[len] = '\0';
    NSLog(@"HandleChangeTitle: %s length: %zu", buf, len);
    NSString *str = @(buf);
    if (str && str.length > (NSUInteger)len - 1)
        [@(buf) substringToIndex:(NSUInteger)len - 1];
    if (str == nil || str.length < 2)
        return;
    if ([_game.metadata.title.lowercaseString isEqualToString:
         _gamefile.lastPathComponent.lowercaseString]) {
        self.window.title = str;
        _game.metadata.title = str;
    }
}

- (void)handleShowError:(char *)buf length:(size_t)len {
    buf[len] = '\0';
    NSLog(@"handleShowError: %s length: %zu", buf, len);
    NSString *str = @(buf);
    if (str && str.length > (NSUInteger)len - 1)
        [@(buf) substringToIndex:(NSUInteger)len - 1];
    if (str == nil || str.length < 2)
        return;
    if (_theme.errorHandling == IGNORE_ERRORS) {
        _pendingErrorMessage = str;
        _errorTimeStamp = [NSDate date];
        return;
    }
    _pendingErrorMessage = nil;
    dispatch_async(dispatch_get_main_queue(), ^{
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = NSLocalizedString(@"ERROR", nil);
        alert.informativeText = str;
        [alert runModal];
    });
}

- (NSUInteger)handleUnprintOnWindow:(GlkWindow *)win string:(unichar *)buf length:(size_t)len {
    NSString *str = [NSString stringWithCharacters:buf length:(NSUInteger)len];
    if (str == nil || len == 0)
        return 0;
    return [win unputString:str];
}

- (NSInteger)handleCanPrintGlyph:(glui32)glyph {
    glui32 uniglyph[1];
    uniglyph[0] = glyph;
    NSData *data = [NSData dataWithBytes:uniglyph length:4];
    NSString *str  = [[NSString alloc] initWithData:data
                                           encoding:NSUTF32LittleEndianStringEncoding];
    return [GlkController unicodeAvailableForChar:str];
}

+ (BOOL)isCharacter:(NSString *)character supportedByFont:(NSString *)fontName
{
    if (character.length == 0 || character.length > 2) {
        return NO;
    }
    CTFontRef ctFont = CTFontCreateWithName((CFStringRef)fontName, 8, NULL);
    CGGlyph glyphs[2];
    BOOL ret = NO;
    UniChar characters[2];
    characters[0] = [character characterAtIndex:0];
    if(character.length == 2) {
        characters[1] = [character characterAtIndex:1];
    }
    ret = CTFontGetGlyphsForCharacters(ctFont, characters, glyphs, (CFIndex)character.length);
    CFRelease(ctFont);
    return ret;
}

+ (BOOL)unicodeAvailableForChar:(NSString *)charString {
    NSArray<NSString *> *fontlist = NSFontManager.sharedFontManager.availableFonts;
    for (NSString *fontName in fontlist) {
        if ([GlkController isCharacter:charString supportedByFont:fontName]) {
            return YES;
        }
    }
    return NO;
}

- (nullable JourneyMenuHandler *)journeyMenuHandler {
    if (_journeyMenuHandler == nil) {
        GlkTextGridWindow *gridwindow = nil;
        GlkTextBufferWindow *bufferwindow = nil;
        for (GlkWindow *win in _gwindows.allValues)
            if ([win isKindOfClass:[GlkTextGridWindow class]]) {
                gridwindow = (GlkTextGridWindow *)win;
            } else if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
                bufferwindow = (GlkTextBufferWindow *)win;
            }
        if (gridwindow == nil || bufferwindow == nil)
            return nil;
        _journeyMenuHandler = [[JourneyMenuHandler alloc] initWithDelegate:self gridWindow:gridwindow bufferWindow:bufferwindow];
    }
    return _journeyMenuHandler;
}

- (nullable InfocomV6MenuHandler *)infocomV6MenuHandler {
    if (_infocomV6MenuHandler == nil) {

        _infocomV6MenuHandler = [[InfocomV6MenuHandler alloc] initWithDelegate:self];
    }
    return _infocomV6MenuHandler;
}

- (BOOL)handleRequest:(struct message *)req
                reply:(struct message *)ans
               buffer:(char *)buf {
//    NSLog(@"glkctl: incoming request %s", msgnames[req->cmd]);

    NSInteger result;
    GlkWindow *reqWin = nil;
    NSColor *bg = nil;

    Theme *theme = _theme;

    if (req->a1 >= 0 && req->a1 < MAXWIN && _gwindows[@(req->a1)])
        reqWin = _gwindows[@(req->a1)];

    switch (req->cmd) {
        case HELLO:
            ans->cmd = OKAY;
            ans->a1 = theme.doGraphics;
            ans->a2 = theme.doSound;
            break;

        case NEXTEVENT:
            if (_windowsToRestore.count) {
                for (GlkWindow *win in _windowsToRestore) {
                    if (![win isKindOfClass:[GlkGraphicsWindow class]])
                        [_gwindows[@(win.name)] postRestoreAdjustments:win];
                }
                _windowsToRestore = nil;
            }
            // If this is the first turn, we try to restore the UI
            // from an autosave file.
            if (_eventcount == 2) {
                if (shouldRestoreUI) {
                    [self restoreUI];
                } else {
                    // If we are not autorestoring, try to guess an input window.
                    for (GlkWindow *win in _gwindows.allValues) {
                        if ([win isKindOfClass:[GlkTextBufferWindow class]] && win.wantsFocus) {
                            [win grabFocus];
                        }
                    }
                }
            }

            if (_eventcount > 1 && !_shouldShowAutorestoreAlert) {
                _mustBeQuiet = NO;
            }

            _eventcount++;

            if (_shouldSpeakNewText) {
                _turns++;
            }

            _numberOfPrintsAndClears += _printsAndClearsThisTurn;
            _printsAndClearsThisTurn = 0;
            if (_quoteBoxes.count) {
                if (_quoteBoxes.lastObject.quoteboxAddedOnPAC == 0)
                    _quoteBoxes.lastObject.quoteboxAddedOnPAC = _numberOfPrintsAndClears;
                if (_numberOfPrintsAndClears > _quoteBoxes.lastObject.quoteboxAddedOnPAC ||
                    _quoteBoxes.count > 1) {
                    GlkTextGridWindow *view = _quoteBoxes.firstObject;
                    [_quoteBoxes removeObjectAtIndex:0];
                    ((GlkTextBufferWindow *)view.quoteboxParent.superview).quoteBox = nil;
                    view.quoteboxParent = nil;
                    if (_quoteBoxes.count == 0) {
                        _quoteBoxes = nil;
                    }
                    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
                        context.duration = 1;
                        view.animator.alphaValue = 0;
                    } completionHandler:^{
                        view.hidden = YES;
                        view.alphaValue = 1;
                        [view removeFromSuperview];
                        view.glkctl = nil;
                    }];
                }
            }

            if (_slowReadAlert != nil) {
                [_slowReadAlert.window close];
                _slowReadAlert = nil;
            }

            [self flushDisplay];

            if (_queue.count) {
                GlkEvent *gevent;
                gevent = _queue[0];

                [gevent writeEvent:sendfh.fileDescriptor];
                [_queue removeObjectAtIndex:0];
                lastRequest = req->cmd;
                return NO; /* keep reading ... we sent the reply */
            } else {
                // No queued events.

                if (!req->a1) {
                    // Argument 1 is FALSE. No waiting for more events. Send a dummy
                    // reply to hand over to the interpreter immediately.
                    ans->cmd = OKAY;
                    break;
                }
            }

            [self guessFocus];

            waitforevent = YES;
            lastRequest = req->cmd;
            return YES; /* stop reading ... terp is waiting for reply */

        case PROMPTOPEN:
            [self handleOpenPrompt:req->a1];
            [self flushDisplay];
            return YES; /* stop reading ... terp is waiting for reply */

        case PROMPTSAVE:
            [self handleSavePrompt:req->a1];
            [self flushDisplay];
            return YES; /* stop reading ... terp is waiting for reply */

        case STYLEHINT:
            [self handleStyleHintOnWindowType:req->a1
                                        style:(NSUInteger)req->a2
                                         hint:(NSUInteger)req->a3
                                        value:req->a4];
            break;

        case STYLEMEASURE:
            result = 0;
            ans->cmd = OKAY;
            ans->a1 = [self handleStyleMeasureOnWin:reqWin
                                              style:(NSUInteger)req->a2
                                               hint:(NSUInteger)req->a3
                                             result:&result];
            ans->a2 = (int)result;
            break;

        case CLEARHINT:
            [self handleClearHintOnWindowType:req->a1 style:(NSUInteger)req->a2 hint:(NSUInteger)req->a3];
            break;

            /*
             * Create and destroy windows and channels
             */

#pragma mark Create and destroy windows and sound channels

        case NEWWIN:
            ans->cmd = OKAY;
            ans->a1 = (int)[self handleNewWindowOfType:req->a1 andName:req->a2];
            break;

        case NEWCHAN:
            ans->cmd = OKAY;
            ans->a1 = [_soundHandler handleNewSoundChannel:(glui32)req->a1];
            break;

        case DELWIN:
            if (reqWin) {
                [_windowsToBeRemoved addObject:reqWin];
                [_gwindows removeObjectForKey:@(req->a1)];
                _shouldCheckForMenu = YES;
            } else
                NSLog(@"delwin called on a non-existant Glk window (%d)", req->a1);

            break;

        case DELCHAN:
            [_soundHandler handleDeleteChannel:req->a1];
            break;

            /*
             * Load images; load and play sounds
             */

#pragma mark Load images; load and play sounds

        case FINDIMAGE:
            ans->cmd = OKAY;
            ans->a1 = [_imageHandler handleFindImageNumber:req->a1];
            break;

        case LOADIMAGE:
            buf[req->len] = 0;
            [_imageHandler handleLoadImageNumber:req->a1
                                            from:@(buf)
                                          offset:(NSUInteger)req->a2
                                            size:(NSUInteger)req->a3];
            break;

        case SIZEIMAGE:
        {
            ans->cmd = OKAY;
            ans->a1 = 0;
            ans->a2 = 0;
            NSImage *lastimage = _imageHandler.lastimage;
            if (lastimage) {
                NSSize size;
                size = lastimage.size;
                ans->a1 = (int)size.width;
                ans->a2 = (int)size.height;
            } else {
                NSLog(@"SIZEIMAGE: No last image found!");
            }
        }
            break;

        case FINDSOUND:
            ans->cmd = OKAY;
            ans->a1 = [_soundHandler handleFindSoundNumber:req->a1];
            break;

        case LOADSOUND:
            buf[req->len] = 0;
            [_soundHandler handleLoadSoundNumber:req->a1
                                            from:@(buf)
                                          offset:(NSUInteger)req->a2
                                          length:(NSUInteger)req->a3];
            break;

        case SETVOLUME:
            [_soundHandler handleSetVolume:(glui32)req->a2
                                   channel:req->a1
                                  duration:(glui32)req->a3
                                    notify:(glui32)req->a4];
            break;

        case PLAYSOUND:
            [_soundHandler handlePlaySoundOnChannel:req->a1 repeats:(glsi32)req->a2 notify:(glui32)req->a3];
            break;

        case STOPSOUND:
            [_soundHandler handleStopSoundOnChannel:req->a1];
            break;

        case PAUSE:
            [_soundHandler handlePauseOnChannel:req->a1];
            break;

        case UNPAUSE:
            [_soundHandler handleUnpauseOnChannel:req->a1];
            break;

        case BEEP:
            if (theme.doSound) {
                if (req->a1 == 1 && theme.beepHigh) {
                    NSSound *sound = [NSSound soundNamed:theme.beepHigh];
                    if (sound) {
                        [sound stop];
                        [sound play];
                    }
                }
                if (req->a1 == 2 && theme.beepLow) {
                    NSSound *sound = [NSSound soundNamed:theme.beepLow];
                    if (sound) {
                        [sound stop];
                        [sound play];
                    }
                    // For Bureaucracy form accessibility
                    if (_form)
                        [_form speakError];
                }
            }

            break;
            /*
             * Window sizing, printing, drawing, etc...
             */

#pragma mark Window sizing, printing, drawing …
        case SHOWERROR:
            [self handleShowError:(char *)buf length:req->len];
            break;

        case CANPRINT:
            ans->cmd = OKAY;
            ans->a1 = (int)[self handleCanPrintGlyph:(glui32)req->a1];
            break;

        case SIZWIN:
            if (reqWin) {
                int x0, y0, x1, y1, checksumWidth, checksumHeight;
                NSRect rect;

                struct sizewinrect *sizewin = malloc(sizeof(struct sizewinrect));

                memcpy(sizewin, buf, sizeof(struct sizewinrect));

                checksumWidth = sizewin->gamewidth;
                checksumHeight = sizewin->gameheight;

                if (fabs(checksumWidth - _gameView.frame.size.width) > 1.0) {
                    free(sizewin);
                    break;
                }

                if (fabs(checksumHeight - _gameView.frame.size.height) > 1.0) {
                    free(sizewin);
                    break;
                }

                x0 = sizewin->x0;
                y0 = sizewin->y0;
                x1 = sizewin->x1;
                y1 = sizewin->y1;
                rect = NSMakeRect(x0, y0, x1 - x0, y1 - y0);
                if (rect.size.width < 0)
                    rect.size.width = 0;
                if (rect.size.height < 0)
                    rect.size.height = 0;
                reqWin.frame = rect;

                NSAutoresizingMaskOptions hmask = NSViewMaxXMargin;
                NSAutoresizingMaskOptions vmask = NSViewMaxYMargin;

                if (fabs(NSMaxX(rect) - _gameView.frame.size.width) < 2.0 &&
                    rect.size.width > 0) {
                    // If window is at right edge, attach to that edge
                    hmask = NSViewWidthSizable;
                }

                // We special-case graphics windows, because the fullscreen animation looks
                // bad if they do not stay pinned to the top of the window.
                if (!([reqWin isKindOfClass:[GlkGraphicsWindow class]] && rect.origin.y == 0) &&
                    (fabs(NSMaxY(rect) - _gameView.frame.size.height) < 2.0 &&
                           rect.size.height > 0)) {
                    // Otherwise, if the Glk window is at the bottom,
                    // pin it to the bottom of the main window
                    vmask = NSViewHeightSizable;
                }

                reqWin.autoresizingMask = hmask | vmask;

                windowdirty = YES;
                free(sizewin);
            } else
                NSLog(@"sizwin called on a non-existant Glk window (%d)", req->a1);
            break;

        case CLRWIN:
            _printsAndClearsThisTurn++;
            if (reqWin) {
                [reqWin clear];
                _shouldCheckForMenu = YES;
            }
            break;

        case SETBGND:
            if (req->a2 < 0)
                bg = theme.bufferBackground;
            else
                bg = [NSColor colorFromInteger:req->a2];
            if (req->a1 == -1) {
                _lastAutoBGColor = bg;
                [self setBorderColor:bg];
                changedBorderThisTurn = YES;
            }

            if (reqWin) {
                [reqWin setBgColor:req->a2];
            }
            break;

        case DRAWIMAGE:
            if (reqWin) {
                NSImage *lastimage = _imageHandler.lastimage;
                if (lastimage && lastimage.size.width > 0 && lastimage.size.height > 0) {
                    struct drawrect *drawstruct = malloc(sizeof(struct drawrect));
                    memcpy(drawstruct, buf, sizeof(struct drawrect));
                    [reqWin drawImage:lastimage
                                 val1:(glsi32)drawstruct->x
                                 val2:(glsi32)drawstruct->y
                                width:drawstruct->width
                               height:drawstruct->height
                                style:drawstruct->style];
                    free(drawstruct);
                }
            }
            break;

        case FILLRECT:
            if (reqWin) {
                [reqWin fillRects:(struct fillrect *)buf count:req->a2];
            }
            break;

        case PRINT:
            _printsAndClearsThisTurn++;
            if (!_gwindows.count && shouldRestoreUI) {
                _windowsToRestore = restoredControllerLate.gwindows.allValues;
                [self restoreUI];
                reqWin = _gwindows[@(req->a1)];
            }
            if (reqWin && req->len) {
                [self handlePrintOnWindow:reqWin
                                    style:(NSUInteger)req->a2
                                   buffer:buf
                                   length:req->len / sizeof(unichar)];
            } else {
                NSLog(@"Print to non-existent window!");
            }
            break;

        case UNPRINT:
            if (!_gwindows.count && shouldRestoreUI) {
                _windowsToRestore = restoredControllerLate.gwindows.allValues;
                [self restoreUI];
                reqWin = _gwindows[@(req->a1)];
            }
            ans->cmd = OKAY;
            ans->a1 = 0;
            ans->a2 = 0;
            if (reqWin && req->len) {
                ans->a1 = (int)[self handleUnprintOnWindow:reqWin string:(unichar *)buf length:req->len / sizeof(unichar)];
            }
            break;

        case MOVETO:
            if (reqWin) {
                NSUInteger x = (NSUInteger)req->a2;
                NSUInteger y = (NSUInteger)req->a3;
                [reqWin moveToColumn:x row:y];
            }
            break;

        case SETECHO:
            if (reqWin && [reqWin isKindOfClass:[GlkTextBufferWindow class]])
                [(GlkTextBufferWindow *)reqWin echo:(req->a2 != 0)];
            break;

            /*
             * Request and cancel events
             */

        case TERMINATORS:
            [self handleSetTerminatorsOnWindow:reqWin
                                        buffer:(glui32 *)buf
                                        length:(glui32)req->a2];
            break;

        case FLOWBREAK:
            if (reqWin) {
                [reqWin flowBreak];
            }
            break;

        case SETZCOLOR:
            if (reqWin) {
                [reqWin setZColorText:(glui32)req->a2 background:(glui32)req->a3];
            }
            break;

        case SETREVERSE:
            if (reqWin) {
                reqWin.currentReverseVideo = (req->a2 != 0);
            }
            break;

        case QUOTEBOX:
            if (reqWin) {
                [((GlkTextGridWindow *)reqWin) quotebox:(NSUInteger)req->a2];
            }
            break;

#pragma mark Request and cancel events

        case INITLINE:
            [self performScroll];

            if (!_gwindows.count && shouldRestoreUI) {
                buf = "\0";
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
                _windowsToRestore = restoredControllerLate.gwindows.allValues;
                [self restoreUI];
                reqWin = _gwindows[@(req->a1)];
            }
            if (reqWin && _gameID != kGameIsAColderLight && !skipNextScriptCommand) {
                NSString *preloaded = [NSString stringWithCharacters:(unichar *)buf length:(NSUInteger)req->len / sizeof(unichar)];
                if (!preloaded.length || [preloaded characterAtIndex:0] == '\0')
                    preloaded = @"";
                [reqWin initLine:preloaded maxLength:(NSUInteger)req->a2];

                if (_commandScriptRunning) {
                    [self.commandScriptHandler sendCommandLineToWindow:reqWin];
                }

                _shouldSpeakNewText = YES;

                // Check if we are in Beyond Zork Definitions menu
                if (_gameID == kGameIsBeyondZork)
                    _shouldCheckForMenu = YES;
            }
            skipNextScriptCommand = NO;
            break;

        case CANCELLINE:
            ans->cmd = OKAY;
            if (reqWin) {
                NSString *str = reqWin.cancelLine;
                ans->len = str.length * sizeof(unichar);
                if (ans->len > GLKBUFSIZE)
                    ans->len = GLKBUFSIZE;
                [str getCharacters:(unsigned short *)buf
                             range:NSMakeRange(0, str.length)];
            }
            break;

        case INITCHAR:
            if (!_gwindows.count && shouldRestoreUI) {
                GlkController *g = restoredControllerLate;
                _windowsToRestore = restoredControllerLate.gwindows.allValues;
                if (g.commandScriptRunning) {
                    CommandScriptHandler *handler = restoredController.commandScriptHandler;
                    handler.commandIndex++;
                    if (handler.commandIndex >= handler.commandArray.count) {
                        restoredController.commandScriptHandler = nil;
                        restoredControllerLate.commandScriptHandler = nil;
                    } else {
                        restoredControllerLate.commandScriptHandler = handler;
                        skipNextScriptCommand = YES;
                        lastScriptKeyTimestamp = [NSDate date];
                    }
                    restoredController = restoredControllerLate;
                }
                [self restoreUI];
                reqWin = _gwindows[@(req->a1)];
            }

            // Hack to fix the Level 9 Adrian Mole games.
            // These request and cancel lots of char events every second,
            // which breaks scrolling, as we normally scroll down
            // one screen on every char event.
            if (lastRequest == PRINT ||
                lastRequest == SETZCOLOR ||
                lastRequest == NEXTEVENT ||
                lastRequest == MOVETO ||
                lastKeyTimestamp.timeIntervalSinceNow < -1) {
                // This flag may be set by GlkBufferWindow as well
                _shouldScrollOnCharEvent = YES;
                _shouldSpeakNewText = YES;
                _shouldCheckForMenu = YES;
            }

            lastKeyTimestamp = [NSDate date];

            if (_shouldScrollOnCharEvent) {
                [self performScroll];
            }

            if (reqWin && !skipNextScriptCommand) {
                [reqWin initChar];
                if (_commandScriptRunning) {
                    if (_gameID != kGameIsAdrianMole || lastScriptKeyTimestamp.timeIntervalSinceNow < -0.5) {
                        [self.commandScriptHandler sendCommandKeyPressToWindow:reqWin];
                        lastScriptKeyTimestamp = [NSDate date];
                    }
                }
            }
            skipNextScriptCommand = NO;
            break;

        case CANCELCHAR:
            if (reqWin)
                [reqWin cancelChar];
            break;

        case INITMOUSE:
            if (!_gwindows.count && shouldRestoreUI) {
                _windowsToRestore = restoredControllerLate.gwindows.allValues;
                [self restoreUI];
                reqWin = _gwindows[@(req->a1)];
            }
            [self performScroll];
            if (reqWin) {
                [reqWin initMouse];
                _shouldSpeakNewText = YES;
            }
            break;

        case CANCELMOUSE:
            if (reqWin)
                [reqWin cancelMouse];
            break;

        case SETLINK:
            if (reqWin) {
                reqWin.currentHyperlink = req->a2;
            }
            break;

        case INITLINK:
            if (!_gwindows.count && shouldRestoreUI) {
                _windowsToRestore = restoredControllerLate.gwindows.allValues;
                [self restoreUI];
                reqWin = _gwindows[@(req->a1)];
            }
            [self performScroll];
            if (reqWin) {
                [reqWin initHyperlink];
                _shouldSpeakNewText = YES;
            }
            break;

        case CANCELLINK:
            if (reqWin) {
                [reqWin cancelHyperlink];
            }
            break;

        case TIMER:
            [self handleSetTimer:(NSUInteger)req->a1];
            break;

#pragma mark Non-standard Glk extensions stuff

        case MAKETRANSPARENT:
            if (reqWin)
                [reqWin makeTransparent];
            break;

        case SETTITLE:
            [self handleChangeTitle:(char *)buf length:req->len];
            break;

        case AUTOSAVE:
            [self handleAutosave:req->a2];
            break;

            // This just kills the interpreter process and restarts it from scratch.
            // Used if an autorestore fails.
        case RESET:
            [self reset:nil];
            break;

            // Used by Tads 3 to adapt the banner width.
        case BANNERCOLS:
            ans->cmd = OKAY;
            ans->a1 = 0;
            if (reqWin && [reqWin isKindOfClass:[GlkTextBufferWindow class]] ) {
                GlkTextBufferWindow *banner = (GlkTextBufferWindow *)reqWin;
                ans->a1 = (int)banner.numberOfColumns;
            }
            break;

            // Used by Tads 3 to adapt the banner height.
        case BANNERLINES:
            ans->cmd = OKAY;
            ans->a1 = 0;
            if (reqWin && [reqWin isKindOfClass:[GlkTextBufferWindow class]] ) {
                GlkTextBufferWindow *banner = (GlkTextBufferWindow *)reqWin;
                ans->a1 = (int)banner.numberOfLines;
            }
            break;

        case PURGEIMG:
            if (req->len) {
                buf[req->len] = 0;
                [_imageHandler purgeImage:req->a1
                          withReplacement:@(buf)
                                     size:(NSUInteger)req->a2];
            } else {
                [_imageHandler purgeImage:req->a1
                          withReplacement:nil
                                     size:0];
            }
            break;

        case REFRESH:
//            This updates an existing window on-the-fly with the styles
//            that normally would only be applied to a new window.
//            It can also update any inline images.
            if ([reqWin isKindOfClass:[GlkTextBufferWindow class]]) {
                reqWin.styleHints = [reqWin deepCopyOfStyleHintsArray:_bufferStyleHints];
                if (req->a2 > 0)
                    [((GlkTextBufferWindow *)reqWin) updateImageAttachmentsWithXScale: req->a2 / 1000.0 yScale: req->a3 / 1000.0 ];
            } else if ([reqWin isKindOfClass:[GlkTextGridWindow class]]) {
                reqWin.styleHints = [reqWin deepCopyOfStyleHintsArray:_gridStyleHints];
            } else {
                break;
            }
            [self flushDisplay];
            [reqWin prefsDidChange];
            break;

        case MENUITEM: {
            if (self.gameID == kGameIsJourney) {
                [self.journeyMenuHandler handleMenuItemOfType:(JourneyMenuType)req->a1 column:(NSUInteger)req->a2 line:(NSUInteger)req->a3 stopflag:(req->a4 == 1) text:(char *)buf length:(NSUInteger)req->len];
            } else {
                [self.infocomV6MenuHandler handleMenuItemOfType:(InfocomV6MenuType)req->a1 index:(NSUInteger)req->a2 total:(NSUInteger)req->a3 text:(char *)buf length:(NSUInteger)req->len];
            }
            break;
        }

        default:
            NSLog(@"glkctl: unhandled request (%d)", req->cmd);
            break;
    }

    if (req->cmd != AUTOSAVE)
        lastRequest = req->cmd;
    return NO; /* keep reading */
}

/*
 *
 */

#pragma mark Interpreter glue

static NSString *signalToName(NSTask *task) {
    switch (task.terminationStatus) {
        case 1:
            return @"sighup";
        case 2:
            return @"sigint";
        case 3:
            return @"sigquit";
        case 4:
            return @"sigill";
        case 5:
            return @"sigtrap";
        case 6:
            return @"sigabrt";
        case 8:
            return @"sigfpe";
        case 9:
            return @"sigkill";
        case 10:
            return @"sigbus";
        case 11:
            return @"sigsegv";
        case 13:
            return @"sigpipe";
        case 15:
            return @"sigterm";
        default:
            return [NSString stringWithFormat:@"%d", task.terminationStatus];
    }
}

static BOOL pollMoreData(int fd) {
    fd_set fdset;
    struct timeval tmout = { 0, 0 }; // return immediately
    FD_ZERO(&fdset);
    FD_SET(fd, &fdset);
    return (select(fd + 1, &fdset, NULL, NULL, &tmout) > 0);
}

- (void)noteTaskDidTerminate:(id)sender {
    if (windowClosedAlready)
        return;

    dead = YES;
    restartingAlready = NO;

    if (timer) {
        [timer invalidate];
        timer = nil;
    }

    [self flushDisplay];
    [task waitUntilExit];

    if (task.terminationStatus != 0) {
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = NSLocalizedString(@"The game has unexpectedly terminated.", nil);
        alert.informativeText = [NSString stringWithFormat:NSLocalizedString(@"Error code: %@.", nil), signalToName(task)];
        if (_pendingErrorMessage && _errorTimeStamp.timeIntervalSinceNow > -3)
            alert.informativeText = _pendingErrorMessage;
        _pendingErrorMessage = nil;
        _mustBeQuiet = YES;
        [alert addButtonWithTitle:NSLocalizedString(@"Oops", nil)];
        [alert beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse returnCode) {}];
    } else {
        NotificationBezel *bezel = [[NotificationBezel alloc] initWithScreen:self.window.screen];
        [bezel showGameOver];

        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            NSAccessibilityPostNotificationWithUserInfo(
                                                        NSApp.mainWindow,
                                                        NSAccessibilityAnnouncementRequestedNotification,
                                                        @{NSAccessibilityPriorityKey: @(NSAccessibilityPriorityHigh),
                                                          NSAccessibilityAnnouncementKey: [NSString stringWithFormat:@"%@ has finished.", self.game.metadata.title]
                                                        });
        });
    }

    [_journeyMenuHandler deleteAllJourneyMenus];

    for (GlkWindow *win in _gwindows.allValues)
        [win terpDidStop];

    self.window.title = [self.window.title stringByAppendingString:NSLocalizedString(@" (finished)", nil)];
    [self performScroll];
    task = nil;
    libcontroller.gameTableDirty = YES;
    [libcontroller updateTableViews];

    // We autosave the UI but delete the terp autosave files
    if (!restartingAlready)
        [self autoSaveOnExit];

    [self deleteFiles:@[ [NSURL fileURLWithPath:self.autosaveFileTerp isDirectory:NO],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave.glksave"] isDirectory:NO],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-GUI.plist"] isDirectory:NO],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-tmp.glksave"] isDirectory:NO],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-tmp.plist"] isDirectory:NO] ]];
}

- (void)queueEvent:(GlkEvent *)gevent {
    GlkEvent *redrawEvent = nil;
    if (gevent.type == EVTARRANGE) {
        Theme *theme = _theme;
        NSDictionary *newArrangeValues = @{
            @"width" : @(gevent.val1),
            @"height" : @(gevent.val2),
            @"bufferMargin" : @(theme.bufferMarginX),
            @"gridMargin" : @(theme.gridMarginX),
            @"charWidth" : @(theme.cellWidth),
            @"lineHeight" : @(theme.cellHeight),
            @"leading" : @(((NSParagraphStyle *)(theme.gridNormal.attributeDict)[NSParagraphStyleAttributeName]).lineSpacing)
        };

        if (!gevent.forced && [lastArrangeValues isEqualToDictionary:newArrangeValues]) {
            return;
        }

        lastArrangeValues = newArrangeValues;
        // Some Inform 7 games only resize graphics on evtype_Redraw
        // so we send a redraw event after every resize event
        if (_eventcount > 2)
            redrawEvent = [[GlkEvent alloc] initRedrawEvent];
    }

    if (gevent.type == EVTKEY || gevent.type == EVTLINE || gevent.type == EVTHYPER || gevent.type == EVTMOUSE)
        _shouldScrollOnCharEvent = YES;

    if (waitforfilename) {
        [_queue addObject:gevent];
    } else if (waitforevent) {
        [gevent writeEvent:sendfh.fileDescriptor];
        waitforevent = NO;
        [readfh waitForDataInBackgroundAndNotify];
    } else {
        [_queue addObject:gevent];
    }
    if (redrawEvent)
        [self queueEvent:redrawEvent];
}

- (void)noteDataAvailable: (id)sender {
    struct message request;
    struct message reply;
    char minibuf[GLKBUFSIZE + 1];
    char *maxibuf;
    char *buf;
    ssize_t n, t;
    BOOL stop;

    int readfd = readfh.fileDescriptor;
    int sendfd = sendfh.fileDescriptor;

again:

    buf = minibuf;
    maxibuf = NULL;

    if (!pollMoreData(readfd))
        return;
    n = read(readfd, &request, sizeof(struct message));
    if (n < (ssize_t)sizeof(struct message))
    {
        if (n < 0)
            NSLog(@"glkctl: could not read message header");
        else
            NSLog(@"glkctl: connection closed");
        return;
    }

    /* this should only happen when sending resources */
    if (request.len > GLKBUFSIZE)
    {
        maxibuf = malloc(request.len);
        if (!maxibuf)
        {
            NSLog(@"glkctl: out of memory for message (%zu bytes)", request.len);
            return;
        }
        buf = maxibuf;
    }

    if (request.len)
    {
        n = 0;
        while (n < (ssize_t)request.len)
        {
            t = read(readfd, buf + n, request.len - (size_t)n);
            if (t <= 0)
            {
                NSLog(@"glkctl: could not read message body");
                if (maxibuf)
                    free(maxibuf);
                return;
            }
            n += t;
        }
    }

    memset(&reply, 0, sizeof reply);

    stop = [self handleRequest: &request reply: &reply buffer: buf];

    if (reply.cmd > NOREPLY)
    {
        write(sendfd, &reply, sizeof(struct message));
        if (reply.len)
            write(sendfd, buf, reply.len);
    }

    if (maxibuf)
        free(maxibuf);

    /* if stop, don't read or wait for more data */
    if (stop)
        return;

    if (pollMoreData(readfd))
        goto again;
    else
        [readfh waitForDataInBackgroundAndNotify];
}


#pragma mark Border color

- (void)setBorderColor:(NSColor *)color fromWindow:(GlkWindow *)aWindow {
    if (changedBorderThisTurn)
        return;
    NSSize windowsize = aWindow.bounds.size;
    if (aWindow.framePending)
        windowsize = aWindow.pendingFrame.size;
    CGFloat relativeSize = (windowsize.width * windowsize.height) / (_gameView.bounds.size.width * _gameView.bounds.size.height);
    if (relativeSize < 0.70 && ![aWindow isKindOfClass:[GlkTextBufferWindow class]])
        return;

    if (aWindow == [self largestWindow]) {
        [self setBorderColor:color];
    }
}

- (void)setBorderColor:(NSColor *)color {
    if (!color) {
        NSLog(@"setBorderColor called with a nil color!");
        return;
    }

    Theme *theme = _theme;
    if (theme.borderBehavior == kAutomatic) {
        _lastAutoBGColor = color;
    } else {
        color = theme.borderColor;
        if (_lastAutoBGColor == nil)
            _lastAutoBGColor = color;
    }

    _bgcolor = color;
    // The Narcolepsy window mask overrides all border colors
    if (_gameID == kGameIsNarcolepsy && theme.doStyles && theme.doGraphics) {
        _borderView.layer.backgroundColor = CGColorGetConstantColor(kCGColorClear);
        return;
    }

    if (theme.doStyles || [color isEqualToColor:theme.bufferBackground] || [color isEqualToColor:theme.gridBackground] || theme.borderBehavior == kUserOverride) {
        _borderView.layer.backgroundColor = color.CGColor;

        [Preferences instance].borderColorWell.color = color;
    }
}

- (GlkWindow *)largestWindow {
    GlkWindow *largestWin = nil;
    CGFloat largestSize = 0;

    NSArray *winArray = _gwindows.allValues;

    // Check for status window plus buffer window arrangement
    // and return the buffer window
    if (_gwindows.count == 2) {
        if ([winArray[0] isKindOfClass:[GlkTextGridWindow class]] && [winArray[1] isKindOfClass:[GlkTextBufferWindow class]] )
            return winArray[1];
        if ([winArray[0] isKindOfClass:[GlkTextBufferWindow class]] && [winArray[1] isKindOfClass:[GlkTextGridWindow class]] )
            return winArray[0];
    }
    for (GlkWindow *win in winArray) {
        NSSize windowsize = win.bounds.size;
        if (win.framePending)
            windowsize = win.pendingFrame.size;
        CGFloat winarea = windowsize.width * windowsize.height;
        if (winarea >= largestSize) {
            largestSize = winarea;
            largestWin = win;
        }
    }

    return largestWin;
}

- (void)noteManagedObjectContextDidChange:(NSNotification *)notification {
    NSSet *updatedObjects = (notification.userInfo)[NSUpdatedObjectsKey];
    NSSet *refreshedObjects = (notification.userInfo)[NSRefreshedObjectsKey];
    if (!updatedObjects)
        updatedObjects = [NSSet new];
    updatedObjects = [updatedObjects setByAddingObjectsFromSet:refreshedObjects];

    if ([updatedObjects containsObject:_game.metadata])
    {
        if (![_game.metadata.title isEqualToString:_gamefile.lastPathComponent]) {
            dispatch_async(dispatch_get_main_queue(), ^{
                self.window.title = self.game.metadata.title;
            });
        }
    }
}


#pragma mark Full screen

- (NSSize)window:(NSWindow *)window willUseFullScreenContentSize:(NSSize)proposedSize {
    if (window != self.window)
        return proposedSize;
    if (_showingCoverImage) {
        [_coverController.imageView positionImage];
    }

    _gameView.autoresizingMask = NSViewHeightSizable | NSViewMinXMargin | NSViewMaxXMargin;
    _borderView.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;

    if (!inFullScreenResize) {
        NSSize borderSize = _borderView.frame.size;
        NSRect contentFrame = _gameView.frame;
        CGFloat midWidth = borderSize.width / 2;
        if (contentFrame.origin.x > midWidth ||  NSMaxX(contentFrame) < midWidth) {
            contentFrame.origin.x = round(borderSize.width - NSWidth(contentFrame) / 2);
            _gameView.frame = contentFrame;
        }
        if (NSWidth(contentFrame) > borderSize.width - 2 * _theme.border || borderSize.width < borderSize.height) {
            contentFrame.size.width = ceil(borderSize.width - 2 * _theme.border);
            contentFrame.origin.x = _theme.border;
            _gameView.frame = contentFrame;
        }
    }

    _borderFullScreenSize = proposedSize;
    return proposedSize;
}

- (NSArray *)customWindowsToEnterFullScreenForWindow:(NSWindow *)window {
    if (window == self.window) {
        if (restoredController && restoredController.inFullscreen) {
            return @[ window ];
        } else {
            if (_showingCoverImage) {
                return [_coverController customWindowsToEnterFullScreenForWindow:window];
            }
            [self makeAndPrepareSnapshotWindow];
            NSWindow *snapshotWindow = snapshotController.window;
            if (snapshotWindow)
                return @[ window, snapshotWindow ];
        }
    }
    return nil;
}

- (NSArray *)customWindowsToExitFullScreenForWindow:(NSWindow *)window {
    if (window == self.window) {
        if (_showingCoverImage) {
            return [_coverController customWindowsToExitFullScreenForWindow:window];
        }
        return @[ window ];
    } else
        return nil;
}

- (void)windowWillEnterFullScreen:(NSNotification *)notification {
    // Save the window frame in _windowPreFullscreenFrame so that it can be restored when leaving fullscreen.

    // If we are starting up in "system" fullscreen,
    // we will use the autosaved windowPreFullscreenFrame
    // instead (which will be set in the restoreUI method)
    if (_restorationHandler == nil) {
        _windowPreFullscreenFrame = self.window.frame;
        [self storeScrollOffsets];
        _ignoreResizes = YES;
        // _ignoreResizes means no storing scroll offsets,
        // but also no arrange events
    }
    // Sanity check the pre-fullscreen window size.
    // If something has gone wrong, such as the autosave-GUI
    // files becoming corrupted or deletd, this will
    // ensure that the window size is still sensible.
    _windowPreFullscreenFrame = [self frameWithSanitycheckedSize:_windowPreFullscreenFrame];
    _inFullscreen = YES;

}

- (void)windowDidFailToEnterFullScreen:(NSWindow *)window {
    _inFullscreen = NO;
    _ignoreResizes = NO;
    inFullScreenResize = NO;
    _gameView.alphaValue = 1;
    [window setFrame:[self frameWithSanitycheckedSize:_windowPreFullscreenFrame] display:YES];
    _gameView.frame = [self contentFrameForWindowed];
    [self restoreScrollOffsets];
    _gameView.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
    _borderView.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
}

- (void)storeScrollOffsets {
    if (_ignoreResizes)
        return;
    for (GlkWindow *win in _gwindows.allValues)
        if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
            ((GlkTextBufferWindow *)win).pendingScrollRestore = NO;
            [(GlkTextBufferWindow *)win storeScrollOffset];
            ((GlkTextBufferWindow *)win).pendingScrollRestore = YES;
        }
}

- (void)restoreScrollOffsets {
    for (GlkWindow *win in _gwindows.allValues)
        if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
            [(GlkTextBufferWindow *)win restoreScrollBarStyle];
            [(GlkTextBufferWindow *)win performSelector:@selector(restoreScroll:) withObject:nil afterDelay:0.2];
        }
}

- (void)window:(NSWindow *)window
startCustomAnimationToEnterFullScreenWithDuration:(NSTimeInterval)duration {
    if (window != self.window)
        return;

    inFullScreenResize = YES;

    // Make sure the window style mask includes the
    // full screen bit
    window.styleMask = (window.styleMask | NSWindowStyleMaskFullScreen);

    if (restoredController && restoredController.inFullscreen) {
        [self startGameInFullScreenAnimationWithDuration:duration];
    } else {
        if (_showingCoverImage) {
            [_coverController enterFullScreenWithDuration:duration];
            return;
        }

        [self enterFullScreenAnimationWithDuration:duration];
    }
}

- (void)enterFullScreenAnimationWithDuration:(NSTimeInterval)duration {
    NSWindow *window = self.window;
    // Make sure the snapshot window style mask includes the
    // full screen bit
    NSWindow *snapshotWindow = snapshotController.window;
    snapshotWindow.styleMask = (snapshotWindow.styleMask | NSWindowStyleMaskFullScreen);
    [snapshotWindow setFrame:window.frame display:YES];

    NSScreen *screen = window.screen;

    if (NSEqualSizes(_borderFullScreenSize, NSZeroSize))
        _borderFullScreenSize = screen.frame.size;

    // The final, full screen frame
    NSRect border_finalFrame = screen.frame;
    border_finalFrame.size = _borderFullScreenSize;

    // The center frame for the window is used during
    // the 1st half of the fullscreen animation and is
    // the window at its original size but moved to the
    // center of its eventual full screen frame.

    NSRect centerBorderFrame = NSMakeRect(floor((screen.frame.size.width -
                                                 _borderView.frame.size.width) /
                                                2), _borderFullScreenSize.height
                                          - _borderView.frame.size.height,
                                          _borderView.frame.size.width,
                                          _borderView.frame.size.height);

    NSRect centerWindowFrame = [window frameRectForContentRect:centerBorderFrame];

    centerWindowFrame.origin.x += screen.frame.origin.x;
    centerWindowFrame.origin.y += screen.frame.origin.y;

    _borderView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _gameView.autoresizingMask = NSViewMinXMargin | NSViewMaxXMargin |
    NSViewMinYMargin; // Attached at top but not bottom or sides

    NSView *localContentView = _gameView;
    NSView *localBorderView = _borderView;
    NSWindow *localSnapshot = snapshotController.window;

    GlkController * __weak weakSelf = self;
    // Hide contentview
    _gameView.alphaValue = 0;

    // Our animation will be broken into five steps.
    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        // First, we move the window to the center
        // of the screen with the snapshot window on top
        context.duration = duration / 4 + 0.1;
        [[localSnapshot animator] setFrame:centerWindowFrame display:YES];
        [[window animator] setFrame:centerWindowFrame display:YES];
    }
     completionHandler:^{
        [NSAnimationContext
         runAnimationGroup:^(NSAnimationContext *context) {
            // and then we enlarge it to its full size.
            context.duration = duration / 4 - 0.1;
            [[window animator]
             setFrame:[window
                       frameRectForContentRect:border_finalFrame]
             display:YES];
        }
         completionHandler:^{
            // We get the invisible content view into position ...
            NSRect newContentFrame = localContentView.frame;

            newContentFrame.origin =
            NSMakePoint(floor((NSWidth(localBorderView.bounds) -
                               NSWidth(localContentView.frame)) /
                              2),
                        floor(NSHeight(localBorderView.bounds) - weakSelf.theme.border - localContentView.frame.size.height)
                        );
            [NSAnimationContext
             runAnimationGroup:^(NSAnimationContext *context) {
                context.duration = duration / 4;
                localContentView.frame = newContentFrame;
                [weakSelf sendArrangeEventWithFrame:localContentView.frame force:NO];
                [weakSelf flushDisplay];
            }
             completionHandler:^{
                // Now we can fade out the snapshot window
                localContentView.alphaValue = 1;

                [NSAnimationContext
                 runAnimationGroup:^(NSAnimationContext *context) {
                    context.duration = duration / 4;
                    [localSnapshot.contentView animator].alphaValue = 0;
                }
                 completionHandler:^{
                    // Finally, we extend the content view vertically if needed.
                    [NSAnimationContext
                     runAnimationGroup:^(NSAnimationContext *context) {
                        context.duration = 0.2;
                        [localContentView animator].frame = [weakSelf contentFrameForFullscreen];
                    }
                     completionHandler:^{
                        // Hide the snapshot window.
                        ((NSView *)localSnapshot.contentView).hidden = YES;
                        ((NSView *)localSnapshot.contentView).alphaValue = 1;

                        // Send an arrangement event to fill
                        // the new extended area
                        [weakSelf sendArrangeEventWithFrame:localContentView.frame force:NO];
                        [weakSelf restoreScrollOffsets];
                        for (GlkTextGridWindow *quotebox in weakSelf.quoteBoxes)
                        {
                            [quotebox performSelector:@selector(quoteboxAdjustSize:) withObject:nil afterDelay:0.1];
                        }
                    }];
                }];
            }];
        }];
    }];
}

- (void)startGameInFullScreenAnimationWithDuration:(NSTimeInterval)duration {

    NSWindow *window = self.window;
    NSScreen *screen = window.screen;

    if (NSEqualSizes(_borderFullScreenSize, NSZeroSize))
        _borderFullScreenSize = screen.frame.size;

    // The final, full screen frame
    NSRect border_finalFrame = screen.frame;
    border_finalFrame.size = _borderFullScreenSize;

    // The center frame for the window is used during
    // the 1st half of the fullscreen animation and is
    // the window at its original size but moved to the
    // center of its eventual full screen frame.

    NSRect centerWindowFrame = [self frameWithSanitycheckedSize:_windowPreFullscreenFrame];
    centerWindowFrame.origin = NSMakePoint(screen.frame.origin.x + floor((screen.frame.size.width -
                                                                          _borderView.frame.size.width) /
                                                                         2), screen.frame.origin.x + _borderFullScreenSize.height
                                           - centerWindowFrame.size.height);

    centerWindowFrame.origin.x += screen.frame.origin.x;
    centerWindowFrame.origin.y += screen.frame.origin.y;

    GlkController * __weak weakSelf = self;

    BOOL stashShouldShowAlert = _shouldShowAutorestoreAlert;
    _shouldShowAutorestoreAlert = NO;

    // Our animation will be broken into three steps.
    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        // First, we move the window to the center
        // of the screen
        context.duration = duration / 2;
        [[window animator] setFrame:centerWindowFrame display:YES];
    }
     completionHandler:^{
        [NSAnimationContext
         runAnimationGroup:^(NSAnimationContext *context) {
            // then, we enlarge the window to its full size.
            context.duration = duration / 2;
            [[window animator]
             setFrame:[window frameRectForContentRect:border_finalFrame]
             display:YES];
        }
         completionHandler:^{
            GlkController *strongSelf = weakSelf;
            // Finally, we get the content view into position ...
            if (strongSelf) {

                [strongSelf enableArrangementEvents];
                [strongSelf sendArrangeEventWithFrame:[strongSelf contentFrameForFullscreen] force:NO];

                strongSelf.shouldShowAutorestoreAlert = stashShouldShowAlert;
                [strongSelf performSelector:@selector(showAutorestoreAlert:) withObject:nil afterDelay:0.1];
                [strongSelf restoreScrollOffsets];
            }
        }];
    }];
}

- (void)enableArrangementEvents {
    inFullScreenResize = NO;
}

- (void)window:window startCustomAnimationToExitFullScreenWithDuration:(NSTimeInterval)duration {
    if (_showingCoverImage) {
        [_coverController exitFullscreenWithDuration:duration];
        return;
    }
    [self storeScrollOffsets];
    _ignoreResizes = YES;
    _windowPreFullscreenFrame = [self frameWithSanitycheckedSize:_windowPreFullscreenFrame];
    NSRect oldFrame = _windowPreFullscreenFrame;

    oldFrame.size.width =
    _gameView.frame.size.width + _theme.border * 2;

    inFullScreenResize = YES;

    _gameView.autoresizingMask =
    NSViewMinXMargin | NSViewMaxXMargin | NSViewMinYMargin;

    NSWindow __weak *localWindow = self.window;
    NSView __weak *localBorderView = _borderView;
    NSView __weak *localContentView =_gameView;
    GlkController * __weak weakSelf = self;

    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        // Make sure the window style mask does not
        // include full screen bit
        context.duration = duration;
        [window
         setStyleMask:(NSUInteger)([window styleMask] & ~(NSUInteger)NSWindowStyleMaskFullScreen)];
        [[window animator] setFrame:oldFrame display:YES];
    }
     completionHandler:^{
        [weakSelf enableArrangementEvents];
        localBorderView.frame = ((NSView *)localWindow.contentView).frame;
        localContentView.frame = [weakSelf contentFrameForWindowed];
        localContentView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        [weakSelf performSelector:@selector(windowDidExitFullScreen:) withObject:nil afterDelay:0];
    }];
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification {
    _ignoreResizes = NO;
    inFullScreenResize = NO;
    if (_gameView.frame.size.width < 200)
        [self adjustContentView];
    [self contentDidResize:_gameView.frame];
    lastSizeInChars = [self contentSizeToCharCells:_gameView.frame.size];
    _gameView.autoresizingMask = NSViewHeightSizable | NSViewMinXMargin | NSViewMaxXMargin;
    [self restoreScrollOffsets];
}

- (void)windowDidExitFullScreen:(NSNotification *)notification {
    _ignoreResizes = NO;
    _inFullscreen = NO;
    inFullScreenResize = NO;
    [self contentDidResize:_gameView.frame];
    [self restoreScrollOffsets];
    if (self.gameID == kGameIsNarcolepsy && _theme.doGraphics && _theme.doStyles) {
        // FIXME: Very ugly hack to fix the Narcolepsy mask layer
        // It breaks when exiting fullscreen after the player
        // manually has resized the window in windowed mode.
        NSRect newFrame = self.window.frame;
        newFrame.size.width += 1;
        [self.window setFrame:newFrame display:YES];
        newFrame.size.width -= 1;
        [self.window setFrame:newFrame display:YES];
        [self adjustMaskLayer:nil];
    }
    _gameView.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
}

- (void)startInFullscreen {
    // First we show the game windowed
    [self.window setFrame:restoredControllerLate.windowPreFullscreenFrame
                  display:NO];
    [self showWindow:nil];
    [self.window makeKeyAndOrderFront:nil];

    _gameView.frame = [self contentFrameForWindowed];

    _gameView.autoresizingMask = NSViewMinXMargin | NSViewMaxXMargin |
    NSViewMinYMargin; // Attached at top but not bottom or sides

    [self contentDidResize:_gameView.frame];
    for (GlkWindow *win in _gwindows.allValues)
        if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
            GlkTextBufferWindow *bufwin = (GlkTextBufferWindow *)win;
            [bufwin restoreScrollBarStyle];
            // This will prevent storing scroll
            // positions during fullscreen animation
            bufwin.pendingScrollRestore = YES;
        }
}

- (void)deferredEnterFullscreen:(id)sender {
    autorestoring = NO;
    [self.window toggleFullScreen:nil];
    [self performSelector:@selector(showAutorestoreAlert:) withObject:nil afterDelay:1];
}

- (CALayer *)takeSnapshot {
    [self showWindow:nil];
    CGImageRef windowSnapshot = CGWindowListCreateImage(CGRectNull,
                                                        kCGWindowListOptionIncludingWindow,
                                                        (CGWindowID)(self.window).windowNumber,
                                                        kCGWindowImageBoundsIgnoreFraming);
    CALayer *snapshotLayer = [CALayer layer];
    snapshotLayer.frame = self.window.frame;
    snapshotLayer.anchorPoint = CGPointMake(0, 0);
    snapshotLayer.contents = CFBridgingRelease(windowSnapshot);
    return snapshotLayer;
}

- (void)makeAndPrepareSnapshotWindow {
    NSWindow *snapshotWindow;
    if (snapshotController.window) {
        snapshotWindow = snapshotController.window;
        snapshotWindow.contentView.hidden = NO;
        for (CALayer *layer in snapshotWindow.contentView.layer.sublayers)
            [layer removeFromSuperlayer];
    } else {
        snapshotWindow = ([[NSWindow alloc]
                           initWithContentRect:self.window.frame
                           styleMask:0
                           backing:NSBackingStoreBuffered
                           defer:NO]);
        snapshotController = [[NSWindowController alloc] initWithWindow:snapshotWindow];
        snapshotWindow.contentView.wantsLayer = YES;
        snapshotWindow.opaque = NO;
        snapshotWindow.releasedWhenClosed = YES;
        snapshotWindow.backgroundColor = NSColor.clearColor;
    }

    CALayer *snapshotLayer = [self takeSnapshot];

    [snapshotWindow setFrame:self.window.frame display:NO];
    [snapshotWindow.contentView.layer addSublayer:snapshotLayer];
    // Compute the frame of the snapshot layer such that the snapshot is
    // positioned exactly on top of the original position of the game window.
    NSRect snapshotLayerFrame =
    [snapshotWindow convertRectFromScreen:self.window.frame];
    snapshotLayer.frame = snapshotLayerFrame;
    [(NSWindowController *)snapshotWindow.delegate showWindow:nil];
    [snapshotWindow orderFront:nil];
}

// Some convenience methods
- (void)adjustContentView {
    NSRect frame;
    if ((self.window.styleMask & NSWindowStyleMaskFullScreen) == NSWindowStyleMaskFullScreen ||
        _borderView.frame.size.width == self.window.screen.frame.size.width || (dead && _inFullscreen && windowRestoredBySystem)) {
        // We are in fullscreen
        frame = [self contentFrameForFullscreen];
    } else {
        // We are not in fullscreen
        frame = [self contentFrameForWindowed];
    }

    NSUInteger border = (NSUInteger)_theme.border;
    NSRect contentRect = frame;
    contentRect.size = NSMakeSize(frame.size.width + 2 * border, frame.size.height + 2 * border);
    contentRect.origin = NSMakePoint(frame.origin.x - border, frame.origin.y - border);
    if (contentRect.size.width < kMinimumWindowWidth || contentRect.size.height < kMinimumWindowHeight) {
        contentRect = [self frameWithSanitycheckedSize:contentRect];
        frame.size.width = contentRect.size.width - 2 * border;
        frame.size.height = contentRect.size.height - 2 * border;
    }

    NSRect windowframe = self.window.frame;
    NSRect frameForContent = [self.window frameRectForContentRect:contentRect];
    if (windowframe.size.width < frameForContent.size.width || windowframe.size.height < frameForContent.size.height) {
        [self.window setFrame:[self.window frameRectForContentRect:frame] display:YES];
    }

    _gameView.frame = frame;
}

- (NSRect)contentFrameForWindowed {
    NSUInteger border = (NSUInteger)_theme.border;
    return NSMakeRect(border, border,
                      round(NSWidth(_borderView.bounds) - border * 2),
                      round(NSHeight(_borderView.bounds) - border * 2));
}

- (NSRect)contentFrameForFullscreen {
    NSUInteger border = (NSUInteger)_theme.border;
    return NSMakeRect(floor((NSWidth(_borderView.bounds) -
                             NSWidth(_gameView.frame)) / 2),
                      border, NSWidth(_gameView.frame),
                      round(NSHeight(_borderView.bounds) - border * 2));
}

#pragma mark Menu Items

- (IBAction)showGameInfo:(id)sender {
    [libcontroller showInfoForGame:_game toggle:NO];
}

- (IBAction)revealGameInFinder:(id)sender {
    [[NSWorkspace sharedWorkspace] selectFile:_gamefile
                     inFileViewerRootedAtPath:@""];
}

- (IBAction)like:(id)sender {
    if (_game.like == 1)
        _game.like = 0;
    else
        _game.like = 1;
}

- (IBAction)dislike:(id)sender {
    if (_game.like == 2)
        _game.like = 0;
    else
        _game.like = 2;
}

- (IBAction)openIfdb:(id)sender {
    NSString *urlString;
    if (_game.metadata.tuid)
        urlString = [@"https://ifdb.tads.org/viewgame?id=" stringByAppendingString:_game.metadata.tuid];
    else
        urlString = [@"https://ifdb.tads.org/viewgame?ifid=" stringByAppendingString:_game.ifid];
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString: urlString]];
}

- (IBAction)download:(id)sender {
    [libcontroller downloadMetadataForGames:@[_game]];
}

- (IBAction)applyTheme:(id)sender {
    NSString *name = ((NSMenuItem *)sender).title;

    Theme *theme = [Fetches findTheme:name inContext:_game.managedObjectContext];

    if (!theme) {
        NSLog(@"applyTheme: found no theme with name %@", name);
        return;
    }

    Preferences *prefwin = [Preferences instance];
    if (prefwin) {
        [prefwin restoreThemeSelection:theme];
    }

    [[NSNotificationCenter defaultCenter]
     postNotification:[NSNotification notificationWithName:@"PreferencesChanged" object:theme]];
}

- (IBAction)deleteGame:(id)sender {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:NSLocalizedString(@"Are you sure?", nil)];
    alert.informativeText = NSLocalizedString(@"Do you want to close this game and delete it from the library?", nil);
    [alert addButtonWithTitle:NSLocalizedString(@"Delete", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

    NSInteger choice = [alert runModal];

    if (choice == NSAlertFirstButtonReturn) {
        _game.hidden = YES;
        [self.window close];
        [_game.managedObjectContext deleteObject:_game];
    }
}

- (void)journeyPartyAction:(id)sender {
    [self.journeyMenuHandler journeyPartyAction:sender];
}

- (void)journeyMemberVerbAction:(id)sender {
    [self.journeyMenuHandler journeyMemberVerbAction:sender];
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {

    SEL action = menuItem.action;

    if (action == @selector(speakMostRecent:) || action == @selector(speakPrevious:) || action == @selector(speakNext:) || action == @selector(speakStatus:)) {
        return (_voiceOverActive);
    } else if (action == @selector(saveAsRTF:)) {
        for (GlkWindow *win in _gwindows.allValues) {
            if ([win isKindOfClass:[GlkTextGridWindow class]] || [win isKindOfClass:[GlkTextBufferWindow class]])
                return YES;
        }
        return NO;
    } else if (action == @selector(like:)) {
        if (_game.like == 1) {
            menuItem.title = NSLocalizedString(@"Liked", nil);
            menuItem.state = NSOnState;
        } else {
            menuItem.title = NSLocalizedString(@"Like", nil);
            menuItem.state = NSOffState;
        }
    } else if (action == @selector(dislike:)) {
        if (_game.like == 2) {
            menuItem.title = NSLocalizedString(@"Disliked", nil);
            menuItem.state = NSOnState;
        } else {
            menuItem.title = NSLocalizedString(@"Disike", nil);
            menuItem.state = NSOffState;
        }
    } else if (action == @selector(applyTheme:)) {
        if ([Preferences instance].darkOverrideActive || [Preferences instance].lightOverrideActive)
            return NO;
        for (NSMenuItem *item in libcontroller.mainThemesSubMenu.submenu.itemArray) {
            if ([item.title isEqual:_game.theme.name])
                item.state = NSOnState;
            else
                item.state = NSOffState;
        }
    } else if (action == @selector(deleteGame:)) {
        return (!_game.hidden);
    }

    return YES;
}

#pragma mark Accessibility

- (BOOL)isAccessibilityElement {
    return NO;
}

- (NSArray *)accessibilityCustomActions {
    NSAccessibilityCustomAction *speakMostRecent = [[NSAccessibilityCustomAction alloc]
                                                    initWithName:NSLocalizedString(@"repeat the text output of the last move", nil) target:self selector:@selector(speakMostRecent:)];
    NSAccessibilityCustomAction *speakPrevious = [[NSAccessibilityCustomAction alloc]
                                                  initWithName:NSLocalizedString(@"step backward through moves", nil) target:self selector:@selector(speakPrevious:)];
    NSAccessibilityCustomAction *speakNext = [[NSAccessibilityCustomAction alloc]
                                              initWithName:NSLocalizedString(@"step forward through moves", nil) target:self selector:@selector(speakNext:)];
    NSAccessibilityCustomAction *speakStatus = [[NSAccessibilityCustomAction alloc]
                                                initWithName:NSLocalizedString(@"speak status bar text", nil) target:self selector:@selector(speakStatus:)];

    return @[speakStatus, speakNext, speakPrevious, speakMostRecent];
}


- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context {

    if ([keyPath isEqualToString:@"voiceOverEnabled"]) {
        _voiceOverActive = [NSWorkspace sharedWorkspace].voiceOverEnabled;
        if (_voiceOverActive) { // VoiceOver was switched on
            // Don't speak or change menus unless we are the top game
            if ([Preferences.instance currentGame] == _game && !dead) {
                [_journeyMenuHandler showJourneyMenus];
                if (_journeyMenuHandler.shouldShowDialog) {
                    [_journeyMenuHandler recreateDialog];
                } else {
                    [self speakOnBecomingKey];
                }
            }
        } else { // VoiceOver was switched off
            [_journeyMenuHandler hideJourneyMenus];
        }
        // We send an event to let the interpreter know VoiceOver status.
        // Only to Bocfel for now.
        if ([_terpname isEqualToString:@"bocfel"])
            [self sendArrangeEventWithFrame:_gameView.frame force:NO];
    } else {
        // Any unrecognized context must belong to super
        [super observeValueForKeyPath:keyPath
                             ofObject:object
                               change:change
                              context:context];
    }
}

- (IBAction)saveAsRTF:(id)sender {
    GlkWindow *largest = self.largestWithMoves;
    if (largest && [largest isKindOfClass:[GlkTextBufferWindow class]] ) {
        [largest saveAsRTF:self];
        return;
    }
    largest = nil;
    NSUInteger longestTextLength = 0;
    for (GlkWindow *win in _gwindows.allValues) {
        if (![win isKindOfClass:[GlkGraphicsWindow class]]) {
            GlkTextBufferWindow *bufWin = (GlkTextBufferWindow *)win;
            NSUInteger length = bufWin.textview.string.length;
            if (length > longestTextLength) {
                longestTextLength = length;
                largest = win;
            }
        }
    }

    if (largest) {
        [largest saveAsRTF:self];
        return;
    }
}


#pragma mark ZMenu

- (void)checkZMenuAndSpeak:(BOOL)speak {
    if (!_voiceOverActive || _mustBeQuiet || _theme.vOSpeakMenu == kVOMenuNone) {
        return;
    }
    if (_shouldCheckForMenu) {
        _shouldCheckForMenu = NO;
        if (!_zmenu) {
            _zmenu = [[ZMenu alloc] initWithGlkController:self];
        }
        if (![_zmenu isMenu]) {
            [NSObject cancelPreviousPerformRequestsWithTarget:_zmenu];
            _zmenu.glkctl = nil;
            _zmenu = nil;
        }

        // Bureaucracy form accessibility
        if (!_zmenu && _gameID == kGameIsBureaucracy) {
            if (!_form) {
                _form = [[BureaucracyForm alloc] initWithGlkController:self];
            }
            if (![_form isForm]) {
                [NSObject cancelPreviousPerformRequestsWithTarget:_form];
                _form.glkctl = nil;
                _form = nil;
            } else if (speak) {
                [_form speakCurrentField];
            }
        }
    }
    if (_zmenu) {
        for (GlkTextBufferWindow *view in _gwindows.allValues) {
            if ([view isKindOfClass:[GlkTextBufferWindow class]])
                [view setLastMove];
        }
        if (speak)
            [_zmenu speakSelectedLine];
    }
}

- (BOOL)showingInfocomV6Menu {
    return (_infocomV6MenuHandler != nil);
}

#pragma mark Speak new text

- (void)speakNewText {
    // Find a "main text window"
    NSMutableArray *windowsWithText = _gwindows.allValues.mutableCopy;
    NSMutableArray *bufWinsWithoutText = [[NSMutableArray alloc] init];
    for (GlkWindow *view in _gwindows.allValues) {
        if ([view isKindOfClass:[GlkGraphicsWindow class]] || ![(GlkTextBufferWindow *)view setLastMove]) {
            // Remove all Glk window objects with no new text to speak
            [windowsWithText removeObject:view];
            if ([view isKindOfClass:[GlkTextBufferWindow class]] && ((GlkTextBufferWindow *)view).moveRanges.count) {
                [bufWinsWithoutText addObject:view];
            }
        }
    }

    if (_quoteBoxes.count)
        [windowsWithText addObject:_quoteBoxes.lastObject];

    if (!windowsWithText.count) {
        return;
    } else {
        NSMutableArray *bufWinsWithText = [[NSMutableArray alloc] init];
        for (GlkWindow *view in windowsWithText)
            if ([view isKindOfClass:[GlkTextBufferWindow class]])
                [bufWinsWithText addObject:view];
        if (bufWinsWithText.count == 1) {
            [self speakLargest:bufWinsWithText];
            return;
        } else if (bufWinsWithText.count == 0 && bufWinsWithoutText.count > 0) {
            [self speakLargest:bufWinsWithoutText];
            return;
        }
    }
    [self speakLargest:windowsWithText];
}

- (void)speakLargest:(NSArray *)array {
    if (_mustBeQuiet || _zmenu || _form || !_voiceOverActive) {
        return;
    }

    GlkWindow *largest = nil;
    if (array.count == 1) {
        largest = (GlkWindow *)array.firstObject;
    } else {
        CGFloat largestSize = 0;
        for (GlkWindow *view in array) {
            CGFloat size = fabs(view.frame.size.width * view.frame.size.height);
            if (size > largestSize) {
                largestSize = size;
                largest = view;
            }
        }
    }
    if (largest) {
        if ([largest isKindOfClass:[GlkTextGridWindow class]] && _quoteBoxes.count) {
            GlkTextGridWindow *box = _quoteBoxes.lastObject;

            NSString *str = box.textview.string;
            if (str.length) {
                str = [@"QUOTE: \n\n" stringByAppendingString:str];
                [self speakString:str];
                return;
            }
        }
        [largest repeatLastMove:self];
    }
}

- (void)speakOnBecomingKey {
    if (_voiceOverActive) {
        _shouldCheckForMenu = YES;
        [self checkZMenuAndSpeak:NO];
        if (_theme.vODelayOn && !_mustBeQuiet) {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                [self speakMostRecentAfterDelay];
            });
        }
    } else {
        _zmenu = nil;
        _form = nil;
    }
}

#pragma mark Speak previous moves

// If sender == self, never announce "No last move to speak!"
- (IBAction)speakMostRecent:(id)sender {
    if (_zmenu) {
        NSString *menuString = [_zmenu menuLineStringWithTitle:YES index:YES total:YES instructions:YES];
        _zmenu.haveSpokenMenu = YES;
        [self speakString:menuString];
        return;
    } else if (_form) {
        NSString *formString = [_form fieldStringWithTitle:YES andIndex:YES andTotal:YES];
        _form.haveSpokenForm = YES;
        [self speakString:formString];
        return;
    }
    GlkWindow *mainWindow = self.largestWithMoves;
    if (!mainWindow) {
        if (sender != self)
            [self speakStringNow:@"No last move to speak!"];
        return;
    }

    if (_quoteBoxes.count) {
        _speechTimeStamp = [NSDate distantPast];
    }
    [mainWindow setLastMove];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [mainWindow repeatLastMove:sender];
    });
}

- (void)speakMostRecentAfterDelay {
    for (GlkWindow *win in _gwindows.allValues) {
        if ([win isKindOfClass:[GlkTextBufferWindow class]])
            [(GlkTextBufferWindow *)win resetLastSpokenString];
    }

    CGFloat delay = _theme.vOHackDelay;
    shouldAddTitlePrefixToSpeech = (delay < 1);
    delay *= NSEC_PER_SEC;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)delay), dispatch_get_main_queue(), ^(void) {
        [self speakMostRecent:self];
    });
}

- (IBAction)speakPrevious:(id)sender {
    GlkWindow *mainWindow = self.largestWithMoves;
    if (!mainWindow) {
        [self speakStringNow:@"No previous move to speak!"];
        return;
    }
    [mainWindow speakPrevious];
}

- (IBAction)speakNext:(id)sender {
    GlkWindow *mainWindow = self.largestWithMoves;
    if (!mainWindow) {
        [self speakStringNow:@"No next move to speak!"];
        return;
    }
    [mainWindow speakNext];
}

- (IBAction)speakStatus:(id)sender {
    GlkWindow *win;

    // Lazy heuristic to find Tads 3 status window: if there are more than one window and
    // only one of them sits at the top, pick that one
    if ( _gwindows.allValues.count > 1 && [_game.detectedFormat isEqualToString:@"tads3"]) {
        NSMutableArray<GlkWindow *> *array = [[NSMutableArray alloc] initWithCapacity: _gwindows.allValues.count];
        for (win in _gwindows.allValues) {
            if (win.frame.origin.y == 0 && ![win isKindOfClass:[GlkGraphicsWindow class]]) {
                [array addObject:win];
            }
        }
        if (array.count == 1) {
            [array.firstObject speakStatus];
            return;
        }
    }

    // Try to find status window to pass this on to
    for (win in _gwindows.allValues) {
        if ([win isKindOfClass:[GlkTextGridWindow class]]) {
            [(GlkTextGridWindow *)win speakStatus];
            return;
        }
    }
    [self speakStringNow:@"No status window found!"];
}

- (void)forceSpeech {
    _lastSpokenString = nil;
    _speechTimeStamp = [NSDate distantPast];
}

- (void)speakStringNow:(NSString *)string {
    [self forceSpeech];
    [self speakString:string];
}

- (void)speakString:(NSString *)string {
    NSString *newString = string;

    if (string.length == 0 || !_voiceOverActive)
        return;

    if ([string isEqualToString: _lastSpokenString] && _speechTimeStamp.timeIntervalSinceNow > -3.0) {
        return;
    }

    _speechTimeStamp = [NSDate date];
    _lastSpokenString = string;

    NSCharacterSet *charset = [NSCharacterSet characterSetWithCharactersInString:@"\u00A0 >\n_\0\uFFFC"];
    newString = [newString stringByTrimmingCharactersInSet:charset];

    unichar nc = '\0';
    NSString *nullChar = [NSString stringWithCharacters:&nc length:1];
    newString = [newString stringByReplacingOccurrencesOfString:nullChar withString:@""];

    if (newString.length == 0)
        newString = string;

    if (shouldAddTitlePrefixToSpeech) {
        newString = [NSString stringWithFormat:@"Now in, %@: %@", [self gameTitle], newString];
        shouldAddTitlePrefixToSpeech = NO;
    }

    NSWindow *window = self.window;

    if (_gameID == kGameIsJourney && Preferences.instance.currentGame == _game) {
        window = NSApp.mainWindow;
    }

    NSAccessibilityPostNotificationWithUserInfo(
                                                window,
                                                NSAccessibilityAnnouncementRequestedNotification,
                                                @{NSAccessibilityPriorityKey: @(NSAccessibilityPriorityHigh),
                                                  NSAccessibilityAnnouncementKey: newString
                                                });
}

- (GlkWindow *)largestWithMoves {
    // Find a "main text window"
    GlkWindow *largest = nil;

// This somewhat reduces VoiceOver confusingly speaking of the command area grid in Journey
    if (_gameID == kGameIsJourney) {
        for (GlkWindow *view in _gwindows.allValues) {
            if ([view isKindOfClass:[GlkTextBufferWindow class]])
                return view;
        }
    }

    NSMutableArray *windowsWithMoves = _gwindows.allValues.mutableCopy;
    for (GlkWindow *view in _gwindows.allValues) {
        // Remove all Glk windows without text from array
        if (!view.moveRanges || !view.moveRanges.count) {
            // An empty window with an attached quotebox is still considered to have text
            if (!(_quoteBoxes && ([view isKindOfClass:[GlkTextBufferWindow class]] && ((GlkTextBufferWindow *)view).quoteBox)))
                [windowsWithMoves removeObject:view];
        }
    }

    if (!windowsWithMoves.count) {
        NSMutableArray *allWindows = _gwindows.allValues.mutableCopy;
        if (_quoteBoxes.count)
            [allWindows addObject:_quoteBoxes.lastObject];
        for (GlkWindow *view in allWindows) {
            if (![view isKindOfClass:[GlkGraphicsWindow class]] && ((GlkTextGridWindow *)view).textview.string.length > 0)
                [windowsWithMoves addObject:view];
        }
        if (!windowsWithMoves.count) {
            // No windows with text
            return nil;
        }
    }

    CGFloat largestSize = 0;
    for (GlkWindow *view in windowsWithMoves) {
        CGFloat size = fabs(view.frame.size.width * view.frame.size.height);
        if (size > largestSize) {
            largestSize = size;
            largest = view;
        }
    }
    return largest;
}

#pragma mark Custom rotors

- (NSArray *)createCustomRotors {
    if (!_rotorHandler) {
        _rotorHandler = [RotorHandler new];
        _rotorHandler.glkctl = self;
    }
    return _rotorHandler.createCustomRotors;
}

- (NSString *)gameTitle {
    return _game.metadata.title;
}

@end
