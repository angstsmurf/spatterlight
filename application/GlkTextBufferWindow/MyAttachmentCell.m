//
//  MyAttachmentCell.m
//  Spatterlight
//
//  Created by Administrator on 2021-04-02.
//

#import "MyAttachmentCell.h"
#import "MarginImage.h"
#import "ImageHandler.h"
#import "ImageDragSource.h"
#include "glk.h"

@interface MyAttachmentCell () <NSSecureCoding> {
    ImageDragSource *dragSource;
    CGFloat lastXHeight;
    CGFloat lastAscender;
    // Display size resolved by the most recent layout pass
    // (cellFrameForTextContainer:). Starts out as the image's size.
    NSSize lastDisplaySize;
}
@end

@implementation MyAttachmentCell

+ (BOOL) supportsSecureCoding {
    return YES;
}

+ (BOOL)isCompatibleWithResponsiveScrolling
{
    return YES;
}

+ (BOOL)isMarginAlignment:(NSInteger)alignment {
    return alignment == imagealign_MarginLeft || alignment == imagealign_MarginRight;
}

- (instancetype)initImageCell:(NSImage *)image
                 andAlignment:(NSInteger)alignment
                    andAttStr:(NSAttributedString *)anattrstr
                           at:(NSUInteger)apos
                        index:(NSInteger)index {
    if ([MyAttachmentCell isMarginAlignment:alignment])
        self = [super initImageCell:nil];
    else
        self = [super initImageCell:image];
    if (self) {
        _glkImgAlign = alignment;
        _attrstr = anattrstr;
        _pos = apos;
        _index = index;
        _imageNumber = -1;
        _naturalSize = image.size;
        lastDisplaySize = image.size;
        if (image.accessibilityDescription.length) {
            self.accessibilityLabel = image.accessibilityDescription;
            _hasDescription = YES;
        }
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        _glkImgAlign = [decoder decodeIntegerForKey:@"align"];
        _marginImgUUID = [decoder decodeObjectOfClass:[NSString class] forKey:@"marginImgUUID"];
        _pos = (NSUInteger)[decoder decodeIntegerForKey:@"pos"];
        _index = [decoder decodeIntegerForKey:@"index"];
        _imageNumber = [decoder containsValueForKey:@"imageNumber"] ?
            [decoder decodeIntegerForKey:@"imageNumber"] : -1;
        lastXHeight = [decoder decodeDoubleForKey:@"lastXHeight"];
        lastAscender = [decoder decodeDoubleForKey:@"lastAscender"];
        _hasDescription = [decoder decodeBoolForKey:@"hasDescription"];
        self.accessibilityLabel = (NSString *)[decoder decodeObjectOfClass:[NSString class] forKey:@"label"];
        // Glk 0.7.6 scaling rule; all absent (0) in pre-rule archives.
        _imagerule = (NSUInteger)[decoder decodeIntegerForKey:@"imagerule"];
        _ruleWidth = (NSUInteger)[decoder decodeIntegerForKey:@"ruleWidth"];
        _ruleHeight = (NSUInteger)[decoder decodeIntegerForKey:@"ruleHeight"];
        _ruleMaxWidth = (NSUInteger)[decoder decodeIntegerForKey:@"ruleMaxWidth"];
        _naturalSize = [decoder decodeSizeForKey:@"naturalSize"];
        if (NSEqualSizes(_naturalSize, NSZeroSize))
            _naturalSize = self.image.size;
        lastDisplaySize = self.image.size;
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];
    [encoder encodeInteger:_glkImgAlign forKey:@"align"];
    [encoder encodeObject:_marginImgUUID forKey:@"marginImgUUID"];
    [encoder encodeObject:self.accessibilityLabel forKey:@"label"];
    [encoder encodeDouble:lastXHeight forKey:@"lastXHeight"];
    [encoder encodeDouble:lastAscender forKey:@"lastAscender"];
    [encoder encodeInteger:(NSInteger)_pos forKey:@"pos"];
    [encoder encodeInteger:_index forKey:@"index"];
    [encoder encodeInteger:_imageNumber forKey:@"imageNumber"];
    [encoder encodeBool:_hasDescription forKey:@"hasDescription"];
    [encoder encodeInteger:(NSInteger)_imagerule forKey:@"imagerule"];
    [encoder encodeInteger:(NSInteger)_ruleWidth forKey:@"ruleWidth"];
    [encoder encodeInteger:(NSInteger)_ruleHeight forKey:@"ruleHeight"];
    [encoder encodeInteger:(NSInteger)_ruleMaxWidth forKey:@"ruleMaxWidth"];
    [encoder encodeSize:_naturalSize forKey:@"naturalSize"];
}

- (BOOL)isMargin {
    return [MyAttachmentCell isMarginAlignment:_glkImgAlign];
}

