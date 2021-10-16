#include <sys/time.h>
#import <QuartzCore/QuartzCore.h>

#import "Constants.h"

#import "AppDelegate.h"
#import "GlkController.h"
#import "Preferences.h"
#import "LibController.h"
#import "GlkEvent.h"

#import "GlkTextGridWindow.h"
#import "GlkTextBufferWindow.h"
#import "GlkGraphicsWindow.h"
#import "BufferTextView.h"
#import "GridTextView.h"

#import "GlkSoundChannel.h"
#import "GlkStyle.h"

#import "Game.h"
#import "Theme.h"
#import "Image.h"
#import "ZMenu.h"
#import "BureaucracyForm.h"

#import "NSColor+integer.h"
#import "NSString+Categories.h"

#import "RotorHandler.h"
#import "InfoController.h"
#import "InputTextField.h"
#import "ImageHandler.h"
#import "SoundHandler.h"
#import "Metadata.h"
#import "CommandScriptHandler.h"
#import "CoverImageHandler.h"
#import "CoverImageView.h"
#import "NotificationBezel.h"
#include "FolderAccess.h"

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
//    "STYLEMEASURE",    "SETBGND",          "SETTITLE",    "AUTOSAVE",
//    "RESET",           "BANNERCOLS",       "BANNERLINES"  "TIMER",
//    "INITCHAR",        "CANCELCHAR",
//    "INITLINE",        "CANCELLINE",       "SETECHO",     "TERMINATORS",
//    "INITMOUSE",       "CANCELMOUSE",      "FILLRECT",    "FINDIMAGE",
//    "LOADIMAGE",       "SIZEIMAGE",        "DRAWIMAGE",   "FLOWBREAK",
//    "NEWCHAN",         "DELCHAN",          "FINDSOUND",   "LOADSOUND",
//    "SETVOLUME",       "PLAYSOUND",        "STOPSOUND",   "PAUSE",
//    "UNPAUSE",         "BEEP",
//    "SETLINK",         "INITLINK",         "CANCELLINK",  "SETZCOLOR",
//    "SETREVERSE",      "QUOTEBOX",         "SHOWERROR",   "NEXTEVENT",
//    "EVTARRANGE",      "EVTREDRAW",        "EVTLINE",     "EVTKEY",
//    "EVTMOUSE",        "EVTTIMER",         "EVTHYPER",    "EVTSOUND",
//    "EVTVOLUME",       "EVTPREFS"};

////static const char *wintypenames[] = {"wintype_AllTypes", "wintype_Pair",
////    "wintype_Blank",    "wintype_TextBuffer",
////    "wintype_TextGrid", "wintype_Graphics"};
//
// static const char *stylenames[] =
//{
//    "style_Normal", "style_Emphasized", "style_Preformatted", "style_Header",
//    "style_Subheader", "style_Alert", "style_Note", "style_BlockQuote",
//    "style_Input", "style_User1", "style_User2", "style_NUMSTYLES"
//};
////
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

- (id) initWithCoder:(NSCoder *)decoder {
    int version = [decoder decodeIntForKey:@"version"];
    if (version <= 0 || version != AUTOSAVE_SERIAL_VERSION)
    {
        NSLog(@"Interpreter autosave file:Wrong serial version!");
        return nil;
    }

    _autosaveTag = (glui32)[decoder decodeInt32ForKey:@"autosaveTag"];

    return self;
}
@end


@implementation GlkHelperView

- (BOOL)isFlipped {
    return YES;
}

- (void)setFrame:(NSRect)frame {
    //    NSLog(@"GlkHelperView (_contentView) setFrame: %@ Previous frame: %@",
    //          NSStringFromRect(frame), NSStringFromRect(self.frame));

    super.frame = frame;
    GlkController *glkctl = _glkctrl;

    if ([glkctl isAlive] && !self.inLiveResize && !glkctl.ignoreResizes) {
        [glkctl contentDidResize:frame];
    }
}

- (void)viewWillStartLiveResize {
    GlkController *glkctl = _glkctrl;
    if ((glkctl.window.styleMask & NSFullScreenWindowMask) !=
        NSFullScreenWindowMask && !glkctl.ignoreResizes)
        [glkctl storeScrollOffsets];
}

- (void)viewDidEndLiveResize {
    GlkController *glkctl = _glkctrl;
    // We use a custom fullscreen width, so don't resize to full screen width
    // when viewDidEndLiveResize is called because we just entered fullscreen
    if ((glkctl.window.styleMask & NSFullScreenWindowMask) !=
        NSFullScreenWindowMask && !glkctl.ignoreResizes) {
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
    BOOL restoredUIOnly;
    BOOL shouldShowAutorestoreAlert;

    NSWindowController *snapshotController;

    BOOL windowClosedAlready;
    BOOL restartingAlready;

    /* the glk objects */
    BOOL windowdirty; /* the contentView needs to repaint */

    GlkController *restoredController;
    GlkController *restoredControllerLate;
    NSMutableData *bufferedData;

    LibController *libcontroller;

    NSSize lastSizeInChars;
    Theme *lastTheme;

    // To fix scrolling in the Adrian Mole games
    NSInteger lastRequest;

    BOOL skipNextScriptCommand;
    //    NSDate *lastFlushTimestamp;
    NSDate *lastKeyTimestamp;
    NSDate *lastResetTimestamp;
}
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
     winRestore:(BOOL)windowRestoredBySystem_ {

    if (!game_) {
        NSLog(@"GlkController runTerp called with nil game!");
        return;
    }

    _soundHandler = [SoundHandler new];
    _soundHandler.glkctl = self;
    _imageHandler = [ImageHandler new];

    //    NSLog(@"glkctl: runterp %@ %@", terpname_, game_.metadata.title);

    // We could use separate versioning for GUI and interpreter autosaves,
    // but it is probably simpler this way
    _autosaveVersion = AUTOSAVE_SERIAL_VERSION;

    _ignoreResizes = YES;

    skipNextScriptCommand = NO;

    _game = game_;

    Game *game = _game;
    _theme = game.theme;
    Theme *theme = _theme;

    libcontroller = ((AppDelegate *)[NSApplication sharedApplication].delegate).libctl;

    [self.window registerForDraggedTypes:@[ NSURLPboardType, NSStringPboardType]];

    if (!theme.name) {
        NSLog(@"GlkController runTerp called with theme without name!");
        game.theme = [Preferences currentTheme];
        _theme = game.theme;
        theme = _theme;
    }

    if (theme.nohacks) {
        [self resetGameDetection];
    } else {
        [self detectGame:game.ifid];
    }

    NSURL *url = [game urlForBookmark];
    _gamefile = url.path;

    if (![[NSFileManager defaultManager] isReadableFileAtPath:_gamefile]) {
        [self.window performClose:nil];
        return;
    }

    [_imageHandler cacheImagesFromBlorb:url];

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

    _supportsAutorestore = [self.window isRestorable];
    game.autosaved = _supportsAutorestore;
    windowRestoredBySystem = windowRestoredBySystem_;

    shouldShowAutorestoreAlert = NO;
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

    self.window.title = game.metadata.title;
    if (NSAppKitVersionNumber >= NSAppKitVersionNumber10_12) {
        [self.window setValue:@2 forKey:@"tabbingMode"];
    }

    waitforevent = NO;
    waitforfilename = NO;
    dead = YES; // This should be YES until the interpreter process is running

    _newTimer = NO;
    _newTimerInterval = 0.2;

    _contentView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

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

    lastContentResize = NSZeroRect;
    _inFullscreen = NO;
    _windowPreFullscreenFrame = self.window.frame;
    _borderFullScreenSize = NSZeroSize;

    restoredController = nil;
    inFullScreenResize = NO;

    if (@available(macOS 10.13, *)) {
        _voiceOverActive = [[NSWorkspace sharedWorkspace] isVoiceOverEnabled];
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(noteAccessibilityStatusChanged:)
                                                     name:@"NSApplicationDidChangeAccessibilityEnhancedUserInterfaceNotification"
                                                   object:nil];
    } else {
        _voiceOverActive = YES;
    }

    [[NSNotificationCenter defaultCenter]
     addObserver:self
     selector:@selector(notePreferencesChanged:)
     name:@"PreferencesChanged"
     object:nil];

    [[NSNotificationCenter defaultCenter]
     addObserver:self
     selector:@selector(noteDefaultSizeChanged:)
     name:@"DefaultSizeChanged"
     object:nil];

    [[NSNotificationCenter defaultCenter]
     addObserver:self
     selector:@selector(noteBorderChanged:)
     name:@"BorderChanged"
     object:nil];

    [[NSNotificationCenter defaultCenter]
     addObserver:self
     selector:@selector(noteManagedObjectContextDidChange:)
     name:NSManagedObjectContextObjectsDidChangeNotification
     object:game.managedObjectContext];

    self.window.representedFilename = _gamefile;

    _borderView.wantsLayer = YES;
    //    _borderView.canDrawSubviewsIntoLayer = YES;
    _borderView.layerContentsRedrawPolicy = NSViewLayerContentsRedrawOnSetNeedsDisplay;
    _lastAutoBGColor = theme.bufferBackground;
    if (theme.borderBehavior == kUserOverride)
        [self setBorderColor:theme.borderColor];
    else
        [self setBorderColor:theme.bufferBackground];

    NSString *autosaveLatePath = [self.appSupportDir
                                  stringByAppendingPathComponent:@"autosave-GUI-late.plist"];

    lastKeyTimestamp = [NSDate distantPast];

    if (self.narcolepsy && theme.doGraphics && theme.doStyles) {
        [self adjustMaskLayer:nil];
    }

    if (_supportsAutorestore && theme.autosave &&
        ([[NSFileManager defaultManager] fileExistsAtPath:self.autosaveFileGUI] || [[NSFileManager defaultManager] fileExistsAtPath:autosaveLatePath])) {
        [self runTerpWithAutorestore];
    } else {
        [self runTerpNormal];
    }
}

