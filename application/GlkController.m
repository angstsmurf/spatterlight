#import "InfoController.h"
#import "InputTextField.h"
#import "NSString+Categories.h"
#import "Metadata.h"
#import "Compatibility.h"
#import "Game.h"
#import "Theme.h"
#import "ZMenu.h"
#import "GlkStyle.h"
#import "NSColor+integer.h"

#import "main.h"
#include "glkimp.h"

#include <sys/time.h>

#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
fprintf(stderr, "%s\n",                                                    \
[[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String])
#else
#define NSLog(...)
#endif

#define MINTIMER 1 /* The game Transparent needs a timer this frequent */

//static const char *msgnames[] = {
//    "NOREPLY",         "OKAY",             "ERROR",       "HELLO",
//    "PROMPTOPEN",      "PROMPTSAVE",       "NEWWIN",      "DELWIN",
//    "SIZWIN",          "CLRWIN",           "MOVETO",      "PRINT",
//    "UNPRINT",         "MAKETRANSPARENT",  "STYLEHINT",   "CLEARHINT",
//    "STYLEMEASURE",    "SETBGND",          "SETTITLE",    "AUTOSAVE",
//    "RESET",           "TIMER",            "INITCHAR",    "CANCELCHAR",
//    "INITLINE",        "CANCELLINE",       "SETECHO",     "TERMINATORS",
//    "INITMOUSE",       "CANCELMOUSE",      "FILLRECT",    "FINDIMAGE",
//    "LOADIMAGE",       "SIZEIMAGE",        "DRAWIMAGE",   "FLOWBREAK",
//    "BEEP",            "SETLINK",
//    "INITLINK",        "CANCELLINK",       "SETZCOLOR",   "SETREVERSE",
//    "NEXTEVENT",       "EVTARRANGE",       "EVTLINE",     "EVTKEY",
//    "EVTMOUSE",        "EVTTIMER",         "EVTHYPER",    "EVTSOUND",
//    "EVTVOLUME",       "EVTPREFS"};

//static const char *wintypenames[] = {"wintype_AllTypes", "wintype_Pair",
//    "wintype_Blank",    "wintype_TextBuffer",
//    "wintype_TextGrid", "wintype_Graphics"};

// static const char *stylenames[] =
//{
//    "style_Normal", "style_Emphasized", "style_Preformatted", "style_Header",
//    "style_Subheader", "style_Alert", "style_Note", "style_BlockQuote",
//    "style_Input", "style_User1", "style_User2", "style_NUMSTYLES"
//};
//
// static const char *stylehintnames[] =
//{
//    "stylehint_Indentation", "stylehint_ParaIndentation",
//    "stylehint_Justification", "stylehint_Size",
//    "stylehint_Weight","stylehint_Oblique", "stylehint_Proportional",
//    "stylehint_TextColor", "stylehint_BackColor", "stylehint_ReverseColor",
//    "stylehint_NUMHINTS"
//};

@interface TempLibrary : NSObject {
}

@property glui32 autosaveTag;
@end

@implementation TempLibrary

- (id) initWithCoder:(NSCoder *)decoder {
    int version = [decoder decodeIntForKey:@"version"];
    if (version <= 0 || version > AUTOSAVE_SERIAL_VERSION)
    {
        NSLog(@"Interpreter autosave file:Wrong serial version!");
        return nil;
    }

    // We don't worry about glkdelegate or the dispatch hooks. (Because this will only be used through updateFromLibrary). Similarly, none of the windows need stylesets yet, and none of the file streams are really open.

    _autosaveTag = (glui32)[decoder decodeInt32ForKey:@"autosaveTag"];

    return self;
}
@end

@implementation GlkHelperView

- (BOOL)isFlipped {
    return YES;
}

- (BOOL)isOpaque {
    return YES;
}

- (void)setFrame:(NSRect)frame {
//    NSLog(@"GlkHelperView (_contentView) setFrame: %@ Previous frame: %@",
//          NSStringFromRect(frame), NSStringFromRect(self.frame));

    super.frame = frame;

    if ([_glkctrl isAlive] && !self.inLiveResize && !_glkctrl.ignoreResizes) {
        [_glkctrl contentDidResize:frame];
    }
}

- (void)viewWillStartLiveResize {
    if ((_glkctrl.window.styleMask & NSFullScreenWindowMask) !=
        NSFullScreenWindowMask && !_glkctrl.ignoreResizes)
        [_glkctrl storeScrollOffsets];
}

- (void)viewDidEndLiveResize {
    // We use a custom fullscreen width, so don't resize to full screen width
    // when viewDidEndLiveResize is called because we just entered fullscreen
    if ((_glkctrl.window.styleMask & NSFullScreenWindowMask) !=
        NSFullScreenWindowMask && !_glkctrl.ignoreResizes) {
        [_glkctrl contentDidResize:self.frame];
        [_glkctrl restoreScrollOffsets];
    }
}

@end

@implementation GlkController

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

    NSLog(@"glkctl: runterp %@ %@", terpname_, game_.metadata.title);

    // We could use separate versioning for GUI and interpreter autosaves,
    // but it is probably simpler this way
    _autosaveVersion = AUTOSAVE_SERIAL_VERSION;

    _ignoreResizes = YES;

    _game = game_;
    _theme = _game.theme;

    libcontroller = ((AppDelegate *)[NSApplication sharedApplication].delegate).libctl;

    if (!_theme.name) {
        NSLog(@"GlkController runTerp called with theme without name!");
        _game.theme = [Preferences currentTheme];
        _theme = _game.theme;
    }

//    if ([[_game.ifid substringToIndex:9] isEqualToString:@"LEVEL9-00"])
//        _adrianMole = YES;
    if ([_game.ifid isEqualToString:@"303E9BDC-6D86-4389-86C5-B8DCF01B8F2A"])
         _deadCities = YES;

    if ([_game.ifid isEqualToString:@"ZCODE-86-870212"] || [_game.ifid isEqualToString:@"ZCODE-116-870602"] || [_game.ifid isEqualToString:@"ZCODE-160-880521"])
        _bureaucracy = YES;

    if ([_game.ifid isEqualToString:@"AC0DAF65-F40F-4A41-A4E4-50414F836E14"])
        _kerkerkruip = YES;

    if ([_game.ifid isEqualToString:@"ZCODE-47-870915"] || [_game.ifid isEqualToString:@"ZCODE-49-870917"] || [_game.ifid isEqualToString:@"ZCODE-51-870923"] || [_game.ifid isEqualToString:@"ZCODE-57-871221"] || [_game.ifid isEqualToString:@"ZCODE-60-880610"])
        _beyondZork = YES;

    if ([_game.ifid isEqualToString:@"BFDE398E-C724-4B9B-99EB-18EE4F26932E"])
        _colderLight = YES;

    if ([_game.ifid isEqualToString:@"afb163f4-4d7b-0dd9-1870-030f2231e19f"])
        _thaumistry = YES;

    _gamefile = [_game urlForBookmark].path;
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
    _shouldSpeakNewText = YES;

    _supportsAutorestore = [self.window isRestorable];
    _game.autosaved = _supportsAutorestore;
    windowRestoredBySystem = windowRestoredBySystem_;

    shouldShowAutorestoreAlert = NO;
    shouldRestoreUI = NO;
    turns = 0;
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

    self.window.title = _game.metadata.title;
    if (NSAppKitVersionNumber >= NSAppKitVersionNumber10_12) {
        [self.window setValue:@2 forKey:@"tabbingMode"];
    }

    waitforevent = NO;
    waitforfilename = NO;
    dead = YES; // This should be YES until the interpreter process is running

    _contentView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    windowdirty = NO;

    lastimageresno = -1;
    lastimage = nil;

    _ignoreResizes = NO;
    _shouldScrollOnCharEvent = NO;

    // If we are resetting, there is a bunch of stuff that we have already done
    // and we can skip
    if (shouldReset) {
        if (!_inFullscreen) {
            _windowPreFullscreenFrame = self.window.frame;
        }
        [self adjustContentView];
        [self forkInterpreterTask];
        return;
    }

    if (!imageCache)
        imageCache = [[NSCache alloc] init];

    lastContentResize = NSZeroRect;
    _inFullscreen = NO;
    _windowPreFullscreenFrame = self.window.frame;
    borderFullScreenSize = NSZeroSize;

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
     selector:@selector(noteManagedObjectContextDidChange:)
     name:NSManagedObjectContextObjectsDidChangeNotification
     object:libcontroller.managedObjectContext];

    self.window.representedFilename = _gamefile;

    _borderView.wantsLayer = YES;
    _borderView.layerContentsRedrawPolicy = NSViewLayerContentsRedrawOnSetNeedsDisplay;
    [self setBorderColor:_theme.bufferBackground];

    NSString *autosaveLatePath = [self.appSupportDir
                                  stringByAppendingPathComponent:@"autosave-GUI-late.plist"];

    if (_supportsAutorestore &&
        ([[NSFileManager defaultManager] fileExistsAtPath:self.autosaveFileGUI] || [[NSFileManager defaultManager] fileExistsAtPath:autosaveLatePath])) {
        [self runTerpWithAutorestore];
    } else {
        [self runTerpNormal];
    }
}

