#import "ScalingScrollView.h"
#import "GlkController.h"

@implementation ScalingScrollView

- (void)awakeFromNib {
    [super awakeFromNib];
    [self setAllowsMagnification:YES];
    [self setMaxMagnification:8.0];
    [self setMinMagnification:0.1];
}

- (IBAction)zoomToActualSize:(id)sender {
    CGFloat scaleFactor = 1.0 / self.magnification;

    NSWindow *win = self.glkctl.window;
    NSRect frame = win.contentView.frame;
    frame.size.width *= scaleFactor;
    frame.size.height *= scaleFactor;

    NSRect winFrame = [win frameRectForContentRect:frame];
    winFrame.origin = win.frame.origin;

    NSRect screenframe = win.screen.visibleFrame;

    CGFloat widthdiff = winFrame.size.width - win.frame.size.width;
    winFrame.origin.x -= round(widthdiff / 2);
    CGFloat screenx = screenframe.origin.x;
    if (winFrame.origin.x < screenx)
        winFrame.origin.x = screenx;
    CGFloat heightdiff = winFrame.size.height - win.frame.size.height;
    winFrame.origin.y -= round(heightdiff / 2);
    CGFloat screeny = screenframe.origin.y;
    if (winFrame.origin.y < screeny)
        winFrame.origin.y = screeny;

    [self animate:winFrame scaleFactor:scaleFactor];

    [self.glkctl myZoomToActualSize];
}

- (IBAction)zoomIn:(id)sender {
    CGFloat scaleFactor = self.magnification;
//    scaleFactor = (scaleFactor > 0.4 && scaleFactor < 0.6) ? 1.0 : scaleFactor * 1.2;
    NSLog(@"zoomIn: scale factor increased from %f to %f", scaleFactor, scaleFactor * 1.2);

    scaleFactor = scaleFactor * 1.2;

//    NSPoint origin = [self adjustOrigin:scaleFactor];

    NSWindow *win = self.glkctl.window;
    NSRect frame = win.contentView.frame;
//    NSLog(@"zoomIn: ratio height (%f) / width (%f) = %f ", frame.size.height, frame.size.width, ratio);

    frame.size.width *= 1.2;
    frame.size.height *= 1.2;

    NSRect winFrame = [win frameRectForContentRect:frame];
    winFrame.origin = win.frame.origin;

    NSRect screenframe = win.screen.visibleFrame;
    NSLog(@"zoomIn: win.screen.visibleFrame %@", NSStringFromRect(screenframe));


    CGFloat widthdiff = winFrame.size.width - win.frame.size.width;
    winFrame.origin.x -= round(widthdiff / 2);
    CGFloat screenx = screenframe.origin.x;
    if (winFrame.origin.x < screenx)
        winFrame.origin.x = screenx;
    CGFloat heightdiff = winFrame.size.height - win.frame.size.height;
    winFrame.origin.y -= round(heightdiff / 2);
    CGFloat screeny = screenframe.origin.y;
    if (winFrame.origin.y < screeny)
        winFrame.origin.y = screeny;

    winFrame = NSIntersectionRect(screenframe, winFrame);

    NSLog(@"zoomIn: Old size: %@ New size %@", NSStringFromSize(win.frame.size), NSStringFromSize(frame.size));

    NSSize size = winFrame.size;
    NSSize minSize = win.minSize;
    win.minSize = NSMakeSize(MIN(size.width, minSize.width), MIN(size.height, minSize.height));

    [self animate: winFrame scaleFactor:scaleFactor];



    [self.glkctl myZoomIn:scaleFactor];
}

- (IBAction)zoomOut:(id)sender {
    CGFloat scaleFactor = self.magnification;
//    scaleFactor = (scaleFactor > 1.8 && scaleFactor < 2.2) ? 1.0 : scaleFactor / 4.0;
    NSLog(@"zoomOut: scale factor decreased from %f to %f", scaleFactor, scaleFactor * 0.8);

    scaleFactor = scaleFactor * 0.8;
    self.minMagnification = MIN(self.minMagnification, scaleFactor);
//    if (scaleFactor < 1.0)
//        scaleFactor = 1.0;
    NSPoint origin = [self adjustOrigin:scaleFactor];

    NSWindow *win = self.glkctl.window;
    NSRect frame = win.contentView.frame;
//    CGFloat ratio = frame.size.height / frame.size.width;
//    NSLog(@"zoomOut: ratio height (%f) / width (%f) = %f ", frame.size.height, frame.size.width, ratio);

    frame.size.width *= 0.8;
//    frame.size.height = frame.size.width * ratio;
    frame.size.height *= 0.8;

    NSRect winFrame = [win frameRectForContentRect:frame];
    winFrame.origin = win.frame.origin;

    NSRect screenframe = win.screen.visibleFrame;
    NSLog(@"zoomOut: win.screen.visibleFrame %@", NSStringFromRect(screenframe));


    CGFloat widthdiff = winFrame.size.width - win.frame.size.width;
    winFrame.origin.x -= round(widthdiff / 2);
    CGFloat screenx = screenframe.origin.x;
    if (winFrame.origin.x < screenx)
        winFrame.origin.x = screenx;
    CGFloat heightdiff = winFrame.size.height - win.frame.size.height;
    winFrame.origin.y -= round(heightdiff / 2);
    CGFloat screeny = screenframe.origin.y;
    if (winFrame.origin.y < screeny)
        winFrame.origin.y = screeny;

    winFrame = NSIntersectionRect(screenframe, winFrame);

    NSLog(@"zoomOut: Old size: %@ New size %@", NSStringFromSize(win.frame.size), NSStringFromSize(winFrame.size));

    [self animate: winFrame scaleFactor:scaleFactor];

    [self.glkctl myZoomOut:scaleFactor];
}

