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

#import "DebugLog.h"

@implementation GlkController (FullScreen)

- (NSSize)window:(NSWindow *)window willUseFullScreenContentSize:(NSSize)proposedSize {
    if (window != self.window)
        return proposedSize;
    if (self.showingCoverImage) {
        [self.coverController.imageView positionImage];
    }

    self.gameView.autoresizingMask = kResizeCenteredFullHeight;
    self.borderView.autoresizingMask = kResizeFill;

    if (!inFullScreenResize) {
        NSSize borderSize = self.borderView.frame.size;
        NSRect contentFrame = self.gameView.frame;
        CGFloat midWidth = borderSize.width / 2;
        if (contentFrame.origin.x > midWidth || NSMaxX(contentFrame) < midWidth) {
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

// Called once the system has applied its own saved state to a window it is
// restoring, which happens some time after we hand the window over through
// the restoration handler. That state is written lazily, so after a crash or
// a force quit it can disagree with our autosave: we restored the game as
// being in fullscreen, in a window as large as the screen, and the system
// then puts the window back as an ordinary one. Go with what the system did
// and fit the game view to the window.
- (void)window:(NSWindow *)window didDecodeRestorableState:(NSCoder *)state {
    if (window != self.window || !self.inFullscreen || self.isInSystemFullscreen)
        return;
    self.inFullscreen = NO;
    self.gameView.autoresizingMask = kResizeFill;
    [self adjustContentView];
    [self restoreScrollOffsets];
}

- (void)windowWillEnterFullScreen:(NSNotification *)notification {
    // Save the window frame in self.windowPreFullscreenFrame so that it can be restored when leaving fullscreen.

    // If we are starting up in "system" fullscreen,
    // we will use the autosaved windowPreFullscreenFrame
    // instead (which will be set in the restoreUI method).
    // The system only takes the window into fullscreen after it has been
    // handed over through the restoration handler, which is gone by now, so
    // that cannot be what tells us. What does is that we already count as
    // being in fullscreen, having been restored that way from the autosave,
    // without having put ourselves there (as startInFullscreen does). The
    // window frame is then the stored fullscreen one, not a frame to go
    // back to.
    if (!self.inFullscreen || self.startingInFullscreen) {
        self.windowPreFullscreenFrame = self.window.frame;
        NSLog(@"windowWillEnterFullScreen calling storeScrollOffsets");
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
    NSLog(@"windowDidFailToEnterFullScreen calling restoreScrollOffsets");
    [self restoreScrollOffsets];
    self.gameView.autoresizingMask = kResizeFill;
    self.borderView.autoresizingMask = kResizeFill;
}

- (void)storeScrollOffsets {
    if (self.ignoreResizes)
        return;
    for (GlkTextBufferWindow *win in self.bufferWindows) {
        win.pendingScrollRestore = NO;
        [win storeScrollOffset];
        win.pendingScrollRestore = YES;
    }
}

- (void)restoreScrollOffsets {
    for (GlkTextBufferWindow *win in self.bufferWindows) {
        [win restoreScrollBarStyle];
        [win performSelector:@selector(restoreScroll:) withObject:nil afterDelay:0.2];
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

// The border view's frame once in fullscreen on screen. The size is
// remembered (in borderFullScreenSize) the first time.
- (NSRect)fullScreenBorderFrameOnScreen:(NSScreen *)screen {
    if (NSEqualSizes(self.borderFullScreenSize, NSZeroSize))
        self.borderFullScreenSize = screen.frame.size;
    NSRect frame = screen.frame;
    frame.size = self.borderFullScreenSize;
    return frame;
}

// Our own enter-fullscreen animation, in five steps, each started from the
// previous one's completion handler: move the window (under the snapshot
// window) to the center of the screen, enlarge it to full size, move the
// still invisible game view into place, fade out the snapshot, and finally
// extend the game view vertically.
- (void)enterFullScreenAnimationWithDuration:(NSTimeInterval)duration {
    NSWindow *window = self.window;
    NSWindow *snapshotWindow = snapshotController.window;
    // Make sure the snapshot window style mask includes the full screen bit
    snapshotWindow.styleMask = (snapshotWindow.styleMask | NSWindowStyleMaskFullScreen);
    [snapshotWindow setFrame:window.frame display:YES];

    NSScreen *screen = window.screen;

    // The final, full screen frame
    NSRect borderFinalFrame = [self fullScreenBorderFrameOnScreen:screen];

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

    centerWindowFrame.origin.x += NSMinX(screen.frame);
    centerWindowFrame.origin.y += NSMinY(screen.frame);

    self.borderView.autoresizingMask = kResizeFill;
    self.gameView.autoresizingMask = kResizeCenteredAtTop;

    // Hide contentview
    self.gameView.alphaValue = 0;

    GlkController * __weak weakSelf = self;
    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = duration / 4 + 0.1;
        [[snapshotWindow animator] setFrame:centerWindowFrame display:YES];
        [[window animator] setFrame:centerWindowFrame display:YES];
    }
     completionHandler:^{
        [weakSelf enterFullScreenEnlargeToFrame:borderFinalFrame duration:duration];
    }];
}

// Step 2: enlarge the window to its full size.
- (void)enterFullScreenEnlargeToFrame:(NSRect)borderFinalFrame duration:(NSTimeInterval)duration {
    NSWindow *window = self.window;
    GlkController * __weak weakSelf = self;
    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = duration / 4 - 0.1;
        [[window animator]
         setFrame:[window frameRectForContentRect:borderFinalFrame]
         display:YES];
    }
     completionHandler:^{
        [weakSelf enterFullScreenPositionGameViewWithDuration:duration];
    }];
}

// Step 3: move the invisible game view into position, centered at the top.
- (void)enterFullScreenPositionGameViewWithDuration:(NSTimeInterval)duration {
    NSView *gameView = self.gameView;
    NSRect newContentFrame = gameView.frame;
    newContentFrame.origin =
    NSMakePoint(floor((NSWidth(self.borderView.bounds) - NSWidth(gameView.frame)) / 2),
                floor(NSHeight(self.borderView.bounds) - self.theme.border - gameView.frame.size.height));

    GlkController * __weak weakSelf = self;
    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = duration / 4;
        gameView.frame = newContentFrame;
        [weakSelf sendArrangeEventWithFrame:gameView.frame force:NO];
        [weakSelf flushDisplay];
    }
     completionHandler:^{
        gameView.alphaValue = 1;
        [weakSelf enterFullScreenFadeOutSnapshotWithDuration:duration];
    }];
}