- (void)runTerpWithAutorestore {
    NSLog(@"runTerpWithAutorestore");
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
    NSDictionary* attrs = [fileManager attributesOfItemAtPath:self.autosaveFileTerp error:&error];
    if (attrs) {
        terpAutosaveDate = (NSDate*)[attrs objectForKey: NSFileCreationDate];
    } else {
        NSLog(@"Error: %@", error);
    }

    attrs = [fileManager attributesOfItemAtPath:self.autosaveFileGUI error:&error];
    if (attrs) {
        GUIAutosaveDate = (NSDate*)[attrs objectForKey: NSFileCreationDate];
    } else {
        NSLog(@"Error: %@", error);
    }

    if (restoredController.autosaveVersion != AUTOSAVE_SERIAL_VERSION) {
        NSLog(@"GUI autosave file is wrong version! Wanted %ld, got %ld. Deleting!", _autosaveVersion, restoredController.autosaveVersion );
        restoredController = nil;
    }

    if ([terpAutosaveDate compare:GUIAutosaveDate] == NSOrderedDescending) {
        NSLog(@"GUI autosave file is created before terp autosave file! Bailing autorestore!");
        restoredController = nil;
    }

    restoredControllerLate = restoredController;

    NSString *autosaveLatePath = [self.appSupportDir
                              stringByAppendingPathComponent:@"autosave-GUI-late.plist"];

    if ([[NSFileManager defaultManager] fileExistsAtPath:autosaveLatePath]) {
        @try {
            restoredControllerLate =
            [NSKeyedUnarchiver unarchiveObjectWithFile:autosaveLatePath];
        } @catch (NSException *ex) {
            NSLog(@"Unable to restore late GUI autosave: %@", ex);
            restoredControllerLate = restoredController;
        }
    }

    attrs = [fileManager attributesOfItemAtPath:autosaveLatePath error:&error];
    if (attrs) {
        GUILateAutosaveDate = (NSDate*)[attrs objectForKey: NSFileCreationDate];
    } else {
        NSLog(@"Error: %@", error);
    }

    if ([GUIAutosaveDate compare:GUILateAutosaveDate] == NSOrderedDescending) {
        NSLog(@"GUI autosave late file is created before GUI autosave file!");
        restoredControllerLate = restoredController;
    }

    if (restoredController.autosaveTag != restoredControllerLate.autosaveTag) {
        NSLog(@"GUI autosave late tag does not match GUI autosave file tag!");
        NSLog(@"restoredController.autosaveTag %ld restoredControllerLate.autosaveTag: %ld", restoredController.autosaveTag, restoredControllerLate.autosaveTag);
        if (restoredControllerLate.autosaveTag == 0)
            restoredControllerLate = restoredController;
        else
            restoredController = restoredControllerLate;
    }

    if (!restoredController) {
        if (restoredControllerLate) {
            restoredController = restoredControllerLate;
        } else {
        // If there exists an autosave file but we failed to read it,
        // (and also no "GUI-late" autosave)
        // delete it and run game without autorestoring
        [self deleteAutosaveFiles];
        _game.autosaved = NO;
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
            _game.autosaved = NO;
            // If we die in fullscreen and close the game,
            // the game should not open in fullscreen the next time
            _inFullscreen = NO;
            [self runTerpNormal];
            return;
        }
    }

    if ([[NSFileManager defaultManager] fileExistsAtPath:self.autosaveFileTerp]) {
        NSLog(@"Interpreter autorestore file exists");
        restoredUIOnly = NO;

        TempLibrary *tempLib =
        [NSKeyedUnarchiver unarchiveObjectWithFile:self.autosaveFileTerp];
        if (tempLib.autosaveTag != restoredController.autosaveTag) {
            NSLog(@"The terp autosave and the GUI autosave have non-matching tags.");
            NSLog(@"The terp autosave tag: %u GUI autosave tag: %ld", tempLib.autosaveTag, restoredController.autosaveTag);

            NSLog(@"Only restore UI state at first turn");
            [self deleteFiles:@[ [NSURL fileURLWithPath:self.autosaveFileGUI],
                                 [NSURL fileURLWithPath:self.autosaveFileTerp] ]];
            restoredUIOnly = YES;
        } else {
            NSLog(@"The terp autosave tag: %u GUI autosave tag: %ld", tempLib.autosaveTag, restoredController.autosaveTag);

            // Only show the alert about autorestoring if this is not a system
            // window restoration, and the user has not suppressed it.
            if (!windowRestoredBySystem) {
                NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

                if ([defaults boolForKey:@"AutorestoreAlertSuppression"]) {
                    NSLog(@"Autorestore alert suppressed");
                    if (![defaults boolForKey:@"AlwaysAutorestore"]) {
                        // The user has checked "Remember this choice" when
                        // choosing to not autorestore
                        [self deleteAutosaveFiles];
                        _game.autosaved = NO;
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
        _game.autosaved = NO;
        [self runTerpNormal];
        return;
    }

    // If this is not a window restoration done by the system,
    // we now re-enter fullscreen manually if the game was
    // closed in fullscreen mode.
    if (!windowRestoredBySystem && _inFullscreen
        && (self.window.styleMask & NSFullScreenWindowMask) != NSFullScreenWindowMask) {
        [self startInFullscreen];
        _startingInFullscreen = YES;
    } else {
        _contentView.autoresizingMask =
        NSViewMinXMargin | NSViewMaxXMargin | NSViewMinYMargin;
        [self.window setFrame:restoredControllerLate.storedWindowFrame display:YES];
        _contentView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    }

    [self adjustContentView];
    shouldRestoreUI = YES;
    [self forkInterpreterTask];

    // The game has to run to its third(?) NEXTEVENT
    // before we can restore the UI properly, so we don't
    // have to do anything else here for now.
}

- (void)runTerpNormal {
    // Just start the game with no autorestore or fullscreen or resetting
    NSRect newContentFrame = [self.window.contentView frame];
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
    lastSizeInChars = [self contentSizeToCharCells:_contentView.frame.size];
    [self forkInterpreterTask];
    [self showWindow:nil];
}

- (void)restoreWindowWhenDead {
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

    restoredController = nil;
}

- (void)forkInterpreterTask {
    /* Fork the interpreter process */

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

#endif // TEE_TERP_OUTPUT

    GlkController * __unsafe_unretained weakSelf = self;

    [[readpipe fileHandleForReading]
     setReadabilityHandler:^(NSFileHandle *file) {
         NSData *data = [file availableData];
         GlkController *strongSelf = weakSelf;
         if(strongSelf) {
             dispatch_async(dispatch_get_main_queue(), ^{
                 [strongSelf noteDataAvailable:data];
             });
         }
     }];

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

    [task launch];
    dead = NO;

    /* Send a prefs and an arrange event first thing */
    GlkEvent *gevent;

    gevent = [[GlkEvent alloc] initPrefsEventForTheme:_theme];
    [self queueEvent:gevent];

    if (!(_inFullscreen && windowRestoredBySystem)) {
        gevent = [[GlkEvent alloc] initArrangeWidth:(NSInteger)_contentView.frame.size.width
                                             height:(NSInteger)_contentView.frame.size.height
                                              theme:_theme
                                              force:NO];
        [self queueEvent:gevent];
    }

    restartingAlready = NO;
}

#pragma mark Autorestore

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

    // Restore scroll position etc
    for (win in [_gwindows allValues]) {
        if (![win isKindOfClass:[GlkGraphicsWindow class]])
            [win postRestoreAdjustments:(restoredControllerLate.gwindows)[@(win.name)]];
        if (win.name == _firstResponderView) {
            winToGrabFocus = win;
        }
    }

    NSNotification *notification = [NSNotification notificationWithName:@"PreferencesChanged" object:_theme];
    [self notePreferencesChanged:notification];

    if (winToGrabFocus)
        [winToGrabFocus grabFocus];

    if (!restoredUIOnly)
        _hasAutoSaved = YES;

    restoredController = nil;
    restoredControllerLate = nil;
    restoredUIOnly = NO;

    // We create a forced arrange event in order to force the interpreter process
    // to re-send us window sizes. The player may have changed settings that affect
    // window size since the autosave was created.,
    [self performSelector:@selector(sendArrangeEvent:) withObject:nil afterDelay:0];
}

- (void)sendArrangeEvent:(id)sender {
    GlkEvent *gevent = [[GlkEvent alloc] initArrangeWidth:(NSInteger)_contentView.frame.size.width
                                                   height:(NSInteger)_contentView.frame.size.height
                                                    theme:_theme
                                                    force:YES];
    [self queueEvent:gevent];
    _shouldStoreScrollOffset = YES;
    // Now we can actually show the window
    [self showWindow:nil];
    [self.window makeKeyAndOrderFront:nil];
    [self.window makeFirstResponder:nil];
    if (_voiceOverActive) {
        GlkTextBufferWindow *largest = [self largestWithMoves];
        if (largest) {
            [largest performSelector:@selector(deferredSpeakMostRecent:) withObject:nil afterDelay:2];
        }
    }
}

- (NSString *)appSupportDir {
    if (!_appSupportDir) {
        NSDictionary *gFolderMap = @{
                                     @"adrift" : @"SCARE",
                                     @"advsys" : @"AdvSys",
                                     @"agt" : @"AGiliTy",
                                     @"glulx" : @"Glulxe",
                                     @"hugo" : @"Hugo",
                                     @"level9" : @"Level 9",
                                     @"magscrolls" : @"Magnetic",
                                     @"quill" : @"UnQuill",
                                     @"tads2" : @"TADS",
                                     @"tads3" : @"TADS",
                                     @"zcode": @"Bocfel",
                                     //@"zcode" : @"Fizmo"
                                     };

        NSDictionary *gFolderMapExt = @{@"acd" : @"Alan 2", @"a3c" : @"Alan 3", @"d$$" : @"AGiliTy"};

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

        if (!_game.detectedFormat) {
            NSLog(@"GlkController appSupportDir: Game %@ has no specified format!", _game.metadata.title);
            return nil;
        }

        NSString *terpFolder =
        [gFolderMap[_game.detectedFormat]
         stringByAppendingString:@" Files"];

        if (!terpFolder) {
            terpFolder = [gFolderMapExt[_gamefile.pathExtension.lowercaseString]
                          stringByAppendingString:@" Files"];
        }

        if (!terpFolder) {
            NSLog(@"GlkController appSupportDir: Could not map game format %@ to a folder name!", _game.detectedFormat);
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

            NSString *dummyfilename = [_game.metadata.title
                                       stringByAppendingPathExtension:@"txt"];

            NSString *dummytext = [NSString
                                   stringWithFormat:
                                   @"This file, %@, was placed here by Spatterlight in order to make "
                                   @"it easier for humans to guess what game these autosave files belong "
                                   @"to. Any files in this folder are for the game %@, or possibly "
                                   @"a game with a different name but identical contents.",
                                   dummyfilename, _game.metadata.title];

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
    NSLog(@"%@ autoSaveOnExit", _game.metadata.title);
    if (_supportsAutorestore) {
        NSString *autosaveLate = [self.appSupportDir
                                  stringByAppendingPathComponent:@"autosave-GUI-late.plist"];
        NSInteger res = [NSKeyedArchiver archiveRootObject:self
                                                    toFile:autosaveLate];
        if (!res) {
            NSLog(@"GUI autosave on exit failed!");
            return;
        } else _game.autosaved = YES;
    }
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        dead = [decoder decodeBoolForKey:@"dead"];
        waitforevent = NO;
        waitforfilename = NO;

        /* the glk objects */

        _autosaveVersion = [decoder decodeIntegerForKey:@"version"];
        _hasAutoSaved = [decoder decodeBoolForKey:@"hasAutoSaved"];
        _autosaveTag =  [decoder decodeIntegerForKey:@"autosaveTag"];

        _gwindows = [decoder decodeObjectForKey:@"gwindows"];

        _storedWindowFrame = [decoder decodeRectForKey:@"windowFrame"];
        _windowPreFullscreenFrame =
            [decoder decodeRectForKey:@"windowPreFullscreenFrame"];

        _storedContentFrame = [decoder decodeRectForKey:@"contentFrame"];
        _storedBorderFrame = [decoder decodeRectForKey:@"borderFrame"];

        _bgcolor = [decoder decodeObjectForKey:@"backgroundColor"];

        _bufferStyleHints = [decoder decodeObjectForKey:@"bufferStyleHints"];
        _gridStyleHints = [decoder decodeObjectForKey:@"gridStyleHints"];

        lastimage = nil;
        lastimageresno = -1;

        _queue = [decoder decodeObjectForKey:@"queue"];
        _firstResponderView = [decoder decodeIntegerForKey:@"firstResponder"];
        _inFullscreen = [decoder decodeBoolForKey:@"fullscreen"];

        _previewDummy =[decoder decodeBoolForKey:@"previewDummy"];

        restoredController = nil;
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];

    [encoder encodeInteger:_autosaveVersion forKey:@"version"];
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

    [encoder encodeBool:_previewDummy forKey:@"previewDummy"];
}

- (void)showAutorestoreAlert:(id)userInfo {

    shouldShowAutorestoreAlert = NO;

    NSAlert *anAlert = [[NSAlert alloc] init];
    anAlert.messageText =
    NSLocalizedString(@"This game was automatically restored from a previous session.", nil);
    anAlert.informativeText = NSLocalizedString(@"Would you like to start over instead?", nil);
    anAlert.showsSuppressionButton = YES;
    anAlert.suppressionButton.title = NSLocalizedString(@"Remember this choice.", nil);
    [anAlert addButtonWithTitle:NSLocalizedString(@"Continue", nil)];
    [anAlert addButtonWithTitle:NSLocalizedString(@"Restart", nil)];

    [anAlert beginSheetModalForWindow:[self window] completionHandler:^(NSInteger result) {

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
    }];
}

- (IBAction)reset:(id)sender {
    if (restartingAlready)
        return;

    restartingAlready = YES;

    [self handleSetTimer:0];

    if (task) {
//        NSLog(@"glkctl reset: force stop the interpreter");
        task.terminationHandler = nil;
        [task.standardOutput fileHandleForReading].readabilityHandler = nil;
        [task terminate];
        task = nil;
    }

    [self deleteAutosaveFiles];
    [self performSelector:@selector(deferredRestart:) withObject:nil afterDelay:0.1];
}

- (void)deferredRestart:(id)sender {

    for (GlkWindow *win in _gwindows.allValues) {
        [win removeFromSuperview];
    }

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

    NSInteger res = [NSKeyedArchiver archiveRootObject:self
                                                toFile:self.autosaveFileGUI];

    if (!res) {
        NSLog(@"Window serialize failed!");
        return;
    }

    _hasAutoSaved = YES;
     NSLog(@"UI autosaved successfully. Tag: %ld", (long)hash);
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
        [self guessFocus];
        [self noteAccessibilityStatusChanged:nil];
    }
}

- (BOOL)windowShouldClose:(id)sender {
    //    NSLog(@"glkctl: windowShouldClose");
    NSAlert *alert;

    if (dead || _supportsAutorestore) {
        [self windowWillClose:nil];
        return YES;
    }

    if ([[NSUserDefaults standardUserDefaults]
         boolForKey:@"closeAlertSuppression"]) {
        NSLog(@"Window close alert suppressed");
        return YES;
    }
    alert = [[NSAlert alloc] init];
    alert.messageText = NSLocalizedString(@"Do you want to abandon the game?", nil);
    alert.informativeText = NSLocalizedString(@"Any unsaved progress will be lost.", nil);
    alert.showsSuppressionButton = YES; // Uses default checkbox title

    [alert addButtonWithTitle:NSLocalizedString(@"Close", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

    [alert beginSheetModalForWindow:self.window
                  completionHandler:^(NSInteger result) {
                      if (result == NSAlertFirstButtonReturn) {
                          if (alert.suppressionButton.state == NSOnState) {
                              // Suppress this alert from now on
                              [[NSUserDefaults standardUserDefaults]
                               setBool:YES
                               forKey:@"closeAlertSuppression"];
                          }
                          [self windowWillClose:nil];
                          [self close];
                      }
                  }];
    return NO;
}

- (void)windowWillClose:(id)sender {
    NSLog(@"glkctl (game %@): windowWillClose", _game ? _game.metadata.title : @"nil");
    if (windowClosedAlready) {
        NSLog(@"windowWillClose called twice!");
        return;
    } else windowClosedAlready = YES;

    [self autoSaveOnExit];

    if (_game && [Preferences instance].currentGame == _game) {
        Game *remainingGameSession = nil;
        if (libcontroller.gameSessions.count)
            remainingGameSession = ((GlkController *)(libcontroller.gameSessions.allValues)[0]).game;
        NSLog(@"GlkController for game %@ closing. Setting preferences current game to %@", _game.metadata.title, remainingGameSession.metadata.title);
        [Preferences changeCurrentGame:remainingGameSession];
    } else {
        if (_game == nil)
            NSLog(@"GlkController windowWillClose called with _game nil!");
        else
            NSLog(@"GlkController for game %@ closing, but preferences currentGame was not the same", _game.metadata.title);
                  //[Preferences instance].currentGame ? [Preferences instance].currentGame.metadata.title : @"nil");
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
        [task terminate];
        task = nil;
    }

    for (GlkWindow *win in [_gwindows allValues])
    {
        win.glkctl = nil;
    }

    _contentView.glkctrl = nil;
    [libcontroller releaseGlkControllerSoon:self];
    libcontroller = nil;
    //[self.window setDelegate:nil]; This segfaults
}

- (void)flushDisplay {
//    lastFlushTimestamp = [NSDate date];

    if (windowdirty) {
    GlkWindow *largest = [self largestWindow];
    if ([largest isKindOfClass:[GlkTextBufferWindow class]] || [largest isKindOfClass:[GlkTextGridWindow class]])
        [(GlkTextBufferWindow *)largest recalcBackground];
    if ([largest isKindOfClass:[GlkTextGridWindow class]])
        [(GlkTextGridWindow *)largest recalcBackground];
        windowdirty = NO;
    }

    for (GlkWindow *win in _windowsToBeAdded) {
        [_contentView addSubview:win];
    }

    _windowsToBeAdded = [[NSMutableArray alloc] init];

    for (GlkWindow *win in [_gwindows allValues]) {
        [win flushDisplay];
    }

    for (GlkWindow *win in _windowsToBeRemoved) {
        [win removeFromSuperview];
    }

    [self checkZMenu];

    if (_shouldSpeakNewText)
        [self speakNewText];

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
    size.width = round(_theme.cellWidth * _theme.defaultCols + (_theme.gridMarginX + _theme.border + 5.0) * 2.0);
    size.height = round(_theme.cellHeight * _theme.defaultRows + (_theme.gridMarginY + _theme.border) * 2.0);
    return size;
}

- (void)contentDidResize:(NSRect)frame {
    if (NSEqualRects(frame, lastContentResize)) {
        //        NSLog(
        //            @"contentDidResize called with same frame as last time. Skipping.");
        return;
    }

    lastContentResize = frame;
    lastSizeInChars = [self contentSizeToCharCells:_contentView.frame.size];

    if (frame.origin.x < 0 || frame.origin.y < 0 || frame.size.width < 0 || frame.size.height < 0) {
        NSLog(@"contentDidResize: weird new frame: %@", NSStringFromRect(frame));
        return;
    }

    if (!inFullScreenResize && !dead) {
        //        NSLog(@"glkctl: contentDidResize: Sending an arrange event with the "
        //              @"new size (%@)",
        //              NSStringFromSize(frame.size));

        GlkEvent *gevent;
        gevent = [[GlkEvent alloc] initArrangeWidth:(NSInteger)frame.size.width
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

        // If window is partly off the screen, move it (just) inside
        if (NSMaxX(winrect) > NSMaxX(screenframe))
            winrect.origin.x = NSMaxX(screenframe) - NSWidth(winrect);

        NSSize minSize = self.window.minSize;
        if (winrect.size.width < minSize.width)
            winrect.size.width = minSize.width;
        if (winrect.size.height < minSize.height)
            winrect.size.height = minSize.height;

        [self.window setFrame:winrect display:YES];
        _contentView.frame = [self contentFrameForWindowed];
    } else {
//        NSLog(@"zoomContentToSize: we are in fullscreen");
        NSRect newframe = NSMakeRect(oldframe.origin.x, oldframe.origin.y,
                                     newSize.width,
                                     NSHeight(_borderView.frame));

        if (NSWidth(newframe) > NSWidth(_borderView.frame) - borders)
            newframe.size.width = NSWidth(_borderView.frame) - borders;

        newframe.origin.x += (NSWidth(oldframe) - NSWidth(newframe)) / 2;

        CGFloat offset = NSHeight(newframe) - NSHeight(oldframe);
        newframe.origin.y -= offset;

        _contentView.frame = newframe;
    }
    _ignoreResizes = NO;
}


- (NSSize)charCellsToContentSize:(NSSize)cells {
    // Only _contentView, does not take border into account
    NSSize size;
    size.width = round(_theme.cellWidth * cells.width + (_theme.gridMarginX + 5.0) * 2.0);
    size.height = round(_theme.cellHeight * cells.height + (_theme.gridMarginY) * 2.0);
//    NSLog(@"charCellsToContentSize: %@ in character cells is %@ in points", NSStringFromSize(cells), NSStringFromSize(size));
    return size;
}

- (NSSize)contentSizeToCharCells:(NSSize)points {
    // Only _contentView, does not take border into account
    NSSize size;
    size.width = round((points.width - (_theme.gridMarginX + 5.0) * 2.0) / _theme.cellWidth);
    size.height = round((points.height - (_theme.gridMarginY) * 2.0) / _theme.cellHeight);
//    NSLog(@"contentSizeToCharCells: %@ in points is %@ in character cells ", NSStringFromSize(points), NSStringFromSize(size));
    return size;
}

/*
 *
 */

#pragma mark Preference and style hint glue

- (void)notePreferencesChanged:(NSNotification *)notify {

    if (_game && !_previewDummy) {
//        NSLog(@"glkctl notePreferencesChanged called for game %@, currently using theme %@", _game.metadata.title, _game.theme.name);
        _theme = _game.theme;
    } else {
//        NSLog(@"notePreferencesChanged: no game.");
        return;
    }

    if (notify.object != _theme && notify.object != nil) {
//        NSLog(@"glkctl: PreferencesChanged called for a different theme (was %@, listening for %@)", ((Theme *)notify.object).name, _theme.name);
        return;
    } else if ( notify.object == nil) {
//        NSLog(@"glkctl: PreferencesChanged with a nil object.");
    }

//    NSLog(@"GlkController for game %@ notePreferencesChanged", _game.metadata.title);

    _shouldStoreScrollOffset = NO;
    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"AdjustSize"]) {
        if (lastTheme != _theme && !NSEqualSizes(lastSizeInChars, NSZeroSize)) { // Theme changed
            NSSize newContentSize = [self charCellsToContentSize:lastSizeInChars];
            NSUInteger borders = (NSUInteger)_theme.border * 2;
            NSSize newSizeIncludingBorders = NSMakeSize(newContentSize.width + borders, newContentSize.height + borders);

            if (!NSEqualSizes(_borderView.bounds.size, newSizeIncludingBorders)
                || !NSEqualSizes(_contentView.bounds.size, newContentSize)) {
                [self zoomContentToSize:newContentSize];

//                NSLog(@"Changed window size to keep size in char cells constant. Previous size in char cells: %@ Current size in char cells: %@", NSStringFromSize(lastSizeInChars), NSStringFromSize([self contentSizeToCharCells:_contentView.frame.size]));
            }
        }
    }

    lastTheme = _theme;
    lastSizeInChars = [self contentSizeToCharCells:_contentView.frame.size];

    [self adjustContentView];

    if (!_gwindows.count) {
        // No _gwindows, nothing to do.
        return;
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
                                          theme:_theme
                                          force:YES];
    [self queueEvent:gevent];

    gevent = [[GlkEvent alloc] initPrefsEventForTheme:_theme];
    [self queueEvent:gevent];

    for (GlkWindow *win in [_gwindows allValues])
    {
        win.theme = _theme;
        [win prefsDidChange];
    }
    _shouldStoreScrollOffset = YES;
}



#pragma mark Zoom

- (IBAction)zoomIn:(id)sender {
    [Preferences zoomIn];
    if (Preferences.instance)
    [Preferences.instance updatePanelAfterZoom];
}

- (IBAction)zoomOut:(id)sender {
    [Preferences zoomOut];
    if (Preferences.instance)
    [Preferences.instance updatePanelAfterZoom];
}

- (IBAction)zoomToActualSize:(id)sender {
    [Preferences zoomToActualSize];
    if (Preferences.instance)
    [Preferences.instance updatePanelAfterZoom];
}

- (void)noteDefaultSizeChanged:(NSNotification *)notification {

    if (notification.object != _game.theme)
    return;

    NSSize sizeAfterZoom = [self defaultContentSize];
    NSRect oldframe = _contentView.frame;

    // Prevent the window from shrinking when zooming in or growing when
    // zooming out, which might otherwise happen at edge cases
    if ((sizeAfterZoom.width < oldframe.size.width && Preferences.zoomDirection == ZOOMIN) ||
        (sizeAfterZoom.width > oldframe.size.width && Preferences.zoomDirection == ZOOMOUT)) {
        return;
    }

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

        // If window is partly off the screen, move it (just) inside
        if (NSMaxX(winrect) > NSMaxX(screenframe))
        winrect.origin.x = NSMaxX(screenframe) - NSWidth(winrect);

        if (NSMinY(winrect) < 0)
        winrect.origin.y = NSMinY(screenframe);

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

        _contentView.frame = newframe;
        [self contentDidResize:newframe];
    }
}


/*
 *
 */

#pragma mark Glk requests

- (void)handleOpenPrompt:(int)fileusage {
    NSURL *directory =
    [NSURL fileURLWithPath:[[NSUserDefaults standardUserDefaults]
                            objectForKey:@"SaveDirectory"]
               isDirectory:YES];

    NSInteger sendfd = sendfh.fileDescriptor;

    // Create and configure the panel.
    NSOpenPanel *panel = [NSOpenPanel openPanel];

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
                  }];

    waitforfilename = NO; /* we're all done, resume normal processing */

    [readfh waitForDataInBackgroundAndNotify];
}

- (void)handleSavePrompt:(int)fileusage {
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
    [panel setCanCreateDirectories:YES];

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
                  }];

    waitforfilename = NO; /* we're all done, resume normal processing */

    [readfh waitForDataInBackgroundAndNotify];
}

