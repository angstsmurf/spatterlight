/*
 * GlkController --
 *
 * An NSWindowController that controls the game window,
 * talks with the interpreter process,
 * handles global glk commands,
 * dispatches sound channel and window commands,
 * queues events for sending to the interpreter.
 *
 * TODO: cache resources (in raw format) so findimage/findsound
 *       will succeed for more than just the last one uploaded.
 */

#import <QuartzCore/QuartzCore.h>

@class Game, Theme, LibController, GlkEvent, GlkWindow, ZMenu, BureaucracyForm, GlkTextGridWindow, GlkSoundChannel, AudioResourceHandler;

#define MAXWIN 64
#define MAXSND 32

@interface GlkHelperView : NSView {
}

@property (weak) IBOutlet GlkController *glkctrl;

@end

@interface GlkController : NSWindowController <NSSecureCoding, NSAccessibilityCustomRotorItemSearchDelegate>

@property NSMutableDictionary *gwindows;
@property NSMutableDictionary <NSNumber *, GlkSoundChannel *> *gchannels;
@property AudioResourceHandler *audioResourceHandler;
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

@property BOOL stopReadingPipe;
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
@property(readonly) BOOL inFullscreen;
@property NSColor *bgcolor;

@property NSInteger eventcount;
@property NSInteger turns;

// To fix scrolling in the Adrian Mole games
@property BOOL shouldScrollOnCharEvent;

@property BOOL shouldStoreScrollOffset;

@property (weak) Theme *stashedTheme;
@property NSString *oldThemeName;

@property BOOL previewDummy;

//@property BOOL adrianMole;
@property BOOL deadCities;
@property BOOL bureaucracy;
@property BOOL usesFont3;
@property BOOL beyondZork;
@property BOOL kerkerkruip;
@property BOOL thaumistry;
@property BOOL colderLight;
@property BOOL trinity;
@property BOOL anchorheadOrig;
@property BOOL curses;

@property NSInteger autosaveVersion;
@property NSInteger autosaveTag;
@property BOOL hasAutoSaved;

@property BOOL voiceOverActive;

@property ZMenu *zmenu;
@property BOOL shouldCheckForMenu;
@property BOOL shouldSpeakNewText;
@property BOOL mustBeQuiet;
@property NSDate *speechTimeStamp;
@property GlkWindow *spokeLast;
@property BureaucracyForm *form;

@property NSMutableArray<GlkTextGridWindow *> *quoteBoxes;

@property NSArray<GlkWindow *> *windowsToRestore;

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

- (void)handleSoundNotification:(NSInteger)notify withSound:(NSInteger)sound;
- (void)handleVolumeNotification:(NSInteger)notify;

- (IBAction)speakMostRecent:(id)sender;
- (IBAction)speakPrevious:(id)sender;
- (IBAction)speakNext:(id)sender;
- (IBAction)speakStatus:(id)sender;

- (GlkWindow *)largestWithMoves;

- (void)speakString:(NSString *)string;

- (NSArray *)accessibilityCustomActions API_AVAILABLE(macos(10.13));
- (NSArray *)createCustomRotors;
- (NSAccessibilityCustomRotorItemResult *)rotor:(NSAccessibilityCustomRotor *)rotor
                      resultForSearchParameters:(NSAccessibilityCustomRotorSearchParameters *)searchParameters API_AVAILABLE(macos(10.13));


@end
