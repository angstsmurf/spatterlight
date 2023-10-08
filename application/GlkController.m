#import <QuartzCore/QuartzCore.h>

#include "glk_dummy_defs.h"

#import "AppDelegate.h"
#import "BufferTextView.h"
#import "Constants.h"
#import "GlkController.h"
#import "GlkGraphicsWindow.h"
#import "GlkTextBufferWindow.h"
#import "GlkTextGridWindow.h"
#import "GridTextView.h"

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
//    "RESET",           "BANNERCOLS",       "BANNERLINES",  "TIMER",
//    "INITCHAR",        "CANCELCHAR",
//    "INITLINE",        "CANCELLINE",       "SETECHO",     "TERMINATORS",
//    "INITMOUSE",       "CANCELMOUSE",      "FILLRECT",    "FINDIMAGE",
//    "LOADIMAGE",       "SIZEIMAGE",        "DRAWIMAGE",   "FLOWBREAK",
//    "NEWCHAN",         "DELCHAN",          "FINDSOUND",   "LOADSOUND",
//    "SETVOLUME",       "PLAYSOUND",        "STOPSOUND",   "PAUSE",
//    "UNPAUSE",         "BEEP",
//    "SETLINK",         "INITLINK",         "CANCELLINK",  "SETZCOLOR",
//    "SETREVERSE",      "QUOTEBOX",         "SHOWERROR",   "CANPRINT",
//    "NEXTEVENT",       "EVTARRANGE",       "EVTREDRAW",   "EVTLINE",
//    "EVTKEY",          "EVTMOUSE",         "EVTTIMER",    "EVTHYPER",
//    "EVTSOUND",        "EVTVOLUME",        "EVTPREFS",    "EVTQUIT" };

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


@implementation GlkHelperView

- (BOOL)isFlipped {
    return YES;
}

- (void)setFrame:(NSRect)frame {
    super.frame = frame;
    GlkController *glkctl = _glkctrl;
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

    BOOL skipNextScriptCommand;
    //    NSDate *lastFlushTimestamp;
    NSDate *lastScriptKeyTimestamp;
    NSDate *lastKeyTimestamp;
    NSDate *lastResetTimestamp;
}

@property BOOL shouldShowAutorestoreAlert;

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

    _ignoreResizes = YES;

    skipNextScriptCommand = NO;

    _game = game_;

    [self.window registerForDraggedTypes:@[ NSURLPboardType, NSStringPboardType]];

    if ([_terpname isEqualToString:@"bocfel"])
        _usesFont3 = YES;



    /* Setup our own stuff */

    _speechTimeStamp = [NSDate distantPast];
    _shouldSpeakNewText = NO;
    _mustBeQuiet = YES;

    _supportsAutorestore = (self.window).restorable;

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

    waitforevent = NO;
    waitforfilename = NO;
    dead = YES; // This should be YES until the interpreter process is running

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

    NSNotificationCenter *notifications = [NSNotificationCenter defaultCenter];
    _voiceOverActive = [NSWorkspace sharedWorkspace].voiceOverEnabled;

    lastContentResize = NSZeroRect;
    _inFullscreen = NO;
    _windowPreFullscreenFrame = self.window.frame;
    _borderFullScreenSize = NSZeroSize;

    restoredController = nil;
    inFullScreenResize = NO;

    self.window.representedFilename = _gamefile;

    _borderView.wantsLayer = YES;
    _borderView.layerContentsRedrawPolicy = NSViewLayerContentsRedrawOnSetNeedsDisplay;
    NSString *autosaveLatePath = [self.appSupportDir
                                  stringByAppendingPathComponent:@"autosave-GUI-late.plist"];

    lastScriptKeyTimestamp = [NSDate distantPast];
    lastKeyTimestamp = [NSDate distantPast];
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
        terpAutosaveDate = (NSDate*)attrs[NSFileCreationDate];
    } else {
        NSLog(@"Error: %@", error);
    }

    // Check creation date of GUI autosave file
    attrs = [fileManager attributesOfItemAtPath:self.autosaveFileGUI error:&error];
    if (attrs) {
        GUIAutosaveDate = (NSDate*)attrs[NSFileCreationDate];
        if (terpAutosaveDate && [terpAutosaveDate compare:GUIAutosaveDate] == NSOrderedDescending) {
            NSLog(@"GUI autosave file is created before terp autosave file!");
        }
    } else {
        NSLog(@"Error: %@", error);
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
            GUILateAutosaveDate = (NSDate*)attrs[NSFileCreationDate];
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

    _inFullscreen = restoredControllerLate.inFullscreen;
    _windowPreFullscreenFrame = restoredController.windowPreFullscreenFrame;
    _shouldStoreScrollOffset = NO;

    // If the process is dead, restore the dead window if this
    // is a system window restoration at application start

    // If this is not a window restoration done by the system,
    // we now re-enter fullscreen manually if the game was
    // closed in fullscreen mode

    shouldRestoreUI = YES;
    _mustBeQuiet = YES;
    [self forkInterpreterTask];

    // The game has to run to its third(?) NEXTEVENT
    // before we can restore the UI properly, so we don't
    // have to do anything else here for now.
}

