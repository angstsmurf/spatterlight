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
        NSWindowStyleMaskFullScreen && !glkctl.ignoreResizes) {
        [glkctl storeScrollOffsets];
    }
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

#import "GlkController_Private.h"

@implementation GlkController

@synthesize autosaveFileGUI = _autosaveFileGUI;
@synthesize autosaveFileTerp = _autosaveFileTerp;

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

    NSString *autosaveLatePath = [[self appSupportDir]
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
    if (autorestoring)
        return;
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


@end
