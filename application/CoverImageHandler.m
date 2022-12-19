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
                            (CGWindowID)(_glkctl.window).windowNumber,
                            kCGWindowImageBoundsIgnoreFraming);

    CALayer *snapshotLayer = [[CALayer alloc] init];
    NSWindow *newWin = [[NSWindow alloc] initWithContentRect:_glkctl.window.contentView.frame
                                                   styleMask: NSWindowStyleMaskBorderless
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
                                                   styleMask: NSWindowStyleMaskBorderless
                                                     backing: NSBackingStoreBuffered
                                                       defer: YES];
    newWin.opaque = YES;
    newWin.backgroundColor = _glkctl.bgcolor;

    [newWin setFrame:[_glkctl.window convertRectToScreen:_glkctl.window.contentView.frame] display:YES];
    return newWin;
}

- (NSWindow *)fadeWindow {
    NSWindow *newWin = [[NSWindow alloc] initWithContentRect:_glkctl.window.contentView.frame
                                                   styleMask: NSWindowStyleMaskBorderless
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
    imagelayer.magnificationFilter = (_glkctl.game.metadata.cover.interpolation == kNearestNeighbor) ? kCAFilterNearest : kCAFilterTrilinear;

    newWin.contentView.wantsLayer = YES;

    [CATransaction begin];
    [CATransaction setValue:(id)kCFBooleanTrue
                     forKey:kCATransactionDisableActions];
    imagelayer.contents = _imageView.image;
    [newWin.contentView.layer addSublayer:imagelayer];
    imagelayer.layoutManager = [CAConstraintLayoutManager layoutManager];
    imagelayer.autoresizingMask = kCALayerHeightSizable | kCALayerWidthSizable;
    imagelayer.frame = _imageView.bounds;
    [CATransaction commit];

    [newWin.windowController showWindow:nil];
    [newWin orderFront:nil];
}

- (void)forkInterpreterTask {
    GlkController *glkctl = _glkctl;
    if (_waitingforkey) {
        _waitingforkey = NO;

        if (_enterFullscreenWindow) {
            [_enterFullscreenWindow orderOut:nil];
        }

        NSWindow *backgroundColorWin  = [self backgroundColorWindow];
        backgroundColorWin.alphaValue = 0;

        if (!(glkctl.narcolepsy && glkctl.theme.doStyles && glkctl.theme.doGraphics))
            [glkctl.window addChildWindow: backgroundColorWin
                                   ordered: NSWindowAbove];

        NSWindow *imageWindow = [self takeSnapshot];
        [glkctl.window addChildWindow: imageWindow
                               ordered: NSWindowAbove];

        [imageWindow orderFront:nil];
        backgroundColorWin.alphaValue = 1;

        [_backgroundView removeFromSuperview];
        [_imageView removeFromSuperview];
        _backgroundView = nil;
        _imageView = nil;

        NSArray<NSView *> *subviews = glkctl.borderView.subviews.copy;
        for (NSView *view in subviews) {
            [view removeFromSuperview];
        }

        if (_glkctl.inFullscreen) {
            NSRect preFullscreen = glkctl.windowPreFullscreenFrame;
            CGFloat border = glkctl.theme.border;
            preFullscreen.size.width -= 2 * border;

            glkctl.contentView.frame = NSMakeRect(NSMidX(glkctl.borderView.frame) - NSMidX(preFullscreen),
                border,
                NSWidth(preFullscreen),
                NSHeight(glkctl.borderView.frame) - 2 * border);
        }
        [glkctl.borderView addSubview:glkctl.contentView];
        [glkctl adjustContentView];

        CABasicAnimation *fadeOutAnimation = [CABasicAnimation animationWithKeyPath:@"opacity"];
        fadeOutAnimation.fromValue = @1.0f;
        fadeOutAnimation.toValue = @0.0f;
        fadeOutAnimation.additive = NO;
        fadeOutAnimation.removedOnCompletion = NO;
        fadeOutAnimation.beginTime = 0.0;
        fadeOutAnimation.duration = 0.5;
        fadeOutAnimation.fillMode = kCAFillModeForwards;

        glkctl.showingCoverImage = NO;
        // FIXME: Just fork the interpreter NSTask here instead
        glkctl.borderView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        glkctl.contentView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

        [glkctl performSelector:@selector(deferredRestart:) withObject:nil afterDelay:0.0];

        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.3 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void){
            [NSAnimationContext
             runAnimationGroup:^(NSAnimationContext *context) {
                context.duration = 0.3;
                [backgroundColorWin animator].alphaValue = 0;
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
            [[NSNotificationCenter defaultCenter]
             removeObserver:self];
        }];
    }
}

- (void)noteManagedObjectContextDidChange:(NSNotification *)notification {
    NSSet *updatedObjects = (notification.userInfo)[NSUpdatedObjectsKey];
    NSSet *refreshedObjects = (notification.userInfo)[NSRefreshedObjectsKey];

    if (!updatedObjects)
        updatedObjects = [NSSet new];
    updatedObjects = [updatedObjects setByAddingObjectsFromSet:refreshedObjects];

    Metadata *metadata = _glkctl.game.metadata;
    if ([updatedObjects containsObject:metadata] ||
        [updatedObjects containsObject:metadata.cover])
    {
        if (metadata.cover == nil) {
            [self forkInterpreterTask];
            return;
        }

        NSImage *image = [[NSImage alloc] initWithData:(NSData *)metadata.cover.data];
        if (image) {
            CoverImageHandler __weak *weakSelf = self;
            dispatch_async(dispatch_get_main_queue(), ^{

            [weakSelf.imageView processImage:image];
            [weakSelf.imageView positionImage];
            });
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
            [self showLogoAndFade];
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

    [[NSNotificationCenter defaultCenter]
     addObserver:self
     selector:@selector(noteManagedObjectContextDidChange:)
     name:NSManagedObjectContextObjectsDidChangeNotification
     object:_glkctl.game.managedObjectContext];

    // We need one view that covers the game window, to catch all mouse clicks
    _backgroundView = [[KeyPressView alloc] initWithFrame:window.contentView.bounds];
    _backgroundView.delegate = self;
    _backgroundView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    // And a (probably) smaller one for the image
    _imageView = [[CoverImageView alloc] initWithFrame:_backgroundView.bounds delegate:self];

    _imageView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    _waitingforkey = YES;

    [_backgroundView addSubview:_imageView];

    [window.contentView addSubview:_backgroundView];

    [_imageView positionImage];

    [window makeFirstResponder:_imageView];

    CABasicAnimation *fadeInAnimation = [CABasicAnimation animationWithKeyPath:@"opacity"];
    fadeInAnimation.fromValue = @0.0f;
    fadeInAnimation.toValue = @1.0f;
    fadeInAnimation.additive = NO;
    fadeInAnimation.removedOnCompletion = NO;
    fadeInAnimation.beginTime = 0.0;
    fadeInAnimation.duration = 0.2;
    fadeInAnimation.fillMode = kCAFillModeForwards;

    [_imageView.layer addAnimation:fadeInAnimation forKey:nil];
}

- (void)showLogoAndFade {
    [_glkctl forkInterpreterTask];
    CoverImageView __block *imageView;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.2 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void){

        NSWindow *fadeWindow = [self fadeWindow];

        imageView = [[CoverImageView alloc] initWithFrame:fadeWindow.contentView.bounds delegate:self];

        [fadeWindow.contentView addSubview:imageView];
        [imageView positionImage];

        imageView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

        NSRect newFrame = fadeWindow.frame;
        CGFloat factor = 0.8;
        CGFloat newWidth = newFrame.size.width * factor;
        CGFloat newHeight = newFrame.size.height * factor;
        CGFloat offsetX = (newFrame.size.width - newWidth) / 2;
        CGFloat offsetY = (newFrame.size.height - newHeight) / 2;
        newFrame = NSMakeRect(newFrame.origin.x + offsetX, newFrame.origin.y + offsetY, newWidth, newHeight);

        [fadeWindow setFrame:newFrame display:YES];

        CALayer *layer = imageView.layer;
        layer.opacity = 1;
        [self.glkctl.window addChildWindow:fadeWindow ordered:NSWindowAbove];

        CABasicAnimation *fadeOutAnimation = [CABasicAnimation animationWithKeyPath:@"opacity"];
        fadeOutAnimation.fromValue = @1.0f;
        fadeOutAnimation.toValue = @0.0f;
        fadeOutAnimation.additive = NO;
        fadeOutAnimation.removedOnCompletion = NO;
        fadeOutAnimation.beginTime = 1.5;
        fadeOutAnimation.duration = 0.5;
        fadeOutAnimation.fillMode = kCAFillModeForwards;


        CFTimeInterval addTime = [layer convertTime:CACurrentMediaTime() fromLayer:nil];
        CAAnimationGroup *group = [CAAnimationGroup animation];
        group.animations = @[fadeOutAnimation];
        group.beginTime = addTime;
        group.duration = 3;
        group.removedOnCompletion = NO;
        group.fillMode = kCAFillModeForwards;

        [layer addAnimation:group forKey:nil];

        GlkController *blockController = self.glkctl;

        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(4 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void){
            [fadeWindow orderOut:nil];
            // Why do these need to be reset here?
            blockController.contentView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
            blockController.borderView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        });
    });
}