- (NSInteger)handleNewWindowOfType:(NSInteger)wintype andName:(NSInteger)name {
    NSInteger i;

    //    NSLog(@"GlkController handleNewWindowOfType: %s",
    //    wintypenames[wintype]);

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

    if (millisecs > 0) {
        if (millisecs < MINTIMER) {
            NSLog(@"glkctl: too small timer interval (%ld); increasing to %d",
                  (unsigned long)millisecs, MINTIMER);
            millisecs = MINTIMER;
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

- (void)handleLoadImageNumber:(int)resno
                         from:(NSString *)path
                       offset:(NSInteger)offset
                       length:(NSUInteger)length {

    if (lastimageresno == resno && lastimage)
        return;

    lastimage = [imageCache objectForKey:@(resno)];
    if (lastimage) {
        lastimageresno = resno;
        return;
    }

    lastimageresno = -1;

    if (lastimage) {
        lastimage = nil;
    }

    NSFileHandle * fileHandle = [NSFileHandle fileHandleForReadingAtPath:path];

    [fileHandle seekToFileOffset:(unsigned long long)offset];
    NSData *data = [fileHandle readDataOfLength:length];

    if (!data)
        return;

    NSArray *reps = [NSBitmapImageRep imageRepsWithData:data];
    NSImageRep *rep = reps[0];
    NSSize size = NSMakeSize(rep.pixelsWide, rep.pixelsHigh);

    if (size.height == 0 || size.width == 0) {
        NSLog(@"glkctl: image size is zero!");
        return;
    }

    lastimage = [[NSImage alloc] initWithSize:size];

    if (!lastimage) {
        NSLog(@"glkctl: failed to decode image");
        return;
    }

    [lastimage addRepresentations:reps];
    lastimageresno = resno;
    [imageCache setObject:lastimage forKey:@(lastimageresno)];
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

    if ([gwindow getStyleVal:style hint:hint value:result])
        return YES;
    else {
        if (hint == stylehint_TextColor) {
            if ([gwindow isKindOfClass:[GlkTextBufferWindow class]])
                *result = [_theme.bufferNormal.color integerColor];
            else
                *result = [_theme.gridNormal.color integerColor];

            return YES;
        }
        if (hint == stylehint_BackColor) {
            if ([gwindow isKindOfClass:[GlkTextBufferWindow class]])
                *result = [_theme.bufferBackground integerColor];
            else
                *result = [_theme.gridBackground integerColor];

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

    if ([gwindow isKindOfClass:[GlkTextBufferWindow class]] &&
        (style & 0xff) != style_Preformatted && style != style_BlockQuote) {
        GlkTextBufferWindow *textwin = (GlkTextBufferWindow *)gwindow;
        NSInteger smartquotes = _theme.smartQuotes;
        NSInteger spaceformat = _theme.spaceFormat;
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

- (void)handleSoundNotification:(NSInteger)notify withSound:(NSInteger)sound {
    GlkEvent *gev = [[GlkEvent alloc] initSoundNotify:notify withSound:sound];
    [self queueEvent:gev];
}

- (void)handleVolumeNotification:(NSInteger)notify {
    GlkEvent *gev = [[GlkEvent alloc] initVolumeNotify:notify];
    [self queueEvent:gev];
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
        id terminator_setting = myDict[key];
        if (terminator_setting) {
            myDict[key] = @(YES);
        } else
            NSLog(@"Illegal line terminator request: %x", buf[i]);
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

- (void)handleUnprintOnWindow:(GlkWindow *)win string:(unichar *)buf length:(size_t)len {

    NSString *str = [NSString stringWithCharacters:buf length:(NSUInteger)len];

    if (str == nil || str.length < 2)
        return;

    [win unputString:str];
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

    if (req->a1 >= 0 && req->a1 < MAXWIN && _gwindows[@(req->a1)])
        reqWin = _gwindows[@(req->a1)];

    switch (req->cmd) {
        case HELLO:
            ans->cmd = OKAY;
            ans->a1 = (int)[Preferences graphicsEnabled];
            ans->a2 = (int)[Preferences soundEnabled];
            break;

        case NEXTEVENT:

            // If this is the first turn, we try to restore the UI
            // from an autosave file.
            if (turns == 2) {
                if (shouldRestoreUI) {
                    [self restoreUI];
                    if (shouldShowAutorestoreAlert && !_startingInFullscreen)
                        [self performSelector:@selector(showAutorestoreAlert:) withObject:nil afterDelay:0.1];
                } else {
                    // If we are not autorestoring, try to guess an input window.
                    for (GlkWindow *win in _gwindows.allValues) {
                        if ([win isKindOfClass:[GlkTextBufferWindow class]] && [win wantsFocus]) {
                            [win grabFocus];
                        }
                    }
                }
            }

            turns++;

            [self flushDisplay];

            if (_queue.count) {
                GlkEvent *gevent;
                gevent = _queue[0];
//            NSLog(@"glkctl: writing queued event %s", msgnames[[gevent type]]);

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

#pragma mark Create and destroy windows

        case NEWWIN:
            ans->cmd = OKAY;
            ans->a1 = (int)[self handleNewWindowOfType:req->a1 andName:req->a2];
            // NSLog(@"glkctl newwin %d (type %d)", ans->a1, req->a1);
            break;

        case DELWIN:
//            NSLog(@"glkctl delwin %d", req->a1);
            if (reqWin) {
                [_windowsToBeRemoved addObject:reqWin];
                [_gwindows removeObjectForKey:@(req->a1)];
                _shouldCheckForMenu = YES;
            } else
                NSLog(@"delwin: something went wrong.");

            break;

            /*
             * Load images
             */

#pragma mark Load images

        case FINDIMAGE:
            ans->cmd = OKAY;
            ans->a1 = [imageCache objectForKey:@(req->a1)] != nil;
            break;

        case LOADIMAGE:
            buf[req->len] = 0;
            [self handleLoadImageNumber:req->a1
                                   from:[NSString stringWithCString:buf encoding:NSUTF8StringEncoding]
                                 offset:req->a2
                                 length:(NSUInteger)req->a3];
            break;

        case SIZEIMAGE:
            ans->cmd = OKAY;
            ans->a1 = 0;
            ans->a2 = 0;
            if (lastimage) {
                NSSize size;
                size = lastimage.size;
                ans->a1 = (int)size.width;
                ans->a2 = (int)size.height;
            }
            break;

        case BEEP:
            if (_theme.doSound) {
                if (req->a1 == 1 && _theme.beepHigh) {
                    NSSound *sound = [NSSound soundNamed:_theme.beepHigh];
                    if (sound) {
                        [sound stop];
                        [sound play];
                    }
                }
                if (req->a1 == 2 && _theme.beepLow) {
                    NSSound *sound = [NSSound soundNamed:_theme.beepLow];
                    if (sound) {
                        [sound stop];
                        [sound play];
                    }
                }
            }

            break;
            /*
             * Window sizing, printing, drawing, etc...
             */

#pragma mark Window sizing, printing, drawing …

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
                bg = _theme.bufferBackground;
            else
                bg = [NSColor colorFromInteger:req->a2];
            if (req->a1 == -1) {
                [self setBorderColor:bg];
            }

            if (reqWin) {
                [reqWin setBgColor:req->a2];
            }
            break;

        case DRAWIMAGE:
            if (reqWin) {
                if (lastimage && !NSEqualSizes(lastimage.size, NSZeroSize)) {
                    struct drawrect *drawstruct = (void *)buf;

                    [reqWin drawImage:lastimage
                                 val1:drawstruct->x
                                 val2:drawstruct->y
                                width:drawstruct->width
                               height:drawstruct->height
                                style:drawstruct->style];
                }
            }
            break;

        case FILLRECT:
            if (reqWin) {
                int realcount = req->len / sizeof(struct fillrect);
                if (realcount == req->a2) {
                    [reqWin fillRects:(struct fillrect *)buf count:req->a2];
                }
            }
            break;

        case PRINT:
            if (reqWin) {
                [self handlePrintOnWindow:reqWin
                                    style:(NSUInteger)req->a2
                                   buffer:(unichar *)buf
                                   length:req->len / sizeof(unichar)];
            }
            break;

        case UNPRINT:
            if (reqWin && req->len) {
                [self handleUnprintOnWindow:reqWin string:(unichar *)buf length:req->len / sizeof(unichar)];
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
                [reqWin setZColorText:req->a2 background:req->a3];
            }
            break;

        case SETREVERSE:
            if (reqWin) {
                reqWin.currentReverseVideo = (req->a2 != 0);
            }
            break;

#pragma mark Request and cancel events

        case INITLINE:
            // NSLog(@"glkctl INITLINE %d", req->a1);
            [self performScroll];
            if (reqWin) {
                [reqWin initLine:[NSString stringWithCharacters:(unichar *)buf length:(NSUInteger)req->len / sizeof(unichar)] maxLength:(NSUInteger)req->a2];

                _shouldSpeakNewText = YES;

                // Check if we are in Beyond Zork Definitions menu
                if (_beyondZork)
                    _shouldCheckForMenu = YES;            }
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

            // Hack to fix the Level 9 Adrian Mole games.
            // These request and cancel lots of char events every second,
            // which breaks scrolling, as we normally scroll down
            // one screen on every char event.
            if (lastRequest == PRINT || lastRequest == SETZCOLOR) {
                // This flag may be set by GlkBufferWindow as well
                _shouldScrollOnCharEvent = YES;
                _shouldSpeakNewText = YES;
                _shouldCheckForMenu = YES;
            }

            if (_shouldScrollOnCharEvent) {
                [self performScroll];
            }

            if (reqWin) {
                [reqWin initChar];
            }
            break;

        case CANCELCHAR:
            //            NSLog(@"glkctl CANCELCHAR %d", req->a1);
            if (reqWin)
                [reqWin cancelChar];
            break;

        case INITMOUSE:
            //            NSLog(@"glkctl initmouse %d", req->a1);
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

        case EVTSOUND:
            NSLog(@"glkctl EVTSOUND %d, %d. Send it back whence it came.",
                  req->a2, req->a3);
            [self handleSoundNotification:req->a3 withSound:req->a2];
            break;

        case EVTVOLUME:
            NSLog(@"glkctl EVTVOLUME %d. Send it back whence it came.", req->a3);
            [self handleVolumeNotification:req->a3];
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

            /*
             * HTML-TADS specifics will go here.
             */

        default:
            NSLog(@"glkctl: unhandled request (%d)", req->cmd);
            break;
    }

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

- (void)noteTaskDidTerminate:(id)sender {
    NSLog(@"glkctl: noteTaskDidTerminate");

    dead = YES;

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
        [alert addButtonWithTitle:NSLocalizedString(@"Oops", nil)];
        [alert beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse returnCode) {}];
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
    if (gevent.type == EVTARRANGE) {
        NSDictionary *newArrangeValues = @{
                                           @"width" : @(gevent.val1),
                                           @"height" : @(gevent.val2),
                                           @"bufferMargin" : @(_theme.bufferMarginX),
                                           @"gridMargin" : @(_theme.gridMarginX),
                                           @"charWidth" : @(_theme.cellWidth),
                                           @"lineHeight" : @(_theme.cellHeight),
                                           @"leading" : @(((NSParagraphStyle *)(_theme.gridNormal.attributeDict)[NSParagraphStyleAttributeName]).lineSpacing)
                                           };

//        NSLog(@"GlkController queueEvent: %@",newArrangeValues);

        if (!gevent.forced && [lastArrangeValues isEqualToDictionary:newArrangeValues]) {
            return;
        }

        lastArrangeValues = newArrangeValues;
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
}

- (void)noteDataAvailable:(NSData *)data {

    struct message request;
    struct message reply;
    char minibuf[GLKBUFSIZE + 1];
    char *maxibuf;
    char *buf;
    BOOL stop = NO;

    int sendfd = sendfh.fileDescriptor;

    if (!data.length) {
        NSLog(@"Connection closed");
        [task terminate];
        return;
    }

    if (bufferedData) {
        [bufferedData appendData:data];
        data = [NSData dataWithData:bufferedData];
        bufferedData = nil;
    }

    NSRange rangeToRead = NSMakeRange(0, sizeof(struct message));

again:

    buf = minibuf;
    maxibuf = NULL;

    if (data.length < sizeof(struct message)) {
        //Too little data to read header. Bailing until we have more.
        bufferedData = [NSMutableData dataWithData:[data subdataWithRange:NSMakeRange(rangeToRead.location, data.length - rangeToRead.location)]];

        return;
    }

    [data getBytes:&request
             range:rangeToRead];

    rangeToRead = NSMakeRange(NSMaxRange(rangeToRead), request.len);

    if (request.len) {

        if (NSMaxRange(rangeToRead) > data.length) {
            //Too little data to read message body. Bailing until we have more.
            bufferedData = [NSMutableData dataWithData:
                            [data subdataWithRange:NSMakeRange(rangeToRead.location - sizeof(struct message),
                                    data.length - rangeToRead.location + sizeof(struct message))]];
            return;
        }

        // Create a maxibuf if we need more space than provided by minibuf.
        // There likely exists a more elegant way to accomplish this
        if (request.len > GLKBUFSIZE) {
            maxibuf = malloc(request.len);
            if (!maxibuf) {
                NSLog(@"glkctl: out of memory for message (%zu bytes)", request.len);
                [task terminate];
                return;
            }
            buf = maxibuf;
        }

        [data getBytes:buf
                 range:rangeToRead];
    }

    memset(&reply, 0, sizeof reply);

    stop = [self handleRequest:&request reply:&reply buffer:buf];

    if (reply.cmd > NOREPLY) {
        write(sendfd, &reply, sizeof(struct message));
        if (reply.len)
            write(sendfd, buf, reply.len);
    }

    if (maxibuf)
        free(maxibuf);

    // if stop is true, don't read or wait for more data (but buffer any unread data).
    if (stop) {
        bufferedData = [NSMutableData dataWithData:
                        [data subdataWithRange:NSMakeRange(NSMaxRange(rangeToRead),
                                                           data.length - NSMaxRange(rangeToRead))]];
        return;
    }

    rangeToRead = NSMakeRange(NSMaxRange(rangeToRead), sizeof(struct message));

    if (NSMaxRange(rangeToRead) <= data.length) {
        goto again;
    } else {
        bufferedData = [NSMutableData dataWithData:
                        [data subdataWithRange:NSMakeRange(rangeToRead.location,
                                                           data.length - rangeToRead.location)]];
        return;
    }
}

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
    self.bgcolor = color;
    //    NSLog(@"GlkController setBorderColor: %@", color);
    if (_theme.doStyles || [color isEqualToColor:_theme.bufferBackground] || [color isEqualToColor:_theme.gridBackground]) {
        CGFloat components[[color numberOfComponents]];
        CGColorSpaceRef colorSpace = [[color colorSpace] CGColorSpace];
        [color getComponents:(CGFloat *)&components];
        CGColorRef cgcol = CGColorCreate(colorSpace, components);

        _borderView.layer.backgroundColor = cgcol;
        self.window.backgroundColor = color;
        CFRelease(cgcol);
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
        if (![_game.metadata.title isEqualToString:_gamefile.lastPathComponent])
            self.window.title =_game.metadata.title;
    }
}


#pragma mark Full screen

- (NSSize)window:(NSWindow *)window willUseFullScreenContentSize:(NSSize)proposedSize {
    borderFullScreenSize = proposedSize;
    return proposedSize;
}

- (NSArray *)customWindowsToEnterFullScreenForWindow:(NSWindow *)window {
    if (window == self.window){
        if (restoredController && restoredController.inFullscreen) {
            return @[ window ];
        } else {
            [self makeAndPrepareSnapshotWindow];
            return @[ window, snapshotWindow ];
        }
    } else return nil;
}

- (NSArray *)customWindowsToExitFullScreenForWindow:(NSWindow *)window {
    if (window == self.window)
        return @[ window ];
    else
        return nil;
}

- (void)windowWillEnterFullScreen:(NSNotification *)notification {
    // Save the window frame so that it can be restored later
    _windowPreFullscreenFrame = self.window.frame;
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
    snapshotWindow = nil;
    [window setFrame:_windowPreFullscreenFrame display:YES];
    _contentView.frame = [self contentFrameForWindowed];
}

- (void)storeScrollOffsets {
    if (_previewDummy || _ignoreResizes)
        return;
    for (GlkWindow *win in [_gwindows allValues])
        if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
            ((GlkTextBufferWindow *)win).pendingScrollRestore = NO;
            [(GlkTextBufferWindow *)win storeScrollOffset];
            ((GlkTextBufferWindow *)win).pendingScrollRestore = YES;
        }
}

- (void)restoreScrollOffsets {
    if (_previewDummy)
        return;
    for (GlkWindow *win in [_gwindows allValues])
        if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
            [(GlkTextBufferWindow *)win restoreScrollBarStyle];
            [(GlkTextBufferWindow *)win performSelector:@selector(restoreScroll:) withObject:nil afterDelay:0.2];
        }
}

- (void)window:(NSWindow *)window
startCustomAnimationToEnterFullScreenWithDuration:(NSTimeInterval)duration {

    inFullScreenResize = YES;

    // Make sure the window style mask includes the
    // full screen bit
    window.styleMask = (window.styleMask | NSFullScreenWindowMask);

    if (restoredController && restoredController.inFullscreen) {
        [self window:window startGameInFullScreenAnimationWithDuration:duration];
    } else {
        [self window:window enterFullScreenAnimationWithDuration:duration];
    }
}

- (void)window:(NSWindow *)window
enterFullScreenAnimationWithDuration:(NSTimeInterval)duration {

    // Make sure the snapshot window style mask includes the
    // full screen bit
    snapshotWindow.styleMask = (snapshotWindow.styleMask | NSFullScreenWindowMask);
    [snapshotWindow setFrame:window.frame display:YES];

    NSScreen *screen = window.screen;

    if (NSEqualSizes(borderFullScreenSize, NSZeroSize))
        borderFullScreenSize = screen.frame.size;

    // The final, full screen frame
    NSRect border_finalFrame = screen.frame;
    border_finalFrame.size = borderFullScreenSize;

    // The center frame for the window is used during
    // the 1st half of the fullscreen animation and is
    // the window at its original size but moved to the
    // center of its eventual full screen frame.

    NSRect centerBorderFrame = NSMakeRect(floor((screen.frame.size.width -
                                                 _borderView.frame.size.width) /
                                                2), borderFullScreenSize.height
                                          - _borderView.frame.size.height,
                                          _borderView.frame.size.width,
                                          _borderView.frame.size.height);

    NSRect centerWindowFrame = [window frameRectForContentRect:centerBorderFrame];

    centerWindowFrame.origin.x += screen.frame.origin.x;
    centerWindowFrame.origin.y += screen.frame.origin.y;

    _contentView.autoresizingMask = NSViewMinXMargin | NSViewMaxXMargin |
    NSViewMinYMargin; // Attached at top but not bottom or sides

    NSView __weak *localContentView = _contentView;
    NSView __weak *localBorderView = _borderView;
    NSWindow *localSnapshot = snapshotWindow;

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
                   context.duration = duration / 10;
                   [[localContentView animator] setFrame:newContentFrame];
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
                        [weakSelf enableArrangementEvents];

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
                             GlkEvent *gevent = [[GlkEvent alloc]
                                                 initArrangeWidth:(NSInteger)localContentView.frame.size.width
                                                 height:(NSInteger)localContentView.frame.size.height
                                                 theme:self.theme
                                                 force:NO];

                             [weakSelf queueEvent:gevent];
                             [weakSelf restoreScrollOffsets];
                         }];
                    }];
               }];
          }];
     }];
}

