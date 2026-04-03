#import "GlkController_Private.h"
#import "GlkController+Autorestore.h"

#import "GlkWindow.h"
#import "GlkTextBufferWindow.h"
#import "GlkTextGridWindow.h"
#import "GlkGraphicsWindow.h"
#import "Theme.h"
#import "GlkEvent.h"
#import "CoverImageHandler.h"
#import "CoverImageView.h"
#import "Preferences.h"

#ifndef DEBUG
#define NSLog(...)
#endif

@implementation GlkController (FullScreen)

- (NSSize)window:(NSWindow *)window willUseFullScreenContentSize:(NSSize)proposedSize {
    if (window != self.window)
        return proposedSize;
    if (self.showingCoverImage) {
        [self.coverController.imageView positionImage];
    }

    self.gameView.autoresizingMask = NSViewHeightSizable | NSViewMinXMargin | NSViewMaxXMargin;
    self.borderView.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;

    if (!inFullScreenResize) {
        NSSize borderSize = self.borderView.frame.size;
        NSRect contentFrame = self.gameView.frame;
        CGFloat midWidth = borderSize.width / 2;
        if (contentFrame.origin.x > midWidth ||  NSMaxX(contentFrame) < midWidth) {
            contentFrame.origin.x = round(borderSize.width - NSWidth(contentFrame) / 2);
            self.gameView.frame = contentFrame;
        }
        if (NSWidth(contentFrame) > borderSize.width - 2 * self.theme.border || borderSize.width < borderSize.height) {
            contentFrame.size.width = ceil(borderSize.width - 2 * self.theme.border);
            contentFrame.origin.x = self.theme.border;
            self.gameView.frame = contentFrame;
        }
    }

    self.borderFullScreenSize = proposedSize;
    return proposedSize;
}

- (NSArray *)customWindowsToEnterFullScreenForWindow:(NSWindow *)window {
    if (window == self.window) {
        if (restoredController && restoredController.inFullscreen) {
            return @[ window ];
        } else {
            if (self.showingCoverImage) {
                return [self.coverController customWindowsToEnterFullScreenForWindow:window];
            }
            [self makeAndPrepareSnapshotWindow];
            NSWindow *snapshotWindow = snapshotController.window;
            if (snapshotWindow)
                return @[ window, snapshotWindow ];
        }
    }
    return nil;
}

- (NSArray *)customWindowsToExitFullScreenForWindow:(NSWindow *)window {
    if (window == self.window) {
        if (self.showingCoverImage) {
            return [self.coverController customWindowsToExitFullScreenForWindow:window];
        }
        return @[ window ];
    } else
        return nil;
}

- (void)windowWillEnterFullScreen:(NSNotification *)notification {
    // Save the window frame in self.windowPreFullscreenFrame so that it can be restored when leaving fullscreen.

    // If we are starting up in "system" fullscreen,
    // we will use the autosaved windowPreFullscreenFrame
    // instead (which will be set in the restoreUI method)
    if (self.restorationHandler == nil) {
        self.windowPreFullscreenFrame = self.window.frame;
        [self storeScrollOffsets];
        self.ignoreResizes = YES;
        // self.ignoreResizes means no storing scroll offsets,
        // but also no arrange events
    }
    // Sanity check the pre-fullscreen window size.
    // If something has gone wrong, such as the autosave-GUI
    // files becoming corrupted or deletd, this will
    // ensure that the window size is still sensible.
    self.windowPreFullscreenFrame = [self frameWithSanitycheckedSize:self.windowPreFullscreenFrame];
    self.inFullscreen = YES;

}

- (void)windowDidFailToEnterFullScreen:(NSWindow *)window {
    self.inFullscreen = NO;
    self.ignoreResizes = NO;
    inFullScreenResize = NO;
    self.gameView.alphaValue = 1;
    [window setFrame:[self frameWithSanitycheckedSize:self.windowPreFullscreenFrame] display:YES];
    self.gameView.frame = [self contentFrameForWindowed];
    [self restoreScrollOffsets];
    self.gameView.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
    self.borderView.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
}