- (void)showLogoAsBezel {
    NotificationBezel *notification = [[NotificationBezel alloc] initWithScreen:_glkctl.window.screen];

    [notification showCoverImage:(NSData *)_glkctl.game.metadata.cover.data interpolation:_glkctl.game.metadata.cover.interpolation];
    [_glkctl forkInterpreterTask];
}

- (void)notePreferencesChanged:(NSNotification *)notify {
    GlkController *glkctl = _glkctl;
    if (glkctl.theme.coverArtStyle != kShowAndWait) {
        if (_waitingforkey)
            [self forkInterpreterTask];
        return;
    }
    if (glkctl.theme.borderBehavior == kUserOverride)
        [glkctl setBorderColor:_glkctl.theme.borderColor];
    else
        [glkctl setBorderColor:_glkctl.theme.bufferBackground];
}

- (void)enterFullScreenWithDuration:(NSTimeInterval)duration {
    _imageView.inFullscreenResize = YES;
    GlkController *glkctl = _glkctl;
    NSWindow *window = glkctl.window;
    glkctl.windowPreFullscreenFrame = window.frame;
    NSScreen *screen = window.screen;
    NSInteger border = glkctl.theme.border;

    NSWindow *imageWindow = self.enterFullscreenWindow;

    // The final, full screen frame
    NSRect borderFinalFrame = screen.frame;

    if (!NSEqualSizes(glkctl.borderFullScreenSize, NSZeroSize))
        borderFinalFrame.size = glkctl.borderFullScreenSize;

    CGFloat fullScreenWidth = borderFinalFrame.size.width;
    CGFloat fullScreenHeight = borderFinalFrame.size.height;

    NSView *borderview = glkctl.borderView;
    NSView *gameContentView = glkctl.contentView;
    borderview.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    gameContentView.autoresizingMask = NSViewMinXMargin | NSViewMaxXMargin |
    NSViewMinYMargin; // Attached at top but not bottom or sides

    _backgroundView.hidden = YES;

    NSImageRep *rep = _imageView.image.representations.lastObject;
    NSSize imageSize = NSMakeSize(rep.pixelsWide, rep.pixelsHigh);

    CGFloat ratio = imageSize.width / imageSize.height;
    NSRect imageFrame = NSMakeRect(0,0, fullScreenHeight * ratio, fullScreenHeight);
    imageFrame.origin.x = (fullScreenWidth - imageFrame.size.width) / 2;

    if (imageFrame.size.width > fullScreenWidth) {
        imageFrame = NSMakeRect(0,0, floor(fullScreenWidth), floor(fullScreenWidth / ratio));
        imageFrame.origin.y = floor((fullScreenHeight - imageFrame.size.height) / 2);
    }

    NSRect contentRect = glkctl.windowPreFullscreenFrame;
    contentRect.origin.x = floor((fullScreenWidth - glkctl.windowPreFullscreenFrame.size.width) / 2);
    contentRect.origin.y = border;
    contentRect.size.height = fullScreenHeight - 2 * border;

    gameContentView.frame = contentRect;

    [window makeFirstResponder:_imageView];

    CoverImageHandler __weak *weakSelf = self;

    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        // We enlarge the window to fill the screen
        context.duration = duration - 0.1;
        context.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
        [[imageWindow animator] setFrame:imageFrame display:YES];
        [[window animator]
         setFrame:[window frameRectForContentRect:borderFinalFrame]
         display:YES];
    } completionHandler:^{
        weakSelf.imageView.inFullscreenResize = NO;
        borderview.frame = borderFinalFrame;
        gameContentView.frame = contentRect;
        weakSelf.imageView.frame = imageFrame;
        weakSelf.backgroundView.frame = borderFinalFrame;
        weakSelf.backgroundView.hidden = NO;
        [imageWindow orderOut:nil];
    }];
}