- (void)window:(NSWindow *)window startGameInFullScreenAnimationWithDuration:(NSTimeInterval)duration {

    NSScreen *screen = window.screen;

    if (NSEqualSizes(borderFullScreenSize, NSZeroSize))
        borderFullScreenSize = screen.frame.size;

    // The final, full screen frame
    NSRect border_finalFrame = screen.frame;
    border_finalFrame.size = borderFullScreenSize;

    // The center frame for the window is used during
    // the 1st half of the fullscreen animation and is
    // the window at its original size but moved to the
    // center of its eventual full screen frame.

    NSRect centerWindowFrame = _windowPreFullscreenFrame;
    centerWindowFrame.origin = NSMakePoint(screen.frame.origin.x + floor((screen.frame.size.width -
                                                                          _borderView.frame.size.width) /
                                                                         2), screen.frame.origin.x + borderFullScreenSize.height
                                           - centerWindowFrame.size.height);

    centerWindowFrame.origin.x += screen.frame.origin.x;
    centerWindowFrame.origin.y += screen.frame.origin.y;

    NSView __weak *localContentView = _contentView;
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
            // Finally, we get the content view into position ...
              [weakSelf enableArrangementEvents];
              localContentView.frame = [weakSelf contentFrameForFullscreen];
              GlkEvent *gevent = [[GlkEvent alloc]
                                  initArrangeWidth:(NSInteger)localContentView.frame.size.width
                                  height:(NSInteger)localContentView.frame.size.height
                                  theme:weakSelf.theme
                                  force:NO];

              [weakSelf queueEvent:gevent];

              if (stashShouldShowAlert)
                  [weakSelf performSelector:@selector(showAutorestoreAlert:) withObject:nil afterDelay:0.1];
              [weakSelf restoreScrollOffsets];
          }];
     }];
}