- (void)storeScrollOffsets {
    if (self.ignoreResizes)
        return;
    for (GlkWindow *win in self.gwindows.allValues)
        if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
            ((GlkTextBufferWindow *)win).pendingScrollRestore = NO;
            [(GlkTextBufferWindow *)win storeScrollOffset];
            ((GlkTextBufferWindow *)win).pendingScrollRestore = YES;
        }
}

- (void)restoreScrollOffsets {
    for (GlkWindow *win in self.gwindows.allValues)
        if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
            [(GlkTextBufferWindow *)win restoreScrollBarStyle];
            [(GlkTextBufferWindow *)win performSelector:@selector(restoreScroll:) withObject:nil afterDelay:0.2];
        }
}

- (void)window:(NSWindow *)window
startCustomAnimationToEnterFullScreenWithDuration:(NSTimeInterval)duration {
    if (window != self.window)
        return;

    inFullScreenResize = YES;

    // Make sure the window style mask includes the full screen bit
    window.styleMask = (window.styleMask | NSWindowStyleMaskFullScreen);

    if (restoredController && restoredController.inFullscreen) {
        [self startGameInFullScreenAnimationWithDuration:duration];
    } else {
        if (self.showingCoverImage) {
            [self.coverController enterFullScreenWithDuration:duration];
            return;
        }

        [self enterFullScreenAnimationWithDuration:duration];
    }
}

- (void)enterFullScreenAnimationWithDuration:(NSTimeInterval)duration {
    NSWindow *window = self.window;
    NSWindow *snapshotWindow = snapshotController.window;
    // Make sure the snapshot window style mask includes the full screen bit
    snapshotWindow.styleMask = (snapshotWindow.styleMask | NSWindowStyleMaskFullScreen);
    [snapshotWindow setFrame:window.frame display:YES];

    NSScreen *screen = window.screen;

    if (NSEqualSizes(self.borderFullScreenSize, NSZeroSize))
        self.borderFullScreenSize = screen.frame.size;

    // The final, full screen frame
    NSRect border_finalFrame = screen.frame;
    border_finalFrame.size = self.borderFullScreenSize;

    // The center frame for the window is used during
    // the 1st half of the fullscreen animation and is
    // the window at its original size but moved to the
    // center of its eventual full screen frame.

    NSRect centerBorderFrame = NSMakeRect(floor((screen.frame.size.width -
                                                 self.borderView.frame.size.width) /
                                                2), self.borderFullScreenSize.height
                                          - self.borderView.frame.size.height,
                                          self.borderView.frame.size.width,
                                          self.borderView.frame.size.height);

    NSRect centerWindowFrame = [window frameRectForContentRect:centerBorderFrame];

    centerWindowFrame.origin.x += screen.frame.origin.x;
    centerWindowFrame.origin.y += screen.frame.origin.y;

    self.borderView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    self.gameView.autoresizingMask = NSViewMinXMargin | NSViewMaxXMargin |
    NSViewMinYMargin; // Attached at top but not bottom or sides

    NSView *localContentView = self.gameView;
    NSView *localBorderView = self.borderView;
    NSWindow *localSnapshot = snapshotController.window;

    GlkController * __weak weakSelf = self;
    // Hide contentview
    self.gameView.alphaValue = 0;

    // Our animation will be broken into five steps.
    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        // First, we move the window to the center
        // of the screen with the snapshot window on top
        context.duration = duration / 4 + 0.1;
        [[localSnapshot animator] setFrame:centerWindowFrame display:YES];
        [[window animator] setFrame:centerWindowFrame display:YES];
    }
     completionHandler:^{
        [NSAnimationContext
         runAnimationGroup:^(NSAnimationContext *context) {
            // and then we enlarge it to its full size.
            context.duration = duration / 4 - 0.1;
            [[window animator]
             setFrame:[window
                       frameRectForContentRect:border_finalFrame]
             display:YES];
        }
         completionHandler:^{
            // We get the invisible content view into position ...
            NSRect newContentFrame = localContentView.frame;

            newContentFrame.origin =
            NSMakePoint(floor((NSWidth(localBorderView.bounds) -
                               NSWidth(localContentView.frame)) /
                              2),
                        floor(NSHeight(localBorderView.bounds) - weakSelf.theme.border - localContentView.frame.size.height)
                        );
            [NSAnimationContext
             runAnimationGroup:^(NSAnimationContext *context) {
                context.duration = duration / 4;
                localContentView.frame = newContentFrame;
                [weakSelf sendArrangeEventWithFrame:localContentView.frame force:NO];
                [weakSelf flushDisplay];
            }
             completionHandler:^{
                // Now we can fade out the snapshot window
                localContentView.alphaValue = 1;

                [NSAnimationContext
                 runAnimationGroup:^(NSAnimationContext *context) {
                    context.duration = duration / 4;
                    [localSnapshot.contentView animator].alphaValue = 0;
                }
                 completionHandler:^{
                    // Finally, we extend the content view vertically if needed.
                    [NSAnimationContext
                     runAnimationGroup:^(NSAnimationContext *context) {
                        context.duration = 0.2;
                        [localContentView animator].frame = [weakSelf contentFrameForFullscreen];
                    }
                     completionHandler:^{
                        // Hide the snapshot window.
                        ((NSView *)localSnapshot.contentView).hidden = YES;
                        ((NSView *)localSnapshot.contentView).alphaValue = 1;

                        // Send an arrangement event to fill
                        // the new extended area
                        [weakSelf sendArrangeEventWithFrame:localContentView.frame force:NO];
                        [weakSelf restoreScrollOffsets];
                        for (GlkTextGridWindow *quotebox in weakSelf.quoteBoxes)
                        {
                            [quotebox performSelector:@selector(quoteboxAdjustSize:) withObject:nil afterDelay:0.1];
                        }
                    }];
                }];
            }];
        }];
    }];
}