- (BOOL)wantsToTrackMouse {
    return YES;
}

- (BOOL)wantsToTrackMouseForEvent:(NSEvent *)theEvent
                           inRect:(NSRect)cellFrame
                           ofView:(NSView *)controlView
                 atCharacterIndex:(NSUInteger)charIndex {
    if (self.isMargin)
        return NO;
    return (theEvent.type == NSEventTypeLeftMouseDragged || theEvent.type == NSEventTypeLeftMouseDown);
}

#pragma mark Glk 0.7.6 rule scaling

+ (NSSize)resolveImageRule:(NSUInteger)imagerule
                     width:(NSUInteger)ruleWidth
                    height:(NSUInteger)ruleHeight
                  maxwidth:(NSUInteger)maxwidth
                   natural:(NSSize)natural
                 available:(CGFloat)availwidth {
    CGFloat w, h;
    switch (imagerule & imagerule_WidthMask) {
        case imagerule_WidthFixed:
            w = (CGFloat)ruleWidth;
            break;
        case imagerule_WidthRatio:
            w = availwidth * (CGFloat)ruleWidth / 65536.0;
            break;
        default: /* imagerule_WidthOrig */
            w = natural.width;
            break;
    }
    switch (imagerule & imagerule_HeightMask) {
        case imagerule_HeightFixed:
            h = (CGFloat)ruleHeight;
            break;
        case imagerule_AspectRatio:
            h = natural.width > 0
                ? w * (natural.height / natural.width) * (CGFloat)ruleHeight / 65536.0
                : 0;
            break;
        default: /* imagerule_HeightOrig */
            h = natural.height;
            break;
    }
    // maxwidth: an additional window-width bound, applied after both rules,
    // reducing proportionally (regardless of how the height was determined).
    if (maxwidth && availwidth > 1) {
        CGFloat maxw = availwidth * (CGFloat)maxwidth / 65536.0;
        if (w > maxw) {
            if (w > 0)
                h = h * maxw / w;
            w = maxw;
        }
    }
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    return NSMakeSize(w, h);
}

// The display size for a given wrap width: the full 0.7.6 rule resolution
// for rule cells; for legacy cells the image size, proportionally reduced
// when wider than the window (the 0.7.6 behaviour change for plain
// glk_image_draw/glk_image_draw_scaled in buffer windows).
- (NSSize)displaySizeForWidth:(CGFloat)availwidth {
    if (_imagerule)
        return [MyAttachmentCell resolveImageRule:_imagerule
                                            width:_ruleWidth
                                           height:_ruleHeight
                                         maxwidth:_ruleMaxWidth
                                          natural:_naturalSize
                                        available:availwidth];
    NSSize size = self.image.size;
    if (availwidth > 1 && size.width > availwidth) {
        size.height = size.height * availwidth / size.width;
        size.width = availwidth;
        if (size.height < 1)
            size.height = 1;
    }
    return size;
}

// The index of the character just before the attachment, whose font and
// link the cell goes by. NSNotFound if the string is gone or too short.
- (NSUInteger)anchorCharacterIndex {
    NSUInteger length = _attrstr.length;
    if (length == 0)
        return NSNotFound;
    NSUInteger lastCharPos = _pos - 1;
    if (_pos == 0 || lastCharPos >= length)
        lastCharPos = 0;
    return lastCharPos;
}

- (NSPoint)cellBaselineOffset {
    NSUInteger lastCharPos = [self anchorCharacterIndex];
    NSFont *font = nil;
    if (lastCharPos != NSNotFound)
        font = [_attrstr attribute:NSFontAttributeName
                           atIndex:lastCharPos
                    effectiveRange:nil];

    CGFloat ascender = font.ascender;
    if (ascender == 0)
        ascender = lastAscender;
    else
        lastAscender = ascender;

    CGFloat xHeight = font.xHeight;
    if (xHeight == 0)
        xHeight = lastXHeight;
    else
        lastXHeight = xHeight;

    CGFloat displayHeight = lastDisplaySize.height > 0
        ? lastDisplaySize.height : self.image.size.height;

    if (_glkImgAlign == imagealign_InlineCenter) {
        return NSMakePoint(0, -(displayHeight / 2) + xHeight / 2);
    } else if (_glkImgAlign == imagealign_InlineDown) {
        return NSMakePoint(0, -displayHeight + ascender);
    }

    return [super cellBaselineOffset];
}

- (NSSize)cellSize {
    if (self.isMargin)
        return NSZeroSize;
    if (lastDisplaySize.width > 0 && !NSEqualSizes(lastDisplaySize, self.image.size))
        return lastDisplaySize;
    return [super cellSize];
}

