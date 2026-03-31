/*
 * GlkController_Private.h
 *
 * Private interface for GlkController, shared across category files.
 * This header exposes ivars and internal methods needed by
 * GlkController categories.
 */

#import "GlkController.h"
#import "Theme.h"

@class TableViewController, CommandScriptHandler, JourneyMenuHandler;

@interface GlkController () {
    @public
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

    // When a game supports autosave, but is still on its first turn,
    // so that no interpreter autosave file exists, we still restore the UI
    // to catch entered text and scroll position
    BOOL restoreUIOnly;

    // Used for fullscreen animation
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

    // Command script handling
    BOOL skipNextScriptCommand;
    NSDate *lastScriptKeyTimestamp;
    NSDate *lastKeyTimestamp;

    kVOMenuPrefsType lastVOSpeakMenu;
    BOOL shouldAddTitlePrefixToSpeech;
    BOOL changedBorderThisTurn;
    BOOL autorestoring;

    // Backing ivars for lazy-initialized properties (accessed from categories)
    NSString *_autosaveFileGUI;
    NSString *_autosaveFileTerp;
    InfocomV6MenuHandler *_infocomV6MenuHandler;
    JourneyMenuHandler *_journeyMenuHandler;
    
    // For optional interpreter output logging
    NSFileHandle *interpreterLogFileHandle;
}

@property (nonatomic) JourneyMenuHandler *journeyMenuHandler;
@property NSURL *saveDir;

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
- (void)performScroll;
- (void)identifyGame:(NSString *)ifid;
- (void)resetGameIdentification;
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