- (void)startGameInFullScreenAnimationWithDuration:(NSTimeInterval)duration {

    NSWindow *window = self.window;
    NSScreen *screen = window.screen;

    if (NSEqualSizes(self.borderFullScreenSize, NSZeroSize))
        self.borderFullScreenSize = screen.frame.size;

    // The final, full screen frame
    NSRect border_finalFrame = screen.frame;
    border_finalFrame.size = self.borderFullScreenSize;

    // The center frame for the window is used during
    // the 1st half of the fullscreen animation and is
    // the window at its original size but moved to the
    // center of its eventual full screen frame.

    NSRect centerWindowFrame = [self frameWithSanitycheckedSize:self.windowPreFullscreenFrame];
    centerWindowFrame.origin = NSMakePoint(screen.frame.origin.x + floor((screen.frame.size.width -
                                                                          self.borderView.frame.size.width) /
                                                                         2), screen.frame.origin.x + self.borderFullScreenSize.height
                                           - centerWindowFrame.size.height);

    centerWindowFrame.origin.x += screen.frame.origin.x;
    centerWindowFrame.origin.y += screen.frame.origin.y;

    GlkController * __weak weakSelf = self;

    BOOL stashShouldShowAlert = self.shouldShowAutorestoreAlert;
    self.shouldShowAutorestoreAlert = NO;

    // Our animation will be broken into three steps.
    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        // First, we move the window to the center
        // of the screen
        context.duration = duration / 2;
        [[window animator] setFrame:centerWindowFrame display:YES];
    }
     completionHandler:^{
        [NSAnimationContext
         runAnimationGroup:^(NSAnimationContext *context) {
            // then, we enlarge the window to its full size.
            context.duration = duration / 2;
            [[window animator]
             setFrame:[window frameRectForContentRect:border_finalFrame]
             display:YES];
        }
         completionHandler:^{
            GlkController *strongSelf = weakSelf;
            // Finally, we get the content view into position ...
            if (strongSelf) {

                [strongSelf enableArrangementEvents];
                [strongSelf sendArrangeEventWithFrame:[strongSelf contentFrameForFullscreen] force:NO];

                strongSelf.shouldShowAutorestoreAlert = stashShouldShowAlert;
                [strongSelf performSelector:@selector(showAutorestoreAlert:) withObject:nil afterDelay:0.1];
                [strongSelf restoreScrollOffsets];
            }
        }];
    }];
}