- (void)runTerpNormal {
    // Just start the game with no autorestore or fullscreen or resetting
    NSRect newContentFrame = (self.window.contentView).frame;

    NSRect newWindowFrame = [self.window frameRectForContentRect:newContentFrame];
    NSRect screenFrame = self.window.screen.visibleFrame;
    // Make sure that the window is shorter than the screen
    if (NSHeight(newWindowFrame) > NSHeight(screenFrame))
        newWindowFrame.size.height = NSHeight(screenFrame);

    newWindowFrame.origin.x = round((NSWidth(screenFrame) - NSWidth(newWindowFrame)) / 2);
    // Place the window just above center by default
    newWindowFrame.origin.y = round(screenFrame.origin.y + (NSHeight(screenFrame) - NSHeight(newWindowFrame)) / 2) + 40;


    [self showWindow:nil];
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

    GlkController * __weak weakSelf = self;
    NSScreen *screen = self.window.screen;
    double delayInSeconds = 1.5;
    dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));

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

#endif // TEE_TERP_OUTPUT

    _queue = [[NSMutableArray alloc] init];

    dead = NO;


    restartingAlready = NO;
}

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

- (void)restoreUI {
    // We try to restore the UI here, in order to catch things
    // like entered text and scrolling, that has changed the UI
    // but not sent any events to the interpreter process.
    // This method is called in handleRequest on NEXTEVENT.

    if (restoredUIOnly) {
        restoredController = restoredControllerLate;
        _shouldShowAutorestoreAlert = NO;
    } else {
        _windowsToBeAdded = [[NSMutableArray alloc] init];
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

    // This makes autorestoring in fullscreen a little less flickery

    GlkWindow *winToGrabFocus = nil;

    // Restore scroll position etc
    for (win in _gwindows.allValues) {
        if (!_windowsToRestore.count) {
            [win postRestoreAdjustments:(restoredControllerLate.gwindows)[@(win.name)]];
        }
        if (win.name == _firstResponderView) {
            winToGrabFocus = win;
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

    [self performSelector:@selector(postRestoreArrange:) withObject:nil afterDelay:0.2];
}


- (void)postRestoreArrange:(id)sender {
    if (_shouldShowAutorestoreAlert && !_startingInFullscreen) {
        _shouldShowAutorestoreAlert = NO;
        [self performSelector:@selector(showAutorestoreAlert:) withObject:nil afterDelay:0.1];
    }

    Theme *stashedTheme = _stashedTheme;
    if (stashedTheme && stashedTheme != _theme) {
        _theme = stashedTheme;
        _stashedTheme = nil;
    }
    NSNotification *notification = [NSNotification notificationWithName:@"PreferencesChanged" object:_theme];
    _shouldStoreScrollOffset = YES;

    // Now we can actually show the window
    [self showWindow:nil];
    [self.window makeKeyAndOrderFront:nil];
    [self.window makeFirstResponder:nil];
}

- (NSString *)autosaveFileGUI {
    if (!_autosaveFileGUI)
        _autosaveFileGUI = [self.appSupportDir
                            stringByAppendingPathComponent:@"autosave-GUI.plist"];
    return _autosaveFileGUI;
}

- (void)deleteAutosaveFiles {
    [self deleteFiles:@[ [NSURL fileURLWithPath:self.autosaveFileGUI],
                         [NSURL fileURLWithPath:self.autosaveFileTerp],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave.glksave"]],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-bak.glksave"]],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-bak.plist"]],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-tmp.glksave"]],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-GUI.plist"]],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-GUI-late.plist"]],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-tmp.plist"]] ]];
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

        restoredController = nil;
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];

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
    [encoder encodeBool:((self.window.styleMask & NSWindowStyleMaskFullScreen) ==
                         NSWindowStyleMaskFullScreen)
                 forKey:@"fullscreen"];

    [encoder encodeInteger:_turns forKey:@"turns"];

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

    GlkController * __weak weakSelf = self;

    [anAlert beginSheetModalForWindow:self.window completionHandler:^(NSInteger result) {

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

        weakSelf.shouldShowAutorestoreAlert = NO;
    }];
}