- (void)enableArrangementEvents {
    inFullScreenResize = NO;
}

- (void)window:window startCustomAnimationToExitFullScreenWithDuration:(NSTimeInterval)duration {
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
         [window
          setStyleMask:(NSUInteger)([window styleMask] & ~(NSUInteger)NSFullScreenWindowMask)];
         [[window animator] setFrame:oldFrame display:YES];
     }
     completionHandler:^{
         [weakSelf enableArrangementEvents];
         localBorderView.frame = ((NSView *)localWindow.contentView).frame;
         localContentView.frame = [weakSelf contentFrameForWindowed];

         localContentView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

         [weakSelf contentDidResize:localContentView.frame];
         [weakSelf restoreScrollOffsets];
     }];
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification {
    snapshotWindow = nil;
    _ignoreResizes = NO;
    [self contentDidResize:_contentView.frame];
}

- (void)windowDidExitFullScreen:(NSNotification *)notification {
    _ignoreResizes = NO;
    _inFullscreen = NO;
    [self contentDidResize:_contentView.frame];
}

- (void)startInFullscreen {
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
            bufwin.pendingScrollRestore = YES;
        }
    [self performSelector:@selector(deferredEnterFullscreen:) withObject:nil afterDelay:1];
}

- (void)deferredEnterFullscreen:(id)sender {
    [self.window toggleFullScreen:nil];
    if (shouldShowAutorestoreAlert)
        [self performSelector:@selector(showAutorestoreAlert:) withObject:nil afterDelay:1];
}