- (void)enableArrangementEvents {
    inFullScreenResize = NO;
}

- (void)window:window startCustomAnimationToExitFullScreenWithDuration:(NSTimeInterval)duration {
    if (self.showingCoverImage) {
        [self.coverController exitFullscreenWithDuration:duration];
        return;
    }
    [self storeScrollOffsets];
    self.ignoreResizes = YES;
    self.windowPreFullscreenFrame = [self frameWithSanitycheckedSize:self.windowPreFullscreenFrame];
    NSRect oldFrame = self.windowPreFullscreenFrame;

    oldFrame.size.width =
    self.gameView.frame.size.width + self.theme.border * 2;

    inFullScreenResize = YES;

    self.gameView.autoresizingMask =
    NSViewMinXMargin | NSViewMaxXMargin | NSViewMinYMargin;

    NSWindow __weak *localWindow = self.window;
    NSView __weak *localBorderView = self.borderView;
    NSView __weak *localContentView =self.gameView;
    GlkController * __weak weakSelf = self;

    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        // Make sure the window style mask does not
        // include full screen bit
        context.duration = duration;
        [window
         setStyleMask:(NSUInteger)([window styleMask] & ~(NSUInteger)NSWindowStyleMaskFullScreen)];
        [[window animator] setFrame:oldFrame display:YES];
    }
     completionHandler:^{
        [weakSelf enableArrangementEvents];
        localBorderView.frame = ((NSView *)localWindow.contentView).frame;
        localContentView.frame = [weakSelf contentFrameForWindowed];
        localContentView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        [weakSelf performSelector:@selector(windowDidExitFullScreen:) withObject:nil afterDelay:0];
    }];
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification {
    self.ignoreResizes = NO;
    inFullScreenResize = NO;
    self.inFullscreen = YES;
    if (self.gameView.frame.size.width < 200)
        [self adjustContentView];
    [self contentDidResize:self.gameView.frame];
    lastSizeInChars = [self contentSizeToCharCells:self.gameView.frame.size];
    self.gameView.autoresizingMask = NSViewHeightSizable | NSViewMinXMargin | NSViewMaxXMargin;
    [self restoreScrollOffsets];
}

- (void)windowDidExitFullScreen:(NSNotification *)notification {
    self.ignoreResizes = NO;
    self.inFullscreen = NO;
    inFullScreenResize = NO;
    [self contentDidResize:self.gameView.frame];
    [self restoreScrollOffsets];
    if (self.gameID == kGameIsNarcolepsy && self.theme.doGraphics && self.theme.doStyles) {
        // FIXME: Very ugly hack to fix the Narcolepsy mask layer
        // It breaks when exiting fullscreen after the player
        // manually has resized the window in windowed mode.
        NSRect newFrame = self.window.frame;
        newFrame.size.width += 1;
        [self.window setFrame:newFrame display:YES];
        newFrame.size.width -= 1;
        [self.window setFrame:newFrame display:YES];
        [self adjustMaskLayer:nil];
    }
    self.gameView.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
}

- (void)startInFullscreen {
    // First we show the game windowed
    [self.window setFrame:restoredControllerLate.windowPreFullscreenFrame
                  display:NO];
    [self showWindow:nil];
    [self.window makeKeyAndOrderFront:nil];

    self.gameView.frame = [self contentFrameForWindowed];

    self.gameView.autoresizingMask = NSViewMinXMargin | NSViewMaxXMargin |
    NSViewMinYMargin; // Attached at top but not bottom or sides

    [self contentDidResize:self.gameView.frame];
    for (GlkWindow *win in self.gwindows.allValues)
        if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
            GlkTextBufferWindow *bufwin = (GlkTextBufferWindow *)win;
            [bufwin restoreScrollBarStyle];
            // This will prevent storing scroll
            // positions during fullscreen animation
            bufwin.pendingScrollRestore = YES;
        }
}

- (void)deferredEnterFullscreen:(id)sender {
    autorestoring = NO;
    [self.window toggleFullScreen:nil];
    [self performSelector:@selector(showAutorestoreAlert:) withObject:nil afterDelay:1];
}