- (IBAction)reset:(id)sender {
    if (lastResetTimestamp && lastResetTimestamp.timeIntervalSinceNow < -1) {
        restartingAlready = NO;
    }

    if (restartingAlready || _showingCoverImage)
        return;

    restartingAlready = YES;
    lastResetTimestamp = [NSDate date];
    _mustBeQuiet = YES;

    [[NSNotificationCenter defaultCenter]
     removeObserver:self name: NSFileHandleDataAvailableNotification
     object: readfh];

    _commandScriptHandler = nil;

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
       winRestore:NO];

    [self.window makeKeyAndOrderFront:nil];
    [self.window makeFirstResponder:nil];
    [self guessFocus];
    NSAccessibilityPostNotification(self.window.firstResponder, NSAccessibilityFocusedUIElementChangedNotification);
}

-(void)cleanup {
    [self deleteAutosaveFiles];

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

    _form = nil;
    _zmenu = nil;

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

- (void)windowDidResignKey:(NSNotification *)notification {
    _mustBeQuiet = YES;
}

- (BOOL)windowShouldClose:(id)sender {
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

    if (timer) {
        // Stop the timer
        [timer invalidate];
        timer = nil;
    }

    libcontroller = nil;
}

- (void)flushDisplay {
    if (windowdirty) {
        GlkWindow *largest = [self largestWindow];
        windowdirty = NO;
    }

    for (GlkWindow *win in _windowsToBeAdded) {
        [_contentView addSubview:win];
    }

    _windowsToBeAdded = [NSMutableArray new];

    for (GlkWindow *win in _gwindows.allValues) {
        [win flushDisplay];
    }

    for (GlkWindow *win in _windowsToBeRemoved) {
        [win removeFromSuperview];
        win.glkctl = nil;
    }

    _shouldSpeakNewText = NO;

    _windowsToBeRemoved = [[NSMutableArray alloc] init];
}


- (void)guessFocus {
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

#pragma mark Window resizing

#pragma mark Zoom


/*
 *
 */


- (NSInteger)handleNewWindowOfType:(NSInteger)wintype andName:(NSInteger)name {
    NSInteger i;

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


#pragma mark Border color

- (void)setBorderColor:(NSColor *)color fromWindow:(GlkWindow *)aWindow {
    NSSize windowsize = aWindow.bounds.size;
    if (aWindow.framePending)
        windowsize = aWindow.pendingFrame.size;
    CGFloat relativeSize = (windowsize.width * windowsize.height) / (_contentView.bounds.size.width * _contentView.bounds.size.height);
    if (relativeSize < 0.70 && ![aWindow isKindOfClass:[GlkTextBufferWindow class]])
        return;
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
        if (winarea > largestSize) {
            largestSize = winarea;
            largestWin = win;
        }
    }

    return largestWin;
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

- (void)storeScrollOffsets {
    if (_ignoreResizes)
        return;
    for (GlkWindow *win in _gwindows.allValues)
        if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
            ((GlkTextBufferWindow *)win).pendingScrollRestore = NO;
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
@end
