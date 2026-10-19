//
//  GlkHelperView.m
//  Spatterlight
//

#import "GlkHelperView.h"
#import "GlkController.h"
#import "GlkController+FullScreen.h"

@implementation GlkHelperView

// Use top-left origin to match Glk's coordinate system.
- (BOOL)isFlipped {
    return YES;
}

// Notify the controller when the frame changes programmatically (not during
// a live resize drag, which is handled separately to avoid excessive events).
- (void)setFrame:(NSRect)frame {
    super.frame = frame;
    GlkController *glkctl = _glkctrl;

    if (glkctl.alive && !self.inLiveResize && !glkctl.ignoreResizes) {
        [glkctl contentDidResize:frame];
    }
}

// Capture scroll offsets before a user-initiated live resize begins,
// so they can be restored afterward to prevent text from jumping around.
- (void)viewWillStartLiveResize {
    GlkController *glkctl = _glkctrl;
    if (!glkctl.isInSystemFullscreen && !glkctl.ignoreResizes) {
        [glkctl storeScrollOffsets];
    }
}

// After the user finishes dragging to resize, send the final content size
// to the controller and restore the previously saved scroll offsets.
- (void)viewDidEndLiveResize {
    GlkController *glkctl = _glkctrl;
    // We use a custom fullscreen width, so don't resize to full screen width
    // when viewDidEndLiveResize is called because we just entered fullscreen
    if (!glkctl.isInSystemFullscreen && !glkctl.ignoreResizes) {
        [glkctl contentDidResize:self.frame];
        [glkctl restoreScrollOffsets];
    }
}

@end
