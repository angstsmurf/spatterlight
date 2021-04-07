//
//  MyAttachmentCell.m
//  Spatterlight
//
//  Created by Administrator on 2021-04-02.
//

#import "MyAttachmentCell.h"
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
        if (image.accessibilityDescription.length)
            self.accessibilityLabel = image.accessibilityDescription;
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        align = [decoder decodeIntegerForKey:@"align"];
        _attrstr = [decoder decodeObjectOfClass:[NSAttributedString class] forKey:@"attstr"];
        _pos = (NSUInteger)[decoder decodeIntegerForKey:@"pos"];
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
    NSString *label = self.image.accessibilityDescription;
    NSUInteger lastCharPos = _pos - 1;

    if (lastCharPos > _attrstr.length)
        lastCharPos = 0;

   NSNumber *linkVal = (NSNumber *)[_attrstr attribute:NSLinkAttributeName
                               atIndex:lastCharPos
                        effectiveRange:nil];

    NSUInteger linkid = linkVal.unsignedIntegerValue;

    if (!label.length) {
        label = [NSString stringWithFormat: @"%@attached image", linkid ? @"Clickable " : @""];
    }
    return label;
}


@end
