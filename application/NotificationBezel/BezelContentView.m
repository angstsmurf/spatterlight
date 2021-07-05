//
//  BezelContentView.m
//  BezelTest
//
//  Created by Administrator on 2021-06-29.
//

#import "BezelContentView.h"
#import "NSFont+Categories.h"
#import <QuartzCore/QuartzCore.h>

@implementation BezelContentView

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];

    CGContextRef context = NSGraphicsContext.currentContext.CGContext;
    if (!context) {
        return;
    }

    if (_type == kGameOver) {
        [self drawGameOver];
        return;
    }

    NSRect textBounds = [self findBezelLabelBoundingBox];

    NSRect bezelBounds = self.bounds;
    CGFloat bezelCenterX = NSMidX(bezelBounds);
    CGFloat messageLabelTop = NSMaxY(textBounds);
    CGFloat halfwayBetweenLabelTopAndBezelTop = (NSMaxY(bezelBounds) + messageLabelTop) / 2;

    NSSize selfSize = bezelBounds.size;

    if (_image) {
        NSImage *icon = _image.copy;

        NSSize iconSize = [self scaleOriginal:icon.size toFitWithinParent:NSMakeSize(selfSize.width * 0.6, selfSize.height * 0.6)];


        NSPoint iconBottomLeftCorner = NSMakePoint(bezelCenterX - (iconSize.width / 2),
                                                   halfwayBetweenLabelTopAndBezelTop - (iconSize.height / 2));

        // Tint the image
        [icon lockFocus];

        [NSColor.grayColor set];
        NSRect imageRect = NSMakeRect(0, 0, iconSize.width, iconSize.height);
        NSRectFillUsingOperation(imageRect, NSCompositingOperationSourceAtop);

        [icon unlockFocus];


        [icon drawInRect:NSMakeRect(iconBottomLeftCorner.x, iconBottomLeftCorner.y, iconSize.width, iconSize.height) fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1 respectFlipped:YES hints:@{NSImageHintInterpolation : @(NSImageInterpolationHigh)}];

    } else if (_topText.length) {
        NSSize topTextSize = NSMakeSize(selfSize.width * 0.4, selfSize.height * 0.5 - messageLabelTop);

        NSPoint topTextBottomLeftCorner = NSMakePoint(bezelCenterX - (topTextSize.width / 2),
                                                      messageLabelTop - 10);

        CGContextSetTextDrawingMode(context, kCGTextFill);

        NSColor *color = NSColor.grayColor;
        NSFont *topFont;
        if (@available(macOS 10.15, *)) {
            topFont = [NSFont monospacedSystemFontOfSize:18 weight:NSFontWeightMedium];
        } else {
            topFont = [NSFont userFixedPitchFontOfSize:18];
            [[NSFontManager sharedFontManager] convertFont:topFont toHaveTrait:NSBoldFontMask];
        }

        topFont = [topFont fontToFitWidth:topTextSize.width sampleText:_topText];

        NSRect topTextRect;
        topTextRect.origin = topTextBottomLeftCorner;
        topTextRect.size = topTextSize;

        NSAttributedString *attributedString = [[NSAttributedString alloc] initWithString:_topText attributes:@{NSFontAttributeName : topFont}];
        NSRect topTextBounds = [attributedString boundingRectWithSize:topTextSize options: 0];

        topTextRect.size.height = topTextBounds.size.height;

        [_topText drawInRect:topTextRect withAttributes:
                      @{ NSForegroundColorAttributeName: color,
                                    NSFontAttributeName: topFont }];


    }

    CGContextSetTextDrawingMode(context, kCGTextFill);

    NSColor *color = [NSColor.textColor colorWithAlphaComponent:NSColor.textColor.alphaComponent * 0.8];

    NSFont *font = [NSFont systemFontOfSize:18];

    [_bottomText drawAtPoint:textBounds.origin withAttributes:@{ NSForegroundColorAttributeName : color , NSFontAttributeName : font } ];
}

