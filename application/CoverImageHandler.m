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
#import "NotificationBezel.h"

#import <QuartzCore/QuartzCore.h>

@interface KeyPressView : NSView

@property (weak) CoverImageHandler *delegate;

@end

@implementation KeyPressView

- (void)keyDown:(NSEvent *)theEvent {
    [_delegate forkInterpreterTask];
}

- (void)mouseUp:(NSEvent *)theEvent {
    [_delegate forkInterpreterTask];
}

@end


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
    newWin.contentView.wantsLayer = YES;

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
    newWin.backgroundColor = _glkctl.bgcolor;

    [newWin setFrame:[_glkctl.window convertRectToScreen:_glkctl.window.contentView.frame] display:YES];
    return newWin;
}

- (NSWindow *)fadeWindow {
    NSWindow *newWin = [[NSWindow alloc] initWithContentRect:_glkctl.window.contentView.frame
                                                   styleMask: NSBorderlessWindowMask
                                                     backing: NSBackingStoreBuffered
                                                       defer: YES];
    newWin.opaque = NO;
    newWin.backgroundColor = NSColor.clearColor;

    [newWin setFrame:_glkctl.window.frame display:YES];

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
        backgroundColorWin.alphaValue = 0;

        if (!(_glkctl.narcolepsy && _glkctl.theme.doStyles && _glkctl.theme.doGraphics))
            [_glkctl.window addChildWindow: backgroundColorWin
                                   ordered: NSWindowAbove];

        NSWindow *imageWindow = [self takeSnapshot];
        [_glkctl.window addChildWindow: imageWindow
                               ordered: NSWindowAbove];


        [imageWindow orderFront:nil];
        backgroundColorWin.alphaValue = 1;

        [_backgroundView removeFromSuperview];
        [_imageView removeFromSuperview];
        _backgroundView = nil;
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

        _glkctl.showingCoverImage = NO;
        // FIXME: Just fork the interpreter NSTask here instead
        [_glkctl adjustContentView];
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

- (void)imageDidChange {
    if (_imageView) {
        NSImage *image = [[NSImage alloc] initWithData:(NSData *)_glkctl.game.metadata.cover.data];
        if (image)
            [_imageView processImage:image];
        if (@available(macOS 10.15, *)) {
            [_imageView positionImage];
        } else {

        }
    }
}

- (void)showLogoWindow {
    // Don't show this if this view is not on the screen
    if (!_glkctl || !_glkctl.window) {
        NSLog(@"CoverImageWindowController: Error!");
        return;
    }

    switch (_glkctl.theme.coverArtStyle) {
        case kShowAndWait:
            [self showLogoAndWaitForKey];
            break;
        case kShowAndFade:
            if (@available(macOS 10.15, *)) {
                [self showLogoAndFade];
            } else {
                [self showLogoAndFadeLegacy];
            }
            break;
        case kShowAsBezel:
            [self showLogoAsBezel];
            break;
        default:
            return;
    }
}

- (void)showLogoAndWaitForKey {
    _glkctl.showingCoverImage = YES;
    NSWindow *window = _glkctl.window;

    [[NSNotificationCenter defaultCenter]
     addObserver:self
     selector:@selector(notePreferencesChanged:)
     name:@"PreferencesChanged"
     object:nil];

    // We need one view that covers the game window, to catch all mouse clicks
//    _backgroundView = [[CoverImageView alloc] initWithFrame:window.contentView.frame];
//
//    _backgroundView.delegate = self;
//    _backgroundView.game = _glkctl.game;
    _backgroundView = [[KeyPressView alloc] initWithFrame:window.contentView.bounds];
    _backgroundView.delegate = self;
    _backgroundView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    // And a (probably) smaller one for the image
//    _imageView = [[CoverImageView alloc] initWithFrame:window.contentView.frame];
    _imageView = [[CoverImageView alloc] initWithFrame:_backgroundView.bounds delegate:self];

    _imageView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    _waitingforkey = YES;

    [_backgroundView addSubview:_imageView];
//    _imageView.layerContentsRedrawPolicy = NSViewLayerContentsRedrawOnSetNeedsDisplay;

    [window.contentView addSubview:_backgroundView];

    if (@available(macOS 10.15, *)) {
        [_imageView positionImage];
    } else {
        //        [_imageView createImage];
    }
    NSLog(@"_imageView frame: %@", NSStringFromRect(_imageView.frame));
    NSLog(@"_imageView alphaValue: %f", _imageView.alphaValue);

    NSLog(@"_imageView.layer frame: %@", NSStringFromRect(_imageView.layer.frame));
    NSLog(@"_imageView.layer bounds: %@", NSStringFromRect(_imageView.layer.bounds));
    NSLog(@"_imageView layer opacity: %f", _imageView.layer.opacity);


    NSLog(@"_contentView frame: %@", NSStringFromRect(_contentView.frame));

//    window.viewsNeedDisplay = YES;
////    _backgroundView.needsDisplay = YES;

    [window makeFirstResponder:_imageView];

    CABasicAnimation *fadeInAnimation = [CABasicAnimation animationWithKeyPath:@"opacity"];
    fadeInAnimation.fromValue = [NSNumber numberWithFloat:0.0];
    fadeInAnimation.toValue = [NSNumber numberWithFloat:1.0];
    fadeInAnimation.additive = NO;
    fadeInAnimation.removedOnCompletion = NO;
    fadeInAnimation.beginTime = 0.0;
    fadeInAnimation.duration = 0.2;
    fadeInAnimation.fillMode = kCAFillModeForwards;

    [_imageView.layer addAnimation:fadeInAnimation forKey:nil];
}

- (void)showLogoAndFade {
    NSWindow *fadeWindow = [self fadeWindow];

    _imageView = [[CoverImageView alloc] initWithFrame:fadeWindow.contentView.bounds delegate:self];

    [fadeWindow.contentView addSubview:_imageView];
    [_imageView positionImage];

    _imageView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    NSRect newFrame = fadeWindow.frame;
    CGFloat factor = 0.8;
    CGFloat newWidth = newFrame.size.width * factor;
    CGFloat newHeight = newFrame.size.height * factor;
    CGFloat offsetX = (newFrame.size.width - newWidth) / 2;
    CGFloat offsetY = (newFrame.size.height - newHeight) / 2;
    newFrame = NSMakeRect(newFrame.origin.x + offsetX, newFrame.origin.y + offsetY, newWidth, newHeight);

    [fadeWindow setFrame:newFrame display:YES];

    CALayer *layer = _imageView.layer;
    layer.opacity = 0;
    [_glkctl.window addChildWindow:fadeWindow ordered:NSWindowAbove];
    [_glkctl forkInterpreterTask];

    CABasicAnimation *fadeInAnimation = [CABasicAnimation animationWithKeyPath:@"opacity"];
    fadeInAnimation.fromValue = [NSNumber numberWithFloat:0.0];
    fadeInAnimation.toValue = [NSNumber numberWithFloat:1.0];
    fadeInAnimation.additive = NO;
    fadeInAnimation.removedOnCompletion = NO;
    fadeInAnimation.beginTime = 0.2;
    fadeInAnimation.duration = 0.05;
    fadeInAnimation.fillMode = kCAFillModeForwards;

    CABasicAnimation *fadeOutAnimation = [CABasicAnimation animationWithKeyPath:@"opacity"];
    fadeOutAnimation.fromValue = [NSNumber numberWithFloat:1.0];
    fadeOutAnimation.toValue = [NSNumber numberWithFloat:0.0];
    fadeOutAnimation.additive = NO;
    fadeOutAnimation.removedOnCompletion = NO;
    fadeOutAnimation.beginTime = 1.5;
    fadeOutAnimation.duration = 0.5;
    fadeOutAnimation.fillMode = kCAFillModeForwards;


    CFTimeInterval addTime = [layer convertTime:CACurrentMediaTime() fromLayer:nil];
    CAAnimationGroup *group = [CAAnimationGroup animation];
    group.animations = @[fadeInAnimation, fadeOutAnimation];
    group.beginTime = addTime;
    group.duration = 3;
    group.removedOnCompletion = NO;

    [layer addAnimation:group forKey:nil];

    GlkController *blockController = _glkctl;

    double delayInSeconds = 4;
    dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
    dispatch_after(popTime, dispatch_get_main_queue(), ^(void){
        [fadeWindow orderOut:nil];
        // Why do these need to be reset here?
        blockController.contentView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        blockController.borderView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    });
}

- (void)showLogoAndFadeLegacy {
    NSWindow *fadeWindow = [self fadeWindow];

    _imageView = [[CoverImageView alloc] initWithFrame:fadeWindow.contentView.bounds delegate:self];
    [fadeWindow.contentView addSubview:_imageView];

    _imageView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _imageView.frame = fadeWindow.contentView.bounds;

    NSRect newFrame = fadeWindow.frame;

    CGFloat factor = 0.8;
    CGFloat newWidth = newFrame.size.width * factor;
    CGFloat newHeight = newFrame.size.height * factor;
    CGFloat offsetX = (newFrame.size.width - newWidth) / 2;
    CGFloat offsetY = (newFrame.size.height - newHeight) / 2;
    newFrame = NSMakeRect(newFrame.origin.x + offsetX, newFrame.origin.y + offsetY, newWidth, newHeight);
    [fadeWindow setFrame:newFrame display:NO];

    [_glkctl.window addChildWindow:fadeWindow ordered:NSWindowAbove];
    [_glkctl forkInterpreterTask];

    _imageView.needsDisplay = YES;

    CoverImageHandler __unsafe_unretained *weakSelf = self;

    double delayInSeconds = 1.5;
    dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
    dispatch_after(popTime, dispatch_get_main_queue(), ^(void){
        [NSAnimationContext
         runAnimationGroup:^(NSAnimationContext *context) {
            // We enlarge the window to fill the screen
            context.duration = 0.5;
            context.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionLinear];
            weakSelf.imageView.animator.alphaValue = 0;
        } completionHandler:^{
            [fadeWindow orderOut:nil];
        }];
    });
}

