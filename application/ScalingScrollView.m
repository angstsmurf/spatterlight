#import "ScalingScrollView.h"
#import "GlkController.h"

@implementation ScalingScrollView

- (void)awakeFromNib {
    [super awakeFromNib];
    [self setAllowsMagnification:YES];
    [self setMaxMagnification:8.0];
    [self setMinMagnification:1.0];
}

- (IBAction)zoomToActualSize:(id)sender {
    [[self animator] setMagnification:1.0];
    [self.glkctl myZoomToActualSize];
}

- (IBAction)zoomIn:(id)sender {
    CGFloat scaleFactor = self.magnification;
//    scaleFactor = (scaleFactor > 0.4 && scaleFactor < 0.6) ? 1.0 : scaleFactor * 1.2;
    scaleFactor = scaleFactor * 1.2;

    NSPoint origin = [self adjustOrigin:scaleFactor];

    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        [[self.contentView animator] setBoundsOrigin:origin];
        [[self animator]
          setMagnification:scaleFactor];
    } completionHandler:^{
//        [self printDiff:scaleFactor];
    }];

    [self.glkctl myZoomIn:scaleFactor];
}

- (IBAction)zoomOut:(id)sender {
    CGFloat scaleFactor = self.magnification;
//    scaleFactor = (scaleFactor > 1.8 && scaleFactor < 2.2) ? 1.0 : scaleFactor / 4.0;
    scaleFactor = scaleFactor * 0.8;
    if (scaleFactor < 1.0)
        scaleFactor = 1.0;
    NSPoint origin = [self adjustOrigin:scaleFactor];

    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        [[self.contentView animator] setBoundsOrigin:origin];
        [[self animator]
          setMagnification:scaleFactor];
    } completionHandler:^{
//        [self printDiff:scaleFactor];
    }];
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


/* Reassure AppKit that ScalingScrollView supports live resize content preservation, even though it's a subclass that could have modified NSScrollView in such a way as to make NSScrollView's live resize content preservation support inoperative. By default this is disabled for NSScrollView subclasses.
*/
- (BOOL)preservesContentDuringLiveResize {
    return [self drawsBackground];
}

-(BOOL)isCompatibleWithResponsiveScrolling {
    return YES;
}

@end
