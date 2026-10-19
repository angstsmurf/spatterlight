/*
 * GlkController_Private.h
 *
 * Private interface for GlkController, shared across category files.
 * This header exposes ivars and internal methods needed by
 * GlkController categories.
 */

#import "GlkController.h"
#import "Theme.h"

#include <sys/select.h>

@class TableViewController, CommandScriptHandler, JourneyMenuHandler, GlkTextBufferWindow;

// Autoresizing masks for the game and border views.
// Fill the superview:
// Must match MAXWIN in glkimp/connect.c
static const NSInteger kMaxGlkWindows = 64;

static const NSAutoresizingMaskOptions kResizeFill = NSViewWidthSizable | NSViewHeightSizable;
// Fixed width, centered horizontally, stretched vertically:
static const NSAutoresizingMaskOptions kResizeCenteredFullHeight = NSViewHeightSizable | NSViewMinXMargin | NSViewMaxXMargin;
// Fixed size, centered horizontally, attached at the top:
static const NSAutoresizingMaskOptions kResizeCenteredAtTop = NSViewMinXMargin | NSViewMaxXMargin | NSViewMinYMargin;

// Wait up to milliseconds (0 = just poll) for fd to become readable.
static inline BOOL waitForData(int fd, int milliseconds) {
    fd_set fdset;
    struct timeval tmout = { milliseconds / 1000, (milliseconds % 1000) * 1000 };
    FD_ZERO(&fdset);
    FD_SET(fd, &fdset);
    return (select(fd + 1, &fdset, NULL, NULL, &tmout) > 0);
}

// Run block on the main queue after the given number of seconds.
static inline void runOnMainAfterDelay(NSTimeInterval seconds, dispatch_block_t block) {
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(seconds * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), block);
}

@interface GlkController () {
    @private
    /* for talking to the interpreter */
    NSTask *task;
    NSFileHandle *readfh;

    /* current state of the protocol */
    NSTimer *timer;
    BOOL waitforevent;    /* terp wants an event */
    BOOL waitforfilename; /* terp wants a filename from a file dialog */
    BOOL dead;            /* le roi est mort! vive le roi! */
    NSDictionary *lastArrangeValues;
    NSRect lastContentResize;

    BOOL inFullScreenResize;

    BOOL windowRestoredBySystem;

    // Used for fullscreen animation
    NSWindowController *snapshotController;

    BOOL windowClosedAlready;
    // We are registered for voiceOverEnabled KVO on NSWorkspace.
    BOOL observingVoiceOver;
    BOOL restartingAlready;

    // The interpreter has stopped for good: either it terminated during this
    // session, or we restored a window whose game had already finished. This is
    // narrower than `dead`, which is also YES during startup before the
    // subprocess is forked -- events queued in that window are legitimate and
    // get drained once the interpreter asks for them, whereas events queued
    // after the interpreter is gone would sit in the queue forever.
    BOOL terpHasStopped;

    /* the glk objects */
    BOOL windowdirty; /* the contentView needs to repaint */

    // Restored windows the interpreter closed before restoreUI adopted their
    // views, so restoreUI must leave them out.
    NSMutableSet<NSNumber *> *windowsClosedBeforeRestore;
    NSMutableData *bufferedData;

    TableViewController *libcontroller;

    NSSize lastSizeInChars;
    Theme *lastTheme;

    // To fix scrolling in the Adrian Mole games
    NSInteger lastRequest;

    // Command script handling
    BOOL skipNextScriptCommand;
    NSDate *lastScriptKeyTimestamp;
    NSDate *lastKeyTimestamp;

    kVOMenuPrefsType lastVOSpeakMenu;
    BOOL shouldAddTitlePrefixToSpeech;
    BOOL changedBorderThisTurn;

    // Backing ivars for lazy-initialized properties (accessed from categories)
    NSString *_autosaveFileGUI;
    NSString *_autosaveFileTerp;
    InfocomV6MenuHandler *_infocomV6MenuHandler;
    JourneyMenuHandler *_journeyMenuHandler;

    // The rest are @public only because SpatterlightTests sets them directly
    // (GridRestyleTests, QuoteBoxTests, BufferImageRescaleTests): sendfh to send
    // replies to /dev/null, the others to fake an autorestore without a
    // running interpreter.
    @public
    NSFileHandle *sendfh;

    BOOL shouldRestoreUI;
    // When a game supports autosave, but is still on its first turn,
    // so that no interpreter autosave file exists, we still restore the UI
    // to catch entered text and scroll position
    BOOL restoreUIOnly;
    BOOL autorestoring;

    GlkController *restoredController;
    GlkController *restoredControllerLate;
}

@property (nonatomic) JourneyMenuHandler *journeyMenuHandler;
// For optional interpreter output logging
@property (nonatomic) NSFileHandle *interpreterLogFileHandle;
@property NSURL *saveDir;

// Internal state shared between the categories
@property NSMutableArray<GlkWindow *> *windowsToBeAdded;
@property NSMutableArray<GlkWindow *> *windowsToBeRemoved;

@property (weak) Theme *stashedTheme;

// Set during autorestore when the autosaved theme name cannot be resolved to
// an existing theme, meaning the archived window styles may not match the
// current theme and must be rebuilt. See adoptRestoredTheme.
@property BOOL restoredStylesStale;

// systemUptime of the most recent window-layout change (window resize, etc.).
// Used to debounce VoiceOver announcements so they fire only after the UI has
// stopped churning (e.g. after a full-screen graphic is dismissed and the
// border/status windows are redrawn), preventing the announcement from being
// interrupted by AppKit re-reading the focused window on each frame change.
@property NSTimeInterval lastLayoutChurnTimestamp;

// Redeclare readonly properties as readwrite for category access
@property(readwrite) NSRect storedWindowFrame;
@property(readwrite) NSRect storedGameViewFrame;
@property(readwrite) BOOL inFullscreen;
@property(readwrite) NSInteger firstResponderView;
@property(readwrite) NSMutableArray *queue;
@property(weak, readwrite) Game *game;
@property(readwrite) NSString *gamefile;

// Internal methods implemented in GlkController.m
- (void)flushDisplay;
- (void)guessFocus;
- (NSArray<GlkTextBufferWindow *> *)bufferWindows;
- (void)identifyGame:(NSString *)ifid;
- (void)identifyGameUnlessNoHacks:(Theme *)theme;
- (void)runTerpNormal;
- (NSRect)frameWithSanitycheckedSize:(NSRect)rect;
- (NSSize)defaultContentSize;
- (NSSize)contentSizeToCharCells:(NSSize)points;
- (NSSize)charCellsToContentSize:(NSSize)cells;
- (void)zoomContentToSize:(NSSize)newSize;
- (void)sendArrangeEventWithFrame:(NSRect)frame force:(BOOL)force;
- (void)notePreferencesChanged:(NSNotification *)notify;
- (void)showGameFileGoneAlert;
- (BOOL)runWindowsRestorationHandler;
- (void)restoreWindowWhenDead;
- (void)noteColorModeChanged:(NSNotification *)notification;
- (void)adjustMaskLayer:(id)sender;
- (CALayer *)createMaskLayer;

@end
