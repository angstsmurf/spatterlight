//
//  CoverImageWindowController.m
//  Spatterlight
//
//  Created by Administrator on 2021-05-29.
//

#import "CoverImageHandler.h"
#import "CoverImageView.h"
#import "GlkController.h"
#import "Theme.h"
#import "Game.h"
#import "Metadata.h"
#import "Image.h"

#import <QuartzCore/QuartzCore.h>

@interface CoverImageHandler ()

@end

@implementation CoverImageHandler

- (instancetype)initWithController:(GlkController *)controller {
    // Create the window
    self = [super init];
    if (self) {
        _glkctl = controller;
    }
    return self;
}

- (NSWindow *)takeSnapshot {
    CGImageRef windowSnapshot =
    CGWindowListCreateImage(CGRectNull,
                            kCGWindowListOptionIncludingWindow,
                            (CGWindowID)[_glkctl.window windowNumber],
                            kCGWindowImageBoundsIgnoreFraming);

    CALayer *snapshotLayer = [[CALayer alloc] init];
    NSWindow *newWin = [[NSWindow alloc] initWithContentRect:_glkctl.window.contentView.frame
                                             styleMask: NSBorderlessWindowMask
                                               backing: NSBackingStoreBuffered
                                                 defer: YES];
    newWin.opaque = NO;
    newWin.backgroundColor = NSColor.clearColor;

    snapshotLayer.frame = newWin.frame;
    snapshotLayer.contents = CFBridgingRelease(windowSnapshot);

    [newWin setFrame:_glkctl.window.frame display:YES];
    newWin.contentView.layer = snapshotLayer;
    return newWin;
}

- (NSWindow *)backgroundColorWindow {
    NSWindow *newWin = [[NSWindow alloc] initWithContentRect:_glkctl.window.contentView.frame
                                                   styleMask: NSBorderlessWindowMask
                                                     backing: NSBackingStoreBuffered
                                                       defer: YES];
    newWin.opaque = YES;
    newWin.backgroundColor = _glkctl.theme.bufferBackground;

    [newWin setFrame:[_glkctl.window convertRectToScreen:_glkctl.window.contentView.frame] display:YES];
    return newWin;
}

- (void)adjustFullScreenWindow:(NSWindow *)newWin {
    NSRect frame = [_glkctl.window.contentView convertRect:_imageView.frame toView:nil];
    NSRect windowFrame = [_glkctl.window convertRectToScreen:frame];
    [newWin setFrame:windowFrame display:YES];
    newWin.opaque = NO;
    newWin.backgroundColor = NSColor.clearColor;

    CALayer *imagelayer = [CALayer layer];
    imagelayer.magnificationFilter = _imageView.sizeInPixels.height < 350 ? kCAFilterNearest : kCAFilterTrilinear;
    imagelayer.contents = _imageView.image;
    newWin.contentView.layer = imagelayer;
    imagelayer.layoutManager = [CAConstraintLayoutManager layoutManager];
    imagelayer.autoresizingMask = kCALayerHeightSizable | kCALayerWidthSizable;
    imagelayer.frame = _imageView.bounds;
    [newWin.windowController showWindow:nil];
    [newWin orderFront:nil];
}

