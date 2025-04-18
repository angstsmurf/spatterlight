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

@class Game, Theme, GlkEvent, GlkWindow, ZMenu, BureaucracyForm, GlkTextGridWindow, GlkSoundChannel, SoundHandler, ImageHandler, RotorHandler, CommandScriptHandler, CoverImageHandler, GlkController, InfocomV6MenuHandler;

#define MAXWIN 64

typedef enum kMinimumWindowSize : NSUInteger {
    kMinimumWindowWidth = 212,
    kMinimumWindowHeight = 100
} kMinimumWindowSize;

@interface GlkHelperView : NSView

@property (weak) IBOutlet GlkController *glkctrl;

@end

@interface GlkController : NSWindowController <NSSecureCoding, NSDraggingDestination>

@property NSMutableDictionary<NSNumber*,GlkWindow*> *gwindows;
@property SoundHandler *soundHandler;
@property ImageHandler *imageHandler;
@property NSMutableArray *imagesToSpeak;

@property NSMutableArray<GlkWindow *> *windowsToBeAdded;
@property NSMutableArray<GlkWindow *> *windowsToBeRemoved;
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
@property(readonly) NSRect storedBorderFrame;

@property NSRect windowPreFullscreenFrame;

@property BOOL ignoreResizes;
@property BOOL startingInFullscreen;
@property NSSize borderFullScreenSize;
@property(readonly) BOOL inFullscreen;
@property BOOL shouldStoreScrollOffset;

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
@property NSInteger turns;

// To fix scrolling in the Adrian Mole games
@property BOOL shouldScrollOnCharEvent;

@property (weak) Theme *stashedTheme;
@property NSString *oldThemeName;

@property NSData *gameData;
@property NSURL *gameFileURL;
@property NSAlert *slowReadAlert;

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

// Command scripts
@property CommandScriptHandler *commandScriptHandler;
@property BOOL commandScriptRunning;
@property NSString *pendingSaveFilePath;

@property CoverImageHandler *coverController;
@property (nonatomic) InfocomV6MenuHandler *infocomV6MenuHandler;


- (void)runTerp:(NSString *)terpname
       withGame:(Game *)game
          reset:(BOOL)shouldReset
     winRestore:(BOOL)windowRestoredBySystem;

- (void)deleteAutosaveFilesForGame:(Game *)game;

- (void)askForAccessToURL:(NSURL *)url showDialog:(BOOL)dialogFlag andThenRunBlock:(void (^)(void))block;


- (IBAction)reset:(id)sender;

- (void)queueEvent:(GlkEvent *)gevent;
- (void)contentDidResize:(NSRect)frame;
- (void)markLastSeen;
- (void)performScroll;
- (void)setBorderColor:(NSColor *)color fromWindow:(GlkWindow *)aWindow;
- (void)restoreUI;
- (void)autoSaveOnExit;
- (void)storeScrollOffsets;
- (void)restoreScrollOffsets;
- (void)adjustContentView;
- (void)cleanup;


// VoiceOver
- (IBAction)speakMostRecent:(id)sender;
- (IBAction)speakPrevious:(id)sender;
- (IBAction)speakNext:(id)sender;
- (IBAction)speakStatus:(id)sender;

@property (NS_NONATOMIC_IOSONLY, readonly, strong) GlkWindow *largestWithMoves;

- (void)speakString:(NSString *)string;
- (void)speakStringNow:(NSString *)string;

@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSArray *accessibilityCustomActions;
@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSArray *createCustomRotors;

- (void)forkInterpreterTask;
- (void)deferredRestart:(id)sender;

- (void)setBorderColor:(NSColor *)color;
- (void)terminateTask;

- (BOOL)showingInfocomV6Menu;

@end