- (CALayer *)takeSnapshot {
    [self showWindow:nil];
    CGImageRef windowSnapshot = CGWindowListCreateImage(
                                                        CGRectNull, kCGWindowListOptionIncludingWindow,
                                                        (CGWindowID)[self.window windowNumber], kCGWindowImageBoundsIgnoreFraming);
    CALayer *snapshotLayer = [[CALayer alloc] init];
    [snapshotLayer setFrame:NSRectToCGRect([self.window frame])];
    [snapshotLayer setContents:CFBridgingRelease(windowSnapshot)];
    [snapshotLayer setAnchorPoint:CGPointMake(0, 0)];
    return snapshotLayer;
}

- (void)makeAndPrepareSnapshotWindow {
    CALayer *snapshotLayer = [self takeSnapshot];
    snapshotWindow = ([[NSWindow alloc]
                       initWithContentRect:self.window.frame
                       styleMask:0
                       backing:NSBackingStoreBuffered
                       defer:NO]);
    [[snapshotWindow contentView] setWantsLayer:YES];
    [snapshotWindow setOpaque:NO];
    [snapshotWindow setBackgroundColor:[NSColor clearColor]];
    [snapshotWindow setFrame:self.window.frame display:NO];
    [[[snapshotWindow contentView] layer] addSublayer:snapshotLayer];
    // Compute the frame of the snapshot layer such that the snapshot is
    // positioned exactly on top of the original position of the game window.
    NSRect snapshotLayerFrame =
    [snapshotWindow convertRectFromScreen:self.window.frame];
    [snapshotLayer setFrame:snapshotLayerFrame];
    [(NSWindowController *)[snapshotWindow delegate] showWindow:nil];
    [snapshotWindow orderFront:nil];
}