- (void)forkInterpreterTask {
    if (_waitingforkey) {
        _waitingforkey = NO;

        if (_enterFullscreenWindow) {
            [_enterFullscreenWindow orderOut:nil];
        }

        if (_exitFullscreenWindow) {
            [_exitFullscreenWindow orderOut:nil];
        }

        NSWindow *backgroundColorWin  = [self backgroundColorWindow];

        [_glkctl.window addChildWindow: backgroundColorWin
                            ordered: NSWindowAbove];

        NSWindow *imageWindow = [self takeSnapshot];
        [_glkctl.window addChildWindow: imageWindow
                               ordered: NSWindowAbove];


        [imageWindow orderFront:nil];

        [_contentView removeFromSuperview];
        [_imageView removeFromSuperview];
        _contentView = nil;
        _imageView = nil;

        NSArray<NSView *> *subviews = _glkctl.borderView.subviews.copy;
        for (NSView *view in subviews) {
            [view removeFromSuperview];
        }

        [_glkctl.borderView addSubview:_glkctl.contentView];
        [_glkctl adjustContentView];

        CABasicAnimation *fadeOutAnimation = [CABasicAnimation animationWithKeyPath:@"opacity"];
        fadeOutAnimation.fromValue = [NSNumber numberWithFloat:1.0];
        fadeOutAnimation.toValue = [NSNumber numberWithFloat:0.0];
        fadeOutAnimation.additive = NO;
        fadeOutAnimation.removedOnCompletion = NO;
        fadeOutAnimation.beginTime = 0.0;
        fadeOutAnimation.duration = 0.5;
        fadeOutAnimation.fillMode = kCAFillModeForwards;

        [_glkctl performSelector:@selector(deferredRestart:) withObject:nil afterDelay:0.0];

        double delayInSeconds = 0.3;
        dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));

        dispatch_after(popTime, dispatch_get_main_queue(), ^(void){
            [NSAnimationContext
             runAnimationGroup:^(NSAnimationContext *context) {
                context.duration = 0.3;
                [[backgroundColorWin animator] setAlphaValue:0];
            } completionHandler:^{
                [backgroundColorWin orderOut:nil];
            }];
        });

        [NSAnimationContext
         runAnimationGroup:^(NSAnimationContext *context) {
            context.duration = 0.5;
            context.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
            [imageWindow.contentView.layer addAnimation:fadeOutAnimation forKey:nil];
        } completionHandler:^{
            [imageWindow orderOut:nil];
        }];
    }
}

- (void)positionImage {
    NSSize imageSize = _imageView.sizeInPixels;
    NSSize windowSize = _contentView.frame.size;

    CGFloat ratio = imageSize.width / imageSize.height;
    NSRect imageFrame = NSMakeRect(0,0, windowSize.width, windowSize.width / ratio);
    if (imageFrame.size.height > windowSize.height) {
        imageFrame = NSMakeRect(0,0, windowSize.height * ratio, windowSize.height);
        imageFrame.origin.x = (windowSize.width - imageFrame.size.width) / 2;
    } else {
        imageFrame.origin.y = (windowSize.height - imageFrame.size.height) / 2;
        if (NSMaxY(imageFrame) > NSMaxY(_contentView.frame))
            imageFrame.origin.y = NSMaxY(_contentView.frame) - imageFrame.size.height;
    }

    _imageView.frame = imageFrame;

}

- (void)showLogoWindow {
    if (!_glkctl) {
        NSLog(@"CoverImageWindowController: Error!");
        return;
    }

    NSWindow *window = _glkctl.window;

    // Don't show this if this view is not on the screen
    if (window == nil) return;

    // We need one view that covers the game window, to catch all mouse clicks
    _contentView = [[CoverImageView alloc] initWithFrame:window.contentView.frame];
    _contentView.delegate = self;
    _contentView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    // And a (probably) smaller one for the image
    _imageView = [[CoverImageView alloc] initWithFrame:window.contentView.frame];
    _imageView.delegate = self;
    [_imageView createImage];
    [self positionImage];
    _imageView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    _waitingforkey = YES;

    [_contentView addSubview:_imageView];
    [window.contentView addSubview:_contentView];

    [window makeFirstResponder:_imageView];

    CGFloat delay = 0.5;
    if ((_glkctl.window.styleMask & NSFullScreenWindowMask) == NSFullScreenWindowMask ||
        NSEqualRects(_glkctl.borderView.frame, window.screen.frame))
        delay = 1;

    CABasicAnimation *fadeInAnimation = [CABasicAnimation animationWithKeyPath:@"opacity"];
    fadeInAnimation.fromValue = [NSNumber numberWithFloat:0.0];
    fadeInAnimation.toValue = [NSNumber numberWithFloat:1.0];
    fadeInAnimation.additive = NO;
    fadeInAnimation.removedOnCompletion = NO;
    fadeInAnimation.beginTime = 0.0;
    fadeInAnimation.duration = 0.6;
    fadeInAnimation.fillMode = kCAFillModeForwards;

    [_imageView.layer addAnimation:fadeInAnimation forKey:nil];
}

