#import "InfoController.h"
#import "NSString+Categories.h"
#import "Metadata.h"
#import "Compatibility.h"
#import "Game.h"
#import "Theme.h"
#import "GlkStyle.h"

#import "main.h"

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
//    "NOREPLY",         "OKAY",       "ERROR",       "HELLO",
//    "PROMPTOPEN",      "PROMPTSAVE", "NEWWIN",      "DELWIN",
//    "SIZWIN",          "CLRWIN",     "MOVETO",      "PRINT",
//    "MAKETRANSPARENT", "STYLEHINT",  "CLEARHINT",   "STYLEMEASURE",
//    "SETBGND",         "SETTITLE",   "AUTOSAVE",    "RESET",
//    "TIMER",           "INITCHAR",   "CANCELCHAR",  "INITLINE",
//    "CANCELLINE",      "SETECHO",    "TERMINATORS", "INITMOUSE",
//    "CANCELMOUSE",     "FILLRECT",   "FINDIMAGE",   "LOADIMAGE",
//    "SIZEIMAGE",       "DRAWIMAGE",  "FLOWBREAK",   "NEWCHAN",
//    "DELCHAN",         "FINDSOUND",  "LOADSOUND",   "SETVOLUME",
//    "PLAYSOUND",       "STOPSOUND",  "SETLINK",     "INITLINK",
//    "CANCELLINK",      "EVTHYPER",   "NEXTEVENT",   "EVTARRANGE",
//    "EVTLINE",         "EVTKEY",     "EVTMOUSE",    "EVTTIMER",
//    "EVTSOUND",        "EVTVOLUME",  "EVTPREFS"};

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

- (void)viewDidEndLiveResize {
    // We use a custom fullscreen width, so don't resize to full screen width
    // when viewDidEndLiveResize is called because we just entered fullscreen
    if ((_glkctrl.window.styleMask & NSFullScreenWindowMask) !=
        NSFullScreenWindowMask && !_glkctrl.ignoreResizes)
        [_glkctrl contentDidResize:self.frame];
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

    _ignoreResizes = YES;

    _game = game_;
    _theme = _game.theme;

    libcontroller = ((AppDelegate *)[NSApplication sharedApplication].delegate).libctl;

    if (!_theme.name) {
        NSLog(@"GlkController runTerp called with theme without name!");
        _game.theme = [Preferences currentTheme];
        _theme = _game.theme;
    }

    NSLog(@"runTerp: theme name:%@", _theme.name);
//    [_theme.bufferNormal printDebugInfo];
    if (_theme.doStyles == NO)
        NSLog(@"glkctl: runterp: doStyles = NO!");

    _gamefile = [_game urlForBookmark].path;
    _terpname = terpname_;

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

    _supportsAutorestore = [self.window isRestorable];
    _game.autosaved = _supportsAutorestore;
    windowRestoredBySystem = windowRestoredBySystem_;

    shouldShowAutorestoreAlert = NO;
    shouldRestoreUI = NO;
    turns = 0;

    lastArrangeValues = @{
                          @"width" : @(0),
                          @"height" : @(0),
                          @"bufferMargin" : @(0),
                          @"gridMargin" : @(0),
                          @"charWidth" : @(0),
                          @"lineHeight" : @(0),
                          @"leading" : @(0)
                          };

    _queue = [[NSMutableArray alloc] init];
    _gwindows = [[NSMutableDictionary alloc] init];
    bufferedData = nil;

    self.window.title = _game.metadata.title;
    if (NSAppKitVersionNumber >= NSAppKitVersionNumber10_12) {
        [self.window setValue:@2 forKey:@"tabbingMode"];
    }

    waitforevent = NO;
    waitforfilename = NO;
    dead = YES; // This should be YES until the interpreter process is running

    previousCharacterCellSize = _theme.gridNormal.cellSize;
    _contentSizeInChars = [self calculateContentSizeInCharsForCellSize:previousCharacterCellSize];

    _contentView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    windowdirty = NO;

    lastimageresno = -1;
    lastsoundresno = -1;
    lastimage = nil;

    _ignoreResizes = NO;

    // If we are resetting, there is a bunch of stuff that we have already done
    // and we can skip
    if (shouldReset) {
        _windowPreFullscreenFrame = self.window.frame;
        [self adjustContentView];
        [self forkInterpreterTask];
        return;
    }

    lastContentResize = NSZeroRect;
    _inFullscreen = NO;
    _windowPreFullscreenFrame = NSZeroRect;
    borderFullScreenSize = NSZeroSize;

    restoredController = nil;
    inFullScreenResize = NO;

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

    self.window.representedFilename = _gamefile;

    [_borderView setWantsLayer:YES];
    [self setBorderColor:_theme.bufferBackground];

    if (_supportsAutorestore &&
        [[NSFileManager defaultManager] fileExistsAtPath:self.autosaveFileGUI]) {
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
    if (!restoredController) {
        // If there exists an autosave file but we failed to read it,
        // delete it and run game without autorestoring
        [self deleteAutosaveFiles];
        _game.autosaved = NO;
        [self runTerpNormal];
        return;
    }

    _inFullscreen = restoredController.inFullscreen;
    _windowPreFullscreenFrame = restoredController.windowPreFullscreenFrame;

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

    // If this is not a window restoration done by the system,
    // we now re-enter fullscreen manually if the game was
    // closed in fullscreen mode.
    if (!windowRestoredBySystem && _inFullscreen && (self.window.styleMask & NSFullScreenWindowMask) !=
        NSFullScreenWindowMask) {
        [self startInFullscreen];
    } else {
        _contentView.autoresizingMask =
        NSViewMinXMargin | NSViewMaxXMargin | NSViewMinYMargin;
        [self.window setFrame:restoredController.storedWindowFrame display:YES];
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
    newContentFrame.size = [self defaultWindowSize];
    NSRect newWindowFrame = [self.window frameRectForContentRect:newContentFrame];
    NSRect screenFrame = self.window.screen.visibleFrame;
    // Make sure that the window is shorter than the screen
    if (newWindowFrame.size.height > screenFrame.size.height)
        newWindowFrame.size.height = screenFrame.size.height;
    [self.window setFrame:newWindowFrame display:NO];
    [self adjustContentView];
    [self.window center];
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
    self.window.title =
    [self.window.title stringByAppendingString:@" (finished)"];

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
                 dispatch_async(dispatch_get_main_queue(), ^{
                     [weakSelf noteDataAvailable:data];
                 });

             }];

    [task setTerminationHandler:^(NSTask *aTask) {
        [aTask.standardOutput fileHandleForReading].readabilityHandler = nil;
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf noteTaskDidTerminate:aTask];
        });
    }];

    [task launch];
    dead = NO;

    /* Send a prefs and an arrange event first thing */
    GlkEvent *gevent;

    gevent = [[GlkEvent alloc] initPrefsEvent];
    [self queueEvent:gevent];

    gevent = [[GlkEvent alloc] initArrangeWidth:(NSInteger)_contentView.frame.size.width
                                         height:(NSInteger)_contentView.frame.size.height
                                          theme:_theme
                                          force:NO];
    [self queueEvent:gevent];

}

