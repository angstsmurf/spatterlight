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

#import "main.h"
#import "Compatibility.h"

#define MAXWIN 64
#define MAXSND 32

// Define the number of custom animation steps
#define DURATION_ADJUSTMENT 0.1
#define ANIMATION_STEPS 2

// A number of flags used when autorestoring.
//
// AUTORESTORED_BY_SYSTEM means that the window was open when
// the application was closed, and restored by the AppDelegate
// restoreWindowWithIdentifier method. The main difference from
// the manual autorestore that occurs when the user clicks on a game
// in the library window or similar, is that fullscreen is handled
// automatically

// AUTORESTORED_IN_FULLSCREEN means that the window was in fullscreen
// when it was closed and should open in fullscreen when autorestored.
// If the AUTORESTORED_BY_SYSTEM is not set, this has to be done
// in the autorestore_UI method.

// AUTORESTORED_BEFORE_FIRST_TURN means that the interpreter has made no
// internal autosave yet, because the game has not really started, but we
// have started the game before and saved any changes that the user has
// done to the interface, such as resizing the window (which will send an 
// arrange event to the interpreter, but may not trigger an autosave),
// typing at the prompt but not yet pressing enter, or scrolling of the
// text buffer. This means that there is a file named autosave-GUI.plist
// created by the Cocoa GUI window server, but not yet a autosave.plist
// created by the interpreter.

// RESETTING means that we have killed the interpreter process and want to
// start the game anew, deleting any existing autosave files. This reuses the
// game window and should not resize it. A reset may be initiated by the user
// from the file menu or autorestore alert at game stor, or may occur automatically
// when the game has reached its end, crashed or when an autorestore attempt failed.

typedef NS_OPTIONS(NSUInteger, AutorestoreOptions) {
    AUTORESTORED_BY_SYSTEM = 1 << 0,
    AUTORESTORED_IN_FULLSCREEN = 1 << 1,
    AUTORESTORED_BEFORE_FIRST_TURN = 1 << 2,
    RESETTING = 1 << 3
};

@interface GlkHelperView : NSView
{
    IBOutlet GlkController *delegate;
}
@end

@interface GlkController : NSWindowController
{
    /* for talking to the interpreter */
    NSTask *task;
    NSFileHandle *readfh;
    NSFileHandle *sendfh;

    /* current state of the protocol */
    NSTimer *timer;
    NSTimer *soundNotificationsTimer;
    BOOL waitforevent; /* terp wants an event */
    BOOL waitforfilename; /* terp wants a filename from a file dialog */
    BOOL dead; /* le roi est mort! vive le roi! */
    NSDictionary *lastArrangeValues;
    NSRect lastContentResize;
    BOOL contentViewResizable;

    BOOL inFullScreenResize;

    /* the glk objects */
    //GlkSoundChannel *gchannels[MAXSND];
    BOOL windowdirty; /* the contentView needs to repaint */

    /* image/sound resource uploading protocol */
    NSInteger lastimageresno;
    NSInteger lastsoundresno;
    NSImage *lastimage;
    NSData *lastsound;

    /* stylehints need to be copied to new windows, so we keep the values around */
    NSInteger styleuse[2][style_NUMSTYLES][stylehint_NUMHINTS];
    NSInteger styleval[2][style_NUMSTYLES][stylehint_NUMHINTS];

    /* keep some info around for the about-box and resetting*/
    NSString *gamefile;
    NSString *gameifid;
    NSString *terpname;

    NSDictionary *gameinfo;

    GlkController *restoredController;
}

@property NSMutableDictionary *gwindows;
@property IBOutlet NSView *borderView;
@property IBOutlet GlkHelperView *contentView;

@property (getter=isAlive, readonly) BOOL alive;
@property (readonly) NSTimeInterval storedTimerLeft;
@property (readonly) NSTimeInterval storedTimerInterval;
@property (readonly) NSRect storedWindowFrame;
@property (readonly) NSRect storedContentFrame;
@property (readonly) CGFloat storedBorder;
@property (readonly) CGFloat storedCharwidth;
@property (readonly) NSUInteger turns;

@property (readonly) NSRect contentFullScreenFrame;
@property (readonly) NSRect contentPreFullScreenFrame;
@property (readonly) NSRect windowPreFullscreenFrame;

@property (readonly) CGFloat fontSizePreFullscreen;

@property NSInteger firstResponderView;

@property NSMutableArray *queue;

@property (nonatomic) NSString *appSupportDir;
@property (nonatomic) NSString *autosaveFileGUI;
@property (nonatomic) NSString *autosaveFileTerp;

@property (readonly) BOOL supportsAutorestore;
@property (readonly) BOOL hasAutorestoredCocoa;
@property (readonly) BOOL hasAutorestoredGlk;
@property (readonly) BOOL storedFullscreen;

- (void) runTerp: (NSString*)terpname
    withGameFile: (NSString*)gamefilename
            IFID: (NSString*)gameifid
            info: (NSDictionary*)gameinfo
         options: (AutorestoreOptions)flags;

- (void) queueEvent: (GlkEvent*)gevent;
- (void) contentDidResize: (NSRect)frame;
- (void) markLastSeen;
- (void) performScroll;
- (void) setBorderColor: (NSColor *)color;
- (void) restoreFocus: (id) sender;
- (void) restoreUI: (GlkController *) controller;

@end