- (void)enterFullScreenWithDuration:(NSTimeInterval)duration {
    NSWindow *window = _glkctl.window;
    NSScreen *screen = window.screen;
    NSInteger border = _glkctl.theme.border;

    NSWindow *imageWindow = self.enterFullscreenWindow;

    // The final, full screen frame
    NSRect borderFinalFrame = screen.frame;

    if (!NSEqualSizes(_glkctl.borderFullScreenSize, NSZeroSize))
        borderFinalFrame.size = _glkctl.borderFullScreenSize;

    CGFloat fullScreenWidth = borderFinalFrame.size.width;
    CGFloat fullScreenHeight = borderFinalFrame.size.height;

    NSView *borderview = _glkctl.borderView;
    NSView *gameContentView = _glkctl.contentView;
    borderview.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    gameContentView.autoresizingMask = NSViewMinXMargin | NSViewMaxXMargin |
    NSViewMinYMargin; // Attached at top but not bottom or sides

    _contentView.hidden = YES;

    NSSize imageSize = _imageView.sizeInPixels;

    CGFloat ratio = imageSize.width / imageSize.height;
    NSRect imageFrame = NSMakeRect(0,0, fullScreenHeight * ratio, fullScreenHeight);
        imageFrame.origin.x = (fullScreenWidth - imageFrame.size.width) / 2;

    if (imageFrame.size.width > fullScreenWidth) {
        imageFrame = NSMakeRect(0,0, floor(fullScreenWidth), floor(fullScreenWidth / ratio));
        imageFrame.origin.y = floor((fullScreenHeight - imageFrame.size.height) / 2);
    }

    NSRect contentRect = _glkctl.windowPreFullscreenFrame;
    contentRect.origin.x = floor((fullScreenWidth - _glkctl.windowPreFullscreenFrame.size.width) / 2);
    contentRect.origin.y = border;
    contentRect.size.height = fullScreenHeight - 2 * border;

    gameContentView.frame = contentRect;

    [window makeFirstResponder:_imageView];

    CoverImageHandler __unsafe_unretained *weakSelf = self;

    // Our animation will be broken into two steps.
    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        // We enlarge the window to fill the screen
        context.duration = duration;
        context.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
        [[window animator]
         setFrame:[window frameRectForContentRect:borderFinalFrame]
         display:YES];
        [[imageWindow animator] setFrame:imageFrame display:YES];
    } completionHandler:^{
        borderview.frame = borderFinalFrame;
        gameContentView.frame = contentRect;
        weakSelf.imageView.frame = imageFrame;
        weakSelf.contentView.frame = borderFinalFrame;
        weakSelf.contentView.hidden = NO;
        [imageWindow orderOut:nil];
    }];
}