// Step 4: fade out the snapshot window, uncovering the game view.
- (void)enterFullScreenFadeOutSnapshotWithDuration:(NSTimeInterval)duration {
    NSWindow *snapshotWindow = snapshotController.window;
    GlkController * __weak weakSelf = self;
    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = duration / 4;
        [snapshotWindow.contentView animator].alphaValue = 0;
    }
     completionHandler:^{
        [weakSelf enterFullScreenExtendGameView];
    }];
}

// Step 5: extend the game view vertically if needed, then let the game
// fill the new area.
- (void)enterFullScreenExtendGameView {
    NSView *gameView = self.gameView;
    NSWindow *snapshotWindow = snapshotController.window;
    GlkController * __weak weakSelf = self;
    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = 0.2;
        [gameView animator].frame = [weakSelf contentFrameForFullscreen];
    }
     completionHandler:^{
        // Hide the snapshot window.
        ((NSView *)snapshotWindow.contentView).hidden = YES;
        ((NSView *)snapshotWindow.contentView).alphaValue = 1;

        GlkController *strongSelf = weakSelf;
        if (!strongSelf)
            return;
        // Send an arrangement event to fill
        // the new extended area
        [strongSelf sendArrangeEventWithFrame:gameView.frame force:NO];
        NSLog(@"enterFullScreenAnimationWithDuration: finished animation, calling restoreScrollOffsets in fullscreen");
        [strongSelf restoreScrollOffsets];
        for (GlkTextGridWindow *quotebox in strongSelf.quoteBoxes) {
            [quotebox performSelector:@selector(quoteboxAdjustSize:) withObject:nil afterDelay:0.1];
        }
    }];
}

