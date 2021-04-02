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
    NSUInteger pos;
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
        pos = apos;
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super initWithCoder:decoder];
    if (self) {
        align = [decoder decodeIntegerForKey:@"align"];
        _attrstr = [decoder decodeObjectOfClass:[NSAttributedString class] forKey:@"attstr"];
        pos = (NSUInteger)[decoder decodeIntegerForKey:@"pos"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [super encodeWithCoder:encoder];
    [encoder encodeInteger:align forKey:@"align"];
    [encoder encodeObject:_attrstr forKey:@"attrstr"];
    [encoder encodeInteger:(NSInteger)pos forKey:@"pos"];
}

- (BOOL)wantsToTrackMouse {
    return NO;
}

- (NSPoint)cellBaselineOffset {

    NSUInteger lastCharPos = pos - 1;

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

@end