- (void)exitFullscreenWithDuration:(NSTimeInterval)duration {
    NSWindow *window = _glkctl.window;
    NSRect oldFrame = _glkctl.windowPreFullscreenFrame;
    NSView *gameContentView = _glkctl.contentView;

    _backgroundView.frame = window.contentView.frame;
    _imageView.frame = _backgroundView.frame;
    _backgroundView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _imageView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    CoverImageView *imageView = _imageView;
    [window makeFirstResponder:_backgroundView];

    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = duration;
        context.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
        // Make sure the window style mask does not
        // include full screen bit
        window.styleMask = (window.styleMask & ~NSWindowStyleMaskFullScreen);
        [[window animator] setFrame:oldFrame display:YES];
    } completionHandler:^{
        [imageView positionImage];
        gameContentView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    }];
}


@synthesize enterFullscreenWindow = _enterFullscreenWindow;

- (NSWindow *)enterFullscreenWindow {
    if (!_enterFullscreenWindow) {
        _enterFullscreenWindow =
        [[NSWindow alloc] initWithContentRect:NSZeroRect
                                    styleMask: NSWindowStyleMaskBorderless
                                      backing: NSBackingStoreBuffered
                                        defer: YES];
    }
    return _enterFullscreenWindow;
}

- (NSArray *)customWindowsToEnterFullScreenForWindow:(NSWindow *)window {
    [self adjustFullScreenWindow:self.enterFullscreenWindow];
    return @[window, self.enterFullscreenWindow];
}

- (NSArray *)customWindowsToExitFullScreenForWindow:(NSWindow *)window {
    return @[window];
}

@end
