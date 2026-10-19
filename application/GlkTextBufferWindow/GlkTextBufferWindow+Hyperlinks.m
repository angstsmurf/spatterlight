#import "GlkTextBufferWindowPrivate.h"

#import "GlkEvent.h"
#import "GlkController.h"
#import "GlkController+InterpreterGlue.h"
#import "MarginContainer.h"
#import "BufferTextView.h"

#import "DebugLog.h"
#import "NSView+Categories.h"

@implementation GlkTextBufferWindow (Hyperlinks)

// A hyperlink was clicked somewhere in the game window. If our line prompt is
// scrolled out of view, watch what the interpreter does with the click: a
// link that enters a command at the prompt should bring the prompt back into
// view, just as typing would (see linkDidEnterCommand).
- (void)linkWasClicked {
    linkClickPending = (line_request && !self.scrolledToBottom);
    lineCancelledAfterLink = nil;
}

// Tell every text buffer window about a hyperlink click. The link and the
// prompt may be in different windows (a side pane of object and exit links).
- (void)noteLinkClick {
    for (GlkWindow *win in self.glkctl.gwindows.allValues) {
        if ([win isKindOfClass:[GlkTextBufferWindow class]])
            [(GlkTextBufferWindow *)win linkWasClicked];
    }
}

// Send the interpreter a Glk link event for linkid. Answers the pending
// hyperlink request, so the caller must have checked hyper_request.
- (void)sendLinkEvent:(NSUInteger)linkid {
    [self noteLinkClick];
    GlkEvent *gev = [[GlkEvent alloc] initLinkEvent:linkid forWindow:self.name];
    [self.glkctl queueEvent:gev];
    hyper_request = NO;
    [self colderLightHack];
}

// Handle mouse clicks on margin images that have hyperlinks.
// Margin images live outside the text storage, so NSTextView's built-in
// link clicking doesn't reach them. This method converts the click point
// to text container coordinates and asks the MarginContainer to look up
// the hyperlink ID. Returns YES if a hyperlink was found and dispatched.
- (BOOL)myMouseDown:(NSEvent *)theEvent {
    // Don't draw a caret right now, even if we clicked at the prompt
    [_textview temporarilyHideCaret];

    if (hyper_request) {
        [self.glkctl markLastSeen];

        NSPoint p = [_textview containerPointForEvent:theEvent];

        NSUInteger linkid = [container findHyperlinkAt:p];
        if (linkid) {
            [self sendLinkEvent:linkid];
            return YES;
        }
    }
    return NO;
}

// NSTextViewDelegate: Handle clicks on inline text hyperlinks.
// Sends a Glk link event with the link's numeric ID back to the
// interpreter. Without a pending hyperlink request the click is swallowed:
// glkimp would hand the event to a game that never asked for it.
- (BOOL)textView:(NSTextView *)textview_
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
    if (!hyper_request) {
        NSLog(@"txtbuf: No hyperlink request in window.");
        return YES;
    }

    [self.glkctl markLastSeen];
    [self sendLinkEvent:((NSNumber *)link).unsignedIntegerValue];
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