- (void) drawGameOver {
    NSImage *icon = [NSImage imageNamed:@"Game Over"];

    NSSize selfSize = self.frame.size;

    NSSize iconSize = NSMakeSize(selfSize.width * 0.6, selfSize.height * 0.6);
    icon.size = iconSize;

    // Tint the image
    [icon lockFocus];

    [NSColor.grayColor set];
    NSRect imageRect = NSMakeRect(0, 0, iconSize.width, iconSize.height);
    NSRectFillUsingOperation(imageRect, NSCompositingOperationSourceAtop);

    [icon unlockFocus];

    CGFloat offset = (selfSize.width - iconSize.width) / 2;

    NSPoint iconBottomLeftCorner = NSMakePoint(offset, offset);

    [icon drawInRect:NSMakeRect(iconBottomLeftCorner.x, iconBottomLeftCorner.y, iconSize.width, iconSize.height) fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:0.8 respectFlipped:YES hints:@{}];
}

- (void)updateLayer {
    [super updateLayer];

    if (!_imageLayer) {
        _imageLayer = [CALayer layer];

        _imageLayer.magnificationFilter = (_interpolation == kNearestNeighbor) ? kCAFilterNearest : kCAFilterTrilinear;

        _imageLayer.frame = self.bounds;
        _imageLayer.opacity = 0.9f;

        _imageLayer.contents = _image;

        [self.layer addSublayer:_imageLayer];

        if (@available(macOS 10.15, *)) {
        } else {
            CAShapeLayer *masklayer = [CAShapeLayer layer];
            masklayer.fillColor = NSColor.blackColor.CGColor;
            masklayer.strokeColor = NSColor.blackColor.CGColor;
            masklayer.frame = self.bounds;
            masklayer.lineJoin = kCALineJoinRound;
            masklayer.lineWidth = 1;
            CGPathRef roundedRectPath = CGPathCreateWithRoundedRect(self.bounds, 18, 18, NULL);
            masklayer.path = roundedRectPath;
            _imageLayer.mask = masklayer;
            CFRelease(roundedRectPath);
        }
    }
}

- (BOOL)allowsVibrancy {
    return _type != kCoverImage;
}

- (BOOL)wantsUpdateLayer {
    return _type == kCoverImage;
}

- (NSRect)findBezelLabelBoundingBox {
    if (!_bottomText.length)
        return NSZeroRect;
    return [self boundingBoxForText:_bottomText font: [NSFont systemFontOfSize:18] bottomMargin:20];
}

- (NSRect)boundingBoxForText:(NSString *)text font:(NSFont *)font bottomMargin:(CGFloat)margin {
    NSAttributedString *attributedString = [[NSAttributedString alloc] initWithString:text attributes:@{NSFontAttributeName : font}];
    NSRect textBounds = [attributedString boundingRectWithSize:self.bounds.size options:0];
    CGFloat textLeftX = NSMidX(self.bounds) - NSMidX(textBounds);
    return NSMakeRect(textLeftX, margin, textBounds.size.width, textBounds.size.height);
}


- (NSSize)scaleOriginal:(NSSize)original toFitWithinParent:(NSSize)parent {
    // If it already fits, no need to scale. Just use the original.
    if (original.height < parent.height
        && original.width < parent.width) {
        return original;
    }
    else {
        CGFloat tooManyPixelsTall = MAX(0, original.height - parent.height);
        CGFloat tooManyPixelsWide = MAX(0, original.width - parent.width);

        BOOL shouldScaleToFitHeight = tooManyPixelsTall > tooManyPixelsWide;

        if (shouldScaleToFitHeight) {
            CGFloat percentChange = parent.height / original.height;
            return NSMakeSize(original.width * percentChange,
                              parent.height);
        }
        else {
            CGFloat percentChange = parent.width / original.width;
            return NSMakeSize(parent.width, original.height * percentChange);
        }
    }
}

@end