#pragma mark Autorestore

- (void)restoreUI {
    // We try to restore the UI here, in order to catch things
    // like entered text and scrolling, that has changed the UI
    // but not sent any events to the interpreter process.
    // This method is called in handleRequest on NEXTEVENT.

    shouldRestoreUI = NO;

    _ignoreResizes = YES;

    GlkWindow *win;

    // Copy values from autorestored GlkController object
    _firstResponderView = restoredController.firstResponderView;
    _storedTimerInterval = restoredController.storedTimerInterval;
    _storedTimerLeft = restoredController.storedTimerLeft;
    _windowPreFullscreenFrame = restoredController.windowPreFullscreenFrame;

    if (restoredController.queue.count)
        NSLog(@"controller.queue contains events");
    for (GlkEvent *event in restoredController.queue)
        [self queueEvent:event];

    // Restart timer
    if (_storedTimerLeft > 0) {
        NSLog(@"storedTimerLeft:%f storedTimerInterval:%f",
              _storedTimerLeft, _storedTimerInterval);
        if (timer) {
            [timer invalidate];
            timer = nil;
        }
        timer =
        [NSTimer scheduledTimerWithTimeInterval:_storedTimerLeft
                                         target:self
                                       selector:@selector(restartTimer:)
                                       userInfo:0
                                        repeats:NO];
        NSLog(@"storedTimerLeft was %f, so started a timer.",
              _storedTimerInterval);

    } else if (_storedTimerInterval > 0) {
        [self handleSetTimer:(NSUInteger)(_storedTimerInterval * 1000)];
        NSLog(@"_storedTimerInterval was %f, so started a timer.",
              _storedTimerLeft);
    }

    // Restore frame size
    _contentView.frame = restoredController.storedContentFrame;

    // Copy all views and GlkWindow objects from restored Controller
    for (id key in restoredController.gwindows) {
        win = _gwindows[key];
        if (win)
            [win removeFromSuperview];
        win = (restoredController.gwindows)[key];

        _gwindows[@(win.name)] = win;
        [win removeFromSuperview];
        [_contentView addSubview:win];
        win.glkctl = self;
        win.theme = _theme;
        // Restore text finders
        if ([win isKindOfClass:[GlkTextBufferWindow class]])
            [(GlkTextBufferWindow *)win restoreTextFinder];
    }

    [self adjustContentView];

    _ignoreResizes = NO;

    // We create a forced arrange event in order to force the interpreter process
    // to re-send us window sizes. The player may have changed settings that affect
    // window size since the autosave was created, and at this point in the autorestore
    // process, we have no other way to know what size the Glk windows should be.
    GlkEvent *gevent = [[GlkEvent alloc] initArrangeWidth:(NSInteger)_contentView.frame.size.width
                                                   height:(NSInteger)_contentView.frame.size.height
                                                    theme:_theme
                                                    force:YES];
    [self queueEvent:gevent];
    [self notePreferencesChanged:nil];

    // Now we can actually show the window
    [self showWindow:nil];
    [self.window makeKeyAndOrderFront:nil];
    [self.window makeFirstResponder:nil];

    // Restore scroll position and focus
    for (win in [_gwindows allValues]) {
        if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
            GlkTextBufferWindow *textbuf = (GlkTextBufferWindow *)win;
            [textbuf restoreScrollBarStyle]; // Windows restoration will mess up the scrollbar style on 10.7
            if (textbuf.restoredAtBottom) {
                [textbuf scrollToBottom];
            } else {
                [textbuf scrollToCharacter:textbuf.restoredLastVisible withOffset:textbuf.restoredScrollOffset];
            }

            [textbuf storeScrollOffset];
        }
        if (win.name == _firstResponderView) {
            [win grabFocus];
        }
    }

    restoredController = nil;
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
                                     @"zcode": @"Frotz",
                                     //@"zcode" : @"Fizmo"
                                     };

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

        if (!_game.metadata.format) {
            NSLog(@"GlkController appSupportDir: Game %@ has no specified format!", _game.metadata.title);
            return nil;
        }

        NSString *terpFolder =
        [gFolderMap[_game.metadata.format]
         stringByAppendingString:@" Files"];

        if (!terpFolder) {
            NSLog(@"GlkController appSupportDir: Could not map game format %@ to a folder name!", _game.metadata.format);
            return nil; 
        }

        NSString *dirstr =
        [@"Spatterlight" stringByAppendingPathComponent:terpFolder];
        dirstr = [dirstr stringByAppendingPathComponent:@"Autosaves"];
        dirstr = [dirstr
                  stringByAppendingPathComponent:[_gamefile signatureFromFile]];
        dirstr = [dirstr
                  stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];

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
    [self deleteFileAtPath:self.autosaveFileGUI];
    [self deleteFileAtPath:self.autosaveFileTerp];
    [self deleteFileAtPath:[self.appSupportDir stringByAppendingPathComponent:
                            @"autosave.glksave"]];
    [self deleteFileAtPath:[self.appSupportDir stringByAppendingPathComponent:
                            @"autosave-tmp.glksave"]];
    [self deleteFileAtPath:[self.appSupportDir stringByAppendingPathComponent:
                            @"autosave-tmp.plist"]];
}

