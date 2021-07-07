//
//  CoverImageView.m
//  Spatterlight
//
//  Created by Administrator on 2021-01-05.
//

#import "CoverImageView.h"
#import "GlkController.h"
#import "CoverImageHandler.h"
#import "Theme.h"
#import "Game.h"
#import "Metadata.h"
#import "Image.h"

#import <QuartzCore/QuartzCore.h>


@implementation CoverImageView

- (instancetype)initWithFrame:(NSRect)frame delegate:(CoverImageHandler *)delegate {
    Game *game = delegate.glkctl.game;
    self = [super initWithGame:game image:nil];
    if (self) {
        _delegate = delegate;
        self.frame = frame;
        NSImageRep *rep = self.image.representations.lastObject;
        _sizeInPixels = NSMakeSize(rep.pixelsWide, rep.pixelsHigh);
        _ratio = _sizeInPixels.width / _sizeInPixels.height;
    }
    return self;
}

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];

    if (@available(macOS 10.15, *))
        return;

    CGContextRef context = NSGraphicsContext.currentContext.CGContext;
    if (!context)
        return;

    NSSize windowSize = self.frame.size;

    NSRect imageFrame = NSMakeRect(0, 0, windowSize.width, windowSize.width / _ratio);
    if (imageFrame.size.height > windowSize.height) {
        imageFrame = NSMakeRect(0,0, windowSize.height * _ratio, windowSize.height);
        imageFrame.origin.x = (windowSize.width - imageFrame.size.width) / 2;
    } else {
        imageFrame.origin.y = (windowSize.height - imageFrame.size.height) / 2;
        if (NSMaxY(imageFrame) > NSMaxY(self.frame))
            imageFrame.origin.y = NSMaxY(self.frame) - imageFrame.size.height;
    }
    [self.image drawInRect:imageFrame fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1 respectFlipped:YES hints:@{NSImageHintInterpolation : @(_interpolation)}];
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event {
    return YES;
}

- (void)keyDown:(NSEvent *)theEvent {
    [_delegate forkInterpreterTask];
}

- (void)mouseUp:(NSEvent *)theEvent {
    [_delegate forkInterpreterTask];
    [super mouseUp:theEvent];
}

- (void)mouseDown:(NSEvent *)theEvent {
    [self superMouseDown:theEvent];
}

- (void)layout {
        if (self.image && !_delegate.glkctl.ignoreResizes) {
            if (@available(macOS 10.15, *)) {
                [self positionImage];
            }
        }

    [super layout];
}

// We ignore mouse clicks if this is an image view in a content view.
// In this way, clicks on the image will work the same as clicks in the
// content view outside it.
//- (NSView *)hitTest:(NSPoint)point {
//    if ([self.superview isKindOfClass:[CoverImageView class]])
//        return nil;
//    return [super hitTest:point];
//}

//- (void)createAndPositionImage {
//    [self createImage];
//
//    Metadata *meta = _delegate.glkctl.game.metadata;
//
//    CALayer *layer = [CALayer layer];
//
//    layer.magnificationFilter = (meta.cover.interpolation == kNearestNeighbor) ? kCAFilterNearest : kCAFilterTrilinear;
//
//    layer.contents = self.image;
//
//    [self setLayer:layer];
//
//    layer.bounds = self.bounds;
//    layer.frame = self.frame;
//    [self positionImage];
//}

//- (void)createImage {
//    Metadata *meta = _delegate.glkctl.game.metadata;
//
//    self.image = [[NSImage alloc] initWithData:(NSData *)meta.cover.data];
//
//    NSImageRep *rep = self.image.representations.lastObject;
//    _sizeInPixels = NSMakeSize(rep.pixelsWide, rep.pixelsHigh);
//
//    _ratio = _sizeInPixels.width / _sizeInPixels.height;
//
//    self.accessibilityLabel = meta.coverArtDescription;
//
//    if (meta.cover.interpolation == kUnset) {
//        meta.cover.interpolation = _sizeInPixels.width < 350 ? kNearestNeighbor : kTrilinear;
//    }
//}

- (void)positionImage {
    if (_ratio == 0)
        return;
    NSView *superview = [self superview];
    NSSize windowSize = superview.frame.size;

    NSRect imageFrame = NSMakeRect(0,0, windowSize.width, windowSize.width / _ratio);
    if (imageFrame.size.height > windowSize.height) {
        imageFrame = NSMakeRect(0,0, windowSize.height * _ratio, windowSize.height);
        imageFrame.origin.x = (windowSize.width - imageFrame.size.width) / 2;
    } else {
        imageFrame.origin.y = (windowSize.height - imageFrame.size.height) / 2;
        if (NSMaxY(imageFrame) > NSMaxY(superview.frame))
            imageFrame.origin.y = NSMaxY(superview.frame) - imageFrame.size.height;
    }

    self.frame = imageFrame;
}


- (BOOL)isAccessibilityElement {
    return (self.image != nil);
}

- (NSString *)accessibilityRole {
    return NSAccessibilityImageRole;
}

@end
