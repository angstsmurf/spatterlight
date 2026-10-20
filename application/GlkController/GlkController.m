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
#import "BuiltInThemes.h"
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


#import "DebugLog.h"

#import "GlkController_Private.h"
#import "NSAlert+Spatterlight.h"

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
    // Warm up the audio output device now, while the game is still booting, so
    // the first synthesised ZX Spectrum BEEP (e.g. a Quill game's intro tune)
    // plays on an already-running device instead of being clipped by CoreAudio
    // cold-start latency.
    [_soundHandler primeBeepAudio];
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

    [self adoptFallbackThemeIfNeeded];

    // Identify the specific game being played so we can apply game-specific
    // workarounds and special behaviors (e.g., Narcolepsy window mask,
    // Beyond Zork font handling, Journey menu system). The "nohacks" theme
    // option disables all game-specific behavior.
    [self identifyGameUnlessNoHacks:_theme];

    // Adopt the restoration handler BEFORE anything below can bail out. The
    // game-file-gone path just below needs it, and a reset re-enters here with
    // a pending handler from the launch that must be closed out (see below),
    // so neither can wait for the autorestore block further down.
    //
    // A reset (the interpreter rejecting its own autosave: win_reset, which
    // arrives as RESET) re-enters runTerp with completionHandler == nil. Any
    // restoration still outstanding from the original launch belongs to the
    // window that is about to be reused, so finish it rather than dropping it
    // on the floor -- AppKit is otherwise never told this restoration
    // completed, and its bookkeeping for the window is left hanging.
    if (_restorationHandler && completionHandler == nil)
        [self runWindowsRestorationHandler];
    windowRestoredBySystem = (completionHandler != nil);
    _restorationHandler = completionHandler;

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
                // Tell AppKit the restoration is over (with no window: the
                // game file is gone, so there is nothing to restore).
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

    [self resetStyleHints];

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

    _shouldShowAutorestoreAlert = NO;
    shouldRestoreUI = NO;
    _eventcount = 0;
    _turns = 0;
    _turnsThisSession = 0;
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
    // Not "stopped": we are about to fork an interpreter. A reset re-enters
    // here, so this must be cleared or the restarted game would refuse input.
    terpHasStopped = NO;

    _gameView.autoresizingMask = kResizeFill;

    windowdirty = NO;

    _ignoreResizes = NO;
    _shouldScrollOnCharEvent = NO;

    _quoteBoxes = nil;
    _stashedTheme = nil;

    // If we are resetting, there is a bunch of stuff that we have already done
    // and we can skip
    if (shouldReset) {
        // An interpreter that rejects its autosave resets us in the middle of
        // an autorestore, before postRestoreArrange: has taken a game that
        // was saved in fullscreen back there. The window then stays as it is,
        // so stop counting it as being in fullscreen.
        if (!self.isInSystemFullscreen)
            _inFullscreen = NO;
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

    [self registerForNotifications];

    lastContentResize = NSZeroRect;

    restoredController = nil;
    windowsClosedBeforeRestore = nil;
    inFullScreenResize = NO;

    self.window.representedFilename = _gamefile;

    [self setUpBorderAndGameView];

    lastScriptKeyTimestamp = [NSDate distantPast];
    lastKeyTimestamp = [NSDate distantPast];

    // Narcolepsy uses a special image-based mask on the game view to create
    // a shaped window effect. Only applied when graphics and styles are on.
    if (self.gameID == kGameIsNarcolepsy && _theme.doGraphics && _theme.doStyles) {
        [self adjustMaskLayer:nil];
    }

    [self startInterpreterOrAutorestore];
}

// A game whose theme has no name (it has none, or it was deleted) gets the
// current theme, or a default one if the game lives in another store.
- (void)adoptFallbackThemeIfNeeded {
    if (_theme.name)
        return;
    NSLog(@"GlkController runTerp called with theme without name!");
    Game *game = _game;
    Theme *currentTheme = Preferences.currentTheme;
    NSManagedObjectContext *gameContext = game.managedObjectContext;
    if (currentTheme) {
        if (gameContext.persistentStoreCoordinator ==
            currentTheme.managedObjectContext.persistentStoreCoordinator)
            game.theme = [gameContext objectWithID:currentTheme.objectID];
        else if (gameContext)
            // A game from another store (a test's in-memory one) cannot
            // borrow the library's theme: objectWithID: would raise.
            game.theme = [BuiltInThemes createDefaultThemeInContext:gameContext forceRebuild:NO];
    }
    _theme = game.theme;
}