- (void)deleteFileAtPath:(NSString *)path {
    NSError *error;
    // I'm not sure if the fileExistsAtPath check is necessary,
    // but someone on Stack Overflow said it was a good idea
    if ([[NSFileManager defaultManager] fileExistsAtPath:path]) {
        if ([[NSFileManager defaultManager] isDeletableFileAtPath:path]) {
            BOOL success =
            [[NSFileManager defaultManager] removeItemAtPath:path
                                                       error:&error];
            if (!success) {
                NSLog(@"Error removing file at path: %@", error);
            }
        }
    }
}

- (void)autoSaveOnExit {
    NSInteger res = [NSKeyedArchiver archiveRootObject:self
                                                toFile:self.autosaveFileGUI];
    if (!res) {
        NSLog(@"GUI autosave on exit failed!");
        return;
    } else _game.autosaved = YES;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        dead = [decoder decodeBoolForKey:@"dead"];
        waitforevent = NO;
        waitforfilename = NO;

        /* the glk objects */

        _gwindows = [decoder decodeObjectForKey:@"gwindows"];

        _storedWindowFrame = [decoder decodeRectForKey:@"windowFrame"];
        _windowPreFullscreenFrame =
            [decoder decodeRectForKey:@"windowPreFullscreenFrame"];

        _storedContentFrame = [decoder decodeRectForKey:@"contentFrame"];
        _storedBorderFrame = [decoder decodeRectForKey:@"borderFrame"];

        lastimage = [decoder decodeObjectForKey:@"lastimage"];
        lastimageresno = [decoder decodeIntegerForKey:@"lastimageresno"];

        _queue = [decoder decodeObjectForKey:@"queue"];
        _storedTimerLeft = [decoder decodeDoubleForKey:@"timerLeft"];
        _storedTimerInterval = [decoder decodeDoubleForKey:@"timerInterval"];
        _firstResponderView = [decoder decodeIntegerForKey:@"firstResponder"];
        _inFullscreen = [decoder decodeBoolForKey:@"fullscreen"];

        restoredController = nil;
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];

    [encoder encodeBool:dead forKey:@"dead"];
    [encoder encodeRect:self.window.frame forKey:@"windowFrame"];
    [encoder encodeRect:_contentView.frame forKey:@"contentFrame"];
    [encoder encodeRect:_borderView.frame forKey:@"borderFrame"];

    [encoder encodeObject:_gwindows forKey:@"gwindows"];
    [encoder encodeRect:_windowPreFullscreenFrame
                 forKey:@"windowPreFullscreenFrame"];
    [encoder encodeObject:_queue forKey:@"queue"];
    _storedTimerLeft = 0;
    _storedTimerInterval = 0;
    if (timer && timer.isValid) {
        _storedTimerLeft =
        [[timer fireDate] timeIntervalSinceDate:[[NSDate alloc] init]];
        _storedTimerInterval = [timer timeInterval];
    }
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
    [encoder encodeDouble:_storedTimerLeft forKey:@"timerLeft"];
    [encoder encodeDouble:_storedTimerInterval forKey:@"timerInterval"];
    [encoder encodeBool:((self.window.styleMask & NSFullScreenWindowMask) ==
                         NSFullScreenWindowMask)
                 forKey:@"fullscreen"];

    [encoder encodeObject:lastimage forKey:@"lastimage"];
    [encoder encodeInteger:lastimageresno forKey:@"lastimageresno"];
}

- (void)showAutorestoreAlert {

    shouldShowAutorestoreAlert = NO;

    NSAlert *anAlert = [[NSAlert alloc] init];
    anAlert.messageText =
    @"This game was automatically restored from a previous session.";
    anAlert.informativeText = @"Would you like to start over instead?";
    anAlert.showsSuppressionButton = YES;
    anAlert.suppressionButton.title = @"Remember this choice.";
    [anAlert addButtonWithTitle:@"Continue"];
    [anAlert addButtonWithTitle:@"Restart"];

    [anAlert beginSheetModalForWindow:self.window
                        modalDelegate:self
                       didEndSelector:@selector(autorestoreAlertDidFinish:
                                                rc:ctx:)
                          contextInfo:NULL];
}

- (void)autorestoreAlertDidFinish:(id)alert rc:(int)result ctx:(void *)ctx {

    NSAlert *anAlert = alert;
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
}

- (IBAction)reset:(id)sender {
    if (timer) {
//        NSLog(@"glkctl reset: force stop the timer");
        [timer invalidate];
        timer = nil;
    }

    if (task) {
//        NSLog(@"glkctl reset: force stop the interpreter");
        [task setTerminationHandler:nil];
        [task.standardOutput fileHandleForReading].readabilityHandler = nil;
        [task terminate];
        task = nil;
    }

    [self deleteAutosaveFiles];

    [self runTerp:(NSString *)_terpname
         withGame:(Game *)_game
            reset:YES
       winRestore:NO];

    [self.window makeKeyAndOrderFront:nil];
    [self.window makeFirstResponder:nil];
    [self guessFocus];
}