// Some convenience methods
- (void)adjustContentView {
    if ((self.window.styleMask & NSFullScreenWindowMask) == NSFullScreenWindowMask ||
        NSEqualRects(_borderView.frame, self.window.screen.frame) || (dead && _inFullscreen && windowRestoredBySystem)) {
        // We are in fullscreen
        _contentView.frame = [self contentFrameForFullscreen];
    } else {
        // We are not in fullscreen
        _contentView.frame = [self contentFrameForWindowed];
    }
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

- (BOOL)isAccessibilityElement {
   return NO;
}

- (NSArray *)accessibilityCustomActions API_AVAILABLE(macos(10.13)) {
    NSAccessibilityCustomAction *speakMostRecent = [[NSAccessibilityCustomAction alloc]
                                                    initWithName:@"repeat the text output of the last move" target:self selector:@selector(speakMostRecent:)];
    NSAccessibilityCustomAction *speakPrevious = [[NSAccessibilityCustomAction alloc]
                                                    initWithName:@"step backward through moves" target:self selector:@selector(speakPrevious:)];
    NSAccessibilityCustomAction *speakNext = [[NSAccessibilityCustomAction alloc]
                                                    initWithName:@"step forward through moves" target:self selector:@selector(speakNext:)];
    NSAccessibilityCustomAction *speakStatus = [[NSAccessibilityCustomAction alloc]
                                                initWithName:@"speak status bar text" target:self selector:@selector(speakStatus:)];

    return @[speakStatus, speakNext, speakPrevious, speakMostRecent];
}

- (void)noteAccessibilityStatusChanged:(NSNotification *)notify {
    if(@available(macOS 10.13, *)) {
        NSWorkspace * ws = [NSWorkspace sharedWorkspace];
        _voiceOverActive = ws.voiceOverEnabled;
        if (_voiceOverActive) {
            _shouldCheckForMenu = YES;
            [self performSelector:@selector(deferredSpeakLargest:) withObject:nil afterDelay:2];
        }
    }
}

- (void)deferredSpeakLargest:(id)sender {
    [self speakLargest:_gwindows.allValues];
}


#pragma mark ZMenu

- (void)checkZMenu {
    if (!_voiceOverActive)
        return;
    if (_shouldCheckForMenu) {
        _shouldCheckForMenu = NO;
        BOOL wasInMenu = NO;
        if (!_zmenu) {
            _zmenu = [[ZMenu alloc] initWithGlkController:self];
        } else {
            wasInMenu = YES;
        }
        if (![_zmenu isMenu]) {
            [NSObject cancelPreviousPerformRequestsWithTarget:_zmenu];
            _zmenu = nil;
        }
    }

    if (_zmenu) {
        [_zmenu speakSelectedLine];
    }
}

#pragma mark Speak new text

- (void)speakNewText {
    // Find a "main text window"
    NSMutableArray *windowsWithText = _gwindows.allValues.mutableCopy;
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
    if (_zmenu || !_voiceOverActive) {
        return;
    }

    GlkTextBufferWindow *largest = nil;
    CGFloat largestSize = 0;
    for (GlkTextBufferWindow *view in array) {
        CGFloat size = fabs(view.frame.size.width * view.frame.size.height);
        if (size > largestSize) {
            largestSize = size;
            largest = view;
        }
    }
    if (largest)
    {
        if (largest != _spokeLast && [_speechTimeStamp timeIntervalSinceNow]  > -0.5)
            return;
        _speechTimeStamp = [NSDate date];
        _spokeLast = largest;
        [largest performSelector:@selector(deferredSpeakMostRecent:) withObject:nil afterDelay:0.1];
    }
}

#pragma mark Speak previous moves

- (IBAction)speakMostRecent:(id)sender {
//    NSLog(@"GlkController: speakMostRecent");
    GlkTextBufferWindow *mainWindow = [self largestWithMoves];
    if (!mainWindow) {
        [self speakString:@"No last move to speak!"];
        return;
    }
    [mainWindow speakMostRecent];
}

- (IBAction)speakPrevious:(id)sender {
//    NSLog(@"GlkController: speakPrevious");
    GlkTextBufferWindow *mainWindow = [self largestWithMoves];
    if (!mainWindow) {
        [self speakString:@"No previous move to speak!"];
        return;
    }
    [mainWindow speakPrevious];
}

- (IBAction)speakNext:(id)sender {
//    NSLog(@"GlkController: speakNext");
    GlkTextBufferWindow *mainWindow = [self largestWithMoves];
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
    NSLog(@"No status window found");
}

- (void)speakString:(NSString *)string {
    if (!string || string.length == 0 || !_voiceOverActive)
        return;

    NSDictionary *announcementInfo =
        @{ NSAccessibilityPriorityKey : @(NSAccessibilityPriorityHigh),
       NSAccessibilityAnnouncementKey : string };

    NSWindow *mainWin = [NSApp mainWindow];

    if (mainWin) {
        NSAccessibilityPostNotificationWithUserInfo(
                                                    mainWin,
                                                    NSAccessibilityAnnouncementRequestedNotification, announcementInfo);
    }
}

- (GlkTextBufferWindow *)largestWithMoves {
    // Find a "main text window"
    GlkTextBufferWindow *largest = nil;
    NSMutableArray *windowsWithMoves = _gwindows.allValues.mutableCopy;
    for (GlkWindow *view in _gwindows.allValues) {
        if (![view isKindOfClass:[GlkTextBufferWindow class]] || !((GlkTextBufferWindow *)view).moveRanges.count) {
            // Remove all GlkTextBufferWindow objects with no list of previous moves
            [windowsWithMoves removeObject:view];
        }
    }

    if (!windowsWithMoves.count) {
        NSLog(@"largestWithMoves: No windows with moves history!");
        return nil;
    }

    CGFloat largestSize = 0;
    for (GlkTextBufferWindow *view in windowsWithMoves) {
        CGFloat size = fabs(view.frame.size.width * view.frame.size.height);
        if (size > largestSize) {
            largestSize = size;
            largest = view;
        }
    }
    return largest;
}

#pragma mark Custom rotors

- (NSAccessibilityCustomRotorItemResult *)rotor:(NSAccessibilityCustomRotor *)rotor
                      resultForSearchParameters:(NSAccessibilityCustomRotorSearchParameters *)searchParameters  API_AVAILABLE(macos(10.13)){

    NSAccessibilityCustomRotorItemResult *searchResult = nil;

    NSAccessibilityCustomRotorItemResult *currentItemResult = searchParameters.currentItem;
    NSAccessibilityCustomRotorSearchDirection direction = searchParameters.searchDirection;
    NSString *filterText = searchParameters.filterString;
    NSRange currentRange = currentItemResult.targetRange;

    NSMutableArray *children = [[NSMutableArray alloc] init];
    NSMutableArray *linkTargetViews = [[NSMutableArray alloc] init];

    NSUInteger currentItemIndex;

    if (rotor.type == NSAccessibilityCustomRotorTypeAny) {
        return [self textSearchResultForString:filterText fromRange: currentRange direction:direction];
    } else if ([rotor.label isEqualToString:NSLocalizedString(@"Command history", nil)]) {
        return [self commandHistoryRotor:rotor resultForSearchParameters:searchParameters];
    } else if ([rotor.label isEqualToString:NSLocalizedString(@"Game windows", nil)]) {
        return [self glkWindowRotor:rotor resultForSearchParameters:searchParameters];
    } else if (rotor.type == NSAccessibilityCustomRotorTypeLink) {
        NSArray *allWindows = _gwindows.allValues;
        if (_colderLight && allWindows.count == 5) {
            allWindows = @[_gwindows[@(3)], _gwindows[@(4)], _gwindows[@(0)], _gwindows[@(1)]];
        }
        for (GlkWindow *view in allWindows) {
            if (![view isKindOfClass:[GlkGraphicsWindow class]]) {
                id targetTextView = ((GlkTextBufferWindow *)view).textview;
                NSArray *links = [view links];

                if (filterText.length && links.count) {
                    __block NSString *text = ((NSTextView *)targetTextView).string;
                    links = [links filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(id object, NSDictionary *bindings) {
                        NSRange range = ((NSValue *)object).rangeValue;
                        NSString *subString = [text substringWithRange:range];
                        return ([subString localizedCaseInsensitiveContainsString:filterText]);
                    }]];
                }

                [children addObjectsFromArray:links];

                while (linkTargetViews.count < children.count)
                    [linkTargetViews addObject:targetTextView];
            }
        }

        currentItemIndex = [children indexOfObject:[NSValue valueWithRange:currentRange]];
    }

    if (currentItemIndex == NSNotFound) {
        // Find the start or end element.
        if (direction == NSAccessibilityCustomRotorSearchDirectionNext) {
            currentItemIndex = 0;
        } else if (direction == NSAccessibilityCustomRotorSearchDirectionPrevious) {
            currentItemIndex = children.count - 1;
        }
    } else {
        if (direction == NSAccessibilityCustomRotorSearchDirectionPrevious) {
            if (currentItemIndex == 0)
                currentItemIndex = NSNotFound;
            else
                currentItemIndex--;
        } else if (direction == NSAccessibilityCustomRotorSearchDirectionNext) {
            if (currentItemIndex == children.count - 1)
                currentItemIndex = NSNotFound;
            else
                currentItemIndex++;
        }
    }

    if (currentItemIndex != NSNotFound) {
        NSRange textRange = ((NSValue *)children[currentItemIndex]).rangeValue;
        id targetElement = linkTargetViews[currentItemIndex];
        searchResult = [[NSAccessibilityCustomRotorItemResult alloc] initWithTargetElement:targetElement];
        unichar firstChar = [((NSTextView *)targetElement).textStorage.string characterAtIndex:textRange.location];
        if (_colderLight && firstChar == '<' && textRange.length == 1) {
            searchResult.customLabel = @"Previous Menu";
        } else if (firstChar == NSAttachmentCharacter) {
            NSDictionary *attrs = [((NSTextView *)targetElement).textStorage attributesAtIndex:textRange.location effectiveRange:nil];
            searchResult.customLabel = [NSString stringWithFormat:@"Image with link I.D. %@", attrs[NSLinkAttributeName]];
        }
        searchResult.targetRange = textRange;
    }
    return searchResult;
}