- (CALayer *)takeSnapshot {
    [self showWindow:nil];
    CGImageRef windowSnapshot = CGWindowListCreateImage(CGRectNull,
                                                        kCGWindowListOptionIncludingWindow,
                                                        (CGWindowID)(self.window).windowNumber,
                                                        kCGWindowImageBoundsIgnoreFraming);
    CALayer *snapshotLayer = [CALayer layer];
    snapshotLayer.frame = self.window.frame;
    snapshotLayer.anchorPoint = CGPointMake(0, 0);
    snapshotLayer.contents = CFBridgingRelease(windowSnapshot);
    return snapshotLayer;
}

- (void)makeAndPrepareSnapshotWindow {
    NSWindow *snapshotWindow;
    if (snapshotController.window) {
        snapshotWindow = snapshotController.window;
        snapshotWindow.contentView.hidden = NO;
        for (CALayer *layer in snapshotWindow.contentView.layer.sublayers)
            [layer removeFromSuperlayer];
    } else {
        snapshotWindow = ([[NSWindow alloc]
                           initWithContentRect:self.window.frame
                           styleMask:0
                           backing:NSBackingStoreBuffered
                           defer:NO]);
        snapshotController = [[NSWindowController alloc] initWithWindow:snapshotWindow];
        snapshotWindow.contentView.wantsLayer = YES;
        snapshotWindow.opaque = NO;
        snapshotWindow.releasedWhenClosed = YES;
        snapshotWindow.backgroundColor = NSColor.clearColor;
    }

    CALayer *snapshotLayer = [self takeSnapshot];

    [snapshotWindow setFrame:self.window.frame display:NO];
    [snapshotWindow.contentView.layer addSublayer:snapshotLayer];
    // Compute the frame of the snapshot layer such that the snapshot is
    // positioned exactly on top of the original position of the game window.
    NSRect snapshotLayerFrame =
    [snapshotWindow convertRectFromScreen:self.window.frame];
    snapshotLayer.frame = snapshotLayerFrame;
    [(NSWindowController *)snapshotWindow.delegate showWindow:nil];
    [snapshotWindow orderFront:nil];
}

// Some convenience methods
- (void)adjustContentView {
    NSRect frame;
    if ((self.window.styleMask & NSWindowStyleMaskFullScreen) == NSWindowStyleMaskFullScreen || self.inFullscreen) {
        // We are in fullscreen
        frame = [self contentFrameForFullscreen];
    } else {
        // We are not in fullscreen
        frame = [self contentFrameForWindowed];
    }

    CGFloat border = self.theme.border;
    NSRect contentRect = frame;
    contentRect.size = NSMakeSize(frame.size.width + 2 * border, frame.size.height + 2 * border);
    contentRect.origin = NSMakePoint(frame.origin.x - border, frame.origin.y - border);
    if (contentRect.size.width < (CGFloat)kMinimumWindowWidth || contentRect.size.height < (CGFloat)kMinimumWindowHeight) {
        contentRect = [self frameWithSanitycheckedSize:contentRect];
        frame.size.width = contentRect.size.width - 2 * border;
        frame.size.height = contentRect.size.height - 2 * border;
    }

    NSRect windowframe = self.window.frame;
    NSRect frameForContent = [self.window frameRectForContentRect:contentRect];
    if (windowframe.size.width < frameForContent.size.width || windowframe.size.height < frameForContent.size.height) {
        [self.window setFrame:[self.window frameRectForContentRect:frame] display:YES];
    }

    self.gameView.frame = frame;
}

- (NSRect)contentFrameForWindowed {
    CGFloat border = self.theme.border;
    return NSMakeRect((CGFloat)border, (CGFloat)border,
                      round(NSWidth(self.borderView.bounds) - border * 2),
                      round(NSHeight(self.borderView.bounds) - border * 2));
}

- (NSRect)contentFrameForFullscreen {
    CGFloat border = self.theme.border;
    return NSMakeRect(floor((NSWidth(self.borderView.bounds) -
                             NSWidth(self.gameView.frame)) / 2),
                      border, NSWidth(self.gameView.frame),
                      round(NSHeight(self.borderView.bounds) - border * 2));
}

@end