// Initialize style hint arrays for both text grid and text buffer windows.
// Glk style hints allow games to override default text appearance (font
// weight, color, indentation, etc.) per style. Each window type gets its
// own style_NUMSTYLES x stylehint_NUMHINTS matrix, initially filled with
// NSNull to indicate "no override" for each hint slot.
- (void)resetStyleHints {
    NSMutableArray *nullarray = [NSMutableArray arrayWithCapacity:stylehint_NUMHINTS];
    for (NSInteger i = 0; i < stylehint_NUMHINTS; i++)
        [nullarray addObject:[NSNull null]];

    _gridStyleHints = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];
    _bufferStyleHints = [NSMutableArray arrayWithCapacity:style_NUMSTYLES];
    for (NSInteger i = 0; i < style_NUMSTYLES; i++) {
        [_gridStyleHints addObject:[nullarray mutableCopy]];
        [_bufferStyleHints addObject:[nullarray mutableCopy]];
    }
}

// VoiceOver, preference, theme and Core Data observers. Removed in
// windowWillClose:.
- (void)registerForNotifications {
    _voiceOverActive = [NSWorkspace sharedWorkspace].voiceOverEnabled;

    if (!observingVoiceOver) {
        [[NSWorkspace sharedWorkspace] addObserver:self forKeyPath:@"voiceOverEnabled" options:(NSKeyValueObservingOptionNew | NSKeyValueObservingOptionOld) context:nil];
        observingVoiceOver = YES;
    }

    NSNotificationCenter *notifications = [NSNotificationCenter defaultCenter];

    [notifications
     addObserver:self
     selector:@selector(notePreferencesChanged:)
     name:kPreferencesChangedNotification
     object:nil];

    [notifications
     addObserver:self
     selector:@selector(noteDefaultSizeChanged:)
     name:kDefaultSizeChangedNotification
     object:nil];

    [notifications
     addObserver:self
     selector:@selector(noteBorderChanged:)
     name:kBorderChangedNotification
     object:nil];

    [notifications
     addObserver:self
     selector:@selector(noteManagedObjectContextDidChange:)
     name:NSManagedObjectContextObjectsDidChangeNotification
     object:_game.managedObjectContext];

    [notifications
     addObserver:self
     selector:@selector(noteColorModeChanged:)
     name:kColorModeChangedNotification
     object:nil];
}

// Layer setup for the border and game views, and the initial border color.
- (void)setUpBorderAndGameView {
    // Enable layer-backing for the border view so we can animate its
    // background color. The border color can be set manually by the user
    // (kUserOverride) or automatically derived from the buffer background.
    _borderView.wantsLayer = YES;
    _borderView.layerContentsRedrawPolicy = NSViewLayerContentsRedrawOnSetNeedsDisplay;

    // Clip the game view to its bounds so no Glk window can draw into the
    // border margin. Glk windows are top-pinned with a fixed height (see the
    // SIZWIN handler), so when the window is shrunk vertically faster than the
    // interpreter re-sends frames -- e.g. while live-resizing Zork Zero's MAP
    // screen -- a full-size graphics window momentarily overhangs the bottom
    // edge, leaking content (Zork Zero's pillars) into the border. The game
    // view already has a backing layer because borderView is layer-backed.
    _gameView.wantsLayer = YES;
    _gameView.layer.masksToBounds = YES;

    _lastAutoBGColor = _theme.bufferBackground;
    if (_theme.borderBehavior == kUserOverride)
        [self setBorderColor:_theme.borderColor];
    else
        [self setBorderColor:_theme.bufferBackground];
}

