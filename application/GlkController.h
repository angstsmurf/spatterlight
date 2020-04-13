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

@class Game, Theme;

#define MAXWIN 64
#define MAXSND 32

@interface GlkHelperView : NSView {
}

@property IBOutlet GlkController *glkctrl;

@end

@interface GlkController : NSWindowController {
    /* for talking to the interpreter */
    NSTask *task;
    NSFileHandle *readfh;
    NSFileHandle *sendfh;

    /* current state of the protocol */
    NSTimer *timer;
    NSTimer *soundNotificationsTimer;
    BOOL waitforevent;    /* terp wants an event */
    BOOL waitforfilename; /* terp wants a filename from a file dialog */
    BOOL dead;            /* le roi est mort! vive le roi! */
    NSDictionary *lastArrangeValues;
    NSRect lastContentResize;

    BOOL inFullScreenResize;

    BOOL windowRestoredBySystem;
    BOOL shouldRestoreUI;
    BOOL shouldShowAutorestoreAlert;

    NSSize borderFullScreenSize;
    NSWindow *snapshotWindow;

    BOOL windowClosedAlready;

    /* the glk objects */
    BOOL windowdirty; /* the contentView needs to repaint */

    /* image/sound resource uploading protocol */
    NSInteger lastimageresno;
    NSInteger lastsoundresno;
    NSImage *lastimage;
    NSData *lastsound;

    GlkController *restoredController;
    NSUInteger turns;
    NSMutableData *bufferedData;

    LibController *libcontroller;

    NSSize lastSizeInChars;
    Theme *lastTheme;

    NSInteger lastRequest;
}

@property NSMutableDictionary *gwindows;
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

@property BOOL shouldScrollOnInputEvent;
@property BOOL shouldStoreScrollOffset;

@property BOOL previewDummy;

@property NSInteger autosaveVersion;

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

@end
