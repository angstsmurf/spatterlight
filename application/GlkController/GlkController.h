/*
 * GlkController --
 *
 * An NSWindowController that controls the game window,
 * talks with the interpreter process,
 * handles global glk commands,
 * dispatches sound channel and window commands,
 * queues events for sending to the interpreter.
 *
 */

#import <AppKit/AppKit.h>

#import "GlkHelperView.h"

@class Game, Theme, GlkEvent, GlkWindow, ZMenu, BureaucracyForm, GlkTextGridWindow, GlkSoundChannel, SoundHandler, ImageHandler, RotorHandler, CommandScriptHandler, CoverImageHandler, GlkController, InfocomV6MenuHandler;

typedef enum kGameIdentity : NSUInteger {
    kGameIsGeneric,
    kGameIsAdrianMole,
    kGameIsArthur,
    kGameIsAnchorheadOriginal,
    kGameIsBeyondZork,
    kGameIsBureaucracy,
    kGameIsAColderLight,
    kGameIsCurses,
    kGameIsDeadCities,
    kGameIsJourney,
    kGameIsJuniorArithmancer,
    kGameIsKerkerkruip,
    kGameIsNarcolepsy,
    kGameIsShogun,
    kGameIsThaumistry,
    kGameIsTrinity,
    kGameIsVespers,
    kGameIsZorkZero
} kGameIdentity;

typedef enum kMinimumWindowSize : NSUInteger {
    kMinimumWindowWidth = 212,
    kMinimumWindowHeight = 100
} kMinimumWindowSize;

@interface GlkController : NSWindowController <NSSecureCoding, NSDraggingDestination>

@property NSMutableDictionary<NSNumber*,GlkWindow*> *gwindows;
@property SoundHandler *soundHandler;
@property ImageHandler *imageHandler;
@property NSMutableArray *imagesToSpeak;

@property IBOutlet NSView *borderView;
@property IBOutlet GlkHelperView *gameView;

// stylehints need to be copied to new windows, so we keep the values around

@property NSMutableArray *gridStyleHints;
@property NSMutableArray *bufferStyleHints;

@property(readonly, getter=isAlive) BOOL alive;

@property(readonly) NSTimeInterval storedTimerLeft;
@property(readonly) NSTimeInterval storedTimerInterval;
@property(readonly) NSRect storedWindowFrame;
@property(readonly) NSRect storedGameViewFrame;

@property NSRect windowPreFullscreenFrame;

@property BOOL ignoreResizes;
@property BOOL startingInFullscreen;
@property NSSize borderFullScreenSize;
@property(readonly) BOOL inFullscreen;
// Whether the window is in system fullscreen right now. inFullscreen is
// our own record of it, which is archived with the autosave.
- (BOOL)isInSystemFullscreen;
@property BOOL shouldStoreScrollOffset;

@property (copy) void (^restorationHandler)(NSWindow *, NSError *);

@property(readonly) NSInteger firstResponderView;

@property(readonly) NSMutableArray *queue;

/* keep some info around for the about-box and resetting*/
@property(weak, readonly) Game *game;
@property(readonly) NSString *gamefile;
@property(readonly) NSString *terpname;
@property (weak) Theme *theme;

@property(strong, nonatomic) NSString *appSupportDir;
@property(strong, nonatomic) NSString *autosaveFileGUI;
@property(strong, nonatomic) NSString *autosaveFileTerp;

@property(readonly) BOOL supportsAutorestore;

@property NSColor *lastAutoBGColor;
@property NSColor *bgcolor;

@property NSInteger eventcount;
// Turns played in the game, carried over by autorestore.
@property NSInteger turns;
// Turns since this launch, not autosaved.
@property NSInteger turnsThisSession;

// To fix scrolling in the Adrian Mole games
@property BOOL shouldScrollOnCharEvent;

@property NSString *oldThemeName;

// The whole game file, read into the heap for the session. Blorb pictures
// are subdata views on it, not copies. Not mapped: a mapped file that is
// replaced or truncated during play crashes with SIGBUS on the next touch.
@property NSData *gameData;
@property NSURL *gameFileURL;
@property NSAlert *slowReadAlert;

@property kGameIdentity gameID;

- (BOOL)zVersion6;

@property BOOL usesFont3;

@property NSInteger autosaveVersion;
@property NSInteger autosaveTag;
@property BOOL hasAutoSaved;

@property BOOL showingCoverImage;

@property BOOL voiceOverActive;

@property ZMenu *zmenu;
@property BOOL shouldCheckForMenu;

@property NSInteger numberOfPrintsAndClears;
@property NSInteger printsAndClearsThisTurn;

// shouldSpeakNewText only applies to the call to
// speakNewText in flushDisplay.

// Its purpose is mainly to avoid speaking the same
// move several times in a row, but also to make
// sure that new moves are spoken even if they
// contain the exact same text as the last.

// It is set to YES when the game starts and whenever
// a new command is entered by the player
// (including single-key presses and link mouseclicks.)
@property BOOL shouldSpeakNewText;

// mustBeQuiet will override shouldSpeakNewText
// without changing its value. If an alert appears, or
// the game is reset, or the game crashes or is closed,
// we want VoiceOver to announce this without getting
// interrupted by the announcement of a new move
// in flushDisplay, but we also don't want to worry
// about restoring the state of shouldSpeakNewText if we
// set mustBeQuiet to YES and then back to NO.

// mustBeQuiet also makes sure that this window won't
// speak if VoiceOver is switched on when a different
// window is key
@property BOOL mustBeQuiet;

@property NSDate *speechTimeStamp;
@property NSDate *lastResetTimestamp;
@property NSString *lastSpokenString;

@property BureaucracyForm *form;

@property BOOL shouldShowAutorestoreAlert;

@property NSString *pendingErrorMessage;
@property NSDate *errorTimeStamp;

@property NSMutableArray<GlkTextGridWindow *> *quoteBoxes;

@property NSArray<GlkWindow *> *windowsToRestore;

@property RotorHandler *rotorHandler;

@property NSURL *secureBookmark;
@property BOOL showingDialog;

// Flag to prevent things from moving around
// when adjusting content size and position after
// border change
@property BOOL movingBorder;

// YES while a Glk timer is running, i.e. the game is producing unattended
// timer-driven output (real-time/animated games). Used to pause auto-scroll
// when the user scrolls up, the same way command-script playback does.
@property(readonly) BOOL timerActive;

// Command scripts
@property CommandScriptHandler *commandScriptHandler;
@property BOOL commandScriptRunning;
@property NSString *pendingSaveFilePath;

@property CoverImageHandler *coverController;
@property (nonatomic) InfocomV6MenuHandler *infocomV6MenuHandler;


- (void)runTerp:(NSString *)terpname
       withGame:(Game *)game
          reset:(BOOL)shouldReset
     restorationHandler:(void (^)(NSWindow *, NSError *))completionHandler;

- (void)askForAccessToURL:(NSURL *)url showDialog:(BOOL)dialogFlag andThenRunBlock:(void (^)(void))block;


- (void)contentDidResize:(NSRect)frame;
- (void)markLastSeen;
- (void)performScroll;

- (void)forkInterpreterTask;
- (void)terminateTask;

@end
