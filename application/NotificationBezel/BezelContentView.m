//
//  BezelContentView.m
//  Spatterlight
//
//  Created by Administrator on 2021-06-29.
//

#import "BezelContentView.h"
#import "NSFont+Categories.h"
#import <QuartzCore/QuartzCore.h>

static const CGFloat kLabelFontSize = 18;
static const CGFloat kLabelBottomMargin = 20;
static const CGFloat kCornerRadius = 18;

@interface BezelContentView ()

@property (nullable) CALayer *imageLayer;

@end

@implementation BezelContentView

+ (NSFont *)labelFont {
    return [NSFont systemFontOfSize:kLabelFontSize];
}

+ (NSFont *)topTextFont {
    if (@available(macOS 10.15, *)) {
        return [NSFont monospacedSystemFontOfSize:kLabelFontSize weight:NSFontWeightMedium];
    } else {
        NSFont *font = [NSFont userFixedPitchFontOfSize:kLabelFontSize];
        return [[NSFontManager sharedFontManager] convertFont:font toHaveTrait:NSBoldFontMask];
    }
}

- (void)drawRect:(NSRect)dirtyRect {
    if (_type == kGameOver) {
        [self drawGameOver];
        return;
    }

    NSRect textBounds = [self findBezelLabelBoundingBox];
    CGFloat messageLabelTop = NSMaxY(textBounds);

    if (_image) {
        [self drawIconAboveLabelTop:messageLabelTop];
    } else if (_topText.length) {
        [self drawTopTextAboveLabelTop:messageLabelTop];
    }

    NSColor *color = [NSColor.textColor colorWithAlphaComponent:NSColor.textColor.alphaComponent * 0.8];

    [_bottomText drawAtPoint:textBounds.origin withAttributes:@{ NSForegroundColorAttributeName : color, NSFontAttributeName : BezelContentView.labelFont }];
}

- (void)drawIconAboveLabelTop:(CGFloat)messageLabelTop {
    NSRect bezelBounds = self.bounds;
    NSSize selfSize = bezelBounds.size;
    CGFloat halfwayBetweenLabelTopAndBezelTop = (NSMaxY(bezelBounds) + messageLabelTop) / 2;

    NSSize iconSize = [self scaleOriginal:_image.size toFitWithinParent:NSMakeSize(selfSize.width * 0.6, selfSize.height * 0.6)];

    NSRect iconRect = NSMakeRect(NSMidX(bezelBounds) - (iconSize.width / 2),
                                 halfwayBetweenLabelTopAndBezelTop - (iconSize.height / 2),
                                 iconSize.width, iconSize.height);

    [[self image:_image tintedWithColor:NSColor.grayColor size:iconSize] drawInRect:iconRect fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1 respectFlipped:YES hints:@{NSImageHintInterpolation : @(NSImageInterpolationHigh)}];
}

- (void)drawTopTextAboveLabelTop:(CGFloat)messageLabelTop {
    NSSize selfSize = self.bounds.size;
    NSSize topTextSize = NSMakeSize(selfSize.width * 0.4, selfSize.height * 0.5 - messageLabelTop);

    // Short words such as "?" get a big glyph; two- and three-letter
    // words are allowed to use almost the full width.
    if (_topText.length == 2)
        topTextSize.width = selfSize.width * 0.85;
    else if (_topText.length == 3)
        topTextSize.width = selfSize.width * 0.9;

    NSFont *topFont = [BezelContentView.topTextFont fontToFitWidth:topTextSize.width sampleText:_topText];

    CGFloat kerning = topFont.pointSize * -0.1;
    NSDictionary *attributes = @{ NSForegroundColorAttributeName: NSColor.grayColor,
                                  NSFontAttributeName: topFont,
                                  NSKernAttributeName: @(kerning) };

    NSAttributedString *attributedString = [[NSAttributedString alloc] initWithString:_topText attributes:attributes];
    NSRect topTextBounds = [attributedString boundingRectWithSize:topTextSize options:0];

    NSRect topTextRect;
    topTextRect.origin = NSMakePoint(NSMidX(self.bounds) - ((topTextBounds.size.width - kerning) / 2),
                                     messageLabelTop - 10);
    topTextRect.size = topTextBounds.size;

    [_topText drawInRect:topTextRect withAttributes:attributes];
}

- (void)drawGameOver {
    // A fresh tinted copy: the named image is a shared, cached instance
    // and must not be resized or drawn into.
    NSImage *icon = [NSImage imageNamed:@"Game Over"];

    NSSize selfSize = self.frame.size;

    NSSize iconSize = NSMakeSize(selfSize.width * 0.9, selfSize.height * 0.9);
    iconSize = [self scaleOriginal:icon.size toFitWithinParent:iconSize];

    NSRect iconRect = NSMakeRect((selfSize.width - iconSize.width) / 2,
                                 (selfSize.height - iconSize.height) / 2,
                                 iconSize.width, iconSize.height);

    [[self image:icon tintedWithColor:NSColor.grayColor size:iconSize] drawInRect:iconRect fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:0.8 respectFlipped:YES hints:@{}];
}

- (NSImage *)image:(NSImage *)image tintedWithColor:(NSColor *)color size:(NSSize)size {
    return [NSImage imageWithSize:size flipped:NO drawingHandler:^BOOL(NSRect dstRect) {
        [image drawInRect:dstRect];
        [color set];
        NSRectFillUsingOperation(dstRect, NSCompositingOperationSourceAtop);
        return YES;
    }];
}

- (void)updateLayer {
    [super updateLayer];

    if (!_imageLayer) {
        _imageLayer = [CALayer layer];
        _imageLayer.opacity = 0.9f;
        [self.layer addSublayer:_imageLayer];

        if (@available(macOS 10.15, *)) {
        } else {
            CAShapeLayer *masklayer = [CAShapeLayer layer];
            masklayer.fillColor = NSColor.blackColor.CGColor;
            masklayer.strokeColor = NSColor.blackColor.CGColor;
            masklayer.frame = self.bounds;
            masklayer.lineJoin = kCALineJoinRound;
            masklayer.lineWidth = 1;
            CGPathRef roundedRectPath = CGPathCreateWithRoundedRect(self.bounds, kCornerRadius, kCornerRadius, NULL);
            masklayer.path = roundedRectPath;
            _imageLayer.mask = masklayer;
            CFRelease(roundedRectPath);
        }
    }

    _imageLayer.magnificationFilter = (_interpolation == kNearestNeighbor) ? kCAFilterNearest : kCAFilterTrilinear;
    _imageLayer.frame = self.bounds;
    _imageLayer.contents = _image;
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
    NSAttributedString *attributedString = [[NSAttributedString alloc] initWithString:_bottomText attributes:@{NSFontAttributeName : BezelContentView.labelFont}];
    NSRect textBounds = [attributedString boundingRectWithSize:self.bounds.size options:0];
    CGFloat textLeftX = NSMidX(self.bounds) - NSMidX(textBounds);
    return NSMakeRect(textLeftX, kLabelBottomMargin, textBounds.size.width, textBounds.size.height);
}

// Scales down (never up) to fit within parent, keeping the aspect ratio.
- (NSSize)scaleOriginal:(NSSize)original toFitWithinParent:(NSSize)parent {
    if (original.width <= 0 || original.height <= 0)
        return NSZeroSize;
    CGFloat scale = MIN(1, MIN(parent.width / original.width, parent.height / original.height));
    return NSMakeSize(original.width * scale, original.height * scale);
}

@end