- (void)showLogoAsBezel {
    NotificationBezel *notification = [[NotificationBezel alloc] initWithScreen:_glkctl.window.screen];

    [notification showCoverImage:(NSData *)_glkctl.game.metadata.cover.data interpolation:_glkctl.game.metadata.cover.interpolation];
    [_glkctl forkInterpreterTask];
}

- (void)notePreferencesChanged:(NSNotification *)notify {
    if (_glkctl.theme.borderBehavior == kUserOverride)
        [_glkctl setBorderColor:_glkctl.theme.borderColor];
    else
        [_glkctl setBorderColor:_glkctl.theme.bufferBackground];
}

- (void)enterFullScreenWithDuration:(NSTimeInterval)duration {
    if (@available(macOS 10.15, *)) {
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

        _backgroundView.hidden = YES;

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
            weakSelf.backgroundView.frame = borderFinalFrame;
            weakSelf.backgroundView.hidden = NO;
            [imageWindow orderOut:nil];
        }];
    } else {
        [self enterFullScreenLegacyWithDuration:duration];
    }
}

- (void)enterFullScreenLegacyWithDuration:(NSTimeInterval)duration {
    NSWindow *window = _glkctl.window;
    NSScreen *screen = window.screen;
    NSInteger border = _glkctl.theme.border;
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
    } completionHandler:^{
        borderview.frame = borderFinalFrame;
        gameContentView.frame = contentRect;
        weakSelf.imageView.frame = borderFinalFrame;
        weakSelf.backgroundView.frame = borderFinalFrame;
    }];
}