- (BOOL)scrolledToBottom {
    CGFloat textContainerInset = 0.0;
    if ([self.documentView isKindOfClass:[NSTextView class]])
        textContainerInset = ((NSTextView *)self.documentView).textContainerInset.height;
    return (NSHeight(self.documentView.bounds) - NSMaxY(self.contentView.bounds) < 2 + textContainerInset);
}

- (NSPoint)adjustOrigin:(CGFloat)scaleFactor {
    NSPoint origin = self.contentView.bounds.origin;

    if ([self scrolledToBottom]) {
        origin.y = origin.y + (origin.y/scaleFactor);
    }

    return origin;
}

@synthesize glkctl = _glkctl;

- (GlkController *)glkctl {
        if (_glkctl == nil) {
            if (self.documentView) {
                _glkctl = (GlkController *)((GlkHelperView *)self.documentView.subviews.firstObject).glkctrl;
            }
        }
    return _glkctl;
}


#pragma mark animation

- (void)makeAndPrepareSnapshotWindow:(NSRect)startingframe {
    CALayer *snapshotLayer = [self takeSnapshot];
    NSWindow *snapshotWindow = ([[NSWindow alloc]
                       initWithContentRect:startingframe
                       styleMask:self.window.styleMask
                       backing:NSBackingStoreBuffered
                       defer:NO]);

    snapshotWindow.contentView.wantsLayer = YES;
    snapshotWindow.opaque = NO;
    snapshotWindow.titlebarAppearsTransparent = YES;
    snapshotWindow.releasedWhenClosed = YES;
//    snapshotWindow.backgroundColor = self.window.backgroundColor;
//    snapshotLayer.backgroundColor = self.window.contentView.layer.backgroundColor;


 //   snapshotWindow.backgroundColor = [NSColor clearColor];
//    snapshotWindow.contentView.layer.backgroundColor = self.window.contentView.layer.backgroundColor;
    [snapshotWindow setFrame:startingframe display:NO];
    [[[snapshotWindow contentView] layer] addSublayer:snapshotLayer];

    _snapshotController = [[NSWindowController alloc] initWithWindow:snapshotWindow];
    // Compute the frame of the snapshot layer such that the snapshot is
    // positioned on startingframe.
    NSRect snapshotLayerFrame =
    [snapshotWindow convertRectFromScreen:startingframe];
    snapshotLayer.frame = snapshotLayerFrame;
    [snapshotWindow makeKeyAndOrderFront:nil];
}

- (CALayer *)takeSnapshot {
    CGImageRef windowSnapshot = CGWindowListCreateImage(
                                                        CGRectNull, kCGWindowListOptionIncludingWindow,
                                                        (CGWindowID)[self.window windowNumber], kCGWindowImageBoundsIgnoreFraming);
    CALayer *snapshotLayer = [[CALayer alloc] init];
    [snapshotLayer setFrame:NSRectToCGRect([self.window frame])];
    [snapshotLayer setContents:CFBridgingRelease(windowSnapshot)];
    [snapshotLayer setAnchorPoint:CGPointMake(0, 0)];
    return snapshotLayer;
}

- (void)animate:(NSRect)finalframe scaleFactor:(CGFloat)scaleFactor{

    NSRect startFrame = self.window.frame;

    [self makeAndPrepareSnapshotWindow:startFrame];
    NSWindow *localSnapshot = _snapshotController.window;
    NSView *snapshotView = localSnapshot.contentView;
    CALayer *snapshotLayer = localSnapshot.contentView.layer.sublayers.firstObject;

    snapshotLayer.layoutManager  = [CAConstraintLayoutManager layoutManager];
    snapshotLayer.autoresizingMask = kCALayerHeightSizable | kCALayerWidthSizable;
    snapshotView.wantsLayer = YES;

    localSnapshot.title = self.window.title;
    localSnapshot.representedFilename = self.window.representedFilename;

    [localSnapshot setFrame:startFrame display:YES];

    NSWindow *win = self.window;

    win.alphaValue = 0;

    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = 0.1;
        context.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionDefault];

        [[localSnapshot animator] setFrame:finalframe display:YES];


    }
     completionHandler:^{
        [self
          setMagnification:scaleFactor];
        self.window.alphaValue = 1.f;

        [win setFrame:finalframe display:YES];
        self.frame = win.contentView.frame;
        if (self.frame.size.width != win.frame.size.width)
            NSLog(@"Error!");
        if (self.contentView.frame.size.width != win.frame.size.width)
            NSLog(@"Error!");
        NSRect newFrame = win.frame;

        if (round(self.documentView.frame.size.width * self.magnification) != win.frame.size.width) {

            CGFloat diff = round(self.documentView.frame.size.width * self.magnification) - win.frame.size.width;
            newFrame.size.width += diff;
        }

        if (round(self.documentView.frame.size.height * self.magnification) != win.contentView.frame.size.width) {

            CGFloat diff = round(self.documentView.frame.size.width * self.magnification) - win.contentView.frame.size.width;
            newFrame.size.height += diff;
        }


        [win setFrame:newFrame display:YES];
        [win makeKeyAndOrderFront:nil];
        snapshotView.hidden = YES;
//        localSnapshot.alphaValue = 0;
        [self.snapshotController close];

//        self.inAnimation = NO;
    }];
}



/* Reassure AppKit that ScalingScrollView supports live resize content preservation, even though it's a subclass that could have modified NSScrollView in such a way as to make NSScrollView's live resize content preservation support inoperative. By default this is disabled for NSScrollView subclasses.
*/
- (BOOL)preservesContentDuringLiveResize {
    return [self drawsBackground];
}

//-(BOOL)isCompatibleWithResponsiveScrolling {
//    return YES;
//}

@end
