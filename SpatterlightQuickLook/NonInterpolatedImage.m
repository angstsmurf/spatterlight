//
//  NonInterpolatedImage.m
//  SpatterlightQuickLook
//
//  Created by Administrator on 2021-10-18.
//

#import <Quartz/Quartz.h>
#import "NonInterpolatedImage.h"
#import "Image.h"
#import "Metadata.h"

@interface NonInterpolatedImage () {
    kImageInterpolationType interpolation;
}
@end

@implementation NonInterpolatedImage

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self) {
        interpolation = kUnset;
    }
    return self;
}


- (BOOL)wantsUpdateLayer {
    return YES;
}

- (void)addImageFromData:(NSData *)imageData {

    NSImage *image = [[NSImage alloc] initWithData:imageData];
    if (!image)
        return;

    self.wantsLayer = YES;
    CALayer *imageLayer = [CALayer layer];

    NSImageRep *rep = image.representations.firstObject;
    NSSize sizeInPixels = NSMakeSize(rep.pixelsWide, rep.pixelsHigh);
    image.size = sizeInPixels;

    if (interpolation == kUnset)
        interpolation = sizeInPixels.width < 350 ? kNearestNeighbor : kTrilinear;

    imageLayer.magnificationFilter = (interpolation == kNearestNeighbor) ? kCAFilterNearest : kCAFilterTrilinear;

    imageLayer.drawsAsynchronously = YES;
    imageLayer.contentsGravity = kCAGravityResize;

    self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawOnSetNeedsDisplay;

    [CATransaction begin];
    [CATransaction setValue:(id)kCFBooleanTrue
                     forKey:kCATransactionDisableActions];

    imageLayer.contents = image;
    imageLayer.frame = self.bounds;
    imageLayer.bounds = self.bounds;

    [self.layer addSublayer:imageLayer];

    imageLayer.layoutManager  = [CAConstraintLayoutManager layoutManager];
    imageLayer.autoresizingMask = kCALayerHeightSizable | kCALayerWidthSizable;

    [CATransaction commit];
}


- (void)addImageFromManagedObject:(Image *)imageObject {

    self.accessibilityLabel = imageObject.imageDescription;
    if (!self.accessibilityLabel.length)
        self.accessibilityLabel =
        [NSString stringWithFormat:@"cover image for %@", imageObject.metadata.anyObject.title];

    interpolation = imageObject.interpolation;

    [self addImageFromData:(NSData *)imageObject.data];
}

#pragma mark Accessibility

- (BOOL)isAccessibilityElement {
    return YES;
}

- (NSString *)accessibilityRole {
    return NSAccessibilityImageRole;
}


@end