- (void)exitFullscreenWithDuration:(NSTimeInterval)duration {
    if (@available(macOS 10.15, *)) {

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
        if (NSMaxY(imageFrame) > NSMaxY(_backgroundView.frame))
            imageFrame.origin.y = NSMaxY(_backgroundView.frame) - imageFrame.size.height;

        _imageView.frame = imageFrame;
        imageFrame.origin.x += oldFrame.origin.x;
        imageFrame.origin.y += oldFrame.origin.y;

        gameContentView.hidden = YES;
        _backgroundView.hidden = YES;
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
            weakSelf.backgroundView.frame = borderView.frame;
            [weakSelf.imageView positionImage];
            weakSelf.backgroundView.hidden = NO;
            gameContentView.hidden = NO;

            weakSelf.backgroundView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
            gameContentView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

            [NSAnimationContext
             runAnimationGroup:^(NSAnimationContext *context) {
                context.duration = 0.4;
                imageWindow.animator.alphaValue = 0;
            } completionHandler:^{
                [imageWindow orderOut:nil];
                imageWindow.alphaValue = 1;
            }];

        }];
    } else {
        [self exitFullscreenSimple:duration];
    }
}

- (void)exitFullscreenSimple:(NSTimeInterval)duration {
    if (self.exitFullscreenWindow)
        [self.exitFullscreenWindow orderOut:nil];
    NSWindow *window = _glkctl.window;
    NSRect oldFrame = _glkctl.windowPreFullscreenFrame;
    NSView *gameContentView = _glkctl.contentView;

    if (@available(macOS 10.15, *)) {
    } else {
        _backgroundView.frame = window.contentView.frame;
        _imageView.frame = _backgroundView.frame;
        _backgroundView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        _imageView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    }

    CoverImageView *imageView = _imageView;
    [window makeFirstResponder:_backgroundView];

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
        if (@available(macOS 10.15, *)) {
            [imageView positionImage];
        }
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
    if (@available(macOS 10.15, *)) {
        [self adjustFullScreenWindow:self.enterFullscreenWindow];
        return @[window, self.enterFullscreenWindow];
    } else {
        return @[window];
    }
}

- (NSArray *)customWindowsToExitFullScreenForWindow:(NSWindow *)window {
    if (@available(macOS 10.15, *)) {
        [self adjustFullScreenWindow:self.exitFullscreenWindow];
        return @[window, self.exitFullscreenWindow];
    } else {
        return @[window];
    }

}

@end