- (void)exitFullscreenWithDuration:(NSTimeInterval)duration {
    [self.enterFullscreenWindow orderOut:nil];

    NSSize imageSize = _imageView.sizeInPixels;

    if (imageSize.width < imageSize.height) {
        [self exitFullscreenSimple:duration];
        return;
    }

    NSWindow *window = _glkctl.window;
    NSView *borderView = _glkctl.borderView;
    NSView *gameContentView = _glkctl.contentView;

    NSRect oldFrame = _glkctl.windowPreFullscreenFrame;

    NSRect imageFrame = NSZeroRect;

    NSWindow *imageWindow = self.exitFullscreenWindow;
    NSSize windowSize = oldFrame.size;

    CGFloat ratio = imageSize.width / imageSize.height;
    imageFrame = NSMakeRect(0,0, windowSize.width, windowSize.width / ratio);

    if (imageFrame.size.height > windowSize.height) {
        [self exitFullscreenSimple:duration];
        return;
    }

    CGFloat heightDiff = 14;

    imageFrame.origin.y = (windowSize.height - imageFrame.size.height) / 2 - heightDiff;
    if (NSMaxY(imageFrame) > NSMaxY(_contentView.frame))
        imageFrame.origin.y = NSMaxY(_contentView.frame) - imageFrame.size.height;

    _imageView.frame = imageFrame;
    imageFrame.origin.x += oldFrame.origin.x;
    imageFrame.origin.y += oldFrame.origin.y;

    gameContentView.hidden = YES;
    _contentView.hidden = YES;
    [window makeFirstResponder:_imageView];

    CoverImageHandler * __unsafe_unretained weakSelf = self;

    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = duration;
        context.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
        // Make sure the window style mask does not
        // include full screen bit
        [window
         setStyleMask:(NSUInteger)([window styleMask] & ~(NSUInteger)NSFullScreenWindowMask)];
        [[window animator] setFrame:oldFrame display:YES];
        [[imageWindow animator] setFrame:imageFrame display:YES];
    } completionHandler:^{
        borderView.frame = window.contentView.frame;
        weakSelf.contentView.frame = borderView.frame;
        [weakSelf positionImage];
        weakSelf.contentView.hidden = NO;
        gameContentView.hidden = NO;
        double delayInSeconds = 0.4;
        dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
        dispatch_after(popTime, dispatch_get_main_queue(), ^(void){
            weakSelf.contentView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
            gameContentView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
            [imageWindow orderOut:nil];
        });
    }];
}

- (void)exitFullscreenSimple:(NSTimeInterval)duration {
    [self.exitFullscreenWindow orderOut:nil];
    NSWindow *window = _glkctl.window;
    NSRect oldFrame = _glkctl.windowPreFullscreenFrame;
    NSView *gameContentView = _glkctl.contentView;

    CoverImageHandler * __unsafe_unretained weakSelf = self;
    [window makeFirstResponder:_contentView];

    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = duration;
        context.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
        // Make sure the window style mask does not
        // include full screen bit
        [window
         setStyleMask:(NSUInteger)([window styleMask] & ~(NSUInteger)NSFullScreenWindowMask)];
        [[window animator] setFrame:oldFrame display:YES];
    } completionHandler:^{
        [weakSelf positionImage];
        gameContentView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    }];
}


@synthesize enterFullscreenWindow = _enterFullscreenWindow;

- (NSWindow *)enterFullscreenWindow {
    if (!_enterFullscreenWindow) {
        _enterFullscreenWindow = [[NSWindow alloc] initWithContentRect:NSZeroRect
                                                             styleMask: NSBorderlessWindowMask
                                                               backing: NSBackingStoreBuffered
                                                                 defer: YES];
    }
    return _enterFullscreenWindow;
}

@synthesize exitFullscreenWindow = _exitFullscreenWindow;

- (NSWindow *)exitFullscreenWindow {
    if (!_exitFullscreenWindow) {
        _exitFullscreenWindow = [[NSWindow alloc] initWithContentRect:NSZeroRect
                                                             styleMask: NSBorderlessWindowMask
                                                               backing: NSBackingStoreBuffered
                                                                 defer: YES];
    }
    return _exitFullscreenWindow;
}

- (NSArray *)customWindowsToEnterFullScreenForWindow:(NSWindow *)window {
   [self adjustFullScreenWindow:self.enterFullscreenWindow];
    return @[window, self.enterFullscreenWindow];
}

- (NSArray *)customWindowsToExitFullScreenForWindow:(NSWindow *)window {
   [self adjustFullScreenWindow:self.exitFullscreenWindow];
    return @[window, self.exitFullscreenWindow];
}

@end
