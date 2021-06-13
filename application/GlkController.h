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

@class Game, Theme, LibController, GlkEvent, GlkWindow, ZMenu, BureaucracyForm, GlkTextGridWindow, GlkSoundChannel, SoundHandler, ImageHandler, RotorHandler, CommandScriptHandler, CoverImageHandler;

#define MAXWIN 64

typedef enum kMinimumWindowSize : NSUInteger {
    kMinimumWindowWidth = 213,
    kMinimumWindowHeight = 107,
} kMinimumWindowSize;

@interface GlkHelperView : NSView

@property (weak) IBOutlet GlkController *glkctrl;

@end

@interface GlkController : NSWindowController <NSSecureCoding, NSDraggingDestination>

@property NSMutableDictionary *gwindows;
@property SoundHandler *soundHandler;
@property ImageHandler *imageHandler;
@property NSMutableArray *imagesToSpeak;

@property NSMutableArray *windowsToBeAdded;
@property NSMutableArray *windowsToBeRemoved;
@property IBOutlet NSView *borderView;
@property IBOutlet GlkHelperView *contentView;

// stylehints need to be copied to new windows, so we keep the values around

@property NSMutableArray *gridStyleHints;
@property NSMutableArray *bufferStyleHints;

@property(readonly, getter=isAlive) BOOL alive;

@property(readonly) NSTimeInterval storedTimerLeft;
@property(readonly) NSTimeInterval storedTimerInterval;
@property(readonly) NSRect storedWindowFrame;
@property(readonly) NSRect storedContentFrame;
@property(readonly) NSRect storedBorderFrame;

@property(readonly) NSRect windowPreFullscreenFrame;

@property BOOL ignoreResizes;
@property BOOL startingInFullscreen;
@property NSSize borderFullScreenSize;
@property(readonly) BOOL inFullscreen;
@property BOOL shouldStoreScrollOffset;

@property BOOL newTimer;
@property CGFloat newTimerInterval;

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

@property NSColor *bgcolor;

@property NSInteger eventcount;
@property NSInteger turns;

// To fix scrolling in the Adrian Mole games
@property BOOL shouldScrollOnCharEvent;

@property (weak) Theme *stashedTheme;
@property NSString *oldThemeName;

@property BOOL previewDummy;

@property BOOL adrianMole;
@property BOOL anchorheadOrig;
@property BOOL beyondZork;
@property BOOL bureaucracy;
@property BOOL colderLight;
@property BOOL curses;
@property BOOL deadCities;
@property BOOL kerkerkruip;
@property BOOL narcolepsy;
@property BOOL thaumistry;
@property BOOL trinity;
@property BOOL usesFont3;


@property NSInteger autosaveVersion;
@property NSInteger autosaveTag;
@property BOOL hasAutoSaved;

@property BOOL showingCoverImage;

@property BOOL voiceOverActive;

@property ZMenu *zmenu;
@property BOOL shouldCheckForMenu;
@property BOOL shouldSpeakNewText;
@property BOOL mustBeQuiet;
@property NSDate *speechTimeStamp;
@property GlkWindow *spokeLast;
@property BureaucracyForm *form;

@property NSString *pendingErrorMessage;

@property NSMutableArray<GlkTextGridWindow *> *quoteBoxes;

@property NSArray<GlkWindow *> *windowsToRestore;

@property RotorHandler *rotorHandler;

- (void)runTerp:(NSString *)terpname
       withGame:(Game *)game
          reset:(BOOL)shouldReset
     winRestore:(BOOL)windowRestoredBySystem;

- (void)deleteAutosaveFilesForGame:(Game *)game;

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

// Flag to prevent things from moving around
// when adjusting content size and position after
// border change
@property BOOL movingBorder;

// Command scripts
@property CommandScriptHandler *commandScriptHandler;
@property BOOL commandScriptRunning;
@property NSString *pendingSaveFilePath;

@property CoverImageHandler *coverController;

// VoiceOver
- (IBAction)speakMostRecent:(id)sender;
- (IBAction)speakPrevious:(id)sender;
- (IBAction)speakNext:(id)sender;
- (IBAction)speakStatus:(id)sender;

- (GlkWindow *)largestWithMoves;

- (void)speakString:(NSString *)string;

- (NSArray *)accessibilityCustomActions API_AVAILABLE(macos(10.13));
- (NSArray *)createCustomRotors;

- (void)forkInterpreterTask;
- (void)deferredRestart:(id)sender;

@end