// Decide whether to autorestore or start fresh. If GUI autosave files
// exist, attempt autorestore; otherwise delete stale files and start normally.
- (void)startInterpreterOrAutorestore {
    BOOL autosaveOn = (_supportsAutorestore && _theme.autosave);

    // Migrate any old-format autosave directories before checking for files.
    if (autosaveOn) {
        [self dealWithOldFormatAutosaveDir];
    }

    NSFileManager *fileManager = [NSFileManager defaultManager];
    if (autosaveOn &&
        ([fileManager fileExistsAtPath:self.autosaveFileGUI] ||
         [fileManager fileExistsAtPath:[self appSupportFile:kAutosaveGUILateFile]])) {
        [self runTerpWithAutorestore];
    } else {
        [self deleteAutosaveFiles];
        [self runTerpNormal];
    }
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

        newWindowFrame = [self cascadeWindowFrame:newWindowFrame];

        [self.window setFrame:newWindowFrame display:NO];
        [self adjustContentView];
    }
    lastSizeInChars = [self contentSizeToCharCells:_gameView.frame.size];
    if (_theme.coverArtStyle != kDontShow && _game.metadata.cover.data) {
        // The cover image is drawn into the window's contentView, so the window
        // must be on screen before we present it.
        if (![self runWindowsRestorationHandler])
            [self showWindow:nil];
        [self deleteAutosaveFiles];
        _gameView.autoresizingMask = kResizeCenteredFullHeight;
        restoredController = nil;
        _coverController = [[CoverImageHandler alloc] initWithController:self];
        [_coverController showLogoWindow];
    } else {
        // Launch the interpreter subprocess before presenting the window so its
        // exec/dyld cold-start overlaps with AppKit's window display and first
        // draw. forkInterpreterTask needs no on-screen window: it sizes the
        // arrange event from _gameView.frame, already set above.
        [self forkInterpreterTask];
        if (![self runWindowsRestorationHandler])
            [self showWindow:nil];
    }
}

