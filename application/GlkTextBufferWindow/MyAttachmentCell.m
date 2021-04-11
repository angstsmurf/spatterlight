//
//  MyAttachmentCell.m
//  Spatterlight
//
//  Created by Administrator on 2021-04-02.
//

#import "MyAttachmentCell.h"
#import "MarginImage.h"
#include "glk.h"

@interface MyAttachmentCell () <NSSecureCoding> {
    NSInteger align;
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

- (instancetype)initImageCell:(NSImage *)image
                 andAlignment:(NSInteger)analignment
                    andAttStr:(NSAttributedString *)anattrstr
                           at:(NSUInteger)apos {
    self = [super initImageCell:image];
    if (self) {
        align = analignment;
        _attrstr = anattrstr;
        _pos = apos;
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
        align = [decoder decodeIntegerForKey:@"align"];
        _attrstr = [decoder decodeObjectOfClass:[NSAttributedString class] forKey:@"attstr"];
        _pos = (NSUInteger)[decoder decodeIntegerForKey:@"pos"];
        _hasDescription = [decoder decodeBoolForKey:@"hasDescription"];
        self.accessibilityLabel = (NSString *)[decoder decodeObjectOfClass:[NSString class] forKey:@"label"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];
    [encoder encodeInteger:align forKey:@"align"];
    [encoder encodeObject:_attrstr forKey:@"attrstr"];
    [encoder encodeObject:self.accessibilityLabel forKey:@"label"];
    [encoder encodeInteger:(NSInteger)_pos forKey:@"pos"];
    [encoder encodeBool:_hasDescription forKey:@"hasDescription"];
}

- (BOOL)wantsToTrackMouse {
    return NO;
}

- (NSPoint)cellBaselineOffset {

    NSUInteger lastCharPos = _pos - 1;

    if (lastCharPos > _attrstr.length)
        lastCharPos = 0;

    NSFont *font = [_attrstr attribute:NSFontAttributeName
                               atIndex:lastCharPos
                        effectiveRange:nil];

    CGFloat xheight = font.ascender;

    if (align == imagealign_InlineCenter) {
        return NSMakePoint(0, -(self.image.size.height / 2) + font.xHeight / 2);
    } else if (align == imagealign_InlineDown) {
        return NSMakePoint(0, -self.image.size.height + xheight);
    }

    return [super cellBaselineOffset];
}

- (NSString *)customA11yLabel {
    if (_marginImage) {
        return _marginImage.customA11yLabel;
    }
    NSString *label = self.image.accessibilityDescription;
    NSUInteger lastCharPos = _pos - 1;

    if (lastCharPos > _attrstr.length)
        lastCharPos = 0;

    NSNumber *linkVal = (NSNumber *)[_attrstr
                                     attribute:NSLinkAttributeName
                                     atIndex:lastCharPos
                                     effectiveRange:nil];

    NSUInteger linkid = linkVal.unsignedIntegerValue;

    if (!label.length) {
        if (linkid)
            label = @"Clickable attached image";
        else
            label = @"Attached image";
    }
    return label;
}

- (NSSize)cellSize {
    if (align == imagealign_MarginLeft || align == imagealign_MarginRight) {
        return NSZeroSize;
    }
    return [super cellSize];
}

- (NSRect)cellFrameForTextContainer:(NSTextContainer *)textContainer
               proposedLineFragment:(NSRect)lineFrag
                      glyphPosition:(NSPoint)position
                     characterIndex:(NSUInteger)charIndex {
    if (align == imagealign_MarginLeft || align == imagealign_MarginRight) {
        return NSZeroRect;
    }
    return [super cellFrameForTextContainer:textContainer
                       proposedLineFragment:lineFrag
                              glyphPosition:position
                             characterIndex:charIndex];
}

- (void)drawWithFrame:(NSRect)cellFrame
               inView:(NSView *)controlView {
    switch (align) {
        case imagealign_MarginLeft:
        case imagealign_MarginRight:
            break;
        default:
            [super drawWithFrame:cellFrame
                          inView:controlView];
            break;
    }
}

- (void)drawWithFrame:(NSRect)cellFrame
               inView:(NSView *)controlView
       characterIndex:(NSUInteger)charIndex {
    switch (align) {
        case imagealign_MarginLeft:
        case imagealign_MarginRight:
            break;
        default:
            [super drawWithFrame:cellFrame
                          inView:controlView
                  characterIndex:charIndex];
            break;
    }

}

- (void)drawWithFrame:(NSRect)cellFrame
               inView:(NSView *)controlView
       characterIndex:(NSUInteger)charIndex
        layoutManager:(NSLayoutManager *)layoutManager  {
    switch (align) {
        case imagealign_MarginLeft:
        case imagealign_MarginRight:
            break;
        default:
            [super drawWithFrame:cellFrame
                          inView:controlView
                  characterIndex:charIndex
                   layoutManager:layoutManager];
            break;
    }

}

- (void)highlight:(BOOL)flag
        withFrame:(NSRect)cellFrame
           inView:(NSView *)controlView {
    switch (align) {
        case imagealign_MarginLeft:
        case imagealign_MarginRight:
            break;
        default:
            [super highlight:flag
                   withFrame:cellFrame
                      inView:controlView];
            break;
    }
}


@end