- (void)handleAutosave:(int)hash {
    NSInteger res = [NSKeyedArchiver archiveRootObject:self
                                                toFile:self.autosaveFileGUI];

    if (!res) {
        NSLog(@"Window serialize failed!");
        return;
    }
    // NSLog(@"Autosave request: %d", hash);
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

- (void)closeAlertDidFinish:(id)alert rc:(int)rc ctx:(void *)ctx {
    if (rc == NSAlertFirstButtonReturn) {
        if (((NSAlert *)alert).suppressionButton.state == NSOnState) {
            // Suppress this alert from now on
            [[NSUserDefaults standardUserDefaults]
             setBool:YES
             forKey:@"closeAlertSuppression"];
        }
        [self windowWillClose:nil];
        [self close];
    }
}

- (void)windowDidBecomeKey:(NSNotification *)notification {
    [Preferences changeCurrentGame:_game];
    if (!dead) {
        [self guessFocus];
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
    alert.messageText = @"Do you want to abandon the game?";
    alert.informativeText = @"Any unsaved progress will be lost.";
    alert.showsSuppressionButton = YES; // Uses default checkbox title

    [alert addButtonWithTitle:@"Close"];
    [alert addButtonWithTitle:@"Cancel"];

    [alert beginSheetModalForWindow:self.window
                      modalDelegate:self
                     didEndSelector:@selector(closeAlertDidFinish:rc:ctx:)
                        contextInfo:NULL];

    return NO;
}


- (void)windowWillClose:(id)sender {
    NSLog(@"glkctl (game %@): windowWillClose", _game ? _game.metadata.title : @"nil");
    if (windowClosedAlready) {
        NSLog(@"windowWillClose called twice!");
        return;
    } else windowClosedAlready = YES;

    if (_supportsAutorestore) {
        [self autoSaveOnExit];
    }

    if (_game.ifid)
        [libcontroller.gameSessions removeObjectForKey:_game.ifid];

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
    //[self.window setDelegate:nil]; This segfaults
}

- (void)flushDisplay {
    for (GlkWindow *win in [_gwindows allValues])
        [win flushDisplay];

    if (windowdirty) {
        [_contentView setNeedsDisplay:YES];
        windowdirty = NO;
    }
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
    for (GlkWindow *win in [_gwindows allValues])
        [win markLastSeen];
}

- (void)performScroll {
    for (GlkWindow *win in [_gwindows allValues])
        if ([win isKindOfClass:[GlkTextBufferWindow class]])
            [win performScroll];
}

#pragma mark Window resizing

- (NSRect)windowWillUseStandardFrame:(NSWindow *)window
                        defaultFrame:(NSRect)defaultFrame {
    // NSLog(@"glkctl: windowWillUseStandardFrame");

    NSSize windowSize = [self defaultWindowSize];
    CGRect screenframe = window.screen.visibleFrame;

    if (windowSize.width > screenframe.size.width)
        windowSize.width = screenframe.size.width;

    NSRect frame = NSMakeRect((NSWidth(screenframe) - windowSize.width) / 2, 0,
                              windowSize.width, NSHeight(screenframe));

    return frame;
}

- (NSSize)defaultWindowSize {
    CGFloat width = round(_theme.cellWidth * _theme.defaultCols + (_theme.gridMarginX + _theme.border + 4.0) * 2.0);
    CGFloat height = round(_theme.cellHeight * _theme.defaultRows + (_theme.gridMarginY + _theme.border + 4.0) * 2.0);

    return NSMakeSize(width, height);
}

- (NSSize)calculateContentSizeInCharsForCellSize:(NSSize)cellSize {
    CGFloat width = round((_contentView.frame.size.width - (_theme.gridMarginX + 5.0) * 2.0) / cellSize.width);
    CGFloat height = round((_contentView.frame.size.height - (_theme.gridMarginY) * 2.0) / cellSize.height);

    if (width < 10 || height < 2) {
        NSLog(@"calculateContentSizeInChars: Error!");
        NSLog(@"calculateContentSizeInChars: width: %f", width);
        CGFloat margins = (_theme.gridMarginX + 5.0) * 2.0;
        NSLog(@"_contentView.frame.size.width (%f) - ((_theme.gridMarginX (%d) + 5.0 (padding)) * 2.0) (%f) /  _theme.cellWidth (%f))", _contentView.frame.size.width, _theme.gridMarginX, margins, _theme.cellWidth);
        NSLog(@"calculateContentSizeInChars: height: %f", height);
        margins = (_theme.gridMarginY) * 2.0;
        NSLog(@"_contentView.frame.size.height (%f) - (_theme.gridMarginY (%d) * 2.0) (%f) /  _theme.cellHeight (%f))", _contentView.frame.size.height, _theme.gridMarginY, margins, _theme.cellHeight);
        return _contentSizeInChars;
    }

    return NSMakeSize(width, height);;
}

- (void)contentDidResize:(NSRect)frame {
    if (NSEqualRects(frame, lastContentResize)) {
        //        NSLog(
        //            @"contentDidResize called with same frame as last time. Skipping.");
        return;
    }

//    NSLog(@"glkctl: contentDidResize: frame:%@ Previous _contentView.frame:%@",
//          NSStringFromRect(frame), NSStringFromRect(lastContentResize));

    if (frame.origin.x < 0 || frame.origin.y < 0 || frame.size.width < 0 || frame.size.height < 0) {
        NSLog(@"contentDidResize: weird new frame: %@", NSStringFromRect(frame));
    }

    lastContentResize = frame;

    NSSize oldContentSizeInChars = _contentSizeInChars;
    _contentSizeInChars = [self calculateContentSizeInCharsForCellSize:_theme.gridNormal.cellSize];

    if (!NSEqualSizes(oldContentSizeInChars, _contentSizeInChars)) {
        NSLog(@"Old contentSizeInChars: %@ new: %@", NSStringFromSize(oldContentSizeInChars), NSStringFromSize(_contentSizeInChars));
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

    if ((self.window.styleMask & NSFullScreenWindowMask) !=
        NSFullScreenWindowMask) {
        NSRect screenframe = [NSScreen mainScreen].visibleFrame;

        NSRect contentRect = NSMakeRect(0, 0, newSize.width, newSize.height);

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

        if (NSMinY(winrect) < NSMinY(screenframe)) {
            NSLog(@"NSMinY(winrect) (%f) < NSMinY(screenframe) (%f)", NSMinY(winrect), NSMinY(screenframe));
            winrect.origin.y = NSMinY(screenframe);
        }

        [self.window setFrame:winrect display:NO animate:NO];
        _contentView.frame = [self contentFrameForWindowed];
    } else {
        NSUInteger borders = (NSUInteger)_theme.border * 2;
        NSRect newframe = NSMakeRect(oldframe.origin.x, oldframe.origin.y,
                                     newSize.width - borders,
                                     NSHeight(_borderView.frame) - borders);

        if (NSWidth(newframe) > NSWidth(_borderView.frame) - borders)
            newframe.size.width = NSWidth(_borderView.frame) - borders;

        newframe.origin.x += (NSWidth(oldframe) - NSWidth(newframe)) / 2;

        CGFloat offset = NSHeight(newframe) - NSHeight(oldframe);
        newframe.origin.y -= offset;

        _contentView.frame = newframe;
    }
    _ignoreResizes = NO;
}

/*
 *
 */

#pragma mark Preference and style hint glue

- (void)notePreferencesChanged:(NSNotification *)notify {

    // If this is the GlkController of the sample text in
    // the preferences window, we use the _dummyTheme property
    if (_game) {
        NSLog(@"glkctl notePreferencesChanged called for game %@, currently using theme %@", _game.metadata.title, _game.theme.name);
        _theme = _game.theme;
    } else {
        _theme = [Preferences currentTheme];
        dead = YES;
    }

    if (notify.object != _theme && notify.object != nil) {
        NSLog(@"glkctl: PreferencesChanged called for a different theme (was %@, listening for %@)", ((Theme *)notify.object).name, _theme.name);
        return;
    } else if ( notify.object == nil) {
        NSLog(@"glkctl: PreferencesChanged with a nil object.");
    }

    NSSize newCellSize = _theme.gridNormal.cellSize;
    NSSize newSize = [self calculateContentSizeInCharsForCellSize:newCellSize];
    NSSize oldSize = [self calculateContentSizeInCharsForCellSize:previousCharacterCellSize];

    if (!NSEqualSizes(previousCharacterCellSize, newCellSize)) {
        NSLog(@"Cell size changed!");

        NSLog(@"Old cell size: %@ New cell size: %@ Old content size in characters: %@", NSStringFromSize(previousCharacterCellSize), NSStringFromSize(newCellSize), NSStringFromSize(oldSize));

        _contentSizeInChars = [self calculateContentSizeInCharsForCellSize:previousCharacterCellSize];


        if (!NSEqualSizes(oldSize, newSize)) {
            [self zoomContentToSize:NSMakeSize(round(_contentSizeInChars.width * _theme.cellWidth + (_theme.gridMarginX + _theme.border + 5.0) * 2), round(_contentSizeInChars.height * _theme.cellHeight + (_theme.gridMarginY + _theme.border) * 2))];

            NSLog(@"self contentFrameForWindowed: %@ Current content frame: %@", NSStringFromRect([self contentFrameForWindowed]), NSStringFromRect(_contentView.frame));

            newSize = [self calculateContentSizeInCharsForCellSize:newCellSize];

            if (!NSEqualSizes(oldSize, newSize)) {
                NSLog(@"Old size in chars: %@ New, adjusted size in chars: %@", NSStringFromSize(oldSize), NSStringFromSize(newSize));
                NSLog(@"Nope, didn't work. Should have been the same.");
            }
        }

        _contentSizeInChars = newSize;
        previousCharacterCellSize = newCellSize;
    }

//    if (!NSEqualSizes(_contentSizeInChars, newSize)) {
//        NSLog(@"Content size in chars changed!");
//        [self zoomContentToSize:NSMakeSize(round(newSize.width * _theme.cellWidth + (_theme.gridMarginX + _theme.border + 5.0) * 2), round(newSize.height * _theme.cellHeight + (_theme.gridMarginY + _theme.border) * 2))];
//        _contentSizeInChars = newSize;
//    }
//

    [self adjustContentView];

    if (!dead) {
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
                                              force:NO];
        [self queueEvent:gevent];

        gevent = [[GlkEvent alloc] initPrefsEvent];
        [self queueEvent:gevent];
    }

    if (!_gwindows.count) {
        NSLog(@"glkctl: notePreferencesChanged called with no _gwindows");
    }

    for (GlkWindow *win in [_gwindows allValues])
    {
        win.theme = _theme;
        [win prefsDidChange];
    }
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

    NSSize sizeAfterZoom = [self defaultWindowSize];
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
        panel.prompt = @"Restore";
    panel.directoryURL = directory;

    // Display the panel attached to the document's window.
    [panel beginSheetModalForWindow:self.window
                  completionHandler:^(NSInteger result) {
                      const char *s;
                      struct message reply;

                      if (result == NSFileHandlingPanelOKButton) {
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
            filename = @"Recordning of ";
            break;
        default:
            prompt = @"Save: ";
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

                      if (result == NSFileHandlingPanelOKButton) {
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
            [_contentView addSubview:win];

            win.styleHints = _gridStyleHints;
            return i;

        case wintype_TextBuffer:
             win = [[GlkTextBufferWindow alloc] initWithGlkController:self name:i];

            _gwindows[@(i)] = win;
            [_contentView addSubview:win];

            win.styleHints = _bufferStyleHints;
            
            return i;

        case wintype_Graphics:
             win = [[GlkGraphicsWindow alloc] initWithGlkController:self name:i];
            _gwindows[@(i)] = win;
            [_contentView addSubview: win];
            return i;
    }

    return -1;
}

- (int)handleNewSoundChannel {
    //    int i;
    //
    //    for (i = 0; i < MAXSND; i++)
    //        if (gchannels[i] == nil)
    //            break;
    //
    //    if (i == MAXSND)
    //        return -1;
    //
    //    gchannels[i] = [[GlkSoundChannel alloc] initWithGlkController: self
    //    name: i];
    //
    return MAXSND;
}

- (void)handleSetTimer:(NSUInteger)millisecs {
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

- (void)handleLoadSoundNumber:(int)resno
                         from:(char *)buffer
                       length:(NSUInteger)length {
    lastsoundresno = -1;

    if (lastsound) {
        lastsound = nil;
    }

    lastsound = [[NSData alloc] initWithBytes:buffer length:length];
    if (lastsound)
        lastsoundresno = resno;
}

- (void)handleLoadImageNumber:(int)resno
                         from:(char *)buffer
                       length:(NSUInteger)length {
    lastimageresno = -1;

    if (lastimage) {
        lastimage = nil;
    }

    NSData *data = [[NSData alloc] initWithBytesNoCopy:buffer
                                                length:length
                                          freeWhenDone:NO];
    if (!data)
        return;

    NSArray *reps = [NSBitmapImageRep imageRepsWithData:data];

    NSSize size = NSZeroSize;

    for (NSImageRep *imageRep in reps) {
        if (imageRep.pixelsWide > size.width)
            size.width = imageRep.pixelsWide;
        if (imageRep.pixelsHigh > size.height)
            size.height = imageRep.pixelsHigh;
    }

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

    NSData *tiffdata = lastimage.TIFFRepresentation;

    lastimage = [[NSImage alloc] initWithData:tiffdata];
    lastimage.size = size;

    lastimageresno = resno;
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

NSInteger colorToInteger(NSColor *color) {
    CGFloat r, g, b, a;
    uint32_t buf[3];
    NSInteger i;
    color = [color colorUsingColorSpaceName:NSCalibratedRGBColorSpace];

    [color getRed:&r green:&g blue:&b alpha:&a];

    buf[0] = (uint32_t)(r * 255);
    buf[1] = (uint32_t)(g * 255);
    buf[2] = (uint32_t)(b * 255);

    i = buf[2] + (buf[1] << 8) + (buf[0] << 16);
    return i;
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
                *result = colorToInteger(_theme.bufferNormal.color);
            else
                *result = colorToInteger(_theme.gridNormal.color);

            return YES;
        }
        if (hint == stylehint_BackColor) {
            if ([gwindow isKindOfClass:[GlkTextBufferWindow class]])
                *result = colorToInteger(_theme.bufferBackground);
            else
                *result = colorToInteger(_theme.gridBackground);

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
        (style & 0xff) != style_Preformatted) {
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
            NSLog(@"Illegal line terminator request: %u", buf[i]);
    }
    gwindow.terminatorsPending = YES;
}

- (void)handleChangeTitle:(char *)buf length:(int)len {
    buf[len] = '\0';
    NSString *str = @(buf);
    [@(buf) substringToIndex:(NSUInteger)len];
    // self.window.title = str;
    NSLog(@"Change title request: %@", str);
}

- (BOOL)handleRequest:(struct message *)req
                reply:(struct message *)ans
               buffer:(char *)buf {
//    NSLog(@"glkctl: incoming request %s", msgnames[req->cmd]);

    NSInteger result;
    GlkWindow *reqWin = nil;

    if (req->a1 >= 0 && req->a1 < MAXWIN && _gwindows[@(req->a1)])
        reqWin = _gwindows[@(req->a1)];

    switch (req->cmd) {
        case HELLO:
            ans->cmd = OKAY;
            ans->a1 = (int)[Preferences graphicsEnabled];
            ans->a2 = (int)[Preferences soundEnabled];
            break;

        case NEXTEVENT:

            // If this is the first turn, we try to restore the UI from an autosave
            // file, in order to catch things like entered text and scrolling, that
            // has changed the UI but not sent any events to the interpreter
            // process.

            if (shouldRestoreUI && turns == 2) {
                [self restoreUI];
                if (shouldShowAutorestoreAlert)
                    [self showAutorestoreAlert];
            }

            turns++;

            [self flushDisplay];

            if (_queue.count) {
                GlkEvent *gevent;
                gevent = _queue[0];
//            NSLog(@"glkctl: writing queued event %s", msgnames[[gevent type]]);

                [gevent writeEvent:sendfh.fileDescriptor];
                [_queue removeObjectAtIndex:0];
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
            ans->a1 = [self handleNewSoundChannel];
            break;

        case DELWIN:
//            NSLog(@"glkctl delwin %d", req->a1);
            if (reqWin) {
                [reqWin removeFromSuperview];
                reqWin = nil;
                [_gwindows removeObjectForKey:@(req->a1)];
            } else
                NSLog(@"delwin: something went wrong.");

            break;

        case DELCHAN:
            //            if (req->a1 >= 0 && req->a1 < MAXSND &&
            //            gchannels[req->a1])
            //            {
            //                gchannels[req->a1] = nil;
            //            }
            break;

            /*
             * Load images; load and play sounds
             */

#pragma mark Load images; load and play sounds

        case FINDIMAGE:
            ans->cmd = OKAY;
            ans->a1 = lastimageresno == req->a1;
            break;

        case FINDSOUND:
            ans->cmd = OKAY;
            ans->a1 = lastsoundresno == req->a1;
            break;

        case LOADIMAGE:
            buf[req->len] = 0;
            [self handleLoadImageNumber:req->a1 from:buf length:req->len];
            break;

        case SIZEIMAGE:
            ans->cmd = OKAY;
            ans->a1 = 0;
            ans->a2 = 0;
            if (lastimage) {
                NSSize size;
                size = lastimage.size;
                ans->a1 = (NSUInteger)size.width;
                ans->a2 = (NSUInteger)size.height;
            }
            break;

        case LOADSOUND:
            buf[req->len] = 0;
            [self handleLoadSoundNumber:req->a1 from:buf length:req->len];
            break;

        case SETVOLUME:
            //            if (req->a1 >= 0 && req->a1 < MAXSND &&
            //            gchannels[req->a1])
            //            {
            //                [gchannels[req->a1] setVolume: req->a2];
            //            }
            break;

        case PLAYSOUND:
            //            if (req->a1 >= 0 && req->a1 < MAXSND &&
            //            gchannels[req->a1])
            //            {
            //                if (lastsound)
            //                    [gchannels[req->a1] play: lastsound repeats:
            //                    req->a2 notify: req->a3];
            //            }
            break;

        case STOPSOUND:
            //            if (req->a1 >= 0 && req->a1 < MAXSND &&
            //            gchannels[req->a1])
            //            {
            //                [gchannels[req->a1] stop];
            //            }
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
                if ([reqWin isKindOfClass:[GlkTextBufferWindow class]])
                    [(GlkTextBufferWindow *)reqWin restoreScroll];

                NSInteger hmask = NSViewMaxXMargin;
                NSInteger vmask = NSViewMaxYMargin;

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
            }
            break;

        case SETBGND:
            if (reqWin) {
                if (![reqWin isKindOfClass:[GlkGraphicsWindow class]]) {
                    NSLog(
                          @"glkctl: SETBGND: ERROR win %d is not a graphics window.",
                          req->a1);
                    break;
                }

                [reqWin setBgColor:req->a2];
            }
            break;

        case DRAWIMAGE:
            if (reqWin) {
                if (lastimage && !NSEqualSizes(lastimage.size, NSZeroSize)) {
                    [reqWin drawImage:lastimage
                                 val1:req->a2
                                 val2:req->a3
                                width:req->a4
                               height:req->a5];
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

        case MOVETO:
            if (reqWin) {
                int x = req->a2;
                int y = req->a3;
                if (x < 0)
                    x = 10000;
                if (y < 0)
                    y = 10000;
                [reqWin moveToColumn:(NSUInteger)x row:(NSUInteger)y];
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
            NSLog(@"glkctl: WEE! WE GOT A FLOWBREAK! ^^;");
            if (reqWin) {
                [reqWin flowBreak];
            }
            break;

#pragma mark Request and cancel events

        case INITLINE:
            // NSLog(@"glkctl INITLINE %d", req->a1);
            [self performScroll];
            if (reqWin) {
                [reqWin initLine:[[NSString alloc]
                                  initWithData:[NSData dataWithBytes:buf
                                                              length:req->len]
                                  encoding:NSUTF8StringEncoding]];
            }
            break;

        case CANCELLINE:
//            NSLog(@"glkctl CANCELLINE %d", req->a1);
            ans->cmd = OKAY;
            if (reqWin) {
                const char *str = [reqWin cancelLine].UTF8String;
                strlcpy(buf, str, GLKBUFSIZE);
                ans->len = strlen(buf);
            }
            break;

        case INITCHAR:
            //            NSLog(@"glkctl initchar %d", req->a1);
            if (reqWin)
                [reqWin initChar];
            break;

        case CANCELCHAR:
            //            NSLog(@"glkctl CANCELCHAR %d", req->a1);
            if (reqWin)
                [reqWin cancelChar];
            break;

        case INITMOUSE:
            //            NSLog(@"glkctl initmouse %d", req->a1);
            [self performScroll];
            if (reqWin)
                [reqWin initMouse];
            break;

        case CANCELMOUSE:
            if (reqWin)
                [reqWin cancelMouse];
            break;

        case SETLINK:
            //            NSLog(@"glkctl set hyperlink %d in window %d", req->a2,
            //            req->a1);
            if (reqWin) {
                [reqWin setHyperlink:(NSUInteger)req->a2];
            }
            break;

        case INITLINK:
            //            NSLog(@"glkctl request hyperlink event in window %d",
            //            req->a1);
            [self performScroll];
            if (reqWin) {
                [reqWin initHyperlink];
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
            NSLog(@"glkctl EVTSOUND %d, %d. Send it back to where it came from.",
                  req->a2, req->a3);
            [self handleSoundNotification:req->a3 withSound:req->a2];
            break;

        case EVTVOLUME:
            NSLog(@"glkctl EVTVOLUME %d. Send it back where it came.", req->a3);
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

    [task waitUntilExit];

    if (task && task.terminationStatus != 0) {
        NSAlert *alert;
        alert = [NSAlert
                 alertWithMessageText:@"The game has unexpectedly terminated."
                 defaultButton:@"Oops"
                 alternateButton:nil
                 otherButton:nil
                 informativeTextWithFormat:@"Error code: %@.", signalToName(task)];

        [alert beginSheetModalForWindow:self.window
                          modalDelegate:nil
                         didEndSelector:nil
                            contextInfo:nil];
    }

    for (GlkWindow *win in [_gwindows allValues])
        [win terpDidStop];

    self.window.title = [self.window.title stringByAppendingString:@" (finished)"];
    task = nil;

    // We autosave the UI but delete the terp autosave files
    [self autoSaveOnExit];
    [self deleteFileAtPath:self.autosaveFileTerp];
    [self deleteFileAtPath:[self.appSupportDir stringByAppendingPathComponent:
                            @"autosave.glksave"]];
    [self deleteFileAtPath:[self.appSupportDir stringByAppendingPathComponent:
                            @"autosave-tmp.glksave"]];
    [self deleteFileAtPath:[self.appSupportDir stringByAppendingPathComponent:
                            @"autosave-tmp.plist"]];
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

    @try {
        [data getBytes:&request
                 range:rangeToRead];
    } @catch (NSException *ex) {
        NSLog(@"glkctl: could not read message header");
        [task terminate];
        return;
    }

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

        @try {
            [data getBytes:buf
                     range:rangeToRead];
        } @catch (NSException *ex) {
            NSLog(@"glkctl: could not read message body");
            if (maxibuf)
                free(maxibuf);
            [task terminate];
            return;
        }
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

- (void)setBorderColor:(NSColor *)color;
{
    CGFloat components[[color numberOfComponents]];
    CGColorSpaceRef colorSpace = [[color colorSpace] CGColorSpace];
    [color getComponents:(CGFloat *)&components];
    CGColorRef cgcol = CGColorCreate(colorSpace, components);

    _borderView.layer.backgroundColor = cgcol;
    self.window.backgroundColor = color;
    CFRelease(cgcol);
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
}

- (void)windowDidFailToEnterFullScreen:(NSWindow *)window {
    _inFullscreen = NO;
    inFullScreenResize = NO;
    _contentView.alphaValue = 1;
    snapshotWindow = nil;
    [window setFrame:_windowPreFullscreenFrame display:YES];
    _contentView.frame = [self contentFrameForWindowed];
}

- (void)storeScrollOffsets {
    for (GlkWindow *win in [_gwindows allValues])
        if ([win isKindOfClass:[GlkTextBufferWindow class]])
            [(GlkTextBufferWindow *)win storeScrollOffset];
}

- (void)restoreScrollOffsets {
    for (GlkWindow *win in [_gwindows allValues])
        if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
            [(GlkTextBufferWindow *)win restoreScrollBarStyle];
            [(GlkTextBufferWindow *)win restoreScroll];
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
    NSWindow __unsafe_unretained *localSnapshot = snapshotWindow;

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
                         floor(NSHeight(localBorderView.bounds) - _theme.border - localContentView.frame.size.height)
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
                                  theme:_theme
                                  force:NO];

              [weakSelf queueEvent:gevent];

              if (stashShouldShowAlert)
                  [weakSelf showAutorestoreAlert];
              [weakSelf restoreScrollOffsets];
          }];
     }];
}

- (void)enableArrangementEvents {
    inFullScreenResize = NO;
}

- (void)window:window startCustomAnimationToExitFullScreenWithDuration:(NSTimeInterval)duration {
    [self storeScrollOffsets];
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
}
- (void)windowDidExitFullScreen:(NSNotification *)notification {
    _inFullscreen = NO;
}

- (void)startInFullscreen {
    [self.window setFrame:_windowPreFullscreenFrame
                  display:YES];

    _contentView.frame = [self contentFrameForWindowed];

    _contentView.autoresizingMask = NSViewMinXMargin | NSViewMaxXMargin |
    NSViewMinYMargin; // Attached at top but not bottom or sides

    [self showWindow:nil];
    [self.window makeKeyAndOrderFront:nil];
    [self.window toggleFullScreen:nil];
}

- (CALayer *)takeSnapshot {
    [self showWindow:nil];
    CGImageRef windowSnapshot = CGWindowListCreateImage(
                                                        CGRectNull, kCGWindowListOptionIncludingWindow,
                                                        [self.window windowNumber], kCGWindowImageBoundsIgnoreFraming);
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

- (NSString *)accessibilityActionDescription:(NSString *)action {
    return [self.window accessibilityActionDescription:action];
}

- (NSArray *)accessibilityActionNames {
    return [self.window accessibilityActionNames];
}

- (BOOL)accessibilityIsAttributeSettable:(NSString *)attribute {
    return [self.window accessibilityIsAttributeSettable:attribute];
}

- (void)accessibilityPerformAction:(NSString *)action {
    [self.window accessibilityPerformAction:action];
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString *)attribute {
    [self.window accessibilitySetValue:value forAttribute:attribute];
}

- (NSArray *)accessibilityAttributeNames {
    NSMutableArray *result =
    [[self.window accessibilityAttributeNames] mutableCopy];
    if (!result)
        result = [[NSMutableArray alloc] init];

    [result addObjectsFromArray:@[ NSAccessibilityContentsAttribute, NSAccessibilityChildrenAttribute,
                                   NSAccessibilityHelpAttribute, NSAccessibilityDescriptionAttribute,
                                   NSAccessibilityTitleAttribute, NSAccessibilityFocusedUIElementAttribute ]];
    return result;
}

- (id)accessibilityFocusedUIElement {
    NSResponder *firstResponder = self.window.firstResponder;

    if (firstResponder == nil)
        return self;

    if ([firstResponder isKindOfClass:[NSView class]]) {
        NSView *windowView = (NSView *)firstResponder;

        while (windowView != nil) {
            if ([windowView isKindOfClass:[GlkWindow class]]) {
                return windowView;
            }

            windowView = windowView.superview;
        }
    }

    return super.accessibilityFocusedUIElement;
}

- (id)accessibilityAttributeValue:(NSString *)attribute {
    if ([attribute isEqualToString:NSAccessibilityChildrenAttribute] ||
        [attribute isEqualToString:NSAccessibilityContentsAttribute]) {
        // return [NSArray arrayWithObjects:gwindows count:MAXWIN];
        // return [NSArray arrayWithObject: rootWindow];
    } else if ([attribute
                isEqualToString:NSAccessibilityFocusedUIElementAttribute]) {
        return self.accessibilityFocusedUIElement;
    } else if ([attribute isEqualToString:NSAccessibilityHelpAttribute] ||
               [attribute
                isEqualToString:NSAccessibilityDescriptionAttribute]) {
                   NSString *description = @"an interactive fiction game";
                   return [NSString stringWithFormat:@"%@ %@",
                           (!dead) ? @"Running" : @"Finished",
                           description];
               } else if ([attribute
                           isEqualToString:NSAccessibilityRoleDescriptionAttribute]) {
                   return @"GLK view";
               } else if ([attribute isEqualToString:NSAccessibilityRoleAttribute]) {
                   return NSAccessibilityGroupRole;
               } else if ([attribute isEqualToString:NSAccessibilityParentAttribute]) {
                   return self.window;
               }

    NSLog(@"%@", attribute);

    return [super accessibilityAttributeValue:attribute];
}

- (BOOL)accessibilityIsIgnored {
    return NO;
}

@end