// Cascade windows so multiple game windows don't stack directly on
// top of each other. Offsets the frame 20px right and 20px down from
// any overlapping game window.
- (NSRect)cascadeWindowFrame:(NSRect)frame {
    if (libcontroller.gameSessions.count < 2)
        return frame;

    NSPoint thisPoint, thatPoint;
    NSInteger repeats = 0;
    BOOL overlapping;
    thisPoint = frame.origin;
    thisPoint.y += NSHeight(frame);
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
    frame.origin = thisPoint;
    frame.origin.y -= NSHeight(frame);
    return frame;
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
    terpHasStopped = YES;

    [self runWindowsRestorationHandler];
    [self restoreUI];
    self.window.title = [self.window.title stringByAppendingString:@" (finished)"];

    // Tell the windows the interpreter is gone, exactly as noteTaskDidTerminate
    // does when a game dies while we are watching. Without this the restored
    // text buffer keeps whatever editable state the archive happened to
    // capture, so a finished game can present a live input line: typing into it
    // queues an event that no interpreter will ever collect, and the window
    // looks locked up.
    for (GlkWindow *win in self.gwindows.allValues)
        [win terpDidStop];

    GlkController * __weak weakSelf = self;
    NSScreen *screen = self.window.screen;
    NSString *title = [NSString stringWithFormat:@"%@ has finished.", _game.metadata.title];
    runOnMainAfterDelay(1.5, ^{
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
    static NSDictionary<NSString *, NSNumber *> *gameIDs;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        gameIDs = @{
            @"ZCODE-5-990206-6B48" : @(kGameIsAnchorheadOriginal),
            @"ZCODE-47-870915" : @(kGameIsBeyondZork),
            @"ZCODE-49-870917" : @(kGameIsBeyondZork),
            @"ZCODE-51-870923" : @(kGameIsBeyondZork),
            @"ZCODE-57-871221" : @(kGameIsBeyondZork),
            @"ZCODE-60-880610" : @(kGameIsBeyondZork),
            @"ZCODE-86-870212" : @(kGameIsBureaucracy),
            @"ZCODE-116-870602" : @(kGameIsBureaucracy),
            @"ZCODE-160-880521" : @(kGameIsBureaucracy),
            @"BFDE398E-C724-4B9B-99EB-18EE4F26932E" : @(kGameIsAColderLight),
            @"ZCODE-7-930428-0000" : @(kGameIsCurses),
            @"ZCODE-8-930603-0000" : @(kGameIsCurses),
            @"ZCODE-10-940120-BD9E" : @(kGameIsCurses),
            @"ZCODE-12-940604-6035" : @(kGameIsCurses),
            @"ZCODE-16-951024-4DE6" : @(kGameIsCurses),
            @"303E9BDC-6D86-4389-86C5-B8DCF01B8F2A" : @(kGameIsDeadCities),
            @"AC0DAF65-F40F-4A41-A4E4-50414F836E14" : @(kGameIsKerkerkruip),
            @"GLULX-1-040108-D8D78266" : @(kGameIsNarcolepsy),
            @"afb163f4-4d7b-0dd9-1870-030f2231e19f" : @(kGameIsThaumistry),
            @"ZCODE-1-851202" : @(kGameIsTrinity),
            @"ZCODE-1-860221" : @(kGameIsTrinity),
            @"ZCODE-14-860313" : @(kGameIsTrinity),
            @"ZCODE-11-860509" : @(kGameIsTrinity),
            @"ZCODE-12-860926" : @(kGameIsTrinity),
            @"ZCODE-15-870628" : @(kGameIsTrinity),
            @"ZCODE-1-050929-F8AB" : @(kGameIsVespers),
            @"ZCODE-1-051128-B5AA" : @(kGameIsVespers),
            @"CF619423-EEC7-4E83-8C66-AE7182D55C89" : @(kGameIsJuniorArithmancer),
            @"ZCODE-0-870831" : @(kGameIsZorkZero),
            @"ZCODE-1-871030" : @(kGameIsZorkZero),
            @"ZCODE-74-880114" : @(kGameIsZorkZero),
            @"ZCODE-96-880224" : @(kGameIsZorkZero),
            @"ZCODE-153-880510" : @(kGameIsZorkZero),
            @"ZCODE-242-880830" : @(kGameIsZorkZero),
            @"ZCODE-242-880901" : @(kGameIsZorkZero),
            @"ZCODE-296-881019" : @(kGameIsZorkZero),
            @"ZCODE-66-890111" : @(kGameIsZorkZero),
            @"ZCODE-343-890217" : @(kGameIsZorkZero),
            @"ZCODE-366-890323" : @(kGameIsZorkZero),
            @"ZCODE-383-890602" : @(kGameIsZorkZero),
            @"ZCODE-387-890612" : @(kGameIsZorkZero),
            @"ZCODE-392-890714" : @(kGameIsZorkZero),
            @"ZCODE-393-890714" : @(kGameIsZorkZero),
            @"ZCODE-40-890502" : @(kGameIsArthur),
            @"ZCODE-41-890504" : @(kGameIsArthur),
            @"ZCODE-54-890606" : @(kGameIsArthur),
            @"ZCODE-63-890622" : @(kGameIsArthur),
            @"ZCODE-74-890714" : @(kGameIsArthur),
            @"ZCODE-278-890209" : @(kGameIsShogun),
            @"ZCODE-278-890211" : @(kGameIsShogun),
            @"ZCODE-279-890217" : @(kGameIsShogun),
            @"ZCODE-280-890217" : @(kGameIsShogun),
            @"ZCODE-281-890222" : @(kGameIsShogun),
            @"ZCODE-282-890224" : @(kGameIsShogun),
            @"ZCODE-283-890228" : @(kGameIsShogun),
            @"ZCODE-284-890302" : @(kGameIsShogun),
            @"ZCODE-286-890306" : @(kGameIsShogun),
            @"ZCODE-288-890308" : @(kGameIsShogun),
            @"ZCODE-289-890309" : @(kGameIsShogun),
            @"ZCODE-290-890311" : @(kGameIsShogun),
            @"ZCODE-291-890313" : @(kGameIsShogun),
            @"ZCODE-292-890314" : @(kGameIsShogun),
            @"ZCODE-295-890321" : @(kGameIsShogun),
            @"ZCODE-311-890510" : @(kGameIsShogun),
            @"ZCODE-320-890627" : @(kGameIsShogun),
            @"ZCODE-321-890629" : @(kGameIsShogun),
            @"ZCODE-322-890706" : @(kGameIsShogun),
            @"ZCODE-142-890205" : @(kGameIsJourney),
            @"ZCODE-2-890303" : @(kGameIsJourney),
            @"ZCODE-11-890304" : @(kGameIsJourney),
            @"ZCODE-3-890310" : @(kGameIsJourney),
            @"ZCODE-5-890310" : @(kGameIsJourney),
            @"ZCODE-10-890313" : @(kGameIsJourney),
            @"ZCODE-26-890316" : @(kGameIsJourney),
            @"ZCODE-30-890322" : @(kGameIsJourney),
            @"ZCODE-51-890522" : @(kGameIsJourney),
            @"ZCODE-54-890526" : @(kGameIsJourney),
            @"ZCODE-76-890615" : @(kGameIsJourney),
            @"ZCODE-77-890616" : @(kGameIsJourney),
            @"ZCODE-79-890627" : @(kGameIsJourney),
            @"ZCODE-83-890706" : @(kGameIsJourney),
        };
    });

    NSString *l9Prefix = (ifid.length >= 10) ? [ifid substringToIndex:10] : nil;
    if ([l9Prefix isEqualToString:@"LEVEL9-001"] || // The Secret Diary of Adrian Mole
        [l9Prefix isEqualToString:@"LEVEL9-002"] || // The Growing Pains of Adrian Mole
        [l9Prefix isEqualToString:@"LEVEL9-019"]) { // The Archers
        _gameID = kGameIsAdrianMole;
        return;
    }

    NSNumber *gameID = ifid ? gameIDs[ifid] : nil;
    _gameID = gameID ? (kGameIdentity)gameID.unsignedIntegerValue : kGameIsGeneric;
}