- (void)runTerpWithAutorestore {
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
        terpAutosaveDate = (NSDate*)[attrs objectForKey: NSFileCreationDate];
    } else {
        NSLog(@"Error: %@", error);
    }

    // Check creation date of GUI autosave file
    attrs = [fileManager attributesOfItemAtPath:self.autosaveFileGUI error:&error];
    if (attrs) {
        GUIAutosaveDate = (NSDate*)[attrs objectForKey: NSFileCreationDate];
        if (terpAutosaveDate && [terpAutosaveDate compare:GUIAutosaveDate] == NSOrderedDescending) {
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
        @try {
            restoredControllerLate =
            [NSKeyedUnarchiver unarchiveObjectWithFile:autosaveLatePath];
        } @catch (NSException *ex) {
            NSLog(@"Unable to restore late GUI autosave: %@", ex);
            restoredControllerLate = restoredController;
        }
    } else {
        NSLog(@"No late autosave exists (%@)", autosaveLatePath);
    }

    if ([[NSFileManager defaultManager] fileExistsAtPath:autosaveLatePath]) {

        // Get creation date of GUI late autosave
        attrs = [fileManager attributesOfItemAtPath:autosaveLatePath error:&error];
        if (attrs) {
            GUILateAutosaveDate = (NSDate*)[attrs objectForKey: NSFileCreationDate];
        } else {
            NSLog(@"Error: %@", error);
        }

        // Try loading GUI late autosave
        @try {
            restoredControllerLate =
            [NSKeyedUnarchiver unarchiveObjectWithFile:autosaveLatePath];
        } @catch (NSException *ex) {
            NSLog(@"Unable to restore late GUI autosave: %@", ex);
            restoredControllerLate = restoredController;
        }
    }

    if ([GUIAutosaveDate compare:GUILateAutosaveDate] == NSOrderedDescending) {
        NSLog(@"GUI late autosave file is created before GUI autosave file!");
        NSLog(@"Do not use it.");
        restoredControllerLate = restoredController;
    }

    if (restoredController.autosaveTag != restoredControllerLate.autosaveTag) {
        NSLog(@"GUI late autosave tag does not match GUI autosave file tag!");
        NSLog(@"restoredController.autosaveTag %ld restoredControllerLate.autosaveTag: %ld", restoredController.autosaveTag, restoredControllerLate.autosaveTag);
        if (restoredControllerLate.autosaveTag == 0)
            restoredControllerLate = restoredController;
        else
            restoredController = restoredControllerLate;
    }

    Game *game = _game;
    Theme *theme = _theme;

    if (!restoredController) {
        if (restoredControllerLate) {
            restoredController = restoredControllerLate;
        } else {
            // If there exists an autosave file but we failed to read it,
            // (and also no "GUI-late" autosave)
            // delete it and run game without autorestoring
            [self deleteAutosaveFiles];
            game.autosaved = NO;
            [self runTerpNormal];
            return;
        }
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
        restoredUIOnly = NO;

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
                [self deleteFiles:@[ [NSURL fileURLWithPath:self.autosaveFileGUI],
                                     [NSURL fileURLWithPath:self.autosaveFileTerp] ]];
                restoredUIOnly = YES;
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
                    NSLog(@"Autorestore alert suppressed");
                    if (![defaults boolForKey:@"AlwaysAutorestore"]) {
                        // The user has checked "Remember this choice" when
                        // choosing to not autorestore
                        [self deleteAutosaveFiles];
                        game.autosaved = NO;
                        [self runTerpNormal];
                        return;
                    }
                } else {
                    shouldShowAutorestoreAlert = YES;
                }
            }
        }
    } else {
        NSLog(@"No interpreter autorestore file exists");
        NSLog(@"Only restore UI state at first turn");
        restoredUIOnly = YES;
    }

    if (restoredUIOnly && restoredControllerLate.hasAutoSaved) {
        NSLog(@"restoredControllerLate was not saved at the first turn!");
        restoredUIOnly = NO;
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
        theme = _theme;
    } else _stashedTheme = nil;

    // If this is not a window restoration done by the system,
    // we now re-enter fullscreen manually if the game was
    // closed in fullscreen mode.
    if (!windowRestoredBySystem && _inFullscreen
        && (self.window.styleMask & NSFullScreenWindowMask) != NSFullScreenWindowMask) {
        _startingInFullscreen = YES;
        [self startInFullscreen];
    } else {
        _contentView.autoresizingMask =
        NSViewMinXMargin | NSViewMaxXMargin | NSViewMinYMargin;
        [self.window setFrame:restoredControllerLate.storedWindowFrame display:YES];
        _contentView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    }

    [self adjustContentView];
    shouldRestoreUI = YES;
    _mustBeQuiet = YES;
    [self forkInterpreterTask];

    // The game has to run to its third(?) NEXTEVENT
    // before we can restore the UI properly, so we don't
    // have to do anything else here for now.
}

- (void)runTerpNormal {
    // Just start the game with no autorestore or fullscreen or resetting
    NSRect newContentFrame = [self.window.contentView frame];
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

        //Very lazy cascading
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
                NSLog(@"Got caught in a cascading infinite loop");
            newWindowFrame.origin = thisPoint;
            newWindowFrame.origin.y -= NSHeight(newWindowFrame);
        }

        [self.window setFrame:newWindowFrame display:NO];
        [self adjustContentView];
    }
    lastSizeInChars = [self contentSizeToCharCells:_contentView.frame.size];
    [self showWindow:nil];
    if (_theme.coverArtStyle != kDontShow && _game.metadata.cover.data) {
        [self deleteAutosaveFiles];
        _contentView.autoresizingMask =
        NSViewMinXMargin | NSViewMaxXMargin | NSViewHeightSizable;
        restoredController = nil;
        _coverController = [[CoverImageHandler alloc] initWithController:self];
        [_coverController showLogoWindow];
    } else
        [self forkInterpreterTask];
}

- (void)restoreWindowWhenDead {
    if (restoredController.showingCoverImage) {
        dead = NO;
        _showingCoverImage = YES;
        [self runTerpNormal];
        return;
    }

    dead = YES;

    [self.window setFrame:restoredController.storedWindowFrame display:NO];

    NSSize defsize = [self.window
                      contentRectForFrameRect:restoredController.storedWindowFrame]
        .size;
    [self.window setContentSize:defsize];
    _borderView.frame = NSMakeRect(0, 0, defsize.width, defsize.height);
    _contentView.frame = restoredController.storedContentFrame;
    _contentView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    [self restoreUI];
    self.window.title = [self.window.title stringByAppendingString:@" (finished)"];

    GlkController * __unsafe_unretained weakSelf = self;
    NSScreen *screen = self.window.screen;
    NSString *title = [NSString stringWithFormat:@"%@ has finished.", _game.metadata.title];
    double delayInSeconds = 1.5;
    dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
    dispatch_after(popTime, dispatch_get_main_queue(), ^(void) {
        NotificationBezel *bezel = [[NotificationBezel alloc] initWithScreen:screen];
        [bezel showGameOver];
        [weakSelf speakString:title];
    });

    restoredController = nil;
}

- (void)detectGame:(NSString *)ifid {
    NSString *l9Substring = nil;
    if (ifid.length >= 10)
        l9Substring = [ifid substringToIndex:10];
    if ([l9Substring isEqualToString:@"LEVEL9-001"] || // The Secret Diary of Adrian Mole
        [l9Substring isEqualToString:@"LEVEL9-002"] || // The Growing Pains of Adrian Mole
        [l9Substring isEqualToString:@"LEVEL9-019"]) { // The Archers
        _adrianMole = YES;
    } else if ([ifid isEqualToString:@"ZCODE-5-990206-6B48"]) {
        _anchorheadOrig = YES;
    } else if ([ifid isEqualToString:@"ZCODE-47-870915"] ||
               [ifid isEqualToString:@"ZCODE-49-870917"] ||
               [ifid isEqualToString:@"ZCODE-51-870923"] ||
               [ifid isEqualToString:@"ZCODE-57-871221"] ||
               [ifid isEqualToString:@"ZCODE-60-880610"]) {
        _beyondZork = YES;
    } else if ([ifid isEqualToString:@"ZCODE-86-870212"] ||
               [ifid isEqualToString:@"ZCODE-116-870602"] ||
               [ifid isEqualToString:@"ZCODE-160-880521"]) {
        _bureaucracy = YES;
    } else if ([ifid isEqualToString:@"BFDE398E-C724-4B9B-99EB-18EE4F26932E"]) {
        _colderLight = YES;
    } else if ([ifid isEqualToString:@"ZCODE-7-930428-0000"] ||
               [ifid isEqualToString:@"ZCODE-8-930603-0000"] ||
               [ifid isEqualToString:@"ZCODE-10-940120-BD9E"] ||
               [ifid isEqualToString:@"ZCODE-12-940604-6035"] ||
               [ifid isEqualToString:@"ZCODE-16-951024-4DE6"]) {
        _curses = YES;
    } else if ([ifid isEqualToString:@"303E9BDC-6D86-4389-86C5-B8DCF01B8F2A"]) {
        _deadCities = YES;
    } else if ([ifid isEqualToString:@"AC0DAF65-F40F-4A41-A4E4-50414F836E14"]) {
        _kerkerkruip = YES;
    } else if ([ifid isEqualToString:@"GLULX-1-040108-D8D78266"]) {
        _narcolepsy = YES;
    } else if ([ifid isEqualToString:@"afb163f4-4d7b-0dd9-1870-030f2231e19f"]) {
        _thaumistry = YES;
    } else if ([ifid isEqualToString:@"ZCODE-1-851202"] ||
               [ifid isEqualToString:@"ZCODE-1-860221"] ||
               [ifid isEqualToString:@"ZCODE-14-860313"] ||
               [ifid isEqualToString:@"ZCODE-11-860509"] ||
               [ifid isEqualToString:@"ZCODE-12-860926"] ||
               [ifid isEqualToString:@"ZCODE-15-870628"]) {
        _trinity = YES;
    }
}

- (void)resetGameDetection {
    _adrianMole = NO;
    _anchorheadOrig = NO;
    _beyondZork = NO;
    _bureaucracy = NO;
    _colderLight = NO;
    _curses = NO;
    _deadCities = NO;
    _kerkerkruip = NO;
    _narcolepsy = NO;
    _thaumistry = NO;
    _trinity = NO;
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

    GlkController * __unsafe_unretained weakSelf = self;

    [task setTerminationHandler:^(NSTask *aTask) {
        [aTask.standardOutput fileHandleForReading].readabilityHandler = nil;
        GlkController *strongSelf = weakSelf;
        if(strongSelf) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [strongSelf noteTaskDidTerminate:aTask];
            });
        }
    }];

    _queue = [[NSMutableArray alloc] init];

    [[NSNotificationCenter defaultCenter]
     addObserver: self
     selector: @selector(noteDataAvailable:)
     name: NSFileHandleDataAvailableNotification
     object: readfh];

    dead = NO;

    if (_secureBookmark == nil) {
        _secureBookmark = [FolderAccess grantAccessToFile:[NSURL fileURLWithPath:_gamefile]];
    }

    [task launch];

    /* Send a prefs and an arrange event first thing */
    GlkEvent *gevent;

    gevent = [[GlkEvent alloc] initPrefsEventForTheme:theme];
    [self queueEvent:gevent];

    if (!(_inFullscreen && windowRestoredBySystem)) {
        [self sendArrangeEventWithFrame:_contentView.frame force:NO];
    }

    restartingAlready = NO;
    [readfh waitForDataInBackgroundAndNotify];
}

