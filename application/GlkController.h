/*
 * GlkController --
 *
 * An NSWindowController that controls the game window,
 * talks with the interpreter process,
 * handles global glk commands,
 * dispatches window commands,
 * queues events for sending to the interpreter.
 *
 */

#import <QuartzCore/QuartzCore.h>

@class Game, Theme, LibController, GlkEvent, GlkWindow, ZMenu;

#define MAXWIN 64

@interface GlkHelperView : NSView {
}

@property IBOutlet GlkController *glkctrl;

@end

@interface GlkController : NSWindowController <NSAccessibilityCustomRotorItemSearchDelegate> {
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

    NSSize borderFullScreenSize;
    NSWindow *snapshotWindow;

    BOOL windowClosedAlready;
    BOOL restartingAlready;

    /* the glk objects */
    BOOL windowdirty; /* the contentView needs to repaint */

    /* image resource uploading protocol */
    NSInteger lastimageresno;
    NSCache *imageCache;
    NSImage *lastimage;

    GlkController *restoredController;
    GlkController *restoredControllerLate;
    NSUInteger turns;
    NSMutableData *bufferedData;

    LibController *libcontroller;

    NSSize lastSizeInChars;
    Theme *lastTheme;

    // To fix scrolling in the Adrian Mole games
    NSInteger lastRequest;

//    NSDate *lastFlushTimestamp;
}

@property NSMutableDictionary *gwindows;
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

@property(readonly) NSInteger firstResponderView;

@property(readonly) NSMutableArray *queue;

/* keep some info around for the about-box and resetting*/
@property(readonly) Game *game;
@property(readonly) NSString *gamefile;
@property(readonly) NSString *terpname;
@property Theme *theme;

@property(strong, nonatomic) NSString *appSupportDir;
@property(strong, nonatomic) NSString *autosaveFileGUI;
@property(strong, nonatomic) NSString *autosaveFileTerp;

@property(readonly) BOOL supportsAutorestore;
@property(readonly) BOOL inFullscreen;
@property NSColor *bgcolor;

// To fix scrolling in the Adrian Mole games
@property BOOL shouldScrollOnCharEvent;

@property BOOL shouldStoreScrollOffset;

@property BOOL previewDummy;

//@property BOOL adrianMole;
@property BOOL deadCities;
@property BOOL bureaucracy;
@property BOOL usesFont3;
@property BOOL beyondZork;
@property BOOL kerkerkruip;
@property BOOL thaumistry;
@property BOOL colderLight;

@property NSInteger autosaveVersion;
@property NSInteger autosaveTag;
@property BOOL hasAutoSaved;

@property BOOL voiceOverActive;

@property ZMenu *zmenu;
@property BOOL shouldCheckForMenu;
@property BOOL shouldSpeakNewText;
@property NSDate *speechTimeStamp;
@property GlkWindow *spokeLast;

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

- (IBAction)speakMostRecent:(id)sender;
- (IBAction)speakPrevious:(id)sender;
- (IBAction)speakNext:(id)sender;
- (IBAction)speakStatus:(id)sender;

- (NSArray *)accessibilityCustomActions API_AVAILABLE(macos(10.13));
- (NSArray *)createCustomRotors;
- (NSAccessibilityCustomRotorItemResult *)rotor:(NSAccessibilityCustomRotor *)rotor
                      resultForSearchParameters:(NSAccessibilityCustomRotorSearchParameters *)searchParameters API_AVAILABLE(macos(10.13));


@end