// Identify the game from its IFID, or treat it as generic if the theme's
// "nohacks" option is enabled.
- (void)identifyGameUnlessNoHacks:(Theme *)theme {
    if (theme.nohacks)
        _gameID = kGameIsGeneric;
    else
        [self identifyGame:_game.ifid];
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
// The interpreter command line: the game file, preceded by any
// interpreter-specific options.
- (NSArray<NSString *> *)interpreterArgumentsForTheme:(Theme *)theme {
    NSArray<NSString *> *arguments = @[ _gamefile ];

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

        arguments = [extraBocfelOptions arrayByAddingObjectsFromArray:arguments];
    }

    // TADS 3 interpreter: -norandomize disables random number generation
    // for deterministic/reproducible testing.
    if ([_game.detectedFormat isEqualToString:@"tads3"]) {
        if (theme.determinism) {
            NSArray *extraTadsOptions = @[@"-norandomize"];
            arguments = [extraTadsOptions arrayByAddingObjectsFromArray:arguments];
        }
    }

    return arguments;
}

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
    
    // Optional: Log interpreter output to file for debugging
    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"LogInterpreterOutput"])
        [self openLogFile];

    task.launchPath = terppath;
    task.arguments = [self interpreterArgumentsForTheme:theme];

    // Hand the interpreter the same per-game signature the GUI uses for its
    // autosave directory. Unlike us, the interpreter cannot fall back to the
    // library's stored hash when the game file is unreadable (e.g. an evicted
    // iCloud file), and letting it recompute the signature independently
    // could make the two processes disagree on the directory.
    NSString *signature = self.gameSignature;
    if (signature.length) {
        NSMutableDictionary<NSString *, NSString *> *environment =
            [NSProcessInfo.processInfo.environment mutableCopy];
        environment[@"SPATTERLIGHT_AUTOSAVE_SIGNATURE"] = signature;
        task.environment = environment;
    }

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
                    runOnMainAfterDelay(0.5, ^{
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
    NSAlert *anAlert = [NSAlert alertWithMessage:[NSString stringWithFormat:NSLocalizedString(@"The game file \"%@\" is no longer accessible. This game will now close.", nil), self.gamefile.lastPathComponent]
                                            info:nil
                                         buttons:nil];

    [anAlert beginSheetModalForWindow:self.window completionHandler:^(NSInteger result) {
        runOnMainAfterDelay(0.1, ^{
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

        if (_journeyMenuHandler && [_journeyMenuHandler updateOnBecameKey:!_shouldShowAutorestoreAlert || _turnsThisSession > 1]) {
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
    [_journeyMenuHandler hideMenus];
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
         boolForKey:kCloseAlertSuppressionKey]) {
        NSLog(@"windowShouldClose: Window close alert suppressed");
        return YES;
    }
    alert = [NSAlert alertWithMessage:NSLocalizedString(@"Do you want to abandon the game?", nil)
                                 info:NSLocalizedString(@"Any unsaved progress will be lost.", nil)
                              buttons:@[NSLocalizedString(@"Close", nil), NSLocalizedString(@"Cancel", nil)]];
    // Only a Close is remembered: ticking the box and cancelling keeps asking.
    [alert showSuppressionButtonWithTitle:nil];

    _mustBeQuiet = YES;

    [alert beginSheetModalForWindow:self.window
                  completionHandler:^(NSInteger result) {
        if (result == NSAlertFirstButtonReturn) {
            if (alert.suppressionButton.state == NSOnState) {
                // Suppress this alert from now on
                [[NSUserDefaults standardUserDefaults]
                 setBool:YES
                 forKey:kCloseAlertSuppressionKey];
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
    NSLog(@"terminateTask");
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
    }
    windowClosedAlready = YES;

    [self requestFinalAutosave];

    // Prevent the interpreter from processing new events during teardown
    waitforfilename = YES;

    if (_showingCoverImage) {
        [[NSNotificationCenter defaultCenter]
         removeObserver:_coverController];
        _coverController = nil;
    }
    [self autoSaveOnExit];
    [_soundHandler stopAllAndCleanUp];

    [self closeLogFile];

    if (_journeyMenuHandler) {
        [_journeyMenuHandler captureMembersMenu];
        [self.journeyMenuHandler hideMenus];
    }

    if (_theme.autosave == NO) {
        _game.autosaved = NO;
        [self deleteAutosaveFiles];
    }

    // Only registered once runTerp got past its early exits.
    if (observingVoiceOver) {
        [[NSWorkspace sharedWorkspace] removeObserver:self forKeyPath:@"voiceOverEnabled"];
        observingVoiceOver = NO;
    }

    if (_game && [Preferences instance].currentGame == _game)
        [self passCurrentGameToAnotherSession];

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

// We were the current game: make another open game session current (and
// key), or none if this was the last one.
- (void)passCurrentGameToAnotherSession {
    GlkController *remainingGameSession = nil;
    for (GlkController *session in libcontroller.gameSessions.allValues) {
        if (session != self) {
            remainingGameSession = session;
            break;
        }
    }
    [Preferences changeCurrentGlkController:remainingGameSession];
    [remainingGameSession.window makeKeyAndOrderFront:nil];
}

// YES while a repeating Glk timer is scheduled.
- (BOOL)timerActive {
    return timer != nil;
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
                if (_journeyMenuHandler.dialogClosedRecently)
                    delay = 1;
                [self scheduleSpeakNewTextAfterDelay:delay];
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
        GlkTextBufferWindow *bufwin = self.bufferWindows.firstObject;
        if (bufwin) {
            [bufwin grabFocus];
            return;
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
    for (GlkTextBufferWindow *win in self.bufferWindows)
        [win markLastSeen];
}

// Scroll all text buffer windows to show new content. Skipped during
// autorestore to prevent premature scrolling before the UI is fully rebuilt.
- (void)performScroll {
    if (autorestoring)
        return;
    for (GlkTextBufferWindow *win in self.bufferWindows)
        [win performScroll];
}

- (BOOL)isInSystemFullscreen {
    return (self.window.styleMask & NSWindowStyleMaskFullScreen) == NSWindowStyleMaskFullScreen;
}

- (NSArray<GlkTextBufferWindow *> *)bufferWindows {
    NSMutableArray<GlkTextBufferWindow *> *result = [[NSMutableArray alloc] init];
    for (GlkWindow *win in _gwindows.allValues)
        if ([win isKindOfClass:[GlkTextBufferWindow class]])
            [result addObject:(GlkTextBufferWindow *)win];
    return result;
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

    NSData *maskData = [_imageHandler dataForImageNumber:3];
    if (!maskData) {
        NSLog(@"createMaskLayer: Failed to load Narcolepsy image 3!");
        return nil;
    } else {
        CIImage *inputImage = [CIImage imageWithData:maskData];

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
// the theme's default character cell dimensions.
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
        [self sendArrangeEventWithFrame:frame force:NO];
    }
}

// The window frame for a content size, clipped to the main screen and keeping
// the window's top edge in place.
- (NSRect)windowFrameForContentSize:(NSSize)size screenHeightMargin:(CGFloat)margin {
    NSRect screenframe = [NSScreen mainScreen].visibleFrame;

    NSRect winrect = [self.window frameRectForContentRect:NSMakeRect(0, 0, size.width, size.height)];
    winrect.origin = self.window.frame.origin;

    // If the new size is too big to fit on screen, clip at screen size
    if (NSHeight(winrect) > NSHeight(screenframe) - margin)
        winrect.size.height = NSHeight(screenframe) - margin;
    if (NSWidth(winrect) > NSWidth(screenframe))
        winrect.size.width = NSWidth(screenframe);

    winrect.origin.y -= NSHeight(winrect) - NSHeight(self.window.frame);
    return winrect;
}

// Resize the game content area to a specific pixel size. Handles both windowed
// and fullscreen modes differently: in windowed mode, resizes the window frame;
// in fullscreen, adjusts the game view within the fixed-size border view.
// Temporarily suppresses resize events to prevent recursive arrange events.
- (void)zoomContentToSize:(NSSize)newSize {
    _ignoreResizes = YES;
    NSRect oldframe = _gameView.frame;

    NSUInteger borders = (NSUInteger)_theme.border * 2;

    if (!self.isInSystemFullscreen) {
        newSize.width += (CGFloat)borders;
        newSize.height += (CGFloat)borders;

        NSRect winrect = [self windowFrameForContentSize:newSize screenHeightMargin:0];
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
    // Defensive: the cell metrics come from the theme's font measurements,
    // and a non-finite height here would end up in the window frame.
    if (isnan(size.height) || isinf(size.height)) {
        NSLog(@"ERROR: charCellsToContentSize: Height is not finite!");
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
    Theme *theme = _theme ?: [Preferences currentTheme];
    if (theme == nil) {
        cellWidth = 8.0;
        cellHeight = 18.0;
        gridMarginX = 5;
        gridMarginY = 0;
    } else {
        cellWidth = theme.cellWidth;
        cellHeight = theme.cellHeight;
        gridMarginX = theme.gridMarginX + 5;
        gridMarginY = theme.gridMarginY;
    }

    size.width = round((points.width - gridMarginX * 2) / cellWidth);
    size.height = round((points.height - gridMarginY * 2) / cellHeight);

    return size;
}

// Respond to border size changes from the preferences panel. Adjusts the window
// frame to accommodate the new border while keeping the content area stable.
// Only applies in windowed mode when "Adjust window size" is enabled.
- (void)noteBorderChanged:(NSNotification *)notify {
    if (notify.object != _theme || self.isInSystemFullscreen || ![[NSUserDefaults standardUserDefaults] boolForKey:kAdjustSizeKey])
        return;
    [Preferences instance].inMagnification = YES;
    _movingBorder = YES;
    CGFloat diff = ((NSNumber *)notify.userInfo[@"diff"]).floatValue;
    _gameView.autoresizingMask = NSViewMaxXMargin | NSViewMaxYMargin | NSViewMinXMargin | NSViewMinYMargin;
    NSRect frame = self.window.frame;
    frame.origin = NSMakePoint(frame.origin.x - diff, frame.origin.y - diff);
    frame.size = NSMakeSize(frame.size.width + (diff * 2), frame.size.height + (diff * 2));
    NSRect contentframe = _gameView.frame;
    contentframe.origin = NSMakePoint(contentframe.origin.x + diff, contentframe.origin.y + diff);
    [self.window setFrame:frame display:NO];
    _gameView.frame = contentframe;
    _movingBorder = NO;
    _gameView.autoresizingMask = kResizeFill;
    NSNotification *notification = [[NSNotification alloc] initWithName:kPreferencesChangedNotification object:_theme userInfo:nil];
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
    if (!_game)
        return;
    if (!_stashedTheme) {
        _theme = [self resolvedTheme];
        theme = _theme;
    }

    if (notify.object != theme && notify.object != nil) {
        //  PreferencesChanged called for a different theme
        return;
    }

    [self identifyGameUnlessNoHacks:theme];

    if (theme.vOSpeakMenu == kVOMenuNone) { // "Check for menu" was switched off
        VoiceOverHelperDetach(_zmenu);
        _zmenu = nil;
        VoiceOverHelperDetach(_form);
        _form = nil;
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

    if (![defaults boolForKey:kSaveInGameDirectoryKey]) {
        self.saveDir = nil;
    }

    _shouldStoreScrollOffset = NO;
    if ([defaults boolForKey:kAdjustSizeKey] && !autorestoring) {
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

    NSRect frame = _gameView.frame;
    frame.size.width = MAX(frame.size.width, 0);
    frame.size.height = MAX(frame.size.height, 0);

    [self queueEvent:[[GlkEvent alloc] initPrefsEventForTheme:theme]];
    [self sendArrangeEventWithFrame:frame force:YES];

    for (GlkWindow *win in _gwindows.allValues)
    {
        if (!autorestoring || win.theme != theme || _restoredStylesStale) {
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
    _theme = [self resolvedTheme];

    if (_theme != oldTheme) {
        [self notePreferencesChanged:[NSNotification notificationWithName:kPreferencesChangedNotification object:_theme]];
    }
}

// The theme this game should use: the light or dark override theme if one
// is active, otherwise the game's own theme. Preferences briefly has both
// overrides flagged while switching, so the light-first order matters.
- (Theme *)resolvedTheme {
    Preferences *prefs = [Preferences instance];
    if (prefs.lightOverrideActive) {
        if (prefs.lightTheme)
            return prefs.lightTheme;
        NSLog(@"ERROR: lightOverrideActive but no hard light theme exists, use normal selected theme");
    } else if (prefs.darkOverrideActive) {
        if (prefs.darkTheme)
            return prefs.darkTheme;
        NSLog(@"ERROR: darkOverrideActive but no hard dark theme exists, use normal selected theme");
    }
    return _game.theme;
}

#pragma mark Zoom

// Zoom actions (Cmd+Plus, Cmd+Minus, Cmd+0) scale the font size and
// optionally resize the window to maintain the same number of character cells.
// Disabled when showing cover art in fullscreen to prevent layout issues.
- (void)performZoom:(void (^)(void))zoom {
    if (_showingCoverImage && _inFullscreen)
        return;
    Preferences *prefs = [Preferences instance];
    prefs.inMagnification = YES;
    zoom();
    [prefs updatePanelAfterZoom];
}

- (IBAction)zoomIn:(id)sender {
    [self performZoom:^{ [Preferences zoomIn]; }];
}

- (IBAction)zoomOut:(id)sender {
    [self performZoom:^{ [Preferences zoomOut]; }];
}

- (IBAction)zoomToActualSize:(id)sender {
    [self performZoom:^{ [Preferences zoomToActualSize]; }];
}

// Respond to changes in the default character cell size (triggered by zoom or
// theme font changes). Resizes the window to match the new dimensions while
// preventing paradoxical size changes (shrinking when zooming in, etc.).
- (void)noteDefaultSizeChanged:(NSNotification *)notification {

    if (notification.object != _game.theme)
        return;

    NSSize sizeAfterZoom;
    if ([Preferences instance].inMagnification) {
        if ([[NSUserDefaults standardUserDefaults] boolForKey:kAdjustSizeKey] == NO)
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

    if (!self.isInSystemFullscreen) {

        NSRect winrect = [self windowFrameForContentSize:sizeAfterZoom screenHeightMargin:1];
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


#pragma mark Menu handlers

// Created on first use. The Journey handler needs the game's grid and
// buffer windows, so it stays nil until both exist.
- (nullable JourneyMenuHandler *)journeyMenuHandler {
    if (_journeyMenuHandler == nil) {
        GlkTextGridWindow *gridwindow = nil;
        GlkTextBufferWindow *bufferwindow = nil;
        for (GlkWindow *win in self.gwindows.allValues)
            if ([win isKindOfClass:[GlkTextGridWindow class]]) {
                gridwindow = (GlkTextGridWindow *)win;
            } else if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
                bufferwindow = (GlkTextBufferWindow *)win;
            }
        if (gridwindow == nil || bufferwindow == nil)
            return nil;
        _journeyMenuHandler = [[JourneyMenuHandler alloc] initWithGlkController:self gridWindow:gridwindow bufferWindow:bufferwindow];
    }
    return _journeyMenuHandler;
}

- (nullable InfocomV6MenuHandler *)infocomV6MenuHandler {
    if (_infocomV6MenuHandler == nil) {
        _infocomV6MenuHandler = [[InfocomV6MenuHandler alloc] initWithGlkController:self];
    }
    return _infocomV6MenuHandler;
}

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
