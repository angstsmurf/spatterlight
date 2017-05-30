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

#define MAXWIN 64
#define MAXSND 32

@interface GlkHelperView : NSView
{
    IBOutlet GlkController *delegate;
}
@end

@interface GlkController : NSWindowController
{
    IBOutlet GlkHelperView *contentView;
    
    /* for talking to the interpreter */
    NSTask *task;
    NSFileHandle *readfh;
    NSFileHandle *sendfh;
    NSMutableArray *queue;
    
    /* current state of the protocol */
    NSTimer *timer;
    NSInteger waitforevent; /* terp wants an event */
    NSInteger waitforfilename; /* terp wants a filename from a file dialog */
    NSInteger dead; /* le roi est mort! vive le roi! */
    
    /* the glk objects */
    GlkWindow *gwindows[MAXWIN];
    GlkSoundChannel *gchannels[MAXSND];
    int windowdirty; /* the contentView needs to repaint */
    
    /* image/sound resource uploading protocol */
    NSInteger lastimageresno;
    NSInteger lastsoundresno;
    NSImage *lastimage;
    NSData *lastsound;
    
    /* stylehints need to be copied to new windows, so we keep the values around */
    NSInteger styleuse[2][style_NUMSTYLES][stylehint_NUMHINTS];
    NSInteger styleval[2][style_NUMSTYLES][stylehint_NUMHINTS];
    
    /* keep some info around for the about-box */
    NSString *gamefile;
    NSString *gameifid;
    NSDictionary *gameinfo;
}

- (void) runTerp: (NSString*)terpname
    withGameFile: (NSString*)gamefilename
            IFID: (NSString*)gameifid
            info: (NSDictionary*)gameinfo;
- (void) queueEvent: (GlkEvent*)gevent;
- (void) contentDidResize: (NSRect)frame;
@property (getter=isAlive, readonly) BOOL alive;
- (void) markLastSeen;
- (void) performScroll;
- (id) windowWithNum: (int)index;

@end