- (void)askForAccessToURL:(NSURL *)url showDialog:(BOOL)dialogFlag andThenRunBlock:(void (^)(void))block {

    _showingDialog = NO;

    NSURL *bookmarkURL = [FolderAccess suitableDirectoryForURL:url];
    if (bookmarkURL) {
        if ([[NSFileManager defaultManager] isReadableFileAtPath:bookmarkURL.path]) {
            [FolderAccess storeBookmark:bookmarkURL];
            [FolderAccess saveBookmarks];
        } else {
            [FolderAccess restoreURL:bookmarkURL];
            if (![[NSFileManager defaultManager] isReadableFileAtPath:bookmarkURL.path]) {

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

                __unsafe_unretained GlkController *weakSelf = self;

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

#pragma mark Autorestore

- (GlkTextGridWindow *)findGridWindowIn:(NSView *)theView
{
    // search the subviews for a view of class GlkTextGridWindow
    __block __weak GlkTextGridWindow * (^weak_findGridWindow)(NSView *);

    GlkTextGridWindow * (^findGridWindow)(NSView *);

    weak_findGridWindow = findGridWindow = ^(NSView *view) {
        if ([view isKindOfClass:[GlkTextGridWindow class]])
            return (GlkTextGridWindow *)view;
        __block GlkTextGridWindow *foundView = nil;
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
    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
    NSManagedObjectContext *context = libcontroller.managedObjectContext;
    Theme *foundTheme = nil;
    fetchRequest.entity = [NSEntityDescription entityForName:@"Theme" inManagedObjectContext:context];
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

    if (restoredUIOnly) {
        restoredController = restoredControllerLate;
        shouldShowAutorestoreAlert = NO;
    }

    shouldRestoreUI = NO;
    _shouldStoreScrollOffset = NO;

    _soundHandler = restoredControllerLate.soundHandler;
    _soundHandler.glkctl = self;
    [_soundHandler restartAll];

    GlkWindow *win;

    // Copy values from autorestored GlkController object
    _firstResponderView = restoredControllerLate.firstResponderView;
    _windowPreFullscreenFrame = restoredControllerLate.windowPreFullscreenFrame;
    _autosaveTag = restoredController.autosaveTag;

    _bufferStyleHints = restoredController.bufferStyleHints;
    _gridStyleHints = restoredController.gridStyleHints;

    // Restore frame size
    _contentView.frame = restoredControllerLate.storedContentFrame;

    // Copy all views and GlkWindow objects from restored Controller
    for (id key in restoredController.gwindows) {

        win = _gwindows[key];

        if (!restoredUIOnly) {
            if (win) {
                [win removeFromSuperview];
                _gwindows[key] = nil;
                win = nil;
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
                if (quotebox) {
                    if (_quoteBoxes == nil)
                        _quoteBoxes = [[NSMutableArray alloc] init];
                    [quotebox removeFromSuperview];
                    [_quoteBoxes addObject:quotebox];
                    quotebox.glkctl = self;
                    quotebox.quoteboxParent = ((GlkTextBufferWindow *)win).textview.enclosingScrollView;
                    NSInteger diff = _turns - restoredController.turns;
                    quotebox.quoteboxAddedOnTurn += diff;
                }
            }

            [win removeFromSuperview];
            [_contentView addSubview:win];

            win.glkctl = self;
            win.theme = _theme;
        }
        GlkWindow *laterWin = (restoredControllerLate.gwindows)[key];
        win.frame = laterWin.frame;
    }

    [self adjustContentView];

    if (restoredControllerLate.bgcolor)
        [self setBorderColor:restoredControllerLate.bgcolor];

    GlkWindow *winToGrabFocus = nil;

    if (restoredController.commandScriptRunning) {
        [self.commandScriptHandler copyPropertiesFrom:restoredController.commandScriptHandler];
    }

    // Restore scroll position etc
    for (win in [_gwindows allValues]) {
        if (![win isKindOfClass:[GlkGraphicsWindow class]] && ![_windowsToRestore count]) {
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

    if (!restoredUIOnly)
        _hasAutoSaved = YES;

    restoredController = nil;
    restoredControllerLate = nil;
    restoredUIOnly = NO;

    // We create a forced arrange event in order to force the interpreter process
    // to re-send us window sizes. The player may have changed settings that
    // affect window size since the autosave was created.

    [self performSelector:@selector(postRestoreArrange:) withObject:nil afterDelay:0];
}


- (void)postRestoreArrange:(id)sender {
    if (shouldShowAutorestoreAlert && !_startingInFullscreen) {
        shouldShowAutorestoreAlert = NO;
        [self performSelector:@selector(showAutorestoreAlert:) withObject:nil afterDelay:0.1];
    }

    Theme *stashedTheme = _stashedTheme;
    if (stashedTheme && stashedTheme != _theme)
    {
        _theme = stashedTheme;
        _stashedTheme = nil;
    }
    NSNotification *notification = [NSNotification notificationWithName:@"PreferencesChanged" object:_theme];
    [self notePreferencesChanged:notification];

    [self sendArrangeEventWithFrame:_contentView.frame force:YES];

    _shouldStoreScrollOffset = YES;

    // Now we can actually show the window
    [self showWindow:nil];
    [self.window makeKeyAndOrderFront:nil];
    [self.window makeFirstResponder:nil];
    if (_startingInFullscreen)
        [self performSelector:@selector(deferredEnterFullscreen:) withObject:nil afterDelay:0.1];
}


- (NSString *)appSupportDir {
    if (!_appSupportDir) {
        NSDictionary *gFolderMap = @{@"adrift" : @"SCARE",
                                     @"advsys" : @"AdvSys",
                                     @"agt" : @"AGiliTy",
                                     @"glulx" : @"Glulxe",
                                     @"hugo" : @"Hugo",
                                     @"level9" : @"Level 9",
                                     @"magscrolls" : @"Magnetic",
                                     @"quill" : @"UnQuill",
                                     @"tads2" : @"TADS",
                                     @"tads3" : @"TADS",
                                     @"zcode": @"Bocfel"
        };

        NSDictionary *gFolderMapExt = @{@"acd" : @"Alan 2",
                                        @"a3c" : @"Alan 3",
                                        @"d$$" : @"AGiliTy"};

        NSError *error;
        NSURL *appSupportURL = [[NSFileManager defaultManager]
                                URLForDirectory:NSApplicationSupportDirectory
                                inDomain:NSUserDomainMask
                                appropriateForURL:nil
                                create:YES
                                error:&error];

        if (error)
            NSLog(@"Could not find Application Support folder. Error: %@",
                  error);

        Game *game = _game;
        NSString *detectedFormat = game.detectedFormat;

        if (!detectedFormat) {
            NSLog(@"GlkController appSupportDir: Game %@ has no specified format!", game.metadata.title);
            return nil;
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
                  stringByAppendingPathComponent:[_gamefile signatureFromFile]];
        dirstr = [dirstr stringByAddingPercentEncodingWithAllowedCharacters:[NSCharacterSet URLPathAllowedCharacterSet]];

        appSupportURL = [NSURL URLWithString:dirstr
                               relativeToURL:appSupportURL];

        if (_supportsAutorestore) {
            [[NSFileManager defaultManager] createDirectoryAtURL:appSupportURL
                                     withIntermediateDirectories:YES
                                                      attributes:nil
                                                           error:NULL];

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

            BOOL succeed =
            [dummytext writeToURL:[NSURL fileURLWithPath:dummyfilepath]
                       atomically:YES
                         encoding:NSUTF8StringEncoding
                            error:&error];
            if (!succeed) {
                NSLog(@"Failed to write dummy file to autosave directory. Error:%@",
                      error);
            }
        }
        _appSupportDir = appSupportURL.path;
    }
    return _appSupportDir;
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

// LibController calls this to reset non-running games
- (void)deleteAutosaveFilesForGame:(Game *)aGame {
    _gamefile = [aGame urlForBookmark].path;
    aGame.autosaved = NO;
    NSLog(@"GlkController deleteAutosaveFilesForGame: set autosaved of game %@ to NO", aGame.metadata.title);
    if (!_gamefile)
        return;
    _game = aGame;
    [self deleteAutosaveFiles];
}

- (void)deleteAutosaveFiles {

    [self deleteFiles:@[ [NSURL fileURLWithPath:self.autosaveFileGUI],
                         [NSURL fileURLWithPath:self.autosaveFileTerp],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave.glksave"]],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-tmp.glksave"]],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-GUI.plist"]],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-GUI-late.plist"]],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-tmp.plist"]] ]];
}

- (void)deleteFiles:(NSArray *)urls {
    [[NSWorkspace sharedWorkspace] recycleURLs:urls completionHandler:^(NSDictionary *newURLs, NSError *error) {
        //        if (error) {
        //            NSLog(@"deleteAutosaveFiles: %@", error);
        //        }
    }];
}

- (void)autoSaveOnExit {
    if (_supportsAutorestore && _theme.autosave) {
        NSString *autosaveLate = [self.appSupportDir
                                  stringByAppendingPathComponent:@"autosave-GUI-late.plist"];


        if (@available(macOS 10.13, *)) {
            NSError *error = nil;

            NSData *data = [NSKeyedArchiver archivedDataWithRootObject:self requiringSecureCoding:NO error:&error];
            [data writeToFile:autosaveLate options:NSDataWritingAtomic error:&error];

            if (error) {
                NSLog(@"autoSaveOnExit: Write returned error: %@", [error localizedDescription]);
                return;
            }

        } else {
            // Fallback on earlier version
            NSInteger res = [NSKeyedArchiver archiveRootObject:self
                                                        toFile:autosaveLate];
            if (!res) {
                NSLog(@"GUI autosave on exit failed!");
                return;
            }
        }
        // NSLog(@"UI autosaved on exit, turn %ld, event count %ld. Tag: %ld", _turns, _eventcount, _autosaveTag);
        _game.autosaved = YES;
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

        _storedContentFrame = [decoder decodeRectForKey:@"contentFrame"];
        _storedBorderFrame = [decoder decodeRectForKey:@"borderFrame"];

        _bgcolor = [decoder decodeObjectOfClass:[NSColor class] forKey:@"backgroundColor"];

        _bufferStyleHints = [decoder decodeObjectOfClass:[NSMutableArray class] forKey:@"bufferStyleHints"];
        _gridStyleHints = [decoder decodeObjectOfClass:[NSMutableArray class] forKey:@"gridStyleHints"];

        _queue = [decoder decodeObjectOfClass:[NSMutableArray class] forKey:@"queue"];

        _firstResponderView = [decoder decodeIntegerForKey:@"firstResponder"];
        _inFullscreen = [decoder decodeBoolForKey:@"fullscreen"];

        _turns = [decoder decodeIntegerForKey:@"turns"];

        _oldThemeName = [decoder decodeObjectOfClass:[NSString class] forKey:@"oldThemeName"];

        _showingCoverImage = [decoder decodeBoolForKey:@"showingCoverImage"];

        _commandScriptRunning = [decoder decodeBoolForKey:@"commandScriptRunning"];
        if (_commandScriptRunning)
            _commandScriptHandler = [decoder decodeObjectOfClass:[CommandScriptHandler class] forKey:@"commandScriptHandler"];

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
    [encoder encodeRect:_contentView.frame forKey:@"contentFrame"];
    [encoder encodeRect:_borderView.frame forKey:@"borderFrame"];

    [encoder encodeObject:_bgcolor forKey:@"backgroundColor"];

    [encoder encodeObject:_bufferStyleHints forKey:@"bufferStyleHints"];
    [encoder encodeObject:_gridStyleHints forKey:@"gridStyleHints"];

    [encoder encodeObject:_gwindows forKey:@"gwindows"];
    [encoder encodeObject:_soundHandler forKey:@"soundHandler"];
    [encoder encodeObject:_imageHandler forKey:@"imageHandler"];

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
    [encoder encodeBool:((self.window.styleMask & NSFullScreenWindowMask) ==
                         NSFullScreenWindowMask)
                 forKey:@"fullscreen"];

    [encoder encodeInteger:_turns forKey:@"turns"];
    [encoder encodeObject:_theme.name forKey:@"oldThemeName"];

    [encoder encodeBool:_showingCoverImage forKey:@"showingCoverImage"];

    [encoder encodeBool:_commandScriptRunning forKey:@"commandScriptRunning"];
    if (_commandScriptRunning)
        [encoder encodeObject:_commandScriptHandler forKey:@"commandScriptHandler"];
}

- (void)showAutorestoreAlert:(id)userInfo {

    _mustBeQuiet = YES;

    NSAlert *anAlert = [[NSAlert alloc] init];
    anAlert.messageText =
    NSLocalizedString(@"This game was automatically restored from a previous session.", nil);
    anAlert.informativeText = NSLocalizedString(@"Would you like to start over instead?", nil);
    anAlert.showsSuppressionButton = YES;
    anAlert.suppressionButton.title = NSLocalizedString(@"Remember this choice.", nil);
    [anAlert addButtonWithTitle:NSLocalizedString(@"Continue", nil)];
    [anAlert addButtonWithTitle:NSLocalizedString(@"Restart", nil)];

    GlkController * __unsafe_unretained weakSelf = self;

    [anAlert beginSheetModalForWindow:[self window] completionHandler:^(NSInteger result) {

        weakSelf.mustBeQuiet = NO;

        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

        NSString *alertSuppressionKey = @"AutorestoreAlertSuppression";
        NSString *alwaysAutorestoreKey = @"AlwaysAutorestore";

        if (anAlert.suppressionButton.state == NSOnState) {
            // Suppress this alert from now on
            [defaults setBool:YES forKey:alertSuppressionKey];
        }

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

        weakSelf->shouldShowAutorestoreAlert = NO;
    }];
}

- (IBAction)reset:(id)sender {
    if (lastResetTimestamp && [lastResetTimestamp timeIntervalSinceNow] < -1) {
        restartingAlready = NO;
    }

    if (restartingAlready || _showingCoverImage)
        return;

    _commandScriptHandler = nil;

    [_soundHandler stopAllAndCleanUp];

    restartingAlready = YES;
    lastResetTimestamp = [NSDate date];
    _mustBeQuiet = YES;

    [self handleSetTimer:0];

    if (task) {
        //        NSLog(@"glkctl reset: force stop the interpreter");
        task.terminationHandler = nil;
        [task.standardOutput fileHandleForReading].readabilityHandler = nil;
        readfh = nil;
        [task terminate];
        task = nil;
    }

    [self performSelector:@selector(deferredRestart:) withObject:nil afterDelay:0.3];
}

- (void)deferredRestart:(id)sender {
    [self deleteAutosaveFiles];
    [self cleanup];
    [self runTerp:(NSString *)_terpname
         withGame:(Game *)_game
            reset:YES
       winRestore:NO];

    [self.window makeKeyAndOrderFront:nil];
    [self.window makeFirstResponder:nil];
    [self guessFocus];
    NSAccessibilityPostNotification(self.window.firstResponder, NSAccessibilityFocusedUIElementChangedNotification);
}

- (void)handleAutosave:(NSInteger)hash {

    _autosaveTag = hash;

    NSInteger res;

    @autoreleasepool {
        if (@available(macOS 10.13, *)) {
            NSError *error = nil;
            NSData *data = [NSKeyedArchiver archivedDataWithRootObject:self requiringSecureCoding:NO error:&error];
            [data writeToFile:self.autosaveFileGUI options:NSDataWritingAtomic error:&error];
            if (error) {
                NSLog(@"handleAutosave: Write returned error: %@", [error localizedDescription]);
                return;
            }
        } else {
            // Fallback on earlier versions
            NSString *tmplibpath =
            [self.appSupportDir stringByAppendingPathComponent:@"autosave-GUI-tmp.plist"];

            res = [NSKeyedArchiver archiveRootObject:self
                                              toFile:tmplibpath];
            if (!res) {
                NSLog(@"Window serialize failed!");
                return;
            }


            /* This is not really atomic, but we're already past the serious failure modes. */
            [[NSFileManager defaultManager] removeItemAtPath:self.autosaveFileGUI error:nil];

            NSError *error;
            res = [[NSFileManager defaultManager] moveItemAtPath:tmplibpath
                                                          toPath:self.autosaveFileGUI error:&error];
            if (!res) {
                NSLog(@"could not move window autosave to final position! %@", error);
                return;
            }
        }
    }

    _hasAutoSaved = YES;
    //    NSLog(@"UI autosaved successfully on turn %ld, event count %ld. Tag: %ld", _turns, _eventcount, _autosaveTag);
}

-(void)cleanup {
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


/*
 *
 */

#pragma mark Cocoa glue

- (IBAction)showGameInfo:(id)sender {
    [libcontroller showInfoForGame:_game];
}

- (IBAction)revealGameInFinder:(id)sender {
    [[NSWorkspace sharedWorkspace] selectFile:_gamefile
                     inFileViewerRootedAtPath:@""];
}

- (BOOL)isAlive {
    return !dead;
}

- (void)windowDidBecomeKey:(NSNotification *)notification {
    [Preferences changeCurrentGame:_game];
    if (!dead) {
        if (_eventcount > 1 && !shouldShowAutorestoreAlert)
            _mustBeQuiet = NO;
        [self guessFocus];
        [self noteAccessibilityStatusChanged:nil];
        if (_voiceOverActive) {
            [self checkZMenu];
            if (!_zmenu)
                [self speakMostRecent:self];
        }
    }
}

- (void)windowDidResignKey:(NSNotification *)notification {
    _mustBeQuiet = YES;
}

- (BOOL)windowShouldClose:(id)sender {
    //    NSLog(@"glkctl: windowShouldClose");
    NSAlert *alert;

    if (dead || _supportsAutorestore) {
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

- (void)windowWillClose:(id)sender {
    if (windowClosedAlready) {
        NSLog(@"windowWillClose called twice!");
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

    if (_game && [Preferences instance].currentGame == _game) {
        Game *remainingGameSession = nil;
        if (libcontroller && libcontroller.gameSessions.count)
            remainingGameSession = ((GlkController *)(libcontroller.gameSessions.allValues)[0]).game;
        [Preferences changeCurrentGame:remainingGameSession];
    }

    if (timer) {
        //        NSLog(@"glkctl: force stop the timer");
        [timer invalidate];
        timer = nil;
    }

    if (task) {
        //        NSLog(@"glkctl: force stop the interpreter");
        [task setTerminationHandler:nil];
        [task.standardOutput fileHandleForReading].readabilityHandler = nil;
        readfh = nil;
        [task terminate];
    }

    if (libcontroller)
        [libcontroller releaseGlkControllerSoon:self];

    if (_secureBookmark)
        [FolderAccess releaseBookmark:_secureBookmark];

    libcontroller = nil;
}

- (void)flushDisplay {
    //    lastFlushTimestamp = [NSDate date];

    [Preferences instance].inMagnification = NO;

    if (windowdirty) {
        GlkWindow *largest = [self largestWindow];
        if ([largest isKindOfClass:[GlkTextBufferWindow class]] || [largest isKindOfClass:[GlkTextGridWindow class]])
            [(GlkTextBufferWindow *)largest recalcBackground];
        windowdirty = NO;
    }

    for (GlkWindow *win in _windowsToBeAdded) {
        [_contentView addSubview:win];
    }

    if (self.narcolepsy && _theme.doGraphics && _theme.doStyles) {
        [self adjustMaskLayer:nil];
    }

    _windowsToBeAdded = [NSMutableArray new];

    for (GlkWindow *win in [_gwindows allValues]) {
        [win flushDisplay];
    }

    for (GlkWindow *win in _windowsToBeRemoved) {
        [win removeFromSuperview];
        win.glkctl = nil;
    }

    [self checkZMenu];

    if (_shouldSpeakNewText && !_mustBeQuiet && !_zmenu && !_form) {
        [self speakNewText];
    }
    _shouldSpeakNewText = NO;

    _windowsToBeRemoved = [[NSMutableArray alloc] init];
}


- (void)guessFocus {
    id focuswin;

    //    NSLog(@"glkctl guessFocus");

    focuswin = self.window.firstResponder;
    while (focuswin) {
        if ([focuswin isKindOfClass:[NSView class]]) {
            if ([focuswin isKindOfClass:[GlkWindow class]])
                break;
            else
                focuswin = [focuswin superview];
        } else
            focuswin = nil;
    }

    // if (focuswin)
    //  NSLog(@"window %ld has focus", (long)[(GlkWindow*)focuswin name]);

    if (focuswin && [focuswin wantsFocus])
        return;

    // NSLog(@"glkctl guessing new window to focus on");

    for (GlkWindow *win in [_gwindows allValues]) {
        if (win.wantsFocus) {
            [win grabFocus];
            return;
        }
    }
}

- (void)markLastSeen {
    for (GlkWindow *win in [_gwindows allValues]) {
        if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
            [win markLastSeen];
        }
    }
}

- (void)performScroll {
    for (GlkWindow *win in [_gwindows allValues])
        if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
            [win performScroll];
        }
}

- (id)windowWillReturnFieldEditor:(NSWindow *)sender
                         toObject:(id)client {
    for (GlkWindow *win in [_gwindows allValues])
        if (win.input == client) {
            return win.input.fieldEditor;
        }
    return nil;
}

#pragma mark Narcolepsy window mask

- (void)adjustMaskLayer:(id)sender {
    CALayer *maskLayer = nil;
    if (!_contentView.layer.mask) {
        _contentView.layer.mask = [self createMaskLayer];
        maskLayer = _contentView.layer.mask;
        maskLayer.layoutManager  = [CAConstraintLayoutManager layoutManager];
        maskLayer.autoresizingMask = kCALayerHeightSizable | kCALayerWidthSizable;

        self.window.opaque = NO;
        self.window.backgroundColor = [NSColor clearColor];
    }

    if (maskLayer) {
        maskLayer.frame = _contentView.bounds;
    }
}

- (CALayer *)createMaskLayer {
    CALayer *layer = [CALayer layer];

    if (![_imageHandler handleFindImageNumber:3]) {
        NSLog(@"Failed to load image 3!");
        return nil;
    } else {
        CIContext *context = [CIContext contextWithOptions:nil];
        CIImage *inputImage = [CIImage imageWithData:_imageHandler.resources[@(3)].data];

        CIFilter *invert = [CIFilter filterWithName:@"CIColorInvert"];

        [invert setValue: inputImage forKey:kCIInputImageKey];

        CIImage *imageWithFirstFilter = [invert valueForKey:kCIOutputImageKey];

        CIFilter *mask = [CIFilter filterWithName:@"CIMaskToAlpha"];
        [mask setValue: imageWithFirstFilter forKey:kCIInputImageKey];

        CIImage *result = [mask valueForKey:kCIOutputImageKey];

        CGRect extent = [result extent];
        CGImageRef cgImage = [context createCGImage:result fromRect:extent];

        layer.contents = CFBridgingRelease(cgImage);
    }
    return layer;
}

#pragma mark Window resizing

- (NSRect)windowWillUseStandardFrame:(NSWindow *)window
                        defaultFrame:(NSRect)screenframe {
    NSLog(@"glkctl: windowWillUseStandardFrame");

    NSSize windowSize = [self defaultContentSize];

    NSRect frame = [window frameRectForContentRect:NSMakeRect(0, 0, windowSize.width, windowSize.height)];;

    if (frame.size.width > screenframe.size.width)
        frame.size.width = screenframe.size.width;
    if (frame.size.height > screenframe.size.height)
        frame.size.height = screenframe.size.height;

    frame.origin = NSMakePoint(self.window.frame.origin.x, self.window.frame.origin.y - (frame.size.height - self.window.frame.size.height));

    if (NSMaxY(frame) > NSMaxY(screenframe))
        frame.origin.y = NSMaxY(screenframe) - frame.size.height;

    return frame;
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
        //        NSLog(@"contentDidResize called with same frame as last time. Skipping.");
        return;
    }

    lastContentResize = frame;
    lastSizeInChars = [self contentSizeToCharCells:_contentView.frame.size];

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
    NSRect oldframe = _contentView.frame;

    NSUInteger borders = (NSUInteger)_theme.border * 2;

    if ((self.window.styleMask & NSFullScreenWindowMask) !=
        NSFullScreenWindowMask) { // We are not in fullscreen

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
        _contentView.frame = [self contentFrameForWindowed];
    } else {
        //        NSLog(@"zoomContentToSize: we are in fullscreen");
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

        _contentView.frame = newframe;
    }
    _ignoreResizes = NO;
}


- (NSSize)charCellsToContentSize:(NSSize)cells {
    // Only _contentView, does not take border into account
    NSSize size;
    Theme *theme = _theme;
    size.width = round(theme.cellWidth * cells.width + (theme.gridMarginX + 5.0) * 2.0);
    size.height = round(theme.cellHeight * cells.height + (theme.gridMarginY) * 2.0);
    //    NSLog(@"charCellsToContentSize: %@ in character cells is %@ in points", NSStringFromSize(cells), NSStringFromSize(size));
    return size;
}

- (NSSize)contentSizeToCharCells:(NSSize)points {
    // Only _contentView, does not take border into account
    NSSize size;
    Theme *theme = _theme;
    size.width = round((points.width - (theme.gridMarginX + 5.0) * 2.0) / theme.cellWidth);
    size.height = round((points.height - (theme.gridMarginY) * 2.0) / theme.cellHeight);
    //    NSLog(@"contentSizeToCharCells: %@ in points is %@ in character cells ", NSStringFromSize(points), NSStringFromSize(size));
    return size;
}

- (void)noteBorderChanged:(NSNotification *)notify {
    if (notify.object != _theme || (self.window.styleMask & NSFullScreenWindowMask) ==
        NSFullScreenWindowMask || ![[NSUserDefaults standardUserDefaults] boolForKey:@"AdjustSize"])
        return;
    [Preferences instance].inMagnification = YES;
    _movingBorder = YES;
    NSInteger diff = ((NSNumber *)notify.userInfo[@"diff"]).integerValue;
    _contentView.autoresizingMask = NSViewMaxXMargin | NSViewMaxYMargin | NSViewMinXMargin | NSViewMinYMargin;
    NSRect frame = self.window.frame;
    frame.origin = NSMakePoint(frame.origin.x - diff, frame.origin.y - diff);
    frame.size = NSMakeSize(frame.size.width + (diff * 2), frame.size.height + (diff * 2));
    NSRect contentframe = _contentView.frame;
    contentframe.origin = NSMakePoint(contentframe.origin.x + diff, contentframe.origin.y + diff);
    [self.window setFrame:frame display:NO];
    [_contentView setFrame:contentframe];
    _movingBorder = NO;
    _contentView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
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
    NSUInteger lastVOSpeakMenu = (NSUInteger)theme.vOSpeakMenu;

    if (_game) {
        if (!_stashedTheme) {
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
        [self resetGameDetection];
    } else {
        [self detectGame:_game.ifid];
    }

    if (!theme.vOSpeakMenu && lastVOSpeakMenu) { // Check for menu was switched off
        if (_zmenu) {
            [NSObject cancelPreviousPerformRequestsWithTarget:_zmenu];
            _zmenu = nil;
        }
        if (_form) {
            [NSObject cancelPreviousPerformRequestsWithTarget:_form];
            _form = nil;
        }
    } else if (theme.vOSpeakMenu && !lastVOSpeakMenu) { // Check for menu was switched on
        [self checkZMenu];
    }

    _shouldStoreScrollOffset = NO;
    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"AdjustSize"]) {
        if (lastTheme != theme && !NSEqualSizes(lastSizeInChars, NSZeroSize)) { // Theme changed
            NSSize newContentSize = [self charCellsToContentSize:lastSizeInChars];
            NSUInteger borders = (NSUInteger)theme.border * 2;
            NSSize newSizeIncludingBorders = NSMakeSize(newContentSize.width + borders, newContentSize.height + borders);

            if (!NSEqualSizes(_borderView.bounds.size, newSizeIncludingBorders)
                || !NSEqualSizes(_contentView.bounds.size, newContentSize)) {
                [self zoomContentToSize:newContentSize];
                //                NSLog(@"Changed window size to keep size in char cells constant. Previous size in char cells: %@ Current size in char cells: %@", NSStringFromSize(lastSizeInChars), NSStringFromSize([self contentSizeToCharCells:_contentView.frame.size]));
            }
        }
    }

    lastTheme = theme;
    lastSizeInChars = [self contentSizeToCharCells:_contentView.frame.size];

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

    CGFloat width = _contentView.frame.size.width;
    CGFloat height = _contentView.frame.size.height;

    if (width < 0)
        width = 0;
    if (height < 0)
        height = 0;

    gevent = [[GlkEvent alloc] initArrangeWidth:(NSInteger)width
                                         height:(NSInteger)height
                                          theme:theme
                                          force:YES];
    [self queueEvent:gevent];

    gevent = [[GlkEvent alloc] initPrefsEventForTheme:theme];
    [self queueEvent:gevent];

    for (GlkWindow *win in [_gwindows allValues])
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
    _contentView.layer.mask = nil;

    if (self.narcolepsy && theme.doGraphics && theme.doStyles) {
        [self adjustMaskLayer:nil];
    }

    _shouldStoreScrollOffset = YES;
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
    NSRect oldframe = _contentView.frame;

    // Prevent the window from shrinking when zooming in or growing when
    // zooming out, which might otherwise happen at edge cases
    if ((sizeAfterZoom.width < oldframe.size.width && Preferences.zoomDirection == ZOOMIN) ||
        (sizeAfterZoom.width > oldframe.size.width && Preferences.zoomDirection == ZOOMOUT)) {
        NSLog(@"noteDefaultSizeChanged: This would change the size in the wrong direction, so skip");
        return;
    }

    NSLog(@"noteDefaultSizeChanged: Old contentView size: %@", NSStringFromSize(_contentView.frame.size));

    if ((self.window.styleMask & NSFullScreenWindowMask) !=
        NSFullScreenWindowMask) {

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
        NSLog(@"noteDefaultSizeChanged: New contentView size: %@", NSStringFromSize(_contentView.frame.size));

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

        _contentView.frame = newframe;
        NSLog(@"noteDefaultSizeChanged: New contentView size: %@", NSStringFromSize(_contentView.frame.size));

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

    if ( [[pboard types] containsObject:NSStringPboardType] ||
        [[pboard types] containsObject:NSURLPboardType] ) {
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
            s = (theDoc.path).UTF8String;
        } else
            s = "";

        reply.cmd = OKAY;
        reply.len = strlen(s);

        write((int)sendfd, &reply, sizeof(struct message));
        if (reply.len)
            write((int)sendfd, s, reply.len);
        self.mustBeQuiet = NO;
    }];

    waitforfilename = NO; /* we're all done, resume normal processing */

    [readfh waitForDataInBackgroundAndNotify];
}

- (void)feedSaveFileToPrompt {
    NSInteger sendfd = sendfh.fileDescriptor;
    waitforfilename = YES; /* don't interrupt */

    struct message reply;
    reply.cmd = OKAY;
    reply.len = _pendingSaveFilePath.length;

    write((int)sendfd, &reply, sizeof(struct message));
    if (reply.len)
        write((int)sendfd, [_pendingSaveFilePath cStringUsingEncoding:NSUTF8StringEncoding], reply.len);

    waitforfilename = NO; /* we're all done, resume normal processing */

    [readfh waitForDataInBackgroundAndNotify];
}

- (void)handleSavePrompt:(int)fileusage {
    _commandScriptRunning = NO;
    _commandScriptHandler = nil;
    NSURL *directory =
    [NSURL fileURLWithPath:[[NSUserDefaults standardUserDefaults]
                            objectForKey:@"SaveDirectory"]
               isDirectory:YES];
    NSSavePanel *panel = [NSSavePanel savePanel];
    //    NSString *prompt;
    NSString *ext;
    NSString *filename;
    NSString *date;

    waitforfilename = YES; /* don't interrupt */

    switch (fileusage) {
        case fileusage_Data:
            //            prompt = @"Save data file: ";
            ext = @"glkdata";
            filename = @"Data";
            break;
        case fileusage_SavedGame:
            //            prompt = @"Save game: ";
            ext = @"glksave";
            break;
        case fileusage_Transcript:
            //            prompt = @"Save transcript: ";
            ext = @"txt";
            filename = @"Transcript of ";
            break;
        case fileusage_InputRecord:
            //            prompt = @"Save recording: ";
            ext = @"rec";
            filename = @"Recordning of ";
            break;
        default:
            //            prompt = @"Save: ";
            ext = nil;
            break;
    }

    //[panel setNameFieldLabel: prompt];
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
            [[NSUserDefaults standardUserDefaults]
             setObject:theFile.path
                 .stringByDeletingLastPathComponent
             forKey:@"SaveDirectory"];
            s = (theFile.path).UTF8String;
        } else
            s = "";

        reply.cmd = OKAY;
        reply.len = strlen(s);

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

    //    NSLog(@"GlkController handleNewWindowOfType: %s",
    //    wintypenames[wintype]);

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

    // NSLog(@"GlkController handleNewWindowOfType: Adding new %s window with
    // name: %ld", wintypenames[wintype], (long)i);

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
    //    NSLog(@"handleSetTimer: %ld millisecs", millisecs);
    if (timer) {
        [timer invalidate];
        timer = nil;
    }

    NSUInteger minTimer = (NSUInteger)_theme.minTimer;

    if (millisecs > 0) {
        if (millisecs < minTimer) {
            NSLog(@"glkctl: too small timer interval (%ld); increasing to %lu",
                  (unsigned long)millisecs, (unsigned long)minTimer);
            millisecs = minTimer;
        }
        if (_kerkerkruip && millisecs == 10) {
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
        _newTimer = YES;
        _newTimerInterval = timer.timeInterval;
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

    // NSLog(@"handleStyleHintOnWindowType: %s style: %s hint: %s value: %d",
    // wintypenames[wintype], stylenames[style], stylehintnames[hint], value);

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
    //    if (styleuse[1][style_Normal][stylehint_TextColor])
    //        NSLog(@"styleuse[1][style_Normal][stylehint_TextColor] is true. "
    //              @"Value:%ld",
    //              (long)styleval[1][style_Normal][stylehint_TextColor]);

    Theme *theme = _theme;
    if ([gwindow getStyleVal:style hint:hint value:result])
        return YES;
    else {
        if (hint == stylehint_TextColor) {
            if ([gwindow isKindOfClass:[GlkTextBufferWindow class]])
                *result = [theme.bufferNormal.color integerColor];
            else
                *result = [theme.gridNormal.color integerColor];

            return YES;
        }
        if (hint == stylehint_BackColor) {
            if ([gwindow isKindOfClass:[GlkTextBufferWindow class]])
                *result = [theme.bufferBackground integerColor];
            else
                *result = [theme.gridBackground integerColor];

            return YES;
        }
    }
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
                     buffer:(unichar *)buf
                     length:(size_t)len {
    NSString *str;

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
    windowdirty = YES;
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

    //    NSLog(@"handleSetTerminatorsOnWindow: %ld length: %u",
    //    (long)gwindow.name, len );

    for (NSInteger i = 0; i < len; i++) {
        key = @(buf[i]);

        // Convert input terminator keys for Beyond Zork arrow keys hack
        if (_beyondZork && _theme.bZTerminator == kBZArrowsSwapped && [gwindow isKindOfClass:[GlkTextBufferWindow class]]) {
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
            if (_beyondZork) {
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
            } else NSLog(@"Illegal line terminator request: %x", buf[i]);
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


- (BOOL)handleRequest:(struct message *)req
                reply:(struct message *)ans
               buffer:(char *)buf {
    //    NSLog(@"glkctl: incoming request %s", msgnames[req->cmd]);

    NSInteger result;
    GlkWindow *reqWin = nil;
    NSColor *bg = nil;

    //    if (req->cmd != NEXTEVENT && [lastFlushTimestamp timeIntervalSinceNow]  < -0.5) {
    //        [self performScroll];
    //        [self flushDisplay];
    //        NSLog(@"Autoscroll (performScroll + flushDisplay) triggered by timer");
    //    }

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
                    [_gwindows[@(win.name)] postRestoreAdjustments:win];
                }
                _windowsToRestore = nil;
            }
            // If this is the first turn, we try to restore the UI
            // from an autosave file.
            if (_eventcount == 2) {
                if (shouldRestoreUI) {
                    //                    CommandScriptHandler *handler = restoredController.commandScriptHandler;
                    //                    if (handler.commandIndex > 0) {
                    //                        handler.commandIndex--;
                    //                        handler.lastCommandType = handler.nextToLastCommandType;
                    //                    }
                    [self restoreUI];
                } else {
                    // If we are not autorestoring, try to guess an input window.
                    for (GlkWindow *win in _gwindows.allValues) {
                        if ([win isKindOfClass:[GlkTextBufferWindow class]] && [win wantsFocus]) {
                            [win grabFocus];
                        }
                    }
                }
            }

            if (_eventcount > 1 && !shouldShowAutorestoreAlert) {
                _mustBeQuiet = NO;
            }

            _eventcount++;

            if (_shouldSpeakNewText) {
                _turns++;
            }

            if (_quoteBoxes.count && (_turns - _quoteBoxes.lastObject.quoteboxAddedOnTurn > 1 || (_turns == 0 && _quoteBoxes.lastObject.quoteboxAddedOnTurn == -1) || _quoteBoxes.count > 1)) {
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

            [self flushDisplay];
            //            for (GlkWindow *win in _gwindows.allValues)
            //                NSLog(@"%@ %ld: %@", [win class], win.name, NSStringFromRect(win.frame));

            if (_queue.count) {
                GlkEvent *gevent;
                gevent = _queue[0];
                //                NSLog(@"glkctl: writing queued event %s", msgnames[[gevent type]]);

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
            return YES; /* stop reading ... terp is waiting for reply */

        case PROMPTSAVE:
            [self handleSavePrompt:req->a1];
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
            // NSLog(@"glkctl newwin %d (type %d)", ans->a1, req->a1);
            break;

        case NEWCHAN:
            ans->cmd = OKAY;
            ans->a1 = [_soundHandler handleNewSoundChannel:req->a1];
            break;

        case DELWIN:
            //            NSLog(@"glkctl delwin %d", req->a1);
            if (reqWin) {
                [_windowsToBeRemoved addObject:reqWin];
                [_gwindows removeObjectForKey:@(req->a1)];
                _shouldCheckForMenu = YES;
            } else
                NSLog(@"delwin: No window with name %d", req->a1);

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
                                            from:[NSString stringWithCString:buf encoding:NSUTF8StringEncoding]
                                          offset:(NSUInteger)req->a2
                                          length:(NSUInteger)req->a3];
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
                                            from:[NSString stringWithCString:buf encoding:NSUTF8StringEncoding]
                                          offset:(NSUInteger)req->a2
                                          length:(NSUInteger)req->a3];
            break;

        case SETVOLUME:
            [_soundHandler handleSetVolume:req->a2
                                   channel:req->a1
                                  duration:req->a3
                                    notify:req->a4];
            break;

        case PLAYSOUND:
            [_soundHandler handlePlaySoundOnChannel:req->a1 repeats:req->a2 notify:req->a3];
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
                    //For Bureaucracy form accessibility
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
        case SIZWIN:
            if (reqWin) {
                uint x0, y0, x1, y1, checksumWidth, checksumHeight;
                NSRect rect;

                struct sizewinrect *sizewin = (void*)buf;

                checksumWidth = sizewin->gamewidth;
                checksumHeight = sizewin->gameheight;

                if (fabs(checksumWidth - _contentView.frame.size.width) > 1.0) {
                    //                    NSLog(@"handleRequest sizwin: wrong checksum width (%d). "
                    //                          @"Current _contentView width is %f",
                    //                          checksumWidth, _contentView.frame.size.width);
                    break;
                }

                if (fabs(checksumHeight - _contentView.frame.size.height) > 1.0) {
                    //                    NSLog(@"handleRequest sizwin: wrong checksum height (%d). "
                    //                          @"Current _contentView height is %f",
                    //                          checksumHeight, _contentView.frame.size.height);
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

                //                NSLog(@"glkctl SIZWIN %ld: %@", (long)reqWin.name, NSStringFromRect(rect));

                reqWin.frame = rect;

                NSAutoresizingMaskOptions hmask = NSViewMaxXMargin;
                NSAutoresizingMaskOptions vmask = NSViewMaxYMargin;

                if (fabs(NSMaxX(rect) - _contentView.frame.size.width) < 2.0 &&
                    rect.size.width > 0) {
                    // If window is at right edge, attach to that edge
                    hmask = NSViewWidthSizable;
                }

                if (fabs(NSMaxY(rect) - _contentView.frame.size.height) < 2.0 &&
                    rect.size.height > 0) {
                    // If window is at bottom, attach to bottom
                    vmask = NSViewHeightSizable;
                }

                reqWin.autoresizingMask = hmask | vmask;

                windowdirty = YES;
            } else
                NSLog(@"sizwin: something went wrong.");

            break;

        case CLRWIN:
            if (reqWin) {
                // NSLog(@"glkctl: CLRWIN %d.", req->a1);
                [reqWin clear];
                _shouldCheckForMenu = YES;
            }
            break;

        case SETBGND:
            //            NSLog(@"glkctl: SETBGND %d, color %x (%d).", req->a1, req->a2, req->a2);
            if (req->a2 < 0)
                bg = theme.bufferBackground;
            else
                bg = [NSColor colorFromInteger:req->a2];
            if (req->a1 == -1) {
                _lastAutoBGColor = bg;
                [self setBorderColor:bg];
            }

            if (reqWin) {
                [reqWin setBgColor:req->a2];
            }
            break;

        case DRAWIMAGE:
            if (reqWin) {
                NSImage *lastimage = _imageHandler.lastimage;
                if (lastimage && lastimage.size.width > 0 && lastimage.size.height > 0) {
                    struct drawrect *drawstruct = (void *)buf;
                    [reqWin drawImage:lastimage
                                 val1:(glsi32)drawstruct->x
                                 val2:(glsi32)drawstruct->y
                                width:drawstruct->width
                               height:drawstruct->height
                                style:drawstruct->style];
                }
            }
            break;

        case FILLRECT:
            if (reqWin) {
                NSInteger realcount = req->len / sizeof(struct fillrect);
                if (realcount == req->a2) {
                    [reqWin fillRects:(struct fillrect *)buf count:req->a2];
                }
            }
            break;

        case PRINT:
            if (!_gwindows.count && shouldRestoreUI) {
                //                NSLog(@"Restoring UI at PRINT");
                //                NSLog(@"at eventcount %ld", _eventcount);
                _windowsToRestore = restoredControllerLate.gwindows.allValues;
                [self restoreUI];
                reqWin = _gwindows[@(req->a1)];
            }
            if (reqWin) {
                [self handlePrintOnWindow:reqWin
                                    style:(NSUInteger)req->a2
                                   buffer:(unichar *)buf
                                   length:req->len / sizeof(unichar)];
            }
            break;

        case UNPRINT:
            if (!_gwindows.count && shouldRestoreUI) {
                _windowsToRestore = restoredControllerLate.gwindows.allValues;
                //                NSLog(@"Restoring UI at UNPRINT");
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
            //NSLog(@"glkctl: WEE! WE GOT A FLOWBREAK! ^^;");
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
            // NSLog(@"glkctl INITLINE %d", req->a1);
            [self performScroll];

            if (!_gwindows.count && shouldRestoreUI) {
                buf = "\0";
                //              NSLog(@"Restoring UI at INITLINE");
                //              NSLog(@"at eventcount %ld", _eventcount);
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
            if (reqWin && !_colderLight && !skipNextScriptCommand) {

                [reqWin initLine:[NSString stringWithCharacters:(unichar *)buf length:(NSUInteger)req->len / sizeof(unichar)] maxLength:(NSUInteger)req->a2];

                if (_commandScriptRunning) {
                    [self.commandScriptHandler sendCommandLineToWindow:reqWin];
                }

                _shouldSpeakNewText = YES;

                // Check if we are in Beyond Zork Definitions menu
                if (_beyondZork)
                    _shouldCheckForMenu = YES;
            }
            skipNextScriptCommand = NO;
            break;

        case CANCELLINE:
            //            NSLog(@"glkctl CANCELLINE %d", req->a1);
            ans->cmd = OKAY;
            if (reqWin) {
                NSString *str = [reqWin cancelLine];
                ans->len = str.length * sizeof(unichar);
                if (ans->len > GLKBUFSIZE)
                    ans->len = GLKBUFSIZE;
                [str getCharacters:(unsigned short *)buf
                             range:NSMakeRange(0, str.length)];
            }
            break;

        case INITCHAR:
            //            NSLog(@"glkctl initchar %d", req->a1);

            if (!_gwindows.count && shouldRestoreUI) {
                GlkController *g = restoredControllerLate;
                _windowsToRestore = restoredControllerLate.gwindows.allValues;
                //                NSLog(@"Restoring UI at INITCHAR");
                //                NSLog(@"at eventcount %ld", _eventcount);
                if (g.commandScriptRunning) {
                    CommandScriptHandler *handler = restoredController.commandScriptHandler;
                    handler.commandIndex++;
                    if (handler.commandIndex >= handler.commandArray.count) {
                        restoredController.commandScriptHandler = nil;
                        restoredControllerLate.commandScriptHandler = nil;
                    } else {
                        restoredControllerLate.commandScriptHandler = handler;

                        skipNextScriptCommand = YES;
                        lastKeyTimestamp = [NSDate date];
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
            if (lastRequest == PRINT || lastRequest == SETZCOLOR || lastRequest == NEXTEVENT || lastRequest == MOVETO) {
                // This flag may be set by GlkBufferWindow as well
                _shouldScrollOnCharEvent = YES;
                _shouldSpeakNewText = YES;
                _shouldCheckForMenu = YES;
            }

            if (_shouldScrollOnCharEvent) {
                [self performScroll];
            }

            if (reqWin && !skipNextScriptCommand) {
                [reqWin initChar];
                if (_commandScriptRunning) {
                    if (!_adrianMole || [lastKeyTimestamp timeIntervalSinceNow] < -0.5) {
                        [self.commandScriptHandler sendCommandKeyPressToWindow:reqWin];
                        lastKeyTimestamp = [NSDate date];
                    }
                }
            }
            skipNextScriptCommand = NO;
            break;

        case CANCELCHAR:
            //            NSLog(@"glkctl CANCELCHAR %d", req->a1);
            if (reqWin)
                [reqWin cancelChar];
            break;

        case INITMOUSE:
            //            NSLog(@"glkctl initmouse %d", req->a1);
            if (!_gwindows.count && shouldRestoreUI) {
                _windowsToRestore = restoredControllerLate.gwindows.allValues;
                //                NSLog(@"Restoring UI at INITMOUSE");
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
            //            NSLog(@"glkctl set hyperlink %d in window %d", req->a2,
            //            req->a1);
            if (reqWin) {
                reqWin.currentHyperlink = req->a2;
            }
            break;

        case INITLINK:
            //            NSLog(@"glkctl request hyperlink event in window %d",
            //            req->a1);
            //            if (!_gwindows.count && shouldRestoreUI) {
            //                //                NSLog(@"Restoring UI at INITLINK");
            //                //                NSLog(@"at eventcount %ld", _eventcount);
            //                _windowsToRestore = restoredControllerLate.gwindows.allValues;
            //                [self restoreUI];
            //                reqWin = _gwindows[@(req->a1)];
            //            }
            [self performScroll];
            if (reqWin) {
                [reqWin initHyperlink];
                _shouldSpeakNewText = YES;
            }
            break;

        case CANCELLINK:
            //            NSLog(@"glkctl cancel hyperlink event in window %d",
            //            req->a1);
            if (reqWin) {
                [reqWin cancelHyperlink];
            }
            break;

        case TIMER:
            [self handleSetTimer:(NSUInteger)req->a1];
            break;

            /*
             * Hugo specifics (hugo doesn't use glk to arrange windows)
             */

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
                ans->a1 = (int)[banner numberOfColumns];
            }
            break;

            // Used by Tads 3 to adapt the banner height.
        case BANNERLINES:
            ans->cmd = OKAY;
            ans->a1 = 0;
            if (reqWin && [reqWin isKindOfClass:[GlkTextBufferWindow class]] ) {
                GlkTextBufferWindow *banner = (GlkTextBufferWindow *)reqWin;
                ans->a1 = (int)[banner numberOfLines];
            }
            break;

            /*
             * HTML-TADS specifics will go here.
             */

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
    NSLog(@"glkctl: noteTaskDidTerminate");

    dead = YES;
    restartingAlready = NO;

    if (timer) {
        [timer invalidate];
        timer = nil;
    }

    [self flushDisplay];
    [task waitUntilExit];

    if (task && task.terminationStatus != 0 ) {
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = NSLocalizedString(@"The game has unexpectedly terminated.", nil);
        alert.informativeText = [NSString stringWithFormat:NSLocalizedString(@"Error code: %@.", nil), signalToName(task)];
        if (_pendingErrorMessage)
            alert.informativeText = _pendingErrorMessage;
        _pendingErrorMessage = nil;
        _mustBeQuiet = YES;
        [alert addButtonWithTitle:NSLocalizedString(@"Oops", nil)];
        [alert beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse returnCode) {}];
    } else {
        NotificationBezel *bezel = [[NotificationBezel alloc] initWithScreen:self.window.screen];
        [bezel showGameOver];
        [self performSelector:@selector(speakString:) withObject:[NSString stringWithFormat:@"%@ has finished.", _game.metadata.title] afterDelay:1];
    }

    for (GlkWindow *win in [_gwindows allValues])
        [win terpDidStop];

    self.window.title = [self.window.title stringByAppendingString:NSLocalizedString(@" (finished)", nil)];
    [self performScroll];
    task = nil;

    // We autosave the UI but delete the terp autosave files
    if (!restartingAlready)
        [self autoSaveOnExit];

    [self deleteFiles:@[ [NSURL fileURLWithPath:self.autosaveFileTerp],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave.glksave"]],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-GUI.plist"]],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-tmp.glksave"]],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-tmp.plist"]] ]];
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

        //        NSLog(@"GlkController queueEvent: %@",newArrangeValues);

        if (!gevent.forced && [lastArrangeValues isEqualToDictionary:newArrangeValues]) {
            return;
        }

        lastArrangeValues = newArrangeValues;
        // Some Inform 7 games only resize graphics on evtype_Redraw
        redrawEvent = [[GlkEvent alloc] initRedrawEvent];
    }
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

- (void)noteDataAvailable: (id)sender
{
    struct message request;
    struct message reply;
    char minibuf[GLKBUFSIZE + 1];
    char *maxibuf;
    char *buf;
    ssize_t n, t;
    BOOL stop;

    int readfd = [readfh fileDescriptor];
    int sendfd = [sendfh fileDescriptor];

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
    //         NSLog(@"setBorderColor %@ fromWindow %ld", color, aWindow.name);
    NSSize windowsize = aWindow.bounds.size;
    if (aWindow.framePending)
        windowsize = aWindow.pendingFrame.size;
    CGFloat relativeSize = (windowsize.width * windowsize.height) / (_contentView.bounds.size.width * _contentView.bounds.size.height);
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
    if (_narcolepsy && theme.doStyles && theme.doGraphics) {
        //        self.bgcolor = [NSColor clearColor];
        _borderView.layer.backgroundColor = CGColorGetConstantColor(kCGColorClear);
        return;
    }
    //    NSLog(@"GlkController setBorderColor: %@", color);
    if (theme.doStyles || [color isEqualToColor:theme.bufferBackground] || [color isEqualToColor:theme.gridBackground] || theme.borderBehavior == kUserOverride) {
        CGFloat components[[color numberOfComponents]];
        CGColorSpaceRef colorSpace = [[color colorSpace] CGColorSpace];
        [color getComponents:(CGFloat *)&components];
        CGColorRef cgcol = CGColorCreate(colorSpace, components);

        _borderView.layer.backgroundColor = cgcol;
        //        self.window.backgroundColor = color;
        CFRelease(cgcol);

        [Preferences instance].borderColorWell.color = color;
    }
}


- (GlkWindow *)largestWindow {
    GlkWindow *largestWin = nil;
    CGFloat largestSize = 0;

    NSArray *winArray = [_gwindows allValues];

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
        if (winarea > largestSize) {
            largestSize = winarea;
            largestWin = win;
        }
    }

    return largestWin;
}

- (void)noteManagedObjectContextDidChange:(NSNotification *)notification {
    NSArray *updatedObjects = (notification.userInfo)[NSUpdatedObjectsKey];

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

    _contentView.autoresizingMask = NSViewHeightSizable | NSViewMinXMargin | NSViewMaxXMargin;
    _borderView.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;

    if (!inFullScreenResize) {
        NSSize borderSize = _borderView.frame.size;
        NSRect contentFrame = _contentView.frame;
        CGFloat midWidth = borderSize.width / 2;
        if (contentFrame.origin.x > midWidth ||  NSMaxX(contentFrame) < midWidth) {
            contentFrame.origin.x = round(borderSize.width - NSWidth(contentFrame) / 2);
            _contentView.frame = contentFrame;
        }
        if (NSWidth(contentFrame) > borderSize.width - 2 * _theme.border || borderSize.width < borderSize.height) {
            contentFrame.size.width = ceil(borderSize.width - 2 * _theme.border);
            contentFrame.origin.x = _theme.border;
            _contentView.frame = contentFrame;
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
    // Save the window frame so that it can be restored later.

    // If we are starting up in fullscreen, we should use the
    // autosaved windowPreFullscreenFrame instead
    if (!(windowRestoredBySystem && _inFullscreen)) {
        _windowPreFullscreenFrame = self.window.frame;
    }
    windowRestoredBySystem = NO;
    _inFullscreen = YES;
    [self storeScrollOffsets];
    _ignoreResizes = YES;
    // _ignoreResizes means no storing scroll offsets,
    // but also no arrange events
}

- (void)windowDidFailToEnterFullScreen:(NSWindow *)window {
    _inFullscreen = NO;
    _ignoreResizes = NO;
    inFullScreenResize = NO;
    _contentView.alphaValue = 1;
    [window setFrame:_windowPreFullscreenFrame display:YES];
    _contentView.frame = [self contentFrameForWindowed];
    [self restoreScrollOffsets];
    _contentView.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
    _borderView.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
}

- (void)storeScrollOffsets {
    if (_ignoreResizes)
        return;
    for (GlkWindow *win in [_gwindows allValues])
        if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
            ((GlkTextBufferWindow *)win).pendingScrollRestore = NO;
            [(GlkTextBufferWindow *)win storeScrollOffset];
            ((GlkTextBufferWindow *)win).pendingScrollRestore = YES;
        }
}

- (void)restoreScrollOffsets {
    for (GlkWindow *win in [_gwindows allValues])
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
    window.styleMask = (window.styleMask | NSFullScreenWindowMask);

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
    snapshotWindow.styleMask = (snapshotWindow.styleMask | NSFullScreenWindowMask);
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
    _contentView.autoresizingMask = NSViewMinXMargin | NSViewMaxXMargin |
    NSViewMinYMargin; // Attached at top but not bottom or sides

    NSView *localContentView = _contentView;
    NSView *localBorderView = _borderView;
    NSWindow *localSnapshot = snapshotController.window;

    GlkController * __unsafe_unretained weakSelf = self;
    // Hide contentview
    _contentView.alphaValue = 0;

    // Our animation will be broken into five steps.
    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        // First, we move the window to the center
        // of the screen with the snapshot window on top
        context.duration = duration / 3;
        [[localSnapshot animator] setFrame:centerWindowFrame display:YES];
        [[window animator] setFrame:centerWindowFrame display:YES];
    }
     completionHandler:^{
        [NSAnimationContext
         runAnimationGroup:^(NSAnimationContext *context) {
            // and then we enlarge it to its full size.
            context.duration = duration * 2 / 3;
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
                context.duration = duration / 5;
                [localContentView setFrame:newContentFrame];
                [weakSelf sendArrangeEventWithFrame:localContentView.frame force:NO];
                [weakSelf flushDisplay];
            }
             completionHandler:^{
                // Now we can fade out the snapshot window
                localContentView.alphaValue = 1;

                [NSAnimationContext
                 runAnimationGroup:^(NSAnimationContext *context) {
                    context.duration = duration / 10;
                    [[localSnapshot.contentView animator] setAlphaValue:0];
                }
                 completionHandler:^{
                    // Finally, we extend the content view vertically if needed.
                    [NSAnimationContext
                     runAnimationGroup:^(NSAnimationContext *context) {
                        context.duration = duration / 7;
                        [[localContentView animator]
                         setFrame:[weakSelf contentFrameForFullscreen]];
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

    NSRect centerWindowFrame = _windowPreFullscreenFrame;
    centerWindowFrame.origin = NSMakePoint(screen.frame.origin.x + floor((screen.frame.size.width -
                                                                          _borderView.frame.size.width) /
                                                                         2), screen.frame.origin.x + _borderFullScreenSize.height
                                           - centerWindowFrame.size.height);

    centerWindowFrame.origin.x += screen.frame.origin.x;
    centerWindowFrame.origin.y += screen.frame.origin.y;

    GlkController * __unsafe_unretained weakSelf = self;

    BOOL stashShouldShowAlert = shouldShowAutorestoreAlert;
    shouldShowAutorestoreAlert = NO;

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
            [strongSelf enableArrangementEvents];
            [strongSelf sendArrangeEventWithFrame:[strongSelf contentFrameForFullscreen] force:NO];

            if (stashShouldShowAlert && strongSelf)
                [strongSelf performSelector:@selector(showAutorestoreAlert:) withObject:nil afterDelay:0.1];
            [strongSelf restoreScrollOffsets];
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
    NSRect oldFrame = _windowPreFullscreenFrame;

    oldFrame.size.width =
    _contentView.frame.size.width + _theme.border * 2;

    inFullScreenResize = YES;

    _contentView.autoresizingMask =
    NSViewMinXMargin | NSViewMaxXMargin | NSViewMinYMargin;

    NSWindow __unsafe_unretained *localWindow = self.window;
    NSView __weak *localBorderView = _borderView;
    NSView __weak *localContentView =_contentView;
    GlkController * __unsafe_unretained weakSelf = self;

    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        // Make sure the window style mask does not
        // include full screen bit
        context.duration = duration;
        [window
         setStyleMask:(NSUInteger)([window styleMask] & ~(NSUInteger)NSFullScreenWindowMask)];
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
    if (_contentView.frame.size.width < 200)
        [self adjustContentView];
    [self contentDidResize:_contentView.frame];
    lastSizeInChars = [self contentSizeToCharCells:_contentView.frame.size];
    _contentView.autoresizingMask = NSViewHeightSizable | NSViewMinXMargin | NSViewMaxXMargin;
    [self restoreScrollOffsets];
}

- (void)windowDidExitFullScreen:(NSNotification *)notification {
    _ignoreResizes = NO;
    _inFullscreen = NO;
    inFullScreenResize = NO;
    [self contentDidResize:_contentView.frame];
    [self restoreScrollOffsets];
    if (self.narcolepsy && _theme.doGraphics && _theme.doStyles) {
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
    _contentView.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
}

- (void)startInFullscreen {
    // First we show the game windowed
    [self.window setFrame:restoredControllerLate.windowPreFullscreenFrame
                  display:NO];
    [self showWindow:nil];
    [self.window makeKeyAndOrderFront:nil];

    _contentView.frame = [self contentFrameForWindowed];

    _contentView.autoresizingMask = NSViewMinXMargin | NSViewMaxXMargin |
    NSViewMinYMargin; // Attached at top but not bottom or sides

    [self contentDidResize:_contentView.frame];
    for (GlkWindow *win in [_gwindows allValues])
        if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
            GlkTextBufferWindow *bufwin = (GlkTextBufferWindow *)win;
            [bufwin restoreScrollBarStyle];
            // This will prevent storing scroll
            // positions during fullscreen animation
            bufwin.pendingScrollRestore = YES;
        }
}

- (void)deferredEnterFullscreen:(id)sender {
    [self.window toggleFullScreen:nil];
    if (self->shouldShowAutorestoreAlert) {
        self->shouldShowAutorestoreAlert = NO;
        [self performSelector:@selector(showAutorestoreAlert:) withObject:nil afterDelay:1];
    }
}

- (CALayer *)takeSnapshot {
    [self showWindow:nil];
    CGImageRef windowSnapshot = CGWindowListCreateImage(CGRectNull,
                                                        kCGWindowListOptionIncludingWindow,
                                                        (CGWindowID)[self.window windowNumber],
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
    if ((self.window.styleMask & NSFullScreenWindowMask) == NSFullScreenWindowMask ||
        _borderView.frame.size.width == self.window.screen.frame.size.width || (dead && _inFullscreen && windowRestoredBySystem)) {
        // We are in fullscreen
        frame = [self contentFrameForFullscreen];
    } else {
        // We are not in fullscreen
        frame = [self contentFrameForWindowed];
    }
    if (frame.size.width < kMinimumWindowWidth)
        frame.size.width = kMinimumWindowWidth;
    if (frame.size.height < kMinimumWindowHeight)
        frame.size.width = kMinimumWindowHeight;

    NSRect windowframe = self.window.frame;
    if (windowframe.size.width < kMinimumWindowWidth)
        windowframe.size.width = kMinimumWindowWidth;
    if (windowframe.size.height < kMinimumWindowHeight)
        windowframe.size.width = kMinimumWindowHeight;
    if (!NSEqualRects(self.window.frame, windowframe))
        [self.window setFrame:windowframe display:YES];

    _contentView.frame = frame;
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
                             NSWidth(_contentView.frame)) / 2),
                      border, NSWidth(_contentView.frame),
                      round(NSHeight(_borderView.bounds) - border * 2));
}

#pragma mark Accessibility

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    if (!_voiceOverActive && (menuItem.action == @selector(speakMostRecent:) || menuItem.action == @selector(speakPrevious:) || menuItem.action == @selector(speakNext:) || menuItem.action == @selector(speakStatus:))) {
        return NO;
    }
    if (menuItem.action == @selector(saveAsRTF:)) {
        for (GlkWindow *win in _gwindows.allValues) {
            if ([win isKindOfClass:[GlkTextGridWindow class]] || [win isKindOfClass:[GlkTextBufferWindow class]])
                return YES;
        }
        return NO;
    }
    return YES;
}

- (BOOL)isAccessibilityElement {
    return NO;
}

- (NSArray *)accessibilityCustomActions API_AVAILABLE(macos(10.13)) {
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

- (void)noteAccessibilityStatusChanged:(NSNotification *)notify {
    if(@available(macOS 10.13, *)) {
        NSWorkspace * ws = [NSWorkspace sharedWorkspace];
        _voiceOverActive = ws.voiceOverEnabled;
        if (_voiceOverActive) {
            if (_eventcount > 2 && !_mustBeQuiet) {
                [self checkZMenu];
                if (_zmenu) {
                    [_zmenu performSelector:@selector(deferredSpeakSelectedLine:) withObject:nil afterDelay:1];
                } else {
                    GlkWindow *largest = [self largestWithMoves];
                    if (largest) {
                        [largest setLastMove];
                        [largest performSelector:@selector(repeatLastMove:) withObject:nil afterDelay:2];
                    }
                }
            }
        } else {
            _zmenu = nil;
            _form = nil;
        }
    }
}

- (void)deferredSpeakLargest:(id)sender {
    [self speakLargest:_gwindows.allValues];
}

- (IBAction)saveAsRTF:(id)sender {
    GlkWindow *largest = [self largestWithMoves];
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

- (void)checkZMenu {
    if (!_voiceOverActive || _mustBeQuiet || !_theme.vOSpeakMenu)
        return;
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
        if (!_zmenu && _bureaucracy) {
            if (!_form) {
                _form = [[BureaucracyForm alloc] initWithGlkController:self];
            }
            if (![_form isForm]) {
                [NSObject cancelPreviousPerformRequestsWithTarget:_form];
                _form.glkctl = nil;
                _form = nil;
            }
            if (_form) {
                [_form speakCurrentField];
            }
        }
    }
    if (_zmenu) {
        for (GlkTextBufferWindow *view in _gwindows.allValues) {
            if ([view isKindOfClass:[GlkTextBufferWindow class]])
                [view setLastMove];
        }
        [_zmenu speakSelectedLine];
    }
}

#pragma mark Speak new text

- (void)speakNewText {
    // Find a "main text window"
    NSMutableArray *windowsWithText = _gwindows.allValues.mutableCopy;
    if (_quoteBoxes.count)
        [windowsWithText addObject:_quoteBoxes.lastObject];
    for (GlkWindow *view in _gwindows.allValues) {
        if ([view isKindOfClass:[GlkGraphicsWindow class]] || ![(GlkTextBufferWindow *)view setLastMove]) {
            // Remove all Glk window objects with no new text to speak
            [windowsWithText removeObject:view];
        }
    }

    if (!windowsWithText.count) {
        //        NSLog(@"speakNewText: No windows with new text!");
        return;
    } else if (windowsWithText.count > 1) {
        NSMutableArray *bufWinsWithText = [[NSMutableArray alloc] init];
        for (GlkWindow *view in windowsWithText)
            if ([view isKindOfClass:[GlkTextBufferWindow class]])
                [bufWinsWithText addObject:view];
        if (bufWinsWithText.count == 1) {
            [self speakLargest:bufWinsWithText];
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
    if (largest)
    {
        if (largest != _spokeLast && [_speechTimeStamp timeIntervalSinceNow]  > -0.5)
            return;
        _speechTimeStamp = [NSDate date];
        _spokeLast = largest;
        [largest performSelector:@selector(repeatLastMove:) withObject:nil afterDelay:0.1];
    }
}

#pragma mark Speak previous moves

- (IBAction)speakMostRecent:(id)sender {
    if (_zmenu) {
        [_zmenu deferredSpeakSelectedLine:self];
        return;
    }
    if (_form) {
        [_form deferredSpeakCurrentField:self];
        return;
    }
    GlkWindow *mainWindow = [self largestWithMoves];
    if (!mainWindow) {
        if (sender != self)
            [self speakString:@"No last move to speak!"];
        return;
    }
    [mainWindow repeatLastMove:nil];
}

- (IBAction)speakPrevious:(id)sender {
    //    NSLog(@"GlkController: speakPrevious");
    GlkWindow *mainWindow = [self largestWithMoves];
    if (!mainWindow) {
        [self speakString:@"No previous move to speak!"];
        return;
    }
    [mainWindow speakPrevious];
}

- (IBAction)speakNext:(id)sender {
    //    NSLog(@"GlkController: speakNext");
    GlkWindow *mainWindow = [self largestWithMoves];
    if (!mainWindow) {
        [self speakString:@"No next move to speak!"];
        return;
    }
    [mainWindow speakNext];
}

- (IBAction)speakStatus:(id)sender {
    GlkWindow *win;

    // Try to find status window to pass this on to
    for (win in _gwindows.allValues) {
        if ([win isKindOfClass:[GlkTextGridWindow class]]) {
            [(GlkTextGridWindow *)win speakStatus];
            return;
        }
    }
    [self speakString:@"No status window found!"];
}

- (void)speakString:(NSString *)string {
    if (!string || string.length == 0 || !_voiceOverActive) {
        return;
    }

    NSString *charSetString = @"\u00A0 >\n_";
    NSCharacterSet *charset = [NSCharacterSet characterSetWithCharactersInString:charSetString];
    string = [string stringByTrimmingCharactersInSet:charset];

    NSDictionary *announcementInfo = @{
        NSAccessibilityPriorityKey : @(NSAccessibilityPriorityHigh),
        NSAccessibilityAnnouncementKey : string
    };

    NSWindow *mainWin = [NSApp mainWindow];

    if (mainWin) {
        NSAccessibilityPostNotificationWithUserInfo(
                                                    mainWin,
                                                    NSAccessibilityAnnouncementRequestedNotification, announcementInfo);
    }
}

- (GlkWindow *)largestWithMoves {
    // Find a "main text window"
    GlkWindow *largest = nil;
    NSMutableArray *windowsWithMoves = _gwindows.allValues.mutableCopy;
    for (GlkWindow *view in _gwindows.allValues) {
        if (!view.moveRanges || !view.moveRanges.count) {
            // Remove all GlkTextBufferWindow objects with no list of previous moves
            if (!_quoteBoxes && ([view isKindOfClass:[GlkTextBufferWindow class]] && ((GlkTextBufferWindow *)view).quoteBox))
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
            NSLog(@"largestWithMoves: No windows with text!");
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

@end