// Like enterFullScreenAnimationWithDuration:, for a game autorestored in
// fullscreen: there is no snapshot, and the game view is laid out by the
// arrange event at the end. Two steps: move the window to the center of the
// screen, then enlarge it.
- (void)startGameInFullScreenAnimationWithDuration:(NSTimeInterval)duration {

    NSWindow *window = self.window;
    NSScreen *screen = window.screen;

    // The final, full screen frame
    NSRect borderFinalFrame = [self fullScreenBorderFrameOnScreen:screen];

    // The center frame for the window is used during
    // the 1st half of the fullscreen animation and is
    // the window at its original size but moved to the
    // center of its eventual full screen frame.

    NSRect centerWindowFrame = [self frameWithSanitycheckedSize:self.windowPreFullscreenFrame];
    centerWindowFrame.origin = NSMakePoint(floor((screen.frame.size.width -
                                                  self.borderView.frame.size.width) / 2),
                                           self.borderFullScreenSize.height - centerWindowFrame.size.height);

    centerWindowFrame.origin.x += NSMinX(screen.frame);
    centerWindowFrame.origin.y += NSMinY(screen.frame);

    // Hold the autorestore alert back until the animation is done.
    BOOL stashShouldShowAlert = self.shouldShowAutorestoreAlert;
    self.shouldShowAutorestoreAlert = NO;

    GlkController * __weak weakSelf = self;
    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = duration / 2;
        [[window animator] setFrame:centerWindowFrame display:YES];
    }
     completionHandler:^{
        [weakSelf startGameInFullScreenEnlargeToFrame:borderFinalFrame
                                             duration:duration / 2
                                            showAlert:stashShouldShowAlert];
    }];
}

// Second step: enlarge the window to its full size, then lay out the game
// for it and show the autorestore alert if one was held back.
- (void)startGameInFullScreenEnlargeToFrame:(NSRect)borderFinalFrame
                                   duration:(NSTimeInterval)duration
                                  showAlert:(BOOL)showAlert {
    NSWindow *window = self.window;
    GlkController * __weak weakSelf = self;
    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = duration;
        [[window animator]
         setFrame:[window frameRectForContentRect:borderFinalFrame]
         display:YES];
    }
     completionHandler:^{
        GlkController *strongSelf = weakSelf;
        if (!strongSelf)
            return;
        [strongSelf enableArrangementEvents];
        [strongSelf sendArrangeEventWithFrame:[strongSelf contentFrameForFullscreen] force:NO];

        strongSelf.shouldShowAutorestoreAlert = showAlert;
        [strongSelf performSelector:@selector(showAutorestoreAlert:) withObject:nil afterDelay:0.1];
        NSLog(@"startGameInFullScreenAnimationWithDuration: Finished. Restoring scroll offsets");
        [strongSelf restoreScrollOffsets];
    }];
}

- (void)enableArrangementEvents {
    inFullScreenResize = NO;
}

- (void)window:(NSWindow *)window startCustomAnimationToExitFullScreenWithDuration:(NSTimeInterval)duration {
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

    self.gameView.autoresizingMask = kResizeCenteredAtTop;

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
        localContentView.autoresizingMask = kResizeFill;
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
    self.gameView.autoresizingMask = kResizeCenteredFullHeight;
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
    self.gameView.autoresizingMask = kResizeFill;
}

- (void)startInFullscreen {
    NSLog(@"startInFullscreen");
    // First we show the game windowed
    [self.window setFrame:restoredControllerLate.windowPreFullscreenFrame
                  display:NO];
    [self showWindow:nil];
    [self.window makeKeyAndOrderFront:nil];

    self.gameView.frame = [self contentFrameForWindowed];

    self.gameView.autoresizingMask = kResizeCenteredAtTop;

    [self contentDidResize:self.gameView.frame];
    for (GlkTextBufferWindow *bufwin in self.bufferWindows) {
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
    if (self.isInSystemFullscreen || self.inFullscreen) {
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