- (NSRect)cellFrameForTextContainer:(NSTextContainer *)textContainer
               proposedLineFragment:(NSRect)lineFrag
                      glyphPosition:(NSPoint)position
                     characterIndex:(NSUInteger)charIndex {
    if (self.isMargin)
        return NSZeroRect;
    // Resolve the display size against the current wrap width, every layout
    // pass, so rule-scaled (and oversized legacy) images track resizes.
    CGFloat availwidth = lineFrag.size.width - 2 * textContainer.lineFragmentPadding;
    NSSize size = [self displaySizeForWidth:availwidth];
    if (!NSEqualSizes(size, lastDisplaySize)) {
        lastDisplaySize = size;
    }
    if (_imagerule || !NSEqualSizes(size, self.image.size)) {
        NSPoint offset = [self cellBaselineOffset];
        return NSMakeRect(offset.x, offset.y, size.width, size.height);
    }
    return [super cellFrameForTextContainer:textContainer
                       proposedLineFragment:lineFrag
                              glyphPosition:position
                             characterIndex:charIndex];
}

// Draw the image scaled into cellFrame when the resolved display size
// differs from the stored image size (rule-scaled cells, and legacy images
// reduced to the window width). Returns NO when the default unscaled
// drawing should run instead.
//
// A cell whose animation has started always draws here, with the current
// frame in place of the stored image.
- (BOOL)drawScaledInFrame:(NSRect)cellFrame {
    NSImage *image = self.image;
    BOOL animating = NO;
    if (_animation && _animationStart != 0) {
        // Switches the shared image to the frame; it must be drawn right away.
        NSImage *frame = [_animation imageForFrame:_animationFrame];
        if (frame) {
            image = frame;
            animating = YES;
        }
    }
    if (!image)
        return NO;
    if (!animating && (lastDisplaySize.width <= 0 ||
                       NSEqualSizes(lastDisplaySize, image.size)))
        return NO;
    NSGraphicsContext *ctx = NSGraphicsContext.currentContext;
    NSImageInterpolation oldInterpolation = ctx.imageInterpolation;
    ctx.imageInterpolation = NSImageInterpolationHigh;
    [image drawInRect:cellFrame
             fromRect:NSZeroRect
            operation:NSCompositingOperationSourceOver
             fraction:1.0
       respectFlipped:YES
                hints:nil];
    ctx.imageInterpolation = oldInterpolation;
    return YES;
}

// The three drawing entry points share one rule: margin cells draw nothing,
// and the rest draw scaled when they need to, unscaled through super if not.
- (void)drawWithFrame:(NSRect)cellFrame
               inView:(NSView *)controlView {
    if (self.isMargin || [self drawScaledInFrame:cellFrame])
        return;
    [super drawWithFrame:cellFrame
                  inView:controlView];
}

- (void)drawWithFrame:(NSRect)cellFrame
               inView:(NSView *)controlView
       characterIndex:(NSUInteger)charIndex {
    if (self.isMargin || [self drawScaledInFrame:cellFrame])
        return;
    [super drawWithFrame:cellFrame
                  inView:controlView
          characterIndex:charIndex];
}

- (void)drawWithFrame:(NSRect)cellFrame
               inView:(NSView *)controlView
       characterIndex:(NSUInteger)charIndex
        layoutManager:(NSLayoutManager *)layoutManager  {
    if (self.isMargin || [self drawScaledInFrame:cellFrame])
        return;
    [super drawWithFrame:cellFrame
                  inView:controlView
          characterIndex:charIndex
           layoutManager:layoutManager];
}

- (void)highlight:(BOOL)flag
        withFrame:(NSRect)cellFrame
           inView:(NSView *)controlView {
    if (!self.isMargin)
        [super highlight:flag
               withFrame:cellFrame
                  inView:controlView];
}

#pragma mark Dragging

- (void)dragTextAttachmentFrom:(NSTextView *)source event:(NSEvent *)event filename:(NSString *)filename inRect:(NSRect)frame {
    if (!dragSource)
        dragSource = [ImageDragSource new];
    [dragSource dragImage:self.image from:source event:event filename:filename frame:frame];
}

#pragma mark Accessibility

- (NSString *)customA11yLabel {
    if (_marginImage) {
        return _marginImage.customA11yLabel;
    }
    NSString *label = self.image.accessibilityDescription;
    NSUInteger lastCharPos = [self anchorCharacterIndex];
    NSNumber *linkVal = nil;
    if (lastCharPos != NSNotFound)
        linkVal = (NSNumber *)[_attrstr attribute:NSLinkAttributeName
                                          atIndex:lastCharPos
                                   effectiveRange:nil];

    NSUInteger linkid = linkVal.unsignedIntegerValue;

    if (!label.length) {
        if (linkid)
            label = NSLocalizedString(@"Clickable attached image", nil);
        else
            label = NSLocalizedString(@"Attached image", nil);
    }
    return label;
}


@end