- (NSAccessibilityCustomRotorItemResult *)textSearchResultForString:(NSString *)searchString fromRange:(NSRange)fromRange direction: (NSAccessibilityCustomRotorSearchDirection)direction  API_AVAILABLE(macos(10.13)){

    NSAccessibilityCustomRotorItemResult *searchResult = nil;

    NSTextView *bestMatch = nil;
    NSRange bestMatchRange;

    if (searchString.length) {
        BOOL searchFound = NO;
        for (GlkWindow *view in _gwindows.allValues) {
            if (![view isKindOfClass:[GlkGraphicsWindow class]]) {
                NSString *contentString = ((GlkTextBufferWindow *)view).textview.string;

                NSRange resultRange = [contentString rangeOfString:searchString options:NSCaseInsensitiveSearch range:NSMakeRange(0, contentString.length - 1) locale:nil];

                if (resultRange.location == NSNotFound)
                    continue;

                NSRange realRange = resultRange;

                NSLog(@"Found string \"%@\" in %@ %ld", searchString, [view class], view.name);

                if (direction == NSAccessibilityCustomRotorSearchDirectionPrevious) {
                    searchFound = (realRange.location < fromRange.location);
                } else if (direction == NSAccessibilityCustomRotorSearchDirectionNext) {
                    searchFound = (realRange.location >= NSMaxRange(fromRange));
                }
                if (searchFound) {
                    searchResult = [[NSAccessibilityCustomRotorItemResult alloc] initWithTargetElement:((GlkTextBufferWindow *)view).textview];
                    searchResult.targetRange = realRange;
                    return searchResult;
                }

                bestMatchRange = resultRange;
                bestMatch = ((GlkTextBufferWindow *)view).textview;

            }
        }
    }
    if (bestMatch) {
        searchResult = [[NSAccessibilityCustomRotorItemResult alloc] initWithTargetElement:bestMatch];
        searchResult.targetRange = bestMatchRange;
    }
    return searchResult;
}

- (NSAccessibilityCustomRotorItemResult *)glkWindowRotor:(NSAccessibilityCustomRotor *)rotor
                               resultForSearchParameters:(NSAccessibilityCustomRotorSearchParameters *)searchParameters  API_AVAILABLE(macos(10.13)){
    NSAccessibilityCustomRotorItemResult *searchResult = nil;

    NSAccessibilityCustomRotorItemResult *currentItemResult = searchParameters.currentItem;
    NSAccessibilityCustomRotorSearchDirection direction = searchParameters.searchDirection;
    NSString *filterText = searchParameters.filterString;

    NSMutableArray *children = [[NSMutableArray alloc] init];
    NSMutableArray *strings = [[NSMutableArray alloc] init];

    NSArray *allWindows = _gwindows.allValues;
    allWindows = [allWindows sortedArrayUsingComparator:
                  ^NSComparisonResult(id obj1, id obj2){
        CGFloat y1 = ((NSView *)obj1).frame.origin.y;
        CGFloat y2 = ((NSView *)obj2).frame.origin.y;
        if (y1 > y2) {
            return (NSComparisonResult)NSOrderedDescending;
        }
        if (y1 < y2) {
            return (NSComparisonResult)NSOrderedAscending;
        }
        return (NSComparisonResult)NSOrderedSame;
    }];

    for (GlkWindow *win in allWindows) {
        if (![win isKindOfClass:[GlkGraphicsWindow class]]) {
            GlkTextBufferWindow *bufWin = (GlkTextBufferWindow *)win;
            NSTextView *textview = bufWin.textview;
            NSString *string = textview.string;
            if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
                if (bufWin.moveRanges.count) {
                    NSRange range = ((NSValue *)bufWin.moveRanges.lastObject).rangeValue;
                    string = [string substringFromIndex:range.location];
                }
            }
            NSString *kindString;
            if ([win isKindOfClass:[GlkTextGridWindow class]]) {
                kindString = @"Grid";
            } else {
                kindString = @"Buffer";
            }

            string = [NSString stringWithFormat:@"%@ text window%@%@", kindString, (string.length) ? @": " : @"", string];

            NSLog(@"String: %@", string);

            if (filterText.length == 0 || [string localizedCaseInsensitiveContainsString:filterText]) {
                [children addObject:textview];
                [strings addObject:string.copy];
            }
        }
    }

    NSUInteger currentItemIndex = [children indexOfObject:currentItemResult.targetElement];

    if (currentItemIndex == NSNotFound) {
        // Find the start or end element.
        if (direction == NSAccessibilityCustomRotorSearchDirectionNext) {
            currentItemIndex = 0;
        } else if (direction == NSAccessibilityCustomRotorSearchDirectionPrevious) {
            currentItemIndex = children.count - 1;
        }
    } else {
        if (direction == NSAccessibilityCustomRotorSearchDirectionPrevious) {
            if (currentItemIndex == 0) {
                currentItemIndex = NSNotFound;
            } else {
                currentItemIndex--;
            }
        } else if (direction == NSAccessibilityCustomRotorSearchDirectionNext) {
            if (currentItemIndex == children.count - 1) {
                currentItemIndex = NSNotFound;
            } else {
                currentItemIndex++;
            }
        }
    }

    if (currentItemIndex == NSNotFound) {
        return nil;
    }

    GlkWindow *targetWindow = children[currentItemIndex];

    if (targetWindow) {
        searchResult = [[NSAccessibilityCustomRotorItemResult alloc] initWithTargetElement: targetWindow];
        searchResult.customLabel = strings[currentItemIndex];
    }

    return searchResult;
}

- (NSAccessibilityCustomRotorItemResult *)commandHistoryRotor:(NSAccessibilityCustomRotor *)rotor
                      resultForSearchParameters:(NSAccessibilityCustomRotorSearchParameters *)searchParameters  API_AVAILABLE(macos(10.13)){

    NSAccessibilityCustomRotorItemResult *searchResult = nil;

    NSAccessibilityCustomRotorSearchDirection direction = searchParameters.searchDirection;
    NSRange currentRange = searchParameters.currentItem.targetRange;

    NSString *filterText = searchParameters.filterString;

    GlkTextBufferWindow *largest = [self largestWithMoves];
    if (!largest)
        return nil;

    NSArray *children = [[largest.moveRanges reverseObjectEnumerator] allObjects];

    if (children.count > 50)
        children = [children subarrayWithRange:NSMakeRange(0, 50)];
    NSMutableArray *strings = [[NSMutableArray alloc] initWithCapacity:children.count];
    NSMutableArray *mutableChildren = [[NSMutableArray alloc] initWithCapacity:children.count];

    for (NSValue *child in children) {
        NSRange range = child.rangeValue;
        NSString *string = [largest.textview.string substringWithRange:range];

        if (filterText.length == 0 || [string localizedCaseInsensitiveContainsString:filterText]) {
            [strings addObject:string];
            [mutableChildren addObject:child];
        }
    }

    children = mutableChildren;

    NSUInteger currentItemIndex = [children indexOfObject:[NSValue valueWithRange:currentRange]];

     if (currentItemIndex == NSNotFound) {
        // Find the start or end element.
        if (direction == NSAccessibilityCustomRotorSearchDirectionNext) {
            currentItemIndex = 0;
        } else if (direction == NSAccessibilityCustomRotorSearchDirectionPrevious) {
            currentItemIndex = children.count - 1;
        }
    } else {
        if (direction == NSAccessibilityCustomRotorSearchDirectionPrevious) {
            if (currentItemIndex == 0) {
                currentItemIndex = NSNotFound;
            } else {
                currentItemIndex--;
            }
        } else if (direction == NSAccessibilityCustomRotorSearchDirectionNext) {
            if (currentItemIndex == children.count - 1) {
                currentItemIndex = NSNotFound;
            } else {
                currentItemIndex++;
            }
        }
    }

    if (currentItemIndex == NSNotFound) {
        return nil;
    }

    NSValue *targetRangeValue = children[currentItemIndex];

    if (targetRangeValue) {
        NSRange textRange = targetRangeValue.rangeValue;
        searchResult = [[NSAccessibilityCustomRotorItemResult alloc] initWithTargetElement:largest.textview];
        searchResult.targetRange = textRange;
        // By adding a custom label, all ranges are reliably listed in the rotor
        NSString *charSetString = @"\u00A0 >\n";
        NSCharacterSet *charset = [NSCharacterSet characterSetWithCharactersInString:charSetString];
        NSString *string = strings[currentItemIndex];
        string = [string stringByTrimmingCharactersInSet:charset];

        searchResult.customLabel = string;
    }

    return searchResult;
}

- (NSArray *)createCustomRotors {
    if (@available(macOS 10.13, *)) {
        NSMutableArray *rotorsArray = [[NSMutableArray alloc] init];

        BOOL hasLinks = NO;
        for (GlkWindow *view in _gwindows.allValues) {
            if (![view isKindOfClass:[GlkGraphicsWindow class]] && view.links.count) {
                hasLinks = YES;
            }
        }

        // Create the link rotor
        if (hasLinks) {
            NSAccessibilityCustomRotor *linkRotor = [[NSAccessibilityCustomRotor alloc] initWithRotorType:NSAccessibilityCustomRotorTypeLink itemSearchDelegate:self];
            [rotorsArray addObject:linkRotor];
        }
        // Create the text search rotor.
        NSAccessibilityCustomRotor *textSearchRotor = [[NSAccessibilityCustomRotor alloc] initWithRotorType:NSAccessibilityCustomRotorTypeAny itemSearchDelegate:self];
        [rotorsArray addObject:textSearchRotor];
//        // Create the command history rotor
        if ([self largestWithMoves]) {
            NSAccessibilityCustomRotor *commandHistoryRotor = [[NSAccessibilityCustomRotor alloc] initWithLabel:NSLocalizedString(@"Command history", nil) itemSearchDelegate:self];
            [rotorsArray addObject:commandHistoryRotor];
        }
        // Create the Glk windows rotor
        if (_gwindows.count) {
            NSAccessibilityCustomRotor *glkWindowRotor = [[NSAccessibilityCustomRotor alloc] initWithLabel:NSLocalizedString(@"Game windows", nil) itemSearchDelegate:self];
            [rotorsArray addObject:glkWindowRotor];
        }
        return rotorsArray;
    } else {
        return @[];
    }
}

@end
