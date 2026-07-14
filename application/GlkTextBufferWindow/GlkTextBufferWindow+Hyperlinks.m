#import "GlkTextBufferWindowPrivate.h"

#import "GlkEvent.h"
#import "GlkController.h"
#import "GlkController+InterpreterGlue.h"
#import "MarginContainer.h"
#import "BufferTextView.h"

// In release builds, suppress NSLog entirely.
#ifndef DEBUG
#define NSLog(...)
#endif // DEBUG

@implementation GlkTextBufferWindow (Hyperlinks)

// Begin listening for hyperlink click events from the interpreter.
- (void)initHyperlink {
    hyper_request = YES;
}

// Stop listening for hyperlink click events.
- (void)cancelHyperlink {
    hyper_request = NO;
}

// Handle mouse clicks on margin images that have hyperlinks.
// Margin images live outside the text storage, so NSTextView's built-in
// link clicking doesn't reach them. This method converts the click point
// to text container coordinates and asks the MarginContainer to look up
// the hyperlink ID. Returns YES if a hyperlink was found and dispatched.
- (BOOL)myMouseDown:(NSEvent *)theEvent {
    GlkEvent *gev;

    // Don't draw a caret right now, even if we clicked at the prompt
    [_textview temporarilyHideCaret];

    if (hyper_request) {
        [self.glkctl markLastSeen];

        NSPoint p;
        p = theEvent.locationInWindow;
        p = [_textview convertPoint:p fromView:nil];
        p.x -= _textview.textContainerInset.width;
        p.y -= _textview.textContainerInset.height;

        NSUInteger linkid = [container findHyperlinkAt:p];
        if (linkid) {
            gev = [[GlkEvent alloc] initLinkEvent:linkid forWindow:self.name];
            [self.glkctl queueEvent:gev];
            hyper_request = NO;
            [self colderLightHack];
            return YES;
        }
    }
    return NO;
}

// NSTextViewDelegate: Handle clicks on inline text hyperlinks.
// Sends a Glk link event with the link's numeric ID back to the
// interpreter. Also triggers the colderLightHack if needed.
- (BOOL)textView:(NSTextView *)textview_
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {

    if (!hyper_request) {
        NSLog(@"txtbuf: No hyperlink request in window.");
    }

    [self.glkctl markLastSeen];

    GlkEvent *gev =
    [[GlkEvent alloc] initLinkEvent:((NSNumber *)link).unsignedIntegerValue
                          forWindow:self.name];
    [self.glkctl queueEvent:gev];

    hyper_request = NO;
    [self colderLightHack];
    return YES;
}

// Game-specific workaround: "A Colder Light" only updates its title
// bar in response to arrange events. After a hyperlink click, we send
// a synthetic arrange event so the game refreshes its display.
- (void)colderLightHack {

    GlkController *glkctl = self.glkctl;
    if (glkctl.gameID == kGameIsAColderLight) {
        GlkEvent *gev = [[GlkEvent alloc] initArrangeWidth:(NSInteger)glkctl.gameView.frame.size.width
                                                    height:(NSInteger)glkctl.gameView.frame.size.height
                                                     theme:glkctl.theme
                                                     force:YES];
        [glkctl queueEvent:gev];
    }
}

@end
